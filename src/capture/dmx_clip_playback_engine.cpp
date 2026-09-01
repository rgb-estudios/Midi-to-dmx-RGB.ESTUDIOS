#include "capture/dmx_clip_playback_engine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace aeyla::capture {
namespace {

// DMX runs at <=60 Hz (22.7 ms at the normal 44 Hz). Polling the file
// cursor every 1 ms creates needless wakeups and mutex traffic beside the
// audio engine. 4 ms preserves sub-frame response with far less pressure.
constexpr auto kWorkerSleep = std::chrono::milliseconds(4);

bool valid_sample_rate(double sample_rate) noexcept {
  return std::isfinite(sample_rate) &&
         sample_rate >= 8000.0 && sample_rate <= 768000.0;
}

}  // namespace

DmxClipPlaybackEngine::DmxClipPlaybackEngine() = default;

DmxClipPlaybackEngine::~DmxClipPlaybackEngine() { shutdown(); }

void DmxClipPlaybackEngine::attach(
    output::ArtNetOutputWorker* output) noexcept {
  const std::scoped_lock lock(mutex_);
  if(output_ != output && output_ != nullptr)
    output_->set_override_enabled(false);
  output_ = output;
}

bool DmxClipPlaybackEngine::load_clip(const std::filesystem::path& path,
                                      double sample_rate,
                                      std::string& error_message) {
  error_message.clear();
  if(!valid_sample_rate(sample_rate)) {
    error_message = "La frecuencia de muestreo del host está fuera del rango admitido";
    return false;
  }

  disarm();
  {
    const std::scoped_lock lock(mutex_);
    reader_.close();
    std::string reader_error;
    if(!reader_.open(path, reader_error)) {
      error_message = std::move(reader_error);
      loaded_.store(false, std::memory_order_release);
      return false;
    }
    const auto info = reader_.info();
    if(!info.open || info.frame_count == 0U) {
      reader_.close();
      loaded_.store(false, std::memory_order_release);
      error_message = "El clip DMX validado no contiene cuadros reproducibles";
      return false;
    }

    sample_rate_ = sample_rate;
    range_start_frame_ = 0U;
    range_end_frame_exclusive_ = info.frame_count;
    cursor_samples_.store(0U, std::memory_order_relaxed);
    hold_frame_.fill(0U);
    hold_valid_ = false;
    current_frame_.store(0U, std::memory_order_relaxed);
    progress_.store(0.0, std::memory_order_relaxed);
    transport_.store(DmxClipTransportState::ready, std::memory_order_release);
    clock_source_.store(DmxClipClockSource::host_samples,
                        std::memory_order_release);
    monotonic_anchor_samples_ = 0U;
    monotonic_anchor_time_ = std::chrono::steady_clock::now();
    rendering_offline_.store(false, std::memory_order_release);
    heartbeat_ok_.store(false, std::memory_order_release);
    set_error({});
    loaded_.store(true, std::memory_order_release);
  }
  ensure_thread();
  return true;
}

bool DmxClipPlaybackEngine::replace_armed_clip(
    DmxTakeFileReader& validated_reader,
    double sample_rate,
    std::uint64_t range_start_frame,
    std::uint64_t range_end_frame_exclusive,
    DmxClipClockSource clock_source,
    std::uint64_t elapsed_samples,
    std::string& error_message) {
  error_message.clear();
  if(!valid_sample_rate(sample_rate)) {
    error_message = "La frecuencia de muestreo del host está fuera del rango admitido";
    return false;
  }
  const auto candidate = validated_reader.info();
  if(!candidate.open || candidate.frame_count == 0U) {
    error_message = "La toma preparada no contiene cuadros DMX validados";
    return false;
  }
  if(range_start_frame >= range_end_frame_exclusive ||
     range_end_frame_exclusive > candidate.frame_count) {
    error_message = "El rango preparado está fuera de la toma validada";
    return false;
  }

  {
    const std::scoped_lock lock(mutex_);
    if(!armed_.load(std::memory_order_acquire)) {
      error_message = "La autoridad anterior se desarmó durante la preparación";
      return false;
    }
    if(output_ == nullptr || output_->stats().fail_closed) {
      error_message = "La salida Art-Net requiere rearme manual";
      return false;
    }
    if(rendering_offline_.load(std::memory_order_acquire)) {
      error_message = "El renderizado sin conexión bloquea el cambio de canción";
      return false;
    }

    // The Art-Net worker keeps retransmitting the old held frame throughout
    // candidate validation. This single swap is the authority boundary: no
    // DISARM/BLACKOUT gap is introduced between Songs.
    reader_.swap(validated_reader);
    sample_rate_ = sample_rate;
    range_start_frame_ = range_start_frame;
    range_end_frame_exclusive_ = range_end_frame_exclusive;
    cursor_samples_.store(elapsed_samples, std::memory_order_relaxed);
    hold_valid_ = false;
    current_frame_.store(range_start_frame, std::memory_order_relaxed);
    progress_.store(0.0, std::memory_order_relaxed);
    clock_source_.store(clock_source, std::memory_order_release);
    monotonic_anchor_samples_ = elapsed_samples;
    monotonic_anchor_time_ = std::chrono::steady_clock::now();
    transport_.store(DmxClipTransportState::playing,
                     std::memory_order_release);
    loaded_.store(true, std::memory_order_release);
    set_error({});
  }
  ensure_thread();
  return true;
}

void DmxClipPlaybackEngine::unload() noexcept {
  disarm();
  const std::scoped_lock lock(mutex_);
  reader_.close();
  sample_rate_ = 0.0;
  range_start_frame_ = 0U;
  range_end_frame_exclusive_ = 0U;
  cursor_samples_.store(0U, std::memory_order_relaxed);
  hold_frame_.fill(0U);
  hold_valid_ = false;
  loaded_.store(false, std::memory_order_release);
  transport_.store(DmxClipTransportState::ready, std::memory_order_release);
  clock_source_.store(DmxClipClockSource::host_samples,
                      std::memory_order_release);
  current_frame_.store(0U, std::memory_order_relaxed);
  progress_.store(0.0, std::memory_order_relaxed);
}

bool DmxClipPlaybackEngine::set_play_range(
    std::uint64_t start_frame,
    std::uint64_t end_frame_exclusive,
    std::string& error_message) {
  error_message.clear();
  if(armed_.load(std::memory_order_acquire)) {
    error_message = "Desarma la salida del clip DMX antes de editar su rango";
    return false;
  }

  const std::scoped_lock lock(mutex_);
  const auto info = reader_.info();
  if(!loaded_.load(std::memory_order_acquire) || !info.open) {
    error_message = "Carga un clip DMX antes de editar su rango";
    return false;
  }
  if(start_frame >= end_frame_exclusive ||
     end_frame_exclusive > info.frame_count) {
    error_message = "El rango del clip DMX está fuera de los límites de la toma";
    return false;
  }

  range_start_frame_ = start_frame;
  range_end_frame_exclusive_ = end_frame_exclusive;
  cursor_samples_.store(0U, std::memory_order_relaxed);
  hold_valid_ = false;
  current_frame_.store(start_frame, std::memory_order_relaxed);
  progress_.store(0.0, std::memory_order_relaxed);
  transport_.store(DmxClipTransportState::ready, std::memory_order_release);
  return true;
}

void DmxClipPlaybackEngine::reset_play_range() noexcept {
  if(armed_.load(std::memory_order_acquire)) return;
  const std::scoped_lock lock(mutex_);
  const auto info = reader_.info();
  if(!loaded_.load(std::memory_order_acquire) || !info.open) return;
  range_start_frame_ = 0U;
  range_end_frame_exclusive_ = info.frame_count;
  cursor_samples_.store(0U, std::memory_order_relaxed);
  hold_valid_ = false;
  current_frame_.store(0U, std::memory_order_relaxed);
  progress_.store(0.0, std::memory_order_relaxed);
  transport_.store(DmxClipTransportState::ready, std::memory_order_release);
}

bool DmxClipPlaybackEngine::arm(std::string& error_message) {
  error_message.clear();
  {
    const std::scoped_lock lock(mutex_);
    if(!loaded_.load(std::memory_order_acquire) || !reader_.info().open) {
      error_message = "Carga un clip DMX validado antes de armar la salida";
      return false;
    }
    if(output_ == nullptr) {
      error_message = "El reproductor DMX no está conectado a la salida Art-Net";
      return false;
    }
    if(rendering_offline_.load(std::memory_order_acquire)) {
      error_message = "El renderizado sin conexión bloquea la salida DMX física";
      return false;
    }
  }

  ensure_thread();
  set_error({});
  armed_.store(true, std::memory_order_release);

  // ARMAR grants physical authority independently of artistic transport.
  // Publish frame zero/current cursor immediately so receivers keep a healthy
  // Art-Net stream while the DAW is stopped. PLAY/PAUSE/RESET only move or
  // freeze the artistic cursor; DISARM/BLACKOUT is the authority boundary.
  {
    const std::scoped_lock lock(mutex_);
    if(!publish_cursor_frame_locked()) {
      armed_.store(false, std::memory_order_release);
      if(output_ != nullptr)
        output_->set_override_enabled(false);
      error_message = error();
      if(error_message.empty())
        error_message = "No se pudo preparar el cuadro DMX inicial al armar";
      return false;
    }
    if(output_ != nullptr &&
       !rendering_offline_.load(std::memory_order_acquire))
      output_->set_override_enabled(true);
  }
  return true;
}

void DmxClipPlaybackEngine::disarm() noexcept {
  armed_.store(false, std::memory_order_release);
  const std::scoped_lock lock(mutex_);
  if(output_ != nullptr)
    output_->set_override_enabled(false);
}

bool DmxClipPlaybackEngine::play_from_start(
    DmxClipClockSource clock_source,
    std::string& error_message,
    std::uint64_t elapsed_samples) {
  error_message.clear();
  const std::scoped_lock lock(mutex_);
  if(!loaded_.load(std::memory_order_acquire) || !reader_.info().open) {
    error_message = "Carga un clip DMX antes de reproducir";
    return false;
  }
  cursor_samples_.store(elapsed_samples, std::memory_order_relaxed);
  hold_valid_ = false;
  current_frame_.store(range_start_frame_, std::memory_order_relaxed);
  progress_.store(0.0, std::memory_order_relaxed);
  clock_source_.store(clock_source, std::memory_order_release);
  monotonic_anchor_samples_ = elapsed_samples;
  monotonic_anchor_time_ = std::chrono::steady_clock::now();
  transport_.store(DmxClipTransportState::playing, std::memory_order_release);
  return true;
}

bool DmxClipPlaybackEngine::pause(std::string& error_message,
                                  std::uint64_t rewind_samples) {
  error_message.clear();
  const std::scoped_lock lock(mutex_);
  if(transport_.load(std::memory_order_acquire) != DmxClipTransportState::playing) {
    error_message = "El clip DMX sólo puede pausarse mientras está reproduciendo";
    return false;
  }
  if(uses_monotonic_clock())
    update_monotonic_cursor_locked(std::chrono::steady_clock::now());
  else if(rewind_samples > 0U) {
    auto cursor = cursor_samples_.load(std::memory_order_relaxed);
    while(!cursor_samples_.compare_exchange_weak(
        cursor, rewind_samples >= cursor ? 0U : cursor - rewind_samples,
        std::memory_order_relaxed, std::memory_order_relaxed)) {
    }
  }
  transport_.store(DmxClipTransportState::paused, std::memory_order_release);
  return true;
}

bool DmxClipPlaybackEngine::resume(std::string& error_message,
                                   std::uint64_t elapsed_samples) {
  error_message.clear();
  const std::scoped_lock lock(mutex_);
  if(transport_.load(std::memory_order_acquire) != DmxClipTransportState::paused) {
    error_message = "El clip DMX sólo puede reanudarse desde pausa";
    return false;
  }
  if(uses_monotonic_clock()) {
    monotonic_anchor_samples_ = cursor_samples_.load(std::memory_order_relaxed);
    monotonic_anchor_time_ = std::chrono::steady_clock::now();
  }
  else if(elapsed_samples > 0U)
    cursor_samples_.fetch_add(elapsed_samples, std::memory_order_relaxed);
  transport_.store(DmxClipTransportState::playing, std::memory_order_release);
  return true;
}

bool DmxClipPlaybackEngine::seek_frame(std::uint64_t frame_index,
                                       std::string& error_message) {
  error_message.clear();
  if(armed_.load(std::memory_order_acquire)) {
    error_message = "Desarma la salida DMX antes de mover el cabezal del editor";
    return false;
  }
  if(transport_.load(std::memory_order_acquire) ==
     DmxClipTransportState::playing) {
    error_message = "Pausa o detén el clip DMX antes de mover el cabezal";
    return false;
  }

  const std::scoped_lock lock(mutex_);
  const auto info = reader_.info();
  if(!loaded_.load(std::memory_order_acquire) || !info.open ||
     !valid_sample_rate(sample_rate_)) {
    error_message = "Carga un clip DMX antes de mover el cabezal";
    return false;
  }
  if(frame_index < range_start_frame_ ||
     frame_index >= range_end_frame_exclusive_) {
    error_message = "El cabezal del editor está fuera del rango ENTRADA / SALIDA";
    return false;
  }

  DmxUniverse frame{};
  if(!reader_.read_frame(frame_index, frame, error_message)) {
    error_message = "No se pudo previsualizar el cuadro DMX · " + error_message;
    return false;
  }

  const auto offset = frame_index - range_start_frame_;
  const long double samples =
      static_cast<long double>(offset) *
      static_cast<long double>(sample_rate_) /
      static_cast<long double>(info.frames_per_second);
  const auto effective_frames =
      range_end_frame_exclusive_ - range_start_frame_;
  const double progress = effective_frames <= 1U
      ? 1.0
      : std::clamp(static_cast<double>(offset) /
                       static_cast<double>(effective_frames - 1U),
                   0.0, 1.0);

  hold_frame_ = frame;
  hold_valid_ = true;
  cursor_samples_.store(static_cast<std::uint64_t>(std::llround(samples)),
                        std::memory_order_relaxed);
  current_frame_.store(frame_index, std::memory_order_relaxed);
  progress_.store(progress, std::memory_order_relaxed);
  transport_.store(DmxClipTransportState::paused, std::memory_order_release);
  return true;
}

void DmxClipPlaybackEngine::stop_and_reset(
    bool preserve_armed_authority) noexcept {
  transport_.store(DmxClipTransportState::ready, std::memory_order_release);
  const std::scoped_lock lock(mutex_);
  cursor_samples_.store(0U, std::memory_order_relaxed);
  monotonic_anchor_samples_ = 0U;
  monotonic_anchor_time_ = std::chrono::steady_clock::now();
  hold_valid_ = false;
  current_frame_.store(range_start_frame_, std::memory_order_relaxed);
  progress_.store(0.0, std::memory_order_relaxed);

  if(preserve_armed_authority && armed_.load(std::memory_order_acquire)) {
    if(!publish_cursor_frame_locked()) {
      armed_.store(false, std::memory_order_release);
      if(output_ != nullptr)
        output_->set_override_enabled(false);
      return;
    }

    if(output_ != nullptr) {
      const bool can_authorize =
          !rendering_offline_.load(std::memory_order_acquire);
      output_->set_override_enabled(can_authorize);
    }
    return;
  }

  if(output_ != nullptr)
    output_->set_override_enabled(false);
}

void DmxClipPlaybackEngine::advance_samples(std::uint32_t processed_samples,
                                            bool rendering_offline) noexcept {
  rendering_offline_.store(rendering_offline, std::memory_order_release);
  if(rendering_offline || processed_samples == 0U ||
     transport_.load(std::memory_order_acquire) != DmxClipTransportState::playing ||
     uses_monotonic_clock())
    return;

  cursor_samples_.fetch_add(static_cast<std::uint64_t>(processed_samples),
                            std::memory_order_relaxed);
}

void DmxClipPlaybackEngine::synchronize_host_cursor(
    std::uint64_t cursor_samples) noexcept {
  if(uses_monotonic_clock()) return;
  const auto state = transport_.load(std::memory_order_acquire);
  if(state != DmxClipTransportState::playing &&
     state != DmxClipTransportState::paused)
    return;
  cursor_samples_.store(cursor_samples, std::memory_order_relaxed);
}

void DmxClipPlaybackEngine::set_host_heartbeat_ok(bool ok) noexcept {
  heartbeat_ok_.store(ok, std::memory_order_release);
  // Revalidate after acquiring the reader/output mutex. A transport change may
  // happen between the first atomic read and this side effect; stale PLAYING
  // must never disable a newly READY/PAUSED/ENDED armed carrier.
  if(!ok && !uses_monotonic_clock()) {
    const std::scoped_lock lock(mutex_);
    if(output_ != nullptr &&
       !heartbeat_ok_.load(std::memory_order_acquire) &&
       transport_.load(std::memory_order_acquire) ==
           DmxClipTransportState::playing &&
       !uses_monotonic_clock())
      output_->set_override_enabled(false);
  }
}

DmxClipPlaybackStatus DmxClipPlaybackEngine::status() const {
  DmxClipPlaybackStatus result;
  result.running = running_.load(std::memory_order_acquire);
  result.loaded = loaded_.load(std::memory_order_acquire);
  result.armed = armed_.load(std::memory_order_acquire);
  result.host_heartbeat_ok = heartbeat_ok_.load(std::memory_order_acquire);
  result.rendering_offline = rendering_offline_.load(std::memory_order_acquire);
  result.transport = transport_.load(std::memory_order_acquire);
  result.clock_source = clock_source_.load(std::memory_order_acquire);
  result.current_frame = current_frame_.load(std::memory_order_relaxed);
  result.cursor_samples = cursor_samples_.load(std::memory_order_relaxed);
  result.progress = progress_.load(std::memory_order_relaxed);
  result.error = error();
  {
    const std::scoped_lock lock(mutex_);
    result.hold_valid = hold_valid_;
    result.range_start_frame = range_start_frame_;
    result.range_end_frame_exclusive = range_end_frame_exclusive_;
  }
  return result;
}

bool DmxClipPlaybackEngine::uses_monotonic_clock() const noexcept {
  return clock_source_.load(std::memory_order_acquire) ==
         DmxClipClockSource::monotonic_realtime;
}

void DmxClipPlaybackEngine::ensure_thread() {
  if(worker_.joinable()) return;
  stop_requested_.store(false, std::memory_order_release);
  worker_ = std::thread([this]() { run(); });
}

void DmxClipPlaybackEngine::shutdown() noexcept {
  armed_.store(false, std::memory_order_release);
  stop_requested_.store(true, std::memory_order_release);
  if(worker_.joinable()) worker_.join();
  running_.store(false, std::memory_order_release);
  heartbeat_ok_.store(false, std::memory_order_release);
  const std::scoped_lock lock(mutex_);
  if(output_ != nullptr)
    output_->set_override_enabled(false);
  reader_.close();
  loaded_.store(false, std::memory_order_release);
}

bool DmxClipPlaybackEngine::publish_cursor_frame_locked() {
  const auto info = reader_.info();
  if(!loaded_.load(std::memory_order_acquire) || !info.open ||
     !valid_sample_rate(sample_rate_) || range_start_frame_ >= range_end_frame_exclusive_) {
    set_error("El runtime del clip DMX perdió un estado de carga válido");
    transport_.store(DmxClipTransportState::fault, std::memory_order_release);
    return false;
  }

  const auto effective_frames = range_end_frame_exclusive_ - range_start_frame_;
  const auto cursor = cursor_samples_.load(std::memory_order_relaxed);
  const long double scaled =
      static_cast<long double>(cursor) *
      static_cast<long double>(info.frames_per_second) /
      static_cast<long double>(sample_rate_);
  std::uint64_t offset = scaled <= 0.0L
      ? 0U
      : static_cast<std::uint64_t>(std::floor(scaled));

  bool ended = false;
  if(offset >= effective_frames) {
    offset = effective_frames - 1U;
    ended = true;
  }
  const std::uint64_t frame_index = range_start_frame_ + offset;

  if(!hold_valid_ || current_frame_.load(std::memory_order_relaxed) != frame_index) {
    DmxUniverse frame{};
    std::string read_error;
    if(!reader_.read_frame(frame_index, frame, read_error)) {
      set_error("Falló la lectura de un cuadro del clip DMX · " + read_error);
      transport_.store(DmxClipTransportState::fault, std::memory_order_release);
      return false;
    }
    hold_frame_ = frame;
    hold_valid_ = true;
    current_frame_.store(frame_index, std::memory_order_relaxed);
    if(output_ != nullptr)
      output_->publish_override(hold_frame_, ++generation_);
  }

  const double progress = effective_frames <= 1U
      ? 1.0
      : std::clamp(static_cast<double>(offset) /
                       static_cast<double>(effective_frames - 1U),
                   0.0, 1.0);
  progress_.store(ended ? 1.0 : progress, std::memory_order_relaxed);
  if(ended)
    transport_.store(DmxClipTransportState::ended, std::memory_order_release);
  return true;
}

void DmxClipPlaybackEngine::update_monotonic_cursor_locked(
    std::chrono::steady_clock::time_point now) noexcept {
  if(!uses_monotonic_clock() ||
     transport_.load(std::memory_order_acquire) != DmxClipTransportState::playing ||
     !valid_sample_rate(sample_rate_))
    return;

  const long double elapsed = std::max(
      0.0L,
      std::chrono::duration<long double>(now - monotonic_anchor_time_).count());
  const long double advanced = elapsed * static_cast<long double>(sample_rate_);
  const long double maximum = static_cast<long double>(
      std::numeric_limits<std::uint64_t>::max() - monotonic_anchor_samples_);
  const auto bounded = static_cast<std::uint64_t>(
      std::floor(std::min(advanced, maximum)));
  cursor_samples_.store(monotonic_anchor_samples_ + bounded,
                        std::memory_order_relaxed);
}

void DmxClipPlaybackEngine::run() noexcept {
  running_.store(true, std::memory_order_release);

  while(!stop_requested_.load(std::memory_order_acquire)) {
    output::ArtNetOutputWorker* output = nullptr;
    {
      const std::scoped_lock lock(mutex_);
      output = output_;
    }

    if(!armed_.load(std::memory_order_acquire) || output == nullptr) {
      std::this_thread::sleep_for(kWorkerSleep);
      continue;
    }

    const auto observed_state = transport_.load(std::memory_order_acquire);
    const bool observed_host_clock_required =
        observed_state == DmxClipTransportState::playing &&
        !uses_monotonic_clock();
    if((observed_host_clock_required &&
        !heartbeat_ok_.load(std::memory_order_acquire)) ||
       rendering_offline_.load(std::memory_order_acquire)) {
      // State may have changed after observed_state was loaded (notably an
      // atomic armed Song swap). Revalidate before revoking authority so a
      // stale PLAYING observation cannot punch a one-cycle hole in READY/HOLD.
      const auto current_state = transport_.load(std::memory_order_acquire);
      const bool current_host_clock_required =
          current_state == DmxClipTransportState::playing &&
          !uses_monotonic_clock();
      const bool offline = rendering_offline_.load(std::memory_order_acquire);
      if(offline ||
         (current_host_clock_required &&
          !heartbeat_ok_.load(std::memory_order_acquire))) {
        output->set_override_enabled(false);
        std::this_thread::sleep_for(kWorkerSleep);
        continue;
      }
    }

    if(observed_state == DmxClipTransportState::fault) {
      if(transport_.load(std::memory_order_acquire) ==
         DmxClipTransportState::fault) {
        output->set_override_enabled(false);
        std::this_thread::sleep_for(kWorkerSleep);
        continue;
      }
    }

    // READY is a stopped artistic transport, not a disconnected endpoint. A
    // valid held frame stays authoritative while ARM remains active. Re-read
    // transport under the same mutex as hold_valid_: a stale READY snapshot
    // must never disable a clip that has just been atomically replaced into
    // PLAYING with a new reader/cursor.
    bool ready_without_hold = false;
    {
      const std::scoped_lock lock(mutex_);
      const auto current_state = transport_.load(std::memory_order_acquire);
      ready_without_hold = current_state == DmxClipTransportState::ready &&
                           !hold_valid_;
    }
    if(ready_without_hold) {
      output->set_override_enabled(false);
      std::this_thread::sleep_for(kWorkerSleep);
      continue;
    }

    bool published = false;
    {
      const std::scoped_lock lock(mutex_);
      const auto current_state = transport_.load(std::memory_order_acquire);
      if(current_state == DmxClipTransportState::playing &&
         uses_monotonic_clock())
        update_monotonic_cursor_locked(std::chrono::steady_clock::now());
      published = publish_cursor_frame_locked();
    }

    if(!published) {
      armed_.store(false, std::memory_order_release);
      output->set_override_enabled(false);
    } else {
      const auto after = transport_.load(std::memory_order_acquire);
      const bool state_authoritative =
          after == DmxClipTransportState::ready ||
          after == DmxClipTransportState::playing ||
          after == DmxClipTransportState::paused ||
          after == DmxClipTransportState::ended;
      const bool clock_authoritative =
          after != DmxClipTransportState::playing || uses_monotonic_clock() ||
          heartbeat_ok_.load(std::memory_order_acquire);
      output->set_override_enabled(
          state_authoritative && clock_authoritative &&
          !rendering_offline_.load(std::memory_order_acquire));
    }

    std::this_thread::sleep_for(kWorkerSleep);
  }

  running_.store(false, std::memory_order_release);
}

void DmxClipPlaybackEngine::set_error(std::string message) noexcept {
  try {
    const std::scoped_lock lock(error_mutex_);
    error_ = std::move(message);
  } catch(...) {
  }
}

std::string DmxClipPlaybackEngine::error() const {
  const std::scoped_lock lock(error_mutex_);
  return error_;
}

}  // namespace aeyla::capture
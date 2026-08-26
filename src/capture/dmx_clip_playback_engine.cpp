#include "capture/dmx_clip_playback_engine.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace aeyla::capture {
namespace {

constexpr auto kWorkerSleep = std::chrono::milliseconds(1);

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
    rendering_offline_.store(false, std::memory_order_release);
    heartbeat_ok_.store(false, std::memory_order_release);
    set_error({});
    loaded_.store(true, std::memory_order_release);
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
  return true;
}

void DmxClipPlaybackEngine::disarm() noexcept {
  armed_.store(false, std::memory_order_release);
  const std::scoped_lock lock(mutex_);
  if(output_ != nullptr)
    output_->set_override_enabled(false);
}

bool DmxClipPlaybackEngine::play_from_start(std::string& error_message) {
  error_message.clear();
  const std::scoped_lock lock(mutex_);
  if(!loaded_.load(std::memory_order_acquire) || !reader_.info().open) {
    error_message = "Carga un clip DMX antes de reproducir";
    return false;
  }
  cursor_samples_.store(0U, std::memory_order_relaxed);
  hold_valid_ = false;
  current_frame_.store(range_start_frame_, std::memory_order_relaxed);
  progress_.store(0.0, std::memory_order_relaxed);
  transport_.store(DmxClipTransportState::playing, std::memory_order_release);
  return true;
}

bool DmxClipPlaybackEngine::pause(std::string& error_message) {
  error_message.clear();
  if(transport_.load(std::memory_order_acquire) != DmxClipTransportState::playing) {
    error_message = "El clip DMX sólo puede pausarse mientras está reproduciendo";
    return false;
  }
  transport_.store(DmxClipTransportState::paused, std::memory_order_release);
  return true;
}

bool DmxClipPlaybackEngine::resume(std::string& error_message) {
  error_message.clear();
  if(transport_.load(std::memory_order_acquire) != DmxClipTransportState::paused) {
    error_message = "El clip DMX sólo puede reanudarse desde pausa";
    return false;
  }
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

void DmxClipPlaybackEngine::stop_and_reset() noexcept {
  transport_.store(DmxClipTransportState::ready, std::memory_order_release);
  const std::scoped_lock lock(mutex_);
  cursor_samples_.store(0U, std::memory_order_relaxed);
  hold_valid_ = false;
  current_frame_.store(range_start_frame_, std::memory_order_relaxed);
  progress_.store(0.0, std::memory_order_relaxed);
  if(output_ != nullptr)
    output_->set_override_enabled(false);
}

void DmxClipPlaybackEngine::advance_samples(std::uint32_t processed_samples,
                                            bool rendering_offline) noexcept {
  rendering_offline_.store(rendering_offline, std::memory_order_release);
  if(rendering_offline || processed_samples == 0U ||
     transport_.load(std::memory_order_acquire) != DmxClipTransportState::playing)
    return;

  cursor_samples_.fetch_add(static_cast<std::uint64_t>(processed_samples),
                            std::memory_order_relaxed);
}

void DmxClipPlaybackEngine::set_host_heartbeat_ok(bool ok) noexcept {
  heartbeat_ok_.store(ok, std::memory_order_release);
  if(!ok) {
    const std::scoped_lock lock(mutex_);
    if(output_ != nullptr)
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

void DmxClipPlaybackEngine::run() noexcept {
  running_.store(true, std::memory_order_release);

  while(!stop_requested_.load(std::memory_order_acquire)) {
    output::ArtNetOutputWorker* output = nullptr;
    {
      const std::scoped_lock lock(mutex_);
      output = output_;
    }

    if(!armed_.load(std::memory_order_acquire) || output == nullptr ||
       !heartbeat_ok_.load(std::memory_order_acquire) ||
       rendering_offline_.load(std::memory_order_acquire)) {
      if(output != nullptr)
        output->set_override_enabled(false);
      std::this_thread::sleep_for(kWorkerSleep);
      continue;
    }

    const auto state = transport_.load(std::memory_order_acquire);
    if(state == DmxClipTransportState::ready ||
       state == DmxClipTransportState::fault) {
      output->set_override_enabled(false);
      std::this_thread::sleep_for(kWorkerSleep);
      continue;
    }

    bool published = false;
    {
      const std::scoped_lock lock(mutex_);
      published = publish_cursor_frame_locked();
    }

    if(!published) {
      armed_.store(false, std::memory_order_release);
      output->set_override_enabled(false);
    } else {
      const auto after = transport_.load(std::memory_order_acquire);
      const bool authoritative = after == DmxClipTransportState::playing ||
          after == DmxClipTransportState::paused ||
          after == DmxClipTransportState::ended;
      output->set_override_enabled(authoritative);
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

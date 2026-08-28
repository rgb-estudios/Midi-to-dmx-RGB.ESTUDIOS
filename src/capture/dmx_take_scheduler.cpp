#include "capture/dmx_take_scheduler.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace aeyla::capture {

DmxTakeScheduler::DmxTakeScheduler() = default;
DmxTakeScheduler::~DmxTakeScheduler() { shutdown(); }

void DmxTakeScheduler::attach(output::ArtNetOutputWorker* output,
                              const runtime::HostTransportMailbox* host) noexcept {
  {
    const std::scoped_lock lock(mutex_);
    output_ = output;
    host_ = host;
  }
  file_player_.attach(output);
}

bool DmxTakeScheduler::load_take(const DmxTake* take,
                                 std::string& error_message) {
  error_message.clear();
  if(take == nullptr || take->frames.empty() || take->frames_per_second == 0U) {
    error_message = "La toma seleccionada no contiene cuadros DMX reproducibles";
    return false;
  }
  if(status().playing) {
    error_message = "Detén la toma activa antes de cargar otra";
    return false;
  }

  file_player_.disarm();
  file_player_.unload();
  file_mode_.store(false, std::memory_order_release);

  const std::scoped_lock lock(mutex_);
  take_ = take;
  range_start_frame_ = take_->effective_start_frame();
  range_end_frame_exclusive_ = take_->effective_end_frame_exclusive();
  if(range_end_frame_exclusive_ <= range_start_frame_ + 1U) {
    range_start_frame_ = 0U;
    range_end_frame_exclusive_ = take_->frames.size();
  }
  current_frame_ = range_start_frame_;
  hold_frame_ = take_->frames[current_frame_];
  hold_valid_ = true;
  progress_.store(0.0, std::memory_order_release);
  error_.clear();
  if(armed_.load(std::memory_order_acquire))
    publish_hold_locked();
  return true;
}

bool DmxTakeScheduler::load_take_file(const std::filesystem::path& path,
                                      double sample_rate,
                                      std::string& error_message) {
  error_message.clear();
  if(status().armed) {
    error_message = "Desarma la salida DMX antes de cargar otra toma";
    return false;
  }
  if(status().playing) {
    error_message = "Detén el clip DMX activo antes de cargar otro";
    return false;
  }

  playing_.store(false, std::memory_order_release);
  armed_.store(false, std::memory_order_release);
  {
    const std::scoped_lock lock(mutex_);
    take_ = nullptr;
    hold_valid_ = false;
    error_.clear();
  }

  if(!file_player_.load_clip(path, sample_rate, error_message)) {
    file_mode_.store(false, std::memory_order_release);
    return false;
  }
  file_mode_.store(true, std::memory_order_release);
  ensure_thread();
  return true;
}

bool DmxTakeScheduler::replace_armed_take_file(
    DmxTakeFileReader& validated_reader,
    double sample_rate,
    std::size_t start_frame,
    std::size_t end_frame_exclusive,
    DmxClipClockSource clock_source,
    std::uint64_t elapsed_samples,
    std::string& error_message) {
  if(!file_mode_.load(std::memory_order_acquire)) {
    error_message = "La toma activa no usa el reproductor respaldado por archivo";
    return false;
  }
  if(!file_player_.replace_armed_clip(
         validated_reader, sample_rate,
         static_cast<std::uint64_t>(start_frame),
         static_cast<std::uint64_t>(end_frame_exclusive), clock_source,
         elapsed_samples, error_message))
    return false;
  armed_.store(true, std::memory_order_release);
  playing_.store(false, std::memory_order_release);
  file_mode_.store(true, std::memory_order_release);
  {
    const std::scoped_lock lock(mutex_);
    error_.clear();
  }
  return true;
}

bool DmxTakeScheduler::set_play_range(std::size_t start_frame,
                                      std::size_t end_frame_exclusive,
                                      std::string& error_message) {
  if(file_mode_.load(std::memory_order_acquire))
    return file_player_.set_play_range(start_frame, end_frame_exclusive,
                                       error_message);

  error_message.clear();
  if(playing_.load(std::memory_order_acquire)) {
    error_message = "Detén la reproducción antes de editar ENTRADA / SALIDA";
    return false;
  }

  const std::scoped_lock lock(mutex_);
  if(take_ == nullptr || take_->frames.empty()) {
    error_message = "Carga una toma antes de editar ENTRADA / SALIDA";
    return false;
  }
  if(start_frame >= take_->frames.size()) {
    error_message = "La ENTRADA está fuera de la toma grabada";
    return false;
  }
  if(end_frame_exclusive == 0U || end_frame_exclusive > take_->frames.size()) {
    error_message = "La SALIDA está fuera de la toma grabada";
    return false;
  }
  if(end_frame_exclusive <= start_frame + 1U) {
    error_message = "ENTRADA / SALIDA deben dejar al menos dos cuadros DMX";
    return false;
  }

  range_start_frame_ = start_frame;
  range_end_frame_exclusive_ = end_frame_exclusive;
  current_frame_ = range_start_frame_;
  hold_frame_ = take_->frames[current_frame_];
  hold_valid_ = true;
  progress_.store(0.0, std::memory_order_release);
  error_.clear();
  if(armed_.load(std::memory_order_acquire))
    publish_hold_locked();
  return true;
}

void DmxTakeScheduler::reset_play_range() noexcept {
  if(file_mode_.load(std::memory_order_acquire)) {
    file_player_.reset_play_range();
    return;
  }

  if(playing_.load(std::memory_order_acquire))
    return;
  const std::scoped_lock lock(mutex_);
  if(take_ == nullptr || take_->frames.empty())
    return;
  range_start_frame_ = 0U;
  range_end_frame_exclusive_ = take_->frames.size();
  current_frame_ = 0U;
  hold_frame_ = take_->frames.front();
  hold_valid_ = true;
  progress_.store(0.0, std::memory_order_release);
  if(armed_.load(std::memory_order_acquire))
    publish_hold_locked();
}

bool DmxTakeScheduler::play(std::string& error_message,
                            DmxClipClockSource clock_source,
                            std::uint64_t elapsed_samples) {
  if(file_mode_.load(std::memory_order_acquire))
    return file_player_.play_from_start(clock_source, error_message,
                                        elapsed_samples);

  error_message.clear();
  ensure_thread();
  const std::scoped_lock lock(mutex_);
  if(take_ == nullptr || take_->frames.empty()) {
    error_message = "No hay una toma cargada";
    return false;
  }
  if(range_end_frame_exclusive_ <= range_start_frame_ + 1U ||
     range_end_frame_exclusive_ > take_->frames.size()) {
    error_message = "El rango ENTRADA / SALIDA de la toma no es válido";
    return false;
  }
  current_frame_ = range_start_frame_;
  hold_frame_ = take_->frames[current_frame_];
  hold_valid_ = true;
  progress_.store(0.0, std::memory_order_release);
  play_started_ = std::chrono::steady_clock::now();
  playing_.store(true, std::memory_order_release);
  if(armed_.load(std::memory_order_acquire))
    publish_hold_locked();
  return true;
}

bool DmxTakeScheduler::pause(std::string& error_message,
                             std::uint64_t rewind_samples) {
  if(file_mode_.load(std::memory_order_acquire))
    return file_player_.pause(error_message, rewind_samples);
  stop_hold();
  error_message.clear();
  return true;
}

bool DmxTakeScheduler::resume(std::string& error_message,
                              std::uint64_t elapsed_samples) {
  if(file_mode_.load(std::memory_order_acquire))
    return file_player_.resume(error_message, elapsed_samples);
  error_message = "REANUDAR sólo está disponible en el reproductor DMX desde disco";
  return false;
}

bool DmxTakeScheduler::seek_frame(std::size_t frame_index,
                                  std::string& error_message) {
  if(file_mode_.load(std::memory_order_acquire))
    return file_player_.seek_frame(frame_index, error_message);
  error_message =
      "El cabezal del editor sólo está disponible para tomas respaldadas en disco";
  return false;
}

void DmxTakeScheduler::stop_hold() noexcept {
  if(file_mode_.load(std::memory_order_acquire)) {
    const auto current = file_player_.status().transport;
    if(current == DmxClipTransportState::playing) {
      std::string ignored;
      (void)file_player_.pause(ignored);
    }
    return;
  }

  playing_.store(false, std::memory_order_release);
  const std::scoped_lock lock(mutex_);
  if(armed_.load(std::memory_order_acquire) && hold_valid_)
    publish_hold_locked();
}

void DmxTakeScheduler::stop_reset() noexcept {
  if(file_mode_.load(std::memory_order_acquire)) {
    file_player_.stop_and_reset();
    return;
  }
  stop_hold();
  const std::scoped_lock lock(mutex_);
  if(take_ == nullptr || take_->frames.empty()) return;
  current_frame_ = range_start_frame_;
  hold_frame_ = take_->frames[current_frame_];
  hold_valid_ = true;
  progress_.store(0.0, std::memory_order_release);
}

void DmxTakeScheduler::advance_samples(std::uint32_t processed_samples,
                                       bool rendering_offline) noexcept {
  if(file_mode_.load(std::memory_order_acquire))
    file_player_.advance_samples(processed_samples, rendering_offline);
}

void DmxTakeScheduler::synchronize_host_cursor(
    std::uint64_t cursor_samples) noexcept {
  if(file_mode_.load(std::memory_order_acquire))
    file_player_.synchronize_host_cursor(cursor_samples);
}

bool DmxTakeScheduler::arm(std::string& error_message) {
  error_message.clear();
  ensure_thread();

  output::ArtNetOutputWorker* output = nullptr;
  const runtime::HostTransportMailbox* host = nullptr;
  {
    const std::scoped_lock lock(mutex_);
    output = output_;
    host = host_;
  }
  if(output == nullptr) {
    error_message = "La salida Art-Net no está conectada";
    return false;
  }
  if(!output->stats().running) {
    error_message = "La salida Art-Net no está activa";
    return false;
  }
  if(host == nullptr) {
    error_message = "No está disponible el pulso de vida del DAW";
    return false;
  }
  const auto host_state = host->latest();
  if(host_state.revision == 0U) {
    error_message = "El DAW aún no ha publicado un pulso de audio válido";
    return false;
  }
  if(host_state.rendering_offline) {
    error_message = "La salida DMX está bloqueada durante renderizado sin conexión";
    return false;
  }

  if(file_mode_.load(std::memory_order_acquire)) {
    file_player_.set_host_heartbeat_ok(true);
    output->prepare_explicit_rearm();
    if(!file_player_.arm(error_message))
      return false;
    armed_.store(true, std::memory_order_release);
    heartbeat_ok_.store(true, std::memory_order_release);
    {
      const std::scoped_lock lock(mutex_);
      error_.clear();
    }
    return true;
  }

  const std::scoped_lock lock(mutex_);
  if(take_ == nullptr || !hold_valid_) {
    error_message = "Carga o graba una toma antes de armar la salida";
    return false;
  }

  output_->prepare_explicit_rearm();
  heartbeat_ok_.store(true, std::memory_order_release);
  armed_.store(true, std::memory_order_release);
  output_->publish_override(hold_frame_, generation_++);
  output_->set_override_enabled(true);
  error_.clear();
  return true;
}

void DmxTakeScheduler::disarm() noexcept {
  armed_.store(false, std::memory_order_release);
  file_player_.disarm();
  if(output_ != nullptr)
    output_->set_override_enabled(false);
}

DmxTakeSchedulerStatus DmxTakeScheduler::status() const {
  if(file_mode_.load(std::memory_order_acquire)) {
    const auto file = file_player_.status();
    DmxTakeSchedulerStatus result;
    result.running = file.running;
    result.armed = file.armed;
    result.playing = file.transport == DmxClipTransportState::playing;
    result.paused = file.transport == DmxClipTransportState::paused;
    result.ended = file.transport == DmxClipTransportState::ended;
    result.file_backed = true;
    result.hold_valid = file.hold_valid;
    result.host_heartbeat_ok = file.host_heartbeat_ok;
    result.monotonic_clock =
        file.clock_source == DmxClipClockSource::monotonic_realtime;
    result.progress = std::clamp(file.progress, 0.0, 1.0);
    result.range_start_frame = static_cast<std::size_t>(file.range_start_frame);
    result.range_end_frame_exclusive =
        static_cast<std::size_t>(file.range_end_frame_exclusive);
    result.current_frame = static_cast<std::size_t>(file.current_frame);
    result.cursor_samples = file.cursor_samples;
    result.error = file.error;
    if(result.error.empty()) {
      const std::scoped_lock lock(mutex_);
      result.error = error_;
    }
    return result;
  }

  DmxTakeSchedulerStatus result;
  result.running = running_.load(std::memory_order_acquire);
  result.armed = armed_.load(std::memory_order_acquire);
  result.playing = playing_.load(std::memory_order_acquire);
  result.progress = std::clamp(progress_.load(std::memory_order_acquire), 0.0, 1.0);
  result.host_heartbeat_ok = heartbeat_ok_.load(std::memory_order_acquire);
  {
    const std::scoped_lock lock(mutex_);
    result.hold_valid = hold_valid_;
    result.range_start_frame = range_start_frame_;
    result.range_end_frame_exclusive = range_end_frame_exclusive_;
    result.current_frame = current_frame_;
    result.error = error_;
  }
  return result;
}

void DmxTakeScheduler::ensure_thread() {
  if(worker_.joinable()) return;
  stop_requested_.store(false, std::memory_order_release);
  worker_ = std::thread([this]() { run(); });
}

void DmxTakeScheduler::shutdown() noexcept {
  playing_.store(false, std::memory_order_release);
  armed_.store(false, std::memory_order_release);
  stop_requested_.store(true, std::memory_order_release);
  file_player_.disarm();
  if(output_ != nullptr)
    output_->set_override_enabled(false);
  if(worker_.joinable()) worker_.join();
  file_player_.unload();
  running_.store(false, std::memory_order_release);
}

void DmxTakeScheduler::publish_hold_locked() {
  if(output_ == nullptr || !hold_valid_) return;
  output_->publish_override(hold_frame_, generation_++);
}

void DmxTakeScheduler::update_position_locked(
    std::chrono::steady_clock::time_point now) {
  if(take_ == nullptr || take_->frames.empty() || take_->frames_per_second == 0U)
    return;
  if(range_end_frame_exclusive_ <= range_start_frame_ + 1U ||
     range_end_frame_exclusive_ > take_->frames.size())
    return;

  const double elapsed = std::chrono::duration<double>(now - play_started_).count();
  const double rawOffset = std::max(0.0, elapsed) *
                           static_cast<double>(take_->frames_per_second);
  std::size_t offset = static_cast<std::size_t>(rawOffset);
  const std::size_t rangeFrames = range_end_frame_exclusive_ - range_start_frame_;
  if(offset >= rangeFrames) {
    offset = rangeFrames - 1U;
    playing_.store(false, std::memory_order_release);
    progress_.store(1.0, std::memory_order_release);
  } else {
    const double progress = rangeFrames <= 1U
                                ? 1.0
                                : static_cast<double>(offset) /
                                      static_cast<double>(rangeFrames - 1U);
    progress_.store(progress, std::memory_order_release);
  }

  current_frame_ = range_start_frame_ + offset;
  hold_frame_ = take_->frames[current_frame_];
  hold_valid_ = true;
}

void DmxTakeScheduler::run() noexcept {
  running_.store(true, std::memory_order_release);
  using Clock = std::chrono::steady_clock;
  constexpr auto kLoopPeriod = std::chrono::milliseconds(2);
  constexpr auto kHeartbeatTimeout = std::chrono::milliseconds(750);
  std::uint64_t lastRevision = 0U;
  auto lastHeartbeat = Clock::now();

  while(!stop_requested_.load(std::memory_order_acquire)) {
    const auto now = Clock::now();

    bool hostSafe = false;
    runtime::HostTransportSnapshot hostSnapshot{};
    if(host_ != nullptr) {
      hostSnapshot = host_->latest();
      if(hostSnapshot.revision != 0U && hostSnapshot.revision != lastRevision) {
        lastRevision = hostSnapshot.revision;
        lastHeartbeat = now;
      }
      hostSafe = hostSnapshot.revision != 0U && !hostSnapshot.rendering_offline &&
                 now - lastHeartbeat <= kHeartbeatTimeout;
    }
    heartbeat_ok_.store(hostSafe, std::memory_order_release);

    if(file_mode_.load(std::memory_order_acquire)) {
      file_player_.set_host_heartbeat_ok(hostSafe);

      const auto fileStatus = file_player_.status();
      const bool hostOffline = hostSnapshot.revision != 0U &&
                               hostSnapshot.rendering_offline;
      if((hostOffline || (!hostSafe && !file_player_.uses_monotonic_clock())) &&
         fileStatus.armed) {
        file_player_.disarm();
        armed_.store(false, std::memory_order_release);
        const std::scoped_lock lock(mutex_);
        error_ = hostOffline
            ? "Salida DMX desarmada: el host inició renderizado sin conexión"
            : "Salida DMX desarmada automáticamente: se perdió el pulso del host";
      }
      std::this_thread::sleep_for(kLoopPeriod);
      continue;
    }

    if(!hostSafe && armed_.load(std::memory_order_acquire)) {
      armed_.store(false, std::memory_order_release);
      if(output_ != nullptr)
        output_->set_override_enabled(false);
      const std::scoped_lock lock(mutex_);
      error_ = "Salida de toma desarmada automáticamente por seguridad del host";
    }

    {
      const std::scoped_lock lock(mutex_);
      if(playing_.load(std::memory_order_acquire))
        update_position_locked(now);
      if(armed_.load(std::memory_order_acquire) && hold_valid_)
        publish_hold_locked();
    }

    std::this_thread::sleep_for(kLoopPeriod);
  }

  if(output_ != nullptr)
    output_->set_override_enabled(false);
  running_.store(false, std::memory_order_release);
}

}  // namespace aeyla::capture

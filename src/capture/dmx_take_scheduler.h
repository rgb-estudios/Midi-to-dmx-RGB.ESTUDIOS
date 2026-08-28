#pragma once

#include "capture/artnet_capture_worker.h"
#include "capture/dmx_clip_playback_engine.h"
#include "output/artnet_output_worker.h"
#include "runtime/host_transport_mailbox.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace aeyla::capture {

struct DmxTakeSchedulerStatus {
  bool running{false};
  bool armed{false};
  bool playing{false};
  bool paused{false};
  bool ended{false};
  bool file_backed{false};
  bool hold_valid{false};
  bool host_heartbeat_ok{false};
  bool monotonic_clock{false};
  double progress{0.0};
  std::size_t range_start_frame{0U};
  std::size_t range_end_frame_exclusive{0U};
  std::size_t current_frame{0U};
  std::string error;
};

// Puente de compatibilidad del reproductor de tomas.
//
// El camino de producción R07 usa load_take_file(): archivo respaldado en disco,
// cursor relativo por muestras y transporte independiente de la posición global
// del DAW. El camino load_take() en RAM permanece temporalmente para pruebas y
// compatibilidad mientras se retira la arquitectura antigua.
class DmxTakeScheduler final {
 public:
  DmxTakeScheduler();
  ~DmxTakeScheduler();

  DmxTakeScheduler(const DmxTakeScheduler&) = delete;
  DmxTakeScheduler& operator=(const DmxTakeScheduler&) = delete;

  void attach(output::ArtNetOutputWorker* output,
              const runtime::HostTransportMailbox* host) noexcept;

  // Compatibilidad heredada en RAM.
  [[nodiscard]] bool load_take(const DmxTake* take,
                               std::string& error_message);

  // Camino de producto: no carga el payload completo en RAM.
  [[nodiscard]] bool load_take_file(const std::filesystem::path& path,
                                    double sample_rate,
                                    std::string& error_message);

  [[nodiscard]] bool set_play_range(std::size_t start_frame,
                                    std::size_t end_frame_exclusive,
                                    std::string& error_message);
  void reset_play_range() noexcept;

  [[nodiscard]] bool play(
      std::string& error_message,
      DmxClipClockSource clock_source = DmxClipClockSource::host_samples);
  [[nodiscard]] bool pause(std::string& error_message);
  [[nodiscard]] bool resume(std::string& error_message);
  [[nodiscard]] bool seek_frame(std::size_t frame_index,
                                std::string& error_message);
  void stop_hold() noexcept;
  void stop_reset() noexcept;

  // ÚNICA vía de reloj para el modo file-backed. Es segura para el callback de
  // audio: sólo suma muestras a un contador atómico del reproductor relativo.
  // El worker de seguridad observa el heartbeat, pero nunca vuelve a inferir
  // tiempo desde la posición absoluta del Arrangement.
  void advance_samples(std::uint32_t processed_samples,
                       bool rendering_offline) noexcept;

  [[nodiscard]] bool arm(std::string& error_message);
  void disarm() noexcept;

  [[nodiscard]] DmxTakeSchedulerStatus status() const;

 private:
  void ensure_thread();
  void shutdown() noexcept;
  void run() noexcept;
  void publish_hold_locked();
  void update_position_locked(std::chrono::steady_clock::time_point now);

  mutable std::mutex mutex_;
  output::ArtNetOutputWorker* output_{nullptr};
  const runtime::HostTransportMailbox* host_{nullptr};
  const DmxTake* take_{nullptr};
  DmxUniverse hold_frame_{};
  bool hold_valid_{false};
  std::size_t range_start_frame_{0U};
  std::size_t range_end_frame_exclusive_{0U};
  std::size_t current_frame_{0U};
  std::chrono::steady_clock::time_point play_started_{};
  std::uint64_t generation_{2000000000ULL};
  std::string error_;

  DmxClipPlaybackEngine file_player_{};
  std::atomic<bool> file_mode_{false};

  std::thread worker_;
  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> running_{false};
  std::atomic<bool> armed_{false};
  std::atomic<bool> playing_{false};
  std::atomic<double> progress_{0.0};
  std::atomic<bool> heartbeat_ok_{false};
};

}  // namespace aeyla::capture

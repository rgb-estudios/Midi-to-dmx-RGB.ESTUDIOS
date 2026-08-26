#pragma once

#include "capture/dmx_take_file_reader.h"
#include "output/artnet_output_worker.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace aeyla::capture {

enum class DmxClipTransportState : std::uint8_t {
  ready = 0,
  playing,
  paused,
  ended,
  fault,
};

struct DmxClipPlaybackStatus {
  bool running{false};
  bool loaded{false};
  bool armed{false};
  bool hold_valid{false};
  bool host_heartbeat_ok{false};
  bool rendering_offline{false};
  DmxClipTransportState transport{DmxClipTransportState::ready};
  std::uint64_t current_frame{0U};
  std::uint64_t range_start_frame{0U};
  std::uint64_t range_end_frame_exclusive{0U};
  std::uint64_t cursor_samples{0U};
  double progress{0.0};
  std::string error;
};

// Reproductor DMX respaldado por archivo y gobernado por un cursor RELATIVO.
//
// Contrato de producto R07:
// - la posición absoluta del arreglo del DAW NO es el reloj artístico;
// - el DAW entrega comandos MIDI y bloques de muestras procesadas;
// - REPRODUCIR/REINICIAR comienza en cursor 0;
// - PAUSA mantiene cursor + DMX y REANUDAR continúa desde ese punto;
// - advance_samples() es la única vía para avanzar el tiempo artístico;
// - el reloj de pared sólo puede usarse como vigilancia de vida del host.
//
// advance_samples() está diseñado para el callback de audio: sólo opera sobre
// atómicos, no toma mutex, no asigna memoria y no realiza E/S de archivo/red.
class DmxClipPlaybackEngine final {
 public:
  DmxClipPlaybackEngine();
  ~DmxClipPlaybackEngine();

  DmxClipPlaybackEngine(const DmxClipPlaybackEngine&) = delete;
  DmxClipPlaybackEngine& operator=(const DmxClipPlaybackEngine&) = delete;

  void attach(output::ArtNetOutputWorker* output) noexcept;

  [[nodiscard]] bool load_clip(const std::filesystem::path& path,
                               double sample_rate,
                               std::string& error_message);
  void unload() noexcept;

  [[nodiscard]] bool set_play_range(std::uint64_t start_frame,
                                    std::uint64_t end_frame_exclusive,
                                    std::string& error_message);
  void reset_play_range() noexcept;

  [[nodiscard]] bool arm(std::string& error_message);
  void disarm() noexcept;

  // Comandos de transporte. Deben invocarse fuera del callback de audio.
  [[nodiscard]] bool play_from_start(std::string& error_message);
  [[nodiscard]] bool pause(std::string& error_message);
  [[nodiscard]] bool resume(std::string& error_message);
  void stop_and_reset() noexcept;

  // Ruta de tiempo de audio en tiempo real. El integrador debe respetar el
  // sampleOffset de un evento MIDI al decidir cuántas muestras se contabilizan
  // antes/después del comando.
  void advance_samples(std::uint32_t processed_samples,
                       bool rendering_offline) noexcept;

  // El integrador publica la vida del host. Perder heartbeat deshabilita la
  // autoridad física, pero nunca mueve el cursor artístico.
  void set_host_heartbeat_ok(bool ok) noexcept;

  [[nodiscard]] DmxClipPlaybackStatus status() const;

 private:
  void ensure_thread();
  void shutdown() noexcept;
  void run() noexcept;
  void set_error(std::string message) noexcept;
  [[nodiscard]] std::string error() const;
  [[nodiscard]] bool publish_cursor_frame_locked();

  mutable std::mutex mutex_;
  output::ArtNetOutputWorker* output_{nullptr};
  DmxTakeFileReader reader_{};
  double sample_rate_{0.0};
  std::uint64_t range_start_frame_{0U};
  std::uint64_t range_end_frame_exclusive_{0U};
  DmxUniverse hold_frame_{};
  bool hold_valid_{false};
  std::uint64_t generation_{3000000000ULL};

  mutable std::mutex error_mutex_;
  std::string error_;

  std::thread worker_;
  std::atomic<bool> stop_requested_{false};
  std::atomic<bool> running_{false};
  std::atomic<bool> loaded_{false};
  std::atomic<bool> armed_{false};
  std::atomic<bool> heartbeat_ok_{false};
  std::atomic<bool> rendering_offline_{false};
  std::atomic<DmxClipTransportState> transport_{DmxClipTransportState::ready};
  std::atomic<std::uint64_t> cursor_samples_{0U};
  std::atomic<std::uint64_t> current_frame_{0U};
  std::atomic<double> progress_{0.0};
};

}  // namespace aeyla::capture

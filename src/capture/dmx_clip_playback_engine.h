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

enum class DmxClipClockSource : std::uint8_t {
  host_samples = 0,
  monotonic_realtime,
};

struct DmxClipPlaybackStatus {
  bool running{false};
  bool loaded{false};
  bool armed{false};
  bool hold_valid{false};
  bool host_heartbeat_ok{false};
  bool rendering_offline{false};
  DmxClipTransportState transport{DmxClipTransportState::ready};
  DmxClipClockSource clock_source{DmxClipClockSource::host_samples};
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
// - los disparos DAW/MIDI avanzan exclusivamente mediante advance_samples();
// - la reproducción manual de operador puede usar un reloj monotónico propio
//   para no detenerse si el host cierra su dispositivo de audio al perder foco.
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
  [[nodiscard]] bool replace_armed_clip(
      DmxTakeFileReader& validated_reader,
      double sample_rate,
      std::uint64_t range_start_frame,
      std::uint64_t range_end_frame_exclusive,
      DmxClipClockSource clock_source,
      std::uint64_t elapsed_samples,
      std::string& error_message);
  void unload() noexcept;

  [[nodiscard]] bool set_play_range(std::uint64_t start_frame,
                                    std::uint64_t end_frame_exclusive,
                                    std::string& error_message);
  void reset_play_range() noexcept;

  [[nodiscard]] bool arm(std::string& error_message);
  void disarm() noexcept;

  // Comandos de transporte. Deben invocarse fuera del callback de audio.
  [[nodiscard]] bool play_from_start(DmxClipClockSource clock_source,
                                     std::string& error_message,
                                     std::uint64_t elapsed_samples = 0U);
  [[nodiscard]] bool pause(std::string& error_message,
                           std::uint64_t rewind_samples = 0U);
  [[nodiscard]] bool resume(std::string& error_message,
                            std::uint64_t elapsed_samples = 0U);
  // Previsualización segura del editor: lee un cuadro desde disco y mueve el
  // cursor relativo sin publicar Art-Net. Sólo se admite con salida desarmada
  // y transporte detenido.
  [[nodiscard]] bool seek_frame(std::uint64_t frame_index,
                                std::string& error_message);
  // Reset técnico por defecto: vuelve a cero y retira autoridad física. La
  // capa de operador/MIDI puede conservar ARM + carrier de forma explícita.
  void stop_and_reset(bool preserve_armed_authority = false) noexcept;

  // Ruta de tiempo de audio en tiempo real. El integrador debe respetar el
  // sampleOffset de un evento MIDI al decidir cuántas muestras se contabilizan
  // antes/después del comando.
  void advance_samples(std::uint32_t processed_samples,
                       bool rendering_offline) noexcept;
  void synchronize_host_cursor(std::uint64_t cursor_samples) noexcept;

  // El integrador publica la vida del host. Perder heartbeat deshabilita la
  // autoridad física, pero nunca mueve el cursor artístico.
  void set_host_heartbeat_ok(bool ok) noexcept;

  [[nodiscard]] DmxClipPlaybackStatus status() const;
  [[nodiscard]] bool uses_monotonic_clock() const noexcept;

 private:
  void ensure_thread();
  void shutdown() noexcept;
  void run() noexcept;
  void set_error(std::string message) noexcept;
  [[nodiscard]] std::string error() const;
  [[nodiscard]] bool publish_cursor_frame_locked();
  void update_monotonic_cursor_locked(
      std::chrono::steady_clock::time_point now) noexcept;

  mutable std::mutex mutex_;
  output::ArtNetOutputWorker* output_{nullptr};
  DmxTakeFileReader reader_{};
  double sample_rate_{0.0};
  std::uint64_t range_start_frame_{0U};
  std::uint64_t range_end_frame_exclusive_{0U};
  DmxUniverse hold_frame_{};
  bool hold_valid_{false};
  std::uint64_t generation_{3000000000ULL};
  std::chrono::steady_clock::time_point monotonic_anchor_time_{};
  std::uint64_t monotonic_anchor_samples_{0U};

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
  std::atomic<DmxClipClockSource> clock_source_{DmxClipClockSource::host_samples};
  std::atomic<std::uint64_t> cursor_samples_{0U};
  std::atomic<std::uint64_t> current_frame_{0U};
  std::atomic<double> progress_{0.0};
};

}  // namespace aeyla::capture

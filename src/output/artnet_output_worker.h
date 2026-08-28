#pragma once

#include "core/dmx_compiler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace aeyla::output {

inline constexpr std::uint16_t kAeylaArtNetFramesPerSecond = 44U;
inline constexpr std::uint32_t kAeylaArtNetBlackoutBurstFrames = 3U;
inline constexpr std::uint32_t kAeylaArtNetFailClosedErrorThreshold = 3U;

struct ArtNetOutputConfig {
  // IPv4 local numérica opcional. AEYLA la fija explícitamente cuando el
  // operador selecciona la interfaz TX. Si está vacía, decide el sistema.
  std::string source_ipv4;
  std::string target_ipv4;
  std::uint16_t udp_port{6454U};
  std::uint16_t port_address{0U};
  std::uint16_t channel_count{512U};
  // Contrato de show R07: captura, reproducción y TX trabajan a 44 Hz.
  std::uint16_t frames_per_second{kAeylaArtNetFramesPerSecond};
};

struct ArtNetOutputStats {
  bool running{false};
  bool enabled{false};
  bool override_enabled{false};
  bool fail_closed{false};
  std::uint16_t configured_fps{0U};
  std::uint64_t published_generation{0U};
  std::uint64_t last_sent_generation{0U};
  std::uint64_t sent_packets{0U};
  std::uint64_t blackout_packets{0U};
  std::uint64_t send_errors{0U};
  std::uint64_t consecutive_send_errors{0U};
  std::uint64_t timing_misses{0U};
  std::uint64_t fail_closed_events{0U};
  std::uint64_t stale_publish_drops{0U};
};

[[nodiscard]] bool validate_artnet_output_config(
    const ArtNetOutputConfig& config, std::string& error_message) noexcept;

// Transporte Art-Net dedicado y no realtime.
//
// Existen dos autoridades mutuamente priorizadas:
// 1) salida normal del modelo (`publish_latest` + `set_enabled`), y
// 2) reproducción de clip/toma (`publish_override` + `set_override_enabled`).
//
// La reproducción tiene prioridad mientras está habilitada. Si ninguna
// autoridad queda activa se transmite una ráfaga corta de BLACKOUT. Tres
// errores de envío consecutivos provocan fail-closed: ambas autoridades quedan
// deshabilitadas y el rearme debe ser explícito.
class ArtNetOutputWorker final {
 public:
  ArtNetOutputWorker();
  ~ArtNetOutputWorker();

  ArtNetOutputWorker(const ArtNetOutputWorker&) = delete;
  ArtNetOutputWorker& operator=(const ArtNetOutputWorker&) = delete;

  // Se usa antes de start() cuando Routing selecciona una interfaz TX física.
  void set_preferred_source_ipv4(std::string source_ipv4);

  [[nodiscard]] bool start(const ArtNetOutputConfig& config,
                           std::string& error_message);
  void stop() noexcept;

  void publish_latest(const DmxUniverse& universe, std::uint64_t generation);
  // Única vía para limpiar un fail-closed. Debe invocarse exclusivamente desde
  // una acción de ARMAR explícita del operador; los ticks periódicos no pueden
  // recuperar autoridad por sí solos.
  void prepare_explicit_rearm() noexcept;
  void set_enabled(bool enabled) noexcept;

  void publish_override(const DmxUniverse& universe, std::uint64_t generation);
  void set_override_enabled(bool enabled) noexcept;
  [[nodiscard]] bool override_enabled() const noexcept;

  [[nodiscard]] ArtNetOutputStats stats() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace aeyla::output

#pragma once

#include "core/dmx_compiler.h"

#include <cstdint>
#include <memory>
#include <string>

namespace aeyla::output {

struct ArtNetOutputConfig {
  // Optional numeric local IPv4 bind. Empty means let the OS route normally.
  // AEYLA Show Player sets this explicitly when the operator selects a TX NIC.
  std::string source_ipv4;
  std::string target_ipv4;
  std::uint16_t udp_port{6454U};
  std::uint16_t port_address{0U};
  std::uint16_t channel_count{512U};
  std::uint16_t frames_per_second{30U};
};

struct ArtNetOutputStats {
  bool running{false};
  bool enabled{false};
  bool override_enabled{false};
  std::uint64_t published_generation{0U};
  std::uint64_t last_sent_generation{0U};
  std::uint64_t sent_packets{0U};
  std::uint64_t blackout_packets{0U};
  std::uint64_t send_errors{0U};
  std::uint64_t stale_publish_drops{0U};
};

[[nodiscard]] bool validate_artnet_output_config(
    const ArtNetOutputConfig& config, std::string& error_message) noexcept;

// Dedicated non-realtime Art-Net transport.
//
// There are two mutually prioritised frame authorities:
// 1) normal semantic/model output (`publish_latest` + `set_enabled`), and
// 2) captured Take output (`publish_override` + `set_override_enabled`).
//
// Take override wins while enabled. The ordinary runtime can continue to
// publish model frames without corrupting a replay. Disabling the override
// returns authority to the model; if neither authority is enabled, the worker
// sends one explicit zero-DMX safety packet.
class ArtNetOutputWorker final {
 public:
  ArtNetOutputWorker();
  ~ArtNetOutputWorker();

  ArtNetOutputWorker(const ArtNetOutputWorker&) = delete;
  ArtNetOutputWorker& operator=(const ArtNetOutputWorker&) = delete;

  // Used before start() when Routing selects an explicit TX NIC. A start config
  // with an empty source_ipv4 inherits this preferred source.
  void set_preferred_source_ipv4(std::string source_ipv4);

  [[nodiscard]] bool start(const ArtNetOutputConfig& config,
                           std::string& error_message);
  void stop() noexcept;

  void publish_latest(const DmxUniverse& universe, std::uint64_t generation);
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

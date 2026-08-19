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
  // Alpha v1 deliberately accepts a numeric IPv4 address only. Avoiding DNS in
  // the runtime removes a blocking/failure-prone dependency from show output.
  std::string target_ipv4;
  std::uint16_t udp_port{6454U};
  std::uint16_t port_address{0U};
  std::uint16_t channel_count{512U};
  std::uint16_t frames_per_second{30U};
};

struct ArtNetOutputStats {
  bool running{false};
  bool enabled{false};
  std::uint64_t published_generation{0U};
  std::uint64_t last_sent_generation{0U};
  std::uint64_t sent_packets{0U};
  std::uint64_t blackout_packets{0U};
  std::uint64_t send_errors{0U};
  std::uint64_t stale_publish_drops{0U};
};

// Pure, non-network preflight used before changing persisted output settings.
// Alpha v1 accepts only explicit numeric unicast IPv4 targets: no DNS,
// multicast or implicit destination discovery. source_ipv4 may be empty for OS
// routing or a numeric local IPv4 when an explicit TX adapter is selected.
[[nodiscard]] bool validate_artnet_output_config(
    const ArtNetOutputConfig& config, std::string& error_message) noexcept;

// Dedicated non-realtime Art-Net transport.
//
// Contract:
// - never call start/stop from an audio callback;
// - publish_latest replaces one mailbox frame; it never queues history;
// - while enabled the worker refreshes the latest frame at fixed FPS;
// - disabling requests one zero-DMX ArtDMX packet and then stops refreshing;
// - start begins disabled; a caller must explicitly enable output;
// - no DNS, discovery, filesystem or UI work occurs in the worker;
// - source_ipv4 binds the UDP socket to one selected local NIC when supplied.
class ArtNetOutputWorker final {
 public:
  ArtNetOutputWorker();
  ~ArtNetOutputWorker();

  ArtNetOutputWorker(const ArtNetOutputWorker&) = delete;
  ArtNetOutputWorker& operator=(const ArtNetOutputWorker&) = delete;

  [[nodiscard]] bool start(const ArtNetOutputConfig& config,
                           std::string& error_message);
  void stop() noexcept;

  void publish_latest(const DmxUniverse& universe, std::uint64_t generation);
  void set_enabled(bool enabled) noexcept;

  [[nodiscard]] ArtNetOutputStats stats() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace aeyla::output

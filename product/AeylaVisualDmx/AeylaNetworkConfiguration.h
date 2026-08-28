#pragma once

#include "network/network_interfaces.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

enum class AeylaNetworkConfigurationState {
  idle,
  awaiting_permission,
  applying,
  committed,
  rolled_back,
  failed,
};

struct AeylaNetworkConfigurationSnapshot final {
  AeylaNetworkConfigurationState state{AeylaNetworkConfigurationState::idle};
  std::uint64_t revision{0U};
  std::string ipv4;
  std::uint8_t prefix_length{0U};
  std::string message;

  [[nodiscard]] bool busy() const noexcept {
    return state == AeylaNetworkConfigurationState::awaiting_permission ||
           state == AeylaNetworkConfigurationState::applying;
  }
};

// Thin host/platform adapter for privileged network changes. The DAW and the
// plug-in remain at normal user privilege; only the separately-built helper is
// elevated for one bounded request.
class AeylaNetworkConfiguration final {
public:
  AeylaNetworkConfiguration() = default;
  ~AeylaNetworkConfiguration();

  AeylaNetworkConfiguration(const AeylaNetworkConfiguration&) = delete;
  AeylaNetworkConfiguration& operator=(const AeylaNetworkConfiguration&) = delete;

  [[nodiscard]] bool Start(
      const aeyla::network::NetworkInterface& adapter,
      std::string ipv4,
      std::uint8_t prefix_length,
      std::string& error_message);

  [[nodiscard]] AeylaNetworkConfigurationSnapshot Snapshot() const;
  void Shutdown() noexcept;

private:
  void Run(aeyla::network::NetworkInterface adapter,
           std::string ipv4,
           std::uint8_t prefix_length) noexcept;
  void Publish(AeylaNetworkConfigurationState state,
               std::string ipv4,
               std::uint8_t prefix_length,
               std::string message) noexcept;

  mutable std::mutex mMutex;
  AeylaNetworkConfigurationSnapshot mSnapshot;
  std::thread mWorker;
  std::atomic<bool> mBusy{false};
};

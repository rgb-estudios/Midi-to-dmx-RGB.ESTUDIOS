#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aeyla::network {

struct NetworkInterface {
  std::string id;
  std::string name;
  std::string ipv4;
  std::uint8_t prefix_length{0U};
  bool loopback{false};

  bool operator==(const NetworkInterface&) const = default;
};

// Enumerates currently-up IPv4 interfaces. Loopback is returned last so it
// remains available for controlled local tests without becoming the default
// show route when a physical adapter exists.
[[nodiscard]] std::vector<NetworkInterface> enumerate_ipv4_interfaces();

}  // namespace aeyla::network

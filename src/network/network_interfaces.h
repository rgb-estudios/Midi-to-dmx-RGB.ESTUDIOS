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
  std::uint32_t interface_index{0U};
  bool wireless{false};

  bool operator==(const NetworkInterface&) const = default;
};

// Enumerates currently-up interfaces. A physical adapter without an assigned
// IPv4 remains visible with an empty `ipv4`, allowing the privileged AEYLA
// helper to add a show-network address without first leaving the product.
// Loopback is excluded from operator-facing results.
[[nodiscard]] std::vector<NetworkInterface> enumerate_ipv4_interfaces();

}  // namespace aeyla::network

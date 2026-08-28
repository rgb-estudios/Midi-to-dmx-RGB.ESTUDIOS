#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace aeyla::network {

struct Ipv4Network final {
  std::string address;
  std::uint32_t address_value{0U};
  std::uint32_t mask_value{0U};
  std::uint8_t prefix_length{0U};
  std::string mask;
  std::string directed_broadcast;
};

// Parses one strict dotted-decimal IPv4 address. Whitespace, abbreviated
// octets, signs and trailing characters are rejected.
[[nodiscard]] bool parse_ipv4(std::string_view text,
                              std::uint32_t& value) noexcept;

[[nodiscard]] std::string format_ipv4(std::uint32_t value);

[[nodiscard]] std::uint32_t mask_from_prefix(
    std::uint8_t prefix_length) noexcept;

[[nodiscard]] std::optional<std::uint8_t> prefix_from_mask(
    std::uint32_t mask) noexcept;

[[nodiscard]] bool is_usable_local_ipv4(std::uint32_t value) noexcept;

[[nodiscard]] std::optional<Ipv4Network> make_ipv4_network(
    std::string_view address,
    std::string_view mask,
    std::string& error_message);

[[nodiscard]] std::optional<Ipv4Network> make_ipv4_network(
    std::string_view address,
    std::uint8_t prefix_length,
    std::string& error_message);

}  // namespace aeyla::network

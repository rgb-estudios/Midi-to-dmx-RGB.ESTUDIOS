#include "network/ipv4_configuration.h"

#include <charconv>

namespace aeyla::network {

bool parse_ipv4(std::string_view text, std::uint32_t& value) noexcept {
  value = 0U;
  std::size_t begin = 0U;
  for(int index = 0; index < 4; ++index) {
    const std::size_t end = index == 3 ? text.size() : text.find('.', begin);
    if(end == std::string_view::npos || end == begin) return false;

    unsigned octet = 0U;
    const auto parsed = std::from_chars(text.data() + begin,
                                        text.data() + end, octet);
    if(parsed.ec != std::errc{} || parsed.ptr != text.data() + end ||
       octet > 255U)
      return false;

    value = (value << 8U) | octet;
    begin = end + 1U;
  }
  return begin == text.size() + 1U;
}

std::string format_ipv4(std::uint32_t value) {
  return std::to_string((value >> 24U) & 0xFFU) + "." +
         std::to_string((value >> 16U) & 0xFFU) + "." +
         std::to_string((value >> 8U) & 0xFFU) + "." +
         std::to_string(value & 0xFFU);
}

std::uint32_t mask_from_prefix(std::uint8_t prefix_length) noexcept {
  if(prefix_length == 0U) return 0U;
  if(prefix_length >= 32U) return 0xFFFFFFFFU;
  return static_cast<std::uint32_t>(
      0xFFFFFFFFULL << (32U - prefix_length));
}

std::optional<std::uint8_t> prefix_from_mask(std::uint32_t mask) noexcept {
  if(mask == 0U) return std::nullopt;
  const std::uint32_t inverted = ~mask;
  if((inverted & (inverted + 1U)) != 0U) return std::nullopt;

  std::uint8_t prefix = 0U;
  while((mask & 0x80000000U) != 0U) {
    ++prefix;
    mask <<= 1U;
  }
  return prefix;
}

bool is_usable_local_ipv4(std::uint32_t value) noexcept {
  const std::uint8_t first = static_cast<std::uint8_t>(value >> 24U);
  if(value == 0U || value == 0xFFFFFFFFU) return false;
  if(first == 0U || first == 127U || first >= 224U) return false;
  return true;
}

std::optional<Ipv4Network> make_ipv4_network(
    std::string_view address,
    std::string_view mask,
    std::string& error_message) {
  error_message.clear();
  std::uint32_t address_value = 0U;
  std::uint32_t mask_value = 0U;
  if(!parse_ipv4(address, address_value)) {
    error_message = "La dirección IPv4 local no es válida";
    return std::nullopt;
  }
  if(!is_usable_local_ipv4(address_value)) {
    error_message = "La dirección IPv4 local no puede usarse para Art-Net";
    return std::nullopt;
  }
  if(!parse_ipv4(mask, mask_value)) {
    error_message = "La máscara de subred no es válida";
    return std::nullopt;
  }
  const auto prefix = prefix_from_mask(mask_value);
  if(!prefix.has_value()) {
    error_message = "La máscara de subred debe ser contigua y distinta de cero";
    return std::nullopt;
  }
  return make_ipv4_network(address, *prefix, error_message);
}

std::optional<Ipv4Network> make_ipv4_network(
    std::string_view address,
    std::uint8_t prefix_length,
    std::string& error_message) {
  error_message.clear();
  if(prefix_length == 0U || prefix_length > 32U) {
    error_message = "El prefijo IPv4 debe estar entre /1 y /32";
    return std::nullopt;
  }

  std::uint32_t address_value = 0U;
  if(!parse_ipv4(address, address_value) ||
     !is_usable_local_ipv4(address_value)) {
    error_message = "La dirección IPv4 local no es válida para Art-Net";
    return std::nullopt;
  }

  const std::uint32_t mask_value = mask_from_prefix(prefix_length);
  const std::uint32_t network = address_value & mask_value;
  const std::uint32_t broadcast = network | ~mask_value;
  if(address_value == network || address_value == broadcast ||
     broadcast == 0xFFFFFFFFU) {
    error_message =
        "La dirección elegida no puede ser la red ni el broadcast de la subred";
    return std::nullopt;
  }

  return Ipv4Network{std::string(address), address_value, mask_value,
                     prefix_length, format_ipv4(mask_value),
                     format_ipv4(broadcast)};
}

}  // namespace aeyla::network

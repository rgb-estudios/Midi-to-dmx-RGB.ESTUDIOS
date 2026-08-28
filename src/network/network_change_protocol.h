#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace aeyla::network {

inline constexpr std::string_view kNetworkChangeRequestHeader =
    "AEYLA_NETWORK_REQUEST_V1";
inline constexpr std::string_view kNetworkChangeResultHeader =
    "AEYLA_NETWORK_RESULT_V1";

struct NetworkChangeRequest final {
  std::uint32_t adapter_index{0U};
  std::string adapter_id;
  std::string ipv4;
  std::uint8_t prefix_length{0U};
  std::string nonce;
};

enum class NetworkChangeResultState {
  committed,
  rolled_back,
  failed,
};

struct NetworkChangeResult final {
  NetworkChangeResultState state{NetworkChangeResultState::failed};
  std::string ipv4;
  std::uint8_t prefix_length{0U};
  std::string nonce;
  std::string message;
};

[[nodiscard]] std::string encode_network_change_request(
    const NetworkChangeRequest& request);

[[nodiscard]] std::optional<NetworkChangeRequest>
decode_network_change_request(std::string_view text,
                              std::string& error_message);

[[nodiscard]] std::string encode_network_change_result(
    const NetworkChangeResult& result);

[[nodiscard]] std::optional<NetworkChangeResult>
decode_network_change_result(std::string_view text,
                             std::string& error_message);

}  // namespace aeyla::network

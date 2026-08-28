#include "network/network_change_protocol.h"

#include "network/ipv4_configuration.h"

#include <array>
#include <charconv>
#include <map>

namespace aeyla::network {
namespace {

constexpr std::size_t kMaximumDocumentBytes = 4096U;

bool safe_scalar(std::string_view value, std::size_t maximum) noexcept {
  if(value.empty() || value.size() > maximum) return false;
  for(const char character : value) {
    const auto byte = static_cast<unsigned char>(character);
    if(byte < 0x20U || character == '=' || character == '\r' ||
       character == '\n')
      return false;
  }
  return true;
}

bool safe_nonce(std::string_view nonce) noexcept {
  if(nonce.size() != 32U) return false;
  for(const char character : nonce) {
    const bool digit = character >= '0' && character <= '9';
    const bool lower = character >= 'a' && character <= 'f';
    if(!digit && !lower) return false;
  }
  return true;
}

std::optional<std::map<std::string, std::string>> parse_document(
    std::string_view text,
    std::string_view expected_header,
    std::string& error_message) {
  error_message.clear();
  if(text.empty() || text.size() > kMaximumDocumentBytes) {
    error_message = "El documento de cambio de red supera el tamaño permitido";
    return std::nullopt;
  }

  const auto first_end = text.find('\n');
  const std::string_view header = text.substr(0U, first_end);
  if(header != expected_header) {
    error_message = "La versión del documento de red no es compatible";
    return std::nullopt;
  }

  std::map<std::string, std::string> fields;
  std::size_t cursor = first_end == std::string_view::npos
                           ? text.size()
                           : first_end + 1U;
  while(cursor < text.size()) {
    const auto line_end = text.find('\n', cursor);
    std::string_view line = text.substr(
        cursor, line_end == std::string_view::npos
                    ? text.size() - cursor
                    : line_end - cursor);
    if(!line.empty() && line.back() == '\r') line.remove_suffix(1U);
    cursor = line_end == std::string_view::npos ? text.size() : line_end + 1U;
    if(line.empty()) continue;

    const auto separator = line.find('=');
    if(separator == std::string_view::npos || separator == 0U) {
      error_message = "El documento de red contiene una línea inválida";
      return std::nullopt;
    }
    std::string key(line.substr(0U, separator));
    std::string value(line.substr(separator + 1U));
    if(!safe_scalar(key, 48U) || !safe_scalar(value, 1024U) ||
       !fields.emplace(std::move(key), std::move(value)).second) {
      error_message = "El documento de red contiene campos inválidos o repetidos";
      return std::nullopt;
    }
  }
  return fields;
}

template <typename Integer>
bool parse_integer(std::string_view text, Integer& value) noexcept {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

std::string sanitize_message(std::string_view message) {
  std::string result;
  result.reserve(std::min<std::size_t>(message.size(), 1000U));
  for(const char character : message) {
    if(result.size() >= 1000U) break;
    if(character == '\r' || character == '\n' || character == '=')
      result.push_back(' ');
    else if(static_cast<unsigned char>(character) >= 0x20U)
      result.push_back(character);
  }
  if(result.empty()) result = "Sin detalle";
  return result;
}

}  // namespace

std::string encode_network_change_request(
    const NetworkChangeRequest& request) {
  return std::string(kNetworkChangeRequestHeader) + "\n" +
         "adapter_index=" + std::to_string(request.adapter_index) + "\n" +
         "adapter_id=" + request.adapter_id + "\n" +
         "ipv4=" + request.ipv4 + "\n" +
         "prefix=" + std::to_string(request.prefix_length) + "\n" +
         "nonce=" + request.nonce + "\n";
}

std::optional<NetworkChangeRequest> decode_network_change_request(
    std::string_view text,
    std::string& error_message) {
  const auto fields = parse_document(text, kNetworkChangeRequestHeader,
                                     error_message);
  if(!fields.has_value()) return std::nullopt;
  static constexpr std::array<std::string_view, 5> required{
      "adapter_index", "adapter_id", "ipv4", "prefix", "nonce"};
  if(fields->size() != required.size()) {
    error_message = "El documento de red contiene campos desconocidos";
    return std::nullopt;
  }
  for(const auto key : required) {
    if(!fields->contains(std::string(key))) {
      error_message = "El documento de red está incompleto";
      return std::nullopt;
    }
  }

  NetworkChangeRequest request;
  unsigned prefix = 0U;
  if(!parse_integer(fields->at("adapter_index"), request.adapter_index) ||
     request.adapter_index == 0U ||
     !parse_integer(fields->at("prefix"), prefix) || prefix == 0U ||
     prefix > 32U) {
    error_message = "El índice o prefijo del adaptador no es válido";
    return std::nullopt;
  }
  request.prefix_length = static_cast<std::uint8_t>(prefix);
  request.adapter_id = fields->at("adapter_id");
  request.ipv4 = fields->at("ipv4");
  request.nonce = fields->at("nonce");
  if(!safe_scalar(request.adapter_id, 256U) ||
     !safe_nonce(request.nonce)) {
    error_message = "La identidad de la solicitud de red no es válida";
    return std::nullopt;
  }
  std::string network_error;
  if(!make_ipv4_network(request.ipv4, request.prefix_length,
                        network_error).has_value()) {
    error_message = std::move(network_error);
    return std::nullopt;
  }
  return request;
}

std::string encode_network_change_result(const NetworkChangeResult& result) {
  const char* state = result.state == NetworkChangeResultState::committed
                          ? "committed"
                          : result.state == NetworkChangeResultState::rolled_back
                                ? "rolled_back"
                                : "failed";
  return std::string(kNetworkChangeResultHeader) + "\n" +
         "state=" + state + "\n" +
         "ipv4=" + result.ipv4 + "\n" +
         "prefix=" + std::to_string(result.prefix_length) + "\n" +
         "nonce=" + result.nonce + "\n" +
         "message=" + sanitize_message(result.message) + "\n";
}

std::optional<NetworkChangeResult> decode_network_change_result(
    std::string_view text,
    std::string& error_message) {
  const auto fields = parse_document(text, kNetworkChangeResultHeader,
                                     error_message);
  if(!fields.has_value()) return std::nullopt;
  if(fields->size() != 5U || !fields->contains("state") ||
     !fields->contains("ipv4") || !fields->contains("prefix") ||
     !fields->contains("nonce") || !fields->contains("message")) {
    error_message = "El resultado del helper de red está incompleto";
    return std::nullopt;
  }

  NetworkChangeResult result;
  const auto& state = fields->at("state");
  if(state == "committed")
    result.state = NetworkChangeResultState::committed;
  else if(state == "rolled_back")
    result.state = NetworkChangeResultState::rolled_back;
  else if(state == "failed")
    result.state = NetworkChangeResultState::failed;
  else {
    error_message = "El helper devolvió un estado desconocido";
    return std::nullopt;
  }

  unsigned prefix = 0U;
  if(!parse_integer(fields->at("prefix"), prefix) || prefix > 32U) {
    error_message = "El helper devolvió un prefijo inválido";
    return std::nullopt;
  }
  result.prefix_length = static_cast<std::uint8_t>(prefix);
  result.ipv4 = fields->at("ipv4");
  result.nonce = fields->at("nonce");
  result.message = fields->at("message");
  if(!safe_nonce(result.nonce)) {
    error_message = "El helper devolvió una identidad de transacción inválida";
    return std::nullopt;
  }
  if(!result.ipv4.empty()) {
    std::uint32_t ignored = 0U;
    if(!parse_ipv4(result.ipv4, ignored)) {
      error_message = "El helper devolvió una IPv4 inválida";
      return std::nullopt;
    }
  }
  return result;
}

}  // namespace aeyla::network

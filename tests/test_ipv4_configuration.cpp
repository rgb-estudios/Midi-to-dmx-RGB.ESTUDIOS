#include "network/ipv4_configuration.h"
#include "network/network_change_protocol.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
  if(!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  using namespace aeyla::network;

  std::string error;
  const auto network = make_ipv4_network(
      "2.0.0.20", "255.0.0.0", error);
  require(network.has_value(), error);
  require(network->prefix_length == 8U &&
              network->directed_broadcast == "2.255.255.255",
          "Art-Net /8 network was not derived correctly");

  require(!make_ipv4_network("2.0.0.0", "255.0.0.0", error).has_value(),
          "network address was accepted as a local host");
  require(!make_ipv4_network("2.255.255.255", "255.0.0.0", error).has_value(),
          "broadcast address was accepted as a local host");
  require(!make_ipv4_network("2.0.0.20", "255.0.255.0", error).has_value(),
          "non-contiguous mask was accepted");
  require(!make_ipv4_network("127.0.0.2", 8U, error).has_value(),
          "loopback address was accepted for a physical show route");

  const NetworkChangeRequest request{
      12U, "{12345678-1234-1234-1234-123456789abc}",
      "2.0.0.20", 8U, "00112233445566778899aabbccddeeff"};
  const auto decoded = decode_network_change_request(
      encode_network_change_request(request), error);
  require(decoded.has_value(), error);
  require(decoded->adapter_index == request.adapter_index &&
              decoded->adapter_id == request.adapter_id &&
              decoded->ipv4 == request.ipv4 &&
              decoded->prefix_length == request.prefix_length &&
              decoded->nonce == request.nonce,
          "network change request did not round-trip exactly");

  const NetworkChangeResult committed{
      NetworkChangeResultState::committed, "2.0.0.20", 8U,
      "00112233445566778899aabbccddeeff",
      "IPv4 AEYLA aplicada"};
  const auto decoded_result = decode_network_change_result(
      encode_network_change_result(committed), error);
  require(decoded_result.has_value(), error);
  require(decoded_result->state == NetworkChangeResultState::committed &&
              decoded_result->nonce == committed.nonce &&
              decoded_result->message == committed.message,
          "network helper result did not round-trip");

  std::cout << "AEYLA IPv4 configuration PASS: validate + protocol\n";
  return EXIT_SUCCESS;
}

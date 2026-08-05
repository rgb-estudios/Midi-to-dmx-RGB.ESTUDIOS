#include "runtime/plugin_state.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

using aeyla::runtime::PluginComponentState;
using aeyla::runtime::PluginStateError;
using aeyla::runtime::ProjectLocatorMode;
}  // namespace

int main() {
  PluginComponentState state;
  for (std::size_t i = 0; i < state.project_uuid.size(); ++i) {
    state.project_uuid[i] = static_cast<std::uint8_t>(i + 1);
  }
  for (std::size_t i = 0; i < state.project_checksum.size(); ++i) {
    state.project_checksum[i] = static_cast<std::uint8_t>(255U - i);
  }
  state.project_schema_major = 1;
  state.project_schema_minor = 4;
  state.grand_master = 0.625F;
  state.blackout = true;
  state.locator_mode = ProjectLocatorMode::relative_companion;
  state.project_locator = "show/AEYLA_SHOW.aeylashow";

  const auto encoded = aeyla::runtime::encode_plugin_component_state(state);
  check(encoded.ok(), "valid state should encode");
  check(!encoded.bytes.empty(), "encoded state should not be empty");

  const auto decoded = aeyla::runtime::decode_plugin_component_state(encoded.bytes);
  check(decoded.ok(), "valid encoded state should decode");
  check(decoded.state == state, "state should survive exact round trip");
  check(decoded.state.blackout, "blackout should persist as safe state");

  // Every truncated prefix must fail; no partial state may be accepted.
  for (std::size_t size = 0; size < encoded.bytes.size(); ++size) {
    const auto truncated = aeyla::runtime::decode_plugin_component_state(
        std::span<const std::uint8_t>(encoded.bytes.data(), size));
    check(!truncated.ok(), "truncated prefix must fail");
  }

  // Corrupted magic.
  {
    auto bytes = encoded.bytes;
    bytes[0] ^= 0xFFU;
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    check(result.error == PluginStateError::bad_magic, "bad magic must be rejected");
  }

  // Unsupported major version (little endian at bytes 8..9).
  {
    auto bytes = encoded.bytes;
    bytes[8] = 2;
    bytes[9] = 0;
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    check(result.error == PluginStateError::unsupported_major_version,
          "unsupported major version must be rejected");
  }

  // Payload larger than actual bytes.
  {
    auto bytes = encoded.bytes;
    bytes[12] = 0xFFU;
    bytes[13] = 0xFFU;
    bytes[14] = 0;
    bytes[15] = 0;
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    check(result.error == PluginStateError::invalid_payload_size,
          "oversized payload must be rejected");
  }

  // Unsafe relative locators must not encode.
  {
    auto invalid = state;
    invalid.project_locator = "../outside.aeylashow";
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::unsafe_relative_locator,
          "relative parent traversal must be rejected");
  }
  {
    auto invalid = state;
    invalid.project_locator = "C:\\absolute\\show.aeylashow";
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::unsafe_relative_locator,
          "Windows absolute path must be rejected in relative mode");
  }
  {
    auto invalid = state;
    invalid.project_locator = std::string("show/ok", 7) + '\0' + "hidden.aeylashow";
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::invalid_argument,
          "embedded NUL in locator must be rejected");
  }

  // Locator/mode consistency.
  {
    auto invalid = state;
    invalid.locator_mode = ProjectLocatorMode::none;
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::inconsistent_locator,
          "none mode with locator must be rejected");
  }

  // Invalid master values must be rejected, never silently clamped.
  for (float value : {-0.01F, 1.01F, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()}) {
    auto invalid = state;
    invalid.grand_master = value;
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::invalid_grand_master,
          "invalid grand master must be rejected");
  }

  // Oversized locators must be rejected before allocation-heavy serialization.
  {
    auto invalid = state;
    invalid.project_locator.assign(aeyla::runtime::kMaxProjectLocatorBytes + 1, 'a');
    const auto result = aeyla::runtime::encode_plugin_component_state(invalid);
    check(result.error == PluginStateError::locator_too_large,
          "oversized locator must be rejected");
  }

  // Same-major future minor payloads may append fields and remain readable.
  {
    auto bytes = encoded.bytes;
    bytes[10] = 1;  // format minor = 1
    bytes[11] = 0;
    std::uint32_t payload_size = static_cast<std::uint32_t>(bytes[12]) |
                                 (static_cast<std::uint32_t>(bytes[13]) << 8U) |
                                 (static_cast<std::uint32_t>(bytes[14]) << 16U) |
                                 (static_cast<std::uint32_t>(bytes[15]) << 24U);
    payload_size += 4;
    bytes[12] = static_cast<std::uint8_t>(payload_size & 0xFFU);
    bytes[13] = static_cast<std::uint8_t>((payload_size >> 8U) & 0xFFU);
    bytes[14] = static_cast<std::uint8_t>((payload_size >> 16U) & 0xFFU);
    bytes[15] = static_cast<std::uint8_t>((payload_size >> 24U) & 0xFFU);
    bytes.insert(bytes.end(), {0x12U, 0x34U, 0x56U, 0x78U});
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    check(result.ok() && result.state == state,
          "same-major future minor state with appended fields should remain readable");
  }

  // Unknown locator mode must be rejected after decoding.
  {
    auto bytes = encoded.bytes;
    bytes[25] = 0xFFU;
    const auto result = aeyla::runtime::decode_plugin_component_state(bytes);
    check(result.error == PluginStateError::invalid_locator_mode,
          "unknown locator mode must be rejected");
  }

  // Deterministic malformed-input sweep: decoder must never throw or crash.
  {
    std::mt19937 generator(0xAE71A5U);
    std::uniform_int_distribution<int> length_distribution(0, 1024);
    std::uniform_int_distribution<int> byte_distribution(0, 255);
    for (int case_index = 0; case_index < 10000; ++case_index) {
      std::vector<std::uint8_t> bytes(
          static_cast<std::size_t>(length_distribution(generator)));
      for (auto& byte : bytes) {
        byte = static_cast<std::uint8_t>(byte_distribution(generator));
      }
      (void)aeyla::runtime::decode_plugin_component_state(bytes);
    }
    check(true, "malformed-input sweep completed");
  }

  if (failures == 0) {
    std::cout << "All plugin state tests passed.\n";
    return EXIT_SUCCESS;
  }

  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}

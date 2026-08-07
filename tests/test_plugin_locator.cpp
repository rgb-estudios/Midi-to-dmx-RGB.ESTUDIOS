#include "runtime/plugin_state.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

aeyla::runtime::PluginComponentState base_state() {
  aeyla::runtime::PluginComponentState state;
  for (std::size_t index = 0U; index < state.project_uuid.size(); ++index)
    state.project_uuid[index] = static_cast<std::uint8_t>(index + 1U);
  state.project_uuid[6] = static_cast<std::uint8_t>(
      (state.project_uuid[6] & 0x0FU) | 0x40U);
  state.project_uuid[8] = static_cast<std::uint8_t>(
      (state.project_uuid[8] & 0x3FU) | 0x80U);
  state.project_schema_major = 1U;
  state.project_schema_minor = 0U;
  state.grand_master = 0.73F;
  state.blackout = true;
  return state;
}
}  // namespace

int main() {
  using namespace aeyla::runtime;

  auto absolute = base_state();
  absolute.locator_mode = ProjectLocatorMode::absolute_user_confirmed;
  absolute.project_locator =
      (std::filesystem::temp_directory_path() /
       "AEYLA" / "locator-roundtrip.aeylashow").string();

  const auto encoded = encode_plugin_component_state(absolute);
  check(encoded.ok(),
        "native absolute project locator must encode on the current platform");
  if (encoded.ok()) {
    const auto decoded = decode_plugin_component_state(encoded.bytes);
    check(decoded.ok(), "encoded absolute locator must decode");
    check(decoded.ok() && decoded.state == absolute,
          "absolute locator must survive exact component-state round-trip");
  }

  auto relative_disguised = base_state();
  relative_disguised.locator_mode =
      ProjectLocatorMode::absolute_user_confirmed;
  relative_disguised.project_locator = "shows/not-absolute.aeylashow";
  check(!encode_plugin_component_state(relative_disguised).ok(),
        "absolute-user-confirmed mode must reject a relative path");

  auto package_relative = base_state();
  package_relative.locator_mode = ProjectLocatorMode::package_relative;
  package_relative.project_locator = "shows/relative.aeylashow";
  check(encode_plugin_component_state(package_relative).ok(),
        "normalized package-relative locator must remain serializable");

  auto traversal = package_relative;
  traversal.project_locator = "../outside.aeylashow";
  check(!encode_plugin_component_state(traversal).ok(),
        "package-relative locator must reject parent traversal");

  if (failures == 0) {
    std::cout << "All AEYLA plugin locator tests passed.\n";
    return EXIT_SUCCESS;
  }

  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}

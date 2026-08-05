#pragma once

#include "core/attributes.h"
#include "core/fixture_profile.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace aeyla {

using DmxUniverse = std::array<std::uint8_t, 512>;

struct PatchedFixture {
  std::string logical_id;
  std::uint16_t address{1};
  bool active{true};
  FixtureProfile profile;
  AttributeFrame values{};
};

struct CompileResult {
  DmxUniverse universe{};
  std::vector<std::string> warnings;
};

CompileResult compile_dmx(const std::vector<PatchedFixture>& fixtures);

}  // namespace aeyla

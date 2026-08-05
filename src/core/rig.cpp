#include "core/rig.h"

#include <array>

namespace aeyla {

std::vector<LogicalFixturePosition> make_floor_rig_14() {
  std::vector<LogicalFixturePosition> fixtures;
  for (int side = 0; side < 2; ++side) {
    for (int i = 0; i < 7; ++i) {
      const std::string id = std::string(side == 0 ? "L" : "R") + std::to_string(i + 1);
      const float x = side == 0 ? 0.08F + i * 0.055F : 0.59F + i * 0.055F;
      fixtures.push_back({id, x, 0.76F, true});
    }
  }
  return fixtures;
}

std::vector<LogicalFixturePosition> make_floor_rig_10() {
  auto fixtures = make_floor_rig_14();
  constexpr std::array<int, 4> inactive{2, 4, 9, 11};  // L3, L5, R3, R5
  for (const int index : inactive) fixtures.at(static_cast<std::size_t>(index)).active = false;
  return fixtures;
}

}  // namespace aeyla

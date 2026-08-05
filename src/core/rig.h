#pragma once

#include <string>
#include <vector>

namespace aeyla {

struct LogicalFixturePosition {
  std::string id;
  float x{0.0F};
  float y{0.0F};
  bool active{true};
};

std::vector<LogicalFixturePosition> make_floor_rig_14();
std::vector<LogicalFixturePosition> make_floor_rig_10();

}  // namespace aeyla

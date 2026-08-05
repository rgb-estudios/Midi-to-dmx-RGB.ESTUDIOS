#pragma once

#include "core/attributes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aeyla {

enum class ChannelMode { Continuous, Range, Constant, Trigger, Ignore };

struct ChannelSpec {
  std::uint16_t slot{1};
  Attribute attribute{Attribute::Dimmer};
  ChannelMode mode{ChannelMode::Continuous};
  std::uint8_t dmx_min{0};
  std::uint8_t dmx_max{255};
  std::uint8_t home{0};
  std::uint8_t constant_value{0};
  bool invert{false};
};

struct FixtureProfile {
  std::string id;
  std::string name;
  std::uint16_t footprint{1};
  std::vector<ChannelSpec> channels;

  [[nodiscard]] bool has(Attribute attribute) const {
    for (const auto& channel : channels) {
      if (channel.mode != ChannelMode::Ignore && channel.attribute == attribute) return true;
    }
    return false;
  }
};

}  // namespace aeyla

#include "core/dmx_compiler.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace aeyla {
namespace {

std::uint8_t map_channel(const ChannelSpec& channel, float normalized) {
  if (channel.mode == ChannelMode::Ignore) return channel.home;
  if (channel.mode == ChannelMode::Constant) return channel.constant_value;
  if (channel.mode == ChannelMode::Trigger) {
    return normalized >= 0.5F ? channel.dmx_max : channel.home;
  }

  float value = clamp_normalized(normalized);
  if (channel.invert) value = 1.0F - value;
  const auto span = static_cast<float>(channel.dmx_max) - static_cast<float>(channel.dmx_min);
  return static_cast<std::uint8_t>(std::lround(static_cast<float>(channel.dmx_min) + value * span));
}

}  // namespace

CompileResult compile_dmx(const std::vector<PatchedFixture>& fixtures) {
  CompileResult result{};

  for (const auto& fixture : fixtures) {
    if (!fixture.active) continue;
    if (fixture.address < 1 || fixture.address + fixture.profile.footprint - 1 > 512) {
      result.warnings.push_back(fixture.logical_id + ": address/footprint exceeds universe");
      continue;
    }

    const float dimmer = get(fixture.values, Attribute::Dimmer);
    const bool has_dimmer = fixture.profile.has(Attribute::Dimmer);

    for (const auto& channel : fixture.profile.channels) {
      if (channel.slot < 1 || channel.slot > fixture.profile.footprint) {
        result.warnings.push_back(fixture.logical_id + ": channel slot outside profile footprint");
        continue;
      }

      float logical = get(fixture.values, channel.attribute);
      const bool colour = channel.attribute == Attribute::Red || channel.attribute == Attribute::Green ||
                          channel.attribute == Attribute::Blue || channel.attribute == Attribute::White ||
                          channel.attribute == Attribute::Amber || channel.attribute == Attribute::UV ||
                          channel.attribute == Attribute::Lime;
      if (colour && !has_dimmer) logical *= dimmer;

      const std::size_t index = static_cast<std::size_t>(fixture.address + channel.slot - 2);
      result.universe.at(index) = map_channel(channel, logical);
    }
  }

  return result;
}

}  // namespace aeyla

#include "core/attributes.h"

#include <algorithm>
#include <array>

namespace aeyla {
namespace {
constexpr std::array<std::string_view, attribute_count> kNames{
    "dimmer", "shutter", "strobe", "red", "green", "blue", "white", "amber",
    "uv", "lime", "macro", "speed", "reset", "zoom", "fan", "haze"};
}

std::string_view attribute_name(Attribute attribute) {
  return kNames.at(static_cast<std::size_t>(attribute));
}

float clamp_normalized(float value) { return std::clamp(value, 0.0F, 1.0F); }

float get(const AttributeFrame& frame, Attribute attribute) {
  return frame.at(static_cast<std::size_t>(attribute));
}

void set(AttributeFrame& frame, Attribute attribute, float value) {
  frame.at(static_cast<std::size_t>(attribute)) = clamp_normalized(value);
}

}  // namespace aeyla

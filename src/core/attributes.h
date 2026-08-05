#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace aeyla {

enum class Attribute : std::size_t {
  Dimmer,
  Shutter,
  Strobe,
  Red,
  Green,
  Blue,
  White,
  Amber,
  UV,
  Lime,
  Macro,
  Speed,
  Reset,
  Zoom,
  Fan,
  Haze,
  Count
};

constexpr std::size_t attribute_count = static_cast<std::size_t>(Attribute::Count);
using AttributeFrame = std::array<float, attribute_count>;

std::string_view attribute_name(Attribute attribute);
float clamp_normalized(float value);
float get(const AttributeFrame& frame, Attribute attribute);
void set(AttributeFrame& frame, Attribute attribute, float value);

}  // namespace aeyla

#include "core/color_transform.h"

#include <algorithm>

namespace aeyla {

AttributeFrame transform_rgb(const RgbColor& input, const ColorTransformSettings& settings) {
  AttributeFrame frame{};
  const float r = clamp_normalized(input.red);
  const float g = clamp_normalized(input.green);
  const float b = clamp_normalized(input.blue);
  const float intensity = clamp_normalized(settings.intensity);

  const float neutral = std::min({r, g, b});
  const float white = neutral * clamp_normalized(settings.white_extraction);
  const float amber_basis = std::max(0.0F, std::min(r, g * 1.25F) - b * 0.25F);
  const float lime_basis = std::max(0.0F, std::min(g, (r + g) * 0.5F) - b * 0.20F);

  set(frame, Attribute::Dimmer, intensity);
  set(frame, Attribute::Red, std::max(0.0F, r - white));
  set(frame, Attribute::Green, std::max(0.0F, g - white));
  set(frame, Attribute::Blue, std::max(0.0F, b - white));
  set(frame, Attribute::White, white);
  set(frame, Attribute::Amber, amber_basis * clamp_normalized(settings.amber_extraction));
  set(frame, Attribute::Lime, lime_basis * clamp_normalized(settings.lime_extraction));
  set(frame, Attribute::UV, clamp_normalized(settings.uv_manual));
  return frame;
}

}  // namespace aeyla

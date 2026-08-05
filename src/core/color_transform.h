#pragma once

#include "core/attributes.h"

namespace aeyla {

struct RgbColor {
  float red{0.0F};
  float green{0.0F};
  float blue{0.0F};
};

struct ColorTransformSettings {
  float intensity{1.0F};
  float white_extraction{0.65F};
  float amber_extraction{0.35F};
  float lime_extraction{0.20F};
  float uv_manual{0.0F};
};

AttributeFrame transform_rgb(const RgbColor& rgb, const ColorTransformSettings& settings);

}  // namespace aeyla

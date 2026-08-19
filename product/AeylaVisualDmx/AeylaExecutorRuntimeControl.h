#pragma once

#include "AeylaVisualDmx.h"

// Legacy semantic executor overlay is intentionally dormant in the Show Player
// capture/replay gate. The class remains attached by the existing shared shell
// so APP/VST3/AUv2 layout code does not fork, but it draws nothing and never
// intercepts mouse input. Song/Take operation now belongs to AeylaMainControl.
class AeylaExecutorRuntimeControl final : public IControl
{
public:
  static IRECT BoundsFor(const IRECT& window)
  {
    return IRECT(window.L, window.B, window.L, window.B);
  }

  AeylaExecutorRuntimeControl(const IRECT& bounds, AeylaVisualDmx& plug)
  : IControl(bounds)
  , mPlug(plug)
  {
  }

  void Draw(IGraphics& g) override
  {
    (void)g;
  }

  bool IsHit(float x, float y) const override
  {
    (void)x;
    (void)y;
    return false;
  }

private:
  AeylaVisualDmx& mPlug;
};

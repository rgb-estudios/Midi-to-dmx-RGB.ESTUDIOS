#pragma once

#include "AeylaVisualDmx.h"

#include <array>
#include <cstdio>

class AeylaExecutorRuntimeControl final : public IControl
{
public:
  static IRECT BoundsFor(const IRECT& window)
  {
    constexpr float margin = 16.0F;
    constexpr float footerHeight = 36.0F;
    constexpr float executorHeight = 122.0F;
    const float footerTop = window.B - footerHeight;
    return IRECT(window.L + margin,
                 footerTop - executorHeight,
                 window.R - margin,
                 footerTop - 10.0F);
  }

  AeylaExecutorRuntimeControl(const IRECT& bounds, AeylaVisualDmx& plug)
  : IControl(bounds)
  , mPlug(plug)
  {
  }

  void Draw(IGraphics& g) override
  {
    BuildButtons();

    const IColor panel(255, 17, 19, 24);
    const IColor raised(255, 21, 23, 29);
    const IColor line(255, 47, 51, 62);
    const IColor text(255, 236, 238, 242);
    const IColor muted(255, 139, 145, 158);
    const IColor red(255, 231, 45, 55);
    const IColor redDark(255, 113, 20, 28);

    g.FillRoundRect(panel, mRECT, 10.0F);
    g.DrawRoundRect(line, mRECT, 10.0F, nullptr, 1.0F);
    g.DrawText(IText(12.0F, text, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "EXECUTORS  /  SHARED MIDI + DMX ENGINE",
               IRECT(mRECT.L + 14.0F, mRECT.T + 8.0F,
                     mRECT.R - 14.0F, mRECT.T + 31.0F));

    static constexpr const char* labels[] = {
        "SOLID RED", "GRADIENT", "COLD WAVE", "NOISE",
        "CHASE", "WHITE LIFT", "UV LIFT", "STROBE"};

    const int active = mPlug.ActiveExecutor();
    for(int index = 0; index < 8; ++index)
    {
      const IRECT& button = mButtons[static_cast<std::size_t>(index)];
      const bool selected = index == active;
      g.FillRoundRect(selected ? redDark : raised, button, 8.0F);
      g.DrawRoundRect(selected ? red : line, button, 8.0F,
                      nullptr, selected ? 2.0F : 1.0F);

      char label[64];
      std::snprintf(label, sizeof(label), "%02d\n%s", 36 + index, labels[index]);
      g.DrawText(IText(10.0F, selected ? text : muted,
                       "AeylaUI", EAlign::Center, EVAlign::Middle),
                 label, button.GetPadded(-5.0F));
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    BuildButtons();

    for(int index = 0; index < 8; ++index)
    {
      if(Contains(mButtons[static_cast<std::size_t>(index)], x, y))
      {
        if(mPressedExecutor >= 0 && mPressedExecutor != index)
          mPlug.ReleaseExecutorFromUI(mPressedExecutor);

        mPressedExecutor = index;
        mPlug.TriggerExecutorFromUI(index, 1.0F);
        SetDirty(false);
        return;
      }
    }
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    (void) x;
    (void) y;
    (void) mod;

    if(mPressedExecutor >= 0)
    {
      mPlug.ReleaseExecutorFromUI(mPressedExecutor);
      mPressedExecutor = -1;
      SetDirty(false);
    }
  }

private:
  static bool Contains(const IRECT& rectangle, float x, float y) noexcept
  {
    return x >= rectangle.L && x <= rectangle.R &&
           y >= rectangle.T && y <= rectangle.B;
  }

  void BuildButtons()
  {
    const IRECT area(mRECT.L + 12.0F,
                     mRECT.T + 42.0F,
                     mRECT.R - 12.0F,
                     mRECT.B - 12.0F);
    constexpr float gap = 8.0F;
    const float width = (area.W() - gap * 7.0F) / 8.0F;

    for(int index = 0; index < 8; ++index)
    {
      const float left = area.L + static_cast<float>(index) * (width + gap);
      mButtons[static_cast<std::size_t>(index)] =
          IRECT(left, area.T, left + width, area.B);
    }
  }

  AeylaVisualDmx& mPlug;
  std::array<IRECT, 8> mButtons{};
  int mPressedExecutor{-1};
};

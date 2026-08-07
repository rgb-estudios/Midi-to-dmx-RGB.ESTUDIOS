#pragma once

#include "AeylaVisualDmx.h"

#include <cstdio>

class AeylaRuntimeStatusControl final : public IControl
{
public:
  AeylaRuntimeStatusControl(const IRECT& bounds, AeylaVisualDmx& plug)
  : IControl(bounds)
  , mPlug(plug)
  {
    SetIgnoreMouse(true);
  }

  void Draw(IGraphics& g) override
  {
    const IRECT footer(mRECT.L, mRECT.B - 38.0F, mRECT.R, mRECT.B);
    const IColor background(255, 8, 9, 12);
    const IColor line(255, 47, 51, 62);
    const IColor text(255, 226, 229, 234);
    const IColor muted(255, 139, 145, 158);
    const IColor warning(255, 245, 154, 43);
    const IColor valid(255, 57, 211, 132);
    const IColor danger(255, 231, 45, 55);

    g.FillRect(background, footer);
    g.DrawLine(line, footer.L, footer.T, footer.R, footer.T, nullptr, 1.0F);

    const IText leftText(10.0F, text, "AeylaUI", EAlign::Near, EVAlign::Middle);
    const IText centreText(10.0F, muted, "AeylaUI", EAlign::Center, EVAlign::Middle);

    g.DrawText(leftText,
               "ALPHA 0.3  ·  DOCUMENT-DRIVEN  ·  NOT SHOW READY",
               IRECT(footer.L + 16.0F, footer.T, footer.L + footer.W() * 0.38F, footer.B));

    char runtime[192];
    std::snprintf(runtime, sizeof(runtime),
                  "DMX %llu  ·  %d ACTIVE CH  ·  MIDI DROP %llu  ·  STATE ERR %llu",
                  static_cast<unsigned long long>(mPlug.DmxGeneration()),
                  mPlug.DmxNonZeroChannels(),
                  static_cast<unsigned long long>(mPlug.DroppedMidiEvents()),
                  static_cast<unsigned long long>(mPlug.HostStateRestoreErrors()));
    g.DrawText(centreText, runtime,
               IRECT(footer.L + footer.W() * 0.31F,
                     footer.T,
                     footer.L + footer.W() * 0.72F,
                     footer.B));

    char state[180];
    const char* project = mPlug.ProjectValid() ? "PROJECT VALID" : "PROJECT INVALID";
    const char* backend = mPlug.BackendReady() ? "BACKEND READY" : "BACKEND DISCONNECTED";
    const char* output = mPlug.OutputArmed() ? "OUTPUT ARMED" : "OUTPUT DISARMED";
    const char* blackout = mPlug.EffectiveBlackout() ? "BLACKOUT" : "PREVIEW";
    std::snprintf(state, sizeof(state), "%s  ·  %s  ·  %s  ·  %s",
                  project, backend, output, blackout);

    IColor stateColor = mPlug.BackendReady() ? valid : warning;
    if(mPlug.OutputArmed() || !mPlug.ProjectValid() ||
       mPlug.HostStateRestoreErrors() > 0U)
      stateColor = danger;

    g.DrawText(IText(10.0F, stateColor, "AeylaUI", EAlign::Far, EVAlign::Middle),
               state,
               IRECT(footer.L + footer.W() * 0.67F, footer.T,
                     footer.R - 16.0F, footer.B));
  }

private:
  AeylaVisualDmx& mPlug;
};

#pragma once

#include "AeylaVisualDmx.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>

class AeylaMainControl final : public IControl
{
public:
  AeylaMainControl(const IRECT& bounds, AeylaVisualDmx& plug)
  : IControl(bounds,
             {kParamBlackout,
              kParamGrandMaster,
              kParamRigMode,
              kParamSource,
              kParamSpeed,
              kParamWhiteExtract,
              kParamAmberExtract,
              kParamUV})
  , mPlug(plug)
  {
  }

  void Draw(IGraphics& g) override
  {
    BuildLayout();
    mPhase = static_cast<double>(mPlug.VisualPhase());
    mPrimaryColor = FromNormalized(mPlug.ActiveLookColor(false));
    mSecondaryColor = FromNormalized(mPlug.ActiveLookColor(true));

    g.FillRect(kBackground, mRECT);
    DrawHeader(g);
    DrawSourcePanel(g);
    DrawCanvas(g);
    DrawInspector(g);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    BuildLayout();

    if(Contains(mBlackoutButton, x, y))
    {
      SetValueFromUserInput(GetValue(kValBlackout) > 0.5 ? 0.0 : 1.0, kValBlackout);
      return;
    }

    if(Contains(mRigButton, x, y))
    {
      SetValueFromUserInput(GetValue(kValRigMode) > 0.5 ? 0.0 : 1.0, kValRigMode);
      return;
    }

    for(int i = 0; i < static_cast<int>(mSourceButtons.size()); ++i)
    {
      if(Contains(mSourceButtons[static_cast<std::size_t>(i)], x, y))
      {
        SetValueFromUserInput(static_cast<double>(i) / 4.0, kValSource);
        return;
      }
    }

    if(Contains(mStoreLookButton, x, y))
    {
      ReportAuthoringResult(mPlug.StoreLookFromUI());
      SetDirty(false);
      return;
    }
    if(Contains(mPreviousLookButton, x, y))
    {
      (void) mPlug.SelectAdjacentLookFromUI(-1);
      SetDirty(false);
      return;
    }
    if(Contains(mNextLookButton, x, y))
    {
      (void) mPlug.SelectAdjacentLookFromUI(1);
      SetDirty(false);
      return;
    }
    if(Contains(mNewSongButton, x, y))
    {
      ReportAuthoringResult(mPlug.CreateSongFromUI());
      SetDirty(false);
      return;
    }
    if(Contains(mPreviousSongButton, x, y))
    {
      (void) mPlug.SelectAdjacentSongFromUI(-1);
      SetDirty(false);
      return;
    }
    if(Contains(mNextSongButton, x, y))
    {
      (void) mPlug.SelectAdjacentSongFromUI(1);
      SetDirty(false);
      return;
    }
    if(Contains(mStoreCueButton, x, y))
    {
      ReportAuthoringResult(mPlug.StoreCueAtPlayheadFromUI());
      SetDirty(false);
      return;
    }

    for(int i = 0; i < static_cast<int>(mFixtureButtons.size()); ++i)
    {
      if(Contains(mFixtureButtons[static_cast<std::size_t>(i)], x, y))
      {
        mSelectedFixture = i;
        SetDirty(false);
        return;
      }
    }

    if(Contains(mGrandMasterSlider, x, y))
    {
      SetSliderFromX(x, mGrandMasterSlider, kValGrandMaster);
      return;
    }
    if(Contains(mLookIntensitySlider, x, y))
    {
      SetLookIntensityFromX(x);
      return;
    }
    if(Contains(mSpeedSlider, x, y))
    {
      SetSliderFromX(x, mSpeedSlider, kValSpeed);
      return;
    }
    if(Contains(mWhiteSlider, x, y))
    {
      SetSliderFromX(x, mWhiteSlider, kValWhite);
      return;
    }
    if(Contains(mAmberSlider, x, y))
    {
      SetSliderFromX(x, mAmberSlider, kValAmber);
      return;
    }
    if(Contains(mUVSlider, x, y))
    {
      SetSliderFromX(x, mUVSlider, kValUV);
      return;
    }
    if(Contains(mFixtureMaskButton, x, y))
    {
      (void) mPlug.ToggleFixtureInActiveLookFromUI(mSelectedFixture);
      SetDirty(false);
      return;
    }

    if(Contains(mColorTargetButton, x, y))
    {
      mEditingSecondaryColor = !mEditingSecondaryColor;
      SetDirty(false);
      return;
    }

    for(std::size_t i = 0; i < mPaletteButtons.size(); ++i)
    {
      if(Contains(mPaletteButtons[i], x, y))
      {
        const auto& selected = Palette()[i];
        (void) mPlug.SetActiveLookColorFromUI(
            mEditingSecondaryColor,
            static_cast<float>(selected.R) / 255.0F,
            static_cast<float>(selected.G) / 255.0F,
            static_cast<float>(selected.B) / 255.0F);
        SetDirty(false);
        return;
      }
    }
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    (void) dX;
    (void) dY;
    (void) mod;

    if(Contains(mGrandMasterSlider, x, y))
      SetSliderFromX(x, mGrandMasterSlider, kValGrandMaster);
    else if(Contains(mLookIntensitySlider, x, y))
      SetLookIntensityFromX(x);
    else if(Contains(mSpeedSlider, x, y))
      SetSliderFromX(x, mSpeedSlider, kValSpeed);
    else if(Contains(mWhiteSlider, x, y))
      SetSliderFromX(x, mWhiteSlider, kValWhite);
    else if(Contains(mAmberSlider, x, y))
      SetSliderFromX(x, mAmberSlider, kValAmber);
    else if(Contains(mUVSlider, x, y))
      SetSliderFromX(x, mUVSlider, kValUV);
  }

private:
  enum EValueIndexes
  {
    kValBlackout = 0,
    kValGrandMaster,
    kValRigMode,
    kValSource,
    kValSpeed,
    kValWhite,
    kValAmber,
    kValUV
  };

  static constexpr IColor kBackground{255, 8, 9, 12};
  static constexpr IColor kPanel{255, 17, 19, 24};
  static constexpr IColor kPanelRaised{255, 24, 27, 34};
  static constexpr IColor kLine{255, 47, 51, 62};
  static constexpr IColor kText{255, 236, 238, 242};
  static constexpr IColor kMuted{255, 139, 145, 158};
  static constexpr IColor kRed{255, 231, 45, 55};
  static constexpr IColor kRedDark{255, 113, 20, 28};
  static constexpr IColor kGreen{255, 57, 211, 132};
  static constexpr IColor kBlue{255, 59, 140, 246};
  static constexpr IColor kAmber{255, 245, 154, 43};
  static constexpr IColor kUV{255, 161, 85, 247};

  static bool Contains(const IRECT& r, float x, float y) noexcept
  {
    return x >= r.L && x <= r.R && y >= r.T && y <= r.B;
  }

  static IColor Mix(const IColor& a, const IColor& b, double t)
  {
    t = std::clamp(t, 0.0, 1.0);
    const auto mixChannel = [t](int x, int y) {
      return static_cast<int>(std::lround(static_cast<double>(x) +
                                          (static_cast<double>(y - x) * t)));
    };
    return IColor(mixChannel(a.A, b.A),
                  mixChannel(a.R, b.R),
                  mixChannel(a.G, b.G),
                  mixChannel(a.B, b.B));
  }

  static IColor FromNormalized(const std::array<float, 3>& color)
  {
    return IColor(255,
                  static_cast<int>(std::lround(
                      std::clamp(color[0], 0.0F, 1.0F) * 255.0F)),
                  static_cast<int>(std::lround(
                      std::clamp(color[1], 0.0F, 1.0F) * 255.0F)),
                  static_cast<int>(std::lround(
                      std::clamp(color[2], 0.0F, 1.0F) * 255.0F)));
  }

  static IColor ContrastText(const IColor& color)
  {
    const int luminance = color.R * 299 + color.G * 587 + color.B * 114;
    return luminance > 150000 ? IColor(255, 8, 9, 12) : kText;
  }

  static const std::array<IColor, 8>& Palette()
  {
    static const std::array<IColor, 8> colors = {
        IColor(255, 255, 255, 255), IColor(255, 232, 28, 45),
        IColor(255, 245, 154, 43), IColor(255, 30, 88, 232),
        IColor(255, 30, 211, 232), IColor(255, 220, 47, 196),
        IColor(255, 44, 205, 105), IColor(255, 118, 48, 220)};
    return colors;
  }

  void BuildLayout()
  {
    const float width = mRECT.W();
    const float height = mRECT.H();
    const float margin = 16.0F;
    const float headerH = 70.0F;
    const float footerH = 36.0F;
    const float executorH = 122.0F;
    const float leftW = std::clamp(width * 0.18F, 190.0F, 250.0F);
    const float rightW = std::clamp(width * 0.23F, 250.0F, 330.0F);

    mHeader = IRECT(mRECT.L, mRECT.T, mRECT.R, mRECT.T + headerH);
    mFooter = IRECT(mRECT.L, mRECT.B - footerH, mRECT.R, mRECT.B);
    mExecutors = IRECT(mRECT.L + margin,
                       mFooter.T - executorH,
                       mRECT.R - margin,
                       mFooter.T - 10.0F);

    const float contentTop = mHeader.B + 10.0F;
    const float contentBottom = mExecutors.T - 10.0F;
    mSources = IRECT(mRECT.L + margin,
                     contentTop,
                     mRECT.L + margin + leftW,
                     contentBottom);
    mInspector = IRECT(mRECT.R - margin - rightW,
                       contentTop,
                       mRECT.R - margin,
                       contentBottom);
    mCanvas = IRECT(mSources.R + 10.0F,
                    contentTop,
                    mInspector.L - 10.0F,
                    contentBottom);

    mBlackoutButton = IRECT(mHeader.R - 152.0F, mHeader.T + 16.0F,
                            mHeader.R - 16.0F, mHeader.B - 16.0F);

    mRigButton = IRECT(mCanvas.R - 98.0F, mCanvas.T + 14.0F,
                       mCanvas.R - 14.0F, mCanvas.T + 46.0F);
    mColorTargetButton = IRECT(mInspector.L + 16.0F, mInspector.T + 104.0F,
                               mInspector.R - 16.0F, mInspector.T + 130.0F);
    const float paletteGap = 5.0F;
    const float paletteWidth =
        (mInspector.W() - 32.0F - paletteGap * 7.0F) / 8.0F;
    for(std::size_t i = 0; i < mPaletteButtons.size(); ++i)
    {
      const float left = mInspector.L + 16.0F +
                         static_cast<float>(i) * (paletteWidth + paletteGap);
      mPaletteButtons[i] =
          IRECT(left, mInspector.T + 136.0F, left + paletteWidth,
                mInspector.T + 162.0F);
    }
    mFixtureMaskButton = IRECT(mInspector.L + 16.0F, mInspector.T + 169.0F,
                               mInspector.R - 16.0F, mInspector.T + 195.0F);

    const IRECT sourceArea(mSources.L + 12.0F, mSources.T + 64.0F,
                           mSources.R - 12.0F, mSources.T + 294.0F);
    for(int i = 0; i < 5; ++i)
    {
      const float top = sourceArea.T + static_cast<float>(i) * 44.0F;
      mSourceButtons[static_cast<std::size_t>(i)] =
          IRECT(sourceArea.L, top, sourceArea.R, top + 34.0F);
    }

    constexpr float navigationWidth = 34.0F;
    mPreviousLookButton = IRECT(mSources.L + 12.0F, mSources.B - 264.0F,
                                mSources.L + 12.0F + navigationWidth,
                                mSources.B - 230.0F);
    mNextLookButton = IRECT(mSources.R - 12.0F - navigationWidth,
                            mSources.B - 264.0F, mSources.R - 12.0F,
                            mSources.B - 230.0F);
    mLookStatus = IRECT(mPreviousLookButton.R + 6.0F, mSources.B - 264.0F,
                        mNextLookButton.L - 6.0F, mSources.B - 230.0F);
    mStoreLookButton = IRECT(mSources.L + 12.0F, mSources.B - 222.0F,
                             mSources.R - 12.0F, mSources.B - 188.0F);
    mNewSongButton = IRECT(mSources.L + 12.0F, mSources.B - 180.0F,
                           mSources.R - 12.0F, mSources.B - 146.0F);
    mNextSongButton = IRECT(mSources.R - 12.0F - navigationWidth,
                            mSources.B - 138.0F, mSources.R - 12.0F,
                            mSources.B - 104.0F);
    mPreviousSongButton = IRECT(mSources.L + 12.0F, mSources.B - 138.0F,
                                mSources.L + 12.0F + navigationWidth,
                                mSources.B - 104.0F);
    mSongStatus = IRECT(mPreviousSongButton.R + 6.0F, mSources.B - 138.0F,
                        mNextSongButton.L - 6.0F, mSources.B - 104.0F);
    mStoreCueButton = IRECT(mSources.L + 12.0F, mSources.B - 96.0F,
                            mSources.R - 12.0F, mSources.B - 62.0F);

    const float sliderL = mInspector.L + 18.0F;
    const float sliderR = mInspector.R - 18.0F;
    float sliderTop = mInspector.T + 205.0F;
    mGrandMasterSlider = IRECT(sliderL, sliderTop, sliderR, sliderTop + 30.0F);
    sliderTop += 57.0F;
    mLookIntensitySlider = IRECT(sliderL, sliderTop, sliderR, sliderTop + 30.0F);
    sliderTop += 57.0F;
    mSpeedSlider = IRECT(sliderL, sliderTop, sliderR, sliderTop + 30.0F);
    sliderTop += 57.0F;
    mWhiteSlider = IRECT(sliderL, sliderTop, sliderR, sliderTop + 30.0F);
    sliderTop += 57.0F;
    mAmberSlider = IRECT(sliderL, sliderTop, sliderR, sliderTop + 30.0F);
    sliderTop += 57.0F;
    mUVSlider = IRECT(sliderL, sliderTop, sliderR, sliderTop + 30.0F);

    const IRECT mapArea(mCanvas.L + 38.0F, mCanvas.T + 88.0F,
                        mCanvas.R - 38.0F, mCanvas.B - 44.0F);
    for(int row = 0; row < 2; ++row)
    {
      for(int col = 0; col < 7; ++col)
      {
        const int index = row * 7 + col;
        const float x = mapArea.L + mapArea.W() * (static_cast<float>(col) / 6.0F);
        const float y = mapArea.T + mapArea.H() * (row == 0 ? 0.28F : 0.72F);
        mFixtureButtons[static_cast<std::size_t>(index)] =
            IRECT(x - 18.0F, y - 18.0F, x + 18.0F, y + 18.0F);
      }
    }
  }

  void DrawHeader(IGraphics& g)
  {
    g.FillRect(kPanel, mHeader);
    g.DrawLine(kLine, mHeader.L, mHeader.B, mHeader.R, mHeader.B, nullptr, 1.0F);

    const IText titleText(25.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle);
    const IText smallText(11.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle);
    g.DrawText(titleText, "AEYLA  /  VISUAL DMX", IRECT(mHeader.L + 18.0F, mHeader.T + 9.0F,
                                                         mHeader.L + 390.0F, mHeader.T + 43.0F));
    g.DrawText(smallText, "GRAPHICAL ALPHA  ·  SHARED STANDALONE + VST3",
               IRECT(mHeader.L + 19.0F, mHeader.T + 41.0F,
                     mHeader.L + 460.0F, mHeader.B - 8.0F));

    const bool midiActive = mPlug.LastMidiNote() >= 0;
    const IColor midiColor = midiActive ? kGreen : kMuted;
    g.FillCircle(midiColor, mHeader.L + 500.0F, mHeader.T + 31.0F, 5.0F);
    char midiLabel[96];
    std::snprintf(midiLabel, sizeof(midiLabel), "MIDI  %llu  ·  NOTE %d",
                  static_cast<unsigned long long>(mPlug.MidiEventCount()),
                  mPlug.LastMidiNote());
    g.DrawText(smallText, midiLabel,
               IRECT(mHeader.L + 512.0F, mHeader.T + 16.0F,
                     mHeader.L + 760.0F, mHeader.T + 47.0F));

    const bool blackout = GetValue(kValBlackout) > 0.5;
    DrawButton(g, mBlackoutButton,
               blackout ? "BLACKOUT ON" : "BLACKOUT",
               blackout ? kRed : kPanelRaised,
               kText);
  }

  void DrawSourcePanel(IGraphics& g)
  {
    DrawPanel(g, mSources);
    const IText section(13.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle);
    const IText caption(10.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle);
    g.DrawText(section, "VISUAL SOURCES",
               IRECT(mSources.L + 14.0F, mSources.T + 14.0F,
                     mSources.R - 14.0F, mSources.T + 38.0F));
    g.DrawText(caption, "Click a source to map it across the rig",
               IRECT(mSources.L + 14.0F, mSources.T + 36.0F,
                     mSources.R - 14.0F, mSources.T + 58.0F));

    static constexpr const char* names[] = {"SOLID", "GRADIENT", "WAVE", "NOISE", "CHASE"};
    const int current = SourceIndex();
    for(int i = 0; i < 5; ++i)
    {
      DrawButton(g,
                 mSourceButtons[static_cast<std::size_t>(i)],
                 names[i],
                 i == current ? kRedDark : kPanelRaised,
                 i == current ? kText : kMuted);
    }

    DrawButton(g, mPreviousLookButton, "<", kPanelRaised, kText);
    const auto lookStatus = mPlug.ActiveLookStatus();
    DrawButton(g, mLookStatus, lookStatus.c_str(), kPanelRaised, kMuted);
    DrawButton(g, mNextLookButton, ">", kPanelRaised, kText);
    DrawButton(g, mStoreLookButton, "1  ·  STORE LOOK", kPanelRaised, kText);
    DrawButton(g, mNewSongButton, "2  ·  NEW SONG", kPanelRaised, kText);
    DrawButton(g, mPreviousSongButton, "<", kPanelRaised, kText);
    const auto songStatus = mPlug.ActiveSongStatus();
    DrawButton(g, mSongStatus, songStatus.c_str(), kPanelRaised, kMuted);
    DrawButton(g, mNextSongButton, ">", kPanelRaised, kText);
    DrawButton(g, mStoreCueButton, "3  ·  STORE CUE @ PLAYHEAD",
               kRedDark, kText);

    const IRECT info(mSources.L + 12.0F, mSources.B - 52.0F,
                     mSources.R - 12.0F, mSources.B - 12.0F);
    g.FillRoundRect(IColor(255, 12, 13, 17), info, 8.0F);
    g.FillCircle(kAmber, info.L + 17.0F, info.MH(), 4.0F);
    g.DrawText(caption, "PREVIEW ONLY  /  NO PHYSICAL DMX",
               IRECT(info.L + 28.0F, info.T + 4.0F,
                     info.R - 10.0F, info.B - 4.0F));
  }

  void DrawCanvas(IGraphics& g)
  {
    DrawPanel(g, mCanvas);
    const IText section(13.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle);
    const IText caption(10.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle);
    g.DrawText(section, "VISUAL CANVAS  /  FIXTURE SAMPLER",
               IRECT(mCanvas.L + 16.0F, mCanvas.T + 14.0F,
                     mCanvas.R - 116.0F, mCanvas.T + 38.0F));
    g.DrawText(caption, "The texture is sampled at each numbered point",
               IRECT(mCanvas.L + 16.0F, mCanvas.T + 37.0F,
                     mCanvas.R - 116.0F, mCanvas.T + 58.0F));

    const bool rig14 = GetValue(kValRigMode) > 0.5;
    DrawButton(g, mRigButton, rig14 ? "RIG 14" : "RIG 10",
               rig14 ? kBlue : kPanelRaised, kText);

    const IRECT visual(mCanvas.L + 18.0F, mCanvas.T + 68.0F,
                       mCanvas.R - 18.0F, mCanvas.B - 18.0F);
    DrawVisualTexture(g, visual);

    const int activeExecutor = mPlug.ActiveExecutor();

    for(int i = 0; i < 14; ++i)
    {
      const int col = i % 7;
      const bool physical = rig14 || col < 5;
      const IRECT& fixture = mFixtureButtons[static_cast<std::size_t>(i)];
      const float cx = fixture.MW();
      const float cy = fixture.MH();
      const double sample = std::clamp((cx - visual.L) / std::max(1.0F, visual.W()), 0.0F, 1.0F);
      IColor fixtureColor = SampleColor(sample, cy, visual);
      if(!physical)
        fixtureColor = IColor(90, fixtureColor.R, fixtureColor.G, fixtureColor.B);

      const bool selected = i == mSelectedFixture;
      g.FillCircle(fixtureColor, cx, cy, selected ? 14.0F : 11.0F);
      g.DrawCircle(selected ? kText : IColor(180, 8, 8, 10), cx, cy,
                   selected ? 17.0F : 13.0F, nullptr, selected ? 2.5F : 1.5F);

      char label[8];
      std::snprintf(label, sizeof(label), "%d", i + 1);
      const IText number(9.0F, physical ? IColor(255, 5, 6, 8) : kMuted,
                         "AeylaUI", EAlign::Center, EVAlign::Middle);
      g.DrawText(number, label, fixture);
    }

    if(activeExecutor >= 0)
    {
      char activeText[64];
      std::snprintf(activeText, sizeof(activeText), "EXECUTOR %02d ACTIVE",
                    activeExecutor + 1);
      const IRECT badge(mCanvas.L + 18.0F, mCanvas.B - 54.0F,
                        mCanvas.L + 170.0F, mCanvas.B - 24.0F);
      g.FillRoundRect(kRed, badge, 6.0F);
      const IText badgeText(10.0F, kText, "AeylaUI", EAlign::Center, EVAlign::Middle);
      g.DrawText(badgeText, activeText, badge);
    }
  }

  void DrawVisualTexture(IGraphics& g, const IRECT& visual)
  {
    g.FillRoundRect(IColor(255, 5, 6, 9), visual, 10.0F);
    const int source = SourceIndex();
    const int strips = 72;

    if(source == 0)
    {
      g.FillRoundRect(mPrimaryColor, visual.GetPadded(-2.0F), 9.0F);
      return;
    }

    if(source == 1)
    {
      for(int i = 0; i < strips; ++i)
      {
        const double t = static_cast<double>(i) / static_cast<double>(strips - 1);
        const float left = visual.L + visual.W() * static_cast<float>(t);
        const float right = visual.L + visual.W() * static_cast<float>(i + 1) /
                                           static_cast<float>(strips);
        const IColor c = Mix(mPrimaryColor, mSecondaryColor, t);
        g.FillRect(c, IRECT(left, visual.T, right + 1.0F, visual.B));
      }
      return;
    }

    if(source == 2)
    {
      for(int i = 0; i < strips; ++i)
      {
        const double t = static_cast<double>(i) / static_cast<double>(strips - 1);
        const double wave = 0.5 + 0.5 * std::sin((t * 4.0 + mPhase * 2.0) * 3.141592653589793);
        const IColor c = Mix(mPrimaryColor, mSecondaryColor, wave);
        const float left = visual.L + visual.W() * static_cast<float>(t);
        const float right = visual.L + visual.W() * static_cast<float>(i + 1) /
                                           static_cast<float>(strips);
        g.FillRect(c, IRECT(left, visual.T, right + 1.0F, visual.B));
      }
      return;
    }

    if(source == 3)
    {
      constexpr int cols = 18;
      constexpr int rows = 10;
      for(int row = 0; row < rows; ++row)
      {
        for(int col = 0; col < cols; ++col)
        {
          const std::uint32_t seed = static_cast<std::uint32_t>(
              col * 73856093U ^ row * 19349663U ^
              static_cast<int>(mPhase * 60.0) * 83492791U);
          const double n = static_cast<double>((seed >> 8U) & 255U) / 255.0;
          const IColor c = Mix(mPrimaryColor, mSecondaryColor, n);
          const float cellW = visual.W() / static_cast<float>(cols);
          const float cellH = visual.H() / static_cast<float>(rows);
          g.FillRect(c, IRECT(visual.L + col * cellW,
                              visual.T + row * cellH,
                              visual.L + (col + 1) * cellW + 1.0F,
                              visual.T + (row + 1) * cellH + 1.0F));
        }
      }
      return;
    }

    g.FillRect(IColor(255, 7, 8, 11), visual);
    const float chaseW = std::max(40.0F, visual.W() * 0.18F);
    const float centre = visual.L + static_cast<float>(mPhase) * (visual.W() + chaseW) - chaseW;
    for(int i = 0; i < 36; ++i)
    {
      const double t = static_cast<double>(i) / 35.0;
      const float left = centre - chaseW * 0.5F + chaseW * static_cast<float>(t);
      const IColor c = Mix(mSecondaryColor, mPrimaryColor,
                           1.0 - std::abs(t * 2.0 - 1.0));
      g.FillRect(c, IRECT(left, visual.T, left + chaseW / 35.0F + 1.0F, visual.B));
    }
  }

  IColor SampleColor(double normalizedX, float y, const IRECT& visual) const
  {
    const int source = SourceIndex();
    if(source == 0)
      return mPrimaryColor;
    if(source == 1)
      return Mix(mPrimaryColor, mSecondaryColor, normalizedX);
    if(source == 2)
    {
      const double wave = 0.5 + 0.5 * std::sin((normalizedX * 4.0 + mPhase * 2.0) *
                                               3.141592653589793);
      return Mix(mPrimaryColor, mSecondaryColor, wave);
    }
    if(source == 3)
    {
      const int row = static_cast<int>(10.0F * (y - visual.T) / std::max(1.0F, visual.H()));
      const int col = static_cast<int>(18.0 * normalizedX);
      const std::uint32_t seed = static_cast<std::uint32_t>(
          col * 73856093U ^ row * 19349663U ^
          static_cast<int>(mPhase * 60.0) * 83492791U);
      const double n = static_cast<double>((seed >> 8U) & 255U) / 255.0;
      return Mix(mPrimaryColor, mSecondaryColor, n);
    }

    const double distance = std::abs(normalizedX - mPhase);
    const double intensity = std::clamp(1.0 - distance * 7.0, 0.0, 1.0);
    return Mix(mSecondaryColor, mPrimaryColor, intensity);
  }

  void DrawInspector(IGraphics& g)
  {
    DrawPanel(g, mInspector);
    const IText section(13.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle);
    const IText caption(10.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle);
    const IText valueText(11.0F, kText, "AeylaUI", EAlign::Far, EVAlign::Middle);

    g.DrawText(section, "FIXTURE INSPECTOR",
               IRECT(mInspector.L + 16.0F, mInspector.T + 14.0F,
                     mInspector.R - 16.0F, mInspector.T + 38.0F));

    char fixtureName[64];
    std::snprintf(fixtureName, sizeof(fixtureName), "FIXTURE %02d  ·  %s%d",
                  mSelectedFixture + 1,
                  mSelectedFixture < 7 ? "L" : "R",
                  (mSelectedFixture % 7) + 1);
    g.DrawText(IText(17.0F, kText, "AeylaUI", EAlign::Near, EVAlign::Middle),
               fixtureName,
               IRECT(mInspector.L + 16.0F, mInspector.T + 48.0F,
                     mInspector.R - 16.0F, mInspector.T + 77.0F));
    g.DrawText(caption, "PROFILE  ·  RGBWALUV 13CH",
               IRECT(mInspector.L + 16.0F, mInspector.T + 78.0F,
                     mInspector.R - 16.0F, mInspector.T + 101.0F));
    DrawButton(g, mColorTargetButton,
               mEditingSecondaryColor ? "EDITING SECONDARY COLOR"
                                      : "EDITING PRIMARY COLOR",
               mEditingSecondaryColor ? mSecondaryColor : mPrimaryColor,
               ContrastText(mEditingSecondaryColor ? mSecondaryColor
                                                   : mPrimaryColor));

    const auto& palette = Palette();
    for(std::size_t i = 0; i < mPaletteButtons.size(); ++i)
    {
      const IRECT& swatch = mPaletteButtons[i];
      const auto& color = palette[i];
      g.FillRoundRect(color, swatch, 4.0F);
      const IColor& active = mEditingSecondaryColor ? mSecondaryColor : mPrimaryColor;
      const bool selected = color.R == active.R && color.G == active.G &&
                            color.B == active.B;
      g.DrawRoundRect(selected ? ContrastText(color) : kLine, swatch, 4.0F, nullptr,
                      selected ? 2.5F : 1.0F);
    }

    const bool included = mPlug.FixtureIncludedInActiveLook(mSelectedFixture);
    DrawButton(g, mFixtureMaskButton,
               included ? "FIXTURE INCLUDED IN LOOK" : "FIXTURE EXCLUDED FROM LOOK",
               included ? kPanelRaised : kRedDark,
               included ? kGreen : kText);

    DrawSlider(g, mGrandMasterSlider, "GRAND MASTER", GetValue(kValGrandMaster), kRed, valueText);
    DrawSlider(g, mLookIntensitySlider, "LOOK INTENSITY",
               mPlug.ActiveLookIntensity(), kGreen, valueText);
    DrawSlider(g, mSpeedSlider, "ANIMATION SPEED", GetValue(kValSpeed), kBlue, valueText);
    DrawSlider(g, mWhiteSlider, "WHITE EXTRACTION", GetValue(kValWhite), kText, valueText);
    DrawSlider(g, mAmberSlider, "AMBER EXTRACTION", GetValue(kValAmber), kAmber, valueText);
    DrawSlider(g, mUVSlider, "UV MANUAL", GetValue(kValUV), kUV, valueText);
  }

  void ReportAuthoringResult(const aeyla::product::AuthoringResult& result)
  {
    if(result.succeeded)
      return;
    if(auto* ui = GetUI())
      ui->ShowMessageBox(result.message.c_str(), "AEYLA · AUTHORING", kMB_OK);
  }

  void DrawPanel(IGraphics& g, const IRECT& r)
  {
    g.FillRoundRect(kPanel, r, 10.0F);
    g.DrawRoundRect(kLine, r, 10.0F, nullptr, 1.0F);
  }

  void DrawButton(IGraphics& g,
                  const IRECT& r,
                  const char* label,
                  const IColor& fill,
                  const IColor& textColor)
  {
    g.FillRoundRect(fill, r, 7.0F);
    g.DrawRoundRect(kLine, r, 7.0F, nullptr, 1.0F);
    g.DrawText(IText(10.0F, textColor, "AeylaUI", EAlign::Center, EVAlign::Middle),
               label, r.GetPadded(-4.0F));
  }

  void DrawSlider(IGraphics& g,
                  const IRECT& r,
                  const char* label,
                  double value,
                  const IColor& accent,
                  const IText& valueText)
  {
    const IText labelText(10.0F, kMuted, "AeylaUI", EAlign::Near, EVAlign::Middle);
    const IRECT labelRect(r.L, r.T - 24.0F, r.R, r.T - 4.0F);
    g.DrawText(labelText, label, labelRect);

    char valueLabel[16];
    std::snprintf(valueLabel, sizeof(valueLabel), "%d%%",
                  static_cast<int>(std::lround(value * 100.0)));
    g.DrawText(valueText, valueLabel, labelRect);

    const IRECT track(r.L, r.MH() - 4.0F, r.R, r.MH() + 4.0F);
    g.FillRoundRect(IColor(255, 35, 38, 46), track, 4.0F);
    const float fillR = track.L + track.W() * static_cast<float>(value);
    if(fillR > track.L)
      g.FillRoundRect(accent, IRECT(track.L, track.T, fillR, track.B), 4.0F);
    g.FillCircle(kText, fillR, track.MH(), 6.0F);
  }

  void SetSliderFromX(float x, const IRECT& slider, int valueIndex)
  {
    const double value = std::clamp((x - slider.L) / std::max(1.0F, slider.W()),
                                    0.0F, 1.0F);
    SetValueFromUserInput(value, valueIndex);
  }

  void SetLookIntensityFromX(float x)
  {
    const float value = std::clamp(
        (x - mLookIntensitySlider.L) /
            std::max(1.0F, mLookIntensitySlider.W()),
        0.0F, 1.0F);
    (void) mPlug.SetActiveLookIntensityFromUI(value);
    SetDirty(false);
  }

  int SourceIndex() const
  {
    return std::clamp(static_cast<int>(std::lround(GetValue(kValSource) * 4.0)), 0, 4);
  }

  AeylaVisualDmx& mPlug;
  double mPhase{0.0};
  int mSelectedFixture{0};

  IRECT mHeader{};
  IRECT mFooter{};
  IRECT mSources{};
  IRECT mCanvas{};
  IRECT mInspector{};
  IRECT mExecutors{};
  IRECT mBlackoutButton{};
  IRECT mRigButton{};
  IRECT mColorTargetButton{};
  std::array<IRECT, 8> mPaletteButtons{};
  IRECT mFixtureMaskButton{};
  IRECT mGrandMasterSlider{};
  IRECT mLookIntensitySlider{};
  IRECT mSpeedSlider{};
  IRECT mWhiteSlider{};
  IRECT mAmberSlider{};
  IRECT mUVSlider{};
  std::array<IRECT, 5> mSourceButtons{};
  IRECT mStoreLookButton{};
  IRECT mPreviousLookButton{};
  IRECT mLookStatus{};
  IRECT mNextLookButton{};
  IRECT mNewSongButton{};
  IRECT mPreviousSongButton{};
  IRECT mSongStatus{};
  IRECT mNextSongButton{};
  IRECT mStoreCueButton{};
  std::array<IRECT, 14> mFixtureButtons{};
  bool mEditingSecondaryColor{false};
  IColor mPrimaryColor{255, 232, 28, 45};
  IColor mSecondaryColor{255, 245, 154, 43};
};

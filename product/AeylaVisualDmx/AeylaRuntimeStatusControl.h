#pragma once

#include "AeylaVisualDmx.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>

class AeylaRuntimeStatusControl final : public IControl
{
public:
  AeylaRuntimeStatusControl(const IRECT& bounds, AeylaVisualDmx& plug)
  : IControl(bounds)
  , mPlug(plug)
  {
  }

  void Draw(IGraphics& g) override
  {
    if(mLiveOpen)
    {
      DrawLive(g);
      DrawOperatorFrame(g);
      return;
    }

    BuildButtons();
    const IRECT footer = Footer();
    const IColor background(255, 8, 9, 12);
    const IColor raised(255, 21, 23, 29);
    const IColor line(255, 47, 51, 62);
    const IColor text(255, 226, 229, 234);
    const IColor muted(255, 136, 146, 160);
    const IColor warning(255, 245, 154, 43);
    const IColor valid(255, 57, 211, 132);
    const IColor danger(255, 231, 45, 55);
    const IColor verify(255, 68, 214, 255);
    const IColor brand(255, 202, 145, 255);

    const bool recording = mPlug.TakeRecording();
    const auto midiMapping = mPlug.ShowMidiMapping();
    const bool midiShowEnabled = midiMapping.enabled;

    DrawBrandHeader(g, background, text, muted, brand);
    DrawTopLiveTab(g, raised, line, text, brand);

    g.FillRect(background, footer);
    g.DrawLine(line, footer.L, footer.T, footer.R, footer.T, nullptr, 1.0F);

    IColor railColor = brand;
    if(mPlug.TakeOutputLive() || recording)
      railColor = danger;
    else if(mPlug.TakeOutputArmed())
      railColor = warning;
    else if(!mPlug.RuntimeHealthy() || mPlug.RenderingOffline())
      railColor = danger;
    else if(mPlug.TakePlaying())
      railColor = valid;
    else if(midiShowEnabled)
      railColor = verify;
    g.FillRect(railColor,
               IRECT(footer.L, footer.T, footer.R, footer.T + 3.0F));

    static constexpr const char* labels[] = {
        "NUEVO", "ABRIR", "GUARDAR", "GUARDAR COMO", "EN VIVO"};
    const bool projectChangeBlocked = recording;
    for(std::size_t index = 0; index < mButtons.size(); ++index)
    {
      if(mButtons[index].W() <= 0.0F || mButtons[index].H() <= 0.0F)
        continue;
      const bool blocked = projectChangeBlocked && index < 2U;
      const bool liveButton = index == mButtons.size() - 1U;
      const IColor buttonFill = liveButton
          ? IColor(255, 23, 17, 29)
          : (blocked ? IColor(255, 35, 31, 25) : raised);
      const IColor buttonLine = liveButton
          ? brand
          : (blocked ? warning : line);
      g.FillRoundRect(buttonFill, mButtons[index], 5.0F);
      g.DrawRoundRect(buttonLine, mButtons[index], 5.0F, nullptr, 1.0F);
      g.DrawText(IText(12.0F,
                       liveButton ? brand : (blocked ? warning : text),
                       "AeylaUI", EAlign::Center, EVAlign::Middle),
                 labels[index], mButtons[index]);
    }

    std::string projectLabel = mPlug.ProjectDirty() ? "SIN GUARDAR  ·  " : "GUARDADO  ·  ";
    projectLabel += mPlug.CurrentProjectPath().empty()
        ? mPlug.ProjectName()
        : mPlug.CurrentProjectPath().filename().string();
    g.DrawText(IText(12.0F, mPlug.ProjectDirty() ? warning : valid,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               projectLabel.c_str(), mProjectStatus);

    const std::string backend = mPlug.OutputBackendStatus();
    g.DrawText(IText(12.0F, mPlug.BackendReady() ? valid : warning,
                     "AeylaUI", EAlign::Center, EVAlign::Middle),
               backend.c_str(), mBackendStatus);

    std::string state;
    if(mPlug.TakeOutputLive())
      state = "TOMA AL AIRE";
    else if(mPlug.TakeOutputArmed())
      state = "ARMADA · CARRIER ACTIVO";
    else if(recording)
      state = "CAPTURANDO AVOLITES";
    else if(mPlug.TakePlaying())
      state = "PLAY · REPRODUCIENDO";
    else
      state = "LISTO / DESARMADO";

    if(mPlug.RenderingOffline())
      state = "RENDER OFFLINE · SALIDA INHIBIDA";
    else if(!mPlug.RuntimeHealthy())
      state = "FALLA DEL MOTOR";

    IColor stateColor = valid;
    IColor stateFill(255, 10, 18, 16);
    if(mPlug.TakeOutputLive()) {
      stateColor = danger;
      stateFill = IColor(255, 49, 14, 22);
    }
    else if(mPlug.TakeOutputArmed()) {
      stateColor = warning;
      stateFill = IColor(255, 39, 29, 14);
    }
    else if(recording) {
      stateColor = danger;
      stateFill = IColor(255, 49, 14, 22);
    }
    else if(mPlug.TakePlaying()) {
      stateColor = valid;
      stateFill = IColor(255, 10, 28, 18);
    }
    else if(mPlug.RenderingOffline() || !mPlug.RuntimeHealthy()) {
      stateColor = danger;
      stateFill = IColor(255, 49, 14, 22);
    }
    else if(midiShowEnabled) {
      stateColor = verify;
      stateFill = IColor(255, 8, 18, 24);
    }

    const IRECT outputAura(mOutputStatus.L - 4.0F, mOutputStatus.T + 4.0F,
                           mOutputStatus.R + 4.0F, mOutputStatus.B - 4.0F);
    g.FillRoundRect(recording || mPlug.TakeOutputLive()
                        ? IColor(42, 231, 45, 55)
                        : (mPlug.TakePlaying()
                              ? IColor(32, 57, 211, 132)
                              : (mPlug.TakeOutputArmed()
                                    ? IColor(34, 245, 154, 43)
                                    : IColor(24, 202, 145, 255))),
                    outputAura, 8.0F);
    g.FillRoundRect(stateFill, mOutputStatus, 6.0F);
    g.DrawRoundRect(stateColor, mOutputStatus, 6.0F, nullptr, 1.0F);

    const float mid = mOutputStatus.T + mOutputStatus.H() * 0.53F;
    const IRECT authorityLine(mOutputStatus.L + 8.0F, mOutputStatus.T,
                              mOutputStatus.R - 8.0F, mid);
    const IRECT midiLine(mOutputStatus.L + 8.0F, mid - 2.0F,
                         mOutputStatus.R - 8.0F, mOutputStatus.B);
    g.DrawText(IText(11.0F, stateColor, "AeylaUI", EAlign::Far,
                     EVAlign::Middle),
               state.c_str(), authorityLine);

    const std::string captureKeys =
        "N" + std::to_string(midiMapping.capture_start_note) + " START / N" +
        std::to_string(midiMapping.capture_stop_note) + " STOP";
    const std::string midiRecState = recording
        ? captureKeys + " · CAPTURANDO"
        : (midiShowEnabled
              ? captureKeys + " · MIDI LISTO"
              : captureKeys + " · MIDI SHOW OFF");
    g.DrawText(IText(9.5F,
                     recording ? danger :
                         (midiShowEnabled ? verify : muted),
                     "AeylaUI", EAlign::Far, EVAlign::Middle),
               midiRecState.c_str(), midiLine);

    DrawOperatorFrame(g);
  }

  bool IsHit(float x, float y) const override
  {
    if(mLiveOpen)
      return Contains(mRECT, x, y);
    const IRECT topLive = TopLiveTab();
    return Footer().Contains(x, y) ||
           (topLive.W() > 0.0F && Contains(topLive, x, y));
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)mod;
    if(mLiveOpen)
    {
      HandleLiveMouseDown(x, y);
      return;
    }

    const IRECT topLive = TopLiveTab();
    if(topLive.W() > 0.0F && Contains(topLive, x, y))
    {
      OpenLiveWorkspace();
      return;
    }

    BuildButtons();
    if(Contains(mButtons[0], x, y))
    {
      if(mPlug.TakeRecording())
      {
        ReportFileStatus(mPlug.NewProjectFromUI());
        return;
      }
      ConfirmDiscardThen([this]() {
        ReportFileStatus(mPlug.NewProjectFromUI());
        SetDirty(false);
      });
      return;
    }
    if(Contains(mButtons[1], x, y))
    {
      if(mPlug.TakeRecording())
      {
        ReportFileStatus(mPlug.OpenProjectFromUI(std::filesystem::path{}));
        return;
      }
      ConfirmDiscardThen([this]() { PromptOpen(); });
      return;
    }
    if(Contains(mButtons[2], x, y))
    {
      if(mPlug.CurrentProjectPath().empty()) PromptSaveAs();
      else ReportFileStatus(mPlug.SaveProjectFromUI());
      SetDirty(false);
      return;
    }
    if(Contains(mButtons[3], x, y))
    {
      PromptSaveAs();
      return;
    }
    if(mButtons[4].W() > 0.0F && Contains(mButtons[4], x, y))
    {
      OpenLiveWorkspace();
    }
  }

  void OnMouseDrag(float x, float y, float dX, float dY,
                   const IMouseMod& mod) override
  {
    (void)y;
    (void)dX;
    (void)dY;
    (void)mod;
    if(!mLiveOpen || mDraggingMemory < 0)
      return;
    ApplyFaderFromX(static_cast<std::size_t>(mDraggingMemory), x);
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    (void)x;
    (void)y;
    (void)mod;
    mDraggingMemory = -1;
  }

private:
  [[nodiscard]] IRECT Footer() const noexcept
  {
    return IRECT(mRECT.L, mRECT.B - 50.0F, mRECT.R, mRECT.B);
  }

  [[nodiscard]] IRECT TopLiveTab() const noexcept
  {
    // At the minimum supported plugin width there is not enough room between
    // the existing technical tabs and ARM/PANIC. The footer EN VIVO button is
    // retained only as the compact fallback. At normal show width EN VIVO is
    // promoted to the top navigation where the operator can actually find it.
    if(mRECT.W() < 1080.0F)
      return {};
    return IRECT(mRECT.L + 624.0F, mRECT.T + 26.0F,
                 mRECT.L + 724.0F, mRECT.T + 72.0F);
  }

  static bool Contains(const IRECT& rectangle, float x, float y) noexcept
  {
    return x >= rectangle.L && x <= rectangle.R &&
           y >= rectangle.T && y <= rectangle.B;
  }

  static bool Empty(const WDL_String& value) noexcept
  {
    const char* text = value.Get();
    return text == nullptr || text[0] == '\0';
  }

  static std::filesystem::path PathFromUtf8(const char* text)
  {
    if(text == nullptr || text[0] == '\0')
      return {};
    const std::string bytes(text);
    std::u8string utf8;
    utf8.reserve(bytes.size());
    for(const unsigned char value : bytes)
      utf8.push_back(static_cast<char8_t>(value));
    return std::filesystem::path(utf8);
  }

  static std::filesystem::path DialogPath(const WDL_String& fileName,
                                          const WDL_String& path)
  {
    const auto file = PathFromUtf8(fileName.Get());
    if(file.is_absolute()) return file;
    return PathFromUtf8(path.Get()) / file;
  }

  void DrawBrandHeader(IGraphics& g,
                       const IColor& background,
                       const IColor& text,
                       const IColor& muted,
                       const IColor& brand)
  {
    const IRECT cover(mRECT.L + 8.0F, mRECT.T + 8.0F,
                      mRECT.L + 306.0F, mRECT.T + 82.0F);
    g.FillRect(background, cover);
    g.DrawText(IText(20.0F, text, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "RGB LIVE CONTROL",
               IRECT(cover.L + 6.0F, cover.T + 2.0F,
                     cover.R - 4.0F, cover.T + 34.0F));
    g.DrawText(IText(9.8F, muted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "RGB ESTUDIOS · SHOW / AEYLA",
               IRECT(cover.L + 6.0F, cover.T + 32.0F,
                     cover.R - 4.0F, cover.T + 52.0F));

    const float lineLeft = cover.L + 6.0F;
    const float lineRight = cover.L + 132.0F;
    g.DrawLine(IColor(255, 232, 166, 201), lineLeft, cover.T + 57.0F,
               lineRight - 10.0F, cover.T + 57.0F, nullptr, 1.0F);
    g.DrawLine(brand, lineLeft + 9.0F, cover.T + 60.0F,
               lineRight, cover.T + 60.0F, nullptr, 1.0F);
    g.DrawLine(IColor(255, 137, 216, 255), lineLeft, cover.T + 63.0F,
               lineRight - 18.0F, cover.T + 63.0F, nullptr, 1.0F);

    const IRECT badge(cover.L + 148.0F, cover.T + 53.0F,
                      cover.R - 5.0F, cover.T + 72.0F);
    g.FillRoundRect(IColor(255, 24, 17, 31), badge, 4.0F);
    g.DrawRoundRect(brand, badge, 4.0F, nullptr, 1.0F);
    g.DrawText(IText(8.8F, brand, "AeylaUI", EAlign::Center, EVAlign::Middle),
               "R10.2 PRETEST", badge.GetPadded(-2.0F));
  }

  void DrawTopLiveTab(IGraphics& g,
                      const IColor& raised,
                      const IColor& line,
                      const IColor& text,
                      const IColor& brand)
  {
    const IRECT tab = TopLiveTab();
    if(tab.W() <= 0.0F)
      return;
    g.FillRoundRect(IColor(255, 24, 17, 31), tab, 6.0F);
    g.DrawRoundRect(brand, tab, 6.0F, nullptr, 1.2F);
    g.DrawText(IText(11.5F, brand, "AeylaUI", EAlign::Center, EVAlign::Middle),
               "EN VIVO", tab.GetPadded(-4.0F));
    (void)raised;
    (void)line;
    (void)text;
  }

  void DrawOperatorFrame(IGraphics& g)
  {
    const bool recording = mPlug.TakeRecording();
    const bool playing = !recording && mPlug.TakePlaying();
    if(!recording && !playing)
      return;

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const double milliseconds = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    const float pulse = static_cast<float>(
        0.5 + 0.5 * std::sin(milliseconds * 0.008));
    const int alphaOuter = static_cast<int>(85.0F + pulse * 95.0F);
    const int alphaInner = static_cast<int>(180.0F + pulse * 70.0F);

    const IColor outer = recording
        ? IColor(alphaOuter, 231, 45, 55)
        : IColor(alphaOuter, 57, 211, 132);
    const IColor inner = recording
        ? IColor(alphaInner, 244, 52, 63)
        : IColor(alphaInner, 66, 226, 145);

    const IRECT frame = mRECT.GetPadded(-3.0F);
    g.DrawRoundRect(outer, frame, 10.0F, nullptr, 4.0F);
    g.DrawRoundRect(inner, frame.GetPadded(-2.0F), 8.0F, nullptr, 1.5F);

    const float pillWidth = recording ? 76.0F : 86.0F;
    const IRECT pill(mRECT.MW() - pillWidth * 0.5F, mRECT.T + 4.0F,
                     mRECT.MW() + pillWidth * 0.5F, mRECT.T + 27.0F);
    g.FillRoundRect(IColor(245, 10, 11, 15), pill, 5.0F);
    g.DrawRoundRect(inner, pill, 5.0F, nullptr, 1.2F);
    g.DrawText(IText(10.5F, inner, "AeylaUI", EAlign::Center, EVAlign::Middle),
               recording ? "●  REC" : "▶  PLAY", pill);
  }

  void OpenLiveWorkspace()
  {
    mLiveOpen = true;
    mLiveMessageError = false;
    mLiveMessage = "EN VIVO · PREPARADA no reemplaza AL AIRE hasta una acción de reproducción.";
    SetDirty(false);
  }

  void BuildButtons()
  {
    const IRECT footer = Footer();
    constexpr float left = 12.0F;
    constexpr float topPad = 8.0F;
    constexpr float gap = 6.0F;
    constexpr std::array<float, 5U> widths{
        58.0F, 58.0F, 68.0F, 98.0F, 76.0F};
    const bool topLiveAvailable = TopLiveTab().W() > 0.0F;
    float cursor = footer.L + left;
    for(std::size_t index = 0; index < mButtons.size(); ++index)
    {
      if(topLiveAvailable && index == 4U)
      {
        mButtons[index] = IRECT{};
        continue;
      }
      mButtons[index] = IRECT(cursor, footer.T + topPad,
                              cursor + widths[index], footer.B - topPad);
      cursor += widths[index] + gap;
    }

    const float lastRight = topLiveAvailable ? mButtons[3].R : mButtons[4].R;
    const float statusLeft = lastRight + 14.0F;
    const float statusRight = footer.R - 14.0F;
    const float available = std::max(0.0F, statusRight - statusLeft);
    const float projectRight = statusLeft + available * 0.36F;
    const float backendRight = projectRight + available * 0.26F;
    mProjectStatus = IRECT(statusLeft, footer.T, projectRight, footer.B);
    mBackendStatus = IRECT(projectRight, footer.T, backendRight, footer.B);
    mOutputStatus = IRECT(backendRight, footer.T + 6.0F,
                          statusRight, footer.B - 6.0F);
  }

  void BuildLiveLayout()
  {
    const float left = mRECT.L + 18.0F;
    const float right = mRECT.R - 18.0F;
    const float top = mRECT.T + 18.0F;
    mLiveCloseButton = IRECT(right - 78.0F, top, right, top + 34.0F);
    mLivePanicButton = IRECT(right - 210.0F, top, right - 88.0F, top + 34.0F);
    mLiveArmButton = IRECT(right - 354.0F, top, right - 220.0F, top + 34.0F);

    const float transportTop = top + 54.0F;
    constexpr std::array<float, 4U> widths{82.0F, 112.0F, 82.0F, 82.0F};
    float tx = left;
    for(std::size_t index = 0U; index < mLiveTransport.size(); ++index)
    {
      mLiveTransport[index] = IRECT(tx, transportTop,
                                   tx + widths[index], transportTop + 36.0F);
      tx += widths[index] + 8.0F;
    }

    const float contentTop = transportTop + 52.0F;
    const float contentBottom = mRECT.B - 66.0F;
    const float split = left + (right - left) * 0.36F;
    mLiveSetlistPanel = IRECT(left, contentTop, split - 10.0F, contentBottom);
    mLiveMemoryPanel = IRECT(split + 10.0F, contentTop, right, contentBottom);
    mLiveMessageRect = IRECT(left, contentBottom + 10.0F,
                             right, mRECT.B - 12.0F);

    const std::size_t songCount = std::min<std::size_t>(
        mPlug.SongCount(), mLiveSongRows.size());
    const float rowTop = mLiveSetlistPanel.T + 38.0F;
    const float availableRows = std::max(1.0F, mLiveSetlistPanel.B - rowTop - 10.0F);
    const float rowHeight = std::clamp(
        availableRows / std::max<std::size_t>(songCount, 1U), 24.0F, 34.0F);
    for(std::size_t index = 0U; index < mLiveSongRows.size(); ++index)
    {
      if(index >= songCount)
      {
        mLiveSongRows[index] = IRECT{};
        continue;
      }
      const float y = rowTop + static_cast<float>(index) * rowHeight;
      mLiveSongRows[index] = IRECT(mLiveSetlistPanel.L + 8.0F, y,
                                   mLiveSetlistPanel.R - 8.0F,
                                   y + rowHeight - 3.0F);
    }

    const float gridLeft = mLiveMemoryPanel.L + 8.0F;
    const float gridRight = mLiveMemoryPanel.R - 8.0F;
    const float gridTop = mLiveMemoryPanel.T + 40.0F;
    const float gridBottom = mLiveMemoryPanel.B - 8.0F;
    constexpr float gridGap = 10.0F;
    const float cardW = (gridRight - gridLeft - gridGap) * 0.5F;
    const float cardH = (gridBottom - gridTop - gridGap) * 0.5F;

    for(std::size_t index = 0U; index < mLiveMemoryCards.size(); ++index)
    {
      const std::size_t column = index % 2U;
      const std::size_t row = index / 2U;
      const float x = gridLeft + static_cast<float>(column) * (cardW + gridGap);
      const float y = gridTop + static_cast<float>(row) * (cardH + gridGap);
      const IRECT card(x, y, x + cardW, y + cardH);
      mLiveMemoryCards[index] = card;

      const float chipTop = card.T + 35.0F;
      const float chipH = 24.0F;
      const float usable = card.W() - 20.0F;
      const float chipW = (usable - 18.0F) * 0.25F;
      float chipX = card.L + 10.0F;
      mLiveLearnButtons[index] = IRECT(chipX, chipTop,
                                       chipX + chipW, chipTop + chipH);
      chipX += chipW + 6.0F;
      mLiveModeButtons[index] = IRECT(chipX, chipTop,
                                      chipX + chipW, chipTop + chipH);
      chipX += chipW + 6.0F;
      mLiveFadeButtons[index] = IRECT(chipX, chipTop,
                                      chipX + chipW, chipTop + chipH);
      chipX += chipW + 6.0F;
      mLiveMidiButtons[index] = IRECT(chipX, chipTop,
                                      card.R - 10.0F, chipTop + chipH);

      const float controlTop = chipTop + chipH + 10.0F;
      const float controlBottom = card.B - 10.0F;
      mLiveMainButtons[index] = IRECT(card.L + 10.0F, controlTop,
                                      card.R - 10.0F, controlBottom);
      const float trackY = controlTop + (controlBottom - controlTop) * 0.54F;
      mLiveFaders[index] = IRECT(card.L + 22.0F, trackY - 5.0F,
                                 card.R - 22.0F, trackY + 5.0F);
    }
  }

  void DrawLive(IGraphics& g)
  {
    BuildLiveLayout();
    const IColor background(255, 8, 9, 12);
    const IColor panel(255, 12, 14, 18);
    const IColor raised(255, 21, 23, 29);
    const IColor line(255, 47, 51, 62);
    const IColor text(255, 226, 229, 234);
    const IColor muted(255, 136, 146, 160);
    const IColor warning(255, 245, 154, 43);
    const IColor valid(255, 57, 211, 132);
    const IColor danger(255, 231, 45, 55);
    const IColor verify(255, 68, 214, 255);
    const IColor brand(255, 202, 145, 255);

    g.FillRect(background, mRECT);
    g.DrawText(IText(22.0F, text, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "RGB LIVE CONTROL",
               IRECT(mRECT.L + 20.0F, mRECT.T + 5.0F,
                     mRECT.L + 275.0F, mRECT.T + 36.0F));
    g.DrawText(IText(10.0F, brand, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "EN VIVO · RGB ESTUDIOS · SHOW / AEYLA · R10.2 PRETEST",
               IRECT(mRECT.L + 20.0F, mRECT.T + 34.0F,
                     mRECT.L + 390.0F, mRECT.T + 57.0F));

    const auto tx = mPlug.ArtNetOutputStatus();
    const auto rx = mPlug.ArtNetCaptureStatus();
    std::string authority = "SALIDA DESARMADA";
    IColor authorityColor = muted;
    if(mPlug.TakeOutputLive()) {
      authority = "AL AIRE · " + std::to_string(tx.configured_fps) + " Hz";
      authorityColor = danger;
    }
    else if(mPlug.TakeOutputArmed()) {
      authority = "ARMADA · CARRIER " + std::to_string(tx.configured_fps) + " Hz";
      authorityColor = warning;
    }
    DrawStatusPill(g,
                   IRECT(mRECT.L + 405.0F, mRECT.T + 18.0F,
                         mRECT.L + 610.0F, mRECT.T + 48.0F),
                   authority, authorityColor);
    DrawStatusPill(g,
                   IRECT(mRECT.L + 620.0F, mRECT.T + 18.0F,
                         mRECT.L + 805.0F, mRECT.T + 48.0F),
                   rx.signal_present ? "AVOLITES RX OK" : "AVOLITES RX SIN SEÑAL",
                   rx.signal_present ? valid : warning);

    g.FillRoundRect(mPlug.TakeOutputArmed() ? IColor(255, 39, 29, 14) : raised,
                    mLiveArmButton, 5.0F);
    g.DrawRoundRect(mPlug.TakeOutputArmed() ? warning : line,
                    mLiveArmButton, 5.0F, nullptr, 1.0F);
    g.DrawText(IText(10.5F, mPlug.TakeOutputArmed() ? warning : text,
                     "AeylaUI", EAlign::Center, EVAlign::Middle),
               mPlug.TakeOutputArmed() ? "DESARMAR SALIDA" : "ARMAR SALIDA",
               mLiveArmButton);

    const bool blackout = mPlug.GlobalBlackout();
    g.FillRoundRect(blackout ? IColor(255, 74, 20, 26) : IColor(255, 49, 14, 22),
                    mLivePanicButton, 5.0F);
    g.DrawRoundRect(danger, mLivePanicButton, 5.0F, nullptr, 1.2F);
    g.DrawText(IText(10.5F, danger, "AeylaUI", EAlign::Center, EVAlign::Middle),
               blackout ? "SALIR APAGÓN" : "PANIC / APAGÓN",
               mLivePanicButton);

    g.FillRoundRect(raised, mLiveCloseButton, 5.0F);
    g.DrawRoundRect(brand, mLiveCloseButton, 5.0F, nullptr, 1.0F);
    g.DrawText(IText(10.5F, brand, "AeylaUI", EAlign::Center, EVAlign::Middle),
               "VOLVER", mLiveCloseButton);

    static constexpr std::array<const char*, 4U> transportLabels{
        "PREV", "PLAY / PAUSA", "HOLD", "NEXT"};
    for(std::size_t index = 0U; index < mLiveTransport.size(); ++index)
    {
      const bool play = index == 1U;
      g.FillRoundRect(play && mPlug.TakePlaying()
                          ? IColor(255, 12, 46, 31)
                          : raised,
                      mLiveTransport[index], 5.0F);
      g.DrawRoundRect(play ? valid : line,
                      mLiveTransport[index], 5.0F, nullptr, 1.0F);
      g.DrawText(IText(10.5F, play ? valid : text,
                       "AeylaUI", EAlign::Center, EVAlign::Middle),
                 transportLabels[index], mLiveTransport[index]);
    }

    g.FillRoundRect(panel, mLiveSetlistPanel, 8.0F);
    g.DrawRoundRect(line, mLiveSetlistPanel, 8.0F, nullptr, 1.0F);
    g.DrawText(IText(12.0F, text, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "SETLIST",
               IRECT(mLiveSetlistPanel.L + 12.0F, mLiveSetlistPanel.T + 5.0F,
                     mLiveSetlistPanel.R - 12.0F, mLiveSetlistPanel.T + 27.0F));
    g.DrawText(IText(8.5F, muted, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "PREPARADA ≠ AL AIRE",
               IRECT(mLiveSetlistPanel.L + 12.0F, mLiveSetlistPanel.T + 22.0F,
                     mLiveSetlistPanel.R - 12.0F, mLiveSetlistPanel.T + 38.0F));

    const std::size_t songCount = std::min<std::size_t>(
        mPlug.SongCount(), mLiveSongRows.size());
    const std::size_t prepared = mPlug.ActiveSongIndex();
    const int live = mPlug.ActiveTakeSongIndex();
    for(std::size_t index = 0U; index < songCount; ++index)
    {
      const bool isLive = live >= 0 && static_cast<std::size_t>(live) == index;
      const bool isPrepared = prepared == index;
      IColor rowFill = raised;
      IColor rowLine = line;
      IColor rowText = text;
      std::string badge;
      if(isLive) {
        rowFill = IColor(255, 49, 14, 22);
        rowLine = danger;
        rowText = danger;
        badge = "AL AIRE";
      }
      if(isPrepared && !isLive) {
        rowFill = IColor(255, 8, 18, 24);
        rowLine = verify;
        rowText = verify;
        badge = "PREPARADA";
      }
      g.FillRoundRect(rowFill, mLiveSongRows[index], 4.0F);
      g.DrawRoundRect(rowLine, mLiveSongRows[index], 4.0F, nullptr, 1.0F);
      const std::string label =
          (index + 1U < 10U ? "0" : "") + std::to_string(index + 1U) + "  " +
          mPlug.SongName(index);
      g.DrawText(IText(10.2F, rowText, "AeylaUI",
                       EAlign::Near, EVAlign::Middle),
                 label.c_str(), mLiveSongRows[index].GetPadded(-8.0F));
      if(!badge.empty())
        g.DrawText(IText(8.8F, rowText, "AeylaUI",
                         EAlign::Far, EVAlign::Middle),
                   badge.c_str(), mLiveSongRows[index].GetPadded(-8.0F));
    }

    g.FillRoundRect(panel, mLiveMemoryPanel, 8.0F);
    g.DrawRoundRect(line, mLiveMemoryPanel, 8.0F, nullptr, 1.0F);
    g.DrawText(IText(12.0F, text, "AeylaUI", EAlign::Near, EVAlign::Middle),
               "MEMORIAS OPERATIVAS",
               IRECT(mLiveMemoryPanel.L + 12.0F, mLiveMemoryPanel.T + 5.0F,
                     mLiveMemoryPanel.R - 12.0F, mLiveMemoryPanel.T + 27.0F));
    g.DrawText(IText(8.5F, muted, "AeylaUI", EAlign::Far, EVAlign::Middle),
               "DMX LEARN · NOTE / CC · MASKED LTP",
               IRECT(mLiveMemoryPanel.L + 12.0F, mLiveMemoryPanel.T + 6.0F,
                     mLiveMemoryPanel.R - 12.0F, mLiveMemoryPanel.T + 28.0F));

    for(std::size_t index = 0U; index < mLiveMemoryCards.size(); ++index)
      DrawMemoryModule(g, index, line, text, muted, warning, valid, verify, brand);

    const IColor messageColor = mLiveMessageError ? danger : muted;
    g.DrawText(IText(10.5F, messageColor, "AeylaUI",
                     EAlign::Near, EVAlign::Middle),
               mLiveMessage.c_str(), mLiveMessageRect);
  }

  void DrawMemoryModule(IGraphics& g,
                        std::size_t index,
                        const IColor& line,
                        const IColor& text,
                        const IColor& muted,
                        const IColor& warning,
                        const IColor& valid,
                        const IColor& verify,
                        const IColor& brand)
  {
    const auto view = mPlug.LiveMemoryViewFromUI(index);
    const bool active = view.level > 0.005F || view.target_level > 0.005F;
    const IColor accent = view.midi_learning ? brand
        : (view.learning ? verify
            : (!view.configured ? warning : (active ? valid : line)));
    const IColor cardFill = active
        ? IColor(255, 10, 25, 19)
        : IColor(255, 18, 20, 25);
    const auto& card = mLiveMemoryCards[index];
    g.FillRoundRect(cardFill, card, 7.0F);
    g.DrawRoundRect(accent, card, 7.0F, nullptr, active ? 1.5F : 1.0F);

    g.DrawText(IText(12.0F, active ? valid : text,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               view.name.c_str(),
               IRECT(card.L + 10.0F, card.T + 5.0F,
                     card.R - 96.0F, card.T + 29.0F));

    std::string status;
    if(view.learning)
      status = "DMX 1/2";
    else if(!view.configured)
      status = "SIN DMX";
    else
      status = std::to_string(view.channel_count) + " CH";
    const std::string levelText =
        std::to_string(static_cast<int>(std::lround(view.level * 100.0F))) + "%";
    g.DrawText(IText(10.0F, accent, "AeylaUI", EAlign::Far, EVAlign::Middle),
               (status + " · " + levelText).c_str(),
               IRECT(card.R - 150.0F, card.T + 5.0F,
                     card.R - 10.0F, card.T + 29.0F));

    const std::string learnLabel = view.learning ? "CAPT ON" : "DMX";
    DrawChip(g, mLiveLearnButtons[index], learnLabel,
             view.learning ? verify : (view.configured ? text : warning));
    DrawChip(g, mLiveModeButtons[index],
             view.mode == aeyla::output::LiveMemoryControlMode::toggle
                 ? "BOTÓN" : "FADER",
             view.mode == aeyla::output::LiveMemoryControlMode::fader
                 ? brand : text);
    const std::string fadeLabel = view.fade_ms == 100U ? "0.1 s"
        : (view.fade_ms == 1500U ? "1.5 s" : "1.0 s");
    DrawChip(g, mLiveFadeButtons[index], fadeLabel, text);
    DrawChip(g, mLiveMidiButtons[index], MidiLabel(view),
             view.midi_learning ? brand
                 : (view.midi_kind == aeyla::live_memory_session::MidiBindingKind::none
                        ? muted : valid));

    if(view.mode == aeyla::output::LiveMemoryControlMode::toggle)
      DrawTogglePad(g, index, view, text, muted, valid, line);
    else
      DrawFaderPad(g, index, view, text, muted, brand, line);
  }

  void DrawTogglePad(IGraphics& g,
                     std::size_t index,
                     const aeyla::live_memory_session::MemoryView& view,
                     const IColor& text,
                     const IColor& muted,
                     const IColor& valid,
                     const IColor& line)
  {
    const bool on = view.target_level > 0.5F;
    const auto& pad = mLiveMainButtons[index];
    g.FillRoundRect(on ? IColor(255, 12, 46, 31) : IColor(255, 11, 12, 16),
                    pad, 7.0F);
    g.DrawRoundRect(on ? valid : line, pad, 7.0F, nullptr, on ? 1.8F : 1.0F);
    g.DrawText(IText(16.0F, on ? valid : text,
                     "AeylaUI", EAlign::Center, EVAlign::Middle),
               on ? "ON" : "OFF", pad);
    g.DrawText(IText(8.5F, on ? valid : muted,
                     "AeylaUI", EAlign::Center, EVAlign::Bottom),
               "TOGGLE · MIDI NOTE",
               IRECT(pad.L + 6.0F, pad.T + 5.0F,
                     pad.R - 6.0F, pad.B - 6.0F));
  }

  void DrawFaderPad(IGraphics& g,
                    std::size_t index,
                    const aeyla::live_memory_session::MemoryView& view,
                    const IColor& text,
                    const IColor& muted,
                    const IColor& brand,
                    const IColor& line)
  {
    const auto& pad = mLiveMainButtons[index];
    const auto& track = mLiveFaders[index];
    const float normalized = std::clamp(view.level, 0.0F, 1.0F);
    const float handleX = track.L + track.W() * normalized;

    g.FillRoundRect(IColor(255, 11, 12, 16), pad, 7.0F);
    g.DrawRoundRect(view.transitioning ? brand : line,
                    pad, 7.0F, nullptr, 1.0F);

    g.DrawText(IText(9.5F, muted, "AeylaUI", EAlign::Near, EVAlign::Top),
               "0", IRECT(track.L, pad.T + 8.0F,
                          track.L + 30.0F, pad.T + 25.0F));
    g.DrawText(IText(9.5F, muted, "AeylaUI", EAlign::Far, EVAlign::Top),
               "100", IRECT(track.R - 40.0F, pad.T + 8.0F,
                            track.R, pad.T + 25.0F));

    g.FillRoundRect(IColor(255, 5, 7, 9), track, 5.0F);
    if(normalized > 0.0F)
    {
      const IRECT fill(track.L, track.T, handleX, track.B);
      g.FillRoundRect(brand, fill, 5.0F);
    }
    g.DrawRoundRect(line, track, 5.0F, nullptr, 1.0F);

    const IRECT handle(handleX - 7.0F, track.T - 11.0F,
                       handleX + 7.0F, track.B + 11.0F);
    g.FillRoundRect(IColor(255, 225, 229, 235), handle, 4.0F);
    g.DrawRoundRect(brand, handle, 4.0F, nullptr, 1.2F);

    const std::string percentage =
        std::to_string(static_cast<int>(std::lround(normalized * 100.0F))) + "%";
    g.DrawText(IText(15.0F, normalized > 0.0F ? brand : text,
                     "AeylaUI", EAlign::Center, EVAlign::Middle),
               percentage.c_str(),
               IRECT(pad.L + 10.0F, track.B + 14.0F,
                     pad.R - 10.0F, pad.B - 6.0F));
    g.DrawText(IText(8.5F, muted, "AeylaUI", EAlign::Center, EVAlign::Top),
               "FADER CONTINUO · MIDI CC",
               IRECT(pad.L + 10.0F, pad.T + 7.0F,
                     pad.R - 10.0F, track.T - 10.0F));
  }

  static std::string MidiLabel(
      const aeyla::live_memory_session::MemoryView& view)
  {
    if(view.midi_learning)
      return view.mode == aeyla::output::LiveMemoryControlMode::toggle
          ? "MIDI…NOTE" : "MIDI…CC";
    if(view.midi_kind == aeyla::live_memory_session::MidiBindingKind::note)
      return "N" + std::to_string(view.midi_number) +
             "/" + std::to_string(view.midi_channel);
    if(view.midi_kind ==
       aeyla::live_memory_session::MidiBindingKind::control_change)
      return "CC" + std::to_string(view.midi_number) +
             "/" + std::to_string(view.midi_channel);
    return "MIDI";
  }

  void DrawStatusPill(IGraphics& g,
                      const IRECT& rect,
                      const std::string& label,
                      const IColor& accent)
  {
    g.FillRoundRect(IColor(255, 13, 15, 19), rect, 6.0F);
    g.DrawRoundRect(accent, rect, 6.0F, nullptr, 1.0F);
    g.DrawText(IText(9.5F, accent, "AeylaUI",
                     EAlign::Center, EVAlign::Middle),
               label.c_str(), rect.GetPadded(-4.0F));
  }

  void DrawChip(IGraphics& g,
                const IRECT& rect,
                const std::string& label,
                const IColor& accent)
  {
    g.FillRoundRect(IColor(255, 15, 17, 22), rect, 4.0F);
    g.DrawRoundRect(accent, rect, 4.0F, nullptr, 1.0F);
    g.DrawText(IText(8.2F, accent, "AeylaUI",
                     EAlign::Center, EVAlign::Middle),
               label.c_str(), rect.GetPadded(-2.0F));
  }

  void HandleLiveMouseDown(float x, float y)
  {
    BuildLiveLayout();
    if(Contains(mLiveCloseButton, x, y))
    {
      mLiveOpen = false;
      mDraggingMemory = -1;
      SetDirty(false);
      return;
    }
    if(Contains(mLivePanicButton, x, y))
    {
      mPlug.SetBlackoutFromUI(!mPlug.GlobalBlackout());
      mLiveMessageError = false;
      mLiveMessage = mPlug.GlobalBlackout()
          ? "APAGÓN ACTIVO · salida desarmada y memorias EN VIVO en OFF."
          : "APAGÓN DESACTIVADO · la salida sigue desarmada hasta ARMAR SALIDA.";
      SetDirty(false);
      return;
    }
    if(Contains(mLiveArmButton, x, y))
    {
      ReportLive(mPlug.ToggleTakeOutputArmFromUI());
      SetDirty(false);
      return;
    }

    if(Contains(mLiveTransport[0], x, y))
    {
      if(!mPlug.SelectAdjacentSongFromUI(-1))
        SetLiveMessage(false, "PREV no pudo cambiar la canción preparada.");
      else
        SetLiveMessage(true, "PREV · canción anterior PREPARADA; AL AIRE no cambia.");
      SetDirty(false);
      return;
    }
    if(Contains(mLiveTransport[1], x, y))
    {
      ReportLive(mPlug.ToggleActiveTakePlaybackFromUI());
      SetDirty(false);
      return;
    }
    if(Contains(mLiveTransport[2], x, y))
    {
      mPlug.StopActiveTakePlaybackFromUI();
      SetLiveMessage(true, "HOLD · se conserva el último frame DMX y el carrier sigue activo.");
      SetDirty(false);
      return;
    }
    if(Contains(mLiveTransport[3], x, y))
    {
      if(!mPlug.SelectAdjacentSongFromUI(1))
        SetLiveMessage(false, "NEXT no pudo cambiar la canción preparada.");
      else
        SetLiveMessage(true, "NEXT · canción siguiente PREPARADA; AL AIRE no cambia.");
      SetDirty(false);
      return;
    }

    const std::size_t songCount = std::min<std::size_t>(
        mPlug.SongCount(), mLiveSongRows.size());
    for(std::size_t index = 0U; index < songCount; ++index)
    {
      if(!Contains(mLiveSongRows[index], x, y)) continue;
      if(mPlug.SelectSongFromUI(index))
        SetLiveMessage(true,
            "PREPARADA · " + mPlug.SongName(index) +
                " · la toma AL AIRE conserva autoridad hasta reproducción.");
      else
        SetLiveMessage(false, "No fue posible preparar esa canción.");
      SetDirty(false);
      return;
    }

    for(std::size_t index = 0U; index < mLiveMemoryCards.size(); ++index)
    {
      if(Contains(mLiveLearnButtons[index], x, y))
      {
        ReportLive(mPlug.LearnLiveMemoryFromAvolitesFromUI(index));
        SetDirty(false);
        return;
      }
      if(Contains(mLiveModeButtons[index], x, y))
      {
        ReportLive(mPlug.ToggleLiveMemoryModeFromUI(index));
        SetDirty(false);
        return;
      }
      if(Contains(mLiveFadeButtons[index], x, y))
      {
        ReportLive(mPlug.CycleLiveMemoryFadeFromUI(index, 1));
        SetDirty(false);
        return;
      }
      if(Contains(mLiveMidiButtons[index], x, y))
      {
        (void)mPlug.LiveMemoryViewFromUI(index);
        const auto result = aeyla::live_memory_session::arm_midi_learn(&mPlug, index);
        ReportLiveSession(result);
        SetDirty(false);
        return;
      }

      const auto view = mPlug.LiveMemoryViewFromUI(index);
      if(view.mode == aeyla::output::LiveMemoryControlMode::toggle &&
         Contains(mLiveMainButtons[index], x, y))
      {
        ReportLive(mPlug.ToggleLiveMemoryFromUI(index));
        SetDirty(false);
        return;
      }
      if(view.mode == aeyla::output::LiveMemoryControlMode::fader &&
         Contains(mLiveMainButtons[index], x, y))
      {
        mDraggingMemory = static_cast<int>(index);
        ApplyFaderFromX(index, x);
        SetDirty(false);
        return;
      }
    }
  }

  void ApplyFaderFromX(std::size_t index, float x)
  {
    if(index >= mLiveFaders.size()) return;
    const auto& rect = mLiveFaders[index];
    const float normalized = std::clamp(
        (x - rect.L) / std::max(1.0F, rect.W()), 0.0F, 1.0F);
    const auto result = mPlug.SetLiveMemoryLevelFromUI(index, normalized);
    if(!result.succeeded)
      ReportLive(result);
    else
      mLiveMessageError = false;
  }

  void SetLiveMessage(bool ok, std::string message)
  {
    mLiveMessageError = !ok;
    mLiveMessage = std::move(message);
  }

  void ReportLive(const aeyla::product::AuthoringResult& result)
  {
    SetLiveMessage(result.succeeded, result.message);
  }

  void ReportLiveSession(const aeyla::live_memory_session::ActionResult& result)
  {
    SetLiveMessage(result.succeeded, result.message);
  }

  void ReportFileStatus(const aeyla::product::ProjectFileStatus& status)
  {
    if(status.succeeded) return;
    std::string message = status.message;
    if(!status.diagnostics.empty())
      message += "\n\n" + status.diagnostics.front();
    GetUI()->ShowMessageBox(message.c_str(),
                            "RGB LIVE CONTROL · ERROR DE PROYECTO", kMB_OK);
  }

  void ConfirmDiscardThen(std::function<void()> action)
  {
    if(!mPlug.ProjectDirty())
    {
      action();
      return;
    }

    GetUI()->ShowMessageBox(
        "El show AEYLA tiene cambios sin guardar. ¿Continuar y descartarlos?",
        "RGB LIVE CONTROL · CAMBIOS SIN GUARDAR", kMB_YESNO,
        [action = std::move(action)](EMsgBoxResult result) {
          if(result == kYES) action();
        });
  }

  void PromptOpen()
  {
    mDialogFileName.Set("");
    mDialogPath.Set(mPlug.CurrentProjectPath().empty()
                        ? ""
                        : mPlug.CurrentProjectPath().parent_path().string().c_str());
    GetUI()->PromptForFile(
        mDialogFileName, mDialogPath, EFileAction::Open, ".aeylashow",
        [this](const WDL_String& fileName, const WDL_String& path) {
          if(Empty(fileName)) return;
          ReportFileStatus(mPlug.OpenProjectFromUI(DialogPath(fileName, path)));
          (void)mPlug.RefreshNetworkInterfacesFromUI();
          SetDirty(false);
        });
  }

  void PromptSaveAs()
  {
    mDialogFileName.Set(mPlug.CurrentProjectPath().empty()
                            ? "AEYLA-Show.aeylashow"
                            : mPlug.CurrentProjectPath().filename().string().c_str());
    mDialogPath.Set(mPlug.CurrentProjectPath().empty()
                        ? ""
                        : mPlug.CurrentProjectPath().parent_path().string().c_str());
    GetUI()->PromptForFile(
        mDialogFileName, mDialogPath, EFileAction::Save, ".aeylashow",
        [this](const WDL_String& fileName, const WDL_String& path) {
          if(Empty(fileName)) return;
          auto target = DialogPath(fileName, path);
          if(target.extension() != ".aeylashow") target += ".aeylashow";
          ReportFileStatus(mPlug.SaveProjectAsFromUI(target));
          SetDirty(false);
        });
  }

  AeylaVisualDmx& mPlug;
  std::array<IRECT, 5> mButtons{};
  IRECT mProjectStatus{};
  IRECT mBackendStatus{};
  IRECT mOutputStatus{};

  bool mLiveOpen{false};
  bool mLiveMessageError{false};
  int mDraggingMemory{-1};
  std::string mLiveMessage{"EN VIVO · memorias en OFF hasta aprendizaje y ARM explícito."};
  IRECT mLiveCloseButton{};
  IRECT mLivePanicButton{};
  IRECT mLiveArmButton{};
  std::array<IRECT, 4> mLiveTransport{};
  IRECT mLiveSetlistPanel{};
  IRECT mLiveMemoryPanel{};
  IRECT mLiveMessageRect{};
  std::array<IRECT, 15> mLiveSongRows{};
  std::array<IRECT, 4> mLiveMemoryCards{};
  std::array<IRECT, 4> mLiveLearnButtons{};
  std::array<IRECT, 4> mLiveModeButtons{};
  std::array<IRECT, 4> mLiveFadeButtons{};
  std::array<IRECT, 4> mLiveMidiButtons{};
  std::array<IRECT, 4> mLiveMainButtons{};
  std::array<IRECT, 4> mLiveFaders{};

  WDL_String mDialogFileName;
  WDL_String mDialogPath;
};

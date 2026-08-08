#pragma once

#include "AeylaVisualDmx.h"

#include <array>
#include <cstdio>
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
    BuildButtons();

    const IRECT footer(mRECT.L, mRECT.B - 42.0F, mRECT.R, mRECT.B);
    const IColor background(255, 8, 9, 12);
    const IColor raised(255, 21, 23, 29);
    const IColor line(255, 47, 51, 62);
    const IColor text(255, 226, 229, 234);
    const IColor muted(255, 139, 145, 158);
    const IColor warning(255, 245, 154, 43);
    const IColor valid(255, 57, 211, 132);
    const IColor danger(255, 231, 45, 55);

    // The top-most status control owns the ARM interaction as well as the
    // footer. This removes an ambiguous state where the lower visual control
    // looked clickable but an ARM request could be rejected silently by the
    // safety model. A visible control must either work or explain why it cannot.
    DrawArmState(g, raised, line, text, muted, warning, valid, danger);

    g.FillRect(background, footer);
    g.DrawLine(line, footer.L, footer.T, footer.R, footer.T, nullptr, 1.0F);

    static constexpr const char* labels[] = {
        "NEW", "OPEN", "SAVE", "SAVE AS", "SET SONG START"};
    for(std::size_t index = 0; index < mButtons.size(); ++index)
    {
      g.FillRoundRect(raised, mButtons[index], 5.0F);
      g.DrawRoundRect(line, mButtons[index], 5.0F, nullptr, 1.0F);
      g.DrawText(IText(9.0F, text, "AeylaUI", EAlign::Center, EVAlign::Middle),
                 labels[index], mButtons[index]);
    }

    const bool dirty = mPlug.ProjectDirty();
    const auto& fileStatus = mPlug.ProjectFileStatus();
    const auto& currentPath = mPlug.CurrentProjectPath();

    std::string projectLabel = dirty ? "UNSAVED  ·  " : "SAVED  ·  ";
    projectLabel += mPlug.ProjectName();
    if(!currentPath.empty())
      projectLabel += "  ·  " + currentPath.filename().string();

    IColor projectColor = dirty ? warning : valid;
    if(fileStatus.operation != aeyla::product::ProjectFileOperation::none &&
       !fileStatus.succeeded)
      projectColor = danger;

    g.DrawText(IText(9.0F, projectColor, "AeylaUI", EAlign::Near, EVAlign::Middle),
               projectLabel.c_str(),
               IRECT(footer.L + 390.0F, footer.T, footer.L + 620.0F, footer.B));

    char runtime[160];
    std::snprintf(runtime, sizeof(runtime),
                  "DMX %llu  ·  %d CH  ·  DROP %llu  ·  STATE %llu",
                  static_cast<unsigned long long>(mPlug.DmxGeneration()),
                  mPlug.DmxNonZeroChannels(),
                  static_cast<unsigned long long>(mPlug.DroppedMidiEvents()),
                  static_cast<unsigned long long>(mPlug.HostStateRestoreErrors()));
    g.DrawText(IText(9.0F, muted, "AeylaUI", EAlign::Center, EVAlign::Middle),
               runtime,
               IRECT(footer.L + 620.0F, footer.T,
                     footer.L + 850.0F, footer.B));

    char state[220];
    const char* project = mPlug.ProjectValid() ? "PROJECT VALID" : "PROJECT INVALID";
    const char* backend = mPlug.BackendReady() ? "BACKEND READY" : "BACKEND OFF";
    const char* output = mPlug.OutputArmed() ? "ARMED" : "DISARMED";
    const char* blackout = mPlug.EffectiveBlackout() ? "BLACKOUT" : "PREVIEW";
    const char* binding = mPlug.ActiveSongBound() ? "START SET" : "START UNSET";
    const char* offline = !mPlug.RuntimeHealthy() ? "RUNTIME FAULT" :
        (mPlug.RenderingOffline() ? "OFFLINE INHIBIT" : "REALTIME");
    std::snprintf(state, sizeof(state), "%s  ·  %s  ·  %s  ·  %s  ·  %s  ·  %s",
                  project, backend, output, blackout, binding, offline);

    IColor stateColor = mPlug.BackendReady() ? valid : warning;
    if(mPlug.OutputArmed() || !mPlug.ProjectValid() || !mPlug.RuntimeHealthy() ||
       mPlug.RenderingOffline() ||
       mPlug.HostStateRestoreErrors() > 0U)
      stateColor = danger;

    g.DrawText(IText(9.0F, stateColor, "AeylaUI", EAlign::Far, EVAlign::Middle),
               state,
               IRECT(footer.L + 850.0F, footer.T,
                     footer.R - 14.0F, footer.B));
  }

  bool IsHit(float x, float y) const override
  {
    // The control is visually full-window but must never swallow the editor.
    // It owns only the file/runtime footer and the ARM button for explicit
    // safety feedback. Everything else passes through to the actual editor.
    const IRECT footer(mRECT.L, mRECT.B - 42.0F, mRECT.R, mRECT.B);
    return footer.Contains(x, y) || ArmButton().Contains(x, y);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    BuildButtons();

    if(Contains(ArmButton(), x, y))
    {
      HandleArmClick();
      return;
    }

    if(Contains(mButtons[0], x, y))
    {
      ConfirmDiscardThen([this]() {
        ReportFileStatus(mPlug.NewProjectFromUI());
        SetDirty(false);
      });
      return;
    }

    if(Contains(mButtons[1], x, y))
    {
      ConfirmDiscardThen([this]() { PromptOpen(); });
      return;
    }

    if(Contains(mButtons[2], x, y))
    {
      if(mPlug.CurrentProjectPath().empty())
        PromptSaveAs();
      else
      {
        ReportFileStatus(mPlug.SaveProjectFromUI());
        SetDirty(false);
      }
      return;
    }

    if(Contains(mButtons[3], x, y))
    {
      PromptSaveAs();
      return;
    }

    if(Contains(mButtons[4], x, y))
    {
      if(!mPlug.SetActiveSongStartFromPlayheadFromUI())
      {
        GetUI()->ShowMessageBox(
            "AEYLA could not bind the active Song.\n\n"
            "Load a programmed Song and place the DAW playhead at its exact "
            "start before pressing SET SONG START.",
            "AEYLA · SONG START NOT SET",
            kMB_OK);
      }
      SetDirty(false);
    }
  }

private:
  static bool Contains(const IRECT& rectangle, float x, float y) noexcept
  {
    return x >= rectangle.L && x <= rectangle.R &&
           y >= rectangle.T && y <= rectangle.B;
  }

  [[nodiscard]] IRECT ArmButton() const noexcept
  {
    constexpr float headerHeight = 70.0F;
    const IRECT header(mRECT.L, mRECT.T, mRECT.R, mRECT.T + headerHeight);
    return IRECT(header.R - 292.0F, header.T + 16.0F,
                 header.R - 164.0F, header.B - 16.0F);
  }

  void DrawArmState(IGraphics& g,
                    const IColor& raised,
                    const IColor& line,
                    const IColor& text,
                    const IColor& muted,
                    const IColor& warning,
                    const IColor& valid,
                    const IColor& danger)
  {
    const IRECT arm = ArmButton();
    const bool armed = mPlug.OutputArmed();
    const bool projectValid = mPlug.ProjectValid();
    const bool backendReady = mPlug.BackendReady();
    const bool runtimeHealthy = mPlug.RuntimeHealthy();
    const bool renderingOffline = mPlug.RenderingOffline();

    const char* label = "ARM OUTPUT";
    IColor fill = raised;
    IColor labelColor = text;

    if(armed)
    {
      label = "OUTPUT ARMED";
      fill = valid;
      labelColor = IColor(255, 7, 30, 20);
    }
    else if(!projectValid)
    {
      label = "ARM LOCKED · PROJECT";
      fill = IColor(255, 35, 30, 24);
      labelColor = warning;
    }
    else if(!runtimeHealthy)
    {
      label = "ARM LOCKED · RUNTIME";
      fill = IColor(255, 48, 18, 24);
      labelColor = danger;
    }
    else if(renderingOffline)
    {
      label = "ARM LOCKED · OFFLINE";
      fill = IColor(255, 48, 18, 24);
      labelColor = danger;
    }
    else if(!backendReady)
    {
      label = "ARM LOCKED · BACKEND";
      fill = IColor(255, 30, 30, 34);
      labelColor = muted;
    }

    g.FillRoundRect(fill, arm, 7.0F);
    g.DrawRoundRect(armed ? danger : line, arm, 7.0F, nullptr,
                    armed ? 2.0F : 1.0F);
    g.DrawText(IText(9.0F, labelColor, "AeylaUI", EAlign::Center, EVAlign::Middle),
               label, arm.GetPadded(-4.0F));
  }

  void HandleArmClick()
  {
    if(mPlug.OutputArmed())
    {
      mPlug.ForceDisarmFromUI();
      SetDirty(false);
      return;
    }

    if(!mPlug.ProjectValid())
    {
      GetUI()->ShowMessageBox(
          "ARM is locked because the current AEYLA project is invalid.\n\n"
          "Open or repair a valid .aeylashow before enabling physical output.",
          "AEYLA · ARM LOCKED",
          kMB_OK);
      return;
    }

    if(!mPlug.RuntimeHealthy() || mPlug.RenderingOffline())
    {
      GetUI()->ShowMessageBox(
          mPlug.RenderingOffline()
              ? "ARM is locked while the host is rendering offline.\n\n"
                "AEYLA remains disarmed and blacked out; physical network "
                "output is inhibited. Return to realtime playback and arm "
                "again explicitly."
              : "ARM is locked because the independent lighting runtime "
                "reported a fault.\n\nReload the plugin before attempting "
                "physical output.",
          "AEYLA · ARM LOCKED",
          kMB_OK);
      return;
    }

    if(!mPlug.BackendReady())
    {
      GetUI()->ShowMessageBox(
          "ARM is locked because the physical output backend is not connected yet.\n\n"
          "This build is intentionally PREVIEW / NO DMX. Art-Net will only become "
          "armable after backend preflight and safety integration pass.",
          "AEYLA · ARM LOCKED",
          kMB_OK);
      return;
    }

    mPlug.ToggleOutputArmFromUI();
    if(!mPlug.OutputArmed())
    {
      GetUI()->ShowMessageBox(
          "ARM request was rejected by the runtime safety gate.\n\n"
          "The lighting show is not performance-ready yet (for example, no valid "
          "lighting sequence/cue program is loaded).",
          "AEYLA · SHOW NOT READY",
          kMB_OK);
    }
    SetDirty(false);
  }

  static bool Empty(const WDL_String& value) noexcept
  {
    const char* text = value.Get();
    return text == nullptr || text[0] == '\0';
  }

  static std::filesystem::path DialogPath(const WDL_String& fileName,
                                          const WDL_String& path)
  {
    const auto file = std::filesystem::u8path(fileName.Get());
    if(file.is_absolute())
      return file;
    return std::filesystem::u8path(path.Get()) / file;
  }

  void BuildButtons()
  {
    const IRECT footer(mRECT.L, mRECT.B - 42.0F, mRECT.R, mRECT.B);
    constexpr float left = 12.0F;
    constexpr float topPad = 8.0F;
    constexpr float gap = 6.0F;
    constexpr float widths[] = {52.0F, 56.0F, 54.0F, 68.0F, 112.0F};

    float cursor = footer.L + left;
    for(std::size_t index = 0; index < mButtons.size(); ++index)
    {
      mButtons[index] = IRECT(cursor, footer.T + topPad,
                              cursor + widths[index], footer.B - topPad);
      cursor += widths[index] + gap;
    }
  }

  void ReportFileStatus(const aeyla::product::ProjectFileStatus& status)
  {
    if(status.succeeded)
      return;

    std::string message = status.message;
    if(!status.diagnostics.empty())
      message += "\n\n" + status.diagnostics.front();
    GetUI()->ShowMessageBox(message.c_str(), "AEYLA project error", kMB_OK);
  }

  void ConfirmDiscardThen(std::function<void()> action)
  {
    if(!mPlug.ProjectDirty())
    {
      action();
      return;
    }

    GetUI()->ShowMessageBox(
        "The current AEYLA project has unsaved changes. Continue and discard them?",
        "Unsaved AEYLA project",
        kMB_YESNO,
        [action = std::move(action)](EMsgBoxResult result) {
          if(result == kYES)
            action();
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
          if(Empty(fileName))
            return;
          ReportFileStatus(mPlug.OpenProjectFromUI(DialogPath(fileName, path)));
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
          if(Empty(fileName))
            return;
          auto target = DialogPath(fileName, path);
          if(target.extension() != ".aeylashow")
            target += ".aeylashow";
          ReportFileStatus(mPlug.SaveProjectAsFromUI(target));
          SetDirty(false);
        });
  }

  AeylaVisualDmx& mPlug;
  std::array<IRECT, 5> mButtons{};
  WDL_String mDialogFileName;
  WDL_String mDialogPath;
};

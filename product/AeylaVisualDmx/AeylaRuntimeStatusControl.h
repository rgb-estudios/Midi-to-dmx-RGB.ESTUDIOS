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
    mPlug.RefreshVisualSpeedFromUI();
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

    g.FillRect(background, footer);
    g.DrawLine(line, footer.L, footer.T, footer.R, footer.T, nullptr, 1.0F);

    static constexpr const char* labels[] = {"NEW", "OPEN", "SAVE", "SAVE AS"};
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
               IRECT(footer.L + 264.0F, footer.T, footer.L + 550.0F, footer.B));

    char runtime[160];
    std::snprintf(runtime, sizeof(runtime),
                  "DMX %llu  ·  %d CH  ·  DROP %llu  ·  STATE %llu",
                  static_cast<unsigned long long>(mPlug.DmxGeneration()),
                  mPlug.DmxNonZeroChannels(),
                  static_cast<unsigned long long>(mPlug.DroppedMidiEvents()),
                  static_cast<unsigned long long>(mPlug.HostStateRestoreErrors()));
    g.DrawText(IText(9.0F, muted, "AeylaUI", EAlign::Center, EVAlign::Middle),
               runtime,
               IRECT(footer.L + 535.0F, footer.T,
                     footer.L + footer.W() * 0.73F, footer.B));

    char state[170];
    const char* project = mPlug.ProjectValid() ? "PROJECT VALID" : "PROJECT INVALID";
    const char* backend = mPlug.BackendReady() ? "BACKEND READY" : "BACKEND OFF";
    const char* output = mPlug.OutputArmed() ? "ARMED" : "DISARMED";
    const char* blackout = mPlug.EffectiveBlackout() ? "BLACKOUT" : "PREVIEW";
    std::snprintf(state, sizeof(state), "%s  ·  %s  ·  %s  ·  %s",
                  project, backend, output, blackout);

    IColor stateColor = mPlug.BackendReady() ? valid : warning;
    if(mPlug.OutputArmed() || !mPlug.ProjectValid() ||
       mPlug.HostStateRestoreErrors() > 0U)
      stateColor = danger;

    g.DrawText(IText(9.0F, stateColor, "AeylaUI", EAlign::Far, EVAlign::Middle),
               state,
               IRECT(footer.L + footer.W() * 0.70F, footer.T,
                     footer.R - 14.0F, footer.B));
  }

  bool IsHit(float x, float y) const override
  {
    // This control draws the runtime/file footer but its draw RECT spans the
    // complete editor. iPlug2 searches controls from front to back, so using
    // the default full-window hit test makes this overlay swallow every click
    // intended for the main editor and executor controls underneath it.
    // Restrict mouse ownership to the footer only; the rest must pass through.
    const IRECT footer(mRECT.L, mRECT.B - 42.0F, mRECT.R, mRECT.B);
    return footer.Contains(x, y);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    BuildButtons();

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
      PromptSaveAs();
  }

private:
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
    constexpr float widths[] = {52.0F, 56.0F, 54.0F, 68.0F};

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
  std::array<IRECT, 4> mButtons{};
  WDL_String mDialogFileName;
  WDL_String mDialogPath;
};

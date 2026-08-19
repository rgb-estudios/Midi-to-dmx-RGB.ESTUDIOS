#pragma once

#include "AeylaVisualDmx.h"

#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
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
    const IRECT footer = Footer();
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

    static constexpr const char* labels[] = {
        "NEW", "OPEN", "SAVE", "SAVE AS", "OUTPUT SETUP"};
    for(std::size_t index = 0; index < mButtons.size(); ++index)
    {
      g.FillRoundRect(raised, mButtons[index], 5.0F);
      g.DrawRoundRect(index == 4U ? warning : line,
                      mButtons[index], 5.0F, nullptr, 1.0F);
      g.DrawText(IText(9.0F, index == 4U ? warning : text,
                       "AeylaUI", EAlign::Center, EVAlign::Middle),
                 labels[index], mButtons[index]);
    }

    std::string projectLabel = mPlug.ProjectDirty() ? "UNSAVED  ·  " : "SAVED  ·  ";
    projectLabel += mPlug.ProjectName();
    if(!mPlug.CurrentProjectPath().empty())
      projectLabel += "  ·  " + mPlug.CurrentProjectPath().filename().string();
    g.DrawText(IText(8.5F, mPlug.ProjectDirty() ? warning : valid,
                     "AeylaUI", EAlign::Near, EVAlign::Middle),
               projectLabel.c_str(),
               IRECT(footer.L + 372.0F, footer.T,
                     footer.L + 650.0F, footer.B));

    const std::string backend = mPlug.OutputBackendStatus();
    g.DrawText(IText(8.5F, mPlug.BackendReady() ? valid : warning,
                     "AeylaUI", EAlign::Center, EVAlign::Middle),
               backend.c_str(),
               IRECT(footer.L + 650.0F, footer.T,
                     footer.L + 865.0F, footer.B));

    std::string state;
    if(mPlug.TakeOutputArmed())
      state = "TAKE ON AIR";
    else if(mPlug.TakeRecording())
      state = "CAPTURING AVOLITES";
    else if(mPlug.TakePlaying())
      state = "TAKE PLAY / PREVIEW";
    else
      state = "READY / DISARMED";

    if(mPlug.RenderingOffline())
      state = "OFFLINE RENDER · OUTPUT INHIBITED";
    else if(!mPlug.RuntimeHealthy())
      state = "RUNTIME FAULT";

    IColor stateColor = valid;
    if(mPlug.TakeOutputArmed()) stateColor = danger;
    else if(mPlug.TakeRecording()) stateColor = warning;
    else if(mPlug.RenderingOffline() || !mPlug.RuntimeHealthy()) stateColor = danger;

    g.DrawText(IText(8.5F, stateColor, "AeylaUI", EAlign::Far, EVAlign::Middle),
               state.c_str(),
               IRECT(footer.L + 865.0F, footer.T,
                     footer.R - 14.0F, footer.B));
  }

  bool IsHit(float x, float y) const override
  {
    return Footer().Contains(x, y);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)mod;
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
    if(Contains(mButtons[4], x, y))
      PromptOutputSetup();
  }

  void OnTextEntryCompletion(const char* str, int valIdx) override
  {
    if(valIdx != kOutputConfigTextEntry || str == nullptr)
      return;
    const auto result = mPlug.ConfigureArtNetFromUI(str);
    if(!result.succeeded)
    {
      GetUI()->ShowMessageBox(
          (result.message +
           "\n\nUse IPv4@universe (example: 2.0.0.50@0), or type OFF.")
              .c_str(),
          "AEYLA · ART-NET PREFLIGHT", kMB_OK);
    }
    else
    {
      // Re-apply explicit RX/TX NIC routing and restart the capture listener so
      // its Port-Address matches the newly configured output universe.
      (void)mPlug.RefreshNetworkInterfacesFromUI();
    }
    SetDirty(false);
  }

private:
  [[nodiscard]] IRECT Footer() const noexcept
  {
    return IRECT(mRECT.L, mRECT.B - 42.0F, mRECT.R, mRECT.B);
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

  static std::filesystem::path DialogPath(const WDL_String& fileName,
                                          const WDL_String& path)
  {
    const auto file = std::filesystem::u8path(fileName.Get());
    if(file.is_absolute()) return file;
    return std::filesystem::u8path(path.Get()) / file;
  }

  void BuildButtons()
  {
    const IRECT footer = Footer();
    constexpr float left = 12.0F;
    constexpr float topPad = 8.0F;
    constexpr float gap = 6.0F;
    constexpr float widths[] = {52.0F, 56.0F, 54.0F, 68.0F, 110.0F};
    float cursor = footer.L + left;
    for(std::size_t index = 0; index < mButtons.size(); ++index)
    {
      mButtons[index] = IRECT(cursor, footer.T + topPad,
                              cursor + widths[index], footer.B - topPad);
      cursor += widths[index] + gap;
    }
  }

  void PromptOutputSetup()
  {
    std::string current = mPlug.OutputBackendStatus();
    static constexpr std::string_view prefix = "ARTNET ";
    if(current.rfind(prefix, 0U) == 0U)
      current.erase(0U, prefix.size());
    else
      current = "2.0.0.50@0";

    GetUI()->CreateTextEntry(
        *this,
        IText(12.0F, IColor(255, 236, 238, 242), "AeylaUI",
              EAlign::Center, EVAlign::Middle),
        mButtons[4], current.c_str(), kOutputConfigTextEntry);
  }

  void ReportFileStatus(const aeyla::product::ProjectFileStatus& status)
  {
    if(status.succeeded) return;
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
        "Unsaved AEYLA project", kMB_YESNO,
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
  static constexpr int kOutputConfigTextEntry = 1001;
  std::array<IRECT, 5> mButtons{};
  WDL_String mDialogFileName;
  WDL_String mDialogPath;
};

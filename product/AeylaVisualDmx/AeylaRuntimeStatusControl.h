#pragma once

#include "AeylaVisualDmx.h"

#include <algorithm>
#include <array>
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

    // R09 verification beacon. A previous R08 binary still painted the old
    // "R07 PRETEST" subtitle in the main control; this overlay intentionally
    // covers that line so the operator can identify the loaded binary at a
    // glance without opening an installer or checking a filesystem timestamp.
    const IRECT verifyAura(mRECT.L + 10.0F, mRECT.T + 51.0F,
                           mRECT.L + 296.0F, mRECT.T + 82.0F);
    const IRECT verifyBadge(mRECT.L + 14.0F, mRECT.T + 55.0F,
                            mRECT.L + 292.0F, mRECT.T + 78.0F);
    const bool recording = mPlug.TakeRecording();
    const bool midiShowEnabled = mPlug.ShowMidiMapping().enabled;
    g.FillRoundRect(recording ? IColor(46, 231, 45, 55)
                              : IColor(38, 68, 214, 255),
                    verifyAura, 9.0F);
    g.FillRoundRect(recording ? IColor(255, 49, 14, 22)
                              : IColor(255, 8, 18, 24),
                    verifyBadge, 6.0F);
    g.DrawRoundRect(recording ? danger : verify,
                    verifyBadge, 6.0F, nullptr, 1.4F);
    const std::string verifyLabel = recording
        ? "R09 PRETEST  ·  N42 START / N43 STOP  ·  CAPTURANDO"
        : (midiShowEnabled
              ? "R09 PRETEST  ·  N42 START / N43 STOP  ·  LISTO"
              : "R09 PRETEST  ·  N42 START / N43 STOP  ·  MIDI SHOW OFF");
    g.DrawText(IText(10.5F, recording ? danger : verify,
                     "AeylaUI", EAlign::Center, EVAlign::Middle),
               verifyLabel.c_str(), verifyBadge.GetPadded(-5.0F));

    g.FillRect(background, footer);
    g.DrawLine(line, footer.L, footer.T, footer.R, footer.T, nullptr, 1.0F);

    // Thin illuminated authority rail: it stays intentionally visible even
    // when the footer text is being scanned quickly from FOH.
    IColor railColor = valid;
    if(mPlug.TakeOutputLive() || recording)
      railColor = danger;
    else if(mPlug.TakeOutputArmed())
      railColor = warning;
    else if(!mPlug.RuntimeHealthy() || mPlug.RenderingOffline())
      railColor = danger;
    else if(midiShowEnabled)
      railColor = verify;
    g.FillRect(railColor,
               IRECT(footer.L, footer.T, footer.R, footer.T + 3.0F));

    static constexpr const char* labels[] = {
        "NUEVO", "ABRIR", "GUARDAR", "GUARDAR COMO"};
    const bool projectChangeBlocked = recording;
    for(std::size_t index = 0; index < mButtons.size(); ++index)
    {
      const bool blocked = projectChangeBlocked && index < 2U;
      g.FillRoundRect(blocked ? IColor(255, 35, 31, 25) : raised,
                      mButtons[index], 5.0F);
      g.DrawRoundRect(blocked ? warning : line,
                      mButtons[index], 5.0F, nullptr, 1.0F);
      g.DrawText(IText(12.0F, blocked ? warning : text,
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

    // Output configuration has exactly one operator authority: the ART-NET
    // NETWORK panel in AeylaMainControl. The footer is status-only and can no
    // longer open the legacy IPv4@universe editor that previously overrode the
    // simplified TX route.
    const std::string backend = mPlug.OutputBackendStatus();
    g.DrawText(IText(12.0F, mPlug.BackendReady() ? valid : warning,
                     "AeylaUI", EAlign::Center, EVAlign::Middle),
               backend.c_str(), mBackendStatus);

    std::string state;
    if(mPlug.TakeOutputLive())
      state = "TOMA AL AIRE";
    else if(mPlug.TakeOutputArmed())
      state = "TOMA ARMADA · ESPERA REPRODUCIR";
    else if(recording)
      state = "CAPTURANDO AVOLITES";
    else if(mPlug.TakePlaying())
      state = "REPRODUCCIÓN / PREVIA";
    else
      state = "LISTO / DESARMADO";

    if(mPlug.RenderingOffline())
      state = "RENDERIZADO SIN CONEXIÓN · SALIDA INHIBIDA";
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
    else if(mPlug.RenderingOffline() || !mPlug.RuntimeHealthy()) {
      stateColor = danger;
      stateFill = IColor(255, 49, 14, 22);
    }
    else if(midiShowEnabled) {
      stateColor = verify;
      stateFill = IColor(255, 8, 18, 24);
    }

    // Backlit output capsule. The top line is physical authority, while the
    // lower line explicitly exposes the fixed N42/N43 capture trigger state.
    const IRECT outputAura(mOutputStatus.L - 4.0F, mOutputStatus.T + 4.0F,
                           mOutputStatus.R + 4.0F, mOutputStatus.B - 4.0F);
    g.FillRoundRect(recording || mPlug.TakeOutputLive()
                        ? IColor(42, 231, 45, 55)
                        : (mPlug.TakeOutputArmed()
                              ? IColor(34, 245, 154, 43)
                              : IColor(24, 68, 214, 255)),
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

    const std::string midiRecState = recording
        ? "N42 START · N43 STOP · CAPTURANDO"
        : (midiShowEnabled
              ? "N42 START · N43 STOP · LISTO"
              : "N42 START · N43 STOP · ACTIVA MIDI SHOW");
    g.DrawText(IText(9.5F,
                     recording ? danger :
                         (midiShowEnabled ? verify : muted),
                     "AeylaUI", EAlign::Far, EVAlign::Middle),
               midiRecState.c_str(), midiLine);
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
  }

private:
  [[nodiscard]] IRECT Footer() const noexcept
  {
    return IRECT(mRECT.L, mRECT.B - 50.0F, mRECT.R, mRECT.B);
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

  void BuildButtons()
  {
    const IRECT footer = Footer();
    constexpr float left = 12.0F;
    constexpr float topPad = 8.0F;
    constexpr float gap = 6.0F;
    constexpr float widths[] = {58.0F, 58.0F, 68.0F, 98.0F};
    float cursor = footer.L + left;
    for(std::size_t index = 0; index < mButtons.size(); ++index)
    {
      mButtons[index] = IRECT(cursor, footer.T + topPad,
                              cursor + widths[index], footer.B - topPad);
      cursor += widths[index] + gap;
    }

    const float statusLeft = mButtons.back().R + 14.0F;
    const float statusRight = footer.R - 14.0F;
    const float available = std::max(0.0F, statusRight - statusLeft);
    const float projectRight = statusLeft + available * 0.40F;
    const float backendRight = projectRight + available * 0.28F;
    mProjectStatus = IRECT(statusLeft, footer.T, projectRight, footer.B);
    mBackendStatus = IRECT(projectRight, footer.T, backendRight, footer.B);
    mOutputStatus = IRECT(backendRight, footer.T + 6.0F,
                          statusRight, footer.B - 6.0F);
  }

  void ReportFileStatus(const aeyla::product::ProjectFileStatus& status)
  {
    if(status.succeeded) return;
    std::string message = status.message;
    if(!status.diagnostics.empty())
      message += "\n\n" + status.diagnostics.front();
    GetUI()->ShowMessageBox(message.c_str(), "AEYLA · ERROR DE PROYECTO", kMB_OK);
  }

  void ConfirmDiscardThen(std::function<void()> action)
  {
    if(!mPlug.ProjectDirty())
    {
      action();
      return;
    }

    GetUI()->ShowMessageBox(
        "El proyecto AEYLA tiene cambios sin guardar. ¿Continuar y descartarlos?",
        "AEYLA · CAMBIOS SIN GUARDAR", kMB_YESNO,
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
  std::array<IRECT, 4> mButtons{};
  IRECT mProjectStatus{};
  IRECT mBackendStatus{};
  IRECT mOutputStatus{};
  WDL_String mDialogFileName;
  WDL_String mDialogPath;
};
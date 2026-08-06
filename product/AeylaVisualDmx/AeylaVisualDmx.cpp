#include "AeylaVisualDmx.h"
#include "AeylaMainControl.h"
#include "IPlug_include_in_plug_src.h"

#include <algorithm>

AeylaVisualDmx::AeylaVisualDmx(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamBlackout)->InitBool("Blackout", true);
  GetParam(kParamGrandMaster)->InitPercentage("Grand Master", 100.0);
  GetParam(kParamRigMode)->InitEnum("Rig Mode", 0, 2, "", IParam::kFlagsNone,
                                     "", "10 fixtures", "14 fixtures");
  GetParam(kParamSource)->InitEnum("Visual Source", 1, 5, "", IParam::kFlagsNone,
                                    "", "Solid", "Gradient", "Wave", "Noise", "Chase");
  GetParam(kParamSpeed)->InitPercentage("Animation Speed", 35.0);
  GetParam(kParamWhiteExtract)->InitPercentage("White Extraction", 20.0);
  GetParam(kParamAmberExtract)->InitPercentage("Amber Extraction", 15.0);
  GetParam(kParamUV)->InitPercentage("UV Manual", 0.0);

  // The graphical alpha intentionally uses a diagnostic/null backend. This
  // allows the arm interaction to be tested without transmitting DMX.
  mSafety.set_project_valid(true);
  mSafety.set_backend_ready(true);

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this,
                        PLUG_WIDTH,
                        PLUG_HEIGHT,
                        PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    const IRECT bounds = pGraphics->GetBounds();

    if(pGraphics->NControls())
    {
      if(auto* background = pGraphics->GetBackgroundControl())
        background->SetRECT(bounds);
      if(auto* main = pGraphics->GetControlWithTag(kCtrlTagMain))
        main->SetRECT(bounds);
      return;
    }

    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, true);
    pGraphics->AttachPanelBackground(IColor(255, 8, 9, 12));

    if(!pGraphics->LoadFont("AeylaUI", "Arial", ETextStyle::Normal))
      pGraphics->LoadFont("AeylaUI", "Times New Roman", ETextStyle::Normal);

    pGraphics->AttachControl(new AeylaMainControl(bounds, *this), kCtrlTagMain);
  };
#endif
}

#if IPLUG_DSP
void AeylaVisualDmx::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  (void) inputs;

  // This plugin is a silent MIDI-controlled lighting runtime. Always clear the
  // advertised stereo bus so it cannot inject sound into an Ableton track.
  for(int channel = 0; channel < 2; ++channel)
  {
    if(outputs[channel])
      std::fill(outputs[channel], outputs[channel] + nFrames, static_cast<sample>(0));
  }
}

void AeylaVisualDmx::ProcessMidiMsg(const IMidiMsg& msg)
{
  const auto status = msg.StatusMsg();
  const int note = msg.NoteNumber();
  const int velocity = msg.Velocity();

  if((status == IMidiMsg::kNoteOn || status == IMidiMsg::kNoteOff) && note >= 0)
  {
    mMidiEventCount.fetch_add(1, std::memory_order_relaxed);
    mLastMidiNote.store(note, std::memory_order_relaxed);

    const bool isNoteOn = status == IMidiMsg::kNoteOn && velocity > 0;
    if(note >= 36 && note <= 43)
    {
      if(isNoteOn)
        mActiveExecutor.store(note - 36, std::memory_order_relaxed);
      else if(mActiveExecutor.load(std::memory_order_relaxed) == note - 36)
        mActiveExecutor.store(-1, std::memory_order_relaxed);
    }
  }
}

void AeylaVisualDmx::OnReset()
{
  mLastMidiNote.store(-1, std::memory_order_relaxed);
  mActiveExecutor.store(-1, std::memory_order_relaxed);
}

void AeylaVisualDmx::OnActivate(bool active)
{
  if(!active)
  {
    mOutputArmed.store(false, std::memory_order_release);
    mActiveExecutor.store(-1, std::memory_order_relaxed);
  }
}
#endif

void AeylaVisualDmx::OnIdle()
{
#if IPLUG_EDITOR
  if(auto* ui = GetUI())
  {
    if(auto* main = ui->GetControlWithTag(kCtrlTagMain))
      main->SetDirty(false);
  }
#endif
}

void AeylaVisualDmx::ToggleOutputArmFromUI() noexcept
{
  SetOutputArmed(!OutputArmed());
}

void AeylaVisualDmx::ForceDisarmFromUI() noexcept
{
  SetOutputArmed(false);
}

void AeylaVisualDmx::SetOutputArmed(bool armed) noexcept
{
  if(armed)
  {
    if(mSafety.request_arm())
      mOutputArmed.store(true, std::memory_order_release);
  }
  else
  {
    mSafety.disarm();
    mOutputArmed.store(false, std::memory_order_release);
  }
}

#include "AeylaVisualDmx.h"
#include "AeylaExecutorRuntimeControl.h"
#include "AeylaMainControl.h"
#include "AeylaRuntimeStatusControl.h"
#include "IPlug_include_in_plug_src.h"

#include <algorithm>
#include <chrono>
#include <cmath>

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

  // The application model starts with a valid development document but a
  // disconnected diagnostic backend. Preview is available; real output cannot
  // be armed until a named backend is configured and healthy.
  SyncSnapshotToAtomics();

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
    const IRECT executorBounds = AeylaExecutorRuntimeControl::BoundsFor(bounds);

    if(pGraphics->NControls())
    {
      if(auto* background = pGraphics->GetBackgroundControl())
        background->SetRECT(bounds);
      if(auto* main = pGraphics->GetControlWithTag(kCtrlTagMain))
        main->SetRECT(bounds);
      if(auto* executors = pGraphics->GetControlWithTag(kCtrlTagExecutorRuntime))
        executors->SetRECT(executorBounds);
      if(auto* status = pGraphics->GetControlWithTag(kCtrlTagRuntimeStatus))
        status->SetRECT(bounds);
      return;
    }

    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, true);
    pGraphics->AttachPanelBackground(IColor(255, 8, 9, 12));

    if(!pGraphics->LoadFont("AeylaUI", "Arial", ETextStyle::Normal))
      pGraphics->LoadFont("AeylaUI", "Times New Roman", ETextStyle::Normal);

    pGraphics->AttachControl(new AeylaMainControl(bounds, *this), kCtrlTagMain);
    pGraphics->AttachControl(new AeylaExecutorRuntimeControl(executorBounds, *this),
                             kCtrlTagExecutorRuntime);
    pGraphics->AttachControl(new AeylaRuntimeStatusControl(bounds, *this),
                             kCtrlTagRuntimeStatus);
  };
#endif
}

#if IPLUG_DSP
void AeylaVisualDmx::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  (void) inputs;

  // Silent MIDI-controlled lighting runtime: the host callback only clears the
  // advertised bus. It performs no project, graphics, DMX, network or file work.
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

    aeyla::runtime::HostEvent event{};
    event.type = status == IMidiMsg::kNoteOn && velocity > 0
                     ? aeyla::runtime::HostEventType::note_on
                     : aeyla::runtime::HostEventType::note_off;
    event.note = static_cast<std::uint8_t>(std::clamp(note, 0, 127));
    event.value = static_cast<float>(std::clamp(velocity, 0, 127)) / 127.0F;
    (void) mHostIngress.try_submit(event);
  }
}

void AeylaVisualDmx::OnReset()
{
  mLastMidiNote.store(-1, std::memory_order_relaxed);

  aeyla::runtime::HostEvent event{};
  event.type = aeyla::runtime::HostEventType::all_notes_off;
  (void) mHostIngress.try_submit(event);
}

void AeylaVisualDmx::OnActivate(bool active)
{
  if(!active)
    mHostDeactivationPending.store(true, std::memory_order_release);
}

void AeylaVisualDmx::OnParamChange(int paramIdx)
{
  (void) paramIdx;
  // Parameter callbacks may originate from host processing. Only publish a
  // compact atomic flag here; the non-realtime idle path updates the model.
  mParameterUpdatePending.store(true, std::memory_order_release);
}
#endif

void AeylaVisualDmx::OnIdle()
{
  if(mHostDeactivationPending.exchange(false, std::memory_order_acq_rel))
  {
    mModel.disarm(aeyla::runtime::RuntimeSafetyReason::host_deactivation);
    mModel.set_blackout(true);
    GetParam(kParamBlackout)->Set(1.0);
    mParameterUpdatePending.store(true, std::memory_order_release);
  }

  ApplyPendingParameterState();
  DrainHostEvents();

  const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
  const double seconds = std::chrono::duration<double>(elapsed).count();
  const double speed = 0.20 + (GetParam(kParamSpeed)->Value() / 100.0) * 2.80;
  mModel.set_phase(static_cast<float>(std::fmod(seconds * speed, 1.0)));
  SyncSnapshotToAtomics();

#if IPLUG_EDITOR
  if(auto* ui = GetUI())
  {
    if(auto* main = ui->GetControlWithTag(kCtrlTagMain))
      main->SetDirty(false);
    if(auto* executors = ui->GetControlWithTag(kCtrlTagExecutorRuntime))
      executors->SetDirty(false);
    if(auto* status = ui->GetControlWithTag(kCtrlTagRuntimeStatus))
      status->SetDirty(false);
  }
#endif
}

void AeylaVisualDmx::ApplyPendingParameterState()
{
  if(!mParameterUpdatePending.exchange(false, std::memory_order_acq_rel))
    return;

  mModel.set_blackout(GetParam(kParamBlackout)->Bool());
  mModel.set_grand_master(static_cast<float>(GetParam(kParamGrandMaster)->Value() / 100.0));
  mModel.set_rig14(GetParam(kParamRigMode)->Int() == 1);

  const int source = std::clamp(GetParam(kParamSource)->Int(), 0, 4);
  mModel.set_visual_source(static_cast<aeyla::product::VisualSource>(source));
  mModel.set_white_extraction(
      static_cast<float>(GetParam(kParamWhiteExtract)->Value() / 100.0));
  mModel.set_amber_extraction(
      static_cast<float>(GetParam(kParamAmberExtract)->Value() / 100.0));
  mModel.set_uv_manual(static_cast<float>(GetParam(kParamUV)->Value() / 100.0));
}

void AeylaVisualDmx::DrainHostEvents()
{
  if(mHostIngress.consume_transient_release_request())
    mModel.release_transients();

  aeyla::runtime::HostEvent event{};
  while(mHostIngress.try_consume(event))
    mModel.handle_host_event(event);
}

void AeylaVisualDmx::ToggleOutputArmFromUI()
{
  SetOutputArmed(!OutputArmed());
}

void AeylaVisualDmx::ForceDisarmFromUI()
{
  SetOutputArmed(false);
}

void AeylaVisualDmx::TriggerExecutorFromUI(int executorIndex, float velocity)
{
  if(executorIndex < 0 || executorIndex > 7)
    return;

  aeyla::runtime::HostEvent event{};
  event.type = aeyla::runtime::HostEventType::note_on;
  event.note = static_cast<std::uint8_t>(36 + executorIndex);
  event.value = std::clamp(velocity, 0.0F, 1.0F);
  mModel.handle_host_event(event);
  SyncSnapshotToAtomics();
}

void AeylaVisualDmx::ReleaseExecutorFromUI(int executorIndex)
{
  if(executorIndex < 0 || executorIndex > 7)
    return;

  aeyla::runtime::HostEvent event{};
  event.type = aeyla::runtime::HostEventType::note_off;
  event.note = static_cast<std::uint8_t>(36 + executorIndex);
  event.value = 0.0F;
  mModel.handle_host_event(event);
  SyncSnapshotToAtomics();
}

void AeylaVisualDmx::SetOutputArmed(bool armed)
{
  if(armed)
    (void) mModel.request_arm();
  else
    mModel.disarm();

  SyncSnapshotToAtomics();
}

void AeylaVisualDmx::SyncSnapshotToAtomics() noexcept
{
  const auto& snapshot = mModel.snapshot();
  mOutputArmed.store(snapshot.output_armed, std::memory_order_release);
  mEffectiveBlackout.store(snapshot.blackout, std::memory_order_release);
  mBackendReady.store(snapshot.backend_ready, std::memory_order_release);
  mProjectValid.store(snapshot.project_valid, std::memory_order_release);
  mActiveExecutor.store(snapshot.active_executor, std::memory_order_relaxed);
  mDmxGeneration.store(snapshot.generation, std::memory_order_relaxed);

  const int nonZero = static_cast<int>(std::count_if(
      snapshot.dmx.begin(), snapshot.dmx.end(),
      [](std::uint8_t value) { return value != 0; }));
  mDmxNonZeroChannels.store(nonZero, std::memory_order_relaxed);
}

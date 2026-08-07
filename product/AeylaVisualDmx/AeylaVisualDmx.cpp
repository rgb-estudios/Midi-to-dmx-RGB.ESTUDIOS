#include "AeylaVisualDmx.h"
#include "AeylaExecutorRuntimeControl.h"
#include "AeylaMainControl.h"
#include "AeylaRuntimeStatusControl.h"
#include "IPlug_include_in_plug_src.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {

int HexValue(char value)
{
  if(value >= '0' && value <= '9') return value - '0';
  if(value >= 'a' && value <= 'f') return 10 + value - 'a';
  if(value >= 'A' && value <= 'F') return 10 + value - 'A';
  return -1;
}

bool DecodeCanonicalUuid(std::string_view text,
                         std::array<std::uint8_t, 16>& bytes) noexcept
{
  if(text.size() != 36U)
    return false;

  std::size_t source = 0U;
  for(std::size_t destination = 0U; destination < bytes.size(); ++destination)
  {
    if(source == 8U || source == 13U || source == 18U || source == 23U)
    {
      if(text[source] != '-')
        return false;
      ++source;
    }

    if(source + 1U >= text.size())
      return false;
    const int high = HexValue(text[source]);
    const int low = HexValue(text[source + 1U]);
    if(high < 0 || low < 0)
      return false;
    bytes[destination] = static_cast<std::uint8_t>((high << 4) | low);
    source += 2U;
  }
  return source == text.size();
}

bool IsZero(const std::array<std::uint8_t, 16>& bytes) noexcept
{
  return std::all_of(bytes.begin(), bytes.end(),
                     [](std::uint8_t value) { return value == 0U; });
}

bool IsZero(const std::array<std::uint8_t, 32>& bytes) noexcept
{
  return std::all_of(bytes.begin(), bytes.end(),
                     [](std::uint8_t value) { return value == 0U; });
}

std::array<std::uint8_t, 4> EncodeLittleEndian32(std::uint32_t value) noexcept
{
  return {
      static_cast<std::uint8_t>(value & 0xFFU),
      static_cast<std::uint8_t>((value >> 8U) & 0xFFU),
      static_cast<std::uint8_t>((value >> 16U) & 0xFFU),
      static_cast<std::uint8_t>((value >> 24U) & 0xFFU),
  };
}

std::uint32_t DecodeLittleEndian32(
    const std::array<std::uint8_t, 4>& bytes) noexcept
{
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

}  // namespace

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
  RefreshHostStateCache();

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

bool AeylaVisualDmx::SerializeState(IByteChunk& chunk) const
{
  try
  {
    aeyla::runtime::PluginComponentState state;
    {
      const std::scoped_lock lock(mHostStateMutex);
      state = mHostStateCache;
    }

    const auto encoded = aeyla::runtime::encode_plugin_component_state(state);
    if(!encoded.ok() || encoded.bytes.size() >
                            static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
      return false;

    const auto length = EncodeLittleEndian32(
        static_cast<std::uint32_t>(encoded.bytes.size()));
    chunk.PutBytes(length.data(), static_cast<int>(length.size()));
    if(!encoded.bytes.empty())
      chunk.PutBytes(encoded.bytes.data(), static_cast<int>(encoded.bytes.size()));

    // iPlug parameters follow the AEYLA component block. Output Arm is not a
    // parameter and is deliberately absent from both state sections.
    return SerializeParams(chunk);
  }
  catch(...)
  {
    return false;
  }
}

int AeylaVisualDmx::UnserializeState(const IByteChunk& chunk, int startPos)
{
  const auto reject = [&]() {
    mHostStateRestoreErrors.fetch_add(1U, std::memory_order_relaxed);
    mHostStateRestoreRejected.store(true, std::memory_order_release);
    return -1;
  };

  try
  {
    std::array<std::uint8_t, 4> lengthBytes{};
    int position = chunk.GetBytes(lengthBytes.data(),
                                  static_cast<int>(lengthBytes.size()), startPos);
    if(position < 0)
      return reject();

    const std::uint32_t encodedLength = DecodeLittleEndian32(lengthBytes);
    if(encodedLength == 0U ||
       encodedLength > aeyla::runtime::kMaxPluginStateBytes ||
       encodedLength > static_cast<std::uint32_t>(chunk.Size() - position))
      return reject();

    std::vector<std::uint8_t> encoded(encodedLength);
    position = chunk.GetBytes(encoded.data(), static_cast<int>(encoded.size()), position);
    if(position < 0)
      return reject();

    const auto decoded = aeyla::runtime::decode_plugin_component_state(
        std::span<const std::uint8_t>(encoded.data(), encoded.size()));
    if(!decoded.ok())
      return reject();

    const int parameterEnd = UnserializeParams(chunk, position);
    if(parameterEnd < 0)
      return reject();

    {
      const std::scoped_lock lock(mHostStateMutex);
      mPendingHostState = decoded.state;
    }
    return parameterEnd;
  }
  catch(...)
  {
    return reject();
  }
}

void AeylaVisualDmx::OnIdle()
{
  ApplyPendingHostState();

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
  RefreshHostStateCache();

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

void AeylaVisualDmx::ApplyPendingHostState()
{
  if(mHostStateRestoreRejected.exchange(false, std::memory_order_acq_rel))
  {
    mModel.release_transients();
    mModel.disarm(aeyla::runtime::RuntimeSafetyReason::project_reload);
    mModel.set_blackout(true);
    GetParam(kParamBlackout)->Set(1.0);
    mParameterUpdatePending.store(true, std::memory_order_release);
  }

  std::optional<aeyla::runtime::PluginComponentState> pending;
  {
    const std::scoped_lock lock(mHostStateMutex);
    pending = std::move(mPendingHostState);
    mPendingHostState.reset();
  }
  if(!pending.has_value())
    return;

  mModel.release_transients();
  mModel.disarm(aeyla::runtime::RuntimeSafetyReason::project_reload);

  std::array<std::uint8_t, 16> currentUuid{};
  const bool currentUuidValid = DecodeCanonicalUuid(
      mModel.project_document().project_id, currentUuid);
  const bool savedHasIdentity = !IsZero(pending->project_uuid);
  const bool uuidMismatch = savedHasIdentity &&
                            (!currentUuidValid || pending->project_uuid != currentUuid);
  const bool schemaMismatch = pending->project_schema_major !=
                              mModel.project_document().schema_version.major;

  aeyla::runtime::PluginComponentState previousCache;
  {
    const std::scoped_lock lock(mHostStateMutex);
    previousCache = mHostStateCache;
    mHostStateCache = *pending;
  }
  const bool checksumMismatch = !IsZero(pending->project_checksum) &&
                                pending->project_checksum != previousCache.project_checksum;

  if(uuidMismatch || schemaMismatch || checksumMismatch)
  {
    // The Set identifies a project that is not the currently loaded package.
    // Until locator-based asynchronous loading is implemented, expose an
    // invalid project and publish only the safe frame.
    mModel.set_project_valid(false);
    mModel.set_blackout(true);
    GetParam(kParamBlackout)->Set(1.0);
    mHostStateRestoreErrors.fetch_add(1U, std::memory_order_relaxed);
  }

  // UnserializeParams already restored host-visible preferences. Applying them
  // is deferred here, outside the host processing callback. Output remains
  // disarmed regardless of the saved Set.
  mParameterUpdatePending.store(true, std::memory_order_release);
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

void AeylaVisualDmx::RefreshHostStateCache()
{
  const auto& snapshot = mModel.snapshot();
  if(!snapshot.project_valid)
    return;

  std::array<std::uint8_t, 16> uuid{};
  if(!DecodeCanonicalUuid(snapshot.project_id, uuid))
    return;

  const std::scoped_lock lock(mHostStateMutex);
  if(mHostStateCache.project_uuid != uuid)
  {
    mHostStateCache.project_checksum.fill(0U);
    mHostStateCache.locator_mode = aeyla::runtime::ProjectLocatorMode::none;
    mHostStateCache.project_locator.clear();
  }
  mHostStateCache.project_uuid = uuid;
  mHostStateCache.project_schema_major =
      mModel.project_document().schema_version.major;
  mHostStateCache.project_schema_minor =
      mModel.project_document().schema_version.minor;
  mHostStateCache.grand_master = snapshot.grand_master;
  mHostStateCache.blackout = snapshot.blackout;
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

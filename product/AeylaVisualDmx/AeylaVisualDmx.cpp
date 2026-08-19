#include "AeylaVisualDmx.h"
#include "AeylaExecutorRuntimeControl.h"
#include "AeylaMainControl.h"
#include "AeylaRuntimeStatusControl.h"
#include "IPlug_include_in_plug_src.h"
#include "runtime/host_song_binding.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iterator>
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

std::string TrimAscii(std::string_view value)
{
  while(!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
    value.remove_prefix(1U);
  while(!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
    value.remove_suffix(1U);
  return std::string(value);
}

bool IsOffSpecification(std::string_view value) noexcept
{
  if(value.size() != 3U)
    return false;
  return std::tolower(static_cast<unsigned char>(value[0])) == 'o' &&
         std::tolower(static_cast<unsigned char>(value[1])) == 'f' &&
         std::tolower(static_cast<unsigned char>(value[2])) == 'f';
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
  CaptureAllParameterValuesFromHost();
  SyncSnapshotToAtomicsLocked();
  RefreshHostStateCacheLocked();

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

  StartRuntimeWorker();
}

AeylaVisualDmx::~AeylaVisualDmx()
{
  mArtNetOutput.set_enabled(false);
  StopRuntimeWorker();
  mArtNetOutput.stop();
}

#if IPLUG_DSP
void AeylaVisualDmx::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  (void) inputs;

  // Publish only the newest absolute host transport snapshot. This is bounded,
  // lock-free and contains no file/network/UI work. Lighting runtime can later
  // reconstruct Stop/Seek/Loop from host truth without replaying audio blocks.
  mHostTransport.publish(GetTransportIsRunning(),
                         GetRenderingOffline(),
                         GetSamplePos(),
                         GetPPQPos(),
                         GetTempo());

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
    // iPlug reports channels as 0..15. AEYLA's authored show contract uses
    // conventional MIDI channels 1..16, so normalize at the wrapper boundary.
    event.channel = static_cast<std::uint8_t>(
        std::clamp(msg.Channel() + 1, 1, 16));
    event.note = static_cast<std::uint8_t>(std::clamp(note, 0, 127));
    event.value = static_cast<float>(std::clamp(velocity, 0, 127)) / 127.0F;
    event.sample_offset = msg.mOffset;

    const double blockSample = GetSamplePos();
    const int nonNegativeOffset = std::max(msg.mOffset, 0);
    const double maximumBase =
        static_cast<double>(std::numeric_limits<std::int64_t>::max() -
                            static_cast<std::int64_t>(nonNegativeOffset));
    if(std::isfinite(blockSample) && blockSample >= 0.0 &&
       blockSample <= maximumBase)
    {
      event.project_sample = static_cast<std::int64_t>(blockSample) +
                             static_cast<std::int64_t>(nonNegativeOffset);
    }

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
  // Parameter callbacks may originate from host processing. Only mirror the
  // changed scalar into lock-free atomics; the independent runtime worker
  // applies it to the model outside the audio callback and outside UI OnIdle.
  CaptureParameterValueFromHost(paramIdx);
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
  // Functional runtime work intentionally does not live here. Hosts are free
  // to suspend UI idle callbacks when the editor is closed; OnIdle only asks
  // visible controls to redraw snapshots already published by RuntimeLoop().
  const bool blackout = mParamBlackout.load(std::memory_order_acquire);
  if(GetParam(kParamBlackout)->Bool() != blackout)
    GetParam(kParamBlackout)->Set(blackout ? 1.0 : 0.0);

  if(mLookParameterUiSyncPending.exchange(false, std::memory_order_acq_rel))
  {
    GetParam(kParamSource)->Set(
        static_cast<double>(mParamSource.load(std::memory_order_acquire)));
    GetParam(kParamSpeed)->Set(
        static_cast<double>(mParamSpeed.load(std::memory_order_acquire)) * 100.0);
    GetParam(kParamWhiteExtract)->Set(static_cast<double>(
        mParamWhiteExtract.load(std::memory_order_acquire)) * 100.0);
    GetParam(kParamAmberExtract)->Set(static_cast<double>(
        mParamAmberExtract.load(std::memory_order_acquire)) * 100.0);
    GetParam(kParamUV)->Set(
        static_cast<double>(mParamUv.load(std::memory_order_acquire)) * 100.0);
  }
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

void AeylaVisualDmx::StartRuntimeWorker()
{
  mRuntimeStopRequested.store(false, std::memory_order_release);
  mRuntimeExited.store(false, std::memory_order_release);
  mRuntimeFaulted.store(false, std::memory_order_release);
  try
  {
    mRuntimeThread = std::thread([this]() { RuntimeLoop(); });
  }
  catch(...)
  {
    mRuntimeExited.store(true, std::memory_order_release);
    mRuntimeFaulted.store(true, std::memory_order_release);
    mParamBlackout.store(true, std::memory_order_release);
    mOutputArmed.store(false, std::memory_order_release);
    mEffectiveBlackout.store(true, std::memory_order_release);
    mDmxNonZeroChannels.store(0, std::memory_order_relaxed);
    try
    {
      mModel.release_transients();
      mModel.disarm(aeyla::runtime::RuntimeSafetyReason::runtime_fault);
      mModel.set_blackout(true);
      SyncSnapshotToAtomicsLocked();
    }
    catch(...)
    {
    }
  }
}

void AeylaVisualDmx::StopRuntimeWorker() noexcept
{
  mRuntimeStopRequested.store(true, std::memory_order_release);
  if(mRuntimeThread.joinable())
    mRuntimeThread.join();

  mParamBlackout.store(true, std::memory_order_release);
  mOutputArmed.store(false, std::memory_order_release);
  mEffectiveBlackout.store(true, std::memory_order_release);
  mDmxNonZeroChannels.store(0, std::memory_order_relaxed);
  try
  {
    const std::scoped_lock lock(mModelMutex);
    mModel.release_transients();
    mModel.disarm(aeyla::runtime::RuntimeSafetyReason::shutdown);
    mModel.set_blackout(true);
    SyncSnapshotToAtomicsLocked();
  }
  catch(...)
  {
  }
}

void AeylaVisualDmx::RuntimeLoop() noexcept
{
  using Clock = std::chrono::steady_clock;
  constexpr auto kRuntimePeriod = std::chrono::milliseconds(4);
  auto next = Clock::now();

  while(!mRuntimeStopRequested.load(std::memory_order_acquire))
  {
    RuntimeTick();
    next += kRuntimePeriod;
    const auto now = Clock::now();
    if(next <= now)
      next = now + kRuntimePeriod;
    std::this_thread::sleep_until(next);
  }
  mRuntimeExited.store(true, std::memory_order_release);
}

void AeylaVisualDmx::RuntimeTick() noexcept
{
  try
  {
    const std::scoped_lock lock(mModelMutex);
    if(mRuntimeFaulted.load(std::memory_order_acquire))
    {
      mModel.release_transients();
      mModel.disarm(aeyla::runtime::RuntimeSafetyReason::runtime_fault);
      mModel.set_blackout(true);
      mParamBlackout.store(true, std::memory_order_release);
      SyncSnapshotToAtomicsLocked();
      return;
    }
    ApplyPendingHostStateLocked();

    if(mHostDeactivationPending.exchange(false, std::memory_order_acq_rel))
    {
      mModel.release_transients();
      mModel.disarm(aeyla::runtime::RuntimeSafetyReason::host_deactivation);
      mModel.set_blackout(true);
      mParamBlackout.store(true, std::memory_order_release);
    }

    ApplyPendingParameterStateLocked();

    const auto host = mHostTransport.latest();
    const bool wasOffline = mRenderingOffline.exchange(
        host.rendering_offline, std::memory_order_acq_rel);
    if(host.rendering_offline)
    {
      if(!wasOffline)
        mModel.release_transients();
      mModel.disarm(aeyla::runtime::RuntimeSafetyReason::offline_render);
      mModel.set_blackout(true);
      mParamBlackout.store(true, std::memory_order_release);
    }

    const auto& snapshotBeforeTransport = mModel.snapshot();
    const auto& show = mModel.show_program();
    const bool hasActiveSong = snapshotBeforeTransport.active_song_index <
                               show.songs.size();
    const aeyla::show::SongProgram* activeSong = hasActiveSong
        ? &show.songs[snapshotBeforeTransport.active_song_index]
        : nullptr;

    std::optional<double> hostStartPpq;
    if(activeSong != nullptr)
    {
      const std::scoped_lock stateLock(mHostStateMutex);
      const auto binding = std::find_if(
          mHostStateCache.song_bindings.begin(),
          mHostStateCache.song_bindings.end(),
          [&](const aeyla::runtime::SessionSongBinding& candidate) {
            return candidate.song_id == activeSong->song_id;
          });
      if(binding != mHostStateCache.song_bindings.end())
        hostStartPpq = binding->host_start_ppq;
    }
    mActiveSongBound.store(hostStartPpq.has_value(), std::memory_order_release);

    bool hostSongPositionSafe = false;
    if(host.running && activeSong != nullptr)
    {
      aeyla::runtime::HostEvent transportEvent{};
      if(!mLastHostRunning)
      {
        transportEvent.type = aeyla::runtime::HostEventType::transport_started;
        mModel.handle_host_event(transportEvent);
      }

      if(hostStartPpq.has_value())
      {
        const aeyla::runtime::HostSongBinding binding{
            activeSong->song_id, *hostStartPpq};
        const auto projection = aeyla::runtime::project_host_transport_to_song(
            host, binding, *activeSong);
        if(projection.state == aeyla::runtime::SongProjectionState::in_song)
        {
          hostSongPositionSafe = true;
          const bool discontinuity = !mLastHostRunning ||
              mLastProjectedSongId != activeSong->song_id ||
              projection.tick < mLastProjectedTick;
          if(discontinuity)
            mModel.seek_active_song_tick(projection.tick);
          else if(projection.tick != mLastProjectedTick)
            mModel.advance_active_song_tick(projection.tick);
          mLastProjectedSongId = activeSong->song_id;
          mLastProjectedTick = projection.tick;
        }
        else
        {
          // Missing/out-of-range timing cannot inherit an old cue. Seeking to
          // the explicit song end resolves to no scene and therefore black.
          mModel.seek_active_song_tick(activeSong->length_ticks);
          mLastProjectedSongId = activeSong->song_id;
          mLastProjectedTick = activeSong->length_ticks;
        }
      }
      else if(!mLastHostRunning || mLastProjectedSongId != activeSong->song_id)
      {
        // ADR 0002 forbids silently assuming host PPQ zero. A running unbound
        // Song is a deterministic safe state until SET SONG START is used.
        mModel.seek_active_song_tick(activeSong->length_ticks);
        mLastProjectedSongId = activeSong->song_id;
        mLastProjectedTick = activeSong->length_ticks;
      }
    }
    else if(mLastHostRunning)
    {
      aeyla::runtime::HostEvent transportEvent{};
      transportEvent.type = aeyla::runtime::HostEventType::transport_stopped;
      mModel.handle_host_event(transportEvent);
      mLastProjectedSongId.clear();
      mLastProjectedTick = 0U;
    }
    mLastHostRunning = host.running;

    // Discrete live MIDI is applied after absolute Play/Seek/Loop
    // reconstruction so a note arriving in the first running block is not
    // erased by the same tick's authoritative transport transition.
    DrainHostEventsLocked();
    if(host.running && activeSong != nullptr && !hostSongPositionSafe)
    {
      // A running but unbound/before/after Song is a hard non-output state.
      // Discard any live latch that arrived in this tick so missing placement
      // data can never produce a short artistic flash between worker frames.
      mModel.seek_active_song_tick(activeSong->length_ticks);
    }

    const double normalizedSpeed = std::clamp(
        static_cast<double>(mParamSpeed.load(std::memory_order_acquire)),
        0.0, 1.0);
    const double cyclesPerQuarter = 0.125 + normalizedSpeed * 1.875;
    const auto phase = aeyla::runtime::phase_from_host_ppq(
        host, cyclesPerQuarter);
    mModel.set_phase(phase.value_or(0.0F));
    SyncSnapshotToAtomicsLocked();
    PublishOutputFrameLocked(host.rendering_offline);
    RefreshHostStateCacheLocked();
  }
  catch(...)
  {
    // Runtime exceptions latch safe. The worker stays alive only to keep
    // publishing blackout; plugin reload is required before ARM can recover.
    mRuntimeFaulted.store(true, std::memory_order_release);
    mParamBlackout.store(true, std::memory_order_release);
    mOutputArmed.store(false, std::memory_order_release);
    mEffectiveBlackout.store(true, std::memory_order_release);
    mDmxNonZeroChannels.store(0, std::memory_order_relaxed);
    try
    {
      const std::scoped_lock lock(mModelMutex);
      mModel.release_transients();
      mModel.disarm(aeyla::runtime::RuntimeSafetyReason::runtime_fault);
      mModel.set_blackout(true);
      SyncSnapshotToAtomicsLocked();
    }
    catch(...)
    {
    }
  }
}

void AeylaVisualDmx::ApplyPendingHostStateLocked()
{
  if(mHostStateRestoreRejected.exchange(false, std::memory_order_acq_rel))
  {
    mModel.release_transients();
    mModel.disarm(aeyla::runtime::RuntimeSafetyReason::project_reload);
    mModel.set_blackout(true);
    mParamBlackout.store(true, std::memory_order_release);
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
  const auto currentSchemaMajor =
      mModel.project_document().schema_version.major;
  const bool supportedLegacyMigration =
      pending->project_schema_major == 1U && currentSchemaMajor == 2U;
  const bool schemaMismatch = pending->project_schema_major !=
                                  currentSchemaMajor &&
                              !supportedLegacyMigration;

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
    mParamBlackout.store(true, std::memory_order_release);
    mHostStateRestoreErrors.fetch_add(1U, std::memory_order_relaxed);
    const std::scoped_lock stateLock(mHostStateMutex);
    mHostStateCache.song_bindings.clear();
  }

  // UnserializeParams already restored host-visible preferences. Applying them
  // is deferred here, outside the host processing callback. Output remains
  // disarmed regardless of the saved Set.
  mParameterUpdatePending.store(true, std::memory_order_release);
}

void AeylaVisualDmx::ApplyPendingParameterStateLocked()
{
  if(!mParameterUpdatePending.exchange(false, std::memory_order_acq_rel))
    return;

  const auto visualBefore = mModel.project_document().visual;
  const bool rig14Before = mModel.snapshot().rig14;
  mModel.set_blackout(mParamBlackout.load(std::memory_order_acquire));
  mModel.set_grand_master(mParamGrandMaster.load(std::memory_order_acquire));
  mModel.set_rig14(mParamRig14.load(std::memory_order_acquire));

  const int source = std::clamp(mParamSource.load(std::memory_order_acquire), 0, 4);
  const bool sourceChanged = source != mLastAppliedSource;
  mModel.set_visual_source(static_cast<aeyla::product::VisualSource>(source));
  mLastAppliedSource = source;
  if(sourceChanged)
  {
    const auto& visual = mModel.project_document().visual;
    mParamSpeed.store(visual.speed, std::memory_order_release);
    mParamWhiteExtract.store(visual.white_extraction, std::memory_order_release);
    mParamAmberExtract.store(visual.amber_extraction, std::memory_order_release);
    mParamUv.store(visual.uv_manual, std::memory_order_release);
    mLookParameterUiSyncPending.store(true, std::memory_order_release);
  }
  else
  {
    mModel.set_visual_speed(mParamSpeed.load(std::memory_order_acquire));
    mModel.set_white_extraction(mParamWhiteExtract.load(std::memory_order_acquire));
    mModel.set_amber_extraction(mParamAmberExtract.load(std::memory_order_acquire));
    mModel.set_uv_manual(mParamUv.load(std::memory_order_acquire));
  }
  if(visualBefore != mModel.project_document().visual ||
     rig14Before != mModel.snapshot().rig14)
    mParamBlackout.store(true, std::memory_order_release);
}

void AeylaVisualDmx::DrainHostEventsLocked()
{
  if(mHostIngress.consume_transient_release_request())
    mModel.release_transients();

  aeyla::runtime::HostEvent event{};
  while(mHostIngress.try_consume(event))
    mModel.handle_host_event(event);
}

void AeylaVisualDmx::RefreshHostStateCacheLocked()
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
    mHostStateCache.song_bindings.clear();
  }
  mHostStateCache.project_uuid = uuid;
  mHostStateCache.project_schema_major =
      mModel.project_document().schema_version.major;
  mHostStateCache.project_schema_minor =
      mModel.project_document().schema_version.minor;
  mHostStateCache.grand_master = snapshot.grand_master;
  mHostStateCache.blackout = snapshot.blackout;
}

void AeylaVisualDmx::CaptureParameterValueFromHost(int paramIdx) noexcept
{
  switch(paramIdx)
  {
    case kParamBlackout:
      mParamBlackout.store(GetParam(kParamBlackout)->Bool(),
                           std::memory_order_release);
      break;
    case kParamGrandMaster:
      mParamGrandMaster.store(static_cast<float>(
          GetParam(kParamGrandMaster)->Value() / 100.0),
          std::memory_order_release);
      break;
    case kParamRigMode:
      mParamRig14.store(GetParam(kParamRigMode)->Int() == 1,
                        std::memory_order_release);
      break;
    case kParamSource:
      mParamSource.store(std::clamp(GetParam(kParamSource)->Int(), 0, 4),
                         std::memory_order_release);
      break;
    case kParamSpeed:
      mParamSpeed.store(static_cast<float>(GetParam(kParamSpeed)->Value() / 100.0),
                        std::memory_order_release);
      break;
    case kParamWhiteExtract:
      mParamWhiteExtract.store(static_cast<float>(
          GetParam(kParamWhiteExtract)->Value() / 100.0),
          std::memory_order_release);
      break;
    case kParamAmberExtract:
      mParamAmberExtract.store(static_cast<float>(
          GetParam(kParamAmberExtract)->Value() / 100.0),
          std::memory_order_release);
      break;
    case kParamUV:
      mParamUv.store(static_cast<float>(GetParam(kParamUV)->Value() / 100.0),
                     std::memory_order_release);
      break;
    default:
      break;
  }
}

void AeylaVisualDmx::CaptureAllParameterValuesFromHost() noexcept
{
  for(int param = 0; param < kNumParams; ++param)
    CaptureParameterValueFromHost(param);
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
  event.channel = 1U;
  event.note = static_cast<std::uint8_t>(36 + executorIndex);
  event.value = std::clamp(velocity, 0.0F, 1.0F);
  const std::scoped_lock lock(mModelMutex);
  mModel.handle_host_event(event);
  SyncSnapshotToAtomicsLocked();
}

void AeylaVisualDmx::ReleaseExecutorFromUI(int executorIndex)
{
  if(executorIndex < 0 || executorIndex > 7)
    return;

  aeyla::runtime::HostEvent event{};
  event.type = aeyla::runtime::HostEventType::note_off;
  event.channel = 1U;
  event.note = static_cast<std::uint8_t>(36 + executorIndex);
  event.value = 0.0F;
  const std::scoped_lock lock(mModelMutex);
  mModel.handle_host_event(event);
  SyncSnapshotToAtomicsLocked();
}

bool AeylaVisualDmx::SetActiveSongStartFromPlayheadFromUI()
{
  const auto host = mHostTransport.latest();
  if(host.revision == 0U || !host.ppq_position_valid ||
     !std::isfinite(host.ppq_position))
    return false;

  const std::scoped_lock lock(mModelMutex);
  const auto& snapshot = mModel.snapshot();
  if(snapshot.active_song_id.empty())
    return false;

  {
    const std::scoped_lock stateLock(mHostStateMutex);
    auto binding = std::find_if(
        mHostStateCache.song_bindings.begin(),
        mHostStateCache.song_bindings.end(),
        [&](const aeyla::runtime::SessionSongBinding& candidate) {
          return candidate.song_id == snapshot.active_song_id;
        });
    if(binding == mHostStateCache.song_bindings.end())
    {
      if(mHostStateCache.song_bindings.size() >=
         aeyla::runtime::kMaxSessionSongBindings)
        return false;
      mHostStateCache.song_bindings.push_back(
          {snapshot.active_song_id, host.ppq_position});
    }
    else
    {
      binding->host_start_ppq = host.ppq_position;
    }
  }

  // A placement change changes which cue owns the current DAW position. Force
  // a safe boundary and require explicit operator recovery.
  mModel.release_transients();
  mModel.disarm(aeyla::runtime::RuntimeSafetyReason::project_reload);
  mModel.set_blackout(true);
  mParamBlackout.store(true, std::memory_order_release);
  mActiveSongBound.store(true, std::memory_order_release);
  mLastProjectedSongId.clear();
  mLastProjectedTick = 0U;
  SyncSnapshotToAtomicsLocked();
  return true;
}

bool AeylaVisualDmx::SelectAdjacentSongFromUI(int direction)
{
  if(direction == 0)
    return false;
  const std::scoped_lock lock(mModelMutex);
  const auto& snapshot = mModel.snapshot();
  if(snapshot.song_count == 0U)
    return false;

  const std::size_t current = snapshot.active_song_index;
  std::size_t target = current;
  if(direction < 0 && current > 0U)
    target = current - 1U;
  else if(direction > 0 && current + 1U < snapshot.song_count)
    target = current + 1U;
  if(target == current || !mModel.select_song(target))
    return false;

  bool bound = false;
  {
    const std::scoped_lock stateLock(mHostStateMutex);
    bound = std::any_of(
        mHostStateCache.song_bindings.begin(),
        mHostStateCache.song_bindings.end(),
        [&](const aeyla::runtime::SessionSongBinding& candidate) {
          return candidate.song_id == mModel.snapshot().active_song_id;
        });
  }
  mActiveSongBound.store(bound, std::memory_order_release);
  mParamBlackout.store(true, std::memory_order_release);
  mLastProjectedSongId.clear();
  mLastProjectedTick = 0U;
  SyncSnapshotToAtomicsLocked();
  return true;
}

std::string AeylaVisualDmx::ActiveSongStatus() const
{
  const std::scoped_lock lock(mModelMutex);
  const auto& snapshot = mModel.snapshot();
  if(snapshot.song_count == 0U)
    return "NO SONG";
  return "SONG " + std::to_string(snapshot.active_song_index + 1U) + "/" +
         std::to_string(snapshot.song_count) + " · " + snapshot.active_song_name;
}

bool AeylaVisualDmx::SelectAdjacentLookFromUI(int direction)
{
  if(direction == 0)
    return false;
  const std::scoped_lock lock(mModelMutex);
  const auto& document = mModel.project_document();
  const auto current = std::find_if(
      document.looks.begin(), document.looks.end(),
      [&](const aeyla::project::LookDocument& look) {
        return look.look_id == document.visual.active_look_id;
      });
  if(current == document.looks.end())
    return false;
  const std::size_t currentIndex = static_cast<std::size_t>(
      std::distance(document.looks.begin(), current));
  std::size_t target = currentIndex;
  if(direction < 0 && currentIndex > 0U)
    target = currentIndex - 1U;
  else if(direction > 0 && currentIndex + 1U < document.looks.size())
    target = currentIndex + 1U;
  if(target == currentIndex || !mModel.select_look(target))
    return false;

  const auto& selected = mModel.project_document().looks[target];
  int source = 1;
  if(selected.source == "solid") source = 0;
  else if(selected.source == "wave") source = 2;
  else if(selected.source == "noise") source = 3;
  else if(selected.source == "chase") source = 4;
  mParamSource.store(source, std::memory_order_release);
  mLastAppliedSource = source;
  mParamSpeed.store(selected.speed, std::memory_order_release);
  mParamWhiteExtract.store(selected.white_extraction, std::memory_order_release);
  mParamAmberExtract.store(selected.amber_extraction, std::memory_order_release);
  mParamUv.store(selected.uv_manual, std::memory_order_release);
  mParamBlackout.store(true, std::memory_order_release);
  mLookParameterUiSyncPending.store(true, std::memory_order_release);
  SyncSnapshotToAtomicsLocked();
  return true;
}

std::string AeylaVisualDmx::ActiveLookStatus() const
{
  const std::scoped_lock lock(mModelMutex);
  const auto& document = mModel.project_document();
  const auto current = std::find_if(
      document.looks.begin(), document.looks.end(),
      [&](const aeyla::project::LookDocument& look) {
        return look.look_id == document.visual.active_look_id;
      });
  if(current == document.looks.end())
    return "NO LOOK";
  const auto index = static_cast<std::size_t>(
      std::distance(document.looks.begin(), current));
  return "LOOK " + std::to_string(index + 1U) + "/" +
         std::to_string(document.looks.size()) + " · " + current->name;
}

aeyla::product::AuthoringResult AeylaVisualDmx::StoreLookFromUI()
{
  CaptureAllParameterValuesFromHost();
  const std::scoped_lock lock(mModelMutex);
  ApplyPendingParameterStateLocked();
  auto result = mModel.store_current_look();
  if(result.succeeded)
    mParamBlackout.store(true, std::memory_order_release);
  SyncSnapshotToAtomicsLocked();
  return result;
}

aeyla::product::AuthoringResult AeylaVisualDmx::CreateSongFromUI()
{
  const std::scoped_lock lock(mModelMutex);
  auto result = mModel.create_song();
  if(result.succeeded)
  {
    mParamBlackout.store(true, std::memory_order_release);
    mActiveSongBound.store(false, std::memory_order_release);
    mLastProjectedSongId.clear();
    mLastProjectedTick = 0U;
  }
  SyncSnapshotToAtomicsLocked();
  return result;
}

aeyla::product::AuthoringResult AeylaVisualDmx::StoreCueAtPlayheadFromUI()
{
  const auto host = mHostTransport.latest();
  if(host.revision == 0U || !host.ppq_position_valid ||
     !std::isfinite(host.ppq_position))
    return {false, {}, "DAW playhead position is unavailable"};

  const std::scoped_lock lock(mModelMutex);
  const auto& snapshot = mModel.snapshot();
  const auto& show = mModel.show_program();
  if(snapshot.active_song_index >= show.songs.size())
    return {false, {}, "Create a Song before storing a Cue"};
  const auto& song = show.songs[snapshot.active_song_index];

  std::optional<double> hostStartPpq;
  {
    const std::scoped_lock stateLock(mHostStateMutex);
    const auto binding = std::find_if(
        mHostStateCache.song_bindings.begin(),
        mHostStateCache.song_bindings.end(),
        [&](const aeyla::runtime::SessionSongBinding& candidate) {
          return candidate.song_id == song.song_id;
        });
    if(binding != mHostStateCache.song_bindings.end())
      hostStartPpq = binding->host_start_ppq;
  }
  if(!hostStartPpq.has_value())
    return {false, {}, "Set the active Song start from the DAW playhead first"};

  const aeyla::runtime::HostSongBinding binding{song.song_id, *hostStartPpq};
  const auto authoringTick =
      aeyla::runtime::project_host_transport_to_authoring_tick(
          host, binding, song);
  if(!authoringTick.has_value())
    return {false, {}, "DAW playhead is before the active Song start or unavailable"};

  auto result = mModel.store_cue_at_tick(*authoringTick);
  if(result.succeeded)
    mParamBlackout.store(true, std::memory_order_release);
  SyncSnapshotToAtomicsLocked();
  return result;
}

bool AeylaVisualDmx::ToggleFixtureInActiveLookFromUI(int fixtureIndex)
{
  if(fixtureIndex < 0 || fixtureIndex >= 14)
    return false;
  const std::scoped_lock lock(mModelMutex);
  const bool changed = mModel.toggle_active_look_fixture(
      static_cast<std::size_t>(fixtureIndex));
  if(changed)
    mParamBlackout.store(true, std::memory_order_release);
  SyncSnapshotToAtomicsLocked();
  return changed;
}

bool AeylaVisualDmx::FixtureIncludedInActiveLook(int fixtureIndex) const
{
  if(fixtureIndex < 0 || fixtureIndex >= 14)
    return false;
  const std::scoped_lock lock(mModelMutex);
  return mModel.active_look_fixture_enabled(
      static_cast<std::size_t>(fixtureIndex));
}

bool AeylaVisualDmx::SetActiveLookColorFromUI(bool secondary,
                                              float red, float green,
                                              float blue)
{
  const std::scoped_lock lock(mModelMutex);
  const bool changed = mModel.set_active_look_color(
      secondary, {red, green, blue});
  if(changed)
    mParamBlackout.store(true, std::memory_order_release);
  SyncSnapshotToAtomicsLocked();
  return changed;
}

std::array<float, 3> AeylaVisualDmx::ActiveLookColor(bool secondary) const
{
  const std::scoped_lock lock(mModelMutex);
  return mModel.active_look_color(secondary);
}

bool AeylaVisualDmx::SetActiveLookIntensityFromUI(float intensity)
{
  const std::scoped_lock lock(mModelMutex);
  const bool changed = mModel.set_active_look_intensity(intensity);
  if(changed)
    mParamBlackout.store(true, std::memory_order_release);
  SyncSnapshotToAtomicsLocked();
  return changed;
}

float AeylaVisualDmx::ActiveLookIntensity() const
{
  const std::scoped_lock lock(mModelMutex);
  return mModel.active_look_intensity();
}

aeyla::product::AuthoringResult AeylaVisualDmx::ConfigureArtNetFromUI(
    std::string_view specification)
{
  const std::string normalized = TrimAscii(specification);
  if(IsOffSpecification(normalized))
  {
    const std::scoped_lock lock(mModelMutex);
    auto result = mModel.disable_output_backend();
    RefreshOutputBackendFromProjectLocked();
    SyncSnapshotToAtomicsLocked();
    return result;
  }

  const std::size_t separator = normalized.rfind('@');
  if(separator == std::string::npos || separator == 0U ||
     separator + 1U >= normalized.size())
    return {false, {}, "Use numeric IPv4@universe, for example 2.0.0.20@0, or OFF"};

  const std::string target = TrimAscii(
      std::string_view(normalized).substr(0U, separator));
  const std::string universeText = TrimAscii(
      std::string_view(normalized).substr(separator + 1U));
  unsigned universe = 0U;
  const auto parsed = std::from_chars(
      universeText.data(), universeText.data() + universeText.size(), universe);
  if(parsed.ec != std::errc{} ||
     parsed.ptr != universeText.data() + universeText.size() ||
     universe > 0x7FFFU)
    return {false, {}, "Art-Net universe must be a number from 0 to 32767"};

  aeyla::output::ArtNetOutputConfig preflight;
  preflight.target_ipv4 = target;
  preflight.port_address = static_cast<std::uint16_t>(universe);
  preflight.frames_per_second = 40U;
  std::string error;
  if(!aeyla::output::validate_artnet_output_config(preflight, error))
    return {false, {}, error};

  const std::scoped_lock lock(mModelMutex);
  auto result = mModel.configure_artnet_output(
      target, static_cast<std::uint16_t>(universe));
  if(!result.succeeded)
    return result;
  RefreshOutputBackendFromProjectLocked();
  SyncSnapshotToAtomicsLocked();
  if(!mModel.snapshot().backend_ready)
    return {false, target, mOutputBackendError.empty()
                                ? "Art-Net backend preflight failed"
                                : mOutputBackendError};
  return {true, target, "Art-Net ready at " + target + "@" + universeText};
}

std::string AeylaVisualDmx::OutputBackendStatus() const
{
  const std::scoped_lock lock(mModelMutex);
  const auto& output = mModel.project_document().output;
  if(output.backend != "artnet" || output.target.empty())
    return "OUTPUT OFF";
  return "ARTNET " + output.target + "@" + std::to_string(output.universe);
}

std::string AeylaVisualDmx::OutputBackendError() const
{
  const std::scoped_lock lock(mModelMutex);
  return mOutputBackendError;
}

void AeylaVisualDmx::RefreshOutputBackendFromProjectLocked()
{
  mArtNetOutput.set_enabled(false);
  mArtNetOutput.stop();
  mLastArtNetSendErrors = 0U;
  mOutputBackendError.clear();

  const auto& output = mModel.project_document().output;
  if(output.backend != "artnet" || output.target.empty())
  {
    mModel.set_backend_ready(false);
    if(output.backend != "none")
      mOutputBackendError = "Configured output backend is not implemented";
    return;
  }

  aeyla::output::ArtNetOutputConfig config;
  config.target_ipv4 = output.target;
  config.port_address = output.universe;
  config.frames_per_second = 40U;
  if(!mArtNetOutput.start(config, mOutputBackendError))
  {
    mModel.set_backend_ready(false);
    return;
  }
  mModel.set_backend_ready(true);
}

void AeylaVisualDmx::PublishOutputFrameLocked(bool renderingOffline)
{
  const auto& snapshot = mModel.snapshot();
  if(!snapshot.backend_ready)
  {
    mArtNetOutput.set_enabled(false);
    return;
  }

  mArtNetOutput.publish_latest(snapshot.dmx, snapshot.generation);
  mArtNetOutput.set_enabled(snapshot.output_armed && !renderingOffline);

  const auto stats = mArtNetOutput.stats();
  if(stats.send_errors <= mLastArtNetSendErrors)
    return;

  mLastArtNetSendErrors = stats.send_errors;
  mOutputBackendError = "Art-Net send failed; output was disarmed";
  mArtNetOutput.set_enabled(false);
  mModel.release_transients();
  mModel.set_backend_ready(false);
  mModel.disarm(aeyla::runtime::RuntimeSafetyReason::backend_unavailable);
  mModel.set_blackout(true);
  mParamBlackout.store(true, std::memory_order_release);
  SyncSnapshotToAtomicsLocked();
}

void AeylaVisualDmx::SetOutputArmed(bool armed)
{
  const std::scoped_lock lock(mModelMutex);
  if(armed && RuntimeHealthy() &&
     !mRenderingOffline.load(std::memory_order_acquire))
    (void) mModel.request_arm();
  else if(armed)
    mModel.disarm(mRenderingOffline.load(std::memory_order_acquire)
                      ? aeyla::runtime::RuntimeSafetyReason::offline_render
                      : aeyla::runtime::RuntimeSafetyReason::runtime_fault);
  else
    mModel.disarm();

  SyncSnapshotToAtomicsLocked();
  PublishOutputFrameLocked(
      mRenderingOffline.load(std::memory_order_acquire));
}

void AeylaVisualDmx::SyncSnapshotToAtomicsLocked() noexcept
{
  const auto& snapshot = mModel.snapshot();
  mOutputArmed.store(snapshot.output_armed, std::memory_order_release);
  mEffectiveBlackout.store(snapshot.blackout, std::memory_order_release);
  mBackendReady.store(snapshot.backend_ready, std::memory_order_release);
  mProjectValid.store(snapshot.project_valid, std::memory_order_release);
  mActiveExecutor.store(snapshot.active_executor, std::memory_order_relaxed);
  mDmxGeneration.store(snapshot.generation, std::memory_order_relaxed);
  mVisualPhase.store(snapshot.phase, std::memory_order_relaxed);

  const int nonZero = static_cast<int>(std::count_if(
      snapshot.dmx.begin(), snapshot.dmx.end(),
      [](std::uint8_t value) { return value != 0; }));
  mDmxNonZeroChannels.store(nonZero, std::memory_order_relaxed);
}

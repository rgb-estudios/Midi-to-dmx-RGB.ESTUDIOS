#include "AeylaVisualDmx.h"
#include "AeylaMainControl.h"
#include "AeylaRuntimeStatusControl.h"
#include "AeylaTakeLibrarySession.h"
#include "IPlug_include_in_plug_src.h"
#include "network/ipv4_configuration.h"
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
  GetParam(kParamBlackout)->InitBool("Apagón", true);
  GetParam(kParamGrandMaster)->InitPercentage("Master general", 100.0);
  GetParam(kParamRigMode)->InitEnum("Modo de rig", 0, 2, "", IParam::kFlagsNone,
                                     "", "10 luminarias", "14 luminarias");
  GetParam(kParamSource)->InitEnum("Fuente visual", 1, 5, "", IParam::kFlagsNone,
                                    "", "Color plano", "Degradado", "Onda", "Ruido", "Secuencia");
  GetParam(kParamSpeed)->InitPercentage("Velocidad de animación", 35.0);
  GetParam(kParamWhiteExtract)->InitPercentage("Extracción de blanco", 20.0);
  GetParam(kParamAmberExtract)->InitPercentage("Extracción de ámbar", 15.0);
  GetParam(kParamUV)->InitPercentage("UV manual", 0.0);

  CaptureAllParameterValuesFromHost();
  SyncSnapshotToAtomicsLocked();
  RefreshHostStateCacheLocked();
  mShowMidiMappingPacked.store(aeyla::runtime::pack_show_midi_mapping(
                                   mHostStateCache.show_midi),
                               std::memory_order_release);
  mShowMidiCaptureStartNote.store(mHostStateCache.show_midi.capture_start_note,
                                  std::memory_order_release);
  mShowMidiCaptureStopNote.store(mHostStateCache.show_midi.capture_stop_note,
                                 std::memory_order_release);

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
    pGraphics->AttachControl(new AeylaRuntimeStatusControl(bounds, *this),
                             kCtrlTagRuntimeStatus);
  };
#endif

  aeyla::live_memory_session::register_runtime(
      this, &mArtNetOutput, &mArtNetCapture);
  StartRuntimeWorker();
}

AeylaVisualDmx::~AeylaVisualDmx()
{
  mArtNetOutput.set_enabled(false);
  StopRuntimeWorker();
  mNetworkConfiguration.Shutdown();
  mArtNetOutput.stop();
  aeyla::live_memory_session::clear(this);
  aeyla::take_library_session::clear(this);
}

#if IPLUG_DSP
void AeylaVisualDmx::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  (void) inputs;

  const bool renderingOffline = GetRenderingOffline();
  const bool transportRunning = GetTransportIsRunning();

  const bool transportWasRunning = mAudioTransportRunning.exchange(
      transportRunning, std::memory_order_acq_rel);
  if(!transportWasRunning && transportRunning &&
     mArtNetCapture.streamed_recording_active())
  {
    mPendingCaptureTransportFrame.store(
        mArtNetCapture.recorded_frames_fast(), std::memory_order_release);
    mCaptureTransportMarkerRevision.fetch_add(1U, std::memory_order_release);
  }

  mHostTransport.publish(transportRunning,
                         renderingOffline,
                         GetSamplePos(),
                         GetPPQPos(),
                         GetTempo());
  if(nFrames > 0)
  {
    mAudioAdvanceSequence.fetch_add(1U, std::memory_order_acq_rel);
    if(transportRunning)
    {
      if(!mShowTransportMutation.load(std::memory_order_acquire))
        mTakeScheduler.advance_samples(static_cast<std::uint32_t>(nFrames),
                                       renderingOffline);
      mProcessedTransportSamples.fetch_add(
          static_cast<std::uint64_t>(nFrames), std::memory_order_release);
    }
    mProcessedAudioSamples.fetch_add(static_cast<std::uint64_t>(nFrames),
                                     std::memory_order_release);
    mAudioAdvanceSequence.fetch_add(1U, std::memory_order_release);
  }

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

    const auto channel = static_cast<std::uint8_t>(
        std::clamp(msg.Channel() + 1, 1, 16));
    const auto midiNote = static_cast<std::uint8_t>(std::clamp(note, 0, 127));
    const bool positiveNoteOn = status == IMidiMsg::kNoteOn && velocity > 0;

    if(positiveNoteOn)
    {
      const auto learnTarget = mShowMidiLearnTarget.exchange(
          aeyla::runtime::ShowMidiLearnTarget::none,
          std::memory_order_acq_rel);
      if(learnTarget != aeyla::runtime::ShowMidiLearnTarget::none)
      {
        const std::uint32_t packed =
            static_cast<std::uint32_t>(learnTarget) |
            (static_cast<std::uint32_t>(channel) << 8U) |
            (static_cast<std::uint32_t>(midiNote) << 16U) |
            (1U << 24U);
        mPendingMidiLearnPacked.store(packed, std::memory_order_release);
        return;
      }
    }

    aeyla::runtime::ShowMidiMatch showMatch{};
    const auto showMapping = ShowMidiMapping();
    const bool mappedShowNote = aeyla::runtime::match_show_midi_note(
        showMapping, channel, midiNote, 127U, showMatch);
    if(mappedShowNote)
    {
      if(positiveNoteOn)
      {
        const auto completed = mProcessedAudioSamples.load(
            std::memory_order_acquire);
        const auto transportCompleted = mProcessedTransportSamples.load(
            std::memory_order_acquire);
        const auto offset = static_cast<std::uint32_t>(
            std::max(msg.mOffset, 0));
        const auto captureFrame = mArtNetCapture.recorded_frames_fast();
        const auto showEvent = aeyla::runtime::make_show_midi_event(
            showMatch.command, showMatch.song_index, channel, midiNote,
            completed, transportCompleted, offset, captureFrame);
        (void)mShowMidiIngress.try_submit(showEvent);
      }
      return;
    }

    aeyla::runtime::HostEvent event{};
    event.type = status == IMidiMsg::kNoteOn && velocity > 0
                     ? aeyla::runtime::HostEventType::note_on
                     : aeyla::runtime::HostEventType::note_off;
    event.channel = channel;
    event.note = midiNote;
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

    (void)mHostIngress.try_submit(event);
    return;
  }

  if(status == IMidiMsg::kControlChange)
  {
    // R10.1: a CC is copied as a compact queue event only. No live-memory
    // lookup, mutex, Art-Net call or UI work is allowed in the host callback.
    mMidiEventCount.fetch_add(1, std::memory_order_relaxed);
    aeyla::runtime::HostEvent event{};
    event.type = aeyla::runtime::HostEventType::note_on;
    event.channel = static_cast<std::uint8_t>(
        std::clamp(msg.Channel() + 1, 1, 16));
    event.note = static_cast<std::uint8_t>(std::clamp<int>(msg.mData1, 0, 127));
    event.reserved = 1U;
    event.value = static_cast<float>(std::clamp<int>(msg.mData2, 0, 127)) / 127.0F;
    event.sample_offset = msg.mOffset;
    (void)mHostIngress.try_submit(event);
  }
}

void AeylaVisualDmx::OnReset()
{
  mLastMidiNote.store(-1, std::memory_order_relaxed);
  mAudioTransportRunning.store(false, std::memory_order_release);

  // Host/device reset is not an operator withdrawal command. REAPER may call
  // this while rebuilding or refocusing the plug-in. Keep physical Art-Net
  // authority untouched; only release transient MIDI state.
  aeyla::runtime::HostEvent event{};
  event.type = aeyla::runtime::HostEventType::all_notes_off;
  (void)mHostIngress.try_submit(event);
}

void AeylaVisualDmx::OnActivate(bool active)
{
  // Editor/host activation is not DESARMAR and is not APAGÓN TOTAL.
  // Physical authority survives normal focus/window lifecycle changes.
  (void)active;
}

void AeylaVisualDmx::OnParamChange(int paramIdx)
{
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
    mGlobalBlackout.store(true, std::memory_order_release);
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
  mTakeScheduler.disarm();
  mRuntimeStopRequested.store(true, std::memory_order_release);
  if(mRuntimeThread.joinable())
    mRuntimeThread.join();

  mParamBlackout.store(true, std::memory_order_release);
  mOutputArmed.store(false, std::memory_order_release);
  mGlobalBlackout.store(true, std::memory_order_release);
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
    ReconcileNetworkConfiguration();
    const auto host = mHostTransport.latest();
    if(mArtNetCapture.streamed_recording_active())
    {
      const auto markerRevision = mCaptureTransportMarkerRevision.load(
          std::memory_order_acquire);
      if(markerRevision != mLastCaptureTransportMarkerRevision)
      {
        const auto markerFrame = mPendingCaptureTransportFrame.load(
            std::memory_order_acquire);
        (void)mCaptureSyncAnchor.anchor_transport_snapshot(markerFrame);
        mLastCaptureTransportMarkerRevision = markerRevision;
      }

      const auto capture = mArtNetCapture.stats();
      (void)mCaptureSyncAnchor.observe(host, capture.recorded_frames);
    }
    else
    {
      mLastCaptureTransportMarkerRevision =
          mCaptureTransportMarkerRevision.load(std::memory_order_acquire);
    }

    const std::scoped_lock lock(mModelMutex);
    if(mRuntimeFaulted.load(std::memory_order_acquire))
    {
      mTakeScheduler.disarm();
      mModel.release_transients();
      mModel.disarm(aeyla::runtime::RuntimeSafetyReason::runtime_fault);
      mModel.set_blackout(true);
      mParamBlackout.store(true, std::memory_order_release);
      SyncSnapshotToAtomicsLocked();
      return;
    }
    ApplyPendingHostStateLocked();

    if(mHostResetPending.exchange(false, std::memory_order_acq_rel))
    {
      // Defensive compatibility for a reset flag queued before R10.4. Never
      // convert a host reset into an operator blackout or scheduler disarm.
      SetShowMidiMessage("HOST RESET · AUTORIDAD ART-NET CONSERVADA");
    }

    if(mHostDeactivationPending.exchange(false, std::memory_order_acq_rel))
    {
      // Losing editor/host activation is a normal DAW lifecycle event. It must
      // not alter scheduler ARM, carrier, active Take or operator blackout.
      SetShowMidiMessage("HOST INACTIVO · AUTORIDAD ART-NET CONSERVADA");
    }

    ApplyPendingParameterStateLocked();

    const bool wasOffline = mRenderingOffline.exchange(
        host.rendering_offline, std::memory_order_acq_rel);
    if(host.rendering_offline)
    {
      if(!wasOffline)
      {
        ClearShowMidiCommandsLocked();
        if(ShowMidiMapping().enabled)
          mMidiPreflightCursor.store(0, std::memory_order_release);
      }
      mTakeScheduler.disarm();
      if(!wasOffline)
        mModel.release_transients();
      mModel.disarm(aeyla::runtime::RuntimeSafetyReason::offline_render);
      mModel.set_blackout(true);
      mParamBlackout.store(true, std::memory_order_release);
    }

    if(!host.rendering_offline)
      DrainShowMidiCommandsLocked(host);

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
          mModel.seek_active_song_tick(activeSong->length_ticks);
          mLastProjectedSongId = activeSong->song_id;
          mLastProjectedTick = activeSong->length_ticks;
        }
      }
      else if(!mLastHostRunning || mLastProjectedSongId != activeSong->song_id)
      {
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

    DrainHostEventsLocked();
    if(host.running && activeSong != nullptr && !hostSongPositionSafe)
    {
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
    mRuntimeFaulted.store(true, std::memory_order_release);
    mParamBlackout.store(true, std::memory_order_release);
    mOutputArmed.store(false, std::memory_order_release);
    mGlobalBlackout.store(true, std::memory_order_release);
    mEffectiveBlackout.store(true, std::memory_order_release);
    mDmxNonZeroChannels.store(0, std::memory_order_relaxed);
    mActiveTakeSongIndex.store(-1, std::memory_order_release);
    mTakeScheduler.disarm();
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

void AeylaVisualDmx::ReconcileNetworkConfiguration() noexcept
{
  try
  {
    const auto operation = mNetworkConfiguration.Snapshot();
    if(operation.revision == 0U ||
       operation.revision == mLastNetworkConfigurationRevision.load(
           std::memory_order_acquire))
      return;
    if(operation.busy() ||
       operation.state == AeylaNetworkConfigurationState::idle)
    {
      mLastNetworkConfigurationRevision.store(operation.revision,
                                              std::memory_order_release);
      return;
    }

    if(operation.state != AeylaNetworkConfigurationState::committed)
    {
      const std::scoped_lock lock(mNetworkMutex);
      mNetworkConfigurationMessage = operation.state ==
              AeylaNetworkConfigurationState::rolled_back
          ? "CAMBIO REVERTIDO · " + operation.message
          : "CAMBIO RECHAZADO · " + operation.message;
      mLastNetworkConfigurationRevision.store(operation.revision,
                                              std::memory_order_release);
      return;
    }

    std::string networkError;
    const auto network = aeyla::network::make_ipv4_network(
        operation.ipv4, operation.prefix_length, networkError);
    if(!network.has_value())
    {
      const std::scoped_lock lock(mNetworkMutex);
      mNetworkConfigurationMessage =
          "CAMBIO NO APLICADO AL RUNTIME · " + networkError;
      mLastNetworkConfigurationRevision.store(operation.revision,
                                              std::memory_order_release);
      return;
    }

    const auto discovered = aeyla::network::enumerate_ipv4_interfaces();
    std::string pendingTxId;
    std::string previousRxId;
    std::string previousRxAddress;
    {
      const std::scoped_lock lock(mNetworkMutex);
      pendingTxId = mPendingTxAdapterId;
      if(mRxInterfaceIndex < mNetworkInterfaces.size())
      {
        previousRxId = mNetworkInterfaces[mRxInterfaceIndex].id;
        previousRxAddress = mNetworkInterfaces[mRxInterfaceIndex].ipv4;
      }
    }

    const auto tx = std::find_if(
        discovered.begin(), discovered.end(),
        [&](const aeyla::network::NetworkInterface& item) {
          return item.id == pendingTxId && item.ipv4 == operation.ipv4 &&
                 item.prefix_length == operation.prefix_length;
        });
    if(tx == discovered.end())
    {
      const std::scoped_lock lock(mNetworkMutex);
      mNetworkConfigurationMessage =
          "CAMBIO SIN CONFIRMAR · Windows no devolvió la IPv4 en el adaptador TX";
      mLastNetworkConfigurationRevision.store(operation.revision,
                                              std::memory_order_release);
      return;
    }

    std::size_t nextRx = 0U;
    const auto rx = std::find_if(
        discovered.begin(), discovered.end(),
        [&](const aeyla::network::NetworkInterface& item) {
          return item.id == previousRxId &&
                 (previousRxAddress.empty() || item.ipv4 == previousRxAddress);
        });
    if(rx != discovered.end())
      nextRx = static_cast<std::size_t>(std::distance(discovered.begin(), rx));
    const auto nextTx =
        static_cast<std::size_t>(std::distance(discovered.begin(), tx));

    {
      const std::scoped_lock lock(mNetworkMutex);
      mNetworkInterfaces = discovered;
      mRxInterfaceIndex = nextRx;
      mTxInterfaceIndex = nextTx;
      mNetworkConfigurationMessage = operation.message;
    }

    mTakeScheduler.disarm();
    mArtNetOutput.set_preferred_source_ipv4(network->address);
    {
      const std::scoped_lock lock(mModelMutex);
      const auto configured = mModel.configure_artnet_output(
          network->directed_broadcast, 0U);
      if(!configured.succeeded)
      {
        const std::scoped_lock networkLock(mNetworkMutex);
        mNetworkConfigurationMessage =
            "IPv4 APLICADA / SALIDA BLOQUEADA · " + configured.message;
        SyncSnapshotToAtomicsLocked();
        mLastNetworkConfigurationRevision.store(operation.revision,
                                                std::memory_order_release);
        return;
      }
      RefreshOutputBackendFromProjectLocked();
      mParamBlackout.store(true, std::memory_order_release);
      SyncSnapshotToAtomicsLocked();
      const bool ready = mModel.snapshot().backend_ready;
      const std::scoped_lock networkLock(mNetworkMutex);
      mNetworkConfigurationMessage = ready
          ? "RED LISTA · " + network->address + "/" +
                std::to_string(network->prefix_length) + " → " +
                network->directed_broadcast +
                " · U1 · SALIDA DESARMADA · APAGÓN ACTIVO"
          : "IPv4 APLICADA / MOTOR ART-NET BLOQUEADO · " + mOutputBackendError;
    }
    RestartCaptureInputFromRouting();
    mLastNetworkConfigurationRevision.store(operation.revision,
                                            std::memory_order_release);
  }
  catch(...)
  {
    const std::scoped_lock lock(mNetworkMutex);
    mNetworkConfigurationMessage =
        "FALLA AL CONFIRMAR LA RED · salida física permanece desarmada";
    mLastNetworkConfigurationRevision.store(
        mNetworkConfiguration.Snapshot().revision,
        std::memory_order_release);
  }
}

void AeylaVisualDmx::ApplyPendingHostStateLocked()
{
  if(mHostStateRestoreRejected.exchange(false, std::memory_order_acq_rel))
  {
    ClearShowMidiCommandsLocked();
    mTakeScheduler.disarm();
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

  ClearShowMidiCommandsLocked();
  mTakeScheduler.disarm();
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
  mShowMidiMappingPacked.store(
      aeyla::runtime::pack_show_midi_mapping(pending->show_midi),
      std::memory_order_release);
  mShowMidiCaptureStartNote.store(pending->show_midi.capture_start_note,
                                  std::memory_order_release);
  mShowMidiCaptureStopNote.store(pending->show_midi.capture_stop_note,
                                 std::memory_order_release);
  mMidiPreflightCursor.store(pending->show_midi.enabled ? 0 : -1,
                             std::memory_order_release);
  SetShowMidiMessage(pending->show_midi.enabled
      ? "MIDI SHOW RESTAURADO · canal " +
            std::to_string(pending->show_midi.channel) +
            " · salida física permanece desarmada"
      : "MIDI SHOW DESACTIVADO · mapa restaurado");
  const bool checksumMismatch = !IsZero(pending->project_checksum) &&
                                pending->project_checksum != previousCache.project_checksum;

  if(uuidMismatch || schemaMismatch || checksumMismatch)
  {
    mModel.set_project_valid(false);
    mModel.set_blackout(true);
    mParamBlackout.store(true, std::memory_order_release);
    mHostStateRestoreErrors.fetch_add(1U, std::memory_order_relaxed);
    {
      const std::scoped_lock stateLock(mHostStateMutex);
      mHostStateCache.song_bindings.clear();
      mHostStateCache.take_library_locator.clear();
      mHostStateCache.take_bindings.clear();
    }
    aeyla::take_library_session::clear(this);
  }
  else
  {
    aeyla::take_library_session::stage_persisted_state(
        this, mModel.project_document().project_id,
        pending->take_library_locator, pending->take_bindings);
  }

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
  {
    if(aeyla::live_memory_session::process_midi_event(this, event))
      continue;
    // `reserved == 1` is an EN VIVO CC. Unmapped CC did not reach the legacy
    // artistic model before R10.1 and must remain inert rather than masquerade
    // as a NoteOn.
    if(event.reserved == 1U)
      continue;
    mModel.handle_host_event(event);
  }

  // MIDI Learn is finalized on this non-realtime thread. Persist its authored
  // binding into the project controller and mark the project unsaved here so
  // the footer cannot continue to claim GUARDADO after a successful Learn.
  CommitLiveMemoryPersistenceDirtyLocked();
}

void AeylaVisualDmx::RefreshHostStateCacheLocked()
{
  const auto& snapshot = mModel.snapshot();
  if(!snapshot.project_valid)
    return;

  std::array<std::uint8_t, 16> uuid{};
  if(!DecodeCanonicalUuid(snapshot.project_id, uuid))
    return;

  aeyla::take_library_session::ensure_scope(this, snapshot.project_id);
  std::vector<std::string> songIds;
  const auto& show = mModel.show_program();
  songIds.reserve(std::min(
      show.songs.size(), aeyla::runtime::kMaxSessionTakeBindings));
  for(const auto& song : show.songs)
  {
    if(songIds.size() >= aeyla::runtime::kMaxSessionTakeBindings)
      break;
    songIds.push_back(song.song_id);
  }
  auto hostTakeState =
      aeyla::take_library_session::snapshot_for_host(this, songIds);

  const std::scoped_lock lock(mHostStateMutex);
  if(mHostStateCache.project_uuid != uuid)
  {
    mHostStateCache.project_checksum.fill(0U);
    mHostStateCache.locator_mode = aeyla::runtime::ProjectLocatorMode::none;
    mHostStateCache.project_locator.clear();
    mHostStateCache.song_bindings.clear();
    mHostStateCache.take_library_locator.clear();
    mHostStateCache.take_bindings.clear();
  }
  mHostStateCache.project_uuid = uuid;
  mHostStateCache.project_schema_major =
      mModel.project_document().schema_version.major;
  mHostStateCache.project_schema_minor =
      mModel.project_document().schema_version.minor;
  mHostStateCache.grand_master = snapshot.grand_master;
  mHostStateCache.blackout = snapshot.global_blackout;
  mHostStateCache.take_library_locator =
      std::move(hostTakeState.library_locator);
  mHostStateCache.take_bindings = std::move(hostTakeState.bindings);
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
  if(direction == 0 || TakeRecording())
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
        mHostStateCache.song_bindings.begin(), mHostStateCache.song_bindings.end(),
        [&](const aeyla::runtime::SessionSongBinding& candidate) {
          return candidate.song_id == mModel.snapshot().active_song_id;
        });
  }
  mActiveSongBound.store(bound, std::memory_order_release);
  mMidiPreloadSongRequest.store(static_cast<int>(target),
                                std::memory_order_release);
  SetShowMidiMessage("PREPARADA · " + mModel.snapshot().active_song_name +
                     " · la canción al aire continúa");
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
    return "SIN CANCIÓN";
  return "CANCIÓN " + std::to_string(snapshot.active_song_index + 1U) + "/" +
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
    return "SIN LOOK";
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
  if(TakeRecording())
    return {false, {}, "Detén y guarda la toma antes de crear otra canción"};
  mTakeScheduler.stop_reset();
  mTakeScheduler.disarm();
  mLoadedTakeSongIndex.store(-1, std::memory_order_release);
  mActiveTakeSongIndex.store(-1, std::memory_order_release);
  const std::scoped_lock lock(mModelMutex);
  auto result = mModel.create_song();
  if(result.succeeded)
  {
    mParamBlackout.store(true, std::memory_order_release);
    mActiveSongBound.store(false, std::memory_order_release);
    mLastProjectedSongId.clear();
    mLastProjectedTick = 0U;
    if(ShowMidiMapping().enabled)
      mMidiPreflightCursor.store(0, std::memory_order_release);
  }
  SyncSnapshotToAtomicsLocked();
  return result;
}

aeyla::product::AuthoringResult AeylaVisualDmx::StoreCueAtPlayheadFromUI()
{
  const auto host = mHostTransport.latest();
  if(host.revision == 0U || !host.ppq_position_valid ||
     !std::isfinite(host.ppq_position))
    return {false, {}, "La posición del cursor del DAW no está disponible"};

  const std::scoped_lock lock(mModelMutex);
  const auto& snapshot = mModel.snapshot();
  const auto& show = mModel.show_program();
  if(snapshot.active_song_index >= show.songs.size())
    return {false, {}, "Crea una canción antes de guardar un cue"};
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
    return {false, {}, "Primero fija el inicio de la canción desde el cursor del DAW"};

  const aeyla::runtime::HostSongBinding binding{song.song_id, *hostStartPpq};
  const auto authoringTick =
      aeyla::runtime::project_host_transport_to_authoring_tick(
          host, binding, song);
  if(!authoringTick.has_value())
    return {false, {}, "El cursor del DAW está antes del inicio de la canción o no está disponible"};

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
    return {false, {}, "Usa IPv4@universo numérico, por ejemplo 2.0.0.20@0, o DESACTIVADO"};

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
    return {false, {}, "El universo Art-Net debe ser un número entre 0 y 32767"};

  aeyla::output::ArtNetOutputConfig preflight;
  preflight.target_ipv4 = target;
  preflight.port_address = static_cast<std::uint16_t>(universe);
  preflight.frames_per_second = aeyla::output::kAeylaArtNetFramesPerSecond;
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
                                ? "Falló la comprobación previa del backend Art-Net"
                                : mOutputBackendError};
  return {true, target, "Art-Net listo en " + target + "@" + universeText};
}

std::string AeylaVisualDmx::OutputBackendStatus() const
{
  const std::scoped_lock lock(mModelMutex);
  const auto& output = mModel.project_document().output;
  if(output.backend != "artnet" || output.target.empty())
    return "SALIDA DESACTIVADA";
  return "ART-NET " + output.target + " · U" +
         std::to_string(static_cast<unsigned>(output.universe) + 1U);
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
  mLastHandledArtNetFailClosedEvents = 0U;
  mOutputBackendError.clear();

  const auto& output = mModel.project_document().output;
  if(output.backend != "artnet" || output.target.empty())
  {
    mModel.set_backend_ready(false);
    if(output.backend != "none")
      mOutputBackendError = "El backend de salida configurado no está implementado";
    return;
  }

  aeyla::output::ArtNetOutputConfig config;
  config.target_ipv4 = output.target;
  config.port_address = output.universe;
  config.frames_per_second = aeyla::output::kAeylaArtNetFramesPerSecond;
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
  mArtNetOutput.set_blackout_latched(snapshot.global_blackout && !renderingOffline);
  mArtNetOutput.set_enabled(snapshot.output_armed && !renderingOffline);

  const auto stats = mArtNetOutput.stats();
  if(!stats.fail_closed)
  {
    if(stats.send_errors > mLastArtNetSendErrors)
    {
      mLastArtNetSendErrors = stats.send_errors;
      mOutputBackendError =
          "Art-Net registró un error transitorio; la salida continúa vigilada";
    }
    else if(stats.consecutive_send_errors == 0U &&
            (mOutputBackendError.rfind(
                 "Art-Net registró un error transitorio", 0U) == 0U ||
             mOutputBackendError.rfind(
                 "Art-Net acumuló tres errores consecutivos", 0U) == 0U))
    {
      mOutputBackendError.clear();
    }
    return;
  }

  if(stats.fail_closed_events <= mLastHandledArtNetFailClosedEvents)
  {
    mArtNetOutput.set_enabled(false);
    return;
  }
  mLastHandledArtNetFailClosedEvents = stats.fail_closed_events;

  mLastArtNetSendErrors = stats.send_errors;
  mOutputBackendError =
      "Art-Net acumuló tres errores consecutivos; desactiva APAGÓN y vuelve a ARMAR manualmente";
  mTakeScheduler.disarm();
  mArtNetOutput.set_enabled(false);
  mModel.release_transients();
  mModel.disarm(aeyla::runtime::RuntimeSafetyReason::backend_unavailable);
  mModel.set_blackout(true);
  mParamBlackout.store(true, std::memory_order_release);
  SyncSnapshotToAtomicsLocked();
}

void AeylaVisualDmx::SetOutputArmed(bool armed)
{
  if(armed)
  {
    mShowMidiLearnTarget.store(aeyla::runtime::ShowMidiLearnTarget::none,
                               std::memory_order_release);
    mPendingMidiLearnPacked.store(0U, std::memory_order_release);
  }
  const std::scoped_lock lock(mModelMutex);
  if(armed && RuntimeHealthy() &&
     !mRenderingOffline.load(std::memory_order_acquire))
  {
    if(mModel.request_arm())
      mArtNetOutput.prepare_explicit_rearm();
  }
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
  mGlobalBlackout.store(snapshot.global_blackout, std::memory_order_release);
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

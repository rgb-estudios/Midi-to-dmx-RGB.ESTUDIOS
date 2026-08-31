#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include "capture/artnet_capture_worker.h"
#include "capture/dmx_capture_sync_anchor.h"
#include "capture/dmx_take_activity.h"
#include "capture/dmx_take_file_store.h"
#include "capture/dmx_take_scheduler.h"
#include "network/network_interfaces.h"
#include "AeylaNetworkConfiguration.h"
#include "AeylaLiveMemorySession.h"
#include "product/application_model.h"
#include "product/project_file_controller.h"
#include "product/project_identity.h"
#include "output/artnet_output_worker.h"
#include "runtime/host_event_ingress.h"
#include "runtime/host_transport_mailbox.h"
#include "runtime/plugin_state.h"
#include "runtime/show_midi_control.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <thread>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

constexpr int kNumPresets = 1;

enum EParams
{
  kParamBlackout = 0,
  kParamGrandMaster,
  kParamRigMode,
  kParamSource,
  kParamSpeed,
  kParamWhiteExtract,
  kParamAmberExtract,
  kParamUV,
  kNumParams
};

enum EControlTags
{
  kCtrlTagMain = 100,
  kCtrlTagRuntimeStatus,
  kNumCtrlTags
};

using namespace iplug;
using namespace igraphics;

struct AeylaTakeEditorSnapshot
{
  bool available{false};
  bool raw_source{true};
  std::filesystem::path path;
  std::string take_name;
  std::size_t version_index{0U};
  std::size_t version_count{0U};
  std::uint64_t frame_count{0U};
  std::uint64_t start_frame{0U};
  std::uint64_t end_frame_exclusive{0U};
  std::uint64_t current_frame{0U};
  std::uint16_t frames_per_second{0U};
  std::array<std::uint8_t, aeyla::capture::kMaximumTakeActivityBuckets>
      activity_level{};
  std::array<std::uint8_t, aeyla::capture::kMaximumTakeActivityBuckets>
      activity_motion{};
  std::size_t activity_count{0U};
};

class AeylaVisualDmx final : public Plugin
{
public:
  explicit AeylaVisualDmx(const InstanceInfo& info);
  ~AeylaVisualDmx() override;

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnActivate(bool active) override;
  void OnParamChange(int paramIdx) override;
#endif

  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;
  void OnIdle() override;

  void ToggleOutputArmFromUI();
  void ForceDisarmFromUI();
  // Operator-facing APAGÓN TOTAL. This is a physical zero-DMX mask with
  // absolute priority; it preserves ARM/carrier. DESARMAR is a separate action.
  // Releasing APAGÓN reveals the underlying HOLD/Take state without auto-arming.
  void SetBlackoutFromUI(bool enabled);
  void TriggerExecutorFromUI(int executorIndex, float velocity);
  void ReleaseExecutorFromUI(int executorIndex);
  [[nodiscard]] bool SetActiveSongStartFromPlayheadFromUI();
  [[nodiscard]] bool SelectAdjacentSongFromUI(int direction);
  [[nodiscard]] bool SelectSongFromUI(std::size_t songIndex);
  [[nodiscard]] std::size_t SongCount() const;
  [[nodiscard]] std::size_t ActiveSongIndex() const;
  [[nodiscard]] std::string SongName(std::size_t songIndex) const;
  [[nodiscard]] aeyla::product::AuthoringResult RenameSongFromUI(
      std::size_t songIndex, std::string_view name);
  [[nodiscard]] std::string ActiveSongStatus() const;
  [[nodiscard]] bool SelectAdjacentLookFromUI(int direction);
  [[nodiscard]] std::string ActiveLookStatus() const;
  [[nodiscard]] aeyla::product::AuthoringResult StoreLookFromUI();
  [[nodiscard]] aeyla::product::AuthoringResult CreateSongFromUI();
  [[nodiscard]] aeyla::product::AuthoringResult StoreCueAtPlayheadFromUI();
  [[nodiscard]] bool ToggleFixtureInActiveLookFromUI(int fixtureIndex);
  [[nodiscard]] bool FixtureIncludedInActiveLook(int fixtureIndex) const;
  [[nodiscard]] bool SetActiveLookColorFromUI(bool secondary,
                                               float red, float green,
                                               float blue);
  [[nodiscard]] std::array<float, 3> ActiveLookColor(bool secondary) const;
  [[nodiscard]] bool SetActiveLookIntensityFromUI(float intensity);
  [[nodiscard]] float ActiveLookIntensity() const;
  [[nodiscard]] aeyla::product::AuthoringResult ConfigureArtNetFromUI(
      std::string_view specification);
  [[nodiscard]] std::string OutputBackendStatus() const;
  [[nodiscard]] std::string OutputBackendError() const;

  [[nodiscard]] bool RefreshNetworkInterfacesFromUI();
  [[nodiscard]] bool CycleRxInterfaceFromUI(int direction);
  [[nodiscard]] bool CycleTxInterfaceFromUI(int direction);
  [[nodiscard]] std::string RxInterfaceStatus() const;
  [[nodiscard]] std::string TxInterfaceStatus() const;
  [[nodiscard]] std::optional<aeyla::network::NetworkInterface>
  SelectedTxInterface() const;
  [[nodiscard]] std::size_t NetworkInterfaceCount() const;
  [[nodiscard]] aeyla::product::AuthoringResult
  ApplyTxNetworkFromUI(std::string ipv4, std::string mask);
  [[nodiscard]] std::string NetworkConfigurationStatus() const;
  [[nodiscard]] bool NetworkConfigurationBusy() const;

  [[nodiscard]] aeyla::product::AuthoringResult ToggleTakeCaptureFromUI();
  [[nodiscard]] aeyla::product::AuthoringResult ToggleActiveTakePlaybackFromUI();
  void StopActiveTakePlaybackFromUI();
  [[nodiscard]] aeyla::product::AuthoringResult ToggleTakeOutputArmFromUI();
  [[nodiscard]] bool TakeRecording() const noexcept;
  [[nodiscard]] bool TakePlaying() const noexcept;
  [[nodiscard]] bool TakeOutputArmed() const noexcept;
  [[nodiscard]] bool TakeOutputLive() const noexcept
  {
    return TakeOutputArmed() && mArtNetOutput.override_enabled();
  }
  [[nodiscard]] std::string ActiveTakeStatus() const;
  [[nodiscard]] std::string CaptureInputStatus() const;
  [[nodiscard]] double ActiveTakePlaybackProgress() const;

  [[nodiscard]] aeyla::runtime::ShowMidiMapping ShowMidiMapping() const noexcept;
  [[nodiscard]] aeyla::product::AuthoringResult ToggleShowMidiFromUI();
  [[nodiscard]] aeyla::product::AuthoringResult CycleShowMidiChannelFromUI(
      int direction);
  [[nodiscard]] aeyla::product::AuthoringResult BeginShowMidiLearnFromUI(
      aeyla::runtime::ShowMidiLearnTarget target);
  [[nodiscard]] aeyla::runtime::ShowMidiLearnTarget ShowMidiLearnTarget() const noexcept;
  [[nodiscard]] std::string ShowMidiStatus() const;
  [[nodiscard]] bool ShowMidiConfigurationLocked() const noexcept
  {
    const auto output = mArtNetOutput.stats();
    return TakeOutputArmed() || OutputArmed() ||
           output.enabled || output.override_enabled;
  }
  [[nodiscard]] bool ShowMidiPreflightBusy() const noexcept
  {
    return mMidiPreflightCursor.load(std::memory_order_acquire) >= 0;
  }
  [[nodiscard]] int ActiveTakeSongIndex() const noexcept
  {
    return mActiveTakeSongIndex.load(std::memory_order_acquire);
  }

  // Non-destructive Take editor. Delta is expressed in seconds. The source
  // recording remains untouched; playback starts/ends at the edited frame
  // boundaries. Editing is blocked while REC, PLAY or physical Take output is
  // active so the scheduler cannot be reconfigured in flight.
  [[nodiscard]] aeyla::product::AuthoringResult AdjustActiveTakeInFromUI(
      double deltaSeconds);
  [[nodiscard]] aeyla::product::AuthoringResult AdjustActiveTakeOutFromUI(
      double deltaSeconds);
  [[nodiscard]] aeyla::product::AuthoringResult ResetActiveTakeTrimFromUI();
  [[nodiscard]] aeyla::product::AuthoringResult ConsolidateActiveTakeFromUI();
  [[nodiscard]] aeyla::product::AuthoringResult SetActiveTakeInFrameFromUI(
      std::uint64_t frameIndex);
  [[nodiscard]] aeyla::product::AuthoringResult SetActiveTakeOutFrameFromUI(
      std::uint64_t frameIndexExclusive);
  [[nodiscard]] aeyla::product::AuthoringResult SeekActiveTakeFrameFromUI(
      std::uint64_t frameIndex);
  [[nodiscard]] aeyla::product::AuthoringResult CycleActiveTakeVersionFromUI(
      int direction);
  [[nodiscard]] aeyla::product::AuthoringResult ReturnToRawTakeFromUI();
  [[nodiscard]] AeylaTakeEditorSnapshot ActiveTakeEditorSnapshot() const;

  [[nodiscard]] aeyla::output::ArtNetOutputStats ArtNetOutputStatus() const noexcept
  {
    return mArtNetOutput.stats();
  }
  [[nodiscard]] aeyla::capture::ArtNetCaptureStats ArtNetCaptureStatus() const noexcept
  {
    return mArtNetCapture.stats();
  }

  // R10 EN VIVO. The per-instance session binds lazily so simply opening or
  // closing the editor never owns the socket and never changes Art-Net ARM.
  [[nodiscard]] std::size_t LiveMemoryCount() const noexcept
  {
    return aeyla::live_memory_session::kOperatorMemoryCount;
  }

  [[nodiscard]] aeyla::live_memory_session::MemoryView LiveMemoryViewFromUI(
      std::size_t index)
  {
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    return aeyla::live_memory_session::view(this, index);
  }

  [[nodiscard]] aeyla::product::AuthoringResult LearnLiveMemoryFromAvolitesFromUI(
      std::size_t index)
  {
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    const auto result = aeyla::live_memory_session::learn_from_avolites(this, index);
    if(result.succeeded) CommitLiveMemoryPersistenceIfDirtyFromUI();
    return {result.succeeded, {}, result.message};
  }

  [[nodiscard]] aeyla::product::AuthoringResult CancelLiveMemoryLearnFromUI(
      std::size_t index)
  {
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    const auto result = aeyla::live_memory_session::cancel_learn(this, index);
    return {result.succeeded, {}, result.message};
  }

  [[nodiscard]] aeyla::product::AuthoringResult ToggleLiveMemoryFromUI(
      std::size_t index)
  {
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    const auto result = aeyla::live_memory_session::toggle(this, index);
    return {result.succeeded, {}, result.message};
  }

  [[nodiscard]] aeyla::product::AuthoringResult SetLiveMemoryLevelFromUI(
      std::size_t index, float level)
  {
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    const auto result = aeyla::live_memory_session::set_fader_level(
        this, index, level);
    return {result.succeeded, {}, result.message};
  }

  [[nodiscard]] aeyla::product::AuthoringResult CycleLiveMemoryFadeFromUI(
      std::size_t index, int direction)
  {
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    const auto result = aeyla::live_memory_session::cycle_fade(
        this, index, direction);
    if(result.succeeded) CommitLiveMemoryPersistenceIfDirtyFromUI();
    return {result.succeeded, {}, result.message};
  }

  [[nodiscard]] aeyla::product::AuthoringResult ToggleLiveMemoryModeFromUI(
      std::size_t index)
  {
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    const auto result = aeyla::live_memory_session::toggle_mode(this, index);
    if(result.succeeded) CommitLiveMemoryPersistenceIfDirtyFromUI();
    return {result.succeeded, {}, result.message};
  }

  void ResetLiveMemoriesFromUI() noexcept
  {
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    aeyla::live_memory_session::reset_levels(this);
  }

  aeyla::product::ProjectFileStatus NewProjectFromUI()
  {
    if(TakeRecording())
      return {aeyla::product::ProjectFileOperation::new_project,
              false,
              mProjectFiles.current_path(),
              "Detén y guarda la toma antes de crear otro proyecto",
              {"Cambiar de proyecto durante GRABAR podría separar el archivo de su canción."}};
    StopActiveTakePlaybackFromUI();
    mTakeScheduler.disarm();
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    aeyla::live_memory_session::reset_levels(this);

    aeyla::product::ProjectFileStatus status;
    aeyla::project::LiveMemoryPersistentState liveState;
    {
      const std::scoped_lock lock(mModelMutex);
      status = mProjectFiles.new_project(
          aeyla::product::generate_project_uuid(),
          aeyla::product::current_utc_timestamp());
      if(status.succeeded)
      {
        liveState = mProjectFiles.live_memory_state();
        mLoadedTakeSongIndex.store(-1, std::memory_order_release);
        mActiveTakeSongIndex.store(-1, std::memory_order_release);
        SyncParametersFromProject();
        RefreshOutputBackendFromProjectLocked();
        if(ShowMidiMapping().enabled)
          mMidiPreflightCursor.store(0, std::memory_order_release);
      }
      SyncSnapshotToAtomicsLocked();
    }

    if(status.succeeded)
    {
      const auto restored = aeyla::live_memory_session::restore_persistent_state(
          this, liveState);
      if(!restored.succeeded)
      {
        status.succeeded = false;
        status.message = "Proyecto nuevo creado, pero las memorias EN VIVO no pudieron inicializarse";
        status.diagnostics.push_back(restored.message);
      }
    }
    return status;
  }

  aeyla::product::ProjectFileStatus OpenProjectFromUI(
      const std::filesystem::path& path)
  {
    if(TakeRecording())
      return {aeyla::product::ProjectFileOperation::open,
              false,
              mProjectFiles.current_path(),
              "Detén y guarda la toma antes de abrir otro proyecto",
              {"La grabación activa conserva la identidad de la canción actual."}};
    StopActiveTakePlaybackFromUI();
    mTakeScheduler.disarm();
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    // Failed Open must not erase the current memory definitions. We only force
    // their runtime levels OFF before validating the candidate package.
    aeyla::live_memory_session::reset_levels(this);

    aeyla::product::ProjectFileStatus status;
    aeyla::project::LiveMemoryPersistentState liveState;
    {
      const std::scoped_lock lock(mModelMutex);
      status = mProjectFiles.open(path);
      if(status.succeeded)
      {
        liveState = mProjectFiles.live_memory_state();
        mLoadedTakeSongIndex.store(-1, std::memory_order_release);
        mActiveTakeSongIndex.store(-1, std::memory_order_release);
        SyncParametersFromProject();
        RefreshOutputBackendFromProjectLocked();
        if(ShowMidiMapping().enabled)
          mMidiPreflightCursor.store(0, std::memory_order_release);
      }
      SyncSnapshotToAtomicsLocked();
    }

    if(status.succeeded)
    {
      const auto restored = aeyla::live_memory_session::restore_persistent_state(
          this, liveState);
      if(!restored.succeeded)
      {
        status.succeeded = false;
        status.message = "Proyecto abierto en modo seguro, pero live.bin no pudo publicarse";
        status.diagnostics.push_back(restored.message);
      }
    }
    return status;
  }

  aeyla::product::ProjectFileStatus SaveProjectFromUI()
  {
    CaptureAllParameterValuesFromHost();
    const std::scoped_lock lock(mModelMutex);
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    mProjectFiles.set_live_memory_state(
        aeyla::live_memory_session::persistent_state(this));
    PrepareProjectForSave();
    const auto status = mProjectFiles.save(
        aeyla::product::current_utc_timestamp());
    if(status.succeeded)
      (void)aeyla::live_memory_session::consume_persistence_dirty(this);
    SyncSnapshotToAtomicsLocked();
    return status;
  }

  aeyla::product::ProjectFileStatus SaveProjectAsFromUI(
      const std::filesystem::path& path)
  {
    CaptureAllParameterValuesFromHost();
    const std::scoped_lock lock(mModelMutex);
    aeyla::live_memory_session::register_runtime(
        this, &mArtNetOutput, &mArtNetCapture);
    mProjectFiles.set_live_memory_state(
        aeyla::live_memory_session::persistent_state(this));
    PrepareProjectForSave();
    const auto status = mProjectFiles.save_as(
        path, aeyla::product::current_utc_timestamp());
    if(status.succeeded)
      (void)aeyla::live_memory_session::consume_persistence_dirty(this);
    SyncSnapshotToAtomicsLocked();
    return status;
  }

  [[nodiscard]] const aeyla::product::ProjectFileStatus& ProjectFileStatus() const noexcept
  {
    return mProjectFiles.status();
  }

  [[nodiscard]] const std::filesystem::path& CurrentProjectPath() const noexcept
  {
    return mProjectFiles.current_path();
  }

  [[nodiscard]] bool ProjectDirty() const noexcept
  {
    const std::scoped_lock lock(mModelMutex);
    return mModel.snapshot().project_dirty;
  }

  [[nodiscard]] std::string ProjectName() const
  {
    const std::scoped_lock lock(mModelMutex);
    return mModel.snapshot().project_name;
  }

  [[nodiscard]] bool OutputArmed() const noexcept
  {
    return mOutputArmed.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool EffectiveBlackout() const noexcept
  {
    return mEffectiveBlackout.load(std::memory_order_acquire);
  }

  // The red header control owns the global operator/safety latch. It must not
  // mirror the Show renderer's effective black state because an unresolved
  // Cue is allowed to be black while an independent recorded Take is armed.
  [[nodiscard]] bool GlobalBlackout() const noexcept
  {
    return mGlobalBlackout.load(std::memory_order_acquire);
  }

  // Canonical product workspace shared by the UI layers. This is presentation
  // state only: changing workspace never touches ARM, blackout or transport.
  void SetUiWorkspaceFromUI(int workspace) noexcept
  {
    mUiWorkspace.store(std::clamp(workspace, 0, 3),
                       std::memory_order_release);
  }

  [[nodiscard]] int UiWorkspace() const noexcept
  {
    return mUiWorkspace.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool BackendReady() const noexcept
  {
    return mBackendReady.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool ProjectValid() const noexcept
  {
    return mProjectValid.load(std::memory_order_acquire);
  }

  [[nodiscard]] int LastMidiNote() const noexcept
  {
    return mLastMidiNote.load(std::memory_order_relaxed);
  }

  [[nodiscard]] int ActiveExecutor() const noexcept
  {
    return mActiveExecutor.load(std::memory_order_relaxed);
  }

  [[nodiscard]] std::uint64_t MidiEventCount() const noexcept
  {
    return mMidiEventCount.load(std::memory_order_relaxed);
  }

  [[nodiscard]] std::uint64_t DroppedMidiEvents() const noexcept
  {
    return mHostIngress.dropped_events();
  }

  [[nodiscard]] std::uint64_t HostStateRestoreErrors() const noexcept
  {
    return mHostStateRestoreErrors.load(std::memory_order_relaxed);
  }

  [[nodiscard]] std::uint64_t DmxGeneration() const noexcept
  {
    return mDmxGeneration.load(std::memory_order_relaxed);
  }

  [[nodiscard]] int DmxNonZeroChannels() const noexcept
  {
    return mDmxNonZeroChannels.load(std::memory_order_relaxed);
  }

  [[nodiscard]] float VisualPhase() const noexcept
  {
    return mVisualPhase.load(std::memory_order_relaxed);
  }

  [[nodiscard]] bool ActiveSongBound() const noexcept
  {
    return mActiveSongBound.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool RenderingOffline() const noexcept
  {
    return mRenderingOffline.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool RuntimeHealthy() const noexcept
  {
    return !mRuntimeExited.load(std::memory_order_acquire) &&
           !mRuntimeFaulted.load(std::memory_order_acquire);
  }

private:
  void StartRuntimeWorker();
  void StopRuntimeWorker() noexcept;
  void RuntimeLoop() noexcept;
  void RuntimeTick() noexcept;
  void ReconcileNetworkConfiguration() noexcept;
  void ApplyPendingHostStateLocked();
  void ApplyPendingParameterStateLocked();
  void DrainHostEventsLocked();
  void DrainShowMidiCommandsLocked(
      const aeyla::runtime::HostTransportSnapshot& host);
  void ClearShowMidiCommandsLocked() noexcept;
  [[nodiscard]] bool StartPreparedTakeFromMidiLocked(
      std::size_t song_index, std::uint64_t trigger_sample,
      std::string& error_message);
  [[nodiscard]] bool PreloadPreparedTakeForMidiLocked(
      std::size_t song_index, std::string& error_message);
  void SetShowMidiMessage(std::string message);
  void SyncShowMidiMappingToState(
      const aeyla::runtime::ShowMidiMapping& mapping);
  [[nodiscard]] std::uint64_t BeginShowTransportMutation() noexcept;
  void EndShowTransportMutation() noexcept;
  void SynchronizeShowTransportCursor(std::uint64_t trigger_sample,
                                      std::uint64_t base_cursor = 0U) noexcept;
  void RefreshHostStateCacheLocked();
  void SyncSnapshotToAtomicsLocked() noexcept;
  void CaptureParameterValueFromHost(int paramIdx) noexcept;
  void CaptureAllParameterValuesFromHost() noexcept;
  void SetOutputArmed(bool armed);
  void RefreshOutputBackendFromProjectLocked();
  void PublishOutputFrameLocked(bool renderingOffline);
  void RestartCaptureInputFromRouting();
  [[nodiscard]] std::string SelectedRxIpv4() const;
  [[nodiscard]] std::string SelectedTxIpv4() const;
  [[nodiscard]] std::string ActiveSongIdLocked() const;
  [[nodiscard]] bool ApplyCapturedTakeAutoIn(
      const aeyla::capture::TakeFileIndexEntry& entry,
      std::uint64_t anchorFrame,
      std::string& error);

  void CommitLiveMemoryPersistenceDirtyLocked()
  {
    if(!aeyla::live_memory_session::consume_persistence_dirty(this))
      return;
    mProjectFiles.set_live_memory_state(
        aeyla::live_memory_session::persistent_state(this));
    mModel.mark_project_unsaved();
  }

  void CommitLiveMemoryPersistenceIfDirtyFromUI()
  {
    const std::scoped_lock lock(mModelMutex);
    CommitLiveMemoryPersistenceDirtyLocked();
    SyncSnapshotToAtomicsLocked();
  }

  void PrepareProjectForSave()
  {
    mModel.set_blackout(mParamBlackout.load(std::memory_order_acquire));
    mModel.set_grand_master(mParamGrandMaster.load(std::memory_order_acquire));
    mModel.set_rig14(mParamRig14.load(std::memory_order_acquire));
    const int source = std::clamp(mParamSource.load(std::memory_order_acquire), 0, 4);
    mModel.set_visual_source(static_cast<aeyla::product::VisualSource>(source));
    mModel.set_visual_speed(mParamSpeed.load(std::memory_order_acquire));
    mModel.set_white_extraction(mParamWhiteExtract.load(std::memory_order_acquire));
    mModel.set_amber_extraction(mParamAmberExtract.load(std::memory_order_acquire));
    mModel.set_uv_manual(mParamUv.load(std::memory_order_acquire));
  }

  void SyncParametersFromProject()
  {
    const auto& document = mModel.project_document();
    int sourceIndex = 1;
    const auto activeLook = std::find_if(
        document.looks.begin(), document.looks.end(),
        [&](const aeyla::project::LookDocument& look) {
          return look.look_id == document.visual.active_look_id;
        });
    if(activeLook != document.looks.end())
    {
      const std::string_view source = activeLook->source;
      if(source == "solid") sourceIndex = 0;
      else if(source == "gradient") sourceIndex = 1;
      else if(source == "wave") sourceIndex = 2;
      else if(source == "noise") sourceIndex = 3;
      else if(source == "chase") sourceIndex = 4;
    }

    const bool rig14 = document.fixtures.size() == 14U &&
        std::all_of(document.fixtures.begin(), document.fixtures.end(),
                    [](const aeyla::project::FixtureDocument& fixture) {
                      return fixture.enabled;
                    });

    GetParam(kParamBlackout)->Set(1.0);
    GetParam(kParamRigMode)->Set(rig14 ? 1.0 : 0.0);
    GetParam(kParamSource)->Set(static_cast<double>(sourceIndex));
    GetParam(kParamSpeed)->Set(document.visual.speed * 100.0);
    GetParam(kParamWhiteExtract)->Set(document.visual.white_extraction * 100.0);
    GetParam(kParamAmberExtract)->Set(document.visual.amber_extraction * 100.0);
    GetParam(kParamUV)->Set(document.visual.uv_manual * 100.0);
    mParamBlackout.store(true, std::memory_order_release);
    mParamRig14.store(rig14, std::memory_order_release);
    mParamSource.store(sourceIndex, std::memory_order_release);
    mLastAppliedSource = sourceIndex;
    mParamSpeed.store(document.visual.speed, std::memory_order_release);
    mParamWhiteExtract.store(document.visual.white_extraction,
                             std::memory_order_release);
    mParamAmberExtract.store(document.visual.amber_extraction,
                             std::memory_order_release);
    mParamUv.store(document.visual.uv_manual, std::memory_order_release);
    mParameterUpdatePending.store(true, std::memory_order_release);
  }

  aeyla::runtime::HostEventIngress<1024> mHostIngress{};
  aeyla::runtime::ShowMidiIngress<256> mShowMidiIngress{};
  aeyla::runtime::HostTransportMailbox mHostTransport{};
  mutable std::mutex mModelMutex;
  aeyla::product::ApplicationModel mModel{};
  aeyla::product::ProjectFileController mProjectFiles{mModel};
  aeyla::output::ArtNetOutputWorker mArtNetOutput{};
  aeyla::capture::ArtNetCaptureWorker mArtNetCapture{};
  std::string mOutputBackendError;
  std::uint64_t mLastArtNetSendErrors{0U};
  std::uint64_t mLastHandledArtNetFailClosedEvents{0U};

  mutable std::mutex mNetworkMutex;
  std::vector<aeyla::network::NetworkInterface> mNetworkInterfaces;
  std::size_t mRxInterfaceIndex{0U};
  std::size_t mTxInterfaceIndex{0U};
  std::string mCaptureInputError;
  AeylaNetworkConfiguration mNetworkConfiguration{};
  std::atomic<std::uint64_t> mLastNetworkConfigurationRevision{0U};
  std::string mPendingTxAdapterId;
  std::string mNetworkConfigurationMessage;

  aeyla::capture::DmxTakeScheduler mTakeScheduler{};
  aeyla::capture::DmxCaptureSyncAnchor mCaptureSyncAnchor{};
  // UI-owned identity of the streamed file currently being recorded. The
  // stop path indexes this exact target instead of guessing from timestamps.
  std::filesystem::path mActiveCaptureTarget;

  // Realtime capture synchronization bridge. ProcessBlock snapshots the 44 Hz
  // recorder cursor exactly on the host STOP->PLAY boundary, then publishes a
  // monotonically increasing marker revision. RuntimeTick commits the frame to
  // the non-realtime sync state machine. No mutex/file/network work is added to
  // the audio callback.
  std::atomic<bool> mAudioTransportRunning{false};
  std::atomic<std::uint64_t> mPendingCaptureTransportFrame{0U};
  std::atomic<std::uint64_t> mCaptureTransportMarkerRevision{0U};
  std::uint64_t mLastCaptureTransportMarkerRevision{0U};

  std::thread mRuntimeThread;
  std::atomic<bool> mRuntimeStopRequested{false};
  std::atomic<bool> mRuntimeExited{true};
  std::atomic<bool> mRuntimeFaulted{false};

  mutable std::mutex mHostStateMutex;
  aeyla::runtime::PluginComponentState mHostStateCache{};
  std::optional<aeyla::runtime::PluginComponentState> mPendingHostState;
  std::optional<aeyla::runtime::ShowMidiEvent> mPendingShowMidiEvent;
  std::array<std::unique_ptr<aeyla::capture::DmxTakeFileReader>,
             aeyla::runtime::kShowMidiSongCapacity> mPreparedMidiTakeReaders{};
  std::array<std::filesystem::path,
             aeyla::runtime::kShowMidiSongCapacity> mPreparedMidiTakePaths{};
  std::array<std::uint64_t,
             aeyla::runtime::kShowMidiSongCapacity> mPreparedMidiTakeStarts{};
  std::array<std::uint64_t,
             aeyla::runtime::kShowMidiSongCapacity> mPreparedMidiTakeEnds{};

  mutable std::mutex mShowMidiMutex;
  std::string mShowMidiMessage{"MIDI SHOW DESACTIVADO · configura y habilita cuando el show esté listo"};

  std::atomic<bool> mParameterUpdatePending{true};
  std::atomic<bool> mLookParameterUiSyncPending{false};
  std::atomic<bool> mHostDeactivationPending{false};
  // OnReset may execute on a host processing thread. It only raises this
  // lock-free request; RuntimeTick performs the destructive scheduler/model
  // reset outside the audio callback. This prevents a take loaded at an old
  // sample rate from surviving an Ableton audio-engine reset.
  std::atomic<bool> mHostResetPending{false};
  std::atomic<bool> mHostStateRestoreRejected{false};
  std::atomic<bool> mOutputArmed{false};
  std::atomic<bool> mGlobalBlackout{true};
  std::atomic<bool> mEffectiveBlackout{true};
  std::atomic<int> mUiWorkspace{0};
  std::atomic<bool> mBackendReady{false};
  std::atomic<bool> mProjectValid{false};
  std::atomic<int> mLastMidiNote{-1};
  std::atomic<int> mActiveExecutor{-1};
  std::atomic<std::uint64_t> mMidiEventCount{0};
  std::atomic<std::uint64_t> mProcessedAudioSamples{0U};
  std::atomic<std::uint64_t> mProcessedTransportSamples{0U};
  std::atomic<std::uint64_t> mAudioAdvanceSequence{0U};
  std::atomic<bool> mShowTransportMutation{false};
  std::atomic<std::uint64_t> mShowMidiMappingPacked{0U};
  std::atomic<std::uint8_t> mShowMidiCaptureStartNote{
      aeyla::runtime::kShowMidiCaptureStartNote};
  std::atomic<std::uint8_t> mShowMidiCaptureStopNote{
      aeyla::runtime::kShowMidiCaptureStopNote};
  std::atomic<std::uint32_t> mPendingMidiLearnPacked{0U};
  std::atomic<aeyla::runtime::ShowMidiLearnTarget> mShowMidiLearnTarget{
      aeyla::runtime::ShowMidiLearnTarget::none};
  std::atomic<int> mLoadedTakeSongIndex{-1};
  std::atomic<int> mActiveTakeSongIndex{-1};
  std::atomic<int> mMidiPreloadSongRequest{-1};
  std::atomic<int> mMidiPreflightCursor{-1};
  std::atomic<std::uint64_t> mHostStateRestoreErrors{0};
  std::atomic<std::uint64_t> mDmxGeneration{0};
  std::atomic<int> mDmxNonZeroChannels{0};
  std::atomic<float> mVisualPhase{0.0F};
  std::atomic<bool> mParamBlackout{true};
  std::atomic<float> mParamGrandMaster{1.0F};
  std::atomic<bool> mParamRig14{false};
  std::atomic<int> mParamSource{1};
  std::atomic<float> mParamSpeed{0.35F};
  std::atomic<float> mParamWhiteExtract{0.20F};
  std::atomic<float> mParamAmberExtract{0.15F};
  std::atomic<float> mParamUv{0.0F};
  std::atomic<bool> mRenderingOffline{false};
  std::atomic<bool> mActiveSongBound{false};
  int mLastAppliedSource{1};
  bool mLastHostRunning{false};
  std::string mLastProjectedSongId;
  std::uint64_t mLastProjectedTick{0U};
};
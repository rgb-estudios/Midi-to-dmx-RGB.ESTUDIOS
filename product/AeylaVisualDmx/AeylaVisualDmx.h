#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include "capture/artnet_capture_worker.h"
#include "capture/dmx_take_activity.h"
#include "capture/dmx_take_scheduler.h"
#include "network/network_interfaces.h"
#include "product/application_model.h"
#include "product/project_file_controller.h"
#include "product/project_identity.h"
#include "output/artnet_output_worker.h"
#include "runtime/host_event_ingress.h"
#include "runtime/host_transport_mailbox.h"
#include "runtime/plugin_state.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
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
  kCtrlTagExecutorRuntime,
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
  // Operator-facing BLACKOUT path. Enabling blackout performs one deterministic
  // disarm boundary for both semantic and Take authority, then latches blackout.
  // Disabling blackout never auto-arms output.
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
  [[nodiscard]] std::size_t NetworkInterfaceCount() const;

  [[nodiscard]] aeyla::product::AuthoringResult ToggleTakeCaptureFromUI();
  [[nodiscard]] aeyla::product::AuthoringResult ToggleActiveTakePlaybackFromUI();
  void StopActiveTakePlaybackFromUI();
  [[nodiscard]] aeyla::product::AuthoringResult ToggleTakeOutputArmFromUI();
  [[nodiscard]] bool TakeRecording() const noexcept;
  [[nodiscard]] bool TakePlaying() const noexcept;
  [[nodiscard]] bool TakeOutputArmed() const noexcept;
  [[nodiscard]] bool HasActiveTake() const;
  [[nodiscard]] std::string ActiveTakeStatus() const;
  [[nodiscard]] std::string CaptureInputStatus() const;
  [[nodiscard]] double ActiveTakePlaybackProgress() const;

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
  [[nodiscard]] double ActiveTakeInSeconds() const;
  [[nodiscard]] double ActiveTakeOutSeconds() const;
  [[nodiscard]] double ActiveTakeOriginalDurationSeconds() const;
  [[nodiscard]] double ActiveTakeEffectiveDurationSeconds() const;

  [[nodiscard]] std::uint64_t CaptureAcceptedPackets() const noexcept;
  [[nodiscard]] std::uint64_t CaptureSequenceGaps() const noexcept;

  [[nodiscard]] std::uint64_t ArtNetSentPackets() const noexcept
  {
    return mArtNetOutput.stats().sent_packets;
  }
  [[nodiscard]] std::uint64_t ArtNetSendErrors() const noexcept
  {
    return mArtNetOutput.stats().send_errors;
  }

  aeyla::product::ProjectFileStatus NewProjectFromUI()
  {
    StopActiveTakePlaybackFromUI();
    mTakeScheduler.disarm();
    const std::scoped_lock lock(mModelMutex);
    const auto status = mProjectFiles.new_project(
        aeyla::product::generate_project_uuid(),
        aeyla::product::current_utc_timestamp());
    if(status.succeeded)
    {
      SyncParametersFromProject();
      RefreshOutputBackendFromProjectLocked();
    }
    SyncSnapshotToAtomicsLocked();
    return status;
  }

  aeyla::product::ProjectFileStatus OpenProjectFromUI(
      const std::filesystem::path& path)
  {
    StopActiveTakePlaybackFromUI();
    mTakeScheduler.disarm();
    const std::scoped_lock lock(mModelMutex);
    const auto status = mProjectFiles.open(path);
    if(status.succeeded)
    {
      SyncParametersFromProject();
      RefreshOutputBackendFromProjectLocked();
    }
    SyncSnapshotToAtomicsLocked();
    return status;
  }

  aeyla::product::ProjectFileStatus SaveProjectFromUI()
  {
    CaptureAllParameterValuesFromHost();
    const std::scoped_lock lock(mModelMutex);
    PrepareProjectForSave();
    const auto status = mProjectFiles.save(
        aeyla::product::current_utc_timestamp());
    SyncSnapshotToAtomicsLocked();
    return status;
  }

  aeyla::product::ProjectFileStatus SaveProjectAsFromUI(
      const std::filesystem::path& path)
  {
    CaptureAllParameterValuesFromHost();
    const std::scoped_lock lock(mModelMutex);
    PrepareProjectForSave();
    const auto status = mProjectFiles.save_as(
        path, aeyla::product::current_utc_timestamp());
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
  void ApplyPendingHostStateLocked();
  void ApplyPendingParameterStateLocked();
  void DrainHostEventsLocked();
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
  aeyla::runtime::HostTransportMailbox mHostTransport{};
  mutable std::mutex mModelMutex;
  aeyla::product::ApplicationModel mModel{};
  aeyla::product::ProjectFileController mProjectFiles{mModel};
  aeyla::output::ArtNetOutputWorker mArtNetOutput{};
  aeyla::capture::ArtNetCaptureWorker mArtNetCapture{};
  std::string mOutputBackendError;
  std::uint64_t mLastArtNetSendErrors{0U};

  mutable std::mutex mNetworkMutex;
  std::vector<aeyla::network::NetworkInterface> mNetworkInterfaces;
  std::size_t mRxInterfaceIndex{0U};
  std::size_t mTxInterfaceIndex{0U};
  std::string mCaptureInputError;

  aeyla::capture::DmxTakeScheduler mTakeScheduler{};

  std::thread mRuntimeThread;
  std::atomic<bool> mRuntimeStopRequested{false};
  std::atomic<bool> mRuntimeExited{true};
  std::atomic<bool> mRuntimeFaulted{false};

  mutable std::mutex mHostStateMutex;
  aeyla::runtime::PluginComponentState mHostStateCache{};
  std::optional<aeyla::runtime::PluginComponentState> mPendingHostState;

  std::atomic<bool> mParameterUpdatePending{true};
  std::atomic<bool> mLookParameterUiSyncPending{false};
  std::atomic<bool> mHostDeactivationPending{false};
  std::atomic<bool> mHostStateRestoreRejected{false};
  std::atomic<bool> mOutputArmed{false};
  std::atomic<bool> mEffectiveBlackout{true};
  std::atomic<bool> mBackendReady{false};
  std::atomic<bool> mProjectValid{false};
  std::atomic<int> mLastMidiNote{-1};
  std::atomic<int> mActiveExecutor{-1};
  std::atomic<std::uint64_t> mMidiEventCount{0};
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

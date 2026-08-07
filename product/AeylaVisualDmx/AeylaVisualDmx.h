#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include "product/application_model.h"
#include "runtime/host_event_ingress.h"

#include <atomic>
#include <cstdint>

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

class AeylaVisualDmx final : public Plugin
{
public:
  explicit AeylaVisualDmx(const InstanceInfo& info);

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnActivate(bool active) override;
  void OnParamChange(int paramIdx) override;
#endif

  void OnIdle() override;

  void ToggleOutputArmFromUI();
  void ForceDisarmFromUI();
  void TriggerExecutorFromUI(int executorIndex, float velocity);
  void ReleaseExecutorFromUI(int executorIndex);

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

  [[nodiscard]] std::uint64_t DmxGeneration() const noexcept
  {
    return mDmxGeneration.load(std::memory_order_relaxed);
  }

  [[nodiscard]] int DmxNonZeroChannels() const noexcept
  {
    return mDmxNonZeroChannels.load(std::memory_order_relaxed);
  }

private:
  void ApplyPendingParameterState();
  void DrainHostEvents();
  void SyncSnapshotToAtomics() noexcept;
  void SetOutputArmed(bool armed);

  aeyla::runtime::HostEventIngress<1024> mHostIngress{};
  aeyla::product::ApplicationModel mModel{};

  std::atomic<bool> mParameterUpdatePending{true};
  std::atomic<bool> mHostDeactivationPending{false};
  std::atomic<bool> mOutputArmed{false};
  std::atomic<bool> mEffectiveBlackout{true};
  std::atomic<bool> mBackendReady{false};
  std::atomic<bool> mProjectValid{false};
  std::atomic<int> mLastMidiNote{-1};
  std::atomic<int> mActiveExecutor{-1};
  std::atomic<std::uint64_t> mMidiEventCount{0};
  std::atomic<std::uint64_t> mDmxGeneration{0};
  std::atomic<int> mDmxNonZeroChannels{0};
};

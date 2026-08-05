#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include "runtime/runtime_safety_state.h"

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
#endif

  void OnIdle() override;

  void ToggleOutputArmFromUI() noexcept;
  void ForceDisarmFromUI() noexcept;

  [[nodiscard]] bool OutputArmed() const noexcept
  {
    return mOutputArmed.load(std::memory_order_acquire);
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

private:
  void SetOutputArmed(bool armed) noexcept;

  std::atomic<bool> mOutputArmed{false};
  std::atomic<int> mLastMidiNote{-1};
  std::atomic<int> mActiveExecutor{-1};
  std::atomic<std::uint64_t> mMidiEventCount{0};
  aeyla::runtime::RuntimeSafetyState mSafety{};
};

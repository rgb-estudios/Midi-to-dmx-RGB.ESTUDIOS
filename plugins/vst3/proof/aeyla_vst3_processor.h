#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"

#include "runtime/host_event_ingress.h"
#include "runtime/plugin_state.h"
#include "runtime/runtime_safety_state.h"

namespace Steinberg::AeylaVst3 {

class Processor final : public Vst::AudioEffect {
 public:
  Processor();
  ~Processor() SMTG_OVERRIDE = default;

  static FUnknown* createInstance(void*) {
    return static_cast<Vst::IAudioProcessor*>(new Processor());
  }

  tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE;
  tresult PLUGIN_API terminate() SMTG_OVERRIDE;
  tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE;
  tresult PLUGIN_API setupProcessing(Vst::ProcessSetup& setup) SMTG_OVERRIDE;
  tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) SMTG_OVERRIDE;
  tresult PLUGIN_API process(Vst::ProcessData& data) SMTG_OVERRIDE;
  tresult PLUGIN_API setState(IBStream* state) SMTG_OVERRIDE;
  tresult PLUGIN_API getState(IBStream* state) SMTG_OVERRIDE;

 private:
  void processParameterChanges(Vst::ProcessData& data) noexcept;
  void processEvents(Vst::ProcessData& data) noexcept;
  void clearOutputs(Vst::ProcessData& data) noexcept;
  void publishParameter(Vst::ProcessData& data, Vst::ParamID id,
                        Vst::ParamValue value, int32 sampleOffset = 0) noexcept;

  aeyla::runtime::HostEventIngress<1024> eventIngress_{};
  aeyla::runtime::RuntimeSafetyState safety_{};
  aeyla::runtime::PluginComponentState componentState_{};

  bool armParameterHigh_{false};
  bool armSeenLowSinceActivation_{false};
  Vst::ParamValue midiActivity_{0.0};
  std::uint64_t lastPublishedDroppedEvents_{0};
};

}  // namespace Steinberg::AeylaVst3

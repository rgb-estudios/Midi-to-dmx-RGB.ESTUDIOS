#include "aeyla_vst3_controller.h"

#include "aeyla_vst3_cids.h"
#include "runtime/plugin_state.h"

#include "pluginterfaces/base/ibstream.h"

#include <vector>

namespace Steinberg::AeylaVst3 {
namespace {

bool readStateBytes(IBStream* stream, std::vector<std::uint8_t>& bytes) {
  if (!stream) return false;
  bytes.assign(aeyla::runtime::kMaxPluginStateBytes, 0U);
  int32 bytesRead = 0;
  const auto result = stream->read(bytes.data(), static_cast<int32>(bytes.size()), &bytesRead);
  if ((result != kResultOk && result != kResultTrue) || bytesRead <= 0) {
    bytes.clear();
    return false;
  }
  bytes.resize(static_cast<std::size_t>(bytesRead));
  return true;
}

}  // namespace

tresult PLUGIN_API Controller::initialize(FUnknown* context) {
  const auto result = EditControllerEx1::initialize(context);
  if (result != kResultOk) return result;

  parameters.addParameter(STR16("Output Arm Request"), nullptr, 1, 0.0,
                          Vst::ParameterInfo::kNoFlags, kOutputArmRequestId);
  parameters.addParameter(STR16("Blackout"), nullptr, 1, 1.0,
                          Vst::ParameterInfo::kCanAutomate, kBlackoutId);
  parameters.addParameter(STR16("Grand Master"), STR16("%"), 0, 1.0,
                          Vst::ParameterInfo::kCanAutomate, kGrandMasterId);
  parameters.addParameter(STR16("MIDI Activity"), nullptr, 0, 0.0,
                          Vst::ParameterInfo::kIsReadOnly, kMidiActivityId);
  parameters.addParameter(STR16("Dropped Events"), nullptr, 0, 0.0,
                          Vst::ParameterInfo::kIsReadOnly, kDroppedEventsId);
  return kResultOk;
}

tresult PLUGIN_API Controller::terminate() {
  return EditControllerEx1::terminate();
}

tresult PLUGIN_API Controller::setComponentState(IBStream* state) {
  std::vector<std::uint8_t> bytes;
  if (!readStateBytes(state, bytes)) return kResultFalse;

  const auto decoded = aeyla::runtime::decode_plugin_component_state(bytes);
  if (!decoded.ok()) return kResultFalse;

  // Never restore an armed state. The operator must create a new low->high
  // request after every load/activation, and the backend must also be ready.
  setParamNormalized(kOutputArmRequestId, 0.0);
  setParamNormalized(kBlackoutId, decoded.state.blackout ? 1.0 : 0.0);
  setParamNormalized(kGrandMasterId, decoded.state.grand_master);
  setParamNormalized(kMidiActivityId, 0.0);
  setParamNormalized(kDroppedEventsId, 0.0);
  return kResultOk;
}

tresult PLUGIN_API Controller::setState(IBStream*) {
  return kResultOk;
}

tresult PLUGIN_API Controller::getState(IBStream*) {
  return kResultOk;
}

IPlugView* PLUGIN_API Controller::createView(FIDString) {
  // The first proof deliberately uses the host's generic parameter UI. This
  // validates scan/load/MIDI/state before a custom visual editor is introduced.
  return nullptr;
}

}  // namespace Steinberg::AeylaVst3

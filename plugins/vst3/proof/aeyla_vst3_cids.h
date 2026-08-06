#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace Steinberg::AeylaVst3 {

enum ParameterIds : Vst::ParamID {
  kOutputArmRequestId = 1000,
  kBlackoutId = 1001,
  kGrandMasterId = 1002,
  kMidiActivityId = 1003,
  kDroppedEventsId = 1004
};

// Permanent identifiers. Never change after a public build is distributed.
static const FUID kProcessorUid(0xA3E14D4A, 0x1B234FA1, 0x9A10E9C4, 0x708A11D2);
static const FUID kControllerUid(0x86D0F2B7, 0x5FCB49DB, 0xB46163CE, 0xE1D3A947);

inline constexpr auto kPluginName = "AEYLA Visual DMX Alpha";
inline constexpr auto kPluginCategory = "Instrument|Synth";

}  // namespace Steinberg::AeylaVst3

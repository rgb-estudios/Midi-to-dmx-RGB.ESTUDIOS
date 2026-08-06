#include "aeyla_vst3_cids.h"
#include "aeyla_vst3_controller.h"
#include "aeyla_vst3_processor.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF("RGB Estudios",
                  "https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS",
                  "mailto:contacto@rgbestudios.cl")

DEF_CLASS2(INLINE_UID_FROM_FUID(Steinberg::AeylaVst3::kProcessorUid),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           Steinberg::AeylaVst3::kPluginName,
           Vst::kDistributable,
           Steinberg::AeylaVst3::kPluginCategory,
           FULL_VERSION_STR,
           kVstVersionString,
           Steinberg::AeylaVst3::Processor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(Steinberg::AeylaVst3::kControllerUid),
           PClassInfo::kManyInstances,
           kVstComponentControllerClass,
           "AEYLA Visual DMX Alpha Controller",
           0,
           "",
           FULL_VERSION_STR,
           kVstVersionString,
           Steinberg::AeylaVst3::Controller::createInstance)

END_FACTORY

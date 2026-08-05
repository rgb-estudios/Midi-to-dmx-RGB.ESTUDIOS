#pragma once

#include "pluginterfaces/base/fplatform.h"

#define MAJOR_VERSION_INT 0
#define MAJOR_VERSION_STR "0"
#define SUB_VERSION_INT 1
#define SUB_VERSION_STR "1"
#define RELEASE_NUMBER_INT 0
#define RELEASE_NUMBER_STR "0"
#define BUILD_NUMBER_INT 1
#define BUILD_NUMBER_STR "1"
#define FULL_VERSION_STR "0.1.0-alpha.1"
#define VERSION_STR FULL_VERSION_STR

#define stringOriginalFilename "AeylaVisualDmxAlpha.vst3"
#if SMTG_PLATFORM_64
#define stringFileDescription "AEYLA Visual DMX Alpha VST3 (64Bit)"
#else
#define stringFileDescription "AEYLA Visual DMX Alpha VST3"
#endif
#define stringCompanyName "RGB Estudios\0"
#define stringLegalCopyright "Copyright(c) 2026 RGB Estudios."
#define stringLegalTrademarks "VST is a trademark of Steinberg Media Technologies GmbH"
#define stringFileVersion FULL_VERSION_STR
#define stringProductVersion FULL_VERSION_STR

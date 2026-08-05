#define PLUG_NAME "AEYLA Visual DMX"
#define PLUG_MFR "RGBEstudios"
#define PLUG_VERSION_HEX 0x00020000
#define PLUG_VERSION_STR "0.2.0-alpha.1"
#define PLUG_UNIQUE_ID 'AyVD'
#define PLUG_MFR_ID 'RGBE'
#define PLUG_URL_STR "https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS"
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR "Copyright 2026 RGB Estudios"
#define PLUG_CLASS_NAME AeylaVisualDmx

#define BUNDLE_NAME "AeylaVisualDmx"
#define BUNDLE_MFR "RGBEstudios"
#define BUNDLE_DOMAIN "cl"
#define SHARED_RESOURCES_SUBPATH "AeylaVisualDmx"

// Silent instrument topology so Ableton places it on a MIDI track.
#define PLUG_CHANNEL_IO "0-2"
#define PLUG_LATENCY 0
#define PLUG_TYPE 1
#define PLUG_DOES_MIDI_IN 1
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0

#define PLUG_HAS_UI 1
#define PLUG_WIDTH 1280
#define PLUG_HEIGHT 800
#define PLUG_FPS 30
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 1
#define PLUG_MIN_WIDTH 960
#define PLUG_MIN_HEIGHT 620
#define PLUG_MAX_WIDTH 2560
#define PLUG_MAX_HEIGHT 1600

#define AUV2_ENTRY AeylaVisualDmx_Entry
#define AUV2_ENTRY_STR "AeylaVisualDmx_Entry"
#define AUV2_FACTORY AeylaVisualDmx_Factory
#define AUV2_VIEW_CLASS AeylaVisualDmx_View
#define AUV2_VIEW_CLASS_STR "AeylaVisualDmx_View"

#define AAX_TYPE_IDS 'AyV1'
#define AAX_TYPE_IDS_AUDIOSUITE 'AyA1'
#define AAX_PLUG_MFR_STR "RGB Estudios"
#define AAX_PLUG_NAME_STR "AEYLA Visual DMX\nAyVD"
#define AAX_PLUG_CATEGORY_STR "Instrument"
#define AAX_DOES_AUDIOSUITE 0

#define VST3_SUBCATEGORY "Instrument|Synth"
#define CLAP_MANUAL_URL "https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS"
#define CLAP_SUPPORT_URL "https://github.com/rgb-estudios/Midi-to-dmx-RGB.ESTUDIOS/issues"
#define CLAP_DESCRIPTION "Visual-to-DMX runtime and editor for AEYLA"
#define CLAP_FEATURES "instrument", "utility"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

cmake_minimum_required(VERSION 3.25)

# The product persistence layer uses C++17 filesystem, which Apple exposes from
# macOS 10.15. Our supported DAW matrix starts at macOS 11 (Live 12's current
# supported floor is 11.7.10), so reject accidental legacy deployment targets
# rather than producing a bundle that cannot link or load reliably.
if(APPLE)
  if(NOT CMAKE_OSX_DEPLOYMENT_TARGET OR
     CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "11.0")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
        "Minimum macOS version supported by AEYLA Visual DMX" FORCE)
  endif()

  # iPlug2 creates universal plug-in targets when IPLUG2_UNIVERSAL is enabled,
  # but ordinary dependency targets still follow CMAKE_OSX_ARCHITECTURES. Keep
  # miniz universal too or the Intel slice links against an arm64-only archive.
  if(IPLUG2_UNIVERSAL)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
        "Architectures for AEYLA universal macOS products" FORCE)
  endif()
endif()

project(AeylaVisualDmxProduct VERSION 0.3.3 LANGUAGES C CXX)

include(FetchContent)

# Pin the UI framework independently from its git submodules. GitHub's source
# fetch used by CMake does not reliably populate nested submodules across all
# hosted runners, so every required dependency is materialized explicitly.
if(NOT DEFINED IPLUG2_DIR)
  FetchContent_Declare(
    iplug2
    GIT_REPOSITORY https://github.com/iPlug2/iPlug2.git
    GIT_TAG 584df5a3306f3a9a62b5ebb803d3fb58134abdcf
    GIT_SHALLOW FALSE
  )

  FetchContent_MakeAvailable(iplug2)
  set(IPLUG2_DIR "${iplug2_SOURCE_DIR}" CACHE PATH "Pinned iPlug2 framework root" FORCE)
endif()

if(NOT EXISTS "${IPLUG2_DIR}/iPlug2.cmake")
  message(FATAL_ERROR "IPLUG2_DIR does not contain iPlug2.cmake: ${IPLUG2_DIR}")
endif()

# iPlug2 expects the Steinberg SDK at this exact framework-relative location.
set(_aeyla_vst3_sdk_dir "${IPLUG2_DIR}/Dependencies/IPlug/VST3_SDK")
if(NOT EXISTS "${_aeyla_vst3_sdk_dir}/pluginterfaces")
  FetchContent_Declare(
    aeyla_vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG 9fad9770f2ae8542ab1a548a68c1ad1ac690abe0
    GIT_SHALLOW FALSE
    SOURCE_DIR "${_aeyla_vst3_sdk_dir}"
  )
  FetchContent_MakeAvailable(aeyla_vst3sdk)
endif()

if(NOT EXISTS "${_aeyla_vst3_sdk_dir}/pluginterfaces")
  message(FATAL_ERROR
    "The pinned Steinberg VST3 SDK was not populated at ${_aeyla_vst3_sdk_dir}.")
endif()

if(NOT EXISTS "${_aeyla_vst3_sdk_dir}/base/source/baseiids.cpp")
  message(FATAL_ERROR
    "The pinned Steinberg VST3 SDK is incomplete: base/source/baseiids.cpp is missing.")
endif()

# The product binary uses the same pinned ZIP implementation and compile-time
# limits as the core test build. It is deliberately built here rather than
# relying on a system package, preserving Windows/macOS parity.
FetchContent_Declare(
  aeyla_product_miniz_source
  GIT_REPOSITORY https://github.com/richgel999/miniz.git
  GIT_TAG 77d0dce8627735138c51770d1799a1ef48f2117d
  GIT_SHALLOW FALSE
  SOURCE_SUBDIR _aeyla_no_project_cmake
)
FetchContent_MakeAvailable(aeyla_product_miniz_source)

set(_aeyla_miniz_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/miniz")
file(MAKE_DIRECTORY "${_aeyla_miniz_generated_dir}")
file(WRITE "${_aeyla_miniz_generated_dir}/miniz_export.h"
  "#pragma once\n#define MINIZ_EXPORT\n")

add_library(aeyla_product_miniz STATIC
  "${aeyla_product_miniz_source_SOURCE_DIR}/miniz.c"
  "${aeyla_product_miniz_source_SOURCE_DIR}/miniz_zip.c"
  "${aeyla_product_miniz_source_SOURCE_DIR}/miniz_tinfl.c"
  "${aeyla_product_miniz_source_SOURCE_DIR}/miniz_tdef.c"
)
target_include_directories(aeyla_product_miniz PUBLIC
  "${aeyla_product_miniz_source_SOURCE_DIR}"
  "${_aeyla_miniz_generated_dir}"
)
target_compile_definitions(aeyla_product_miniz PUBLIC
  MINIZ_NO_TIME
  MINIZ_NO_ZLIB_APIS
)
if(MSVC)
  target_compile_options(aeyla_product_miniz PRIVATE /W3)
else()
  target_compile_options(aeyla_product_miniz PRIVATE
    -Wall -Wextra -Wno-unused-parameter)
endif()

# Compatibility patch for the pinned iPlug2 proof revision. IColor is a simple
# four-integer value type, but its constructor is not marked constexpr.
set(_aeyla_iplug_color_header "${IPLUG2_DIR}/IGraphics/IGraphicsStructs.h")
file(READ "${_aeyla_iplug_color_header}" _aeyla_iplug_color_source)
set(_aeyla_iplug_color_old
  "  IColor(int a = 255, int r = 0, int g = 0, int b = 0) : A(a), R(r), G(g), B(b) {}")
set(_aeyla_iplug_color_new
  "  constexpr IColor(int a = 255, int r = 0, int g = 0, int b = 0) : A(a), R(r), G(g), B(b) {}")

string(FIND "${_aeyla_iplug_color_source}" "${_aeyla_iplug_color_new}" _aeyla_color_already_patched)
if(_aeyla_color_already_patched EQUAL -1)
  string(FIND "${_aeyla_iplug_color_source}" "${_aeyla_iplug_color_old}" _aeyla_color_patch_point)
  if(_aeyla_color_patch_point EQUAL -1)
    message(FATAL_ERROR
      "Pinned iPlug2 IColor constructor changed; review the compatibility patch before building.")
  endif()

  string(REPLACE
    "${_aeyla_iplug_color_old}"
    "${_aeyla_iplug_color_new}"
    _aeyla_iplug_color_source
    "${_aeyla_iplug_color_source}")
  file(WRITE "${_aeyla_iplug_color_header}" "${_aeyla_iplug_color_source}")
  message(STATUS "Applied AEYLA constexpr IColor compatibility patch to pinned iPlug2")
endif()

# AEYLA is a lighting application, not an audio workstation. The generic iPlug2
# standalone host attempts to initialise DirectSound/ASIO before opening its
# window, which can terminate silently on machines with unavailable or unusual
# audio devices. Make no-I/O mode the standalone default. This does not affect
# VST3/AUv2 MIDI delivery inside a DAW. Standalone MIDI will be supplied by the
# dedicated AEYLA MIDI service rather than by the audio-host wrapper.
set(_aeyla_iplug_app_main "${IPLUG2_DIR}/IPlug/APP/IPlugAPP_main.cpp")
file(READ "${_aeyla_iplug_app_main}" _aeyla_iplug_app_main_source)
set(_aeyla_app_host_old
  "    IPlugAPPHost* pAppHost = IPlugAPPHost::Create();\n\n    // Parse command line arguments")
set(_aeyla_app_host_new
  "    IPlugAPPHost* pAppHost = IPlugAPPHost::Create();\n\n    // AEYLA standalone must never depend on an audio driver to show its UI.\n    pAppHost->SetNoIO(true);\n\n    // Parse command line arguments")

string(FIND "${_aeyla_iplug_app_main_source}" "${_aeyla_app_host_new}" _aeyla_no_io_already_patched)
if(_aeyla_no_io_already_patched EQUAL -1)
  string(FIND "${_aeyla_iplug_app_main_source}" "${_aeyla_app_host_old}" _aeyla_no_io_patch_point)
  if(_aeyla_no_io_patch_point EQUAL -1)
    message(FATAL_ERROR
      "Pinned iPlug2 standalone startup changed; review the no-I/O startup patch.")
  endif()

  string(REPLACE
    "${_aeyla_app_host_old}"
    "${_aeyla_app_host_new}"
    _aeyla_iplug_app_main_source
    "${_aeyla_iplug_app_main_source}")
  file(WRITE "${_aeyla_iplug_app_main}" "${_aeyla_iplug_app_main_source}")
  message(STATUS "Applied AEYLA no-audio standalone startup patch")
endif()

# The pinned standalone Init() assumes InitMidi() always creates RtMidi objects.
# In AEYLA no-I/O mode InitMidi() deliberately returns without creating them,
# so the subsequent SelectMIDIDevice() calls dereference null mMidiIn/mMidiOut
# and crash at address 0 on Windows. Keep the upstream source pinned and patch
# only that invalid no-I/O path; DAW VST3/AUv2 MIDI remains unchanged.
set(_aeyla_iplug_app_host "${IPLUG2_DIR}/IPlug/APP/IPlugAPP_host.cpp")
file(READ "${_aeyla_iplug_app_host}" _aeyla_iplug_app_host_source)
set(_aeyla_midi_select_old
  "  InitMidi(); // creates RTMidiIn and RTMidiOut objects\n  ProbeMidiIO(); // find out what midi IO devs are available and put their names in the global variables gMidiInputDevs / gMidiOutputDevs\n  SelectMIDIDevice(ERoute::kInput, mState.mMidiInDev.Get());\n  SelectMIDIDevice(ERoute::kOutput, mState.mMidiOutDev.Get());")
set(_aeyla_midi_select_new
  "  InitMidi(); // creates RTMidiIn and RTMidiOut objects\n  ProbeMidiIO(); // find out what midi IO devs are available and put their names in the global variables gMidiInputDevs / gMidiOutputDevs\n  if(!mNoIO && !IsScreenshotMode())\n  {\n    SelectMIDIDevice(ERoute::kInput, mState.mMidiInDev.Get());\n    SelectMIDIDevice(ERoute::kOutput, mState.mMidiOutDev.Get());\n  }")

string(FIND "${_aeyla_iplug_app_host_source}" "${_aeyla_midi_select_new}" _aeyla_midi_no_io_already_patched)
if(_aeyla_midi_no_io_already_patched EQUAL -1)
  string(FIND "${_aeyla_iplug_app_host_source}" "${_aeyla_midi_select_old}" _aeyla_midi_no_io_patch_point)
  if(_aeyla_midi_no_io_patch_point EQUAL -1)
    message(FATAL_ERROR
      "Pinned iPlug2 MIDI startup changed; review the no-I/O MIDI guard before building.")
  endif()

  string(REPLACE
    "${_aeyla_midi_select_old}"
    "${_aeyla_midi_select_new}"
    _aeyla_iplug_app_host_source
    "${_aeyla_iplug_app_host_source}")
  file(WRITE "${_aeyla_iplug_app_host}" "${_aeyla_iplug_app_host_source}")
  message(STATUS "Applied AEYLA no-I/O MIDI null-guard patch to pinned iPlug2")
endif()

# Temporary R08 diagnostic breadcrumbs for the Windows graphical standalone.
# The hosted runner reports a second 0xC0000005 with no usable WER module/stack.
# Record only coarse startup boundaries so we can identify whether the remaining
# failure is before CreateDialog, inside WM_INITDIALOG/OpenWindow, or after UI
# attachment. This patch is Windows-only and must be removed before freezing the
# final PRETEST SHA.
if(WIN32)
  set(_aeyla_trace_anchor "UINT gScrollMessage;\n")
  set(_aeyla_trace_impl [=[UINT gScrollMessage;

void AeylaStartupTrace(const char* stage)
{
  if(stage == nullptr)
    return;
  char tempPath[MAX_PATH] = {};
  const DWORD tempLength = GetTempPathA(MAX_PATH, tempPath);
  if(tempLength == 0 || tempLength >= MAX_PATH)
    return;
  static const char kFileName[] = "AEYLA_GRAPHICAL_STARTUP_TRACE.txt";
  if(tempLength + sizeof(kFileName) >= MAX_PATH)
    return;
  char path[MAX_PATH] = {};
  lstrcpyA(path, tempPath);
  lstrcatA(path, kFileName);
  HANDLE file = CreateFileA(path, FILE_APPEND_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if(file == INVALID_HANDLE_VALUE)
    return;
  DWORD written = 0;
  WriteFile(file, stage, static_cast<DWORD>(lstrlenA(stage)), &written, nullptr);
  static const char newline[] = "\r\n";
  WriteFile(file, newline, 2U, &written, nullptr);
  FlushFileBuffers(file);
  CloseHandle(file);
}
]=])
  string(FIND "${_aeyla_iplug_app_main_source}" "void AeylaStartupTrace(const char* stage)" _aeyla_trace_impl_present)
  if(_aeyla_trace_impl_present EQUAL -1)
    string(FIND "${_aeyla_iplug_app_main_source}" "${_aeyla_trace_anchor}" _aeyla_trace_anchor_pos)
    if(_aeyla_trace_anchor_pos EQUAL -1)
      message(FATAL_ERROR "Pinned iPlug2 trace anchor changed; cannot instrument graphical startup safely.")
    endif()
    string(REPLACE "${_aeyla_trace_anchor}" "${_aeyla_trace_impl}" _aeyla_iplug_app_main_source "${_aeyla_iplug_app_main_source}")
  endif()

  set(_aeyla_trace_create_old "    IPlugAPPHost* pAppHost = IPlugAPPHost::Create();")
  set(_aeyla_trace_create_new "    AeylaStartupTrace(\"WinMain:before-CreateHost\");\n    IPlugAPPHost* pAppHost = IPlugAPPHost::Create();\n    AeylaStartupTrace(\"WinMain:after-CreateHost\");")
  string(REPLACE "${_aeyla_trace_create_old}" "${_aeyla_trace_create_new}" _aeyla_iplug_app_main_source "${_aeyla_iplug_app_main_source}")

  set(_aeyla_trace_init_old "    pAppHost->Init();\n    pAppHost->TryToChangeAudio();")
  set(_aeyla_trace_init_new "    AeylaStartupTrace(\"WinMain:before-Init\");\n    pAppHost->Init();\n    AeylaStartupTrace(\"WinMain:after-Init\");\n    pAppHost->TryToChangeAudio();\n    AeylaStartupTrace(\"WinMain:after-TryToChangeAudio\");")
  string(REPLACE "${_aeyla_trace_init_old}" "${_aeyla_trace_init_new}" _aeyla_iplug_app_main_source "${_aeyla_iplug_app_main_source}")

  set(_aeyla_trace_dialog_old "    CreateDialog(gHINSTANCE, MAKEINTRESOURCE(IDD_DIALOG_MAIN), GetDesktopWindow(), IPlugAPPHost::MainDlgProc);")
  set(_aeyla_trace_dialog_new "    AeylaStartupTrace(\"WinMain:before-CreateDialog\");\n    HWND aeylaDialog = CreateDialog(gHINSTANCE, MAKEINTRESOURCE(IDD_DIALOG_MAIN), GetDesktopWindow(), IPlugAPPHost::MainDlgProc);\n    AeylaStartupTrace(aeylaDialog != nullptr ? \"WinMain:after-CreateDialog-ok\" : \"WinMain:after-CreateDialog-null\");")
  string(REPLACE "${_aeyla_trace_dialog_old}" "${_aeyla_trace_dialog_new}" _aeyla_iplug_app_main_source "${_aeyla_iplug_app_main_source}")
  file(WRITE "${_aeyla_iplug_app_main}" "${_aeyla_iplug_app_main_source}")

  set(_aeyla_iplug_app_dialog "${IPLUG2_DIR}/IPlug/APP/IPlugAPP_dialog.cpp")
  file(READ "${_aeyla_iplug_app_dialog}" _aeyla_iplug_app_dialog_source)
  set(_aeyla_dialog_extern_old "extern float GetScaleForHWND(HWND hWnd);\n#define GET_MENU() GetMenu(gHWND)")
  set(_aeyla_dialog_extern_new "extern float GetScaleForHWND(HWND hWnd);\nextern void AeylaStartupTrace(const char* stage);\n#define GET_MENU() GetMenu(gHWND)")
  string(REPLACE "${_aeyla_dialog_extern_old}" "${_aeyla_dialog_extern_new}" _aeyla_iplug_app_dialog_source "${_aeyla_iplug_app_dialog_source}")

  set(_aeyla_dialog_init_old [=[    case WM_INITDIALOG:
    {
      gHWND = hwndDlg;
      IPlugAPP* pPlug = pAppHost->GetPlug();

      if (!pAppHost->OpenWindow(gHWND))
      {
        DBGMSG("couldn't attach gui\n");
      }

      ClientResize(hwndDlg, pPlug->GetEditorWidth(), pPlug->GetEditorHeight());

      ShowWindow(hwndDlg, SW_SHOW);]=])
  set(_aeyla_dialog_init_new [=[    case WM_INITDIALOG:
    {
      AeylaStartupTrace("MainDlg:WM_INITDIALOG-enter");
      gHWND = hwndDlg;
      IPlugAPP* pPlug = pAppHost->GetPlug();
      AeylaStartupTrace("MainDlg:before-OpenWindow");

      if (!pAppHost->OpenWindow(gHWND))
      {
        DBGMSG("couldn't attach gui\n");
      }
      AeylaStartupTrace("MainDlg:after-OpenWindow");

      ClientResize(hwndDlg, pPlug->GetEditorWidth(), pPlug->GetEditorHeight());
      AeylaStartupTrace("MainDlg:after-ClientResize");

      ShowWindow(hwndDlg, SW_SHOW);
      AeylaStartupTrace("MainDlg:after-ShowWindow");]=])
  string(FIND "${_aeyla_iplug_app_dialog_source}" "${_aeyla_dialog_init_old}" _aeyla_dialog_init_pos)
  if(_aeyla_dialog_init_pos EQUAL -1)
    message(FATAL_ERROR "Pinned iPlug2 WM_INITDIALOG changed; cannot instrument graphical startup safely.")
  endif()
  string(REPLACE "${_aeyla_dialog_init_old}" "${_aeyla_dialog_init_new}" _aeyla_iplug_app_dialog_source "${_aeyla_iplug_app_dialog_source}")

  set(_aeyla_minmax_old "    case WM_GETMINMAXINFO:\n    {\n      if (!pAppHost)")
  set(_aeyla_minmax_new "    case WM_GETMINMAXINFO:\n    {\n      AeylaStartupTrace(\"MainDlg:WM_GETMINMAXINFO\");\n      if (!pAppHost)")
  string(REPLACE "${_aeyla_minmax_old}" "${_aeyla_minmax_new}" _aeyla_iplug_app_dialog_source "${_aeyla_iplug_app_dialog_source}")
  file(WRITE "${_aeyla_iplug_app_dialog}" "${_aeyla_iplug_app_dialog_source}")
  message(STATUS "Applied temporary AEYLA Windows graphical startup breadcrumbs")
endif()

set(IPLUG_DEPLOY_PLUGINS OFF CACHE BOOL "Do not deploy CI proof artifacts" FORCE)
set(IGRAPHICS_BACKEND NANOVG CACHE STRING "Lightweight shared UI backend" FORCE)

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

if(WIN32)
  add_executable(AeylaNetworkHelper
    ../tools/network/windows/AeylaNetworkHelper.cpp
    ../src/network/ipv4_configuration.cpp
    ../src/network/network_change_protocol.cpp
    ../src/network/network_interfaces.cpp
  )
  target_include_directories(AeylaNetworkHelper PRIVATE ../src)
  target_compile_features(AeylaNetworkHelper PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(AeylaNetworkHelper PRIVATE /W4 /permissive-)
  else()
    target_compile_options(AeylaNetworkHelper PRIVATE
      -Wall -Wextra -Wpedantic -Werror)
  endif()
  target_link_libraries(AeylaNetworkHelper PRIVATE ws2_32 iphlpapi advapi32)
endif()

add_subdirectory(AeylaVisualDmx)

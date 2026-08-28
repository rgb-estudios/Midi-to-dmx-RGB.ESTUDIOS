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

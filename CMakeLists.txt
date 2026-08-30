cmake_minimum_required(VERSION 3.20)

# AEYLA's persistence layer uses std::filesystem, which libc++ only exposes
# from macOS 10.15. The supported DAW/product floor is macOS 11, so normalize
# the target before project() performs compiler and linker feature checks.
if(APPLE AND
   (NOT CMAKE_OSX_DEPLOYMENT_TARGET OR
    CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "11.0"))
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
      "Minimum macOS version supported by AEYLA Visual DMX" FORCE)
endif()

project(aeyla_visual_dmx_core VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
find_package(Threads REQUIRED)

# Pinned ZIP implementation for the cross-platform .aeylashow container.
# The dependency is populated without executing its project-level CMake so it
# cannot alter AEYLA output directories or global build options.
FetchContent_Declare(
  aeyla_miniz_source
  GIT_REPOSITORY https://github.com/richgel999/miniz.git
  GIT_TAG 77d0dce8627735138c51770d1799a1ef48f2117d
  GIT_SHALLOW FALSE
  SOURCE_SUBDIR _aeyla_no_project_cmake
)
FetchContent_MakeAvailable(aeyla_miniz_source)

set(AEYLA_MINIZ_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/miniz")
file(MAKE_DIRECTORY "${AEYLA_MINIZ_GENERATED_DIR}")
file(WRITE "${AEYLA_MINIZ_GENERATED_DIR}/miniz_export.h"
  "#pragma once\n#define MINIZ_EXPORT\n")

add_library(aeyla_miniz STATIC
  "${aeyla_miniz_source_SOURCE_DIR}/miniz.c"
  "${aeyla_miniz_source_SOURCE_DIR}/miniz_zip.c"
  "${aeyla_miniz_source_SOURCE_DIR}/miniz_tinfl.c"
  "${aeyla_miniz_source_SOURCE_DIR}/miniz_tdef.c"
)
target_include_directories(aeyla_miniz PUBLIC
  "${aeyla_miniz_source_SOURCE_DIR}"
  "${AEYLA_MINIZ_GENERATED_DIR}"
)
target_compile_definitions(aeyla_miniz PUBLIC
  MINIZ_NO_TIME
  MINIZ_NO_ZLIB_APIS
)
if(MSVC)
  target_compile_options(aeyla_miniz PRIVATE /W3)
else()
  target_compile_options(aeyla_miniz PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_library(aeyla_core
  src/core/attributes.cpp
  src/core/color_transform.cpp
  src/core/dmx_compiler.cpp
  src/core/artnet_packet.cpp
  src/core/rig.cpp
)

target_include_directories(aeyla_core PUBLIC src)
if(MSVC)
  target_compile_options(aeyla_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(aeyla_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(aeyla_core_demo apps/cli/main.cpp)
target_link_libraries(aeyla_core_demo PRIVATE aeyla_core)

enable_testing()
add_executable(aeyla_core_tests tests/test_main.cpp)
target_link_libraries(aeyla_core_tests PRIVATE aeyla_core)
add_test(NAME aeyla_core_tests COMMAND aeyla_core_tests)

add_library(aeyla_runtime
  src/runtime/plugin_state.cpp
  src/runtime/show_midi_control.cpp
  src/runtime/runtime_safety_state.cpp
  src/runtime/host_song_binding.cpp
  src/product/application_model.cpp
  src/product/project_file_controller.cpp
  src/product/project_identity.cpp
  src/project/project_document.cpp
  src/project/project_state_store.cpp
  src/project/project_package.cpp
  src/show/show_program.cpp
  src/show/show_program_codec.cpp
  src/network/network_interfaces.cpp
  src/network/ipv4_configuration.cpp
  src/network/network_change_protocol.cpp
  src/capture/artnet_capture_worker.cpp
  src/capture/dmx_capture_sync_anchor.cpp
  src/capture/dmx_clip_playback_engine.cpp
  src/capture/dmx_take_activity.cpp
  src/capture/dmx_take_consolidator.cpp
  src/capture/dmx_take_file_store.cpp
  src/capture/dmx_take_file_reader.cpp
  src/capture/dmx_take_scheduler.cpp
  src/capture/dmx_take_transport.cpp
  src/capture/dmx_take_stream_writer.cpp
  src/output/dmx_live_memory.cpp
  src/output/artnet_output_worker.cpp
)
target_include_directories(aeyla_runtime PUBLIC src)
target_link_libraries(aeyla_runtime PUBLIC
  aeyla_core
  aeyla_miniz
  Threads::Threads
)
if(WIN32)
  target_link_libraries(aeyla_runtime PUBLIC ws2_32 iphlpapi)
endif()
if(MSVC)
  target_compile_options(aeyla_runtime PRIVATE /W4 /permissive-)
else()
  target_compile_options(aeyla_runtime PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

function(aeyla_add_strict_test target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE aeyla_runtime)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME ${target} COMMAND ${target})
endfunction()

aeyla_add_strict_test(aeyla_runtime_tests tests/test_host_event_queue.cpp)
aeyla_add_strict_test(aeyla_host_transport_mailbox_tests tests/test_host_transport_mailbox.cpp)
aeyla_add_strict_test(aeyla_host_song_binding_tests tests/test_host_song_binding.cpp)
aeyla_add_strict_test(aeyla_plugin_state_tests tests/test_plugin_state.cpp)
aeyla_add_strict_test(aeyla_show_midi_control_tests tests/test_show_midi_control.cpp)
aeyla_add_strict_test(aeyla_runtime_safety_tests tests/test_runtime_safety.cpp)
aeyla_add_strict_test(aeyla_application_model_tests tests/test_application_model.cpp)
aeyla_add_strict_test(aeyla_project_document_tests tests/test_project_document.cpp)
aeyla_add_strict_test(aeyla_project_state_store_tests tests/test_project_state_store.cpp)
aeyla_add_strict_test(aeyla_project_package_tests tests/test_project_package.cpp)
aeyla_add_strict_test(aeyla_project_file_controller_tests tests/test_project_file_controller.cpp)
aeyla_add_strict_test(aeyla_show_program_tests tests/test_show_program.cpp)
aeyla_add_strict_test(aeyla_show_performance_preflight_tests tests/test_show_performance_preflight.cpp)
aeyla_add_strict_test(aeyla_show_program_codec_tests tests/test_show_program_codec.cpp)
aeyla_add_strict_test(aeyla_programmer_tests tests/test_programmer.cpp)
aeyla_add_strict_test(aeyla_dmx_live_memory_tests tests/test_dmx_live_memory.cpp)
aeyla_add_strict_test(aeyla_artnet_output_worker_tests tests/test_artnet_output_worker.cpp)
aeyla_add_strict_test(aeyla_artnet_output_gate_r07_tests tests/test_artnet_output_gate_r07.cpp)
aeyla_add_strict_test(aeyla_artnet_capture_worker_tests tests/test_artnet_capture_worker.cpp)
aeyla_add_strict_test(aeyla_dmx_capture_sync_anchor_tests tests/test_dmx_capture_sync_anchor.cpp)
aeyla_add_strict_test(aeyla_ipv4_configuration_tests tests/test_ipv4_configuration.cpp)
aeyla_add_strict_test(aeyla_dmx_clip_playback_engine_tests tests/test_dmx_clip_playback_engine.cpp)
aeyla_add_strict_test(aeyla_artnet_armed_carrier_tests tests/test_artnet_armed_carrier.cpp)
aeyla_add_strict_test(aeyla_ableton_silent_carrier_tests tests/test_ableton_silent_carrier.cpp)
aeyla_add_strict_test(aeyla_dmx_take_activity_tests tests/test_dmx_take_activity.cpp)
aeyla_add_strict_test(aeyla_dmx_take_consolidator_tests tests/test_dmx_take_consolidator.cpp)
aeyla_add_strict_test(aeyla_dmx_take_file_store_tests tests/test_dmx_take_file_store.cpp)
aeyla_add_strict_test(aeyla_dmx_take_file_reader_tests tests/test_dmx_take_file_reader.cpp)
aeyla_add_strict_test(aeyla_dmx_take_transport_tests tests/test_dmx_take_transport.cpp)
aeyla_add_strict_test(aeyla_dmx_take_stream_writer_tests tests/test_dmx_take_stream_writer.cpp)

add_executable(aeyla_take_library_session_tests
  tests/test_take_library_session.cpp
  product/AeylaVisualDmx/AeylaTakeLibrarySession.cpp
)
target_include_directories(aeyla_take_library_session_tests PRIVATE
  product/AeylaVisualDmx
)
target_link_libraries(aeyla_take_library_session_tests PRIVATE aeyla_runtime)
if(MSVC)
  target_compile_options(aeyla_take_library_session_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(aeyla_take_library_session_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME aeyla_take_library_session_tests
         COMMAND aeyla_take_library_session_tests)

add_executable(aeyla_live_memory_session_tests
  tests/test_live_memory_session.cpp
  product/AeylaVisualDmx/AeylaLiveMemorySession.cpp
)
target_include_directories(aeyla_live_memory_session_tests PRIVATE
  product/AeylaVisualDmx
)
target_link_libraries(aeyla_live_memory_session_tests PRIVATE aeyla_runtime)
if(MSVC)
  target_compile_options(aeyla_live_memory_session_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(aeyla_live_memory_session_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME aeyla_live_memory_session_tests
         COMMAND aeyla_live_memory_session_tests)

option(AEYLA_BUILD_STANDALONE_PROOF "Build the AEYLA shared-runtime standalone diagnostic" OFF)
if(AEYLA_BUILD_STANDALONE_PROOF)
  add_executable(AeylaVisualDmxStandalone apps/standalone/main.cpp)
  target_link_libraries(AeylaVisualDmxStandalone PRIVATE aeyla_runtime)
  target_compile_features(AeylaVisualDmxStandalone PRIVATE cxx_std_20)

  if(MSVC)
    target_compile_options(AeylaVisualDmxStandalone PRIVATE /W4 /permissive-)
  else()
    target_compile_options(AeylaVisualDmxStandalone PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endif()

option(AEYLA_BUILD_VST3_PROOF "Build the first silent AEYLA VST3 proof" OFF)
if(AEYLA_BUILD_VST3_PROOF)
  set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG 9fad9770f2ae8542ab1a548a68c1ad1ac690abe0
    GIT_SHALLOW FALSE
    GIT_SUBMODULES_RECURSE TRUE
  )
  FetchContent_MakeAvailable(vst3sdk)
  smtg_enable_vst3_sdk()
  add_subdirectory(plugins/vst3/proof)
endif()

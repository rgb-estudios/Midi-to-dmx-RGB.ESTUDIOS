cmake_minimum_required(VERSION 3.25)
project(AeylaVisualDmx VERSION 0.3.3)

if(NOT DEFINED IPLUG2_DIR)
  message(FATAL_ERROR "IPLUG2_DIR must be provided by product/CMakeLists.txt")
endif()

# Apply narrowly scoped compatibility/safety patches to the exact pinned iPlug2
# source before the APP/VST3 targets compile it. On Windows this makes an
# unavailable OpenGL/NanoVG editor fail cleanly instead of terminating the host.
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/AeylaIPlugWindowsGraphicsHardening.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/AeylaIPlugWindowsNanoVGPreflight.cmake)

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

set(AEYLA_PLUGIN_FORMATS APP VST3)
if(APPLE)
  # Logic Pro does not host VST3. AUv2 is a required first-class product
  # format on macOS and shares the exact project/runtime engine with APP/VST3.
  list(APPEND AEYLA_PLUGIN_FORMATS AU)
endif()

iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    AeylaVisualDmx.cpp
    AeylaShowPlayerIntegration.cpp
    AeylaMidiShowIntegration.cpp
    AeylaTakeTrimIntegration.cpp
    AeylaTakeLibrarySession.cpp
    AeylaTakeLibrarySession.h
    AeylaLiveMemorySession.cpp
    AeylaLiveMemorySession.h
    AeylaNetworkConfiguration.cpp
    AeylaNetworkConfiguration.h
    AeylaVisualDmx.h
    AeylaMainControl.h
    AeylaRuntimeStatusControl.h
    config.h
    resources/resource.h
    ../../src/core/attributes.cpp
    ../../src/core/artnet_packet.cpp
    ../../src/core/color_transform.cpp
    ../../src/core/dmx_compiler.cpp
    ../../src/core/rig.cpp
    ../../src/runtime/plugin_state.cpp
    ../../src/runtime/show_midi_control.cpp
    ../../src/runtime/runtime_safety_state.cpp
    ../../src/runtime/host_song_binding.cpp
    ../../src/project/project_document.cpp
    ../../src/project/live_memory_state.cpp
    ../../src/project/project_package.cpp
    ../../src/project/project_state_store.cpp
    ../../src/product/application_model.cpp
    ../../src/product/project_file_controller.cpp
    ../../src/product/project_identity.cpp
    ../../src/show/show_program.cpp
    ../../src/show/show_program_codec.cpp
    ../../src/network/network_interfaces.cpp
    ../../src/network/ipv4_configuration.cpp
    ../../src/network/network_change_protocol.cpp
    ../../src/capture/artnet_capture_worker.cpp
    ../../src/capture/dmx_capture_sync_anchor.cpp
    ../../src/capture/dmx_clip_playback_engine.cpp
    ../../src/capture/dmx_take_activity.cpp
    ../../src/capture/dmx_take_consolidator.cpp
    ../../src/capture/dmx_take_file_reader.cpp
    ../../src/capture/dmx_take_file_store.cpp
    ../../src/capture/dmx_take_scheduler.cpp
    ../../src/capture/dmx_take_stream_writer.cpp
    ../../src/capture/dmx_take_transport.cpp
    ../../src/output/dmx_live_memory.cpp
    ../../src/output/artnet_output_worker.cpp
  FORMATS
    ${AEYLA_PLUGIN_FORMATS}
  UI
    IGRAPHICS
)

if(MSVC)
  set_property(TARGET aeyla_product_miniz PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

foreach(target_name
        AeylaVisualDmx-app
        AeylaVisualDmx-vst3
        AeylaVisualDmx-au)
  if(TARGET ${target_name})
    target_include_directories(${target_name} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    )
    target_link_libraries(${target_name} PRIVATE aeyla_product_miniz)
    if(WIN32)
      target_link_libraries(${target_name} PRIVATE ws2_32 iphlpapi shell32)
    endif()
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    if(MSVC)
      target_compile_options(${target_name} PRIVATE /W4 /permissive-)
      target_link_options(${target_name} PRIVATE /DEBUG:FULL)
    else()
      target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
  endif()
endforeach()

if(WIN32 AND TARGET AeylaNetworkHelper)
  foreach(target_name AeylaVisualDmx-app AeylaVisualDmx-vst3)
    if(TARGET ${target_name})
      add_dependencies(${target_name} AeylaNetworkHelper)
      add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:AeylaNetworkHelper>
          $<TARGET_FILE_DIR:${target_name}>/AeylaNetworkHelper.exe
        COMMENT "Embedding the least-privilege AEYLA network helper")
    endif()
  endforeach()
endif()

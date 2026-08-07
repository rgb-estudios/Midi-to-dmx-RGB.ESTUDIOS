cmake_minimum_required(VERSION 3.25)
project(AeylaVisualDmx VERSION 0.3.0)

if(NOT DEFINED IPLUG2_DIR)
  message(FATAL_ERROR "IPLUG2_DIR must be provided by product/CMakeLists.txt")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

iplug_add_plugin(${PROJECT_NAME}
  SOURCES
    AeylaVisualDmx.cpp
    AeylaVisualDmx.h
    AeylaMainControl.h
    AeylaExecutorRuntimeControl.h
    AeylaRuntimeStatusControl.h
    config.h
    resources/resource.h
    ../../src/core/attributes.cpp
    ../../src/core/color_transform.cpp
    ../../src/core/dmx_compiler.cpp
    ../../src/core/rig.cpp
    ../../src/runtime/runtime_safety_state.cpp
    ../../src/project/project_document.cpp
    ../../src/product/application_model.cpp
  FORMATS
    APP
    VST3
  UI
    IGRAPHICS
)

foreach(target_name AeylaVisualDmx-app AeylaVisualDmx-vst3)
  if(TARGET ${target_name})
    target_include_directories(${target_name} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    )
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    if(MSVC)
      target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
      target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
  endif()
endforeach()

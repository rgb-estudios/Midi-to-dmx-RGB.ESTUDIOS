cmake_minimum_required(VERSION 3.25)
project(AeylaVisualDmxProduct VERSION 0.2.0 LANGUAGES C CXX)

# CI and release builds pre-clone the exact iPlug2 revision recursively so its
# nested VST3 SDK and graphics dependencies are present before configuration.
# A local developer may omit IPLUG2_DIR; CMake then performs the same pinned
# recursive fetch as a fallback.
if(NOT DEFINED IPLUG2_DIR)
  include(FetchContent)

  FetchContent_Declare(
    iplug2
    GIT_REPOSITORY https://github.com/iPlug2/iPlug2.git
    GIT_TAG 584df5a3306f3a9a62b5ebb803d3fb58134abdcf
    GIT_SHALLOW FALSE
    GIT_SUBMODULES_RECURSE TRUE
  )

  FetchContent_MakeAvailable(iplug2)
  set(IPLUG2_DIR "${iplug2_SOURCE_DIR}" CACHE PATH "Pinned iPlug2 framework root" FORCE)
endif()

if(NOT EXISTS "${IPLUG2_DIR}/iPlug2.cmake")
  message(FATAL_ERROR "IPLUG2_DIR does not contain iPlug2.cmake: ${IPLUG2_DIR}")
endif()

if(NOT EXISTS "${IPLUG2_DIR}/Dependencies/IPlug/VST3_SDK/pluginterfaces")
  message(FATAL_ERROR
    "The pinned iPlug2 checkout is missing its VST3 SDK submodule. "
    "Clone iPlug2 recursively or run git submodule update --init --recursive.")
endif()

set(IPLUG_DEPLOY_PLUGINS OFF CACHE BOOL "Do not deploy CI proof artifacts" FORCE)
set(IGRAPHICS_BACKEND NANOVG CACHE STRING "Lightweight shared UI backend" FORCE)

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

add_subdirectory(AeylaVisualDmx)

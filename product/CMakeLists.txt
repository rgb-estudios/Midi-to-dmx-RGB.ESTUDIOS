cmake_minimum_required(VERSION 3.25)
project(AeylaVisualDmxProduct VERSION 0.2.0 LANGUAGES C CXX)

include(FetchContent)

# Pinned proof revision. This branch exists to validate a single graphical
# codebase as both standalone application and VST3. Do not float this ref.
FetchContent_Declare(
  iplug2
  GIT_REPOSITORY https://github.com/iPlug2/iPlug2.git
  GIT_TAG 584df5a3306f3a9a62b5ebb803d3fb58134abdcf
  GIT_SHALLOW FALSE
  GIT_SUBMODULES_RECURSE TRUE
)

FetchContent_GetProperties(iplug2)
if(NOT iplug2_POPULATED)
  FetchContent_Populate(iplug2)
endif()

set(IPLUG2_DIR "${iplug2_SOURCE_DIR}" CACHE PATH "Pinned iPlug2 framework root" FORCE)
set(IPLUG_DEPLOY_PLUGINS OFF CACHE BOOL "Do not deploy CI proof artifacts" FORCE)
set(IGRAPHICS_BACKEND NANOVG CACHE STRING "Lightweight shared UI backend" FORCE)

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

add_subdirectory(AeylaVisualDmx)

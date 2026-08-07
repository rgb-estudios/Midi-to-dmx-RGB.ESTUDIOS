cmake_minimum_required(VERSION 3.20)
project(aeyla_visual_dmx_core VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

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
target_compile_definitions(aeyla_miniz PUBLIC MINIZ_NO_TIME)
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

# Shared product/runtime/project state used by standalone and VST3 adapters.
add_library(aeyla_runtime
  src/runtime/plugin_state.cpp
  src/runtime/runtime_safety_state.cpp
  src/product/application_model.cpp
  src/project/project_document.cpp
  src/project/project_state_store.cpp
  src/project/project_package.cpp
)
target_include_directories(aeyla_runtime PUBLIC src)
target_link_libraries(aeyla_runtime PUBLIC aeyla_core aeyla_miniz)
if(MSVC)
  target_compile_options(aeyla_runtime PRIVATE /W4 /permissive-)
else()
  target_compile_options(aeyla_runtime PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(aeyla_runtime_tests tests/test_host_event_queue.cpp)
target_link_libraries(aeyla_runtime_tests PRIVATE aeyla_runtime)
if(MSVC)
  target_compile_options(aeyla_runtime_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(aeyla_runtime_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME aeyla_runtime_tests COMMAND aeyla_runtime_tests)

add_executable(aeyla_plugin_state_tests tests/test_plugin_state.cpp)
target_link_libraries(aeyla_plugin_state_tests PRIVATE aeyla_runtime)
if(MSVC)
  target_compile_options(aeyla_plugin_state_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(aeyla_plugin_state_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME aeyla_plugin_state_tests COMMAND aeyla_plugin_state_tests)

add_executable(aeyla_runtime_safety_tests tests/test_runtime_safety.cpp)
target_link_libraries(aeyla_runtime_safety_tests PRIVATE aeyla_runtime)
if(MSVC)
  target_compile_options(aeyla_runtime_safety_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(aeyla_runtime_safety_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME aeyla_runtime_safety_tests COMMAND aeyla_runtime_safety_tests)

add_executable(aeyla_application_model_tests tests/test_application_model.cpp)
target_link_libraries(aeyla_application_model_tests PRIVATE aeyla_runtime)
if(MSVC)
  target_compile_options(aeyla_application_model_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(aeyla_application_model_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME aeyla_application_model_tests COMMAND aeyla_application_model_tests)

add_executable(aeyla_project_document_tests tests/test_project_document.cpp)
target_link_libraries(aeyla_project_document_tests PRIVATE aeyla_runtime)
if(MSVC)
  target_compile_options(aeyla_project_document_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(aeyla_project_document_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME aeyla_project_document_tests COMMAND aeyla_project_document_tests)

add_executable(aeyla_project_state_store_tests tests/test_project_state_store.cpp)
target_link_libraries(aeyla_project_state_store_tests PRIVATE aeyla_runtime)
if(MSVC)
  target_compile_options(aeyla_project_state_store_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(aeyla_project_state_store_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME aeyla_project_state_store_tests COMMAND aeyla_project_state_store_tests)

add_executable(aeyla_project_package_tests tests/test_project_package.cpp)
target_link_libraries(aeyla_project_package_tests PRIVATE aeyla_runtime)
if(MSVC)
  target_compile_options(aeyla_project_package_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(aeyla_project_package_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME aeyla_project_package_tests COMMAND aeyla_project_package_tests)

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

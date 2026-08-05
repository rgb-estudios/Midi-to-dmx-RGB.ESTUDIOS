cmake_minimum_required(VERSION 3.20)
project(aeyla_visual_dmx_core VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# Shared runtime primitives and state used by standalone and future VST3 adapters.
add_library(aeyla_runtime
  src/runtime/plugin_state.cpp
)
target_include_directories(aeyla_runtime PUBLIC src)
target_link_libraries(aeyla_runtime PUBLIC aeyla_core)
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

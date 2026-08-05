smtg_add_vst3plugin(AeylaVisualDmxAlpha
  aeyla_vst3_cids.h
  aeyla_vst3_processor.h
  aeyla_vst3_processor.cpp
  aeyla_vst3_controller.h
  aeyla_vst3_controller.cpp
  aeyla_vst3_entry.cpp
)

target_link_libraries(AeylaVisualDmxAlpha
  PRIVATE
    sdk
    aeyla_runtime
)

target_include_directories(AeylaVisualDmxAlpha
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(AeylaVisualDmxAlpha PRIVATE cxx_std_20)

if(MSVC)
  target_compile_options(AeylaVisualDmxAlpha PRIVATE /W4 /permissive-)
else()
  target_compile_options(AeylaVisualDmxAlpha PRIVATE -Wall -Wextra -Wpedantic)
endif()

smtg_target_configure_version_file(AeylaVisualDmxAlpha)

if(SMTG_MAC)
  smtg_target_set_bundle(AeylaVisualDmxAlpha
    BUNDLE_IDENTIFIER cl.rgbestudios.aeyla-visual-dmx-alpha
    COMPANY_NAME "RGB Estudios"
  )
elseif(SMTG_WIN AND MSVC)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT AeylaVisualDmxAlpha)
endif()

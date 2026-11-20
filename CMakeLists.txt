cmake_minimum_required(VERSION 3.22)
project(TENRINGS VERSION 1.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
  enable_language(OBJCXX)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt)
  add_subdirectory(JUCE)
elseif(DEFINED TENRINGS_JUCE_DIR)
  add_subdirectory(${TENRINGS_JUCE_DIR} juce_build)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Tenrings
  COMPANY_NAME "koobacorp"
  BUNDLE_ID "com.koobacorp.tenrings"
  PLUGIN_MANUFACTURER_CODE Koob
  PLUGIN_CODE TRng
  FORMATS VST3
  PRODUCT_NAME "TENRINGS"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  CAMERA_PERMISSION_ENABLED TRUE
  CAMERA_PERMISSION_TEXT "TENRINGS uses the camera to track your hands. Video never leaves your Mac."
  VST3_CATEGORIES Instrument Drum)

target_sources(Tenrings PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp)
if(APPLE)
  target_sources(Tenrings PRIVATE src/CameraMac.mm)
  set_source_files_properties(src/CameraMac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
  target_link_libraries(Tenrings PRIVATE "-framework AVFoundation" "-framework Vision" "-framework CoreMedia" "-framework CoreVideo")
else()
  target_sources(Tenrings PRIVATE src/CameraStub.cpp)
endif()
target_include_directories(Tenrings PUBLIC src)
target_compile_definitions(Tenrings PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_PLUGINHOST_VST3=0 JUCE_PLUGINHOST_AU=0 $<IF:$<PLATFORM_ID:Darwin>,JUCE_USE_CAMERA=1,JUCE_USE_CAMERA=0>)
target_link_libraries(Tenrings PRIVATE juce::juce_audio_processors juce::juce_audio_devices juce::juce_audio_utils juce::juce_gui_basics $<$<PLATFORM_ID:Darwin>:juce::juce_video>
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
target_compile_options(Tenrings PUBLIC $<$<CONFIG:Release>:-O3>)
target_compile_definitions(Tenrings PUBLIC $<$<CONFIG:Release>:NDEBUG=1>)
set_target_properties(Tenrings PROPERTIES VISIBILITY_INLINES_HIDDEN ON CXX_VISIBILITY_PRESET hidden)

enable_testing()
add_executable(tenrings_engine_tests tests/test_engine.cpp)
target_include_directories(tenrings_engine_tests PRIVATE src)
add_test(NAME engine_drum_keys_lead_rules COMMAND tenrings_engine_tests)

option(TENRINGS_BUILD_TOOLS "Build plugin tests" ON)
if(TENRINGS_BUILD_TOOLS)
  juce_add_console_app(tenrings_plugin_tests PRODUCT_NAME "tenrings_plugin_tests")
  target_sources(tenrings_plugin_tests PRIVATE tests/plugin_tests.cpp)
  target_include_directories(tenrings_plugin_tests PRIVATE src tests $<TARGET_PROPERTY:Tenrings,INCLUDE_DIRECTORIES>)
  target_compile_definitions(tenrings_plugin_tests PRIVATE $<TARGET_PROPERTY:Tenrings,COMPILE_DEFINITIONS>)
  target_link_libraries(tenrings_plugin_tests PRIVATE Tenrings juce::juce_recommended_config_flags)
  add_test(NAME plugin_every_control_midi_sound COMMAND tenrings_plugin_tests)
endif()

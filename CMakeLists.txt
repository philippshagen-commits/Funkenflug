cmake_minimum_required(VERSION 3.22)
project(Funkenflug VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # no VC++ redistributable needed
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/JUCE/CMakeLists.txt")
  add_subdirectory(JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Funkenflug
  COMPANY_NAME "Funkenflug"
  PLUGIN_MANUFACTURER_CODE Fkfl
  PLUGIN_CODE Fkf1
  FORMATS VST3 Standalone
  PRODUCT_NAME "Funkenflug"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  VST3_CATEGORIES Instrument
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Funkenflug PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/Renderer.cpp
  Source/PipeWriter.cpp
  Source/SoundFont.cpp
  Source/Sfz.cpp)

target_compile_definitions(Funkenflug PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_PLUGINHOST_VST3=1
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Funkenflug
  PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_opengl
  PUBLIC juce::juce_recommended_config_flags)

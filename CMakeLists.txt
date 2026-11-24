cmake_minimum_required(VERSION 3.22)
project(PurrrVoiceChannel VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Offline tools and tests (no JUCE needed)
add_executable(pvc_render tools/render.cpp)
target_include_directories(pvc_render PRIVATE include)

# Plugin. Pass -DPVC_JUCE_DIR=/path/to/JUCE to use a local JUCE, otherwise JUCE is fetched at build time.
option(PVC_BUILD_PLUGIN "Build the plugin" ON)
if(PVC_BUILD_PLUGIN)
  if(PVC_JUCE_DIR)
    add_subdirectory(${PVC_JUCE_DIR} juce)
  else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.16-security GIT_SHALLOW ON)
    FetchContent_MakeAvailable(juce)
  endif()

  juce_add_plugin(PurrrVoiceChannel
    COMPANY_NAME "Studio 87"
    BUNDLE_ID "com.studio87.purrrvoicechannel"
    PLUGIN_MANUFACTURER_CODE Spkc
    PLUGIN_CODE PvcH
    FORMATS VST3 Standalone
    PRODUCT_NAME "Purrr Voice Channel"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Dynamics EQ
    ICON_BIG assets/icon.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

  juce_add_binary_data(PvcAssets SOURCES assets/ui/faceplate.png assets/fonts/BarlowCondensed-SemiBold.ttf assets/fonts/BarlowCondensed-Bold.ttf assets/fonts/JetBrainsMono-Regular.ttf)
  target_sources(PurrrVoiceChannel PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp)
  target_include_directories(PurrrVoiceChannel PRIVATE include src)
  target_compile_definitions(PurrrVoiceChannel PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
  target_link_libraries(PurrrVoiceChannel PRIVATE PvcAssets juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

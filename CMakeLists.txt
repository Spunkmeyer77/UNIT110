cmake_minimum_required(VERSION 3.22)

# must come before project(), or the build targets the macOS version it was built on
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

project(UNIT110 VERSION 1.0.6)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout if -DJUCE_DIR=... is given, otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(T110_FORMATS AU VST3 Standalone)
elseif(WIN32)
    set(T110_FORMATS VST3 Standalone)
else()
    set(T110_FORMATS VST3 LV2 Standalone)
endif()

# Copy the built plug-ins into the user's plug-in folders (turned off on the build servers)
option(T110_COPY_PLUGINS "Copy plug-ins to the plug-in folders after building" ON)

juce_add_plugin(UNIT110
    COMPANY_NAME "Transistor Labs"
    PLUGIN_MANUFACTURER_CODE Trlb
    PLUGIN_CODE U110
    FORMATS ${T110_FORMATS}
    LV2URI "urn:transistorlabs:unit110"
    PRODUCT_NAME "UNIT 110"
    BUNDLE_ID com.transistorlabs.unit110
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Drum
    COPY_PLUGIN_AFTER_BUILD ${T110_COPY_PLUGINS})

juce_add_binary_data(UNIT110Assets SOURCES Assets/panel.png Assets/pcb.png Assets/knob.png)

target_sources(UNIT110 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MidiExport.h)

target_compile_definitions(UNIT110 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(UNIT110
    PRIVATE
        UNIT110Assets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional: headless test app (renders the sequencer, checks state and recording, snapshots the editor)
option(T110_BUILD_TESTS "Build the plugin test app" OFF)
if(T110_BUILD_TESTS)
    juce_add_console_app(UNIT110Test PRODUCT_NAME "UNIT110Test")
    target_sources(UNIT110Test PRIVATE tools/plugin_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(UNIT110Test PRIVATE
        JucePlugin_Name="UNIT 110" JucePlugin_VersionString="${PROJECT_VERSION}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 ${T110_TEST_DEFS})
    target_link_libraries(UNIT110Test PRIVATE UNIT110Assets juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()

# SPDX-License-Identifier: AGPL-3.0-or-later
set(GLINT_MANUFACTURER_CODE "Elmk" CACHE STRING "4-char AU manufacturer code")

juce_add_plugin(Glint
    COMPANY_NAME "electricmonk"
    COMPANY_WEBSITE "https://github.com/electricmonk/glint"
    BUNDLE_ID "io.github.electricmonk.glint"
    PLUGIN_MANUFACTURER_CODE ${GLINT_MANUFACTURER_CODE}
    PLUGIN_CODE Glnt
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Glint"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Reverb
    COPY_PLUGIN_AFTER_BUILD TRUE
    HARDENED_RUNTIME_ENABLED TRUE
)

target_sources(Glint PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    ParameterLayout.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Knob.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Controls.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/ParticleCloud.cpp
)
target_include_directories(Glint PRIVATE ${CMAKE_SOURCE_DIR}/src)

juce_add_binary_data(GlintBinaryData SOURCES
    ${CMAKE_SOURCE_DIR}/resources/fonts/space-grotesk-500.ttf
    ${CMAKE_SOURCE_DIR}/resources/fonts/space-grotesk-600.ttf
    ${CMAKE_SOURCE_DIR}/resources/fonts/inter-400.ttf
    ${CMAKE_SOURCE_DIR}/resources/fonts/inter-500.ttf
    ${CMAKE_SOURCE_DIR}/resources/fonts/inter-600.ttf
    ${CMAKE_SOURCE_DIR}/resources/fonts/jetbrains-mono-400.ttf
    ${CMAKE_SOURCE_DIR}/resources/fonts/jetbrains-mono-500.ttf
)
set_target_properties(GlintBinaryData PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(Glint PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(Glint
    PRIVATE
        glint_dsp
        GlintBinaryData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

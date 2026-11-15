# SPDX-License-Identifier: AGPL-3.0-or-later
set(GLINT_MANUFACTURER_CODE "Shyl" CACHE STRING "4-char AU manufacturer code")

juce_add_plugin(Glint
    COMPANY_NAME "Glint Audio"
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
)

target_compile_definitions(Glint PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(Glint
    PRIVATE
        glint_dsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

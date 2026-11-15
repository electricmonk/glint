# SPDX-License-Identifier: AGPL-3.0-or-later
include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(glint_tests
    test_engine.cpp
    test_grains.cpp
    test_reverb.cpp
    test_shimmer.cpp
    test_macros.cpp
)
target_link_libraries(glint_tests PRIVATE glint_dsp Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(glint_tests)

# Plugin-level tests (link the JUCE shared code of the plugin).
add_executable(glint_plugin_tests plugin_tests.cpp)
target_include_directories(glint_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(glint_plugin_tests PRIVATE Glint Catch2::Catch2 GlintBinaryData glint_dsp
    juce::juce_audio_utils juce::juce_recommended_config_flags)
target_compile_definitions(glint_plugin_tests PRIVATE JUCE_STANDALONE_APPLICATION=1)
catch_discover_tests(glint_plugin_tests TEST_SPEC "~[snapshot]")

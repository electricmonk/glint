// SPDX-License-Identifier: AGPL-3.0-or-later
// Host parameters: the 11 macros plus the Shimmer/Grains switches and grain
// Direction (added in state version 2; Direction became a 0..100% reverse
// share in version 3). IDs are frozen once they ship; bump kStateVersion
// instead of renaming.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Macros.h"

namespace glint::params {

constexpr int kStateVersion = 3;
constexpr int kParamVersionHint = 1;

namespace id {
inline constexpr const char* bloom = "macro.bloom";
inline constexpr const char* space = "macro.space";
inline constexpr const char* density = "macro.density";
inline constexpr const char* drift = "macro.drift";
inline constexpr const char* spread = "macro.spread";
inline constexpr const char* stereoDensity = "macro.stereoDensity";
inline constexpr const char* tone = "macro.tone";
inline constexpr const char* shimmer = "macro.shimmer";
inline constexpr const char* texture = "macro.texture";
inline constexpr const char* mix = "macro.mix";
inline constexpr const char* freeze = "macro.freeze";
// State version 2.
inline constexpr const char* shimmerOn = "macro.shimmerOn";
inline constexpr const char* grainsOn = "macro.grainsOn";
inline constexpr const char* direction = "macro.direction";
} // namespace id

// UI/host value text (matches docs/ui/UI-SPEC.md).
juce::String formatValue(const juce::String& paramId, float value);

inline const juce::StringArray& shimmerNames()
{
    static const juce::StringArray names { "Octave", "Fifth", "Oct + 5th", "Sub + Oct" };
    return names;
}
inline const juce::StringArray& textureNames()
{
    static const juce::StringArray names { "Clean", "Tape", "Dust" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Cached raw parameter pointers for the audio thread.
struct Pointers {
    std::atomic<float>* bloom = nullptr;
    std::atomic<float>* space = nullptr;
    std::atomic<float>* density = nullptr;
    std::atomic<float>* drift = nullptr;
    std::atomic<float>* spread = nullptr;
    std::atomic<float>* stereoDensity = nullptr;
    std::atomic<float>* tone = nullptr;
    std::atomic<float>* shimmer = nullptr;
    std::atomic<float>* texture = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* freeze = nullptr;
    std::atomic<float>* shimmerOn = nullptr;
    std::atomic<float>* grainsOn = nullptr;
    std::atomic<float>* direction = nullptr;

    void bind(juce::AudioProcessorValueTreeState& s);
    MacroValues read() const noexcept;
};

} // namespace glint::params

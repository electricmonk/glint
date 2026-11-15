// SPDX-License-Identifier: AGPL-3.0-or-later
// The 11 macros: the only host parameters. IDs are frozen once Phase 4 lands;
// bump kStateVersion instead of renaming.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Macros.h"

namespace glint::params {

constexpr int kStateVersion = 1;
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

    void bind(juce::AudioProcessorValueTreeState& s);
    MacroValues read() const noexcept;
};

} // namespace glint::params

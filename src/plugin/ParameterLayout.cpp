// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Parameters.h"

#include <cmath>

namespace glint::params {

juce::String formatValue(const juce::String& paramId, float v)
{
    const int r = juce::roundToInt(v);
    if (paramId == id::space) {
        const float s = 1.5f * std::pow(20.0f, v / 100.0f);
        return (s < 10.0f ? juce::String(s, 1) : juce::String(juce::roundToInt(s))) + " s tail";
    }
    if (paramId == id::stereoDensity)
        return r < 0 ? "centre " + juce::String(-r) : (r > 0 ? "edges " + juce::String(r) : juce::String("even"));
    if (paramId == id::tone)
        return r < 0 ? "dark " + juce::String(-r) : (r > 0 ? "bright " + juce::String(r) : juce::String("neutral"));
    if (paramId == id::bloom && v > 100.0f)
        return "bloom " + juce::String(r) + "%";
    return juce::String(r) + "%";
}

namespace {

std::unique_ptr<juce::AudioParameterFloat> percent(const char* pid, const char* name, float lo, float hi, float def)
{
    const juce::String idStr(pid);
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { idStr, kParamVersionHint }, name, juce::NormalisableRange<float>(lo, hi, 0.01f), def,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction([idStr](float v, int) { return formatValue(idStr, v); })
            .withValueFromStringFunction([](const juce::String& t) {
                const auto s = t.trim().toLowerCase();
                // Accept "dark 20"/"centre 30" (negative) as well as plain numbers.
                const bool neg = s.startsWith("dark") || s.startsWith("centre") || s.startsWith("center") || s.startsWith("-");
                const float x = static_cast<float>(s.retainCharacters("0123456789.").getDoubleValue());
                return neg ? -x : x;
            }));
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(percent(id::bloom, "Bloom", 0.0f, 110.0f, 60.0f));
    layout.add(percent(id::space, "Space", 0.0f, 100.0f, 65.0f));
    layout.add(percent(id::density, "Density", 0.0f, 100.0f, 45.0f));
    layout.add(percent(id::drift, "Drift", 0.0f, 100.0f, 35.0f));
    layout.add(percent(id::spread, "Spread", 0.0f, 100.0f, 70.0f));
    layout.add(percent(id::stereoDensity, "Stereo Density", -100.0f, 100.0f, 0.0f));
    layout.add(percent(id::tone, "Tone", -100.0f, 100.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { id::shimmer, kParamVersionHint }, "Shimmer",
                                                            shimmerNames(), 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { id::texture, kParamVersionHint }, "Texture",
                                                            textureNames(), 0));
    layout.add(percent(id::mix, "Mix", 0.0f, 100.0f, 35.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id::freeze, kParamVersionHint }, "Freeze", false));
    return layout;
}

void Pointers::bind(juce::AudioProcessorValueTreeState& s)
{
    bloom = s.getRawParameterValue(id::bloom);
    space = s.getRawParameterValue(id::space);
    density = s.getRawParameterValue(id::density);
    drift = s.getRawParameterValue(id::drift);
    spread = s.getRawParameterValue(id::spread);
    stereoDensity = s.getRawParameterValue(id::stereoDensity);
    tone = s.getRawParameterValue(id::tone);
    shimmer = s.getRawParameterValue(id::shimmer);
    texture = s.getRawParameterValue(id::texture);
    mix = s.getRawParameterValue(id::mix);
    freeze = s.getRawParameterValue(id::freeze);
}

MacroValues Pointers::read() const noexcept
{
    MacroValues m;
    m.bloom = bloom->load(std::memory_order_relaxed) * 0.01f;
    m.space = space->load(std::memory_order_relaxed) * 0.01f;
    m.density = density->load(std::memory_order_relaxed) * 0.01f;
    m.drift = drift->load(std::memory_order_relaxed) * 0.01f;
    m.spread = spread->load(std::memory_order_relaxed) * 0.01f;
    m.stereoDensity = stereoDensity->load(std::memory_order_relaxed) * 0.01f;
    m.tone = tone->load(std::memory_order_relaxed) * 0.01f;
    m.mix = mix->load(std::memory_order_relaxed) * 0.01f;
    m.shimmer = static_cast<ShimmerInterval>(juce::jlimit(0, 3, static_cast<int>(shimmer->load(std::memory_order_relaxed))));
    // UI textures are Clean / Tape / Dust; the engine also has Digital (unused).
    switch (static_cast<int>(texture->load(std::memory_order_relaxed))) {
    case 1: m.texture = Texture::Tape; break;
    case 2: m.texture = Texture::Dust; break;
    default: m.texture = Texture::Clean; break;
    }
    m.freeze = freeze->load(std::memory_order_relaxed) >= 0.5f;
    return m;
}

} // namespace glint::params

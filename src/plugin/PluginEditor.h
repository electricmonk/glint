// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class GlintAudioProcessor;

class GlintAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
    explicit GlintAudioProcessorEditor(GlintAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlintAudioProcessorEditor)
};

// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/Controls.h"
#include "ui/Theme.h"
#include "ui/Knob.h"
#include "ui/ParticleCloud.h"

class GlintAudioProcessor;

// Snaps the window to 75 / 100 / 125 / 150 % of 820 x 560.
class StepConstrainer final : public juce::ComponentBoundsConstrainer {
public:
    static constexpr int kBaseW = 820, kBaseH = 560;
    static constexpr float kSteps[4] = { 0.75f, 1.0f, 1.25f, 1.5f };
    StepConstrainer();
    void checkBounds(juce::Rectangle<int>& bounds, const juce::Rectangle<int>& previous, const juce::Rectangle<int>& limits,
                     bool top, bool left, bool bottom, bool right) override;
};

class GlintAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit GlintAudioProcessorEditor(GlintAudioProcessor&);
    ~GlintAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    class Content;
    void timerCallback() override;
    void setScale(float s);

    glint::ui::FontHandle fonts_;
    StepConstrainer constrainer_;
    std::unique_ptr<Content> content_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlintAudioProcessorEditor)
};

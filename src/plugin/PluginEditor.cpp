// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"
#include "PluginProcessor.h"

GlintAudioProcessorEditor::GlintAudioProcessorEditor(GlintAudioProcessor& p)
    : AudioProcessorEditor(&p)
{
    setSize(820, 560);
}

void GlintAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0e1014));
    g.setColour(juce::Colour(0xffeceae6));
    g.setFont(juce::FontOptions(28.0f));
    g.drawText("GLINT", getLocalBounds(), juce::Justification::centred);
}

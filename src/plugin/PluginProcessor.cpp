// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"

GlintAudioProcessor::GlintAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void GlintAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine_.prepare(sampleRate, samplesPerBlock);
}

void GlintAudioProcessor::releaseResources() {}

bool GlintAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo())
        return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void GlintAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();

    // Mono in -> duplicate to right before processing.
    if (numIn == 1 && buffer.getNumChannels() >= 2)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);

    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    engine_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
}

juce::AudioProcessorEditor* GlintAudioProcessor::createEditor()
{
    return new GlintAudioProcessorEditor(*this);
}

void GlintAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree state("GlintState");
    state.setProperty("version", 1, nullptr);
    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void GlintAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes)));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlintAudioProcessor();
}

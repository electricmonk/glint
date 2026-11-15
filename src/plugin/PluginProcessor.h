// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "dsp/GlintEngine.h"

class GlintAudioProcessor final : public juce::AudioProcessor {
public:
    GlintAudioProcessor();
    ~GlintAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram_.load(); }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return apvts_; }
    glint::GrainEventFifo& grainEvents() noexcept { return engine_.grainEvents(); }
    float loopLimiterGain() const noexcept { return limiterGain_.load(std::memory_order_relaxed); }
    int activeGrains() const noexcept { return activeGrains_.load(std::memory_order_relaxed); }

private:
    juce::AudioProcessorValueTreeState apvts_;
    glint::params::Pointers params_;
    glint::GlintEngine engine_;
    std::atomic<int> currentProgram_ { 0 };
    std::atomic<float> limiterGain_ { 1.0f };
    std::atomic<int> activeGrains_ { 0 };
    bool wasBypassed_ = false;
    juce::AudioBuffer<float> dryCopy_; // preallocated for the bypass fade-out

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlintAudioProcessor)
};

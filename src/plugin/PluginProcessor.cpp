// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "Presets.h"

using namespace glint;

GlintAudioProcessor::GlintAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts_(*this, nullptr, "GlintState", params::createLayout())
{
    params_.bind(apvts_);
}

void GlintAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine_.setMacros(params_.read());
    engine_.prepare(sampleRate, samplesPerBlock);
    dryCopy_.setSize(2, std::max(samplesPerBlock, 4096), false, true, false);
    wasBypassed_ = false;
}

void GlintAudioProcessor::releaseResources() {}

void GlintAudioProcessor::reset()
{
    engine_.setMacros(params_.read());
    engine_.reset(1);
}

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
    if (numSamples == 0 || buffer.getNumChannels() < 2)
        return;

    // Mono in -> both channels.
    if (numIn == 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);

    if (wasBypassed_) {
        // Coming back from bypass: fade the wet signal back in (no click).
        engine_.resumeAfterGap();
        wasBypassed_ = false;
    }

    engine_.setMacros(params_.read());
    engine_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);

    limiterGain_.store(engine_.loopLimiterGain(), std::memory_order_relaxed);
    activeGrains_.store(engine_.activeGrains(), std::memory_order_relaxed);
}

void GlintAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Dry pass-through (no latency, so nothing to compensate).
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() >= 2)
        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());

    const int n = buffer.getNumSamples();
    if (!wasBypassed_ && n > 0 && buffer.getNumChannels() >= 2) {
        // First bypassed block: keep processing and crossfade wet -> dry over
        // ~10 ms so the tail doesn't stop mid-waveform.
        const int len = std::min(n, dryCopy_.getNumSamples());
        for (int ch = 0; ch < 2; ++ch)
            dryCopy_.copyFrom(ch, 0, buffer, ch, 0, len);
        processBlock(buffer, midi);
        const int fade = std::min(len, std::max(1, static_cast<int>(getSampleRate() * 0.01)));
        for (int ch = 0; ch < 2; ++ch) {
            float* out = buffer.getWritePointer(ch);
            const float* dry = dryCopy_.getReadPointer(ch);
            for (int i = 0; i < len; ++i) {
                const float t = i < fade ? static_cast<float>(i + 1) / static_cast<float>(fade) : 1.0f;
                out[i] = out[i] + (dry[i] - out[i]) * t;
            }
        }
    }
    wasBypassed_ = true;
}

juce::AudioProcessorEditor* GlintAudioProcessor::createEditor()
{
    return new GlintAudioProcessorEditor(*this);
}

int GlintAudioProcessor::getNumPrograms()
{
    return static_cast<int>(presets::kFactory.size());
}

const juce::String GlintAudioProcessor::getProgramName(int index)
{
    if (index < 0 || index >= getNumPrograms())
        return {};
    return presets::kFactory[static_cast<size_t>(index)].name;
}

void GlintAudioProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram_.store(index);
    const auto& p = presets::kFactory[static_cast<size_t>(index)];
    auto set = [this](const char* pid, float plain) {
        if (auto* prm = apvts_.getParameter(pid)) {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost(prm->convertTo0to1(plain));
            prm->endChangeGesture();
        }
    };
    set(params::id::bloom, p.bloom);
    set(params::id::space, p.space);
    set(params::id::density, p.density);
    set(params::id::drift, p.drift);
    set(params::id::spread, p.spread);
    set(params::id::stereoDensity, p.stereoDensity);
    set(params::id::tone, p.tone);
    set(params::id::mix, p.mix);
    set(params::id::shimmer, static_cast<float>(p.shimmer));
    set(params::id::texture, static_cast<float>(p.texture));
}

void GlintAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto tree = apvts_.copyState();
    tree.setProperty("stateVersion", params::kStateVersion, nullptr);
    tree.setProperty("program", currentProgram_.load(), nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, destData);
}

void GlintAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(apvts_.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml(*xml);
    // Future versions migrate here, keyed on stateVersion. Version 1 is current.
    const int version = tree.getProperty("stateVersion", 1);
    juce::ignoreUnused(version);
    currentProgram_.store(juce::jlimit(0, getNumPrograms() - 1, static_cast<int>(tree.getProperty("program", 0))));
    tree.removeProperty("stateVersion", nullptr);
    tree.removeProperty("program", nullptr);
    apvts_.replaceState(tree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlintAudioProcessor();
}

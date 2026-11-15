// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ParticleCloud.h"

#include <algorithm>
#include <cmath>

#include "Theme.h"

namespace glint::ui {

ParticleCloud::ParticleCloud(GrainEventFifo& fifo)
    : fifo_(fifo)
{
    pending_.reserve(64);
    frame_.reserve(64);
    particles_.reserve(160);
    setTitle("Grain cloud");
    setDescription("Particles drift upward, placed left to right by stereo position");
    setAccessible(true);
    setInterceptsMouseClicks(false, false);
    startTimerHz(60);
}

ParticleCloud::~ParticleCloud()
{
    stopTimer();
}

void ParticleCloud::timerCallback()
{
    GrainEvent e;
    while (fifo_.pop(e)) {
        if (e.level < 0.0f) {
            frame_.swap(pending_);
            pending_.clear();
            // Spawn particles for grains we haven't seen yet.
            for (const auto& g : frame_) {
                if (g.id + 100000u < lastId_)
                    lastId_ = 0; // engine was reset
                if (g.id > lastId_ && particles_.size() < 160) {
                    const float seed = static_cast<float>((g.id * 2654435761u) % 1000u) / 1000.0f;
                    particles_.push_back({ g.pan, g.level, 0.0f, 0.8f + 0.4f * seed, seed, g.shimmer });
                }
            }
            for (const auto& g : frame_)
                lastId_ = std::max(lastId_, g.id);
        } else if (pending_.size() < 64) {
            pending_.push_back(e);
        }
    }
    // Rise over ~3-4 s (slower when frozen).
    const float dt = (state_.frozen ? 0.5f : 1.0f) / (60.0f * (2.6f + state_.space * 1.4f));
    for (auto& p : particles_)
        p.age += dt * p.speed;
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.age >= 1.0f; }),
                     particles_.end());
    t_ += 1.0;
    repaint();
}

void ParticleCloud::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float W = bounds.getWidth(), H = bounds.getHeight();

    g.setColour(colour::well);
    g.fillRoundedRectangle(bounds, 12.0f);

    // Centre line (dashed).
    {
        juce::Path p;
        p.startNewSubPath(W * 0.5f, 10.0f);
        p.lineTo(W * 0.5f, H - 10.0f);
        juce::Path dashed;
        const float dashes[] = { 2.0f, 5.0f };
        juce::PathStrokeType(1.0f).createDashedStroke(dashed, p, dashes, 2);
        g.setColour(colour::centreLine);
        g.fillPath(dashed);
    }

    // Horizon brightens with Bloom.
    const float bloomN = juce::jlimit(0.0f, 1.0f, state_.bloom / 1.1f);
    g.setColour(colour::gold.withAlpha(0.08f + bloomN * 0.5f));
    g.fillRect(0.0f, H - 12.5f, W, 1.0f);

    // Particles.
    const float marginX = 28.0f;
    for (size_t i = 0; i < particles_.size(); ++i) {
        const auto& p = particles_[i];
        const float wobble = std::sin(static_cast<float>(t_) * 0.04f + p.seed * 40.0f) * state_.drift * 16.0f;
        const float x = W * 0.5f + p.pan * (W * 0.5f - marginX) + wobble;
        const float y = H - 16.0f - p.age * (H - 32.0f);
        const float lvl = juce::jlimit(0.0f, 1.0f, 0.25f + 0.75f * p.seed + p.level);
        const float r = 1.6f + 2.6f * lvl + state_.space * 1.6f;
        const float o = std::sin(juce::MathConstants<float>::pi * p.age) * (0.35f + 0.6f * lvl);
        const auto col = p.shimmer ? colour::gold : colour::ice;
        // Soft dot: halo + core.
        g.setColour(col.withAlpha(o * 0.25f));
        g.fillEllipse(x - r * 2.0f, y - r * 2.0f, r * 4.0f, r * 4.0f);
        g.setColour(col.withAlpha(o));
        g.fillEllipse(x - r, y - r, r * 2.0f, r * 2.0f);
    }

    // L / R and status.
    g.setFont(font(FontFace::Inter400, 11.0f));
    g.setColour(colour::textDim);
    g.drawText("L", juce::Rectangle<float>(12.0f, H - 24.0f, 20.0f, 14.0f), juce::Justification::centredLeft, false);
    g.drawText("R", juce::Rectangle<float>(W - 32.0f, H - 24.0f, 20.0f, 14.0f), juce::Justification::centredRight, false);

    juce::String status;
    juce::Colour statusColour = colour::gold;
    if (state_.frozen) {
        status = "FROZEN";
        statusColour = colour::ice;
    } else if (state_.bloom > 1.0f) {
        status = "BLOOMING";
    }
    if (status.isNotEmpty()) {
        g.setColour(statusColour);
        g.setFont(font(FontFace::Inter400, 11.0f).withExtraKerningFactor(0.14f));
        g.drawText(status, juce::Rectangle<float>(12.0f, 10.0f, 200.0f, 14.0f), juce::Justification::centredLeft, false);
    }

    g.setColour(colour::hairline);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);
}

} // namespace glint::ui

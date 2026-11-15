// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/EventFifo.h"

namespace glint::ui {

// Grains drawn as soft dots rising from bottom to top over their lifetime;
// x = stereo pan. Fed from the audio thread through a lock-free FIFO.
class ParticleCloud : public juce::Component, private juce::Timer {
public:
    struct State {
        float bloom = 0.6f;  // 0..1.1
        float drift = 0.35f; // 0..1
        float space = 0.65f; // 0..1
        bool frozen = false;
    };

    explicit ParticleCloud(GrainEventFifo& fifo);
    ~ParticleCloud() override;
    void setState(const State& s) { state_ = s; }
    void paint(juce::Graphics&) override;
    // Drain the FIFO now (also called at 60 Hz by the timer).
    void poll() { timerCallback(); }

private:
    void timerCallback() override;

    // One visual particle per grain: it rises over a few seconds (longer than
    // the grain itself) so the cloud reads as a field, not a flicker.
    struct Particle {
        float pan, level, age, speed, seed;
        bool shimmer;
    };

    GrainEventFifo& fifo_;
    std::vector<GrainEvent> pending_, frame_;
    std::vector<Particle> particles_;
    uint32_t lastId_ = 0;
    State state_;
    double t_ = 0.0;
};

} // namespace glint::ui

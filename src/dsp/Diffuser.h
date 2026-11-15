// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>

#include "dsp/DelayLine.h"

namespace glint {

// Pre-delay + 4 series Schroeder allpasses per channel.
class Diffuser {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    // diffusion 0..1, preDelaySamples smoothed by the caller.
    void process(float& l, float& r, float diffusion, float preDelaySamples) noexcept;

private:
    static constexpr int kStages = 4;
    DelayLine preL_, preR_;
    std::array<DelayLine, kStages> apL_, apR_;
    std::array<int, kStages> lenL_ {}, lenR_ {};
};

} // namespace glint

// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>

#include "dsp/DelayLine.h"
#include "dsp/Modulation.h"
#include "dsp/Smoothers.h"

namespace glint {

struct FdnSettings {
    float size = 0.7f;        // 0..1
    float decayS = 8.0f;      // RT60 at 1 kHz; >= 60 is "infinite"
    float dampingHz = 9000.0f;
    float modDepth = 0.3f;    // 0..1
    float modRateHz = 0.4f;
    float width = 1.0f;       // 0..2
};

// 8-line feedback delay network with a Hadamard matrix, per-line one-pole
// damping, slow LFO + random-drift modulation on every line.
// The input is scaled so the steady-state energy gain is about 1 whatever the
// decay, which keeps the shimmer loop around it stable. makeupGain() gives
// the output gain to apply outside the loop.
class Fdn {
public:
    static constexpr int kLines = 8;

    void prepare(double sampleRate);
    void reset() noexcept;
    // Call at control rate (every few samples); targets are smoothed inside.
    void setSettings(const FdnSettings& s) noexcept;
    void process(float inL, float inR, float& outL, float& outR) noexcept;
    float makeupGain() const noexcept { return makeup_; }
    // Scale for signals fed back from the output to the input, so that the
    // loop sees roughly unit energy gain whatever the decay.
    float loopNorm() const noexcept { return loopNorm_; }
    // Extra scale on the recirculation gains (Bloom limiter), <= 1.
    void setGainScale(float s) noexcept { gainScale_ = s; }

private:
    void updateCoefficients() noexcept;

    double sampleRate_ = 48000.0;
    FdnSettings settings_;
    std::array<DelayLine, kLines> lines_;
    std::array<OnePoleLP, kLines> damp_;
    std::array<float, kLines> baseLen_ {};  // samples at size 1
    std::array<float, kLines> gain_ {};
    std::array<float, kLines> inGain_ {};
    std::array<float, kLines> state_ {};
    ModBank<kLines> mod_;
    Smoother sizeSmooth_;
    float dampCoeff_ = 0.5f;
    float makeup_ = 1.0f;
    float loopNorm_ = 1.0f;
    float gainScale_ = 1.0f;
    float modDepthSamples_ = 0.0f;
    int controlCounter_ = 0;
};

} // namespace glint

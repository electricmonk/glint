// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include "dsp/SpectralShifter.h"
#include "dsp/Smoothers.h"

namespace glint {

struct ShimmerSettings {
    float feedback = 0.6f;     // 0..1.1 (engine Feedback)
    float blend = 0.7f;        // 0..1 shifted vs unshifted inside the loop
    float shiftA = 12.0f, shiftB = 7.0f, shiftBLevel = 0.0f;
    float highCutHz = 8000.0f, lowCutHz = 120.0f;
};

// Perceptual loop gain: sustain 1/(1-g) grows exponentially with feedback,
// continuous through 100%, where the soft clipper and limiter take over.
float loopGainForFeedback(float feedback) noexcept;

// Takes the reverb output, pitch-shifts, filters, soft-clips and returns what
// is fed back. Holds the loop near -6 dBFS above 100% (Bloom limiter) and
// resets on non-finite samples.
class ShimmerLoop {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void setSettings(const ShimmerSettings& s) noexcept;
    // reverbL/R: reverb output this sample. Returns the loop signal to feed
    // back (before routing).
    void process(float reverbL, float reverbR, float& retL, float& retR) noexcept;

    // Peak of the audible wet signal (before the limiter gain), for the limiter.
    void setDetectLevel(float peak) noexcept { detectLevel_ = peak; }
    uint64_t nonFiniteCount() const noexcept { return nonFinite_; }
    float limiterGain() const noexcept { return limGain_; }

private:
    double sampleRate_ = 48000.0;
    ShimmerSettings settings_;
    SpectralShifter shL_, shR_;
    Svf hcL_, hcR_, lcL_, lcR_;
    DcBlocker dcL_, dcR_;
    Smoother gain_, blend_;
    float env_ = 0.0f;
    float limGain_ = 1.0f;
    float detectLevel_ = 0.0f;
    float envAttack_ = 0.0f, envRelease_ = 0.0f, limCoeff_ = 0.0f, limRelease_ = 0.0f;
    int filterCounter_ = 0;
    uint64_t nonFinite_ = 0;
};

} // namespace glint

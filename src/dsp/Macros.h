// SPDX-License-Identifier: AGPL-3.0-or-later
// All macro -> engine curves live here (one place to tune).
#pragma once

#include <algorithm>
#include <cmath>

#include "dsp/Params.h"

namespace glint {

enum class ShimmerInterval { Octave = 0, Fifth = 1, OctFifth = 2, SubOct = 3 };

// The host-visible controls (11 macros + 3 switches). Continuous ones are smoothed by GlintEngine
// before map() runs; percentages are 0..1 (Bloom 0..1.1, bipolar -1..1).
struct MacroValues {
    float bloom = 0.60f;
    float space = 0.65f;
    float density = 0.45f;
    float drift = 0.35f;
    float spread = 0.70f;
    float stereoDensity = 0.0f;
    float tone = 0.0f;
    ShimmerInterval shimmer = ShimmerInterval::Octave;
    Texture texture = Texture::Clean;
    float mix = 0.35f;
    bool freeze = false;
    bool shimmerOn = true;
    bool grainsOn = true;
    float reverse = 0.0f; // Direction: share of grains played backwards, 0..1
};

namespace macros {

inline float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }
inline float expLerp(float a, float b, float t) noexcept { return a * std::pow(b / a, t); }
inline float smoothstep(float e0, float e1, float x) noexcept
{
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Fixed (no control) values.
constexpr float kGrainTime = 0.10f;       // ~1.6 s back in a 16 s buffer
constexpr float kGrainShape = 0.62f;      // smooth, bell-like
constexpr float kLoopLowCutHz = 120.0f;
constexpr float kModRateHz = 0.4f;
constexpr float kGrainsLevelDb = -3.0f;   // grains heard directly in the wet signal
constexpr float kMaxGrainRateHz = 40.0f;

// Range of Bloom, Space and Drift: the full knob covers what 0..65% of the
// original curves did (above that the tail and wobble were too much to use).
// Bloom's 100..110% zone still runs on to infinite hold.
constexpr float kRangeScale = 0.65f;
inline float bloomDepth(float knob) noexcept
{
    const float b = std::clamp(knob, 0.0f, 1.1f);
    return b <= 1.0f ? b * kRangeScale : kRangeScale + (b - 1.0f) / 0.1f * (1.1f - kRangeScale);
}
inline float spaceDepth(float knob) noexcept { return std::clamp(knob, 0.0f, 1.0f) * kRangeScale; }
inline float driftDepth(float knob) noexcept { return std::clamp(knob, 0.0f, 1.0f) * kRangeScale; }
// Reverb decay for a Space setting before Bloom's stretch (also the UI text).
inline float spaceDecayS(float knob) noexcept { return 1.5f * std::pow(20.0f, spaceDepth(knob)); }

inline EngineParams map(const MacroValues& m) noexcept
{
    EngineParams p;

    // Bloom (scaled by bloomDepth): Feedback (perceptual curve lives in ShimmerLoop), shimmer blend
    // 40->90%, Decay x1->x2, Serial -> Grains-in-loop from 70% up.
    const float b = bloomDepth(m.bloom);
    p.feedback = b;
    p.shimmerBlend = lerp(0.4f, 0.9f, b / 1.1f);
    const float decayMul = 1.0f + b / 1.1f;
    p.routing = Routing::GrainsInLoop;
    p.grainsInLoop = smoothstep(0.7f, 1.0f, b) * 0.5f;

    // Space (scaled by spaceDepth; curves below are over 0..1 of depth): size 30->100%,
    // decay 1.5->30 s (log; 1.5->10.5 s over the knob), diffusion 60->95%, pre-delay 10->80 ms.
    const float s = spaceDepth(m.space);
    p.reverbSize = lerp(0.3f, 1.0f, s);
    p.decayS = std::min(spaceDecayS(m.space) * decayMul, 59.0f);
    if (b > 1.0f) // Bloom: decay glides to infinite at 110%; the limiter holds the level.
        p.decayS = expLerp(p.decayS, EngineParams::kInfiniteDecay, std::min(1.0f, (b - 1.0f) / 0.1f));
    p.diffusion = lerp(0.6f, 0.95f, s);
    p.preDelayMs = lerp(10.0f, 80.0f, s);

    // Density: random grain rate 0->40/s, size 900->150 ms.
    const float d = std::clamp(m.density, 0.0f, 1.0f);
    p.grainRateHz = kMaxGrainRateHz * d * d * (0.4f + 0.6f * d) / 1.0f;
    p.grainRateHz = std::max(p.grainRateHz, 0.0f);
    p.sizeMs = expLerp(900.0f, 150.0f, d);
    p.grainDensity = 1.0f; // fully random spawn times
    p.shape = kGrainShape;
    // With no grains the input still has to reach the reverb.
    p.directToReverb = lerp(1.0f, 0.35f, smoothstep(0.0f, 0.5f, d));

    // Drift (scaled by driftDepth): random time 10->70%, pitch jitter 0->0.3 st, mod depth 15->70%.
    const float r = driftDepth(m.drift);
    p.randTime = lerp(0.1f, 0.7f, r);
    p.pitchJitterSt = 0.3f * r;
    p.modDepth = lerp(0.15f, 0.7f, r);
    p.modRateHz = kModRateHz;

    // Spread: grain pan width; reverb width 80->150%.
    p.spread = std::clamp(m.spread, 0.0f, 1.0f);
    p.width = lerp(0.8f, 1.5f, p.spread);
    p.stereoDensity = std::clamp(m.stereoDensity, -1.0f, 1.0f);

    // Tone: loop high cut 2->9 kHz, damping 3->12 kHz (neutral sits slightly darker than the dry signal).
    const float t = (std::clamp(m.tone, -1.0f, 1.0f) + 1.0f) * 0.5f;
    p.loopHighCutHz = expLerp(2000.0f, 9000.0f, t);
    p.dampingHz = expLerp(3000.0f, 12000.0f, t);
    p.loopLowCutHz = kLoopLowCutHz;

    // Shimmer interval.
    switch (m.shimmer) {
    case ShimmerInterval::Octave: p.shiftA = 12.0f; p.shiftB = 12.0f; p.shiftBLevel = 0.0f; break;
    case ShimmerInterval::Fifth: p.shiftA = 7.0f; p.shiftB = 7.0f; p.shiftBLevel = 0.0f; break;
    case ShimmerInterval::OctFifth: p.shiftA = 12.0f; p.shiftB = 19.0f; p.shiftBLevel = 0.6f; break;
    case ShimmerInterval::SubOct: p.shiftA = -12.0f; p.shiftB = 12.0f; p.shiftBLevel = 0.8f; break;
    }

    // Direction: share of grains played backwards (0 forward .. 1 reverse).
    p.reverse = std::clamp(m.reverse, 0.0f, 1.0f);
    p.shimmerOn = m.shimmerOn;
    p.grainsOn = m.grainsOn;

    p.color = m.texture;
    p.mix = std::clamp(m.mix, 0.0f, 1.0f);
    p.freeze = m.freeze;

    // Fixed.
    p.time = kGrainTime;
    p.pitchSt = 0.0f;
    p.grainFeedback = 0.0f;
    p.outputGainDb = 0.0f;
    p.grainsLevelDb = kGrainsLevelDb;
    p.reverbLevelDb = 0.0f;
    p.inputGainDb = 0.0f;
    return p;
}

} // namespace macros
} // namespace glint

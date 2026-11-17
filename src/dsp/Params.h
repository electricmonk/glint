// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include "dsp/Color.h"

namespace glint {

enum class Routing { Serial = 0, Parallel = 1, GrainsInLoop = 2 };

// Plain struct of engine target values (PLAN section 5b). Percentages are
// stored as 0..1 fractions (Feedback 0..1.1, bipolar ones -1..1).
struct EngineParams {
    // Input
    float inputGainDb = 0.0f;
    // Loop
    float feedback = 0.6f;          // 0..1.1
    // Grains
    float grainDensity = 0.3f;      // -1 regular .. +1 random (randomness)
    float grainRateHz = 10.0f;      // mean rate
    float spread = 0.6f;            // 0..1
    float stereoDensity = 0.0f;     // -1..1
    float time = 0.3f;              // 0..1 of buffer
    float sizeMs = 250.0f;          // 10..4000, >= 4000 infinite
    float pitchSt = 0.0f;
    float shape = 0.5f;
    float randTime = 0.2f, randSize = 0.0f, randPitch = 0.0f;
    float pitchJitterSt = 0.0f;
    float reverse = 0.0f;           // probability a grain plays backwards
    float grainFeedback = 0.0f;     // 0..0.95
    bool freeze = false;
    bool grainsOn = true;           // off: no new grains, grain output faded out, input straight to the reverb
    Texture color = Texture::Clean;
    float directToReverb = 0.5f;    // how much of the input bypasses the grains into the reverb
    // Shimmer
    bool shimmerOn = true;          // off: the whole reverb + shimmer block is faded out (grains only)
    float shimmerBlend = 0.7f;      // 0..1
    float shiftA = 12.0f, shiftB = 7.0f, shiftBLevel = 0.0f;
    float loopHighCutHz = 8000.0f, loopLowCutHz = 120.0f;
    // Reverb
    float reverbSize = 0.7f;        // 0..1
    float decayS = 8.0f;            // 0.3..60, >= kInfiniteDecay infinite
    float preDelayMs = 20.0f;
    float diffusion = 0.8f;
    float dampingHz = 9000.0f;
    float modDepth = 0.3f, modRateHz = 0.4f;
    // Routing
    Routing routing = Routing::Serial;
    float grainsInLoop = 0.0f;      // 0..1 crossfade Serial -> Grains-in-loop (Bloom)
    // Output
    float grainsLevelDb = 0.0f, reverbLevelDb = 0.0f;
    float mix = 0.35f, width = 1.0f, outputGainDb = 0.0f;

    static constexpr float kInfiniteDecay = 60.0f;

    // Name-based setter used by glint-render JSON. Returns false if unknown.
    bool set(std::string_view name, double value) noexcept;
    bool setString(std::string_view name, std::string_view value) noexcept;
};

} // namespace glint

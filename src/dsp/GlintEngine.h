// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "dsp/Diffuser.h"
#include "dsp/EventFifo.h"
#include "dsp/Fdn.h"
#include "dsp/GrainEngine.h"
#include "dsp/Macros.h"
#include "dsp/Params.h"
#include "dsp/ShimmerLoop.h"
#include "dsp/Smoothers.h"

namespace glint {

// Owns everything. Signal flow (PLAN section 4):
//   input -> grain engine -> diffusion -> FDN -> output mix
//                 ^                        |
//                 +-- loop return <- shimmer loop (pitch shift, filters, clip)
class GlintEngine {
public:
    static constexpr int kControlBlock = 16;

    void prepare(double sampleRate, int maxBlockSize);
    void reset(uint32_t seed = 1) noexcept;

    // Plugin path: macro targets (smoothed per sample inside).
    void setMacros(const MacroValues& m) noexcept;
    // Render/test path: engine parameters directly (bypasses the macros).
    void setEngineParams(const EngineParams& p) noexcept;

    // Fade the wet signal in from silence over ~20 ms (e.g. after bypass).
    void rampInWet() noexcept { wetGate_.snap(0.0f); wetGate_.setTarget(1.0f); }
    // After a gap in processing (e.g. bypass): drop stale tails and recording
    // without large memory clears, then fade the wet signal back in.
    void resumeAfterGap() noexcept;

    // In-place stereo processing. Any block size; no allocation or locks.
    void process(float* left, float* right, int numSamples) noexcept;

    double sampleRate() const noexcept { return sampleRate_; }
    GrainEventFifo& grainEvents() noexcept { return events_; }
    const EngineParams& currentParams() const noexcept { return current_; }

    // Diagnostics.
    uint64_t nonFiniteCount() const noexcept { return nonFinite_ + shimmer_.nonFiniteCount(); }
    int activeGrains() const noexcept { return grains_.activeGrains(); }
    float loopLimiterGain() const noexcept { return shimmer_.limiterGain(); }
    float bloomAmount() const noexcept { return macroSmooth_[0].value(); }
    bool frozen() const noexcept { return current_.freeze; }

private:
    void applyParams(const EngineParams& p) noexcept;
    void tickMacros(int n) noexcept;

    double sampleRate_ = 48000.0;
    GrainEngine grains_;
    Diffuser diffuser_;
    Fdn fdn_;
    ShimmerLoop shimmer_;
    GrainEventFifo events_;

    bool useMacros_ = true;
    MacroValues macros_;
    std::array<Smoother, 8> macroSmooth_; // bloom space density drift spread stereo tone mix
    EngineParams current_;

    Smoother wetGate_;
    Smoother mix_, preDelay_, direct_, gil_, grainsLevel_, reverbLevel_, inGain_, outGain_;

    std::array<float, kControlBlock> writeL_ {}, writeR_ {}, grainL_ {}, grainR_ {}, retL_ {}, retR_ {};
    float loopL_ = 0.0f, loopR_ = 0.0f;
    uint64_t nonFinite_ = 0;
    uint32_t seed_ = 1;
};

} // namespace glint

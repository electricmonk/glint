// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <vector>

#include "dsp/Color.h"
#include "dsp/EventFifo.h"
#include "dsp/Rng.h"
#include "dsp/Smoothers.h"
#include "dsp/beads_port/Grain.h"

namespace glint {

struct GrainParams {
    float time = 0.3f;          // 0..1 of the recorded buffer
    float sizeMs = 250.0f;      // 10..4000; >= kInfiniteSizeMs means infinite (delay mode)
    float pitchSt = 0.0f;       // +-24
    float shape = 0.5f;         // 0..1 (square .. decay .. bell .. reverse)
    float densityHz = 10.0f;    // mean grain rate, 0..~130
    float randomness = 0.3f;    // 0 regular pulse train .. 1 Poisson
    float randTime = 0.2f;      // 0..1
    float randSize = 0.0f;      // 0..1
    float randPitch = 0.0f;     // 0..1 (scales +-12 st)
    float pitchJitterSt = 0.0f; // extra +- semitone jitter per grain
    float reverse = 0.0f;       // probability 0..1
    float spread = 0.6f;        // 0..1 pan width
    float stereoDensity = 0.0f; // -1 centre .. +1 edges
    bool freeze = false;
    Texture texture = Texture::Clean;

    static constexpr float kInfiniteSizeMs = 4000.0f;
};

// Beads-style granular engine: a continuously recording stereo buffer that
// grains read from. Allocation happens in prepare() only.
class GrainEngine {
public:
    static constexpr int kMaxGrains = 32;

    void prepare(double sampleRate, double maxBufferSeconds = 16.0);
    void reset(uint32_t seed = 1) noexcept;
    void setParams(const GrainParams& p) noexcept;
    // Forget the recording without clearing memory (cheap on the audio thread):
    // new grains only read what is recorded from now on.
    void softReset() noexcept;

    // Fraction (0..1) of the recorded signal that comes from the shimmer loop;
    // only used to colour particles in the visualiser.
    void setShimmerShare(float s) noexcept { shimmerShare_ = s; }
    void setEventFifo(GrainEventFifo* fifo) noexcept { fifo_ = fifo; }

    // Records writeL/R into the buffer and adds grain output into outL/R.
    void process(const float* writeL, const float* writeR, float* outL, float* outR, int n) noexcept;

    int activeGrains() const noexcept { return pool_.countActive(); }
    uint64_t grainsStarted() const noexcept { return grainsStarted_; }

    // Buffer interface used by beads_port::Grain.
    struct Buffer {
        std::vector<float> l, r;
        int64_t capacity_ = 0;
        int64_t write = 0;      // index of the most recent sample
        int64_t recorded = 0;   // samples recorded so far (<= capacity)
        bool frozen_ = false;
        int64_t capacity() const noexcept { return capacity_; }
        bool frozen() const noexcept { return frozen_; }
        // delay in samples behind the most recent sample, Hermite interpolated.
        void read(double delay, float& outL, float& outR) const noexcept;
    };

private:
    void spawnGrain(float fractional) noexcept;
    float shapePan(float u) const noexcept;
    void writeSample(float l, float r) noexcept;
    void pushEvents() noexcept;

    double sampleRate_ = 48000.0;
    Buffer buffer_;
    beads_port::GrainPool<kMaxGrains> pool_;
    Color color_;
    Rng rng_ { 1 };
    GrainParams params_;
    Texture pendingTexture_ = Texture::Clean;

    Smoother rate_;   // grains per second, ~50 ms
    Smoother spread_; // ~30-50 ms glide for playing grains
    double phase_ = 0.0;
    float intervalScale_ = 1.0f;

    // Freeze tail (Beads): what arrived while frozen is crossfaded back in.
    std::vector<float> tailL_, tailR_;
    int tailSize_ = 256;
    int tailWrite_ = 0;
    int tailXfade_ = 0;
    bool wasFrozen_ = false;

    float shimmerShare_ = 0.0f;
    GrainEventFifo* fifo_ = nullptr;
    int eventCounter_ = 0;
    int eventInterval_ = 800;
    uint64_t grainsStarted_ = 0;
};

} // namespace glint

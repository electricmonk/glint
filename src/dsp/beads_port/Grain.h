// SPDX-License-Identifier: MIT
// Copyright 2017 Emilie Gillet.
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Adapted for Glint from beads/dsp/grain.h, beads/dsp/delay_grain.h and
// beads/dsp/granular_player.h:
//  - read positions are double-precision sample offsets instead of Q22.10
//    int32 (which overflows for long buffers at high sample rates);
//  - the buffer is any type exposing capacity(), frozen() and
//    read(double delay, float& l, float& r);
//  - the head-crossing safety zone and crossfade times are in seconds and
//    converted with the host sample rate;
//  - pan gains are recomputed per sample from a unit pan position so that the
//    host can glide the stereo spread of grains that are already playing;
//  - std::partial_sort / std::rotate scheduling kept as in the original.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "dsp/beads_port/GrainEnvelope.h"

namespace glint::beads_port {

class Grain {
public:
    void init(float sampleRate) noexcept
    {
        envelope_.init(sampleRate);
        stealing_.resetAndPause();
        remaining_ = 0;
        delay_ = 0.0;
        speed_ = 1.0;
        safetyZone_ = std::max(16.0, 0.005 * sampleRate);
    }

    template <class Buffer>
    void start(const Buffer& buffer, double delay, double speed, float duration, float shape,
               float gain, float panUnit, float fractionalSample, bool shimmer) noexcept
    {
        const double capacity = static_cast<double>(buffer.capacity());
        const double consumption = speed - (buffer.frozen() ? 0.0 : 1.0);
        delay -= fractionalSample * consumption;

        if (consumption != 0.0)
            duration = static_cast<float>(std::min<double>(duration, capacity / std::fabs(consumption)));
        if (consumption > 0.0)
            delay = std::max(delay, static_cast<double>(duration) * consumption);
        else if (consumption < 0.0)
            delay = std::min(delay, capacity + static_cast<double>(duration) * consumption);

        delay_ = delay;
        speed_ = speed;
        gain_ = gain;
        panUnit_ = panUnit;
        duration_ = std::max(duration, 1.0f);
        age_ = 0;
        shimmer_ = shimmer;
        envelope_.start(duration, shape, fractionalSample);
        remaining_ = static_cast<int>(duration);
        stealing_.resetAndPause();
    }

    // Adds the grain's contribution. panWidth scales the unit pan position.
    template <class Buffer>
    void render(const Buffer& buffer, float panWidth, float& outL, float& outR) noexcept
    {
        if (done())
            return;

        delay_ -= speed_;
        if (!buffer.frozen())
            delay_ += 1.0;

        const double tail = static_cast<double>(buffer.capacity());
        if (delay_ < 0.0 || delay_ >= tail) {
            delay_ = 0.0;
            envelope_.stop();
            remaining_ = 0;
            return;
        }

        // Fade near the write head and near the end of the buffer.
        const double d = std::min(delay_, tail - delay_);
        const float fadeRamp = d < safetyZone_ ? static_cast<float>(d / safetyZone_) : 1.0f;

        float l, r;
        buffer.read(delay_, l, r);
        const float env = envelope_.render() * stealing_.render() * smoothStep(fadeRamp);
        lastEnv_ = env;
        l *= env;
        r *= env;

        // Roughly equal power, first-order approximation (as in Beads).
        float pan = 0.5f + 0.5f * std::clamp(panUnit_ * panWidth, -1.0f, 1.0f);
        float g00, g01, g10, g11;
        if (pan < 0.5f) {
            pan *= 2.0f;
            g00 = gain_;
            g10 = 0.0f;
            g11 = pan * (2.0f - pan) * gain_;
            g01 = (1.0f - pan) * (1.0f + pan) * gain_;
        } else {
            pan = (1.0f - pan) * 2.0f;
            g11 = gain_;
            g01 = 0.0f;
            g00 = pan * (2.0f - pan) * gain_;
            g10 = (1.0f - pan) * (1.0f + pan) * gain_;
        }
        outL += l * g00 + r * g01;
        outR += l * g10 + r * g11;

        ++age_;
        --remaining_;
        if (done())
            remaining_ = 0;
    }

    bool done() const noexcept { return envelope_.done() || stealing_.done(); }
    int remaining() const noexcept { return remaining_; }

    void fadeOut(int duration) noexcept
    {
        if (remaining_ > duration)
            remaining_ = stealing_.start(static_cast<float>(duration));
    }

    // Visualiser accessors.
    float panUnit() const noexcept { return panUnit_; }
    float age01() const noexcept { return std::min(1.0f, static_cast<float>(age_) / duration_); }
    float level() const noexcept { return lastEnv_ * gain_; }
    bool shimmer() const noexcept { return shimmer_; }
    void setId(uint32_t id) noexcept { id_ = id; }
    uint32_t id() const noexcept { return id_; }

    static bool compare(const Grain* a, const Grain* b) noexcept { return a->remaining_ < b->remaining_; }

private:
    GrainEnvelope envelope_;
    VoiceStealingEnvelope stealing_;
    double delay_ = 0.0;
    double speed_ = 1.0;
    double safetyZone_ = 256.0;
    float gain_ = 1.0f;
    float panUnit_ = 0.0f;
    float duration_ = 1.0f;
    float lastEnv_ = 0.0f;
    int remaining_ = 0;
    int age_ = 0;
    bool shimmer_ = false;
    uint32_t id_ = 0;
};

// Grain pool with Beads' voice-stealing scheduler.
template <int NumGrains>
class GrainPool {
public:
    static constexpr int kNumGrains = NumGrains;

    void init(float sampleRate) noexcept
    {
        instantXfade_ = static_cast<int>(sampleRate * 0.005f);
        simpleThreshold_ = static_cast<int>(sampleRate * 0.025f);
        preemptiveXfade_ = static_cast<int>(sampleRate * 0.1f);
        reset(sampleRate);
    }

    void reset(float sampleRate) noexcept
    {
        for (int i = 0; i < NumGrains; ++i) {
            grains_[i].init(sampleRate);
            priority_[i] = &grains_[i];
        }
        interval_ = 1;
    }

    int countActive() const noexcept
    {
        int n = 0;
        for (const auto& g : grains_)
            n += g.done() ? 0 : 1;
        return n;
    }

    Grain* newGrain() noexcept { return priority_[0]; }

    // Call after starting newGrain(): reorders and pre-fades the next victims.
    void sort() noexcept
    {
        constexpr int kSafety = 4;
        int fade[kSafety];
        if (interval_ < simpleThreshold_) {
            std::rotate(&priority_[0], &priority_[1], &priority_[NumGrains]);
            fade[0] = interval_ + 1;
            fade[1] = instantXfade_;
            fade[2] = instantXfade_;
            fade[3] = preemptiveXfade_;
            if (interval_ > instantXfade_)
                std::swap(fade[0], fade[2]);
        } else {
            std::partial_sort(&priority_[0], &priority_[kSafety], &priority_[NumGrains], Grain::compare);
            fade[0] = instantXfade_;
            fade[1] = instantXfade_;
            fade[2] = interval_;
            fade[3] = preemptiveXfade_;
            if (interval_ > preemptiveXfade_)
                std::swap(fade[2], fade[3]);
        }
        for (int i = 0; i < kSafety; ++i)
            priority_[i]->fadeOut(fade[i]);
        interval_ = 0;
    }

    void tick() noexcept { ++interval_; }

    std::array<Grain, NumGrains>& grains() noexcept { return grains_; }
    const std::array<Grain, NumGrains>& grains() const noexcept { return grains_; }

private:
    std::array<Grain, NumGrains> grains_ {};
    Grain* priority_[NumGrains] {};
    int instantXfade_ = 240;
    int simpleThreshold_ = 1200;
    int preemptiveXfade_ = 4800;
    int interval_ = 1;
};

} // namespace glint::beads_port

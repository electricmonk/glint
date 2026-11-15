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
// Adapted for Glint from beads/dsp/grain_envelope.h and
// beads/dsp/voice_stealing_envelope.h: namespaced, no stmlib dependency,
// minimum attack/release derived from the host sample rate.
#pragma once

#include <algorithm>

namespace glint::beads_port {

inline float smoothStep(float x) noexcept { return x * x * (3.0f - 2.0f * x); }

class GrainEnvelope {
public:
    void init(float sampleRate) noexcept
    {
        stage_ = Done;
        phase_ = 0.0f;
        inc_[Attack] = inc_[Sustain] = inc_[Release] = 0.5f;
        inc_[Done] = 0.0f;
        smoothness_ = 0.0f;
        minAttack_ = 0.0005f * sampleRate;
        minRelease_ = 0.010f * sampleRate;
    }

    // shape: 0 square .. 1/3 decay .. 2/3 bell .. 1 reverse-envelope
    void start(float duration, float shape, float fractionalSample = 0.0f) noexcept
    {
        stage_ = Attack;
        phase_ = 0.0f;

        const float sustain = std::max(1.0f - 3.0f * shape, 0.0f);
        shape = (shape - 0.33333333f) * 1.5f;
        if (shape < 0.0f)
            shape = 0.0f;
        else if (shape < 0.5f)
            shape = shape * shape * shape * 4.0f;
        const float attack = shape;
        const float release = (1.0f - attack) * (1.0f - sustain);

        const float attackDuration = std::max(duration * attack, minAttack_ * (1.0f + 3.0f * sustain));
        const float sustainDuration = std::max(duration * sustain, 2.0f);
        const float releaseDuration = std::max(duration * release, minRelease_);

        inc_[Attack] = 1.0f / attackDuration;
        inc_[Sustain] = 1.0f / sustainDuration;
        inc_[Release] = 1.0f / releaseDuration;
        smoothness_ = attack < 0.5f ? 2.0f * attack : 2.0f - 2.0f * attack;

        phase_ += inc_[stage_] * fractionalSample;
        wrap();
    }

    void stop() noexcept { stage_ = Done; }

    float render() noexcept
    {
        if (stage_ == Done)
            return 0.0f;
        float value;
        if (stage_ == Sustain) {
            value = 1.0f;
        } else {
            const bool attack = stage_ == Attack;
            const float t = attack ? phase_ : 1.0f - phase_;
            const float base = attack ? t : (t * t) * (t * t);
            value = base + (smoothStep(t) - base) * smoothness_;
        }
        phase_ += inc_[stage_];
        wrap();
        return value;
    }

    bool done() const noexcept { return stage_ >= Done; }

private:
    enum Stage { Attack, Sustain, Release, Done, NumStages };

    void wrap() noexcept
    {
        while (phase_ >= 1.0f && stage_ < Done) {
            const float t = (phase_ - 1.0f) / inc_[stage_];
            stage_ = static_cast<Stage>(static_cast<int>(stage_) + 1);
            phase_ = t * inc_[stage_];
        }
    }

    Stage stage_ = Done;
    float phase_ = 0.0f;
    float inc_[NumStages] {};
    float smoothness_ = 0.0f;
    float minAttack_ = 24.0f;
    float minRelease_ = 480.0f;
};

class VoiceStealingEnvelope {
public:
    void resetAndPause() noexcept
    {
        value_ = 1.0f;
        decrement_ = 0.0f;
    }
    int start(float duration) noexcept
    {
        decrement_ = 1.0f / std::max(duration, 1.0f);
        return static_cast<int>(value_ * duration);
    }
    float render() noexcept
    {
        value_ -= decrement_;
        return std::max(value_, 0.0f);
    }
    bool done() const noexcept { return value_ <= 0.0f; }

private:
    float value_ = 1.0f;
    float decrement_ = 0.0f;
};

} // namespace glint::beads_port

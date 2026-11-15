// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cmath>

#include "dsp/Rng.h"

namespace glint {

// N decorrelated slow modulators: sine LFO (rates spread around a base rate)
// plus a smoothed random walk. Output roughly in [-1, 1].
template <int N>
class ModBank {
public:
    void prepare(double sampleRate, uint32_t seed) noexcept
    {
        sampleRate_ = sampleRate;
        rng_.seed(seed);
        for (int i = 0; i < N; ++i) {
            phase_[i] = static_cast<double>(i) / N;
            walk_[i] = walkTarget_[i] = 0.0f;
        }
        walkCoeff_ = static_cast<float>(1.0 - std::exp(-2.0 * 3.14159265358979 * 0.15 / sampleRate));
        walkCounter_ = 0;
    }
    void setRate(float hz) noexcept
    {
        for (int i = 0; i < N; ++i) {
            const double spread = 0.72 + 0.56 * static_cast<double>(i) / (N > 1 ? N - 1 : 1);
            inc_[i] = hz * spread / sampleRate_;
        }
    }
    // Advance one sample and return modulator i values via out.
    void tick(std::array<float, N>& out) noexcept
    {
        if (--walkCounter_ <= 0) {
            walkCounter_ = static_cast<int>(sampleRate_ * 0.37);
            for (int i = 0; i < N; ++i)
                walkTarget_[i] = rng_.bipolar();
        }
        for (int i = 0; i < N; ++i) {
            phase_[i] += inc_[i];
            if (phase_[i] >= 1.0)
                phase_[i] -= 1.0;
            walk_[i] += (walkTarget_[i] - walk_[i]) * walkCoeff_;
            out[i] = 0.7f * fastSin(phase_[i]) + 0.3f * walk_[i];
        }
    }

private:
    // Parabolic sine approximation on phase 0..1, max error ~0.1%.
    static float fastSin(double ph) noexcept
    {
        float x = static_cast<float>(ph) * 2.0f - 1.0f; // -1..1 maps to -pi..pi
        float y = 4.0f * x * (1.0f - std::fabs(x));
        y = 0.225f * (y * std::fabs(y) - y) + y;
        return -y;
    }

    double sampleRate_ = 48000.0;
    Rng rng_ { 7 };
    std::array<double, N> phase_ {};
    std::array<double, N> inc_ {};
    std::array<float, N> walk_ {};
    std::array<float, N> walkTarget_ {};
    float walkCoeff_ = 0.0f;
    int walkCounter_ = 0;
};

} // namespace glint

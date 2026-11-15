// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dsp/PitchShifter.h"

#include <cmath>

namespace glint {

void PitchShifter::prepare(double sampleRate, float windowMs)
{
    window_ = static_cast<float>(sampleRate * windowMs * 0.001);
    line_.prepare(static_cast<int>(window_) + 8);
    reset();
}

void PitchShifter::reset() noexcept
{
    line_.reset();
    phase_ = 0.0;
}

void PitchShifter::setSemitones(float st) noexcept
{
    const double ratio = std::exp2(static_cast<double>(st) / 12.0);
    // Delay changes at (1 - ratio) samples per sample.
    inc_ = (1.0 - ratio) / static_cast<double>(window_);
}

float PitchShifter::read() noexcept
{
    phase_ += inc_;
    phase_ -= std::floor(phase_);
    double p2 = phase_ + 0.5;
    if (p2 >= 1.0)
        p2 -= 1.0;
    const float d1 = 1.0f + static_cast<float>(phase_) * window_;
    const float d2 = 1.0f + static_cast<float>(p2) * window_;
    constexpr float kPi = 3.14159265f;
    const float w1 = std::sin(kPi * static_cast<float>(phase_));
    const float w2 = std::sin(kPi * static_cast<float>(p2));
    return line_.readLinear(d1) * w1 + line_.readLinear(d2) * w2;
}

} // namespace glint

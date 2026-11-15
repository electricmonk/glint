// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cmath>

namespace glint {

// One-pole exponential smoother, ticked per sample.
// timeMs is the time to reach ~63% of a step.
class Smoother {
public:
    void prepare(double sampleRate, float timeMs) noexcept
    {
        sampleRate_ = sampleRate;
        setTime(timeMs);
    }
    void setTime(float timeMs) noexcept
    {
        const double n = sampleRate_ * static_cast<double>(timeMs) * 0.001;
        coeff_ = n <= 1.0 ? 1.0f : static_cast<float>(1.0 - std::exp(-1.0 / n));
    }
    void setTarget(float t) noexcept { target_ = t; }
    void snap(float v) noexcept { value_ = target_ = v; }
    float next() noexcept
    {
        value_ += (target_ - value_) * coeff_;
        return value_;
    }
    float value() const noexcept { return value_; }
    float target() const noexcept { return target_; }

private:
    double sampleRate_ = 48000.0;
    float coeff_ = 1.0f;
    float value_ = 0.0f;
    float target_ = 0.0f;
};

inline float dbToGain(float db) noexcept { return std::pow(10.0f, db * 0.05f); }
inline float gainToDb(float g) noexcept { return 20.0f * std::log10(g > 1e-12f ? g : 1e-12f); }
inline float semitonesToRatio(float st) noexcept { return std::exp2(st / 12.0f); }

// One-pole low-pass coefficient for a cutoff in Hz.
inline float onePoleCoeff(float hz, double sampleRate) noexcept
{
    const double x = std::exp(-2.0 * 3.14159265358979323846 * static_cast<double>(hz) / sampleRate);
    return static_cast<float>(1.0 - x);
}

struct OnePoleLP {
    float z = 0.0f;
    float process(float x, float a) noexcept { return z += a * (x - z); }
    void reset() noexcept { z = 0.0f; }
};

struct DcBlocker {
    float x1 = 0.0f, y1 = 0.0f, r = 0.995f;
    void prepare(double sampleRate, float hz = 10.0f) noexcept
    {
        r = static_cast<float>(1.0 - 2.0 * 3.14159265358979323846 * hz / sampleRate);
    }
    float process(float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
};

// TPT state-variable filter (Zavalishin), used for loop high/low cuts.
struct Svf {
    float g = 0.0f, k = 1.41421356f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
    void set(float hz, double sampleRate, float q = 0.7071f) noexcept
    {
        const double maxHz = sampleRate * 0.45;
        const double f = hz > maxHz ? maxHz : hz;
        g = static_cast<float>(std::tan(3.14159265358979323846 * f / sampleRate));
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    // Returns low-pass; high-pass via hp().
    float lp(float v0) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }
    float hp(float v0) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v0 - k * v1 - v2;
    }
    void reset() noexcept { ic1 = ic2 = 0.0f; }
};

inline bool isFiniteF(float x) noexcept { return std::isfinite(x); }

} // namespace glint

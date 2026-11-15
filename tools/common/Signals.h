// SPDX-License-Identifier: AGPL-3.0-or-later
// Test-signal generators for renders and tests.
#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "dsp/Rng.h"

namespace glint::signals {

inline std::vector<float> sine(double sr, double seconds, double hz, double amp)
{
    std::vector<float> x(static_cast<size_t>(sr * seconds));
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = static_cast<float>(amp * std::sin(2.0 * 3.14159265358979 * hz * static_cast<double>(i) / sr));
    return x;
}

// Paul Kellet's refined pink filter on white noise.
inline std::vector<float> pink(double sr, double seconds, double amp, uint32_t seed)
{
    Rng rng(seed);
    std::vector<float> x(static_cast<size_t>(sr * seconds));
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (auto& v : x) {
        const double w = rng.bipolar();
        b0 = 0.99886 * b0 + w * 0.0555179;
        b1 = 0.99332 * b1 + w * 0.0750759;
        b2 = 0.96900 * b2 + w * 0.1538520;
        b3 = 0.86650 * b3 + w * 0.3104856;
        b4 = 0.55000 * b4 + w * 0.5329522;
        b5 = -0.7616 * b5 - w * 0.0168980;
        const double p = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362;
        b6 = w * 0.115926;
        v = static_cast<float>(p * 0.11 * amp);
    }
    return x;
}

// A short plucked phrase (decaying harmonic tones): a musical test input.
inline std::vector<float> notes(double sr, double seconds, double amp)
{
    std::vector<float> x(static_cast<size_t>(sr * seconds), 0.0f);
    const double midi[] = { 60, 64, 67, 72, 71, 67, 64, 62, 60, 55, 60, 64 };
    const double step = 0.42;
    for (size_t k = 0; k < sizeof(midi) / sizeof(midi[0]); ++k) {
        const double f0 = 440.0 * std::pow(2.0, (midi[k] - 69.0) / 12.0);
        const size_t start = static_cast<size_t>(k * step * sr);
        const size_t len = static_cast<size_t>(1.6 * sr);
        for (size_t i = 0; i < len && start + i < x.size(); ++i) {
            const double t = static_cast<double>(i) / sr;
            double v = 0.0;
            for (int h = 1; h <= 6; ++h)
                v += std::sin(2.0 * 3.14159265358979 * f0 * h * t) * std::exp(-t * (2.5 + h * 1.2)) / h;
            const double attack = std::min(1.0, t / 0.004);
            x[start + i] += static_cast<float>(amp * 0.6 * v * attack);
        }
    }
    return x;
}

inline std::vector<float> impulse(double sr, double seconds, double amp)
{
    std::vector<float> x(static_cast<size_t>(sr * seconds), 0.0f);
    if (!x.empty()) x[0] = static_cast<float>(amp);
    return x;
}

} // namespace glint::signals

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

inline std::vector<float> impulse(double sr, double seconds, double amp)
{
    std::vector<float> x(static_cast<size_t>(sr * seconds), 0.0f);
    if (!x.empty()) x[0] = static_cast<float>(amp);
    return x;
}

} // namespace glint::signals

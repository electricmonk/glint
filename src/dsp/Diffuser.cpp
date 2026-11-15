// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dsp/Diffuser.h"

#include <cmath>

namespace glint {

namespace {
constexpr float kMsL[4] = { 4.771f, 3.595f, 12.73f, 9.307f };
constexpr float kMsR[4] = { 5.031f, 3.833f, 13.37f, 8.867f };
} // namespace

void Diffuser::prepare(double sampleRate)
{
    preL_.prepare(static_cast<int>(sampleRate * 0.52) + 8);
    preR_.prepare(static_cast<int>(sampleRate * 0.52) + 8);
    for (int i = 0; i < kStages; ++i) {
        lenL_[static_cast<size_t>(i)] = std::max(1, static_cast<int>(std::lround(kMsL[i] * 0.001 * sampleRate)));
        lenR_[static_cast<size_t>(i)] = std::max(1, static_cast<int>(std::lround(kMsR[i] * 0.001 * sampleRate)));
        apL_[static_cast<size_t>(i)].prepare(lenL_[static_cast<size_t>(i)] + 2);
        apR_[static_cast<size_t>(i)].prepare(lenR_[static_cast<size_t>(i)] + 2);
    }
}

void Diffuser::reset() noexcept
{
    preL_.reset();
    preR_.reset();
    for (auto& d : apL_) d.reset();
    for (auto& d : apR_) d.reset();
}

void Diffuser::process(float& l, float& r, float diffusion, float preDelaySamples) noexcept
{
    preL_.write(l);
    preR_.write(r);
    const float pd = std::max(1.0f, preDelaySamples);
    l = preL_.readLinear(pd);
    r = preR_.readLinear(pd);

    const float g = 0.75f * diffusion;
    for (size_t i = 0; i < kStages; ++i) {
        {
            const float d = apL_[i].tap(lenL_[i]);
            const float v = l + g * d;
            apL_[i].write(v);
            l = d - g * v;
        }
        {
            const float d = apR_[i].tap(lenR_[i]);
            const float v = r + g * d;
            apR_[i].write(v);
            r = d - g * v;
        }
    }
}

} // namespace glint

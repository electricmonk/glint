// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cmath>
#include <cstdint>

namespace glint {

// Per-instance xorshift32: deterministic for a given seed, no global state.
class Rng {
public:
    explicit Rng(uint32_t seed = 1) noexcept { seed_(seed); }
    void seed(uint32_t s) noexcept { seed_(s); }

    uint32_t next() noexcept
    {
        uint32_t x = state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return state_ = x;
    }
    // [0, 1)
    float uniform() noexcept { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
    // [-1, 1)
    float bipolar() noexcept { return 2.0f * uniform() - 1.0f; }

private:
    void seed_(uint32_t s) noexcept { state_ = s ? s * 2654435761u : 0x9e3779b9u; next(); }
    uint32_t state_ = 1;
};

} // namespace glint

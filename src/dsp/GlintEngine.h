// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>

namespace glint {

// Top-level engine. Phase 0: stereo pass-through.
class GlintEngine {
public:
    // Allocate everything here. maxBlockSize may be exceeded by the host;
    // process() must cope with any block size.
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // In-place stereo processing. No allocation, locks, logging or exceptions.
    void process(float* left, float* right, int numSamples) noexcept;

    double sampleRate() const noexcept { return sampleRate_; }

private:
    double sampleRate_ = 48000.0;
    int maxBlockSize_ = 512;
};

} // namespace glint

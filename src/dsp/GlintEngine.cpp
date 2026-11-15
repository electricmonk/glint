// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dsp/GlintEngine.h"

namespace glint {

void GlintEngine::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;
    maxBlockSize_ = maxBlockSize;
    reset();
}

void GlintEngine::reset() {}

void GlintEngine::process(float* left, float* right, int numSamples) noexcept
{
    (void)left;
    (void)right;
    (void)numSamples; // pass-through
}

} // namespace glint

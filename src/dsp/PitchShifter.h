// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "dsp/DelayLine.h"

namespace glint {

// Two-tap delay-line pitch shifter with crossfaded sine windows (equal power).
// No reported latency; the window smear is part of the sound.
class PitchShifter {
public:
    void prepare(double sampleRate, float windowMs);
    void reset() noexcept;
    void setSemitones(float st) noexcept;
    // Write one sample into the shared delay line first (writeInput), then
    // read any number of voices from it.
    void writeInput(float x) noexcept { line_.write(x); }
    float read() noexcept;

private:
    DelayLine line_;
    float window_ = 2880.0f;
    double phase_ = 0.0;
    double inc_ = 0.0;
};

} // namespace glint

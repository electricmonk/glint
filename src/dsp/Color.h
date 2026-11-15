// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cmath>
#include <vector>

#include "dsp/Rng.h"
#include "dsp/Smoothers.h"

namespace glint {

enum class Texture { Clean = 0, Digital = 1, Tape = 2, Dust = 3 };

// Colour applied to what the grain buffer records.
class Color {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void setTexture(Texture t) noexcept { texture_ = t; }
    Texture texture() const noexcept { return texture_; }
    void process(float& l, float& r) noexcept;

private:
    double sampleRate_ = 48000.0;
    Texture texture_ = Texture::Clean;
    Rng rng_ { 0xC0105u };

    // Tape / Dust tone
    float lpCoeffTape_ = 0.5f, lpCoeffDust_ = 0.4f;
    OnePoleLP lpL_, lpR_;

    // Digital: sample-and-hold decimation
    int holdCounter_ = 0;
    float heldL_ = 0.0f, heldR_ = 0.0f;

    // Dust: wow/flutter through a short modulated delay
    std::vector<float> wfL_, wfR_;
    int wfWrite_ = 0;
    int wfMask_ = 0;
    double wowPhase_ = 0.0, flutterPhase_ = 0.0;
    double wowInc_ = 0.0, flutterInc_ = 0.0;
    float wfBase_ = 0.0f, wowDepth_ = 0.0f, flutterDepth_ = 0.0f;
    float driftTarget_ = 0.0f, drift_ = 0.0f;
    int driftCounter_ = 0;

    // Dust: hiss + crackle
    OnePoleLP hissLP_;
};

} // namespace glint

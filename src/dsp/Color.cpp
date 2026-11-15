// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dsp/Color.h"

#include <algorithm>

namespace glint {

namespace {
constexpr double kTwoPi = 6.283185307179586;

inline float tapeSat(float x) noexcept
{
    // Gentle asymmetric-free saturation, unity small-signal gain.
    constexpr float drive = 1.6f;
    return std::tanh(x * drive) / drive;
}
} // namespace

void Color::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    lpCoeffTape_ = onePoleCoeff(11000.0f, sampleRate);
    lpCoeffDust_ = onePoleCoeff(7000.0f, sampleRate);
    xfadeLen_ = std::max(1, static_cast<int>(sampleRate * 0.02));

    // Wow/flutter delay line: up to ~12 ms.
    int size = 1;
    while (size < static_cast<int>(sampleRate * 0.012) + 8)
        size <<= 1;
    wfL_.assign(static_cast<size_t>(size), 0.0f);
    wfR_.assign(static_cast<size_t>(size), 0.0f);
    wfMask_ = size - 1;
    wfBase_ = static_cast<float>(sampleRate * 0.005);
    wowDepth_ = static_cast<float>(sampleRate * 0.0018);
    flutterDepth_ = static_cast<float>(sampleRate * 0.00012);
    wowInc_ = 0.55 / sampleRate;
    flutterInc_ = 6.3 / sampleRate;
    reset();
}

void Color::reset() noexcept
{
    lpL_.reset();
    lpR_.reset();
    dustL_.reset();
    dustR_.reset();
    hissLP_.reset();
    xfade_ = 0;
    previous_ = texture_;
    holdCounter_ = 0;
    heldL_ = heldR_ = 0.0f;
    std::fill(wfL_.begin(), wfL_.end(), 0.0f);
    std::fill(wfR_.begin(), wfR_.end(), 0.0f);
    wfWrite_ = 0;
    wowPhase_ = flutterPhase_ = 0.0;
    drift_ = driftTarget_ = 0.0f;
    driftCounter_ = 0;
    rng_.seed(0xC0105u);
}

void Color::process(float& l, float& r) noexcept
{
    // Every mode's state runs on every sample, so switching (or coming back
    // to) a mode never starts from stale filter or delay-line contents.
    float out[4][2];
    out[0][0] = l; out[0][1] = r;
    for (int m = 1; m < 4; ++m) {
        out[m][0] = l;
        out[m][1] = r;
        render(static_cast<Texture>(m), out[m][0], out[m][1]);
    }
    const auto& cur = out[static_cast<int>(texture_)];
    if (xfade_ > 0) {
        const auto& prev = out[static_cast<int>(previous_)];
        const float t = 1.0f - static_cast<float>(xfade_) / static_cast<float>(xfadeLen_);
        l = prev[0] + (cur[0] - prev[0]) * t;
        r = prev[1] + (cur[1] - prev[1]) * t;
        --xfade_;
        return;
    }
    l = cur[0];
    r = cur[1];
}

void Color::render(Texture t, float& l, float& r) noexcept
{
    switch (t) {
    case Texture::Clean:
        return;

    case Texture::Digital: {
        // ~12-bit, sample rate halved (relative to host).
        if (holdCounter_++ % 2 == 0) {
            constexpr float q = 2048.0f;
            heldL_ = std::round(std::clamp(l, -1.0f, 1.0f) * q) / q;
            heldR_ = std::round(std::clamp(r, -1.0f, 1.0f) * q) / q;
        }
        l = heldL_;
        r = heldR_;
        return;
    }

    case Texture::Tape:
        l = lpL_.process(tapeSat(l), lpCoeffTape_);
        r = lpR_.process(tapeSat(r), lpCoeffTape_);
        return;

    case Texture::Dust: {
        // Wow & flutter.
        wfL_[static_cast<size_t>(wfWrite_)] = l;
        wfR_[static_cast<size_t>(wfWrite_)] = r;
        wowPhase_ += wowInc_;
        if (wowPhase_ >= 1.0) wowPhase_ -= 1.0;
        flutterPhase_ += flutterInc_;
        if (flutterPhase_ >= 1.0) flutterPhase_ -= 1.0;
        if (--driftCounter_ <= 0) {
            driftCounter_ = static_cast<int>(sampleRate_ * 0.25);
            driftTarget_ = rng_.bipolar();
        }
        drift_ += (driftTarget_ - drift_) * 2.0e-5f;
        const float mod = wfBase_
            + wowDepth_ * (0.7f * static_cast<float>(std::sin(kTwoPi * wowPhase_)) + 0.3f * drift_)
            + flutterDepth_ * static_cast<float>(std::sin(kTwoPi * flutterPhase_));
        const float pos = static_cast<float>(wfWrite_) - mod;
        const float fl = std::floor(pos);
        const float frac = pos - fl;
        const int i0 = static_cast<int>(fl) & wfMask_;
        const int i1 = (i0 + 1) & wfMask_;
        l = wfL_[static_cast<size_t>(i0)] + frac * (wfL_[static_cast<size_t>(i1)] - wfL_[static_cast<size_t>(i0)]);
        r = wfR_[static_cast<size_t>(i0)] + frac * (wfR_[static_cast<size_t>(i1)] - wfR_[static_cast<size_t>(i0)]);
        wfWrite_ = (wfWrite_ + 1) & wfMask_;

        // Soft tape-ish tone and a faint hiss (about -66 dBFS).
        l = dustL_.process(tapeSat(l), lpCoeffDust_);
        r = dustR_.process(tapeSat(r), lpCoeffDust_);
        const float hiss = hissLP_.process(rng_.bipolar(), 0.35f) * 0.0009f;
        l += hiss;
        r += hiss * 0.8f + rng_.bipolar() * 0.0002f;
        return;
    }
    }
}

} // namespace glint

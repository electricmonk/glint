// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dsp/Fdn.h"

#include <algorithm>
#include <cmath>

namespace glint {

namespace {
// Mutually prime-ish line lengths in ms at size 1.
constexpr float kBaseMs[Fdn::kLines] = { 43.73f, 51.29f, 59.87f, 67.13f, 73.69f, 83.31f, 91.07f, 101.93f };
// Rows of an 8x8 Hadamard (Sylvester order) used as in/out vectors.
constexpr float kInL[Fdn::kLines] = { 1, -1, -1, 1, 1, -1, -1, 1 };
constexpr float kInR[Fdn::kLines] = { 1, 1, 1, 1, -1, -1, -1, -1 };
constexpr float kOutL[Fdn::kLines] = { 1, -1, 1, -1, 1, -1, 1, -1 };
constexpr float kOutR[Fdn::kLines] = { 1, 1, -1, -1, 1, 1, -1, -1 };
constexpr float kInvSqrt8 = 0.35355339f;
constexpr float kMaxModMs = 1.5f;
constexpr int kControlInterval = 16;

inline void hadamard8(std::array<float, 8>& x) noexcept
{
    for (int h = 1; h < 8; h <<= 1)
        for (int i = 0; i < 8; i += h << 1)
            for (int j = i; j < i + h; ++j) {
                const float a = x[static_cast<size_t>(j)];
                const float b = x[static_cast<size_t>(j + h)];
                x[static_cast<size_t>(j)] = a + b;
                x[static_cast<size_t>(j + h)] = a - b;
            }
    for (auto& v : x)
        v *= kInvSqrt8;
}
} // namespace

void Fdn::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    const int maxLen = static_cast<int>(std::ceil((kBaseMs[kLines - 1] + kMaxModMs + 1.0f) * 0.001 * sampleRate));
    for (int i = 0; i < kLines; ++i) {
        lines_[static_cast<size_t>(i)].prepare(maxLen + 4);
        baseLen_[static_cast<size_t>(i)] = static_cast<float>(kBaseMs[i] * 0.001 * sampleRate);
    }
    mod_.prepare(sampleRate, 0xF0D5u);
    sizeSmooth_.prepare(sampleRate, 250.0f);
    sizeSmooth_.snap(settings_.size);
    reset();
    updateCoefficients();
}

void Fdn::reset() noexcept
{
    for (auto& l : lines_) l.reset();
    for (auto& d : damp_) d.reset();
    state_.fill(0.0f);
    mod_.prepare(sampleRate_, 0xF0D5u);
    mod_.setRate(settings_.modRateHz);
    sizeSmooth_.snap(settings_.size);
    controlCounter_ = 0;
}

void Fdn::setSettings(const FdnSettings& s) noexcept
{
    settings_ = s;
    sizeSmooth_.setTarget(std::clamp(s.size, 0.0f, 1.0f));
    mod_.setRate(std::clamp(s.modRateHz, 0.01f, 10.0f));
}

void Fdn::updateCoefficients() noexcept
{
    const float size = sizeSmooth_.value();
    const float scale = 0.25f + 0.75f * size;
    dampCoeff_ = onePoleCoeff(std::clamp(settings_.dampingHz, 500.0f, 20000.0f), sampleRate_);

    // Damping filter magnitude at 1 kHz: compensate so RT60 at 1 kHz = decay.
    const double w = 2.0 * 3.14159265358979 * 1000.0 / sampleRate_;
    const double a = dampCoeff_;
    const double b = 1.0 - a;
    const double mag1k = a / std::sqrt(1.0 - 2.0 * b * std::cos(w) + b * b);

    const bool infinite = settings_.decayS >= 60.0f;
    const double decay = std::clamp(static_cast<double>(settings_.decayS), 0.1, 60.0);
    double gSum2 = 0.0;
    for (int i = 0; i < kLines; ++i) {
        const double lenSec = baseLen_[static_cast<size_t>(i)] * scale / sampleRate_;
        double g = infinite ? 0.9995 : std::pow(10.0, -3.0 * lenSec / decay) / mag1k;
        g = std::min(g, 0.9995);
        gain_[static_cast<size_t>(i)] = static_cast<float>(g);
        // Quarter-power normalisation: energy gain (1-g^2)^-1/2 grows with decay
        // like a real room; the loop return is normalised separately (loopNorm).
        inGain_[static_cast<size_t>(i)] = static_cast<float>(std::pow(std::max(1e-6, 1.0 - g * g), 0.25));
        gSum2 += g * g;
    }
    const double gAvg2 = gSum2 / kLines;
    loopNorm_ = static_cast<float>(std::pow(std::max(1e-6, 1.0 - gAvg2), 0.25));
    makeup_ = 1.0f;
    modDepthSamples_ = std::clamp(settings_.modDepth, 0.0f, 1.0f) * kMaxModMs * 0.001f * static_cast<float>(sampleRate_);
}

void Fdn::process(float inL, float inR, float& outL, float& outR) noexcept
{
    sizeSmooth_.next();
    if (--controlCounter_ <= 0) {
        controlCounter_ = kControlInterval;
        updateCoefficients();
    }

    std::array<float, kLines> m;
    mod_.tick(m);

    const float scale = 0.25f + 0.75f * sizeSmooth_.value();
    std::array<float, kLines> y;
    for (size_t i = 0; i < kLines; ++i) {
        const float len = baseLen_[i] * scale + modDepthSamples_ * (1.0f + m[i]) + 1.0f;
        y[i] = lines_[i].readHermite(len);
    }

    float l = 0.0f, r = 0.0f;
    for (size_t i = 0; i < kLines; ++i) {
        l += kOutL[i] * y[i];
        r += kOutR[i] * y[i];
    }
    l *= kInvSqrt8;
    r *= kInvSqrt8;

    std::array<float, kLines> fb;
    for (size_t i = 0; i < kLines; ++i)
        fb[i] = damp_[i].process(y[i], dampCoeff_) * gain_[i] * gainScale_;
    hadamard8(fb);
    for (size_t i = 0; i < kLines; ++i)
        lines_[i].write(fb[i] + (kInL[i] * inL + kInR[i] * inR) * inGain_[i]);

    // Width (M/S).
    const float w = std::clamp(settings_.width, 0.0f, 2.0f);
    const float mid = 0.5f * (l + r);
    const float side = 0.5f * (l - r) * w;
    outL = mid + side;
    outR = mid - side;
}

} // namespace glint

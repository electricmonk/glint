// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dsp/ShimmerLoop.h"

#include <algorithm>
#include <cmath>

namespace glint {

namespace {
constexpr float kMaxBelowBloom = 0.97f;  // loop gain clamp below 100%
constexpr float kTargetLevel = 0.5f;     // -6 dBFS
inline float softClip(float x) noexcept { return std::tanh(x); }
} // namespace

float loopGainForFeedback(float fb) noexcept
{
    fb = std::clamp(fb, 0.0f, 1.1f);
    if (fb <= 1.0f) {
        // Sustain 1/(1-g) = 30^(fb^1.4): g(0.3) = 0.47, g(0.5) = 0.72,
        // g(0.7) = 0.88, g(1) ~ 0.967. The exponent keeps the lower half of
        // the knob subtle; plain 30^fb reached 0.82 at 50%, too much on piano.
        const float g = 1.0f - std::pow(30.0f, -std::pow(fb, 1.4f));
        return std::min(g, kMaxBelowBloom);
    }
    // Above 100%: continue smoothly up to ~1.2 at 110%; limiter holds level.
    const float g1 = 1.0f - 1.0f / 30.0f;
    return g1 + (fb - 1.0f) * 2.4f;
}

void ShimmerLoop::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    shL_.prepare(sampleRate);
    shR_.prepare(sampleRate);
    dcL_.prepare(sampleRate, 8.0f);
    dcR_.prepare(sampleRate, 8.0f);
    gain_.prepare(sampleRate, 20.0f);
    blend_.prepare(sampleRate, 30.0f);
    auto tc = [sampleRate](double seconds) { return static_cast<float>(1.0 - std::exp(-1.0 / (seconds * sampleRate))); };
    envAttack_ = tc(0.0005);  // peak detector: near-instant attack
    envRelease_ = tc(0.600);  // slow release: no pumping on the tail
    limCoeff_ = tc(0.005);    // gain reduction: fast
    limRelease_ = tc(0.400);  // gain recovery: slow
    reset();
}

void ShimmerLoop::reset() noexcept
{
    shL_.reset(); shR_.reset();
    hcL_.reset(); hcR_.reset(); lcL_.reset(); lcR_.reset();
    dcL_.reset(); dcR_.reset();
    gain_.snap(loopGainForFeedback(settings_.feedback));
    blend_.snap(settings_.blend);
    env_ = 0.0f;
    limGain_ = 1.0f;
    filterCounter_ = 0;
}

void ShimmerLoop::setSettings(const ShimmerSettings& s) noexcept
{
    settings_ = s;
    gain_.setTarget(loopGainForFeedback(s.feedback));
    blend_.setTarget(std::clamp(s.blend, 0.0f, 1.0f));
    shL_.setShifts(s.shiftA, s.shiftB, s.shiftBLevel);
    shR_.setShifts(s.shiftA, s.shiftB, s.shiftBLevel);
}

void ShimmerLoop::process(float inL, float inR, float& retL, float& retR) noexcept
{
    if (--filterCounter_ <= 0) {
        filterCounter_ = 32;
        hcL_.set(settings_.highCutHz, sampleRate_);
        hcR_.set(settings_.highCutHz, sampleRate_);
        lcL_.set(settings_.lowCutHz, sampleRate_);
        lcR_.set(settings_.lowCutHz, sampleRate_);
    }

    const float g = gain_.next();
    const float blend = blend_.next();
    const float shL = shL_.process(inL);
    const float shR = shR_.process(inR);

    // Only the shifted signal recirculates: feeding the reverb's own output
    // back unshifted would pile up on its modal peaks and run away. Blend sets
    // how much shifted signal goes back (equal-power law).
    const float wS = std::sqrt(blend);
    float l = shL * wS;
    float r = shR * wS;

    l = dcL_.process(lcL_.hp(hcL_.lp(l)));
    r = dcR_.process(lcR_.hp(hcR_.lp(r)));

    // Bloom limiter: track the audible wet level (set by the engine from the
    // previous sample) and pull the loop gain down so the tail sits near -6 dBFS.
    const float level = detectLevel_;
    env_ += (level - env_) * (level > env_ ? envAttack_ : envRelease_);
    const float wanted = env_ > kTargetLevel ? kTargetLevel / env_ : 1.0f;
    limGain_ += (wanted - limGain_) * (wanted < limGain_ ? limCoeff_ : limRelease_);

    l = softClip(l * g * limGain_);
    r = softClip(r * g * limGain_);

    if (!std::isfinite(l) || !std::isfinite(r)) {
        ++nonFinite_;
        reset();
        l = r = 0.0f;
    }
    retL = l;
    retR = r;
}

} // namespace glint

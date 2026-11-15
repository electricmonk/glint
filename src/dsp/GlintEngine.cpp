// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dsp/GlintEngine.h"

#include <algorithm>
#include <cmath>

#include "dsp/DelayLine.h"

namespace glint {

namespace {
constexpr float kHalfPi = 1.57079632679f;
// Smoothing times (ms) for bloom, space, density, drift, spread, stereo, tone, mix.
constexpr float kMacroMs[8] = { 20.0f, 60.0f, 50.0f, 50.0f, 30.0f, 30.0f, 40.0f, 20.0f };
} // namespace

void GlintEngine::prepare(double sampleRate, int maxBlockSize)
{
    (void)maxBlockSize; // processing is chunked internally; any block size works
    sampleRate_ = sampleRate;
    grains_.prepare(sampleRate, 16.0);
    grains_.setEventFifo(&events_);
    diffuser_.prepare(sampleRate);
    fdn_.prepare(sampleRate);
    shimmer_.prepare(sampleRate);
    for (size_t i = 0; i < macroSmooth_.size(); ++i)
        macroSmooth_[i].prepare(sampleRate, kMacroMs[i]);
    mix_.prepare(sampleRate, 20.0f);
    wetGate_.prepare(sampleRate, 8.0f);
    wetGate_.snap(1.0f);
    preDelay_.prepare(sampleRate, 150.0f);
    direct_.prepare(sampleRate, 50.0f);
    gil_.prepare(sampleRate, 80.0f);
    grainsLevel_.prepare(sampleRate, 30.0f);
    reverbLevel_.prepare(sampleRate, 30.0f);
    inGain_.prepare(sampleRate, 20.0f);
    outGain_.prepare(sampleRate, 20.0f);
    reset(seed_);
}

void GlintEngine::reset(uint32_t seed) noexcept
{
    seed_ = seed;
    grains_.reset(seed);
    diffuser_.reset();
    fdn_.reset();
    shimmer_.reset();
    loopL_ = loopR_ = 0.0f;
    nonFinite_ = 0;

    const float mv[8] = { macros_.bloom, macros_.space, macros_.density, macros_.drift,
                          macros_.spread, macros_.stereoDensity, macros_.tone, macros_.mix };
    for (size_t i = 0; i < macroSmooth_.size(); ++i)
        macroSmooth_[i].snap(mv[i]);
    if (useMacros_)
        current_ = macros::map(macros_);
    applyParams(current_);
    mix_.snap(current_.mix);
    preDelay_.snap(current_.preDelayMs * 0.001f * static_cast<float>(sampleRate_));
    direct_.snap(current_.directToReverb);
    gil_.snap(current_.routing == Routing::GrainsInLoop ? current_.grainsInLoop : 0.0f);
    grainsLevel_.snap(dbToGain(current_.grainsLevelDb));
    reverbLevel_.snap(dbToGain(current_.reverbLevelDb));
    inGain_.snap(dbToGain(current_.inputGainDb));
    outGain_.snap(dbToGain(current_.outputGainDb));
}

void GlintEngine::resumeAfterGap() noexcept
{
    grains_.softReset();
    diffuser_.reset();
    fdn_.reset();
    shimmer_.reset();
    loopL_ = loopR_ = 0.0f;
    retL_.fill(0.0f);
    retR_.fill(0.0f);
    // Bypass was fully dry: glide from Mix 0 to the current Mix.
    const float mixTarget = mix_.target();
    mix_.snap(0.0f);
    mix_.setTarget(mixTarget);
    rampInWet();
}

void GlintEngine::setMacros(const MacroValues& m) noexcept
{
    useMacros_ = true;
    macros_ = m;
    const float mv[8] = { m.bloom, m.space, m.density, m.drift, m.spread, m.stereoDensity, m.tone, m.mix };
    for (size_t i = 0; i < macroSmooth_.size(); ++i)
        macroSmooth_[i].setTarget(mv[i]);
}

void GlintEngine::setEngineParams(const EngineParams& p) noexcept
{
    useMacros_ = false;
    current_ = p;
    applyParams(p);
}

void GlintEngine::tickMacros(int n) noexcept
{
    for (auto& s : macroSmooth_)
        for (int i = 0; i < n; ++i)
            s.next();
    MacroValues m = macros_;
    m.bloom = macroSmooth_[0].value();
    m.space = macroSmooth_[1].value();
    m.density = macroSmooth_[2].value();
    m.drift = macroSmooth_[3].value();
    m.spread = macroSmooth_[4].value();
    m.stereoDensity = macroSmooth_[5].value();
    m.tone = macroSmooth_[6].value();
    m.mix = macroSmooth_[7].value();
    current_ = macros::map(m);
    applyParams(current_);
}

void GlintEngine::applyParams(const EngineParams& p) noexcept
{
    GrainParams gp;
    gp.time = p.time;
    gp.sizeMs = p.sizeMs;
    gp.pitchSt = p.pitchSt;
    gp.shape = p.shape;
    gp.densityHz = p.grainRateHz;
    gp.randomness = std::clamp(0.5f * (p.grainDensity + 1.0f), 0.0f, 1.0f);
    gp.randTime = p.randTime;
    gp.randSize = p.randSize;
    gp.randPitch = p.randPitch;
    gp.pitchJitterSt = p.pitchJitterSt;
    gp.reverse = p.reverse;
    gp.spread = p.spread;
    gp.stereoDensity = p.stereoDensity;
    gp.freeze = p.freeze;
    gp.texture = p.color;
    grains_.setParams(gp);

    FdnSettings fs;
    fs.size = p.reverbSize;
    fs.decayS = p.decayS;
    fs.dampingHz = p.dampingHz;
    fs.modDepth = p.modDepth;
    fs.modRateHz = p.modRateHz;
    fs.width = p.width;
    fdn_.setSettings(fs);

    ShimmerSettings ss;
    ss.feedback = p.feedback;
    ss.blend = p.shimmerBlend;
    ss.shiftA = p.shiftA;
    ss.shiftB = p.shiftB;
    ss.shiftBLevel = p.shiftBLevel;
    ss.highCutHz = p.loopHighCutHz;
    ss.lowCutHz = p.loopLowCutHz;
    shimmer_.setSettings(ss);

    const float gil = p.routing == Routing::GrainsInLoop ? std::clamp(p.grainsInLoop, 0.0f, 1.0f) : 0.0f;
    grains_.setShimmerShare(gil);

    mix_.setTarget(std::clamp(p.mix, 0.0f, 1.0f));
    preDelay_.setTarget(std::clamp(p.preDelayMs, 0.0f, 500.0f) * 0.001f * static_cast<float>(sampleRate_));
    direct_.setTarget(std::clamp(p.directToReverb, 0.0f, 1.0f));
    gil_.setTarget(gil);
    grainsLevel_.setTarget(p.grainsLevelDb <= -96.0f ? 0.0f : dbToGain(p.grainsLevelDb));
    reverbLevel_.setTarget(p.reverbLevelDb <= -96.0f ? 0.0f : dbToGain(p.reverbLevelDb));
    inGain_.setTarget(dbToGain(std::clamp(p.inputGainDb, -24.0f, 12.0f)));
    outGain_.setTarget(dbToGain(std::clamp(p.outputGainDb, -24.0f, 12.0f)));
}

void GlintEngine::process(float* left, float* right, int numSamples) noexcept
{
    ScopedFlushDenormals ftz;

    const bool parallel = current_.routing == Routing::Parallel;
    int offset = 0;
    while (offset < numSamples) {
        const int n = std::min(kControlBlock, numSamples - offset);
        float* L = left + offset;
        float* R = right + offset;

        if (useMacros_)
            tickMacros(n);

        // 1) Grain engine: records input (+ part of the loop return), plays grains.
        for (int i = 0; i < n; ++i) {
            const float gi = inGain_.next();
            const float gl = gil_.next();
            writeL_[static_cast<size_t>(i)] = L[i] * gi + retL_[static_cast<size_t>(i)] * gl;
            writeR_[static_cast<size_t>(i)] = R[i] * gi + retR_[static_cast<size_t>(i)] * gl;
            grainL_[static_cast<size_t>(i)] = grainR_[static_cast<size_t>(i)] = 0.0f;
        }
        grains_.process(writeL_.data(), writeR_.data(), grainL_.data(), grainR_.data(), n);

        // 2) Reverb + shimmer loop, per sample.
        const float makeup = fdn_.makeupGain();
        const float gilNow = gil_.value();
        for (int i = 0; i < n; ++i) {
            const size_t k = static_cast<size_t>(i);
            const float gi = inGain_.value();
            const float direct = direct_.next();
            const float inL = L[i] * gi, inR = R[i] * gi;

            const float ln = fdn_.loopNorm();
            float rl = inL * direct + (parallel ? 0.0f : grainL_[k]) + loopL_ * (1.0f - gilNow) * ln;
            float rr = inR * direct + (parallel ? 0.0f : grainR_[k]) + loopR_ * (1.0f - gilNow) * ln;
            if (parallel) {
                rl = inL + loopL_ * ln;
                rr = inR + loopR_ * ln;
            }
            diffuser_.process(rl, rr, current_.diffusion, preDelay_.next());
            float vl, vr;
            fdn_.process(rl, rr, vl, vr);
            shimmer_.process(vl, vr, loopL_, loopR_);
            // Bloom limiter also trims the reverb's own sustain, gently.
            fdn_.setGainScale(1.0f - (1.0f - shimmer_.limiterGain()) * 0.05f);
            retL_[k] = loopL_;
            retR_[k] = loopR_;

            // The Bloom limiter's gain also rides the audible reverb, so the tail
            // is held near -6 dBFS even while the input keeps feeding it.
            const float rv = reverbLevel_.next() * makeup;
            const float gv = grainsLevel_.next();
            float wetL = vl * rv + grainL_[k] * gv;
            float wetR = vr * rv + grainR_[k] * gv;
            shimmer_.setDetectLevel(std::max(std::fabs(wetL), std::fabs(wetR)));
            const float lim = shimmer_.limiterGain() * wetGate_.next();
            wetL *= lim;
            wetR *= lim;

            const float mix = mix_.next();
            const float gd = std::cos(mix * kHalfPi);
            const float gw = std::sin(mix * kHalfPi);
            const float og = outGain_.next();
            float outL = (L[i] * gd + wetL * gw) * og;
            float outR = (R[i] * gd + wetR * gw) * og;

            if (!std::isfinite(outL) || !std::isfinite(outR)) {
                ++nonFinite_;
                fdn_.reset();
                diffuser_.reset();
                shimmer_.reset();
                loopL_ = loopR_ = 0.0f;
                outL = L[i] * gd;
                outR = R[i] * gd;
            }
            L[i] = outL;
            R[i] = outR;
        }
        offset += n;
    }
}

} // namespace glint

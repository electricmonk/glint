// SPDX-License-Identifier: AGPL-3.0-or-later
// Phase 3: +12 st on a 440 Hz sine gives a dominant 880 Hz peak; ten minutes
// of pink noise at Feedback 110% and Decay infinite stays below 0 dBFS with
// no non-finite samples.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "../tools/common/Analysis.h"
#include "../tools/common/Signals.h"
#include "dsp/GlintEngine.h"
#include "dsp/PitchShifter.h"
#include "dsp/ShimmerLoop.h"

using namespace glint;

TEST_CASE("Shimmer: +12 st shifts 440 Hz to a dominant 880 Hz")
{
    const double sr = 48000.0;
    for (float window : { 61.0f, 79.0f }) {
        PitchShifter ps;
        ps.prepare(sr, window);
        ps.setSemitones(12.0f);
        const auto in = signals::sine(sr, 2.0, 440.0, 0.5);
        std::vector<float> out(in.size());
        for (size_t i = 0; i < in.size(); ++i) {
            ps.writeInput(in[i]);
            out[i] = ps.read();
        }
        const double f = analysis::peakFrequency(out, sr, static_cast<size_t>(sr * 0.2), out.size());
        const auto mag = analysis::spectrum(out, static_cast<size_t>(sr * 0.2), out.size());
        const double m880 = analysis::magnitudeAt(mag, sr, 880.0), m440 = analysis::magnitudeAt(mag, sr, 440.0);
        // The two-tap shifter amplitude-modulates at 1/window (~13-16 Hz), so
        // the 880 Hz line comes with sidebands; measure the band, not one bin.
        INFO("window " << window << " ms: peak " << f << " Hz, 880 vs 440: " << analysis::toDb(m880 / m440) << " dB");
        CHECK(std::fabs(f - 880.0) < 20.0);
        CHECK(m880 > m440 * 10.0);
    }
}

TEST_CASE("Shimmer: other intervals land where expected")
{
    const double sr = 48000.0;
    struct Case { float st; double hz; };
    for (auto c : { Case { 7.0f, 440.0 * std::exp2(7.0 / 12.0) }, Case { -12.0f, 220.0 }, Case { 19.0f, 440.0 * std::exp2(19.0 / 12.0) } }) {
        PitchShifter ps;
        ps.prepare(sr, 67.0f);
        ps.setSemitones(c.st);
        const auto in = signals::sine(sr, 2.0, 440.0, 0.5);
        std::vector<float> out(in.size());
        for (size_t i = 0; i < in.size(); ++i) {
            ps.writeInput(in[i]);
            out[i] = ps.read();
        }
        const double f = analysis::peakFrequency(out, sr, static_cast<size_t>(sr * 0.2), out.size());
        INFO(c.st << " st: peak " << f << " Hz, expected " << c.hz);
        CHECK(std::fabs(f - c.hz) < 20.0);
    }
}

TEST_CASE("Shimmer: loop gain is below 1 under 100% and continuous through it")
{
    for (float fb = 0.0f; fb <= 1.0f; fb += 0.01f)
        CHECK(loopGainForFeedback(fb) < 1.0f);
    CHECK(std::fabs(loopGainForFeedback(1.0f) - loopGainForFeedback(1.0001f)) < 0.01f);
    // Perceptual: 50->100% spans as many "sustain doublings" as 0->50%.
    auto sustain = [](float fb) { return 1.0 / (1.0 - loopGainForFeedback(fb)); };
    const double low = std::log(sustain(0.5f) / sustain(0.0f));
    const double high = std::log(sustain(1.0f) / sustain(0.5f));
    CHECK(std::fabs(low - high) / low < 0.2);
}

TEST_CASE("Shimmer: 10 minutes of pink noise at Feedback 110% and Decay infinite stays below 0 dBFS", "[slow]")
{
    const double sr = 48000.0;
    GlintEngine e;
    e.prepare(sr, 512);
    EngineParams p;
    p.feedback = 1.1f;
    p.decayS = EngineParams::kInfiniteDecay;
    p.mix = 1.0f;
    p.shimmerBlend = 0.9f;
    p.routing = Routing::GrainsInLoop;
    p.grainsInLoop = 0.5f;
    e.setEngineParams(p);
    e.reset(1);

    const int block = 512;
    const auto noiseL = signals::pink(sr, 10.0, 0.5, 1); // loop 10 s of noise
    const auto noiseR = signals::pink(sr, 10.0, 0.5, 2);
    std::vector<float> l(block), r(block);
    double peak = 0.0;
    size_t nonFinite = 0;
    const size_t total = static_cast<size_t>(sr * 600.0);
    for (size_t pos = 0; pos < total; pos += block) {
        for (int i = 0; i < block; ++i) {
            const size_t k = (pos + static_cast<size_t>(i)) % noiseL.size();
            l[static_cast<size_t>(i)] = noiseL[k];
            r[static_cast<size_t>(i)] = noiseR[k];
        }
        e.process(l.data(), r.data(), block);
        for (int i = 0; i < block; ++i) {
            const float a = l[static_cast<size_t>(i)], b = r[static_cast<size_t>(i)];
            if (!std::isfinite(a) || !std::isfinite(b)) ++nonFinite;
            else peak = std::max(peak, static_cast<double>(std::max(std::fabs(a), std::fabs(b))));
        }
    }
    INFO("peak " << analysis::toDb(peak) << " dBFS, engine non-finite resets " << e.nonFiniteCount());
    CHECK(nonFinite == 0);
    CHECK(e.nonFiniteCount() == 0);
    CHECK(peak < 1.0);
}

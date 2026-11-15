// SPDX-License-Identifier: AGPL-3.0-or-later
// Phase 2: RT60 at 1 kHz within +-15% of Decay (1..30 s), no CPU spike on
// long silence, L/R tail correlation below 0.3.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#include "../tools/common/Analysis.h"
#include "dsp/Diffuser.h"
#include "dsp/Fdn.h"

using namespace glint;

namespace {

struct Ir {
    std::vector<float> l, r;
};

Ir impulseResponse(double sr, const FdnSettings& s, double seconds, bool stereoIn = true)
{
    Fdn fdn;
    Diffuser diff;
    fdn.prepare(sr);
    diff.prepare(sr);
    fdn.setSettings(s);
    fdn.reset();
    const size_t n = static_cast<size_t>(sr * seconds);
    Ir ir { std::vector<float>(n), std::vector<float>(n) };
    for (size_t i = 0; i < n; ++i) {
        float l = i == 0 ? 1.0f : 0.0f;
        float r = (i == 0 && stereoIn) ? 1.0f : 0.0f;
        diff.process(l, r, 0.8f, 1.0f);
        fdn.process(l, r, ir.l[i], ir.r[i]);
    }
    return ir;
}

} // namespace

TEST_CASE("Reverb: RT60 at 1 kHz is within 15% of Decay from 1 to 30 s")
{
    const double sr = 48000.0;
    for (float decay : { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 30.0f }) {
        FdnSettings s;
        s.decayS = decay;
        s.size = 0.7f;
        s.dampingHz = 9000.0f;
        s.modDepth = 0.3f;
        const auto ir = impulseResponse(sr, s, std::max(2.0, decay * 0.8));
        // Third-octave band at 1 kHz (Q ~ 4.32), standard for RT60 per band.
        const auto band = analysis::bandpass(ir.l, sr, 1000.0, 4.32);
        const double t = analysis::rt60(band, sr, -5.0, -25.0);
        INFO("decay " << decay << " s -> measured RT60 " << t << " s");
        CHECK(t > decay * 0.85);
        CHECK(t < decay * 1.15);
    }
}

TEST_CASE("Reverb: L/R tail correlation below 0.3")
{
    const double sr = 48000.0;
    FdnSettings s;
    s.decayS = 4.0f;
    for (bool stereo : { true, false }) {
        const auto ir = impulseResponse(sr, s, 3.0, stereo);
        const double c = analysis::correlation(ir.l, ir.r, static_cast<size_t>(sr * 0.05), static_cast<size_t>(sr * 3.0));
        INFO((stereo ? "L+R impulse" : "L-only impulse") << ": correlation " << c);
        CHECK(std::fabs(c) < 0.3);
    }
}

TEST_CASE("Reverb: no CPU spike while the tail decays into 60 s of silence")
{
    const double sr = 48000.0;
    Fdn fdn;
    Diffuser diff;
    fdn.prepare(sr);
    diff.prepare(sr);
    FdnSettings s;
    s.decayS = 8.0f;
    fdn.setSettings(s);
    std::vector<double> perSecond;
    float l, r;
    ScopedFlushDenormals ftz;
    for (int sec = 0; sec < 60; ++sec) {
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < static_cast<int>(sr); ++i) {
            float a = (sec == 0 && i == 0) ? 1.0f : 0.0f, b = a;
            diff.process(a, b, 0.8f, 1.0f);
            fdn.process(a, b, l, r);
        }
        perSecond.push_back(std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    }
    auto sorted = perSecond;
    std::sort(sorted.begin(), sorted.end());
    const double median = sorted[sorted.size() / 2];
    const double worst = sorted.back();
    INFO("median " << median * 1000 << " ms/s, worst " << worst * 1000 << " ms/s");
    CHECK(worst < median * 3.0);
    CHECK(std::isfinite(l));
}

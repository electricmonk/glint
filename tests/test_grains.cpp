// SPDX-License-Identifier: AGPL-3.0-or-later
// Phase 1: grain engine renders at 44.1/48/96 kHz are non-silent, finite and
// seed-deterministic; Freeze and Size = infinite behave like Beads.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "../tools/common/Analysis.h"
#include "../tools/common/Signals.h"
#include "dsp/GlintEngine.h"
#include "dsp/GrainEngine.h"

using namespace glint;

namespace {

struct Stereo {
    std::vector<float> l, r;
};

GrainParams defaultGrains()
{
    GrainParams p;
    p.time = 0.05f;
    p.sizeMs = 250.0f;
    p.densityHz = 15.0f;
    p.randomness = 1.0f;
    p.randTime = 0.0f;
    p.spread = 0.8f;
    return p;
}

// Records `input` for its length, then `tail` seconds of silence.
Stereo renderGrains(double sr, const GrainParams& p, uint32_t seed, const std::vector<float>& input, double tail,
                    int block = 256, const GrainParams* afterInput = nullptr)
{
    GrainEngine g;
    g.prepare(sr, 16.0);
    g.setParams(p);
    g.reset(seed);
    const size_t n = input.size() + static_cast<size_t>(tail * sr);
    std::vector<float> in(n, 0.0f);
    std::copy(input.begin(), input.end(), in.begin());
    Stereo out { std::vector<float>(n, 0.0f), std::vector<float>(n, 0.0f) };
    for (size_t pos = 0; pos < n; pos += static_cast<size_t>(block)) {
        if (afterInput && pos >= input.size())
            g.setParams(*afterInput);
        const int len = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), n - pos));
        g.process(in.data() + pos, in.data() + pos, out.l.data() + pos, out.r.data() + pos, len);
    }
    return out;
}

} // namespace

TEST_CASE("Grains: non-silent, finite and seed-deterministic at 44.1, 48 and 96 kHz")
{
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        INFO("sample rate " << sr);
        const auto input = signals::pink(sr, 3.0, 0.5, 11);
        const auto a = renderGrains(sr, defaultGrains(), 42, input, 1.0);
        const auto b = renderGrains(sr, defaultGrains(), 42, input, 1.0);
        const auto c = renderGrains(sr, defaultGrains(), 43, input, 1.0);

        const double rmsDb = analysis::toDb(analysis::rms(a.l, static_cast<size_t>(sr)));
        INFO("rms " << rmsDb << " dB");
        CHECK(rmsDb > -40.0);
        CHECK(analysis::countNonFinite(a.l) + analysis::countNonFinite(a.r) == 0);
        CHECK(a.l == b.l);
        CHECK(a.r == b.r);
        CHECK(a.l != c.l);
    }
}

TEST_CASE("Grains: output does not depend on block size")
{
    const double sr = 48000.0;
    const auto input = signals::pink(sr, 2.0, 0.5, 5);
    const auto a = renderGrains(sr, defaultGrains(), 7, input, 0.5, 1);
    const auto b = renderGrains(sr, defaultGrains(), 7, input, 0.5, 4096);
    const auto c = renderGrains(sr, defaultGrains(), 7, input, 0.5, 333);
    CHECK(a.l == b.l);
    CHECK(a.l == c.l);
}

TEST_CASE("Grains: Freeze keeps playing the frozen buffer after the input stops")
{
    const double sr = 48000.0;
    const auto input = signals::sine(sr, 3.0, 330.0, 0.5);
    GrainParams frozen = defaultGrains();
    frozen.freeze = true;
    // Not frozen: once the buffer region grains read from is silent, output dies.
    const auto live = renderGrains(sr, defaultGrains(), 1, input, 6.0);
    // Frozen at the end of the input: grains keep reading the sine.
    const auto held = renderGrains(sr, defaultGrains(), 1, input, 6.0, 256, &frozen);

    const size_t from = static_cast<size_t>(sr * 7.0), to = static_cast<size_t>(sr * 9.0);
    const double liveDb = analysis::toDb(analysis::rms(live.l, from, to));
    const double heldDb = analysis::toDb(analysis::rms(held.l, from, to));
    INFO("live " << liveDb << " dB, frozen " << heldDb << " dB");
    CHECK(liveDb < -80.0);
    CHECK(heldDb > -30.0);
    CHECK(std::fabs(analysis::peakFrequency(held.l, sr, from, to) - 330.0) < 5.0);
}

TEST_CASE("Grains: Size = infinite acts as a delay that loops when frozen")
{
    const double sr = 48000.0;
    const auto input = signals::sine(sr, 2.0, 220.0, 0.5);
    GrainParams p = defaultGrains();
    p.sizeMs = GrainParams::kInfiniteSizeMs;
    p.time = 0.03f; // ~0.5 s behind the write head
    GrainParams frozen = p;
    frozen.freeze = true;
    const auto out = renderGrains(sr, p, 3, input, 8.0, 256, &frozen);
    CHECK(analysis::countNonFinite(out.l) == 0);
    const size_t from = static_cast<size_t>(sr * 6.0), to = static_cast<size_t>(sr * 9.0);
    const double db = analysis::toDb(analysis::rms(out.l, from, to));
    INFO("rms after 4+ s frozen: " << db << " dB");
    CHECK(db > -30.0);
    CHECK(analysis::peak(out.l) < 2.0);
}

TEST_CASE("Grains: never more than 32 active, rate follows density")
{
    const double sr = 48000.0;
    GrainParams p = defaultGrains();
    p.densityHz = 130.0f;
    p.sizeMs = 2000.0f;
    GrainEngine g;
    g.prepare(sr, 16.0);
    g.setParams(p);
    g.reset(9);
    std::vector<float> in(static_cast<size_t>(sr * 4.0), 0.1f), l(in.size(), 0.0f), r(in.size(), 0.0f);
    int maxActive = 0;
    for (size_t pos = 0; pos < in.size(); pos += 256) {
        g.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 256);
        maxActive = std::max(maxActive, g.activeGrains());
    }
    CHECK(maxActive <= GrainEngine::kMaxGrains);
    CHECK(maxActive >= 24);
    // Mean rate within 15% over 4 s.
    const double rate = static_cast<double>(g.grainsStarted()) / 4.0;
    INFO("rate " << rate);
    CHECK(rate > 130.0 * 0.85);
    CHECK(rate < 130.0 * 1.15);
}

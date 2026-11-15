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

TEST_CASE("Grains: Direction plays grains forwards, backwards or both")
{
    // Record a rising sweep, then play single 400 ms grains from it. Forward
    // grains rise in pitch, reverse grains fall; Both gives a mix.
    const double sr = 48000.0;
    std::vector<float> sweep(static_cast<size_t>(sr * 3.0));
    double ph = 0.0;
    for (size_t i = 0; i < sweep.size(); ++i) {
        const double t = static_cast<double>(i) / sr;
        ph += 2.0 * 3.14159265358979 * 300.0 * std::exp2(t * 1.5) / sr; // 300 Hz rising 1.5 octaves/s
        sweep[i] = static_cast<float>(0.5 * std::sin(ph));
    }
    auto risingShare = [&](float reverse) {
        GrainParams p = defaultGrains();
        p.time = 0.4f; p.sizeMs = 400.0f; p.densityHz = 2.0f; p.randomness = 0.0f; p.spread = 0.0f;
        p.shape = 0.5f; p.reverse = reverse;
        GrainParams frozen = p;
        frozen.freeze = true;
        const auto out = renderGrains(sr, p, 3, sweep, 6.0, 256, &frozen);
        // Zero-crossing rate in 50 ms windows, compared within each grain.
        int rising = 0, falling = 0;
        const size_t w = static_cast<size_t>(sr * 0.05), grain = static_cast<size_t>(sr * 0.5);
        for (size_t start = sweep.size() + grain; start + grain < out.l.size(); start += grain) {
            auto zc = [&](size_t a) {
                int n = 0;
                for (size_t i = a + 1; i < a + w; ++i)
                    n += (out.l[i - 1] < 0.0f) != (out.l[i] < 0.0f);
                return n;
            };
            // Find this grain's loud part: compare its two quarters.
            size_t best = start;
            double bestE = 0.0;
            for (size_t a = start; a + 4 * w < start + grain; a += w / 2) {
                const double e = analysis::rms(out.l, a, a + 4 * w);
                if (e > bestE) { bestE = e; best = a; }
            }
            if (bestE < 1e-3)
                continue;
            const int z0 = zc(best), z1 = zc(best + 3 * w);
            if (z1 > z0 + 1) ++rising;
            else if (z0 > z1 + 1) ++falling;
        }
        INFO("reverse " << reverse << ": rising " << rising << ", falling " << falling);
        CHECK(rising + falling >= 6);
        return static_cast<double>(rising) / std::max(1, rising + falling);
    };
    CHECK(risingShare(0.0f) > 0.9);
    CHECK(risingShare(1.0f) < 0.1);
    const double both = risingShare(0.5f);
    CHECK(both > 0.15);
    CHECK(both < 0.85);
}

TEST_CASE("Grains: switching grains off fades their output and sends the input straight to the reverb")
{
    const double sr = 48000.0;
    GlintEngine e;
    MacroValues m;
    m.mix = 1.0f; m.bloom = 0.0f; m.density = 1.0f;
    m.grainsOn = false;
    e.setMacros(m);
    e.prepare(sr, 512);
    auto l = signals::pink(sr, 3.0, 0.3, 4);
    auto r = signals::pink(sr, 3.0, 0.3, 5);
    for (size_t pos = 0; pos < l.size(); pos += 512)
        e.process(l.data() + pos, r.data() + pos, 512);
    int grains = 0;
    GrainEvent ev;
    while (e.grainEvents().pop(ev))
        if (ev.level > 0.0f)
            ++grains;
    INFO("grain events with grains off: " << grains);
    CHECK(grains == 0);
    // The reverb still gets the input.
    CHECK(analysis::rms(l, static_cast<size_t>(sr), l.size()) > 0.01);
}

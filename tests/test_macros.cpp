// SPDX-License-Identifier: AGPL-3.0-or-later
// Phase 4: automation sweeps of every macro pass a click detector; a 1 s
// Bloom 0 -> 110 -> 0 sweep while Density, Spread and Stereo density move
// passes it with peaks below 0 dBFS.
#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <string>
#include <vector>

#include "../tools/common/Analysis.h"
#include "../tools/common/Signals.h"
#include "dsp/GlintEngine.h"
#include "dsp/Macros.h"

using namespace glint;

namespace {

constexpr double kSr = 48000.0;
constexpr int kBlock = 256;

using Automation = std::function<void(MacroValues&, double t)>;

struct Out {
    std::vector<float> l, r;
};

Out render(const Automation& automate, double seconds, const MacroValues& start = {})
{
    GlintEngine e;
    MacroValues m = start;
    e.setMacros(m);
    e.prepare(kSr, kBlock);
    // Sustained two-tone input: predictable, so any click in the output stands
    // out in the linear-prediction residual.
    auto l = signals::sine(kSr, seconds, 220.0, 0.2);
    const auto b = signals::sine(kSr, seconds, 330.0, 0.1);
    for (size_t i = 0; i < l.size(); ++i) l[i] += b[i];
    Out o { l, l };
    for (size_t pos = 0; pos < l.size(); pos += kBlock) {
        automate(m, static_cast<double>(pos) / kSr);
        e.setMacros(m);
        e.process(o.l.data() + pos, o.r.data() + pos, kBlock);
    }
    return o;
}

// Triangle 0 -> 1 -> 0 between t0 and t0 + dur.
double tri(double t, double t0, double dur)
{
    const double u = (t - t0) / dur;
    if (u <= 0.0 || u >= 1.0) return 0.0;
    return u < 0.5 ? 2.0 * u : 2.0 - 2.0 * u;
}

void checkNoClicks(const Out& o, const std::string& what)
{
    // Ignore the first 0.5 s (engine and grains starting up).
    const size_t skip = static_cast<size_t>(kSr * 0.5);
    std::vector<float> l(o.l.begin() + static_cast<long>(skip), o.l.end());
    std::vector<float> r(o.r.begin() + static_cast<long>(skip), o.r.end());
    const auto cl = analysis::detectClicks(l, kSr);
    const auto cr = analysis::detectClicks(r, kSr);
    // Diagnostic: the worst residual ratio at any level (the check itself
    // ignores residual spikes below 0.001 absolute).
    const auto any = analysis::detectClicks(l, kSr, 12.0, 1e-7);
    INFO(what << ": worst ratio at any level " << any.worstRatio);
    INFO(what << ": clicks L " << cl.clicks << " (worst ratio " << cl.worstRatio << " at "
              << (static_cast<double>(cl.worstIndex + skip) / kSr) << " s), R " << cr.clicks << " (worst " << cr.worstRatio << ")");
    CHECK(cl.clicks == 0);
    CHECK(cr.clicks == 0);
    CHECK(analysis::countNonFinite(o.l) + analysis::countNonFinite(o.r) == 0);
}

} // namespace

TEST_CASE("Macros: baseline render has no clicks")
{
    checkNoClicks(render([](MacroValues&, double) {}, 4.0), "static defaults");
}

TEST_CASE("Macros: every continuous macro sweeps without clicks")
{
    struct Sweep { const char* name; float lo, hi; float MacroValues::*field; };
    const Sweep sweeps[] = {
        { "bloom", 0.0f, 1.1f, &MacroValues::bloom },
        { "space", 0.0f, 1.0f, &MacroValues::space },
        { "density", 0.0f, 1.0f, &MacroValues::density },
        { "drift", 0.0f, 1.0f, &MacroValues::drift },
        { "spread", 0.0f, 1.0f, &MacroValues::spread },
        { "stereoDensity", -1.0f, 1.0f, &MacroValues::stereoDensity },
        { "tone", -1.0f, 1.0f, &MacroValues::tone },
        { "mix", 0.0f, 1.0f, &MacroValues::mix },
    };
    for (const auto& s : sweeps) {
        for (double dur : { 1.0, 0.1 }) { // a normal and a very fast sweep
            const auto o = render([&](MacroValues& m, double t) {
                m.*(s.field) = s.lo + (s.hi - s.lo) * static_cast<float>(tri(t, 1.0, dur));
            }, 4.0);
            checkNoClicks(o, std::string(s.name) + " sweep over " + std::to_string(dur) + " s");
        }
    }
}

TEST_CASE("Macros: Shimmer and Grains switches and Direction changes are click-free")
{
    MacroValues start;
    start.bloom = 0.8f;
    start.density = 0.7f;
    checkNoClicks(render([](MacroValues& m, double t) {
        m.shimmerOn = (static_cast<int>(t / 0.7) % 2) == 0;
    }, 4.0, start), "shimmer on/off");
    checkNoClicks(render([](MacroValues& m, double t) {
        m.grainsOn = (static_cast<int>(t / 0.7) % 2) == 0;
    }, 4.0, start), "grains on/off");
    checkNoClicks(render([](MacroValues& m, double t) {
        m.reverse = static_cast<float>(static_cast<int>(t / 0.6) % 3) * 0.5f;
    }, 4.0, start), "direction changes");
}

TEST_CASE("Macros: switching Shimmer, Texture and Freeze is click-free")
{
    checkNoClicks(render([](MacroValues& m, double t) {
        m.shimmer = static_cast<ShimmerInterval>(static_cast<int>(t / 0.7) % 4);
    }, 4.0), "shimmer interval changes");
    checkNoClicks(render([](MacroValues& m, double t) {
        const int k = static_cast<int>(t / 0.6) % 3;
        m.texture = k == 0 ? Texture::Clean : (k == 1 ? Texture::Tape : Texture::Dust);
    }, 4.0), "texture changes");
    checkNoClicks(render([](MacroValues& m, double t) {
        m.freeze = (static_cast<int>(t / 0.8) % 2) == 1;
    }, 5.0), "freeze toggles");
}

TEST_CASE("Macros: 1 s Bloom 0->110->0 while Density, Spread, Stereo density move")
{
    const auto o = render([](MacroValues& m, double t) {
        const float u = static_cast<float>(tri(t, 1.0, 1.0));
        m.bloom = 1.1f * u;
        m.density = 0.2f + 0.7f * static_cast<float>(tri(t, 0.8, 1.4));
        m.spread = 1.0f - 0.8f * static_cast<float>(tri(t, 1.1, 0.9));
        m.stereoDensity = static_cast<float>(std::sin(t * 5.0));
    }, 4.0, MacroValues {});
    checkNoClicks(o, "combined Bloom sweep");
    const double peak = std::max(analysis::peak(o.l), analysis::peak(o.r));
    INFO("peak " << analysis::toDb(peak) << " dBFS");
    CHECK(peak < 1.0);
}

TEST_CASE("Macros: mapping matches the plan's table")
{
    MacroValues m;
    m.bloom = 0.0f; m.space = 0.0f; m.density = 0.0f; m.drift = 0.0f; m.tone = -1.0f; m.spread = 0.0f;
    auto lo = macros::map(m);
    CHECK(lo.shimmerBlend == 0.4f);
    CHECK(std::abs(lo.decayS - 1.5f) < 1e-3f);
    CHECK(std::abs(lo.reverbSize - 0.3f) < 1e-5f);
    CHECK(std::abs(lo.preDelayMs - 10.0f) < 1e-3f);
    CHECK(lo.grainRateHz == 0.0f);
    CHECK(std::abs(lo.sizeMs - 900.0f) < 1e-2f);
    CHECK(std::abs(lo.randTime - 0.1f) < 1e-5f);
    CHECK(std::abs(lo.loopHighCutHz - 3000.0f) < 1.0f);
    CHECK(std::abs(lo.dampingHz - 4000.0f) < 1.0f);
    CHECK(std::abs(lo.width - 0.8f) < 1e-5f);
    CHECK(lo.grainsInLoop == 0.0f);

    // Bloom, Space and Drift at 100% reach 65% of their underlying curves
    // (macros::kRangeScale); the rest reach the ends of theirs.
    m.bloom = 1.0f; m.space = 1.0f; m.density = 1.0f; m.drift = 1.0f; m.tone = 1.0f; m.spread = 1.0f;
    auto hi = macros::map(m);
    CHECK(std::abs(hi.feedback - 0.65f) < 1e-5f);
    CHECK(std::abs(hi.reverbSize - (0.3f + 0.7f * 0.65f)) < 1e-5f);
    CHECK(std::abs(hi.preDelayMs - (10.0f + 70.0f * 0.65f)) < 1e-3f);
    // Longest tail below the Bloom zone: ~10.5 s from Space, x1.6 from Bloom.
    INFO("decay at Bloom 100 / Space 100: " << hi.decayS << " s");
    CHECK(hi.decayS > 15.0f);
    CHECK(hi.decayS < 18.0f);
    CHECK(std::abs(hi.grainRateHz - 40.0f) < 1e-3f);
    CHECK(std::abs(hi.sizeMs - 150.0f) < 1e-2f);
    CHECK(std::abs(hi.randTime - (0.1f + 0.6f * 0.65f)) < 1e-5f);
    CHECK(std::abs(hi.pitchJitterSt - 0.3f * 0.65f) < 1e-5f);
    CHECK(hi.reverse == 0.0f); // Drift doesn't reverse grains; Direction does
    for (float rv : { 0.0f, 0.3f, 0.5f, 1.0f }) {
        m.reverse = rv;
        CHECK(macros::map(m).reverse == rv);
    }
    m.reverse = 0.0f;
    CHECK(std::abs(hi.loopHighCutHz - 14000.0f) < 2.0f);
    CHECK(std::abs(hi.dampingHz - 16000.0f) < 2.0f);
    CHECK(std::abs(hi.width - 1.5f) < 1e-5f);
    CHECK(hi.grainsInLoop == 0.0f); // grains join the loop only in the Bloom zone

    // Bloom zone: 100 -> 110% runs the rest of the way to infinite hold, continuously.
    m.bloom = 1.05f;
    const auto mid = macros::map(m);
    CHECK(mid.feedback > 0.65f);
    CHECK(mid.feedback < 1.1f);
    CHECK(mid.grainsInLoop > 0.0f);
    CHECK(std::abs(macros::bloomDepth(1.0f) - macros::bloomDepth(1.0001f)) < 0.01f);

    m.bloom = 1.1f;
    auto bloom = macros::map(m);
    CHECK(bloom.decayS >= EngineParams::kInfiniteDecay - 0.01f);
    CHECK(std::abs(bloom.shimmerBlend - 0.9f) < 1e-5f);
}

TEST_CASE("Macros: the click detector catches real discontinuities (negative control)")
{
    auto o = render([](MacroValues&, double) {}, 3.0);
    // A 0.05 step (about -26 dBFS) at 2 s, as a bad parameter jump would cause.
    const size_t at = static_cast<size_t>(kSr * 2.0);
    for (size_t i = at; i < o.l.size(); ++i) o.l[i] += 0.05f;
    const std::vector<float> l(o.l.begin() + static_cast<long>(kSr * 0.5), o.l.end());
    CHECK(analysis::detectClicks(l, kSr).clicks >= 1);

    // And a hard gain jump of -6 dB on the signal itself.
    auto o2 = render([](MacroValues&, double) {}, 3.0);
    for (size_t i = at; i < o2.l.size(); ++i) o2.l[i] *= 0.5f;
    const std::vector<float> l2(o2.l.begin() + static_cast<long>(kSr * 0.5), o2.l.end());
    const auto rep = analysis::detectClicks(l2, kSr);
    INFO("gain-jump worst ratio " << rep.worstRatio);
    CHECK(rep.clicks >= 1);
}

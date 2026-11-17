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
#include "dsp/SpectralShifter.h"
#include "dsp/ShimmerLoop.h"

using namespace glint;

namespace {
std::vector<float> shift(const std::vector<float>& in, double sr, float stA, float stB = 0.0f, float levelB = 0.0f)
{
    SpectralShifter ps;
    ps.prepare(sr);
    ps.setShifts(stA, stB, levelB);
    std::vector<float> out(in.size());
    for (size_t i = 0; i < in.size(); ++i)
        out[i] = ps.process(in[i]);
    return out;
}

// Share of the 300 Hz - 12 kHz energy within +-4 Hz (or 2.5 bins) of the given lines.
double tonalShare(const std::vector<float>& x, double sr, size_t from, std::initializer_list<double> lines)
{
    const auto mag = analysis::spectrum(x, from, x.size());
    const double binHz = sr / static_cast<double>(2 * mag.size());
    const double tol = std::max(4.0, 2.5 * binHz);
    double tot = 0.0, ton = 0.0;
    for (size_t k = 0; k < mag.size(); ++k) {
        const double f = static_cast<double>(k) * binHz, e = mag[k] * mag[k];
        if (f < 300.0 || f > 12000.0)
            continue;
        tot += e;
        for (double l : lines)
            if (std::fabs(f - l) < tol)
                ton += e;
    }
    return ton / std::max(tot, 1e-30);
}
} // namespace

TEST_CASE("Shimmer: +12 st shifts 440 Hz to a clean 880 Hz at every rate")
{
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 }) {
        const auto out = shift(signals::sine(sr, 2.0, 440.0, 0.5), sr, 12.0f);
        const size_t from = static_cast<size_t>(sr * 0.3);
        const double f = analysis::peakFrequency(out, sr, from, out.size());
        const double share = tonalShare(out, sr, from, { 880.0 });
        const double level = analysis::rms(out, from, out.size()) / (0.5 / std::sqrt(2.0));
        // Steady level: 50 ms RMS frames vary by less than 0.5 dB (the old
        // two-tap shifter wobbled by several dB at ~15 Hz).
        double lo = 1e9, hi = 0.0;
        const size_t frame = static_cast<size_t>(sr * 0.05);
        for (size_t p = from; p + frame <= out.size(); p += frame) {
            const double r = analysis::rms(out, p, p + frame);
            lo = std::min(lo, r);
            hi = std::max(hi, r);
        }
        INFO(sr << " Hz: peak " << f << " Hz, tonal share " << share * 100.0 << " %, level " << analysis::toDb(level)
                << " dB, ripple " << analysis::toDb(hi / lo) << " dB");
        CHECK(std::fabs(f - 880.0) < 3.0);
        CHECK(share > 0.99);
        CHECK(std::fabs(analysis::toDb(level)) < 1.0);
        CHECK(analysis::toDb(hi / lo) < 0.5);
    }
}

TEST_CASE("Shimmer: other intervals land where expected")
{
    const double sr = 48000.0;
    struct Case { float st; double hz; };
    for (auto c : { Case { 7.0f, 440.0 * std::exp2(7.0 / 12.0) }, Case { -12.0f, 220.0 }, Case { 19.0f, 440.0 * std::exp2(19.0 / 12.0) } }) {
        const auto out = shift(signals::sine(sr, 2.0, 440.0, 0.5), sr, c.st);
        const size_t from = static_cast<size_t>(sr * 0.3);
        const double f = analysis::peakFrequency(out, sr, from, out.size());
        INFO(c.st << " st: peak " << f << " Hz, expected " << c.hz);
        CHECK(std::fabs(f - c.hz) < 3.0);
    }
    // Two voices: Oct + 5th puts both lines in, B at its level.
    const auto out = shift(signals::sine(sr, 2.0, 440.0, 0.5), sr, 12.0f, 19.0f, 0.6f);
    const auto mag = analysis::spectrum(out, static_cast<size_t>(sr * 0.3), out.size());
    const double a = analysis::magnitudeAt(mag, sr, 880.0), b = analysis::magnitudeAt(mag, sr, 440.0 * std::exp2(19.0 / 12.0));
    INFO("B vs A: " << analysis::toDb(b / a) << " dB");
    CHECK(std::fabs(analysis::toDb(b / a) - analysis::toDb(0.6)) < 1.5);
}

TEST_CASE("Shimmer: 0 st keeps a chord's partials and the noise level")
{
    const double sr = 48000.0;
    SpectralShifter ps;
    ps.prepare(sr);
    ps.setShifts(0.0f, 0.0f, 0.0f);
    // A C-major chord: each partial keeps its level and nothing else appears.
    const double hz[3] = { 261.6, 329.6, 392.0 };
    std::vector<float> in(static_cast<size_t>(sr * 3.0));
    for (size_t i = 0; i < in.size(); ++i) {
        const double t = static_cast<double>(i) / sr;
        double v = 0.0;
        for (double f : hz)
            v += 0.2 * std::sin(2.0 * 3.14159265358979 * f * t);
        in[i] = static_cast<float>(v);
    }
    std::vector<float> out(in.size());
    for (size_t i = 0; i < in.size(); ++i)
        out[i] = ps.process(in[i]);
    const size_t from = static_cast<size_t>(sr * 0.5);
    const auto mi = analysis::spectrum(in, from, in.size()), mo = analysis::spectrum(out, from, out.size());
    for (double f : hz) {
        const double d = analysis::toDb(analysis::magnitudeAt(mo, sr, f, 16384, 8.0) / analysis::magnitudeAt(mi, sr, f, 16384, 8.0));
        INFO(f << " Hz partial: " << d << " dB");
        CHECK(std::fabs(d) < 0.5);
    }
    // Tonal share uses the 300 Hz - 12 kHz band; widen it with the chord root.
    double tot = 0.0, ton = 0.0;
    for (size_t k = 0; k < mo.size(); ++k) {
        const double f = static_cast<double>(k) * sr / 16384.0, e = mo[k] * mo[k];
        if (f < 100.0 || f > 12000.0)
            continue;
        tot += e;
        for (double h : hz)
            if (std::fabs(f - h) < 8.0)
                ton += e;
    }
    INFO("chord tonal share " << 100.0 * ton / tot << " %, latency " << ps.latency());
    CHECK(ton / tot > 0.999);

    // Noise: within 4 dB. Overlapping frames of noise add in power, not in
    // amplitude, so a phase vocoder loses ~3 dB on noise while tones keep
    // their level; in the shimmer loop that thins the diffuse part slightly.
    ps.reset();
    const auto noise = signals::pink(sr, 3.0, 0.3, 7);
    std::vector<float> no(noise.size());
    for (size_t i = 0; i < noise.size(); ++i)
        no[i] = ps.process(noise[i]);
    const size_t lat = static_cast<size_t>(ps.latency());
    const double g = analysis::rms(no, from, no.size()) / analysis::rms(noise, from - lat, noise.size() - lat);
    INFO("pink noise level " << analysis::toDb(g) << " dB");
    CHECK(std::fabs(analysis::toDb(g)) < 4.0);
}

TEST_CASE("Shimmer: loop gain is below 1 under 100% and continuous through it")
{
    for (float fb = 0.0f; fb <= 1.0f; fb += 0.01f)
        CHECK(loopGainForFeedback(fb) < 1.0f);
    CHECK(std::fabs(loopGainForFeedback(1.0f) - loopGainForFeedback(1.0001f)) < 0.01f);
    // Gentle lower half: at 50% the shifted signal loses about a third per
    // pass; most of the build-up happens above 70%.
    CHECK(loopGainForFeedback(0.5f) > 0.65f);
    CHECK(loopGainForFeedback(0.5f) < 0.78f);
    CHECK(loopGainForFeedback(0.7f) > 0.85f);
    for (float fb = 0.01f; fb <= 1.1f; fb += 0.01f)
        CHECK(loopGainForFeedback(fb) > loopGainForFeedback(fb - 0.01f));
}

TEST_CASE("Shimmer: switching the shimmer off bypasses the reverb")
{
    // 2 s of a 440 Hz tone into Bloom 80% with no grains, then silence. With
    // the shimmer on there's a long tail with an octave in it; switched off
    // the wet signal has no reverb at all, so the tail is silent.
    const double sr = 48000.0;
    struct Tail { double rmsDb, octaveDb; };
    auto tail = [&](bool on) {
        GlintEngine e;
        MacroValues m;
        m.bloom = 0.8f; m.mix = 1.0f; m.density = 0.0f; m.drift = 0.0f;
        m.shimmerOn = on;
        e.setMacros(m);
        e.prepare(sr, 512);
        auto l = signals::sine(sr, 2.0, 440.0, 0.25);
        l.resize(static_cast<size_t>(sr * 6.0), 0.0f);
        auto r = l;
        for (size_t pos = 0; pos < l.size(); pos += 512)
            e.process(l.data() + pos, r.data() + pos, static_cast<int>(std::min<size_t>(512, l.size() - pos)));
        const size_t from = static_cast<size_t>(sr * 3.0);
        const auto mag = analysis::spectrum(l, from, l.size());
        return Tail { analysis::toDb(analysis::rms(l, from, l.size())),
                      analysis::toDb(analysis::magnitudeAt(mag, sr, 880.0) / analysis::magnitudeAt(mag, sr, 440.0)) };
    };
    const auto on = tail(true), off = tail(false);
    INFO("tail 1-4 s after the note: shimmer on " << on.rmsDb << " dBFS (octave " << on.octaveDb
                                                  << " dB vs 440), off " << off.rmsDb << " dBFS");
    CHECK(on.rmsDb > -40.0);
    CHECK(on.octaveDb > -12.0);
    CHECK(off.rmsDb < -100.0);
}

TEST_CASE("Shimmer: with the shimmer off the grains still play")
{
    // Grains on, shimmer off: the wet signal is the grains alone.
    const double sr = 48000.0;
    GlintEngine e;
    MacroValues m;
    m.mix = 1.0f; m.density = 0.6f; m.shimmerOn = false;
    e.setMacros(m);
    e.prepare(sr, 512);
    auto l = signals::sine(sr, 3.0, 440.0, 0.25);
    auto r = l;
    for (size_t pos = 0; pos < l.size(); pos += 512)
        e.process(l.data() + pos, r.data() + pos, static_cast<int>(std::min<size_t>(512, l.size() - pos)));
    const double level = analysis::toDb(analysis::rms(l, static_cast<size_t>(sr * 1.0), l.size()));
    INFO("wet level with grains only: " << level << " dBFS");
    CHECK(level > -30.0);
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

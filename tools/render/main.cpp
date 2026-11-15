// SPDX-License-Identifier: AGPL-3.0-or-later
// glint-render: WAV in (or a generated signal) + JSON of parameters -> WAV out.
//
//   glint-render case.json [-o out.wav] [--sr 96000] [--seed N]
//   glint-render --bench [--seconds 30] [--sr 48000] [--block 256] [case.json]
//   glint-render case.json --preset 2      (start from a factory preset)
//
// Case format (all fields optional):
// {
//   "sampleRate": 48000, "seconds": 10, "blockSize": 256, "seed": 1,
//   "input": { "type": "pink|sine|notes|impulse|silence|file", "freq": 440,
//              "level": -12, "seconds": 2, "path": "in.wav" },
//   "macros": { "bloom": 0.6, "shimmer": "octave", "texture": "clean", ... },
//   "engine": { "feedback": 0.5, "routing": "serial", ... },   // instead of macros
//   "automation": [ { "macro": "bloom", "from": 0, "to": 1.1, "start": 1, "end": 2 } ],
//   "events": [ { "t": 3.0, "macros": { "freeze": true } } ]
// }
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "../common/Analysis.h"
#include "../common/Json.h"
#include "../common/Signals.h"
#include "../common/Wav.h"
#include "dsp/GlintEngine.h"
#include "plugin/Presets.h"

using namespace glint;

namespace {

bool applyMacro(MacroValues& m, const std::string& k, const json::Value& v)
{
    const double x = v.isNumber() ? v.num : (v.isBool() ? (v.b ? 1.0 : 0.0) : 0.0);
    if (k == "bloom") m.bloom = static_cast<float>(x);
    else if (k == "space") m.space = static_cast<float>(x);
    else if (k == "density") m.density = static_cast<float>(x);
    else if (k == "drift") m.drift = static_cast<float>(x);
    else if (k == "spread") m.spread = static_cast<float>(x);
    else if (k == "stereoDensity") m.stereoDensity = static_cast<float>(x);
    else if (k == "tone") m.tone = static_cast<float>(x);
    else if (k == "mix") m.mix = static_cast<float>(x);
    else if (k == "freeze") m.freeze = x != 0.0;
    else if (k == "shimmer") {
        if (v.isString()) {
            if (v.str == "octave") m.shimmer = ShimmerInterval::Octave;
            else if (v.str == "fifth") m.shimmer = ShimmerInterval::Fifth;
            else if (v.str == "oct+5th") m.shimmer = ShimmerInterval::OctFifth;
            else if (v.str == "sub+oct") m.shimmer = ShimmerInterval::SubOct;
            else return false;
        } else m.shimmer = static_cast<ShimmerInterval>(static_cast<int>(x));
    } else if (k == "texture") {
        if (v.isString()) {
            if (v.str == "clean") m.texture = Texture::Clean;
            else if (v.str == "tape") m.texture = Texture::Tape;
            else if (v.str == "dust") m.texture = Texture::Dust;
            else return false;
        } else m.texture = static_cast<Texture>(static_cast<int>(x));
    } else return false;
    return true;
}

bool applyEngine(EngineParams& p, const std::string& k, const json::Value& v)
{
    if (v.isString()) return p.setString(k, v.str);
    return p.set(k, v.isBool() ? (v.b ? 1.0 : 0.0) : v.num);
}

float macroGet(const MacroValues& m, const std::string& k)
{
    if (k == "bloom") return m.bloom;
    if (k == "space") return m.space;
    if (k == "density") return m.density;
    if (k == "drift") return m.drift;
    if (k == "spread") return m.spread;
    if (k == "stereoDensity") return m.stereoDensity;
    if (k == "tone") return m.tone;
    if (k == "mix") return m.mix;
    return 0.0f;
}

struct Case {
    json::Value root;
    double sr = 48000.0;
    double seconds = 10.0;
    int block = 256;
    uint32_t seed = 1;
    int preset = -1;
};

std::vector<float> makeInput(const json::Value* in, double sr, double total, uint32_t seed, std::vector<float>* right)
{
    std::string type = in ? in->string("type", "pink") : "pink";
    const double level = in ? in->number("level", -12.0) : -12.0;
    const double amp = std::pow(10.0, level / 20.0);
    const double secs = in ? in->number("seconds", total) : total;
    std::vector<float> l;
    if (type == "sine") l = signals::sine(sr, secs, in ? in->number("freq", 440.0) : 440.0, amp);
    else if (type == "impulse") l = signals::impulse(sr, secs, amp);
    else if (type == "notes") l = signals::notes(sr, secs, amp);
    else if (type == "silence") l.assign(static_cast<size_t>(sr * secs), 0.0f);
    else if (type == "file") {
        wav::Audio a;
        if (!wav::read(in->string("path", ""), a)) {
            std::fprintf(stderr, "cannot read input file\n");
            std::exit(2);
        }
        l = a.l;
        *right = a.r;
    } else l = signals::pink(sr, secs, amp, seed);
    if (type != "file") {
        // Decorrelated right channel for noise, identical for tones/impulses.
        *right = type == "pink" ? signals::pink(sr, secs, amp, seed + 101) : l;
    }
    const size_t n = static_cast<size_t>(sr * total);
    l.resize(n, 0.0f);
    right->resize(n, 0.0f);
    return l;
}

int run(const Case& c, const std::string& outPath, bool bench)
{
    GlintEngine engine;
    engine.prepare(c.sr, c.block);

    MacroValues macros;
    EngineParams params;
    const bool engineMode = c.root.find("engine") != nullptr;
    if (c.preset >= 0 && c.preset < static_cast<int>(presets::kFactory.size())) {
        const auto& p = presets::kFactory[static_cast<size_t>(c.preset)];
        macros.bloom = p.bloom * 0.01f; macros.space = p.space * 0.01f; macros.density = p.density * 0.01f;
        macros.drift = p.drift * 0.01f; macros.spread = p.spread * 0.01f; macros.stereoDensity = p.stereoDensity * 0.01f;
        macros.tone = p.tone * 0.01f; macros.mix = p.mix * 0.01f;
        macros.shimmer = static_cast<ShimmerInterval>(p.shimmer);
        macros.texture = p.texture == 1 ? Texture::Tape : (p.texture == 2 ? Texture::Dust : Texture::Clean);
    }
    if (const auto* m = c.root.find("macros"))
        for (const auto& [k, v] : m->obj)
            if (!applyMacro(macros, k, v)) std::fprintf(stderr, "unknown macro %s\n", k.c_str());
    if (const auto* e = c.root.find("engine"))
        for (const auto& [k, v] : e->obj)
            if (!applyEngine(params, k, v)) std::fprintf(stderr, "unknown engine param %s\n", k.c_str());
    if (engineMode) engine.setEngineParams(params);
    else engine.setMacros(macros);
    engine.reset(c.seed);

    std::vector<float> inR;
    std::vector<float> inL = makeInput(c.root.find("input"), c.sr, c.seconds, c.seed, &inR);
    std::vector<float> L = inL, R = inR;

    const json::Value* automation = c.root.find("automation");
    const json::Value* events = c.root.find("events");
    std::vector<bool> fired(events ? events->arr.size() : 0, false);
    const MacroValues base = macros;

    const auto t0 = std::chrono::steady_clock::now();
    const size_t n = L.size();
    for (size_t pos = 0; pos < n; pos += static_cast<size_t>(c.block)) {
        const int len = static_cast<int>(std::min<size_t>(static_cast<size_t>(c.block), n - pos));
        const double t = static_cast<double>(pos) / c.sr;
        bool changed = false;
        if (events) {
            for (size_t i = 0; i < events->arr.size(); ++i) {
                const auto& ev = events->arr[i];
                if (!fired[i] && t >= ev.number("t", 0.0)) {
                    fired[i] = true;
                    changed = true;
                    if (const auto* m = ev.find("macros"))
                        for (const auto& [k, v] : m->obj) applyMacro(macros, k, v);
                    if (const auto* e = ev.find("engine"))
                        for (const auto& [k, v] : e->obj) applyEngine(params, k, v);
                }
            }
        }
        if (automation) {
            for (const auto& a : automation->arr) {
                const std::string k = a.string("macro", "");
                const double s = a.number("start", 0.0), e = a.number("end", 1.0);
                const double from = a.number("from", macroGet(base, k)), to = a.number("to", from);
                if (t < s) continue;
                // Optional "points": [[t, v], ...] piecewise-linear.
                double v;
                if (const auto* pts = a.find("points"); pts && pts->isArray() && !pts->arr.empty()) {
                    v = pts->arr.back().arr[1].num;
                    for (size_t i = 1; i < pts->arr.size(); ++i) {
                        const double t0p = pts->arr[i - 1].arr[0].num, t1p = pts->arr[i].arr[0].num;
                        if (t <= t1p) {
                            const double u = t1p > t0p ? std::clamp((t - t0p) / (t1p - t0p), 0.0, 1.0) : 1.0;
                            v = pts->arr[i - 1].arr[1].num + (pts->arr[i].arr[1].num - pts->arr[i - 1].arr[1].num) * u;
                            break;
                        }
                    }
                } else {
                    const double u = e > s ? std::clamp((t - s) / (e - s), 0.0, 1.0) : 1.0;
                    v = from + (to - from) * u;
                }
                json::Value jv;
                jv.type = json::Value::Type::Number;
                jv.num = v;
                applyMacro(macros, k, jv);
                changed = true;
            }
        }
        if (changed) {
            if (engineMode) engine.setEngineParams(params);
            else engine.setMacros(macros);
        }
        engine.process(L.data() + pos, R.data() + pos, len);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(t1 - t0).count();
    const double cpu = 100.0 * elapsed / c.seconds;

    const double peakDb = analysis::toDb(std::max(analysis::peak(L), analysis::peak(R)));
    const double rmsDb = analysis::toDb(0.5 * (analysis::rms(L) + analysis::rms(R)));
    const size_t nonFinite = analysis::countNonFinite(L) + analysis::countNonFinite(R);
    const double centroid = analysis::spectralCentroid(L, c.sr);

    std::printf("{ \"sampleRate\": %.0f, \"seconds\": %.2f, \"peakDb\": %.2f, \"rmsDb\": %.2f, "
                "\"centroidHz\": %.1f, \"nonFinite\": %zu, \"engineNonFinite\": %llu, "
                "\"cpuPercentOneCore\": %.3f }\n",
                c.sr, c.seconds, peakDb, rmsDb, centroid, nonFinite,
                static_cast<unsigned long long>(engine.nonFiniteCount()), cpu);

    if (!bench && !outPath.empty()) {
        wav::Audio a;
        a.sampleRate = c.sr;
        a.l = std::move(L);
        a.r = std::move(R);
        if (!wav::write(outPath, a)) {
            std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
            return 1;
        }
    }
    return nonFinite == 0 ? 0 : 3;
}

} // namespace

int main(int argc, char** argv)
{
    std::string casePath, outPath;
    bool bench = false;
    double srOverride = 0.0, secondsOverride = 0.0;
    int blockOverride = 0;
    long seedOverride = -1;
    int presetOverride = -1;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) outPath = argv[++i];
        else if (a == "--bench") bench = true;
        else if (a == "--preset" && i + 1 < argc) presetOverride = std::atoi(argv[++i]);
        else if (a == "--sr" && i + 1 < argc) srOverride = std::atof(argv[++i]);
        else if (a == "--seconds" && i + 1 < argc) secondsOverride = std::atof(argv[++i]);
        else if (a == "--block" && i + 1 < argc) blockOverride = std::atoi(argv[++i]);
        else if (a == "--seed" && i + 1 < argc) seedOverride = std::atol(argv[++i]);
        else if (a == "-h" || a == "--help") {
            std::puts("usage: glint-render [case.json] [-o out.wav] [--sr N] [--seconds S] [--block N] [--seed N] [--bench]");
            return 0;
        } else casePath = a;
    }

    Case c;
    if (!casePath.empty()) {
        std::ifstream f(casePath);
        if (!f) { std::fprintf(stderr, "cannot open %s\n", casePath.c_str()); return 2; }
        std::stringstream ss;
        ss << f.rdbuf();
        try { c.root = json::parse(ss.str()); }
        catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 2; }
    } else {
        c.root.type = json::Value::Type::Object;
    }
    c.sr = c.root.number("sampleRate", 48000.0);
    c.seconds = c.root.number("seconds", bench ? 30.0 : 10.0);
    c.block = static_cast<int>(c.root.number("blockSize", 256));
    c.seed = static_cast<uint32_t>(c.root.number("seed", 1));
    if (srOverride > 0) c.sr = srOverride;
    if (secondsOverride > 0) c.seconds = secondsOverride;
    if (blockOverride > 0) c.block = blockOverride;
    if (seedOverride >= 0) c.seed = static_cast<uint32_t>(seedOverride);
    c.preset = presetOverride >= 0 ? presetOverride : static_cast<int>(c.root.number("preset", -1));
    if (outPath.empty() && !bench) outPath = "render.wav";
    return run(c, outPath, bench);
}

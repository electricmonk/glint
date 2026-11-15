// SPDX-License-Identifier: AGPL-3.0-or-later
// Measurement helpers shared by glint-render and the tests.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace glint::analysis {

constexpr double kPi = 3.14159265358979323846;

inline double rms(const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min(to, x.size());
    if (to <= from) return 0.0;
    double s = 0.0;
    for (size_t i = from; i < to; ++i) s += static_cast<double>(x[i]) * x[i];
    return std::sqrt(s / static_cast<double>(to - from));
}

inline double peak(const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min(to, x.size());
    double p = 0.0;
    for (size_t i = from; i < to; ++i) p = std::max(p, static_cast<double>(std::fabs(x[i])));
    return p;
}

inline size_t countNonFinite(const std::vector<float>& x)
{
    size_t n = 0;
    for (float v : x) n += std::isfinite(v) ? 0 : 1;
    return n;
}

inline double toDb(double g) { return 20.0 * std::log10(std::max(g, 1e-12)); }

inline void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * kPi / static_cast<double>(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0);
            for (size_t j = 0; j < len / 2; ++j) {
                const auto u = a[i + j];
                const auto v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Averaged magnitude spectrum (Hann windows of size n, 50% overlap).
inline std::vector<double> spectrum(const std::vector<float>& x, size_t from, size_t to, size_t n = 16384)
{
    std::vector<double> mag(n / 2, 0.0);
    to = std::min(to, x.size());
    int frames = 0;
    std::vector<std::complex<double>> buf(n);
    for (size_t start = from; start + n <= to; start += n / 2) {
        for (size_t i = 0; i < n; ++i) {
            const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(n));
            buf[i] = { x[start + i] * w, 0.0 };
        }
        fft(buf);
        for (size_t i = 0; i < n / 2; ++i) mag[i] += std::abs(buf[i]);
        ++frames;
    }
    if (frames > 0)
        for (auto& m : mag) m /= frames;
    return mag;
}

inline double peakFrequency(const std::vector<float>& x, double sr, size_t from, size_t to, double minHz = 20.0)
{
    const size_t n = 16384;
    const auto mag = spectrum(x, from, to, n);
    size_t best = 0;
    for (size_t i = static_cast<size_t>(minHz * n / sr); i < mag.size(); ++i)
        if (mag[i] > mag[best]) best = i;
    return static_cast<double>(best) * sr / static_cast<double>(n);
}

// RMS-summed magnitude within +-bandHz of hz.
inline double magnitudeAt(const std::vector<double>& mag, double sr, double hz, size_t n = 16384, double bandHz = 25.0)
{
    const double binHz = sr / static_cast<double>(n);
    const size_t lo = static_cast<size_t>(std::max(0.0, (hz - bandHz) / binHz));
    const size_t hi = std::min(mag.size() - 1, static_cast<size_t>((hz + bandHz) / binHz));
    double e = 0.0;
    for (size_t i = lo; i <= hi; ++i) e += mag[i] * mag[i];
    return std::sqrt(e);
}

inline double spectralCentroid(const std::vector<float>& x, double sr)
{
    const size_t n = 4096;
    if (x.size() < n) return 0.0;
    const auto mag = spectrum(x, 0, x.size(), n);
    double num = 0.0, den = 0.0;
    for (size_t i = 1; i < mag.size(); ++i) {
        const double f = static_cast<double>(i) * sr / static_cast<double>(n);
        num += f * mag[i];
        den += mag[i];
    }
    return den > 0.0 ? num / den : 0.0;
}

// RBJ band-pass (constant 0 dB peak gain).
inline std::vector<float> bandpass(const std::vector<float>& x, double sr, double hz, double q)
{
    const double w0 = 2.0 * kPi * hz / sr;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    const double b0 = alpha / a0, b2 = -alpha / a0;
    const double a1 = -2.0 * std::cos(w0) / a0, a2 = (1.0 - alpha) / a0;
    std::vector<float> y(x.size());
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        const double v = b0 * x[i] + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x[i]; y2 = y1; y1 = v;
        y[i] = static_cast<float>(v);
    }
    return y;
}

// RT60 from an impulse response via Schroeder integration (T20 x 3 by default:
// fit between -5 and -25 dB). Returns seconds, or -1 if the range isn't reached.
inline double rt60(const std::vector<float>& ir, double sr, double fromDb = -5.0, double toDb_ = -25.0)
{
    std::vector<double> edc(ir.size());
    double acc = 0.0;
    for (size_t i = ir.size(); i-- > 0;) {
        acc += static_cast<double>(ir[i]) * ir[i];
        edc[i] = acc;
    }
    if (acc <= 0.0) return -1.0;
    const double total = edc[0];
    // Linear regression on the dB curve between the two levels.
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    size_t n = 0;
    bool reached = false;
    for (size_t i = 0; i < edc.size(); ++i) {
        const double db = 10.0 * std::log10(edc[i] / total);
        if (db > fromDb) continue;
        if (db < toDb_) { reached = true; break; }
        const double t = static_cast<double>(i) / sr;
        sx += t; sy += db; sxx += t * t; sxy += t * db; ++n;
    }
    if (!reached || n < 10) return -1.0;
    const double slope = (static_cast<double>(n) * sxy - sx * sy) / (static_cast<double>(n) * sxx - sx * sx);
    return slope < 0.0 ? -60.0 / slope : -1.0;
}

inline double correlation(const std::vector<float>& a, const std::vector<float>& b, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min({ to, a.size(), b.size() });
    double sab = 0, saa = 0, sbb = 0;
    for (size_t i = from; i < to; ++i) {
        sab += static_cast<double>(a[i]) * b[i];
        saa += static_cast<double>(a[i]) * a[i];
        sbb += static_cast<double>(b[i]) * b[i];
    }
    return (saa > 0 && sbb > 0) ? sab / std::sqrt(saa * sbb) : 0.0;
}

// Click detector: flags samples whose second difference jumps far above the
// local (50 ms) RMS of the second difference. Returns number of clicks found.
struct ClickReport {
    size_t clicks = 0;
    double worstRatio = 0.0;
    size_t worstIndex = 0;
};

inline ClickReport detectClicks(const std::vector<float>& x, double sr, double ratioThreshold = 12.0, double absThreshold = 0.02)
{
    ClickReport rep;
    const size_t n = x.size();
    if (n < 4) return rep;
    std::vector<float> d(n, 0.0f);
    for (size_t i = 2; i < n; ++i) d[i] = x[i] - 2.0f * x[i - 1] + x[i - 2];
    const size_t win = std::max<size_t>(16, static_cast<size_t>(sr * 0.05));
    double acc = 0.0;
    std::vector<double> sq(n);
    for (size_t i = 0; i < n; ++i) sq[i] = static_cast<double>(d[i]) * d[i];
    // Centered moving RMS.
    std::vector<double> prefix(n + 1, 0.0);
    for (size_t i = 0; i < n; ++i) prefix[i + 1] = prefix[i] + sq[i];
    size_t lastClick = 0;
    for (size_t i = 2; i < n; ++i) {
        const size_t a = i > win ? i - win : 0;
        const size_t b = std::min(n, i + win);
        acc = (prefix[b] - prefix[a]) / static_cast<double>(b - a);
        const double local = std::sqrt(acc) + 1e-9;
        const double ratio = std::fabs(d[i]) / local;
        if (std::fabs(d[i]) > absThreshold && ratio > ratioThreshold) {
            if (rep.clicks == 0 || i - lastClick > win) ++rep.clicks;
            lastClick = i;
        }
        if (ratio > rep.worstRatio && std::fabs(d[i]) > absThreshold) {
            rep.worstRatio = ratio;
            rep.worstIndex = i;
        }
    }
    return rep;
}

} // namespace glint::analysis

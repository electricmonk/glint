// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dsp/SpectralShifter.h"

#include <algorithm>
#include <cmath>

namespace glint {

namespace {
constexpr float kTwoPi = 6.28318530718f;
inline float wrapPi(float x) noexcept { return x - kTwoPi * std::nearbyint(x / kTwoPi); }
} // namespace

void SpectralShifter::prepare(double sampleRate)
{
    // ~85 ms frames at any rate: 4096 up to 48 kHz, 8192 at 96, 16384 at 192.
    // Shorter frames smear neighbouring partials of low chords into each other.
    n_ = 4096;
    while (n_ < sampleRate * 0.081)
        n_ *= 2;
    hop_ = n_ / kOverlap;
    fft_.prepare(n_);
    window_.resize(static_cast<size_t>(n_));
    for (int i = 0; i < n_; ++i)
        window_[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(kTwoPi * static_cast<float>(i) / static_cast<float>(n_));
    const size_t bins = static_cast<size_t>(n_ / 2 + 1);
    inFifo_.assign(static_cast<size_t>(n_), 0.0f);
    outFifo_.assign(static_cast<size_t>(n_), 0.0f);
    accum_.assign(static_cast<size_t>(2 * n_), 0.0f);
    for (auto* v : { &lastPhase_, &anaMag_, &anaPhase_, &anaFreq_ })
        v->assign(bins, 0.0f);
    spec_.assign(static_cast<size_t>(n_), {});
    syn_.assign(bins, {});
    for (auto* v : { &peaks_, &regionLo_, &prevPeaks_, &nearestPrev_ })
        v->assign(bins, 0);
    for (int v = 0; v < 2; ++v) {
        peakPhase_[v].assign(bins, 0.0f);
        prevPeakPhase_[v].assign(bins, 0.0f);
    }
    reset();
}

void SpectralShifter::reset() noexcept
{
    std::fill(inFifo_.begin(), inFifo_.end(), 0.0f);
    std::fill(outFifo_.begin(), outFifo_.end(), 0.0f);
    std::fill(accum_.begin(), accum_.end(), 0.0f);
    std::fill(lastPhase_.begin(), lastPhase_.end(), 0.0f);
    numPeaks_ = numPrev_ = 0;
    rover_ = n_ - hop_;
}

void SpectralShifter::setShifts(float semitonesA, float semitonesB, float levelB) noexcept
{
    ratioA_ = std::exp2(std::clamp(semitonesA, -24.0f, 24.0f) / 12.0f);
    ratioB_ = std::exp2(std::clamp(semitonesB, -24.0f, 24.0f) / 12.0f);
    levelB_ = std::clamp(levelB, 0.0f, 1.0f);
}

float SpectralShifter::process(float x) noexcept
{
    inFifo_[static_cast<size_t>(rover_)] = x;
    const float y = outFifo_[static_cast<size_t>(rover_ - (n_ - hop_))];
    if (++rover_ >= n_) {
        rover_ = n_ - hop_;
        processFrame();
    }
    return y;
}

void SpectralShifter::processFrame() noexcept
{
    const int half = n_ / 2;
    const float expct = kTwoPi / static_cast<float>(kOverlap); // phase advance per hop, per bin

    // Analysis: magnitude, phase and true frequency (in bins) per bin.
    for (int i = 0; i < n_; ++i)
        spec_[static_cast<size_t>(i)] = { inFifo_[static_cast<size_t>(i)] * window_[static_cast<size_t>(i)], 0.0f };
    fft_.transform(spec_.data(), false);
    float maxMag = 0.0f;
    for (int k = 0; k <= half; ++k) {
        const size_t s = static_cast<size_t>(k);
        const float mag = std::abs(spec_[s]);
        const float ph = std::arg(spec_[s]);
        const float dev = wrapPi(ph - lastPhase_[s] - static_cast<float>(k) * expct);
        lastPhase_[s] = ph;
        anaMag_[s] = mag;
        anaPhase_[s] = ph;
        anaFreq_[s] = static_cast<float>(k) + dev / expct;
        maxMag = std::max(maxMag, mag);
    }

    // Peaks: local maxima over +-2 bins, above -90 dB of the frame maximum.
    numPeaks_ = 0;
    const float floor = maxMag * 3e-5f;
    for (int k = 2; k <= half - 2; ++k) {
        const float m = anaMag_[static_cast<size_t>(k)];
        if (m > floor && m > anaMag_[static_cast<size_t>(k - 1)] && m >= anaMag_[static_cast<size_t>(k + 1)]
            && m > anaMag_[static_cast<size_t>(k - 2)] && m >= anaMag_[static_cast<size_t>(k + 2)])
            peaks_[static_cast<size_t>(numPeaks_++)] = k;
    }
    // Regions split at the quietest bin between neighbouring peaks.
    for (int p = 0; p < numPeaks_; ++p) {
        if (p == 0) {
            regionLo_[0] = 0;
            continue;
        }
        int lo = peaks_[static_cast<size_t>(p - 1)], best = lo + 1;
        for (int k = lo + 1; k < peaks_[static_cast<size_t>(p)]; ++k)
            if (anaMag_[static_cast<size_t>(k)] < anaMag_[static_cast<size_t>(best)])
                best = k;
        regionLo_[static_cast<size_t>(p)] = best;
    }
    // Nearest peak of the previous frame, for phase continuity.
    for (int p = 0, q = 0; p < numPeaks_; ++p) {
        const int k = peaks_[static_cast<size_t>(p)];
        while (q + 1 < numPrev_ && std::abs(prevPeaks_[static_cast<size_t>(q + 1)] - k) <= std::abs(prevPeaks_[static_cast<size_t>(q)] - k))
            ++q;
        nearestPrev_[static_cast<size_t>(p)] = (numPrev_ > 0 && std::abs(prevPeaks_[static_cast<size_t>(q)] - k) <= 3) ? q : -1;
    }

    std::fill(syn_.begin(), syn_.end(), std::complex<float> {});
    const float norm = 1.0f / std::sqrt(1.0f + levelB_ * levelB_);
    shiftVoice(0, ratioA_, norm);
    shiftVoice(1, ratioB_, levelB_ * norm);

    // Synthesis (Hermitian spectrum, real output).
    for (int j = 0; j <= half; ++j)
        spec_[static_cast<size_t>(j)] = syn_[static_cast<size_t>(j)];
    spec_[0] = { spec_[0].real(), 0.0f };
    spec_[static_cast<size_t>(half)] = { spec_[static_cast<size_t>(half)].real(), 0.0f };
    for (int j = 1; j < half; ++j)
        spec_[static_cast<size_t>(n_ - j)] = std::conj(spec_[static_cast<size_t>(j)]);
    fft_.transform(spec_.data(), true);

    // Hann analysis x Hann synthesis at 4x overlap sums to 1.5.
    const float scale = 1.0f / (static_cast<float>(n_) * 1.5f);
    for (int i = 0; i < n_; ++i)
        accum_[static_cast<size_t>(i)] += spec_[static_cast<size_t>(i)].real() * window_[static_cast<size_t>(i)] * scale;
    std::copy(accum_.begin(), accum_.begin() + hop_, outFifo_.begin());
    std::copy(accum_.begin() + hop_, accum_.begin() + hop_ + n_, accum_.begin());
    std::fill(accum_.begin() + n_, accum_.end(), 0.0f);
    std::copy(inFifo_.begin() + hop_, inFifo_.end(), inFifo_.begin());

    std::copy(peaks_.begin(), peaks_.begin() + numPeaks_, prevPeaks_.begin());
    for (int v = 0; v < 2; ++v)
        std::copy(peakPhase_[v].begin(), peakPhase_[v].begin() + numPeaks_, prevPeakPhase_[v].begin());
    numPrev_ = numPeaks_;
}

void SpectralShifter::shiftVoice(int voice, float ratio, float level) noexcept
{
    const int half = n_ / 2;
    const float expct = kTwoPi / static_cast<float>(kOverlap);
    auto& phases = peakPhase_[voice];
    const auto& prev = prevPeakPhase_[voice];
    for (int p = 0; p < numPeaks_; ++p) {
        const size_t ps = static_cast<size_t>(p);
        const int k = peaks_[ps];
        const float f = anaFreq_[static_cast<size_t>(k)];
        const float target = f * ratio;
        const int delta = static_cast<int>(std::nearbyint(target - f));
        // Synthesis phase of the shifted peak: carried on from the previous
        // frame at the shifted frequency (or the analysis phase when new).
        const int q = nearestPrev_[ps];
        const float synPh = q >= 0 ? wrapPi(prev[static_cast<size_t>(q)] + target * expct) : anaPhase_[static_cast<size_t>(k)];
        phases[ps] = synPh;
        if (level <= 0.0f)
            continue;
        const std::complex<float> rot = std::polar(level, synPh - anaPhase_[static_cast<size_t>(k)]);
        const int lo = regionLo_[ps];
        const int hi = p + 1 < numPeaks_ ? regionLo_[ps + 1] : half + 1;
        for (int b = lo; b < hi; ++b) {
            const int j = b + delta;
            if (j < 0 || j > half)
                continue;
            syn_[static_cast<size_t>(j)] += spec_[static_cast<size_t>(b)] * rot;
        }
    }
}

} // namespace glint

// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <complex>
#include <vector>

#include "dsp/Fft.h"

namespace glint {

// Phase-vocoder pitch shifter for one channel with two voices (A, and B at
// a level) that share one analysis. Peak-region shifting with identity phase
// locking (Laroche & Dolson, 1999): each spectral peak and the bins around
// it move together by a whole number of bins, and the region is rotated by
// the phase the shifted peak frequency has accumulated. Steady tones shift
// without the amplitude modulation of a delay-line shifter. Transients smear
// over one frame (~85 ms), which a reverb loop hides. Each partial keeps a
// constant phase offset, so steady tones are not waveform-exact at 0 st.
//
// Latency is one frame (fftSize samples, ~85 ms at any rate); inside the
// shimmer loop it only adds to the loop delay.
class SpectralShifter {
public:
    static constexpr int kOverlap = 4;

    void prepare(double sampleRate);
    void reset() noexcept;
    // Ratios as semitones; levelB 0..1 (0 = A only).
    void setShifts(float semitonesA, float semitonesB, float levelB) noexcept;
    float process(float x) noexcept;

    int fftSize() const noexcept { return n_; }
    int latency() const noexcept { return n_; }

private:
    void processFrame() noexcept;
    void shiftVoice(int voice, float ratio, float level) noexcept;

    Fft fft_;
    int n_ = 4096, hop_ = 1024, rover_ = 0;
    float ratioA_ = 2.0f, ratioB_ = 2.0f, levelB_ = 0.0f;
    std::vector<float> window_, inFifo_, outFifo_, accum_;
    std::vector<float> lastPhase_, anaMag_, anaPhase_, anaFreq_;
    std::vector<std::complex<float>> spec_, syn_;
    // Peaks of this frame and the last: analysis bin, region start, and the
    // synthesis phase per voice (tracked from the nearest previous peak).
    std::vector<int> peaks_, regionLo_, prevPeaks_, nearestPrev_;
    std::vector<float> peakPhase_[2], prevPeakPhase_[2];
    int numPeaks_ = 0, numPrev_ = 0;
};

} // namespace glint

// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cmath>
#include <complex>
#include <vector>

namespace glint {

// Iterative radix-2 complex FFT. Tables are built in prepare(); transform()
// doesn't allocate.
class Fft {
public:
    void prepare(int size)
    {
        n_ = size;
        bitrev_.assign(static_cast<size_t>(n_), 0);
        int bits = 0;
        while ((1 << bits) < n_)
            ++bits;
        for (int i = 0; i < n_; ++i) {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                r |= ((i >> b) & 1) << (bits - 1 - b);
            bitrev_[static_cast<size_t>(i)] = r;
        }
        twiddle_.resize(static_cast<size_t>(n_ / 2));
        for (int i = 0; i < n_ / 2; ++i) {
            const double a = -2.0 * 3.14159265358979323846 * i / n_;
            twiddle_[static_cast<size_t>(i)] = { static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a)) };
        }
    }

    int size() const noexcept { return n_; }

    // In place. inverse = true computes the unscaled inverse transform.
    void transform(std::complex<float>* x, bool inverse) const noexcept
    {
        for (int i = 0; i < n_; ++i) {
            const int j = bitrev_[static_cast<size_t>(i)];
            if (j > i)
                std::swap(x[i], x[j]);
        }
        for (int len = 2; len <= n_; len <<= 1) {
            const int half = len >> 1;
            const int step = n_ / len;
            for (int i = 0; i < n_; i += len) {
                for (int k = 0; k < half; ++k) {
                    auto w = twiddle_[static_cast<size_t>(k * step)];
                    if (inverse)
                        w = std::conj(w);
                    const auto t = w * x[i + k + half];
                    x[i + k + half] = x[i + k] - t;
                    x[i + k] += t;
                }
            }
        }
    }

private:
    int n_ = 0;
    std::vector<int> bitrev_;
    std::vector<std::complex<float>> twiddle_;
};

} // namespace glint

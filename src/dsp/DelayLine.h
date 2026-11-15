// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace glint {

// Power-of-two circular delay line. Allocate in prepare(), never in process().
class DelayLine {
public:
    void prepare(int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 4)
            size <<= 1;
        buffer_.assign(static_cast<size_t>(size), 0.0f);
        mask_ = size - 1;
        write_ = 0;
    }
    void reset() noexcept
    {
        std::fill(buffer_.begin(), buffer_.end(), 0.0f);
        write_ = 0;
    }
    void write(float x) noexcept
    {
        buffer_[static_cast<size_t>(write_)] = x;
        write_ = (write_ + 1) & mask_;
    }
    // Integer delay >= 1 (1 = the sample written last).
    float tap(int delay) const noexcept { return buffer_[static_cast<size_t>((write_ - delay) & mask_)]; }

    float readLinear(float delay) const noexcept
    {
        const float fl = std::floor(delay);
        const float t = delay - fl;
        const int d = static_cast<int>(fl);
        const float a = tap(d);
        const float b = tap(d + 1);
        return a + (b - a) * t;
    }
    float readHermite(float delay) const noexcept
    {
        const float fl = std::floor(delay);
        const float t = delay - fl;
        const int d = static_cast<int>(fl);
        const float xm1 = tap(d - 1 < 1 ? 1 : d - 1);
        const float x0 = tap(d);
        const float x1 = tap(d + 1);
        const float x2 = tap(d + 2);
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float bNeg = w + a;
        return (((a * t) - bNeg) * t + c) * t + x0;
    }
    int capacity() const noexcept { return mask_ + 1 - 4; }

private:
    std::vector<float> buffer_;
    int mask_ = 0;
    int write_ = 0;
};

// Flush denormals to zero for the lifetime of the object (no JUCE).
class ScopedFlushDenormals {
public:
    ScopedFlushDenormals() noexcept
    {
#if defined(__aarch64__)
        uint64_t fpcr;
        __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
        old_ = fpcr;
        fpcr |= (1ull << 24); // FZ
        __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#elif defined(__x86_64__) || defined(__i386__)
        unsigned int csr;
        __asm__ __volatile__("stmxcsr %0" : "=m"(csr));
        old_ = csr;
        csr |= 0x8040; // FTZ | DAZ
        __asm__ __volatile__("ldmxcsr %0" : : "m"(csr));
#endif
    }
    ~ScopedFlushDenormals()
    {
#if defined(__aarch64__)
        uint64_t fpcr = old_;
        __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#elif defined(__x86_64__) || defined(__i386__)
        unsigned int csr = static_cast<unsigned int>(old_);
        __asm__ __volatile__("ldmxcsr %0" : : "m"(csr));
#endif
    }
    ScopedFlushDenormals(const ScopedFlushDenormals&) = delete;
    ScopedFlushDenormals& operator=(const ScopedFlushDenormals&) = delete;

private:
    unsigned long long old_ = 0;
};

} // namespace glint

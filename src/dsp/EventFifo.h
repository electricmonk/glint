// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace glint {

// What the visualiser needs to draw one grain.
struct GrainEvent {
    float pan = 0.0f;    // -1 (L) .. +1 (R)
    float age = 0.0f;    // 0 (just born) .. 1 (end of life)
    float level = 0.0f;  // envelope x gain; < 0 marks end of a frame
    bool shimmer = false;
};

// Single-producer / single-consumer lock-free ring buffer.
template <class T, size_t Capacity>
class SpscFifo {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    bool push(const T& v) noexcept
    {
        const size_t w = write_.load(std::memory_order_relaxed);
        const size_t r = read_.load(std::memory_order_acquire);
        if (w - r >= Capacity)
            return false;
        data_[w & (Capacity - 1)] = v;
        write_.store(w + 1, std::memory_order_release);
        return true;
    }
    bool pop(T& v) noexcept
    {
        const size_t r = read_.load(std::memory_order_relaxed);
        const size_t w = write_.load(std::memory_order_acquire);
        if (r == w)
            return false;
        v = data_[r & (Capacity - 1)];
        read_.store(r + 1, std::memory_order_release);
        return true;
    }
    size_t freeSpace() const noexcept
    {
        return Capacity - (write_.load(std::memory_order_relaxed) - read_.load(std::memory_order_acquire));
    }

private:
    std::array<T, Capacity> data_ {};
    std::atomic<size_t> write_ { 0 };
    std::atomic<size_t> read_ { 0 };
};

using GrainEventFifo = SpscFifo<GrainEvent, 4096>;

} // namespace glint

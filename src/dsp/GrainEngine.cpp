// SPDX-License-Identifier: AGPL-3.0-or-later
#include "dsp/GrainEngine.h"

#include <algorithm>
#include <cmath>

namespace glint {

void GrainEngine::Buffer::read(double delay, float& outL, float& outR) const noexcept
{
    const double pos = static_cast<double>(write) - delay;
    const double fl = std::floor(pos);
    const float t = static_cast<float>(pos - fl);
    int64_t i = static_cast<int64_t>(fl) - 1;
    i %= capacity_;
    if (i < 0)
        i += capacity_;
    int64_t idx[4];
    for (int k = 0; k < 4; ++k) {
        idx[k] = i;
        if (++i >= capacity_)
            i = 0;
    }
    auto herm = [t](float xm1, float x0, float x1, float x2) noexcept {
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float bNeg = w + a;
        return (((a * t) - bNeg) * t + c) * t + x0;
    };
    const auto* L = l.data();
    const auto* R = r.data();
    outL = herm(L[idx[0]], L[idx[1]], L[idx[2]], L[idx[3]]);
    outR = herm(R[idx[0]], R[idx[1]], R[idx[2]], R[idx[3]]);
}

void GrainEngine::prepare(double sampleRate, double maxBufferSeconds)
{
    sampleRate_ = sampleRate;
    buffer_.capacity_ = std::max<int64_t>(1024, static_cast<int64_t>(sampleRate * maxBufferSeconds));
    buffer_.l.assign(static_cast<size_t>(buffer_.capacity_), 0.0f);
    buffer_.r.assign(static_cast<size_t>(buffer_.capacity_), 0.0f);

    tailSize_ = std::clamp(static_cast<int>(sampleRate * 0.0055), 64, 2048);
    tailL_.assign(static_cast<size_t>(tailSize_), 0.0f);
    tailR_.assign(static_cast<size_t>(tailSize_), 0.0f);

    color_.prepare(sampleRate);
    pool_.init(static_cast<float>(sampleRate));
    rate_.prepare(sampleRate, 50.0f);
    spread_.prepare(sampleRate, 40.0f);
    eventInterval_ = std::max(1, static_cast<int>(sampleRate / 60.0));
    reset(1);
}

void GrainEngine::reset(uint32_t seed) noexcept
{
    std::fill(buffer_.l.begin(), buffer_.l.end(), 0.0f);
    std::fill(buffer_.r.begin(), buffer_.r.end(), 0.0f);
    buffer_.write = 0;
    buffer_.recorded = 0;
    buffer_.frozen_ = false;
    std::fill(tailL_.begin(), tailL_.end(), 0.0f);
    std::fill(tailR_.begin(), tailR_.end(), 0.0f);
    tailWrite_ = tailXfade_ = 0;
    wasFrozen_ = false;
    pool_.reset(static_cast<float>(sampleRate_));
    color_.reset();
    rng_.seed(seed);
    phase_ = 0.0;
    intervalScale_ = 1.0f;
    rate_.snap(params_.densityHz);
    spread_.snap(params_.spread);
    eventCounter_ = 0;
    grainsStarted_ = 0;
}

void GrainEngine::softReset() noexcept
{
    buffer_.recorded = 0;
    tailWrite_ = tailXfade_ = 0;
    wasFrozen_ = false;
    pool_.reset(static_cast<float>(sampleRate_));
    phase_ = 0.0;
}

void GrainEngine::setParams(const GrainParams& p) noexcept
{
    params_ = p;
    pendingTexture_ = p.texture;
    rate_.setTarget(std::max(0.0f, p.densityHz));
    spread_.setTarget(std::clamp(p.spread, 0.0f, 1.0f));
}

// Stereo density: pan = sign(u) * |u|^k, k = 4^(-tilt). tilt > 0 pushes to edges.
float GrainEngine::shapePan(float u) const noexcept
{
    const float tilt = std::clamp(params_.stereoDensity, -1.0f, 1.0f);
    const float k = std::pow(4.0f, -tilt);
    const float a = std::pow(std::fabs(u), k);
    return u < 0.0f ? -a : a;
}

void GrainEngine::spawnGrain(float fractional) noexcept
{
    const auto& p = params_;
    const bool infinite = p.sizeMs >= GrainParams::kInfiniteSizeMs;

    // Size (with randomisation), in samples.
    float sizeMs = infinite ? 4000.0f : std::clamp(p.sizeMs, 10.0f, 4000.0f);
    if (!infinite && p.randSize > 0.0f)
        sizeMs *= std::exp2(p.randSize * 2.0f * rng_.bipolar());
    float duration = sizeMs * 0.001f * static_cast<float>(sampleRate_);

    // Position.
    float t = std::clamp(p.time, 0.0f, 1.0f);
    if (p.randTime > 0.0f) {
        const float target = rng_.uniform();
        t += (target - t) * std::clamp(p.randTime * rng_.uniform() * 1.5f, 0.0f, 1.0f);
    }
    const double safety = std::max(16.0, 0.005 * sampleRate_) + 1.0;
    const double maxDelay = std::max(0.0, static_cast<double>(buffer_.recorded) - 2.0 * safety);
    const double delay = t * maxDelay + safety;

    // Pitch.
    float st = p.pitchSt;
    if (p.randPitch > 0.0f)
        st += p.randPitch * 12.0f * rng_.bipolar();
    if (p.pitchJitterSt > 0.0f)
        st += p.pitchJitterSt * rng_.bipolar();
    double speed = semitonesToRatio(std::clamp(st, -48.0f, 48.0f));
    if (p.reverse > 0.0f && rng_.uniform() < p.reverse)
        speed = -speed;

    const float amplitude = 1.0f / std::sqrt(static_cast<float>(pool_.countActive() + 1));
    const float pan = shapePan(rng_.bipolar());
    const bool shimmer = rng_.uniform() < shimmerShare_;
    const float shape = infinite ? 0.66f : std::clamp(p.shape, 0.0f, 1.0f);

    auto* grain = pool_.newGrain();
    grain->start(buffer_, delay, speed, duration, shape, amplitude, pan, fractional, shimmer);
    grain->setId(static_cast<uint32_t>(grainsStarted_ + 1));
    pool_.sort();
    ++grainsStarted_;
}

void GrainEngine::writeSample(float l, float r) noexcept
{
    const bool frozen = params_.freeze;
    if (frozen) {
        if (!wasFrozen_)
            tailWrite_ = 0;
        if (tailWrite_ < tailSize_) {
            tailL_[static_cast<size_t>(tailWrite_)] = l;
            tailR_[static_cast<size_t>(tailWrite_)] = r;
            ++tailWrite_;
        }
    } else {
        if (wasFrozen_) {
            tailXfade_ = tailWrite_;
            // Colour changes wait until Freeze is released.
            color_.setTexture(pendingTexture_);
        }
        if (tailWrite_ > 0 && tailXfade_ > 0) {
            const int pos = tailXfade_ - tailWrite_;
            const float g = beads_port::smoothStep(static_cast<float>(tailWrite_) / static_cast<float>(tailXfade_));
            l += (tailL_[static_cast<size_t>(pos)] - l) * g;
            r += (tailR_[static_cast<size_t>(pos)] - r) * g;
            --tailWrite_;
        }
        if (++buffer_.write >= buffer_.capacity_)
            buffer_.write = 0;
        buffer_.l[static_cast<size_t>(buffer_.write)] = l;
        buffer_.r[static_cast<size_t>(buffer_.write)] = r;
        if (buffer_.recorded < buffer_.capacity_)
            ++buffer_.recorded;
    }
    wasFrozen_ = frozen;
    buffer_.frozen_ = frozen;
}

void GrainEngine::pushEvents() noexcept
{
    if (fifo_ == nullptr)
        return;
    if (fifo_->freeSpace() < static_cast<size_t>(kMaxGrains + 1))
        return; // UI not reading: drop the frame.
    const float width = spread_.value();
    for (const auto& g : pool_.grains()) {
        if (g.done())
            continue;
        fifo_->push(GrainEvent { std::clamp(g.panUnit() * width, -1.0f, 1.0f), g.age01(), g.level(), g.shimmer(), g.id() });
    }
    fifo_->push(GrainEvent { 0.0f, 0.0f, -1.0f, false, 0 });
}

void GrainEngine::process(const float* writeL, const float* writeR, float* outL, float* outR, int n) noexcept
{
    if (!wasFrozen_ && !params_.freeze)
        color_.setTexture(pendingTexture_);

    const bool infinite = params_.sizeMs >= GrainParams::kInfiniteSizeMs;
    const float invSr = static_cast<float>(1.0 / sampleRate_);
    const float randomness = std::clamp(params_.randomness, 0.0f, 1.0f);

    for (int i = 0; i < n; ++i) {
        float wl = writeL[i], wr = writeR[i];
        color_.process(wl, wr);
        writeSample(wl, wr);

        // Trigger: smoothed rate, regular <-> Poisson intervals.
        const float hz = infinite ? 0.5f : rate_.next();
        const float width = spread_.next();
        if (hz > 0.0f && buffer_.recorded > 0) {
            phase_ += static_cast<double>(hz * invSr / intervalScale_);
            if (phase_ >= 1.0) {
                phase_ -= 1.0;
                const float frac = static_cast<float>(phase_ * intervalScale_ / (hz * invSr));
                // Next interval: mix of regular (1) and exponential (mean 1).
                const float u = std::clamp(rng_.uniform(), 1e-6f, 1.0f - 1e-6f);
                intervalScale_ = (1.0f - randomness) + randomness * -std::log(1.0f - u);
                intervalScale_ = std::max(intervalScale_, 0.05f);
                spawnGrain(std::clamp(frac, 0.0f, 0.999f));
            }
        }

        float l = 0.0f, r = 0.0f;
        for (auto& g : pool_.grains())
            g.render(buffer_, width, l, r);
        pool_.tick();
        outL[i] += l;
        outR[i] += r;

        if (++eventCounter_ >= eventInterval_) {
            eventCounter_ = 0;
            pushEvents();
        }
    }
}

} // namespace glint

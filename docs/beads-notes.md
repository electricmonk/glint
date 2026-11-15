# Beads audit notes

Source: `third_party/eurorack` @ f4cfa76, `beads/dsp/` and `stmlib/` @ e3bd7c9.
Files read: `granular_processor.h`, `granular_player.h`, `delay_granular_player.h`,
`delay_grain.h`, `grain_envelope.h`, `delay_line.h`, `randomized_parameter.h`,
`trigger_generator.cc`, `processor.h/.cc`, `stmlib/utils/random.h`.

## Findings

| Topic | What Beads does | Consequence for Glint |
| --- | --- | --- |
| Sample rate | Hard-coded 48 kHz (`processor.cc`: `Init(48000.0f, …)`, reverb, wow/flutter, codec). Tape modes run the granular core at 24 kHz (`ratio = 2`) with SRC filters designed for 48→24 | Every rate-dependent constant must be derived from the host rate. The `ratio` downsampling path and its fixed FIR coefficients are dropped |
| Block size | Fixed `kBlockSize` from the codec; `Process()` uses VLAs (`bool d_freeze[d_size]` etc.) sized by the block | VLAs are not allowed (stack, non-standard C++). The port processes per sample with no temporaries |
| Buffer memory | One 768 KB SRAM region (3 banks × 256 KB), `max_delay` a template constant derived from it. About 4 s stereo at 16-bit/48 kHz, 8 s at 8-bit/24 kHz | Replace with a float stereo buffer allocated in `prepare()`, 16 s at the host rate (24.6 MB at 192 kHz) |
| Fixed vs float | Audio is float, but storage is int16 / packed 12-bit / 8-bit µ-law ("quality" modes). Read positions are Q22.10 fixed point in `int32_t` | At 16 s × 192 kHz, positions reach 3.07 M samples ≪ 10 = 3.1 G, which overflows `int32_t`. The port uses `double` positions |
| Quality modes | Clean / Digital / Tape / Dust = storage resolution + SRC ratio + soft clip in the feedback path + wow/flutter | Glint's Texture applies colour on buffer writes (Tape: saturation + gentle LP; Dust: wow/flutter + faint hiss; Digital: 12-bit quantise) on a float buffer |
| RNG | `stmlib::Random`: a static (global) LCG shared by every instance | Two plugin instances would interfere, and renders would depend on order. The port uses a per-instance seeded xorshift RNG |
| Parameter scaling | All controls 0–1 (`RandomizedParameter`: base + attenuverter; negative attenuverter = random, positive = CV/LFO). Size < 1/3 = reverse; duration `= 2^(-108 st·shortness/12) × 4 s` | Glint maps its engine parameters (ms, %, st) directly; Size ∞ is handled as delay mode (one long grain re-reading at the write head) |
| Grain scheduler | 32 grains, priority list; on each trigger the 4 "safety" grains with least time left get forced fade-outs (5 ms / grain interval / 100 ms). Amplitude 1/√(active+1) | Keep: it is the heart of the smooth Beads sound. Grain count 32, oldest-first stealing kept |
| Envelope | Attack/sustain/release + smoothness morph from square → decay → bell → reverse-envelope, min attack 0.5 ms, min release 10 ms | Port as is (rate-independent already, given sample counts) |
| Head crossing | Grains are stopped when they hit the write head (`STOP` policy), with a 256-sample safety zone and fade | Keep; zone becomes ~5 ms in samples |
| Freeze | Writes stop; the last 256 samples go into a "freeze tail" crossfaded back in on unfreeze | Keep the tail crossfade, sized in ms |
| Triggers | `TriggerGenerator`: Poisson-like random pulse train or clocked divider; regularity feeds pan spread | Glint uses its own smoothed Poisson rate (Density macro) and its own pan law (Spread/Stereo density), as PLAN §4 asks |
| Pan | Random pan width = 0.5 × active grains × total randomness | Replaced by Spread / Stereo density |
| STM32-specific | `__attribute__((packed))` bitfields, bank-split addressing, `WasteCPUCycles`, CV/ADC/normalisation, calibration, UI | Not ported |
| Desktop build | Headers compile with clang once `stmlib` is available; no tests ship | — |

## Port strategy

Beads' DSP is tied to 48 kHz and to its memory layout, so a straight port of
`GranularProcessor` would mean rewriting most of it anyway. The port therefore
takes the parts that make Beads sound like Beads and leaves the hardware
plumbing behind:

- **Ported (MIT, `src/dsp/beads_port/`):** grain envelope, delay grain with
  head-crossing fade, granular player scheduling and voice stealing,
  freeze-tail crossfade, Hermite interpolation. Converted to float/double,
  sample-rate independent, per-instance RNG, no globals.
- **Written fresh (AGPL):** buffer storage, trigger rate (smoothed Poisson),
  pan law, texture modes, delay (Size ∞) mode.

## GrainEngine interface

```cpp
namespace glint {
struct GrainParams {           // targets; smoothed inside the engine
    float time = 0.3f;         // 0..1 of recorded buffer
    float sizeMs = 250.f;      // 10..4000; > 4000 = infinite (delay mode)
    float pitchSt = 0.f;       // ±24
    float shape = 0.5f;        // 0..1 (Beads envelope morph)
    float densityHz = 10.f;    // grain rate
    float randomness = 0.3f;   // 0 regular .. 1 Poisson
    float randTime = 0.2f, randSize = 0.f, randPitch = 0.f; // 0..1 (pitch: ±0.3 st at 1? no: ±24 st scale)
    float pitchJitterSt = 0.f; // ± semitones of extra jitter
    float reverse = 0.f;       // probability
    float spread = 0.6f;       // 0..1 pan width
    float stereoDensity = 0.f; // -1 centre .. +1 edges
    bool freeze = false;
    Texture texture = Texture::Clean;
};
class GrainEngine {
public:
    void prepare(double sampleRate, double maxBufferSeconds = 16.0);
    void reset(uint32_t seed = 1);
    void setParams(const GrainParams&) noexcept;
    // write = what to record (input or loop return); out = grain output (added).
    void process(const float* writeL, const float* writeR,
                 float* outL, float* outR, int n) noexcept;
    template <class F> void forEachEvent(F&&); // visualiser feed
};
}
```

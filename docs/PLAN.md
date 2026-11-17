# Glint — Granular Shimmer Reverb: Implementation Plan

Owner: Shai · Plan as of 2026-11-15 · Licence: AGPL-3.0-or-later

## 1. Concept

Glint (working name) is a stereo AU/VST3 effect for Logic Pro. It runs audio
through a Beads-style granular engine into a long, diffuse reverb. A
pitch-shifted feedback loop inside that reverb stacks octaves into the
shimmer. The signature sound comes from feeding that loop back into the grain
engine, so every pass gets re-granulated.

The plugin is deliberately opinionated. It's tuned for ethereal atmospheres
and exposes only 11 macro controls (section 5), not dozens of engine
parameters.

**Taken from Beads (via its open-source code):** a continuously recording
buffer that grains read from, Freeze, grain time / size / pitch / shape /
density with per-parameter randomization, and "quality" colour modes.

**Taken from Valhalla Shimmer (as inspiration only, no code):** pitch shifting
inside the reverb's feedback path, very long decays with high diffusion and
slow modulation, and filters inside the loop so octaves build up without
turning harsh or muddy.

**Product goals**
- Sounds musical before it sounds technically correct.
- Under 3% of one Apple-silicon core per instance at 48 kHz with default
  settings.
- No reported latency.
- Freeze, Bloom and automation never click.

## 2. Licensing and naming

| Item | Rule | What the build does |
| --- | --- | --- |
| Beads code | MIT, published in github.com/pichenettes/eurorack on 12 Oct 2026 | Pinned submodule. Ported files keep their MIT headers. Credited in THIRD_PARTY_NOTICES.md and on the About screen |
| "Mutable Instruments" / "Beads" names | Trademark; Gillet advises against reusing module names | Never in the product name, UI, presets or marketing. Credit only in the notices |
| Valhalla Shimmer | Closed source and a trademark | Sound reference only. No code, UI layout, preset names or the word "Valhalla" |
| JUCE 8 | AGPLv3 option (free, source must stay public) | Glint is licensed AGPL-3.0-or-later |
| VST3 SDK | MIT since version 3.8 | Compatible; listed in the notices |
| Fonts | Space Grotesk, Inter, JetBrains Mono under the OFL | Bundled; listed in the notices |
| Apple distribution | Developer ID certificate and notarization | Phase 7 only. Not needed for local use |

The Beads README says the code builds on desktop but hasn't been tested on
hardware. Phase 1 therefore begins with an audit, not a straight port.

## 3. Tech stack and repo layout

- C++20, JUCE 8 (AGPL), CMake presets `mac-debug` / `mac-release`.
- `juce_add_plugin` with `FORMATS AU VST3 Standalone`, universal binary
  (`CMAKE_OSX_ARCHITECTURES=arm64;x86_64`).
- Plugin type `aufx`. Supports stereo in/out and mono-in/stereo-out.
- **Rule: `src/dsp` never includes JUCE.** It stays testable headless.
- Tests: Catch2 for DSP units, plus a `glint-render` CLI (WAV in + JSON of
  parameters in → WAV out; `--bench` for CPU).
- Validation: `auval` and pluginval at strictness 10, wrapped in
  `scripts/validate.sh`.
- CI: GitHub Actions on macOS runs build → tests → pluginval.

```
glint/
  CMakeLists.txt, CMakePresets.json
  CLAUDE.md
  LICENSE                   AGPL-3.0
  THIRD_PARTY_NOTICES.md    Beads (MIT), JUCE (AGPL), VST3 SDK (MIT), fonts (OFL)
  docs/PLAN.md              this file
  docs/ui/UI-SPEC.md        UI spec
  docs/ui/glint-ui-v2.dc.html  UI mockup (reference only)
  docs/beads-notes.md       written in Phase 1
  third_party/
    JUCE/                   submodule, pinned
    eurorack/               submodule, pinned; only beads/ and stmlib/ used
  src/
    dsp/                    pure C++, no JUCE
      beads_port/           adapted Beads sources, MIT headers kept
      GrainEngine.h/.cpp    clean interface over beads_port
      SpectralShifter.h/.cpp, Fft.h
      Diffuser.h/.cpp
      Fdn.h/.cpp
      ShimmerLoop.h/.cpp
      Color.h/.cpp          texture modes: tape, dust (wow/flutter, hiss)
      Modulation.h/.cpp
      Smoothers.h
      Macros.h              macro -> engine mapping curves (one place to tune)
      Params.h              plain struct of engine target values
      GlintEngine.h/.cpp    owns everything, process(block)
    plugin/
      PluginProcessor.h/.cpp   APVTS (11 macros), state, playhead
      ParameterLayout.cpp
      PluginEditor.h/.cpp
    ui/                     knobs, chips, particle cloud
  tools/render/             glint-render CLI
  tests/                    Catch2 tests + render-cases/*.json
  presets/
  scripts/                  build.sh, validate.sh, sign-notarize.sh
```

## 4. DSP architecture

```
           +---------------------- dry (untouched) ----------------------+
           |                                                             v
Input --> Grain engine --> Diffusion --> FDN reverb ----------------> Output mix
              ^                            |    ^
              |                            v    |
              |                      Pitch shift (shimmer)
              |                            |    |
              |                            v    |
              +-- (Grains-in-loop) -- Loop filters + soft clip
```

The shimmer loop takes the reverb output, pitch-shifts and filters it, then
feeds it back into the reverb input. In Grains-in-loop mode the loop return
is also written into the grain buffer. The engine supports Serial, Parallel
and Grains-in-loop, but the UI doesn't expose routing: the Bloom macro
crossfades from Serial into Grains-in-loop from 70% upward.

**Grain engine (adapted from Beads)**
- Stereo recording buffer up to 16 s at the host rate.
- Controls: Time (read position), Size (10 ms to ∞, where ∞ is delay mode),
  Pitch, Shape, Density, each with a random amount; plus Reverse probability
  and pan.
- Freeze stops recording. Colour and channel-count changes wait until Freeze
  is released.
- At most 32 grains, with oldest-first stealing. A seeded RNG makes bounces
  repeatable.

**Texture / colour modes:** Clean, Tape (soft saturation, gentle top-end
loss) and Dust (wow, flutter, faint hiss). They apply to buffer writes. The
engine also keeps a Digital mode, but no macro uses it.

**Shimmer pitch shifter:** a phase vocoder (85 ms frames, 4× overlap) with
peak-region shifting and identity phase locking (Laroche & Dolson), in two
voices (A and B with its own level) sharing one analysis. Steady tones shift
without the amplitude wobble of a delay-line shifter. Its one-frame latency
only lengthens the loop. (Changed 2026-11-15: the first version used a
two-tap delay-line shifter, whose 13–16 Hz wobble made the shimmer grainy on
piano.)

**Reverb:** 4 series allpasses per channel for input diffusion → an 8-line FDN
(Householder or Hadamard matrix, mutually prime delay lengths scaled by Size)
→ one-pole damping per line for frequency-dependent RT60 → slow LFO plus
random drift modulation on every line. Decay runs from 0.3 s to 60 s, plus ∞.

**Loop safety (non-negotiable)**
- Below 100% Feedback, loop gain is clamped below 1.0 whatever Decay is set
  to. Above 100% (Bloom), a slow level-tracking limiter holds the tail near
  −6 dBFS.
- The loop contains a tanh soft clipper, a DC blocker and a high-pass filter.
- NaN/Inf guard: any non-finite sample resets the loop state and is counted
  in debug builds.

**Real-time performance (Bloom/Feedback, Density, Spread, Stereo density)**
- Smoothing is per sample, not per block: about 20 ms for Feedback, 30 ms for
  Spread and 50 ms for Density rate. Tune the times by ear in Phase 4.
- The Feedback curve is perceptual, so 50→100% feels as wide as 0→50%.
  Crossing 100% has no jump: the soft clipper takes over gradually. Pulling
  back below 100% lets the tail decay naturally.
- Density changes only the spawn rate (a smoothly varying Poisson rate). It
  never cuts off grains that are already playing.
- Spread: each new grain gets an equal-power pan within ±Spread. Grains
  already playing glide to the new range over about 50 ms. Spread also
  decorrelates the reverb's left/right inputs.
- Stereo density: pan = sign(u)·|u|^k with u uniform in ±1 and
  k = 4^(−tilt), then scaled by Spread. It affects only new grains.
- Macro mappings run on smoothed values. Knob drags send begin/end gestures,
  so Logic records each move as one smooth automation pass.
- The UI writes through APVTS atomics. The visualizer reads grain events
  through a lock-free FIFO at 60 fps.

**Real-time rules:** no allocation, locks, logging or exceptions in
`process()`; allocate in `prepare()` for up to 192 kHz. Use
`ScopedNoDenormals` and flush-to-zero. Smooth every continuous value and
interpolate delay-time changes. Handle block sizes 1–4096, sample-rate
changes, bypass, and mono-in.

## 5. Parameters

### 5a. Host parameters: 11 macros + 3 switches

IDs are stable strings, such as `macro.bloom`. They never change after
release.

Bloom, Space and Drift are scaled (`macros::kRangeScale` = 0.65): the knob's
0–100% covers 0–65% of the curves below, because the top of the original
curves was too much to use. Bloom's 100–110% zone then runs on through the
rest of its curve to infinite hold.

| Macro | Range | Default | Drives (engine parameters) |
| --- | --- | --- | --- |
| Bloom | 0–110% | 60% | Feedback (loop gain 1 − 30^−(b^1.4): 0.47 at 30%, 0.72 at 50%, 0.88 at 70%); Shimmer blend 40→90%; Decay ×1→×2; from 70% up, crossfades Serial → Grains-in-loop; above 100% = Bloom (limiter holds level) |
| Space | 0–100% | 65% | Reverb Size 30→100%; Decay 1.5→30 s (log; 1.5→10.5 s over the knob); Diffusion 60→95%; Pre-delay 10→80 ms |
| Density | 0–100% | 45% | Random grain rate 0→about 40/s; grain Size 900→150 ms as density rises, so overlap stays even; Shape fixed smooth |
| Drift | 0–100% | 35% | Random time 10→70%; pitch jitter 0→±0.3 st; reverb Mod depth 15→70% |
| Spread | 0–100% | 70% | Grain pan width; reverb Width 80→150% |
| Stereo density | −100 to +100% | 0 (even) | Pan distribution inside Spread: centre ↔ edges |
| Tone | −100 to +100% | 0 | Loop high cut 2→9 kHz; Damping 3→12 kHz (neutral: wet centroid ~12% below the dry piano) |
| Shimmer | Octave / Fifth / Oct + 5th / Sub + Oct | Octave | Shift A, Shift B, B level: +12 · +7 · +12 and +19 · −12 and +12 |
| Texture | Clean / Tape / Dust | Clean | Colour mode |
| Mix | 0–100% | 35% | Dry/wet, constant-power |
| Freeze | on/off | off | Grain buffer recording |
| Shimmer on (`macro.shimmerOn`) | on/off | on | Off: the whole reverb + shimmer block fades out of the wet signal (40 ms); only the grains remain |
| Grains on (`macro.grainsOn`) | on/off | on | Off: no new grains, grain output fades out (40 ms), all input goes straight to the reverb. The buffer keeps recording |
| Direction (`macro.direction`) | 0–100% | 0% (forward) | Share of grains played backwards: 0 forward, 50 half and half, 100 reverse |

The switches and Direction arrived with state version 2; a version-1 state
loads them at their defaults. Version 3 made Direction a percentage; a
version-2 state's choice (0/1/2) loads as 0/50/100%.

Fixed, with no control: grain Pitch 0, Grain feedback 0, no tempo sync,
Output gain 0 dB. All curves live in `src/dsp/Macros.h`.

### 5b. Engine parameters (internal, not host-visible)

These are set by macros, presets and `glint-render` JSON.

| Group | Parameter | Range | Default |
| --- | --- | --- | --- |
| Input | Input gain | −24 to +12 dB | 0 dB |
| Loop | Feedback | 0–110% | 60% |
| Grains | Density | −100 (regular) to +100% (random) | +30% |
| Grains | Spread / Stereo density | 0–100% / ±100% | 60% / 0 |
| Grains | Time | 0–100% of buffer | 30% |
| Grains | Size | 10 ms – 4 s, then ∞ | 250 ms |
| Grains | Pitch | ±24 st | 0 |
| Grains | Shape | 0–100% | 50% |
| Grains | Random time / size / pitch | 0–100% each | 20 / 0 / 0% |
| Grains | Reverse | 0–100% | 0% |
| Grains | Grain feedback | 0–95% | 0% |
| Grains | Freeze | on/off | off |
| Grains | Sync + division | on/off; 1/32–4 bars | off; 1/8 |
| Grains | Colour | Clean, Digital, Tape, Dust | Clean |
| Shimmer | Blend | 0–100% | 70% |
| Shimmer | Shift A / Shift B | ±24 st each | +12 / +7 |
| Shimmer | Shift B level | 0–100% | 0% |
| Shimmer | Loop high cut / low cut | 1–20 kHz / 20–500 Hz | 8 kHz / 120 Hz |
| Reverb | Size | 0–100% | 70% |
| Reverb | Decay | 0.3–60 s, then ∞ | 8 s |
| Reverb | Pre-delay | 0–500 ms | 20 ms |
| Reverb | Diffusion | 0–100% | 80% |
| Reverb | Damping (high cut) | 1–20 kHz | 9 kHz |
| Reverb | Mod depth / rate | 0–100%; 0.05–5 Hz | 30%; 0.4 Hz |
| Routing | Mode | Serial, Parallel, Grains-in-loop | Serial |
| Output | Grains level / Reverb level | −inf to +6 dB | 0 dB |
| Output | Mix / Width / Output gain | 0–100% / 0–200% / ±dB | 35% / 100% / 0 dB |

## 6. Implementation phases

Each phase is its own Claude Code session and PR. Each ends with objective
checks passing, then a listening sign-off from Shai. Phases 1–3 can run in
parallel after Phase 0.

0. **Scaffold.** CMake + JUCE submodule + a pass-through plugin (AU, VST3,
   Standalone; universal binary). Add LICENSE, THIRD_PARTY_NOTICES.md,
   `scripts/validate.sh` and CI.
   - Done when: `auval -v aufx Glnt <mfr>` passes, pluginval passes at
     strictness 10, and the plugin loads on a stereo track in Logic.
1. **Beads audit and port.** Pin the eurorack submodule. Build Beads' DSP in a
   desktop harness. Write `docs/beads-notes.md` covering sample rate, block
   size, buffer memory, fixed vs float, RNG, parameter scaling and quality
   modes. Port into `src/dsp/beads_port/` behind a `GrainEngine` interface
   that works at any sample rate. Add `glint-render`.
   - Done when: renders at 44.1, 48 and 96 kHz give non-silent, finite,
     seed-deterministic grains. Freeze and Size = ∞ behave as in the Beads
     manual.
2. **Reverb core.** Diffuser + 8-line FDN + damping + modulation.
   - Done when: RT60 at 1 kHz is within ±15% of Decay from 1 to 30 s. There's
     no CPU spike on 60 s of silence. L/R tail correlation is below 0.3.
3. **Shimmer loop.** Pitch shifter, loop filters, soft clipper, gain clamp,
   Bloom limiter, NaN guard.
   - Done when: a 440 Hz sine at +12 gives a dominant 880 Hz peak. Ten
     minutes of pink noise at Feedback 110% and Decay ∞ stays below 0 dBFS
     with no non-finite samples.
4. **Integration and macros.** `GlintEngine`, routing modes, the Bloom
   routing crossfade, APVTS with the 11 macros, the `Macros.h` curves,
   smoothing, versioned state save/restore.
   - Done when: automation sweeps of every macro pass a click detector, and
     a 1-second sweep of Bloom 0→110→0% while Density, Spread and Stereo
     density move passes it with peaks below 0 dBFS. State round-trips
     exactly. Bypass and sample-rate changes are clean. pluginval still
     passes at strictness 10.
5. **UI.** First a generic editor for tuning by ear. Then the custom UI per
   `docs/ui/UI-SPEC.md`: knobs, chips, particle cloud, resizable.
   - Done when: no UI action blocks the audio thread, the editor reopens with
     the correct state, and every control is keyboard- and
     screen-reader-accessible.
6. **Presets and polish.** Start from the five presets in the mockup (Halo,
   Night Choir, Cathedral Snow, Tape Ghosts, Dust Halo) and grow to about 20.
   CPU work: SIMD on the FDN, skipping idle grains.
   - Done when: default settings use under 3% of one M1 core at 48 kHz with a
     256-sample buffer (`glint-render --bench`).
7. **Distribution on GitHub Releases.** Developer ID signing, notarization,
   and a .pkg that installs to `/Library/Audio/Plug-Ins/Components` and
   `VST3`.
   - Done when: the plugin installs and passes auval on a clean Mac.

## 7. Testing and validation

- DSP unit tests (Catch2): impulse responses, RT60, FFT peaks, loop
  stability, finite output, parameter sweeps.
- Render regression: `tests/render-cases/*.json` compared on RMS, peak and
  spectral centroid within a tolerance (not bit-exact).
- Stress matrix: 44.1/48/88.2/96/192 kHz × block sizes 1, 32, 256, 1024,
  4096 and random variable sizes × mono/stereo; silence; full-scale noise.
- auval + pluginval on every PR (CI) and locally before a phase closes.
- Logic checklist (manual, Shai): insert on stereo and mono tracks and on an
  Aux send at Mix 100%; automate Freeze and Bloom; check that offline and
  real-time bounces match; save/reopen the project; freeze the track; change
  the sample rate; reset and rescan in Plug-in Manager.
- CPU: `glint-render --bench` at default, max-density and Bloom > 100%.

## 8. Risks

| Risk | Mitigation |
| --- | --- |
| Beads DSP is tied to a fixed rate or memory layout | Fallback: write an original grain engine, using Beads as a behavioural reference (about one extra phase) |
| Shimmer loop runs away or turns harsh | Gain clamp, Bloom limiter, soft clipper, loop filters, 10-minute stability test |
| CPU at 96 kHz+ with dense grains in the loop | Grain cap, idle-grain skipping, SIMD FDN, Eco mode running the reverb at half rate |
| Long tails ring metallic | Prime delay lengths, line modulation, high diffusion, listening sign-off in Phase 2 |

Open: final product name ("Glint" is a placeholder; check trademarks), and
CLAP / AUv3 later.

## Sources

- https://cdm.link/beads-is-now-open-source/
- https://github.com/pichenettes/eurorack
- https://pichenette.readthedocs.io/en/latest/modules/beads/manual/
- https://forum.juce.com/t/important-changes-to-the-juce-end-user-licence-agreement-for-juce-8/61265
- https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/VST3+License

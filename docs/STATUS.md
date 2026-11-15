# Status

Autonomous run on 2026-11-15. Shai asked for progress through the phases
without waiting for sign-off between them; the listening and Logic checks
below are still needed from Shai.

| Phase | State | Branch | Numbers |
| --- | --- | --- | --- |
| 0 Scaffold | Done, Logic load pending | phase-0 (commit 9ea66bc) | auval PASS (`aufx Glnt Shyl`); pluginval strictness 10 SUCCESS (VST3, AU, GUI tests on) |
| 1 Grains | Done | phase-1-3-dsp | Non-silent, finite and seed-deterministic at 44.1/48/96 kHz; output identical for block sizes 1/333/4096; Freeze holds a 330 Hz tone (> −30 dB, peak within 5 Hz); Size ∞ loops when frozen; max 32 grains, rate within 15% |
| 2 Reverb | Done | phase-1-3-dsp | RT60 at 1 kHz (⅓-octave band) for Decay 1/2/4/8/16/30 s: 1.07/2.00/3.96/8.05/16.04/32.3 s (all within ±8%); L/R tail correlation < 0.3; 60 s of silence: worst second < 3× median |
| 3 Shimmer | Done | phase-1-3-dsp | +12 st on 440 Hz: peak in 880 ± 20 Hz band, ≥ 20 dB over 440 Hz; 10 min pink noise at Feedback 110 %, Decay ∞: peak −1.38 dBFS, 0 non-finite |
| 4 Integration | Done | phase-4-6-plugin | All 8 continuous macros swept (1 s and 0.1 s) + Shimmer/Texture/Freeze switching: 0 clicks (worst LPC-residual ratio 4.4 vs threshold 12; injected −26 dB step and −6 dB jump are caught, ratio ~59); combined Bloom 0→110→0 sweep 0 clicks, peak < 0 dBFS; state round-trips byte-exact; bypass in/out 0 clicks; 44.1–192 kHz × blocks 1–4096 + random sizes, mono-in: finite |
| 5 UI | Done | phase-4-6-plugin | Custom UI per UI-SPEC, 75/100/125/150 % steps; editor reopens with state; 8 sliders + 10 buttons with accessible names/values; text contrast ≥ 6.2:1 |
| 6 Presets/polish | Done | phase-4-6-plugin | 20 presets (5 from mockup + 15), all render finite, peaks −1.8 to −4.3 dBFS on a plucked phrase; CPU at 48 kHz/256: 0.81 % of one M4 Pro core (default), 0.85 % max density, 0.89 % Bloom 110 %; 96 kHz 1.6 %, 192 kHz 3.2 % |
| 7 Distribution | Blocked on Shai | — | Unsigned .pkg builds (`scripts/sign-notarize.sh`); needs a Developer ID Application + Installer certificate and a notarytool profile (only an Apple Development identity is installed) |

## Decisions made without Shai (easy to revisit)

- **Beads port strategy:** ported envelope, grain, scheduler and freeze tail
  (MIT); buffer, triggers, pan law and textures written fresh. Reason:
  Beads is hard-wired to 48 kHz and a 768 KB SRAM layout, and its Q22.10
  positions overflow at 16 s × 192 kHz. See `docs/beads-notes.md`.
- **Shimmer loop recirculates only the pitch-shifted signal.** Feeding the
  unshifted reverb output back ran away on the FDN's modal peaks.
- **Bloom > 100 %** glides the reverb decay to infinite and a peak limiter
  (fast down, slow up) holds the wet signal near −6 dBFS.
- **Grain Time** is fixed at 10 % of the 16 s buffer (≈ 1.6 s back) in the
  macro mapping; with Density at 0 the input still reaches the reverb.
- **Dust** has wow, flutter and faint hiss only (an early crackle was
  removed: the plan doesn't ask for it and it read as clicks).
- **Particle cloud:** each new grain spawns a particle that rises over ~3–4 s,
  so the cloud reads as a field instead of flickering 250 ms dots.

## Checked by Shai

- Listening renders (`scripts/render-listening.sh`): sound OK (2026-11-15).

## Waiting for Shai

1. Load Glint on a stereo track in Logic (Phase 0 "Done when"), and run the
   Logic checklist in PLAN §7.
2. AU manufacturer code `Shyl` and company name "Glint Audio" are placeholders.
3. Opening PRs on GitHub (branches are pushed).
4. Phase 7 signing identities (see table).

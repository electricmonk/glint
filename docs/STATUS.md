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
| 7 Distribution | Done (v0.1.0) | main | `Glint-0.1.0.pkg` signed (Developer ID Application + Installer, team J257577DJK), notarized (submission a4449cbb, Accepted, no issues) and stapled; `spctl`: accepted, Notarized Developer ID. AU `aufx Glnt Elmk`, company electricmonk. Released on GitHub as v0.1.0 |

## Shimmer v2, switches and Direction (2026-11-15, branch shimmer-v2)

Shai: the shimmer didn't sound nice and Bloom 50% on piano was almost
unusable; asked for Shimmer/Grains on-off switches and grain direction.

- **Pitch shifter replaced** (two-tap delay line → phase vocoder with
  peak-region phase locking). On a 440 Hz tone at Bloom 50%, non-tonal
  energy in the tail went from −4.0 dB to −8.9 dB relative to the tone; the
  part far from the octave lines (> 170 cents) is now −38 dB, the same as the
  plain reverb, so the shifter adds no grit. What remains within 8–50 cents
  is the reverb's own modulation, re-shifted each pass. Shifter alone:
  ≥ 99.96 % of the output on the target line at 44.1–192 kHz, level ripple
  < 0.5 dB (was several dB at ~15 Hz).
- **Bloom curve gentler below 70 %:** loop gain at 50 % is 0.72 (was 0.82).
  On the piano phrase at Bloom 50 %, the 2–4 kHz band of the wet signal is
  −17.5 dB re total (was −12.6 dB; dry piano −19.5 dB).
- **Switches and Direction:** `macro.shimmerOn`, `macro.grainsOn`,
  `macro.direction` (state version 2; v1 states load them at defaults).
  Shimmer off: octave in the tail −61 dB (on: −3 dB). Grains off: 0 grain
  events. Direction: forward 10/10 grains rise in pitch on a sweep, reverse
  10/10 fall, both 6/4. Switching all three: worst click ratio 5.4–5.8
  (static 4.6, threshold 12). Drift no longer reverses grains.
- **Presets:** Tape Ghosts, Drift Ice → Both; Old Film, Ghost Note →
  Reverse; Glass Harp → grains off (pure shimmer reverb); Whisper Room →
  shimmer off.
- **Fix:** restoring state now sets every parameter from the saved value;
  JUCE skipped bools/choices left at unsnapped values (pluginval caught it on
  the new switches; Freeze had the same latent bug).
- CPU at 48 kHz/256: 1.57 % of one core (was 0.80 %); 96 kHz 3.1 %, 192 kHz
  6.5 %.

## Usable ranges, Shimmer bypass, Direction knob (2026-11-17, branch shimmer-v2)

Shai: switch clicks did nothing; Shimmer off should turn the reverb off;
anything above 60% was almost unusable; Direction should be a knob.

- **Fix:** the chip groups overlapped their heading switches and swallowed
  the clicks. Chip groups now pass clicks through; the editor test hit-tests
  the centre of every button.
- **Shimmer off bypasses the reverb:** a tone's tail 1–4 s after the note is
  below −100 dBFS with the switch off (on: above −40 dBFS); with grains on,
  the grains still play.
- **Ranges:** Bloom, Space and Drift at 100% now reach what 65% did. On the
  piano phrase at the default knob positions, the tail falls 10 dB in 2.4 s
  (was 6.2 s) and 20 dB in 4.4 s (was not within 9 s). Everything at 100%
  (`case-piano-full`): 8.2 s / 14.4 s. Presets keep their knob values, so
  they're gentler too. Whisper Room's shimmer is back on (it relied on
  Shimmer off giving a plain reverb).
- **Direction** is a 0–100% knob (share of grains reversed), state
  version 3; version-2 states map Forward/Both/Reverse to 0/50/100%.

## Tone recentred (2026-11-17, branch shimmer-v2)

Shai: the dark/bright knob leans too far to the bright side.

- Loop high cut 3→14 kHz became 2→9 kHz; damping 4→16 kHz became 3→12 kHz.
  Wet spectral centroid on the piano, as a multiple of the dry piano's:
  dark 0.66 → 0.73, neutral 1.09 → 0.94, bright 1.61 → 1.43.
- **Reverb fix this exposed:** the FDN boosts its loop gain so RT60 at
  1 kHz matches Decay; with dark damping that pushed the low-frequency gain
  to the clamp (near-infinite lows) and a fast Tone sweep clicked through
  the limiter (ratio 37, threshold 12). The boost is now capped so lows ring
  at most 1.5x Decay. RT60 at 1 kHz still within 7% from 1 to 16 s (4%
  short at 30 s); Tone sweep ratio 5.8; 10-minute stress peak −1.32 dBFS.

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
2. AU manufacturer code `Elmk`, company name "electricmonk" (chosen by Shai 2026-11-17; was the placeholder `Shyl` / "Glint Audio").
3. Opening PRs on GitHub (branches are pushed).
4. Phase 7 signing identities (see table).

# Glint

A granular shimmer reverb for Logic Pro (AU) and other hosts (VST3). Audio runs
through a granular engine adapted from open-source Eurorack firmware, into a
long, diffuse 8-line FDN reverb with a pitch-shifted feedback loop. Above 100%,
**Bloom** lets the loop sustain while a limiter holds the tail near −6 dBFS.

Eleven controls: Bloom, Space, Density, Drift, Spread, Stereo density, Tone,
Shimmer interval, Texture, Mix and Freeze, plus on/off switches for the
Shimmer (reverb) and Grains sections and a grain Direction knob (forward to reverse).
20 factory presets.

## Build

Requires Xcode command-line tools, CMake ≥ 3.25 and Ninja.

```
git submodule update --init            # JUCE + eurorack (no --recursive needed)
cmake --preset mac-release && cmake --build --preset mac-release
ctest --preset mac-release             # Catch2 DSP + plugin tests
scripts/validate.sh                    # auval + pluginval (strictness 10)
```

The build copies `Glint.component` and `Glint.vst3` into `~/Library/Audio/Plug-Ins`.
In Logic, the AU appears under Audio Units → Glint Audio → Glint.

## Tools

- `glint-render case.json -o out.wav [--preset N] [--sr N]` renders JSON cases
  (see `tests/render-cases/`); `--bench` reports CPU.
- `scripts/render-listening.sh` renders the listening set into `renders/`.
- `scripts/sign-notarize.sh` builds a .pkg (signed and notarized when Developer
  ID identities are provided).

## Licence

AGPL-3.0-or-later. Third-party components and their licences (MIT, OFL, …)
are listed in `THIRD_PARTY_NOTICES.md`.

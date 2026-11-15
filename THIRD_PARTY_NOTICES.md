# Third-party notices

Glint is licensed under the GNU Affero General Public License v3.0 or later
(see `LICENSE`). It includes or links the following third-party components.

## JUCE 8

- Source: https://github.com/juce-framework/JUCE (submodule `third_party/JUCE`, tag 8.0.15)
- Licence: GNU AGPL v3 (JUCE's open-source licensing option)

## VST3 SDK (bundled with JUCE)

- Source: https://github.com/steinbergmedia/vst3sdk
- Licence: MIT (since VST3 SDK 3.8)
- VST is a registered trademark of Steinberg Media Technologies GmbH.

## Granular engine adapted from open-source Eurorack firmware

- Source: https://github.com/pichenettes/eurorack (submodule `third_party/eurorack`,
  commit f4cfa7697095a28d292bff758beb51deb49b5a86); `beads/` and `stmlib/`
- Copyright Emilie Gillet
- Licence: MIT. Files adapted from it live in `src/dsp/beads_port/` and keep
  their original copyright and MIT notice:
  - `src/dsp/beads_port/GrainEnvelope.h` (from `beads/dsp/grain_envelope.h`, `voice_stealing_envelope.h`)
  - `src/dsp/beads_port/Grain.h` (from `beads/dsp/grain.h`, `delay_grain.h`, `granular_player.h`)

  MIT License text (applies to the files above):

  > Copyright 2017 Emilie Gillet.
  >
  > Permission is hereby granted, free of charge, to any person obtaining a copy
  > of this software and associated documentation files (the "Software"), to deal
  > in the Software without restriction, including without limitation the rights
  > to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  > copies of the Software, and to permit persons to whom the Software is
  > furnished to do so, subject to the following conditions:
  >
  > The above copyright notice and this permission notice shall be included in
  > all copies or substantial portions of the Software.
  >
  > THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  > IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  > FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  > AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  > LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  > OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  > THE SOFTWARE.

## Catch2 (tests only, not shipped)

- Source: https://github.com/catchorg/Catch2 (v3.16.1)
- Licence: Boost Software License 1.0

## Fonts (bundled as BinaryData)

- Space Grotesk — Copyright 2020 The Space Grotesk Project Authors — SIL Open Font License 1.1
  (`resources/fonts/OFL-spacegrotesk.txt`)
- Inter — Copyright 2020 The Inter Project Authors — SIL Open Font License 1.1
  (`resources/fonts/OFL-inter.txt`)
- JetBrains Mono — Copyright 2020 The JetBrains Mono Project Authors — SIL Open Font License 1.1
  (`resources/fonts/OFL-jetbrainsmono.txt`)

Latin subsets of the static instances, via Fontsource.

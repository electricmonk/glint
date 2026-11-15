# Glint UI spec (v2, simple)

The reference mockup is `glint-ui-v2.dc.html` in this folder. It's an HTML
design file, not code to ship. Read it for exact colours, sizes, labels,
value formatting and the five presets, then rebuild the UI in JUCE.

## Window

- 820 × 560 px at 100%, resizable in steps (75%, 100%, 125%, 150%), with the
  aspect ratio locked.
- Dark theme. Background #0E1014; visualizer well #0A0C0F; hairlines #1C2026 /
  #252A31; text #ECEAE6; secondary text #9CA3AB.
- Two accents: warm gold #EBC77A (Bloom and shimmer) and icy blue #9ED8E6
  (grains and Freeze).
- Fonts: Space Grotesk (wordmark, knob labels), Inter (UI text), JetBrains
  Mono (values). Bundle the fonts as BinaryData. They are all under the OFL
  license; add them to THIRD_PARTY_NOTICES.md.

## Layout, top to bottom

1. **Header (40 px):** "GLINT" wordmark at the left. At the right, a preset
   pill (‹ name ›) and a Freeze toggle pill.
2. **Particle cloud (180 px):** grains drawn as soft dots rising from the
   bottom to the top over their lifetime. x = the grain's stereo pan (L left,
   R right), so Spread and Stereo density are visible. Shimmer particles are
   gold, and there are more of them as Bloom rises. Particles wobble sideways
   with Drift. A horizon line at the bottom brightens with Bloom.
   Status text at the top-left: "BLOOMING" when Bloom is above 100%, or
   "FROZEN". Data comes from the audio thread through a lock-free FIFO of
   grain events (pan, age, level, is-shimmer) at 60 fps.
3. **Knob row:** Density, Drift, **Bloom** (large, centred), Space, Spread,
   Stereo density (smaller). Each knob shows a label, then a value underneath.
4. **Footer:** a SHIMMER section (interval chips: Octave · Fifth ·
   Oct + 5th · Sub + Oct), a GRAINS section (Texture chips Clean · Tape ·
   Dust, then Direction chips drawn as arrows: forward · both · reverse),
   then small Tone and Mix knobs. Each section heading is its on/off switch:
   a dot (gold for Shimmer, ice for Grains) filled when on. A switched-off
   section's chips dim to 40% but stay clickable.

## Knob behaviour

- A 270° arc from −135° to +135°. Bipolar knobs (Stereo density, Tone) fill
  from the top centre.
- The Bloom arc turns pale (#FFF1D0) above 100%.
- Drag vertically. Shift gives fine moves. Double-click resets to the
  default. Arrow keys step 1% (Shift: 10%). Mouse wheel works too.
- Every drag sends begin/end change gestures to the host.
- Value text: Space shows the resulting tail length ("8.0 s tail"), Stereo
  density "centre 30" / "even" / "edges 60", Tone "dark 20" / "neutral" /
  "bright 40", Bloom above 100% "bloom 106%".

## Accessibility

- Every control is keyboard-focusable and has an accessible name and value
  (JUCE AccessibilityHandler).
- Text contrast is at least 4.5:1 against its background.

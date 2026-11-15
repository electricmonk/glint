# Glint — working rules

- Read docs/PLAN.md first, and docs/ui/UI-SPEC.md before any UI work.
  Work one phase at a time. Stop at the phase's "Done when" criteria,
  report results with numbers, and wait for my sign-off.
- src/dsp is pure C++20: no JUCE includes. In process(): no allocation,
  locks, logging, exceptions or std::function. Allocate in prepare().
- Every DSP change ships with a Catch2 test or a render case.
- Every source file starts with an SPDX header: AGPL-3.0-or-later for our
  code; Beads-derived files keep MIT plus their original copyright.
- Build: cmake --preset mac-release && cmake --build --preset mac-release
- Test: ctest --preset mac-release
- Validate: scripts/validate.sh (auval + pluginval strictness 10) must pass.
- Never edit third_party/. Beads changes live in src/dsp/beads_port/ and keep
  their MIT headers. Update THIRD_PARTY_NOTICES.md when porting files.
- Never use "Mutable Instruments", "Beads" or "Valhalla" in product strings,
  UI text, parameter names or preset names.
- Only the 11 macros, the Shimmer/Grains switches and grain Direction are host
  parameters. Engine parameters are internal and driven through
  src/dsp/Macros.h. Don't expose them to the host.
- Parameter IDs are frozen once Phase 4 lands. Bump the state version instead.
- You can't hear audio. Back every sonic claim with a measurement, and ask me
  to listen at checkpoints, naming the render file and what to listen for.

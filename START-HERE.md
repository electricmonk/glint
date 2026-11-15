# Handing Glint to Claude Code

## 1. Set up (on your Mac)

You need Xcode, the Xcode command-line tools, CMake 3.25 or later, Logic Pro
and Claude Code.

```
mkdir glint && cd glint && git init
# unzip this bundle into the folder, so CLAUDE.md sits at the repo root
git add -A && git commit -m "Plan, rules and UI spec"
claude
```

## 2. First prompt (paste this)

```
Read CLAUDE.md, docs/PLAN.md and docs/ui/UI-SPEC.md. Do Phase 0.
Then start Phase 1 with the audit only: add third_party/eurorack as a pinned
submodule, read beads/ and stmlib/, and write docs/beads-notes.md (sample
rate, block size, buffer sizes, fixed vs float, RNG, parameter scaling,
quality modes, anything STM32-specific). Propose the GrainEngine C++
interface and the port strategy, then stop and wait for my OK before
porting.
```

## 3. After each phase

1. Check the numbers Claude reports against the phase's "Done when" list.
2. Listen to the renders it names, and load the build in Logic.
3. Then reply:

```
Phase N is approved. Commit, open a PR, then plan Phase N+1 and wait.
```

Phases 1, 2 and 3 don't depend on each other. If you want to run them in
parallel, give each its own git worktree and Claude Code session.

## What's in this bundle

| File | What it is |
| --- | --- |
| CLAUDE.md | Standing rules Claude Code reads every session |
| docs/PLAN.md | Architecture, macros, phases and acceptance tests |
| docs/ui/UI-SPEC.md | The v2 UI in words: layout, colours, knob behaviour |
| docs/ui/glint-ui-v2.dc.html | The mockup's source, for exact colours, sizes and presets. Reference only, not shippable code |

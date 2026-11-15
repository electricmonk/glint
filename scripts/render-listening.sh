#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Render the listening set into renders/ (git-ignored): the render cases and
# every factory preset on a plucked phrase. Prints peak/RMS/CPU per file.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
R="$ROOT/build/${PRESET:-mac-release}/tools/render/glint-render"
OUT="$ROOT/renders"
mkdir -p "$OUT"
for c in "$ROOT"/tests/render-cases/*.json; do
  n="$(basename "$c" .json)"
  printf '%-28s ' "$n"; "$R" "$c" -o "$OUT/case-$n.wav" | sed -E 's/.*"peakDb": ([-0-9.]+), "rmsDb": ([-0-9.]+).*/peak \1 dB, rms \2 dB/'
done
cat > "$OUT/.phrase.json" <<'J'
{ "sampleRate": 48000, "seconds": 16, "seed": 1, "input": { "type": "notes", "level": -6, "seconds": 6 } }
J
for i in $(seq 0 19); do
  name="$(printf 'preset-%02d' "$i")"
  printf '%-28s ' "$name"; "$R" "$OUT/.phrase.json" --preset "$i" -o "$OUT/$name.wav" | sed -E 's/.*"peakDb": ([-0-9.]+), "rmsDb": ([-0-9.]+).*/peak \1 dB, rms \2 dB/'
done
echo "renders in $OUT"

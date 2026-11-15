#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Validate the built plugin with auval and pluginval (strictness 10).
# Usage: scripts/validate.sh [preset]   (default: mac-release)
set -euo pipefail

PRESET="${1:-mac-release}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ARTEFACTS="$ROOT/build/$PRESET/src/plugin/Glint_artefacts"
case "$PRESET" in
  *debug*) CONFIG=Debug ;;
  *) CONFIG=Release ;;
esac
[ -d "$ARTEFACTS/$CONFIG" ] && ARTEFACTS="$ARTEFACTS/$CONFIG"

MFR="$(grep -E '^GLINT_MANUFACTURER_CODE' "$ROOT/build/$PRESET/CMakeCache.txt" | cut -d= -f2)"
MFR="${MFR:-Shyl}"

PLUGINVAL="${PLUGINVAL:-}"
if [ -z "$PLUGINVAL" ]; then
  if command -v pluginval >/dev/null 2>&1; then PLUGINVAL="$(command -v pluginval)"
  else PLUGINVAL="/Applications/pluginval.app/Contents/MacOS/pluginval"; fi
fi

echo "== auval (aufx Glnt $MFR)"
# Make sure the freshly copied component is picked up.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
auval -v aufx Glnt "$MFR" | tee "$ROOT/build/$PRESET/auval.log" | tail -n 5
grep -q "AU VALIDATION SUCCEEDED" "$ROOT/build/$PRESET/auval.log"

for fmt in VST3 AU; do
  case "$fmt" in
    VST3) BUNDLE="$ARTEFACTS/VST3/Glint.vst3" ;;
    AU)   BUNDLE="$ARTEFACTS/AU/Glint.component" ;;
  esac
  echo "== pluginval $fmt (strictness 10)"
  "$PLUGINVAL" --strictness-level 10 --validate-in-process \
    --output-dir "$ROOT/build/$PRESET/pluginval-$fmt" --validate "$BUNDLE" \
    > "$ROOT/build/$PRESET/pluginval-$fmt.log" 2>&1 || {
      tail -n 40 "$ROOT/build/$PRESET/pluginval-$fmt.log"; echo "pluginval $fmt FAILED"; exit 1; }
  tail -n 2 "$ROOT/build/$PRESET/pluginval-$fmt.log"
done

echo "== all validation passed"

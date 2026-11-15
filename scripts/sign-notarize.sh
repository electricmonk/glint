#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Build a .pkg that installs Glint to /Library/Audio/Plug-Ins/{Components,VST3}.
#
# Unsigned (local testing):   scripts/sign-notarize.sh
# Signed + notarized:
#   DEV_ID_APP="Developer ID Application: Name (TEAMID)" \
#   DEV_ID_INSTALLER="Developer ID Installer: Name (TEAMID)" \
#   NOTARY_PROFILE=glint-notary \       # from: xcrun notarytool store-credentials
#   scripts/sign-notarize.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PRESET="${PRESET:-mac-release}"
ART="$ROOT/build/$PRESET/src/plugin/Glint_artefacts/Release"
OUT="$ROOT/build/$PRESET/dist"
VERSION="$(grep -E '^project\(Glint VERSION' "$ROOT/CMakeLists.txt" | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
STAGE="$OUT/stage"

rm -rf "$OUT"
mkdir -p "$STAGE/au" "$STAGE/vst3"
cp -R "$ART/AU/Glint.component" "$STAGE/au/"
cp -R "$ART/VST3/Glint.vst3" "$STAGE/vst3/"

if [ -n "${DEV_ID_APP:-}" ]; then
  echo "== codesign (hardened runtime, timestamp)"
  for b in "$STAGE/au/Glint.component" "$STAGE/vst3/Glint.vst3"; do
    codesign --force --deep --options runtime --timestamp --sign "$DEV_ID_APP" "$b"
    codesign --verify --strict --verbose=2 "$b"
  done
else
  echo "== DEV_ID_APP not set: building an UNSIGNED package (local testing only)"
fi

echo "== pkgbuild"
pkgbuild --root "$STAGE/au" --identifier io.github.electricmonk.glint.au --version "$VERSION" \
  --install-location /Library/Audio/Plug-Ins/Components "$OUT/glint-au.pkg"
pkgbuild --root "$STAGE/vst3" --identifier io.github.electricmonk.glint.vst3 --version "$VERSION" \
  --install-location /Library/Audio/Plug-Ins/VST3 "$OUT/glint-vst3.pkg"

cat > "$OUT/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>Glint $VERSION</title>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <choices-outline>
    <line choice="au"/>
    <line choice="vst3"/>
  </choices-outline>
  <choice id="au" title="Audio Unit (Logic Pro)"><pkg-ref id="io.github.electricmonk.glint.au"/></choice>
  <choice id="vst3" title="VST3"><pkg-ref id="io.github.electricmonk.glint.vst3"/></choice>
  <pkg-ref id="io.github.electricmonk.glint.au">glint-au.pkg</pkg-ref>
  <pkg-ref id="io.github.electricmonk.glint.vst3">glint-vst3.pkg</pkg-ref>
</installer-gui-script>
EOF

PKG="$OUT/Glint-$VERSION.pkg"
echo "== productbuild"
if [ -n "${DEV_ID_INSTALLER:-}" ]; then
  productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" --sign "$DEV_ID_INSTALLER" --timestamp "$PKG"
else
  productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" "$PKG"
fi

if [ -n "${NOTARY_PROFILE:-}" ] && [ -n "${DEV_ID_INSTALLER:-}" ]; then
  echo "== notarize"
  xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$PKG"
  spctl --assess --type install --verbose "$PKG"
fi

echo "== built $PKG"

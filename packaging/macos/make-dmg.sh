#!/bin/bash
# Baut aus dem fertigen Release-Build einen Installer (.pkg) und packt ihn in ein .dmg.
# Aufruf aus dem Projektordner: bash packaging/macos/make-dmg.sh <version>
set -euo pipefail

VERSION="${1:?Version fehlt}"
ART="build/Mangaman_artefacts/Release"
WORK="build/pkg"
DIST="dist"

rm -rf "$WORK" && mkdir -p "$WORK"/{au,vst3,app} "$DIST"

cp -R "$ART/AU/Mangaman.component"   "$WORK/au/"
cp -R "$ART/VST3/Mangaman.vst3"      "$WORK/vst3/"
cp -R "$ART/Standalone/Mangaman.app" "$WORK/app/"

# Ohne Apple-Developer-ID: ad-hoc signieren, damit Apple-Silicon-Macs die Bundles laden
for b in "$WORK/au/Mangaman.component" "$WORK/vst3/Mangaman.vst3" "$WORK/app/Mangaman.app"; do
    codesign --force --deep --sign - "$b"
done

build_component () {   # name  zielordner
    local name="$1" dest="$2"
    pkgbuild --analyze --root "$WORK/$name" "$WORK/$name.plist"
    # Bundles immer an den Zielort installieren, nie an eine alte Kopie woanders
    python3 - "$WORK/$name.plist" <<'PY'
import plistlib, sys
p = sys.argv[1]
with open(p, "rb") as f: items = plistlib.load(f)
for i in items: i["BundleIsRelocatable"] = False
with open(p, "wb") as f: plistlib.dump(items, f)
PY
    pkgbuild --root "$WORK/$name" --component-plist "$WORK/$name.plist" \
             --identifier "com.andi.mangaman.$name" --version "$VERSION" \
             --install-location "$dest" "$WORK/Mangaman-$name.pkg"
}

build_component au   "/Library/Audio/Plug-Ins/Components"
build_component vst3 "/Library/Audio/Plug-Ins/VST3"
build_component app  "/Applications"

productbuild --synthesize \
    --package "$WORK/Mangaman-au.pkg" \
    --package "$WORK/Mangaman-vst3.pkg" \
    --package "$WORK/Mangaman-app.pkg" \
    "$WORK/distribution.xml"
# Titel im Installer-Fenster
sed -i '' 's|<installer-gui-script minSpecVersion="[0-9]*">|&<title>Mangaman '"$VERSION"'</title>|' "$WORK/distribution.xml"

mkdir -p "$WORK/dmg"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" \
    "$WORK/dmg/Mangaman Installer.pkg"
cp packaging/macos/LiesMich.txt "$WORK/dmg/Lies mich.txt"

hdiutil create -volname "Mangaman $VERSION" -srcfolder "$WORK/dmg" -ov -format UDZO \
    "$DIST/Mangaman-$VERSION-macOS.dmg"

echo "Fertig: $DIST/Mangaman-$VERSION-macOS.dmg"

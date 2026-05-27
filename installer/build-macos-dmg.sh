#!/usr/bin/env bash
# Build a macOS .dmg installer for Pigasus.
#
# Usage: ./installer/build-macos-dmg.sh [VERSION] [BUILD_DIR]
#
# Reads the freshly-built .vst3 and .component bundles from BUILD_DIR
# (default ./build) and packages them with `pkgbuild` + `productbuild`,
# then wraps the result in a UDZO-compressed disk image.

set -euo pipefail

VERSION="${1:-0.1.0}"
BUILD_DIR="${2:-build}"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILT_VST3="${REPO_ROOT}/${BUILD_DIR}/Pigasus_artefacts/Release/VST3/Pigasus.vst3"
BUILT_AU="${REPO_ROOT}/${BUILD_DIR}/Pigasus_artefacts/Release/AU/Pigasus.component"

if [ ! -d "$BUILT_VST3" ]; then
    echo "ERROR: VST3 bundle not found at $BUILT_VST3" >&2
    exit 1
fi
if [ ! -d "$BUILT_AU" ]; then
    echo "ERROR: AU bundle not found at $BUILT_AU" >&2
    exit 1
fi

STAGING="${REPO_ROOT}/installer/macos-staging"
DIST_OUT="${REPO_ROOT}/installer/Output"
rm -rf "$STAGING" "$DIST_OUT"
mkdir -p "$STAGING" "$DIST_OUT"

echo "==> Building VST3 component pkg..."
pkgbuild \
    --component "$BUILT_VST3" \
    --identifier "com.anperaudio.pigasus.vst3" \
    --version "$VERSION" \
    --install-location "/Library/Audio/Plug-Ins/VST3" \
    "${STAGING}/Pigasus-VST3.pkg"

echo "==> Building AU component pkg..."
pkgbuild \
    --component "$BUILT_AU" \
    --identifier "com.anperaudio.pigasus.au" \
    --version "$VERSION" \
    --install-location "/Library/Audio/Plug-Ins/Components" \
    "${STAGING}/Pigasus-AU.pkg"

echo "==> Writing distribution.xml..."
cat > "${STAGING}/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Pigasus ${VERSION}</title>
    <organization>com.anperaudio</organization>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <options customize="allow" allow-external-scripts="no" rootVolumeOnly="true" hostArchitectures="arm64,x86_64"/>
    <welcome    language="en"><![CDATA[Pigasus ${VERSION} — saturator + brick-wall limiter.\n\nInstalls the VST3 and AU plug-ins system-wide so every DAW finds them.]]></welcome>
    <conclusion language="en"><![CDATA[Pigasus is installed. Open your DAW and rescan plug-ins.]]></conclusion>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="vst3"
            title="VST3 Plugin"
            description="Installs Pigasus.vst3 into /Library/Audio/Plug-Ins/VST3"
            start_selected="true">
        <pkg-ref id="com.anperaudio.pigasus.vst3"/>
    </choice>
    <choice id="au"
            title="Audio Unit"
            description="Installs Pigasus.component into /Library/Audio/Plug-Ins/Components"
            start_selected="true">
        <pkg-ref id="com.anperaudio.pigasus.au"/>
    </choice>
    <pkg-ref id="com.anperaudio.pigasus.vst3" version="${VERSION}">Pigasus-VST3.pkg</pkg-ref>
    <pkg-ref id="com.anperaudio.pigasus.au"   version="${VERSION}">Pigasus-AU.pkg</pkg-ref>
</installer-gui-script>
EOF

echo "==> Building combined .pkg installer..."
INSTALLER_PKG="${STAGING}/Pigasus-${VERSION}-Installer.pkg"
productbuild \
    --distribution "${STAGING}/distribution.xml" \
    --package-path "$STAGING" \
    "$INSTALLER_PKG"

echo "==> Building .dmg..."
DMG_STAGE="${STAGING}/dmg"
mkdir -p "$DMG_STAGE"
cp "$INSTALLER_PKG" "${DMG_STAGE}/Pigasus Installer.pkg"

# A short README inside the DMG with handling for Gatekeeper warning
cat > "${DMG_STAGE}/Read Me.txt" <<EOF
Pigasus ${VERSION}
==================

Double-click "Pigasus Installer.pkg" to install.

If macOS blocks it with "Apple could not verify Pigasus Installer.pkg is free
of malware":

   Right-click the .pkg → Open → click Open again.

The installer is not yet code-signed, but it is built from the public source
at https://github.com/PerekhodovAnton/PIGASUS by the GitHub Actions workflow.
EOF

DMG_OUT="${DIST_OUT}/Pigasus-${VERSION}.dmg"
hdiutil create \
    -volname "Pigasus ${VERSION}" \
    -srcfolder "$DMG_STAGE" \
    -ov -format UDZO \
    "$DMG_OUT"

echo ""
echo "==> Built: $DMG_OUT"
ls -lh "$DMG_OUT"

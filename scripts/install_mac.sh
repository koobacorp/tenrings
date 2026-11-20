#!/bin/bash
# Copies TENRINGS.vst3 into your user VST3 folder and clears the download quarantine flag.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
DEST="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$DEST"
rm -rf "$DEST/TENRINGS.vst3"
cp -R "$HERE/TENRINGS.vst3" "$DEST/"
xattr -dr com.apple.quarantine "$DEST/TENRINGS.vst3" 2>/dev/null || true
echo "TENRINGS installed to $DEST. In FL Studio: Options > Manage plugins > Find installed plugins."

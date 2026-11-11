#!/usr/bin/env bash
# Build a self-contained AppImage for QtreGBPaint.
#
# Usage:
#   ./packaging/build_appimage.sh
#   make appimage
#
# linuxdeploy and appimagetool are downloaded into temp/appimage-tools when
# missing. The resulting AppImage is written to build/.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${VERSION:-0.9.1}"
TOOLS_DIR="${TOOLS_DIR:-$ROOT_DIR/temp/appimage-tools}"
APPDIR="${APPDIR:-$ROOT_DIR/temp/appimage/QtreGBPaint.AppDir}"
OUTPUT="${OUTPUT:-$ROOT_DIR/build/QtreGBPaint-${VERSION}-x86_64.AppImage}"
LINUXDEPLOY="${LINUXDEPLOY:-$TOOLS_DIR/linuxdeploy-x86_64.AppImage}"
APPIMAGETOOL="${APPIMAGETOOL:-$TOOLS_DIR/appimagetool-x86_64.AppImage}"
CXX="${CXX:-g++}"

download_tool() {
    local output="$1"
    local url="$2"
    if [[ ! -x "$output" ]]; then
        mkdir -p "$(dirname "$output")"
        echo "Downloading $(basename "$output")..."
        curl -L --fail --retry 3 -o "$output" "$url"
        chmod +x "$output"
    fi
}

download_tool "$LINUXDEPLOY" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
download_tool "$APPIMAGETOOL" \
    "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"

mkdir -p "$ROOT_DIR/build"
# Populate the ignored lib/ directory on a fresh checkout.
make -C "$ROOT_DIR" deps
make -C "$ROOT_DIR" release

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/assets" "$APPDIR/usr/lang"
cp "$ROOT_DIR/build/QtreGBPaint-release" "$APPDIR/usr/bin/QtreGBPaint"
cp -a "$ROOT_DIR/assets/." "$APPDIR/usr/assets/"
cp -a "$ROOT_DIR/lang/." "$APPDIR/usr/lang/"
cp "$ROOT_DIR/packaging/qtregbpaint.desktop" \
	"$APPDIR/io.github.kirbyboy.QtreGBPaint.desktop"
cp "$ROOT_DIR/packaging/qtregbpaint.png" "$APPDIR/"
mkdir -p "$APPDIR/usr/share/mime/packages" "$APPDIR/usr/share/metainfo"
cp "$ROOT_DIR/packaging/qtregbpaint.xml" "$APPDIR/usr/share/mime/packages/"
cp "$ROOT_DIR/packaging/io.github.kirbyboy.QtreGBPaint.metainfo.xml" \
	"$APPDIR/usr/share/metainfo/"

"$LINUXDEPLOY" --appimage-extract-and-run \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/QtreGBPaint" \
    --desktop-file "$APPDIR/io.github.kirbyboy.QtreGBPaint.desktop" \
    --icon-file "$APPDIR/qtregbpaint.png"

# linuxdeploy excludes these two by default, but they are needed on systems
# older than the build host.
LIBSTDCXX="$("$CXX" -print-file-name=libstdc++.so.6)"
LIBGCC="$("$CXX" -print-file-name=libgcc_s.so.1)"
cp -L "$LIBSTDCXX" "$APPDIR/usr/lib/"
cp -L "$LIBGCC" "$APPDIR/usr/lib/"

rm -f "$OUTPUT"
ARCH=x86_64 "$APPIMAGETOOL" --no-appstream "$APPDIR" "$OUTPUT"
ls -lh "$OUTPUT"

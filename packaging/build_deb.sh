#!/usr/bin/env bash
# Build a binary Debian package for a GitHub release.
#
# Usage:
#   ./packaging/build_deb.sh
#   make deb
#
# The package is written to build/qtregbpaint_<version>_<arch>.deb. It is a
# binary package built with dpkg-deb; it does not require debhelper.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${VERSION:-0.9.1}"
ARCH="${ARCH:-$(dpkg --print-architecture)}"
PKG_NAME="qtregbpaint"
OUTPUT="${OUTPUT:-$ROOT_DIR/build/${PKG_NAME}_${VERSION}_${ARCH}.deb}"
STAGE_DIR="${STAGE_DIR:-$ROOT_DIR/temp/deb/${PKG_NAME}_${VERSION}_${ARCH}}"
PKG_DIR="$STAGE_DIR/pkg"
DOC_DIR="$PKG_DIR/usr/share/doc/$PKG_NAME"

if ! command -v dpkg-deb >/dev/null 2>&1; then
	echo "error: dpkg-deb is required to build the .deb" >&2
	exit 1
fi

# Build the release binary and make sure the ignored local dependencies exist.
make -C "$ROOT_DIR" deps
make -C "$ROOT_DIR" release

rm -rf "$STAGE_DIR"
mkdir -p "$PKG_DIR/DEBIAN" \
	"$PKG_DIR/usr/bin" \
	"$PKG_DIR/usr/share/qtregbpaint/assets" \
	"$PKG_DIR/usr/share/qtregbpaint/lang" \
	"$PKG_DIR/usr/share/applications" \
	"$PKG_DIR/usr/share/icons/hicolor/256x256/apps" \
	"$PKG_DIR/usr/share/mime/packages" \
	"$PKG_DIR/usr/share/metainfo" \
	"$DOC_DIR"

install -Dm755 "$ROOT_DIR/build/QtreGBPaint-release" \
	"$PKG_DIR/usr/bin/QtreGBPaint"
cp -a "$ROOT_DIR/assets/." "$PKG_DIR/usr/share/qtregbpaint/assets/"
cp -a "$ROOT_DIR/lang/." "$PKG_DIR/usr/share/qtregbpaint/lang/"
install -Dm644 "$ROOT_DIR/packaging/qtregbpaint.png" \
	"$PKG_DIR/usr/share/icons/hicolor/256x256/apps/qtregbpaint.png"
install -Dm644 "$ROOT_DIR/packaging/qtregbpaint.desktop" \
	"$PKG_DIR/usr/share/applications/io.github.kirbyboy.QtreGBPaint.desktop"
install -Dm644 "$ROOT_DIR/packaging/qtregbpaint.xml" \
	"$PKG_DIR/usr/share/mime/packages/qtregbpaint.xml"
install -Dm644 "$ROOT_DIR/packaging/io.github.kirbyboy.QtreGBPaint.metainfo.xml" \
	"$PKG_DIR/usr/share/metainfo/io.github.kirbyboy.QtreGBPaint.metainfo.xml"

install -Dm644 "$ROOT_DIR/README.md" "$DOC_DIR/README.md"
install -Dm644 "$ROOT_DIR/LICENSE.txt" "$DOC_DIR/LICENSE.txt"
install -Dm644 "$ROOT_DIR/assets/LICENSE" "$DOC_DIR/assets-LICENSE.txt"
install -Dm644 "$ROOT_DIR/third_party/ImGuiFileDialog/LICENSE" \
	"$DOC_DIR/ImGuiFileDialog-LICENSE.txt"
install -Dm644 "$ROOT_DIR/THIRD_PARTY_NOTICES.md" \
	"$DOC_DIR/THIRD_PARTY_NOTICES.md"
install -Dm644 "$ROOT_DIR/TRADEMARK.md" "$DOC_DIR/TRADEMARK.md"

{
	printf 'QtreGBPaint %s\n\n' "$VERSION"
	printf 'Copyright (C) 2026 the QtreGBPaint project\n\n'
	printf 'The program code is licensed under GPL-3.0-or-later. The visual\n'
	printf 'assets are dedicated to the public domain under CC0 1.0. The\n'
	printf 'QtreGBPaint name and logo are covered by the trademark policy.\n\n'
	printf '%s\n' '--- LICENSE.txt (program code) ---'
	cat "$ROOT_DIR/LICENSE.txt"
	printf '\n%s\n' '--- assets/LICENSE (visual assets) ---'
	cat "$ROOT_DIR/assets/LICENSE"
	printf '\n%s\n' '--- TRADEMARK.md ---'
	cat "$ROOT_DIR/TRADEMARK.md"
	printf '\n%s\n' '--- THIRD_PARTY_NOTICES.md ---'
	cat "$ROOT_DIR/THIRD_PARTY_NOTICES.md"
} > "$DOC_DIR/copyright"

install -Dm755 "$ROOT_DIR/packaging/deb/postinst" "$PKG_DIR/DEBIAN/postinst"
install -Dm755 "$ROOT_DIR/packaging/deb/postrm" "$PKG_DIR/DEBIAN/postrm"

INSTALLED_SIZE="$(du -sk "$PKG_DIR" | cut -f1)"
cat > "$PKG_DIR/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $VERSION
Architecture: $ARCH
Maintainer: kirbiboy <kirbiboy@users.noreply.github.com>
Installed-Size: $INSTALLED_SIZE
Section: graphics
Priority: optional
Homepage: https://github.com/kirbyboy/QtreGBPaint
Vcs-Git: https://github.com/kirbyboy/QtreGBPaint.git
Depends: libc6, libstdc++6, libgcc-s1, libsdl2-2.0-0, zlib1g
Description: pixel-art paint program with retro handheld-inspired palettes
 QtreGBPaint is a small paint program for indexed pixel art, with a
 Paint-style interface, retro handheld-inspired palettes and its own .idx
 format.
EOF

mkdir -p "$ROOT_DIR/build"
rm -f "$OUTPUT"
dpkg-deb --build --root-owner-group "$PKG_DIR" "$OUTPUT"
echo "Built $OUTPUT"
dpkg-deb --info "$OUTPUT"

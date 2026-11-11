#!/usr/bin/env bash
# Build the files attached to a GitHub release:
#   - Debian package
#   - AppImage
#   - SHA256SUMS-<version>
#
# Usage:
#   ./packaging/build_release_assets.sh
#   VERSION=0.9.1 make release-assets

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${VERSION:-0.9.1}"
ARCH="${ARCH:-$(dpkg --print-architecture)}"
DEB="$ROOT_DIR/build/qtregbpaint_${VERSION}_${ARCH}.deb"
APPIMAGE="$ROOT_DIR/build/QtreGBPaint-${VERSION}-x86_64.AppImage"
SUMS="$ROOT_DIR/build/SHA256SUMS-${VERSION}"

VERSION="$VERSION" "$ROOT_DIR/packaging/build_deb.sh"
VERSION="$VERSION" "$ROOT_DIR/packaging/build_appimage.sh"

rm -f "$SUMS"
(
	cd "$ROOT_DIR/build"
	sha256sum "$(basename "$DEB")" "$(basename "$APPIMAGE")" \
		> "$(basename "$SUMS")"
)

echo
echo "Release assets for $VERSION:"
ls -lh "$DEB" "$APPIMAGE" "$SUMS"
echo
cat "$SUMS"

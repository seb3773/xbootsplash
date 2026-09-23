#!/usr/bin/env bash
set -euo pipefail

PKG_NAME="xbootsplash-gui"
PKG_VERSION="${1:-1.0.0}"
PKG_MAINTAINER="seb3773"
PKG_SECTION="utils"
PKG_PRIORITY="optional"

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SRC_ROOT/.." && pwd)"
ARCH="$(dpkg --print-architecture)"
BUILD_DIR="$SRC_ROOT/build"
PKGROOT="$BUILD_DIR/pkgroot"
PKGTMP="$BUILD_DIR/pkgtmp"

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "error: missing required command: $1" >&2
		exit 1
	}
}

need_cmd cmake
need_cmd pkg-config
need_cmd dpkg-deb
need_cmd strip
need_cmd sed
need_cmd awk
need_cmd du
need_cmd ln
need_cmd install

mkdir -p -- "$BUILD_DIR"
rm -rf -- "$PKGTMP"
mkdir -p -- "$PKGTMP"

# Build using build.sh
echo "info: building $PKG_NAME binary..."
"$SRC_ROOT/build.sh"

BIN_PATH="$BUILD_DIR/xbootsplash-gui"
if test ! -x "$BIN_PATH"; then
	echo "error: missing built binary: $BIN_PATH" >&2
	exit 1
fi

# Stage filesystem layout.
echo "info: staging debian layout..."
rm -rf -- "$PKGROOT"
mkdir -p -- \
	"$PKGROOT/DEBIAN" \
	"$PKGROOT/usr/bin" \
	"$PKGROOT/usr/share/applications" \
	"$PKGROOT/usr/share/metainfo" \
	"$PKGROOT/usr/share/icons/hicolor"

# Install binary.
install -m 0755 "$BIN_PATH" "$PKGROOT/usr/bin/xbootsplash-gui"

# Install AppStream metadata
if [ -f "$SRC_ROOT/io.github.seb3773.xbootsplash.metainfo.xml" ]; then
	install -m 0644 "$SRC_ROOT/io.github.seb3773.xbootsplash.metainfo.xml" "$PKGROOT/usr/share/metainfo/io.github.seb3773.xbootsplash.metainfo.xml"
fi

# Install desktop entry.
cat > "$PKGROOT/usr/share/applications/xbootsplash-gui.desktop" <<EOF
[Desktop Entry]
Version=1.0
Name=XBootsplash Studio
GenericName=Boot Splash Creator & Studio
Comment=Interactive visual studio for ultra-fast Linux boot splashes
Exec=xbootsplash-gui
Icon=xbootsplash
Terminal=false
Type=Application
Categories=Utility;DesktopSettings;
Keywords=bootsplash;splash;boot;initramfs;theme;
EOF
chmod 0644 "$PKGROOT/usr/share/applications/xbootsplash-gui.desktop"

# Install application icon tree
ICON_SRC="$SRC_ROOT/icons/xbootsplash.png"
if test -f "$ICON_SRC"; then
	real_sz="64x64"
	real_dir="$PKGROOT/usr/share/icons/hicolor/$real_sz/apps"
	mkdir -p -- "$real_dir"
	install -m 0644 "$ICON_SRC" "$real_dir/xbootsplash.png"
	for sz in 16x16 22x22 24x24 32x32 48x48; do
		dstdir="$PKGROOT/usr/share/icons/hicolor/$sz/apps"
		mkdir -p -- "$dstdir"
		ln -sf "../../$real_sz/apps/xbootsplash.png" "$dstdir/xbootsplash.png"
	done
else
	echo "warning: missing $ICON_SRC (application icon will not be installed)" >&2
fi

# Specify explicit optimal runtime dependencies
DEPENDS="libtqt3-mt-trinity (>= 4:14.0.0) | libtqt3-mt, libglib2.0-0, gcc, make"

# Strip staged binary
STAGED_BIN="$PKGROOT/usr/bin/xbootsplash-gui"
if command -v sstrip >/dev/null 2>&1; then
	echo "info: stripping staged binary with sstrip"
	sstrip "$STAGED_BIN" >/dev/null 2>&1 || true
else
	echo "info: using strip --strip-all"
	strip --strip-all "$STAGED_BIN" >/dev/null 2>&1 || true
fi

# Debian control file.
INSTALLED_SIZE_KB="$(du -sk "$PKGROOT/usr" | awk '{print $1}')"
cat > "$PKGROOT/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $PKG_VERSION
Section: $PKG_SECTION
Priority: $PKG_PRIORITY
Architecture: $ARCH
Maintainer: $PKG_MAINTAINER
Installed-Size: $INSTALLED_SIZE_KB
Depends: $DEPENDS
Description: Autonomous GUI Studio for ultra-fast Linux boot splashes
 A lightweight, high-performance visual studio built with Trinity Qt3 (TQt3).
 Features interactive real-time positioning, color eyedropper, package
 inspector, hardware VT live testing without rebooting, and 1-click
 generation of native Debian packages and self-contained boot splashes.
EOF

# postinst: refresh caches.
cat > "$PKGROOT/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
	gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
	update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 0755 "$PKGROOT/DEBIAN/postinst"

# postrm: refresh caches.
cat > "$PKGROOT/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
	gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
	update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 0755 "$PKGROOT/DEBIAN/postrm"

OUT_DEB="$PROJECT_ROOT/${PKG_NAME}_${PKG_VERSION}_${ARCH}.deb"
rm -f -- "$OUT_DEB"

echo "info: building .deb package..."
dpkg-deb --build "$PKGROOT" "$OUT_DEB" >/dev/null

echo "=================================================="
echo "✔ Debian package successfully built: $OUT_DEB"
echo "  File size: $(stat -c%s "$OUT_DEB") bytes"
echo "=================================================="
exit 0

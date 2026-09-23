#!/usr/bin/env bash
set -euo pipefail

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SRC_ROOT/.." && pwd)"
BUILD_DIR="$SRC_ROOT/build"
APPDIR="$BUILD_DIR/AppDir"
APP_VERSION="${1:-1.0.0}"

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "error: missing required command: $1" >&2
		exit 1
	}
}

need_cmd cmake
need_cmd pkg-config
need_cmd strip
need_cmd sed
need_cmd awk
need_cmd cp
need_cmd chmod
need_cmd mkdir

# Make sure build dir exists
mkdir -p -- "$BUILD_DIR"

# Build the binary using the existing build.sh script
echo "info: building xbootsplash-gui version $APP_VERSION..."
"$SRC_ROOT/build.sh"

BIN_PATH="$BUILD_DIR/xbootsplash-gui"
if test ! -x "$BIN_PATH"; then
	echo "error: missing built binary: $BIN_PATH" >&2
	exit 1
fi

# Clean and create AppDir structure
echo "info: preparing AppDir..."
rm -rf -- "$APPDIR"
mkdir -p -- \
	"$APPDIR/usr/bin" \
	"$APPDIR/usr/lib" \
	"$APPDIR/usr/share/applications" \
	"$APPDIR/usr/share/metainfo" \
	"$APPDIR/usr/share/icons/hicolor/64x64/apps"

# Copy binary
cp -a "$BIN_PATH" "$APPDIR/usr/bin/xbootsplash-gui"

# Copy AppStream metadata
if [ -f "$SRC_ROOT/io.github.seb3773.xbootsplash.metainfo.xml" ]; then
	cp -a "$SRC_ROOT/io.github.seb3773.xbootsplash.metainfo.xml" "$APPDIR/usr/share/metainfo/io.github.seb3773.xbootsplash.metainfo.xml"
fi

# Strip staged binary
if command -v sstrip >/dev/null 2>&1; then
	echo "info: stripping staged binary with sstrip"
	sstrip "$APPDIR/usr/bin/xbootsplash-gui" >/dev/null 2>&1 || true
else
	echo "info: using strip --strip-all"
	strip --strip-all "$APPDIR/usr/bin/xbootsplash-gui" >/dev/null 2>&1 || true
fi

# Resolve and copy TQt3 and supporting libraries from ldd output
echo "info: copying library dependencies..."
libraries=(
	libtqt-mt.so.3
	libaudio.so.2
	libjpeg.so.62
	libpng16.so.16
)

# Run ldd and extract library paths
for lib in "${libraries[@]}"; do
	libpath=$(ldd "$BIN_PATH" | grep "$lib" | awk '{print $3}' || true)
	if [[ -z "$libpath" || ! -f "$libpath" ]]; then
		# Fallback search in standard paths
		libpath=$(find /lib /usr/lib /lib64 /usr/lib64 /usr/lib/x86_64-linux-gnu -name "$lib*" 2>/dev/null | head -n1 || true)
	fi
	if [[ -n "$libpath" && -f "$libpath" ]]; then
		echo "  -> bundling: $lib ($libpath)"
		cp -L "$libpath" "$APPDIR/usr/lib/"
	else
		echo "  warning: library $lib not resolved"
	fi
done

# Copy icon
ICON_SRC="$SRC_ROOT/icons/xbootsplash.png"
if test -f "$ICON_SRC"; then
	cp -a "$ICON_SRC" "$APPDIR/xbootsplash.png"
	cp -a "$ICON_SRC" "$APPDIR/usr/share/icons/hicolor/64x64/apps/xbootsplash.png"
else
	echo "error: missing $ICON_SRC" >&2
	exit 1
fi

# Create Desktop entry at root of AppDir and usr/share/applications
cat > "$APPDIR/xbootsplash-gui.desktop" <<EOF
[Desktop Entry]
Version=1.0
Name=XBootsplash Studio
GenericName=Boot Splash Creator & Studio
Comment=Interactive Studio for ultra-fast Linux boot splashes
Exec=xbootsplash-gui
Icon=xbootsplash
Terminal=false
Type=Application
Categories=Utility;DesktopSettings;
Keywords=bootsplash;splash;boot;initramfs;theme;
EOF
chmod 0644 "$APPDIR/xbootsplash-gui.desktop"
cp -a "$APPDIR/xbootsplash-gui.desktop" "$APPDIR/usr/share/applications/xbootsplash-gui.desktop"

# Create AppRun entry script
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
exec "$HERE/usr/bin/xbootsplash-gui" "$@"
EOF
chmod 0755 "$APPDIR/AppRun"

# Check for cached appimagetool or download
APPIMAGETOOL="$BUILD_DIR/appimagetool"
if [ ! -s "$APPIMAGETOOL" ]; then
	if [ -s "/home/cdef/_PROJETS/taskmgr/build/appimagetool" ]; then
		echo "info: reusing cached appimagetool from taskmgr..."
		cp -a "/home/cdef/_PROJETS/taskmgr/build/appimagetool" "$APPIMAGETOOL"
	else
		echo "info: downloading appimagetool..."
		wget -q --show-progress -O "$APPIMAGETOOL" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
		chmod +x "$APPIMAGETOOL"
	fi
fi

# Build AppImage using --appimage-extract-and-run to bypass FUSE requirements
OUT_APPIMAGE="$PROJECT_ROOT/xbootsplash-gui-x86_64.AppImage"
rm -f -- "$OUT_APPIMAGE"

echo "info: generating AppImage..."
export ARCH=x86_64
"$APPIMAGETOOL" --appimage-extract-and-run "$APPDIR" "$OUT_APPIMAGE"

echo "=================================================="
echo "✔ AppImage successfully built: $OUT_APPIMAGE"
echo "  File size: $(stat -c%s "$OUT_APPIMAGE") bytes"
echo "=================================================="
exit 0

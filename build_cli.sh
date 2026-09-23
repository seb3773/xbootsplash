#!/bin/bash
#
# build_cli.sh - Package standalone CLI builder into a distributable tarball
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION="1.0.0"
PKG_NAME="xbootsplash-cli-${VERSION}"
ARCHIVE_NAME="${PKG_NAME}.tar.gz"
WORK_DIR=$(mktemp -d -p /tmp xbs_cli_pkg.XXXXXX)

cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

echo "=== Packaging ${ARCHIVE_NAME} ==="

TARGET_DIR="$WORK_DIR/$PKG_NAME"
mkdir -p "$TARGET_DIR"

# Copy required components
cp -r "$SCRIPT_DIR/CLI_src" "$TARGET_DIR/"
cp -r "$SCRIPT_DIR/engine_src" "$TARGET_DIR/"
cp -r "$SCRIPT_DIR/zx0" "$TARGET_DIR/"
cp -r "$SCRIPT_DIR/upkr" "$TARGET_DIR/"
cp -r "$SCRIPT_DIR/icons" "$TARGET_DIR/"
cp "$SCRIPT_DIR/Makefile" "$TARGET_DIR/"

# Create root launcher
cat << 'EOF' > "$TARGET_DIR/build_anim.sh"
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/CLI_src/build_anim.sh" "$@"
EOF
chmod +x "$TARGET_DIR/build_anim.sh"

# Add README for CLI users
cat << 'EOF' > "$TARGET_DIR/README.md"
# xbootsplash CLI

Minimalist, ultra-fast boot splash generator for Linux x86_64.

## Quick Start

### 1. Interactive Builder
```bash
./build_anim.sh /path/to/frames_directory
```

### 2. Direct Rebuild / Automated Build Example
```bash
./build_anim.sh -m 0 -x 0 -y 80 -d 33 -c 000000 /path/to/frames_directory
```

### 3. Install to initramfs
Follow the interactive menu upon build completion, or run:
```bash
./build_anim.sh --install-package your_theme.xbs
```
EOF

# Create tarball
tar -czf "$SCRIPT_DIR/$ARCHIVE_NAME" -C "$WORK_DIR" "$PKG_NAME"

echo "Package created successfully: $SCRIPT_DIR/$ARCHIVE_NAME"
ls -lh "$SCRIPT_DIR/$ARCHIVE_NAME"

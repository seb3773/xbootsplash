#!/usr/bin/env bash
set -euo pipefail

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SRC_ROOT/build"

export PATH="/opt/trinity/bin:$PATH"

need_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing tool: $1" >&2
        return 1
    fi
    return 0
}

need_cmd cmake
need_cmd pkg-config
need_cmd tqmoc

mkdir -p "$BUILD_DIR"

echo "=================================================="
echo "  Configuration and build of xbootsplash-gui"
echo "=================================================="

cmake -S "$SRC_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DXBOOTSPLASH_AGGRESSIVE_FLAGS=ON
cmake --build "$BUILD_DIR" -j"$(nproc)"

BIN_PATH="$BUILD_DIR/xbootsplash-gui"
if [ -x "$BIN_PATH" ]; then
    if command -v sstrip >/dev/null 2>&1; then
        echo "Final optimization with sstrip..."
        sstrip "$BIN_PATH" >/dev/null 2>&1 || true
    else
        echo "Final optimization with strip..."
        strip --strip-all "$BIN_PATH" >/dev/null 2>&1 || true
    fi
    echo ""
    echo "✔ Build succeeded: $BIN_PATH"
    echo "  Binary size: $(stat -c%s "$BIN_PATH") bytes"
else
    echo "❌ Error: $BIN_PATH not found after build!" >&2
    exit 1
fi

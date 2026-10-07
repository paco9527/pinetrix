#!/bin/bash
# Usage:
#   ./makeall.sh aarch64-linux-gnu-             # cross-compile (aarch64)
#   ./makeall.sh aarch64-linux-gnu- buildlib    # + first-time extlib build
#   ./makeall.sh                                # native build

set -e

PREFIX=${1:-}
BUILDLIB=${2:-}

if [ -n "$PREFIX" ]; then
    BUILD_DIR="build-${PREFIX%-}"     # e.g. build-aarch64-linux-gnu
else
    BUILD_DIR="build"
fi

mkdir -p output

if [ "$BUILDLIB" = "buildlib" ]; then
    echo "=== Building extlib ==="
    cd core/extlib
    ./build.sh "$(pwd)" "${PREFIX}gcc" "${PREFIX}g++"
    cd ../..
fi

echo "=== Building pinetrix ($BUILD_DIR) ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
if [ -n "$PREFIX" ]; then
    cmake .. -DCOMPILER_PREFIX="${PREFIX}"
else
    cmake ..
fi
make -j$(nproc)
cd ..

cp "$BUILD_DIR/pinetrix" output/pinetrix 2>/dev/null || true
echo "=== Done: output/pinetrix ==="

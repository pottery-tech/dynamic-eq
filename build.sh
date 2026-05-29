#!/usr/bin/env bash
# Build script for DynamicEQ
# Requires: CMake 3.22+, Clang or GCC, git

set -e

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

echo "=== DynamicEQ Build ==="
echo "Config: $BUILD_TYPE"

# Configure
cmake -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build (use all cores)
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)

echo ""
echo "=== Build complete ==="
echo "Plugins are in: $BUILD_DIR/DynamicEQ_artefacts/$BUILD_TYPE/"

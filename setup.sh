#!/bin/bash

# Setup script for android-mem-kit
# This script initializes git submodules for dependencies

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Android-Mem-Kit Setup ==="
echo ""

# Initialize and update git submodules
echo "Initializing git submodules..."
cd "$SCRIPT_DIR"
git submodule update --init --recursive

echo ""
echo "✓ Dependencies installed successfully"
echo ""
echo "Dependencies:"
echo "  - XDL: $SCRIPT_DIR/deps/xdl"
echo "  - ShadowHook: $SCRIPT_DIR/deps/shadowhook"
echo ""
echo "Next steps:"
echo "1. Set your NDK path:"
echo "   export ANDROID_NDK_HOME=/path/to/android-ndk-r29"
echo ""
echo "2. Build the project:"
echo "   cd build"
echo "   cmake .. -DCMAKE_TOOLCHAIN_FILE=\$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \\"
echo "            -DANDROID_ABI=arm64-v8a \\"
echo "            -DANDROID_PLATFORM=android-21"
echo "   cmake --build ."
echo ""
echo "For more information, see README.md"

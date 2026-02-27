#!/bin/bash

# Setup script for android-mem-kit
# This script clones the required dependencies

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPS_DIR="$SCRIPT_DIR/deps"

echo "=== Android-Mem-Kit Setup ==="
echo ""

# Create deps directory if it doesn't exist
mkdir -p "$DEPS_DIR"

# Clone XDL
if [ -d "$DEPS_DIR/xdl" ]; then
	echo "✓ XDL already exists at: $DEPS_DIR/xdl"
	echo "  To update, run: cd $DEPS_DIR/xdl && git pull"
else
	echo "Cloning XDL..."
	git clone https://github.com/hexhacking/xdl.git "$DEPS_DIR/xdl"
	echo "✓ XDL cloned successfully"
fi

# Clone ShadowHook (android-inline-hook)
if [ -d "$DEPS_DIR/shadowhook" ]; then
	echo "✓ ShadowHook already exists at: $DEPS_DIR/shadowhook"
	echo "  To update, run: cd $DEPS_DIR/shadowhook && git pull"
else
	echo "Cloning ShadowHook (android-inline-hook)..."
	git clone https://github.com/bytedance/android-inline-hook.git "$DEPS_DIR/shadowhook"
	echo "✓ ShadowHook cloned successfully"
fi

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Dependencies installed:"
echo "  - XDL: $DEPS_DIR/xdl"
echo "  - ShadowHook: $DEPS_DIR/shadowhook"
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

#!/bin/bash
# Build script for Linux
# Requires: raylib and libvips-dev installed

echo "============================================"
echo " Image Compressor - Build (Linux)"
echo "============================================"
echo ""

# Setup Paths and dependencies
VIPS_CFLAGS=""
VIPS_LIBS=""
RAYLIB_CFLAGS=""
RAYLIB_LIBS="-lraylib -lGL -lm -lpthread -ldl -lrt -lX11"

# Check for vendor folder
if [ -d "vendor/vips" ]; then
    echo "[INFO] Using vendored libvips..."
    VIPS_CFLAGS="-Ivendor/vips/include"
    VIPS_LIBS="-Lvendor/vips/lib -lvips"
elif pkg-config --exists vips; then
    VIPS_CFLAGS=$(pkg-config --cflags vips)
    VIPS_LIBS=$(pkg-config --libs vips)
else
    echo "[WARN] libvips not found."
    echo "Install with: sudo apt install libvips-dev"
    # We don't auto-download vips on linux as it's too complex
    exit 1
fi

if [ -d "vendor/raylib" ]; then
    echo "[INFO] Using vendored raylib..."
    RAYLIB_CFLAGS="-Ivendor/raylib/include"
    RAYLIB_LIBS="-Lvendor/raylib/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11"
fi

echo "Building..."

gcc src/main.c src/processor.c -o compressor \
    $VIPS_CFLAGS $RAYLIB_CFLAGS \
    -Iinclude \
    $VIPS_LIBS $RAYLIB_LIBS \
    -O2

if [ $? -eq 0 ]; then
    echo ""
    echo "============================================"
    echo " BUILD SUCCESSFUL!"
    echo "============================================"
    echo "Run: ./compressor"
    echo ""
else
    echo ""
    echo "============================================"
    echo " BUILD FAILED"
    echo "============================================"
    echo ""
    echo "Make sure you have installed:"
    echo "  - libvips-dev (or libvips-devel)"
    echo "  - raylib (libraylib-dev)"
    echo ""
fi

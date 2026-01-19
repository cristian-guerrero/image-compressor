#!/bin/bash
# Build script for Linux
# Requires: raylib and libvips-dev installed

echo "============================================"
echo " Image Compressor - Build (Linux)"
echo "============================================"
echo ""

# Check for pkg-config
if ! command -v pkg-config &> /dev/null; then
    echo "ERROR: pkg-config not found."
    echo "Install with: sudo apt install pkg-config"
    exit 1
fi

# Check for vips
if ! pkg-config --exists vips; then
    echo "ERROR: libvips not found."
    echo ""
    echo "Install with:"
    echo "  Ubuntu/Debian: sudo apt install libvips-dev"
    echo "  Arch Linux:    sudo pacman -S libvips"
    echo "  Fedora:        sudo dnf install vips-devel"
    exit 1
fi

echo "Building with libvips and raylib..."
echo "VIPS_CFLAGS: $(pkg-config --cflags vips)"
echo "VIPS_LIBS:   $(pkg-config --libs vips)"
echo ""

gcc main.c processor.c -o compressor \
    $(pkg-config --cflags --libs vips) \
    -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 \
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

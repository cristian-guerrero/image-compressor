#!/bin/bash
# Script to fetch and setup vendor dependencies for Linux

VENDOR_DIR="$(pwd)/vendor"
RAYLIB_VERSION="5.0"

echo "============================================"
echo " Setting up Vendor Dependencies (Linux)"
echo "============================================"

mkdir -p "$VENDOR_DIR"

# --- RAYLIB ---
if [ ! -d "$VENDOR_DIR/raylib" ]; then
    echo ""
    echo "[+] Downloading Raylib $RAYLIB_VERSION..."
    URL="https://github.com/raysan5/raylib/releases/download/$RAYLIB_VERSION/raylib-$RAYLIB_VERSION_linux_amd64.tar.gz"
    curl -L "$URL" -o raylib.tar.gz
    
    echo "[+] Extracting Raylib..."
    mkdir -p "$VENDOR_DIR/raylib_tmp"
    tar -xzf raylib.tar.gz -C "$VENDOR_DIR/raylib_tmp" --strip-components=1
    mv "$VENDOR_DIR/raylib_tmp" "$VENDOR_DIR/raylib"
    rm raylib.tar.gz
else
    echo "[OK] Raylib already exists in vendor/raylib"
fi

# --- LIBVIPS (Note: Hard to vendor on Linux) ---
if [ ! -d "$VENDOR_DIR/vips" ]; then
    echo ""
    echo "[NOTICE] Libvips is complex to vendor on Linux due to shared dependencies."
    echo "It is highly recommended to use the system package manager:"
    echo "  sudo apt install libvips-dev"
    echo ""
    echo "If you truly need it vendorized, you would need to build it from source"
    echo "into the vendor folder. This script currently only vendors Raylib."
else
    echo "[OK] Libvips folder found in vendor/vips"
fi

echo ""
echo "============================================"
echo " VENDOR SETUP COMPLETE"
echo "============================================"
echo ""

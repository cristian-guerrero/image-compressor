#!/bin/bash
set -euo pipefail

echo "============================================"
echo " Image Compressor - Build (Linux)"
echo "============================================"
echo ""

ROOT="$(cd "$(dirname "$0")" && pwd)"
EXTERNAL="$ROOT/external"
CACHE="$ROOT/.build-cache"
BUILD_DIR="$ROOT/build"
mkdir -p "$EXTERNAL" "$CACHE" "$BUILD_DIR"

if ! command -v pkg-config &> /dev/null; then
    echo "ERROR: pkg-config not found (required to read local .pc files)."
    exit 1
fi
if ! command -v wget &> /dev/null; then
    echo "ERROR: wget not found (required to download dependencies)."
    exit 1
fi

RAYLIB_VERSION="${RAYLIB_VERSION:-5.5}"
RAYLIB_URL="${RAYLIB_URL:-https://github.com/raysan5/raylib/releases/download/${RAYLIB_VERSION}/raylib-${RAYLIB_VERSION}_linux_amd64.tar.gz}"
RAYLIB_DIR="$EXTERNAL/raylib"

LIBVIPS_VERSION="${LIBVIPS_VERSION:-8.14.5}"
# Default to sharp-libvips prebuilt binaries (glibc, x64). Override LIBVIPS_URL if you prefer another build.
LIBVIPS_URL="${LIBVIPS_URL:-https://github.com/lovell/sharp-libvips/releases/download/v${LIBVIPS_VERSION}/libvips-${LIBVIPS_VERSION}-linux-x64.tar.gz}"
LIBVIPS_DIR="$EXTERNAL/libvips"

download_and_extract() {
    local url="$1" tar_path="$2" target_root="$3"
    rm -rf "$target_root" "$tar_path"
    echo "Downloading: $url"
    wget -q --show-progress "$url" -O "$tar_path"
    mkdir -p "$target_root"
    tar -xf "$tar_path" -C "$target_root"
}

ensure_raylib() {
    if [ -f "$RAYLIB_DIR/lib/libraylib.a" ] || [ -f "$RAYLIB_DIR/lib/libraylib.so" ]; then
        echo "raylib: using cached copy at $RAYLIB_DIR"
        return
    fi
    local tar_path="$CACHE/raylib.tar.gz"
    local unpack="$CACHE/raylib-unpack"
    download_and_extract "$RAYLIB_URL" "$tar_path" "$unpack"

    local include_dir lib_dir
    include_dir=$(find "$unpack" -type f -name "raylib.h" -print -quit | xargs -r dirname)
    lib_dir=$(find "$unpack" -type f -name "libraylib.*" -print -quit | xargs -r dirname)
    if [ -z "$include_dir" ] || [ -z "$lib_dir" ]; then
        echo "ERROR: raylib package layout not recognized. Set RAYLIB_URL to a compatible tarball."
        exit 1
    fi

    rm -rf "$RAYLIB_DIR"
    mkdir -p "$RAYLIB_DIR/include" "$RAYLIB_DIR/lib"
    cp -r "$include_dir"/* "$RAYLIB_DIR/include/"
    cp -r "$lib_dir"/* "$RAYLIB_DIR/lib/"
    echo "raylib: installed to $RAYLIB_DIR"
}

ensure_libvips() {
    if [ -f "$LIBVIPS_DIR/lib/pkgconfig/vips.pc" ]; then
        echo "libvips: using cached copy at $LIBVIPS_DIR"
        return
    fi
    local tar_path="$CACHE/libvips.tar.xz"
    local unpack="$CACHE/libvips-unpack"
    download_and_extract "$LIBVIPS_URL" "$tar_path" "$unpack"

    local pc_dir
    pc_dir=$(find "$unpack" -path "*/lib/pkgconfig/vips.pc" -print -quit | xargs -r dirname)

    rm -rf "$LIBVIPS_DIR"
    mkdir -p "$LIBVIPS_DIR"

    if [ -n "$pc_dir" ]; then
        local root_dir
        root_dir=$(echo "$pc_dir" | sed 's,/lib/pkgconfig,,')
        cp -r "$root_dir"/* "$LIBVIPS_DIR/"
        echo "libvips: installed to $LIBVIPS_DIR (pkgconfig found)"
        return
    fi

    # Fallback: sharp-libvips tarball sin pkg-config. Copiamos todo y generamos vips.pc mínimo.
    cp -r "$unpack"/* "$LIBVIPS_DIR/"
    mkdir -p "$LIBVIPS_DIR/lib/pkgconfig"
    cat > "$LIBVIPS_DIR/lib/pkgconfig/vips.pc" <<EOF
prefix=\${pcfiledir}/../..
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: vips
Description: libvips image processing
Version: ${LIBVIPS_VERSION}
Libs: -L\${libdir} -lvips-cpp
Cflags: -I\${includedir}
EOF

    # Crear symlink convencional
    if [ -f "$LIBVIPS_DIR/lib/libvips-cpp.so.42" ] && [ ! -f "$LIBVIPS_DIR/lib/libvips-cpp.so" ]; then
        ln -s libvips-cpp.so.42 "$LIBVIPS_DIR/lib/libvips-cpp.so"
    fi

    echo "libvips: installed to $LIBVIPS_DIR (pkgconfig synthesized)"
}

ensure_raylib
ensure_libvips

export PKG_CONFIG_PATH="$LIBVIPS_DIR/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="$LIBVIPS_DIR/lib:$RAYLIB_DIR/lib:${LD_LIBRARY_PATH:-}"

VIPS_CFLAGS=$(pkg-config --cflags vips)
VIPS_LIBS=$(pkg-config --libs vips)

# Some prebuilt libvips packages (sharp-libvips) include glib headers under
# include/glib-2.0 and lib/glib-2.0/include. Ensure those appear in CFLAGS so
# includes like <glib.h> are found when compiling.
if [ -d "$LIBVIPS_DIR/include/glib-2.0" ]; then
    VIPS_CFLAGS="$VIPS_CFLAGS -I$LIBVIPS_DIR/include/glib-2.0"
fi
if [ -d "$LIBVIPS_DIR/lib/glib-2.0/include" ]; then
    VIPS_CFLAGS="$VIPS_CFLAGS -I$LIBVIPS_DIR/lib/glib-2.0/include"
fi

echo "Building with vendored dependencies..."
echo "RAYLIB_DIR: $RAYLIB_DIR"
echo "LIBVIPS_DIR: $LIBVIPS_DIR"
echo "VIPS_CFLAGS: $VIPS_CFLAGS"
echo "VIPS_LIBS:   $VIPS_LIBS"
echo ""

gcc src/main.c src/processor.c -o "$BUILD_DIR/compressor" \
    $VIPS_CFLAGS \
    -Iinclude \
    -I"$RAYLIB_DIR/include" \
    $VIPS_LIBS \
    -L"$RAYLIB_DIR/lib" \
    -lraylib -l:libGL.so.1 -lm -lpthread -ldl -lrt -l:libX11.so.6 -l:libXrandr.so.2 -l:libXi.so.6 -l:libXinerama.so.1 -l:libXcursor.so.1 \
    -Wl,-rpath,'$ORIGIN/../external/libvips/lib:$ORIGIN/../external/raylib/lib' \
    -O2

echo ""
echo "============================================"
echo " BUILD SUCCESSFUL!"
echo "============================================"
echo "Binary: $BUILD_DIR/compressor"
echo "Run: LD_LIBRARY_PATH=$LD_LIBRARY_PATH $BUILD_DIR/compressor"
echo ""

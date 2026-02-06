#!/bin/bash

# Script para compilar y crear un AppImage de image-compressor sin contenedores
# Este script genera un AppImage autocontenido que funciona en cualquier distribución Linux

set -e

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

APP_NAME="ImageCompressor"
APP_VERSION="1.0.0"
PROJECT_DIR="$(pwd)"

echo -e "${GREEN}=== Creando AppImage de $APP_NAME (sin contenedores) ===${NC}\n"

echo -e "${BLUE}[INFO]${NC} Directorio del proyecto: $PROJECT_DIR"
echo -e "${BLUE}[INFO]${NC} Ejecutando build y creación de AppImage...\n"

# ============================================
# PASO 1: Verificar dependencias del sistema
# ============================================
echo -e "${BLUE}[1/7]${NC} Verificando dependencias del sistema..."

# Verificar compilador
if ! command -v gcc &> /dev/null; then
    echo -e "${RED}[ERROR]${NC} gcc no está instalado. Instálalo con: sudo apt install build-essential"
    exit 1
fi

# Verificar pkg-config
if ! command -v pkg-config &> /dev/null; then
    echo -e "${RED}[ERROR]${NC} pkg-config no está instalado. Instálalo con: sudo apt install pkg-config"
    exit 1
fi

# Preferir dependencias vendorizadas en external/
EXTERNAL="$PROJECT_DIR/external"
if [ -f "$EXTERNAL/libvips/lib/pkgconfig/vips.pc" ]; then
    export PKG_CONFIG_PATH="$EXTERNAL/libvips/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    echo -e "${BLUE}[INFO]${NC} Usando libvips vendorizado en $EXTERNAL/libvips"
fi

# Verificar libvips (sistema o vendorizado)
if ! pkg-config --exists vips; then
    echo -e "${RED}[ERROR]${NC} libvips no está instalado ni vendorizado. Instálalo con: sudo apt install libvips-dev or put it in $EXTERNAL/libvips"
    exit 1
fi

# Verificar raylib (sistema o vendorizado)
if pkg-config --exists raylib; then
    echo -e "${GREEN}✓${NC} raylib encontrado via pkg-config"
else
    if [ -d "$EXTERNAL/raylib/include" ] && [ -d "$EXTERNAL/raylib/lib" ]; then
        echo -e "${BLUE}[INFO]${NC} raylib vendorizado encontrado en $EXTERNAL/raylib"
    else
        echo -e "${YELLOW}[WARN]${NC} raylib no encontrado via pkg-config ni en external/, el link enlazará con -lraylib y esperamos que esté en el sistema"
    fi
fi

echo -e "${GREEN}✓${NC} Dependencias verificadas"

# ============================================
# PASO 2: Compilar el binario
# ============================================
echo -e "${BLUE}[2/7]${NC} Compilando binario..."

mkdir -p build

if [ -f "$PROJECT_DIR/build/compressor" ]; then
    echo -e "${BLUE}[INFO]${NC} Binario ya existe en build/, omitiendo compilación"
else
    # Si hay raylib vendorizado, úsalo
    RAYLIB_CFLAGS=""
    RAYLIB_LIBS="-lraylib"
    if [ -d "$EXTERNAL/raylib/include" ]; then
        RAYLIB_CFLAGS="-I$EXTERNAL/raylib/include"
    fi
    if [ -d "$EXTERNAL/raylib/lib" ]; then
        RAYLIB_LIBS="-L$EXTERNAL/raylib/lib -lraylib"
    fi

    # Ajustes para glib headers que a veces vienen en libvips precompilado
    VIPS_CFLAGS_EXTRA=""
    if [ -d "$EXTERNAL/libvips/include/glib-2.0" ]; then
        VIPS_CFLAGS_EXTRA="$VIPS_CFLAGS_EXTRA -I$EXTERNAL/libvips/include/glib-2.0"
    fi
    if [ -d "$EXTERNAL/libvips/lib/glib-2.0/include" ]; then
        VIPS_CFLAGS_EXTRA="$VIPS_CFLAGS_EXTRA -I$EXTERNAL/libvips/lib/glib-2.0/include"
    fi

    # Asegurar que el binario busque librerías en AppDir/usr/lib al correr dentro del AppImage
    RPATH="\$ORIGIN/../lib"
    # También añadir rutas absolutas a external para ejecución local previa al empaquetado
    if [ -d "$EXTERNAL/libvips/lib" ]; then
        RPATH="$RPATH:$EXTERNAL/libvips/lib"
    fi
    if [ -d "$EXTERNAL/raylib/lib" ]; then
        RPATH="$RPATH:$EXTERNAL/raylib/lib"
    fi

    gcc src/main.c src/processor.c \
        $(pkg-config --cflags vips) $VIPS_CFLAGS_EXTRA $RAYLIB_CFLAGS \
        -Iinclude \
        -o build/compressor \
        $(pkg-config --libs vips) $RAYLIB_LIBS \
        -lGL -lm -lpthread -ldl -lrt -lX11 \
        -Wl,-rpath,"$RPATH" \
        -O3 -DNDEBUG

    echo -e "${GREEN}✓${NC} Binario compilado exitosamente"
fi

# ============================================
# PASO 3: Verificar/Descargar herramientas de AppImage
# ============================================
echo -e "${BLUE}[3/7]${NC} Verificando herramientas de AppImage..."

APPIMAGE_TOOLS_DIR="${APPIMAGE_TOOLS_DIR:-$PROJECT_DIR/.build-cache/appimage-tools}"
mkdir -p "$APPIMAGE_TOOLS_DIR"

# URLs de las herramientas de AppImage
LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
APPIMAGETOOL_URL="https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"

# Variables para las herramientas (se asignarán después de verificar)
LINUXDEPLOY=""
APPIMAGETOOL=""

# Función para verificar si un AppImage es ejecutable
verify_appimage() {
    local appimage="$1"
    if [ -f "$appimage" ] && [ -x "$appimage" ]; then
        # Intentar ejecutar con --help para verificar que funciona
        if "$appimage" --help &> /dev/null || "$appimage" --version &> /dev/null 2>&1; then
            return 0
        fi
    fi
    return 1
}

# 1. Verificar si están en el PATH del sistema
echo -e "${YELLOW}[INFO]${NC} Buscando herramientas en el sistema..."

if command -v linuxdeploy &> /dev/null; then
    LINUXDEPLOY=$(command -v linuxdeploy)
    echo -e "${GREEN}✓${NC} linuxdeploy encontrado en el sistema: $LINUXDEPLOY"
fi

if command -v appimagetool &> /dev/null; then
    APPIMAGETOOL=$(command -v appimagetool)
    echo -e "${GREEN}✓${NC} appimagetool encontrado en el sistema: $APPIMAGETOOL"
fi

# 2. Verificar si están en /opt/appimage-tools
if [ -z "$LINUXDEPLOY" ]; then
    if verify_appimage "/opt/appimage-tools/linuxdeploy-x86_64.AppImage"; then
        LINUXDEPLOY="/opt/appimage-tools/linuxdeploy-x86_64.AppImage"
        echo -e "${GREEN}✓${NC} linuxdeploy encontrado en /opt/appimage-tools"
    fi
fi

if [ -z "$APPIMAGETOOL" ]; then
    if verify_appimage "/opt/appimage-tools/appimagetool-x86_64.AppImage"; then
        APPIMAGETOOL="/opt/appimage-tools/appimagetool-x86_64.AppImage"
        echo -e "${GREEN}✓${NC} appimagetool encontrado en /opt/appimage-tools"
    fi
fi

# 3. Verificar si están en el directorio local .appimage-tools
if [ -z "$LINUXDEPLOY" ]; then
    LOCAL_LINUXDEPLOY="$APPIMAGE_TOOLS_DIR/linuxdeploy-x86_64.AppImage"
    if verify_appimage "$LOCAL_LINUXDEPLOY"; then
        LINUXDEPLOY="$LOCAL_LINUXDEPLOY"
        echo -e "${GREEN}✓${NC} linuxdeploy encontrado en .appimage-tools"
    fi
fi

if [ -z "$APPIMAGETOOL" ]; then
    LOCAL_APPIMAGETOOL="$APPIMAGE_TOOLS_DIR/appimagetool-x86_64.AppImage"
    if verify_appimage "$LOCAL_APPIMAGETOOL"; then
        APPIMAGETOOL="$LOCAL_APPIMAGETOOL"
        echo -e "${GREEN}✓${NC} appimagetool encontrado en .appimage-tools"
    fi
fi

# 4. Descargar si no se encontraron en ningún lugar
if [ -z "$LINUXDEPLOY" ]; then
    echo -e "${YELLOW}[INFO]${NC} Descargando linuxdeploy..."
    wget -q --show-progress "$LINUXDEPLOY_URL" -O "$APPIMAGE_TOOLS_DIR/linuxdeploy-x86_64.AppImage"
    chmod +x "$APPIMAGE_TOOLS_DIR/linuxdeploy-x86_64.AppImage"
    LINUXDEPLOY="$APPIMAGE_TOOLS_DIR/linuxdeploy-x86_64.AppImage"
    echo -e "${GREEN}✓${NC} linuxdeploy descargado"
fi

if [ -z "$APPIMAGETOOL" ]; then
    echo -e "${YELLOW}[INFO]${NC} Descargando appimagetool..."
    wget -q --show-progress "$APPIMAGETOOL_URL" -O "$APPIMAGE_TOOLS_DIR/appimagetool-x86_64.AppImage"
    chmod +x "$APPIMAGE_TOOLS_DIR/appimagetool-x86_64.AppImage"
    APPIMAGETOOL="$APPIMAGE_TOOLS_DIR/appimagetool-x86_64.AppImage"
    echo -e "${GREEN}✓${NC} appimagetool descargado"
fi

echo -e "${GREEN}✓${NC} Herramientas de AppImage listas"

# ============================================
# PASO 4: Crear estructura AppDir
# ============================================
echo -e "${BLUE}[4/7]${NC} Creando estructura AppDir..."

APPDIR="$PROJECT_DIR/build/AppDir"

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$APPDIR/usr/share/icons/hicolor/512x512/apps"

# Copiar binario: usar build/compressor si existe, si no compilar
if [ -f "$PROJECT_DIR/build/compressor" ]; then
    cp "$PROJECT_DIR/build/compressor" "$APPDIR/usr/bin/compressor"
    chmod +x "$APPDIR/usr/bin/compressor"
else
    echo -e "${YELLOW}[INFO]${NC} binario no encontrado en build/, ejecutando ./build_linux.sh"
    (cd "$PROJECT_DIR" && ./build_linux.sh)
    cp "$PROJECT_DIR/build/compressor" "$APPDIR/usr/bin/compressor"
    chmod +x "$APPDIR/usr/bin/compressor"
fi

# Crear archivo .desktop
cat > "$APPDIR/usr/share/applications/${APP_NAME}.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=Image Compressor
Comment=Compress images with various formats
Exec=compressor
Icon=ImageCompressor
Categories=Graphics;Utility;
Terminal=false
StartupNotify=true
EOF

# Copiar archivo .desktop a la raíz de AppDir (requerido por appimagetool)
cp "$APPDIR/usr/share/applications/${APP_NAME}.desktop" "$APPDIR/${APP_NAME}.desktop"

# Copiar icono
if [ -f 'resources/icon.png' ]; then
    cp resources/icon.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/compressor.png"
else
    echo -e "${YELLOW}[WARN]${NC} No se encontró icono de 256x256"
fi

if [ -f 'resources/icon512.png' ]; then
    cp resources/icon512.png "$APPDIR/usr/share/icons/hicolor/512x512/apps/compressor.png"
else
    echo -e "${YELLOW}[WARN]${NC} No se encontró icono de 512x512"
fi

# Copiar icono a la raíz de AppDir (requerido por appimagetool)
if [ -f 'resources/icon.png' ]; then
    cp resources/icon.png "$APPDIR/${APP_NAME}.png"
else
    echo -e "${YELLOW}[WARN]${NC} No se encontró icono para AppDir"
fi

# Copiar recursos (fuentes, etc)
echo -e "${BLUE}[INFO]${NC} Copiando recursos a AppDir..."
mkdir -p "$APPDIR/usr/bin/resources"
cp -r resources/* "$APPDIR/usr/bin/resources/"

echo -e "${GREEN}✓${NC} Estructura AppDir creada"

# ============================================
# PASO 5: Copiar dependencias manualmente
# ============================================
echo -e "${BLUE}[5/7]${NC} Copiando dependencias..."

mkdir -p "$APPDIR/usr/lib"

# Librerías del sistema que NO deben copiarse (deben venir del host)
SYSTEM_LIBS='libc.so.6 libm.so.6 libdl.so.2 libpthread.so.0 librt.so.1 ld-linux-x86-64.so.2 libstdc++.so.6 libgcc_s.so.1'

# Función para copiar librerías y sus dependencias recursivamente
copy_libs() {
    local binary="$1"
    local dest="$2"
    local copied_libs="$3"
    
    # Obtener lista de librerías necesarias
    ldd "$binary" | grep '=>' | awk '{print $3}' | sort -u | while read lib; do
        if [ -f "$lib" ]; then
            local libname=$(basename "$lib")
            
            # Saltar librerías del sistema
            if echo "$SYSTEM_LIBS" | grep -q "$libname"; then
                echo "  Omitida (sistema): $libname"
                continue
            fi
            
            # Verificar si ya fue copiada
            if ! echo "$copied_libs" | grep -q "$libname"; then
                # Evitar copiar si el origen ya está dentro del destino (mismo archivo)
                if [ "$(readlink -f "$lib")" != "$(readlink -f "$dest/$libname" 2>/dev/null || true)" ]; then
                    cp "$lib" "$dest/" || true
                    echo "  Copiada: $libname"
                else
                    echo "  Omitida (ya en destino): $libname"
                fi
                copied_libs="$copied_libs $libname"

                # Copiar dependencias de esta librería recursivamente
                copy_libs "$lib" "$dest" "$copied_libs"
            fi
        fi
    done
}

# Copiar librerías del binario principal
copied=''
copy_libs "$APPDIR/usr/bin/compressor" "$APPDIR/usr/lib" "$copied"

# Copiar librerías vendorizadas (libvips, raylib) si existen en external/
if [ -d "$PROJECT_DIR/external/libvips/lib" ]; then
    echo -e "${BLUE}[INFO]${NC} Copiando libvips vendorizado a AppDir/usr/lib..."
    for f in "$PROJECT_DIR/external/libvips/lib/"*; do
        [ -e "$f" ] || continue
        dest="$APPDIR/usr/lib/$(basename "$f")"
        # Si ya existe el destino, omitir (evita copiar sobre sí mismo)
        if [ -e "$dest" ]; then
            continue
        fi
        cp -a "$f" "$dest" 2>/dev/null || true
    done
fi
if [ -d "$PROJECT_DIR/external/raylib/lib" ]; then
    echo -e "${BLUE}[INFO]${NC} Copiando raylib vendorizado a AppDir/usr/lib..."
    for f in "$PROJECT_DIR/external/raylib/lib/"*; do
        [ -e "$f" ] || continue
        dest="$APPDIR/usr/lib/$(basename "$f")"
        # Si ya existe el destino, omitir (evita copiar sobre sí mismo)
        if [ -e "$dest" ]; then
            continue
        fi
        cp -a "$f" "$dest" 2>/dev/null || true
    done
fi

# Copiar librerías específicas de vips que pueden no aparecer en ldd
VIPS_LIBS=$(pkg-config --libs-only-L vips | sed 's/-L//g' || true)
for lib_dir in $VIPS_LIBS; do
    if [ -d "$lib_dir" ]; then
        find "$lib_dir" -maxdepth 1 -name 'libvips*.so*' -exec cp {} "$APPDIR/usr/lib/" \; 2>/dev/null || true
    fi
done

# Intentar copiar dependencias detectadas por ldd (recursivo)
copied=''  # reusar variable
copy_libs "$APPDIR/usr/bin/compressor" "$APPDIR/usr/lib" "$copied"

echo -e "${GREEN}✓${NC} Dependencias copiadas"

# ============================================
# PASO 6: Crear AppRun
# ============================================
echo -e "${BLUE}[6/7]${NC} Creando AppRun..."

cat > "$APPDIR/AppRun" << 'EOF'
#!/bin/bash

SELF=$(readlink -f "$0")
HERE=${SELF%/*}

# Configurar rutas de librerías
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
export PATH="${HERE}/usr/bin:${PATH}"

# Configurar rutas de datos de la aplicación
export XDG_DATA_DIRS="${HERE}/usr/share:${XDG_DATA_DIRS}"

# Cambiar al directorio del binario para que encuentre los recursos relativos
cd "${HERE}/usr/bin"

# Ejecutar la aplicación
exec ./compressor "$@"
EOF

chmod +x "$APPDIR/AppRun"

echo -e "${GREEN}✓${NC} AppRun creado"

# ============================================
# PASO 7: Generar AppImage
# ============================================
echo -e "${BLUE}[7/7]${NC} Generando AppImage..."

APPIMAGE_FILE="build/${APP_NAME}-${APP_VERSION}-x86_64.AppImage"

# Eliminar AppImage anterior si existe
rm -f "${APPIMAGE_FILE}"

# Asegurar que AppDir/usr/lib esté antes en LD_LIBRARY_PATH al ejecutar appimagetool
# (algunas versiones de appimagetool respetan LD_LIBRARY_PATH durante el empaquetado)
export LD_LIBRARY_PATH="$APPDIR/usr/lib:${LD_LIBRARY_PATH:-}"

# Generar AppImage
"$APPIMAGETOOL" "$APPDIR" "${APPIMAGE_FILE}"

# Dar permisos de ejecución
chmod +x "${APPIMAGE_FILE}"

echo -e "${GREEN}✓${NC} AppImage generado: ${APPIMAGE_FILE}"

# ============================================
# Verificación final
# ============================================
echo ''
echo -e "${GREEN}=== AppImage creado exitosamente ===${NC}"
echo ''
echo -e "${BLUE}Archivo:${NC} $APPIMAGE_FILE"
echo -e "${BLUE}Tamaño:${NC}" $(du -h "$APPIMAGE_FILE" | cut -f1)
echo ''
echo -e "${YELLOW}Para ejecutar:${NC}"
echo -e "  ./$APPIMAGE_FILE"
echo ''
echo -e "${YELLOW}Para instalar en el sistema:${NC}"
echo -e "  ./$APPIMAGE_FILE --install"
echo ''

echo -e "${GREEN}=== Proceso completado ===${NC}"

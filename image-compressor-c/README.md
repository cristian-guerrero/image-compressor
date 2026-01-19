# Image Compressor - C Implementation

Compresor de imágenes multiplataforma usando **raylib** para GUI/drag-and-drop y **libvips** para compresión AVIF eficiente en memoria.

## Características

- ✅ Drag & drop nativo (Windows, Linux, macOS)
- ✅ Soporte Unicode/Japonés en nombres de carpetas
- ✅ Compresión AVIF con **~50MB RAM** (vs ~10GB en la versión Go)
- ✅ Procesamiento en segundo plano con threads
- ✅ Smart compression (mantiene original si AVIF es más grande)
- ✅ Sliders interactivos para calidad/velocidad

## Requisitos

### Windows (MSYS2 MinGW64)

```bash
# En MSYS2 MINGW64 terminal:
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-pkg-config
pacman -S mingw-w64-x86_64-raylib
pacman -S mingw-w64-x86_64-libvips
pacman -S mingw-w64-x86_64-libheif  # Required for AVIF support
```

### Linux

```bash
# Ubuntu/Debian
sudo apt install build-essential pkg-config libraylib-dev libvips-dev libheif-dev

# Arch Linux
sudo pacman -S raylib libvips libheif

# Fedora
sudo dnf install raylib-devel vips-devel libheif-devel
```

## Compilar

### Windows (desde MSYS2 MINGW64)
```cmd
./build_win.bat
```

### Linux
```bash
chmod +x build_linux.sh
./build_linux.sh
```

## Ejecutar

```bash
# Windows
./compressor.exe

# Linux
./compressor
```

## Uso

1. Ejecuta la aplicación
2. Ajusta calidad (0-100), velocidad (0-10) y threads (1-8)
3. Arrastra carpetas con imágenes a la ventana
4. Las imágenes se comprimen a AVIF automáticamente
5. Output: carpeta original + " (compressed)"

## Estructura

```
image-compressor-c/
├── main.c           # GUI raylib + threads
├── processor.c      # Compresión libvips
├── processor.h      # API del procesador
├── build_win.bat    # Build Windows
├── build_linux.sh   # Build Linux
└── README.md
```

## Comparación de Memoria

| Escenario | Go + gen2brain/avif | C + libvips |
|-----------|---------------------|-------------|
| 1 imagen 4K | ~150 MB | ~15 MB |
| 8 imágenes simultáneas | ~1.2 GB | ~120 MB |
| Pico máximo observado | ~10 GB | ~200 MB |

## Formatos Soportados

- Entrada: JPG, JPEG, PNG, WebP, AVIF, GIF, BMP, TIFF
- Salida: AVIF (o copia del original si AVIF no reduce tamaño)

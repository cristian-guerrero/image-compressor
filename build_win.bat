@echo off
REM Build script for Windows (MSYS2 MINGW64)
REM Requires: raylib and libvips installed via MSYS2 pacman
REM 
REM IMPORTANT: We compile main.c and processor.c separately to avoid
REM header conflicts between raylib and libvips (via glib -> windows.h)

echo ============================================
echo  Image Compressor - Build (Windows)
echo ============================================
echo.

REM Kill any running instances
taskkill /IM compressor_debug.exe /F >nul 2>&1
taskkill /IM compressor.exe /F >nul 2>&1

REM Check for pkg-config
where pkg-config >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: pkg-config not found.
    echo Make sure you're running from MSYS2 MINGW64 shell.
    echo.
    echo To install dependencies in MSYS2 MINGW64:
    echo   pacman -S mingw-w64-x86_64-gcc
    echo   pacman -S mingw-w64-x86_64-pkg-config
    echo   pacman -S mingw-w64-x86_64-raylib
    echo   pacman -S mingw-w64-x86_64-libvips
    exit /b 1
)

echo Step 1: Compiling processor.c (libvips)...
for /f "delims=" %%i in ('pkg-config --cflags vips') do set VIPS_CFLAGS=%%i
gcc -c src/processor.c -o processor.o %VIPS_CFLAGS% -Iinclude -O2
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile processor.c
    exit /b 1
)

echo Step 2: Compiling main.c (raylib only)...
gcc -c src/main.c -o main.o -Iinclude -O2
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile main.c
    exit /b 1
)

echo Step 3: Linking (Release Mode)...
for /f "delims=" %%i in ('pkg-config --libs vips') do set VIPS_LIBS=%%i
gcc main.o processor.o -o compressor.exe %VIPS_LIBS% -lraylib -lgdi32 -lwinmm -lopengl32 -lpthread -lpsapi -mwindows -Wl,--subsystem,windows
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to link
    exit /b 1
)

echo.
echo ============================================
echo  BUILD SUCCESSFUL!
echo ============================================
echo Run: compressor.exe
echo.

REM Cleanup object files
del main.o processor.o 2>nul

@echo off
REM Build script for Windows (Debug Mode)
REM 

echo ============================================
echo  Image Compressor - BUILD DEBUG (Windows)
echo ============================================
echo.

REM Kill any running instances
taskkill /IM compressor_debug.exe /F >nul 2>&1
taskkill /IM compressor.exe /F >nul 2>&1

REM Check for pkg-config
where pkg-config >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: pkg-config not found.
    exit /b 1
)

echo Step 1: Compiling processor.c (Debug)...
for /f "delims=" %%i in ('pkg-config --cflags vips') do set VIPS_CFLAGS=%%i
gcc -c src/processor.c -o processor_debug.o %VIPS_CFLAGS% -Iinclude -g
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile processor.c
    exit /b 1
)

echo Step 2: Compiling main.c (Debug)...
gcc -c src/main.c -o main_debug.o -Iinclude -g
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile main.c
    exit /b 1
)

echo Step 3: Linking (Debug Mode)...
for /f "delims=" %%i in ('pkg-config --libs vips') do set VIPS_LIBS=%%i
gcc main_debug.o processor_debug.o -o compressor_debug.exe %VIPS_LIBS% -lraylib -lgdi32 -lwinmm -lopengl32 -lpthread
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to link
    exit /b 1
)

echo.
echo ============================================
echo  DEBUG BUILD SUCCESSFUL!
echo ============================================
echo Run: compressor_debug.exe
echo (Terminal will remain open for logs)
echo.

REM Cleanup
del main_debug.o processor_debug.o 2>nul

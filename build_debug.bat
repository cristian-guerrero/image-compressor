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

if not exist "build" mkdir build

REM Setup Paths and dependencies
set "VIPS_CFLAGS="
set "VIPS_LIBS="
set "RAYLIB_CFLAGS="
set "RAYLIB_LIBS=-lraylib"

if exist "vendor\vips" (
    echo [INFO] Using vendored libvips...
    set "VIPS_CFLAGS=-Ivendor\vips\include -Ivendor\vips\include\glib-2.0 -Ivendor\vips\lib\glib-2.0\include"
    set "VIPS_LIBS=-Lvendor\vips\lib -lvips -lgobject-2.0 -lglib-2.0"
) else (
    where pkg-config >nul 2>&1
    if %ERRORLEVEL% NEQ 0 (
        echo [WARN] pkg-config not found and no vendor folder detected.
        echo [INFO] Attempting to download dependencies automatically...
        call fetch_deps.bat
        
        if exist "vendor\vips" (
            set "VIPS_CFLAGS=-Ivendor\vips\include -Ivendor\vips\include\glib-2.0 -Ivendor\vips\lib\glib-2.0\include"
            set "VIPS_LIBS=-Lvendor\vips\bin -lvips -lgobject-2.0 -lglib-2.0"
        ) else (
            echo ERROR: Failed to setup dependencies.
            exit /b 1
        )
    ) else (
        for /f "delims=" %%i in ('pkg-config --cflags vips') do set VIPS_CFLAGS=%%i
        for /f "delims=" %%i in ('pkg-config --libs vips') do set VIPS_LIBS=%%i
    )
)

if exist "vendor\raylib" (
    echo [INFO] Using vendored raylib...
    set "RAYLIB_CFLAGS=-Ivendor\raylib\include"
    set "RAYLIB_LIBS=-Lvendor\raylib\lib -lraylib"
)

echo Step 1: Compiling processor.c (Debug)...
gcc -c src/processor.c -o build/processor_debug.o %VIPS_CFLAGS% -Iinclude -g
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile processor.c
    exit /b 1
)

echo Step 2: Compiling main.c (Debug)...
gcc -c src/main.c -o build/main_debug.o %RAYLIB_CFLAGS% -Iinclude -g
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile main.c
    exit /b 1
)

echo Step 3: Linking (Debug Mode)...
gcc build/main_debug.o build/processor_debug.o -o build/compressor_debug.exe %VIPS_LIBS% %RAYLIB_LIBS% -lgdi32 -lwinmm -lopengl32 -lpthread -lpsapi
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to link
    exit /b 1
)

echo Step 4: Creating Portable Bundle...
if exist "vendor\vips" (
    echo [INFO] Copying libvips DLLs...
    copy "vendor\vips\bin\*.dll" "build\" >nul
    if exist "vendor\vips\bin\vips-modules-*" (
        xcopy /E /I /Y "vendor\vips\bin\vips-modules-*" "build\" >nul
    )
)
if exist "vendor\raylib" (
    if exist "vendor\raylib\lib\*.dll" (
        echo [INFO] Copying raylib DLLs...
        copy "vendor\raylib\lib\*.dll" "build\" >nul
    )
)

echo.
echo ============================================
echo  DEBUG PORTABLE BUILD SUCCESSFUL!
echo ============================================
echo Everything needed is in the 'build' folder.
echo Run: build\compressor_debug.exe
echo.

REM Cleanup
del build\main_debug.o build\processor_debug.o 2>nul

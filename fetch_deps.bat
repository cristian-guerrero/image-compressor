@echo off
setlocal enabledelayedexpansion

set VENDOR_DIR=%~dp0vendor
set RAYLIB_VERSION=5.0
set VIPS_VERSION=8.15.2

echo ============================================
echo  Setting up Vendor Dependencies (Windows)
echo ============================================

if not exist "%VENDOR_DIR%" mkdir "%VENDOR_DIR%"

REM --- RAYLIB ---
if not exist "%VENDOR_DIR%\raylib" (
    echo.
    echo [+] Downloading Raylib %RAYLIB_VERSION%...
    set "RAYLIB_URL=https://github.com/raysan5/raylib/releases/download/%RAYLIB_VERSION%/raylib-%RAYLIB_VERSION%_win64_mingw-w64.zip"
    powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '!RAYLIB_URL!' -OutFile 'raylib.zip'"
    
    echo [+] Extracting Raylib...
powershell -Command "Expand-Archive -Path 'raylib.zip' -DestinationPath '%VENDOR_DIR%' -Force"
    if exist "%VENDOR_DIR%\raylib" rd /s /q "%VENDOR_DIR%\raylib"
    for /d %%i in ("%VENDOR_DIR%\raylib-*") do (
        set "FOLDER_NAME=%%~nxi"
        if "!FOLDER_NAME!" NEQ "raylib" (
            move "%%i" "%VENDOR_DIR%\raylib"
        )
    )
    if not exist "%VENDOR_DIR%\raylib" (
        echo ERROR: Failed to extract Raylib correctly.
        exit /b 1
    )
    del raylib.zip
) else (
    echo [OK] Raylib already exists in vendor\raylib
)

REM --- LIBVIPS ---
if not exist "%VENDOR_DIR%\vips" (
    echo.
    echo [+] Downloading Libvips %VIPS_VERSION%...
    set "VIPS_URL=https://github.com/libvips/build-win64-mxe/releases/download/v%VIPS_VERSION%/vips-dev-w64-all-%VIPS_VERSION%.zip"
    powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '!VIPS_URL!' -OutFile 'vips.zip'"
    
    echo [+] Extracting Libvips...
powershell -Command "Expand-Archive -Path 'vips.zip' -DestinationPath '%VENDOR_DIR%' -Force"
    if exist "%VENDOR_DIR%\vips" rd /s /q "%VENDOR_DIR%\vips"
    for /d %%i in ("%VENDOR_DIR%\vips-dev-*") do (
        set "VIPS_FOLDER=%%~nxi"
        if "!VIPS_FOLDER!" NEQ "vips" (
            move "%%i" "%VENDOR_DIR%\vips"
        )
    )
    if not exist "%VENDOR_DIR%\vips" (
        echo ERROR: Failed to extract Libvips correctly.
        exit /b 1
    )
    del vips.zip
) else (
    echo [OK] Libvips already exists in vendor\vips
)

echo.
echo ============================================
echo  VENDOR SETUP COMPLETE
echo ============================================
echo.


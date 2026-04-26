@echo off
:: ══════════════════════════════════════════════════════════════
::  build_installer_windows.bat — Crea distribuibile Prismalux per Windows
::  Prerequisiti: MSYS2 MINGW64, Qt6 (>= 6.5), CMake, windeployqt6
::  Uso: Apri "Qt 6.x.x MINGW" dal menu Start, poi esegui questo script
:: ══════════════════════════════════════════════════════════════
setlocal enabledelayedexpansion

set BUILD_DIR=build_installer
set DEPLOY_DIR=Prismalux_Deploy
set ZIP_NAME=Prismalux_Windows_Installer.zip

echo [1/4] Configuring CMake...
cmake -B %BUILD_DIR% -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_MAKE_PROGRAM=mingw32-make
if errorlevel 1 goto :error

echo [2/4] Building...
cmake --build %BUILD_DIR% --config Release -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 goto :error

echo [3/4] Deploying Qt DLLs with windeployqt6...
mkdir %DEPLOY_DIR% 2>nul
copy %BUILD_DIR%\Prismalux_GUI.exe %DEPLOY_DIR%\
windeployqt6 --release --no-translations --no-system-d3d-compiler ^
    --no-opengl-sw %DEPLOY_DIR%\Prismalux_GUI.exe
if errorlevel 1 (
    echo WARNING: windeployqt6 non trovato, provo windeployqt...
    windeployqt --release --no-translations %DEPLOY_DIR%\Prismalux_GUI.exe
)

echo [4/4] Creando ZIP...
powershell -Command "Compress-Archive -Path '%DEPLOY_DIR%\*' -DestinationPath '%ZIP_NAME%' -Force"
echo.
echo Fatto! Distribuibile: %ZIP_NAME%
goto :end

:error
echo ERRORE durante la build. Controlla i messaggi sopra.
exit /b 1

:end
endlocal

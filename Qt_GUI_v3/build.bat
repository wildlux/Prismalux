@echo off
REM ══════════════════════════════════════════════════════════════
REM  Prismalux GUI — Build script per Windows
REM  Esegui da:  MSYS2 MINGW64 shell  (opzione A)
REM           o: cmd.exe con Qt nel PATH (opzione B)
REM ══════════════════════════════════════════════════════════════

REM ── Opzione A: MSYS2 MINGW64 ─────────────────────────────────
REM   1. Apri "MSYS2 MINGW64" dal menu Start
REM   2. pacman -S --needed mingw-w64-x86_64-qt6-base mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc
REM   3. Esegui questo script da quella shell:
REM        cd /c/Users/.../Desktop/Prismalux/Qt_GUI
REM        bash build.bat     (o usa build.sh)

REM ── Opzione B: Qt installer ufficiale ────────────────────────
REM   1. Scarica Qt da https://www.qt.io/download-qt-installer
REM   2. Installa Qt 6.x con MinGW 64-bit
REM   3. Modifica CMAKE_PREFIX_PATH sotto con il tuo percorso Qt

set QT_PATH=C:\Qt\6.9.2\mingw_64
set BUILD_DIR=build_win

cmake -B %BUILD_DIR% ^
      -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH="%QT_PATH%"

if errorlevel 1 (
    echo.
    echo [ERRORE] cmake configure fallito.
    echo Verifica che Qt6 sia installato e CMAKE_PREFIX_PATH sia corretto.
    pause
    exit /b 1
)

cmake --build %BUILD_DIR% --config Release -j4

if errorlevel 1 (
    echo.
    echo [ERRORE] Compilazione fallita.
    pause
    exit /b 1
)

echo.
echo [OK] Compilazione completata: %BUILD_DIR%\Prismalux_GUI.exe
echo.

REM ── Deploy DLL Qt ─────────────────────────────────────────────
set DEPLOY=%QT_PATH%\bin\windeployqt.exe
if exist "%DEPLOY%" (
    echo Copio le DLL Qt...
    "%DEPLOY%" %BUILD_DIR%\Prismalux_GUI.exe
    echo [OK] DLL Qt copiate.
) else (
    echo [AVVISO] windeployqt non trovato in %DEPLOY%
    echo Copia manualmente le DLL Qt nella cartella %BUILD_DIR%
)

pause

@echo off
setlocal EnableDelayedExpansion

REM ══════════════════════════════════════════════════════════════
REM  Prismalux v2.8 — Build script per Windows
REM  Lancia questo file dalla ROOT del progetto (dove si trova).
REM
REM  Prerequisiti (una tantum, dalla shell "MSYS2 UCRT64"):
REM    pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base ^
REM                       mingw-w64-ucrt-x86_64-qt6-tools ^
REM                       mingw-w64-ucrt-x86_64-cmake ^
REM                       mingw-w64-ucrt-x86_64-ninja ^
REM                       mingw-w64-ucrt-x86_64-gcc
REM
REM  In alternativa: Qt installer ufficiale da https://www.qt.io/download-qt-installer
REM ══════════════════════════════════════════════════════════════

echo.
echo +--------------------------------------------------+
echo ^|   Prismalux v2.8 - Build Windows                 ^|
echo +--------------------------------------------------+
echo.

REM ── Percorso cartella gui/ ───────────────────────────────────
set SCRIPT_DIR=%~dp0
set GUI_DIR=%SCRIPT_DIR%gui
set BUILD_DIR=%GUI_DIR%\build_win

REM ── Fallback manuale Qt (decommenta se necessario) ───────────
REM set QT_MANUAL=C:\Qt\6.9.0\mingw_64

set MSYS2_ROOT=C:\msys64
set QT_PREFIX=
set CMAKE_BIN=cmake
set NINJA_BIN=
set GEN_FLAG=
set FOUND_ENV=

REM ────────────────────────────────────────────────────────────
REM  1) MSYS2 UCRT64  (consigliato)
REM ────────────────────────────────────────────────────────────
if exist "%MSYS2_ROOT%\ucrt64\lib\cmake\Qt6\Qt6Config.cmake" (
    set QT_PREFIX=%MSYS2_ROOT%\ucrt64
    set CMAKE_BIN=%MSYS2_ROOT%\ucrt64\bin\cmake.exe
    set NINJA_BIN=%MSYS2_ROOT%\ucrt64\bin\ninja.exe
    set GEN_FLAG=-G "Ninja"
    set FOUND_ENV=MSYS2 UCRT64
    goto :env_found
)

REM ────────────────────────────────────────────────────────────
REM  2) MSYS2 MINGW64
REM ────────────────────────────────────────────────────────────
if exist "%MSYS2_ROOT%\mingw64\lib\cmake\Qt6\Qt6Config.cmake" (
    set QT_PREFIX=%MSYS2_ROOT%\mingw64
    set CMAKE_BIN=%MSYS2_ROOT%\mingw64\bin\cmake.exe
    set NINJA_BIN=%MSYS2_ROOT%\mingw64\bin\ninja.exe
    set GEN_FLAG=-G "Ninja"
    set FOUND_ENV=MSYS2 MINGW64
    goto :env_found
)

REM ────────────────────────────────────────────────────────────
REM  3) Qt installer ufficiale — cerca la versione piu' recente
REM ────────────────────────────────────────────────────────────
if exist "C:\Qt" (
    for /f "delims=" %%V in ('dir /b /ad "C:\Qt" 2^>nul ^| findstr /r "^6\." ^| sort /r') do (
        if not defined QT_PREFIX (
            for %%S in (mingw_64 msvc2022_64 msvc2019_64) do (
                if not defined QT_PREFIX (
                    if exist "C:\Qt\%%V\%%S\lib\cmake\Qt6\Qt6Config.cmake" (
                        set QT_PREFIX=C:\Qt\%%V\%%S
                        set CMAKE_BIN=cmake
                        set GEN_FLAG=-G "MinGW Makefiles"
                        set FOUND_ENV=Qt installer %%V (%%S)
                    )
                )
            )
        )
    )
    if defined QT_PREFIX goto :env_found
)

REM ────────────────────────────────────────────────────────────
REM  4) Percorso manuale
REM ────────────────────────────────────────────────────────────
if defined QT_MANUAL (
    if exist "%QT_MANUAL%\lib\cmake\Qt6\Qt6Config.cmake" (
        set QT_PREFIX=%QT_MANUAL%
        set CMAKE_BIN=cmake
        set GEN_FLAG=-G "MinGW Makefiles"
        set FOUND_ENV=manuale (%QT_MANUAL%)
        goto :env_found
    )
)

echo  [ERRORE] Qt6 non trovato in nessuna posizione standard.
echo.
echo  Soluzione A) Installa MSYS2 da https://www.msys2.org/
echo              Poi apri "MSYS2 UCRT64" e digita:
echo              pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base ^
echo                                 mingw-w64-ucrt-x86_64-qt6-tools ^
echo                                 mingw-w64-ucrt-x86_64-cmake ^
echo                                 mingw-w64-ucrt-x86_64-ninja ^
echo                                 mingw-w64-ucrt-x86_64-gcc
echo.
echo  Soluzione B) Installa Qt da https://www.qt.io/download-qt-installer
echo              e decommenta la riga QT_MANUAL in questo script.
echo.
pause
exit /b 1

:env_found
echo  [OK] Ambiente : %FOUND_ENV%
echo  [OK] Qt       : %QT_PREFIX%
echo  [OK] Sorgenti : %GUI_DIR%
echo  [OK] Output   : %BUILD_DIR%
echo.

REM ── Aggiunge bin al PATH ─────────────────────────────────────
set PATH=%QT_PREFIX%\bin;%MSYS2_ROOT%\usr\bin;%PATH%

REM ── Verifica cmake ───────────────────────────────────────────
where cmake >nul 2>&1
if errorlevel 1 (
    echo  [ERRORE] cmake non trovato nel PATH.
    pause
    exit /b 1
)
for /f "tokens=3" %%V in ('cmake --version 2^>^&1 ^| findstr /i "cmake version"') do (
    echo  [OK] cmake %%V
)

REM ── Verifica sorgenti ────────────────────────────────────────
if not exist "%GUI_DIR%\CMakeLists.txt" (
    echo  [ERRORE] %GUI_DIR%\CMakeLists.txt non trovato.
    pause
    exit /b 1
)
echo  [OK] Sorgenti verificati.
echo.

REM ── Configura cmake ──────────────────────────────────────────
echo  Configuro cmake...
cmake -B "%BUILD_DIR%" -S "%GUI_DIR%" ^
    %GEN_FLAG% ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT_PREFIX%"

if errorlevel 1 (
    echo.
    echo  [ERRORE] cmake configure fallito.
    echo  Controlla che Qt6 sia installato correttamente in: %QT_PREFIX%
    pause
    exit /b 1
)

REM ── Compila ──────────────────────────────────────────────────
echo.
echo  Compilazione in corso (2-5 minuti)...
cmake --build "%BUILD_DIR%" --config Release

if errorlevel 1 (
    echo.
    echo  [ERRORE] Compilazione fallita.
    pause
    exit /b 1
)

REM ── Deploy DLL Qt ────────────────────────────────────────────
echo.
echo  Deploy DLL Qt...
set DEPLOY=%QT_PREFIX%\bin\windeployqt.exe
if not exist "%DEPLOY%" set DEPLOY=%QT_PREFIX%\bin\windeployqt6.exe

if exist "%DEPLOY%" (
    "%DEPLOY%" "%BUILD_DIR%\Prismalux_GUI.exe" >nul 2>&1
    echo  [OK] DLL Qt copiate con windeployqt.
) else (
    echo  [INFO] windeployqt non trovato - copia manuale DLL essenziali...
    for %%D in (Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6Network.dll
                libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll
                libdouble-conversion.dll libpcre2-16-0.dll libzstd.dll zlib1.dll
                libfreetype-6.dll libpng16-16.dll libharfbuzz-0.dll
                libmd4c.dll libbz2-1.dll libglib-2.0-0.dll) do (
        if exist "%QT_PREFIX%\bin\%%D" (
            copy /y "%QT_PREFIX%\bin\%%D" "%BUILD_DIR%\" >nul 2>&1
        )
    )
    if not exist "%BUILD_DIR%\platforms" mkdir "%BUILD_DIR%\platforms"
    for %%P in (qwindows.dll qminimal.dll) do (
        for %%BASE in (share\qt6\plugins plugins) do (
            if exist "%QT_PREFIX%\%%BASE\platforms\%%P" (
                copy /y "%QT_PREFIX%\%%BASE\platforms\%%P" "%BUILD_DIR%\platforms\" >nul 2>&1
            )
        )
    )
    if not exist "%BUILD_DIR%\styles" mkdir "%BUILD_DIR%\styles"
    for %%BASE in (share\qt6\plugins plugins) do (
        if exist "%QT_PREFIX%\%%BASE\styles\qwindowsvistastyle.dll" (
            copy /y "%QT_PREFIX%\%%BASE\styles\qwindowsvistastyle.dll" "%BUILD_DIR%\styles\" >nul 2>&1
        )
    )
    echo  [OK] DLL essenziali copiate.
)

REM ── Aggiorna versione in gui/build.bat ───────────────────────
REM (nessuna azione: gui/build.bat e' mantenuto separatamente)

REM ── Risultato ────────────────────────────────────────────────
echo.
echo +--------------------------------------------------+
echo ^|   Prismalux v2.8 compilata con successo!         ^|
echo +--------------------------------------------------+
echo.
echo   Eseguibile : %BUILD_DIR%\Prismalux_GUI.exe
echo.
echo   Tip: per aggiornare tutti i modelli Ollama apri
echo        Prismalux ^> Impostazioni (engrenaggio) ^> Backend
echo        ^> "Aggiorna tutti i modelli Ollama"
echo.

pause
endlocal

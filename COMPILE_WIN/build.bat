@echo off
setlocal EnableDelayedExpansion

REM ═══════════════════════════════════════════════════════════════════
REM  COMPILE_WIN\build.bat — Compila Prismalux con toolchain locale
REM
REM  PRIMA di usare questo script: lancia setup.bat (una volta sola).
REM  Output: COMPILE_WIN\build\Prismalux_GUI.exe (con tutte le DLL Qt)
REM ═══════════════════════════════════════════════════════════════════

set ROOT=%~dp0
set TOOLS=%ROOT%toolchain
set GUI_SRC=%ROOT%..\gui
set BUILD_DIR=%ROOT%build
set QT_VER=6.8.0
set CMAKE_VER=3.31.6

echo.
echo +--------------------------------------------------+
echo ^|   Prismalux v2.8 - Build con toolchain locale    ^|
echo +--------------------------------------------------+
echo.

REM ── Verifica toolchain ───────────────────────────────────────
if not exist "%TOOLS%\ready.txt" (
    echo [ERRORE] Toolchain non configurata.
    echo         Lancia prima setup.bat e aspetta il completamento.
    pause
    exit /b 1
)

REM ── Trova Qt prefix ──────────────────────────────────────────
set QT_PREFIX=%TOOLS%\Qt6\%QT_VER%\mingw_64
if not exist "%QT_PREFIX%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo [ERRORE] Qt6 non trovato in: %QT_PREFIX%
    echo         Rilancia setup.bat per riscaricare Qt.
    pause
    exit /b 1
)

REM ── Trova GCC (Qt tools o winlibs fallback) ──────────────────
set GCC_BIN=
REM Percorso da aqt install-tool (Qt tools)
if exist "%TOOLS%\gcc\Tools\mingw1310_64\bin\gcc.exe" (
    set GCC_BIN=%TOOLS%\gcc\Tools\mingw1310_64\bin
    echo [OK] GCC: toolchain Qt (mingw1310_64)
) else if exist "%TOOLS%\gcc\mingw64\bin\gcc.exe" (
    REM Percorso winlibs fallback
    set GCC_BIN=%TOOLS%\gcc\mingw64\bin
    echo [OK] GCC: winlibs (mingw64)
) else (
    REM Prova GCC di sistema
    where gcc >nul 2>&1
    if not errorlevel 1 (
        echo [WARN] GCC locale non trovato - uso GCC di sistema.
    ) else (
        echo [ERRORE] GCC non trovato. Rilancia setup.bat.
        pause
        exit /b 1
    )
)

REM ── Trova CMake (cartella estratta ha il numero di versione) ──
set CMAKE_BIN=
for /d %%D in ("%CMAKE_DIR%\cmake-*") do (
    if not defined CMAKE_BIN (
        if exist "%%D\bin\cmake.exe" set CMAKE_BIN=%%D\bin
    )
)

REM Fallback: cmake di sistema
if not defined CMAKE_BIN (
    set CMAKE_DIR_VAR=%TOOLS%\cmake
    for /d %%D in ("%CMAKE_DIR_VAR%\cmake-*") do (
        if not defined CMAKE_BIN (
            if exist "%%D\bin\cmake.exe" set CMAKE_BIN=%%D\bin
        )
    )
)
if not defined CMAKE_BIN (
    where cmake >nul 2>&1
    if errorlevel 1 (
        echo [ERRORE] cmake non trovato. Rilancia setup.bat.
        pause
        exit /b 1
    )
    echo [WARN] cmake locale non trovato - uso cmake di sistema.
) else (
    echo [OK] CMake: %CMAKE_BIN%
)

REM ── Trova Ninja ───────────────────────────────────────────────
set NINJA_BIN=
if exist "%TOOLS%\ninja\ninja.exe" (
    set NINJA_BIN=%TOOLS%\ninja
    echo [OK] Ninja: %NINJA_BIN%
)

REM ── Configura PATH con toolchain locale ──────────────────────
set PATH=%QT_PREFIX%\bin
if defined GCC_BIN     set PATH=%GCC_BIN%;%PATH%
if defined CMAKE_BIN   set PATH=%CMAKE_BIN%;%PATH%
if defined NINJA_BIN   set PATH=%NINJA_BIN%;%PATH%
REM Aggiungi PATH originale in fondo per utilita' di sistema
set PATH=%PATH%;%SystemRoot%\system32;%SystemRoot%

REM ── Scegli generatore ─────────────────────────────────────────
set GEN_FLAG=
if defined NINJA_BIN set GEN_FLAG=-G "Ninja"
if not defined NINJA_BIN set GEN_FLAG=-G "MinGW Makefiles"

REM ── Stampa riepilogo ─────────────────────────────────────────
echo [OK] Qt prefix : %QT_PREFIX%
echo [OK] Build dir : %BUILD_DIR%
echo [OK] Generatore: %GEN_FLAG%
echo.

REM ── CMake configure ──────────────────────────────────────────
echo Configuro cmake...
cmake -B "%BUILD_DIR%" -S "%GUI_SRC%" ^
    %GEN_FLAG% ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT_PREFIX%"

if errorlevel 1 (
    echo.
    echo [ERRORE] cmake configure fallito.
    echo         Controlla che setup.bat sia stato completato senza errori.
    pause
    exit /b 1
)

REM ── CMake build ──────────────────────────────────────────────
echo.
echo Compilazione in corso (2-5 minuti)...
cmake --build "%BUILD_DIR%" --config Release

if errorlevel 1 (
    echo.
    echo [ERRORE] Compilazione fallita.
    pause
    exit /b 1
)

REM ── Verifica exe ─────────────────────────────────────────────
if not exist "%BUILD_DIR%\Prismalux_GUI.exe" (
    echo [ERRORE] Prismalux_GUI.exe non trovato dopo la build.
    pause
    exit /b 1
)

REM ── windeployqt — copia DLL Qt ───────────────────────────────
echo.
echo Deploy DLL Qt...
set DEPLOY=%QT_PREFIX%\bin\windeployqt6.exe
if not exist "%DEPLOY%" set DEPLOY=%QT_PREFIX%\bin\windeployqt.exe

if exist "%DEPLOY%" (
    "%DEPLOY%" "%BUILD_DIR%\Prismalux_GUI.exe" >nul 2>&1
    echo [OK] DLL Qt copiate con windeployqt.
) else (
    echo [WARN] windeployqt non trovato - copia DLL essenziali manualmente...
    for %%D in (Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6Network.dll Qt6PrintSupport.dll
                libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll
                libdouble-conversion.dll libpcre2-16-0.dll libzstd.dll zlib1.dll
                libfreetype-6.dll libpng16-16.dll libharfbuzz-0.dll
                libmd4c.dll libbz2-1.dll libglib-2.0-0.dll) do (
        if exist "%QT_PREFIX%\bin\%%D" (
            copy /y "%QT_PREFIX%\bin\%%D" "%BUILD_DIR%\" >nul 2>&1
        )
    )
    if not exist "%BUILD_DIR%\platforms" mkdir "%BUILD_DIR%\platforms"
    for %%BASE in (share\qt6\plugins plugins) do (
        if exist "%QT_PREFIX%\%%BASE\platforms\qwindows.dll" (
            copy /y "%QT_PREFIX%\%%BASE\platforms\qwindows.dll" "%BUILD_DIR%\platforms\" >nul 2>&1
        )
    )
    if not exist "%BUILD_DIR%\styles" mkdir "%BUILD_DIR%\styles"
    for %%BASE in (share\qt6\plugins plugins) do (
        if exist "%QT_PREFIX%\%%BASE\styles\qwindowsvistastyle.dll" (
            copy /y "%QT_PREFIX%\%%BASE\styles\qwindowsvistastyle.dll" "%BUILD_DIR%\styles\" >nul 2>&1
        )
    )
    echo [OK] DLL essenziali copiate.
)

REM ── Risultato ────────────────────────────────────────────────
echo.
echo +--------------------------------------------------+
echo ^|   Prismalux v2.8 compilata con successo!         ^|
echo +--------------------------------------------------+
echo.
echo   Eseguibile: %BUILD_DIR%\Prismalux_GUI.exe
echo.
echo   Puoi copiare tutta la cartella build\ su qualsiasi
echo   PC Windows senza installare nulla (DLL gia' incluse).
echo.
pause
endlocal

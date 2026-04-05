@echo off
setlocal EnableDelayedExpansion

REM ══════════════════════════════════════════════════════════════
REM  Prismalux GUI v2.2 — Build script per Windows
REM  Aggiornato: 2026-03-28
REM
REM  Strategia di rilevamento Qt (ordine di priorita'):
REM    1. MSYS2 UCRT64  (C:\msys64\ucrt64)   — consigliato
REM    2. MSYS2 MINGW64 (C:\msys64\mingw64)
REM    3. Qt installer ufficiale (C:\Qt\6.x\mingw_64)
REM    4. Variabile QT_PATH impostata manualmente qui sotto
REM
REM  Installazione rapida MSYS2 (una tantum):
REM    1. Scarica MSYS2 da https://www.msys2.org/
REM    2. Apri "MSYS2 UCRT64" dal menu Start e digita:
REM       pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base ^
REM                          mingw-w64-ucrt-x86_64-qt6-tools ^
REM                          mingw-w64-ucrt-x86_64-cmake ^
REM                          mingw-w64-ucrt-x86_64-ninja ^
REM                          mingw-w64-ucrt-x86_64-gcc
REM    3. Lancia questo script (doppio clic o cmd.exe)
REM ══════════════════════════════════════════════════════════════

echo.
echo +--------------------------------------------------+
echo ^|   Prismalux GUI v2.2 - Build Windows             ^|
echo +--------------------------------------------------+
echo.

REM ── Fallback manuale: decommenta e imposta il tuo percorso ─
REM set QT_MANUAL=C:\Qt\6.9.0\mingw_64

set BUILD_DIR=build_win
set MSYS2_ROOT=C:\msys64
set QT_PREFIX=
set CMAKE_BIN=
set NINJA_BIN=
set GEN_FLAG=
set FOUND_ENV=

REM ────────────────────────────────────────────────────────────
REM  1) MSYS2 UCRT64
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
REM     in C:\Qt\6.x.y\mingw_64
REM ────────────────────────────────────────────────────────────
if exist "C:\Qt" (
    for /f "delims=" %%V in ('dir /b /ad "C:\Qt" 2^>nul ^| findstr /r "^6\." ^| sort /r') do (
        if not defined QT_PREFIX (
            REM %%V e' il numero di versione (es. 6.9.2)
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
REM  4) Variabile manuale impostata sopra
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
echo  Soluzioni:
echo    A) Installa MSYS2 da https://www.msys2.org/ e poi da "MSYS2 UCRT64":
echo       pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base ^
echo                          mingw-w64-ucrt-x86_64-cmake ^
echo                          mingw-w64-ucrt-x86_64-ninja ^
echo                          mingw-w64-ucrt-x86_64-gcc
echo.
echo    B) Installa Qt da https://www.qt.io/download-qt-installer
echo       e decommenta la riga QT_MANUAL in questo script.
echo.
pause
exit /b 1

:env_found
echo  [OK] Ambiente trovato : %FOUND_ENV%
echo  [OK] Qt prefix        : %QT_PREFIX%

REM ── Aggiunge bin al PATH ─────────────────────────────────────
set PATH=%QT_PREFIX%\bin;%MSYS2_ROOT%\usr\bin;%PATH%

REM ── Verifica cmake ──────────────────────────────────────────
if not defined CMAKE_BIN set CMAKE_BIN=cmake
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
if not exist "CMakeLists.txt" (
    echo  [ERRORE] CMakeLists.txt non trovato.
    echo  Esegui questo script dalla cartella Qt_GUI.
    pause
    exit /b 1
)
if not exist "pages\grafico_page.cpp" (
    echo  [ERRORE] pages\grafico_page.cpp non trovato.
    pause
    exit /b 1
)
echo  [OK] Sorgenti verificati.

REM ── Configura cmake ──────────────────────────────────────────
echo.
echo  Configuro cmake...
cmake -B %BUILD_DIR% -S . ^
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
echo  Compilazione in corso (puo' richiedere 2-5 minuti)...
cmake --build %BUILD_DIR% --config Release

if errorlevel 1 (
    echo.
    echo  [ERRORE] Compilazione fallita.
    pause
    exit /b 1
)

REM ── Deploy DLL Qt ─────────────────────────────────────────────
echo.
echo  Deploy DLL Qt...
set DEPLOY=%QT_PREFIX%\bin\windeployqt.exe
if not exist "%DEPLOY%" set DEPLOY=%QT_PREFIX%\bin\windeployqt6.exe

if exist "%DEPLOY%" (
    "%DEPLOY%" "%BUILD_DIR%\Prismalux_GUI.exe" >nul 2>&1
    echo  [OK] DLL Qt copiate con windeployqt.
) else (
    REM Copia manuale delle DLL essenziali
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
    REM Plugin piattaforme
    if not exist "%BUILD_DIR%\platforms" mkdir "%BUILD_DIR%\platforms"
    for %%P in (qwindows.dll qminimal.dll) do (
        for %%BASE in (share\qt6\plugins plugins) do (
            if exist "%QT_PREFIX%\%%BASE\platforms\%%P" (
                copy /y "%QT_PREFIX%\%%BASE\platforms\%%P" "%BUILD_DIR%\platforms\" >nul 2>&1
            )
        )
    )
    REM Plugin stili
    if not exist "%BUILD_DIR%\styles" mkdir "%BUILD_DIR%\styles"
    for %%BASE in (share\qt6\plugins plugins) do (
        if exist "%QT_PREFIX%\%%BASE\styles\qwindowsvistastyle.dll" (
            copy /y "%QT_PREFIX%\%%BASE\styles\qwindowsvistastyle.dll" "%BUILD_DIR%\styles\" >nul 2>&1
        )
    )
    echo  [OK] DLL essenziali copiate.
)

REM ── Risultato ─────────────────────────────────────────────────
echo.
echo +--------------------------------------------------+
echo ^|   Prismalux GUI v2.2 compilata con successo!     ^|
echo +--------------------------------------------------+
echo.
echo   Eseguibile : %BUILD_DIR%\Prismalux_GUI.exe
echo.
echo   Novita' v2.2 nel Grafico:
echo     [7] Smith - Numeri Primi (reali + gaussiani)
echo     [8] Smith - pi * e * Primi (serie convergenza)
echo.

pause
endlocal

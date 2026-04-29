@echo off
setlocal EnableDelayedExpansion

REM ══════════════════════════════════════════════════════════════
REM  Prismalux v2.8 — Build script per Windows
REM  Lancia questo file dalla ROOT del progetto (doppio clic).
REM
REM  Rilevamento toolchain (ordine di priorita'):
REM    0. COMPILE_WIN\toolchain\  (portatile — lancia prima setup.bat)
REM    1. MSYS2 UCRT64            (C:\msys64\ucrt64)
REM    2. MSYS2 MINGW64           (C:\msys64\mingw64)
REM    3. Qt installer ufficiale  (C:\Qt\6.x\mingw_64)
REM    4. QT_MANUAL               (percorso personalizzato sotto)
REM ══════════════════════════════════════════════════════════════

echo.
echo +--------------------------------------------------+
echo ^|   Prismalux v2.8 - Build Windows                 ^|
echo +--------------------------------------------------+
echo.

set SCRIPT_DIR=%~dp0
set GUI_DIR=%SCRIPT_DIR%gui
set TOOLS=%SCRIPT_DIR%COMPILE_WIN\toolchain

REM ── Fallback manuale Qt (decommenta se necessario) ───────────
REM set QT_MANUAL=C:\Qt\6.9.0\mingw_64

set MSYS2_ROOT=C:\msys64
set QT_PREFIX=
set CMAKE_BIN=cmake
set GEN_FLAG=
set FOUND_ENV=
set BUILD_DIR=%GUI_DIR%\build_win

REM ════════════════════════════════════════════════════════════
REM  0) Toolchain portatile COMPILE_WIN\toolchain\
REM     (scaricata da COMPILE_WIN\setup.bat)
REM ════════════════════════════════════════════════════════════
if exist "%TOOLS%\ready.txt" (
    REM Trova Qt prefix (es. toolchain\Qt6\6.8.0\mingw_64)
    for /d %%V in ("%TOOLS%\Qt6\*") do (
        if not defined QT_PREFIX (
            if exist "%%V\mingw_64\lib\cmake\Qt6\Qt6Config.cmake" (
                set QT_PREFIX=%%V\mingw_64
            )
        )
    )
    if defined QT_PREFIX (
        REM Trova cmake nella toolchain portatile
        for /d %%D in ("%TOOLS%\cmake\cmake-*") do (
            if not defined CMAKE_BIN_FOUND (
                if exist "%%D\bin\cmake.exe" (
                    set CMAKE_BIN=%%D\bin\cmake.exe
                    set CMAKE_BIN_FOUND=1
                )
            )
        )
        REM Trova ninja
        if exist "%TOOLS%\ninja\ninja.exe" (
            set GEN_FLAG=-G "Ninja"
            set NINJA_PATH=%TOOLS%\ninja
        )
        REM Trova GCC (da aqt tools o winlibs)
        set GCC_PATH=
        if exist "%TOOLS%\gcc\Tools\mingw1310_64\bin\gcc.exe" (
            set GCC_PATH=%TOOLS%\gcc\Tools\mingw1310_64\bin
        ) else if exist "%TOOLS%\gcc\mingw64\bin\gcc.exe" (
            set GCC_PATH=%TOOLS%\gcc\mingw64\bin
        )
        REM Aggiunge toolchain al PATH
        set PATH=%QT_PREFIX%\bin
        if defined GCC_PATH    set PATH=!GCC_PATH!;!PATH!
        if defined NINJA_PATH  set PATH=!NINJA_PATH!;!PATH!
        for %%D in ("%CMAKE_BIN%") do set PATH=%%~dpD;!PATH!
        set PATH=!PATH!;%SystemRoot%\system32;%SystemRoot%
        REM Usa build dir separata per non mescolare con build di sistema
        set BUILD_DIR=%SCRIPT_DIR%COMPILE_WIN\build
        set FOUND_ENV=Portatile COMPILE_WIN\toolchain
        goto :env_found
    )
)

REM ════════════════════════════════════════════════════════════
REM  1) MSYS2 UCRT64
REM ════════════════════════════════════════════════════════════
if exist "%MSYS2_ROOT%\ucrt64\lib\cmake\Qt6\Qt6Config.cmake" (
    set QT_PREFIX=%MSYS2_ROOT%\ucrt64
    set CMAKE_BIN=%MSYS2_ROOT%\ucrt64\bin\cmake.exe
    set GEN_FLAG=-G "Ninja"
    set FOUND_ENV=MSYS2 UCRT64
    set PATH=%QT_PREFIX%\bin;%MSYS2_ROOT%\usr\bin;%PATH%
    goto :env_found
)

REM ════════════════════════════════════════════════════════════
REM  2) MSYS2 MINGW64
REM ════════════════════════════════════════════════════════════
if exist "%MSYS2_ROOT%\mingw64\lib\cmake\Qt6\Qt6Config.cmake" (
    set QT_PREFIX=%MSYS2_ROOT%\mingw64
    set CMAKE_BIN=%MSYS2_ROOT%\mingw64\bin\cmake.exe
    set GEN_FLAG=-G "Ninja"
    set FOUND_ENV=MSYS2 MINGW64
    set PATH=%QT_PREFIX%\bin;%MSYS2_ROOT%\usr\bin;%PATH%
    goto :env_found
)

REM ════════════════════════════════════════════════════════════
REM  3) Qt installer ufficiale — cerca versione piu' recente
REM ════════════════════════════════════════════════════════════
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
    if defined QT_PREFIX (
        set PATH=%QT_PREFIX%\bin;%PATH%
        goto :env_found
    )
)

REM ════════════════════════════════════════════════════════════
REM  4) Percorso manuale
REM ════════════════════════════════════════════════════════════
if defined QT_MANUAL (
    if exist "%QT_MANUAL%\lib\cmake\Qt6\Qt6Config.cmake" (
        set QT_PREFIX=%QT_MANUAL%
        set CMAKE_BIN=cmake
        set GEN_FLAG=-G "MinGW Makefiles"
        set FOUND_ENV=Manuale (%QT_MANUAL%)
        set PATH=%QT_PREFIX%\bin;%PATH%
        goto :env_found
    )
)

REM ════════════════════════════════════════════════════════════
REM  Nessun ambiente trovato
REM ════════════════════════════════════════════════════════════
echo  [ERRORE] Qt6 non trovato in nessuna posizione.
echo.
echo  Soluzione rapida (zero install):
echo    1. Apri la cartella COMPILE_WIN\
echo    2. Doppio clic su setup.bat  (scarica ~600 MB, una volta sola)
echo    3. Doppio clic su questo build.bat
echo.
echo  Soluzione con MSYS2:
echo    Installa MSYS2 da https://www.msys2.org/ poi da "MSYS2 UCRT64":
echo    pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base ^
echo                       mingw-w64-ucrt-x86_64-cmake ^
echo                       mingw-w64-ucrt-x86_64-ninja ^
echo                       mingw-w64-ucrt-x86_64-gcc
echo.
echo  Soluzione con Qt installer:
echo    https://www.qt.io/download-qt-installer
echo    e decommenta la riga QT_MANUAL in questo script.
echo.
pause
exit /b 1

:env_found
echo  [OK] Ambiente : %FOUND_ENV%
echo  [OK] Qt       : %QT_PREFIX%
echo  [OK] Sorgenti : %GUI_DIR%
echo  [OK] Output   : %BUILD_DIR%
echo.

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

REM ── CMake configure ──────────────────────────────────────────
echo  Configuro cmake...
cmake -B "%BUILD_DIR%" -S "%GUI_DIR%" ^
    %GEN_FLAG% ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT_PREFIX%"

if errorlevel 1 (
    echo.
    echo  [ERRORE] cmake configure fallito.
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
set DEPLOY=%QT_PREFIX%\bin\windeployqt6.exe
if not exist "%DEPLOY%" set DEPLOY=%QT_PREFIX%\bin\windeployqt.exe

if exist "%DEPLOY%" (
    "%DEPLOY%" "%BUILD_DIR%\Prismalux_GUI.exe" >nul 2>&1
    echo  [OK] DLL Qt copiate con windeployqt.
) else (
    echo  [INFO] windeployqt non trovato - copia manuale DLL essenziali...
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

REM ── Risultato ────────────────────────────────────────────────
echo.
echo +--------------------------------------------------+
echo ^|   Prismalux v2.8 compilata con successo!         ^|
echo +--------------------------------------------------+
echo.
echo   Eseguibile : %BUILD_DIR%\Prismalux_GUI.exe
echo.
echo   Tip: per aggiornare i modelli Ollama apri
echo        Impostazioni (engrenaggio) ^> Backend
echo        ^> "Aggiorna tutti i modelli Ollama"
echo.
pause
endlocal

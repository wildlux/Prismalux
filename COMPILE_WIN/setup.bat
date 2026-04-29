@echo off
setlocal EnableDelayedExpansion

REM ═══════════════════════════════════════════════════════════════════
REM  COMPILE_WIN\setup.bat — Scarica toolchain portatile per Windows
REM
REM  Esegui UNA VOLTA (doppio clic). Scarica ~600 MB:
REM    - Python 3.12  (embeddable, per aqtinstall)
REM    - Qt 6.8.0     (Core + Widgets + Network + PrintSupport)
REM    - GCC 13.1.0   (MinGW-w64 UCRT, matching Qt 6.8)
REM    - CMake 3.31   (portatile, da cmake.org)
REM    - Ninja 1.12   (build system, singolo .exe)
REM
REM  Dopo setup: lancia build.bat per compilare Prismalux.
REM  La cartella toolchain\ NON va in git (gia' in .gitignore).
REM ═══════════════════════════════════════════════════════════════════

set TOOLS=%~dp0toolchain
set PY_DIR=%TOOLS%\python
set QT_DIR=%TOOLS%\Qt6
set GCC_DIR=%TOOLS%\gcc
set CMAKE_DIR=%TOOLS%\cmake
set NINJA_DIR=%TOOLS%\ninja

set QT_VER=6.8.0
set PY_VER=3.12.7
set CMAKE_VER=3.31.6
set NINJA_VER=1.12.1

echo.
echo +----------------------------------------------------------+
echo ^|   Prismalux — Setup toolchain portatile Windows          ^|
echo ^|   Scarica: Python + Qt6 + GCC + CMake + Ninja (~600 MB) ^|
echo +----------------------------------------------------------+
echo.

REM ── Gia' configurata? ─────────────────────────────────────────
if exist "%TOOLS%\ready.txt" (
    echo [OK] Toolchain gia' configurata. Avvia build.bat per compilare.
    echo.
    type "%TOOLS%\ready.txt"
    echo.
    pause
    exit /b 0
)

REM ── Verifica PowerShell ───────────────────────────────────────
where powershell >nul 2>&1
if errorlevel 1 (
    echo [ERRORE] PowerShell non trovato. Richiesto Windows 10 o superiore.
    pause
    exit /b 1
)

REM ── Verifica connessione internet ─────────────────────────────
echo Verifico connessione internet...
powershell -NoProfile -Command "try { Invoke-WebRequest 'https://www.python.org' -UseBasicParsing -TimeoutSec 5 | Out-Null; exit 0 } catch { exit 1 }" >nul 2>&1
if errorlevel 1 (
    echo [ERRORE] Nessuna connessione internet. Riprova quando sei online.
    pause
    exit /b 1
)
echo [OK] Connessione internet attiva.
echo.

REM ── Crea cartelle ────────────────────────────────────────────
mkdir "%TOOLS%"     2>nul
mkdir "%PY_DIR%"    2>nul
mkdir "%QT_DIR%"    2>nul
mkdir "%GCC_DIR%"   2>nul
mkdir "%CMAKE_DIR%" 2>nul
mkdir "%NINJA_DIR%" 2>nul

REM ════════════════════════════════════════════════════════════
REM  STEP 1/5 — Python embeddable
REM ════════════════════════════════════════════════════════════
echo [1/5] Scarico Python %PY_VER% embeddable (~10 MB)...
set PY_URL=https://www.python.org/ftp/python/%PY_VER%/python-%PY_VER%-embed-amd64.zip
set PY_ZIP=%TOOLS%\python_embed.zip

powershell -NoProfile -Command "Invoke-WebRequest '%PY_URL%' -OutFile '%PY_ZIP%' -UseBasicParsing"
if errorlevel 1 (
    echo [ERRORE] Download Python fallito. Controlla la connessione.
    pause
    exit /b 1
)
powershell -NoProfile -Command "Expand-Archive '%PY_ZIP%' -DestinationPath '%PY_DIR%' -Force"
del "%PY_ZIP%" >nul 2>&1

REM Abilita site-packages (richiesto per pip/aqtinstall)
powershell -NoProfile -Command ^
    "Get-ChildItem '%PY_DIR%\*._pth' | ForEach-Object { (Get-Content $_.FullName) -replace '#import site','import site' | Set-Content $_.FullName }"

REM Installa pip
powershell -NoProfile -Command "Invoke-WebRequest 'https://bootstrap.pypa.io/get-pip.py' -OutFile '%PY_DIR%\get-pip.py' -UseBasicParsing"
"%PY_DIR%\python.exe" "%PY_DIR%\get-pip.py" --quiet
del "%PY_DIR%\get-pip.py" >nul 2>&1

REM Installa aqtinstall
"%PY_DIR%\python.exe" -m pip install aqtinstall --quiet
if errorlevel 1 (
    echo [ERRORE] Installazione aqtinstall fallita.
    pause
    exit /b 1
)
echo [OK] Python %PY_VER% + pip + aqtinstall pronti.
echo.

REM ════════════════════════════════════════════════════════════
REM  STEP 2/5 — Qt 6.8.0 (Core, Widgets, Network, PrintSupport)
REM ════════════════════════════════════════════════════════════
echo [2/5] Scarico Qt %QT_VER% win64_mingw (~350 MB, attendere)...
"%PY_DIR%\python.exe" -m aqt install-qt Windows desktop %QT_VER% win64_mingw ^
    --outputdir "%QT_DIR%"
if errorlevel 1 (
    echo [ERRORE] Download Qt6 fallito.
    echo         Riprova o installa Qt manualmente da https://www.qt.io/download-qt-installer
    pause
    exit /b 1
)
echo [OK] Qt %QT_VER% pronto.
echo.

REM ════════════════════════════════════════════════════════════
REM  STEP 3/5 — GCC 13.1.0 (MinGW-w64 UCRT, matching Qt 6.8)
REM ════════════════════════════════════════════════════════════
echo [3/5] Scarico GCC 13.1.0 MinGW-w64 UCRT (~150 MB)...
"%PY_DIR%\python.exe" -m aqt install-tool Windows desktop tools_mingw ^
    qt.tools.win64_mingw1310 --outputdir "%GCC_DIR%"
if errorlevel 1 (
    echo [WARN] Download GCC da Qt tools fallito - provo winlibs come alternativa...
    set WINLIBS_URL=https://github.com/brechtsanders/winlibs_mingw/releases/download/13.1.0posix-16.0.5-11.0.1-ucrt-r3/winlibs-x86_64-posix-seh-gcc-13.1.0-mingw-w64ucrt-11.0.1-r3.zip
    set WINLIBS_ZIP=%TOOLS%\winlibs.zip
    powershell -NoProfile -Command "Invoke-WebRequest '!WINLIBS_URL!' -OutFile '!WINLIBS_ZIP!' -UseBasicParsing"
    if not errorlevel 1 (
        powershell -NoProfile -Command "Expand-Archive '!WINLIBS_ZIP!' -DestinationPath '%GCC_DIR%' -Force"
        del "!WINLIBS_ZIP!" >nul 2>&1
        echo [OK] GCC da winlibs pronto.
    ) else (
        echo [WARN] GCC non scaricato. build.bat usera' il GCC di sistema se disponibile.
    )
) else (
    echo [OK] GCC 13.1.0 pronto.
)
echo.

REM ════════════════════════════════════════════════════════════
REM  STEP 4/5 — CMake 3.31 (portatile da cmake.org)
REM ════════════════════════════════════════════════════════════
echo [4/5] Scarico CMake %CMAKE_VER% (~50 MB)...
set CMAKE_URL=https://github.com/Kitware/CMake/releases/download/v%CMAKE_VER%/cmake-%CMAKE_VER%-windows-x86_64.zip
set CMAKE_ZIP=%TOOLS%\cmake.zip
powershell -NoProfile -Command "Invoke-WebRequest '%CMAKE_URL%' -OutFile '%CMAKE_ZIP%' -UseBasicParsing"
if errorlevel 1 (
    echo [ERRORE] Download CMake fallito.
    pause
    exit /b 1
)
powershell -NoProfile -Command "Expand-Archive '%CMAKE_ZIP%' -DestinationPath '%CMAKE_DIR%' -Force"
del "%CMAKE_ZIP%" >nul 2>&1
echo [OK] CMake %CMAKE_VER% pronto.
echo.

REM ════════════════════════════════════════════════════════════
REM  STEP 5/5 — Ninja (singolo .exe, ~350 KB)
REM ════════════════════════════════════════════════════════════
echo [5/5] Scarico Ninja %NINJA_VER%...
set NINJA_URL=https://github.com/ninja-build/ninja/releases/download/v%NINJA_VER%/ninja-win.zip
set NINJA_ZIP=%TOOLS%\ninja.zip
powershell -NoProfile -Command "Invoke-WebRequest '%NINJA_URL%' -OutFile '%NINJA_ZIP%' -UseBasicParsing"
if not errorlevel 1 (
    powershell -NoProfile -Command "Expand-Archive '%NINJA_ZIP%' -DestinationPath '%NINJA_DIR%' -Force"
    del "%NINJA_ZIP%" >nul 2>&1
    echo [OK] Ninja %NINJA_VER% pronto.
) else (
    echo [WARN] Ninja non scaricato - cmake usera' Make come fallback.
)
echo.

REM ── Scrivi marker di completamento ───────────────────────────
(
    echo Toolchain Prismalux v2.8 — Windows portatile
    echo Qt:    %QT_VER%  ^(win64_mingw^)
    echo GCC:   13.1.0   ^(MinGW-w64 UCRT^)
    echo CMake: %CMAKE_VER%
    echo Ninja: %NINJA_VER%
    echo Setup: %DATE% %TIME%
) > "%TOOLS%\ready.txt"

echo +----------------------------------------------------------+
echo ^|   Toolchain pronta!                                      ^|
echo ^|   Ora lancia build.bat per compilare Prismalux.          ^|
echo +----------------------------------------------------------+
echo.
pause
endlocal

@echo off
chcp 65001 > nul
setlocal EnableDelayedExpansion

REM ══════════════════════════════════════════════════════════════
REM  Prismalux v2.9 — Launcher build Windows
REM  Doppio clic qui per compilare.
REM
REM  Strategia:
REM    1. Cerca Python (sistema o MSYS2) → lancia build.py
REM    2. Se Python non trovato ma MSYS2 presente → bash aggiorna.sh
REM    3. Altrimenti: istruzioni installazione
REM ══════════════════════════════════════════════════════════════

set SCRIPT_DIR=%~dp0
set PYTHON=
set BASH=

REM ── 1) Cerca Python nel PATH di sistema ──────────────────────
for %%P in (python3.exe python.exe py.exe) do (
    if not defined PYTHON (
        where %%P >nul 2>&1 && set PYTHON=%%P
    )
)

REM ── 2) Cerca Python in MSYS2 (tutti i prefissi comuni) ───────
if not defined PYTHON (
    for %%D in (
        "C:\msys64\ucrt64\bin\python3.exe"
        "C:\msys64\mingw64\bin\python3.exe"
        "C:\msys64\usr\bin\python3.exe"
        "C:\msys64\ucrt64\bin\python.exe"
    ) do (
        if not defined PYTHON (
            if exist %%D set PYTHON=%%D
        )
    )
)

REM ── 3) Se Python trovato → lancia build.py ───────────────────
if defined PYTHON (
    echo.
    echo  Python: !PYTHON!
    echo.
    "!PYTHON!" "%SCRIPT_DIR%build.py" %*
    set RC=!ERRORLEVEL!
    echo.
    if !RC! == 0 (
        echo  Build completata con successo.
    ) else (
        echo  Build fallita. Controlla gli errori sopra.
    )
    echo.
    pause
    exit /b !RC!
)

REM ── 4) Python non trovato → prova con MSYS2 bash ─────────────
for %%B in (
    "C:\msys64\usr\bin\bash.exe"
    "C:\msys64\ucrt64\bin\bash.exe"
) do (
    if not defined BASH (
        if exist %%B set BASH=%%B
    )
)

if defined BASH (
    echo.
    echo  Python non trovato — uso MSYS2 bash + aggiorna.sh
    echo.

    REM Converti percorso Windows → Unix
    set _DIR=%SCRIPT_DIR%
    set _DIR=!_DIR:\=/!
    set _DRV=!_DIR:~0,1!
    set _REST=!_DIR:~2!
    if "!_REST:~-1!" == "/" set _REST=!_REST:~0,-1!
    set _UNIX=/!_DRV!!_REST!

    start "Prismalux Build" "!BASH!" --login -i -c ^
        "cd '!_UNIX!' && bash aggiorna.sh --gui; echo ''; read -rp 'Premi INVIO per chiudere...' _"
    exit /b
)

REM ── 5) Niente Python né MSYS2 → istruzioni ───────────────────
echo.
echo  [ERRORE] Ne Python ne MSYS2 trovati.
echo.
echo  Soluzioni (scegli una):
echo.
echo  A) Installa MSYS2 da https://www.msys2.org/
echo     poi apri "MSYS2 UCRT64" e digita:
echo       pacman -S mingw-w64-ucrt-x86_64-python
echo       pacman -S mingw-w64-ucrt-x86_64-qt6-base
echo       pacman -S mingw-w64-ucrt-x86_64-gcc
echo       pacman -S mingw-w64-ucrt-x86_64-cmake
echo       pacman -S mingw-w64-ucrt-x86_64-ninja
echo.
echo  B) Installa Python da https://www.python.org/downloads/
echo     (spunta "Add Python to PATH" durante l'installazione)
echo.
pause
exit /b 1

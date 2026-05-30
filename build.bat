@echo off
chcp 65001 > nul
setlocal EnableDelayedExpansion

REM ══════════════════════════════════════════════════════════════
REM  Prismalux v2.9 — Launcher build Windows
REM  Doppio clic qui per compilare. Tutta la logica e' in build.py
REM ══════════════════════════════════════════════════════════════

set SCRIPT_DIR=%~dp0
set PYTHON=

REM ── Cerca Python nel PATH di sistema ─────────────────────────
for %%P in (python3.exe python.exe py.exe) do (
    if not defined PYTHON (
        where %%P >nul 2>&1 && set PYTHON=%%P
    )
)

REM ── Fallback: Python di MSYS2 UCRT64 ─────────────────────────
if not defined PYTHON (
    if exist "C:\msys64\ucrt64\bin\python3.exe" (
        set PYTHON=C:\msys64\ucrt64\bin\python3.exe
    )
)

REM ── Python non trovato ────────────────────────────────────────
if not defined PYTHON (
    echo.
    echo  [ERRORE] Python non trovato.
    echo.
    echo  Installa Python:
    echo    - Da https://www.python.org/downloads/  (spunta "Add to PATH")
    echo    - Oppure con MSYS2:  pacman -S mingw-w64-ucrt-x86_64-python
    echo.
    pause
    exit /b 1
)

REM ── Lancia build.py ──────────────────────────────────────────
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

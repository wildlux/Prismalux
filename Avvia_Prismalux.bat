@echo off
setlocal EnableDelayedExpansion

REM ══════════════════════════════════════════════════════════════
REM  Prismalux v2.8 — Launcher Windows
REM  Doppio clic qui per avviare il programma.
REM  Se non e' ancora compilato, esegui prima build.bat.
REM ══════════════════════════════════════════════════════════════

set SCRIPT_DIR=%~dp0

REM ── Cerca l'eseguibile nelle posizioni note ───────────────────
set EXE=

REM Toolchain portatile (COMPILE_WIN\setup.bat)
if exist "%SCRIPT_DIR%COMPILE_WIN\build\Prismalux_GUI.exe" (
    set EXE=%SCRIPT_DIR%COMPILE_WIN\build\Prismalux_GUI.exe
    goto :found
)

REM MSYS2 / Qt installer (gui\build_win\)
if exist "%SCRIPT_DIR%gui\build_win\Prismalux_GUI.exe" (
    set EXE=%SCRIPT_DIR%gui\build_win\Prismalux_GUI.exe
    goto :found
)

REM Altre posizioni possibili (build manuale)
for %%P in (
    "%SCRIPT_DIR%gui\build\Prismalux_GUI.exe"
    "%SCRIPT_DIR%build\Prismalux_GUI.exe"
    "%SCRIPT_DIR%build_win\Prismalux_GUI.exe"
) do (
    if exist %%P (
        set EXE=%%P
        goto :found
    )
)

REM ── Eseguibile non trovato ────────────────────────────────────
echo.
echo  [ERRORE] Prismalux_GUI.exe non trovato.
echo.
echo  Il programma non e' ancora compilato.
echo  Esegui prima build.bat (doppio clic).
echo.
echo  Se hai gia' eseguito build.bat e l'errore persiste,
echo  controlla che la compilazione sia andata a buon fine
echo  (nessun messaggio [ERRORE] nell'output di build.bat).
echo.
pause
exit /b 1

:found
REM ── Avvia il programma ───────────────────────────────────────
echo.
echo  Avvio Prismalux...
echo  Percorso: !EXE!
echo.

REM Imposta la working directory nella cartella dell'exe
REM (necessario per trovare themes/, platforms/, ecc.)
for %%D in ("!EXE!") do set EXE_DIR=%%~dpD

pushd "!EXE_DIR!"
start "" "!EXE!"
popd

endlocal

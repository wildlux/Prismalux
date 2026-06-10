@echo off
setlocal EnableDelayedExpansion

REM ══════════════════════════════════════════════════════════════
REM  Prismalux v2.9 — Launcher Windows
REM  Doppio clic qui per avviare il programma.
REM  Se non e' ancora compilato, esegui prima build.bat.
REM ══════════════════════════════════════════════════════════════

set SCRIPT_DIR=%~dp0..\..\

REM ── Cerca l'eseguibile nelle posizioni note ───────────────────
set EXE=

REM Toolchain portatile (Tools\compile_win\setup.bat)
if exist "%SCRIPT_DIR%Tools\compile_win\build\Prismalux_GUI.exe" (
    set EXE=%SCRIPT_DIR%Tools\compile_win\build\Prismalux_GUI.exe
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
REM ── DLL runtime MinGW (libwinpthread, libgcc, libstdc++) ──────
REM windeployqt non le copia: le cerchiamo in MSYS2 o nella cartella dell'exe.
REM Se MSYS2 e' installato (necessario per compilare), le DLL sono li'.
REM ── Copia DLL runtime MinGW nella cartella dell'exe (più affidabile del PATH) ─
for %%D in ("!EXE!") do set EXE_DIR2=%%~dpD
set DLL_NEEDED=0
if not exist "!EXE_DIR2!libwinpthread-1.dll" set DLL_NEEDED=1
if not exist "!EXE_DIR2!libgcc_s_seh-1.dll"  set DLL_NEEDED=1
if not exist "!EXE_DIR2!libstdc++-6.dll"      set DLL_NEEDED=1

if "!DLL_NEEDED!"=="1" (
    set DLL_SRC=
    REM 1. Cartella DLL/ del progetto
    if exist "%SCRIPT_DIR%DLL\libwinpthread-1.dll" set "DLL_SRC=%SCRIPT_DIR%DLL"
    REM 2. MSYS2 su C: o D:
    if "!DLL_SRC!"=="" (
        for %%R in (
            "C:\msys64\ucrt64\bin"
            "C:\msys2\ucrt64\bin"
            "D:\msys64\ucrt64\bin"
            "D:\msys2\ucrt64\bin"
        ) do (
            if "!DLL_SRC!"=="" (
                if exist "%%~R\libwinpthread-1.dll" set "DLL_SRC=%%~R"
            )
        )
    )
    if not "!DLL_SRC!"=="" (
        copy /Y "!DLL_SRC!\libwinpthread-1.dll" "!EXE_DIR2!" >nul 2>&1
        copy /Y "!DLL_SRC!\libgcc_s_seh-1.dll"  "!EXE_DIR2!" >nul 2>&1
        copy /Y "!DLL_SRC!\libstdc++-6.dll"      "!EXE_DIR2!" >nul 2>&1
        echo  [OK] DLL runtime copiate da: !DLL_SRC!
    ) else (
        echo  [WARN] DLL runtime non trovate. Se l'app non parte copia manualmente
        echo         libwinpthread-1.dll libgcc_s_seh-1.dll libstdc++-6.dll
        echo         da C:\msys64\ucrt64\bin\ nella cartella dell'exe.
    )
)

:launch
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

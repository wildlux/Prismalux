@echo off
chcp 65001 > nul
setlocal EnableDelayedExpansion

REM ══════════════════════════════════════════════════════════════
REM  Prismalux — Build Windows
REM  Doppio clic per compilare. Tutta la logica e' in build.py.
REM
REM  Ordine di ricerca Python:
REM    1. COMPILE_WIN\toolchain\python\  (embedded, installato da setup.bat)
REM    2. PATH di sistema                (python3, python, py)
REM    3. MSYS2                          (C:\msys64\ucrt64\bin\  ecc.)
REM ══════════════════════════════════════════════════════════════

set SCRIPT_DIR=%~dp0
set PYTHON=

echo.
echo  +--------------------------------------------------+
echo  ^|   Prismalux — Ricerca Python                     ^|
echo  +--------------------------------------------------+
echo.
echo  Cerco Python nei seguenti percorsi:
echo.

REM ── 1) Python embedded (COMPILE_WIN\toolchain\python) ────────
echo  [COMPILE_WIN\toolchain\python\]
set EMBED_PY=%SCRIPT_DIR%COMPILE_WIN\toolchain\python\python.exe
if exist "%EMBED_PY%" (
    echo    [OK] python.exe trovato in: %EMBED_PY%
    set PYTHON=%EMBED_PY%
) else (
    echo    [--] %EMBED_PY%
    echo         ^(non presente — esegui COMPILE_WIN\setup.bat per scaricarlo^)
)
echo.

REM ── 2) PATH di sistema ───────────────────────────────────────
if not defined PYTHON (
    echo  [Sistema / PATH]
    for %%P in (python3.exe python.exe py.exe) do (
        if not defined PYTHON (
            where %%P >nul 2>&1
            if !ERRORLEVEL! == 0 (
                for /f "delims=" %%F in ('where %%P 2^>nul') do (
                    if not defined PYTHON (
                        echo    [OK] %%P trovato in: %%F
                        set PYTHON=%%P
                    )
                )
            ) else (
                echo    [--] %%P  non trovato nel PATH
            )
        )
    )
    echo.
)

REM ── 3) MSYS2 ─────────────────────────────────────────────────
if not defined PYTHON (
    echo  [MSYS2 — C:\msys64\]
    set MSYS2_PATHS[0]=C:\msys64\ucrt64\bin\python3.exe
    set MSYS2_PATHS[1]=C:\msys64\mingw64\bin\python3.exe
    set MSYS2_PATHS[2]=C:\msys64\usr\bin\python3.exe
    set MSYS2_PATHS[3]=C:\msys64\ucrt64\bin\python.exe
    for /L %%I in (0,1,3) do (
        if not defined PYTHON (
            if exist "!MSYS2_PATHS[%%I]!" (
                echo    [OK] Python trovato in: !MSYS2_PATHS[%%I]!
                set PYTHON=!MSYS2_PATHS[%%I]!
            ) else (
                echo    [--] !MSYS2_PATHS[%%I]!
            )
        ) else (
            echo    [--] !MSYS2_PATHS[%%I]!
        )
    )
    echo.
)

REM ── Riepilogo ─────────────────────────────────────────────────
if not defined PYTHON goto :not_found

echo  +--------------------------------------------------+
echo  ^|   Python trovato — avvio build.py                ^|
echo  +--------------------------------------------------+
echo.
echo  Interprete : !PYTHON!
echo  Script     : %SCRIPT_DIR%build.py
echo.

"!PYTHON!" "%SCRIPT_DIR%build.py" %*
set RC=!ERRORLEVEL!

echo.
echo  ════════════════════════════════════════════════════
if !RC! == 0 (
    echo.
    echo   ##################################################
    echo   ##                                              ##
    echo   ##     BUILD COMPLETATA CON SUCCESSO            ##
    echo   ##     Doppio clic su Avvia_Prismalux.bat       ##
    echo   ##                                              ##
    echo   ##################################################
    echo.
) else (
    echo.
    echo   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    echo   XX                                              XX
    echo   XX        BUILD FALLITA  ^(codice !RC!^)           XX
    echo   XX   Controlla errore.txt per i dettagli        XX
    echo   XX                                              XX
    echo   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    echo.
    if exist "%SCRIPT_DIR%errore.txt" (
        echo  ── Ultime righe di errore.txt ──────────────────────
        powershell -Command "Get-Content '%SCRIPT_DIR%errore.txt' -Tail 20" 2>nul
        echo.
    )
)
echo  ════════════════════════════════════════════════════
echo.
echo  Premi un tasto per chiudere...
pause > nul
exit /b !RC!

:not_found
echo  +--------------------------------------------------+
echo  ^|   [ERRORE] Python non trovato                    ^|
echo  +--------------------------------------------------+
echo.
echo  Python e' necessario per compilare Prismalux su Windows.
echo  Scegli uno dei metodi seguenti:
echo.
echo  ── Metodo A: toolchain portatile (consigliato) ──────
echo     Doppio clic su COMPILE_WIN\setup.bat
echo     Scarica Python + Qt + GCC + CMake + Ninja (~600 MB)
echo     Python sara' in: %SCRIPT_DIR%COMPILE_WIN\toolchain\python\python.exe
echo.
echo  ── Metodo B: Python ufficiale ───────────────────────
echo     1. Scarica da: https://www.python.org/downloads/
echo     2. Durante l'installazione spunta:
echo           "Add Python to PATH"
echo     3. Riavvia questo file dopo l'installazione.
echo.
echo  ── Metodo C: MSYS2 (se gia' installato) ─────────────
echo     Apri "MSYS2 UCRT64" e digita:
echo       pacman -S mingw-w64-ucrt-x86_64-python
echo     Python sara' in: C:\msys64\ucrt64\bin\python3.exe
echo.
echo  ── Metodo D: Microsoft Store ────────────────────────
echo     Cerca "Python 3" nel Microsoft Store e installalo.
echo.
pause
exit /b 1

@echo off
setlocal EnableDelayedExpansion

REM ══════════════════════════════════════════════════════════════
REM  aggiorna.bat — Aggiorna Prismalux su Windows
REM  Lancia questo file dalla ROOT del progetto (doppio clic).
REM
REM  Rilevamento bash (ordine di priorità):
REM    1. MSYS2 UCRT64    (C:\msys64\ucrt64)   — migliore per Qt
REM    2. MSYS2 MINGW64   (C:\msys64\mingw64)
REM    3. MSYS2 usr/bin   (C:\msys64\usr\bin)
REM    4. Git Bash        (C:\Program Files\Git)
REM    5. Cygwin 64-bit   (C:\cygwin64)
REM    6. Cygwin 32-bit   (C:\cygwin)
REM    7. Cygwin registro (percorso personalizzato)
REM    8. Fallback nativo (build.bat + ZIP Python)
REM
REM  Opzioni (passate a aggiorna.sh se in bash, altrimenti ignorate):
REM    --gui          Solo GUI
REM    --zip          Solo ZIP
REM    --no-zip       Salta ZIP
REM    --build-whisper  Compila whisper.cpp
REM    --llama-studio   Compila llama-server
REM ══════════════════════════════════════════════════════════════

set SCRIPT_DIR=%~dp0
set ARGS=%*
set BASH_TYPE=MSYS2

echo.
echo +--------------------------------------------------+
echo ^|   Prismalux — Aggiorna (Windows)                 ^|
echo +--------------------------------------------------+
echo.

REM ── 1) MSYS2 UCRT64 ─────────────────────────────────────────
set MSYS2_ROOT=C:\msys64
if exist "%MSYS2_ROOT%\ucrt64\bin\bash.exe" (
    set BASH=%MSYS2_ROOT%\ucrt64\bin\bash.exe
    set BASH_LABEL=MSYS2_UCRT64
    set BASH_TYPE=MSYS2
    goto :bash_found
)

REM ── 2) MSYS2 MINGW64 ────────────────────────────────────────
if exist "%MSYS2_ROOT%\mingw64\bin\bash.exe" (
    set BASH=%MSYS2_ROOT%\mingw64\bin\bash.exe
    set BASH_LABEL=MSYS2_MINGW64
    set BASH_TYPE=MSYS2
    goto :bash_found
)

REM ── 3) MSYS2 usr/bin/bash ───────────────────────────────────
if exist "%MSYS2_ROOT%\usr\bin\bash.exe" (
    set BASH=%MSYS2_ROOT%\usr\bin\bash.exe
    set BASH_LABEL=MSYS2_USR
    set BASH_TYPE=MSYS2
    goto :bash_found
)

REM ── 4) Git Bash ─────────────────────────────────────────────
for %%G in (
    "%ProgramFiles%\Git\bin\bash.exe"
    "%ProgramFiles(x86)%\Git\bin\bash.exe"
    "%LOCALAPPDATA%\Programs\Git\bin\bash.exe"
) do (
    if exist %%G (
        set BASH=%%~G
        set BASH_LABEL=Git Bash
        set BASH_TYPE=MSYS2
        goto :bash_found
    )
)

REM ── 5) Cygwin 64-bit (percorso standard) ────────────────────
if exist "C:\cygwin64\bin\bash.exe" (
    set BASH=C:\cygwin64\bin\bash.exe
    set BASH_LABEL=Cygwin64
    set BASH_TYPE=CYGWIN
    goto :bash_found
)

REM ── 6) Cygwin 32-bit (percorso standard) ────────────────────
if exist "C:\cygwin\bin\bash.exe" (
    set BASH=C:\cygwin\bin\bash.exe
    set BASH_LABEL=Cygwin32
    set BASH_TYPE=CYGWIN
    goto :bash_found
)

REM ── 7) Cygwin in percorso personalizzato (registro) ─────────
for /f "tokens=2*" %%K in (
    'reg query "HKLM\SOFTWARE\Cygwin\setup" /v rootdir 2^>nul'
) do (
    if exist "%%L\bin\bash.exe" (
        set BASH=%%L\bin\bash.exe
        set BASH_LABEL=Cygwin (registro)
        set BASH_TYPE=CYGWIN
        goto :bash_found
    )
)
for /f "tokens=2*" %%K in (
    'reg query "HKCU\SOFTWARE\Cygwin\setup" /v rootdir 2^>nul'
) do (
    if exist "%%L\bin\bash.exe" (
        set BASH=%%L\bin\bash.exe
        set BASH_LABEL=Cygwin (registro utente)
        set BASH_TYPE=CYGWIN
        goto :bash_found
    )
)

REM ── 8) Nessun bash — build nativa ───────────────────────────
echo  [INFO] bash non trovato (MSYS2 / Git Bash / Cygwin).
echo  Eseguo build nativa: build.bat + ZIP Python.
echo.
goto :native_build

:bash_found
echo  [OK] bash trovato : !BASH! (!BASH_LABEL!)
echo.

REM ── Converti path Windows → Unix ────────────────────────────
REM  MSYS2 / Git Bash : C:\path → /c/path
REM  Cygwin           : C:\path → /cygdrive/c/path
set SCRIPT_UNIX=%SCRIPT_DIR%
set SCRIPT_UNIX=!SCRIPT_UNIX:\=/!
set _DRIVE=!SCRIPT_UNIX:~0,1!
set _REST=!SCRIPT_UNIX:~2!
if "!BASH_TYPE!"=="CYGWIN" (
    set SCRIPT_UNIX=/cygdrive/!_DRIVE!!_REST!
) else (
    set SCRIPT_UNIX=/!_DRIVE!!_REST!
)
REM Rimuovi eventuale slash finale
if "!SCRIPT_UNIX:~-1!" == "/" set SCRIPT_UNIX=!SCRIPT_UNIX:~0,-1!

echo  [INFO] Path Unix  : !SCRIPT_UNIX!
echo  [INFO] Tipo shell : !BASH_TYPE!
echo.

"!BASH!" --login -c "cd '!SCRIPT_UNIX!' && bash aggiorna.sh %ARGS%"
if errorlevel 1 (
    echo.
    echo  [ERRORE] aggiorna.sh ha restituito un errore.
    pause
    exit /b 1
)
goto :done

:native_build
REM ──────────────────────────────────────────────────────────────
REM  Build nativa: chiama build.bat poi crea ZIP con Python
REM ──────────────────────────────────────────────────────────────
set DO_ZIP=1
for %%A in (%ARGS%) do (
    if "%%A"=="--zip"    set "DO_ZIP=1" & goto :skip_gui
    if "%%A"=="--gui"    set "DO_ZIP=0"
    if "%%A"=="--no-zip" set "DO_ZIP=0"
)

REM Compila GUI (a meno che --zip sia l'unico argomento)
set ONLY_ZIP=0
if "%ARGS%"=="--zip" set ONLY_ZIP=1
if "!ONLY_ZIP!"=="0" (
    echo  [PASSO 1/2] Compilazione GUI...
    call "%SCRIPT_DIR%build.bat"
    if errorlevel 1 (
        echo.
        echo  [ERRORE] build.bat ha restituito un errore.
        pause
        exit /b 1
    )
)

:skip_gui
REM Crea ZIP Windows (richiede Python)
if "!DO_ZIP!"=="1" (
    echo.
    echo  [PASSO 2/2] Creo ZIP Windows...
    set ZIP_SCRIPT=%SCRIPT_DIR%scripts\crea_zip_windows.py

    REM Versione da CMakeLists.txt
    set PRISMA_VERSION=2.8
    for /f "tokens=4 delims= " %%V in ('findstr /i "project(Prismalux_GUI VERSION" "%SCRIPT_DIR%gui\CMakeLists.txt" 2^>nul') do (
        set _RAW=%%V
        set PRISMA_VERSION=!_RAW:VERSION=!
        set PRISMA_VERSION=!PRISMA_VERSION:)=!
    )

    set ZIP_OUT=%SCRIPT_DIR%Prismalux_v!PRISMA_VERSION!_Windows.zip

    set PYTHON=
    for %%P in (python3.exe python.exe py.exe) do (
        if not defined PYTHON (
            where %%P >nul 2>&1 && set PYTHON=%%P
        )
    )

    if not defined PYTHON (
        echo  [WARN] Python non trovato — ZIP non creato.
        echo  Installa Python da https://www.python.org/downloads/
    ) else if not exist "!ZIP_SCRIPT!" (
        echo  [WARN] scripts\crea_zip_windows.py non trovato — ZIP non creato.
    ) else (
        "!PYTHON!" "!ZIP_SCRIPT!" --out "!ZIP_OUT!"
        if errorlevel 1 (
            echo  [WARN] ZIP non generato correttamente.
        ) else (
            echo  [OK] ZIP creato: !ZIP_OUT!
        )
    )
)

:done
echo.
echo +--------------------------------------------------+
echo ^|   Aggiornamento completato.                      ^|
echo +--------------------------------------------------+
echo.
echo   Per avviare Prismalux: doppio clic su Avvia_Prismalux.bat
echo.
pause
endlocal

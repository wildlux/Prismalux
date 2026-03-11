@echo off
setlocal EnableDelayedExpansion
chcp 65001 >nul 2>&1

echo.
echo ╔══════════════════════════════════════════════╗
echo ║   Prismalux — Compilazione Windows (.exe)   ║
echo ╚══════════════════════════════════════════════╝
echo.

:: ── Cerca gcc (MinGW / MSYS2 / WinLibs) ───────────────────────────────
set GCC=
set GCC_SOURCE=

:: 1) gcc già nel PATH?
where gcc >nul 2>&1
if %errorlevel%==0 (
    set GCC=gcc
    set GCC_SOURCE=PATH
    goto :gcc_found
)

:: 2) MSYS2 MINGW64 in C:\msys64
if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set GCC=C:\msys64\mingw64\bin\gcc.exe
    set GCC_SOURCE=MSYS2 ^(C:\msys64^)
    goto :gcc_found
)

:: 3) MSYS2 UCRT64
if exist "C:\msys64\ucrt64\bin\gcc.exe" (
    set GCC=C:\msys64\ucrt64\bin\gcc.exe
    set GCC_SOURCE=MSYS2 UCRT64
    goto :gcc_found
)

:: 4) WinLibs / w64devkit in posizioni comuni
for %%P in (
    "C:\w64devkit\bin\gcc.exe"
    "C:\mingw64\bin\gcc.exe"
    "C:\mingw32\bin\gcc.exe"
    "%USERPROFILE%\scoop\apps\mingw\current\bin\gcc.exe"
    "C:\ProgramData\chocolatey\bin\gcc.exe"
) do (
    if exist %%P (
        set GCC=%%~P
        set GCC_SOURCE=%%~P
        goto :gcc_found
    )
)

:: Nessun gcc trovato
echo  [ERRORE] gcc non trovato.
echo.
echo  Installa MinGW in uno di questi modi:
echo.
echo  1) MSYS2 (consigliato):
echo     https://www.msys2.org/
echo     Poi in MSYS2 MINGW64 shell:
echo     pacman -S --needed mingw-w64-x86_64-gcc
echo.
echo  2) WinLibs (standalone, no installazione):
echo     https://winlibs.com/
echo     Estrai e aggiungi la cartella bin al PATH
echo.
echo  3) Scoop:
echo     scoop install mingw
echo.
pause
exit /b 1

:gcc_found
echo  [OK] gcc trovato: %GCC_SOURCE%

:: ── Versione gcc ───────────────────────────────────────────────────────
for /f "tokens=*" %%V in ('"%GCC%" --version 2^>^&1 ^| findstr /n "." ^| findstr "^1:"') do (
    set GCC_VER=%%V
    set GCC_VER=!GCC_VER:~3!
)
echo  [OK] Versione: %GCC_VER%
echo.

:: ── Controlla file sorgente ────────────────────────────────────────────
if not exist "prismalux.c" (
    echo  [ERRORE] prismalux.c non trovato.
    echo  Esegui questo script dalla cartella C_software.
    pause
    exit /b 1
)
if not exist "hw_detect.c" (
    echo  [ERRORE] hw_detect.c non trovato.
    pause
    exit /b 1
)
echo  [OK] Sorgenti trovati: prismalux.c, hw_detect.c

:: ── Crea cartella models se non esiste ────────────────────────────────
if not exist "models" (
    mkdir models
    echo  [OK] Cartella models\ creata
)

:: ── Opzioni di compilazione ────────────────────────────────────────────
set CFLAGS=-O2 -Wall -DWIN32_LEAN_AND_MEAN
set LIBS=-lws2_32
set TARGET=prismalux.exe
set SOURCES=prismalux.c hw_detect.c

:: ── Rilevamento GPU per suggerimento ──────────────────────────────────
echo  Rilevamento GPU...
set HAS_NVIDIA=0
set HAS_AMD=0

where nvidia-smi >nul 2>&1
if %errorlevel%==0 (
    set HAS_NVIDIA=1
    echo  [GPU] NVIDIA rilevata (nvidia-smi disponibile)
)

where rocm-smi >nul 2>&1
if %errorlevel%==0 (
    set HAS_AMD=1
    echo  [GPU] AMD ROCm rilevato (rocm-smi disponibile)
)

if %HAS_NVIDIA%==0 if %HAS_AMD%==0 (
    echo  [GPU] Nessuna GPU accelerata rilevata — modalita CPU (Ollama/llama-server via HTTP)
)
echo.

:: ── Avvia compilazione ────────────────────────────────────────────────
echo  Compilazione in corso...
echo  Comando: %GCC% %CFLAGS% -o %TARGET% %SOURCES% %LIBS%
echo.

"%GCC%" %CFLAGS% -o %TARGET% %SOURCES% %LIBS%

if %errorlevel% neq 0 (
    echo.
    echo  [ERRORE] Compilazione fallita (codice: %errorlevel%)
    echo.
    echo  Possibili cause:
    echo   - Errori di sintassi nel codice sorgente
    echo   - Versione gcc troppo vecchia (serve gcc >= 8)
    echo   - Mancano header Windows (prova con MSYS2 MINGW64)
    pause
    exit /b 1
)

:: ── Successo ──────────────────────────────────────────────────────────
echo.
echo ╔══════════════════════════════════════════════╗
echo ║         Compilazione completata!            ║
echo ╚══════════════════════════════════════════════╝
echo.

:: Dimensione file
for %%F in (%TARGET%) do (
    set /a SIZE_KB=%%~zF/1024
    echo  File:       %TARGET% (!SIZE_KB! KB)
)
echo  Backend:    Ollama + llama-server (HTTP)
echo  GPU:        hw_detect rileva NVIDIA/AMD automaticamente
echo.
echo  ── Come usare ──────────────────────────────
echo.
echo  Modalita Ollama (avvia prima: ollama serve):
echo    prismalux.exe --backend ollama
echo.
echo  Modalita llama-server (avvia prima: llama-server.exe):
echo    prismalux.exe --backend llama-server --port 8080
echo.
echo  Con modello specifico:
echo    prismalux.exe --backend ollama --model deepseek-coder:33b
echo.
echo  Su macchina remota:
echo    prismalux.exe --backend ollama --host 192.168.1.10 --port 11434
echo.
echo  Aiuto completo:
echo    prismalux.exe --help
echo.

:: ── Offre di avviare subito ────────────────────────────────────────────
set /p AVVIA=  Avviare prismalux.exe adesso? [S/n]:
if /i "!AVVIA!"=="n" goto :fine
if /i "!AVVIA!"=="no" goto :fine

echo.
echo  Avvio in modalita Ollama (assicurati che ollama serve sia attivo)...
echo.
prismalux.exe --backend ollama

:fine
echo.
pause
endlocal

@echo off
REM ══════════════════════════════════════════════════════════════
REM  scarica_modelli_whisper.bat
REM  Scarica un modello GGML per whisper-cli.exe
REM
REM  Uso: esegui questo file DOPO aver compilato il progetto.
REM  I modelli vengono salvati in:
REM    %USERPROFILE%\.prismalux\whisper\models\
REM
REM  Modelli disponibili (scegli quello più adatto alla tua RAM):
REM    tiny   (~39 MB)   — velocissimo, meno preciso
REM    base   (~74 MB)   — buon compromesso
REM    small  (~141 MB)  — consigliato
REM    medium (~462 MB)  — alta precisione
REM ══════════════════════════════════════════════════════════════
setlocal enabledelayedexpansion

set "MODELS_DIR=%USERPROFILE%\.prismalux\whisper\models"
set "BASE_URL=https://huggingface.co/ggerganov/whisper.cpp/resolve/main"

echo.
echo  Prismalux — Scarica modello Whisper per riconoscimento vocale
echo  ══════════════════════════════════════════════════════════════
echo.
echo  Cartella destinazione: %MODELS_DIR%
echo.
echo  Scegli il modello da scaricare:
echo    1) ggml-tiny.bin     ~39  MB  (velocissimo)
echo    2) ggml-base.bin     ~74  MB  (bilanciato)
echo    3) ggml-small.bin   ~141  MB  (CONSIGLIATO)
echo    4) ggml-medium.bin  ~462  MB  (alta precisione)
echo    0) Esci
echo.
set /p SCELTA="Scelta [3]: "
if "!SCELTA!"=="" set SCELTA=3

if "!SCELTA!"=="0" goto :EOF

if "!SCELTA!"=="1" set "MODEL=ggml-tiny.bin"    & set "SZ=39 MB"
if "!SCELTA!"=="2" set "MODEL=ggml-base.bin"    & set "SZ=74 MB"
if "!SCELTA!"=="3" set "MODEL=ggml-small.bin"   & set "SZ=141 MB"
if "!SCELTA!"=="4" set "MODEL=ggml-medium.bin"  & set "SZ=462 MB"

if not defined MODEL (
    echo  Scelta non valida.
    pause
    goto :EOF
)

echo.
echo  Download: %MODEL% (%SZ%)...
echo.

mkdir "%MODELS_DIR%" 2>nul

where curl >nul 2>&1
if %errorlevel% equ 0 (
    curl -L --progress-bar -o "%MODELS_DIR%\%MODEL%" "%BASE_URL%/%MODEL%"
) else (
    where powershell >nul 2>&1
    if %errorlevel% equ 0 (
        powershell -Command "Invoke-WebRequest -Uri '%BASE_URL%/%MODEL%' -OutFile '%MODELS_DIR%\%MODEL%' -UseBasicParsing"
    ) else (
        echo  ERRORE: curl e powershell non trovati. Installa curl o aggiorna Windows.
        pause
        goto :EOF
    )
)

if exist "%MODELS_DIR%\%MODEL%" (
    echo.
    echo  ✓ Modello salvato in: %MODELS_DIR%\%MODEL%
    echo.
    echo  Ora puoi usare il pulsante Voce in Prismalux.
) else (
    echo  ERRORE: download fallito. Verifica la connessione internet.
)

echo.
pause

@echo off
setlocal EnableDelayedExpansion

REM ══════════════════════════════════════════════════════════════
REM  aggiorna.bat — Aggiorna Prismalux su Windows  (v2.9)
REM  Lancia questo file dalla ROOT del progetto (doppio clic).
REM
REM  Novità v2.9 (riepilogo sessioni):
REM    - Scheda TFR: C.F. calcolato auto (D.M. 1976), lookup 150 comuni/paesi
REM    - WAN Calcolo Distribuito: TCP:11600, 28 task, cron, dispatcher universale
REM    - Ollama MCP (18°): cache SQLite, 5 tool, solo stdlib Python
REM    - Quiz CCNA Android: 64 -> 209 domande (15 temi, CCNA 200-301)
REM    - BT Android: toggle modalita' chiaro/cifrato (default: chiaro)
REM    - DPI HiDPI/Wayland: dpiScale() in tutte le pages
REM    - i18n: QTranslator + stub .ts in gui/i18n/
REM    - LaTeX KaTeX: rendering formule Analisi 1/2 (QWebEngineView)
REM      widget LatexView, pannello output AI, prompt AI aggiornati
REM    - Randomizer formule (tasto shuffle): 52 formule categorizzate
REM      in Risolvi Passi (eq., deriv., integr., limiti, simpl., diseq.)
REM    - Test SymPy CAT-E: 15 test complessi - 62/62 PASS
REM    - Donazione PayPal: README, FUNDING.yml, app desktop, APK Android
REM    - TODO: tutte le voci completate
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
REM  Sezioni disponibili (passate a aggiorna.sh se bash trovato):
REM    --gui          Solo GUI Qt6
REM    --web          GUI Qt6 + aggiorna interfaccia web embedded (LAN server)
REM    --bat          Solo ZIP Windows con .bat launcher  [DEFAULT su Windows]
REM    --zip          Alias per --bat
REM    --appimage     AppImage Linux — NON DISPONIBILE su Windows
REM
REM  Combinazioni comuni:
REM    (nessun arg)   GUI + ZIP .bat    (default Windows)
REM    --gui --web    GUI + web preview
REM    --bat          Solo ZIP, salta compilazione
REM    --no-zip       GUI senza ZIP
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

REM ── Avviso: --appimage non disponibile su Windows ───────────
set _HAS_APPIMAGE=0
for %%A in (%ARGS%) do (
    if /i "%%A"=="--appimage" set _HAS_APPIMAGE=1
)
if "!_HAS_APPIMAGE!"=="1" (
    echo  [WARN] --appimage non e' disponibile su Windows.
    echo         AppImage e' solo per Linux. Opzione ignorata.
    echo.
    set ARGS=!ARGS:--appimage=!
)
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
REM  Sezioni supportate: --gui, --web, --bat/--zip, --no-zip/--no-bat
REM  Nota: --appimage non disponibile su Windows (ignorato sopra)
REM ──────────────────────────────────────────────────────────────
set DO_ZIP=1
set DO_WEB=0
for %%A in (%ARGS%) do (
    if "%%A"=="--zip"    set "DO_ZIP=1" & goto :skip_gui
    if "%%A"=="--bat"    set "DO_ZIP=1" & goto :skip_gui
    if "%%A"=="--gui"    set "DO_ZIP=0"
    if "%%A"=="--web"    set "DO_WEB=1"
    if "%%A"=="--no-zip" set "DO_ZIP=0"
    if "%%A"=="--no-bat" set "DO_ZIP=0"
)

REM Compila GUI se non si sta facendo solo --zip/--bat
set ONLY_ZIP=0
if "%ARGS%"=="--zip" set ONLY_ZIP=1
if "%ARGS%"=="--bat" set ONLY_ZIP=1
if "!ONLY_ZIP!"=="0" (
    echo  [PASSO 1/2] Compilazione GUI...
    call "%SCRIPT_DIR%build.bat"
    if errorlevel 1 (
        echo.
        echo  [ERRORE] build.bat ha restituito un errore.
        pause
        exit /b 1
    )
    if "!DO_WEB!"=="1" (
        echo.
        echo  [WEB] Interfaccia web embedded in gui/lan_server.cpp
        echo        Avvia Prismalux e apri: http://localhost:11500/web
        echo.
    )
)

:skip_gui
REM Crea ZIP Windows con launcher .bat (richiede Python)
if "!DO_ZIP!"=="1" (
    echo.
    echo  [PASSO 2/2] Creo ZIP Windows con launcher .bat...
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

REM ══════════════════════════════════════════════════════════════
REM  Promemoria aggiornamento Cygwin (ogni 30 giorni, facoltativo)
REM ══════════════════════════════════════════════════════════════
set CYG_SETUP=%SCRIPT_DIR%COMPILE_WIN\setup-x86_64.exe
set CYG_STAMP=%SCRIPT_DIR%COMPILE_WIN\.cygwin_last_update

if not exist "!CYG_SETUP!" goto :done

REM Leggi data ultimo aggiornamento (formato YYYYMMDD)
set LAST_UPD=0
if exist "!CYG_STAMP!" (
    set /p LAST_UPD=<"!CYG_STAMP!"
)

REM Data odierna come YYYYMMDD
for /f "tokens=1-3 delims=/-. " %%A in ('date /t') do (
    REM date /t → gg/mm/aaaa o varianti locali; usiamo wmic per sicurezza
)
for /f %%D in ('wmic os get LocalDateTime /value 2^>nul ^| findstr "LocalDateTime"') do (
    set _DT=%%D
    set _DT=!_DT:LocalDateTime=!
    set _DT=!_DT:~4!
    set TODAY=!_DT:~0,8!
)
if not defined TODAY set TODAY=99999999

REM Calcola giorni trascorsi (approssimazione: confronto numerico YYYYMMDD)
set DAYS_OLD=0
if "!LAST_UPD!" NEQ "0" (
    REM Differenza approssimata: se TODAY > LAST_UPD+30 (come stringa YYYYMMDD)
    set /a _Y1=!LAST_UPD:~0,4!
    set /a _M1=!LAST_UPD:~4,2!
    set /a _D1=!LAST_UPD:~6,2!
    set /a _Y2=!TODAY:~0,4!
    set /a _M2=!TODAY:~4,2!
    set /a _D2=!TODAY:~6,2!
    set /a DAYS_OLD=(!_Y2!-!_Y1!)*365 + (!_M2!-!_M1!)*30 + (!_D2!-!_D1!)
)

REM Proponi aggiornamento se mai fatto o passati ≥30 giorni
if "!LAST_UPD!"=="0" (
    set ASK_CYG=1
) else if !DAYS_OLD! GEQ 30 (
    set ASK_CYG=1
) else (
    set ASK_CYG=0
)

if "!ASK_CYG!"=="1" (
    echo.
    echo +--------------------------------------------------+
    if "!LAST_UPD!"=="0" (
        echo ^|   Cygwin: primo avvio — aggiornamento consigliato ^|
    ) else (
        echo ^|   Cygwin: ultimo aggiornamento !DAYS_OLD! giorni fa     ^|
    )
    echo +--------------------------------------------------+
    echo.
    echo   Vuoi aggiornare Cygwin ora? (facoltativo)
    echo   [S] Si, apri il setup   [N] No, salta   [M] Non chiedere per 90 giorni
    echo.
    set /p CYG_CHOICE="  Scelta [S/N/M]: "
    if /i "!CYG_CHOICE!"=="S" (
        echo.
        echo  Avvio Cygwin setup...
        start "" "!CYG_SETUP!"
        echo !TODAY!>"!CYG_STAMP!"
    ) else if /i "!CYG_CHOICE!"=="M" (
        REM Aggiunge 90 giorni: sposta LAST_UPD in avanti manualmente
        set /a _MX=!TODAY:~4,2!+3
        if !_MX! GTR 12 (
            set /a _MX-=12
            set /a _YX=!TODAY:~0,4!+1
        ) else (
            set _YX=!TODAY:~0,4!
        )
        if !_MX! LSS 10 set _MX=0!_MX!
        echo !_YX!!_MX!!TODAY:~6,2!>"!CYG_STAMP!"
        echo  [OK] Promemoria posticipato di 90 giorni.
    ) else (
        echo  [OK] Aggiornamento saltato.
    )
)

:done
echo.
echo +--------------------------------------------------+
echo ^|   Aggiornamento completato.                      ^|
echo +--------------------------------------------------+
echo.
echo   Sezioni eseguite su questa piattaforma (Windows):
echo     GUI Qt6  : si (build.bat)
echo     ZIP .bat : si (crea_zip_windows.py)
echo     AppImage : N/A — solo Linux
echo.
if "!DO_WEB!"=="1" (
    echo   Web app   : http://localhost:11500/web  [avvia Prismalux prima]
    echo.
)
echo   Per avviare Prismalux: doppio clic su Avvia_Prismalux.bat
echo.
pause
endlocal

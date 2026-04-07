@echo off
setlocal EnableDelayedExpansion

echo.
echo +----------------------------------------------+
echo ^|         Prismalux — Avvio Windows            ^|
echo +----------------------------------------------+
echo.

set BASE=%~dp0
set BUILD=%BASE%Qt_GUI\build_win
set VENV=%BUILD%\py_env
set VENV_PYTHON=%VENV%\Scripts\python.exe
set REQS=%BASE%requirements_python.txt

:: ── Trova Python di sistema (serve solo per creare il venv) ──────────────────
set PYTHON=
where python  >nul 2>&1 && set PYTHON=python
if not defined PYTHON (
    where py      >nul 2>&1 && set PYTHON=py
)
if not defined PYTHON (
    where python3 >nul 2>&1 && set PYTHON=python3
)

if not defined PYTHON (
    echo  [AVVISO] Python non trovato nel PATH.
    echo  La pagina Matematica e gli script AI richiedono Python 3.x.
    echo  Scarica da: https://www.python.org/downloads/
    echo  (spunta "Add Python to PATH" durante l'installazione)
    echo.
    goto :avvia_gui
)

:: ── Crea directory build_win se non esiste ancora ───────────────────────────
if not exist "%BUILD%" (
    echo  [INFO] Creo directory %BUILD%...
    mkdir "%BUILD%"
)

:: ── Crea ambiente virtuale se non esiste ─────────────────────────────────────
if not exist "%VENV_PYTHON%" (
    echo  [INFO] Creo ambiente virtuale Python in py_env\...
    %PYTHON% -m venv "%VENV%"
    if not exist "%VENV_PYTHON%" (
        echo  [AVVISO] Creazione venv fallita - uso Python di sistema.
        set VENV_PYTHON=%PYTHON%
    ) else (
        echo  [OK] Ambiente virtuale creato.
    )
)

:: ── Controlla se tutte le dipendenze sono installate ─────────────────────────
:: Verifica l'insieme completo (non solo 4 pacchetti) per non saltare mai
:: l'installazione di psutil, scipy, matplotlib, pandas, ecc.
if not exist "%REQS%" goto :avvia_gui

"%VENV_PYTHON%" -c "import sympy, mpmath, numpy, pypdf, psutil, pandas, scipy, matplotlib, requests, openpyxl" >nul 2>&1
if %errorlevel% equ 0 goto :avvia_gui

echo  [INFO] Installo dipendenze Python nel venv (solo al primo avvio o dopo aggiornamento)...
echo  (sympy, numpy, psutil, pandas, scipy, matplotlib, pypdf, ...)
echo  Questo puo' richiedere 2-5 minuti alla prima esecuzione.
echo.

:: ── Aggiorna pip (silenzioso) ─────────────────────────────────────────────────
"%VENV_PYTHON%" -m pip install --quiet --upgrade pip ^
    --trusted-host pypi.org --trusted-host files.pythonhosted.org >nul 2>&1

:: ── Installa dipendenze con fallback per ambienti corporate ──────────────────
:: Tentativo 1: pip normale
"%VENV_PYTHON%" -m pip install --quiet -r "%REQS%" ^
    --trusted-host pypi.org --trusted-host files.pythonhosted.org
set PIP_RC=%errorlevel%

if %PIP_RC% neq 0 (
    echo  [AVVISO] Installazione standard fallita (codice %PIP_RC%).
    echo  [INFO] Ritento con --only-binary :all: (evita compilazione da sorgente)...
    "%VENV_PYTHON%" -m pip install --quiet -r "%REQS%" ^
        --only-binary :all: ^
        --trusted-host pypi.org --trusted-host files.pythonhosted.org
    set PIP_RC2=%errorlevel%

    if !PIP_RC2! neq 0 (
        echo  [AVVISO] Secondo tentativo fallito.
        echo  [INFO] Installo pacchetti uno ad uno per identificare il problema...
        for /f "usebackq eol=# tokens=*" %%P in ("%REQS%") do (
            if not "%%P"=="" (
                "%VENV_PYTHON%" -m pip install --quiet "%%P" ^
                    --trusted-host pypi.org --trusted-host files.pythonhosted.org >nul 2>&1
                if errorlevel 1 (
                    echo    [!] Impossibile installare: %%P  ^(continuo senza^)
                )
            )
        )
    ) else (
        echo  [OK] Dipendenze installate ^(modalita' binaria^).
    )
) else (
    echo  [OK] Dipendenze installate.
)

:: ── Verifica finale ───────────────────────────────────────────────────────────
"%VENV_PYTHON%" -c "import sympy; import numpy; print('[OK] Python + sympy + numpy pronti.')" 2>&1
echo.

:: ── Avvia GUI Qt6 ─────────────────────────────────────────────────────────────
:avvia_gui
set GUI=%BUILD%\Prismalux_GUI.exe
set TUI=%BUILD%\prismalux.exe

:: Esporta il percorso Python del venv come variabile d'ambiente
:: La GUI (findPython) la legge come fallback se il percorso relativo non funziona
if exist "%VENV_PYTHON%" (
    set PRISMALUX_PYTHON=%VENV_PYTHON%
)

:: ── Verifica e copia DLL Qt6 mancanti (da MSYS2 se disponibile) ─────────────
:: Evita "DLL non trovata" quando build.bat non e' stato eseguito su questo PC
set MSYS2_DLL=C:\msys64\ucrt64\bin
if exist "%GUI%" (
    if not exist "%BUILD%\Qt6Core.dll" (
        if exist "%MSYS2_DLL%\Qt6Core.dll" (
            echo  [INFO] DLL Qt6 non trovate — copio da MSYS2...
            for %%D in (Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6Network.dll
                        libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll
                        libdouble-conversion.dll libpcre2-16-0.dll libzstd.dll zlib1.dll
                        libfreetype-6.dll libpng16-16.dll libharfbuzz-0.dll
                        libbz2-1.dll libiconv-2.dll libintl-8.dll libmd4c.dll) do (
                if exist "%MSYS2_DLL%\%%D" copy /y "%MSYS2_DLL%\%%D" "%BUILD%\" >nul 2>&1
            )
            if not exist "%BUILD%\platforms" mkdir "%BUILD%\platforms"
            if exist "%MSYS2_DLL%\..\share\qt6\plugins\platforms\qwindows.dll" (
                copy /y "%MSYS2_DLL%\..\share\qt6\plugins\platforms\qwindows.dll" "%BUILD%\platforms\" >nul 2>&1
            )
            if not exist "%BUILD%\styles" mkdir "%BUILD%\styles"
            if exist "%MSYS2_DLL%\..\share\qt6\plugins\styles\qwindowsvistastyle.dll" (
                copy /y "%MSYS2_DLL%\..\share\qt6\plugins\styles\qwindowsvistastyle.dll" "%BUILD%\styles\" >nul 2>&1
            )
            echo  [OK] DLL copiate.
        ) else (
            echo  [AVVISO] DLL Qt6 mancanti e MSYS2 non trovato in C:\msys64
            echo  Esegui build.bat almeno una volta per copiare le DLL necessarie.
            echo.
        )
    )
)

if exist "%GUI%" (
    echo  [OK] Avvio GUI...
    start "" "%GUI%"
    goto :fine
)

echo  [AVVISO] Prismalux_GUI.exe non trovato.
echo  Esegui build.bat per compilare il progetto.
echo  Percorso atteso: Qt_GUI\build_win\Prismalux_GUI.exe
echo.

if exist "%TUI%" (
    echo  [INFO] Avvio TUI (modalita Ollama)...
    "%TUI%" --backend ollama
    goto :fine
)

echo  [ERRORE] Nessun eseguibile trovato. Esegui build.bat.
echo.
pause
exit /b 1

:fine
endlocal

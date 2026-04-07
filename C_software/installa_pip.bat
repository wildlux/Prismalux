@echo off
:: installa_pip.bat — Installa/ripara le dipendenze Python per Prismalux
:: Eseguire come AMMINISTRATORE se pip fallisce con errori di permessi.
setlocal EnableDelayedExpansion

echo.
echo +--------------------------------------------------+
echo ^|  Prismalux — Installazione dipendenze Python     ^|
echo +--------------------------------------------------+
echo.

set BASE=%~dp0
set REQS=%BASE%requirements_python.txt
set BUILD=%BASE%Qt_GUI\build_win
set VENV=%BUILD%\py_env
set VENV_PYTHON=%VENV%\Scripts\python.exe

:: ── Trova Python ─────────────────────────────────────────────
set PYTHON=
where python  >nul 2>&1 && set PYTHON=python
if not defined PYTHON ( where py >nul 2>&1 && set PYTHON=py )
if not defined PYTHON ( where python3 >nul 2>&1 && set PYTHON=python3 )

if not defined PYTHON (
    echo [ERRORE] Python non trovato nel PATH.
    echo Scarica da: https://www.python.org/downloads/
    echo (spunta "Add Python to PATH" durante l'installazione^)
    pause & exit /b 1
)
echo [OK] Python trovato: %PYTHON%
echo.

:: ── Ricrea venv se corrotto ───────────────────────────────────
if exist "%VENV_PYTHON%" (
    "%VENV_PYTHON%" -c "import sys; print('Python', sys.version)" >nul 2>&1
    if errorlevel 1 (
        echo [INFO] Venv sembra corrotto - lo ricreo...
        rmdir /s /q "%VENV%" >nul 2>&1
    )
)

if not exist "%VENV_PYTHON%" (
    if not exist "%BUILD%" mkdir "%BUILD%"
    echo [INFO] Creo ambiente virtuale...
    %PYTHON% -m venv "%VENV%"
    if not exist "%VENV_PYTHON%" (
        echo [AVVISO] Venv non creato - uso Python di sistema.
        set VENV_PYTHON=%PYTHON%
    )
)

:: ── Aggiorna pip ─────────────────────────────────────────────
echo [INFO] Aggiorno pip...
"%VENV_PYTHON%" -m pip install --upgrade pip ^
    --trusted-host pypi.org --trusted-host files.pythonhosted.org
echo.

:: ── Installa dipendenze ──────────────────────────────────────
echo [INFO] Installo dipendenze da requirements_python.txt...
echo.
"%VENV_PYTHON%" -m pip install -r "%REQS%" ^
    --trusted-host pypi.org --trusted-host files.pythonhosted.org
set RC=%errorlevel%

if %RC% neq 0 (
    echo.
    echo [INFO] Tentativo con --only-binary (evita compilazione da sorgente^)...
    "%VENV_PYTHON%" -m pip install -r "%REQS%" ^
        --only-binary :all: ^
        --trusted-host pypi.org --trusted-host files.pythonhosted.org
    set RC2=%errorlevel%
    if !RC2! neq 0 (
        echo.
        echo [INFO] Installo pacchetti uno ad uno...
        for /f "usebackq eol=# tokens=*" %%P in ("%REQS%") do (
            if not "%%P"=="" (
                echo   Installo: %%P
                "%VENV_PYTHON%" -m pip install "%%P" ^
                    --trusted-host pypi.org --trusted-host files.pythonhosted.org
            )
        )
    )
)

:: ── Verifica ─────────────────────────────────────────────────
echo.
echo [INFO] Verifica installazione...
"%VENV_PYTHON%" -c "import sympy, numpy, psutil, pandas; print('[OK] Pacchetti principali pronti.')"
if errorlevel 1 (
    echo [ERRORE] Alcuni pacchetti non sono stati installati correttamente.
    echo Controlla i messaggi sopra e riprova come Amministratore.
) else (
    echo [OK] Tutte le dipendenze Python sono installate correttamente.
    echo Puoi avviare Prismalux con Avvia_Prismalux.bat
)
echo.
pause
endlocal

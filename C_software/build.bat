@echo off
setlocal EnableDelayedExpansion

echo.
echo +--------------------------------------------------+
echo ^|   Prismalux v2.2 - Compilazione Windows (.exe)  ^|
echo ^|   TUI (gcc) + GUI Qt6 (MSYS2 UCRT64)            ^|
echo +--------------------------------------------------+
echo.

:: Cerca gcc (MinGW / MSYS2 / WinLibs)
set GCC=
set GCC_SOURCE=

:: 1) gcc gia nel PATH?
where gcc >nul 2>&1
if %errorlevel%==0 (
    set GCC=gcc
    set GCC_SOURCE=PATH
    goto :gcc_found
)

:: 2) MSYS2 UCRT64 in C:\msys64
if exist "C:\msys64\ucrt64\bin\gcc.exe" (
    set GCC=C:\msys64\ucrt64\bin\gcc.exe
    set GCC_SOURCE=MSYS2 UCRT64
    goto :gcc_found
)

:: 3) MSYS2 MINGW64 in C:\msys64
if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set GCC=C:\msys64\mingw64\bin\gcc.exe
    set GCC_SOURCE=MSYS2 MINGW64
    goto :gcc_found
)

:: 4) WinLibs / w64devkit in posizioni comuni (solo 64-bit)
for %%P in (
    "C:\w64devkit\bin\gcc.exe"
    "C:\mingw64\bin\gcc.exe"
    "%USERPROFILE%\scoop\apps\mingw\current\bin\gcc.exe"
    "C:\ProgramData\chocolatey\bin\gcc.exe"
) do (
    if exist %%P (
        set GCC=%%~P
        set GCC_SOURCE=%%~P
        goto :gcc_found
    )
)

:: Nessun gcc trovato — prova auto-installazione via pacman MSYS2
echo  [INFO] gcc non trovato. Cerco MSYS2 per installarlo automaticamente...
echo.

if exist "C:\msys64\usr\bin\pacman.exe" (
    echo  [INFO] MSYS2 trovato. Installo gcc UCRT64 automaticamente...
    echo  (potrebbe richiedere qualche minuto, connessione internet necessaria)
    echo.
    C:\msys64\usr\bin\pacman.exe -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc
    echo.
    if exist "C:\msys64\ucrt64\bin\gcc.exe" (
        set GCC=C:\msys64\ucrt64\bin\gcc.exe
        set GCC_SOURCE=MSYS2 UCRT64 (auto-installato)
        goto :gcc_found
    )
    echo  [ERRORE] Installazione fallita. Prova manualmente:
    echo  Apri "MSYS2 UCRT64" dal menu Start e digita:
    echo  pacman -S --needed mingw-w64-ucrt-x86_64-gcc
    echo.
    pause
    exit /b 1
)

echo  [ERRORE] gcc non trovato e MSYS2 non e installato.
echo.
echo  Installa MSYS2 da: https://www.msys2.org/
echo  Poi apri "MSYS2 UCRT64" e digita:
echo  pacman -S --needed mingw-w64-ucrt-x86_64-gcc
echo.
pause
exit /b 1

:gcc_found
echo  [OK] gcc trovato: %GCC_SOURCE%

:: Aggiunge la cartella bin di gcc al PATH (necessario per linker e assembler)
for %%F in ("%GCC%") do set GCC_BIN=%%~dpF
set PATH=%GCC_BIN%;%PATH%

:: Versione gcc
for /f "delims=" %%V in ('"%GCC%" --version 2^>^&1') do (
    if not defined GCC_VER set GCC_VER=%%V
)
echo  [OK] Versione: %GCC_VER%
echo.

:: Controlla file sorgente
if not exist "src\main.c" (
    echo  [ERRORE] src\main.c non trovato.
    echo  Esegui questo script dalla cartella C_software.
    pause
    exit /b 1
)
if not exist "src\hw_detect.c" (
    echo  [ERRORE] src\hw_detect.c non trovato.
    pause
    exit /b 1
)
echo  [OK] Sorgenti trovati in src\

:: Crea cartella models se non esiste
if not exist "models" (
    mkdir models
    echo  [OK] Cartella models\ creata
)

:: Opzioni di compilazione
set CFLAGS=-O2 -m64 -march=native -Wall -DWIN32_LEAN_AND_MEAN -Iinclude
set LIBS=-static -lws2_32 -lm -lshell32
set TARGET=prismalux.exe
set SOURCES=src\main.c src\backend.c src\terminal.c src\http.c src\ai.c src\modelli.c src\output.c src\multi_agente.c src\strumenti.c src\hw_detect.c src\agent_scheduler.c src\prismalux_ui.c src\simulatore.c src\quiz.c src\config_toon.c src\context_store.c src\key_router.c src\rag.c src\rag_embed.c src\sim_common.c src\sim_sort.c src\sim_search.c src\sim_math.c src\sim_dp.c src\sim_grafi.c src\sim_tech.c src\sim_stringhe.c src\sim_strutture.c src\sim_greedy.c src\sim_backtrack.c src\sim_llm.c

:: Rilevamento GPU
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
    echo  [GPU] Nessuna GPU accelerata - modalita CPU (Ollama via HTTP)
)
echo.

:: Compilazione
echo  Compilazione in corso...
echo.

"%GCC%" %CFLAGS% -o %TARGET% %SOURCES% %LIBS% 2>errori_compilazione.txt

if %errorlevel% neq 0 (
    echo.
    echo  [ERRORE] Compilazione fallita. Errori:
    echo  ----------------------------------------
    type errori_compilazione.txt
    echo  ----------------------------------------
    echo.
    echo  Gli errori sono salvati in: errori_compilazione.txt
    pause
    exit /b 1
)
del errori_compilazione.txt 2>nul

:: TUI compilata con successo
echo.
echo +----------------------------------------------+
echo ^|        TUI compilata con successo!           ^|
echo +----------------------------------------------+
echo.
for %%F in (%TARGET%) do (
    set /a SIZE_KB=%%~zF/1024
    echo  File:    %TARGET% (!SIZE_KB! KB)
)
echo.

:: Copia prismalux.exe nella cartella della GUI (serve per auto-detect path GUI)
if exist "Qt_GUI\build_win" (
    copy /y %TARGET% "Qt_GUI\build_win\%TARGET%" >nul 2>&1
    echo  [OK] Copiato in Qt_GUI\build_win\%TARGET%
)

:: ════════════════════════════════════════════════
::   FASE 2 — GUI Qt6
:: ════════════════════════════════════════════════
echo +--------------------------------------------------+
echo ^|   Prismalux GUI Qt6 v2.2 - Compilazione         ^|
echo +--------------------------------------------------+
echo.

set MSYS2=C:\msys64
if not exist "%MSYS2%\usr\bin\pacman.exe" (
    echo  [AVVISO] MSYS2 non trovato — GUI Qt6 saltata.
    echo  Per compilare la GUI installa MSYS2 da: https://www.msys2.org/
    goto :solo_tui
)

:: Installa Qt6 + cmake + ninja se mancanti
set NEED_QT=0
if not exist "%MSYS2%\ucrt64\bin\qmake6.exe" set NEED_QT=1
if not exist "%MSYS2%\ucrt64\bin\cmake.exe"  set NEED_QT=1
if not exist "%MSYS2%\ucrt64\bin\ninja.exe"  set NEED_QT=1

if %NEED_QT%==1 (
    echo  [INFO] Qt6/cmake non trovati. Installo via MSYS2 pacman...
    echo  (scarica circa 300 MB, attendere)
    echo.
    "%MSYS2%\usr\bin\pacman.exe" -S --needed --noconfirm ^
        mingw-w64-ucrt-x86_64-qt6-base ^
        mingw-w64-ucrt-x86_64-qt6-tools ^
        mingw-w64-ucrt-x86_64-cmake ^
        mingw-w64-ucrt-x86_64-ninja ^
        mingw-w64-ucrt-x86_64-gcc
    echo.
    if not exist "%MSYS2%\ucrt64\bin\qmake6.exe" (
        echo  [ERRORE] Installazione Qt6 fallita — GUI saltata.
        goto :solo_tui
    )
    echo  [OK] Qt6 installato.
) else (
    echo  [OK] Qt6 e cmake gia installati.
)

if not exist "Qt_GUI\CMakeLists.txt" (
    echo  [AVVISO] Qt_GUI\CMakeLists.txt non trovato — GUI saltata.
    goto :solo_tui
)

:: Aggiunge UCRT64 al PATH per cmake e ninja
set PATH=%MSYS2%\ucrt64\bin;%MSYS2%\usr\bin;%PATH%

set QT_BUILD=Qt_GUI\build_win
echo  Configurazione cmake...
cmake -B %QT_BUILD% -S Qt_GUI ^
      -G "Ninja" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH="%MSYS2%\ucrt64" ^
      -DCMAKE_C_COMPILER="%MSYS2%\ucrt64\bin\gcc.exe" ^
      -DCMAKE_CXX_COMPILER="%MSYS2%\ucrt64\bin\g++.exe"

if %errorlevel% neq 0 (
    echo  [ERRORE] cmake configure fallito — GUI saltata.
    goto :solo_tui
)

echo  Compilazione Qt GUI (2-5 minuti)...
cmake --build %QT_BUILD% --config Release

if %errorlevel% neq 0 (
    echo  [ERRORE] Compilazione Qt fallita — GUI saltata.
    goto :solo_tui
)

:: Deploy DLL Qt
if exist "%MSYS2%\ucrt64\bin\windeployqt6.exe" (
    "%MSYS2%\ucrt64\bin\windeployqt6.exe" "%QT_BUILD%\Prismalux_GUI.exe" >nul 2>&1
) else (
    for %%D in (Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6Network.dll
                libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll
                libdouble-conversion.dll libpcre2-16-0.dll libzstd.dll zlib1.dll
                libfreetype-6.dll libpng16-16.dll libharfbuzz-0.dll) do (
        if exist "%MSYS2%\ucrt64\bin\%%D" copy /y "%MSYS2%\ucrt64\bin\%%D" "%QT_BUILD%\" >nul 2>&1
    )
    if not exist "%QT_BUILD%\platforms" mkdir "%QT_BUILD%\platforms"
    copy /y "%MSYS2%\ucrt64\share\qt6\plugins\platforms\qwindows.dll" "%QT_BUILD%\platforms\" >nul 2>&1
    if not exist "%QT_BUILD%\styles" mkdir "%QT_BUILD%\styles"
    copy /y "%MSYS2%\ucrt64\share\qt6\plugins\styles\qwindowsvistastyle.dll" "%QT_BUILD%\styles\" >nul 2>&1
)

:: Copia prismalux.exe nella cartella della GUI (ora la cartella esiste di sicuro)
copy /y %TARGET% "%QT_BUILD%\%TARGET%" >nul 2>&1
echo  [OK] prismalux.exe copiato in %QT_BUILD%\

echo.
echo +--------------------------------------------------+
echo ^|   GUI Qt v2.2 compilata! Avvio...                ^|
echo ^|   [7] Smith Primi  [8] Smith pi/e/Primi           ^|
echo +--------------------------------------------------+
echo.
start "" "%QT_BUILD%\Prismalux_GUI.exe"
goto :fine

:solo_tui
echo.
echo  Avvio TUI in modalita Ollama...
echo  (assicurati che "ollama serve" sia in esecuzione)
echo.
if exist "Qt_GUI\build_win\prismalux.exe" (
    "Qt_GUI\build_win\prismalux.exe" --backend ollama
) else (
    prismalux.exe --backend ollama
)

:fine
echo.
pause
endlocal

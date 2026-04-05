@echo off
setlocal EnableDelayedExpansion

echo.
echo +----------------------------------------------+
echo ^|   Prismalux GUI Qt6 - Compilazione Windows   ^|
echo +----------------------------------------------+
echo.

:: ── Cerca MSYS2 ──────────────────────────────────────────────────────────
set MSYS2=C:\msys64
if not exist "%MSYS2%\usr\bin\pacman.exe" (
    echo  [ERRORE] MSYS2 non trovato in C:\msys64
    echo  Installa MSYS2 da: https://www.msys2.org/
    echo  Poi riesegui build.bat per installare gcc, e questo script per Qt6.
    pause
    exit /b 1
)
echo  [OK] MSYS2 trovato in %MSYS2%

:: ── Imposta PATH UCRT64 ───────────────────────────────────────────────────
set PATH=%MSYS2%\ucrt64\bin;%MSYS2%\usr\bin;%PATH%

:: ── Controlla se Qt6 e cmake sono gia installati ─────────────────────────
set NEED_INSTALL=0
if not exist "%MSYS2%\ucrt64\bin\qmake6.exe" set NEED_INSTALL=1
if not exist "%MSYS2%\ucrt64\bin\cmake.exe"  set NEED_INSTALL=1
if not exist "%MSYS2%\ucrt64\bin\ninja.exe"  set NEED_INSTALL=1

if %NEED_INSTALL%==1 (
    echo  [INFO] Qt6 e/o cmake non trovati. Installo via MSYS2 pacman...
    echo  (scarica ~300 MB, ci vogliono alcuni minuti)
    echo.
    "%MSYS2%\usr\bin\pacman.exe" -S --needed --noconfirm ^
        mingw-w64-ucrt-x86_64-qt6-base ^
        mingw-w64-ucrt-x86_64-qt6-tools ^
        mingw-w64-ucrt-x86_64-cmake ^
        mingw-w64-ucrt-x86_64-ninja ^
        mingw-w64-ucrt-x86_64-gcc
    echo.
    if not exist "%MSYS2%\ucrt64\bin\qmake6.exe" (
        echo  [ERRORE] Installazione Qt6 fallita.
        pause
        exit /b 1
    )
    echo  [OK] Qt6 installato con successo.
) else (
    echo  [OK] Qt6 e cmake gia installati.
)
echo.

:: ── Controlla sorgenti ───────────────────────────────────────────────────
if not exist "Qt_GUI\CMakeLists.txt" (
    echo  [ERRORE] Qt_GUI\CMakeLists.txt non trovato.
    echo  Esegui questo script dalla cartella C_software.
    pause
    exit /b 1
)
echo  [OK] Sorgenti Qt_GUI trovati.

:: ── Configura con cmake ──────────────────────────────────────────────────
set BUILD_DIR=Qt_GUI\build_win
set QT6_DIR=%MSYS2%\ucrt64\lib\cmake\Qt6

echo  Configurazione cmake...
cmake -B %BUILD_DIR% -S Qt_GUI ^
      -G "Ninja" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH="%MSYS2%\ucrt64" ^
      -DCMAKE_C_COMPILER="%MSYS2%\ucrt64\bin\gcc.exe" ^
      -DCMAKE_CXX_COMPILER="%MSYS2%\ucrt64\bin\g++.exe"

if %errorlevel% neq 0 (
    echo.
    echo  [ERRORE] cmake configure fallito.
    pause
    exit /b 1
)
echo  [OK] cmake configurato.
echo.

:: ── Compila ──────────────────────────────────────────────────────────────
echo  Compilazione in corso (potrebbe richiedere 2-5 minuti)...
echo.
cmake --build %BUILD_DIR% --config Release

if %errorlevel% neq 0 (
    echo.
    echo  [ERRORE] Compilazione fallita.
    pause
    exit /b 1
)

:: ── Deploy DLL Qt ────────────────────────────────────────────────────────
echo.
echo  Copia DLL Qt necessarie...
if exist "%MSYS2%\ucrt64\bin\windeployqt6.exe" (
    "%MSYS2%\ucrt64\bin\windeployqt6.exe" "%BUILD_DIR%\Prismalux_GUI.exe"
    echo  [OK] DLL Qt copiate.
) else (
    :: Copia manuale DLL essenziali
    for %%D in (
        Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6Network.dll
        libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll
        libdouble-conversion.dll libicudt74.dll libicuin74.dll libicuuc74.dll
        libmd4c.dll libpcre2-16-0.dll libzstd.dll zlib1.dll libfreetype-6.dll
        libpng16-16.dll libharfbuzz-0.dll libbz2-1.dll libglib-2.0-0.dll
    ) do (
        if exist "%MSYS2%\ucrt64\bin\%%D" (
            copy /y "%MSYS2%\ucrt64\bin\%%D" "%BUILD_DIR%\" >nul 2>&1
        )
    )
    :: Copia plugin Qt (necessari per avviare la finestra)
    if not exist "%BUILD_DIR%\platforms" mkdir "%BUILD_DIR%\platforms"
    copy /y "%MSYS2%\ucrt64\share\qt6\plugins\platforms\qwindows.dll" "%BUILD_DIR%\platforms\" >nul 2>&1
    if not exist "%BUILD_DIR%\styles" mkdir "%BUILD_DIR%\styles"
    copy /y "%MSYS2%\ucrt64\share\qt6\plugins\styles\qwindowsvistastyle.dll" "%BUILD_DIR%\styles\" >nul 2>&1
    echo  [OK] DLL Qt copiate manualmente.
)

:: ── Successo ─────────────────────────────────────────────────────────────
echo.
echo +----------------------------------------------+
echo ^|      Qt GUI compilata con successo!          ^|
echo +----------------------------------------------+
echo.
echo  Eseguibile: %BUILD_DIR%\Prismalux_GUI.exe
echo.
echo  Per avviarla:
echo    Assicurati che Ollama sia in esecuzione, poi:
echo    %BUILD_DIR%\Prismalux_GUI.exe
echo.

echo  Avvio Prismalux GUI...
echo.
start "" "%BUILD_DIR%\Prismalux_GUI.exe"

echo.
pause
endlocal

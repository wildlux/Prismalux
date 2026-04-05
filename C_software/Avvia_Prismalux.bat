@echo off
setlocal EnableDelayedExpansion

echo.
echo +----------------------------------------------+
echo ^|         Prismalux — Avvio Windows            ^|
echo +----------------------------------------------+
echo.

:: Percorso GUI (cartella dove si trova questo .bat)
set BASE=%~dp0
set GUI=%BASE%Qt_GUI\build_win\Prismalux_GUI.exe
set TUI=%BASE%Qt_GUI\build_win\prismalux.exe

:: ── Controlla se la GUI e' compilata ────────────────────────
if exist "%GUI%" goto :avvia_gui

echo  [AVVISO] Prismalux_GUI.exe non trovato.
echo  Devi prima compilare il progetto con build.bat
echo.
echo  Percorso atteso: Qt_GUI\build_win\Prismalux_GUI.exe
echo.

:: Fallback: prova la TUI se almeno quella c'e'
if exist "%TUI%" goto :avvia_tui

echo  [ERRORE] Nessun eseguibile trovato.
echo  Esegui build.bat per compilare il progetto.
echo.
pause
exit /b 1

:: ── Avvia GUI Qt6 ────────────────────────────────────────────
:avvia_gui
echo  [OK] Avvio GUI...
start "" "%GUI%"
goto :fine

:: ── Fallback TUI ─────────────────────────────────────────────
:avvia_tui
echo  [INFO] GUI non trovata, avvio TUI (modalita Ollama)...
echo  (assicurati che "ollama serve" sia in esecuzione)
echo.
"%TUI%" --backend ollama

:fine
endlocal

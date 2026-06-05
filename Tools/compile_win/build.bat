@echo off
REM Collegamento a build.bat nella root del progetto.
REM Doppio clic qui oppure sulla root — stesso risultato.
cd /d "%~dp0.."
call build.bat

#!/usr/bin/env bash
# Avvia l'app Kivy in modalità desktop (sviluppo/test)
set -e
cd "$(dirname "$0")"

# Emula dimensioni telefono
export KIVY_WINDOW=sdl2
export DISPLAY="${DISPLAY:-:0}"

# Python 3.11 richiesto: 3.14 non ha _window_sdl2.so compilato
python3.11 main.py

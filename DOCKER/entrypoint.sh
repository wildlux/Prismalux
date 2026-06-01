#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────
#  Entrypoint Prismalux Docker
#  Prepara l'ambiente Qt6/X11 prima di lanciare il binario.
# ─────────────────────────────────────────────────────────────────────
set -euo pipefail

# Symlink temi nella home utente (Prismalux li cerca accanto al binario)
THEMES_SRC="/usr/local/share/prismalux/themes"
THEMES_DST="/home/prismalux/themes"
if [ -d "$THEMES_SRC" ] && [ ! -L "$THEMES_DST" ]; then
    ln -sf "$THEMES_SRC" "$THEMES_DST"
fi

# Verifica display X11
if [ -z "${DISPLAY:-}" ]; then
    echo "⚠  DISPLAY non impostato — usa: docker run -e DISPLAY=\$DISPLAY ..."
    echo "   oppure avvia con: bash DOCKER/avvia_docker.sh"
    exit 1
fi

# Fallback piattaforma Qt: xcb (X11) oppure wayland
if [ -z "${QT_QPA_PLATFORM:-}" ]; then
    if [ -n "${WAYLAND_DISPLAY:-}" ]; then
        export QT_QPA_PLATFORM=wayland
    else
        export QT_QPA_PLATFORM=xcb
    fi
fi

exec "$@"

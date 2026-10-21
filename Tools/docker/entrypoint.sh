#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────
#  Entrypoint Prismalux Docker
#  Prepara l'ambiente Qt6/X11/Wayland prima di lanciare il binario.
# ─────────────────────────────────────────────────────────────────────
set -euo pipefail

# Runtime dir per Wayland/Qt. Se il socket Wayland del host è montato,
# questa directory coincide con quella del host.
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/runtime-prismalux}"
mkdir -p "$XDG_RUNTIME_DIR" 2>/dev/null || true
chmod 700 "$XDG_RUNTIME_DIR" 2>/dev/null || true

# I temi sono già accanto al binario: /usr/local/bin/themes/
if [ ! -d /usr/local/bin/themes ]; then
    echo "⚠  /usr/local/bin/themes non trovato — i temi useranno il fallback."
fi

# Verifica display X11/Wayland
if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    echo "⚠  DISPLAY/WAYLAND_DISPLAY non impostato."
    echo "   Avvia con: bash Tools/docker/avvia_docker.sh"
    echo "   oppure: docker run -e DISPLAY=\$DISPLAY ..."
    exit 1
fi

# Fallback piattaforma Qt: wayland se richiesto, altrimenti xcb
if [ -z "${QT_QPA_PLATFORM:-}" ]; then
    if [ -n "${WAYLAND_DISPLAY:-}" ]; then
        export QT_QPA_PLATFORM=wayland
    else
        export QT_QPA_PLATFORM=xcb
    fi
fi

exec "$@"

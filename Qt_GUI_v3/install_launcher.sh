#!/bin/bash
# ══════════════════════════════════════════════════════════════
#  Prismalux — Installa launcher KDE Plasma / Linux
# ══════════════════════════════════════════════════════════════
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="$SCRIPT_DIR/build/Prismalux_GUI"
DESKTOP_SRC="$SCRIPT_DIR/prismalux.desktop"
ICO_SRC="$SCRIPT_DIR/../ICONA/prismaluce.ico"

ICON_DIR="$HOME/.local/share/icons/hicolor/256x256/apps"
APP_DIR="$HOME/.local/share/applications"
DESKTOP_LINK="$HOME/Desktop/Prismalux.desktop"

# ── Controlla binario ─────────────────────────────────────────
if [ ! -f "$BINARY" ]; then
    echo "❌  Binario non trovato: $BINARY"
    echo "   Compila prima con:  ./build.sh"
    exit 1
fi

# ── Crea cartelle ─────────────────────────────────────────────
mkdir -p "$ICON_DIR"
mkdir -p "$APP_DIR"

# ── Converti icona .ico → .png ────────────────────────────────
ICON_DST="$ICON_DIR/prismalux.png"
if command -v convert &>/dev/null; then
    convert "$ICO_SRC"[0] "$ICON_DST" 2>/dev/null \
        && echo "✅  Icona installata: $ICON_DST" \
        || echo "⚠️   convert fallito, copia .ico come fallback"
    # fallback se convert ha prodotto 0 byte
    if [ ! -s "$ICON_DST" ]; then
        cp "$ICO_SRC" "$HOME/.local/share/icons/prismalux.ico"
        sed -i 's|^Icon=prismalux|Icon='"$HOME"'/.local/share/icons/prismalux.ico|' "$DESKTOP_SRC"
    fi
elif command -v magick &>/dev/null; then
    magick "$ICO_SRC"[0] "$ICON_DST" \
        && echo "✅  Icona installata: $ICON_DST" \
        || echo "⚠️   magick fallito"
else
    echo "⚠️   ImageMagick non trovato — uso percorso assoluto .ico"
    sed -i 's|^Icon=prismalux|Icon='"$ICO_SRC"'|' "$DESKTOP_SRC"
fi

# ── Aggiorna percorso Exec nel .desktop con il path assoluto ──
DESKTOP_TMP="$APP_DIR/prismalux.desktop"
cp "$DESKTOP_SRC" "$DESKTOP_TMP"
sed -i "s|^Exec=.*|Exec=$BINARY|" "$DESKTOP_TMP"
if [ -s "$ICON_DST" ]; then
    sed -i "s|^Icon=.*|Icon=prismalux|" "$DESKTOP_TMP"
fi
chmod 644 "$DESKTOP_TMP"

# ── Shortcut sul Desktop ──────────────────────────────────────
cp "$DESKTOP_TMP" "$DESKTOP_LINK"
chmod +x "$DESKTOP_LINK"

# ── Aggiorna database applicazioni ───────────────────────────
update-desktop-database "$APP_DIR" 2>/dev/null || true
gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
kbuildsycoca6 2>/dev/null || kbuildsycoca5 2>/dev/null || true

echo ""
echo "✅  Launcher installato!"
echo "   Menu KDE:    Applicazioni → Utility → Prismalux"
echo "   Desktop:     ~/Desktop/Prismalux.desktop"
echo ""
echo "Se l'icona non appare subito, esci e rientra in KDE oppure:"
echo "   kquitapp6 plasmashell && kstart6 plasmashell"

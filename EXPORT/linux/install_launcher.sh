#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════
#  Prismalux — Installazione Linux
#  Uso: bash install_launcher.sh [--system | --user (default)]
#
#  --user   (default) installa in ~/.local/ senza sudo
#  --system           installa in /opt/ con sudo
#  --uninstall        rimuovi installazione utente
# ══════════════════════════════════════════════════════════════
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

APPIMAGE="$ROOT_DIR/EXPORT/linux/Prismalux-x86_64.AppImage"
[ -f "$APPIMAGE" ] || APPIMAGE="$ROOT_DIR/Prismalux-x86_64.AppImage"

BINARY="$ROOT_DIR/gui/build_gui/Prismalux_GUI"
DESKTOP_SRC="$ROOT_DIR/gui/prismalux.desktop"
ICO_SRC="$ROOT_DIR/ICONA/prismaluce.ico"
PNG_SRC="$ROOT_DIR/ICONA/prismaluce.png"

C="\033[36m"; G="\033[32m"; Y="\033[33m"; R="\033[31m"; N="\033[0m"; B="\033[1m"
ok()   { echo -e "  ${G}✅${N} $*"; }
warn() { echo -e "  ${Y}⚠${N}  $*"; }
fail() { echo -e "  ${R}❌${N} $*" >&2; exit 1; }
step() { echo -e "\n${B}${C}▶ $*${N}"; }

MODE="user"
for arg in "$@"; do
    case "$arg" in
        --system)    MODE="system" ;;
        --user)      MODE="user" ;;
        --uninstall) MODE="uninstall" ;;
    esac
done

# ── Modalità uninstall ────────────────────────────────────────
if [ "$MODE" = "uninstall" ]; then
    step "Rimozione installazione utente..."
    rm -f  "$HOME/.local/bin/prismalux"
    rm -f  "$HOME/.local/share/applications/prismalux.desktop"
    rm -f  "$HOME/.local/share/icons/hicolor/256x256/apps/prismalux.png"
    rm -f  "$HOME/.local/share/icons/hicolor/256x256/apps/prismalux.ico"
    rm -rf "$HOME/.local/opt/prismalux"
    rm -f  "$HOME/Desktop/Prismalux.desktop"
    update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
    gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
    ok "Installazione utente rimossa."
    exit 0
fi

echo ""
echo -e "${B}════════════════════════════════════════${N}"
echo -e "${B}  Prismalux — Installazione Linux ($MODE)${N}"
echo -e "${B}════════════════════════════════════════${N}"

# ── Scegli sorgente (AppImage preferita, poi binario) ─────────
if [ -f "$APPIMAGE" ]; then
    SOURCE="$APPIMAGE"
    SOURCE_TYPE="AppImage"
elif [ -f "$BINARY" ]; then
    SOURCE="$BINARY"
    SOURCE_TYPE="binario"
else
    fail "Nessun eseguibile trovato.\nCompila prima con: python3 build.py\noppure: ./aggiorna.sh --gui"
fi
ok "Sorgente: $SOURCE ($SOURCE_TYPE)"

# ── Percorsi di installazione ─────────────────────────────────
if [ "$MODE" = "system" ]; then
    command -v sudo &>/dev/null || fail "sudo non trovato — usa --user"
    INSTALL_DIR="/opt/prismalux"
    BIN_DIR="/usr/local/bin"
    ICON_DIR="/usr/share/icons/hicolor/256x256/apps"
    APP_DIR="/usr/share/applications"
    DESKTOP_LINK="/root/Desktop/Prismalux.desktop"
    SUDO="sudo"
else
    INSTALL_DIR="$HOME/.local/opt/prismalux"
    BIN_DIR="$HOME/.local/bin"
    ICON_DIR="$HOME/.local/share/icons/hicolor/256x256/apps"
    APP_DIR="$HOME/.local/share/applications"
    DESKTOP_LINK="$HOME/Desktop/Prismalux.desktop"
    SUDO=""
fi

# ── Crea cartelle ─────────────────────────────────────────────
step "Creo cartelle..."
$SUDO mkdir -p "$INSTALL_DIR" "$BIN_DIR" "$ICON_DIR" "$APP_DIR"
ok "Cartelle pronte"

# ── Copia eseguibile ──────────────────────────────────────────
step "Installo eseguibile..."
EXEC_NAME="Prismalux_GUI"
[ "$SOURCE_TYPE" = "AppImage" ] && EXEC_NAME="Prismalux.AppImage"
$SUDO cp "$SOURCE" "$INSTALL_DIR/$EXEC_NAME"
$SUDO chmod +x "$INSTALL_DIR/$EXEC_NAME"
ok "Copiato → $INSTALL_DIR/$EXEC_NAME"

# ── Wrapper script con APPIMAGE_EXTRACT_AND_RUN (compatibile FUSE2/3) ─────────
# Ubuntu 24.04+ non ha libfuse2 di default; APPIMAGE_EXTRACT_AND_RUN=1
# estrae l'AppImage in una dir temporanea ed esegue senza FUSE.
if [ "$SOURCE_TYPE" = "AppImage" ]; then
    WRAPPER="$INSTALL_DIR/prismalux-launch"
    $SUDO tee "$WRAPPER" > /dev/null << 'WRAPPER_EOF'
#!/usr/bin/env bash
# Rileva FUSE 2; se assente usa extract-and-run (nessuna dipendenza esterna)
if ldconfig -p 2>/dev/null | grep -q libfuse.so.2 || \
   [ -f /usr/lib/x86_64-linux-gnu/libfuse.so.2 ] || \
   [ -f /usr/lib/libfuse.so.2 ]; then
    exec "$(dirname "$0")/Prismalux.AppImage" "$@"
else
    exec env APPIMAGE_EXTRACT_AND_RUN=1 "$(dirname "$0")/Prismalux.AppImage" "$@"
fi
WRAPPER_EOF
    $SUDO chmod +x "$WRAPPER"
    ok "Wrapper FUSE-agnostico → $WRAPPER"
fi

# ── Crea symlink in BIN_DIR ───────────────────────────────────
if [ "$SOURCE_TYPE" = "AppImage" ] && [ -f "$INSTALL_DIR/prismalux-launch" ]; then
    $SUDO ln -sf "$INSTALL_DIR/prismalux-launch" "$BIN_DIR/prismalux"
else
    $SUDO ln -sf "$INSTALL_DIR/$EXEC_NAME" "$BIN_DIR/prismalux"
fi
ok "Launcher → $BIN_DIR/prismalux"

# ── Copia themes/ se binario diretto ──────────────────────────
if [ "$SOURCE_TYPE" = "binario" ] && [ -d "$ROOT_DIR/gui/themes" ]; then
    $SUDO cp -r "$ROOT_DIR/gui/themes" "$INSTALL_DIR/themes"
    ok "Temi → $INSTALL_DIR/themes/"
fi

# ── Icona ─────────────────────────────────────────────────────
step "Installo icona..."
ICON_DST="$ICON_DIR/prismalux.png"
if [ -f "$PNG_SRC" ]; then
    $SUDO cp "$PNG_SRC" "$ICON_DST"
    ok "Icona PNG → $ICON_DST"
elif command -v convert &>/dev/null && [ -f "$ICO_SRC" ]; then
    $SUDO convert "$ICO_SRC"[0] "$ICON_DST" 2>/dev/null \
        && ok "Icona .ico → .png → $ICON_DST" \
        || warn "convert fallito — uso .ico direttamente"
elif command -v magick &>/dev/null && [ -f "$ICO_SRC" ]; then
    $SUDO magick "$ICO_SRC"[0] "$ICON_DST" 2>/dev/null \
        && ok "Icona → $ICON_DST" \
        || warn "magick fallito"
else
    warn "Icona non installata (ImageMagick non trovato e nessun PNG disponibile)"
fi

# ── File .desktop ─────────────────────────────────────────────
step "Installo .desktop..."
DESKTOP_DST="$APP_DIR/prismalux.desktop"
ICON_KEY="prismalux"
[ -f "$ICON_DST" ] || ICON_KEY="$ICO_SRC"

$SUDO tee "$DESKTOP_DST" > /dev/null << DESKTOP_EOF
[Desktop Entry]
Name=Prismalux
Comment=AI Suite — Ollama, RAG, Multi-Agente, Matematica, Bioinformatica
Exec=$( [ "$SOURCE_TYPE" = "AppImage" ] && echo "$INSTALL_DIR/prismalux-launch" || echo "$INSTALL_DIR/$EXEC_NAME" )
Icon=$ICON_KEY
Terminal=false
Type=Application
Categories=Education;Science;Utility;
Keywords=AI;LLM;Ollama;RAG;
StartupWMClass=Prismalux_GUI
DESKTOP_EOF
$SUDO chmod 644 "$DESKTOP_DST"
ok ".desktop → $DESKTOP_DST"

# ── Shortcut Desktop utente ───────────────────────────────────
if [ -d "$(dirname "$DESKTOP_LINK")" ]; then
    cp "$DESKTOP_DST" "$DESKTOP_LINK" 2>/dev/null || true
    chmod +x "$DESKTOP_LINK" 2>/dev/null || true
    ok "Shortcut → $DESKTOP_LINK"
fi

# ── Aggiorna database ─────────────────────────────────────────
step "Aggiorno database applicazioni..."
update-desktop-database "$APP_DIR" 2>/dev/null || true
gtk-update-icon-cache -f -t "$(dirname "$(dirname "$ICON_DIR")")" 2>/dev/null || true
kbuildsycoca6 2>/dev/null || kbuildsycoca5 2>/dev/null || true
ok "Database aggiornato"

# ── Riepilogo ─────────────────────────────────────────────────
echo ""
echo -e "${B}${G}════════════════════════════════════════${N}"
echo -e "${B}${G}  Prismalux installato! ($MODE)${N}"
echo -e "${B}${G}════════════════════════════════════════${N}"
echo -e "  Eseguibile : $INSTALL_DIR/$EXEC_NAME"
echo -e "  Comando    : prismalux"
echo -e "  Menu       : Applicazioni → Utility → Prismalux"
[ -f "$DESKTOP_LINK" ] && echo -e "  Desktop    : $DESKTOP_LINK"
echo ""
echo -e "  Per disinstallare: bash EXPORT/linux/install_launcher.sh --uninstall"
echo ""

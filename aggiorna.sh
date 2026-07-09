#!/usr/bin/env bash
# Build Prismalux_GUI + aggiorna file .desktop
# Percorso canonico build: build_gui/ nella root del progetto.
# Uso:
#   ./aggiorna.sh            — build + aggiorna .desktop
#   ./aggiorna.sh --gui      — solo build (nessun aggiornamento .desktop, per crea_appimage.sh)
#   ./aggiorna.sh --test     — build + test suite
#   ./aggiorna.sh --install  — build + installa icona nel sistema
set -e
cd "$(dirname "$0")"
ROOT="$(pwd)"
BIN="$ROOT/build_gui/Prismalux_GUI"
DESKTOP_SRC="$ROOT/Prismalux.desktop"
DESKTOP_SYS="$HOME/.local/share/applications/prismalux.desktop"

ONLY_BUILD=false
[[ "$*" == *--gui* ]] && ONLY_BUILD=true

echo "==> Build Prismalux..."
cmake -B "$ROOT/build_gui" "$ROOT/gui/" -DCMAKE_BUILD_TYPE=Release -Wno-dev -q 2>/dev/null || true
cmake --build "$ROOT/build_gui" -j$(( $(nproc) > 4 ? 4 : $(nproc) ))

# Solo build richiesta (es. da crea_appimage.sh)
if $ONLY_BUILD; then
    echo "==> Fatto! Binario: $BIN"
    exit 0
fi

# Test opzionali: esegui con ./aggiorna.sh --test
if [[ "$*" == *--test* ]]; then
    echo "==> Build + run test suite (build_tests/)..."
    cmake -B "$ROOT/build_tests" "$ROOT/gui/" -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release -Wno-dev
    cmake --build "$ROOT/build_tests" -j$(( $(nproc) > 4 ? 4 : $(nproc) ))
    ctest --test-dir "$ROOT/build_tests" \
          --exclude-regex "AiIntegration|AiStress|TeamCollab|MultiAgenteLive|AgenteAutonomoLive|ChatLive" \
          -j4 --output-on-failure
    echo "==> Test completati."
fi

echo "==> Installo icona..."
ICON_SRC="$ROOT/prismalux.png"
ICON_DST="$HOME/.local/share/icons/hicolor/256x256/apps/prismalux.png"
if [ -f "$ICON_SRC" ]; then
    mkdir -p "$(dirname "$ICON_DST")"
    cp "$ICON_SRC" "$ICON_DST"
    gtk-update-icon-cache "$HOME/.local/share/icons/hicolor/" 2>/dev/null || true
    echo "    ✓ $ICON_DST"
fi

echo "==> Aggiorno Prismalux.desktop..."
sed -i "s|^Exec=.*|Exec=$BIN|"  "$DESKTOP_SRC"
sed -i "s|^Path=.*|Path=$ROOT|" "$DESKTOP_SRC"
chmod +x "$DESKTOP_SRC"
gio set "$DESKTOP_SRC" metadata::trusted true 2>/dev/null || true
echo "    ✓ $DESKTOP_SRC"

echo "==> Sincronizza in ~/.local/share/applications/..."
cp "$DESKTOP_SRC" "$DESKTOP_SYS"
update-desktop-database "$HOME/.local/share/applications/" 2>/dev/null || true
echo "    ✓ $DESKTOP_SYS"

echo "==> Fatto! Binario: $BIN"

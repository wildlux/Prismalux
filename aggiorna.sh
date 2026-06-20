#!/usr/bin/env bash
# Build Prismalux_GUI + aggiorna file .desktop
# Unica sorgente di verità: Prismalux/Prismalux.desktop
# Non crea mai duplicati fuori dalla cartella del progetto.
set -e
cd "$(dirname "$0")"
ROOT="$(pwd)"
BIN="$ROOT/build_gui/Prismalux_GUI"
DESKTOP_SRC="$ROOT/Prismalux.desktop"
DESKTOP_SYS="$HOME/.local/share/applications/prismalux.desktop"

echo "==> Build Prismalux..."
cmake -B "$ROOT/build_gui" "$ROOT/gui/" -DCMAKE_BUILD_TYPE=Release -Wno-dev -q 2>/dev/null || true
cmake --build "$ROOT/build_gui" -j$(( $(nproc) > 4 ? 4 : $(nproc) ))

# Test opzionali: esegui con ./aggiorna.sh --test
if [[ "$*" == *--test* ]]; then
    echo "==> Build + run test suite (gui/build_tests)..."
    cmake -B "$ROOT/build_tests" "$ROOT/gui/" -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release -Wno-dev
    cmake --build "$ROOT/build_tests" -j$(( $(nproc) > 4 ? 4 : $(nproc) ))
    ctest --test-dir "$ROOT/build_tests" \
          --exclude-regex "AiIntegration|AiStress|TeamCollab|MultiAgenteLive" \
          -j4 --output-on-failure
    echo "==> Test completati."
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

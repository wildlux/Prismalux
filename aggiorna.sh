#!/usr/bin/env bash
# Build Prismalux_GUI + aggiorna tutti i file .desktop
set -e
cd "$(dirname "$0")"
ROOT="$(pwd)"
BIN="$ROOT/gui/build_gui/Prismalux_GUI"

echo "==> Build Prismalux..."
cmake --build gui/build_gui -j$(nproc)

echo "==> Aggiorno file .desktop..."
update_desktop() {
    local f="$1"
    if [ -f "$f" ]; then
        sed -i "s|^Exec=.*|Exec=$BIN|" "$f"
        sed -i "s|^Path=.*|Path=$ROOT|" "$f"
        echo "    ✓ $f"
    fi
}

update_desktop "$ROOT/Prismalux.desktop"
update_desktop "$HOME/.local/share/applications/prismalux.desktop"
update_desktop "$HOME/.local/share/applications/Prismalux.desktop"

chmod +x "$ROOT/Prismalux.desktop" 2>/dev/null || true
gio set "$ROOT/Prismalux.desktop" metadata::trusted true 2>/dev/null || true
update-desktop-database "$HOME/.local/share/applications/" 2>/dev/null || true

echo "==> Fatto! Binario: $BIN"

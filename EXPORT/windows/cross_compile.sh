#!/usr/bin/env bash
# cross_compile.sh — Compila Prismalux_GUI.exe su Linux via mingw-w64
# Uso: bash EXPORT/windows/cross_compile.sh
#
# Cosa fa:
#   1. Verifica mingw-w64 (x86_64-w64-mingw32-g++)
#   2. Scarica Qt6 mingw via aqtinstall se non presente
#   3. cmake configure + build → gui/build_cross_win/Prismalux_GUI.exe
#   4. Copia DLL Qt6 + runtime mingw accanto all'exe
#   5. Copia themes/
# ─────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GUI_DIR="$ROOT/gui"
BUILD_DIR="$ROOT/build_cross_win"
QT_CROSS_DIR="$ROOT/COMPILE_WIN/qt6_cross"
QT_VER="6.8.0"
QT_ARCH="win64_mingw"
QT_PREFIX="$QT_CROSS_DIR/$QT_VER/mingw_64"
TOOLCHAIN="$ROOT/Tools/cmake/toolchain-mingw64.cmake"

C="\033[36m"; G="\033[32m"; Y="\033[33m"; R="\033[31m"; N="\033[0m"; B="\033[1m"
ok()   { echo -e "  ${G}✅${N} $*"; }
warn() { echo -e "  ${Y}⚠${N}  $*"; }
fail() { echo -e "  ${R}❌${N} $*" >&2; exit 1; }
step() { echo -e "\n${B}${C}▶ $*${N}"; }

echo ""
echo -e "${B}════════════════════════════════════════${N}"
echo -e "${B}  Cross-compilazione Windows (mingw-w64)${N}"
echo -e "${B}════════════════════════════════════════${N}"

# ── 1. Verifica mingw-w64 ─────────────────────────────────────
step "Verifico mingw-w64..."
if ! command -v x86_64-w64-mingw32-g++ &>/dev/null; then
    fail "x86_64-w64-mingw32-g++ non trovato.\nInstalla con:\n  sudo apt install mingw-w64"
fi
ok "x86_64-w64-mingw32-g++ $(x86_64-w64-mingw32-g++ --version | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')"

# ── 2. Qt6 mingw — scarica se non presente ────────────────────
step "Qt6 mingw ($QT_VER)..."

# Usa sempre il venv dedicato per aqtinstall (evita PEP 668 / pip rotto)
VENV_DIR="$ROOT/COMPILE_WIN/.venv_tools"
VENV_PY="$VENV_DIR/bin/python3"
VENV_PIP="$VENV_DIR/bin/pip"

if [ ! -x "$VENV_PY" ]; then
    echo -e "  Creo venv strumenti in $VENV_DIR..."
    python3 -m venv "$VENV_DIR" \
        || fail "python3 -m venv fallito. Installa con: sudo apt install python3-venv"
fi
if ! "$VENV_PY" -m aqt --version &>/dev/null 2>&1; then
    echo -e "  Installo aqtinstall nel venv..."
    "$VENV_PIP" install aqtinstall --quiet \
        || fail "Installazione aqtinstall fallita."
fi
ok "aqtinstall pronto: $VENV_PY"

if [ -f "$QT_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]; then
    ok "Qt6 già presente: $QT_PREFIX"
else
    echo -e "  Qt6 non trovato — scarico (~300 MB)..."
    mkdir -p "$QT_CROSS_DIR"
    "$VENV_PY" -m aqt install-qt linux desktop "$QT_VER" "$QT_ARCH" \
        --outputdir "$QT_CROSS_DIR" \
        || fail "Download Qt6 fallito."
    ok "Qt6 scaricato → $QT_PREFIX"
fi

# ── 3. cmake configure ────────────────────────────────────────
step "cmake configure..."
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    ok "Cache cmake già presente — salto configure"
else
    cmake -B "$BUILD_DIR" -S "$GUI_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
        -DCMAKE_BUILD_TYPE=Release \
        -G Ninja 2>&1 | tail -5
    ok "Configure completato"
fi

# ── 4. Compilazione ───────────────────────────────────────────
NPROC=$(( $(nproc) - 1 ))
[ "$NPROC" -lt 1 ] && NPROC=1
step "Compilazione ($NPROC thread)..."
cmake --build "$BUILD_DIR" --config Release -j "$NPROC"

EXE="$BUILD_DIR/Prismalux_GUI.exe"
[ -f "$EXE" ] || fail "Binario non trovato: $EXE"
ok "Compilato → $EXE  ($(du -sh "$EXE" | cut -f1))"

# ── 5. Copia DLL Qt6 ─────────────────────────────────────────
step "Copia DLL Qt6 + runtime mingw..."
QT_BIN="$QT_PREFIX/bin"

QT_DLLS=(
    Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6Network.dll
    Qt6PrintSupport.dll Qt6Sql.dll Qt6Xml.dll Qt6DBus.dll
)
RUNTIME_DLLS=(
    libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll
    libdouble-conversion.dll libpcre2-16-0.dll libzstd.dll
    zlib1.dll libfreetype-6.dll libpng16-16.dll
    libharfbuzz-0.dll libmd4c.dll libbrotlidec.dll libbrotlicommon.dll
    libicudt73.dll libicuin73.dll libicuuc73.dll
)

copied=0
for dll in "${QT_DLLS[@]}" "${RUNTIME_DLLS[@]}"; do
    src="$QT_BIN/$dll"
    [ -f "$src" ] && cp "$src" "$BUILD_DIR/$dll" && (( copied++ )) || true
done

# Plugin platforms/
mkdir -p "$BUILD_DIR/platforms"
for plat in "$QT_PREFIX/plugins/platforms/qwindows.dll" \
             "$QT_PREFIX/plugins/platforms/qdirect2d.dll"; do
    [ -f "$plat" ] && cp "$plat" "$BUILD_DIR/platforms/" || true
done

# Plugin sqldrivers/ (per SQLite)
mkdir -p "$BUILD_DIR/sqldrivers"
[ -f "$QT_PREFIX/plugins/sqldrivers/qsqlite.dll" ] && \
    cp "$QT_PREFIX/plugins/sqldrivers/qsqlite.dll" "$BUILD_DIR/sqldrivers/" || true

ok "$copied DLL copiate"

# ── 6. Temi ───────────────────────────────────────────────────
if [ -d "$GUI_DIR/themes" ] && [ ! -d "$BUILD_DIR/themes" ]; then
    cp -r "$GUI_DIR/themes" "$BUILD_DIR/themes"
    ok "themes/ copiato"
fi

# ── Risultato ─────────────────────────────────────────────────
echo ""
echo -e "${B}${G}════════════════════════════════════════${N}"
echo -e "${B}${G}  Cross-compilazione completata!${N}"
echo -e "${B}${G}════════════════════════════════════════${N}"
echo -e "  Exe     : $EXE"
echo -e "  DLL     : $BUILD_DIR/"
echo -e ""
echo -e "  Ora esegui: python3 EXPORT/windows/crea_zip_windows.py"
echo -e "  Lo ZIP includerà automaticamente il binario compilato."
echo ""

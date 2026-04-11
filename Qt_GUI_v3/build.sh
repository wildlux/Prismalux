#!/bin/bash
# ══════════════════════════════════════════════════════════════
#  Prismalux GUI — Build script universale (Linux + MSYS2)
# ══════════════════════════════════════════════════════════════
set -e

BUILD_DIR="build"

# ── Rilevamento ambiente ──────────────────────────────────────
if [ -d "/mingw64/lib/cmake/Qt6" ]; then
    # MSYS2 MINGW64
    CMAKE_EXTRA="-DCMAKE_PREFIX_PATH=/mingw64 -G 'MinGW Makefiles'"
    echo "Ambiente: MSYS2 MINGW64"
elif [ -d "/c/Qt" ]; then
    # Qt installer su Windows (Git Bash / MSYS2)
    QT_VER=$(ls /c/Qt/ | grep '^6\.' | sort -V | tail -1)
    CMAKE_EXTRA="-DCMAKE_PREFIX_PATH=/c/Qt/$QT_VER/mingw_64"
    echo "Qt trovato in: /c/Qt/$QT_VER/mingw_64"
else
    # Linux nativo
    CMAKE_EXTRA=""
    echo "Ambiente: Linux"
fi

echo "Configuro progetto..."
eval cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release $CMAKE_EXTRA

echo "Compilo..."
cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"

echo ""
echo "✅  Compilazione completata!"
echo "   Eseguibile: $BUILD_DIR/Prismalux_GUI"
echo ""
echo "Per avviare:"
echo "   ./$BUILD_DIR/Prismalux_GUI"

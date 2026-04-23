#!/bin/bash
# ══════════════════════════════════════════════════════════════
#  Prismalux GUI v2.2 — Build script universale (Linux + MSYS2)
#  Aggiornato: 2026-03-28
#  Novità v2.2: grafico_page — SmithPrime (idx 7) + MathConst
#               pi-e-Primi (idx 8) nel Diagramma di Smith
# ══════════════════════════════════════════════════════════════
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="build"

echo ""
echo "🍺  Prismalux GUI v2.2 — Build"
echo "══════════════════════════════════════════"

# ── Rilevamento ambiente ──────────────────────────────────────
IS_MSYS2=0
IS_LINUX=0
CMAKE_EXTRA=""

if [ -d "/mingw64/lib/cmake/Qt6" ]; then
    IS_MSYS2=1
    CMAKE_EXTRA="-DCMAKE_PREFIX_PATH=/mingw64 -G 'MinGW Makefiles'"
    echo "  Ambiente : MSYS2 MINGW64"
elif [ -d "/ucrt64/lib/cmake/Qt6" ]; then
    IS_MSYS2=1
    CMAKE_EXTRA="-DCMAKE_PREFIX_PATH=/ucrt64 -G 'MinGW Makefiles'"
    echo "  Ambiente : MSYS2 UCRT64"
elif [ -d "/c/Qt" ]; then
    IS_MSYS2=1
    QT_VER=$(ls /c/Qt/ 2>/dev/null | grep '^6\.' | sort -V | tail -1)
    if [ -n "$QT_VER" ]; then
        CMAKE_EXTRA="-DCMAKE_PREFIX_PATH=/c/Qt/$QT_VER/mingw_64"
        echo "  Ambiente : Windows Qt installer  (Qt $QT_VER)"
    else
        echo "  [AVVISO] Qt non trovato in /c/Qt — proseguo senza prefix path"
    fi
else
    IS_LINUX=1
    echo "  Ambiente : Linux"
fi

# ── Controllo dipendenze (solo Linux) ────────────────────────
if [ "$IS_LINUX" -eq 1 ]; then
    echo ""
    echo "  Controllo dipendenze..."

    MISSING_PKGS=""

    # cmake
    if ! command -v cmake &>/dev/null; then
        MISSING_PKGS="$MISSING_PKGS cmake"
    else
        CMAKE_VER=$(cmake --version | head -1 | awk '{print $3}')
        echo "  ✓ cmake $CMAKE_VER"
    fi

    # g++ (per AUTOMOC e compilazione C++)
    if ! command -v g++ &>/dev/null; then
        MISSING_PKGS="$MISSING_PKGS g++"
    else
        GPP_VER=$(g++ --version | head -1 | awk '{print $NF}')
        echo "  ✓ g++ $GPP_VER"
    fi

    # Qt6: cerca pkg-config o cmake config
    QT6_OK=0
    if pkg-config --exists Qt6Core 2>/dev/null; then
        QT6_VER=$(pkg-config --modversion Qt6Core 2>/dev/null)
        echo "  ✓ Qt6 $QT6_VER  (pkg-config)"
        QT6_OK=1
    elif [ -d /usr/lib/x86_64-linux-gnu/cmake/Qt6 ] || \
         [ -d /usr/lib/cmake/Qt6 ] || \
         [ -d /usr/local/lib/cmake/Qt6 ]; then
        echo "  ✓ Qt6  (cmake config trovato)"
        QT6_OK=1
    fi

    if [ "$QT6_OK" -eq 0 ]; then
        echo "  ✗ Qt6 non trovato"
        MISSING_PKGS="$MISSING_PKGS qt6-base-dev"
    fi

    if [ -n "$MISSING_PKGS" ]; then
        echo ""
        echo "  ❌  Dipendenze mancanti: $MISSING_PKGS"
        echo ""
        echo "  Installa con:"
        # Debian/Ubuntu
        if command -v apt-get &>/dev/null; then
            echo "    sudo apt-get install cmake g++ qt6-base-dev qt6-tools-dev"
            echo "    # oppure Qt5: sudo apt-get install qt5-default"
        # Fedora/RHEL
        elif command -v dnf &>/dev/null; then
            echo "    sudo dnf install cmake gcc-c++ qt6-qtbase-devel"
        # Arch
        elif command -v pacman &>/dev/null; then
            echo "    sudo pacman -S cmake gcc qt6-base"
        fi
        echo ""
        exit 1
    fi
fi

# ── Configura ─────────────────────────────────────────────────
echo ""
echo "  Configuro progetto..."
eval cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    $CMAKE_EXTRA

# ── Compila ──────────────────────────────────────────────────
echo ""
echo "  Compilo ($(nproc 2>/dev/null || echo 4) thread)..."
cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"

# ── Risultato ─────────────────────────────────────────────────
echo ""
echo "══════════════════════════════════════════"
echo "✅  Prismalux GUI v2.2 — Build completato!"
echo ""
echo "  Eseguibile : $BUILD_DIR/Prismalux_GUI"
BIN="$BUILD_DIR/Prismalux_GUI"
if [ -f "$BIN" ]; then
    BIN_SIZE=$(du -sh "$BIN" 2>/dev/null | cut -f1)
    echo "  Dimensione : $BIN_SIZE"
fi
echo ""
echo "  Per avviare:"
echo "    ./$BUILD_DIR/Prismalux_GUI"
echo ""
echo "  Novità v2.2 — Grafico:"
echo "    [7] Smith — Numeri Primi (reali + gaussiani)"
echo "    [8] Smith — pi * e * Primi (serie convergenza)"
echo ""

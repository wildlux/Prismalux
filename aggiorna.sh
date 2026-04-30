#!/bin/bash
# ══════════════════════════════════════════════════════════════
#  aggiorna.sh — Ricompila Prismalux GUI Qt6 — Linux, macOS, Windows (MSYS2/Git Bash)
#
#  Uso:
#    ./aggiorna.sh              # GUI + ZIP Windows + AppImage (Linux only)
#    ./aggiorna.sh --gui        # solo GUI Qt6
#    ./aggiorna.sh --zip        # solo ZIP Windows (richiede Python3)
#    ./aggiorna.sh --appimage   # solo AppImage Linux (Linux only)
#    ./aggiorna.sh --no-zip     # GUI + AppImage, salta ZIP
#    ./aggiorna.sh --no-appimage# GUI + ZIP, salta AppImage
#    ./aggiorna.sh --whisper    # solo download binario whisper-cli.exe
#    ./aggiorna.sh --no-whisper # salta download binario whisper-cli.exe
#    ./aggiorna.sh --build-whisper  # compila whisper.cpp da sorgente
#    ./aggiorna.sh --llama-studio   # compila llama-server + llama-cli
#
#  Su Windows: lancia da "MSYS2 UCRT64" o "Git Bash"
#    bash aggiorna.sh [opzioni]
# ══════════════════════════════════════════════════════════════
set -euo pipefail

# ── Colori ─────────────────────────────────────────────────────
R='\033[0;31m'; G='\033[0;32m'; Y='\033[0;33m'
C='\033[0;36m'; B='\033[1;37m'; N='\033[0m'

# ── Rilevamento piattaforma ─────────────────────────────────────
_UNAME="$(uname -s 2>/dev/null || echo Unknown)"
case "$_UNAME" in
    MINGW*|MSYS*|CYGWIN*)  IS_WIN=1; IS_MAC=0 ;;
    Darwin*)               IS_WIN=0; IS_MAC=1 ;;
    *)                     IS_WIN=0; IS_MAC=0 ;;
esac

if [ "$IS_WIN" = "1" ]; then
    echo -e "${C}▶ Piattaforma rilevata: Windows (${_UNAME})${N}"
elif [ "$IS_MAC" = "1" ]; then
    echo -e "${C}▶ Piattaforma rilevata: macOS${N}"
else
    echo -e "${C}▶ Piattaforma rilevata: Linux${N}"
fi

# ── Percorsi ───────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_GUI="$SCRIPT_DIR/gui"
ZIP_SCRIPT="$SCRIPT_DIR/scripts/crea_zip_windows.py"
APPIMAGE_SCRIPT="$SCRIPT_DIR/scripts/crea_appimage.sh"
APPIMAGE_OUT="$SCRIPT_DIR/Prismalux-x86_64.AppImage"
WHISPER_WIN_DIR="$SCRIPT_DIR/whisper_win"

# ── Adattamenti per OS ──────────────────────────────────────────
if [ "$IS_WIN" = "1" ]; then
    QT_BUILD="$QT_GUI/build_win"
    EXE_EXT=".exe"
    NPROC="${NUMBER_OF_PROCESSORS:-4}"
else
    QT_BUILD="$QT_GUI/build"
    EXE_EXT=""
    NPROC="$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"
fi
GUI_BIN="$QT_BUILD/Prismalux_GUI${EXE_EXT}"

# ── Rilevamento Qt6 su Windows (MSYS2) ─────────────────────────
CMAKE_EXTRA=""
if [ "$IS_WIN" = "1" ]; then
    _found_qt=""
    for _qt_prefix in /ucrt64 /mingw64 /mingw32; do
        if [ -f "${_qt_prefix}/lib/cmake/Qt6/Qt6Config.cmake" ]; then
            _found_qt="$_qt_prefix"
            break
        fi
    done
    if [ -n "$_found_qt" ]; then
        CMAKE_EXTRA="-DCMAKE_PREFIX_PATH=$_found_qt"
        # Usa Ninja se disponibile (molto più veloce di make su Windows)
        if command -v ninja &>/dev/null; then
            CMAKE_EXTRA="$CMAKE_EXTRA -G Ninja"
        fi
        echo -e "  ${C}Qt6 rilevato: $_found_qt${N}"
    else
        echo -e "${Y}⚠  Qt6 non trovato in /ucrt64 o /mingw64.${N}"
        echo -e "${Y}   Installa con: pacman -S mingw-w64-ucrt-x86_64-qt6-base${N}"
        echo -e "${Y}   oppure usa build.bat (rileva automaticamente).${N}"
    fi
fi

# ── Versione letta da CMakeLists.txt (fonte unica di verità) ───
PRISMA_VERSION=$(grep -m1 'project(Prismalux_GUI VERSION' "$QT_GUI/CMakeLists.txt" \
                 | grep -oE '[0-9]+\.[0-9]+' | head -1)
PRISMA_VERSION="${PRISMA_VERSION:-2.8}"
ZIP_OUT="$SCRIPT_DIR/Prismalux_v${PRISMA_VERSION}_Windows.zip"

# ── Flags ──────────────────────────────────────────────────────
DO_GUI=1
DO_ZIP=1
DO_APPIMAGE=1
DO_WHISPER_WIN=0
DO_BUILD_WHISPER=0
DO_LLAMA_STUDIO=0

# AppImage non supportata su Windows e macOS
[ "$IS_WIN" = "1" ] && DO_APPIMAGE=0
[ "$IS_MAC" = "1" ] && DO_APPIMAGE=0

for arg in "$@"; do
    case "$arg" in
        --gui)           DO_GUI=1; DO_ZIP=0; DO_APPIMAGE=0; DO_WHISPER_WIN=0 ;;
        --zip)           DO_GUI=0; DO_ZIP=1; DO_APPIMAGE=0 ;;
        --appimage)      DO_GUI=0; DO_ZIP=0; DO_APPIMAGE=1 ;;
        --whisper)       DO_GUI=0; DO_ZIP=0; DO_APPIMAGE=0; DO_WHISPER_WIN=1 ;;
        --build-whisper) DO_BUILD_WHISPER=1 ;;
        --llama-studio)  DO_LLAMA_STUDIO=1 ;;
        --no-zip)        DO_ZIP=0 ;;
        --no-appimage)   DO_APPIMAGE=0 ;;
        --no-whisper)    DO_WHISPER_WIN=0 ;;
        -h|--help)
            echo "Uso: $0 [--gui|--zip|--appimage|--no-zip|--no-appimage"
            echo "         --whisper|--build-whisper|--llama-studio|--no-whisper]"
            echo ""
            echo "  --build-whisper  Compila whisper.cpp da sorgente"
            echo "  --llama-studio   Compila llama-server + llama-cli"
            echo ""
            echo "  Su Windows (MSYS2/Git Bash): bash aggiorna.sh [opzioni]"
            exit 0 ;;
        *) echo -e "${R}Opzione sconosciuta: $arg${N}"; exit 1 ;;
    esac
done

# ── Helper ─────────────────────────────────────────────────────
step() { echo -e "\n${C}▶ $*${N}"; }
ok()   { echo -e "${G}✅ $*${N}"; }
fail() { echo -e "${R}❌ $*${N}"; exit 1; }

T_START=$(date +%s)

# ══════════════════════════════════════════════════════════════
#  1. GUI Qt6
# ══════════════════════════════════════════════════════════════
if [ "$DO_GUI" = "1" ]; then
    step "Compilo GUI Qt6 (${_UNAME})..."
    cd "$QT_GUI"

    if [ ! -f "$QT_BUILD/CMakeCache.txt" ]; then
        step "  Prima configurazione cmake..."
        # shellcheck disable=SC2086
        cmake -B "$QT_BUILD" -DCMAKE_BUILD_TYPE=Release $CMAKE_EXTRA \
            2>&1 | grep -E "(CMAKE|error:|Configuring|fatal)" || true
    fi

    # shellcheck disable=SC2086
    cmake --build "$QT_BUILD" -j"$NPROC" 2>&1 \
        | grep -E "(Building|Linking|error:|warning:|Built target)" || true

    [ -f "$GUI_BIN" ] || fail "Binario GUI non trovato: $GUI_BIN"
    ok "GUI compilata → $GUI_BIN"

    # Desktop entry solo su Linux
    if [ "$IS_WIN" = "0" ] && [ "$IS_MAC" = "0" ]; then
        DESKTOP_OUT="$SCRIPT_DIR/Prismalux.desktop"
        cat > "$DESKTOP_OUT" <<DESKTOP_EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Prismalux
GenericName=Centro di Controllo AI
Comment=Pipeline Agenti, Tutor AI, Strumenti Pratici
Exec=$GUI_BIN
Icon=prismalux
Terminal=false
Categories=Education;Science;Utility;
Keywords=AI;agenti;matematica;tutor;ollama;
StartupNotify=true
StartupWMClass=Prismalux_GUI
DESKTOP_EOF
        chmod +x "$DESKTOP_OUT"
        ok "Prismalux.desktop aggiornato → $DESKTOP_OUT"
    fi
fi

# ══════════════════════════════════════════════════════════════
#  1b. whisper.cpp — compilazione da sorgente
# ══════════════════════════════════════════════════════════════
if [ "$DO_BUILD_WHISPER" = "1" ]; then
    WHISPER_SRC="$SCRIPT_DIR/whisper.cpp"
    WHISPER_CLI="$WHISPER_SRC/build/bin/whisper-cli${EXE_EXT}"

    if [ -f "$WHISPER_CLI" ]; then
        ok "whisper-cli già presente → $WHISPER_CLI"
    else
        step "Compilo whisper.cpp da sorgente..."

        if [ ! -d "$WHISPER_SRC/.git" ]; then
            echo -e "  ${C}Clone whisper.cpp...${N}"
            git clone --depth=1 https://github.com/ggml-org/whisper.cpp "$WHISPER_SRC"
        else
            echo -e "  ${C}Aggiorno sorgenti whisper.cpp...${N}"
            git -C "$WHISPER_SRC" pull --depth=1 --rebase 2>/dev/null || true
        fi

        # shellcheck disable=SC2086
        cmake -B "$WHISPER_SRC/build" -S "$WHISPER_SRC" \
            -DCMAKE_BUILD_TYPE=Release \
            -DWHISPER_BUILD_TESTS=OFF \
            -DWHISPER_BUILD_EXAMPLES=ON \
            -DBUILD_SHARED_LIBS=OFF \
            $CMAKE_EXTRA \
            2>&1 | grep -E "(CMAKE|error:|Configuring|fatal)" || true

        cmake --build "$WHISPER_SRC/build" --target whisper-cli -j"$NPROC" \
            2>&1 | grep -E "(Building|Linking|error:|Built target|fatal)" || true

        [ -f "$WHISPER_CLI" ] || fail "whisper-cli non trovato dopo la build"
        ok "whisper-cli compilato → $WHISPER_CLI"
    fi
fi

# ══════════════════════════════════════════════════════════════
#  1c. llama.cpp Studio — compila llama-server + llama-cli
# ══════════════════════════════════════════════════════════════
if [ "$DO_LLAMA_STUDIO" = "1" ]; then
    LLAMA_STUDIO_DIR="$SCRIPT_DIR/llama_cpp_studio"
    LLAMA_SRC="$LLAMA_STUDIO_DIR/llama.cpp"
    LLAMA_SERVER="$LLAMA_SRC/build/bin/llama-server${EXE_EXT}"
    LLAMA_CLI="$LLAMA_SRC/build/bin/llama-cli${EXE_EXT}"

    if [ -f "$LLAMA_SERVER" ] && [ -f "$LLAMA_CLI" ]; then
        ok "llama-server + llama-cli già presenti → $LLAMA_SRC/build/bin/"
    else
        step "Compilo llama.cpp Studio (llama-server + llama-cli)..."

        CMAKE_GPU_FLAGS=""
        if [ "$IS_WIN" = "0" ]; then
            command -v nvidia-smi &>/dev/null && CMAKE_GPU_FLAGS="-DGGML_CUDA=ON"
            { [ -d /opt/rocm ] || command -v rocm-smi &>/dev/null; } \
                && CMAKE_GPU_FLAGS="$CMAKE_GPU_FLAGS -DGGML_HIPBLAS=ON"
            command -v vulkaninfo &>/dev/null && CMAKE_GPU_FLAGS="$CMAKE_GPU_FLAGS -DGGML_VULKAN=ON"
        fi
        [ -z "$CMAKE_GPU_FLAGS" ] && CMAKE_GPU_FLAGS="-DGGML_OPENMP=ON"

        if [ ! -d "$LLAMA_SRC/.git" ]; then
            mkdir -p "$LLAMA_STUDIO_DIR"
            echo -e "  ${C}Clone llama.cpp...${N}"
            git clone --depth=1 https://github.com/ggml-org/llama.cpp "$LLAMA_SRC"
        fi

        # shellcheck disable=SC2086
        cmake -B "$LLAMA_SRC/build" -S "$LLAMA_SRC" \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_SHARED_LIBS=OFF \
            -DGGML_NATIVE=ON \
            -DLLAMA_BUILD_TESTS=OFF \
            -DLLAMA_BUILD_EXAMPLES=ON \
            $CMAKE_GPU_FLAGS \
            $CMAKE_EXTRA \
            2>&1 | grep -E "(CMAKE|error:|Configuring|fatal)" || true

        cmake --build "$LLAMA_SRC/build" \
            --target llama-server llama-cli -j"$NPROC" \
            2>&1 | grep -E "(Building|Linking|error:|Built target|fatal)" || true

        [ -f "$LLAMA_SERVER" ] || fail "llama-server non trovato dopo la build"
        ok "llama-server → $LLAMA_SERVER"
        ok "llama-cli    → $LLAMA_CLI"
    fi
fi

# ══════════════════════════════════════════════════════════════
#  2. whisper-cli.exe precompilato per Windows (GitHub Releases)
# ══════════════════════════════════════════════════════════════
if [ "$DO_WHISPER_WIN" = "1" ]; then
    step "Scarico whisper Windows precompilato..."

    if ! command -v curl &>/dev/null; then
        echo -e "${Y}⚠  curl non trovato — salto download whisper Windows.${N}"
        DO_WHISPER_WIN=0
    else
        mkdir -p "$WHISPER_WIN_DIR"
        WHISPER_EXE="$WHISPER_WIN_DIR/whisper-cli.exe"

        echo -e "  ${C}Interrogo GitHub API...${N}"

        _whisper_fetch_url() {
            local assets
            assets=$(curl -fsSL "https://api.github.com/repos/$1/releases/latest" 2>/dev/null \
                | grep '"browser_download_url"' | grep -iE '\.zip' | grep -o 'https://[^"]*')
            echo "$assets" | grep -i 'blas-bin-x64'  | head -1 && return
            echo "$assets" | grep -i 'bin-x64'        | head -1 && return
            echo "$assets" | grep -iE 'win32|Win32'   | grep -v blas | head -1 && return
        }

        WHISPER_ZIP_URL=$(_whisper_fetch_url "ggml-org/whisper.cpp")
        [ -z "$WHISPER_ZIP_URL" ] && WHISPER_ZIP_URL=$(_whisper_fetch_url "ggerganov/whisper.cpp")

        if [ -z "$WHISPER_ZIP_URL" ]; then
            echo -e "${Y}⚠  Impossibile trovare il release Windows — riprova con: ./aggiorna.sh --whisper${N}"
            DO_WHISPER_WIN=0
        else
            echo -e "  Release: ${C}$(basename "$WHISPER_ZIP_URL")${N}"
            TMP_ZIP="$(mktemp /tmp/whisper_win_XXXXXX.zip)"
            curl -fsSL --progress-bar -o "$TMP_ZIP" "$WHISPER_ZIP_URL" \
                || { rm -f "$TMP_ZIP"; DO_WHISPER_WIN=0; }

            if [ "$DO_WHISPER_WIN" = "1" ]; then
                if command -v unzip &>/dev/null; then
                    TMP_DIR="$(mktemp -d /tmp/whisper_win_dir_XXXXXX)"
                    unzip -q "$TMP_ZIP" -d "$TMP_DIR" 2>/dev/null || true
                    find "$TMP_DIR" -maxdepth 3 -type f \( -iname "*.exe" -o -iname "*.dll" \) \
                        -exec cp {} "$WHISPER_WIN_DIR/" \;
                    rm -rf "$TMP_DIR"
                fi
                rm -f "$TMP_ZIP"

                N_FILES=$(ls "$WHISPER_WIN_DIR/" 2>/dev/null | wc -l)
                if [ -f "$WHISPER_EXE" ] && [ -s "$WHISPER_EXE" ]; then
                    ok "whisper_win/ → $N_FILES file ($(du -sh "$WHISPER_WIN_DIR" | cut -f1))"
                else
                    echo -e "${Y}⚠  whisper-cli.exe non trovato nell'archivio.${N}"
                fi
            fi
        fi
    fi
fi

# ══════════════════════════════════════════════════════════════
#  3. ZIP Windows (richiede Python3 — disponibile su Linux e MSYS2)
# ══════════════════════════════════════════════════════════════
if [ "$DO_ZIP" = "1" ]; then
    step "Rigenero ZIP Windows..."
    cd "$SCRIPT_DIR"

    # Trova Python3 (su Windows MSYS2 può chiamarsi "python" o "python3")
    _PYTHON=""
    for _py in python3 python py; do
        if command -v "$_py" &>/dev/null; then
            _PYTHON="$_py"
            break
        fi
    done

    if [ -z "$_PYTHON" ]; then
        echo -e "${Y}⚠  Python non trovato — salto ZIP Windows.${N}"
        DO_ZIP=0
    else
        WHISPER_EXE="$WHISPER_WIN_DIR/whisper-cli.exe"
        if [ ! -f "$WHISPER_EXE" ] && command -v curl &>/dev/null; then
            step "  whisper_win/ vuota — scarico whisper-cli.exe automaticamente..."

            _whisper_fetch_url2() {
                local assets
                assets=$(curl -fsSL "https://api.github.com/repos/$1/releases/latest" 2>/dev/null \
                    | grep '"browser_download_url"' | grep -iE '\.zip' | grep -o 'https://[^"]*')
                echo "$assets" | grep -i 'blas-bin-x64' | head -1 && return
                echo "$assets" | grep -i 'bin-x64'       | head -1 && return
                echo "$assets" | grep -iE 'win32|Win32'  | grep -v blas | head -1 && return
            }

            WHISPER_ZIP_URL=$(_whisper_fetch_url2 "ggml-org/whisper.cpp")
            [ -z "$WHISPER_ZIP_URL" ] && WHISPER_ZIP_URL=$(_whisper_fetch_url2 "ggerganov/whisper.cpp")

            if [ -n "$WHISPER_ZIP_URL" ]; then
                echo -e "  Release: ${C}$(basename "$WHISPER_ZIP_URL")${N}"
                mkdir -p "$WHISPER_WIN_DIR"
                TMP_ZIP="$(mktemp /tmp/whisper_win_XXXXXX.zip)"
                if curl -fsSL --progress-bar -o "$TMP_ZIP" "$WHISPER_ZIP_URL" 2>/dev/null; then
                    if command -v unzip &>/dev/null; then
                        TMP_DIR="$(mktemp -d /tmp/whisper_win_dir2_XXXXXX)"
                        unzip -q "$TMP_ZIP" -d "$TMP_DIR" 2>/dev/null || true
                        find "$TMP_DIR" -maxdepth 3 -type f \( -iname "*.exe" -o -iname "*.dll" \) \
                            -exec cp {} "$WHISPER_WIN_DIR/" \;
                        rm -rf "$TMP_DIR"
                    fi
                fi
                rm -f "$TMP_ZIP"
                if [ -f "$WHISPER_EXE" ] && [ -s "$WHISPER_EXE" ]; then
                    ok "  whisper_win/ scaricato ($(du -sh "$WHISPER_WIN_DIR" | cut -f1))"
                else
                    echo -e "${Y}  ⚠  whisper-cli.exe non trovato nel release — ZIP senza whisper.${N}"
                fi
            else
                echo -e "${Y}  ⚠  GitHub API non raggiungibile — ZIP senza whisper.${N}"
            fi
        elif [ -f "$WHISPER_EXE" ]; then
            echo -e "  whisper-cli.exe già presente ($(du -sh "$WHISPER_EXE" | cut -f1))"
        fi

        [ -f "$ZIP_SCRIPT" ] || fail "Script ZIP non trovato: $ZIP_SCRIPT"
        "$_PYTHON" "$ZIP_SCRIPT" --out "$ZIP_OUT" \
            2>&1 | grep -E "(Fatto|file|ZIP|\+|skip|errore)" || true
        [ -f "$ZIP_OUT" ] || fail "ZIP non generato"
        ok "ZIP aggiornato → $ZIP_OUT  ($(du -sh "$ZIP_OUT" | cut -f1))"
    fi
fi

# ══════════════════════════════════════════════════════════════
#  4. AppImage Linux (solo Linux)
# ══════════════════════════════════════════════════════════════
if [ "$DO_APPIMAGE" = "1" ]; then
    if [ "$IS_WIN" = "1" ] || [ "$IS_MAC" = "1" ]; then
        echo -e "${Y}⚠  AppImage disponibile solo su Linux — saltato.${N}"
    else
        step "Genero AppImage Linux..."
        cd "$SCRIPT_DIR"
        [ -f "$APPIMAGE_SCRIPT" ] || fail "Script AppImage non trovato: $APPIMAGE_SCRIPT"
        [ -x "$APPIMAGE_SCRIPT" ] || chmod +x "$APPIMAGE_SCRIPT"
        "$APPIMAGE_SCRIPT" --no-build 2>&1 \
            | grep -E "(✅|❌|AppImage|Success|Genero)" || true
        [ -f "$APPIMAGE_OUT" ] || fail "AppImage non generata: $APPIMAGE_OUT"
        ok "AppImage aggiornata → $APPIMAGE_OUT  ($(du -sh "$APPIMAGE_OUT" | cut -f1))"
    fi
fi

# ══════════════════════════════════════════════════════════════
#  Riepilogo
# ══════════════════════════════════════════════════════════════
T_END=$(date +%s)
echo ""
echo -e "${B}════════════════════════════════════════${N}"
echo -e "${B}  Prismalux v${PRISMA_VERSION} — aggiornamento completato  ${N}"
echo -e "${B}════════════════════════════════════════${N}"
[ "$DO_GUI"           = "1" ] && [ -f "$GUI_BIN" ] && \
    echo -e "  ${G}GUI${N}           $GUI_BIN"
[ "$DO_BUILD_WHISPER" = "1" ] && [ -f "$SCRIPT_DIR/whisper.cpp/build/bin/whisper-cli${EXE_EXT}" ] && \
    echo -e "  ${G}whisper-cli${N}   $SCRIPT_DIR/whisper.cpp/build/bin/whisper-cli${EXE_EXT}"
[ "$DO_LLAMA_STUDIO"  = "1" ] && [ -f "$SCRIPT_DIR/llama_cpp_studio/llama.cpp/build/bin/llama-server${EXE_EXT}" ] && \
    echo -e "  ${G}llama-studio${N}  $SCRIPT_DIR/llama_cpp_studio/llama.cpp/build/bin/"
[ "$DO_ZIP"           = "1" ] && [ -f "$ZIP_OUT" ]      && echo -e "  ${G}ZIP${N}           $ZIP_OUT"
[ "$DO_APPIMAGE"      = "1" ] && [ -f "$APPIMAGE_OUT" ] && echo -e "  ${G}AppImage${N}      $APPIMAGE_OUT"
[ "$DO_WHISPER_WIN"   = "1" ] && [ -f "$WHISPER_WIN_DIR/whisper-cli.exe" ] && \
    echo -e "  ${G}Whisper WIN${N}   $WHISPER_WIN_DIR/whisper-cli.exe"
echo -e "  ${Y}Tempo: $(( T_END - T_START ))s${N}"
echo ""

#!/bin/bash
# ══════════════════════════════════════════════════════════════
#  aggiorna.sh — Ricompila Prismalux GUI Qt6 — Linux, macOS, Windows (MSYS2/Git Bash)
#  Versione: si legge da gui/CMakeLists.txt (attualmente v2.9)
#
#  Novità v2.9 (riepilogo sessioni):
#    - Scheda TFR dedicata (Finanza): C.F. calcolato automaticamente (algoritmo
#      D.M. 23/12/1976), lookup ~150 comuni/paesi, RAG auto-fill
#    - WAN Calcolo Distribuito: server/client TCP:11600, 28 tipi task,
#      cron scheduler, dispatcher universale (AI/shell/Python/Git/Graphviz…)
#    - Analisi Fenomeni: allegati PDF/TXT/MD/CSV con pdftotext
#    - Ollama MCP (18°): cache SQLite modelli, 5 tool, solo stdlib Python
#    - Quiz CCNA Android: 64 → 209 domande in 15 temi (CCNA 200-301)
#    - BT Android: toggle 🔓/🔒 per modalità chiaro/cifrato (default: chiaro)
#    - DPI HiDPI/Wayland: dpiScale() in tutte le pages
#    - i18n: QTranslator in main.cpp, stub .ts in gui/i18n/
#    - LaTeX KaTeX: rendering formule in Analisi 1/2 (QWebEngineView + KaTeX)
#      widget LatexView + pannello 🔬 output AI; prompt AI aggiornati
#    - Randomizer 🔀 Risolvi Passi: 52 formule categorizzate (test CAT-E inclusi)
#    - Test SymPy CAT-E: 15 test complessi (62/62 PASS)
#    - Donazione PayPal: README, .github/FUNDING.yml, app desktop, APK Android
#    - TTS + STT: web tab "Voce" (SpeechSynthesis + MediaRecorder→Whisper),
#      APK Android QTextToSpeech + fix STT upload
#    - FEAT-1 Multi-Agente [tab 9]: GraphMemory SQLite, MasterAgent→JSON→sub-agenti
#      con depends_on, sintesi finale, export DOT/JSON/TXT
#    - FEAT-2 Grafo RAG [Ricerca tab]: RagGraph LLM→entità+relazioni,
#      visualizzazione Graphviz, click-nodo dettaglio, filtro live
#
#  Uso:
#    ./aggiorna.sh              # GUI + AppImage + ZIP Windows + ZIP Sorgenti Linux (Linux) / GUI + ZIP (Windows)
#    ./aggiorna.sh --gui        # solo GUI Qt6
#    ./aggiorna.sh --zip        # solo ZIP Windows (richiede Python3)
#    ./aggiorna.sh --zip-linux  # solo ZIP Sorgenti Linux (include Prismalux.desktop)
#    ./aggiorna.sh --appimage   # solo AppImage Linux (Linux only)
#    ./aggiorna.sh --no-zip     # GUI + AppImage + ZIP Linux, salta ZIP Windows
#    ./aggiorna.sh --no-zip-linux # GUI + AppImage + ZIP Windows, salta ZIP Sorgenti Linux
#    ./aggiorna.sh --no-appimage# GUI + ZIP, salta AppImage
#    ./aggiorna.sh --app        # GUI + .app bundle macOS (macOS only)
#    ./aggiorna.sh --no-app     # salta .app bundle macOS
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
ZIP_SCRIPT="$SCRIPT_DIR/EXPORT/windows/crea_zip_windows.py"
APPIMAGE_SCRIPT="$SCRIPT_DIR/EXPORT/linux/crea_appimage.sh"
APPIMAGE_OUT="$SCRIPT_DIR/EXPORT/linux/Prismalux-x86_64.AppImage"
APP_BUNDLE_OUT="$SCRIPT_DIR/Prismalux.app"
WHISPER_WIN_DIR="$SCRIPT_DIR/whisper_win"

# ── Adattamenti per OS ──────────────────────────────────────────
if [ "$IS_WIN" = "1" ]; then
    QT_BUILD="$QT_GUI/build_win"
    EXE_EXT=".exe"
    _RAW_NPROC="${NUMBER_OF_PROCESSORS:-2}"
else
    # Usa build_gui nella root del progetto (directory di build standard)
    QT_BUILD="$SCRIPT_DIR/build_gui"
    EXE_EXT=""
    _RAW_NPROC="$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 2)"
fi
# Core - 1 per mantenere il sistema reattivo durante la build (minimo 1)
NPROC=$(( _RAW_NPROC > 1 ? _RAW_NPROC - 1 : 1 ))
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
ZIP_LINUX_OUT="$SCRIPT_DIR/Prismalux_v${PRISMA_VERSION}_Sorgenti_Linux.zip"

# ── Flags ──────────────────────────────────────────────────────
DO_GUI=1
DO_WEB=0          # ricompila GUI + mostra URL web app
DO_ZIP=0          # default OFF — Windows lo attiva automaticamente
DO_ZIP_LINUX=0    # default OFF — Linux lo attiva automaticamente
DO_APPIMAGE=1     # default ON  — Windows/macOS lo disattivano
DO_APP_BUNDLE=0   # default OFF — macOS lo attiva automaticamente
DO_WHISPER_WIN=0
DO_BUILD_WHISPER=0
DO_LLAMA_STUDIO=0
DO_WIN_CROSS=0     # cross-compila .exe Windows da Linux (richiede mingw-w64 + Qt6 mingw)
DO_INSTALL=0       # installa Prismalux nel sistema Linux dopo la build

# ── Default per piattaforma ─────────────────────────────────────
#   Linux  → AppImage ON,  ZIP ON   (utile per distribuire a utenti Windows)
#   Windows→ ZIP ON,       AppImage OFF
#   macOS  → .app bundle ON, né ZIP né AppImage
if [ "$IS_WIN" = "1" ]; then
    DO_ZIP=1
    DO_APPIMAGE=0
elif [ "$IS_MAC" = "1" ]; then
    DO_APPIMAGE=0
    DO_APP_BUNDLE=1
else
    DO_ZIP=1         # Linux: crea anche ZIP Windows
    DO_ZIP_LINUX=1   # Linux: crea anche ZIP Sorgenti Linux
fi

for arg in "$@"; do
    case "$arg" in
        --gui)           DO_GUI=1; DO_ZIP=0; DO_APPIMAGE=0; DO_APP_BUNDLE=0; DO_WEB=0; DO_WHISPER_WIN=0 ;;
        --web)           DO_GUI=1; DO_ZIP=0; DO_APPIMAGE=0; DO_APP_BUNDLE=0; DO_WEB=1; DO_WHISPER_WIN=0 ;;
        --bat|--zip)     DO_GUI=0; DO_ZIP=1; DO_APPIMAGE=0; DO_APP_BUNDLE=0; DO_WEB=0 ;;
        --zip-linux)     DO_GUI=0; DO_ZIP=0; DO_ZIP_LINUX=1; DO_APPIMAGE=0; DO_APP_BUNDLE=0; DO_WEB=0 ;;
        --appimage)      DO_GUI=0; DO_ZIP=0; DO_APPIMAGE=1; DO_APP_BUNDLE=0; DO_WEB=0 ;;
        --app)           DO_GUI=1; DO_ZIP=0; DO_APPIMAGE=0; DO_APP_BUNDLE=1; DO_WEB=0 ;;
        --whisper)       DO_GUI=0; DO_ZIP=0; DO_APPIMAGE=0; DO_APP_BUNDLE=0; DO_WEB=0; DO_WHISPER_WIN=1 ;;
        --build-whisper) DO_BUILD_WHISPER=1 ;;
        --llama-studio)  DO_LLAMA_STUDIO=1 ;;
        --win-cross)     DO_WIN_CROSS=1 ;;
        --install)       DO_INSTALL=1 ;;
        --no-zip)        DO_ZIP=0 ;;
        --no-bat)        DO_ZIP=0 ;;
        --no-zip-linux)  DO_ZIP_LINUX=0 ;;
        --no-appimage)   DO_APPIMAGE=0 ;;
        --no-app)        DO_APP_BUNDLE=0 ;;
        --no-web)        DO_WEB=0 ;;
        --no-whisper)    DO_WHISPER_WIN=0 ;;
        -h|--help)
            echo "Uso: $0 [opzioni]"
            echo ""
            echo "  Sezioni (seleziona una o più):"
            echo "  --gui        Solo GUI Qt6 (binario desktop)"
            echo "  --web        GUI Qt6 + aggiorna interfaccia web embedded (LAN server)"
            echo "  --bat        Solo ZIP Windows (.bat + binari) — usabile da Linux/Windows"
            echo "  --zip-linux  Solo ZIP Sorgenti Linux (include Prismalux.desktop) — solo su Linux"
            echo "  --win-cross  Cross-compila Prismalux_GUI.exe per Windows da Linux (mingw-w64)"
            echo "  --install    Installa Prismalux nel sistema Linux dopo la build"
            echo "  --zip        Alias per --bat"
            echo "  --appimage   Solo AppImage Linux — solo su Linux"
            echo "  --app        GUI + .app bundle macOS — solo su macOS"
            echo ""
            echo "  Default per piattaforma (senza argomenti):"
            if [ "$IS_WIN" = "1" ]; then
            echo "    Windows → GUI + ZIP (.bat)                [AppImage: N/A]"
            elif [ "$IS_MAC" = "1" ]; then
            echo "    macOS   → GUI + .app bundle               [ZIP/AppImage: N/A]"
            else
            echo "    Linux   → GUI + AppImage + ZIP Windows + ZIP Sorgenti Linux     [--no-zip / --no-zip-linux per saltare]"
            fi
            echo ""
            echo "  Extra:"
            echo "  --build-whisper  Compila whisper.cpp da sorgente"
            echo "  --llama-studio   Compila llama-server + llama-cli"
            echo "  --whisper        Scarica whisper-cli.exe precompilato (solo Windows ZIP)"
            echo ""
            echo "  Su Windows (MSYS2/Git Bash): bash aggiorna.sh [opzioni]"
            exit 0 ;;
        *) echo -e "${R}Opzione sconosciuta: $arg${N}"; exit 1 ;;
    esac
done

# AppImage richiede Linux — blocca esecuzione se richiesta su altra piattaforma
if [ "$DO_APPIMAGE" = "1" ] && { [ "$IS_WIN" = "1" ] || [ "$IS_MAC" = "1" ]; }; then
    echo -e "${Y}⚠  --appimage disponibile solo su Linux — saltato.${N}"
    DO_APPIMAGE=0
fi

# ── Helper ─────────────────────────────────────────────────────
step() { echo -e "\n${C}▶ $*${N}"; }
ok()   { echo -e "${G}✅ $*${N}"; }
fail() { echo -e "${R}❌ $*${N}"; exit 1; }

echo -e "  ${C}Core disponibili: ${_RAW_NPROC}  →  job paralleli: ${NPROC} (${_RAW_NPROC}-1)${N}"

T_START=$(date +%s)

# ══════════════════════════════════════════════════════════════
#  1. GUI Qt6
# ══════════════════════════════════════════════════════════════
if [ "$DO_GUI" = "1" ]; then
    step "Compilo GUI Qt6 (${_UNAME})..."

    # ── Su Windows delega a build.py (evita problemi con grep/tee/cmake MSYS2) ──
    if [ "$IS_WIN" = "1" ]; then
        _PYTHON=""
        for _py in python3 python py; do
            if command -v "$_py" &>/dev/null; then
                _PYTHON="$_py"
                break
            fi
        done
        # Fallback: Python di MSYS2 UCRT64
        if [ -z "$_PYTHON" ] && [ -f "/ucrt64/bin/python3.exe" ]; then
            _PYTHON="/ucrt64/bin/python3.exe"
        fi

        if [ -n "$_PYTHON" ]; then
            step "  Delego a build.py (Python: $_PYTHON)..."
            "$_PYTHON" "$SCRIPT_DIR/build.py"
            [ -f "$GUI_BIN" ] || fail "Binario GUI non trovato dopo build.py: $GUI_BIN"
            ok "GUI compilata → $GUI_BIN"
        else
            fail "Python non trovato. Installa con: pacman -S mingw-w64-ucrt-x86_64-python"
        fi
    else
        # ── Linux / macOS: cmake dalla root del progetto ───────────────────────
        # Comando: cmake -B build_gui gui/ -DCMAKE_BUILD_TYPE=Release
        #          cmake --build build_gui -j$(nproc)
        cd "$SCRIPT_DIR"
        _CMAKE_LOG="$QT_BUILD/prismalux_build.log"
        mkdir -p "$QT_BUILD"

        if [ ! -f "$QT_BUILD/CMakeCache.txt" ]; then
            step "  Prima configurazione cmake..."
            # shellcheck disable=SC2086
            cmake -B "$QT_BUILD" "$QT_GUI" -DCMAKE_BUILD_TYPE=Release $CMAKE_EXTRA \
                2>&1 | tee "$_CMAKE_LOG"
            [ ${PIPESTATUS[0]} -eq 0 ] || fail "cmake configure fallito"
        fi

        step "  Compilazione ($NPROC thread)..."
        cmake --build "$QT_BUILD" -j"$NPROC" 2>&1 | tee -a "$_CMAKE_LOG"
        [ ${PIPESTATUS[0]} -eq 0 ] || fail "cmake build fallito"

        [ -f "$GUI_BIN" ] || fail "Binario GUI non trovato: $GUI_BIN"
        ok "GUI compilata → $GUI_BIN"

        # Copia NotoColorEmoji nella cartella fonts/ della build (fallback emoji)
        NOTO_EMOJI_SRC="/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf"
        FONTS_DIR="$QT_BUILD/fonts"
        if [ -f "$NOTO_EMOJI_SRC" ] && [ ! -f "$FONTS_DIR/NotoColorEmoji.ttf" ]; then
            mkdir -p "$FONTS_DIR"
            cp "$NOTO_EMOJI_SRC" "$FONTS_DIR/"
            ok "NotoColorEmoji.ttf copiato → $FONTS_DIR/"
        fi
    fi

    # Desktop entry solo su Linux
    if [ "$IS_WIN" = "0" ] && [ "$IS_MAC" = "0" ]; then
        DESKTOP_OUT="$SCRIPT_DIR/Prismalux.desktop"
        _ICON_PATH="$SCRIPT_DIR/ICONA/prismalux.png"
        [ -f "$_ICON_PATH" ] || _ICON_PATH="prismalux"   # fallback a nome simbolico
        cat > "$DESKTOP_OUT" <<DESKTOP_EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Prismalux v${PRISMA_VERSION}
GenericName=Piattaforma AI Locale
Comment=Pipeline Agenti · RAG · Matematica LaTeX · MCPs · Calcolo Distribuito
Exec=$GUI_BIN
Icon=$_ICON_PATH
Terminal=false
Categories=Education;Science;Utility;Office;
Keywords=AI;agenti;matematica;LaTeX;tutor;ollama;llama;rag;mcp;calcolo;
StartupNotify=true
StartupWMClass=Prismalux_GUI
DESKTOP_EOF
        chmod +x "$DESKTOP_OUT"
        ok "Prismalux.desktop aggiornato → $DESKTOP_OUT"

        # Installa nel menu di sistema utente
        _APPS_DIR="$HOME/.local/share/applications"
        mkdir -p "$_APPS_DIR"
        cp "$DESKTOP_OUT" "$_APPS_DIR/Prismalux.desktop"
        chmod +x "$_APPS_DIR/Prismalux.desktop"
        # Aggiorna il database del menu (ignora se il tool non c'è)
        command -v update-desktop-database &>/dev/null \
            && update-desktop-database "$_APPS_DIR" 2>/dev/null || true
        # Su GNOME: marca il file come "trusted" per evitare il blocco "avvio non sicuro"
        command -v gio &>/dev/null \
            && gio set "$_APPS_DIR/Prismalux.desktop" metadata::trusted true 2>/dev/null || true
        ok "Installato nel menu → $_APPS_DIR/Prismalux.desktop"
    fi
fi

# ══════════════════════════════════════════════════════════════
#  1b. Web — interfaccia web embedded (lan_server)
# ══════════════════════════════════════════════════════════════
if [ "$DO_WEB" = "1" ]; then
    step "Web app (aggiornamento interfaccia LAN server)..."

    # La web app è embedded in gui/lan_server.cpp — ricompilare la GUI è sufficiente.
    # Genera anche un index.html standalone in web_preview/ per test/debug.
    WEB_PREVIEW_DIR="$SCRIPT_DIR/web_preview"
    WEB_PREVIEW_HTML="$WEB_PREVIEW_DIR/index.html"
    mkdir -p "$WEB_PREVIEW_DIR"

    # Estrai HTML embedded da lan_server.cpp con Python
    python3 - "$SCRIPT_DIR/gui/lan_server.cpp" "$WEB_PREVIEW_HTML" <<'PYEOF' 2>/dev/null
import sys, re

src_path, out_path = sys.argv[1], sys.argv[2]
with open(src_path, encoding='utf-8', errors='replace') as f:
    src = f.read()

# Trova il blocco che costruisce la risposta HTTP /web
# Le stringhe C++ sono concatenate con `sendChunk(sock,` oppure `resp +=`
chunks = []
# Pattern: cattura ogni stringa C++ letterale (gestisce escape)
for m in re.finditer(r'"((?:[^"\\]|\\(?:n|t|r|\\|"|x[0-9a-fA-F]{1,2}|[0-9]{1,3}))*)"', src):
    s = m.group(1)
    # Decodifica sequenze di escape comuni
    s = s.replace('\\n', '\n').replace('\\t', '\t').replace('\\r', '\r')
    s = s.replace('\\"', '"').replace('\\\\', '\\')
    # Filtra: deve sembrare HTML/CSS/JS (contenere tag o rule CSS o JS)
    if any(k in s for k in ('<html', '<head', '<body', '<div', '<style', '<script',
                             'font-family', 'background', 'function ', 'const ', 'var ',
                             'fetch(', 'getElementById', 'innerHTML')):
        chunks.append(s)

html = ''.join(chunks)
if len(html) < 500:
    print("Estratto insufficiente, uso fallback", file=sys.stderr)
    sys.exit(1)

with open(out_path, 'w', encoding='utf-8') as f:
    f.write(html)
print(f"Estratti {len(html)} caratteri → {out_path}")
PYEOF

    if [ -f "$WEB_PREVIEW_HTML" ] && [ -s "$WEB_PREVIEW_HTML" ]; then
        _WEB_SIZE=$(du -sh "$WEB_PREVIEW_HTML" | cut -f1)
        ok "Web preview → $WEB_PREVIEW_HTML  (${_WEB_SIZE})"
    else
        echo -e "${Y}  ⚠  Estrazione HTML non completata — apri comunque http://localhost:11500/web${N}"
    fi

    ok "Web app aggiornata — avvia Prismalux e accedi a http://localhost:11500/web"
fi

# ══════════════════════════════════════════════════════════════
#  1c. whisper.cpp — compilazione da sorgente
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

        # ── Chiedi se includere RAG ───────────────────────────────
        _ZIP_EXTRA_ARGS=""
        RAG_DIR="$SCRIPT_DIR/RAG"
        if [ -d "$RAG_DIR" ] && [ "$(ls -A "$RAG_DIR" 2>/dev/null)" ]; then
            RAG_SIZE="$(du -sh "$RAG_DIR" 2>/dev/null | cut -f1)"
            echo ""
            echo -e "  ${C}Vuoi includere la cartella RAG nello ZIP?${N}"
            echo -e "  RAG contiene documenti locali usati dall'AI (${Y}${RAG_SIZE}${N})"
            echo -e "  Il tuo amico li riceverà già pronti senza doverli cercare."
            echo -e ""
            printf "  Includi RAG? [s/N] "
            read -r _rag_answer </dev/tty || _rag_answer="n"
            case "$_rag_answer" in
                [sS]|[yY]) _ZIP_EXTRA_ARGS="--include-rag"; ok "RAG incluso nello ZIP" ;;
                *)          ok "RAG escluso (ZIP più leggero)" ;;
            esac
            echo ""
        fi

        "$_PYTHON" "$ZIP_SCRIPT" --out "$ZIP_OUT" $_ZIP_EXTRA_ARGS \
            2>&1 | grep -E "(SORGENTI|BINARIO|Fatto|file|\+|skip|errore|warn)" || true
        [ -f "$ZIP_OUT" ] || fail "ZIP non generato"
        ok "ZIP aggiornato → $ZIP_OUT  ($(du -sh "$ZIP_OUT" | cut -f1))"
    fi
fi

# ══════════════════════════════════════════════════════════════
#  3b. ZIP Sorgenti Linux (include Prismalux.desktop — solo Linux)
# ══════════════════════════════════════════════════════════════
if [ "$DO_ZIP_LINUX" = "1" ]; then
    if [ "$IS_WIN" = "1" ] || [ "$IS_MAC" = "1" ]; then
        echo -e "${Y}⚠  ZIP Sorgenti Linux disponibile solo su Linux — saltato.${N}"
    else
        step "Rigenero ZIP Sorgenti Linux..."
        cd "$SCRIPT_DIR"

        if ! command -v zip &>/dev/null; then
            echo -e "${Y}⚠  'zip' non trovato — salto ZIP Sorgenti Linux.${N}"
            echo -e "${Y}   Installa con: sudo apt install zip${N}"
            DO_ZIP_LINUX=0
        else
            rm -f "$ZIP_LINUX_OUT"

            _DT="$SCRIPT_DIR/Prismalux.desktop"
            _DT_ARG=""
            if [ -f "$_DT" ]; then
                _DT_ARG="Prismalux.desktop"
            else
                echo -e "${Y}  ⚠  Prismalux.desktop non trovato — incluso nel ZIP quando esiste.${N}"
                echo -e "${Y}     Esegui prima: ./aggiorna.sh --gui${N}"
            fi

            _SETUP_SH=""
            [ -f "$SCRIPT_DIR/COMPILA_INSTALLA_Prismalux.sh" ] && _SETUP_SH="COMPILA_INSTALLA_Prismalux.sh"

            zip -r "$ZIP_LINUX_OUT" \
                gui/ ICONA/ MCPs/ "BEST_PRACTICE_&_GOAL/" scripts/ \
                EXPORT/linux/ \
                README.md LICENSE aggiorna.sh \
                ${_SETUP_SH:+"$_SETUP_SH"} \
                ${_DT_ARG:+"$_DT_ARG"} \
                -x "*/build/*" \
                -x "*/build_*" \
                -x "*/.git/*" \
                -x "*/__pycache__/*" \
                -x "*/models/*" \
                -x "*/llama.cpp/*" \
                -x "*/llama_cpp_studio/*" \
                -x "*.o" -x "*.a" -x "*.so" -x "*.so.*" \
                -x "*.pyc" -x "*.AppImage" \
                -x "Prismalux_v*.zip" \
                > /dev/null
            [ -f "$ZIP_LINUX_OUT" ] || fail "ZIP Sorgenti Linux non generato"
            ok "ZIP Sorgenti Linux → $ZIP_LINUX_OUT  ($(du -sh "$ZIP_LINUX_OUT" | cut -f1))"
        fi
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
#  5. macOS App Bundle (.app + macdeployqt + DMG opzionale)
# ══════════════════════════════════════════════════════════════
if [ "$DO_APP_BUNDLE" = "1" ]; then
    if [ "$IS_MAC" != "1" ]; then
        echo -e "${Y}⚠  --app disponibile solo su macOS — saltato.${N}"
    else
        step "Genero .app bundle macOS..."

        APP_CONTENTS="$APP_BUNDLE_OUT/Contents"
        APP_MACOS="$APP_CONTENTS/MacOS"
        APP_RES="$APP_CONTENTS/Resources"

        [ -f "$GUI_BIN" ] || fail "Binario non trovato: $GUI_BIN — esegui prima --gui"

        # ── Struttura bundle ────────────────────────────────────────
        rm -rf "$APP_BUNDLE_OUT"
        mkdir -p "$APP_MACOS" "$APP_RES"

        cp "$GUI_BIN" "$APP_MACOS/Prismalux_GUI"
        chmod +x "$APP_MACOS/Prismalux_GUI"
        echo -e "  Binario   → $APP_MACOS/Prismalux_GUI"

        # ── Temi → Resources/ ──────────────────────────────────────
        if [ -d "$QT_GUI/themes" ]; then
            cp -r "$QT_GUI/themes" "$APP_RES/themes"
            echo -e "  Temi      → $APP_RES/themes/"
        fi

        # ── Icona PNG → ICNS (sips + iconutil, entrambi built-in macOS) ─
        _ICON_KEY=""
        _ICON_PNG="$SCRIPT_DIR/ICONA/prismalux.png"
        if [ -f "$_ICON_PNG" ] && command -v sips &>/dev/null && command -v iconutil &>/dev/null; then
            _ICONSET="$(mktemp -d /tmp/prismalux_iconset_XXXXXX).iconset"
            mkdir -p "$_ICONSET"
            for _sz in 16 32 128 256 512; do
                sips -z "$_sz"        "$_sz"        "$_ICON_PNG" \
                    --out "$_ICONSET/icon_${_sz}x${_sz}.png"       2>/dev/null || true
                sips -z "$((_sz*2))" "$((_sz*2))" "$_ICON_PNG" \
                    --out "$_ICONSET/icon_${_sz}x${_sz}@2x.png"    2>/dev/null || true
            done
            iconutil -c icns "$_ICONSET" -o "$APP_RES/prismalux.icns" 2>/dev/null \
                && _ICON_KEY="prismalux" || true
            rm -rf "$_ICONSET"
            [ -n "$_ICON_KEY" ] && echo -e "  Icona     → $APP_RES/prismalux.icns"
        fi

        # ── Info.plist ─────────────────────────────────────────────
        {
        cat <<PLIST_EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>Prismalux_GUI</string>
    <key>CFBundleIdentifier</key>
    <string>com.wildlux.prismalux</string>
    <key>CFBundleName</key>
    <string>Prismalux</string>
    <key>CFBundleDisplayName</key>
    <string>Prismalux v${PRISMA_VERSION}</string>
    <key>CFBundleVersion</key>
    <string>${PRISMA_VERSION}.0</string>
    <key>CFBundleShortVersionString</key>
    <string>${PRISMA_VERSION}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSRequiresAquaSystemAppearance</key>
    <false/>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
PLIST_EOF
        [ -n "$_ICON_KEY" ] && printf '    <key>CFBundleIconFile</key>\n    <string>%s</string>\n' "$_ICON_KEY"
        echo '</dict>'
        echo '</plist>'
        } > "$APP_CONTENTS/Info.plist"
        echo -e "  Info.plist → $APP_CONTENTS/Info.plist"

        # ── macdeployqt — copia Qt frameworks nel bundle ────────────
        # Cerca in: PATH, Homebrew (Apple Silicon e Intel), Qt installer
        _MACDEPLOYQT=""
        command -v macdeployqt &>/dev/null && _MACDEPLOYQT="macdeployqt"
        if [ -z "$_MACDEPLOYQT" ]; then
            # qmake conosce esattamente dove si trovano i tool Qt
            _QT_BINS="$(qmake -query QT_INSTALL_BINS 2>/dev/null || true)"
            [ -x "$_QT_BINS/macdeployqt" ] && _MACDEPLOYQT="$_QT_BINS/macdeployqt"
        fi
        if [ -z "$_MACDEPLOYQT" ]; then
            for _qdir in \
                /opt/homebrew/opt/qt6/bin \
                /usr/local/opt/qt6/bin \
                /opt/homebrew/bin \
                "$HOME/Qt/6"*/macos/bin \
                "$HOME/Qt/6"*/clang_64/bin; do
                # shellcheck disable=SC2086
                _candidate="$(ls $_qdir/macdeployqt 2>/dev/null | head -1)"
                [ -x "$_candidate" ] && { _MACDEPLOYQT="$_candidate"; break; }
            done
        fi

        if [ -n "$_MACDEPLOYQT" ]; then
            echo -e "  macdeployqt → ${C}$_MACDEPLOYQT${N}"
            "$_MACDEPLOYQT" "$APP_BUNDLE_OUT" -verbose=1 2>&1 \
                | grep -E "(copying|Deploying|error|WARNING|Cannot)" || true
            ok "Qt frameworks copiati nel bundle"
        else
            echo -e "${Y}  ⚠  macdeployqt non trovato — bundle non autonomo.${N}"
            echo -e "${Y}     Installa Qt6: brew install qt6${N}"
            echo -e "${Y}     poi: export PATH=\"/opt/homebrew/opt/qt6/bin:\$PATH\"${N}"
        fi

        _APP_SIZE="$(du -sh "$APP_BUNDLE_OUT" 2>/dev/null | cut -f1)"
        ok ".app bundle → $APP_BUNDLE_OUT  ($_APP_SIZE)"

        # ── DMG — creato se create-dmg o hdiutil è disponibile ─────
        # create-dmg (brew install create-dmg) produce un DMG con sfondo e link /Applications
        # hdiutil è built-in macOS e produce un DMG semplice
        _DMG_OUT="$SCRIPT_DIR/Prismalux_v${PRISMA_VERSION}_macOS.dmg"
        rm -f "$_DMG_OUT"
        if command -v create-dmg &>/dev/null; then
            step "  Genero DMG (create-dmg)..."
            create-dmg \
                --volname "Prismalux v${PRISMA_VERSION}" \
                --window-pos 200 120 --window-size 600 400 \
                --icon-size 100 \
                --icon "Prismalux.app" 175 190 \
                --hide-extension "Prismalux.app" \
                --app-drop-link 425 190 \
                "$_DMG_OUT" "$APP_BUNDLE_OUT" 2>/dev/null || true
        elif command -v hdiutil &>/dev/null; then
            step "  Genero DMG (hdiutil)..."
            _TMP_DMG="$(mktemp -d /tmp/prismalux_dmg_XXXXXX)"
            cp -r "$APP_BUNDLE_OUT" "$_TMP_DMG/"
            hdiutil create \
                -volname "Prismalux v${PRISMA_VERSION}" \
                -srcfolder "$_TMP_DMG" \
                -ov -format UDZO \
                "$_DMG_OUT" 2>/dev/null || true
            rm -rf "$_TMP_DMG"
        fi
        [ -f "$_DMG_OUT" ] \
            && ok "DMG → $_DMG_OUT  ($(du -sh "$_DMG_OUT" | cut -f1))" \
            || echo -e "${Y}  DMG non generato (installa create-dmg: brew install create-dmg)${N}"
    fi
fi

# ══════════════════════════════════════════════════════════════
#  6. Cross-compilazione Windows da Linux (--win-cross)
# ══════════════════════════════════════════════════════════════
if [ "$DO_WIN_CROSS" = "1" ]; then
    if [ "$IS_WIN" = "1" ] || [ "$IS_MAC" = "1" ]; then
        echo -e "${Y}⚠  --win-cross disponibile solo su Linux — saltato.${N}"
    else
        step "Cross-compilazione Windows (mingw-w64)..."
        CROSS_SCRIPT="$SCRIPT_DIR/EXPORT/windows/cross_compile.sh"
        [ -f "$CROSS_SCRIPT" ] || fail "Script non trovato: $CROSS_SCRIPT"
        chmod +x "$CROSS_SCRIPT"
        bash "$CROSS_SCRIPT"
        WIN_CROSS_EXE="$SCRIPT_DIR/build_cross_win/Prismalux_GUI.exe"
        [ -f "$WIN_CROSS_EXE" ] \
            && ok "Exe Windows → $WIN_CROSS_EXE  ($(du -sh "$WIN_CROSS_EXE" | cut -f1))" \
            || fail "Cross-compilazione fallita"
        # Rigenera lo ZIP includendo il binario appena compilato
        if [ "$DO_ZIP" = "1" ] && [ -n "$_PYTHON" ]; then
            step "  Rigenero ZIP con binario Windows incluso..."
            "$_PYTHON" "$ZIP_SCRIPT" --out "$ZIP_OUT" \
                2>&1 | grep -E "(SORGENTI|BINARIO|Fatto|file|\+|skip)" || true
            ok "ZIP aggiornato con .exe → $ZIP_OUT  ($(du -sh "$ZIP_OUT" | cut -f1))"
        fi
    fi
fi

# ══════════════════════════════════════════════════════════════
#  7. Installazione Linux (--install)
# ══════════════════════════════════════════════════════════════
if [ "$DO_INSTALL" = "1" ]; then
    if [ "$IS_WIN" = "1" ] || [ "$IS_MAC" = "1" ]; then
        echo -e "${Y}⚠  --install disponibile solo su Linux — saltato.${N}"
    else
        step "Installazione Linux..."
        INSTALL_SCRIPT="$SCRIPT_DIR/EXPORT/linux/install_launcher.sh"
        [ -f "$INSTALL_SCRIPT" ] || fail "Script non trovato: $INSTALL_SCRIPT"
        chmod +x "$INSTALL_SCRIPT"
        bash "$INSTALL_SCRIPT" --user
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
if [ "$DO_GUI" = "1" ] || [ "$DO_WEB" = "1" ]; then
    [ -f "$GUI_BIN" ] && echo -e "  ${G}GUI${N}           $GUI_BIN"
fi
if [ "$DO_WEB" = "1" ]; then
    _WEB_PREVIEW="$SCRIPT_DIR/web_preview/index.html"
    [ -f "$_WEB_PREVIEW" ] \
        && echo -e "  ${G}Web preview${N}  $_WEB_PREVIEW" \
        || true
    echo -e "  ${G}Web URL${N}      http://localhost:11500/web  (avvia Prismalux prima)"
fi
[ "$DO_BUILD_WHISPER" = "1" ] && [ -f "$SCRIPT_DIR/whisper.cpp/build/bin/whisper-cli${EXE_EXT}" ] && \
    echo -e "  ${G}whisper-cli${N}  $SCRIPT_DIR/whisper.cpp/build/bin/whisper-cli${EXE_EXT}"
[ "$DO_LLAMA_STUDIO"  = "1" ] && [ -f "$SCRIPT_DIR/llama_cpp_studio/llama.cpp/build/bin/llama-server${EXE_EXT}" ] && \
    echo -e "  ${G}llama-studio${N} $SCRIPT_DIR/llama_cpp_studio/llama.cpp/build/bin/"
[ "$DO_ZIP" = "1" ] && [ -f "$ZIP_OUT" ] && \
    echo -e "  ${G}ZIP / .bat${N}   $ZIP_OUT  ($(du -sh "$ZIP_OUT" | cut -f1))"
[ "$DO_ZIP_LINUX" = "1" ] && [ -f "$ZIP_LINUX_OUT" ] && \
    echo -e "  ${G}ZIP Linux${N}    $ZIP_LINUX_OUT  ($(du -sh "$ZIP_LINUX_OUT" | cut -f1))"
[ "$DO_APPIMAGE" = "1" ] && [ -f "$APPIMAGE_OUT" ] && \
    echo -e "  ${G}AppImage${N}     $APPIMAGE_OUT  ($(du -sh "$APPIMAGE_OUT" | cut -f1))"
[ "$DO_APP_BUNDLE" = "1" ] && [ -d "$APP_BUNDLE_OUT" ] && \
    echo -e "  ${G}.app bundle${N}  $APP_BUNDLE_OUT  ($(du -sh "$APP_BUNDLE_OUT" | cut -f1))"
_DMG_SUMMARY="$SCRIPT_DIR/Prismalux_v${PRISMA_VERSION}_macOS.dmg"
[ "$DO_APP_BUNDLE" = "1" ] && [ -f "$_DMG_SUMMARY" ] && \
    echo -e "  ${G}DMG${N}          $_DMG_SUMMARY  ($(du -sh "$_DMG_SUMMARY" | cut -f1))"
[ "$DO_WHISPER_WIN"   = "1" ] && [ -f "$WHISPER_WIN_DIR/whisper-cli.exe" ] && \
    echo -e "  ${G}Whisper WIN${N}  $WHISPER_WIN_DIR/whisper-cli.exe"
[ "$DO_WIN_CROSS" = "1" ] && [ -f "$SCRIPT_DIR/build_cross_win/Prismalux_GUI.exe" ] && \
    echo -e "  ${G}Win .exe${N}     $SCRIPT_DIR/build_cross_win/Prismalux_GUI.exe  ($(du -sh "$SCRIPT_DIR/build_cross_win/Prismalux_GUI.exe" | cut -f1))"
[ "$DO_INSTALL"   = "1" ] && \
    echo -e "  ${G}Installato${N}   prismalux  (comando disponibile nel terminale)"
echo -e "  ${Y}Tempo: $(( T_END - T_START ))s${N}"
echo ""

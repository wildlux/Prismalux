#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  compila_kubuntu.sh — Script unico per Kubuntu/Ubuntu
#
#  Fasi (tutte attive di default):
#    1. Installa dipendenze Qt6 via apt
#    2. Compila GUI Qt6 con cmake
#    3. Esporta eseguibile standalone (binario + temi + launcher + menu KDE)
#    4. Genera AppImage (scarica appimagetool se serve)
#
#  Uso:
#    bash compila_kubuntu.sh              # tutto (raccomandato prima volta)
#    bash compila_kubuntu.sh --solo-build # salta apt install
#    bash compila_kubuntu.sh --native     # ottimizzazioni CPU native
#    bash compila_kubuntu.sh --lto        # Link-Time Optimization
#    bash compila_kubuntu.sh --all-opt    # native + LTO (massima performance)
#    bash compila_kubuntu.sh --no-appimage# salta generazione AppImage
#
#  Compatibile con: Kubuntu / Ubuntu 22.04, 24.04, 26.04 e derivate
# ═══════════════════════════════════════════════════════════════════════════
set -euo pipefail

# ── Colori ──────────────────────────────────────────────────────────────────
R='\033[0;31m'; G='\033[0;32m'; Y='\033[0;33m'
C='\033[0;36m'; B='\033[1;37m'; N='\033[0m'

ok()   { echo -e "${G}  ✅ $*${N}"; }
info() { echo -e "${C}  ──  $*${N}"; }
warn() { echo -e "${Y}  ⚠   $*${N}"; }
fail() { echo -e "${R}  ❌  $*${N}"; exit 1; }
step() { echo -e "\n${B}▶ $*${N}"; }

# ── Percorsi ────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
QT_GUI="$SCRIPT_DIR/gui"
QT_BUILD="$SCRIPT_DIR/build_gui"
GUI_BIN="$QT_BUILD/Prismalux_GUI"
APPDIR="$SCRIPT_DIR/AppDir"
APPIMAGE_OUT="$SCRIPT_DIR/Prismalux-x86_64.AppImage"
ICON_PNG="$SCRIPT_DIR/ICONA/prismalux.png"
QT_LIBS_DIR="/usr/lib/x86_64-linux-gnu"
QT_PLUGINS_DIR="${QT_LIBS_DIR}/qt6/plugins"
NPROC=$(nproc 2>/dev/null || echo 2)

# ── Flag ────────────────────────────────────────────────────────────────────
DO_DEPS=1
OPT_NATIVE=0
OPT_LTO=0
DO_APPIMAGE=1

for arg in "$@"; do
    case "$arg" in
        --solo-build)  DO_DEPS=0 ;;
        --native)      OPT_NATIVE=1 ;;
        --lto)         OPT_LTO=1 ;;
        --all-opt)     OPT_NATIVE=1; OPT_LTO=1 ;;
        --no-appimage) DO_APPIMAGE=0 ;;
        --help|-h)
            echo "Uso: bash compila_kubuntu.sh [opzioni]"
            echo ""
            echo "  (nessuna)      installa Qt6 + compila + eseguibile + AppImage"
            echo "  --solo-build   salta apt install (già fatto)"
            echo "  --native       ottimizzazioni CPU native (-march=native -O3)"
            echo "  --lto          Link-Time Optimization (più lento da compilare, più veloce a runtime)"
            echo "  --all-opt      native + LTO — massima performance, non copiare su altri PC"
            echo "  --no-appimage  salta generazione AppImage"
            exit 0 ;;
        *) warn "Opzione sconosciuta: $arg" ;;
    esac
done

echo ""
echo -e "${B}╔══════════════════════════════════════════════════════════╗${N}"
echo -e "${B}║      Prismalux — Build nativa Kubuntu/Ubuntu             ║${N}"
echo -e "${B}╚══════════════════════════════════════════════════════════╝${N}"
echo ""

UBUNTU_VER=$(grep "^VERSION_ID=" /etc/os-release 2>/dev/null | tr -d '"' | cut -d= -f2 || echo "")
DISTRO_NAME=$(grep PRETTY_NAME /etc/os-release 2>/dev/null | cut -d= -f2 | tr -d '"' || echo "Linux")
PRISMA_VERSION=$(grep -m1 'project(Prismalux_GUI VERSION' "$QT_GUI/CMakeLists.txt" \
                 | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "2.9")

info "Distro  : $DISTRO_NAME"
info "Versione: $UBUNTU_VER"
info "Core CPU: $NPROC"
info "Progetto: $SCRIPT_DIR  (Prismalux v${PRISMA_VERSION})"
echo ""

# ════════════════════════════════════════════════════════════════════════════
#  1. Dipendenze Qt6 + build tools
# ════════════════════════════════════════════════════════════════════════════
if [ "$DO_DEPS" = "1" ]; then
    step "Installazione dipendenze Qt6 + build tools..."
    info "Richiede password sudo (una volta sola)"
    echo ""

    sudo apt-get update -qq

    # Pacchetti sempre necessari
    sudo apt-get install -y \
        build-essential cmake git \
        libgl-dev libglib2.0-dev \
        qt6-base-dev \
        2>&1 | grep -E "^(Get:|Inst |Reading|Building|E:)" || true

    # Pacchetti opzionali — installati ignorando errori
    OPT_PKGS=(
        libqt6sql6                    # driver SQLite (GraphMemory, quiz CCNA)
        qt6-multimedia-dev            # microfono reale (QAudioSource)
        libqt6multimedia6
        libqt6keychain-dev            # token LAN nel keyring di sistema
        qtkeychain-qt6-dev
        graphviz                      # visualizzazione grafi
        poppler-utils                 # pdftotext per Analisi Fenomeni
        python3 python3-pip           # MCPs Python
    )

    info "Pacchetti opzionali (gli errori sono ignorati)..."
    for pkg in "${OPT_PKGS[@]}"; do
        sudo apt-get install -y "$pkg" 2>/dev/null && ok "  $pkg" \
            || warn "  $pkg non disponibile (funzionalità opzionale)"
    done

    # Qt6 WebEngine (LaTeX KaTeX) — grande (~400 MB), chiede conferma
    echo ""
    echo -e "  ${C}Qt6 WebEngine abilita il rendering LaTeX (formule Analisi 1/2).${N}"
    echo -e "  ${C}Richiede ~400 MB di download. Senza, le formule usano testo normale.${N}"
    printf "\n  Installare Qt6 WebEngine? [S/n] "
    read -r _web_ans </dev/tty || _web_ans="s"
    case "$_web_ans" in
        [nN]) warn "Qt6 WebEngine saltato" ;;
        *)
            sudo apt-get install -y qt6-webengine-dev libqt6webenginewidgets6 2>/dev/null \
                && ok "Qt6 WebEngine installato" \
                || warn "Qt6 WebEngine non disponibile su questa versione"
            # KaTeX JS (usato da LatexView in /usr/share/javascript/katex/)
            sudo apt-get install -y libjs-katex 2>/dev/null \
                && ok "libjs-katex installato" \
                || warn "libjs-katex non trovato — LaTeX userà fallback testo"
            ;;
    esac

    ok "Dipendenze installate"
fi

# ════════════════════════════════════════════════════════════════════════════
#  2. Compilazione con cmake
# ════════════════════════════════════════════════════════════════════════════
step "Compilazione GUI Qt6..."

[ -d "$QT_GUI" ] || fail "Cartella sorgenti gui/ non trovata: $QT_GUI"

CMAKE_CXX_FLAGS="-O2"
CMAKE_C_FLAGS="-O2"
CMAKE_EXTRA=""

if [ "$OPT_NATIVE" = "1" ]; then
    CMAKE_CXX_FLAGS="-O3 -march=native -mtune=native"
    CMAKE_C_FLAGS="-O3 -march=native -mtune=native"
    info "Native CPU optimization attiva (-march=native -O3)"
fi
if [ "$OPT_LTO" = "1" ]; then
    CMAKE_EXTRA="-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON"
    info "LTO (Link-Time Optimization) attivo"
fi

BUILD_LOG="$QT_BUILD/build.log"
mkdir -p "$QT_BUILD"

if [ ! -f "$QT_BUILD/CMakeCache.txt" ]; then
    info "Configurazione cmake..."
    cmake -B "$QT_BUILD" "$QT_GUI" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS" \
        -DCMAKE_C_FLAGS="$CMAKE_C_FLAGS" \
        $CMAKE_EXTRA \
        2>&1 | tee "$BUILD_LOG" | grep -E "(STATUS|error:|WARN|Qt6|Qt5|fatal)" || true
    [ ${PIPESTATUS[0]} -eq 0 ] || fail "cmake configure fallito — vedi: $BUILD_LOG"
else
    info "Cache cmake esistente, solo rebuild"
fi

T0=$(date +%s)
info "Build con $NPROC thread..."
cmake --build "$QT_BUILD" -j"$NPROC" 2>&1 | tee -a "$BUILD_LOG" | \
    grep -E "(Building|Linking|error:|fatal)" | grep -v "note:" || true
[ ${PIPESTATUS[0]} -eq 0 ] || fail "Compilazione fallita — vedi: $BUILD_LOG"
T1=$(date +%s)

[ -f "$GUI_BIN" ] || fail "Binario non trovato: $GUI_BIN"
BIN_SIZE=$(du -sh "$GUI_BIN" | cut -f1)
ok "Compilato in $(( T1 - T0 ))s  →  $GUI_BIN  ($BIN_SIZE)"

# ════════════════════════════════════════════════════════════════════════════
#  3. Eseguibile standalone — binario + launcher + menu KDE
# ════════════════════════════════════════════════════════════════════════════
step "Esportazione eseguibile standalone..."

# Script avvio (punta sempre al binario nella directory del progetto)
LAUNCHER="$SCRIPT_DIR/avvia_prismalux.sh"
cat > "$LAUNCHER" <<LAUNCH_EOF
#!/usr/bin/env bash
cd "\$(cd "\$(dirname "\$(readlink -f "\$0")")" && pwd)"
exec "$GUI_BIN" "\$@"
LAUNCH_EOF
chmod +x "$LAUNCHER"
ok "Launcher  → $LAUNCHER"

# Icona
_ICON_KEY="$ICON_PNG"
[ -f "$_ICON_KEY" ] || _ICON_KEY="application-x-executable"

# Desktop entry nel menu KDE
_APPS_DIR="$HOME/.local/share/applications"
mkdir -p "$_APPS_DIR"
cat > "$_APPS_DIR/Prismalux.desktop" <<DESKTOP_EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Prismalux v${PRISMA_VERSION}
GenericName=Piattaforma AI Locale
Comment=Pipeline Agenti · RAG · Matematica LaTeX · MCPs · GPU nativa
Exec=$LAUNCHER
Icon=$_ICON_KEY
Terminal=false
Categories=Education;Science;Utility;Office;
Keywords=AI;agenti;matematica;LaTeX;ollama;llama;rag;mcp;gpu;
StartupNotify=true
StartupWMClass=Prismalux_GUI
DESKTOP_EOF
chmod +x "$_APPS_DIR/Prismalux.desktop"

# Shortcut sul Desktop
if [ -d "$HOME/Desktop" ]; then
    cp "$_APPS_DIR/Prismalux.desktop" "$HOME/Desktop/Prismalux.desktop"
    chmod +x "$HOME/Desktop/Prismalux.desktop"
    gio set "$HOME/Desktop/Prismalux.desktop" metadata::trusted true 2>/dev/null || true
    ok "Desktop   → $HOME/Desktop/Prismalux.desktop"
fi

# Aggiorna database menu
update-desktop-database "$_APPS_DIR" 2>/dev/null || true
kbuildsycoca6 2>/dev/null || kbuildsycoca5 2>/dev/null || true
ok "Menu KDE  → cerca \"Prismalux\" nelle applicazioni"

# ════════════════════════════════════════════════════════════════════════════
#  4. AppImage
# ════════════════════════════════════════════════════════════════════════════
if [ "$DO_APPIMAGE" = "1" ]; then
    step "Generazione AppImage..."

    # Scarica appimagetool se non presente
    APPIMAGETOOL="${HOME}/.local/bin/appimagetool"
    if [ ! -x "$APPIMAGETOOL" ]; then
        APPIMAGETOOL="$(command -v appimagetool 2>/dev/null || echo "")"
    fi
    if [ -z "$APPIMAGETOOL" ] || [ ! -x "$APPIMAGETOOL" ]; then
        info "appimagetool non trovato — scarico..."
        mkdir -p "$HOME/.local/bin"
        _TOOL_URL="https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
        if command -v curl &>/dev/null; then
            curl -fsSL --progress-bar -o "$HOME/.local/bin/appimagetool" "$_TOOL_URL" \
                && chmod +x "$HOME/.local/bin/appimagetool" \
                || { warn "Download appimagetool fallito — AppImage saltata"; DO_APPIMAGE=0; }
        elif command -v wget &>/dev/null; then
            wget -q --show-progress -O "$HOME/.local/bin/appimagetool" "$_TOOL_URL" \
                && chmod +x "$HOME/.local/bin/appimagetool" \
                || { warn "Download appimagetool fallito — AppImage saltata"; DO_APPIMAGE=0; }
        else
            warn "curl/wget non trovati — AppImage saltata"
            DO_APPIMAGE=0
        fi
        APPIMAGETOOL="$HOME/.local/bin/appimagetool"
    fi
fi

if [ "$DO_APPIMAGE" = "1" ]; then
    # ── Prepara AppDir ──────────────────────────────────────────────────────
    rm -rf "$APPDIR"
    for d in usr/bin usr/lib usr/share/icons/hicolor/256x256/apps \
              usr/plugins/platforms usr/plugins/xcbglintegrations \
              usr/plugins/imageformats usr/plugins/iconengines \
              usr/plugins/platformthemes usr/plugins/styles usr/plugins/tls \
              usr/plugins/wayland-decoration-client \
              usr/plugins/wayland-graphics-integration-client \
              usr/plugins/wayland-shell-integration \
              usr/plugins/multimedia; do
        mkdir -p "$APPDIR/$d"
    done

    # Binario + temi
    cp "$GUI_BIN" "$APPDIR/usr/bin/Prismalux_GUI"
    chmod +x "$APPDIR/usr/bin/Prismalux_GUI"
    THEMES_SRC="$QT_BUILD/themes"
    [ -d "$THEMES_SRC" ] && cp -r "$THEMES_SRC" "$APPDIR/usr/bin/themes"

    # ── Funzione copia .so ──────────────────────────────────────────────────
    _copy_lib() {
        local lib="$1" dst="$APPDIR/usr/lib"
        [ -f "$lib" ] || return 0
        local bn; bn="$(basename "$lib")"
        [ -f "$dst/$bn" ] && return 0
        cp "$lib" "$dst/"
        if [ -L "$lib" ]; then
            local real; real="$(readlink -f "$lib")"
            [ -f "$dst/$(basename "$real")" ] || cp "$real" "$dst/" 2>/dev/null || true
        fi
    }

    # Dipendenze dirette del binario
    while IFS= read -r line; do
        lp=$(echo "$line" | awk '{print $3}')
        [ -f "$lp" ] || continue
        case "$(basename "$lp")" in
            libc.so*|libpthread.so*|libdl.so*|librt.so*|libm.so*|\
            libgcc_s.so*|ld-linux*.so*|libGL.so*|libEGL.so*|\
            libdrm.so*|libGLX.so*|libOpenGL.so*|libGLdispatch.so*) continue ;;
        esac
        _copy_lib "$lp"
    done < <(ldd "$APPDIR/usr/bin/Prismalux_GUI" 2>/dev/null)

    # Qt6 libs aggiuntive
    for lib_name in \
        libQt6Widgets libQt6Network libQt6Gui libQt6Core libQt6DBus \
        libQt6OpenGL libQt6OpenGLWidgets libQt6PrintSupport \
        libQt6Svg libQt6Xml libQt6Concurrent libQt6Sql \
        libicui18n libicuuc libicudata \
        libstdc++ libdouble-conversion libpcre2-16 libzstd \
        libharfbuzz libfreetype libfontconfig libpng16 \
        libxkbcommon libxkbcommon-x11 libxcb libxcb-util \
        libxcb-cursor libxcb-icccm4 libxcb-image libxcb-keysyms1 \
        libxcb-randr libxcb-render libxcb-render-util libxcb-shape \
        libxcb-shm libxcb-sync libxcb-xfixes libxcb-xinerama \
        libxcb-xkb libX11 libX11-xcb libXext libmd4c libb2; do
        lp=$(find "$QT_LIBS_DIR" -maxdepth 1 -name "${lib_name}.so*" 2>/dev/null | head -1)
        [ -n "$lp" ] && _copy_lib "$lp"
    done

    # ── Plugin Qt ───────────────────────────────────────────────────────────
    _copy_plugin() {
        local src="$1" ddir="$2"
        [ -f "$src" ] || return 0
        cp "$src" "$APPDIR/usr/plugins/$ddir/"
        while IFS= read -r line; do
            local lp; lp=$(echo "$line" | awk '{print $3}')
            [ -f "$lp" ] || continue
            case "$(basename "$lp")" in
                libc.so*|libpthread.so*|libm.so*|libdl.so*|\
                librt.so*|libgcc_s.so*|ld-linux*.so*) continue ;;
            esac
            _copy_lib "$lp"
        done < <(ldd "$src" 2>/dev/null)
    }

    for f in "$QT_PLUGINS_DIR/platforms"/libqxcb.so \
              "$QT_PLUGINS_DIR/platforms"/libqwayland-generic.so \
              "$QT_PLUGINS_DIR/platforms"/libqwayland-egl.so \
              "$QT_PLUGINS_DIR/platforms"/libqminimal.so \
              "$QT_PLUGINS_DIR/platforms"/libqoffscreen.so; do
        _copy_plugin "$f" "platforms"
    done
    for f in "$QT_PLUGINS_DIR/xcbglintegrations"/*.so; do
        [ -f "$f" ] && _copy_plugin "$f" "xcbglintegrations"
    done
    for f in "$QT_PLUGINS_DIR/imageformats"/libq{gif,jpeg,png,svg,ico,webp}.so; do
        _copy_plugin "$f" "imageformats"
    done
    for d in iconengines platformthemes styles tls \
              wayland-decoration-client wayland-graphics-integration-client \
              wayland-shell-integration multimedia; do
        [ -d "$QT_PLUGINS_DIR/$d" ] || continue
        for f in "$QT_PLUGINS_DIR/$d"/*.so; do
            [ -f "$f" ] && _copy_plugin "$f" "$d"
        done
    done

    # ── qt.conf ─────────────────────────────────────────────────────────────
    cat > "$APPDIR/usr/bin/qt.conf" <<'EOF'
[Paths]
Prefix = ..
Plugins = plugins
Libraries = lib
EOF

    # ── AppRun ──────────────────────────────────────────────────────────────
    cat > "$APPDIR/AppRun" <<'APPRUN'
#!/usr/bin/env bash
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="${HERE}/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
[ -z "${WAYLAND_DISPLAY:-}" ] && [ -z "${QT_QPA_PLATFORM:-}" ] && export QT_QPA_PLATFORM=xcb
export FONTCONFIG_PATH="/etc/fonts"
export QT_AUTO_SCREEN_SCALE_FACTOR=1
exec "${HERE}/usr/bin/Prismalux_GUI" "$@"
APPRUN
    chmod +x "$APPDIR/AppRun"

    # ── .desktop + icona nella AppDir ───────────────────────────────────────
    cat > "$APPDIR/prismalux.desktop" <<DESK_EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Prismalux v${PRISMA_VERSION}
Comment=Pipeline Agenti · RAG · Matematica LaTeX · GPU nativa
Exec=Prismalux_GUI
Icon=prismalux
Terminal=false
Categories=Education;Science;Utility;
StartupWMClass=Prismalux_GUI
DESK_EOF

    if [ -f "$ICON_PNG" ]; then
        cp "$ICON_PNG" "$APPDIR/prismalux.png"
        cp "$ICON_PNG" "$APPDIR/usr/share/icons/hicolor/256x256/apps/prismalux.png"
        ln -sf "usr/share/icons/hicolor/256x256/apps/prismalux.png" "$APPDIR/.DirIcon" 2>/dev/null || true
    fi

    # ── Genera AppImage ─────────────────────────────────────────────────────
    rm -f "$APPIMAGE_OUT"
    ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMAGETOOL" \
        --no-appstream \
        "$APPDIR" "$APPIMAGE_OUT" 2>&1 | grep -v "^$" || true

    if [ -f "$APPIMAGE_OUT" ]; then
        APPIMAGE_SIZE=$(du -sh "$APPIMAGE_OUT" | cut -f1)
        ok "AppImage → $APPIMAGE_OUT  ($APPIMAGE_SIZE)"
    else
        warn "AppImage non generata (appimagetool ha fallito)"
    fi
fi

# ════════════════════════════════════════════════════════════════════════════
#  Riepilogo hardware + output finale
# ════════════════════════════════════════════════════════════════════════════
step "Riepilogo..."

CPU_MODEL=$(grep -m1 "model name" /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs || echo "N/D")
RAM_GB=$(awk '/MemTotal/{printf "%.0f", $2/1024/1024}' /proc/meminfo 2>/dev/null || echo "N/D")
info "CPU: $CPU_MODEL  ($NPROC core)"
info "RAM: ${RAM_GB} GB"
if command -v nvidia-smi &>/dev/null; then
    GPU_INFO=$(nvidia-smi --query-gpu=name,memory.total --format=csv,noheader 2>/dev/null | head -1 || echo "N/D")
    info "GPU (NVIDIA): $GPU_INFO  → Ollama la usa automaticamente"
elif command -v rocm-smi &>/dev/null; then
    info "GPU (AMD ROCm rilevata)  → assicurati che Ollama sia compilato con ROCm"
elif ls /sys/class/drm/card*/device/vendor 2>/dev/null | xargs grep -l "0x1002" &>/dev/null; then
    info "GPU (AMD via DRM)  → avvia ollama con: OLLAMA_NUM_GPU=99 ollama serve"
else
    info "GPU: usa 'ollama list' per verificare il backend in uso"
fi

echo ""
echo -e "${B}════════════════════════════════════════════════════════${N}"
echo -e "${B}  Prismalux v${PRISMA_VERSION} — Pronto!${N}"
echo -e "${B}════════════════════════════════════════════════════════${N}"
echo ""
echo -e "  ${G}Eseguibile ${N}→  $GUI_BIN  ($BIN_SIZE)"
echo -e "  ${G}Launcher   ${N}→  bash $LAUNCHER"
[ -f "$APPIMAGE_OUT" ] && echo -e "  ${G}AppImage   ${N}→  $APPIMAGE_OUT  ($APPIMAGE_SIZE)"
echo -e "  ${G}Menu KDE   ${N}→  cerca \"Prismalux\" nelle applicazioni"
[ -f "$HOME/Desktop/Prismalux.desktop" ] && \
    echo -e "  ${G}Desktop    ${N}→  $HOME/Desktop/Prismalux.desktop"
echo ""
echo -e "  ${C}Prima di avviare Prismalux, avvia Ollama:${N}"
echo -e "    ollama serve          # usa GPU automaticamente se rilevata"
echo -e "    ollama list           # modelli disponibili"
echo ""
if [ "$OPT_NATIVE" = "1" ]; then
    echo -e "  ${Y}Binario ottimizzato con -march=native — specifico per questo CPU${N}"
    echo -e "  ${Y}Non copiarlo su altri PC con CPU diverso${N}"
    echo ""
fi
echo -e "  ${C}Ricompila dopo aggiornamenti:${N}"
echo -e "    bash compila_kubuntu.sh --solo-build"
echo ""

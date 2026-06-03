#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  compila.sh — Compila Prismalux v2.9 su Kubuntu / Ubuntu 22.04-26.04
#
#  Uso:
#    bash compila.sh              # installa dipendenze Qt6 + Python + compila
#    bash compila.sh --solo-build # salta apt e pip (già fatto)
#    bash compila.sh --solo-pip   # salta apt e cmake, installa solo pip
#    bash compila.sh --help
#
#  Al termine trovi il binario in:  build_gui/Prismalux_GUI
# ═══════════════════════════════════════════════════════════════════════════
set -euo pipefail

# ── Colori ──────────────────────────────────────────────────────────────────
R='\033[0;31m'; G='\033[0;32m'; Y='\033[0;33m'
C='\033[0;36m'; B='\033[1;37m'; N='\033[0m'

ok()   { echo -e "${G}  [OK]  $*${N}"; }
info() { echo -e "${C}  --    $*${N}"; }
warn() { echo -e "${Y}  [!]   $*${N}"; }
fail() { echo -e "${R}  [ERR] $*${N}"; exit 1; }
step() { echo -e "\n${B}▶ $*${N}"; }

# ── Argomenti ───────────────────────────────────────────────────────────────
DO_DEPS=1
DO_PIP=1
DO_BUILD=1
for arg in "$@"; do
    case "$arg" in
        --solo-build) DO_DEPS=0; DO_PIP=0 ;;
        --solo-pip)   DO_DEPS=0; DO_BUILD=0 ;;
        --help|-h)
            echo "Uso: bash compila.sh [opzione]"
            echo ""
            echo "  (nessuno)    installa Qt6 + Python pip + compila (consigliato prima volta)"
            echo "  --solo-build salta apt e pip, solo cmake (già tutto installato)"
            echo "  --solo-pip   installa/aggiorna solo i pacchetti pip"
            exit 0 ;;
        *) warn "Opzione sconosciuta: $arg (ignorata)" ;;
    esac
done

# ── Percorsi ────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GUI_SRC="$SCRIPT_DIR/gui"
BUILD_DIR="$SCRIPT_DIR/build_gui"
GUI_BIN="$BUILD_DIR/Prismalux_GUI"
NPROC=$(nproc 2>/dev/null || echo 2)

# ── Versione dal CMakeLists ──────────────────────────────────────────────────
VERSIONE=$(grep -m1 'project(Prismalux_GUI VERSION' "$GUI_SRC/CMakeLists.txt" \
           | grep -oE '[0-9]+\.[0-9]+' | head -1 2>/dev/null || echo "2.9")

echo ""
echo -e "${B}╔══════════════════════════════════════════════════════════╗${N}"
echo -e "${B}║       Prismalux v${VERSIONE} — Build Kubuntu/Ubuntu           ║${N}"
echo -e "${B}╚══════════════════════════════════════════════════════════╝${N}"
echo ""

DISTRO=$(grep PRETTY_NAME /etc/os-release 2>/dev/null | cut -d= -f2 | tr -d '"' || echo "Linux")
info "Distro : $DISTRO"
info "CPU    : $NPROC thread"
info "Sorgenti: $GUI_SRC"
echo ""

# ════════════════════════════════════════════════════════════════════════════
#  1. Dipendenze
# ════════════════════════════════════════════════════════════════════════════
if [ "$DO_DEPS" = "1" ]; then
    step "Installazione dipendenze Qt6..."
    info "Richiede la password sudo (una sola volta)"
    echo ""

    sudo apt-get update -qq

    # Pacchetti obbligatori
    sudo apt-get install -y \
        build-essential \
        cmake \
        libgl-dev \
        libglib2.0-dev \
        qt6-base-dev \
        qt6-base-dev-tools \
        qt6-tools-dev \
        qt6-tools-dev-tools
    ok "Qt6 base installato"

    # Pacchetti opzionali (gli errori sono ignorati)
    OPZIONALI=(
        "libqt6sql6-dev"           # GraphMemory + quiz CCNA SQLite
        "qt6-multimedia-dev"       # livello microfono reale (QAudioSource)
        "libqt6multimedia6"
        "libqt6keychain-dev"       # token LAN nel keyring di sistema
        "qtkeychain-qt6-dev"
        "libssl-dev"               # connessioni HTTPS
        "graphviz"                 # visualizzazione grafi Multi-Agente e RAG
        "poppler-utils"            # pdftotext per Analisi Fenomeni
        "python3"                  # MCPs Python
        "python3-pip"
    )

    info "Pacchetti opzionali (gli errori sono ignorati)..."
    for pkg in "${OPZIONALI[@]}"; do
        sudo apt-get install -y "$pkg" 2>/dev/null \
            && ok "  $pkg" \
            || warn "  $pkg non disponibile su questa versione (funzionalita' opzionale)"
    done

    # Qt6 WebEngine (LaTeX KaTeX) — richiede ~400 MB, chiede conferma
    echo ""
    echo -e "  ${C}Qt6 WebEngine abilita il rendering LaTeX (formule Analisi 1/2).${N}"
    echo -e "  ${C}Richiede circa 400 MB di download. Senza, le formule mostrano testo normale.${N}"
    printf "\n  Installare Qt6 WebEngine? [S/n] "
    read -r _ans </dev/tty || _ans="s"
    case "$_ans" in
        [nN]) warn "Qt6 WebEngine saltato" ;;
        *)
            sudo apt-get install -y qt6-webengine-dev libqt6webenginewidgets6 2>/dev/null \
                && ok "Qt6 WebEngine installato" \
                || warn "Qt6 WebEngine non disponibile su questa versione"
            sudo apt-get install -y libjs-katex 2>/dev/null \
                && ok "libjs-katex installato" \
                || warn "libjs-katex non trovato — LaTeX usera' il fallback testo"
            ;;
    esac

    ok "Dipendenze Qt6 completate"
fi

# ════════════════════════════════════════════════════════════════════════════
#  2. Pacchetti Python (MCPs e strumenti)
# ════════════════════════════════════════════════════════════════════════════
if [ "$DO_PIP" = "1" ]; then
    step "Installazione pacchetti Python (MCPs)..."

    # Verifica python3 disponibile
    if ! command -v python3 &>/dev/null; then
        warn "python3 non trovato — pacchetti Python saltati"
    else
        PY_VER=$(python3 --version 2>&1)
        info "Python: $PY_VER"

        # Funzione pip_install — gestisce Ubuntu 24.04+ (externally-managed)
        pip_install() {
            local args=("$@")
            if python3 -m pip install "${args[@]}" -q 2>/dev/null; then
                return 0
            elif python3 -m pip install "${args[@]}" -q --break-system-packages 2>/dev/null; then
                return 0
            else
                return 1
            fi
        }

        # ── Pacchetti core (sempre installati) ──────────────────────────────
        info "Pacchetti core..."
        CORE_PKGS=(
            "requests>=2.31.0"          # HTTP client
            "numpy>=1.26.0"             # array numerici
            "scipy>=1.12.0"             # algebra lineare, statistica
            "sympy>=1.12.0"             # matematica simbolica (test CAT-E)
            "pandas>=2.2.0"             # dataframe, analisi dati
            "matplotlib>=3.8.0"         # grafici 2D/3D
            "seaborn>=0.13.0"           # grafici statistici
            "plotly>=5.20.0"            # grafici interattivi HTML
            "opencv-python>=4.9.0"      # computer vision
            "Pillow>=10.0.0"            # immagini PIL
            "scikit-learn>=1.4.0"       # machine learning classico
        )
        pip_install "${CORE_PKGS[@]}" \
            && ok "  pacchetti core installati" \
            || warn "  errore pacchetti core — prova manualmente: pip install ${CORE_PKGS[*]}"

        # ── Documenti Office e PDF ───────────────────────────────────────────
        info "Documenti Office e PDF..."
        DOC_PKGS=(
            "openpyxl>=3.1.0"           # Excel .xlsx
            "python-docx>=1.0.0"        # Word .docx
            "python-pptx>=0.6.23"       # PowerPoint .pptx
            "pdfminer.six>=20221105"    # estrazione testo PDF
            "pypdf>=4.0.0"              # lettura PDF
        )
        pip_install "${DOC_PKGS[@]}" \
            && ok "  Office/PDF installati" \
            || warn "  errore Office/PDF"

        # ── MCPs specifici ───────────────────────────────────────────────────
        info "MCPs specifici..."
        MCP_PKGS=(
            "graphviz>=0.20.0"          # MCP Graphviz (Python binding)
            "obsws-python>=1.7.0"       # MCP OBS WebSocket
            "pyserial>=3.5"             # MCP TinyMCP (Arduino seriale)
            "gns3fy>=0.8.0"             # MCP GNS3
            "avogadro>=0.0.1"           # MCP Avogadro (chimica)
        )
        for pkg in "${MCP_PKGS[@]}"; do
            pip_install "$pkg" \
                && ok "  $pkg" \
                || warn "  $pkg non installato (MCP opzionale)"
        done

        # ── RDKit (chemioinformatica) — opzionale, può richiedere conda ─────
        if pip_install "rdkit>=2023.9.0" 2>/dev/null; then
            ok "  rdkit (chemioinformatica)"
        else
            warn "  rdkit non installabile via pip — installa con: sudo apt install python3-rdkit"
        fi

        # ── LibreOffice UNO (MCP Office modalità diretta) ───────────────────
        sudo apt-get install -y python3-uno libreoffice-common 2>/dev/null \
            && ok "  python3-uno (LibreOffice UNO)" \
            || warn "  python3-uno non installato (MCP Office in modalita' file)"

        # ── Stable Diffusion (GPU, ~10 GB) — chiede conferma ────────────────
        echo ""
        echo -e "  ${C}Stable Diffusion richiede circa 10 GB (GPU NVIDIA consigliata).${N}"
        echo -e "  ${C}Senza, il tab Stable Diffusion non funziona.${N}"
        printf "\n  Installare Stable Diffusion (diffusers + torch)? [s/N] "
        read -r _sd_ans </dev/tty || _sd_ans="n"
        case "$_sd_ans" in
            [sS])
                info "Download torch + diffusers (~10 GB, attendere)..."
                pip_install \
                    "diffusers>=0.27.0" \
                    "transformers>=4.40.0" \
                    "accelerate>=0.30.0" \
                    "torch>=2.2.0" \
                    "Pillow>=10.0.0" \
                    && ok "Stable Diffusion installato" \
                    || warn "Errore installazione Stable Diffusion"
                ;;
            *) warn "Stable Diffusion saltato" ;;
        esac

        # ── Installa dai requirements.txt del progetto (se presenti) ────────
        REQ_MAIN="$SCRIPT_DIR/requirements.txt"
        if [ -f "$REQ_MAIN" ]; then
            info "requirements.txt principale..."
            pip_install -r "$REQ_MAIN" \
                && ok "  requirements.txt principale" \
                || warn "  alcuni pacchetti in requirements.txt non installati"
        fi

        # requirements.txt per ogni MCP
        if [ -d "$SCRIPT_DIR/MCPs" ]; then
            info "requirements.txt dei MCPs..."
            while IFS= read -r req; do
                mcp_name=$(basename "$(dirname "$req")")
                # Salta requirements.txt vuoti o solo commenti
                if grep -qE '^[^#]' "$req" 2>/dev/null; then
                    pip_install -r "$req" 2>/dev/null \
                        && ok "  MCPs/$mcp_name" \
                        || warn "  MCPs/$mcp_name — alcuni pacchetti non installati"
                fi
            done < <(find "$SCRIPT_DIR/MCPs" -name "requirements.txt" | sort)
        fi

        ok "Pacchetti Python completati"
    fi
fi

# ════════════════════════════════════════════════════════════════════════════
#  4. Compilazione cmake
# ════════════════════════════════════════════════════════════════════════════
if [ "$DO_BUILD" = "0" ]; then
    echo ""
    ok "Opzione --solo-pip: compilazione saltata"
    exit 0
fi

step "Compilazione GUI Qt6..."

[ -d "$GUI_SRC" ] || fail "Cartella sorgenti non trovata: $GUI_SRC"

mkdir -p "$BUILD_DIR"
LOG="$BUILD_DIR/build.log"

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    info "Configurazione cmake (prima esecuzione)..."
    cmake -B "$BUILD_DIR" "$GUI_SRC" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O2" \
        2>&1 | tee "$LOG" | grep -E "(STATUS|error:|WARN|Qt6|Qt5|fatal)" || true
    [ "${PIPESTATUS[0]}" -eq 0 ] || fail "cmake configure fallito — vedi: $LOG"
else
    info "Cache cmake esistente — solo rebuild"
fi

T0=$(date +%s)
info "Build con $NPROC thread..."
cmake --build "$BUILD_DIR" -j"$NPROC" 2>&1 | tee -a "$LOG" | \
    grep -E "(Building|Linking|error:|fatal)" | grep -v "note:" || true
[ "${PIPESTATUS[0]}" -eq 0 ] || fail "Compilazione fallita — vedi: $LOG"
T1=$(date +%s)

[ -f "$GUI_BIN" ] || fail "Binario non trovato dopo la build: $GUI_BIN"
BIN_SIZE=$(du -sh "$GUI_BIN" | cut -f1)
ok "Compilato in $(( T1 - T0 ))s  —  $GUI_BIN  ($BIN_SIZE)"

# ════════════════════════════════════════════════════════════════════════════
#  5. Crea launcher + voce nel menu KDE
# ════════════════════════════════════════════════════════════════════════════
step "Launcher e menu KDE..."

LAUNCHER="$SCRIPT_DIR/avvia_prismalux.sh"
cat > "$LAUNCHER" <<LAUNCH
#!/usr/bin/env bash
cd "\$(cd "\$(dirname "\$(readlink -f "\$0")")" && pwd)"
exec "$GUI_BIN" "\$@"
LAUNCH
chmod +x "$LAUNCHER"
ok "Launcher  -> $LAUNCHER"

ICON="$SCRIPT_DIR/ICONA/prismalux.png"
[ -f "$ICON" ] || ICON="application-x-executable"

APPS_DIR="$HOME/.local/share/applications"
mkdir -p "$APPS_DIR"
cat > "$APPS_DIR/Prismalux.desktop" <<DESK
[Desktop Entry]
Version=1.0
Type=Application
Name=Prismalux v${VERSIONE}
Comment=Pipeline Agenti AI, RAG, Matematica LaTeX, MCPs
Exec=$LAUNCHER
Icon=$ICON
Terminal=false
Categories=Education;Science;Utility;
StartupWMClass=Prismalux_GUI
DESK
chmod +x "$APPS_DIR/Prismalux.desktop"

if [ -d "$HOME/Desktop" ]; then
    cp "$APPS_DIR/Prismalux.desktop" "$HOME/Desktop/Prismalux.desktop"
    chmod +x "$HOME/Desktop/Prismalux.desktop"
    gio set "$HOME/Desktop/Prismalux.desktop" metadata::trusted true 2>/dev/null || true
    ok "Desktop   -> $HOME/Desktop/Prismalux.desktop"
fi

update-desktop-database "$APPS_DIR" 2>/dev/null || true
kbuildsycoca6 2>/dev/null || kbuildsycoca5 2>/dev/null || true
ok "Menu KDE  -> cerca \"Prismalux\" nelle applicazioni"

# ════════════════════════════════════════════════════════════════════════════
#  Riepilogo finale
# ════════════════════════════════════════════════════════════════════════════
echo ""
echo -e "${B}════════════════════════════════════════════════════════${N}"
echo -e "${B}  Prismalux v${VERSIONE} pronto!${N}"
echo -e "${B}════════════════════════════════════════════════════════${N}"
echo ""
echo -e "  ${G}Binario  ${N}->  $GUI_BIN  ($BIN_SIZE)"
echo -e "  ${G}Avvio   ${N}->  bash $LAUNCHER"
echo ""
echo -e "  ${C}Prima di avviare Prismalux, avvia Ollama:${N}"
echo -e "    ollama serve        # usa GPU se disponibile"
echo -e "    ollama list         # modelli disponibili"
echo ""
echo -e "  ${C}Ricompila dopo aggiornamenti sorgenti:${N}"
echo -e "    bash compila.sh --solo-build"
echo ""

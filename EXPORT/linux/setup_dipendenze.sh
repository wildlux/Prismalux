#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════
#  Prismalux — Setup dipendenze Linux
#  Installa: FUSE 2, Python 3, pip e i pacchetti Python necessari
#
#  Uso:
#    bash setup_dipendenze.sh              # installa tutto
#    bash setup_dipendenze.sh --solo-fuse  # solo FUSE 2 (per AppImage)
#    bash setup_dipendenze.sh --solo-pip   # solo pacchetti Python
#    bash setup_dipendenze.sh --base       # solo pacchetti essenziali (rapido)
#
#  Compatibile con: Ubuntu 22.04 / 24.04 / 26.04 e derivate (Kubuntu, Xubuntu…)
# ═══════════════════════════════════════════════════════════════════

set -euo pipefail

# ── Colori ────────────────────────────────────────────────────────
RED='\033[0;31m'; GRN='\033[0;32m'; CYN='\033[0;36m'
YLW='\033[0;33m'; BLD='\033[1m'; NC='\033[0m'

ok()   { echo -e "${GRN}  [OK]${NC}    $*"; }
info() { echo -e "${CYN}  ──${NC}      $*"; }
warn() { echo -e "${YLW}  [WARN]${NC}  $*"; }
err()  { echo -e "${RED}  [ERRORE]${NC} $*"; }
step() { echo -e "\n${BLD}${CYN}▶ $*${NC}"; }

# ── Argomenti ─────────────────────────────────────────────────────
SOLO_FUSE=false
SOLO_PIP=false
SOLO_BASE=false

for arg in "$@"; do
    case "$arg" in
        --solo-fuse) SOLO_FUSE=true ;;
        --solo-pip)  SOLO_PIP=true  ;;
        --base)      SOLO_BASE=true ;;
        --help|-h)
            echo "Uso: bash setup_dipendenze.sh [--solo-fuse | --solo-pip | --base]"
            echo "  (nessun argomento) = installa tutto"
            exit 0 ;;
    esac
done

# ── Root del progetto (script in EXPORT/linux/ → risale di 2 livelli) ────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
REQ_MAIN="$ROOT/requirements.txt"
REQ_MCP_DIR="$ROOT/MCPs"

echo ""
echo -e "${BLD}╔══════════════════════════════════════════════════════╗${NC}"
echo -e "${BLD}║        Prismalux — Setup dipendenze Linux            ║${NC}"
echo -e "${BLD}╚══════════════════════════════════════════════════════╝${NC}"
echo ""
info "Root progetto : $ROOT"
echo ""

# ════════════════════════════════════════════════════════════════
#  1. FUSE 2 (necessario per avviare l'AppImage senza APPIMAGE_EXTRACT_AND_RUN)
# ════════════════════════════════════════════════════════════════
install_fuse() {
    step "Installazione FUSE 2..."

    if ldconfig -p 2>/dev/null | grep -q "libfuse.so.2"; then
        ok "libfuse2 già presente — AppImage funzionerà con doppio clic"
        return
    fi

    if ! command -v apt-get &>/dev/null; then
        warn "apt non trovato — installa manualmente libfuse2 o libfuse2t64"
        return
    fi

    info "Richiede password sudo..."
    # Ubuntu 24.04+ usa libfuse2t64, versioni precedenti usano libfuse2
    if apt-cache show libfuse2t64 &>/dev/null 2>&1; then
        sudo apt-get install -y libfuse2t64
    else
        sudo apt-get install -y libfuse2
    fi
    ok "FUSE 2 installato — ora puoi avviare l'AppImage con doppio clic"
}

# ════════════════════════════════════════════════════════════════
#  2. Python 3 e pip
# ════════════════════════════════════════════════════════════════
check_python() {
    step "Verifica Python 3..."

    if command -v python3 &>/dev/null; then
        PY_VER=$(python3 --version 2>&1)
        ok "Python trovato: $PY_VER"
    else
        warn "Python 3 non trovato — installazione..."
        sudo apt-get install -y python3
    fi

    if command -v pip3 &>/dev/null || python3 -m pip --version &>/dev/null 2>&1; then
        ok "pip disponibile"
    else
        warn "pip non trovato — installazione..."
        sudo apt-get install -y python3-pip
    fi
}

# ════════════════════════════════════════════════════════════════
#  3. Installazione pacchetti pip
# ════════════════════════════════════════════════════════════════

# Rileva se pip è "externally managed" (Ubuntu 24.04+) e scegli il metodo
pip_install() {
    local req_file="$1"
    local label="$2"

    if [ ! -f "$req_file" ]; then
        warn "File non trovato: $req_file"
        return
    fi

    info "Installo: $label"

    # Prova prima senza flag, poi con --break-system-packages se necessario
    if python3 -m pip install -r "$req_file" -q 2>/dev/null; then
        ok "$label installati"
    elif python3 -m pip install -r "$req_file" -q --break-system-packages 2>/dev/null; then
        ok "$label installati (--break-system-packages)"
    else
        warn "Alcuni pacchetti di '$label' potrebbero non essere stati installati."
        warn "Prova manualmente: pip install -r $req_file"
    fi
}

install_pip_base() {
    step "Installazione pacchetti base (rapida)..."
    info "Installo: requests, numpy, scipy, sympy, pandas, matplotlib"
    if python3 -m pip install requests numpy scipy sympy pandas matplotlib -q 2>/dev/null || \
       python3 -m pip install requests numpy scipy sympy pandas matplotlib -q --break-system-packages 2>/dev/null; then
        ok "Pacchetti base installati"
    else
        warn "Errore installazione base — prova: pip install requests numpy scipy sympy pandas matplotlib"
    fi
}

install_pip_tutti() {
    step "Installazione pacchetti Python completi..."
    info "Questo può richiedere 10-20 minuti alla prima esecuzione."
    echo ""

    # requirements.txt principale
    pip_install "$REQ_MAIN" "requirements.txt principale"

    # requirements.txt per ogni MCP
    step "Installazione requirements MCP (plugin)..."
    for req in "$REQ_MCP_DIR"/*/requirements.txt; do
        if [ -f "$req" ]; then
            mcp_name=$(basename "$(dirname "$req")")
            pip_install "$req" "MCPs/$mcp_name"
        fi
    done
}

# ════════════════════════════════════════════════════════════════
#  Esecuzione in base agli argomenti
# ════════════════════════════════════════════════════════════════

if $SOLO_FUSE; then
    install_fuse

elif $SOLO_PIP; then
    check_python
    install_pip_tutti

elif $SOLO_BASE; then
    check_python
    install_pip_base

else
    # Tutto
    install_fuse
    check_python
    install_pip_tutti
fi

# ════════════════════════════════════════════════════════════════
#  Riepilogo finale
# ════════════════════════════════════════════════════════════════
echo ""
echo -e "${BLD}${GRN}╔══════════════════════════════════════════════════════╗${NC}"
echo -e "${BLD}${GRN}║              Setup completato!                       ║${NC}"
echo -e "${BLD}${GRN}╚══════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  Per avviare Prismalux:"
echo -e "  ${CYN}./Prismalux-x86_64.AppImage${NC}          (se hai FUSE 2)"
echo -e "  ${CYN}APPIMAGE_EXTRACT_AND_RUN=1 ./Prismalux-x86_64.AppImage${NC}  (senza FUSE)"
echo ""

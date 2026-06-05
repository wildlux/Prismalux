#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════
#  avvia_docker.sh — Avvia Prismalux in Docker con supporto X11/Wayland
#
#  Uso:
#    bash DOCKER/avvia_docker.sh           # avvio normale
#    bash DOCKER/avvia_docker.sh --build   # rebuild immagine + avvio
#    bash DOCKER/avvia_docker.sh --shell   # entra nel container (debug)
#    bash DOCKER/avvia_docker.sh --nvidia  # abilita GPU NVIDIA
#    bash DOCKER/avvia_docker.sh --amd     # abilita GPU AMD via /dev/dri
# ═══════════════════════════════════════════════════════════════════════
set -euo pipefail

R='\033[0;31m'; G='\033[0;32m'; Y='\033[0;33m'
C='\033[0;36m'; B='\033[1;37m'; N='\033[0m'

ok()   { echo -e "${G}  ✅ $*${N}"; }
info() { echo -e "${C}  ──  $*${N}"; }
warn() { echo -e "${Y}  ⚠   $*${N}"; }
fail() { echo -e "${R}  ❌  $*${N}"; exit 1; }
step() { echo -e "\n${B}▶ $*${N}"; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

DO_BUILD=0
DO_SHELL=0
GPU_NVIDIA=0
GPU_AMD=0

for arg in "$@"; do
    case "$arg" in
        --build)  DO_BUILD=1 ;;
        --shell)  DO_SHELL=1 ;;
        --nvidia) GPU_NVIDIA=1 ;;
        --amd)    GPU_AMD=1 ;;
        --help|-h)
            echo "Uso: bash DOCKER/avvia_docker.sh [opzioni]"
            echo "  --build   Ricostruisce l'immagine prima di avviare"
            echo "  --shell   Apre una shell nel container (debug)"
            echo "  --nvidia  Abilita GPU NVIDIA (richiede nvidia-container-toolkit)"
            echo "  --amd     Abilita GPU AMD via /dev/dri"
            exit 0 ;;
    esac
done

echo ""
echo -e "${B}╔══════════════════════════════════════════════════════════╗${N}"
echo -e "${B}║          Prismalux — Avvio Docker                        ║${N}"
echo -e "${B}╚══════════════════════════════════════════════════════════╝${N}"
echo ""

# ── Verifica Docker ──────────────────────────────────────────────────
command -v docker &>/dev/null || fail "Docker non trovato — installa con: sudo apt install docker.io"
docker info &>/dev/null || fail "Docker non in esecuzione — avvia con: sudo systemctl start docker"

# ── Verifica Ollama sul host ─────────────────────────────────────────
if curl -sf http://localhost:11434/api/tags &>/dev/null; then
    ok "Ollama in ascolto su localhost:11434"
else
    warn "Ollama non risponde su localhost:11434"
    warn "Avvia prima: ollama serve"
    echo ""
    printf "  Continuare comunque? [s/N] "
    read -r ans </dev/tty || ans="n"
    [[ "$ans" =~ ^[sS]$ ]] || exit 0
fi

# ── Build immagine ───────────────────────────────────────────────────
if [ "$DO_BUILD" = "1" ] || ! docker image inspect prismalux:latest &>/dev/null; then
    step "Build immagine Docker (può richiedere 10-20 min alla prima esecuzione)..."
    docker build \
        -t prismalux:latest \
        -f "$SCRIPT_DIR/Dockerfile" \
        "$ROOT" \
        && ok "Immagine prismalux:latest pronta" \
        || fail "Build Docker fallita"
fi

# ── Permesso X11 ─────────────────────────────────────────────────────
step "Configurazione X11..."
if command -v xhost &>/dev/null; then
    xhost +local:docker &>/dev/null && ok "xhost: accesso locale Docker abilitato" || true
fi

# ── Argomenti GPU ────────────────────────────────────────────────────
GPU_ARGS=()
if [ "$GPU_NVIDIA" = "1" ]; then
    GPU_ARGS=(--gpus all)
    info "GPU NVIDIA abilitata (--gpus all)"
elif [ "$GPU_AMD" = "1" ]; then
    GPU_ARGS=(--device /dev/dri)
    info "GPU AMD abilitata (/dev/dri)"
fi

# ── CMD override ────────────────────────────────────────────────────
CMD_OVERRIDE="Prismalux_GUI"
[ "$DO_SHELL" = "1" ] && CMD_OVERRIDE="bash"

# ── Volume dati ─────────────────────────────────────────────────────
docker volume create prismalux_data &>/dev/null || true

# ── Avvio container ──────────────────────────────────────────────────
step "Avvio Prismalux..."
info "DISPLAY=$DISPLAY"
info "Rete: host (Ollama su localhost:11434 raggiungibile)"
[ "$DO_SHELL" = "1" ] && info "Modalità: shell interattiva"
echo ""

# GPU AMD/Intel via DRI (aggiunto di default — ignorato se /dev/dri non esiste)
DRI_ARGS=()
[ -d /dev/dri ] && DRI_ARGS=(--device /dev/dri)

docker run --rm -it \
    --name prismalux \
    --network host \
    -e DISPLAY="${DISPLAY:-:0}" \
    -e WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-}" \
    -e QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}" \
    -e QT_AUTO_SCREEN_SCALE_FACTOR=1 \
    -e XAUTHORITY="${XAUTHORITY:-}" \
    -e XDG_RUNTIME_DIR=/tmp/runtime-prismalux \
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
    -v "${XAUTHORITY:-/dev/null}:/home/prismalux/.Xauthority:ro" \
    -v prismalux_data:/data/prismalux \
    "${DRI_ARGS[@]}" \
    "${GPU_ARGS[@]}" \
    prismalux:latest \
    "$CMD_OVERRIDE"

echo ""
ok "Sessione Prismalux terminata"

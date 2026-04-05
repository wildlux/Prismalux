#!/usr/bin/env bash
# avvia_qt.sh — Avvia llama-server + Prismalux GUI Qt con un solo click
# Uso: ./avvia_qt.sh              → Qwen2.5-3B (default, qualità alta)
#      ./avvia_qt.sh smol         → SmolLM2-135M (velocissimo, leggero)
#      ./avvia_qt.sh stop         → ferma tutto

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

LLAMA_BIN="$SCRIPT_DIR/llama_cpp_studio/llama.cpp/build/bin"
MODELS_DIR="$SCRIPT_DIR/C_software/models"
GUI_BIN="$SCRIPT_DIR/Qt_GUI/build/Prismalux_GUI"
PID_FILE="/tmp/prismalux_llama_server.pid"
PORT=8081

MODEL_QWEN="$MODELS_DIR/Qwen2.5-3B-Instruct-Q4_K_M.gguf"
MODEL_SMOL="$MODELS_DIR/SmolLM2-135M-Instruct-Q4_K_M.gguf"

# ── Colori ────────────────────────────────────────────────────────
GRN='\033[0;32m'; YLW='\033[0;33m'; RED='\033[0;31m'; CYN='\033[0;36m'; RST='\033[0m'

ferma_server() {
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if kill -0 "$PID" 2>/dev/null; then
            echo -e "${YLW}  Fermo llama-server (PID $PID)...${RST}"
            kill "$PID"
        fi
        rm -f "$PID_FILE"
    fi
    # Pulisce anche eventuali server orfani sulla porta
    fuser -k ${PORT}/tcp 2>/dev/null
}

# ── Comando stop ──────────────────────────────────────────────────
if [ "$1" = "stop" ]; then
    ferma_server
    echo -e "${GRN}  Fermato.${RST}"
    exit 0
fi

# ── Selezione modello ─────────────────────────────────────────────
if [ "$1" = "smol" ]; then
    MODEL="$MODEL_SMOL"
    MODEL_NAME="SmolLM2-135M"
else
    MODEL="$MODEL_QWEN"
    MODEL_NAME="Qwen2.5-3B"
fi

echo -e "\n${CYN}  🍺 Prismalux — Avvio GUI Qt${RST}"
echo -e "${CYN}  ─────────────────────────────────────────${RST}"

# ── Controlli prerequisiti ────────────────────────────────────────
if [ ! -f "$LLAMA_BIN/llama-server" ]; then
    echo -e "${RED}  ✗ llama-server non trovato in:${RST}"
    echo -e "    $LLAMA_BIN"
    echo -e "${YLW}  Compila prima con il menu 'Compila llama.cpp' dentro llama_cpp_studio.${RST}"
    exit 1
fi

if [ ! -f "$MODEL" ]; then
    echo -e "${RED}  ✗ Modello non trovato: $MODEL_NAME${RST}"
    echo -e "${YLW}  Scaricalo da Prismalux → Impostazioni → Scarica modello.${RST}"
    exit 1
fi

if [ ! -f "$GUI_BIN" ]; then
    echo -e "${RED}  ✗ GUI Qt non compilata in:${RST}"
    echo -e "    $GUI_BIN"
    echo -e "${YLW}  Compila con: cd Qt_GUI && ./build.sh${RST}"
    exit 1
fi

# ── Ferma eventuale server precedente ────────────────────────────
ferma_server

# ── Avvia llama-server ────────────────────────────────────────────
echo -e "  🦙 Carico modello: ${CYN}${MODEL_NAME}${RST}"
echo -e "  📡 Porta: ${CYN}${PORT}${RST}"

export LD_LIBRARY_PATH="$LLAMA_BIN:$LD_LIBRARY_PATH"

"$LLAMA_BIN/llama-server" \
    -m "$MODEL" \
    --port $PORT \
    --host 127.0.0.1 \
    --log-disable \
    > /tmp/prismalux_llama.log 2>&1 &

SERVER_PID=$!
echo $SERVER_PID > "$PID_FILE"

# ── Attende che il server sia pronto ─────────────────────────────
echo -ne "  ⏳ Attendo server"
for i in $(seq 1 30); do
    sleep 1
    if curl -sf "http://127.0.0.1:${PORT}/health" >/dev/null 2>&1; then
        echo -e "\r  ${GRN}✓ Server pronto${RST}                  "
        break
    fi
    echo -ne "."
    if [ $i -eq 30 ]; then
        echo -e "\r  ${RED}✗ Timeout — server non risponde${RST}"
        echo -e "${YLW}  Log: /tmp/prismalux_llama.log${RST}"
        cat /tmp/prismalux_llama.log | tail -5
        exit 1
    fi
done

# ── Avvia GUI Qt ──────────────────────────────────────────────────
echo -e "  🖥️  Avvio GUI..."
echo -e "${CYN}  ─────────────────────────────────────────${RST}"
echo -e "${YLW}  In Manutenzione scegli:${RST}"
echo -e "    Backend → llama-server  |  Porta → ${PORT}"
echo -e "${CYN}  ─────────────────────────────────────────${RST}\n"

"$GUI_BIN" &

# ── Quando la GUI si chiude, ferma anche il server ───────────────
GUI_PID=$!
wait $GUI_PID
echo -e "\n${YLW}  GUI chiusa — fermo llama-server...${RST}"
ferma_server
echo -e "${GRN}  Tutto fermato. Alla prossima libagione di sapere. 🍺${RST}\n"

#!/bin/bash
# ══════════════════════════════════════════════════════════════
#  Prismalux — Build script (TUI statica con llama.cpp)
#  Aggiornato: 2026-03-28
# ══════════════════════════════════════════════════════════════
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

echo ""
echo "🍺 Prismalux — Build TUI con llama.cpp statico"
echo "──────────────────────────────────────────────"

# ── 1. Clona llama.cpp se non esiste ─────────────────────────
if [ ! -d "llama.cpp" ]; then
    echo "📥 Clone llama.cpp..."
    git clone --depth=1 https://github.com/ggml-org/llama.cpp llama.cpp
else
    echo "✓ llama.cpp già presente"
fi

# ── 2. Compila llama.cpp con cmake ───────────────────────────
LLAMA_LIB_1="llama.cpp/build/libllama.a"
LLAMA_LIB_2="llama.cpp/build/src/libllama.a"

if [ ! -f "$LLAMA_LIB_1" ] && [ ! -f "$LLAMA_LIB_2" ]; then
    echo "🔨 Compilazione llama.cpp (5-15 minuti)..."

    CMAKE_GPU_FLAGS=""
    echo ""
    echo "🔍 Rilevamento GPU acceleratori..."

    if command -v nvidia-smi &>/dev/null; then
        echo "  ✓ NVIDIA GPU → abilita CUDA"
        CMAKE_GPU_FLAGS="$CMAKE_GPU_FLAGS -DGGML_CUDA=ON"
    fi
    if command -v rocm-smi &>/dev/null || [ -d /opt/rocm ]; then
        echo "  ✓ AMD ROCm → abilita HIP/ROCm"
        CMAKE_GPU_FLAGS="$CMAKE_GPU_FLAGS -DGGML_HIPBLAS=ON"
    fi
    if command -v vulkaninfo &>/dev/null; then
        echo "  ✓ Vulkan → abilita Vulkan backend"
        CMAKE_GPU_FLAGS="$CMAKE_GPU_FLAGS -DGGML_VULKAN=ON"
    fi
    if [ -z "$CMAKE_GPU_FLAGS" ]; then
        echo "  ℹ  Nessuna GPU — build CPU-only (OpenMP)"
        CMAKE_GPU_FLAGS="-DGGML_OPENMP=ON"
    fi
    echo ""

    cmake -S llama.cpp -B llama.cpp/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DGGML_NATIVE=ON \
        -DLLAMA_BUILD_TESTS=OFF \
        -DLLAMA_BUILD_EXAMPLES=OFF \
        $CMAKE_GPU_FLAGS
    cmake --build llama.cpp/build --config Release -j"$(nproc 2>/dev/null || echo 4)"
    echo "✓ llama.cpp compilato"
else
    echo "✓ llama.cpp già compilato"
fi

# ── 3. Librerie statiche .a ───────────────────────────────────
LLAMA_LIBS=$(find llama.cpp/build -name "*.a" 2>/dev/null | sort | tr '\n' ' ')
if [ -z "$LLAMA_LIBS" ]; then
    echo "❌ Nessuna libreria .a trovata in llama.cpp/build/"
    echo "   Rimuovi llama.cpp/build/ e rilanciare lo script."
    exit 1
fi
echo "📚 Librerie: $(echo "$LLAMA_LIBS" | wc -w | tr -d ' ') file .a"

# ── 4. Verifica API llama.h ───────────────────────────────────
echo "🔍 Verifica API llama.h..."
LLAMA_H="llama.cpp/include/llama.h"
if grep -q "llama_load_model_from_file" "$LLAMA_H" 2>/dev/null; then
    echo "   ✓ API legacy (llama_load_model_from_file)"
elif grep -q "llama_model_load_from_file" "$LLAMA_H" 2>/dev/null; then
    echo "   ⚠  API nuova (llama_model_load_from_file) — aggiorno wrapper..."
    sed -i 's/llama_load_model_from_file/llama_model_load_from_file/g' src/llama_wrapper.cpp
    sed -i 's/llama_free_model/llama_model_free/g' src/llama_wrapper.cpp
    echo "   ✓ src/llama_wrapper.cpp aggiornato"
else
    echo "   ⚠  Versione API sconosciuta — proseguo con API legacy"
fi

# ── 5. Compila llama_wrapper.cpp ─────────────────────────────
echo "🔧 Compilazione src/llama_wrapper.cpp..."
g++ -O2 -std=c++17 \
    -I llama.cpp/include \
    -I llama.cpp/ggml/include \
    -I include \
    -c src/llama_wrapper.cpp -o src/llama_wrapper.o

# ── 6. Compila tutti i sorgenti C ────────────────────────────
echo "🔧 Compilazione sorgenti C..."
CFLAGS="-O2 -Wall -Iinclude -Wno-stringop-truncation -Wno-format-truncation"

C_SRCS=(
    src/hw_detect.c
    src/agent_scheduler.c
    src/prismalux_ui.c
    src/backend.c
    src/terminal.c
    src/http.c
    src/ai.c
    src/modelli.c
    src/output.c
    src/multi_agente.c
    src/strumenti.c
    src/quiz.c
    src/config_toon.c
    src/context_store.c
    src/key_router.c
    src/rag.c
    src/rag_embed.c
    src/sim_common.c
    src/sim_sort.c
    src/sim_search.c
    src/sim_math.c
    src/sim_dp.c
    src/sim_grafi.c
    src/sim_tech.c
    src/sim_stringhe.c
    src/sim_strutture.c
    src/sim_greedy.c
    src/sim_backtrack.c
    src/sim_llm.c
)

C_OBJS=()
for SRC in "${C_SRCS[@]}"; do
    if [ -f "$SRC" ]; then
        OBJ="${SRC%.c}.o"
        gcc $CFLAGS -c "$SRC" -o "$OBJ"
        C_OBJS+=("$OBJ")
    else
        echo "  ⚠  $SRC non trovato — saltato"
    fi
done

# ── 7. Compila main.c con -DWITH_LLAMA_STATIC ─────────────────
echo "🔧 Compilazione src/main.c (statico)..."
gcc $CFLAGS -DWITH_LLAMA_STATIC -c src/main.c -o src/main.o

# ── 8. Link finale ────────────────────────────────────────────
echo "🔗 Linking..."
g++ -O2 -o prismalux \
    src/main.o \
    "${C_OBJS[@]}" \
    src/llama_wrapper.o \
    -Wl,--start-group $LLAMA_LIBS -Wl,--end-group \
    -lpthread -lm -ldl -lgomp \
    $(pkg-config --libs cublas 2>/dev/null || true)

# ── 9. Crea cartella models ───────────────────────────────────
mkdir -p models

BIN_SIZE="$(du -sh prismalux 2>/dev/null | cut -f1 || echo '?')"

echo ""
echo "✅ Build TUI completata!"
echo ""
echo "   Binario  : $(pwd)/prismalux  ($BIN_SIZE)"
echo ""
echo "   Come usarlo:"
echo "   1. Metti un file .gguf in: $(pwd)/models/"
echo "   2. Avvia:  ./prismalux"
echo ""
echo "   Modelli consigliati (da Hugging Face — cerca 'gguf Q4_K_M'):"
echo "   · Qwen2.5-3B-Instruct-Q4_K_M.gguf        (~2.0 GB)"
echo "   · Phi-3-mini-4k-instruct-q4.gguf          (~2.2 GB)"
echo "   · mistral-7b-instruct-v0.2.Q4_K_M.gguf    (~4.1 GB)"
echo "   · Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf  (~4.9 GB)"
echo ""
echo "   Download rapido:"
echo "   pip install huggingface_hub"
echo "   huggingface-cli download Qwen/Qwen2.5-3B-Instruct-GGUF \\"
echo "       Qwen2.5-3B-Instruct-Q4_K_M.gguf --local-dir models/"
echo ""

# ── 10. Build Qt GUI (opzionale) ─────────────────────────────
echo "──────────────────────────────────────────────"
echo "  Vuoi compilare anche la Qt GUI? (necessita Qt6)"
echo "  [S/n]: "
read -r -t 10 REPLY || REPLY="n"
case "$REPLY" in
    [Ss]|"")
        if [ -f "Qt_GUI/build.sh" ]; then
            echo ""
            bash Qt_GUI/build.sh
        else
            echo "  ⚠  Qt_GUI/build.sh non trovato — GUI saltata."
        fi
        ;;
    *)
        echo "  → GUI saltata. Per compilarla: cd Qt_GUI && ./build.sh"
        ;;
esac
echo ""

# ── 11. Compila whisper-cli (opzionale — per riconoscimento vocale) ──
echo "──────────────────────────────────────────────"
echo "  Vuoi compilare whisper.cpp (riconoscimento vocale)? [s/N]: "
read -r -t 15 REPLY || REPLY="n"
case "$REPLY" in
    [Ss])
        WHISPER_SRC="$DIR/whisper.cpp"
        WHISPER_CLI="$WHISPER_SRC/build/bin/whisper-cli"
        if [ -f "$WHISPER_CLI" ]; then
            echo "  ✓ whisper-cli già presente: $WHISPER_CLI"
        else
            echo "📥 Clone whisper.cpp..."
            [ ! -d "$WHISPER_SRC/.git" ] && \
                git clone --depth=1 https://github.com/ggml-org/whisper.cpp "$WHISPER_SRC"
            echo "🔨 Build whisper-cli..."
            cmake -B "$WHISPER_SRC/build" -S "$WHISPER_SRC" \
                -DCMAKE_BUILD_TYPE=Release \
                -DWHISPER_BUILD_TESTS=OFF \
                -DWHISPER_BUILD_EXAMPLES=ON \
                -DBUILD_SHARED_LIBS=OFF
            cmake --build "$WHISPER_SRC/build" --target whisper-cli -j"$(nproc 2>/dev/null || echo 4)"
            [ -f "$WHISPER_CLI" ] && echo "✅ whisper-cli → $WHISPER_CLI" || echo "❌ Build fallita."
        fi
        ;;
    *)
        echo "  → whisper saltato. Per compilarlo: ./aggiorna.sh --build-whisper"
        ;;
esac
echo ""

# ── 12. Compila llama-server + llama-cli per GUI (opzionale) ──────────
echo "──────────────────────────────────────────────"
echo "  Vuoi compilare llama-server + llama-cli (GUI → AI Locale)? [s/N]: "
read -r -t 15 REPLY || REPLY="n"
case "$REPLY" in
    [Ss])
        LLAMA_STUDIO_SRC="$DIR/llama_cpp_studio/llama.cpp"
        LLAMA_SERVER_BIN="$LLAMA_STUDIO_SRC/build/bin/llama-server"
        if [ -f "$LLAMA_SERVER_BIN" ]; then
            echo "  ✓ llama-server già presente: $LLAMA_SERVER_BIN"
        else
            CMAKE_GPU_FLAGS=""
            command -v nvidia-smi &>/dev/null && CMAKE_GPU_FLAGS="-DGGML_CUDA=ON"
            [ -d /opt/rocm ] && CMAKE_GPU_FLAGS="$CMAKE_GPU_FLAGS -DGGML_HIPBLAS=ON"
            [ -z "$CMAKE_GPU_FLAGS" ] && CMAKE_GPU_FLAGS="-DGGML_OPENMP=ON"

            if [ ! -d "$LLAMA_STUDIO_SRC/.git" ]; then
                mkdir -p "$(dirname "$LLAMA_STUDIO_SRC")"
                echo "📥 Clone llama.cpp (llama-studio)..."
                git clone --depth=1 https://github.com/ggml-org/llama.cpp "$LLAMA_STUDIO_SRC"
            fi
            echo "🔨 Build llama-server + llama-cli..."
            cmake -B "$LLAMA_STUDIO_SRC/build" -S "$LLAMA_STUDIO_SRC" \
                -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
                -DGGML_NATIVE=ON -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=ON \
                $CMAKE_GPU_FLAGS
            cmake --build "$LLAMA_STUDIO_SRC/build" \
                --target llama-server llama-cli -j"$(nproc 2>/dev/null || echo 4)"
            [ -f "$LLAMA_SERVER_BIN" ] && \
                echo "✅ llama-server → $LLAMA_SERVER_BIN" || \
                echo "❌ Build fallita."
        fi
        ;;
    *)
        echo "  → llama-studio saltato. Per compilarlo: ./aggiorna.sh --llama-studio"
        ;;
esac
echo ""

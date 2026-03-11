#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

echo ""
echo "🍺 Prismalux — Build con llama.cpp statico"
echo "──────────────────────────────────────────"

# 1. Clona llama.cpp se non esiste
if [ ! -d "llama.cpp" ]; then
    echo "📥 Clone llama.cpp..."
    git clone --depth=1 https://github.com/ggml-org/llama.cpp llama.cpp
else
    echo "✓ llama.cpp già presente"
fi

# 2. Compila llama.cpp con cmake
LLAMA_LIB_CANDIDATE_1="llama.cpp/build/libllama.a"
LLAMA_LIB_CANDIDATE_2="llama.cpp/build/src/libllama.a"

if [ ! -f "$LLAMA_LIB_CANDIDATE_1" ] && [ ! -f "$LLAMA_LIB_CANDIDATE_2" ]; then
    echo "🔨 Compilazione llama.cpp (può richiedere 5-15 minuti)..."

    # ── Rilevamento GPU per cmake ──────────────────────────────────────────
    CMAKE_GPU_FLAGS=""

    echo ""
    echo "🔍 Rilevamento GPU acceleratori..."

    if command -v nvidia-smi &>/dev/null; then
        echo "  ✓ NVIDIA GPU rilevata → abilita CUDA"
        CMAKE_GPU_FLAGS="$CMAKE_GPU_FLAGS -DGGML_CUDA=ON"
    fi

    if command -v rocm-smi &>/dev/null || [ -d /opt/rocm ]; then
        echo "  ✓ AMD ROCm rilevato → abilita HIP/ROCm"
        CMAKE_GPU_FLAGS="$CMAKE_GPU_FLAGS -DGGML_HIPBLAS=ON"
    fi

    if command -v vulkaninfo &>/dev/null; then
        echo "  ✓ Vulkan rilevato → abilita Vulkan backend"
        CMAKE_GPU_FLAGS="$CMAKE_GPU_FLAGS -DGGML_VULKAN=ON"
    fi

    if [ -z "$CMAKE_GPU_FLAGS" ]; then
        echo "  ℹ  Nessuna GPU accelerata rilevata — build CPU-only (OpenMP)"
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

# 3. Raccogli tutte le librerie statiche .a prodotte da cmake
LLAMA_LIBS=$(find llama.cpp/build -name "*.a" 2>/dev/null | sort | tr '\n' ' ')
if [ -z "$LLAMA_LIBS" ]; then
    echo "❌ Nessuna libreria .a trovata in llama.cpp/build/"
    echo "   Prova a rimuovere la cartella llama.cpp/build/ e rilanciare lo script."
    exit 1
fi
echo "📚 Librerie trovate: $(echo "$LLAMA_LIBS" | wc -w | tr -d ' ') file .a"

# 4. Verifica API llama.h: controlla quale nome funzione è presente
echo "🔍 Verifica API llama.h..."
LLAMA_H="llama.cpp/include/llama.h"
if grep -q "llama_load_model_from_file" "$LLAMA_H" 2>/dev/null; then
    echo "   ✓ API legacy (llama_load_model_from_file) — wrapper compatibile"
elif grep -q "llama_model_load_from_file" "$LLAMA_H" 2>/dev/null; then
    echo "   ⚠  API nuova (llama_model_load_from_file) rilevata."
    echo "   Aggiornamento automatico llama_wrapper.cpp..."
    sed -i 's/llama_load_model_from_file/llama_model_load_from_file/g' llama_wrapper.cpp
    sed -i 's/llama_free_model/llama_model_free/g' llama_wrapper.cpp
    echo "   ✓ llama_wrapper.cpp aggiornato alle API nuove."
else
    echo "   ⚠  Impossibile rilevare versione API — proseguo con le API legacy."
fi

# 5. Compila llama_wrapper.cpp
echo "🔧 Compilazione wrapper C++..."
g++ -O2 -std=c++17 \
    -I llama.cpp/include \
    -I llama.cpp/ggml/include \
    -c llama_wrapper.cpp -o llama_wrapper.o

# 6. Compila hw_detect.c
echo "🔧 Compilazione hw_detect.c..."
gcc -O2 -Wall -c hw_detect.c -o hw_detect.o

# 7. Compila prismalux_ui.c
echo "🔧 Compilazione prismalux_ui.c..."
gcc -O2 -Wall -I . -c prismalux_ui.c -o prismalux_ui.o

# 8. Compila prismalux.c
echo "🔧 Compilazione prismalux.c..."
gcc -O2 -Wall -DWITH_LLAMA_STATIC \
    -I . \
    -c prismalux.c -o prismalux.o

# 9. Link finale
echo "🔗 Linking..."
g++ -O2 -o prismalux \
    prismalux.o prismalux_ui.o llama_wrapper.o hw_detect.o \
    -Wl,--start-group $LLAMA_LIBS -Wl,--end-group \
    -lpthread -lm -ldl -lgomp $(pkg-config --libs cublas 2>/dev/null || true)

# 8. Crea cartella models se non esiste
mkdir -p models

# Calcola dimensione binario
BIN_SIZE="$(du -sh prismalux 2>/dev/null | cut -f1 || echo '?')"

echo ""
echo "✅ Build completato!"
echo ""
echo "   Binario: $(pwd)/prismalux  ($BIN_SIZE)"
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
echo "   Download rapido con huggingface-cli:"
echo "   pip install huggingface_hub"
echo "   huggingface-cli download Qwen/Qwen2.5-3B-Instruct-GGUF \\"
echo "       Qwen2.5-3B-Instruct-Q4_K_M.gguf --local-dir models/"
echo ""

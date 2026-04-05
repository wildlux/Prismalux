CC       = gcc
CXX      = g++
CFLAGS   = -O2 -Wall -Iinclude
CXXFLAGS = -O2 -std=c++17 -Wall -Iinclude -Illama.cpp/include -Illama.cpp/ggml/include
TARGET   = prismalux

# ── Sorgenti principali ───────────────────────────────────────────────────────
SRCS = src/main.c \
       src/backend.c \
       src/terminal.c \
       src/http.c \
       src/ai.c \
       src/modelli.c \
       src/output.c \
       src/multi_agente.c \
       src/strumenti.c \
       src/rag.c \
       src/rag_embed.c \
       src/jlt.c \
       src/jlt_rag.c \
       src/jlt_index.c \
       src/config_toon.c \
       src/context_store.c \
       src/key_router.c \
       src/hw_detect.c \
       src/agent_scheduler.c \
       src/prismalux_ui.c \
       src/simulatore.c \
       src/quiz.c

OBJS = $(SRCS:.c=.o)

# ── Moduli del Simulatore Algoritmi (split da simulatore.c) ───────────────────
SIM_OBJ = src/sim_common.o \
          src/sim_sort.o \
          src/sim_search.o \
          src/sim_math.o \
          src/sim_dp.o \
          src/sim_grafi.o \
          src/sim_tech.o \
          src/sim_stringhe.o \
          src/sim_strutture.o \
          src/sim_greedy.o \
          src/sim_backtrack.o \
          src/sim_llm.o

# Tutte le .a prodotte da cmake (build statica)
LLAMA_LIBS = $(shell find llama.cpp/build -name "*.a" 2>/dev/null | sort | tr '\n' ' ')

.PHONY: all static http models clean check-api vram_bench vram_bench_static windows test test_cs \
        test_bench test_perf test_sha256 test_memory test_rag test_rag_bench test_security test_golden test_version \
        test_sim_smoke jlt_demo jlt_speedup_demo jlt_speed_test

# ── Default: HTTP-only (Ollama + llama-server), nessuna dipendenza ───────────
all: http

http: $(OBJS) $(SIM_OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(SIM_OBJ) -lpthread -lm
	@echo ""
	@echo "  $(TARGET) compilato \xe2\x80\x94 modalita HTTP"
	@echo "   ./$(TARGET) --backend ollama"
	@echo "   ./$(TARGET) --backend llama-server --port 8080"
	@echo "   ./$(TARGET) --help"

# ── Johnson–Lindenstrauss Transforms demo (standalone, zero dipendenze) ──────
jlt_demo: src/jlt.c src/jlt_demo.c include/jlt.h
	$(CC) $(CFLAGS) -o jlt_demo src/jlt.c src/jlt_demo.c -lm
	@echo ""
	@echo "  jlt_demo compilato — arXiv:2103.00564"
	@echo "  Uso: ./jlt_demo [seed]"

jlt_speedup_demo: src/jlt.c src/jlt_speedup_demo.c include/jlt.h
	$(CC) $(CFLAGS) -o jlt_speedup_demo src/jlt.c src/jlt_speedup_demo.c -lm
	@echo "  jlt_speedup_demo compilato — misura speedup 7x senza Ollama"

# ── JLT Ollama Speed Test (benchmark reale con chiamate Ollama) ───────────
jlt_speed_test: jlt_ollama_speed_test.c \
                src/rag_embed.c src/jlt.c src/jlt_rag.c src/jlt_index.c src/http.c
	$(CC) $(CFLAGS) -o jlt_speed_test jlt_ollama_speed_test.c \
	    src/rag_embed.c src/jlt.c src/jlt_rag.c src/jlt_index.c src/http.c \
	    -lpthread -lm
	@echo ""
	@echo "  jlt_speed_test compilato — benchmark Ollama reale: rag_embed vs JLT"
	@echo "  Uso: ./jlt_speed_test   (richiede Ollama + modello embedding)"

# Regola generica per .c → .o (con dipendenze sugli header in include/)
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── Build con llama.cpp statico (richiede ./build.sh prima) ──────────────────
static: src/llama_wrapper.o $(OBJS) $(SIM_OBJ)
	@if [ -z "$(LLAMA_LIBS)" ]; then echo "Esegui prima: ./build.sh"; exit 1; fi
	$(CC) $(CFLAGS) -DWITH_LLAMA_STATIC -c src/main.c -o src/_main_static.o
	$(CXX) -O2 -o $(TARGET) \
	    $(filter-out src/main.o, $(OBJS)) src/_main_static.o src/llama_wrapper.o \
	    $(SIM_OBJ) \
	    -Wl,--start-group $(LLAMA_LIBS) -Wl,--end-group \
	    -lpthread -lm -ldl -lgomp
	rm -f src/_main_static.o
	@echo ""
	@echo "  $(TARGET) compilato \xe2\x80\x94 llama.cpp statico"
	@echo "   Metti i .gguf in: models/"

src/llama_wrapper.o: src/llama_wrapper.cpp include/llama_wrapper.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ── vram_bench: benchmark VRAM (HTTP/Ollama, nessuna dipendenza) ──────────────
vram_bench: src/vram_bench.c src/agent_scheduler.o src/hw_detect.o
	$(CC) $(CFLAGS) -o vram_bench src/vram_bench.c src/agent_scheduler.o src/hw_detect.o -lpthread -lm
	@echo ""
	@echo "  vram_bench compilato \xe2\x80\x94 modalita Ollama HTTP"
	@echo "   ./vram_bench                         # profila tutti i modelli Ollama"
	@echo "   ./vram_bench --model mistral:7b       # solo un modello"
	@echo "   ./vram_bench --output mio_profilo.json"

# ── vram_bench_static: con llama.cpp statico ───────────────────────────────────
vram_bench_static: src/vram_bench.c src/llama_wrapper.o src/agent_scheduler.o src/hw_detect.o
	@if [ -z "$(LLAMA_LIBS)" ]; then echo "Esegui prima: ./build.sh"; exit 1; fi
	$(CC) $(CFLAGS) -DWITH_LLAMA_STATIC -c src/vram_bench.c -o src/_vb_static.o
	$(CXX) -O2 -o vram_bench_static src/_vb_static.o src/llama_wrapper.o src/agent_scheduler.o src/hw_detect.o \
	    -Wl,--start-group $(LLAMA_LIBS) -Wl,--end-group \
	    -lpthread -lm -ldl -lgomp
	rm -f src/_vb_static.o
	@echo ""
	@echo "  vram_bench_static compilato"
	@echo "   ./vram_bench_static --backend llama --models-dir models/"

models:
	mkdir -p models

# Verifica API llama.cpp clonato
check-api:
	@if grep -q "llama_load_model_from_file" llama.cpp/include/llama.h 2>/dev/null; then \
	    echo "API: legacy  llama_load_model_from_file -- wrapper OK"; \
	elif grep -q "llama_model_load_from_file" llama.cpp/include/llama.h 2>/dev/null; then \
	    echo "API: nuova   llama_model_load_from_file -- aggiorna src/llama_wrapper.cpp"; \
	else \
	    echo "llama.h non trovato -- esegui prima ./build.sh"; \
	fi

# ── Test automatici del motore AI ────────────────────────────────────────────
test: test_engine
	./test_engine

test_engine: test_engine.c src/http.c src/backend.c src/ai.c src/terminal.c \
             src/hw_detect.c src/agent_scheduler.c src/prismalux_ui.c
	$(CC) $(CFLAGS) -o test_engine $^ -lpthread -lm

test_multiagente: test_multiagente.c src/http.c src/backend.c src/ai.c \
                  src/terminal.c src/hw_detect.c src/agent_scheduler.c src/prismalux_ui.c
	$(CC) $(CFLAGS) -o test_multiagente $^ -lpthread -lm

# ── Test hardware platform (Xeon RAM + Nvidia GPU) ───────────────────────────
test_hw_platform: test_hw_platform.c src/hw_detect.c
	$(CC) $(CFLAGS) -o test_hw_platform $^ -lpthread -lm

test_hw: test_hw_platform
	./test_hw_platform --all

# ── Context Store semantico (prototipo ricerca) ───────────────────────────
test_cs: src/context_store.o src/key_router.o test_context_store.c
	$(CC) $(CFLAGS) -o test_cs test_context_store.c \
	    src/context_store.o src/key_router.o
	@echo "  test_cs compilato — ./test_cs"

test_cs_random: src/context_store.o src/key_router.o test_cs_random.c
	$(CC) $(CFLAGS) -o test_cs_random test_cs_random.c \
	    src/context_store.o src/key_router.o
	@echo "  test_cs_random compilato — ./test_cs_random"

test_perf: test_perf.c src/agent_scheduler.c src/hw_detect.c
	$(CC) $(CFLAGS) -o test_perf test_perf.c \
	    src/agent_scheduler.c src/hw_detect.c -lm
	@echo "  test_perf compilato — ./test_perf"

test_sha256: test_sha256.c
	$(CC) -O2 -o test_sha256 test_sha256.c -lm
	@echo "  test_sha256 compilato — ./test_sha256"

test_memory: test_memory.c src/agent_scheduler.c src/hw_detect.c \
             src/context_store.c src/key_router.c
	$(CC) $(CFLAGS) -o test_memory test_memory.c \
	    src/agent_scheduler.c src/hw_detect.c \
	    src/context_store.c src/key_router.c -lm
	@echo "  test_memory compilato — ./test_memory"

# ── Benchmark unificato (timer + scheduler + RAG TF + RAG Embed + streaming) ──
test_bench: test_bench.c src/rag.c src/rag_embed.c src/http.c \
            src/agent_scheduler.c src/hw_detect.c
	$(CC) $(CFLAGS) -o test_bench test_bench.c \
	    src/rag.c src/rag_embed.c src/http.c \
	    src/agent_scheduler.c src/hw_detect.c \
	    -lpthread -lm
	@echo "  test_bench compilato — ./test_bench"
	@echo "   Output: ~/.prismalux_bench.json"

test_rag: test_rag.c src/rag.c
	$(CC) $(CFLAGS) -o test_rag test_rag.c src/rag.c
	@echo "  test_rag compilato — ./test_rag"

test_rag_bench: test_rag_bench.c src/rag.c src/rag_embed.c src/jlt.c src/jlt_rag.c src/jlt_index.c src/http.c
	$(CC) $(CFLAGS) -o test_rag_bench test_rag_bench.c \
	    src/rag.c src/rag_embed.c src/jlt.c src/jlt_rag.c src/jlt_index.c src/http.c -lpthread -lm
	@echo "  test_rag_bench compilato — ./test_rag_bench (tri-engine: TF + Embed + JLT-SRHT)"

test_security: test_security.c
	$(CC) -O2 -o test_security test_security.c -lm
	@echo "  test_security compilato — ./test_security"

test_golden: test_golden.c
	$(CC) -O2 -o test_golden test_golden.c -lm
	@echo "  test_golden compilato — ./test_golden"

test_version: test_version.c
	$(CC) -O2 -o test_version test_version.c -lm
	@echo "  test_version compilato — ./test_version"

test_sim_smoke: test_sim_smoke.c $(SIM_OBJ) \
               src/terminal.o src/backend.o src/prismalux_ui.o \
               src/hw_detect.o src/http.o src/ai.o src/agent_scheduler.o
	$(CC) $(CFLAGS) -o test_sim_smoke test_sim_smoke.c \
	    $(SIM_OBJ) \
	    src/terminal.o src/backend.o src/prismalux_ui.o \
	    src/hw_detect.o src/http.o src/ai.o src/agent_scheduler.o \
	    -lpthread -lm
	@echo "  test_sim_smoke compilato — ./test_sim_smoke"

clean:
	rm -f $(TARGET) vram_bench vram_bench_static test_engine test_multiagente \
	      test_hw_platform test_bench test_cs test_cs_random test_perf test_sha256 \
	      test_memory test_rag test_rag_bench test_security test_golden test_version \
	      test_sim_smoke \
	      src/*.o *.o

windows:
	gcc -O2 -Wall -Iinclude -o prismalux.exe \
	    src/main.c src/backend.c src/terminal.c src/http.c src/ai.c \
	    src/modelli.c src/output.c src/multi_agente.c src/strumenti.c \
	    src/hw_detect.c src/agent_scheduler.c src/prismalux_ui.c \
	    src/simulatore.c src/quiz.c src/rag.c src/rag_embed.c \
	    src/sim_common.c src/sim_sort.c src/sim_search.c src/sim_math.c \
	    src/sim_dp.c src/sim_grafi.c src/sim_tech.c src/sim_stringhe.c \
	    src/sim_strutture.c src/sim_greedy.c src/sim_backtrack.c \
	    -lws2_32 -lm
	gcc -O2 -Wall -Iinclude -o vram_bench.exe \
	    src/vram_bench.c src/agent_scheduler.c src/hw_detect.c \
	    -lws2_32 -lm
	@echo "  prismalux.exe + vram_bench.exe -- Windows HTTP-only"

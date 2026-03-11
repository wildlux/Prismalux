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
       src/hw_detect.c \
       src/agent_scheduler.c \
       src/prismalux_ui.c

OBJS = $(SRCS:.c=.o)

# Tutte le .a prodotte da cmake (build statica)
LLAMA_LIBS = $(shell find llama.cpp/build -name "*.a" 2>/dev/null | sort | tr '\n' ' ')

.PHONY: all static http models clean check-api vram_bench vram_bench_static windows

# ── Default: HTTP-only (Ollama + llama-server), nessuna dipendenza ───────────
all: http

http: $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) -lpthread -lm
	@echo ""
	@echo "  $(TARGET) compilato \xe2\x80\x94 modalita HTTP"
	@echo "   ./$(TARGET) --backend ollama"
	@echo "   ./$(TARGET) --backend llama-server --port 8080"
	@echo "   ./$(TARGET) --help"

# Regola generica per .c → .o (con dipendenze sugli header in include/)
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── Build con llama.cpp statico (richiede ./build.sh prima) ──────────────────
static: src/llama_wrapper.o $(OBJS)
	@if [ -z "$(LLAMA_LIBS)" ]; then echo "Esegui prima: ./build.sh"; exit 1; fi
	$(CC) $(CFLAGS) -DWITH_LLAMA_STATIC -c src/main.c -o src/_main_static.o
	$(CXX) -O2 -o $(TARGET) \
	    $(filter-out src/main.o, $(OBJS)) src/_main_static.o src/llama_wrapper.o \
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

clean:
	rm -f $(TARGET) vram_bench vram_bench_static \
	      src/*.o *.o

windows:
	gcc -O2 -Wall -Iinclude -o prismalux.exe \
	    src/main.c src/backend.c src/terminal.c src/http.c src/ai.c \
	    src/modelli.c src/output.c src/multi_agente.c src/strumenti.c \
	    src/hw_detect.c src/agent_scheduler.c src/prismalux_ui.c \
	    -lws2_32 -lm
	gcc -O2 -Wall -Iinclude -o vram_bench.exe \
	    src/vram_bench.c src/agent_scheduler.c src/hw_detect.c \
	    -lws2_32 -lm
	@echo "  prismalux.exe + vram_bench.exe -- Windows HTTP-only"

CC       = gcc
CXX      = g++
CFLAGS   = -O2 -Wall
CXXFLAGS = -O2 -std=c++17 -Wall -I. -Illama.cpp/include -Illama.cpp/ggml/include
TARGET   = prismalux
SRC      = prismalux.c
HW_SRC   = hw_detect.c
HW_OBJ   = hw_detect.o
UI_SRC   = prismalux_ui.c
UI_OBJ   = prismalux_ui.o
AS_SRC   = agent_scheduler.c
AS_OBJ   = agent_scheduler.o

# Tutte le .a prodotte da cmake (build statica)
LLAMA_LIBS = $(shell find llama.cpp/build -name "*.a" 2>/dev/null | sort | tr '\n' ' ')

.PHONY: all static http models clean check-api vram_bench vram_bench_static

# ── Default: HTTP-only (Ollama + llama-server), nessuna dipendenza ───────────
all: http

http: $(SRC) $(HW_OBJ) $(UI_OBJ) $(AS_OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $< $(UI_OBJ) $(HW_OBJ) $(AS_OBJ) -lpthread -lm
	@echo ""
	@echo "✅  $(TARGET) compilato — modalità HTTP"
	@echo "   ./$(TARGET) --backend ollama"
	@echo "   ./$(TARGET) --backend llama-server --port 8080"
	@echo "   ./$(TARGET) --help"

$(UI_OBJ): $(UI_SRC) prismalux_ui.h hw_detect.h
	$(CC) $(CFLAGS) -c $(UI_SRC) -o $(UI_OBJ)

$(AS_OBJ): $(AS_SRC) agent_scheduler.h hw_detect.h
	$(CC) $(CFLAGS) -c $(AS_SRC) -o $(AS_OBJ)

# ── Build con llama.cpp statico (richiede ./build.sh prima) ──────────────────
static: llama_wrapper.o $(HW_OBJ) $(UI_OBJ) $(AS_OBJ)
	@if [ -z "$(LLAMA_LIBS)" ]; then echo "❌ Esegui prima: ./build.sh"; exit 1; fi
	$(CC) $(CFLAGS) -DWITH_LLAMA_STATIC -I. -c $(SRC) -o _prismalux_static.o
	$(CXX) -O2 -o $(TARGET) _prismalux_static.o llama_wrapper.o $(UI_OBJ) $(HW_OBJ) $(AS_OBJ) \
	    -Wl,--start-group $(LLAMA_LIBS) -Wl,--end-group \
	    -lpthread -lm -ldl -lgomp
	rm -f _prismalux_static.o
	@echo ""
	@echo "✅  $(TARGET) compilato — llama.cpp statico"
	@echo "   Metti i .gguf in: models/"

llama_wrapper.o: llama_wrapper.cpp llama_wrapper.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(HW_OBJ): $(HW_SRC) hw_detect.h
	$(CC) $(CFLAGS) -c $(HW_SRC) -o $(HW_OBJ)

# ── vram_bench: benchmark VRAM (HTTP/Ollama, nessuna dipendenza) ──────────────
vram_bench: vram_bench.c $(AS_OBJ) $(HW_OBJ)
	$(CC) $(CFLAGS) -o vram_bench vram_bench.c $(AS_OBJ) $(HW_OBJ) -lpthread -lm
	@echo ""
	@echo "✅  vram_bench compilato — modalità Ollama HTTP"
	@echo "   ./vram_bench                         # profila tutti i modelli Ollama"
	@echo "   ./vram_bench --model mistral:7b       # solo un modello"
	@echo "   ./vram_bench --output mio_profilo.json"

# ── vram_bench_static: con llama.cpp statico (misura VRAM locale .gguf) ───────
vram_bench_static: vram_bench.c llama_wrapper.o $(AS_OBJ) $(HW_OBJ)
	@if [ -z "$(LLAMA_LIBS)" ]; then echo "❌ Esegui prima: ./build.sh"; exit 1; fi
	$(CC) $(CFLAGS) -DWITH_LLAMA_STATIC -I. -c vram_bench.c -o _vb_static.o
	$(CXX) -O2 -o vram_bench_static _vb_static.o llama_wrapper.o $(AS_OBJ) $(HW_OBJ) \
	    -Wl,--start-group $(LLAMA_LIBS) -Wl,--end-group \
	    -lpthread -lm -ldl -lgomp
	rm -f _vb_static.o
	@echo ""
	@echo "✅  vram_bench_static compilato"
	@echo "   ./vram_bench_static --backend llama --models-dir models/"

models:
	mkdir -p models

# Verifica API llama.cpp clonato
check-api:
	@if grep -q "llama_load_model_from_file" llama.cpp/include/llama.h 2>/dev/null; then \
	    echo "API: legacy  llama_load_model_from_file — wrapper OK"; \
	elif grep -q "llama_model_load_from_file" llama.cpp/include/llama.h 2>/dev/null; then \
	    echo "API: nuova   llama_model_load_from_file — aggiorna llama_wrapper.cpp"; \
	else \
	    echo "llama.h non trovato — esegui prima ./build.sh"; \
	fi

clean:
	rm -f $(TARGET) vram_bench vram_bench_static \
	      _prismalux_static.o _vb_static.o \
	      llama_wrapper.o $(HW_OBJ) $(UI_OBJ) $(AS_OBJ)

windows:
	gcc -O2 -Wall -o prismalux.exe prismalux.c prismalux_ui.c hw_detect.c agent_scheduler.c -lws2_32 -lm
	gcc -O2 -Wall -o vram_bench.exe vram_bench.c agent_scheduler.c hw_detect.c -lws2_32 -lm
	@echo "✅ prismalux.exe + vram_bench.exe — Windows HTTP-only"

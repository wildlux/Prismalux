# Prismalux — Versione C

> *"Costruito per i mortali che aspirano alla saggezza."*

Versione C di Prismalux: un singolo binario compilato, **zero dipendenze runtime**,
che gira direttamente nel terminale senza Python, venv o pip.

---

## Struttura della cartella

```
C_software/
├── prismalux.c          ← sorgente principale (unico file C)
├── llama_wrapper.h      ← interfaccia C per llama.cpp
├── llama_wrapper.cpp    ← wrapper C++ attorno alle API di llama.cpp
├── Makefile             ← build: make http | make static
├── build.sh             ← script automatico clone + cmake + link
├── models/              ← metti qui i tuoi file .gguf
└── prismalux            ← binario già compilato (modalità HTTP)
```

---

## 3 backend selezionabili

Il programma supporta tre backend AI intercambiabili con il flag `--backend`:

| Backend | Comando | Requisito |
|---|---|---|
| `llama` | `./prismalux` | `./build.sh` + file `.gguf` in `models/` |
| `ollama` | `./prismalux --backend ollama` | Ollama in esecuzione (`ollama serve`) |
| `llama-server` | `./prismalux --backend llama-server` | llama-server in esecuzione |

### Differenze

- **llama (statico)** — l'AI gira _dentro_ il binario stesso. Nessun server,
  nessuna connessione di rete. Richiede compilazione con `./build.sh` e un file `.gguf`.
- **Ollama** — si connette via HTTP a `localhost:11434`. Basta avere Ollama installato
  e un modello scaricato (`ollama pull llama3.2`).
- **llama-server** — si connette via HTTP all'API OpenAI-compatibile di llama-server
  (default `localhost:8080`). Ottimo se vuoi controllare esattamente quale modello usa ogni agente.

---

## Come compilare

### Modalità HTTP — solo Ollama / llama-server (già compilato, zero attesa)

Richiede solo `gcc`, nessuna libreria extra:

```bash
cd ~/Desktop/Prismalux/C_software
make
# oppure direttamente:
gcc -O2 -Wall -o prismalux prismalux.c
```

Il binario `prismalux` pesa ~35 KB e non ha dipendenze runtime oltre alla libc standard.

### Modalità statica con llama.cpp (una volta sola, circa 10-15 minuti)

```bash
cd ~/Desktop/Prismalux/C_software
./build.sh
```

Lo script fa in automatico:
1. Clona llama.cpp da GitHub (`git clone --depth=1`)
2. Compila con cmake (`-DBUILD_SHARED_LIBS=OFF -DGGML_NATIVE=ON`)
3. Compila `llama_wrapper.cpp`
4. Linka tutto in un unico binario `prismalux`

Dopo la build il binario fa inferenza AI internamente — nessun server necessario.

---

## Esempi di avvio

```bash
# Ollama locale (porta default 11434)
./prismalux --backend ollama

# Ollama con modello specifico già selezionato
./prismalux --backend ollama --model deepseek-coder:33b

# llama-server su porta 8080
./prismalux --backend llama-server --port 8080

# llama-server su macchina remota
./prismalux --backend llama-server --host 192.168.1.10 --port 8080

# llama statico (dopo build.sh), directory modelli custom
./prismalux --models-dir /home/utente/modelli

# Aiuto completo
./prismalux --help
```

---

## Tutti i flag disponibili

| Flag | Descrizione | Default |
|---|---|---|
| `--backend` | `llama` / `ollama` / `llama-server` | `llama` |
| `--host` | Indirizzo del server HTTP | `127.0.0.1` |
| `--port` | Porta del server HTTP | `11434` (ollama) / `8080` (llama-server) |
| `--model` | Nome modello per HTTP backend | primo disponibile |
| `--models-dir` | Directory file `.gguf` per backend llama | `./models` |
| `--help` | Mostra questo aiuto | — |

---

## Scaricare un modello .gguf

Per il backend statico (llama), ti serve un file `.gguf` nella cartella `models/`.
Esempi consigliati da Hugging Face (cerca "gguf"):

| Modello | Dimensione | Uso |
|---|---|---|
| `Qwen2.5-3B-Instruct-Q4_K_M.gguf` | ~2 GB | veloce, buona qualità |
| `Phi-3-mini-4k-instruct-q4.gguf` | ~2.2 GB | ottimo su CPU |
| `mistral-7b-instruct-v0.2.Q4_K_M.gguf` | ~4.1 GB | qualità alta |
| `deepseek-coder-6.7b-instruct.Q4_K_M.gguf` | ~4 GB | specializzato codice |

Con `huggingface-cli` (opzionale):
```bash
pip install huggingface_hub
huggingface-cli download Qwen/Qwen2.5-3B-Instruct-GGUF \
    Qwen2.5-3B-Instruct-Q4_K_M.gguf --local-dir models/
```

---

## Funzionalità

### Menu principale

```
1. Multi-Agente AI    — pipeline 4 agenti, backend configurabile per agente
2. Strumento Pratico  — Dichiarazione 730 e Partita IVA con AI
3. Tutor AI           — chat con "Oracolo", domande libere
4. Cambia Modello     — seleziona modello per il backend attivo
5. Backend & Rete     — cambia backend, host, porta
Q. Esci
```

### Multi-Agente con backend per-agente

La funzione più potente: 4 agenti AI collaborano in sequenza sul tuo task.

```
Ricercatore  →  Programmatore  →  Critico  →  Ottimizzatore
```

All'avvio del Multi-Agente il programma chiede:
```
Configurare backend per ogni agente? [s/N]:
```

Se rispondi `s`, puoi assegnare ad ogni agente il suo backend, host, porta e modello.
Questo permette, ad esempio, di avere agenti specializzati su modelli diversi che
girano in parallelo come processi separati:

| Agente | Backend | Porta | Modello consigliato |
|---|---|---|---|
| Ricercatore | ollama | 11434 | llama3.2 |
| Programmatore | ollama | 11434 | deepseek-coder:33b |
| Critico | llama-server | 8080 | mistral-7b |
| Ottimizzatore | llama-server | 8081 | qwen2.5-coder |

Per avviare più istanze di llama-server su porte diverse:
```bash
llama-server -m models/mistral-7b.gguf    --port 8080 &
llama-server -m models/qwen2.5-coder.gguf --port 8081 &
./prismalux --backend ollama   # poi configura gli agenti dal menu
```

L'output finale viene salvato automaticamente in `../esportazioni/` (o nella
directory corrente se non esiste).

---

## Compilazione su Windows

Con MinGW/MSYS2 installato:

```bash
# Solo HTTP (Ollama/llama-server)
gcc -O2 -Wall -o prismalux.exe prismalux.c -lws2_32

# Con llama statico: usa build.sh dentro MSYS2 MINGW64 shell
```

---

## Vantaggi rispetto alla versione Python

| | Python (Prismalux) | C (questa versione) |
|---|---|---|
| Dipendenze | Python + pip + venv + 6 librerie | nessuna |
| Avvio | ~2-3 secondi | istantaneo |
| Distribuzione | intera cartella progetto | singolo file binario |
| Backend AI | solo Ollama | Ollama + llama-server + llama statico |
| Multi-agente | backend unico | backend diverso per ogni agente |
| Dimensione | ~50 MB (con venv) | ~35 KB (HTTP) / ~30 MB (statico) |

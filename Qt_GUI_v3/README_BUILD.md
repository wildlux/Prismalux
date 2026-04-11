# Prismalux GUI — Istruzioni di Build

## Prerequisiti

### Linux (Ubuntu/Debian)
```bash
sudo apt install qt6-base-dev cmake build-essential
```

### Linux (Arch/Manjaro)
```bash
sudo pacman -S qt6-base cmake gcc
```

### Windows (MSYS2 MINGW64)
```bash
pacman -S --needed mingw-w64-x86_64-qt6-base mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc
```

---

## Build

```bash
cd Qt_GUI
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Esegui
./build/Prismalux_GUI
```

### Build rapido (unico comando)
```bash
cd Qt_GUI && cmake -B build && cmake --build build && ./build/Prismalux_GUI
```

---

## Struttura progetto

```
Qt_GUI/
├── CMakeLists.txt           ← build system (Qt6/Qt5, AUTOMOC, hw_detect.c)
├── CLAUDE.md                ← guida architettura e convenzioni per sviluppo
├── main.cpp                 ← QApplication entry point
├── mainwindow.h/cpp         ← finestra principale (header · sidebar · stack)
├── hardware_monitor.h/cpp   ← polling CPU/RAM/GPU (usa ../C_software/src/hw_detect.c)
├── ai_client.h/cpp          ← client HTTP Ollama/llama-server (QNetworkAccessManager)
├── theme_manager.h/cpp      ← carica/salva tema QSS da themes/
├── prismalux_paths.h        ← unico punto di verità per percorsi e costanti
├── style.qss                ← tema dark cyan Prismalux v2
├── widgets/
│   ├── spinner_widget.h     ← spinner braille animato (Unicode + QTimer)
│   └── status_badge.h       ← dot colorato + etichetta stato (Offline/Online/Starting/Error)
├── pages/
│   ├── agenti_page.*        ← Pipeline 6 Agenti + Motore Byzantino anti-allucinazione
│   ├── pratico_page.*       ← 730, P.IVA, Cerca Lavoro
│   ├── impara_page.*        ← Tutor AI · Quiz · Dashboard Progressi
│   ├── personalizza_page.*  ← llama.cpp Studio: compila · modelli · server/chat
│   └── impostazioni_page.*  ← Backend · Modello · Info Hardware
└── themes/
    ├── dark_cyan.qss        ← tema default
    ├── dark_amber.qss
    ├── dark_purple.qss
    └── light.qss
```

---

## Dipendenze C backend (già incluse)

Il progetto compila automaticamente `../C_software/src/hw_detect.c`
per il rilevamento GPU (NVIDIA/AMD/Intel/ATI) senza librerie esterne.

Tutto il networking AI usa `QNetworkAccessManager` Qt nativo.

---

## Funzionalità implementate (v2.1)

| Sezione | Funzionalità | Stato |
|---------|---|---|
| **Header** | Gauge CPU/RAM/GPU in tempo reale | ✅ |
| | Toggle backend Ollama ↔ llama-server | ✅ |
| | Avvia/ferma llama-server + spinner animato | ✅ |
| | Profilo matematico (Xeon 64 GB, Q4/Q8) | ✅ |
| | Emergenza RAM 🚨 (ferma modelli + drop_caches) | ✅ |
| | Scorciatoie tastiera Alt+1/2/3 | ✅ |
| **Agenti AI** | Pipeline 6 agenti configurabile | ✅ |
| | Motore Byzantino anti-allucinazione | ✅ |
| | Auto-assegnazione ruoli via orchestratore LLM | ✅ |
| | Modalità Matematico Teorico | ✅ |
| | Supporto llama-server (endpoint /v1/models) | ✅ |
| **Finanza** | Assistente dichiarazione 730 | ✅ |
| | Calcolatore regime forfettario / P.IVA | ✅ |
| | Cerca Lavoro (ricerca web + CV reader) | ✅ |
| **Impara** | Tutor AI — Oracolo streaming | ✅ |
| | Quiz Interattivi | 🌫️ prossimamente |
| | Dashboard Progressi | 🌫️ prossimamente |
| **Impostazioni** | llama.cpp Studio (compila, modelli, server) | ✅ |
| | Rilevamento GPU NVIDIA/AMD/Intel cross-platform | ✅ |
| | Selezione backend/modello | ✅ |
| | Download modelli matematici da Hugging Face | ✅ |
| **Temi** | Dark Cyan / Dark Amber / Dark Purple / Light | ✅ |

---

## Variabili CMake

| Variabile | Significato |
|-----------|-------------|
| `PRISMALUX_ROOT` | Radice del progetto (iniettata automaticamente da CMake, usata da `prismalux_paths.h`) |
| `CMAKE_BUILD_TYPE` | `Release` per produzione, `Debug` per sviluppo |

---

## Note per lo sviluppo

- Leggi `CLAUDE.md` per convenzioni architetturali, pattern di codice e ottimizzazioni future.
- Tutti i percorsi usano `PrismaluxPaths` (`prismalux_paths.h`) — nessun hardcode.
- I widget in `widgets/*.h` con `Q_OBJECT` devono essere in `CPP_SRCS` del CMakeLists
  affinché AUTOMOC generi il vtable corretto.
- Per aggiornare il tema: modifica `style.qss` (o i file in `themes/`) — il CMake
  lo copia automaticamente nella directory di build a ogni configure.

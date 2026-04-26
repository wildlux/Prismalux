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

### Windows (MSYS2 UCRT64 — consigliato)
```bash
pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base \
                   mingw-w64-ucrt-x86_64-cmake \
                   mingw-w64-ucrt-x86_64-ninja \
                   mingw-w64-ucrt-x86_64-gcc
```
Poi esegui `gui/build.bat` (rilevamento Qt automatico).

---

## Build Linux

```bash
cd gui
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Esegui
./build/Prismalux_GUI
```

### Build rapido
```bash
cd gui && cmake -B build && cmake --build build -j$(nproc) && ./build/Prismalux_GUI
```

### Con test
```bash
cmake -B build_tests -DBUILD_TESTS=ON && cmake --build build_tests -j$(nproc)
```

---

## Build Windows

Doppio clic su `gui/build.bat` — rileva automaticamente Qt in MSYS2 UCRT64,
MSYS2 MINGW64 o Qt installer ufficiale (`C:\Qt\`).

---

## Struttura progetto

```
gui/
├── CMakeLists.txt           ← build system (Qt6/Qt5, AUTOMOC, hw_detect.c)
├── CLAUDE.md                ← architettura e convenzioni sviluppo
├── main.cpp                 ← QApplication entry point
├── mainwindow.h/cpp         ← finestra principale (header · sidebar · stack)
├── hardware_monitor.h/cpp   ← polling CPU/RAM/GPU ogni 2s
├── ai_client.h/cpp          ← client HTTP Ollama/llama-server
├── prismalux_paths.h        ← unico punto di verità per percorsi e costanti
├── hw_detect.c/h            ← rilevamento GPU NVIDIA/AMD/Intel cross-platform
├── widgets/                 ← componenti UI riusabili
│   ├── tts_speak.h          ← TTS: Piper (Linux) / SAPI PowerShell (Windows)
│   ├── chat_bubble.h/cpp    ← bolla chat con supporto TTS e copia
│   ├── spinner_widget.h     ← spinner braille animato
│   └── status_badge.h       ← dot stato Offline/Online/Starting/Error
└── pages/
    ├── agenti_page.*        ← Pipeline 6 Agenti + Motore Byzantino
    ├── manutenzione_page.*  ← Backend · Hardware · Bug Tracker · Cron
    ├── impostazioni_page.*  ← Connessione · LLM · Voce · Tema · RAG
    ├── grafico_page.*       ← Grafico 2D/3D (20+ tipi)
    ├── programmazione_page.*← Editor + Git + REPL
    ├── matematica_page.*    ← Matematica + Grafico
    ├── impara_page.*        ← Tutor AI · Sfida
    ├── opencode_page.*      ← OpenCode HTTP+SSE
    └── ...
```

---

## Variabili CMake

| Variabile | Significato |
|-----------|-------------|
| `PRISMALUX_ROOT` | Radice del progetto (iniettata da CMake, usata da `prismalux_paths.h`) |
| `CMAKE_BUILD_TYPE` | `Release` per produzione · `Debug` per sviluppo |
| `BUILD_TESTS` | `ON` per compilare la suite di test |

---

## Note sviluppo

- Leggi `CLAUDE.md` per convenzioni architetturali e pattern di codice.
- Tutti i percorsi usano `PrismaluxPaths` in `prismalux_paths.h` — nessun hardcode.
- Widget in `widgets/*.h` con `Q_OBJECT` devono essere in `CPP_SRCS` per AUTOMOC.

<div align="center">

# 🍺 Prismalux

### *"Costruito per i mortali che aspirano alla saggezza."*

[![C++/Qt6](https://img.shields.io/badge/GUI-C%2B%2B%20%2F%20Qt6-green?style=flat-square&logo=qt)](https://www.qt.io/)
[![Version](https://img.shields.io/badge/versione-3.0-blue?style=flat-square)](CHANGELOG)
[![License](https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square)](https://github.com/wildlux/Prismalux/blob/master/LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20%7C%20Android%20(WIP)-informational?style=flat-square)](https://github.com/wildlux/Prismalux)

**Piattaforma AI locale — zero cloud, zero abbonamenti, tutto sul tuo hardware.**

</div>

---

## Funzionalità principali

| | |
|---|---|
| 🤖 **Multi-Agente** | Pipeline di 6 agenti specializzati + agente autonomo ReAct |
| 🛡️ **Anti-Allucinazione** | 4 agenti logici: genera → critica → verifica → giudica |
| 📂 **RAG ibrido** | Ricerca semantica su file locali + web con fonti citate |
| 💭 **Think Mode** | Ragionamento inline espandibile on-demand |
| 🧠 **Memoria cross-sessione** | Knowledge automatico senza intervento manuale |
| 🎨 **Stable Diffusion** | Generazione immagini integrata (AUTOMATIC1111/Forge/SD.Next) |
| 🔬 **63 Simulazioni algoritmiche** | Grafi, ML, DP, ordinamento — passo-passo |
| 🔗 **17 Plugin MCP** | Blender, Office, GNS3, RDKit, Bioconda, Cytoscape, Arduino… |

---

## Tab GUI

| # | Tab | Shortcut | Contenuto |
|---|-----|----------|-----------|
| 0 | 🤖 Intelligenza Artificiale | `Alt+1` | Pipeline · Byzantino · CHAT RAG · Agente Autonomo |
| 1 | 🛠 Strumenti AI | `Alt+2` | Studio · Scrittura · Ricerca · Cerca Lavoro · Cron |
| 2 | 🎬 Multimedia | — | Audio AI · Stable Diffusion · Mappe Graphviz |
| 3 | 📁 File AI | — | File AI · Wiki & Web · Excel/CSV · PDF · Word |
| 4 | 💻 Programmazione | `Alt+3` | Editor + AI · Agentica · Interpreter Python |
| 5 | π Matematica | `Alt+4` | Formule · Grafici · Calcoli simbolici |
| 6 | 🔬 Ricerca | `Alt+5` | Paper · Brevetti · Cytoscape · RDKit · Bioconda · Avogadro |
| 7 | 🕹 APP Controller | `Alt+6` | Blender · FreeCAD · Office · Anki · KiCAD · OBS · Godot · OpenCode |
| 8 | 🌐 LAN & WAN | — | Android LAN (QR APK) · GNS3 |
| 9 | 📚 Impara | `Alt+7` | Finanza · Tutor AI · Quiz |

---

## Avvio rapido

**Linux / macOS**
```bash
git clone git@github.com:wildlux/Prismalux.git
cd Prismalux/gui
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
./build/Prismalux_GUI
```

**Windows** — `build.bat` (una tantum) poi `Avvia_Prismalux.bat`.  
Richiede MSYS2 UCRT64 o Qt installer.

**Prerequisiti:** Qt6, cmake, gcc/clang, [Ollama](https://ollama.com) + almeno un modello.

```bash
curl -fsSL https://ollama.com/install.sh | sh
ollama pull qwen3:8b    # ~5 GB — ottimo italiano, think nativo
```

---

## Modelli consigliati

```bash
ollama pull qwen3:4b           # 8 GB RAM / 4 GB VRAM
ollama pull qwen3:8b           # 16 GB RAM             ★ consigliato
ollama pull qwen2.5-coder:7b   # coding
ollama pull qwen3:30b          # 64 GB RAM
```

> 💡 **VRAM scarsa?** Usa *Impostazioni → Modalità Misto GPU+CPU*.  
> 💾 **RAM limitata?** Attiva *zRAM lz4/zstd* da *Impostazioni → Hardware*.

---

## Test

```bash
cmake -B build_tests -DBUILD_TESTS=ON && cmake --build build_tests -j$(nproc)
ctest --test-dir build_tests -j4   # 35/37 PASS
```

Le 2 suite non-pass richiedono Ollama attivo + modello specifico (non sono bug del codice).

---

## Struttura

```
Prismalux/
├── gui/              ← GUI C++/Qt6 (pages/, widgets/, themes/, tests/)
├── ANDROID/          ← PrismaluxMobile Qt6/Android (APK precompilato)
├── MCPs/             ← Plugin MCP
├── models/           ← Modelli GGUF locali
├── RAG/              ← Documenti indicizzabili
├── KNOWLEDGE_USER/   ← Memoria utente (locale, non in git)
├── build.bat         ← Windows: compila
├── Avvia_Prismalux.bat ← Windows: avvia
└── aggiorna.sh       ← Linux: build + AppImage + ZIP Windows
```

---

## Contribuisci

PR benvenute. Regole essenziali:

1. Branch tematico (`feature/...`, `fix/...`)
2. Build pulita + `ctest` verde
3. Lambda nelle `connect()`: il context object (4° argomento) deve essere sempre specificato e tutti i puntatori catturati devono essere figli di quel context. Logica non banale → slot nominato. `static QMetaObject::Connection` è vietata.

Vedi `gui/CLAUDE.md` per le convenzioni complete.

---

## Licenza

**MIT** — usa, modifica e distribuisci liberamente, anche in ambito commerciale.

<div align="center">

[![GitHub](https://img.shields.io/badge/GitHub-wildlux%2FPrismalux-181717?style=flat-square&logo=github)](https://github.com/wildlux/Prismalux)

</div>

<div align="center">

# 🍺 Prismalux

### *"Costruito per i mortali che aspirano alla saggezza."*

[![C++/Qt6](https://img.shields.io/badge/GUI-C%2B%2B%20%2F%20Qt6-green?style=flat-square&logo=qt)](https://www.qt.io/)
[![Version](https://img.shields.io/badge/versione-3.0-blue?style=flat-square)](CHANGELOG)
[![License](https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20%7C%20Android%20(WIP)-informational?style=flat-square)](https://github.com/wildlux/Prismalux)
[![Tests](https://img.shields.io/badge/Test%20Suite-33%2B%20suite%20%7C%20100%25%20pass-brightgreen?style=flat-square)](#test)

**Piattaforma AI locale. Potente. Privata. Tua.**

*Zero cloud · Zero abbonamenti · Zero compromessi — tutto gira sul tuo hardware*

</div>

---

## 🧠 Core Intelligente

| | |
|---|---|
| 🖥️ **GUI moderna C++/Qt6** | Interfaccia fluida, reattiva e nativa per Linux / Windows / macOS |
| 🤝 **Architettura Multi-Agente** | Coordinamento intelligente tra agenti AI specializzati |
| 🤖 **Agente Autonomo ReAct** | Pianifica, ragiona e agisce in autonomia con verifica passo-passo |
| 🛡️ **Anti-Allucinazione™** | 4 agenti logici: genera → critica → verifica → giudica |
| 📂 **RAG Ibrido** | Ricerca semantica su file locali + web, con fonti citate e verificabili |
| ➗ **Motore Matematico Locale** | Calcoli simbolici e numerici in microsecondi, zero dipendenze esterne |
| 🔬 **63 Simulazioni Algoritmiche** | Visualizza e sperimenta con algoritmi di grafi, ML, DP, ordinamento e altro |

---

## ⚡ Esperienza Utente Avanzata

- 💭 **Think Mode Inline** — vedi il ragionamento dell'AI in tempo reale, espandibile on-demand con toggle ▶️/🔻
- 🧠 **Memoria Automatica Cross-Sessione** — il contesto impara dalle tue interazioni, senza intervento manuale
- 🔁 **Feedback 👍/👎 integrato** — costruisci un dataset DPO personale in totale privacy
- 📖 **Wiki AI** — Wikipedia multilingua con analisi istantanea, riassunto, verifica fatti e approfondimento
- 🛡️ **Code Interpreter Sandbox** — esegui codice Python generato dall'AI in ambiente isolato (Docker / locale)
- 🗂️ **File AI** — ricerca semantica intelligente tra i tuoi file con analisi automatica del contenuto
- 🔴 **Ollama ottimizzato** — card di errore con suggerimenti GPU/Misto/zRAM quando la RAM non basta

---

## 🎨 Creatività & Personalizzazione

- 🎨 **Stable Diffusion integrato** (AUTOMATIC1111 / Forge / SD.Next) — genera immagini senza uscire dalla piattaforma
- 🎭 **Personalità AI modulabili** — scegli lo stile di risposta del modello:

  | Personalità | Stile |
  |-------------|-------|
  | 🤖 Jarvis (Tony Stark) | Professionale, preciso, ironia britannica |
  | 🚗 KITT (Knight Rider) | Sofisticato, calmo, riferimenti alla guida |
  | 🌿 Yoda (Star Wars) | Sintassi invertita, saggio, sereno |
  | 🎮 Snake (Metal Gear) | Diretto, tattico, frasi brevi e incisive |
  | 💨 Sonic | Rapido, energico, leggermente impaziente |
  | 🍄 Super Mario | Entusiasta, "Wahoo!" e "Mamma mia!" |

- ✏️ **Disegno → 3D** — schizza un'idea e ottieni un modello tridimensionale pronto per l'uso (via MCP Blender)
- 🎤 **"Conversa con [Nome]"** — pulsante vocale contestuale che cambia in base alla personalità attiva

---

## 🔗 MCP — Modular Connectivity Plugins

Connetti Prismalux ai tuoi strumenti preferiti, tutto in locale:

| Categoria | Plugin disponibili |
|-----------|-------------------|
| 🎬 Creatività | Blender, KiCAD, Disegno→3D |
| 📊 Produttività | Office Suite, Anki per lo studio |
| 🔌 Hardware & IoT | Arduino + ESP32, Network Analyzer, STM32 |
| 🔧 Sviluppo | Code Interpreter Sandbox, OpenCode, Debugger integrato |

---

## 🖥️ Tab GUI

| # | Tab | Shortcut | Contenuto |
|---|-----|----------|-----------|
| 0 | 🤖 Intelligenza Artificiale | `Alt+1` | Pipeline 6 agenti · Byzantino · CHAT RAG · **🤖 Agente Autonomo** · TTS/STT |
| 1 | 🛠 Strumenti AI | `Alt+2` | Studio · Scrittura · Ricerca · 💼 Cerca Lavoro · Libri · Cron · **🗂 File AI** · **📖 Wiki** · **🎨 Stable Diffusion** · 📱 LAN Android |
| 2 | 💻 Programmazione | `Alt+3` | Editor + correzione AI + Agentica + **🧪 Interpreter Python** |
| 3 | π Matematica | `Alt+4` | Sequenze→Formula · espressioni locali · costanti alta precisione · 45+ grafici |
| 4 | 🔬 Ricerca | `Alt+5` | Paper · Brevetti · Documenti tecnici |
| 5 | 🕹 APP Controller | `Alt+6` | Blender · Office · Anki · KiCAD · TinyMCP (Arduino/ESP32/STM32) · OpenCode |
| 6 | 📚 Impara | `Alt+7` | Finanza · 730/IVA/Mutuo/PAC/Pensione · Impara con AI · Sfida te stesso |
| ⚙️ | Impostazioni | — | Hardware · AI Locale · Parametri · RAG · Voce · Aspetto · Personalità |

---

## 🚀 Avvio rapido

**Linux / macOS**
```bash
git clone git@github.com:wildlux/Prismalux.git
cd Prismalux/gui
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
./build/Prismalux_GUI
```

**Windows** — nessuna installazione manuale richiesta:
```
1. Doppio clic su  build.bat           ← compila (una tantum, ~3 min)
2. Doppio clic su  Avvia_Prismalux.bat ← avvia il programma ogni volta
```
`build.bat` rileva automaticamente MSYS2 UCRT64/MINGW64, Qt installer o toolchain portatile.

**Prerequisiti:** `Qt6`, `cmake`, `gcc/clang`, `Ollama` + almeno un modello.

```bash
# Installa Ollama
curl -fsSL https://ollama.com/install.sh | sh

# Modello consigliato per iniziare (ottimo italiano + think nativo)
ollama pull qwen3:8b    # ~5 GB
```

---

## 🤖 Modelli consigliati

```bash
# GPU 4 GB VRAM (es. GTX 1050 / RX 580)
ollama pull qwen3:4b              # ~2.5 GB — veloce, think nativo

# 8 GB RAM (CPU)
ollama pull qwen3:4b              # ~2.5 GB — ottimo su hardware limitato

# 16–32 GB RAM
ollama pull qwen3:8b              # ~5 GB — ottimo italiano ★ consigliato
ollama pull deepseek-r1:7b        # ~5 GB — ragionamento matematico
ollama pull qwen2.5-coder:7b      # ~5 GB — top coding 7B

# 64 GB RAM (es. Xeon)
ollama pull qwen3:30b             # ~18 GB — qualità vicina ai modelli cloud
ollama pull deepseek-r1:32b       # ~20 GB — ragionamento avanzato

# Su AMD GPU lenta: forza CPU
OLLAMA_NUM_GPU=0 ollama serve
```

> 💡 **4 GB VRAM scarsa?** Usa **Modalità Misto GPU+CPU** da *Impostazioni → Hardware*: riempie la VRAM al massimo e manda il resto in RAM.  
> 💾 **RAM limitata?** Attiva **zRAM lz4/zstd** da *Impostazioni → Hardware* per guadagnare ~30–40% di memoria effettiva.

---

## 🏗️ Build con test

```bash
cmake -B build_tests -DBUILD_TESTS=ON && cmake --build build_tests -j$(nproc)
ctest --test-dir build_tests -j4          # esegui tutte le suite
ctest --test-dir build_tests -R NomeSuite # esegui una suite specifica
```

<details>
<summary>📋 33+ suite di test disponibili</summary>

| Suite | Test | Cosa verifica |
|-------|------|---------------|
| `test_signal_lifetime` | 36+ | Dangling observer, signal leakage |
| `test_rag_engine` | 15 | Chunking, embedding, search, round-trip |
| `test_rag_engine_avanzato` | 35 | Proprietà JLT math, search edge, save/load 1000 chunk |
| `test_code_utils` | 14+ | Estrazione Python da risposta AI |
| `test_app_controller` | 100+ | extractCode, state machine, Anki JSON, KiCAD, MCU |
| `test_programmazione_page` | 46 | isIntentionalError, parseNumbers, widget+segnali |
| `test_thinking_detect` | 13 | _extractThinkingToken, classifyQuery, think-capable detect |
| `test_ai_integration` | 41 | AiClient REALE ↔ Ollama: classifyQuery, AiChatParams, chat¹ |
| `test_ai_stress` | 80+ | Matrice 24 param, stress 20 req, 14 edge case¹ |
| `test_simulatore_algos` | 54 | BubbleSort/sorting/ricerca/AlgoStep, BigO evalClass |
| `test_rag_engine_avanzato` | 35 | Proprietà JLT math, search edge, 1000 chunk |
| `test_lavoro_data` | 32 | kOfferte integrità dataset, filtri, tipoIcon |
| `test_hardware_monitor` | 11 | hw_detect struttura HWInfo, thread polling |
| `test_knowledge_injection` | 16 | prependKnowledge, cache 30s, toggle QSettings |
| `test_qr_code_widget` | 18 | QR code generazione, rendering Qt, dialog APK |
| ... e altre 18+ suite | | |

> ¹ Richiedono Ollama attivo + modello installato (`PRISMALUX_TEST_MODEL=qwen2.5-coder:7b`). Senza Ollama: **100% delle suite no-network passano**.

</details>

---

## 🔐 Filosofia Zero-Compromise

```
✅ 100% locale — nessun dato lascia il tuo computer
✅ Zero abbonamenti — nessun costo mensile, nessuna API key obbligatoria
✅ Open source MIT — leggi, modifica, distribuisci liberamente
✅ Hardware-first — ottimizzato anche per 8 GB RAM
✅ Privacy by design — feedback, memoria e log restano sul tuo disco
```

---

## 💡 Per chi è Prismalux?

Sviluppatori, maker, ricercatori e appassionati che vogliono **il potere dell'AI senza cedere il controllo**.

Leggero, modulare, trasparente: progettato per chi preferisce la sostanza alle promesse.

> *"La birra è conoscenza divina — ogni sorso un dato in più."* 🍺

---

## 🤝 Contribuisci

Le PR sono benvenute! Linee guida:

1. **Forka** il repo e crea un branch tematico (`feature/...`, `fix/...`, `docs/...`)
2. **Rispetta** lo stile di codice (clang-format, niente commenti ovvi, niente magia hardcoded)
3. **Testa** con `ctest` e verifica che la build sia pulita
4. **Apri** una Pull Request con descrizione chiara e screenshot/GIF se applicabile

---

## 📦 Struttura

```
Prismalux/
├── gui/                    ← GUI C++/Qt6 (pages/, widgets/, themes/, tests/)
├── ANDROID/                ← PrismaluxMobile — client Qt6/Android per Ollama su LAN
│   └── PrismaluxMobile.apk ← APK precompilato (servito via LAN Server)
├── COMPILE_WIN/            ← Toolchain portatile Windows zero-install
├── llama.cpp/              ← Clone llama.cpp
├── models/                 ← Modelli GGUF locali
├── MCPs/                   ← Plugin MCP (blender_addon, office_bridge)
├── RAG/                    ← Documenti indicizzabili
├── KNOWLEDGE_USER/         ← Memoria persistente utente (locale, non in git)
├── build.bat               ← Windows: compila il sorgente
├── Avvia_Prismalux.bat     ← Windows: avvia il programma compilato
└── aggiorna.sh             ← Linux: build + AppImage + ZIP Windows
```

---

## 📱 Android — PrismaluxMobile *(in sviluppo)*

Client Qt6/C++ per Android. Si connette a Ollama sul PC via WiFi LAN.

| Schermata | Funzione |
|-----------|----------|
| 🤖 Chat | Streaming token · RAG keyword · history 6 turni |
| 📷 Camera | Preview live · analisi vision model · OCR → Chat |
| 🔋 Bluetooth | Scan BLE · badge segnale · copia MAC |
| ⚙️ Impostazioni | Host/porta · lista modelli · temperatura · RAG |

**Scarica l'APK:** avvia Prismalux desktop → *Strumenti AI → 📱 LAN Android* → scansiona il QR code.

---

## 📄 Licenza

Distribuito sotto licenza **MIT** — usa, modifica e distribuisci liberamente, anche in ambito commerciale.

Prismalux è un progetto indipendente. Non affiliato con Meta, OpenAI, Stability AI o altre entità commerciali.

---

<div align="center">

**Locale. Potente. Tuo.**

[![GitHub](https://img.shields.io/badge/GitHub-wildlux%2FPrismalux-181717?style=flat-square&logo=github)](https://github.com/wildlux/Prismalux)

</div>

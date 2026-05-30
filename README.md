<div align="center">

<img src="ICONA/prismalux.png" width="96" alt="Prismalux logo"/>

# 🍺 Prismalux

### *"Costruito per i mortali che aspirano alla saggezza."*

[![Version](https://img.shields.io/badge/versione-2.9-blue?style=flat-square)](CHANGELOG)
[![C++/Qt6](https://img.shields.io/badge/GUI-C%2B%2B%20%2F%20Qt6-41CD52?style=flat-square&logo=qt)](https://www.qt.io/)
[![License](https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20%7C%20Android-informational?style=flat-square)](https://github.com/wildlux/Prismalux)
[![AppImage](https://img.shields.io/badge/AppImage-Linux%20x86__64-orange?style=flat-square)](EXPORT/linux/Prismalux-x86_64.AppImage)

**Piattaforma AI locale e distribuita — zero cloud, zero abbonamenti, tutto sul tuo hardware.**

[![Donate PayPal](https://img.shields.io/badge/Dona%20con-PayPal-00457C?style=flat-square&logo=paypal&logoColor=white)](https://www.paypal.com/donate?token=_oGlLIKu1FVK4KdKsH7ft6L90cMRZpN_zCgeFRglYUfvs_HVkyKKY5w5m198MiQ8yAyzdMpUIp_nZd9b)

</div>

---

## Cos'è Prismalux

Prismalux è un'applicazione desktop Qt6 (C++) pensata per chi vuole sfruttare modelli AI locali (Ollama, llama-server) senza dipendere da cloud o abbonamenti. È una piattaforma modulare che integra in un'unica finestra:

- **Pipeline multi-agente** con anti-allucinazione a 4 livelli logici
- **Multi-Agente con GraphMemory** — decomposizione task, sub-agenti, memoria a grafo SQLite
- **RAG ibrido JLT + Grafo Conoscenza** — ricerca semantica + entità/relazioni estratte da LLM
- **105 simulazioni algoritmiche** visualizzate passo per passo
- **18 plugin MCP** per Blender, FreeCAD, GNS3, RDKit, Cytoscape, OBS, Ollama cache...
- **Calcolo distribuito WAN** (BOINC-like) su rete locale con 28 tipi di task
- **Matematica simbolica** con SymPy, grafico interattivo, formule LaTeX (KaTeX)
- **Voce**: TTS (SpeechSynthesis / QTextToSpeech) + STT (Whisper locale/server)
- **Ricerca scientifica**: paper arXiv, brevetti, Bioconda, Grafo RAG, analisi fenomeni
- **App mobile Android** (BLE chat, quiz CCNA, TTS, sincronizzazione LAN)

---

## Novità v2.9

| Feature | Descrizione |
|---------|-------------|
| 🕸️ **Multi-Agente + Memoria a Grafo** | Tab [9]: MasterAgent decompone il compito in sub-task JSON → agenti specializzati in sequenza con `depends_on` → sintesi finale. **GraphMemory** SQLite-backed: nodi+archi, BFS, export DOT/JSON/TXT |
| 🕸️ **Grafo Conoscenza RAG** | Tab 🕸️ Grafo RAG in Ricerca: LLM estrae entità+relazioni dai tuoi documenti → grafo navigabile con Graphviz, click-nodo dettagli, filtro live |
| 🎙️ **TTS + STT ovunque** | Web app: tab "Voce" con SpeechSynthesis + MediaRecorder→Whisper. APK Android: QTextToSpeech box + fix STT upload |
| 🔢 **LaTeX KaTeX** | Pannelli teoria Analisi 1/2 renderizzati con KaTeX (QWebEngineView). Pannello 🔬 output AI. Prompt AI aggiornati. |
| 🔀 **Randomizer formule** | 52 formule categorizzate in Risolvi Passi (equazioni/derivate/integrali/limiti/semplificazioni/disequazioni) |
| 🧪 **Test suite** | 41 suite ctest (38 no-Ollama) — include CAT-E SymPy (15 test reali) + GraphMemory (65 test: nodi/archi/BFS/SQL-injection/`changed()`) |
| ☕ **Donazione PayPal** | Badge README, pulsante Sponsor GitHub (`.github/FUNDING.yml`), pulsante in Impostazioni e APK |
| 💼 **Scheda TFR** | C.F. calcolato automaticamente (D.M. 1976 + ~150 comuni/paesi), calcolo rivalutato, Compila da RAG |
| 🖧 **WAN Calcolo Distribuito** | Server/client TCP:11600, dispatcher 28 task, cron scheduler |
| 🦙 **Ollama MCP** | Cache SQLite modelli, 5 tool (list/info/search/sync/pull), solo stdlib |
| 🎓 **Quiz CCNA 209 domande** | 15 temi CCNA 200-301 completo |
| 📐 **DPI/i18n** | `dpiScale()` su tutte le pages; `QTranslator` + stub `.ts` |

---

## Funzionalità principali

| | |
|---|---|
| 🤖 **Pipeline Multi-Agente** | 6 agenti specializzati in sequenza + agente autonomo ReAct |
| 🕸️ **Multi-Agente + GraphMemory** | MasterAgent decompone → sub-agenti con `depends_on` → memoria a grafo SQLite (nodi, archi, BFS, DOT) |
| 🛡️ **Anti-Allucinazione** | 4 agenti logici: Originale → Avvocato del Diavolo → Gemello → Giudice |
| 📂 **RAG Ibrido JLT** | Ricerca semantica 256-dim su documenti locali; inietta fonti citate nel prompt |
| 🕸️ **Grafo Conoscenza RAG** | LLM estrae entità+relazioni dai documenti → grafo navigabile (Graphviz, click-nodo, filtro) |
| 🎙️ **TTS + STT** | Web: SpeechSynthesis + MediaRecorder→Whisper. Android: QTextToSpeech + upload Whisper |
| 💭 **Think Mode** | Ragionamento `<think>` espandibile inline, budget 1-4, auto-classificatore query |
| 🧠 **Memoria cross-sessione** | Knowledge Base automatica in `user_knowledge.md`; MCP Knowledge Updater |
| 🔢 **LaTeX KaTeX** | Formule matematiche renderizzate con KaTeX (QWebEngineView) in Analisi 1/2 + output AI |
| 🎨 **Stable Diffusion** | Generazione immagini via AUTOMATIC1111/Forge/SD.Next (API locale) |
| 🔬 **105 Simulazioni** | Algoritmi visualizzati barra per barra con spiegazione e complessità O-grande |
| 🔗 **18 Plugin MCP** | JSON-RPC 2.0 stdio — Blender, Office, GNS3, RDKit, Cytoscape, OBS, Godot, **Ollama cache**... |
| 🖧 **WAN Compute** | Calcolo distribuito LAN/WAN: server TCP + dispatcher 28 task + cron |
| 📱 **App Android** | Qt6 native: BLE chat AES-256-GCM, Quiz CCNA **209 domande**, TTS/STT, sincronizzazione LAN |
| 🌐 **LAN Server** | Web app embedded su porta 11500: chat, matematica, Voce (TTS+STT), Whisper, Graphviz |
| 🃏 **Byzantino** | Gioco di maggioranza a tolleranza di guasto: m agenti, n disonesti configurabili |

---

## Interfaccia — Tab completi

| # | Tab | Shortcut | Contenuto |
|---|-----|----------|-----------|
| 0 | 🤖 **Intelligenza Artificiale** | `Alt+1` | Pipeline 6 agenti · Byzantino · CHAT RAG · Agente Autonomo ReAct |
| 1 | 🛠 **Strumenti** | `Alt+2` | Assistente AI · 💰 Finanza (730, P.IVA, Calcolatori, TFR) · ⏱ Cron · Impara · Sfida! |
| 2 | 🎬 **Multimedia** | — | Audio AI (Whisper STT + TTS) · Stable Diffusion · Mappe Graphviz |
| 3 | 📁 **File AI** | — | Analisi file · Wiki & Web · Excel/CSV · PDF · Word/Testo |
| 4 | 💻 **Programmazione** | `Alt+3` | Editor+AI · Agentica · Translitter · Reverse Eng. · Git MCP · Python REPL · Interpreter · Rete & Network · Driver & Kernel |
| 5 | π **Matematica** | `Alt+4` | Sequenza→Formula · Costanti · N-esimo · Espressione · **Risolvi Passi** (🔀 52 formule) · Analisi 1&2 (**LaTeX KaTeX**) |
| 6 | 🔬 **Ricerca** | `Alt+5` | Paper · Brevetto · Cerca arXiv/Brevetti · Lavoro · Cytoscape—Bio · RDKit · Bioconda · RAB₀-L · BLHM · Analisi Fenomeni · **🕸️ Grafo RAG** · Astrale |
| 7 | 🕹 **APP Controller** | `Alt+6` | Blender · FreeCAD · Office · CloudCompare · Anki · KiCAD · TinyMCP · OBS · OpenCode · Godot |
| 8 | 🌐 **LAN & WAN** | — | LAN Android (QR APK · ADB USB) · GNS3 MCP · **WAN Compute** (🧠 Solo questo PC \| 🌐 Rete LAN) → MasterAgent → Sub-agenti → **GraphMemory** |
| ⚙️ | **Impostazioni** | header | Backend AI · Modelli · Think Mode · Voce · Visual · Hardware · Memoria · Test |

### Dettaglio tab Strumenti — Finanza

| Voce | Funzione |
|------|---------|
| 📄 Assistente 730 | Chat AI specializzata: detrazioni, rimborsi, modello precompilato |
| 💼 Partita IVA | Regime forfettario, calcolo imposte, contributi INPS |
| 💰 Calcolatori Finanza | Mutuo (piano ammortamento), PAC, Stima Pensione INPS con grafico |
| 💼 Scheda TFR | Form personale + calcolo art. 2120 c.c. + rivalutazione + fiscalità separata + **Compila da RAG** |

### Dettaglio tab Programmazione

| Sub-tab | Funzione |
|---------|---------|
| 💻 Programmazione | Editor codice con AI (Python, C, C++, JS, Bash) + esecuzione |
| 🤖 Agentica | Agente AI per generazione codice multi-step |
| 🔀 Translitter | Traduzione automatica tra linguaggi di programmazione |
| 🔍 Reverse Eng. | Analisi e decompilazione codice sorgente/binario |
| 🔧 Git MCP | Operazioni Git assistite da AI (clone, commit, diff, log) |
| 🐍 REPL | Python REPL interattivo |
| 🧪 Interpreter | Code Interpreter con sandbox Python + matplotlib PNG |
| 🌐 Rete & Network | Cattura pacchetti · Scan LAN |
| 🔧 Driver & Kernel | Driver NVIDIA · AMD · Kernel Linux |

---

## Simulazioni Algoritmiche (105 algoritmi)

### 📊 Ordinamento (19)
Bubble, Selection, Insertion, Shell, Cocktail, Comb, Gnome, Odd-Even, Cycle, Pancake, **Quick**, **Merge**, **Heap**, Bitonic, Counting, Radix, Bucket, **Tim**, Stooge

### 🔍 Ricerca (10)
Linear, Binary, Jump, Ternary, Interpolation, Exponential, Fibonacci, Two Pointers, Boyer-Moore Voting, Quickselect

### 🗂 Strutture Dati (8)
Stack, Queue, Deque, Min-Heap Build, Hash Table, Segment Tree, Fenwick Tree (BIT), LRU Cache

### 🕸 Grafi (11)
BFS, DFS, **Dijkstra**, Bellman-Ford, Floyd-Warshall, Topological Sort, **Kruskal MST**, **Prim MST**, Union-Find, Tarjan SCC, **A\***

### 🧩 Dynamic Programming (10)
Coin Change, LIS, 0/1 Knapsack, LCS, Edit Distance, Matrix Chain, Egg Drop, Rod Cutting, Subset Sum DP, Max Product Subarray

### 🏆 Greedy (6)
Activity Selection, Fractional Knapsack, Huffman Coding, Job Scheduling, Coin Change Greedy, Min Platforms

### 🌳 Backtracking (5)
N-Queens, Subset Sum, Permutazioni, Flood Fill, Rat in a Maze

### 🔤 Stringhe (5)
KMP Search, Rabin-Karp, Z-Algorithm, Longest Palindrome, Longest Common Prefix

### ➗ Matematica (16)
Crivello di Eratostene, Crivello di Sundaram, GCD Euclideo, GCD Esteso, Fast Power, Fattorizzazione Prima, Miller-Rabin, Pascal Triangle, Fibonacci DP, Numeri di Catalan, Monte Carlo Pi, Congettura di Collatz, Karatsuba, Prefix Sum, Kadane (Max Subarray), Metodo di Horner

### 🪟 Pattern (8)
Sliding Window, Dutch National Flag, Trapping Rain Water, Next Greater Element, Fisher-Yates Shuffle, Stock Max Profit, Max Circular Subarray, Count Inversions

### 🎲 Classici (7)
Reservoir Sampling, Floyd Cycle Detection, Torri di Hanoi, Game of Life (1D), Rule 30 (Wolfram), Spiral Matrix, Sierpinski Triangle

---

## WAN Calcolo Distribuito

Il tab **LAN & WAN → WAN Compute** trasforma ogni PC che esegue Prismalux in un nodo di calcolo. Non richiede software aggiuntivo — solo TCP porta 11600.

```
Server (Prismalux A)           Rete LAN/WAN            Client (Prismalux B, C, D)
┌────────────────────────┐                        ┌─────────────────────────┐
│  • Avvia server :11600 │  ←── TCP JSON ──►     │  • Connetti a Server    │
│  • Coda task           │                        │  • Esegui task locali   │
│  • Cron scheduler      │                        │  • Invia risultati      │
│  • Monitoraggio nodi   │                        │  • AI locale Ollama     │
└────────────────────────┘                        └─────────────────────────┘
```

### 28 tipi di task disponibili

| Categoria | Tipi |
|-----------|------|
| 🤖 **AI & LLM** | `ai_query`, `code_assist`, `code_review`, `code_translate`, `code_reverse`, `math_solve`, `math_seq`, `paper_gen`, `web_search`, `ai_tutor`, `ai_data_analysis`, `ai_fenomeno`, `ai_730`, `ai_tfr` |
| 💻 **Codice & Shell** | `python_repl`, `eval_script`, `shell_cmd`, `git_cmd` |
| 📐 **Matematica** | `math_expr`, `math_nth` |
| 📁 **File & Sistema** | `file_read`, `file_write`, `system_info`, `net_info` |
| 🎨 **Grafica** | `graphviz_render`, `matplotlib_plot` |

Ogni tipo ha un **template payload** pre-compilato automaticamente alla selezione. Il **cron scheduler** integrato dispatcha task a intervalli configurabili (1-1440 min).

---

## Plugin MCP (18)

| Plugin | Funzione |
|--------|---------|
| `blender_addon` | Genera/modifica scene 3D Blender da prompt o PDF |
| `freecad_mcp` | Parametrica FreeCAD: solidi, assiemi, disegni tecnici |
| `office_bridge` | Crea/modifica documenti Word, Excel, PowerPoint |
| `gns3_mcp` | Topologie di rete GNS3: router, switch, link, simulazione |
| `cytoscape_mcp` | Analisi reti biologiche (PPI, pathway, genomica) |
| `rdkit_mcp` | Chemioinformatica: SMILES, fingerprint, similarità molecolare |
| `bioconda_mcp` | Pipeline bioinformatica Python (Biopython, SeqIO, BLAST) |
| `avogadro_mcp` | Modellazione molecolare 3D e ottimizzazione geometrica |
| `graphviz_mcp` | Mappe concettuali e grafi da testo DOT |
| `obs_mcp` | Controllo OBS Studio: scene, sorgenti, registrazione |
| `opencode_mcp` | Agente agentica OpenCode (SSE streaming) |
| `godot_mcp` | Script GDScript per Godot Engine |
| `kicad_mcp` | Schemi elettronici e PCB KiCad |
| `anki_mcp` | Generazione deck Anki da argomento o testo |
| `stable_diffusion_local` | API Stable Diffusion (AUTOMATIC1111/Forge/SD.Next) |
| `tinymcp` | Bridge MCP generico per tool personalizzati |
| `knowledge_mcp` | Aggiornamento automatico della Knowledge Base utente |
| `ollama_mcp` | **Nuovo** — Cache SQLite modelli Ollama; tool: list, info, search, sync, pull |

---

## Avvio rapido

### Linux — AppImage (consigliato, no installazione)

```bash
chmod +x EXPORT/linux/Prismalux-x86_64.AppImage
./EXPORT/linux/Prismalux-x86_64.AppImage
```

### Linux — Compila da sorgente

```bash
git clone https://github.com/wildlux/Prismalux.git
cd Prismalux
bash aggiorna.sh --gui          # compila GUI + crea AppImage
# oppure manualmente:
cmake -B build_gui gui/ -DCMAKE_BUILD_TYPE=Release
cmake --build build_gui -j$(nproc)
./build_gui/Prismalux_GUI
```

### Windows

```bat
REM Una tantum — richiede MSYS2 UCRT64 o Qt Installer
aggiorna.bat           :: compila + crea ZIP distribuibile
REM poi:
Avvia_Prismalux.bat    :: avvia l'app
```

### macOS

```bash
brew install qt cmake
cmake -B build_gui gui/ -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
cmake --build build_gui -j$(sysctl -n hw.logicalcpu)
./build_gui/Prismalux_GUI
```

### Prerequisiti

| Componente | Versione minima | Note |
|-----------|----------------|------|
| Qt6 | 6.4+ | Base, Network, Multimedia, PrintSupport, Svg |
| CMake | 3.20+ | |
| GCC / Clang | 11+ / 13+ | C++17 |
| [Ollama](https://ollama.com) | qualsiasi | Avvia con `ollama serve` |
| Python 3 | 3.10+ | Per Code Interpreter e ZIP Windows |
| pdftotext | — | `apt install poppler-utils` (opzionale, per PDF in Analisi Fenomeni) |

```bash
# Installa Ollama + modello consigliato
curl -fsSL https://ollama.com/install.sh | sh
ollama pull qwen3:8b     # ~5 GB — ottimo italiano, think nativo
```

---

## Modelli consigliati

```bash
# Leggeri (8 GB RAM / 4 GB VRAM)
ollama pull qwen3:4b               # ★ ottimo rapporto qualità/peso
ollama pull llama3.2:3b            # veloce, buon inglese

# Bilanciati (16 GB RAM) — consigliati
ollama pull qwen3:8b               # ★★ italiano eccellente + think mode
ollama pull mistral:7b-instruct

# Coding (qualsiasi RAM)
ollama pull qwen2.5-coder:7b       # ★ code review, generazione, debug
ollama pull deepseek-coder:6.7b

# Potenti (32-64 GB RAM)
ollama pull qwen3:30b
ollama pull deepseek-r1:32b        # ragionamento step-by-step
```

> 💡 **VRAM insufficiente?** Impostazioni → Modalità Misto GPU+CPU (overflow automatico in RAM)  
> 💾 **RAM ≤ 8 GB?** Attiva zRAM lz4/zstd in Impostazioni → Hardware

---

## Backend AI supportati

| Backend | Porta | Quando usarlo |
|---------|-------|---------------|
| **Ollama** (default) | 11434 | Installazione semplice, auto-gestione modelli |
| **llama-server** | 8081 | Controllo fine su `n_gpu_layers`, `ctx`, batch |
| **OpenCode** | 8092 | Agente agentico con accesso filesystem |

Cambio backend da UI: intestazione → selettore backend → porta → conferma.

---

## Build e Test

```bash
# Test headless (no Ollama richiesto per 33/36 suite)
cmake -B build_tests gui/ -DBUILD_TESTS=ON
cmake --build build_tests -j$(nproc)
ctest --test-dir build_tests -j4

# Test con Ollama attivo (suite AI stress)
ollama serve &
ctest --test-dir build_tests -j1 -R AiStress
```

**Stato test:**
- ✅ 33/36 suite passano senza Ollama
- ⚠️ `SimulatoreAlgos` — FLAKY in `-j4`, PASS standalone (`RESOURCE_LOCK cpu_heavy`)
- ⚠️ `AiStress` — richiede Ollama + modello + ≥2 GB RAM libera
- ⚠️ `SttWhisper` — richiede microfono attivo

---

## Struttura del progetto

```
Prismalux/
├── gui/                          ← GUI C++/Qt6 (sorgente principale)
│   ├── pages/                    ← Una pagina = un file .cpp/.h
│   │   ├── agenti_page.*         ← Pipeline multi-agente (15 moduli)
│   │   ├── agenti_multi_page.*   ← 🕸️ Multi-Agente + GraphMemory (dentro WAN Compute)
│   │   ├── lan_wan_page.*        ← LAN Android + GNS3 + WAN Compute (Solo PC | Rete LAN)
│   │   ├── pratico_page.*        ← 730, P.IVA, Finanza, Scheda TFR
│   │   ├── ricerca_page.*        ← Paper, brevetti, bio, Grafo RAG, analisi fenomeni
│   │   ├── programmazione_page.* ← Editor, REPL, Rete, Driver
│   │   ├── matematica_page.*     ← SymPy, formule, grafici, LaTeX KaTeX
│   │   ├── simulatore_page.*     ← 105 algoritmi visualizzati
│   │   └── ...                   ← altri 20+ moduli
│   ├── widgets/                  ← Componenti riutilizzabili (incl. LatexView)
│   ├── graph_memory.h/cpp        ← GraphMemory SQLite (nodi, archi, BFS, DOT)
│   ├── rag_graph.h/cpp           ← RagGraph: estrazione LLM → GraphMemory
│   ├── themes/                   ← Temi QSS (dark, light, solarized…)
│   ├── tests/                    ← Suite ctest (41 suite, CAT-E SymPy, GraphMemory)
│   ├── CMakeLists.txt            ← Build (versione unica di verità)
│   └── CLAUDE.md                 ← Convenzioni di sviluppo
│
├── EXPORT/                       ← Artefatti e script di distribuzione per piattaforma
│   ├── linux/
│   │   ├── crea_appimage.sh      ← Genera AppImage Linux
│   │   ├── install_launcher.sh   ← Installa shortcut KDE/GNOME
│   │   └── Prismalux-x86_64.AppImage
│   ├── windows/
│   │   ├── crea_zip_windows.py   ← Genera ZIP distribuibile Windows
│   │   ├── build_installer_windows.bat ← Crea Prismalux_Deploy/ con DLL Qt
│   │   └── Prismalux_v2.9_Windows.zip
│   ├── android/
│   │   ├── build_apk.sh          ← Compila PrismaluxMobile.apk
│   │   ├── installa_xiaomi.sh    ← Installa via ADB su Xiaomi
│   │   └── test_apk.sh / test_utente.sh
│   └── macos/                    ← (futuro: crea_dmg.sh)
│
├── ANDROID/                      ← App Android Qt6
│   ├── android_app/              ← BLE chat, Quiz CCNA, TTS, STT, LAN sync
│   └── PrismaluxMobile.apk       ← APK precompilato (scaricabile via QR in-app)
│
├── MCPs/                         ← 18 plugin MCP (Python, JSON-RPC 2.0 stdio)
├── RAG/                          ← Documenti per RAG (locale, non in git)
├── KNOWLEDGE_USER/               ← Memoria utente (locale, non in git)
├── BEST_PRACTICE_&_GOAL/         ← Regole, obiettivi, TODO, operazioni
│   ├── REGOLE_IRREMOVIBILI.md    ← 15 convenzioni fisse del software
│   ├── TODO.md                   ← Funzionalità pendenti
│   ├── OPERAZIONI.txt            ← Checklist operazioni completate
│   ├── BEST_PRACTICE_SECURITY.md
│   ├── BEST_PRACTICE_USER_EXPERIENCE.md
│   ├── BEST_PRACTICE_MANAGE_LLM.md
│   └── BEST_PRACTICE_BONUS.md
│
├── COMPILE_WIN/                  ← Toolchain portatile Windows (setup.bat scarica ~600 MB)
│   ├── setup.bat                 ← Scarica Qt6 + GCC + CMake + Ninja
│   └── build.bat                 ← Stub → chiama build.bat radice
│
├── scripts/                      ← Utility interne
│   ├── download_model            ← Download modelli GGUF
│   └── genera_quiz_ccna.py       ← Genera domande quiz CCNA
│
├── Test/                         ← Test Python (AI integration, WAN, RAG)
│   ├── run_all_tests.py
│   ├── test_wan_compute.py
│   └── test_rag_paper.py / test_coding_mistral.py / …
│
├── build.bat                     ← Windows: build GUI (entry point, rileva MSYS2/Qt)
├── aggiorna.bat                  ← Windows: build + ZIP distribuibile
├── aggiorna.sh                   ← Linux/macOS: build + AppImage + ZIP
├── Avvia_Prismalux.bat           ← Windows: lancia l'app compilata
├── avvia.sh                      ← Linux: lancia l'app compilata
└── .github/FUNDING.yml           ← Sponsor GitHub (PayPal)
```

---

## App Mobile Android

**PrismaluxMobile** (cartella `ANDROID/`) è un'app Qt6 nativa per Android:

| Feature | Dettaglio |
|---------|-----------|
| 📱 LAN Chat | Connessione al server Prismalux desktop via QR code o IP |
| 🔵 BLE Chat | Comunicazione Bluetooth LE cifrata AES-256-GCM |
| 🎓 Quiz CCNA | **209 domande** in 15 temi (CCNA 200-301 completo) con feedback interattivo |
| 🎙️ Audio AI | Registrazione + trascrizione Whisper + **TTS** (QTextToSpeech, voce italiana) |
| 🔊 Sintesi Vocale | Box TTS dedicato: scrivi testo → ascoltalo in italiano |
| ☕ Donazione | Pulsante PayPal nella pagina Info |
| ⚙️ Impostazioni | Configurazione backend, porta, token |

**Installazione:**  
`LAN & WAN → LAN Android → scansiona QR` → installa l'APK sul telefono.

---

## Contribuire

PR benvenute. Regole essenziali:

1. **Branch tematico**: `feature/...`, `fix/...`, `refactor/...`
2. **Build pulita**: nessun errore di compilazione
3. **Test**: `ctest --test-dir build_tests -j4` verde sulle suite no-Ollama
4. **Lambda nelle `connect()`**: il context object (4° argomento) deve essere sempre specificato; tutti i puntatori catturati devono essere figli di quel context. Logica > 2 righe → slot nominato. `static QMetaObject::Connection` vietata.
5. **Path**: usa sempre `P::root()`, `P::kOllamaPort` ecc. da `prismalux_paths.h` — mai hardcode.
6. **Emoji in stringhe C++**: concatena quando il carattere successivo è cifra hex (`"\xe2\x80\x9c" "Testo"` non `"\xe2\x80\x9cTesto"`).

Vedi `gui/CLAUDE.md` per le convenzioni complete (ThemeManager, LanServer shutdown, AiClient API, Think Mode, RAG, MCP).

---

## Supporta il progetto ☕

Prismalux è software libero, sviluppato nel tempo libero. Se ti è utile e vuoi contribuire alla sua crescita, puoi offrire un caffè:

<div align="center">

[![Dona con PayPal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/donate?token=_oGlLIKu1FVK4KdKsH7ft6L90cMRZpN_zCgeFRglYUfvs_HVkyKKY5w5m198MiQ8yAyzdMpUIp_nZd9b)

</div>

Ogni contributo aiuta a finanziare lo sviluppo di nuove funzionalità, il supporto multipiattaforma e il tempo dedicato al progetto. Grazie! 🙏

---

## Licenza

**MIT** — usa, modifica e distribuisci liberamente, anche in ambito commerciale.  
Vedi [LICENSE](LICENSE).

---

<div align="center">

[![GitHub](https://img.shields.io/badge/GitHub-wildlux%2FPrismalux-181717?style=flat-square&logo=github)](https://github.com/wildlux/Prismalux)
[![Issues](https://img.shields.io/github/issues/wildlux/Prismalux?style=flat-square)](https://github.com/wildlux/Prismalux/issues)
[![Stars](https://img.shields.io/github/stars/wildlux/Prismalux?style=flat-square)](https://github.com/wildlux/Prismalux/stargazers)

*Prismalux — AI locale per chi non vuole dipendere da nessuno.*

</div>

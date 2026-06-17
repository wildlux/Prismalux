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

[![Donate PayPal](https://img.shields.io/badge/Dona%20con-PayPal-00457C?style=flat-square&logo=paypal&logoColor=white)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=wildlux%40gmail.com&item_name=Prismalux&currency_code=EUR&source=url)

</div>

---

## Cos'è Prismalux

Prismalux è un'applicazione desktop Qt6 (C++) pensata per chi vuole sfruttare modelli AI locali (Ollama, llama-server) senza dipendere da cloud o abbonamenti. È una piattaforma modulare che integra in un'unica finestra:

- **Pipeline multi-agente** con anti-allucinazione a 4 livelli logici
- **Multi-Agente con GraphMemory** — decomposizione task, sub-agenti, memoria a grafo SQLite
- **RAG ibrido JLT + Grafo Conoscenza** — ricerca semantica + entità/relazioni estratte da LLM
- **105 simulazioni algoritmiche** visualizzate passo per passo
- **50 plugin MCP** per Blender, FreeCAD, GNS3, RDKit, Cytoscape, OBS, Ollama cache, sicurezza, analisi architetturale, best practice...
- **Sicurezza informatica** — 5 MCP dedicati: secrets scanner, audit CVE, CVE lookup NVD, network recon, SAST
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
| 🗺️ **Mappa OSM + routing** | Tab "🗺 Mappa OSM" in Multimedia: WorldMapWidget con itinerari multi-tappa (A→B→C), routing OSRM (Auto/Piedi/Bici), polyline colorata, distanza km + tempo stimato |
| 🤖 **Dev Agent 2 colonne** | AppController [10]: layout QSplitter H — Log+Diff a sinistra, Cronologia+Git a destra; 6 slot git: log, restore, fetch+reset GitHub, stash push/list/pop |
| 🧪 **Test suite** | 41 suite ctest (38 no-Ollama) — include CAT-E SymPy (15 test reali) + GraphMemory (65 test: nodi/archi/BFS/SQL-injection/`changed()`) |
| 🎛️ **TriModeButton** | Pulsante ovale a 3 settori (Chat / Agentico / Conversa) nel tab AI: cicla con Shift+Tab, colori dal tema. Supporto emoji SVG OpenMoji (impostabile in Visuale → Aspetto) |
| ⚡ **Tool Veloci / 🔌 Tool Lenti** | Tool AI separati in due pannelli distinti: Function Tools in-process (⚡, griglia 2 col) e Plugin MCP subprocess (🔌, griglia 4 col). Comportamento radio: un solo pannello aperto alla volta |
| 🫧 **Arrotondamento bolle live** | Cambio raggio bolle in Visuale → Aspetto si applica immediatamente alle bolle già visibili (regex replace sull'HTML del log) |
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
| 🗺️ **Mappa OSM** | Mappa OpenStreetMap interattiva con itinerari multi-tappa e routing OSRM (Auto/Piedi/Bici) — offline tile cache |
| 🎨 **Stable Diffusion** | Generazione immagini via AUTOMATIC1111/Forge/SD.Next (API locale) |
| 🔬 **105 Simulazioni** | Algoritmi visualizzati barra per barra con spiegazione e complessità O-grande |
| 🔗 **50 Plugin MCP** | JSON-RPC 2.0 stdio — Blender, Office, GNS3, RDKit, Cytoscape, OBS, Godot, **Ollama cache**, 5 sicurezza, 7 best practice, 12 produttività, chat memory, web scraper, note... |
| 🖧 **WAN Compute** | Calcolo distribuito LAN/WAN: server TCP + dispatcher 28 task + cron |
| 📱 **App Android** | Qt6 native: BLE chat AES-256-GCM, Quiz CCNA **209 domande**, TTS/STT, sincronizzazione LAN |
| 🌐 **LAN Server** | Web app embedded su porta 11500: chat, matematica, Voce (TTS+STT), Whisper, Graphviz · **tab bar a 2 righe per sezione** · controllo font A-/A+ persistente · QR con token incluso |
| 🃏 **Byzantino** | Gioco di maggioranza a tolleranza di guasto: m agenti, n disonesti configurabili |

---

## Interfaccia — Tab completi

| # | Tab | Shortcut | Contenuto |
|---|-----|----------|-----------|
| 0 | 🤖 **Intelligenza Artificiale** | `Alt+1` | Pipeline 6 agenti · Byzantino · CHAT RAG · Agente Autonomo ReAct |
| 1 | 🛠 **Strumenti** | `Alt+2` | Assistente AI · 💰 Finanza (730, P.IVA, Calcolatori, TFR) · ⏱ Cron · Impara · Sfida! |
| 2 | 🎬 **Multimedia** | — | Audio AI (Whisper STT + TTS) · Stable Diffusion · 🕸 Mappe concettuali · **🗺 Mappa OSM** (itinerari OSRM) · Sintetizzatore · OCR webcam |
| 3 | 📁 **File AI** | — | Analisi file · Wiki & Web · Excel/CSV · PDF · Word/Testo |
| 4 | 💻 **Programmazione** | `Alt+3` | Editor+AI · Agentica · Translitter · Reverse Eng. · Git MCP · Python REPL · Interpreter · Rete & Network · Driver & Kernel |
| 5 | π **Matematica** | `Alt+4` | Sequenza→Formula · Costanti · N-esimo · Espressione · **Risolvi Passi** (🔀 52 formule) · Analisi 1&2 (**LaTeX KaTeX**) |
| 6 | 🔬 **Ricerca** | `Alt+5` | Paper · Brevetto · Cerca arXiv/Brevetti · Lavoro · Cytoscape—Bio · RDKit · Bioconda · RAB₀-L · BLHM · Analisi Fenomeni · **🕸️ Grafo RAG** · Astrale |
| 7 | 🕹 **APP Controller** | `Alt+6` | Blender · FreeCAD · Office · CloudCompare · Anki · KiCAD · TinyMCP · OBS · OpenCode · Godot · **🤖 Dev Agent** (git restore + LangGraph) |
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

## Plugin MCP (50)

Ogni plugin ha `requirements.txt`. Tutti usano il protocollo **JSON-RPC 2.0 stdio** e sono compatibili sia con Claude Code (`~/.claude/settings.json`) sia con l'interfaccia **McpAddonsPage** interna all'app.

### Applicazioni e creatività (18)

| Plugin | Funzione | Dipendenze pip |
|--------|---------|---------------|
| `blender_addon` | Scene 3D Blender da prompt o PDF | nessuna (API Blender) |
| `freecad_mcp` | Modellazione FreeCAD: solidi, disegni tecnici | nessuna (bridge HTTP) |
| `office_bridge` | Crea/modifica Word, Excel, PowerPoint | `python-docx openpyxl python-pptx` |
| `gns3_mcp` | Topologie GNS3: router, switch, link | `gns3fy requests` |
| `cytoscape_mcp` | Reti biologiche PPI/pathway/genomica | nessuna (CyREST) |
| `rdkit_mcp` | Chemioinformatica: SMILES, fingerprint | `rdkit Pillow` |
| `bioconda_mcp` | Pipeline bioinformatica BLAST/BWA/STAR | nessuna (subprocess conda) |
| `avogadro_mcp` | Modellazione molecolare 3D | `avogadro` |
| `graphviz_mcp` | Mappe concettuali e grafi DOT | `graphviz` + binario `dot` |
| `obs_mcp` | OBS Studio: scene, sorgenti, registrazione | `obsws-python` |
| `opencode_mcp` | Agente agentica OpenCode (SSE streaming) | nessuna |
| `godot_mcp` | GDScript per Godot Engine | nessuna (bridge HTTP) |
| `kicad_mcp` | Schemi elettronici e PCB | nessuna (Scripting Console) |
| `anki_mcp` | Generazione deck Anki da testo | nessuna (AnkiConnect) |
| `stable_diffusion_local` | Text-to-image Stable Diffusion locale | `torch diffusers transformers accelerate Pillow` |
| `tinymcp` | Compila/carica sketch Arduino/ESP32/Pico | `pyserial` |
| `knowledge_mcp` | Aggiornamento automatico Knowledge Base | nessuna |
| `ollama_mcp` | Cache SQLite modelli Ollama (list/info/search/sync/pull) | nessuna |

### Sviluppo Prismalux — MCP dedicati (5)

Ottimizzati per il workflow di sviluppo interno: navigazione codebase, build, deploy Android, git, RAG.

| Plugin | Funzione | Tool chiave |
|--------|---------|------------|
| `prismalux_search_mcp` | Ricerca simboli, file, TODO nel codebase | `grep_symbol`, `find_file`, `read_file`, `list_pages`, `get_context`, `search_todo` |
| `prismalux_build_mcp` | Build e test senza uscire dalla sessione | `build`, `run_tests`, `cmake_config`, `get_build_log`, `check_compile` |
| `android_adb_mcp` | Gestione dispositivo Android via ADB | `list_devices`, `install_apk`, `read_logcat`, `screenshot`, `push_file`, `shell_safe` |
| `git_prismalux_mcp` | Git con output formattato/troncato | `log`, `diff`, `blame`, `search_commits`, `recent_files`, `branch_info` |
| `rag_manager_mcp` | Documenti RAG e KNOWLEDGE\_USER/ | `list_docs`, `add_doc`, `search_content`, `save_knowledge`, `stats` |

### Sicurezza informatica (5)

Strumenti di audit e ricognizione — tutti con fallback graceful se il tool di sistema non è installato. Nessuna dipendenza pip obbligatoria: usano stdlib Python + API HTTP gratuite.

| Plugin | Funzione | Tool chiave | Dipendenze sistema |
|--------|---------|------------|-------------------|
| `secrets_scanner_mcp` | Rileva segreti hardcodati nel codebase (22 pattern: API key, JWT, chiavi private, token, URL con credenziali) | `scan_project`, `scan_staged`, `scan_diff`, `add_whitelist` | nessuna (stdlib) |
| `dep_audit_mcp` | Audit CVE sui requirements Python via OSV.dev API + pip-audit | `audit_requirements`, `audit_project`, `check_package` | `pip-audit` opzionale (`pipx install pip-audit`) |
| `cve_lookup_mcp` | Ricerca CVE in NVD/NIST e OSV.dev, spiegazione CVSS | `lookup_cve`, `search_cve`, `recent_critical`, `cvss_explain` | nessuna (HTTP stdlib) |
| `network_recon_mcp` | Ricognizione rete: port scan, DNS, WHOIS, verifica porte Prismalux | `scan_ports`, `check_prismalux`, `dns_lookup`, `scan_local_net` | `nmap whois dnsutils traceroute` (con fallback socket Python) |
| `sast_mcp` | Analisi statica sicurezza codice Python (bandit) e C++ (cppcheck + 13 pattern custom) | `bandit_scan`, `cppcheck_scan`, `regex_audit`, `full_audit` | `cppcheck` (apt) + `bandit` (pipx) opzionali |

**Installazione tool sicurezza (consigliata):**

```bash
# Strumenti di sistema
sudo apt install nmap whois dnsutils traceroute cppcheck

# Tool Python — FUORI dal venv principale (evita conflitti)
pipx install bandit
pipx install pip-audit
pipx install safety
```

> `scan_staged` è il tool più utile nel quotidiano: va eseguito prima di ogni `git commit` per intercettare segreti prima che finiscano nel repository.

### Sviluppo software — Best Practice (7)

MCP orientati alla qualità del codice: architettura, UX, sicurezza OWASP, test, performance, changelog e licenze. Tutti in stdlib Python, nessuna dipendenza pip obbligatoria.

| Plugin | Funzione | Tool chiave |
|--------|---------|------------|
| `arch_analyzer_mcp` | Analisi architetturale C++: dipendenze circolari, God Class, coupling afferente/efferente, complessità ciclomatica | `analyze_dependencies`, `find_god_classes`, `coupling_report`, `check_conventions`, `complexity_estimate` |
| `ui_ux_checker_mcp` | Verifica UI/UX: dpiScale(), touch target ≥44dp (Android), tooltip mancanti, contrasto WCAG AA, accessibilità | `check_dpi`, `check_touch_targets`, `check_tooltips`, `check_qss_contrast`, `full_ux_audit` |
| `owasp_mcp` | OWASP Top 10 adattato a Qt6 C++ e Python: injection, broken auth, crypto, config, logging insicuro | `check_a01`…`check_a09`, `full_scan`, `explain` |
| `test_generator_mcp` | Generazione scheletri test QTest (C++) e pytest (Python) da header/file sorgente | `generate_qt_tests`, `generate_pytest`, `list_untested`, `analyze_test_gaps`, `suggest_edge_cases` |
| `perf_analyzer_mcp` | Performance: valgrind memcheck/callgrind, perf stat hardware counters, cProfile Python, dimensione binario ELF | `valgrind_memcheck`, `callgrind_profile`, `perf_stat`, `python_profile`, `check_binary_size` |
| `changelog_mcp` | Conventional Commits: genera CHANGELOG.md, valida messaggi, suggerisce bump semver | `list_unreleased`, `validate_commits`, `suggest_version`, `generate_entry`, `format_release` |
| `license_checker_mcp` | Licenze PyPI: verifica GPL/LGPL/MIT, compatibilità per prodotto commerciale, genera NOTICE.txt | `check_requirements`, `lookup_license`, `check_compatibility`, `generate_notice` |

**Dipendenze di sistema opzionali (degradano gracefully se assenti):**

```bash
# perf_analyzer_mcp
sudo apt install valgrind kcachegrind linux-perf binutils
pip install memory-profiler

# test_generator_mcp (per compilare i test generati)
sudo apt install qtbase5-dev
pip install pytest pytest-asyncio pytest-qt
```

### Produttività e infrastruttura (15)

MCP pratici per il workflow quotidiano: i18n, database, monitoraggio, API, snippet, OCR, SSH, documentazione, traduzione, Docker, email, backup cloud, memoria chat, web scraping, note.

| Plugin | Funzione | Tool chiave | Dipendenze |
|--------|---------|------------|------------|
| `qt_i18n_mcp` | Gestione .ts Qt6: lupdate/lrelease, stringhe non tradotte, copertura | `run_lupdate`, `find_untranslated`, `coverage_report`, `add_translation` | `qttools5-dev-tools` (apt) |
| `sqlite_inspector_mcp` | Query read-only su graph_memory.db e rag_graph.db | `query`, `schema`, `stats`, `export_csv`, `vacuum` | stdlib only |
| `system_monitor_mcp` | CPU/RAM/GPU in tempo reale, processi Ollama, porte Prismalux | `cpu_mem`, `gpu_usage`, `watch_processes`, `network_io` | `psutil` (opzionale) |
| `api_tester_mcp` | Test endpoint LAN server :11500, WAN Compute :11600, Ollama | `check_lan_server`, `check_ollama`, `load_test`, `http_post` | stdlib only |
| `snippet_mcp` | Boilerplate Qt6 (page, widget, MCP, QTest) + snippet pattern | `new_page`, `new_widget`, `new_mcp`, `new_test`, `get_snippet` | stdlib only |
| `ocr_mcp` | OCR PDF scansionati → testo → RAG/ con Tesseract | `ocr_pdf`, `ocr_to_rag`, `batch_ocr`, `list_rag_pdfs` | `tesseract-ocr`, `pdf2image`, `pytesseract` |
| `ssh_remote_mcp` | SSH/rsync su host remoti, deploy WAN client | `connect_test`, `run_command`, `sync_build`, `deploy_wan_client` | `ssh rsync` (apt) |
| `docs_generator_mcp` | Doxygen HTML, copertura commenti, funzioni non documentate | `run_doxygen`, `check_coverage`, `find_undocumented`, `generate_doxyfile` | `doxygen graphviz` (apt) |
| `translation_mcp` | Auto-traduzione .ts Qt6 con LibreTranslate (self-hosted) | `translate_ts_file`, `translate_string`, `batch_translate` | LibreTranslate locale o remoto |
| `docker_mcp` | Gestione container Docker: run/stop/logs/build/exec | `list_containers`, `run_container`, `container_logs`, `build_image` | `docker.io` (apt) |
| `email_notify_mcp` | Notifiche email SMTP (build pass/fail, alert sistema) | `configure`, `notify_build`, `notify_alert`, `send_notification` | stdlib only (smtplib) |
| `cloud_backup_mcp` | Backup RAG/KNOWLEDGE/DB su Google Drive, Dropbox, S3 con rclone | `backup_all`, `backup_rag`, `restore_rag`, `list_cloud_files` | `rclone` (apt/curl) |
| `chat_memory_mcp` | Cronologia conversazioni LLM con ricerca temporale ("cosa ho detto stamattina?") | `save_exchange`, `ask_history`, `search_history`, `get_recent` | stdlib only (sqlite3) |
| `web_scraper_mcp` | Scarica e pulisce pagine web, Wikipedia, abstract arXiv → RAG/ | `fetch_to_rag`, `fetch_wikipedia`, `fetch_arxiv`, `batch_fetch` | stdlib only (urllib) |
| `note_mcp` | Note rapide con tag, scadenza e ricerca; reminder "due_today" | `add_note`, `search_notes`, `due_today`, `update_note` | stdlib only (sqlite3) |

### Riferimenti API ufficiali

| Strumento | Documentazione |
|-----------|---------------|
| Blender Python API | [docs.blender.org/api](https://docs.blender.org/api/current/info_quickstart.html) |
| FreeCAD Python | [wiki.freecad.org — Power Users Hub](https://wiki.freecad.org/Power_users_hub) |
| Bioconda | [bioconda.github.io](https://bioconda.github.io/) |
| GDScript (Godot) | [docs.godotengine.org — GDScript Basics](https://docs.godotengine.org/en/stable/tutorials/scripting/gdscript/gdscript_basics.html) |
| KiCAD Python | [pypi.org/project/kicad-python](https://pypi.org/project/kicad-python/) |
| Ollama Python SDK | `pip install ollama` |

---

## Avvio rapido

### Linux — AppImage (consigliato, zero installazione)

```bash
# Scarica e avvia
chmod +x Prismalux-x86_64.AppImage
./Prismalux-x86_64.AppImage

# Se il PC non ha FUSE 2 (Ubuntu 24.04+)
APPIMAGE_EXTRACT_AND_RUN=1 ./Prismalux-x86_64.AppImage

# Installa FUSE 2 una volta sola (poi funziona con doppio clic)
sudo apt install libfuse2t64
```

### Linux — PC nuovo (script automatico)

```bash
# 1. Installa FUSE 2 + Python + tutti i pacchetti pip
bash EXPORT/linux/setup_dipendenze.sh

# Solo FUSE 2 (per AppImage)
bash EXPORT/linux/setup_dipendenze.sh --solo-fuse

# Solo pip base rapido
bash EXPORT/linux/setup_dipendenze.sh --base

# 2. Avvia
./EXPORT/linux/Prismalux-x86_64.AppImage

# 3. Integrazione desktop (icona nel menu applicazioni)
bash EXPORT/linux/install_launcher.sh
```

### Linux — Compila da sorgente

```bash
# Installa dipendenze di compilazione
sudo apt install cmake ninja-build build-essential \
    qt6-base-dev qt6-tools-dev qt6-webengine-dev \
    libqt6sql6-sqlite qt6-multimedia-dev \
    libqt6bluetooth6 qt6-speech qt6keychain-dev \
    libonnxruntime-dev ffmpeg graphviz \
    sqlite3 openssl libssl-dev

# Compila (metodo semplice — multipiattaforma)
python3 build.py

# Oppure con cmake diretto
cmake -B gui/build_gui gui/ -DCMAKE_BUILD_TYPE=Release
cmake --build gui/build_gui -j$(nproc)

# Avvia
./gui/build_gui/Prismalux_GUI

# Compila + genera AppImage + ZIP Windows
./aggiorna.sh

# Solo GUI (senza AppImage/ZIP)
./aggiorna.sh --gui

# Installa nel sistema (icona + menu + comando `prismalux`)
./aggiorna.sh --install
```

### Windows

```bat
REM Prima volta — scarica Python embedded + Qt + GCC + CMake + Ninja (~600 MB)
COMPILE_WIN\setup.bat

REM Doppio clic per compilare (trova Python automaticamente)
build.bat

REM Avvia l'app
Avvia_Prismalux.bat
```

> `build.bat` cerca Python in: `COMPILE_WIN\toolchain\python\` → PATH sistema → MSYS2.  
> Al termine mostra chiaramente **BUILD COMPLETATA** o **BUILD FALLITA** con le ultime righe di errore.  
> In caso di errore viene generato `errore.txt` nella root con i dettagli.

### macOS

```bash
brew install qt@6 cmake ninja
python3 build.py
# oppure manualmente:
cmake -B build_mac gui/ -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
cmake --build build_mac -j$(sysctl -n hw.logicalcpu)
./build_mac/Prismalux_GUI
```

### Prerequisiti

#### Obbligatori

| Componente | Versione minima | Note |
|-----------|----------------|------|
| Qt6 | 6.4+ | Base, Network, Multimedia, PrintSupport, WebEngine |
| CMake | 3.20+ | |
| GCC / Clang | 11+ / 13+ | C++17 |
| [Ollama](https://ollama.com) | qualsiasi | Avvia con `ollama serve` — porta 11434 |
| Python 3 | 3.10+ | Per `build.py` e MCPs — `pip install -r requirements.txt` |

#### Opzionali — funzionalità aggiuntive

| Componente | Installazione | Funzionalità abilitata |
|-----------|--------------|----------------------|
| `qt6-webengine-dev` | `apt install qt6-webengine-dev` | Rendering LaTeX KaTeX (Analisi 1/2, output AI) |
| `qt6-svg-dev` | `apt install qt6-svg-dev` | Emoji SVG OpenMoji nel TriModeButton (opzionale, fallback testo emoji) |
| `libqt6bluetooth6` | `apt install libqt6bluetooth6` | BLE Chat Android AES-256-GCM |
| `qt6-speech` | `apt install qt6-speech` | TTS Android (QTextToSpeech voice loop) |
| `qt6keychain-dev` | `apt install qt6keychain-dev` | Token LAN nel keyring di sistema (fallback: file 0600) |
| `libonnxruntime-dev` | `apt install libonnxruntime-dev` | Embedder RAG locale ONNX (senza Python) |
| `ffmpeg` | `apt install ffmpeg` | STT Whisper (estrazione WAV), MCP Multimedia |
| `graphviz` | `apt install graphviz` | Rendering grafi DOT: GraphMemory, RagGraph, Multi-Agente |
| `android-tools-adb` | `apt install android-tools-adb` | MCP android-adb, deploy APK via ADB |
| `ripgrep` | `apt install ripgrep` | MCP prismalux-search (grep\_symbol veloce) |
| `poppler-utils` | `apt install poppler-utils` | pdftotext — PDF in Analisi Fenomeni, RAG |
| `libfuse2t64` | `apt install libfuse2t64` | AppImage su Ubuntu 24.04+ |
| **Sicurezza** | | |
| `nmap` | `apt install nmap` | MCP network-recon (port scan con fallback socket) |
| `whois dnsutils traceroute` | `apt install whois dnsutils traceroute` | MCP network-recon (WHOIS, DNS, traceroute) |
| `cppcheck` | `apt install cppcheck` | MCP sast (analisi statica C++ `gui/`) |
| `bandit` | `pipx install bandit` | MCP sast (analisi sicurezza codice Python MCPs/) |
| `pip-audit` | `pipx install pip-audit` | MCP dep-audit (CVE nei requirements) |

**Installazione completa in una riga (Linux):**

```bash
sudo apt install cmake ninja-build build-essential git \
    qt6-base-dev qt6-tools-dev qt6-webengine-dev qt6-multimedia-dev qt6-svg-dev \
    libqt6sql6-sqlite libqt6bluetooth6 qt6-speech qt6keychain-dev \
    libonnxruntime-dev ffmpeg graphviz sqlite3 openssl libssl-dev \
    android-tools-adb ripgrep poppler-utils libfuse2t64 \
    nmap whois dnsutils traceroute cppcheck pipx
```

```bash
# Installa Ollama + modello consigliato
curl -fsSL https://ollama.com/install.sh | sh
ollama pull qwen3:8b     # ~5 GB — ottimo italiano, think nativo

# Dipendenze Python (core)
pip install -r requirements.txt

# Dipendenze Python aggiuntive per funzionalità specifiche
pip install psutil python-dotenv watchdog          # utilità sistema e RAG auto-reload
pip install beautifulsoup4 lxml                    # web scraping (wiki, ricerca online)
pip install cryptography PyJWT                     # crittografia runtime, token BLE/WAN
pip install faster-whisper                         # STT Whisper (4× più veloce, ~150 MB)

# Tool di audit sicurezza (fuori dal venv principale)
pipx install bandit && pipx install pip-audit && pipx install safety
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
cmake -B Test/build_tests gui/ -DBUILD_TESTS=ON
cmake --build Test/build_tests -j$(nproc)
ctest --test-dir Test/build_tests -j4

# Test con Ollama attivo (suite AI stress)
ollama serve &
ctest --test-dir Test/build_tests -j1 -R AiStress
```

**Stato test:**
- ✅ 38/41 suite passano senza Ollama
- ⚠️ `SimulatoreAlgos` — FLAKY in `-j4`, PASS standalone (`RESOURCE_LOCK cpu_heavy`)
- ⚠️ `AiStress` / `AiIntegration` / `TeamCollab` — richiedono Ollama + modello + ≥2 GB RAM libera
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
│   ├── Prismalux_v2.9_Sorgenti_Linux.zip         ← sorgenti pure Linux
│   ├── Prismalux_v2.9_Sorgenti_Compila_Linux.zip ← script + sorgenti compilazione
│   ├── linux/
│   │   ├── crea_appimage.sh      ← Genera AppImage Linux
│   │   ├── install_launcher.sh   ← Installa shortcut KDE/GNOME
│   │   ├── Prismalux-x86_64.AppImage
│   │   └── Prismalux_v2.9_Linux.zip  ← AppImage + script install
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
├── MCPs/                         ← 50 plugin MCP (Python, JSON-RPC 2.0 stdio)
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
├── EXTERNAL_DeviceS/             ← Script/config dispositivi fisici (cam WIBY, Tuya, PTZ — non in git)
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
3. **Test**: `ctest --test-dir Test/build_tests -j4` verde sulle suite no-Ollama
4. **Lambda nelle `connect()`**: il context object (4° argomento) deve essere sempre specificato; tutti i puntatori catturati devono essere figli di quel context. Logica > 2 righe → slot nominato. `static QMetaObject::Connection` vietata.
5. **Path**: usa sempre `P::root()`, `P::kOllamaPort` ecc. da `prismalux_paths.h` — mai hardcode.
6. **Emoji in stringhe C++**: concatena quando il carattere successivo è cifra hex (`"\xe2\x80\x9c" "Testo"` non `"\xe2\x80\x9cTesto"`).

Vedi `gui/CLAUDE.md` per le convenzioni complete (ThemeManager, LanServer shutdown, AiClient API, Think Mode, RAG, MCP).

---

## Supporta il progetto ☕

Prismalux è software libero, sviluppato nel tempo libero. Se ti è utile e vuoi contribuire alla sua crescita, puoi offrire un caffè:

<div align="center">

[![Dona con PayPal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=wildlux%40gmail.com&item_name=Prismalux&currency_code=EUR&source=url)

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

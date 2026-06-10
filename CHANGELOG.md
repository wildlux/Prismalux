# Changelog — Prismalux

Tutte le versioni rilevanti. Il formato segue [Keep a Changelog](https://keepachangelog.com/it/1.0.0/).

---

## [2.9] — 2026-06-11 (corrente)

### Aggiunto
- **TTFT nell'header** — label `⚡ Nms` (verde/arancio/rosso) dopo ogni risposta AI
- **Auto-update GitHub** — controllo GitHub API 10s dopo l'avvio; notifica in status bar
- **Foto profilo CV** (LavoroPage) — anteprima circolare 48×48 con selezione JPG/PNG
- **CloudCompare** — pulsante "Apri CloudCompare" + istruzioni install apt/flatpak/win
- **RAG globale** — `AiClient::chat()` inietta automaticamente il contesto RAG in tutte le tab
- **VoiceCloner** — clonazione vocale XTTS-v2 (tab Multimedia → Clona Voce)
- **Hermes Agent** — memoria persistente sessioni + skill self-improving
- **`_inject_science`** — 50 conversioni scientifiche dirette nella chat (Ohm, RC, kWh, pH…)
- **Ricerca online fallback** — link `websearch:` quando LLM non conosce la risposta
- **Generatore Policy** — crea policy di sicurezza di rete (tab Programmazione → Rete)
- **Calcolo sottoreti con grafo** — CIDR → nodi Graphviz (tab Programmazione → Rete)
- **RAG multiformat** — OCR immagini (tesseract + vision LLM), video (Whisper), Office, PDF
- **DeepSeek warning** — badge "no vision" / "no tools" se il modello non supporta la feature
- **Temperatura nell'header** — indicatore colorato CPU/GPU (rosso ≥90°C, arancio ≥75°C)
- **Embedding selezionabile** — combo modello embedding RAG con indicatore compatibilità
- **Pausa/Riprendi RAG** — pulsante ⏸/⏵ durante l'indicizzazione
- **Controllo temperatura RAG** — auto-pausa a ≥80°C, riprende a <75°C
- **Modello Sicurezza** — raccomandazione `deepseek-coder:6.7b-instruct-q4_K_M` per GPU ≤4 GB

### Fisso
- Versione finestra mostrava v2.1 invece di v2.9 (path build_gui)
- File AI → spostato dentro Strumenti (sub-tab 10)
- DeepSeek vision/tools: warning dinamico invece di silenziosa disabilitazione
- `bgMode` — cambio chat non interrompe più la risposta in corso (buffer→sessione originale)
- Bug iniezione RAG — mostra errore embedding con nome modello + istruzione `ollama pull`
- 2× `_inject_science` — 50 conversioni completate (era troncata a metà)

---

## [2.8] — 2026-05-28

### Aggiunto
- **Multi-Agente** (tab 9 🕸️) — MasterAgent decompone in SubTask con `depends_on`; GraphMemory live
- **GraphMemory** — SQLite-backed: nodi/archi tipizzati, BFS neighbours, DOT/JSON export
- **RagGraph** — estrae entità+relazioni dai documenti RAG via LLM → GraphMemory
- **Grafo RAG** (tab Ricerca) — lista nodi filtrabile, click → dettaglio+vicini, PNG dark
- **LaTeX KaTeX** — rendering formule in Matematica (Analisi 1/2) e output AI
- **LatexView widget** — `QWebEngineView + KaTeX` con fallback `QTextEdit`
- **Scheda TFR** — pagina dedicata con calcolo automatico (D.M. 1976 + Belfiore ~120 comuni)
- **WAN Calcolo Distribuito** — TCP 11600, 28 task, cron 1-1440 min, dashboard BOINC-style
- **TLS LAN** — `QSslServer` + certificato self-signed generato al primo avvio
- **Coda FIFO LAN** — richieste LLM serializzate per multi-utente
- **Auth WAN** — token Bearer obbligatorio server+client; sandbox shell opt-in
- **SecurityAnalyzerPage** — 4 agenti Ollama paralleli (Injection/Segreti/Memoria/Config)
- **DevAgent LangGraph** — modifica codice in autonomia: read→patch→compile→fix (tab AppController)
- **OCR webcam** — tab Multimedia; OpenCV + tesseract
- **Mappa OSM + routing** — WorldMapWidget con waypoint, OSRM, polyline
- **Randomizer 52 formule** — shuffle casuale in Risolvi Passi (Matematica)
- **Tool use AI** — 9 tool: calc, fetch_url, ricerca, leggi_file, lista_file, python, search_rag, graph_memory, get_knowledge
- **Telegram Bot locale** — python-telegram-bot subprocess; IPC JSON stdin/stdout; whitelist ID
- **WhatsApp Bot locale** — polling bridge, whitelist numeri, auto-risposta AI
- **41 suite ctest** — 38 no-Ollama; include GraphMemory (65 test), SciCompute, RagGraph

### Fisso
- `LanServer` shutdown: `blockSignals(true)` evita SIGSEGV
- Coda LLM: secondo client non sovrascrive più lo stream del primo
- Parser HTTP hardening: whitelist metodi, null-byte, Content-Length >25MB → 400
- XSS lista lavoro: `innerHTML` → `createElement/textContent`
- CORS: rimosso `Access-Control-Allow-Origin: *`
- KaTeX: rimosso CDN jsdelivr, servito da risorse Qt locali

---

## [2.5] — 2026-05-19

### Aggiunto
- **App Android** (Qt6) — BLE chat AES-256-GCM, Quiz CCNA 209 domande, TTS/STT, LAN sync
- **LAN Server** — TCP 11500, endpoint `/api/chat`, `/api/generate`, `/api/whisper`, `/apk`
- **DPI scaling** — `dpi_utils.h` + `dpiScale(N)` per tutti i widget strutturali
- **i18n** — `QTranslator` + ~720 stringhe `tr()` in italiano
- **Microfono reale** — `QAudioSource` per STT dal vivo
- **Chat persistence** — `ChatHistory` in `~/.prismalux_chats/` + sidebar sessioni
- **Stop LLM istantaneo** — `m_ai->abort()` con `aborted()` pulito
- **QKeychain** — token LAN nel keychain di sistema (nessuna password in QSettings)
- **Step-Down Rule** — ogni funzione in `gui/pages/` richiama solo funzioni di livello inferiore
- **VPN & Tunnel** — tab dedicato in Programmazione → Rete (n2n, WireGuard, tinc)
- **Supply chain hash** — SHA-256 dei file GGUF in `KNOWLEDGE_USER/model_hashes.json`

---

## [2.2] — 2026-04-26

### Aggiunto
- **Analisi Fenomeni** — upload file (PDF/immagini), RAG + LLM
- **Carta Astrale** — coordinate manuali (nome+lat+lon quando città mancante)
- **RAG condiviso** — `AgentsConfigDialog::m_sharedRag` iniettato prima del RAG per-agente
- **Export chat** — PDF, MD, HTML, TXT con timestamp
- **Slider "Correggi con AI"** — 1-10 iterazioni con preview ∞ in Programmazione
- **MONITOR TTFT** — `elapsed timer` per risposta in ogni bolla AI
- **CCNA Quiz** — 209 domande, 15 temi
- **Sfida "Rivedi errori"** — salva sbagliate + dialog scroll colori+spiegazione

### Rinominato / Spostato
- `Cerca Lavoro` → `Lavoro` (tab Strumenti)
- Impostazioni: 14 tab flat → 4 gruppi (AI Locale / LLM / Aspetto / Sistema)

---

## [2.0] — 2026-04-01 (prima release GUI Qt)

### Aggiunto
- **GUI Qt6** — porta principale, sostituisce TUI C e script Python
- **Pipeline 6 agenti** — ruoli configurabili, Byzantino, Agente Autonomo
- **RAG** — engine JLT 256-dim, indicizzazione asincrona, chunk search
- **Matematica SymPy** — Sequenza→Formula, N-esimo, Espressione, Risolvi Passi
- **Programmazione** — Editor+AI, Agentica, Translitter, Reverse Engineering, REPL Python
- **Ricerca** — Paper arXiv, Brevetti, Cytoscape-Bio, RDKit, Bioconda
- **APP Controller** — Blender, FreeCAD, Office, Anki, KiCAD, OBS, OpenCode, Godot
- **LAN & WAN** — LAN Android (QR/ADB), GNS3 MCP
- **18 plugin MCP** — Ollama, Knowledge, DevAgent, Anki, GNS3, Cytoscape, RDKit…
- **Temi QSS** — Dark, Light, Solarized, Monokai, Nord, Dracula…
- **HardwareMonitor** — polling CPU/RAM/GPU ogni 2s, gauge nell'header
- **Build multipiattaforma** — `build.py` (Linux/macOS/Windows), `build.bat` launcher Windows

---

> Prismalux — *"Costruito per i mortali che aspirano alla saggezza."* 🍺

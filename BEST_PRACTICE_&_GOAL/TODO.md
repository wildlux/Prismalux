# Prismalux — TODO pendenti

> Aggiornato: 2026-05-30 | Versione: 2.9
> Build: `cmake --build build_gui -j$(nproc)`

---

## 📂 Feature pendenti

- [ ] **Auto-copia documento in RAG/** — quando l'utente carica un file (drag & drop o browse)
  per l'indicizzazione RAG e il file **non risiede già** in `Prismalux/RAG/`,
  copiarlo automaticamente dentro quella cartella prima di indicizzarlo.

  **Perché:** i documenti fuori da `RAG/` vengono indicizzati in sessione ma non
  ritrovati al riavvio (RagGraph scansiona solo `~/prismalux_rag_docs/` e `P::root()+"/RAG/"`).
  La copia garantisce persistenza senza che l'utente debba spostarli manualmente.

  **Comportamento atteso:**
  - File già in `RAG/` → indicizza senza toccare nulla
  - File esterno → copia in `RAG/<nomefile>` (sovrascrittura opzionale se esiste già)
  - Mostra messaggio: `"📄 Copiato in RAG/ — il documento è ora persistente"`
  - Se la copia fallisce (permessi, disco pieno) → indicizza comunque dalla posizione originale
    e avvisa l'utente

  **Punti di intervento:**
  - `agenti_page_ui.cpp` — zona drop RAG (drag & drop PDF/txt/md)
  - `impostazioni_page_ai.cpp` o `impostazioni_page_slots.cpp` — browse file per indicizzazione
  - `rag_graph.cpp::addFile()` — eventuale copia automatica a livello di engine

---

## 🛡️ Analizzatore di Sicurezza Difensiva

> Scopo: aiutare l'utente a **trovare e correggere** falle — mai a sfruttarle.
> Posizionamento naturale: sub-tab in **Programmazione [4]** o pannello dedicato in **Ricerca [6]**.

- [ ] **SecurityAnalyzerPage** — analisi difensiva del codice e della configurazione locale

  ### Analisi codice (LLM locale)
  - Carica file sorgente (C++/Python/JS/Bash) o incolla testo
  - LLM analizza e segnala:
    - Credenziali/token hardcoded nel codice
    - Comandi shell non sanificati (injection risk)
    - Buffer/memory issues (C/C++: strcpy, gets, sprintf senza bounds)
    - Dipendenze con versioni note vulnerabili
    - Path traversal, open redirect, SSRF nei server HTTP
  - Output: lista falle con **severità** (critica/alta/media/bassa) + **rimedio suggerito**
  - "Spiega il rischio" — bottone per approfondimento AI sulla singola falla

  ### Scanner dipendenze (offline)
  - Legge `requirements.txt`, `requirements.lock`, `CMakeLists.txt`, `package.json`
  - Confronta con una copia locale del database OSV/NVD (aggiornabile manualmente)
  - Segnala: pacchetto → CVE → CVSS score → versione sicura disponibile
  - Esporta report in Markdown/PDF

  ### Audit configurazione locale
  - Controlla porte aperte in ascolto sul PC (confronta con quelle note di Prismalux)
  - Verifica permessi file sensibili (`KNOWLEDGE_USER/`, token LAN, `.claude/`)
  - Controlla che i token LAN siano nel keychain e non in QSettings in chiaro
  - Segnala `.env` o file con credenziali senza `.gitignore`

  ### Audit dipendenze Prismalux stesso
  - Hash SHA-256 dei GGUF in `KNOWLEDGE_USER/model_hashes.json` — già implementato, da integrare nella UI
  - Verifica integrità dei 18 MCP Python (hash file sorgente al primo avvio, confronto ad ogni riavvio)
  - Alert se un MCP viene modificato esternamente

  ### Stack previsto
  - LLM locale (Ollama) per analisi semantica del codice
  - `pip-audit` o parsing offline di OSV JSON per CVE dipendenze
  - `ss -tlnp` / `QNetworkInterface` per porte locali
  - Nessuna chiamata cloud, nessun invio di codice all'esterno

---

## 🧪 Test mancanti (gap analysis 2026-05-30)

> Suite attuali: 41 (38 no-Ollama). Le aree sotto non hanno suite ctest.

### 🔴 Critici — logica complessa, zero test

- [ ] **test_pratico_finanza** — `pratico_page.cpp`
  - Codice Fiscale: algoritmo D.M. 23/12/1976 (consonanti/vocali, data, comune, check digit)
  - Lookup Belfiore: ~150 comuni + paesi esteri (codici Z), edge case nomi con apostrofo/accento
  - Calcolatori: mutuo (piano ammortamento francese), PAC (rendimento composto), pensione INPS
  - Scheda TFR: rivalutazione 1.5%+75% ISTAT, calcolo art. 2120 c.c., fiscalità separata

- [ ] **test_wan_compute_tasks** — `lan_wan_page.cpp`
  - Esecuzione dei 28 tipi di task (oggi solo struct dati testata, mai l'execution flow)
  - `wanCliHandleTask()`: ai_query, shell_cmd, python_repl, file_read/write, graphviz_render…
  - Dispatcher: assegnazione pending→idle, cron scheduler, retry su errore

- [ ] **test_rag_graph_pipeline** — `rag_graph.cpp`
  - Pipeline completa: file → LLM → JSON entità+relazioni → GraphMemory
  - Parsing JSON LLM (con <think>, code fence, JSON malformato)
  - `updateNode()` su entità già esistente (importance +0.1 invece di duplicare)
  - Separazione DB: `rag_graph.db` ≠ `graph_memory.db`

### 🟡 Importanti — funzionalità usate quotidianamente

- [ ] **test_docker_sandbox** — `code_interpreter_widget.cpp`
  - Esecuzione Python in sandbox Docker (`--rm -m256m --cpus 0.5 --network none`)
  - Fallback PythonExec quando Docker assente
  - Cattura stdout/stderr, timeout, matplotlib PNG output

- [ ] **test_translitter** — `programmazione_page_translitter.cpp`
  - Traduzione tra linguaggi: Python↔JS, Python↔C++, Python↔Java
  - Preservazione struttura (classi, funzioni, import)

- [ ] **test_file_parser** — `strumenti_file_page.cpp`
  - Estrazione testo da PDF (via pdftotext), Word (.docx), CSV
  - Chunking e indicizzazione nel RAG locale
  - Gestione file corrotti/vuoti/troppo grandi

- [ ] **test_repl_python** — `programmazione_page_git_repl.cpp`
  - Avvio/riavvio processo Python interattivo
  - I/O asincrono: stdin→stdout streaming
  - Importazione librerie, errori runtime, timeout

### 🟠 Bot locali — App Controller (tab già presenti, da implementare)

- [ ] **Telegram Bot locale** — `app_controller_page.cpp::buildTelegramTab()`
  - Token bot da @BotFather configurabile in Impostazioni
  - Messaggi in entrata → AI locale risponde
  - Comandi: `/ask`, `/status`, `/task`, `/stop`
  - Notifiche proattive (task WAN completato, alert sistema)
  - Whitelist ID Telegram (sicurezza)
  - Stack: python-telegram-bot o Telethon via MCP (`github.com/chigwell/telegram-mcp`)

- [ ] **WhatsApp Bot locale** — `app_controller_page.cpp::buildWhatsAppTab()`
  - Bridge locale via Baileys/whatsapp-web.js (no API ufficiali)
  - QR code autenticazione WhatsApp Web integrato nel tab
  - Messaggi in entrata → prompt AI → risposta automatica
  - Comandi: `!ask`, `!status`, `!immagine`
  - Whitelist numeri autorizzati
  - Nessun account Business richiesto
  - Stack: MCP whatsapp-mcp (`github.com/lharries/whatsapp-mcp`)

### 🟢 Nice-to-have — feature specializzate

- [ ] **test_astrale** — `ricerca_page_astrale.cpp`
  - Calcolo posizioni planetarie (coordinate manuali lat/lon)
  - Output testo + SVG/PNG carta natale

- [ ] **test_blhm_rab0l** — `ricerca_page.cpp` sezioni BLHM e RAB₀-L
  - Rendering canvas, formula matematica, interazione con PDF RAG

- [ ] **test_gns3_mcp** — `lan_wan_page.cpp` tab GNS3
  - Connessione REST API GNS3 (mock server)
  - Generazione codice Python topologia, esecuzione via subprocess

- [ ] **test_multi_agente_live** — `agenti_multi_page.cpp`
  - Pipeline completa con Ollama reale (simile a AiStress)
  - Decomposizione JSON, depends_on BFS, sintesi finale, nodi GraphMemory creati

---

## ✅ Implementati

### Sessione 2026-05-29 — TODO completati

- [x] **FEAT-1 parallelo** — Pool di 3 `AiClient` (`kMaxParallel = 3`)
  in `AgentiMultiPage`. `runNextPendingTask()` avvia TUTTI i task con
  dipendenze soddisfatte contemporaneamente (fino a 3). `initPool()` sinc
  i client col modello corrente ad ogni decomposizione.

- [x] **Cross-pollination agenti→grafo RAG** — `AgentiMultiPage::setExtRagMemory()`
  riceve la `GraphMemory` di RagGraph; `onTaskResultDone()` scrive ogni
  risultato come nodo `"fact"` anche nel grafo RAG. Collegato in
  `MainWindow::buildMultiAgentTab()`.

- [x] **Auto-trigger RagGraph** — `RicercaPage::onAutoRagTrigger()` collega
  `ImpostazioniPage::indexingFinished` → avvio automatico del RagGraph
  dopo 800 ms (delay per flush FS). Cablato in `ensureSettingsDialog()`.

### Sessione 2026-05-29 — FEAT-1 Multi-Agente + FEAT-2 Grafo RAG

- [x] **GraphMemory** (`gui/graph_memory.h/cpp`) — fondazione comune:
  SQLite-backed, nodi+archi tipizzati, BFS neighbours, ricerca testuale,
  export DOT/JSON/TXT, pruneByImportance, segnale `changed()` real-time

- [x] **FEAT-1 Multi-Agente** (`gui/pages/agenti_multi_page.h/cpp`) — tab [9] 🕸️:
  MasterAgent decompone JSON → SubTask con `depends_on`,
  sub-agenti sequenziali con contesto condiviso, sintesi finale,
  GraphMemory live (nodi, archi, export TXT, clear), viste Grafo DOT / JSON

- [x] **FEAT-2 RagGraph** (`gui/rag_graph.h/cpp`) — estrazione LLM:
  scansiona `~/prismalux_rag_docs/` e `Prismalux/RAG/`, estrae entità+relazioni
  via LLM (JSON), persiste in GraphMemory separata (`~/.prismalux/rag_graph.db`)

- [x] **FEAT-2 Tab 🕸️ Grafo RAG** in RicercaPage:
  lista nodi filtrabile, click→dettaglio+vicini, Graphviz PNG dark-theme,
  DOT source, progresso file per file, stop, svuota

### Sessione 2026-05-29 — Voce + Donazione

- [x] **TTS + STT web** — tab "🎙️ Voce" (ex Whisper):
  SpeechSynthesis (voce/velocità configurabili), MediaRecorder→/api/whisper,
  Invia in Chat, barra livello microfono

- [x] **TTS Android** — box "🔊 Sintesi Vocale" in AudioPage:
  QTextToSpeech, pulsante Parla/Stop, voce italiana

- [x] **Fix STT Android** — rimosso `m_ai->transcribeAudio()` inesistente,
  ora usa `uploadWhisper()` direttamente

- [x] **Donazione PayPal** — README badge + sezione, `.github/FUNDING.yml`,
  pulsante in Impostazioni→Ringraziamenti e APK Android Info page

### Sessioni precedenti

- [x] LaTeX KaTeX rendering (Analisi 1/2, output AI) + LatexView widget
- [x] Randomizer 🔀 52 formule Risolvi Passi
- [x] Test SymPy CAT-E (15 test, 62/62 PASS)
- [x] Scheda TFR con C.F. automatico (D.M. 1976 + Belfiore)
- [x] WAN Calcolo Distribuito (TCP:11600, 28 task, cron)
- [x] Ollama MCP (18° plugin, cache SQLite, 5 tool)
- [x] Quiz CCNA 209 domande (15 temi)
- [x] DPI dpiScale(), i18n QTranslator, supply chain hash

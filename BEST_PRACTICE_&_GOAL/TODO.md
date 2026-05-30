# Prismalux — TODO pendenti

> Aggiornato: 2026-05-29 | Versione: 2.9
> Build: `cmake --build build_gui -j$(nproc)`

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

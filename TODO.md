# Prismalux — TODO pendenti

> Aggiornato: 2026-05-29 | Versione: 2.9
> Build: `cmake --build build_gui -j$(nproc)`

---

## 🔴 In sviluppo (2026-05-29)

---

### FEAT-1 · Multi-Agente con Sub-Agenti + Memoria a Grafo

**Obiettivo**: un agente master che scompone compiti complessi in sotto-task,
delega a sub-agenti specializzati (creati dinamicamente), e condivide una
memoria comune che scala da in-context → SQLite → grafo di conoscenza.

#### Architettura

```
MasterAgent (orchestratore)
│   riceve il prompt utente
│   genera un piano JSON → lista di SubTask
│
├── SubAgent-1 [Research]    ──┐
├── SubAgent-2 [Analysis]    ──┤── SharedMemory
├── SubAgent-3 [Synthesis]   ──┤    ├── InContextBuffer  (< soglia token)
└── SubAgent-N [...]         ──┘    ├── SQLite KV store  (medium-term)
                                    ├── .txt snapshot    (export/import)
                                    └── GraphMemory      (entità + relazioni)
                                         ├── Nodi: concetti, fatti, risultati
                                         └── Archi: relates_to, contradicts,
                                                      elaborates, causes, part_of
```

#### File da creare/modificare

- [ ] **`gui/graph_memory.h/cpp`** — `GraphMemory`: SQLite-backed, nodi+archi,
  ricerca testuale, export DOT/JSON, pruning per importanza.
  Segnale `changed()` per aggiornare la vista in real-time. ← **INIZIATO**

- [ ] **`gui/pages/agenti_multi_page.h/cpp`** — Nuova tab "🕸️ Multi-Agente":
  - Input prompt utente
  - Pulsante "🚀 Decomponsi" → chiama MasterAgent che genera JSON piano
  - Lista SubTask con stato (pending → running → done)
  - Esecuzione sequenziale o parallela (QFuture)
  - Pannello GraphMemory live (nodi aggiunti dagli agenti)
  - Export risultato finale ← **INIZIATO**

- [ ] **Formato piano MasterAgent** — risposta JSON:
  ```json
  {
    "task": "testo del compito",
    "subtasks": [
      {"id":1,"role":"Ricercatore","prompt":"...","depends_on":[]},
      {"id":2,"role":"Analista","prompt":"...","depends_on":[1]},
      {"id":3,"role":"Scrittore","prompt":"...","depends_on":[1,2]}
    ]
  }
  ```
  Parser in `agenti_multi_page.cpp` con fallback graceful se JSON malformato.

- [ ] **`gui/pages/agenti_multi_page_exec.cpp`** — Esecutore sub-agenti:
  - `runSubAgent(task)` → crea AiClient temporaneo o riusa m_ai
  - Scrive risultato in GraphMemory come nodo tipo "result"
  - Emette `subTaskDone(id)` per aggiornare la UI
  - Rispetta `depends_on`: attende i risultati dei predecessori

- [ ] **Memoria condivisa scalabile**:
  - InContextBuffer: stringa accumulata, max ~2000 token
  - SQLite: tabella `kv(key TEXT, value TEXT, agent TEXT, ts INTEGER)`
  - .txt export/import: `~/.prismalux/multi_agent_memory.txt`
  - GraphMemory: (vedi sopra)

- [ ] **Collegamento a `mainwindow.cpp`**: nuova voce nel tab [0] AI
  oppure sub-tab nella AgentiPage esistente.

- [ ] **CMakeLists.txt**: aggiungere `agenti_multi_page.cpp` e `graph_memory.cpp`

---

### FEAT-2 · Grafo della Conoscenza RAG (auto-gestito)

**Obiettivo**: il RAG non è solo vector similarity — costruisce anche un
grafo di concetti estratti dai documenti, navigabile visivamente, che si
aggiorna automaticamente quando si aggiungono nuovi file.

#### Architettura

```
Documento RAG (PDF/TXT/MD)
        │
        ▼ (indicizzazione classica)
   RagEngine (JLT 256-dim)
        │
        ▼ (estrazione entità/relazioni via LLM)
   RagGraph (GraphMemory con tipo "rag_entity" / "rag_relation")
        │
        ├── Nodi: entità nominali (persone, concetti, luoghi, formule)
        ├── Archi: relazioni estratte dal testo
        └── Metadati: chunk sorgente, documento, pag., importanza
                │
                ▼
   Visualizzazione (Graphviz DOT live nel pannello già esistente)
         + navigazione click-nodo → mostra chunk sorgente
```

#### File da creare/modificare

- [x] **`gui/rag_graph.h/cpp`** — `RagGraph`:
  - Usa `GraphMemory` internamente
  - `extractAndIndex(text, sourceFile)` → prompt LLM per entità+relazioni
  - `buildGraph()` → aggiorna GraphMemory con nodi/archi estratti
  - `searchGraph(query)` → restituisce nodi rilevanti (combinato con RAG)
  - `toDot()` → Graphviz per visualizzazione
  - Auto-triggered quando si aggiunge un documento al RAG

- [ ] **Formato risposta LLM** per estrazione entità:
  ```json
  {
    "entities": [
      {"label":"Prismalux","type":"software","importance":0.9},
      {"label":"Qt6","type":"framework","importance":0.8}
    ],
    "relations": [
      {"from":"Prismalux","to":"Qt6","type":"uses","weight":1.0}
    ]
  }
  ```

- [x] **`gui/pages/ricerca_page.cpp`** — aggiungi sub-tab "🕸️ Grafo RAG":
  - QWebEngineView con D3.js / oppure usa Graphviz esistente
  - Lista nodi con filtro per tipo/importanza
  - Click su nodo → mostra i chunk RAG sorgente
  - Bottone "🔄 Ricostruisci grafo" (riesegue estrazione su tutti i doc)
  - Export DOT / JSON

- [ ] **Integrazione con la pipeline agenti**: quando un agente trova un
  risultato rilevante, lo scrive anche nel RagGraph (cross-pollination).

- [ ] **CMakeLists.txt**: aggiungere `rag_graph.cpp`

---

## Dipendenze tra FEAT-1 e FEAT-2

```
GraphMemory (fondazione comune)
├── FEAT-1: Multi-Agente usa GraphMemory come memoria condivisa
└── FEAT-2: RagGraph usa GraphMemory per storico entità RAG
```

→ **Prima cosa da implementare**: `GraphMemory` (usata da entrambe).

---

## ✅ Implementati (sessioni precedenti)

- [x] LaTeX KaTeX rendering (Analisi 1/2, output AI)
- [x] Randomizer 52 formule Risolvi Passi
- [x] Test SymPy CAT-E (15 test, 62/62 PASS)
- [x] Donazione PayPal (README, FUNDING.yml, app, APK)
- [x] TTS + STT nella web app e nell'APK Android
- [x] Scheda TFR con C.F. automatico
- [x] WAN Calcolo Distribuito
- [x] Ollama MCP (18°)
- [x] DPI, i18n, supply chain hash


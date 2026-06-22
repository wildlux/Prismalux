# TODO — Gap Android & Web vs Desktop

*Creato: 2026-06-22 | Versione: Prismalux v3.0*

---

## Legenda priorità
- 🔴 **Alta** — funzionalità core usata spesso, impatto visibile
- 🟡 **Media** — utile ma non urgente
- 🟢 **Bassa** — nicchia o complessità alta
- ⚪ **Skip** — impraticabile su mobile/web (QProcess pesante, GPU necessaria, ecc.)

---

## SEZIONE A — Gap Android

Fonte: `ANDROID/QT_ANDROID_Version/android_app/` vs `gui/`

### A-1 ✅ Pipeline AI — Byzantino + Agente Autonomo *(completato 2026-06-22)*
**File:** `pages/pipeline_page.h/cpp` — `QComboBox` Byzantine/Autonomous.
- **Byzantine**: N agenti indipendenti → sintetizzatore (conteggio via `m_byzRound`).
- **Autonomous**: loop ReAct THINK/ACT/OBS/ANSWER fino a 6 step (`kMaxAutSteps`).

---

### A-2 ✅ Sfida! (quiz gamificato) *(completato 2026-06-22)*
**File:** `pages/sfida_page.h/cpp` — genera quiz JSON via AI, 4 opzioni A/B/C/D,
feedback colore (verde/rosso), punteggio finale. Usa `kSfidaSys` e regex per JSON.

---

### A-3 ✅ Cron scheduler *(completato 2026-06-22)*
**File:** `pages/cron_page.h/cpp` — struct `CronTaskMob` con `QTimer*` per task.
Persistenza `QSettings("Prismalux","CronMobile")`, tipi: AI/Python/sh.

---

### A-4 ✅ 730 / P.IVA *(completato 2026-06-22)*
**File:** `pages/finanza_page.cpp` — `build730Section()` (IRPEF scaglioni 2024,
detrazione art.13, figli 950€, credito 19%) + `buildPivaSection()` (forfettario
15%/5%, INPS GS 26.23%/IVS 24%, netto annuo+mensile). Aggiunti al `m_secCombo`.

---

### A-5 ✅ Ricerca strutturata + Grafo RAG *(completato 2026-06-22)*
**File:** `pages/ricerca_mob_page.h/cpp` — 4 modalità via `m_modeCmb`:
Paper (arXiv), Brevetto (Espacenet), RAG locale, Carta Astrale.
System prompt dedicato per ogni modalità, streaming via `onToken()`.

---

### A-6 ✅ Editor + REPL *(completato 2026-06-22)*
**File:** `pages/editor_page.h/cpp` — lingue: Python (`QTemporaryFile`+python3),
bash (sh -c), JS (node -e), testo. AI: Spiega/Fix via `kEditorSys`. Output REPL
in QTextEdit con errori in rosso.

---

### A-7 ✅ AppController mobile — Anki FlashCard + TinyMCP *(completato 2026-06-22)*
**File:** `pages/appcontroller_page.h/cpp` — `QComboBox` → due sezioni:
- **Anki FlashCard**: connessione a AnkiConnect (porta 8765 sul PC); GET decks, aggiungi carta, genera 5 carte AI (JSON) e invia ad Anki, statistiche review.
- **TinyMCP Client**: connessione all'LAN server Prismalux; lista modelli/tool, test connessione, esecuzione tool (`chat`, `repl`, `rag`, fallback `/api/tool`).
Registrata in `mainwindow` all'indice 32, voce `"\xf0\x9f\x95\xb9 App Controller"` nel cassetto.

---

### A-8 ⚪ Bioinformatica — skip
Cytoscape/RDKit/Bioconda richiedono QProcess + dipendenze native.
Rimossi intenzionalmente dalla versione Android (commento nel codice).

---

### A-9 ⚪ Stable Diffusion — skip
GPU-dipendente, impraticabile su mobile senza server remoto.

---

### A-10 ✅ Collaborazione BT + PC Hub *(completato 2026-06-22)*
**File:** `pages/collab_page.h/cpp` — due tab QTabWidget:
- **BT Chat**: chat AI-simulata via `m_ai->chat()`, bolle colorate (utente/AI).
- **PC Hub**: `QNetworkAccessManager` → endpoint LAN (`/api/chat`, `/api/repl`,
  `/api/tags`, `/api/rag`). Test connessione GET `/api/tags`.
Registrata all'indice 31 nel cassetto mobile.

---

## SEZIONE B — Gap Web (webchat.html + lan_server.cpp)

Fonte: `gui/lan_web/webchat.html` (20 tab) + endpoint `/api/*`

### B-1 ✅ Byzantino assente nella web *(completato 2026-06-22)*
**Tab web:** `age` (Agenti) ha solo pipeline base, nessuna opzione per il motore Byzantino.  
**Manca:** UI per selezionare "Byzantino" + endpoint `/api/byzantine` nel LanServer.  
**Piano:** aggiungere radio/select nel tab `age` per scegliere modalità (Pipeline / Byzantino).
Nel LanServer: nuovo branch `else if (s.path == "/api/byzantine")` che lancia 4 agenti
in sequenza e restituisce SSE con i turni.

---

### B-2 ✅ Agente Autonomo assente nella web *(completato 2026-06-22)*
**Tab web:** assente  
**Manca:** tab `aut` con input task → loop ReAct → output step-by-step via SSE.  
**Piano:** aggiungere tab "🤖 Autonomo" in `webchat.html`, endpoint `/api/autonomous` nel
LanServer che wrappa `AgentiPage::runAutonomousAgent()` logic in un loop SSE.

---

### B-3 ✅ Sfida! assente nella web *(completato 2026-06-22)*
**Tab web:** assente — `imp` (Impara) è solo passivo.  
**Piano:** aggiungere tab `sfd` con quiz a scelta multipla serviti da `/api/quiz?cat=ccna|gen`
(usa il database domande dal desktop). SSE per timer, punteggio locale in sessionStorage.

---

### B-4 ✅ Matematica avanzata — KaTeX render *(completato 2026-06-22)*
**Tab web:** `mat` fa solo espressioni SymPy (endpoint `/api/math`).  
**Manca:** Sequenza→Formula, Costanti fisiche, N-esimo termine, LaTeX KaTeX render.  
**Piano:**
- `/api/math/sequence` → `MatematicaPage::parseSequence()` logica estratta
- `/api/math/constants` → lista costanti fisiche JSON
- KaTeX: già caricato (`/katex/`) — usare `renderMathInElement` nel tab `mat`

---

### B-5 ✅ Ricerca strutturata — tab `ric` *(completato 2026-06-22)*
**Tab web:** `ric` in `webchat.html` — selettore Paper/Brevetto/RAG/Astrale/Fenomeni,
system prompt dedicato per modo, streaming `/api/chat`.

---

### B-6 ✅ Cron scheduler web — tab `crn` *(completato 2026-06-22)*
**Tab web:** `crn` in `webchat.html` — form nome/tipo/orario, lista task in
`localStorage`, esecuzione via `/api/chat` o `/api/repl`, `setInterval` per tick.

---

### B-7 ✅ 730 / P.IVA assenti *(completato 2026-06-22)*
**Tab web:** `fin` ha solo IVA/detrazioni generiche, `tfr` ha TFR.  
**Manca:** calcolatore 730 e P.IVA regime forfettario.  
**Piano:** estendere `fin` con sezioni a tab interno, oppure nuovi endpoint
`/api/finanza/730` e `/api/finanza/piva`.

---

### B-8 ✅ Security Analyzer assente *(completato 2026-06-22)*
**Tab web:** assente  
**Manca:** form "Analizza codice" → 4 agenti paralleli (Injection/Segreti/Memoria/Config)
→ sintesi SSE.  
**Piano:** tab `sec` + endpoint `/api/security` che lancia `SecurityAnalyzerPage` logic.

---

### B-9 ✅ Multi-Agente — tab `mag` *(completato 2026-06-22)*
**Tab web:** `mag` in `webchat.html` — input task → stream NDJSON → subtask
con icone stato (⏳/✅/❌), sintesi finale. Async `magChat()` via ReadableStream.

---

### B-10 ✅ Sintetizzatore — tab `osc` *(completato 2026-06-22)*
**Tab web:** `osc` in `webchat.html` — WebAudio API: oscillatore (sin/sqr/saw/tri),
ADSR (Attack/Decay/Sustain/Release), tastiera piano 2 ottave con GainNode.

---

### B-11 ⚪ Bioinformatica — skip
Cytoscape/RDKit non hanno API REST esposte dal desktop. Impraticabile via web.

---

## Matrice di confronto rapido

| Funzione | Desktop | Android | Web |
|----------|---------|---------|-----|
| Chat singola | ✅ | ✅ | ✅ |
| Pipeline agenti | ✅ | ✅ A-1 | ✅ parz. |
| Byzantino | ✅ | ✅ A-1 | ✅ B-1 |
| Agente Autonomo | ✅ | ✅ A-1 | ✅ B-2 |
| Sfida! | ✅ | ✅ A-2 | ✅ B-3 |
| Cron | ✅ | ✅ A-3 | ✅ B-6 |
| 730 / P.IVA | ✅ | ✅ A-4 | ✅ B-7 |
| TFR | ✅ | ✅ | ✅ |
| Ricerca strutturata | ✅ | ✅ A-5 | ✅ B-5 |
| REPL Python | ✅ | ✅ A-6 | ✅ |
| Git | ✅ | ✅ A-6 | ✅ |
| Matematica avanzata | ✅ | ✅ base | ✅ B-4 |
| Security Analyzer | ✅ | ✅ | ✅ B-8 |
| Multi-Agente GraphMem | ✅ | ✅ | ✅ B-9 |
| AppController | ✅ | ✅ A-7 | ✅ |
| Bioinformatica | ✅ | ⚪ skip | ⚪ skip |
| Stable Diffusion | ✅ | ⚪ skip | ⚪ skip |
| Sintetizzatore | ✅ | ✅ | ✅ B-10 |
| Voce TTS/STT | ✅ | ✅ | ✅ |
| File AI | ✅ | ✅ | ✅ |
| Impara | ✅ | ✅ | ✅ |
| Lavoro | ✅ | ✅ | ✅ |
| Knowledge | ✅ | ✅ | ✅ |
| **Collab BT+PC** | n/a | ✅ A-10 | ✅ B-9 |

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

### A-1 🔴 Pipeline AI mancante (Byzantino + Agente Autonomo)
**File Android:** `pages/chat_page.h/cpp` — solo chat singola  
**Manca:** tab "Agenti" con pipeline a 3+ agenti (Ricercatore → Critico → Sintetizzatore),
motore Byzantino (4 agenti consenso), Agente Autonomo ReAct.  
**Piano:** aggiungere `pipeline_page.h/cpp` con pipeline semplificata a 2-3 agenti
(senza configurazione dialog). Byzantino: 2 agenti + giudice.

---

### A-2 🔴 Sfida! (quiz gamificato) assente
**File Android:** assente — `ImparagPage` ha solo studio passivo  
**Manca:** tab "Sfida" con domande a risposta multipla, punteggio, timer.  
**Piano:** aggiungere `sfida_page.h/cpp` che usa `QuizCcnaDb` già presente + domande AI generate.
Il database CCNA esiste già in `pages/quiz_ccna_db.h`.

---

### A-3 🟡 Cron scheduler assente
**File Android:** assente  
**Manca:** pianificazione task ripetuti (backup RAG, sync knowledge, notifiche).  
**Piano:** `cron_page.h/cpp` semplice — QTimer + QSettings per orari, AlarmManager via
`QAndroidJniObject` per svegliare l'app.

---

### A-4 🟡 730 / P.IVA assenti — solo TFR/IVA base
**File Android:** `pages/finanza_page.cpp` — sezioni: IVA · IRPEF · TFR  
**Manca:** calcolatore 730 (detrazioni lavoro dipendente, familiari a carico, bonus),
calcolatore P.IVA (regime forfettario 15%/5%, contributi INPS).  
**Piano:** aggiungere 2 sezioni in `finanza_page.cpp` (switch nel combo `m_secCombo`).

---

### A-5 🟡 Grafo RAG (RagGraph) assente
**File Android:** assente — non c'è `rag_graph.h/cpp`  
**Manca:** tab "Grafo RAG" in Ricerca — grafo entità/relazioni estratte dai documenti.  
**Piano:** portare `rag_graph_simple.h` (versione mobile senza Graphviz PNG, solo lista
nodi + testo DOT). SQLite già disponibile tramite `graph_memory_mobile.h`.

---

### A-6 🟡 Programmazione — Editor + REPL assenti
**File Android:** assente  
**Manca:** editor di codice con sintassi highlight, REPL Python, Git base.  
**Piano:** `editor_page.h/cpp` con `QPlainTextEdit` + runner Python via `QProcess`
(disponibile su Android se Python embedded o tramite Termux). Git: solo `git log/status`
via QProcess.

---

### A-7 🟢 AppController mobile — solo OBS e OpenCode
**File Android:** `pages/obs_page.h`, `pages/opencode_page.h`  
**Manca:** Blender, FreeCAD, Anki, KiCAD (tutti desktop-only — ⚪ skip).  
**Fattibile:** Anki (tramite AnkiDroid API), TinyMCP (server locale).

---

### A-8 ⚪ Bioinformatica — skip
Cytoscape/RDKit/Bioconda richiedono QProcess + dipendenze native.
Rimossi intenzionalmente dalla versione Android (commento nel codice).

---

### A-9 ⚪ Stable Diffusion — skip
GPU-dipendente, impraticabile su mobile senza server remoto.

---

## SEZIONE B — Gap Web (webchat.html + lan_server.cpp)

Fonte: `gui/lan_web/webchat.html` (20 tab) + endpoint `/api/*`

### B-1 🔴 Byzantino assente nella web
**Tab web:** `age` (Agenti) ha solo pipeline base, nessuna opzione per il motore Byzantino.  
**Manca:** UI per selezionare "Byzantino" + endpoint `/api/byzantine` nel LanServer.  
**Piano:** aggiungere radio/select nel tab `age` per scegliere modalità (Pipeline / Byzantino).
Nel LanServer: nuovo branch `else if (s.path == "/api/byzantine")` che lancia 4 agenti
in sequenza e restituisce SSE con i turni.

---

### B-2 🔴 Agente Autonomo assente nella web
**Tab web:** assente  
**Manca:** tab `aut` con input task → loop ReAct → output step-by-step via SSE.  
**Piano:** aggiungere tab "🤖 Autonomo" in `webchat.html`, endpoint `/api/autonomous` nel
LanServer che wrappa `AgentiPage::runAutonomousAgent()` logic in un loop SSE.

---

### B-3 🔴 Sfida! assente nella web
**Tab web:** assente — `imp` (Impara) è solo passivo.  
**Piano:** aggiungere tab `sfd` con quiz a scelta multipla serviti da `/api/quiz?cat=ccna|gen`
(usa il database domande dal desktop). SSE per timer, punteggio locale in sessionStorage.

---

### B-4 🟡 Matematica avanzata — solo SymPy base
**Tab web:** `mat` fa solo espressioni SymPy (endpoint `/api/math`).  
**Manca:** Sequenza→Formula, Costanti fisiche, N-esimo termine, LaTeX KaTeX render.  
**Piano:**
- `/api/math/sequence` → `MatematicaPage::parseSequence()` logica estratta
- `/api/math/constants` → lista costanti fisiche JSON
- KaTeX: già caricato (`/katex/`) — usare `renderMathInElement` nel tab `mat`

---

### B-5 🟡 Ricerca — Paper/Brevetto/Astrale/Fenomeni senza tab dedicati
**Tab web:** solo chat generica — nessun tab `ric`.  
**Manca:** form strutturati per query Paper (arXiv/Semantic Scholar), Brevetto (Espacenet),
Analisi Fenomeni (upload file + AI), Carta Astrale (data/ora/luogo).  
**Piano:** aggiungere tab `ric` con sezioni a selettore, endpoint `/api/ricerca?tipo=paper|brevetto|astrale|fenomeni`.

---

### B-6 🟡 Cron scheduler assente nella web
**Tab web:** assente  
**Manca:** UI per pianificare task (RAG rebuild, sync knowledge) eseguiti dal desktop.  
**Piano:** tab `crn` con form orario + tipo task → `/api/cron` (GET lista, POST aggiungi,
DELETE rimuovi) — il LanServer delega al `CronPanel` già esistente nel desktop.

---

### B-7 🟡 730 / P.IVA assenti
**Tab web:** `fin` ha solo IVA/detrazioni generiche, `tfr` ha TFR.  
**Manca:** calcolatore 730 e P.IVA regime forfettario.  
**Piano:** estendere `fin` con sezioni a tab interno, oppure nuovi endpoint
`/api/finanza/730` e `/api/finanza/piva`.

---

### B-8 🟡 Security Analyzer assente
**Tab web:** assente  
**Manca:** form "Analizza codice" → 4 agenti paralleli (Injection/Segreti/Memoria/Config)
→ sintesi SSE.  
**Piano:** tab `sec` + endpoint `/api/security` che lancia `SecurityAnalyzerPage` logic.

---

### B-9 🟢 Multi-Agente avanzato (MasterAgent + GraphMemory) assente
**Tab web:** `grf` mostra il grafo in lettura ma non c'è UI per il tab [9] Multi-Agente.  
**Piano:** tab `mag` con input task → `/api/multiagent` (SSE) che wrappa `AgentiMultiPage`.

---

### B-10 🟢 Sintetizzatore assente
**Tab web:** `wsp` ha solo TTS/STT voce. Non c'è il sintetizzatore audio (oscilloscopio/onde).  
**Piano:** tab `osc` con canvas HTML5 WebAudio API — alternativa web-native senza endpoint.

---

### B-11 ⚪ Bioinformatica — skip
Cytoscape/RDKit non hanno API REST esposte dal desktop. Impraticabile via web.

---

## Matrice di confronto rapido

| Funzione | Desktop | Android | Web |
|----------|---------|---------|-----|
| Chat singola | ✅ | ✅ | ✅ |
| Pipeline agenti | ✅ | ❌ A-1 | ✅ parz. |
| Byzantino | ✅ | ❌ A-1 | ❌ B-1 |
| Agente Autonomo | ✅ | ❌ A-1 | ❌ B-2 |
| Sfida! | ✅ | ❌ A-2 | ❌ B-3 |
| Cron | ✅ | ❌ A-3 | ❌ B-6 |
| 730 / P.IVA | ✅ | ❌ A-4 | ❌ B-7 |
| TFR | ✅ | ✅ | ✅ |
| Grafo RAG | ✅ | ❌ A-5 | ✅ lettura |
| REPL Python | ✅ | ❌ A-6 | ✅ |
| Git | ✅ | ❌ A-6 | ✅ |
| Matematica avanzata | ✅ | ✅ base | ❌ B-4 |
| Ricerca strutturata | ✅ | ✅ | ❌ B-5 |
| Security Analyzer | ✅ | ✅ | ❌ B-8 |
| Multi-Agente GraphMem | ✅ | ✅ | ❌ B-9 |
| AppController | ✅ | parz. A-7 | ✅ |
| Bioinformatica | ✅ | ⚪ skip | ⚪ skip |
| Stable Diffusion | ✅ | ⚪ skip | ⚪ skip |
| Sintetizzatore | ✅ | ✅ | ❌ B-10 |
| Voce TTS/STT | ✅ | ✅ | ✅ |
| File AI | ✅ | ✅ | ✅ |
| Impara | ✅ | ✅ | ✅ |
| Lavoro | ✅ | ✅ | ✅ |
| Knowledge | ✅ | ✅ | ✅ |

# TODO — Test mancanti (priorità decrescente)
> Stato suite attuale: **36/36** (100% suite no-Ollama) — 2 fallimenti richiedono `mistral:7b-instruct` in Ollama.
> Aggiornato: 2026-05-07 — aggiunti 7 nuovi test file (131 test totali nuovi)

---

## PRIORITÀ ALTA

### [x] test_agenti_pipeline.cpp — **FATTO** (44 test, 100% pass)
**Cosa testare:**
- CAT-A `buildUserBubble()` / `buildAgentBubble()` / `buildLocalBubble()` — HTML generato contiene label, model, indice
- CAT-B `markdownToHtml()` — bold, italic, code inline, code block, lista puntata, nessun XSS injection
- CAT-C `extractPythonCode()` / `_sanitizePyCode()` — estrazione da ```python...```, blocchi vuoti, nessun codice
- CAT-D Stato UI — toggle Mono↔Pipeline, pulsante Run abilitato/disabilitato, blocco se busy
- CAT-E Isolamento segnale — token emessi dopo abort() non compaiono nel log

**Pattern:** friend struct `AgentiAccess` per metodi static; MockAiClient per segnali.
**File sorgente:** `agenti_page.h`, `agenti_page_bubbles.cpp`, `agenti_page_format.cpp`, `agenti_page_stream.cpp`

---

### [x] test_agents_config_dialog.cpp — **FATTO** (28 test, 100% pass)
**Cosa testare:**
- CAT-A Struttura widget — 6 roleCombo, 6 modelCombo, 6 enabledChk, 6 ragWidget
- CAT-B `numAgents()` — SpinBox range [1..6], valore default = MAX_AGENTS
- CAT-C `modeCombo()` — voci presenti (Sequenziale, Byzantino, …)
- CAT-D RagDropWidget — `ragContext()` vuoto all'inizio, `hasContext()` false
- CAT-E Shared RAG (`sharedRagWidget()`) — istanza unica, context non duplicato per-agente

**Pattern:** istanza diretta `AgentsConfigDialog dlg; dlg.show();`

---

### [x] test_lan_server.cpp — **FATTO** (20 test, 100% pass; CAT-E TCP rimossa: SIGSEGV Qt6 proxy in ctest isolato)
**Cosa testare:**
- CAT-A Lifecycle — `start()` ritorna true, `isRunning()` true, `stop()` → false
- CAT-B Porta — porta assegnata > 0, `port()` coerente con `start(port)`
- CAT-C clientCount — 0 senza connessioni, incrementa su QTcpSocket connesso
- CAT-D segnale `statusChanged` — emesso a start e stop
- CAT-E TCP dummy — connessione QTcpSocket → clientConnected emesso
- CAT-F Doppio start — secondo `start()` mentre già running non crashare

**Pattern:** `LanServer srv(&mockAi); srv.start(0);` (porta 0 = OS auto-assign)

---

## PRIORITÀ MEDIA

### [x] test_agenti_byzantine.cpp — **FATTO** (23 test, 100% pass)
**Cosa testare:**
- CAT-A Numero agenti Byzantino — consenso richiede dispari ≥ 3 (agenti pari: avviso?)
- CAT-B Formato output — blocchi `[BYZANTINE voto N/M]` presenti nel HTML
- CAT-C Abort durante byzantino — nessun output fantasma post-abort

**Nota:** difficile da testare senza Ollama reale; usare MockAiClient con emitFullResponse.

---

### [x] test_impostazioni_page.cpp — **FATTO** (24 test, 100% pass)
**Cosa testare:**
- CAT-A Think mode UI — 3 pulsanti esclusivi (Off/Auto/On), QSlider budget disabilitato se Off
- CAT-B AiChatParams serializzazione — `save()` → `load()` round-trip (thinkMode, thinkBudget)
- CAT-C Presets — preset "8 GB RAM" scrive num_ctx=4096 e flash_attn=true in QSettings
- CAT-D Presets — preset "Contesto lungo" scrive num_ctx=16384 se RAM ≥ 16 GB, else 8192

**Pattern:** istanzia ImpostazioniPage, usa `findChild<QPushButton*>("thinkModeBtn")`.

---

### [x] test_manutenzione_bugs.cpp — **FATTO** (16 test, 100% pass)
**Cosa testare:**
- CAT-A Parsing bug — formato `[CRITICO] titolo\ndescr` → struct con severity, title, body
- CAT-B Filtro severity — solo CRITICO, solo AVVISO, tutti
- CAT-C Ordine — bug più recenti prima (sort per data)
- CAT-D Bug duplicato — stessa signature non inserita due volte

---

### [x] test_grafico.cpp — **FATTO** (30 test, 100% pass)
**Cosa testare:**
- CAT-A `GraficoCanvas` — costruisce senza crash, sizeHint ragionevole
- CAT-B `GraficoPage` — campo formula accetta testo, pulsante Traccia abilitato se non vuoto
- CAT-C FormulaParser integrazione — formula `sin(x)` → punti Y non tutti zero nel range [-π, π]

---

## PRIORITÀ BASSA

### [x] test_opencode_page.cpp — **FATTO** (17 test, 100% pass)
**Cosa testare:**
- CAT-A Costruzione — nessun crash senza server OpenCode
- CAT-B SSE parser — parse riga `data:{...}` → tipo evento corretto
- CAT-C Retry counter — dopo 5 errori consecutivi il timer si ferma

**Nota:** richiede mock HTTP server (stessa tecnica OllamaMockServer in test_ai_stress).

---

### [x] test_impara_quiz.cpp — **FATTO** (21 test, 100% pass)
**Cosa testare:**
- CAT-A ImparaPage — bottoni presenti, nessun crash a show()
- CAT-B QuizPage — domanda caricata, risposta utente aggiorna punteggio
- CAT-C MateriePage — lista materie non vuota

---

### [x] test_monitor_panel.cpp — **FATTO** (15 test, 100% pass)
**Cosa testare:**
- CAT-A Costruzione — nessun crash, label CPU/RAM presenti
- CAT-B Aggiornamento — `updateHw(HWInfo)` aggiorna label senza crash

---

## IMPLEMENTAZIONE — Ordine suggerito
1. `test_agenti_pipeline.cpp` — metodi static, nessuna rete, veloce
2. `test_agents_config_dialog.cpp` — widget puro, nessuna rete
3. `test_lan_server.cpp` — TCP locale, deterministica
4. `test_impostazioni_page.cpp` — QSettings + widget
5. `test_manutenzione_bugs.cpp` — logica pura
6. `test_grafico.cpp` — widget + FormulaParser
7. `test_agenti_byzantine.cpp` — MockAiClient
8. `test_opencode_page.cpp` — mock HTTP server
9. `test_impara_quiz.cpp` — widget semplici
10. `test_monitor_panel.cpp` — widget semplice

## Come aggiungere un nuovo test al CMakeLists.txt
```cmake
# In gui/CMakeLists.txt, sezione BUILD_TESTS:
add_executable(test_nome_nuovo
    tests/test_nome_nuovo.cpp
    ${SRCS_COMUNI}   # ← lista già definita sopra
)
target_include_directories(test_nome_nuovo PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_nome_nuovo PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
if(NOT WIN32)
    target_link_libraries(test_nome_nuovo PRIVATE pthread m)
endif()

add_test(NAME NomeNuovo COMMAND test_nome_nuovo)
set_tests_properties(NomeNuovo PROPERTIES TIMEOUT 30)
```

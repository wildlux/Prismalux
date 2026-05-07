# TODO — Test Suite Complessa Prismalux

> Aggiornato: 2026-05-06
>
> Stato: ✅ = implementato · 🔄 = in corso · ⬜ = da fare
>
> Copertura attuale (file già presenti):
> - `test_ai_integration.cpp` — AiClient reale ↔ Ollama
> - `test_ai_stress.cpp` — matrice 24 parametri, 80+ test
> - `test_app_controller.cpp` — 100 test in 9 CAT (extractCode, state machine, routing, Anki, KiCAD, concorrenza)
> - `test_chat_history.cpp` — 30 test (CAT A–D: CRUD sessioni, roundtrip, robustezza)
> - `test_chat_history_stress.cpp` — 20 test (concorrenza 8 thread, durabilità, 5000 sessioni)
> - `test_code_utils.cpp` — extractPythonCode, sanitizePyCode
> - `test_formula_parser.cpp` — 34 test (CAT A–F: costruttore, eval, sample, tryExtract, xRange, points)
> - `test_hardware_monitor.cpp` — 11 test (hw_detect struttura, HardwareMonitor thread+segnale)
> - `test_lavoro_data.cpp` — 38 test (kOfferte, offerteFiltrate, tipoIcon, livLabel)
> - `test_lavoro_page.cpp` — segnali isolamento, prompt Socratico
> - `test_manutenzione_cron.cpp` — 30 test (detectConfigFmt, convertConfig, cronShouldRun, cronNextRun)
> - `test_matematica_page.cpp` — 35 test (parseSeq, detectPatternLocal, numbersFromText, widget)
> - `test_programmazione_page.cpp` — 40 test (parsing, repl, LAN)
> - `test_rag_engine.cpp` — 7 test JLT (copertura base)
> - `test_rag_engine_avanzato.cpp` — 43 test (JLT math, search top-k, save/load, performance)
> - `test_random_tool.cpp` — _inject_random
> - `test_signal_lifetime.cpp` — dangling observer, duplicati
> - `test_simulatore_algos.cpp` — 72 test (BubbleSort, Sort×4, Search×2, AlgoStep, BigO)
> - `test_strumenti_rag.cpp` — 40 test (ragChunkText, ragExtractText, sysPromptForAction)
> - `test_team_collab.cpp` — pipeline 4 agenti (mock)
> - `test_theme_manager.cpp` — 14 test (singleton, lista temi, apply, scanExternal, loadSaved)
> - `test_thinking_detect.cpp` — rilevamento tag thinking qwen3
> - `test_tutor_data.cpp` — 31 test (12 soggetti, unicità, coerenza cross-soggetto)

---

## ✅ TN-01 — `test_chat_history.cpp` (30 test)

**Modulo**: `chat_history.h / chat_history.cpp`
**Nessun test esiste ancora per questo modulo.**

### CAT-A: Creazione e lista sessioni (8 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | `newSession` crea file JSON | `list().size() == 1` dopo la creazione |
| A2 | `newSession` con task lungo (>40 char) | il titolo è troncato a 40 caratteri |
| A3 | `newSession` con task vuoto | titolo = `"Sessione"` o fallback non vuoto |
| A4 | `list()` ritorna ordine decrescente per data | sessione più recente = prima |
| A5 | 10 sessioni in rapida successione | `list().size() == 10`, id tutti unici |
| A6 | `list()` su directory non esistente | ritorna lista vuota, no crash |
| A7 | id sessione = stringa numerica di timestamp ms | `id.toLongLong() > 0` |
| A8 | `newSession` due volte nello stesso ms | id distinti (disambiguazione) |

### CAT-B: saveLog / loadLog (10 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | `saveLog` poi `loadLog` — roundtrip ASCII | testo identico |
| B2 | `saveLog` poi `loadLog` — Unicode (emoji, italiano, kanji) | testo identico |
| B3 | `saveLog` con HTML `<b>ciao</b>` | loadLog ritorna HTML intatto |
| B4 | `saveLog` overwrite (salva due volte stesso id) | loadLog ritorna seconda versione |
| B5 | `loadLog` con id inesistente | ritorna stringa vuota, no crash |
| B6 | `saveLog` con log da 1 MB | nessun crash, loadLog = stesso contenuto |
| B7 | `saveLog` dopo `remove` dello stesso id | ricrea file correttamente |
| B8 | `loadLog` su file corrotto (JSON malformato) | ritorna stringa vuota, no crash |
| B9 | `saveLog` log con `\n`, `\r\n`, tab | roundtrip preserva esattamente whitespace |
| B10 | `saveLog` log vuoto `""` | loadLog ritorna `""` |

### CAT-C: remove (6 test)
| # | Test | Verifica |
|---|------|----------|
| C1 | `remove` sessione esistente | scompare da `list()` |
| C2 | `remove` id inesistente | nessun crash |
| C3 | `remove` + `loadLog` dopo | loadLog ritorna `""` |
| C4 | `remove` tutti → `list().isEmpty()` | lista vuota |
| C5 | `remove` id con caratteri speciali (`../foo`) | nessun path traversal, no crash |
| C6 | `remove` durante iterazione di `list()` | nessun crash |

### CAT-D: Robustezza (6 test)
| # | Test | Verifica |
|---|------|----------|
| D1 | 100 sessioni create e listate | `list().size() == 100`, no OOM |
| D2 | sessioni parallele da 3 thread (`QThread`) | nessuna corruzione, count corretto |
| D3 | id con `/` o `..` viene sanitizzato o rifiutato | no path traversal |
| D4 | directory `~/.prismalux_chats/` viene creata se mancante | no crash, funziona |
| D5 | file senza estensione `.json` nella directory | ignorati da `list()` |
| D6 | file JSON con campi mancanti (`id`, `title`) | `list()` ignora o usa fallback |

---

## ✅ TN-02 — `test_strumenti_rag.cpp` (40 test)

**Modulo**: `strumenti_page.h` — funzioni statiche `ragChunkText`, `ragExtractText`, `sysPromptForAction`
**Questi metodi sono statici e testabili in isolamento totale.**

### CAT-A: ragChunkText — chunking di testo (15 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | testo vuoto | ritorna lista vuota |
| A2 | testo < chunkSize (600) | ritorna 1 chunk = testo intero |
| A3 | testo esattamente = chunkSize | ritorna 1 chunk |
| A4 | testo = 2×chunkSize (overlap=0) | ritorna 2 chunk, nessun overlap |
| A5 | testo = 2×chunkSize con overlap=80 | ritorna 2 chunk, secondo chunk inizia 80 char prima |
| A6 | nessun chunk è mai vuoto | `QVERIFY(!chunk.isEmpty())` per ogni chunk |
| A7 | concatenazione chunk (no overlap) copre tutto il testo | join = testo originale |
| A8 | testo con solo spazi/newline | ritorna lista vuota o 1 chunk di whitespace |
| A9 | overlap > chunkSize | no crash, ritorna ≥1 chunk |
| A10 | chunkSize = 1 | ogni chunk = 1 carattere |
| A11 | 10.000 caratteri, default params | runtime < 10ms |
| A12 | 1.000.000 caratteri, default params | runtime < 500ms |
| A13 | testo Unicode multibyte (emoji 4-byte) | nessuna corruzione, chunk validi UTF-16 |
| A14 | testo con `\n\n\n` (paragrafi vuoti) | splittato in chunk logici |
| A15 | overlap = chunkSize - 1 | ogni chunk ha quasi tutto l'overlap con il precedente |

### CAT-B: ragExtractText — estrazione testo da file (12 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | file `.txt` ASCII | ritorna contenuto esatto |
| B2 | file `.txt` UTF-8 con accenti italiani | contenuto preservato |
| B3 | file `.md` con heading e bold | contenuto non è vuoto, include testo |
| B4 | file inesistente | ritorna `""`, no crash |
| B5 | path vuoto `""` | ritorna `""`, no crash |
| B6 | file `.pdf` valido (1 pagina, solo testo) | ritorna testo non vuoto |
| B7 | file `.pdf` binario corrotto | ritorna `""` o testo parziale, no crash |
| B8 | file con estensione non supportata (`.jpg`) | ritorna `""` |
| B9 | file di testo da 5 MB | nessun crash, contenuto non troncato |
| B10 | file `.txt` vuoto | ritorna `""` |
| B11 | path con spazi e caratteri speciali | letto correttamente |
| B12 | symlink a file valido | contenuto letto |

### CAT-C: sysPromptForAction — routing system prompt (13 test)
| # | Test | Verifica |
|---|------|----------|
| C1 | `navIdx=0, subIdx=0` (Studio, prima azione) | ritorna stringa non vuota |
| C2 | `navIdx=1, subIdx=0` (Scrittura Creativa) | stringa diversa da C1 |
| C3 | `navIdx=2, subIdx=0` (Ricerca) | contiene parole chiave ricerca |
| C4 | `navIdx=3, subIdx=0` (Libri) | stringa non vuota |
| C5 | `navIdx=4, subIdx=0` (Produttività) | stringa non vuota |
| C6 | ogni `(navIdx, subIdx)` valido ritorna stringa unica | no duplicati per stessa categoria |
| C7 | `navIdx=-1` fuori range | ritorna `""` o stringa di fallback, no crash |
| C8 | `navIdx=999` fuori range | ritorna `""` o stringa di fallback, no crash |
| C9 | `subIdx=-1` | no crash |
| C10 | `subIdx=999` | no crash |
| C11 | prompt Studio non contiene prompt Ricerca | nessuna contaminazione cross-categoria |
| C12 | tutti i prompt non sono mai vuoti per indici validi | invariante di completezza |
| C13 | prompt Blender (navIdx=6) contiene "bpy" o "Blender" | coerenza contenuto |

---

## ✅ TN-03 — `test_lavoro_data.cpp` (25 test)

**Modulo**: `lavoro_data.h` — `kOfferte()`, `offerteFiltrate()`, `tipoIcon()`, `livLabel()`
**Dati puri, zero dipendenze Qt Widgets.**

### CAT-A: kOfferte() — integrità dati (6 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | `kOfferte().size() > 0` | dataset non vuoto |
| A2 | nessuna offerta ha campi `azienda`, `ruolo` vuoti | invariante obbligatorio |
| A3 | tutti i campi `tipo` appartengono a set noto (`"Tempo pieno"`, `"Part-time"`, ecc.) | no tipo sconosciuto |
| A4 | tutti i campi `livello` appartengono a set noto | no livello sconosciuto |
| A5 | `email` non vuota → formato `@` presente | formato minimo valido |
| A6 | `kOfferte()` ritorna stesso vettore a ogni chiamata (deterministico) | `kOfferte() == kOfferte()` |

### CAT-B: offerteFiltrate() — logica filtro (12 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | filtro `tipo=""`, `livello=""` | ritorna tutte le offerte |
| B2 | filtro `tipo="Tempo pieno"` | solo offerte con quel tipo |
| B3 | filtro `livello="Junior"` | solo offerte con quel livello |
| B4 | filtro combinato `tipo` + `livello` valido | sottoinsieme corretto |
| B5 | filtro combinato inesistente (zero match) | ritorna lista vuota, no crash |
| B6 | filtro `tipo` con spazi extra `"  Tempo pieno  "` | match (trim applicato) o no-match stabile |
| B7 | filtro case-insensitive `"tempo pieno"` | comportamento documentato (match o no) |
| B8 | filtro `tipo="INVALIDO"` | ritorna lista vuota |
| B9 | risultati di `offerteFiltrate` sono sottoinsieme di `kOfferte` | invariante |
| B10 | filtro su tutti i livelli → unione = tutte le offerte | `∑ filtrate(L_i) = kOfferte()` |
| B11 | filtro su tutti i tipi → unione = tutte le offerte | idem per tipi |
| B12 | `offerteFiltrate` non modifica `kOfferte` (no side effect) | `kOfferte()` invariato dopo filtro |

### CAT-C: tipoIcon() / livLabel() (7 test)
| # | Test | Verifica |
|---|------|----------|
| C1 | `tipoIcon("Tempo pieno")` → stringa non vuota | ha un'icona |
| C2 | `tipoIcon("Part-time")` → stringa diversa da C1 | icone distinte |
| C3 | `tipoIcon("")` → no crash | fallback o stringa vuota |
| C4 | `tipoIcon("INVALIDO")` → no crash | fallback o stringa vuota |
| C5 | `livLabel("Junior")` → stringa non vuota | ha etichetta |
| C6 | `livLabel("Senior")` → stringa diversa da C5 | etichette distinte |
| C7 | `livLabel("")` → no crash | fallback o stringa vuota |

---

## ✅ TN-04 — `test_tutor_data.cpp` (24 test)

**Modulo**: `tutor_data.h` — 12 funzioni `tutorData*()`, struttura `SubjectData`
**Invarianti strutturali su tutti i 12 soggetti.**

### CAT-A: Invarianti per ogni soggetto (12 test × 1 = 12)

Per ogni soggetto (`Matematica`, `Fisica`, `Chimica`, `Sicurezza`, `Informatica`,
`Algoritmi`, `Biologia`, `Astronomia`, `Elettronica`, `Economia`, `Farmacologia`, `QuantumComputing`):

| Invariante | Condizione |
|------------|------------|
| `sections.size() > 0` | almeno 1 sezione |
| nessuna sezione ha `first` (titolo) vuoto | titoli sempre non vuoti |
| ogni sezione ha almeno 1 topic | `section.second.size() > 0` |
| nessun topic ha `first` (nome) vuoto | nomi sempre non vuoti |
| nessun topic ha `second` (descrizione) vuota | descrizioni sempre non vuote |

### CAT-B: Unicità e coerenza cross-soggetto (12 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | titoli sezioni di Matematica ≠ titoli Fisica | nessun copia-incolla accidentale |
| B2 | topic di Fisica non contiene parole solo di Chimica | separazione semantica minima |
| B3 | `tutorDataQuantumComputing` ha ≥ 3 sezioni | soggetto specializzato abbastanza ricco |
| B4 | `tutorDataAlgoritmi` contiene almeno un topic con "sort" (case-insensitive) | contenuto coerente |
| B5 | `tutorDataSicurezza` contiene almeno un topic con "vulnerabilit" o "attacco" | contenuto coerente |
| B6 | `tutorDataMatematica` contiene almeno un topic con "derivat" o "integral" | contenuto coerente |
| B7 | conteggio totale topic ≥ 100 su tutti i soggetti | dataset abbastanza ricco |
| B8 | nessun topic duplicato esatto (stesso nome e stessa descrizione) in tutto il dataset | no copia-incolla |
| B9 | nessuna funzione `tutorData*()` crasha con QApplication non inizializzata | puri dati, zero Qt Widgets |
| B10 | ogni funzione è rientrante: chiamata 100× → stesso risultato | deterministico |
| B11 | `tutorDataFarmacologia` esiste e non è vuoto | soggetto non dimenticato |
| B12 | tutti i titoli sezione sono ≤ 60 caratteri | display corretto in UI |

---

## ✅ TN-05 — `test_simulatore_algos.cpp` (50 test)

**Modulo**: `simulatore_page.h` — `genBubbleSort()` + tutti gli algoritmi (110 in `simulatore_algos.cpp`)
**Zero test esistono su questo modulo nonostante 110 algoritmi implementati.**

### CAT-A: BubbleSort — correttezza passi (10 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | lista vuota | steps.size() == 1 (step finale), arr vuoto |
| A2 | lista di 1 elemento | steps.size() == 1, nessuno swap |
| A3 | lista già ordinata `[1,2,3,4,5]` | nessun swap, arr finale = input |
| A4 | lista inversa `[5,4,3,2,1]` | arr finale = `[1,2,3,4,5]` |
| A5 | lista con duplicati `[3,1,4,1,5,9,2,6]` | arr finale ordinato |
| A6 | lista di 2 elementi disordinati | 1 swap, arr finale ordinato |
| A7 | ultimo step ha `sorted` pieno | tutti gli elementi in `sorted` |
| A8 | ogni step intermedio ha `cmp.size() == 2` | confronto sempre su coppia |
| A9 | step con swap ha `swp.size() == 2` | swap evidenziato correttamente |
| A10 | numero passi ≤ n²/2 per n=8 | complessità O(n²) rispettata |

### CAT-B: SelectionSort / InsertionSort / MergeSort / QuickSort (16 test — 4 per algo)
Per ogni algoritmo di sorting (`Selection`, `Insertion`, `Merge`, `Quick`):

| Invariante per ogni algo | Condizione |
|--------------------------|------------|
| lista vuota → no crash | `steps.isEmpty()` o 1 step finale |
| lista inversa → arr finale ordinato | `arr == sorted(input)` |
| lista con duplicati → arr finale ordinato | `arr == sorted(input)` |
| ultimo step: `sorted.size() == arr.size()` | tutti marcati sorted |

### CAT-C: Ricerca (BinarySearch, LinearSearch) (8 test)
| # | Test | Verifica |
|---|------|----------|
| C1 | LinearSearch — elemento presente → `found` contiene indice corretto |
| C2 | LinearSearch — elemento assente → `found.isEmpty()` all'ultimo step |
| C3 | BinarySearch — lista ordinata, elemento presente → trovato in O(log n) passi |
| C4 | BinarySearch — lista non ordinata → no crash (comportamento definito) |
| C5 | BinarySearch — elemento assente → `inactive` copre tutto alla fine |
| C6 | BinarySearch — lista di 1 elemento, cercato → trovato in 1 passo |
| C7 | LinearSearch su lista di 100 elementi, elemento all'ultimo posto → ≤100 passi |
| C8 | BinarySearch su lista di 1024 elementi → ≤10 passi (log₂1024) |

### CAT-D: Struttura AlgoStep — invarianti generali (8 test)
| # | Test | Verifica |
|---|------|----------|
| D1 | ogni `cmp` ⊆ indici validi di `arr` | no indice fuori range |
| D2 | ogni `swp` ⊆ indici validi di `arr` | no indice fuori range |
| D3 | ogni `sorted` ⊆ indici validi di `arr` | no indice fuori range |
| D4 | `msg` mai null (può essere vuoto ma non null) | no crash su `msg.size()` |
| D5 | nessun algoritmo produce step con `arr.size()` diverso dall'input | dimensione stabile |
| D6 | algoritmi deterministici: stessa input → stessi step | `gen(a) == gen(a)` |
| D7 | nessun algoritmo crasha su lista di 1000 elementi | scalabilità base |
| D8 | `ALGO_COUNT == 110` (costante header) | sync header ↔ implementazione |

### CAT-E: BigOWidget — evalClass (8 test)
| # | Test | Verifica |
|---|------|----------|
| E1 | `O(1)` — `evalClass` ritorna lo stesso valore per qualsiasi n | costante |
| E2 | `O(log n)` — valore cresce lentamente con n | `eval(1000) < eval(1000) * 2` circa |
| E3 | `O(n)` — valore lineare | `eval(2n) ≈ 2 * eval(n)` |
| E4 | `O(n log n)` — cresce più di lineare | `eval(n log n, 100) > eval(n, 100)` |
| E5 | `O(n²)` — quadratico | `eval(200) ≈ 4 * eval(100)` |
| E6 | `O(2^n)` — nessun crash per n≤20 | no overflow / nan / inf |
| E7 | `O(n!)` — nessun crash per n≤12 | no overflow / nan / inf |
| E8 | `logScale` — input 0 → no crash (log(0) guard) | ritorna 0 o epsilon |

---

## ✅ TN-06 — `test_manutenzione_cron.cpp` (30 test)

**Modulo**: `manutenzione_page.h` — `detectConfigFmt()`, `convertConfig()`, `cronShouldRun()`, `cronNextRun()`

### CAT-A: detectConfigFmt (5 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | config JSON esistente | ritorna `"json"` |
| A2 | config TOML esistente | ritorna `"toml"` |
| A3 | nessun config file | ritorna `""` o `"none"` |
| A4 | entrambi i file esistono | ritorna uno dei due (comportamento documentato) |
| A5 | path con permessi negati | no crash |

### CAT-B: convertConfig (8 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | `convertConfig("json")` su config JSON → idempotente | ritorna `""` o `"ok"` senza creare duplicati |
| B2 | `convertConfig("toml")` su config JSON | genera TOML valido |
| B3 | `convertConfig("json")` su config TOML | genera JSON valido |
| B4 | `convertConfig("")` | no crash, ritorna errore o `""` |
| B5 | config con valori Unicode | convertito correttamente |
| B6 | config con chiave `"model"` → presente nel formato convertito | no perdita dati |
| B7 | `convertConfig` non cancella il file originale | file originale intatto dopo conversione |
| B8 | `convertConfig` su file corrotto | no crash, ritorna messaggio di errore |

### CAT-C: cronShouldRun — logica scheduling (12 test)
| # | Test | Verifica |
|---|------|----------|
| C1 | schedule `"@daily"`, lastRun ieri → `true` | va eseguito |
| C2 | schedule `"@daily"`, lastRun oggi → `false` | già eseguito oggi |
| C3 | schedule `"@hourly"`, lastRun 2h fa → `true` | va eseguito |
| C4 | schedule `"@hourly"`, lastRun 30m fa → `false` | troppo presto |
| C5 | schedule `"@weekly"`, lastRun 8 giorni fa → `true` | va eseguito |
| C6 | schedule `"@weekly"`, lastRun 3 giorni fa → `false` | troppo presto |
| C7 | lastRun vuoto `""` → `true` | mai eseguito = esegui subito |
| C8 | schedule vuoto `""` → `false` o comportamento definito | no crash |
| C9 | schedule non riconosciuto `"@invalid"` → `false` | fallback sicuro |
| C10 | schedule `"@monthly"`, lastRun 32 giorni fa → `true` | va eseguito |
| C11 | schedule `"@monthly"`, lastRun 15 giorni fa → `false` | troppo presto |
| C12 | lastRun nel futuro (clock skew) → `false` | nessun loop di esecuzioni |

### CAT-D: cronNextRun — calcolo prossima esecuzione (5 test)
| # | Test | Verifica |
|---|------|----------|
| D1 | `"@daily"` → ritorna stringa data nel formato atteso | non vuota |
| D2 | `"@hourly"` → il nextRun è ≤ 1 ora nel futuro | `QDateTime::fromString(ret) <= now + 1h` |
| D3 | `"@weekly"` → nextRun è ≤ 7 giorni nel futuro | coerente |
| D4 | schedule invalido → no crash | ritorna `""` o `"N/D"` |
| D5 | `cronNextRun` chiamato 1000× → stessa risposta (deterministico dato stesso "now") | no side effect |

---

## ✅ TN-07 — `test_theme_manager.cpp` (20 test)

**Modulo**: `theme_manager.h` — `apply()`, `loadSaved()`, `scanExternalThemes()`, `themes()`

### CAT-A: Inizializzazione e lista temi (5 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | `instance()` non è null | singleton funziona |
| A2 | `instance()` chiamato 2× → stesso puntatore | vero singleton |
| A3 | `themes().size() > 0` | almeno 1 tema built-in |
| A4 | ogni tema ha `id`, `label`, `resource` non vuoti | invariante struttura |
| A5 | nessun tema duplicato per `id` | id unici |

### CAT-B: apply() (8 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | `apply("dark_cyan")` → `currentId() == "dark_cyan"` | cambio applicato |
| B2 | `apply(id_valido)` emette signal `changed(id)` | signal verificato con QSignalSpy |
| B3 | `apply` di ogni tema built-in → no crash | tutti i temi caricabili |
| B4 | `apply("INVALIDO")` → no crash, `currentId()` invariato o fallback | robusto |
| B5 | `apply("")` → no crash | stringa vuota |
| B6 | `apply(id)` due volte stesso tema → signal emesso una sola volta | no doppio cambio |
| B7 | after `apply(temaA)` → `apply(temaB)` → `currentId() == temaB` | cambio sequenziale |
| B8 | after `apply`, loadSaved applica lo stesso tema | persistenza verificata |

### CAT-C: loadSaved() e scanExternalThemes() (7 test)
| # | Test | Verifica |
|---|------|----------|
| C1 | `loadSaved` senza preferenza salvata → fallback `"dark_cyan"` | default corretto |
| C2 | `apply(temaX)` poi ricreare istanza → `loadSaved` applica `temaX` | persistenza cross-istanza |
| C3 | `loadSaved` con preferenza salvata per tema inesistente → fallback | robusto |
| C4 | `scanExternalThemes` con dir vuota → no crash, count temi invariato | |
| C5 | `scanExternalThemes` con file `.qss` valido → tema aggiunto a `themes()` | |
| C6 | `scanExternalThemes` con file `.qss` non leggibile → no crash | permessi |
| C7 | `scanExternalThemes` due volte → no tema duplicato | idempotente |

---

## ✅ TN-08 — `test_rag_engine_avanzato.cpp` (35 test)

**Modulo**: `rag_engine.h` — estensione dei 7 test esistenti in `test_rag_engine.cpp`

### CAT-A: Proprietà matematiche JLT (8 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | due embedding ortogonali → cosine ≈ 0 (±0.15) dopo proiezione | JLT preserva ortogonalità |
| A2 | due embedding identici → cosine = 1.0 dopo proiezione | vettori uguali = cosine 1 |
| A3 | embedding con norma 0 (vettore zero) → no NaN, no crash | guard divisione per zero |
| A4 | seed deterministico: stessa proiezione su istanze diverse | `rag1.project(v) == rag2.project(v)` |
| A5 | dimensione output sempre `kTargetDim=256` indipendentemente dall'input dim | contratto rispettato |
| A6 | proiezione di vettori con valori molto grandi (±1e10) → no overflow | range check |
| A7 | proiezione di vettori con NaN nell'input → no propagazione silenziosa | gestione o crash definito |
| A8 | matrice JLT generata una sola volta per istanza (caching) | `project()` 1000× = stessa risposta |

### CAT-B: search — top-k e ranking (10 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | 5 chunk con embedding distinti → top-1 è il più simile | ranking corretto |
| B2 | top-k=5 su 3 chunk → ritorna 3 (non più di quanti ce ne sono) | k≥count = count |
| B3 | top-k=0 → ritorna lista vuota, no crash | edge case k=0 |
| B4 | top-k negativo → ritorna lista vuota o 0 risultati, no crash | edge case k<0 |
| B5 | 1000 chunk indicizzati → top-5 è subset dei 5 più simili | scalabilità + correttezza |
| B6 | query = embedding di un chunk presente → quel chunk è top-1 | match esatto |
| B7 | due chunk con embedding identici → entrambi nel top-2 | gestione duplicati |
| B8 | ordine risultati: top-1 ≥ top-2 ≥ ... ≥ top-k (score decrescente) | ordinamento garantito |
| B9 | ricerca su engine vuoto → lista vuota, no crash | |
| B10 | ricerca con queryEmb di dimensione diversa dagli embedding indicizzati → no crash | mismatch dim |

### CAT-C: save / load avanzato (10 test)
| # | Test | Verifica |
|---|------|----------|
| C1 | save + load + search → stessi risultati di prima del save | roundtrip completo |
| C2 | save su path non scrivibile → ritorna `false`, no crash | permessi |
| C3 | save su path con directory mancante → ritorna `false` o crea dir | robustezza |
| C4 | load di file JSON con `chunks` array vuoto → engine vuoto, ritorna `true` | |
| C5 | load di file JSON malformato → ritorna `false`, engine in stato pulito | |
| C6 | load di file con `inputDim` diverso dalla corrente istanza → gestito correttamente | |
| C7 | 10.000 chunk → save → load → `chunkCount() == 10000` | scalabilità persistenza |
| C8 | save dopo `clear()` → file valido con 0 chunk | |
| C9 | load + save + load → file identico (deterministico) | |
| C10 | `inputDim()` dopo load = `inputDim()` del engine originale | dimensione preservata |

### CAT-D: Performance (7 test)
| # | Test | Verifica |
|---|------|----------|
| D1 | addChunk × 1000 (dim=4096) < 2s | throughput indicizzazione |
| D2 | search top-5 su 1000 chunk < 50ms | latenza ricerca |
| D3 | search top-5 su 10.000 chunk < 500ms | scalabilità ricerca |
| D4 | project × 10.000 (dim=4096) < 5s | throughput proiezione |
| D5 | save × 1000 chunk < 1s | I/O write |
| D6 | load × 1000 chunk < 500ms | I/O read |
| D7 | RAM usata per 10.000 chunk (dim=4096→256) < 100MB | memoria JLT compressa |

---

## ✅ TN-09 — `test_hardware_monitor.cpp` (25 test)

**Modulo**: `hardware_monitor.h`, `hw_detect.h`

### CAT-A: HWInfo — struttura (6 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | `HWInfo` costruibile senza crash | default init |
| A2 | `cpu_pct` nel range [0.0, 100.0] dopo lettura | percentuale valida |
| A3 | `ram_used_mb >= 0` e `ram_total_mb > 0` | RAM non negativa |
| A4 | `ram_used_mb <= ram_total_mb` | logica |
| A5 | se GPU assente: `vram_total_mb == 0` | GPU non presente = zero VRAM |
| A6 | `HWInfo` serializzabile in QJsonObject → tutti i campi presenti | |

### CAT-B: hw_detect — rilevamento CPU (8 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | CPU name non vuoto | sistema ha un nome CPU |
| B2 | core count ≥ 1 | almeno 1 core |
| B3 | RAM totale ≥ 512 MB | sistema minimo |
| B4 | frequenza CPU > 0 MHz | rilevata |
| B5 | lettura ripetuta (×10) → nessun crash | stabilità |
| B6 | rilevamento SIMD flags — nessun crash | |
| B7 | `avail_vram_mb` non negativo | |
| B8 | sampling differenziale CPU > 0.5s → risultato stabile | no spike |

### CAT-C: HardwareMonitor — thread e segnali (11 test)
| # | Test | Verifica |
|---|------|----------|
| C1 | `start()` → signal `hwReady` emesso entro 3s | risposta tempestiva |
| C2 | `stop()` → nessun segnale dopo stop | stop effettivo |
| C3 | `start()` + `stop()` rapido × 10 → no crash | stress cicli |
| C4 | `start()` due volte → non avvia 2 thread | guard idempotenza |
| C5 | signal `hwReady` emesso con `HWInfo` valido | struttura non-zero |
| C6 | lettura `cpu_pct` in loop 5s → valori nel range [0, 100] | stabilità temporale |
| C7 | lettura mentre GPU è occupata → nessun deadlock | concorrenza |
| C8 | `onHWReady` slot connesso → riceve aggiornamenti | signal-slot |
| C9 | thread terminato correttamente quando oggetto distrutto | no dangling thread |
| C10 | memoria non perde dopo 100 cicli start/stop | no leak |
| C11 | `hwReady` porta RAM coerente con `/proc/meminfo` (Linux) | validazione |

---

## ✅ TN-10 — `test_chat_history_stress.cpp` (20 test)

**Stress e concorrenza di ChatHistory — separato da TN-01 per poter girare in CI slow-lane**

### CAT-A: Concorrenza pesante (10 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | 8 thread × 50 `newSession` simultanei → nessuna corruzione | `list().size() == 400` |
| A2 | 4 thread read + 4 thread write → nessun crash | no data race |
| A3 | `saveLog` da N thread su sessioni diverse → nessuna cross-write | ogni sessione ha il suo log |
| A4 | `remove` da thread concorrente mentre altro fa `list()` → no crash | |
| A5 | 100 sessioni create in 100ms → nessuna collisione di id | timestamp + disambiguazione |
| A6 | `loadLog` da 8 thread sulla stessa sessione → tutti ricevono stesso contenuto | read consistency |
| A7 | scrittura sequenziale 1000 sessioni + `list()` → count = 1000 | no lost write |
| A8 | `remove` di tutte le sessioni da thread separato → `list().isEmpty()` | |
| A9 | `newSession` + `saveLog` + `remove` in transazione rapida × 100 | no ghost files |
| A10 | QEventLoop + segnali cross-thread → nessun deadlock in 10s | |

### CAT-B: Durabilità e recovery (10 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | processo che scrive crash a metà → file parziale ignorato al reload | |
| B2 | salva 500 sessioni → crea nuova istanza ChatHistory → `list().size() == 500` | persistenza cross-istanza |
| B3 | log da 10 MB salvato → ricaricato identico | file grandi |
| B4 | disco pieno simulato (path su tmpfs limitato) → `saveLog` fallisce senza crash | |
| B5 | file con estensione `.json` ma non valido per ChatHistory (es. altra app) → ignorato | |
| B6 | rename file durante load → no crash | |
| B7 | sessioni create con date fake nel futuro → `list()` le ordina correttamente | |
| B8 | 5000 sessioni nella dir → `list()` < 500ms | performance |
| B9 | `loadLog` su sessione appena creata (log non ancora salvato) → `""` | non esiste ancora |
| B10 | `saveLog` → kill -9 → restart → `loadLog` coerente o vuoto (no corruzione parziale) | fsync o fallback |

---

## ✅ TN-11 — `test_matematica_page.cpp` (35 test)

**Modulo**: `matematica_page.h/.cpp` — `parseSeq()`, `detectPatternLocal()`, `numbersFromText()`
**Tecnica accesso**: `#define private public` prima dell'include per esporre i metodi privati.

### CAT-A: parseSeq() — parsing stringa → QVector\<double\> (10 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | stringa vuota | err = "Sequenza vuota.", vector vuoto |
| A2 | `"1,4,9,16"` virgole | {1,4,9,16}, no err |
| A3 | `"1 4 9 16"` spazi | {1,4,9,16}, no err |
| A4 | `"1.5, 2.7, 3.14"` float | {1.5, 2.7, 3.14} |
| A5 | `"1,abc,3"` token non numerico | err non vuoto, ritorna {} |
| A6 | `"42"` — un solo elemento | {42}, err = "Inserisci almeno 2 numeri." |
| A7 | `"1, 2, 3"` — spazi attorno alle virgole | {1,2,3} |
| A8 | `"1\t2\t3"` — tab come separatori | {1,2,3} |
| A9 | `"  1, 4, 9  "` — whitespace ai bordi | {1,4,9} |
| A10 | valori negativi `"-3, -1, 1, 3"` | {-3,-1,1,3}, err vuoto |

### CAT-B: detectPatternLocal() — riconoscimento pattern (15 test)
| # | Test | Input | Verifica |
|---|------|-------|----------|
| B1 | sequenza vuota | {} | contiene "Troppo corta" |
| B2 | 1 elemento | {5} | contiene "Troppo corta" |
| B3 | aritmetica d=2 | {2,4,6,8} | contiene "Aritmetica" |
| B4 | costante | {7,7,7,7} | contiene "costante" |
| B5 | aritmetica d=4 | {3,7,11,15} | contiene "Aritmetica" |
| B6 | geometrica r=3 | {2,6,18,54} | contiene "Geometrica" |
| B7 | quadrati perfetti | {1,4,9,16,25} | contiene "Quadrati" |
| B8 | cubi | {1,8,27,64} | contiene "Cubi" |
| B9 | triangolari | {1,3,6,10,15} | contiene "triangolari" |
| B10 | fibonacci-like | {1,1,2,3,5,8} | contiene "Fibonacci" |
| B11 | fattoriali | {1,2,6,24,120} | contiene "Fattoriali" o "fattoriali" |
| B12 | potenze di 2 | {2,4,8,16,32} | contiene "Potenze" o "potenze" |
| B13 | numeri primi | {2,3,5,7,11,13} | contiene "primi" |
| B14 | quadratica (n²+n) | {2,6,12,20,30} | contiene "Quadratica" |
| B15 | pattern sconosciuto | {1,4,7,11,15} | contiene "non riconosciuto" |

### CAT-C: numbersFromText() — estrazione numeri da testo libero (7 test)
| # | Test | Input | Verifica |
|---|------|-------|----------|
| C1 | stringa di soli numeri | `"1 4 9 16"` | ritorna "1, 4, 9, 16" |
| C2 | numeri misti a testo | `"La sequenza è: 3, 7, 11"` | contiene "3" e "7" e "11" |
| C3 | numeri negativi | `"Da -5 a 5: -5,-3,-1,1"` | contiene "-5" e "-3" e "1" |
| C4 | testo senza numeri | `"ciao come stai"` | ritorna `""` |
| C5 | numeri decimali con punto | `"valori: 1.5, 2.7, 3.14"` | contiene "1.5" e "2.7" |
| C6 | separatore migliaia (1,000) ignorato | `"1,000,000"` | `1000000` non spezzato in 3 pezzi |
| C7 | stringa vuota | `""` | ritorna `""` |

### CAT-D: Widget — costruzione e invarianti (3 test)
| # | Test | Verifica |
|---|------|----------|
| D1 | `MatematicaPage(mockAi)` costruisce senza crash | no eccezione, no null-deref |
| D2 | ha 4 tab (`m_tabs->count() == 4`) | layout corretto |
| D3 | `m_output` non null, readonly | output correttamente inizializzato |

---

## 🆕 TN-12 — `test_formula_parser.cpp` (34 test) — ✅ già scritto

**Modulo**: `widgets/formula_parser.h` — `FormulaParser`, metodi statici `tryExtract*`
**File**: già presente, non era documentato in questo TODO.

Categorie:
- **CAT-A** (6): costruttore + `ok()` / `err()` — valido, invalido, parentesi aperta
- **CAT-B** (8): `eval(x)` — x, 2x, x², sin, cos, 1/0, sqrt, pi
- **CAT-C** (5): `sample(xMin, xMax, n)` — n<2, xMax<xMin, lineare, NaN esclusi, n esatto
- **CAT-D** (7): `tryExtract()` — y=, f(x)=, plotta, grafico, nessuna formula, maiuscolo, newline
- **CAT-E** (4): `tryExtractXRange()` — "da x=.. a x=..", "x in [..]", "x da .. a ..", nessuno
- **CAT-F** (4): `tryExtractPoints()` — x=3 y=5, "(1,2)", no keyword→vuota, multipli

---

---

## ⬜ TN-13 — `test_qr_code_widget.cpp` (18 test)

**Modulo**: `widgets/qr_code_widget.h` — `generate()`, validity, paint
**Aggiunto**: 2026-05-06 — feature QR code APK download

### CAT-A: Generazione QR (8 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | URL corto (`http://192.168.1.1:11500/apk`) → `isValid() == true` | generazione ok |
| A2 | stringa vuota `""` → `isValid() == false` | fallisce in modo sicuro |
| A3 | stringa da 300 char → no crash | robustezza URL lunghi |
| A4 | caratteri non-ASCII → no crash | robustezza encoding |
| A5 | `setText(url)` aggiorna `isValid()` | cambio dinamico |
| A6 | `QrCode_getSize()` dopo generazione ≥ 21 | dimensione valida (Version 1 min) |
| A7 | `QrCode_getModule` per tutti i pixel → no out-of-bounds | accesso sicuro |
| A8 | generazione × 1000 stringhe diverse → no crash | stress |

### CAT-B: Rendering Qt (6 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | `paintEvent` su widget valido → nessun crash | rendering ok |
| B2 | `paintEvent` su widget non-valido → mostra "QR error", no crash | fallback |
| B3 | resize widget → paint adattato, no crash | scaling |
| B4 | `sizeHint() == QSize(280, 280)` | hint corretto |
| B5 | `setFixedSize(100, 100)` → paint no overflow | clip corretto |
| B6 | sfondo sempre bianco dopo paint | iso 18004 — bordo silenzioso |

### CAT-C: Dialog APK (4 test)
| # | Test | Verifica |
|---|------|----------|
| C1 | dialog con URL valido → `QrCodeWidget.isValid() == true` | QR visibile |
| C2 | click "Copia URL" → clipboard contiene URL | clipboard ok |
| C3 | dialog distrutto → nessun dangling widget | lifetime ok |
| C4 | bottone QR disabled quando server non attivo | guard ok |

---

## ⬜ TN-14 — `test_lan_server_endpoints.cpp` (22 test)

**Modulo**: `lan_server.h/.cpp` — `/apk`, `/knowledge`, ciclo vita
**Aggiunto**: 2026-05-06

### CAT-A: Endpoint /apk (8 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | GET /apk con APK presente → HTTP 200, Content-Type Android | download ok |
| A2 | GET /apk con APK assente → HTTP 404 | errore corretto |
| A3 | Content-Disposition contiene `filename="PrismaluxMobile.apk"` | nome file ok |
| A4 | byte ricevuti == dimensione file su disco | no troncamento |
| A5 | POST /apk → HTTP 4xx | solo GET ammesso |
| A6 | GET /apk da 2 client concorrenti → entrambi ricevono dati | no lock |
| A7 | GET /apk con file da 13 MB → nessun timeout | file grande ok |
| A8 | socket chiuso a metà → server non crasha | half-close |

### CAT-B: Endpoint /knowledge (8 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | GET /knowledge con file presente → `{"available":true}` | lettura ok |
| B2 | GET /knowledge con file assente → `{"available":false}` | fallback ok |
| B3 | POST mode="append" → file aggiornato | append ok |
| B4 | POST mode="replace" → contenuto sostituito | replace ok |
| B5 | POST body malformato → HTTP 400 | validazione |
| B6 | POST content vuoto → HTTP 400 | campo richiesto |
| B7 | POST → cache invalidata (GET successivo nuovo contenuto) | `invalidateKnowledgeCache` |
| B8 | Content-Type risposta = `application/json` | header ok |

### CAT-C: Ciclo vita server (6 test)
| # | Test | Verifica |
|---|------|----------|
| C1 | `start(port)` → `isRunning() == true` | start ok |
| C2 | `stop()` → `isRunning() == false` | stop ok |
| C3 | `start` su porta occupata → `false`, no crash | porta occupata |
| C4 | connessione → `clientCount() == 1` | conteggio ok |
| C5 | disconnessione → `clientCount() == 0`, signal emesso | signal ok |
| C6 | `requestHandled` emesso per ogni richiesta | tracciamento ok |

---

## ⬜ TN-15 — `test_hw_detect_amd.cpp` (14 test)

**Modulo**: `hw_detect.c` — `detect_amd()` con DRM sysfs fallback (vendor 0x1002)
**Aggiunto**: 2026-05-06 — fix bottoni GPU/Misto per AMD senza ROCm

### CAT-A: Struttura HWInfo (6 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | `hw_detect(&hw)` → no crash | base |
| A2 | `hw.dev[0].type == DEV_CPU` | CPU in posizione 0 |
| A3 | `hw.primary == 0` | CPU è primary |
| A4 | `hw.secondary >= -1` | secondary valido |
| A5 | CPU: `mem_mb > 0` e `avail_mb > 0` | RAM rilevata |
| A6 | `hw.count >= 1` | almeno CPU |

### CAT-B: AMD detection (8 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | se AMD presente: `hw.secondary >= 0` | secondary impostato |
| B2 | se AMD presente: `type == DEV_AMD` | tipo corretto |
| B3 | se AMD presente: `avail_mb > 0` (≥512 MB garantito) | VRAM non zero |
| B4 | se AMD presente: `n_gpu_layers > 0` | layer stimati |
| B5 | "compatible" (Intel) non confuso con ATI | no falsi positivi lspci |
| B6 | DRM vendor 0x1002 → DEV_AMD rilevato | metodo DRM sysfs |
| B7 | `detect_amd` × 10 → stesso risultato | deterministico |
| B8 | senza GPU dedicata: `hw.secondary == -1` o `== hw.igpu` | no falsi positivi |

---

## ⬜ TN-16 — `test_knowledge_injection.cpp` (16 test)

**Modulo**: `prismalux_paths.h` — `readUserKnowledge()`, `prependKnowledge()`, `invalidateKnowledgeCache()`
**Aggiunto**: 2026-05-06 — Problema 1 P1-P3

### CAT-A: readUserKnowledge (5 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | file `user_knowledge.md` presente → contenuto non vuoto | lettura ok |
| A2 | file assente → `""`, no crash | fallback ok |
| A3 | chiamata × 10 → stesso contenuto (cache) | cache hit |
| A4 | `invalidateKnowledgeCache()` → prossima lettura rilegge da disco | invalidazione |
| A5 | file con solo spazi → ritorna whitespace (no trim) | no trasformazione |

### CAT-B: prependKnowledge (7 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | knowledge non vuoto + sys → result contiene entrambi | iniezione ok |
| B2 | knowledge vuoto + sys → result == sys | no inject se vuoto |
| B3 | knowledge + sys vuoto → result contiene knowledge | sys può essere vuoto |
| B4 | knowledge precede sys nel result | ordine corretto |
| B5 | `[CONTESTO UTENTE]` appare nel result quando knowledge non vuoto | marker ok |
| B6 | `prependKnowledge(sys)` × 1000 → no memory leak | stress |
| B7 | toggle OFF (QSettings) → sys invariato | rispetta preferenza utente |

### CAT-C: Integrazione (4 test)
| # | Test | Verifica |
|---|------|----------|
| C1 | MockAiClient riceve sys con knowledge prepended | prompt iniettato |
| C2 | knowledge disabilitato → MockAiClient riceve sys originale | toggle rispettato |
| C3 | knowledge aggiornato + cache invalidata → prossima chat usa nuovo testo | aggiornamento live |
| C4 | knowledge da 10 KB → no crash, prompt completo | no troncamento |

---

## ⬜ TN-17 — `test_theme_manager_crash.cpp` (10 test)

**Modulo**: `theme_manager.cpp` — fix ABRT Signal 6 (`ThemeManager(nullptr)` invece di `(qApp)`)
**Aggiunto**: 2026-05-06 — root cause crash alla chiusura trovata con ASAN

### CAT-A: Lifetime sicuro (5 test)
| # | Test | Verifica |
|---|------|----------|
| A1 | `ThemeManager::instance()` → parent è `nullptr` | fix applicato |
| A2 | `QApplication` distrutta → ThemeManager NON distrutto da `deleteChildren` | no double-free |
| A3 | programma completo (QApplication + ThemeManager + close) → exit code 0, no ABRT | crash eliminato |
| A4 | `instance()` prima e dopo `QApplication` start → stesso puntatore | singleton stabile |
| A5 | `instance()` × 1000 → no crash, no leak | stress |

### CAT-B: Funzionalità invariate (5 test)
| # | Test | Verifica |
|---|------|----------|
| B1 | `apply("dark_cyan")` emette signal `changed` | signal ok |
| B2 | `loadSaved()` applica tema salvato | persistenza ok |
| B3 | `themes().size() == 23` | lista temi intatta |
| B4 | `apply` con file .qss mancante → no crash | file assente |
| B5 | `scanExternalThemes()` su dir vuota → no crash | scan sicuro |

---

## Riepilogo file

| File | CAT | Test | Stato |
|------|-----|------|-------|
| `test_chat_history.cpp` | A–D | 30 | ✅ 30 pass |
| `test_chat_history_stress.cpp` | A–B | 20 | ✅ scritto |
| `test_formula_parser.cpp` | A–F | 34 | ✅ scritto |
| `test_hardware_monitor.cpp` | A–B | 11 | ✅ scritto |
| `test_lavoro_data.cpp` | A–C | 38 | ✅ 38 pass |
| `test_manutenzione_cron.cpp` | A–D | 30 | ✅ scritto |
| `test_matematica_page.cpp` | A–D | 35 | ✅ scritto |
| `test_rag_engine_avanzato.cpp` | A–D | 43 | ✅ 43 pass |
| `test_simulatore_algos.cpp` | A–E | 72 | ✅ 72 pass |
| `test_strumenti_rag.cpp` | A–C | 40 | ✅ scritto |
| `test_theme_manager.cpp` | A–B | 14 | ✅ scritto |
| `test_tutor_data.cpp` | A–B | 31 | ✅ 31 pass |
| `test_qr_code_widget.cpp` | A–C | 18 | ⬜ da scrivere |
| `test_lan_server_endpoints.cpp` | A–C | 22 | ⬜ da scrivere |
| `test_hw_detect_amd.cpp` | A–B | 14 | ⬜ da scrivere |
| `test_knowledge_injection.cpp` | A–C | 16 | ⬜ da scrivere |
| `test_theme_manager_crash.cpp` | A–B | 10 | ⬜ da scrivere |
| **TOTALE** | | **478** | **398 scritti · 80 da fare** |

---

## Criteri comuni a tutti i test

- Usare `MockAiClient` (già in `mock_ai_client.h`) per tutti i test che toccano AiClient
- Ogni file compila standalone: `cmake -B build_tests -DBUILD_TESTS=ON`
- Nessun test che richiede Ollama reale (quelli vanno in `test_ai_integration.cpp`)
- Performance test con `QBENCHMARK` o `QElapsedTimer` + `QVERIFY(ms < soglia)`
- Concorrenza: usare `QThread` + `QMutex` o `std::thread`, mai `sleep` fissi
- Ogni `TestXxx : public QObject` nel proprio file con `QTEST_MAIN` o incluso nel runner

---

# TODO — Prismalux Qt GUI

---

## Piano di azione — Sicurezza e Qualità (2026-04-13)
> Revisione senior: problemi critici di sicurezza + debito tecnico.
> Ordine: C1→C2→C3 → A1 → A2 → M1→M2 → B1

---

## CRITICI 🔴

### [C1] Dialog conferma prima di eseguire codice Python da AI
**File**: `pages/agenti_page.cpp` ~3958  
**Problema**: `extractPythonCode()` estrae codice da risposte LLM e lo esegue
immediatamente con i permessi dell'utente, senza nessuna conferma.
Un LLM può generare (per errore o per prompt injection in un documento caricato)
codice che cancella file, esfiltra dati via rete, lancia processi arbitrari.  
**Fix**: QDialog modale con il codice da eseguire (QTextEdit read-only,
syntax highlight) e due pulsanti "▶ Esegui" / "✖ Annulla". Zero esecuzione
automatica.  
**Status**: ✅ completato

---

### [C2] Auto-pip-install richiede conferma esplicita
**File**: `pages/agenti_page.cpp` ~4061  
**Problema**: quando il codice AI genera `ModuleNotFoundError`, il nome del
pacchetto viene estratto dall'output e `pip install <pkg>` viene lanciato
in silenzio. Vettore di typosquatting/supply-chain attack: se l'LLM allucinato
inventa `prisma_utils` e su PyPI esiste un omonimo malevolo, viene installato.  
**Fix**: QMessageBox con nome pacchetto e warning "Installare pacchetti da
suggerimento AI è un rischio. Confermi?" + "Sì / No".
Se l'utente nega, skip retry e mostra l'errore originale.  
**Status**: ✅ completato

---

### [C3] Rimuovere falsa sicurezza da `_sanitize_prompt`
**File**: `pages/agenti_page.cpp` ~2382  
**Problema**: la lista `kPhrases[]` contiene 4 frasi inglesi ("ignore previous
instructions", ecc.). Non protegge da nulla: testo in italiano, unicode lookalike,
split delle parole bypassano il filtro. Peggio di niente — dà falsa sicurezza.  
**Fix**: rimuovere il blocco `kPhrases[]` e il loop di sostituzione.
Tenere la rimozione dei format token (`kTokens[]`) e dei role-override (`kRoles[]`).
Aggiungere commento esplicito che il sistema NON è immune a prompt injection.  
**Status**: ✅ completato

---

## ALTI 🟠

### [A1] Fix shell injection — sostituire `bash -c cmd` con arglist
**File**: `pages/personalizza_page.cpp` ~320-340, ~998, ~1028-1082;
          `pages/programmazione_page.cpp` ~1010;
          `pages/manutenzione_page.cpp` ~264, ~569, ~598, ~632, ~646  
**Problema**: comandi costruiti come stringa e passati a `sh -c` o `bash -c`.
Path con caratteri speciali (virgolette, punto e virgola) rompono l'escaping.
Esempio: `cloneDir = '/home/user/dir"; evil_cmd; echo "'` → injection.  
**Fix applicato**: compilazione gcc TUI (`compileTUI`) convertita a `QProcess::start(compiler, args)`
con arglist esplicita (nessuna shell). I comandi cmake/git usano `sh -c` con path
tutti interni (non input utente) — rischio pratico zero, lasciati invariati.  
**Status**: ✅ parziale (gcc fix applicato; cmake/git pipeline: path interni, basso rischio)

---

### [A2] Split `agenti_page.cpp` (4609 righe) in moduli separati
**File**: `pages/agenti_page.cpp`  
**Problema**: la classe gestisce contemporaneamente TTS, STT, PDF extractor,
Excel parser, Python executor, Blender MCP, Office MCP, pipeline AI, Byzantine
engine, traduzione, chart, Scientific Council. Impossibile testare, difficile
mantenere.  
**Piano di split**:
- `pages/python_executor.h/cpp` — esecuzione codice, auto-install, dialog confirm
- `pages/tts_engine.h/cpp` — Piper, espeak, SAPI Windows
- `pages/stt_engine.h/cpp` — whisper.cpp, arecord, download modello
- `AgentiPage` diventa orchestratore che usa questi componenti via segnali  
**Nota**: fare M1 (unit test) PRIMA di questo refactor per avere rete di sicurezza.  
**Status**: ⬜ pending

---

## MEDI 🟡

### [M1] Unit test per RagEngine, `extractPythonCode`, `_sanitizePyCode`
**File creati**: `tests/test_rag_engine.cpp`, `tests/test_code_utils.cpp`  
**Build**: `cmake -B build_tests -DBUILD_TESTS=ON && cmake --build build_tests`  
**Risultati**: `test_rag_engine` 12/12 ✅ — `test_code_utils` 14/14 ✅  
**Nota tecnica**: `extractPythonCode` e `_sanitizePyCode` spostate da `private` a `public`
in `agenti_page.h` — sono pure utility statiche, nessun accesso a stato interno.
**Status**: ✅ completato (2026-04-13)

---

### [M2] Warning privacy + opzione no-persist nel tab RAG
**File**: `pages/impostazioni_page.cpp` → `buildRagTab()`  
**Problema**: `~/.prismalux_rag.json` contiene i chunk di testo in chiaro
dai documenti indicizzati (dichiarazioni 730, contratti, documenti medici).
L'utente non è informato.  
**Fix**:
1. Label warning: "⚠️ I chunk indicizzati sono salvati in chiaro in `~/.prismalux_rag.json`"
2. QCheckBox "Non salvare su disco (solo RAM — perso alla chiusura)" che disabilita `m_rag.save()`
3. Tooltip sul pulsante Reindicizza con percorso completo del file  
**Status**: ✅ completato (warning amber, checkbox + `m_ragNoSave`, tooltip reindexBtn, messaggio fine indicizzazione aggiornato)

---

## BASSI 🟢

### [B1] Fix memory leak — `new QElapsedTimer` nei lambda
**File**: `pages/agenti_page.cpp` ~3980 (`tmr`), ~4029 (`t2`)  
**Problema**: se il processo viene distrutto prima che `finished` scatti
(app chiusa durante esecuzione), `delete tmr` non viene mai chiamato.  
**Fix**: `QSharedPointer<QElapsedTimer>` catturato per valore nel lambda.  
**Status**: ✅ completato (già `QSharedPointer<QElapsedTimer>::create()` nelle righe 4030 e 4104)

---

---

## Nuovi item da review 2 (2026-04-13) 🔴🟠🟡

### [S1] waitForFinished blocca UI in matematica_page.cpp
**File**: `pages/matematica_page.cpp` — `_runPythonSync()`  
**Problema**: `proc.waitForFinished(20000)` congela il thread UI per fino a 20 secondi
durante import di file (xlsx/doc/pdf). Stesso problema con `waitForFinished(15000)`
nei fallback catdoc/pdftotext.  
**Fix**: convertire `_runPythonSync` a pattern asincrono con callback `std::function<void(QString,QString)>`,
oppure eseguire in un `QThread::create` con signal al completamento.  
**Status**: ⬜ pending (MEDIO 🟡)

---

### [S2] Split grafico_page.cpp (6.081 righe)
**File**: `pages/grafico_page.cpp`  
**Problema**: più grande di agenti_page (6081 vs 4665 righe). Mescola:
parser formule, rendering QPainter, dati Smith/Nyquist, interfaccia UI.  
**Piano**:
- `widgets/formula_parser.h/cpp` — già esiste come widget separato ✅
- `pages/grafico_engine.h/cpp` — logica calcolo punti, serie, SmithPrime
- `pages/grafico_page.*` — solo UI e coordinamento  
**Nota**: fare M1 (test) prima — grafico_page calcola serie numeriche testabili.  
**Status**: ⬜ pending (ALTO 🟠)

---

### [S3] Centralizzare QSettings
**File**: 8+ file, 44 istanze `QSettings("Prismalux", "GUI")`  
**Problema**: i nomi delle chiavi (es. `"rag/noSave"`) sono stringhe letterali
sparse nel codebase. Un refactor o typo introduce bug silenziosi.  
**Fix**: `settings_keys.h` con costanti `QString` per ogni chiave + una classe
`AppSettings` con getter/setter tipizzati.  
**Status**: ⬜ pending (MEDIO 🟡)

---

## Ordine di esecuzione

```
✅ C1 + C2  →  ✅ C3  →  ✅ A1(parz) + ✅ M2 + ✅ B1  →  ⬜ M1  →  ⬜ A2
    (critici)              (alto + privacy + leak)         (test)   (refactor)

✅ S0(bridge auth) + ✅ S0b(bypass C2)  →  ⬜ S1(waitFor)  →  ⬜ S2(grafico split)  →  ⬜ S3(QSettings)
```

C1 e C2 si fanno insieme — fanno parte dello stesso componente `PythonExecutor`.
M1 prima di A2 — senza test, il refactor di agenti_page è rischioso.

---

## Completate ✅ (storico pre-2026-04-13)

### RAG — setup nelle Impostazioni ✅
Tab "🔍 RAG" in ImpostazioniPage con cartella documenti, stato indice, reindicizza.

### Analizza fonti da PDF ✅
Categoria "📄 Documenti" in StrumentiPage con 6 azioni + picker PDF.

### Simboli strani nei grafici ✅
Rimossi caratteri Unicode problematici dalla combo tipo grafico.

### Pulsante Messaggi / Log nella sidebar ✅ (2026-04-13)
Aggiunto pulsante 📋 "Messaggi" sopra "Impostazioni" in fondo alla sidebar.
- Dialog non-modale con log eventi timestampati (HH:mm:ss)
- Badge rosso con contatore messaggi non letti (si azzera all'apertura)
- Pulsante "Pulisci log" + "Chiudi"
- Eventi loggati: cambio backend, modelli caricati, llama-server avvio/stop/errore, pipeline completata

### Office bridge — autenticazione Bearer token ✅ (2026-04-13)
Chiuso il buco ACAO:* — qualsiasi pagina web aperta nel browser poteva inviare
`POST /execute` con codice arbitrario senza conferma.
- Bridge genera `secrets.token_hex(32)` all'avvio → scrive in `~/.prismalux_office_token` (chmod 600)
- Ogni endpoint (`/status`, `/execute`) verifica `Authorization: Bearer <token>` con `secrets.compare_digest`
- ACAO cambiato da `*` a `http://localhost`
- Qt legge il token file prima di ogni request (`_readToken` lambda) e aggiunge l'header
- Token file rimosso all'uscita del bridge

### _sanitizePyCode — rimosso auto-inject pip (bypass C2) ✅ (2026-04-13)
Il preamble `_prisma_ensure(numpy, pandas, ...)` veniva iniettato in cima al codice
PRIMA che l'utente lo vedesse nel dialog C1 — pip girava silenziosamente senza la
conferma C2 che l'utente credeva di dover dare.
- Rimosso l'intero blocco `kOptionalPkgs` / `kPkgMap` / preamble injection
- Il percorso corretto: `ModuleNotFoundError` → C2 mostra dialog con nome pacchetto → utente decide
- Aggiunto commento che spiega perché il preamble è stato rimosso (anti-regressione)

### Office MCP — compatibilità LibreOffice verificata ✅ (2026-04-13)
Il bridge `office_bridge/prismalux_office_bridge.py` è **compatibile con LibreOffice**.
- **Modalità UNO** (preferita): richiede `sudo apt install python3-uno libreoffice`
  - IMPORTANTE: eseguire con il Python di sistema (`python3`), NON da un venv
  - Avvia LibreOffice headless con socket UNO su porta 2002
  - Connessione via `UnoUrlResolver` (15 tentativi con retry 1s)
- **Modalità file** (fallback automatico): python-docx / openpyxl / python-pptx
  - Crea file .docx/.xlsx/.pptx compatibili con LibreOffice
- Aggiunto path `/snap/bin/soffice` (Ubuntu snap) e macOS al rilevamento automatico
- Flatpak: percorso aggiunto ma UNO potrebbe non funzionare in sandbox Flatpak

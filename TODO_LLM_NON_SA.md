# TODO — Strategia "LLM non sa la risposta"

**Priorità:** Alta — impatta ogni interazione in cui il modello è ignorante o fuori data  
**File principali:** `gui/pages/main_ai_pipeline.cpp`, `gui/pages/main_ai_slots.cpp`, `gui/pages/main_ai_stream.cpp`

---

## Problema

Quando l'LLM non conosce la risposta, il comportamento attuale è:
- Due banner separati e ridondanti (check `kUncertainPhrases` + regex `reNonSo`) che possono apparire insieme
- La ricerca web post-incertezza richiede un click manuale dell'utente
- Dopo la ricerca, l'LLM NON viene ri-interrogato automaticamente con i risultati
- Falsi positivi: frasi come "non so se preferisci X" scattano il banner inutilmente

---

## Task

### ✅ TASK-1 — Unificare i due check di incertezza *(completato 2026-06-21)*
**File:** `main_ai_pipeline.cpp` righe 382–414 e 966–1080  
**Cosa fare:**
- Eliminare il check con `kUncertainPhrases` (riga 382) — è più debole e ridondante
- Mantenere solo la regex `reNonSo` (riga 966) come unico punto di rilevamento
- Aggiungere alla regex una guardia per escludere falsi positivi:
  pattern come `"non so se"`, `"non so quale"`, `"non so cosa preferisci"` → NON sono incertezza dichiarata
  ```cpp
  // Guardia falso positivo: "non so se/quale/cosa/come preferisci..."
  static const QRegularExpression reFalsoPositivo(
      R"(non\s+so\s+(?:se|quale|cosa|come|quando|dove|chi)\b)",
      QRegularExpression::CaseInsensitiveOption);
  if (reFalsoPositivo.match(rawResp).hasMatch()) uncertain = false;
  ```

---

### ✅ TASK-2 — Auto-websearch quando l'LLM esprime incertezza *(completato 2026-06-21)*
**File:** `main_ai_pipeline.cpp` (in `_finishedPipeline()`) + `main_ai_slots.cpp`  
**Cosa fare:**
- Quando `reNonSo` scatta, invece di mostrare il link "Cerca online →", lanciare automaticamente `runWebSearchAgent(m_taskOriginal)` in background
- Mostrare un indicatore visivo durante la ricerca: `"🔍 Il modello non sa — cerco online..."`
- Usare un flag `m_autoWebSearchPending` per evitare loop se la ri-risposta è ancora incerta

**Flusso:**
```
reNonSo scatta
  → inserisce riga "🔍 Cerco online automaticamente..."
  → lancia runWebSearchAgent(m_taskOriginal)
  → se trovato → TASK-3
  → se NORESULT → mostra banner inserimento manuale (già esistente)
```

**Attenzione:** `m_ai` potrebbe essere busy. Usare un AiClient dal pool se disponibile,
altrimenti accodare e lanciare quando `finished()` è emesso.

---

### ✅ TASK-3 — Re-query LLM con contesto web dopo auto-websearch *(completato 2026-06-21)*
**File:** `main_ai_stream.cpp` (in `runWebSearchAgent()`, callback `runToolCall`)  
**Cosa fare:**
- Aggiungere un parametro `bool isAutoRetry = false` a `runWebSearchAgent()`
- Se `isAutoRetry == true` e i risultati sono validi:
  - NON mostrare la bolla "Cerco online: **query**..." (già mostrata prima)
  - Costruire system prompt con i risultati come contesto
  - Chiamare `m_ai->chat(sys, userWithContext)` per ottenere una risposta aggiornata
  - Mostrare la risposta come bolla AI normale con header `"🌐 (risposta aggiornata con ricerca online)"`
- Se `isAutoRetry == false` (percorso manuale già esistente): comportamento invariato

---

### ✅ TASK-4 — Check RAG proattivo prima della chiamata LLM *(completato 2026-06-21)*
**File:** `main_ai_pipeline.cpp` (in `runPipeline()`, prima di `advancePipeline()`)  
**Cosa fare:**
- Prima di chiamare l'LLM, fare `m_ragEngine->search(task, 3)` se il RAG è disponibile
- Se restituisce chunks con score > soglia (es. 0.6): iniettare automaticamente nel task
  come `"## Contesto dai tuoi documenti:\n" + chunks`
- Se non trova nulla: procedere normalmente (già si fa via injection globale, verificare che sia attivo)
- Questo riduce i casi in cui l'LLM dichiara incertezza su argomenti già presenti nel RAG dell'utente

**Nota:** verificare che `AiClient::chat` con RAG injection sia già attivo su tutti i path
(pipeline multi-agente, chat singola, agente autonomo). Correggere eventuali path che lo saltano.

---

### ✅ TASK-5 — Guardia anti-loop e flag stato *(completato 2026-06-21)*
**File:** `main_ai.h` + `main_ai_pipeline.cpp`  
**Cosa fare:**
- Aggiungere membro `bool m_autoRetryActive = false;`
- Prima di auto-lanciare websearch (TASK-2): `if (m_autoRetryActive) return;`
- Settare `m_autoRetryActive = true` prima del retry, `false` in `_finishedPipeline()` o `onAiAborted()`
- Questo impedisce che una ri-risposta ancora incerta scateni un secondo retry infinito

---

## Ordine di implementazione consigliato

```
TASK-5 (flag) → TASK-1 (unifica check) → TASK-2 (auto-websearch) → TASK-3 (re-query) → TASK-4 (RAG proattivo)
```

TASK-5 e TASK-1 sono prerequisiti degli altri — vanno fatti prima.
TASK-4 è indipendente e può essere fatto in parallelo a TASK-2/3.

---

## Test da scrivere dopo l'implementazione

- `test_agenti_pipeline`: aggiungere CAT-B per `reNonSo` regex (falsi positivi, veri positivi)
- `test_agenti_pipeline`: verificare che `m_autoRetryActive` impedisca il loop
- Smoke test manuale: domanda "Chi ha vinto le elezioni ieri?" → deve auto-cercare online

---

*Creato: 2026-06-21 | Versione: Prismalux v3.0*

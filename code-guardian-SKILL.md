---
name: code-guardian
description: >
  Analizzatore, correttore ed esecutore di codice con verifica automatica pre-esecuzione.
  USA QUESTO SKILL ogni volta che l'utente vuole analizzare, pulire, correggere o eseguire
  file di codice (Python, Bash, C, YAML, JSON, TOML). Attivalo anche se l'utente dice
  "controlla il file", "analizza", "esegui questo", "c'è qualcosa di sbagliato?",
  "puoi fixare?", "esegui e dimmi cosa succede", o qualsiasi variante.
  Questo skill evita che errori stilistici banali (trailing whitespace, indentazione,
  import sbagliati) sfuggano prima dell'esecuzione, e cattura output ed errori runtime.
---

# Code Guardian

Skill per analisi, auto-fix ed esecuzione sicura di codice. Opera in tre fasi sequenziali:
**Analisi → Fix automatici → Esecuzione con report**.

---

## Linguaggi supportati

| Linguaggio | Estensioni | Strumenti di analisi |
|---|---|---|
| Python | `.py` | `py_compile`, `pylint` / `flake8` (se disponibili), AST |
| Bash/Shell | `.sh`, `.bash` | `bash -n` (dry-run syntax check) |
| C | `.c`, `.h` | `gcc -fsyntax-only` |
| YAML | `.yaml`, `.yml` | `python -c "import yaml"` |
| JSON | `.json` | `python -m json.tool` |
| TOML | `.toml` | `python -c "import tomllib"` / `tomli` |

---

## Flusso operativo

### FASE 1 — Analisi pre-fix

Prima di toccare qualsiasi file, esegui una scansione completa e cataloga i problemi trovati in queste categorie:

**AUTO-FIX (applicare subito, senza chiedere):**
- Trailing whitespace (spazi/tab a fine riga)
- Blank lines con whitespace invisibile
- Newline mancante a fine file
- Indentazione inconsistente — SOLO se il pattern è ovvio e uniforme (es. tutto il file usa 2 spazi ma dovrebbe usare 4, o mix tab/spazi chiaramente accidentale)
- Import e `from` errati/inutilizzati — solo se rilevabili staticamente con certezza (es. `import os` non usato, `from x import *` con x inesistente)
- Fix stilistici puri che non alterano la logica

**RICHIEDE CONFERMA (mostrare diff e chiedere prima):**
- Errori di sintassi che richiedono modifica alla logica
- Indentazione ambigua (potrebbe cambiare il comportamento)
- Import rimossi che potrebbero avere side effects
- Qualsiasi modifica che Claude non è 100% sicuro sia solo stilistica

**SOLO SEGNALAZIONE (non toccare, riferire nel report):**
- Warning logici / code smells
- Dipendenze mancanti
- Problemi di runtime non prevedibili staticamente

---

### FASE 2 — Applicazione fix

Per ogni fix automatico:
1. Mostra un **diff compatto** (formato `--- before / +++ after`) nel report finale
2. Applica la modifica al file
3. Ri-esegui la validazione sintattica sul file modificato per confermare che il fix non ha rotto nulla

Per i fix che richiedono conferma:
1. Mostra il problema specifico con contesto (numero di riga, codice circostante)
2. Proponi la soluzione con diff
3. Aspetta risposta prima di procedere
4. Dopo conferma, applica e continua

---

### FASE 3 — Esecuzione e cattura output

Dopo i fix (o se non ci sono fix da fare), esegui il codice:

**Python:**
```bash
python3 file.py [args]
```

**Bash:**
```bash
bash file.sh [args]
```

**C** — compila prima, poi esegui:
```bash
gcc -Wall -o /tmp/cg_output file.c && /tmp/cg_output [args]
```

**Config files (YAML/JSON/TOML)** — non eseguibili, solo validazione finale.

Cattura sempre:
- Exit code
- stdout completo
- stderr completo
- Tempo di esecuzione (usa `time` o `time.time()`)

---

### FASE 4 — Report finale

Dopo l'esecuzione, mostra sempre un report strutturato così:

```
═══════════════════════════════════════
  CODE GUARDIAN — REPORT
  File: nome_file.py
  Data: [timestamp]
═══════════════════════════════════════

📋 ANALISI
  Righe analizzate: N
  Problemi trovati: N

✅ FIX AUTOMATICI APPLICATI (N)
  • [riga X] Rimosso trailing whitespace (3 righe)
  • [riga Y] Corretto import inutilizzato: `import sys`
  • [diff compatto se rilevante]

⚠️  SEGNALAZIONI (nessuna azione)
  • [eventuali warning non critici]

🚀 ESECUZIONE
  Comando: python3 file.py
  Exit code: 0
  Tempo: 0.34s

📤 OUTPUT
  [stdout completo]

🔴 ERRORI (se presenti)
  [stderr completo]

═══════════════════════════════════════
  STATO FINALE: ✅ OK  /  ❌ ERRORI
═══════════════════════════════════════
```

---

## Comportamento per tipo di richiesta

| Richiesta utente | Azione |
|---|---|
| "analizza questo file" | Fase 1 + 2 + report (no esecuzione) |
| "esegui questo" | Fase 1 + 2 + 3 + report completo |
| "c'è qualcosa di sbagliato?" | Fase 1 + report solo analisi |
| "pulisci e basta" | Fase 1 + 2, no esecuzione |
| "esegui così com'è" | Fase 3 diretta (analisi leggera, no fix) |

---

## Regole fondamentali

1. **Mai modificare la logica** senza conferma esplicita
2. **Mai eseguire** codice con errori di sintassi non risolti
3. **Sempre mostrare il diff** anche per i fix automatici (nel report)
4. **Se il file non esiste** o non è leggibile, segnalarlo subito senza procedere
5. **Se mancano dipendenze** (import non installati), segnalarlo prima di eseguire
6. **Per i file C**, mai lasciare binari temporanei in giro — pulisci dopo l'esecuzione

---

## Note su import errati (Python)

Considera un import "sicuro da rimuovere" SOLO se:
- Il modulo importato non appare MAI nel resto del file (ricerca testuale)
- Non è un import con side effects noti (es. `import antigravity`, `import readline`)
- Non usa `__all__` o pattern dinamici (`getattr`, `importlib`)

In caso di dubbio, **segnala ma non toccare**.

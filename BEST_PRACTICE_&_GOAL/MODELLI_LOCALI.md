# Modelli LLM locali — guida per questa macchina

> Aggiornato: 2026-05-31
> Scopo: scegliere il modello Ollama giusto per assistenza al codice (C++/Qt6, Python),
> soprattutto per i fix di sicurezza elencati nel TODO.

## Hardware di riferimento (macchina di sviluppo Linux)

| Componente | Valore | Implicazione |
|------------|--------|--------------|
| CPU | Intel i7-7700HQ — 4 core / 8 thread (2017) | inference CPU possibile ma lenta |
| RAM | 7.6 GB totali, **~4.7 GB liberi** con KDE+browser | **vincolo principale** |
| GPU | NVIDIA GTX 1050 — **4 GB VRAM** (driver 580) | modelli ≤4B Q4 girano interi in GPU |
| GPU integrata | Intel HD 630 | non usata per LLM |
| OS | Kubuntu 26.04, Wayland | Ollama 0.24.0 vede la GPU |

**Regola pratica dimensioni (quantizzazione Q4):**
- **≤4B Q4 (~2-3 GB)** → entra tutto nei 4 GB di VRAM → **veloce**. Sweet spot di questa macchina.
- **7B Q4 (~4.5 GB)** → non entra in VRAM, offload parziale su CPU/RAM → lento e al limite della RAM.
- **≥13B** → non adatto: supera RAM+VRAM, swap, inutilizzabile.

---

## Modelli che HAI già — quale usare

Per **scrivere/correggere codice**, il migliore locale che hai è:

### ⭐ `deepseek-coder:6.7b` (3.8 GB)
Unico tra i tuoi addestrato apposta sul codice. Usalo così:
- **Task circoscritti**, una funzione alla volta, con il contesto già pronto
  (es. "ecco questa funzione, aggiungi la verifica del token così").
- **Non** chiedergli di progettare da solo soluzioni su più file (es. "sistema la sicurezza del WAN") → si perde.
- **Verifica sempre** il risultato ricompilando (`python3 build.py`) e con i test.

Secondo parere sul *ragionamento* (non sulla scrittura): `qwen3:4b` ha la modalità "thinking".

### Modelli che NON servono per il codice
- `moondream` → vision (descrive immagini)
- `nomic-embed-text` → embedding (serve al RAG, non genera testo)
- `gemma-1B-LinuxCLI`, `llama3.2:3b`, `phi3:3.8b`, `mistral:7b`, `qwen3.5:4b` → generalisti, ok per spiegazioni e snippet brevi, non affidabili su codice complesso

### Modelli `:cloud` (qwen3-next:80b, deepseek-v4-flash, gemini-3-flash)
Girano sul **cloud di Ollama / Google**, non su questa macchina. Molto capaci ma
**il tuo sorgente esce dal PC** → valutare la privacy prima di usarli.

---

## Modelli che NON hai ma puoi far girare qui (consigliati)

Tutti scaricabili con `ollama pull <nome>`. In ordine di consiglio per questa macchina:

### ⭐⭐ `qwen2.5-coder:3b` (~2 GB) — MIGLIOR SCELTA per questa macchina
- Coding-specialized, **entra intero nei 4 GB di VRAM** → veloce e fluido.
- Qualità sul codice superiore a tutti i generalisti 3-4B che hai.
- Ideale per i fix circoscritti del TODO senza rallentare il sistema.
```bash
ollama pull qwen2.5-coder:3b
```

### ⭐ `qwen2.5-coder:7b` (~4.7 GB) — migliore qualità, ma lento qui
- Oggi tra i migliori coding model open nella fascia 7B, batte `deepseek-coder:6.7b`.
- **Su questa macchina va in offload parziale** (non entra nei 4 GB VRAM) ed è al limite
  della RAM → usabile per task non urgenti, non per uso interattivo veloce.
```bash
ollama pull qwen2.5-coder:7b
```

### `qwen2.5-coder:1.5b` (~1 GB) — autocomplete / task semplici
- Velocissimo, per completamento e modifiche banali. Qualità limitata sul ragionamento.

### Alternative (stessa fascia, se vuoi confrontare)
| Modello | Taglia | Note |
|---------|--------|------|
| `codegemma:2b` / `codegemma:7b` | ~1.6 / ~5 GB | coding Google, buon completamento |
| `starcoder2:3b` | ~1.7 GB | completamento codice multi-linguaggio |
| `granite-code:3b` | ~2 GB | coding IBM, buono su istruzioni |

### Sconsigliati su questa macchina (troppo grandi)
- `deepseek-coder-v2:16b-lite` (MoE 16B, ~9 GB) → supera RAM+VRAM
- Qualsiasi modello ≥13B non-MoE

---

## Strategia consigliata per i fix di sicurezza del TODO

1. **Progettazione** del fix (capire il flusso su più file, dove mettere l'auth) →
   modello forte: Claude/Opus (già nel codice, niente esce dal PC) o un `:cloud` se accetti la privacy.
2. **Esecuzione meccanica** (un fix circoscritto alla volta, contesto fornito) →
   `qwen2.5-coder:3b` (se scaricato) o `deepseek-coder:6.7b`.
3. **Verifica** → `python3 build.py` + `ctest --test-dir build_tests` (suite `LanServer`, ecc.).

> Nota: un fix di sicurezza plausibile ma sbagliato è peggio di nessun fix.
> Non fidarsi a scatola chiusa dell'output di un modello locale ≤7B su codice critico.

---

## Perché i benchmark ingannano (benchmark vs realtà)

Un modello che "va forte nei benchmark" spesso delude nell'uso reale. Non è un caso: è
sistematico. Le cause principali:

1. **Contaminazione (leakage).** I benchmark famosi (HumanEval, MBPP, GSM8K, MMLU) sono
   pubblici e finiscono nei dati di addestramento → il modello li ha già *visti*, non li
   *ragiona*. Punteggio alto, comprensione bassa.
2. **Il benchmark non assomiglia al tuo task.** Un test di coding = "scrivi una funzione di
   10 righe da una docstring". Il lavoro reale = "capisci 3000 righe su più file e modifica
   senza rompere nulla". Abilità diverse: un 4B può eccellere nel primo e crollare nel secondo.
3. **"Teaching to the test".** I produttori ottimizzano per i benchmark perché sono marketing.
4. **La quantizzazione.** I benchmark girano in fp16; tu giri Q4 (compresso per stare in 4 GB
   VRAM). La compressione degrada la qualità, e i modelli piccoli ne soffrono di più.
5. **Generalista ≠ specialista del codice.** Un modello nuovo e generalista (es. `qwen3.5:4b`)
   può battere nei benchmark *generali* uno specialista del codice (`qwen2.5-coder`), ma sul
   **codice reale** lo specialista — anche più vecchio o piccolo — di solito vince.
   "Versione più alta" ≠ "migliore per il tuo task".

**Conclusione:** non scegliere dai benchmark. Prova il modello **sul tuo codice** e giudica da lì.

---

## Posso crearmi un modello "su misura" (es. solo matematica)?

Equivoco comune: *"rimuovo gli argomenti inutili → modello più piccolo e veloce"*. **Non
funziona così.** Un LLM non tiene la conoscenza in cassetti separati ("storia", "matematica")
che puoi togliere: i concetti sono distribuiti e intrecciati su miliardi di pesi, e la bravura
in matematica dipende anche da capacità linguistiche/di ragionamento imparate da dati non
matematici. Un modello "solo matematica" **non sarebbe più piccolo né più veloce** solo perché
risponde solo di matematica.

### Cosa si può fare davvero

| Tecnica | Riduce dimensione/velocizza? | Fattibile su questa macchina? |
|---------|------------------------------|-------------------------------|
| **Modelfile + system prompt** (`ollama create`) | No (stessi pesi) | ✅ Sì, gratis e immediato |
| **RAG** (dai i tuoi materiali come contesto) | No | ✅ Sì — Prismalux ce l'ha già |
| **Fine-tuning (QLoRA)** | No (anzi non riduce) | ⚠️ Molto difficile con 4 GB VRAM |
| **Quantizzazione** (Q4/Q3) | ✅ Sì | ✅ Già in uso |
| **Pruning / distillazione** | ✅ Sì | ❌ Ricerca avanzata, non in casa |

### La strada giusta per "un tutor di matematica per le medie" (o per l'università)

Non serve un modello diverso: serve **lo stesso modello con istruzioni diverse**. Esempio di
Modelfile Ollama:

```dockerfile
FROM qwen2.5-math:7b
SYSTEM """Sei un tutor di matematica per la scuola media.
Spiega passo passo, con parole semplici e un esempio concreto.
Non usare notazione universitaria. Se l'argomento è troppo avanzato, riportalo al livello scolastico."""
PARAMETER temperature 0.3
```
```bash
ollama create mate-medie -f Modelfile
ollama run mate-medie
```

Per la versione "università" cambi **solo il system prompt** (linguaggio formale, dimostrazioni,
notazione rigorosa). Stesso modello, due personalità.

- Questo **non** velocizza l'inference, ma **migliora la qualità delle risposte** nel dominio
  (è questo il senso utile di "ottimizzare").
- Per i **contenuti specifici** del programma scolastico → usa il **RAG** del progetto: carichi
  i libri/appunti e il modello risponde sui *tuoi* materiali, senza riaddestrare nulla.
- Per andare più **veloce**: l'unica leva reale è un **modello più piccolo** o una
  **quantizzazione più aggressiva**, non "togliere argomenti".

> Modelli matematici da provare (specialisti): `qwen2.5-math:1.5b` / `:7b`, `mathstral:7b`,
> `deepseek-math` — più adatti del generalista se l'uso è esclusivamente matematica.

---

## Progetti per comparare gli LLM

Lo stato cambia spesso → verificare i link aggiornati. Le leaderboard sono medie: il giudice
finale resta **provare il modello sul proprio task**.

| Progetto | Cosa misura | Utile per |
|----------|-------------|-----------|
| **LMArena** (ex Chatbot Arena) | voti umani testa-a-testa (ELO) | qualità generale percepita |
| **Aider LLM Leaderboard** | edita codice reale, % task passati | ⭐ coding (il più rilevante) |
| **Artificial Analysis** | qualità + velocità + prezzo, anche modelli aperti | scegliere il modello pratico |
| **LiveBench** | benchmark anti-contaminazione, rinnovato | punteggi meno gonfiati |
| **SWE-bench** | risolve issue reali di GitHub | coding "vero", molto difficile |
| **Hugging Face** | leaderboard tematiche varie | confronti per dominio |

Per uso coding + modelli locali, i due più sensati: **Aider** (coding reale) e
**Artificial Analysis** (include i modelli aperti, con velocità/costo oltre alla qualità).

> Il software è una leva enorme oltre alla scelta del modello: **quantizzazione** (modelli più
> grandi sullo stesso hardware), **RAG**, e **orchestrazione agentica** (più passaggi /
> auto-verifica / consenso) — un modello piccolo orchestrato bene può battere uno grande "nudo".
> Prismalux fa già questo (pipeline 6 agenti, Byzantine, RAG, GraphMemory).

---

## Modelli per la RICERCA (scientifica, matematica, metodologica)

> Caso d'uso: sviluppare e validare ricerca originale (es. BLHM, RAB0-L) — ragionare,
> criticare metodologia, fare matematica, progettare validazioni. Diverso dal coding puro.

**Verità onesta:** per il **ragionamento di ricerca di alto livello** nessun modello locale
≤7B (quindi nessuno girabile su 4 GB VRAM) è all'altezza. Trovare difetti metodologici o fare
matematica non banale richiede un modello forte. I locali piccoli sbagliano in modo
plausibile → in ricerca un ragionamento errato costa settimane.

### Cosa usare per ciascun pezzo

| Compito di ricerca | Modello giusto |
|--------------------|----------------|
| Ragionare, criticare metodologia, matematica avanzata | Modello **frontier** (Claude/Opus) o `:cloud` — non un locale piccolo |
| Scrivere codice di validazione (Python, numpy/scipy, bioinfo) | `qwen2.5-coder:3b` locale |
| Conti/derivazioni matematiche | `qwen2.5-math` o un reasoning model |
| Ragionamento locale (il meglio possibile sui 4 GB) | `qwen3:4b` (thinking) o un `deepseek-r1` distill — utili ma limitati |

⚠️ **Privacy:** i paper sono "Confidenziali". I modelli `:cloud` mandano il testo fuori dal PC.
Per ricerca riservata preferire un modello locale o valutare con attenzione il cloud.

### Per fare ricerca seriamente in locale (richiede hardware)

Con **24 GB di VRAM** (vedi `HARDWARE_LLM.md`) si possono girare i **reasoning model da 32B**:
`QwQ-32B`, `DeepSeek-R1-Distill-Qwen-32B`. Adatti al ragionamento scientifico **e** mantengono
la confidenzialità (tutto in locale). È il vero salto per la ricerca in casa.

### Regole d'oro per usare un LLM nella ricerca

1. **Non è una fonte di verità.** Allucina **citazioni, numeri, risultati** con sicurezza.
   Verifica SEMPRE ogni citazione/fatto (i modelli inventano paper inesistenti).
2. **Assistente, non oracolo.** Usalo per ragionare e scrivere, mai come autorità finale.
3. **La validazione vera non la fa l'LLM:** la fanno gli **esperimenti su dati reali**
   (es. NCBI RefSeq/ClinVar per RAB0-L, baseline reali per BLHM). L'LLM aiuta a scrivere
   il codice e a ragionare, ma il risultato scientifico nasce dai dati.

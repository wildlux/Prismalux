#!/usr/bin/env python3
"""
test_prompt_levels.py — Test 4 livelli di esperienza prompt utente.

Obiettivo: capire come i modelli rispondono a input di qualità diversa
e validare/migliorare classifyQuery() prima della produzione.

Livelli:
  L1 = pochissima esperienza  (vago, colloquiale, typo, zero contesto)
  L2 = media esperienza       (chiaro ma generico, qualche contesto)
  L3 = alta esperienza        (specifico, tecnico, strutturato)
  L4 = altissima esperienza   (preciso, vincoli espliciti, richiede ragionamento)

Metriche raccolte per ogni risposta:
  - tempo di risposta (ms)
  - lunghezza risposta (caratteri)
  - classifyQuery locale (Simple/Auto/Complex) — simula la logica Qt
  - think attivato? (stima locale, poi confronto con comportamento reale)
  - coverage: risposta contiene le parole chiave attese?
  - verbosity_ok: risposta proporzionata alla complessità del prompt?

Esegui:
  python3 test_prompt_levels.py
  python3 test_prompt_levels.py --models qwen3:4b mistral:7b-instruct
  python3 test_prompt_levels.py --save
  python3 test_prompt_levels.py --think   # forza think:true per confronto
"""
import argparse
import json
import os
import sys
import time
import urllib.request
from datetime import datetime

# ── Utilità condivise ───────────────────────────────────────────
try:
    from test_utils import ask_ollama, bar
except ImportError:
    def bar(n, total, width=20):
        filled = int(width * n / max(total, 1))
        return "█" * filled + "░" * (width - filled)

OLLAMA_URL = "http://127.0.0.1:11434/api/chat"


# ══════════════════════════════════════════════════════════════
#  classifyQuery — replica della logica Qt (ai_client.cpp)
#  Usata per predire se il sistema applicherà think:true/false
# ══════════════════════════════════════════════════════════════
COMPLEX_KW = [
    "spiega", "analizza", "confronta", "descri", "illustra",
    "approfondisci", "come funziona", "differenza", "pro e contro",
    "vantaggi", "svantaggi", "implementa", "algoritmo", "codice",
    "perché", "ragiona", "dimostra", "riassumi", "elabora",
    "architettura", "compara", "elenca tutto", "dimmi tutto",
    "trade-off", "thread-safe", "ottimizza", "analisi", "implementazione",
]

def classify_query(text: str) -> str:
    """Replica di AiClient::classifyQuery() da ai_client.cpp."""
    length = len(text)
    lower  = text.lower()
    if length > 200:
        return "Complex"
    for kw in COMPLEX_KW:
        if kw in lower:
            return "Complex"
    # BUG NOTO (riga 99 ai_client.cpp): entrambi i rami → Simple
    # Qui riplichiamo il comportamento ATTUALE (buggy) per misurarne l'impatto.
    return "Simple"   # ← sia <= 30 che > 30 → Simple (bug T1)

def classify_query_fixed(text: str) -> str:
    """Versione CORRETTA di classifyQuery (fix T1 proposto)."""
    length = len(text)
    lower  = text.lower()
    if length > 200:
        return "Complex"
    for kw in COMPLEX_KW:
        if kw in lower:
            return "Complex"
    return "Simple" if length <= 30 else "Auto"


# ══════════════════════════════════════════════════════════════
#  Suite di test — 4 livelli × 4 domande = 16 casi
# ══════════════════════════════════════════════════════════════
#
# Ogni test:
#   (id, livello, domanda, keywords_attese, max_char_ok, note)
#
# keywords_attese: parole che DEVONO comparire nella risposta (coverage)
# max_char_ok:     soglia oltre la quale la risposta è "troppo verbosa"
#                  per quel livello (verbosity check)

TESTS = [

    # ── L1: pochissima esperienza ───────────────────────────────
    # Input vago, colloquiale, zero contesto, possibili typo
    ("L1-A", 1,
     "dimmi qualcosa sulla ia",
     ["intelligenza", "artificiale", "machine", "apprendimento", "AI"],
     800,
     "Input minimo. Risposta breve attesa, no muri di testo."),

    ("L1-B", 1,
     "come funziona il computer",
     ["processore", "CPU", "memoria", "hardware", "software"],
     900,
     "Domanda larghissima. Risposta deve restare accessibile."),

    ("L1-C", 1,
     "voglio imparare a programmare da dove inizio",
     ["linguaggio", "Python", "JavaScript", "tutorial", "esercizi"],
     1000,
     "Utente principiante assoluto — risposta pratica, non teorica."),

    ("L1-D", 1,
     "cos'è internet",
     ["rete", "connessione", "protocollo", "web", "server"],
     700,
     "Domanda elementare. Risposta corta e chiara."),

    # ── L2: media esperienza ────────────────────────────────────
    # Chiaro ma generico, qualche termine tecnico
    ("L2-A", 2,
     "Come si legge un file CSV in Python?",
     ["csv", "open", "reader", "pandas", "colonne"],
     1200,
     "Risposta pratica con esempio codice attesa."),

    ("L2-B", 2,
     "Qual è la differenza tra una lista e un dizionario in Python?",
     ["lista", "dizionario", "chiave", "indice", "valore"],
     1200,
     "Confronto semplice — deve essere chiaro e conciso."),

    ("L2-C", 2,
     "Come funziona un'API REST?",
     ["HTTP", "GET", "POST", "endpoint", "JSON"],
     1300,
     "Spiegazione pratica, idealmente con un esempio."),

    ("L2-D", 2,
     "Spiega cosa sono i puntatori in C",
     ["puntatore", "memoria", "indirizzo", "dereferenzia", "*"],
     1400,
     "Concetto medio — risposta tecnica ma accessibile."),

    # ── L3: alta esperienza ─────────────────────────────────────
    # Specifico, tecnico, strutturato
    ("L3-A", 3,
     "Scrivi una funzione Python per BFS su un grafo come dizionario di adiacenza. "
     "Restituisci la lista dei nodi visitati in ordine.",
     ["def ", "queue", "visited", "deque", "append"],
     2000,
     "Deve restituire codice funzionante con BFS corretto."),

    ("L3-B", 3,
     "Qual è la complessità temporale e spaziale di QuickSort nel caso medio e peggiore? "
     "Spiega perché il caso peggiore si verifica.",
     ["O(n log n)", "O(n²)", "pivot", "partizione", "caso peggiore"],
     1800,
     "Risposta tecnica precisa con analisi Big-O."),

    ("L3-C", 3,
     "Differenza tra TCP e UDP: quando usi l'uno o l'altro in un'applicazione di rete?",
     ["TCP", "UDP", "affidabile", "latenza", "streaming"],
     1600,
     "Confronto tecnico con casi d'uso concreti."),

    ("L3-D", 3,
     "Come implementeresti un rate limiter in un'API REST? Descrivi almeno due strategie.",
     ["token bucket", "sliding window", "Redis", "limite", "richieste"],
     2000,
     "Risposta ingegneristica con almeno 2 pattern noti."),

    # ── L4: altissima esperienza ────────────────────────────────
    # Preciso, vincoli espliciti, richiede ragionamento profondo
    ("L4-A", 4,
     "Implementa in Python un LRU Cache thread-safe con O(1) get/put "
     "usando OrderedDict e threading.RLock. Includi type hints completi e "
     "almeno 3 casi di test con assert.",
     ["OrderedDict", "RLock", "def get", "def put", "assert"],
     3500,
     "Codice completo, corretto, thread-safe. No pseudocodice."),

    ("L4-B", 4,
     "Analizza i trade-off tra B-tree e LSM-tree per un database write-heavy "
     "con 10M+ record e RAM limitata a 8 GB. Considera write amplification, "
     "read amplification e space amplification.",
     ["B-tree", "LSM", "write amplification", "compaction", "SSTa"],
     2500,
     "Analisi tecnica profonda — deve coprire tutti e 3 gli amplification factor."),

    ("L4-C", 4,
     "Descrivi il protocollo Raft per il consenso distribuito. Spiega come gestisce "
     "le elezioni del leader, la replicazione del log e il recupero da failure, "
     "confrontandolo brevemente con Paxos.",
     ["Raft", "leader", "term", "log", "Paxos"],
     3000,
     "Risposta da esperto di sistemi distribuiti — deve citare i meccanismi chiave."),

    ("L4-D", 4,
     "Qual è la differenza fondamentale tra memoria virtuale con paginazione e "
     "segmentazione? Come la TLB mitiga il costo della traduzione degli indirizzi? "
     "Descrivi il thrashing e come il working set model lo previene.",
     ["paginazione", "segmentazione", "TLB", "thrashing", "working set"],
     3000,
     "SO avanzato — deve coprire tutti i concetti richiesti."),
]


# ══════════════════════════════════════════════════════════════
#  Chiamata Ollama con metriche
# ══════════════════════════════════════════════════════════════
SYSTEM_PROMPT = (
    "Sei un assistente tecnico preciso. Rispondi SEMPRE in italiano. "
    "Adatta la complessità della risposta alla domanda ricevuta: "
    "domande semplici → risposte brevi e chiare; domande tecniche → "
    "risposte complete e precise. Non aggiungere testo superfluo."
)

# Modelli che supportano il parametro think (famiglia qwen3/deepseek-r1)
# Mistral, llama, ecc. → 400 Bad Request se think viene inviato
THINK_CAPABLE_PREFIXES = ("qwen3", "qwen3.5", "deepseek-r1", "qwen2.5")

def model_supports_think(model: str) -> bool:
    base = model.split(":")[0].lower()
    return any(base.startswith(p) for p in THINK_CAPABLE_PREFIXES)

def call_ollama_timed(model: str, prompt: str,
                      think: bool = False,
                      num_predict: int = 1024,
                      timeout: int = 120) -> dict:
    """Chiama Ollama e restituisce risposta + metriche.

    - Il parametro think viene inviato SOLO ai modelli che lo supportano.
    - Se qwen3 ritorna contenuto vuoto con think:ON, il blocco <think>
      è stato separato da Ollama: estraiamo il campo thinking come fallback.
    """
    use_think = think and model_supports_think(model)

    body = {
        "model":  model,
        "stream": False,
        "messages": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user",   "content": prompt},
        ],
        "options": {
            "temperature":    0.4,
            "top_p":          0.9,
            "repeat_penalty": 1.05,
            "num_predict":    num_predict,
            "num_ctx":        4096,
        },
    }
    if use_think:
        body["think"] = True   # solo per modelli compatibili

    req = urllib.request.Request(
        OLLAMA_URL,
        data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    t0 = time.time()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            data = json.loads(r.read())
        elapsed_ms = int((time.time() - t0) * 1000)
        msg     = data.get("message", {})
        content = msg.get("content", "").strip()
        # Fallback: se content vuoto ma c'è il blocco thinking separato
        # (Ollama separa <think>...</think> nel campo "thinking")
        if not content:
            content = msg.get("thinking", "").strip()
            if content:
                content = f"[solo thinking — no risposta finale]\n{content[:300]}"
        return {
            "ok":         True,
            "response":   content,
            "elapsed_ms": elapsed_ms,
            "chars":      len(content),
            "think_used": use_think,
            "error":      None,
        }
    except Exception as e:
        elapsed_ms = int((time.time() - t0) * 1000)
        return {
            "ok":         False,
            "response":   "",
            "elapsed_ms": elapsed_ms,
            "chars":      0,
            "think_used": use_think,
            "error":      str(e),
        }


# ══════════════════════════════════════════════════════════════
#  Valutazione automatica
# ══════════════════════════════════════════════════════════════
def evaluate(result: dict, keywords: list[str], max_char_ok: int) -> dict:
    """Valuta una risposta e restituisce metriche di qualità."""
    if not result["ok"]:
        return {"coverage": 0.0, "verbosity_ok": False, "score": 0}

    resp_lower = result["response"].lower()

    # Coverage: quante keyword attese compaiono nella risposta?
    hits = sum(1 for kw in keywords if kw.lower() in resp_lower)
    coverage = hits / len(keywords) if keywords else 1.0

    # Verbosity: la risposta è entro il limite atteso per quel livello?
    verbosity_ok = result["chars"] <= max_char_ok

    # Score composito 0-100
    score = int(coverage * 70) + (30 if verbosity_ok else 0)

    return {
        "coverage":     round(coverage, 2),
        "hits":         hits,
        "total_kw":     len(keywords),
        "verbosity_ok": verbosity_ok,
        "score":        score,
    }


# ══════════════════════════════════════════════════════════════
#  Calcolo num_predict dinamico (simula la logica Qt corretta)
# ══════════════════════════════════════════════════════════════
def dynamic_num_predict(query_type: str, level: int) -> int:
    """Simula il comportamento post-fix T1+T2."""
    if query_type == "Simple":
        return 512
    if query_type == "Complex":
        return 1024 + (level - 1) * 256   # L1→1024, L2→1280, L3→1536, L4→1792
    return 768   # Auto


# ══════════════════════════════════════════════════════════════
#  Main
# ══════════════════════════════════════════════════════════════
LEVEL_NAMES = {1: "L1 pochissima", 2: "L2 media", 3: "L3 alta", 4: "L4 altissima"}
LEVEL_COLORS = {1: "\033[94m", 2: "\033[92m", 3: "\033[93m", 4: "\033[91m"}
RESET = "\033[0m"
BOLD  = "\033[1m"

def main():
    parser = argparse.ArgumentParser(description="Test livelli prompt utente")
    parser.add_argument("--models", nargs="+",
                        default=["qwen3:4b", "mistral:7b-instruct"],
                        help="Modelli da testare (default: qwen3:4b mistral:7b-instruct)")
    parser.add_argument("--save", "-s", action="store_true",
                        help="Salva risultati in ~/.prismalux/prompt_level_results.json")
    parser.add_argument("--think", action="store_true",
                        help="Forza think:true per tutti i test (confronto)")
    parser.add_argument("--level", type=int, choices=[1,2,3,4],
                        help="Esegui solo un livello (1-4)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Mostra testo completo delle risposte")
    args = parser.parse_args()

    models = args.models
    tests  = [t for t in TESTS if args.level is None or t[1] == args.level]

    print(f"\n{BOLD}{'='*70}{RESET}")
    print(f"{BOLD}  Prismalux — Test Livelli Prompt Utente{RESET}")
    print(f"  Modelli: {', '.join(models)}")
    print(f"  Test:    {len(tests)} casi × {len(models)} modelli = {len(tests)*len(models)} chiamate")
    print(f"  Think:   {'FORZATO ON' if args.think else 'dinamico (simula classifyQuery)'}")
    print(f"{'='*70}\n")

    all_results = []
    model_totals = {m: {"score": 0, "time": 0, "n": 0} for m in models}
    level_totals = {l: {"score": 0, "n": 0} for l in [1,2,3,4]}

    for tid, level, prompt, keywords, max_char, note in tests:
        qt_buggy = classify_query(prompt)
        qt_fixed = classify_query_fixed(prompt)
        think_on  = args.think or (qt_fixed == "Complex")
        num_pred  = dynamic_num_predict(qt_fixed, level)

        col = LEVEL_COLORS[level]
        print(f"{col}{BOLD}[{tid}]{RESET} {col}{LEVEL_NAMES[level]}{RESET}")
        print(f"  Prompt  : {prompt[:80]}{'...' if len(prompt)>80 else ''}")
        print(f"  classify: buggy={qt_buggy}  fixed={qt_fixed}  "
              f"think_richiesto={'ON' if think_on else 'OFF'}  num_predict={num_pred}")
        print(f"  Nota    : {note}")

        row = {
            "id": tid, "level": level, "prompt": prompt,
            "classify_buggy": qt_buggy, "classify_fixed": qt_fixed,
            "think": think_on, "num_predict": num_pred,
            "models": {}
        }

        for model in models:
            sys.stdout.write(f"  {model:30} → ")
            sys.stdout.flush()

            res  = call_ollama_timed(model, prompt, think=think_on, num_predict=num_pred)
            eval = evaluate(res, keywords, max_char)

            # Stampa risultato inline
            if not res["ok"]:
                print(f"\033[31mERRORE: {res['error']}\033[0m")
            else:
                cov_bar   = bar(eval["hits"], eval["total_kw"], 10)
                verb_sym  = "✓" if eval["verbosity_ok"] else "⚠"
                # think_sym riflette se think è stato REALMENTE inviato al modello
                think_sym = "🧠" if res.get("think_used") else "⚡"
                print(
                    f"{think_sym} {res['elapsed_ms']:5}ms  "
                    f"{res['chars']:5}c  "
                    f"cov [{cov_bar}] {eval['hits']}/{eval['total_kw']}  "
                    f"verb{verb_sym}  "
                    f"score={eval['score']:3}/100"
                )
                if args.verbose:
                    print(f"\n    {'─'*60}")
                    for line in res["response"][:600].split("\n"):
                        print(f"    {line}")
                    if res["chars"] > 600:
                        print(f"    ... [{res['chars']-600} caratteri omessi]")
                    print(f"    {'─'*60}\n")

            row["models"][model] = {**res, **eval}
            model_totals[model]["score"] += eval["score"]
            model_totals[model]["time"]  += res["elapsed_ms"]
            model_totals[model]["n"]     += 1
            level_totals[level]["score"] += eval["score"]
            level_totals[level]["n"]     += 1

        all_results.append(row)
        print()

    # ── Riepilogo ───────────────────────────────────────────────
    print(f"\n{BOLD}{'='*70}")
    print(f"  RIEPILOGO FINALE")
    print(f"{'='*70}{RESET}\n")

    print(f"{BOLD}  Per modello:{RESET}")
    for model in models:
        t = model_totals[model]
        avg_score = t["score"] / t["n"] if t["n"] else 0
        avg_time  = t["time"]  / t["n"] if t["n"] else 0
        score_bar = bar(int(avg_score), 100, 20)
        print(f"  {model:30} score={avg_score:5.1f}/100 [{score_bar}]  "
              f"tempo medio={avg_time:6.0f}ms")

    print(f"\n{BOLD}  Per livello:{RESET}")
    for level in [1, 2, 3, 4]:
        t = level_totals[level]
        avg = t["score"] / t["n"] if t["n"] else 0
        col = LEVEL_COLORS[level]
        lb  = bar(int(avg), 100, 20)
        print(f"  {col}{LEVEL_NAMES[level]:20}{RESET} score={avg:5.1f}/100 [{lb}]")

    # ── Analisi classifyQuery bug ────────────────────────────────
    misclassified = [
        r for r in all_results
        if r["classify_buggy"] != r["classify_fixed"]
    ]
    print(f"\n{BOLD}  Impatto bug T1 (classifyQuery):{RESET}")
    print(f"  Prompt che verrebbero classificati DIVERSAMENTE dopo il fix:")
    for r in misclassified:
        print(f"  [{r['id']}] buggy={r['classify_buggy']}  "
              f"fixed={r['classify_fixed']}  "
              f"→ '{r['prompt'][:55]}...'")
    if not misclassified:
        print("  (nessuno — tutti i prompt già classificati correttamente)")

    # ── Salvataggio ─────────────────────────────────────────────
    if args.save:
        out_dir  = os.path.expanduser("~/.prismalux")
        os.makedirs(out_dir, exist_ok=True)
        out_path = os.path.join(out_dir, "prompt_level_results.json")
        output   = {
            "timestamp": datetime.now().isoformat(),
            "models":    models,
            "think_forced": args.think,
            "results":   all_results,
            "totals": {
                "by_model": model_totals,
                "by_level": level_totals,
            }
        }
        with open(out_path, "w") as f:
            json.dump(output, f, indent=2, ensure_ascii=False)
        print(f"\n  Risultati salvati → {out_path}")

    print(f"\n{BOLD}  Bug aperti da risolvere dopo questo test:{RESET}")
    print(f"  [T1] classifyQuery riga 99 — {len(misclassified)} prompt impattati")
    print(f"  [T2] chat() wrapper usa QueryAuto — think/num_predict mai applicati")
    print(f"  Fix: 3 righe totali in ai_client.cpp\n")


if __name__ == "__main__":
    main()

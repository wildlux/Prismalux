#!/usr/bin/env python3
"""
temp_benchmark.py — Benchmark temperatura per ogni modello Ollama installato.

Per ogni modello testa le temperature [0.1, 0.3, 0.5, 0.7, 0.9, 1.1].
Per ogni coppia (modello, temperatura) esegue N_RUNS query di test e calcola
uno score 0-100. Genera grafici matplotlib e un JSON con i risultati.

Uso:
    python3 temp_benchmark.py [--host 127.0.0.1] [--port 11434] [--runs 3] [--out ./benchmark_out]
"""

import argparse
import json
import os
import re
import sys
import time
from dataclasses import dataclass, field
from typing import Optional

try:
    import requests
except ImportError:
    sys.exit("Installa requests: pip install requests")

try:
    import numpy as np
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.lines import Line2D
    HAS_PLOT = True
except ImportError:
    HAS_PLOT = False
    print("[WARN] matplotlib/numpy non disponibili — salvo solo JSON", file=sys.stderr)

# ── Configurazione ────────────────────────────────────────────────────────────

TEMPERATURES = [0.1, 0.3, 0.5, 0.7, 0.9, 1.1]
N_RUNS       = 3        # ripetizioni per (modello, temperatura)
TIMEOUT_S    = 90       # timeout per singola chiamata Ollama
SYSTEM_PROMPT = (
    "Il tuo nome e' Prismalux. Sei l'assistente AI dell'applicazione Prismalux. "
    "Rispondi sempre in italiano. Sii conciso."
)

# ── Domande di test con valutatore ────────────────────────────────────────────

@dataclass
class Question:
    text: str
    weight: float          # peso nel punteggio finale (somma = 1.0)
    keywords_ok: list      # parole/pattern che fanno salire lo score
    keywords_bad: list     # parole/pattern che abbassano lo score
    description: str = ""

QUESTIONS = [
    Question(
        text="Come ti chiami?",
        weight=0.35,
        keywords_ok=["prismalux", "prisma"],
        keywords_bad=["non lo so", "non sono sicuro", "non ho un nome", "non ne sono", "incerto",
                      "non posso", "non saprei", "sconosciuto"],
        description="Identità AI"
    ),
    Question(
        text="Qual è la capitale d'Italia?",
        weight=0.20,
        keywords_ok=["roma"],
        keywords_bad=["non lo so", "non sono sicuro", "non conosco"],
        description="Conoscenza geografica"
    ),
    Question(
        text="Quanto fa 2 + 2?",
        weight=0.15,
        keywords_ok=["4", "quattro"],
        keywords_bad=["non lo so", "non posso calcolare"],
        description="Aritmetica base"
    ),
    Question(
        text="Descrivi brevemente cosa sei in una frase.",
        weight=0.15,
        keywords_ok=["assistente", "ai", "intelligenza artificiale", "prismalux"],
        keywords_bad=["non lo so", "non posso", "sconosciuto"],
        description="Auto-descrizione"
    ),
    Question(
        text="Chi ha scritto la Divina Commedia?",
        weight=0.15,
        keywords_ok=["dante", "alighieri"],
        keywords_bad=["non lo so", "non sono sicuro"],
        description="Cultura generale"
    ),
]

# ── Chiamata Ollama ───────────────────────────────────────────────────────────

THINK_CAPABLE = ("qwen3", "qwen3.5", "deepseek-r1", "qwq", "qwen2.5")

def _is_think_capable(model: str) -> bool:
    return any(model.startswith(p) for p in THINK_CAPABLE)

def ollama_chat(base_url: str, model: str, user: str, temperature: float) -> Optional[str]:
    """Chiama /api/chat e restituisce il testo della risposta o None in caso di errore."""
    opts: dict = {
        "temperature": temperature,
        "num_predict": 120,    # risposta breve
    }
    # Modelli reasoning: disabilita thinking via parametro API (non testo nel prompt).
    # think:false fa rispondere direttamente senza blocco <think>.
    if _is_think_capable(model):
        opts["think"] = False

    payload = {
        "model":    model,
        "messages": [
            {"role": "system",  "content": SYSTEM_PROMPT},
            {"role": "user",    "content": user},
        ],
        "stream":  False,
        "options": opts,
    }
    try:
        r = requests.post(f"{base_url}/api/chat", json=payload, timeout=TIMEOUT_S)
        r.raise_for_status()
        return r.json()["message"]["content"].strip()
    except Exception as e:
        print(f"    [ERR] {e}", file=sys.stderr)
        return None

# ── Valutazione risposta ─────────────────────────────────────────────────────

def score_response(q: Question, response: Optional[str]) -> float:
    """Restituisce 0.0–1.0 per una singola risposta."""
    if response is None:
        return 0.0
    r = response.lower()
    # Penalità per keyword negativi (es. "non lo so")
    for kw in q.keywords_bad:
        if kw in r:
            return 0.0
    # Bonus per keyword positivi
    for kw in q.keywords_ok:
        if kw in r:
            return 1.0
    # Risposta presente ma non contiene keyword né negativi → 0.5 (neutro)
    return 0.5 if len(r.strip()) > 3 else 0.0

# ── Lista modelli Ollama ──────────────────────────────────────────────────────

def list_models(base_url: str) -> list[str]:
    try:
        r = requests.get(f"{base_url}/api/tags", timeout=10)
        r.raise_for_status()
        return [m["name"] for m in r.json().get("models", [])]
    except Exception as e:
        sys.exit(f"Impossibile ottenere lista modelli da Ollama: {e}")

# ── Benchmark principale ──────────────────────────────────────────────────────

@dataclass
class TempResult:
    temperature: float
    score: float           # 0–100, media pesata su tutte le domande
    per_question: dict = field(default_factory=dict)  # q.description → score 0-100

def benchmark_model(base_url: str, model: str, n_runs: int) -> list[TempResult]:
    results = []
    for temp in TEMPERATURES:
        print(f"  T={temp:.1f}", end="", flush=True)
        q_scores = {q.description: [] for q in QUESTIONS}

        for _run in range(n_runs):
            for q in QUESTIONS:
                ans = ollama_chat(base_url, model, q.text, temp)
                s   = score_response(q, ans)
                q_scores[q.description].append(s)
            print(".", end="", flush=True)

        # Media per domanda, poi media pesata
        q_avg   = {k: (sum(v) / len(v)) * 100 for k, v in q_scores.items()}
        total   = sum(q.weight * q_avg[q.description] for q in QUESTIONS)
        results.append(TempResult(temp, round(total, 2), q_avg))
    print()
    return results

# ── Stima temperatura ottimale ────────────────────────────────────────────────

def find_optimal(results: list[TempResult]) -> tuple[float, float]:
    """
    Restituisce (temp_ottimale, score_ottimale).
    Strategia: la temperatura più bassa con score >= 80; se nessuna supera 80,
    prende il massimo assoluto.
    """
    threshold = 80.0
    candidates = [(r.temperature, r.score) for r in results if r.score >= threshold]
    if candidates:
        return min(candidates, key=lambda x: x[0])
    best = max(results, key=lambda r: r.score)
    return best.temperature, best.score

# ── Fit polinomiale ───────────────────────────────────────────────────────────

def poly_fit(temps: list[float], scores: list[float], degree: int = 2):
    """Ritorna (coefficienti, np.poly1d) o (None, None) se numpy non disponibile."""
    if not HAS_PLOT:
        return None, None
    coeffs = np.polyfit(temps, scores, degree)
    return coeffs, np.poly1d(coeffs)

# ── Generazione grafici ───────────────────────────────────────────────────────

COLORS = [
    "#10a37f", "#3b82f6", "#f59e0b", "#ef4444",
    "#8b5cf6", "#ec4899", "#06b6d4", "#84cc16",
]

def make_plots(all_results: dict[str, list[TempResult]], out_dir: str):
    if not HAS_PLOT:
        return
    os.makedirs(out_dir, exist_ok=True)

    # ── Grafico combinato: tutti i modelli su un unico plot ──
    fig, ax = plt.subplots(figsize=(12, 7))
    fig.patch.set_facecolor("#0f172a")
    ax.set_facecolor("#1e293b")
    ax.tick_params(colors="#94a3b8")
    ax.xaxis.label.set_color("#cbd5e1")
    ax.yaxis.label.set_color("#cbd5e1")
    ax.title.set_color("#f8fafc")
    for spine in ax.spines.values():
        spine.set_edgecolor("#334155")

    legend_elements = []
    for idx, (model, results) in enumerate(all_results.items()):
        color = COLORS[idx % len(COLORS)]
        temps  = [r.temperature for r in results]
        scores = [r.score for r in results]
        label  = model.split(":")[0][:22]  # tronca etichetta

        ax.plot(temps, scores, "o-", color=color, linewidth=2, markersize=6, alpha=0.9)

        # Fit polinomiale
        _, p = poly_fit(temps, scores)
        if p is not None:
            t_fine = np.linspace(min(temps), max(temps), 200)
            ax.plot(t_fine, np.clip(p(t_fine), 0, 100), "--", color=color, alpha=0.4, linewidth=1)

        # Punto ottimale
        opt_t, opt_s = find_optimal(results)
        ax.axvline(opt_t, color=color, linestyle=":", alpha=0.3, linewidth=1)
        ax.plot(opt_t, opt_s, "*", color=color, markersize=14, zorder=5)

        legend_elements.append(
            Line2D([0], [0], color=color, linewidth=2, label=f"{label} (opt T={opt_t:.1f})")
        )

    ax.set_xlabel("Temperatura", fontsize=12)
    ax.set_ylabel("Score  (0 = confuso, 100 = perfetto)", fontsize=12)
    ax.set_title("Benchmark temperatura modelli Ollama — Prismalux", fontsize=14, pad=14)
    ax.set_xlim(0.0, 1.25)
    ax.set_ylim(-5, 105)
    ax.axhline(80, color="#f59e0b", linestyle="--", alpha=0.5, linewidth=1, label="Soglia 80")
    ax.legend(handles=legend_elements + [Line2D([0],[0], color="#f59e0b", linestyle="--", label="Soglia 80")],
              facecolor="#0f172a", edgecolor="#334155", labelcolor="#cbd5e1", fontsize=9)
    ax.grid(True, color="#334155", alpha=0.5, linewidth=0.5)

    path_svg = os.path.join(out_dir, "benchmark_combined.svg")
    fig.savefig(path_svg, format="svg", bbox_inches="tight", facecolor=fig.get_facecolor())
    path_combined = os.path.join(out_dir, "benchmark_combined.png")
    fig.savefig(path_combined, dpi=150, bbox_inches="tight", facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"[GRAFICO] {path_svg}")

    # ── Grafici individuali ──
    for model, results in all_results.items():
        safe_name = re.sub(r"[^a-zA-Z0-9_.-]", "_", model)
        fig, axes = plt.subplots(1, 2, figsize=(13, 5))
        fig.patch.set_facecolor("#0f172a")

        temps  = [r.temperature for r in results]
        scores = [r.score for r in results]
        opt_t, opt_s = find_optimal(results)

        for ax in axes:
            ax.set_facecolor("#1e293b")
            ax.tick_params(colors="#94a3b8")
            for sp in ax.spines.values(): sp.set_edgecolor("#334155")
            ax.grid(True, color="#334155", alpha=0.5, linewidth=0.5)

        # Sinistra: score globale + fit
        ax0 = axes[0]
        ax0.plot(temps, scores, "o-", color="#10a37f", linewidth=2.5, markersize=8)
        _, p = poly_fit(temps, scores)
        if p is not None:
            t_fine = np.linspace(min(temps), max(temps), 300)
            ax0.plot(t_fine, np.clip(p(t_fine), 0, 100), "--", color="#3b82f6",
                     linewidth=1.5, label="Fit polinomiale (grado 2)")
        ax0.axvline(opt_t, color="#f59e0b", linestyle="--", linewidth=1.5,
                    label=f"Ottimale T={opt_t:.1f} (score={opt_s:.0f})")
        ax0.plot(opt_t, opt_s, "*", color="#f59e0b", markersize=16, zorder=6)
        ax0.axhline(80, color="#ef4444", linestyle=":", alpha=0.5, linewidth=1, label="Soglia 80")
        ax0.set_xlabel("Temperatura"); ax0.set_ylabel("Score globale")
        ax0.set_title(f"{model}\nScore globale vs temperatura", color="#f8fafc")
        ax0.set_xlim(0.0, 1.25); ax0.set_ylim(-5, 105)
        ax0.legend(facecolor="#0f172a", edgecolor="#334155", labelcolor="#cbd5e1", fontsize=8)
        ax0.xaxis.label.set_color("#cbd5e1"); ax0.yaxis.label.set_color("#cbd5e1")

        # Destra: score per domanda
        ax1 = axes[1]
        q_names = [q.description for q in QUESTIONS]
        color_q = ["#10a37f","#3b82f6","#f59e0b","#8b5cf6","#ef4444"]
        for qi, (q, cq) in enumerate(zip(QUESTIONS, color_q)):
            q_scores = [r.per_question.get(q.description, 0) for r in results]
            ax1.plot(temps, q_scores, "o-", color=cq, linewidth=1.5,
                     markersize=5, label=q.description)
        ax1.set_xlabel("Temperatura"); ax1.set_ylabel("Score domanda")
        ax1.set_title("Score per domanda", color="#f8fafc")
        ax1.set_xlim(0.0, 1.25); ax1.set_ylim(-5, 105)
        ax1.legend(facecolor="#0f172a", edgecolor="#334155", labelcolor="#cbd5e1", fontsize=8)
        ax1.xaxis.label.set_color("#cbd5e1"); ax1.yaxis.label.set_color("#cbd5e1")

        fig.suptitle(f"Benchmark: {model}", color="#f8fafc", fontsize=13)
        fig.tight_layout()
        out_path = os.path.join(out_dir, f"benchmark_{safe_name}.png")
        fig.savefig(out_path, dpi=150, bbox_inches="tight", facecolor=fig.get_facecolor())
        plt.close(fig)
        print(f"[GRAFICO] {out_path}")

# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Benchmark temperatura Ollama")
    parser.add_argument("--host",  default="127.0.0.1")
    parser.add_argument("--port",  default=11434, type=int)
    parser.add_argument("--runs",  default=N_RUNS, type=int,
                        help="Ripetizioni per (modello, temperatura)")
    parser.add_argument("--out",   default="./benchmark_out",
                        help="Cartella output (grafici + JSON)")
    parser.add_argument("--models", default="",
                        help="Lista modelli separati da virgola (default: tutti)")
    parser.add_argument("--temps",  default="",
                        help="Temperature personalizzate es. '0.1,0.3,0.5' (default: predefinite)")
    args = parser.parse_args()

    base_url = f"http://{args.host}:{args.port}"

    global TEMPERATURES
    if args.temps:
        TEMPERATURES = sorted(set(float(t.strip()) for t in args.temps.split(",")))

    models = [m.strip() for m in args.models.split(",") if m.strip()] if args.models else []
    if not models:
        models = list_models(base_url)
    if not models:
        sys.exit("Nessun modello trovato. Avvia Ollama con: ollama serve")

    print(f"Modelli da testare ({len(models)}): {', '.join(models)}")
    print(f"Temperature: {TEMPERATURES}")
    print(f"Ripetizioni per punto: {args.runs}")
    print(f"Output: {args.out}\n")

    all_results: dict[str, list[TempResult]] = {}
    start_total = time.time()

    for model in models:
        print(f"{'─'*60}")
        print(f"Modello: {model}")
        t0 = time.time()
        res = benchmark_model(base_url, model, args.runs)
        elapsed = time.time() - t0
        all_results[model] = res

        opt_t, opt_s = find_optimal(res)
        print(f"  Risultati: " + "  ".join(f"T{r.temperature:.1f}={r.score:.0f}" for r in res))
        print(f"  Temperatura ottimale: {opt_t:.1f}  (score={opt_s:.0f})  [{elapsed:.0f}s]")

    total_elapsed = time.time() - start_total
    print(f"\n{'═'*60}")
    print(f"Benchmark completato in {total_elapsed:.0f}s")

    # ── Riepilogo ottimali ──
    print("\n## Riepilogo temperature ottimali")
    summary = {}
    for model, res in all_results.items():
        opt_t, opt_s = find_optimal(res)
        summary[model] = {"optimal_temperature": opt_t, "optimal_score": opt_s,
                          "per_temperature": [{"temperature": r.temperature,
                                               "score": r.score,
                                               "per_question": r.per_question}
                                              for r in res]}
        print(f"  {model:<40} T={opt_t:.1f}  score={opt_s:.0f}")

    # ── Salva JSON ──
    os.makedirs(args.out, exist_ok=True)
    json_path = os.path.join(args.out, "benchmark_results.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)
    print(f"\n[JSON] {json_path}")

    # ── Genera grafici ──
    if HAS_PLOT:
        make_plots(all_results, args.out)
    else:
        print("[INFO] Installa matplotlib + numpy per i grafici: pip install matplotlib numpy")

    # ── Consiglio globale ──
    if summary:
        avg_opt = sum(v["optimal_temperature"] for v in summary.values()) / len(summary)
        print(f"\n{'═'*60}")
        print(f"Temperatura ottimale media su tutti i modelli: {avg_opt:.2f}")
        print(f"Consiglio: imposta temperatura = {round(avg_opt*10)/10:.1f} come default in Prismalux.")

if __name__ == "__main__":
    main()

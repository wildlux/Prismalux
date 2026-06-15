"""
ai_trace.py — Tavola di traccia per modelli LLM
Mostra in tempo reale: thinking tokens, logprobs, entropia per token,
e confronto fiducia tra modelli sulla stessa domanda.

Uso:
  python3 ai_trace.py --model qwen3:4b --domanda "Come ti chiami?"
  python3 ai_trace.py --compare "llama3.2:3b,qwen3:4b" --domanda "Come ti chiami?"
  python3 ai_trace.py --model qwen3:4b --full        # mostra tutto il thinking
"""

import argparse, json, math, sys, time, textwrap
import requests
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np

OLLAMA   = "http://127.0.0.1:11434"
TIMEOUT  = 180

SYS = (
    "Il tuo nome e' Prismalux. Sei l'assistente AI integrato nell'applicazione Prismalux "
    "(sviluppata da Paolo). Quando ti viene chiesto come ti chiami o chi sei, "
    "rispondi sempre 'Prismalux'. "
)

BG, FG  = "#0f172a", "#e2e8f0"
GRN, RED, YLW, ACC, VIOL = "#4ade80", "#f87171", "#facc15", "#38bdf8", "#a78bfa"
ORG = "#fb923c"

# ─────────────────────────────────────────────────────────────────────────────
def stream_chat(model: str, domanda: str, temperature: float = 0.1) -> dict:
    """Stream /api/chat con logprobs. Ritorna dict con tokens, logprobs, thinking."""
    THINK_CAPABLE = ("qwen3", "qwen3.5", "deepseek-r1", "qwq", "qwen2.5")
    opts: dict = {"temperature": temperature, "num_predict": 512}
    # Reasoning models: think:false via parametro API — non testo nel prompt
    if any(model.startswith(p) for p in THINK_CAPABLE):
        opts["think"] = False

    payload = {
        "model": model,
        "messages": [
            {"role": "system", "content": SYS},
            {"role": "user",   "content": domanda},
        ],
        "stream": True,
        "options": opts,
        "logprobs": True,   # Ollama >= 0.4 supporta logprobs in /api/chat
    }

    tokens       = []
    logprobs     = []
    full_text    = ""
    thinking_txt = ""
    response_txt = ""
    in_think     = False
    t0           = time.time()

    print(f"\n{'━'*60}")
    print(f"  Modello: {model}   T={temperature}")
    print(f"  Domanda: {domanda}")
    print(f"{'━'*60}")

    try:
        with requests.post(f"{OLLAMA}/api/chat", json=payload,
                           stream=True, timeout=TIMEOUT) as r:
            r.raise_for_status()
            thinking_shown = False
            for line in r.iter_lines():
                if not line:
                    continue
                obj = json.loads(line)
                chunk = obj.get("message", {}).get("content", "")
                full_text += chunk

                # Rileva thinking block
                if "<think>" in full_text and not in_think:
                    in_think = True
                if in_think and "</think>" not in full_text:
                    thinking_txt += chunk
                    if not thinking_shown:
                        print(f"\n  \033[38;5;240m[THINKING]\033[0m ", end="", flush=True)
                        thinking_shown = True
                    # Mostra pensiero in grigio scuro (max 120 char per riga)
                    sys.stdout.write(f"\033[38;5;240m{chunk}\033[0m")
                    sys.stdout.flush()
                elif in_think and "</think>" in full_text:
                    in_think = False
                    # estrai thinking completo
                    s = full_text.find("<think>") + 7
                    e = full_text.find("</think>")
                    thinking_txt = full_text[s:e]
                    response_txt = full_text[e+8:]
                    print(f"\n  \033[38;5;240m[fine thinking — {len(thinking_txt)} char]\033[0m")
                    if response_txt:
                        print(f"\n  \033[92m[RISPOSTA]\033[0m {response_txt}", end="", flush=True)
                else:
                    response_txt += chunk
                    print(chunk, end="", flush=True)

                # Logprobs (se disponibili)
                lp = obj.get("logprobs")
                if lp and isinstance(lp, list):
                    for entry in lp:
                        tok = entry.get("token", "")
                        lv  = entry.get("logprob", 0.0)
                        tokens.append(tok)
                        logprobs.append(lv)
                elif lp and isinstance(lp, dict):
                    for tok, lv in zip(lp.get("tokens", []), lp.get("token_logprobs", [])):
                        tokens.append(tok)
                        logprobs.append(lv if lv is not None else -10.0)

                if obj.get("done"):
                    break

    except requests.exceptions.ReadTimeout:
        print(f"\n  \033[91m[TIMEOUT dopo {TIMEOUT}s]\033[0m")
        return {"tokens": tokens, "logprobs": logprobs,
                "thinking": thinking_txt, "response": response_txt,
                "full": full_text, "elapsed": time.time() - t0,
                "timeout": True}

    elapsed = time.time() - t0
    print(f"\n\n  Tempo: {elapsed:.1f}s  |  Token risposta: {len(response_txt.split())}")

    # Se non ci sono thinking tokens ma la risposta è presente
    if not response_txt and not in_think:
        response_txt = full_text

    return {
        "tokens":   tokens,
        "logprobs": logprobs,
        "thinking": thinking_txt,
        "response": response_txt,
        "full":     full_text,
        "elapsed":  elapsed,
        "timeout":  False,
    }


def entropy_from_logprob(lp: float) -> float:
    """Entropia approssimata dal logprob del token più probabile."""
    p = math.exp(lp)
    p = max(min(p, 1.0 - 1e-9), 1e-9)
    return -p * math.log2(p) - (1 - p) * math.log2(1 - p)


# ─────────────────────────────────────────────────────────────────────────────
def plot_trace(result: dict, model: str, domanda: str, out_path: str):
    """Genera il grafico tavola di traccia."""
    tokens   = result["tokens"]
    logprobs = result["logprobs"]
    thinking = result["thinking"]
    response = result["response"]

    has_logprobs = len(logprobs) > 0
    has_thinking = len(thinking) > 10

    fig = plt.figure(figsize=(14, 8), facecolor=BG)
    fig.suptitle(
        f"Tavola di Traccia AI — {model}\nDomanda: \"{domanda}\"",
        color=FG, fontsize=12, y=0.97
    )

    rows = 2 if has_logprobs else 1
    gs   = gridspec.GridSpec(rows, 2, figure=fig, hspace=0.45, wspace=0.3,
                             left=0.06, right=0.97, top=0.90, bottom=0.07)

    # ── Pannello 1: testo risposta + thinking ─────────────────────────────
    ax_txt = fig.add_subplot(gs[0, 0])
    ax_txt.set_facecolor("#0d1b2a")
    ax_txt.axis("off")
    ax_txt.set_title("Output modello", color=FG, fontsize=9, loc="left")

    if has_thinking:
        think_short = textwrap.fill(thinking[:400] + ("…" if len(thinking) > 400 else ""), 55)
        ax_txt.text(0.02, 0.95, f"[THINKING — {len(thinking)} char]",
                    transform=ax_txt.transAxes, color="#64748b",
                    fontsize=7, va="top", fontstyle="italic")
        ax_txt.text(0.02, 0.88, think_short,
                    transform=ax_txt.transAxes, color="#475569",
                    fontsize=6.5, va="top", wrap=True,
                    fontfamily="monospace")
        resp_y = 0.40
    else:
        resp_y = 0.85

    resp_short = textwrap.fill(response[:300] + ("…" if len(response) > 300 else ""), 55)
    ax_txt.text(0.02, resp_y, "[RISPOSTA]",
                transform=ax_txt.transAxes, color=GRN,
                fontsize=8, va="top", fontweight="bold")
    ax_txt.text(0.02, resp_y - 0.07, resp_short,
                transform=ax_txt.transAxes, color=FG,
                fontsize=7.5, va="top", fontfamily="monospace")

    elapsed_str = f"{result['elapsed']:.1f}s"
    if result.get("timeout"):
        elapsed_str += " [TIMEOUT]"
    ax_txt.text(0.02, 0.04, f"Tempo: {elapsed_str}",
                transform=ax_txt.transAxes, color=YLW, fontsize=8)

    # ── Pannello 2: statistiche qualitative ───────────────────────────────
    ax_stats = fig.add_subplot(gs[0, 1])
    ax_stats.set_facecolor("#0d1b2a")
    ax_stats.axis("off")
    ax_stats.set_title("Diagnostica", color=FG, fontsize=9, loc="left")

    # Calcola metriche qualitative
    resp_lower = response.lower()
    kw_ok  = ["prismalux", "prisma"]
    kw_bad = ["non lo so", "non ho un nome", "non sono sicuro", "non posso"]
    id_ok  = any(k in resp_lower for k in kw_ok)
    id_bad = any(k in resp_lower for k in kw_bad)
    id_col = GRN if id_ok else (RED if id_bad else YLW)
    id_txt = "✅ Identità corretta" if id_ok else ("❌ Identità errata" if id_bad else "⚠️  Identità ambigua")

    lang_it = sum(1 for w in ["sono","il","la","un","di","e","che","ho","mi","si"]
                  if w in resp_lower.split())
    lang_cn = any(ord(c) > 0x4E00 and ord(c) < 0x9FFF for c in response)
    lang_txt = "🇨🇳 Cinese" if lang_cn else (f"🇮🇹 Italiano ({lang_it} kw)" if lang_it > 1 else "❓ Altra lingua")

    think_ratio = len(thinking) / max(len(response), 1)
    think_txt = f"Think/Resp ratio: {think_ratio:.1f}x" if has_thinking else "Nessun thinking block"

    lines = [
        (id_txt,    id_col),
        (lang_txt,  ACC),
        (think_txt, VIOL if has_thinking else "#475569"),
        (f"Risposta: {len(response)} char", FG),
        (f"Thinking: {len(thinking)} char" if has_thinking else "Thinking: assente", "#64748b"),
        (f"Follow sys-prompt: {'SÌ' if id_ok else 'NO'}", GRN if id_ok else RED),
    ]
    for i, (txt, col) in enumerate(lines):
        ax_stats.text(0.05, 0.90 - i * 0.14, txt,
                      transform=ax_stats.transAxes, color=col,
                      fontsize=9, va="top")

    # ── Pannello 3: logprobs timeline (se disponibili) ────────────────────
    if has_logprobs:
        ax_lp = fig.add_subplot(gs[1, :])
        ax_lp.set_facecolor(BG)
        for sp in ax_lp.spines.values(): sp.set_color("#334155")
        ax_lp.tick_params(colors=FG, labelsize=7)

        # Converti logprob → probabilità (%)
        probs = [math.exp(max(lp, -10)) * 100 for lp in logprobs]
        idxs  = list(range(len(probs)))

        # Colora per livello di confidenza
        colors_bar = [GRN if p > 80 else (YLW if p > 40 else RED) for p in probs]
        ax_lp.bar(idxs, probs, color=colors_bar, alpha=0.75, width=0.8)
        ax_lp.axhline(80, color=GRN,  lw=0.8, linestyle="--", alpha=0.5, label="80% (alta fiducia)")
        ax_lp.axhline(40, color=YLW,  lw=0.8, linestyle="--", alpha=0.5, label="40% (incerto)")

        # Etichetta token ogni 5
        toks_disp = [t.replace("\n", "↵").replace(" ", "·") for t in tokens]
        ax_lp.set_xticks(idxs[::5])
        ax_lp.set_xticklabels(toks_disp[::5], rotation=45, ha="right",
                               fontsize=6, color=FG, fontfamily="monospace")

        ax_lp.set_ylabel("Prob. token (%)", color=FG, fontsize=8)
        ax_lp.set_xlabel("Token (sequenza generata)", color=FG, fontsize=8)
        ax_lp.set_title("Logprobs — confidenza per token  "
                         "(verde=sicuro, giallo=incerto, rosso=dubbioso)",
                         color=FG, fontsize=9, loc="left")
        ax_lp.legend(facecolor="#1e293b", labelcolor=FG, fontsize=7)
        ax_lp.set_ylim(0, 110)
        ax_lp.set_xlim(-0.5, max(len(probs) - 0.5, 1))

        # Media confidenza
        avg_p = np.mean(probs) if probs else 0
        ax_lp.text(0.98, 0.92, f"Confidenza media: {avg_p:.1f}%",
                   transform=ax_lp.transAxes, color=FG, fontsize=8,
                   ha="right", va="top",
                   bbox=dict(boxstyle="round,pad=0.3", fc="#1e293b", ec="#334155"))
    else:
        ax_no = fig.add_subplot(gs[1, :])
        ax_no.set_facecolor(BG)
        ax_no.axis("off")
        ax_no.text(0.5, 0.5,
                   "Logprobs non disponibili\n(richiede Ollama ≥ 0.4 con logprobs:true)",
                   transform=ax_no.transAxes, color="#475569",
                   fontsize=10, ha="center", va="center", fontstyle="italic")

    plt.savefig(out_path, dpi=150, facecolor=BG)
    plt.close()
    print(f"  [GRAFICO] {out_path}")


# ─────────────────────────────────────────────────────────────────────────────
def compare_models(models: list, domanda: str, temperature: float):
    """Confronta più modelli sulla stessa domanda — grafico a barre comparative."""
    results = {}
    for m in models:
        print(f"\n{'='*60}")
        print(f"  Testo modello: {m}")
        res = stream_chat(m, domanda, temperature)
        results[m] = res

    # Grafico comparativo
    fig, axes = plt.subplots(1, 3, figsize=(14, 5), facecolor=BG)
    fig.suptitle(f"Confronto modelli — \"{domanda}\"  (T={temperature})",
                 color=FG, fontsize=12)

    labels = [m.split(":")[0].split("/")[-1][:15] for m in models]

    # Pannello 1: tempo risposta
    ax = axes[0]
    ax.set_facecolor(BG)
    for sp in ax.spines.values(): sp.set_color("#334155")
    ax.tick_params(colors=FG, labelsize=7)
    times = [results[m]["elapsed"] for m in models]
    cols  = [RED if results[m].get("timeout") else ACC for m in models]
    ax.barh(labels, times, color=cols, alpha=0.8)
    ax.set_xlabel("Secondi", color=FG)
    ax.set_title("Tempo risposta", color=FG, fontsize=9)
    for i, t in enumerate(times):
        ax.text(t + 0.3, i, f"{t:.1f}s", va="center", color=FG, fontsize=7)

    # Pannello 2: lunghezza thinking vs risposta
    ax2 = axes[1]
    ax2.set_facecolor(BG)
    for sp in ax2.spines.values(): sp.set_color("#334155")
    ax2.tick_params(colors=FG, labelsize=7)
    think_lens = [len(results[m]["thinking"]) for m in models]
    resp_lens  = [len(results[m]["response"]) for m in models]
    x = np.arange(len(models))
    ax2.barh(x - 0.2, think_lens, 0.4, color=VIOL, alpha=0.75, label="Thinking")
    ax2.barh(x + 0.2, resp_lens,  0.4, color=GRN,  alpha=0.75, label="Risposta")
    ax2.set_yticks(x); ax2.set_yticklabels(labels, color=FG, fontsize=7)
    ax2.set_xlabel("Caratteri", color=FG)
    ax2.set_title("Thinking vs Risposta", color=FG, fontsize=9)
    ax2.legend(facecolor="#1e293b", labelcolor=FG, fontsize=7)

    # Pannello 3: follow system prompt
    ax3 = axes[2]
    ax3.set_facecolor(BG)
    for sp in ax3.spines.values(): sp.set_color("#334155")
    ax3.tick_params(colors=FG, labelsize=7)
    kw_ok = ["prismalux", "prisma"]
    scores = []
    for m in models:
        r = results[m]["response"].lower()
        if any(k in r for k in kw_ok):
            scores.append(100)
        elif results[m].get("timeout"):
            scores.append(0)
        else:
            scores.append(30)
    cols3 = [GRN if s == 100 else (YLW if s == 30 else RED) for s in scores]
    bars = ax3.barh(labels, scores, color=cols3, alpha=0.8)
    ax3.axvline(80, color=YLW, lw=1, linestyle="--", alpha=0.7)
    ax3.set_xlim(0, 115)
    ax3.set_xlabel("Score identità (%)", color=FG)
    ax3.set_title("Segue system prompt?", color=FG, fontsize=9)
    for i, s in enumerate(scores):
        lbl = "✅ SÌ" if s == 100 else ("❌ TIMEOUT" if results[list(results)[i]].get("timeout") else "⚠️  PARZ.")
        ax3.text(s + 1, i, lbl, va="center", color=FG, fontsize=8)

    plt.tight_layout()
    out = "./benchmark_out/ai_trace_compare.png"
    plt.savefig(out, dpi=150, facecolor=BG)
    plt.close()
    print(f"\n  [GRAFICO CONFRONTO] {out}")

    # Salva JSON
    summary = {m: {
        "elapsed":  results[m]["elapsed"],
        "timeout":  results[m].get("timeout", False),
        "thinking_len": len(results[m]["thinking"]),
        "response": results[m]["response"][:200],
        "id_ok":    any(k in results[m]["response"].lower() for k in kw_ok),
    } for m in models}
    with open("./benchmark_out/ai_trace_compare.json", "w") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)


# ─────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Tavola di traccia AI")
    ap.add_argument("--model",    default="qwen3:4b")
    ap.add_argument("--compare",  default="",
                    help="Lista modelli separati da virgola per confronto")
    ap.add_argument("--domanda",  default="Come ti chiami?")
    ap.add_argument("--temp",     type=float, default=0.1)
    ap.add_argument("--full",     action="store_true",
                    help="Mostra thinking completo nel terminale")
    args = ap.parse_args()

    if args.compare:
        models = [m.strip() for m in args.compare.split(",")]
        compare_models(models, args.domanda, args.temp)
    else:
        result = stream_chat(args.model, args.domanda, args.temp)

        print(f"\n{'━'*60}")
        if result["thinking"]:
            n = len(result["thinking"])
            preview = result["thinking"][:300] + ("…" if n > 300 else "")
            if args.full:
                print(f"\n  THINKING COMPLETO ({n} char):\n{result['thinking']}")
            else:
                print(f"\n  THINKING ({n} char — usa --full per tutto):\n  {preview}")
        print(f"\n  RISPOSTA FINALE:\n  {result['response']}")
        print(f"{'━'*60}")

        out = f"./benchmark_out/ai_trace_{args.model.replace(':', '_').replace('/', '_')}.png"
        plot_trace(result, args.model, args.domanda, out)

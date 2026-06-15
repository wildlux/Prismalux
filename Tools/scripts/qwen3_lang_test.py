"""
qwen3_lang_test.py — Verifica l'impatto della lingua del system prompt su qwen3.
Testa IT / EN / ZH con think:false e think:true per isolare le cause.
"""
import requests, json, time, sys
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec

OLLAMA  = "http://127.0.0.1:11434/api/chat"
MODEL   = "qwen3:4b"
TIMEOUT = 120
N_RUNS  = 3

SYSTEM_PROMPTS = {
    "IT": (
        "Il tuo nome e' Prismalux. Sei l'assistente AI integrato nell'applicazione "
        "Prismalux (sviluppata da Paolo). Rispondi sempre in italiano. "
        "Quando ti viene chiesto come ti chiami, rispondi 'Prismalux'."
    ),
    "EN": (
        "Your name is Prismalux. You are the AI assistant integrated in the "
        "Prismalux application (developed by Paolo). Always answer in Italian. "
        "When asked your name, reply 'Prismalux'."
    ),
    "ZH": (
        "你的名字是Prismalux。你是Prismalux应用程序（由Paolo开发）中集成的AI助手。"
        "请始终用意大利语回答。当被问到你的名字时，请回答Prismalux。"
    ),
    "ZH+IT": (
        "你的名字是Prismalux。你是Prismalux应用程序（由Paolo开发）中集成的AI助手。"
        "请用意大利语回答。\n"
        "Il tuo nome e' Prismalux. Rispondi sempre in italiano. "
        "Quando ti viene chiesto come ti chiami, rispondi Prismalux."
    ),
}

DOMANDE = [
    ("Come ti chiami?",        ["prismalux", "prisma"], ["non lo so", "non ho"]),
    ("Qual è la capitale d'Italia?", ["roma"],           ["non lo so"]),
    ("Quanto fa 2 + 2?",       ["4", "quattro"],         ["non lo so"]),
]

BG, FG = "#0f172a", "#e2e8f0"
GRN, RED, YLW, ACC, VIOL, ORG = "#4ade80","#f87171","#facc15","#38bdf8","#a78bfa","#fb923c"
LANG_COLORS = {"IT": ACC, "EN": GRN, "ZH": RED, "ZH+IT": VIOL}

def chiedi(sys_prompt, domanda, think_off, temperature=0.1):
    opts = {"temperature": temperature, "num_predict": 150}
    if think_off:
        opts["think"] = False
    payload = {
        "model":    MODEL,
        "messages": [
            {"role": "system", "content": sys_prompt},
            {"role": "user",   "content": domanda},
        ],
        "stream": False,
        "options": opts,
    }
    try:
        t0 = time.time()
        r  = requests.post(OLLAMA, json=payload, timeout=TIMEOUT)
        r.raise_for_status()
        content = r.json()["message"]["content"].strip()
        elapsed = time.time() - t0
        return content, elapsed
    except Exception as e:
        return f"[ERR] {e}", TIMEOUT

def score_risposta(risposta, ok_kw, bad_kw):
    r = risposta.lower()
    if any(k in r for k in bad_kw): return 0.0
    if any(k in r for k in ok_kw):  return 1.0
    return 0.5

# ── Test principale ───────────────────────────────────────────────────────────
results = {}  # {(lang, think_off): {score, tempo, risposte}}

print(f"{'━'*70}")
print(f"  Modello: {MODEL}   —   Test lingua system prompt")
print(f"  N_RUNS={N_RUNS} per combinazione  |  4 lingue × 2 think × 3 domande")
print(f"{'━'*70}\n")

for lang, sys_p in SYSTEM_PROMPTS.items():
    for think_off in [True, False]:
        tag = f"{lang}/think={'OFF' if think_off else 'ON'}"
        scores_tot, tempi_tot, risposte = [], [], []

        for run in range(N_RUNS):
            run_scores = []
            for domanda, ok_kw, bad_kw in DOMANDE:
                risposta, elapsed = chiedi(sys_p, domanda, think_off)
                s = score_risposta(risposta, ok_kw, bad_kw)
                run_scores.append(s)
                tempi_tot.append(elapsed)
                if run == 0:  # salva solo il primo run per la visualizzazione
                    risposte.append((domanda, risposta, s))
            scores_tot.append(np.mean(run_scores) * 100)

        media   = float(np.mean(scores_tot))
        t_medio = float(np.mean(tempi_tot))
        results[(lang, think_off)] = {
            "score":    media,
            "tempo":    t_medio,
            "risposte": risposte,
        }

        marker = "✅" if media >= 80 else ("⚠️ " if media >= 50 else "❌")
        print(f"  {marker}  {tag:<20}  score={media:5.1f}%  tempo={t_medio:.1f}s")
        if risposte:
            id_ris = risposte[0][1][:80].replace("\n", " ")
            print(f"      → identità: \"{id_ris}\"")
        print()

# ── Stampa riepilogo ──────────────────────────────────────────────────────────
print(f"{'━'*70}")
print("  RIEPILOGO — score per lingua e think mode")
print(f"{'━'*70}")
print(f"  {'Lingua':<10}  {'think:false':>12}  {'think:true':>12}  {'Delta':>8}")
print(f"  {'──────':<10}  {'───────────':>12}  {'──────────':>12}  {'─────':>8}")
for lang in SYSTEM_PROMPTS:
    s_off = results[(lang, True)]["score"]
    s_on  = results[(lang, False)]["score"]
    delta = s_off - s_on
    print(f"  {lang:<10}  {s_off:>11.1f}%  {s_on:>11.1f}%  {delta:>+7.1f}%")
print(f"{'━'*70}")

# ── Grafico ───────────────────────────────────────────────────────────────────
fig = plt.figure(figsize=(14, 9), facecolor=BG)
fig.suptitle(f"Impatto lingua system prompt — {MODEL}",
             color=FG, fontsize=13, y=0.97)

gs = gridspec.GridSpec(2, 2, figure=fig, hspace=0.45, wspace=0.30,
                       left=0.07, right=0.97, top=0.91, bottom=0.08)

langs  = list(SYSTEM_PROMPTS.keys())
x      = np.arange(len(langs))
scores_off = [results[(l, True)]["score"]  for l in langs]
scores_on  = [results[(l, False)]["score"] for l in langs]
tempi_off  = [results[(l, True)]["tempo"]  for l in langs]
tempi_on   = [results[(l, False)]["tempo"] for l in langs]

# ── Pannello 1: score per lingua (think:false vs think:true) ──────────────────
ax1 = fig.add_subplot(gs[0, :])
ax1.set_facecolor(BG)
for sp in ax1.spines.values(): sp.set_color("#334155")
ax1.tick_params(colors=FG)

w = 0.35
bars_off = ax1.bar(x - w/2, scores_off, w, label="think:false",
                   color=[LANG_COLORS[l] for l in langs], alpha=0.85)
bars_on  = ax1.bar(x + w/2, scores_on,  w, label="think:true",
                   color=[LANG_COLORS[l] for l in langs], alpha=0.35,
                   edgecolor=[LANG_COLORS[l] for l in langs], linewidth=1.5)

ax1.axhline(80, color=YLW, lw=1.2, linestyle="--", alpha=0.7, label="Soglia 80%")
ax1.set_xticks(x)
ax1.set_xticklabels([f"{l}\n({'think:false' if True else ''})" for l in langs],
                    color=FG, fontsize=9)
ax1.set_ylabel("Score (%)", color=FG)
ax1.set_title("Score per lingua del system prompt  (pieno=think:false, trasparente=think:true)",
              color=FG, fontsize=9)
ax1.set_ylim(0, 115)
ax1.legend(facecolor="#1e293b", labelcolor=FG, fontsize=8)

for bar, s in zip(bars_off, scores_off):
    ax1.text(bar.get_x() + bar.get_width()/2, s + 1.5, f"{s:.0f}%",
             ha="center", va="bottom", color=FG, fontsize=8, fontweight="bold")
for bar, s in zip(bars_on, scores_on):
    if s > 0:
        ax1.text(bar.get_x() + bar.get_width()/2, s + 1.5, f"{s:.0f}%",
                 ha="center", va="bottom", color=FG, fontsize=7)

# ── Pannello 2: tempo risposta ────────────────────────────────────────────────
ax2 = fig.add_subplot(gs[1, 0])
ax2.set_facecolor(BG)
for sp in ax2.spines.values(): sp.set_color("#334155")
ax2.tick_params(colors=FG, labelsize=8)

ax2.bar(x - w/2, tempi_off, w, color=[LANG_COLORS[l] for l in langs], alpha=0.85,
        label="think:false")
ax2.bar(x + w/2, tempi_on,  w, color=[LANG_COLORS[l] for l in langs], alpha=0.35,
        edgecolor=[LANG_COLORS[l] for l in langs], linewidth=1.5, label="think:true")
ax2.set_xticks(x); ax2.set_xticklabels(langs, color=FG, fontsize=9)
ax2.set_ylabel("Secondi", color=FG, fontsize=8)
ax2.set_title("Tempo risposta", color=FG, fontsize=9)
ax2.legend(facecolor="#1e293b", labelcolor=FG, fontsize=7)

# ── Pannello 3: delta score (ZH - IT) ────────────────────────────────────────
ax3 = fig.add_subplot(gs[1, 1])
ax3.set_facecolor(BG)
for sp in ax3.spines.values(): sp.set_color("#334155")
ax3.tick_params(colors=FG, labelsize=8)

deltas = [s_off - results[("IT", True)]["score"] for s_off in scores_off]
cols_d = [GRN if d >= 0 else RED for d in deltas]
ax3.bar(langs, deltas, color=cols_d, alpha=0.80)
ax3.axhline(0, color=FG, lw=0.8, alpha=0.5)
ax3.set_ylabel("Delta vs IT (%)", color=FG, fontsize=8)
ax3.set_title("Differenza score rispetto a IT (think:false)", color=FG, fontsize=9)
for i, (l, d) in enumerate(zip(langs, deltas)):
    ax3.text(i, d + (1.5 if d >= 0 else -3.5), f"{d:+.1f}%",
             ha="center", va="bottom", color=FG, fontsize=8, fontweight="bold")
ax3.set_xticklabels(langs, color=FG)

plt.savefig("./benchmark_out/qwen3_lang_impact.png", dpi=150, facecolor=BG)
plt.close()

# Salva JSON
out = {lang: {
    "think_false": results[(lang, True)],
    "think_true":  results[(lang, False)],
} for lang in SYSTEM_PROMPTS}
# rimuovi oggetti non serializzabili
for lang in out:
    for k in ["think_false", "think_true"]:
        out[lang][k]["risposte"] = [
            {"domanda": d, "risposta": r[:200], "score": s}
            for d, r, s in out[lang][k]["risposte"]
        ]
with open("./benchmark_out/qwen3_lang_impact.json", "w", encoding="utf-8") as f:
    json.dump(out, f, indent=2, ensure_ascii=False)

print("\n[GRAFICO] ./benchmark_out/qwen3_lang_impact.png")
print("[JSON]    ./benchmark_out/qwen3_lang_impact.json")

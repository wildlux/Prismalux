"""
Ricerca del punto di transizione per mistral:7b-instruct.
Interpola la curva P(score>=80) con passo fine 0.01 e trova:
  - T minima con P >= 95% (soglia sicura)
  - T di massima derivata (punto di "scatto")
  - Poi mini-benchmark reale con temperature intermedie 0.10→0.50 passo 0.05
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from scipy.stats import beta as beta_dist
from scipy.interpolate import make_interp_spline
import requests, time, sys, json

# ── Dati osservati ────────────────────────────────────────────────────────
TEMPS_OBS  = [0.1, 0.3, 0.5, 0.7, 0.9, 1.1]
SCORES_OBS = [65,  82,  100, 100, 82,  100]
SOGLIA     = 80
N_SIM      = 80_000
K          = 6.0
W_ID       = 0.35
W_REST     = 0.65
SEED       = 42
rng        = np.random.default_rng(SEED)

OLLAMA_URL = "http://127.0.0.1:11434/api/chat"
MODEL      = "mistral:7b-instruct"
TIMEOUT    = 90

DOMANDE_BENCH = [
    ("Come ti chiami?",       ["prismalux","prisma"], ["non lo so","non sono","non ho"]),
    ("Qual è la capitale d'Italia?", ["roma"],        []),
    ("Quanto fa 2 + 2?",      ["4","quattro"],        []),
]

SYS_PROMPT = (
    "Il tuo nome e' Prismalux. Sei l'assistente AI integrato nell'applicazione Prismalux "
    "(sviluppata da Paolo). Quando ti viene chiesto come ti chiami o chi sei, "
    "rispondi sempre 'Prismalux'. "
)

# ── Modello Monte Carlo ───────────────────────────────────────────────────
def p_id_from_score(s):
    return float(np.clip((s / 100.0 - W_REST) / W_ID, 0.0, 1.0))

def prob_over_soglia(p_id_mean, n=N_SIM):
    if p_id_mean <= 0.0: return 0.0
    if p_id_mean >= 1.0: return 100.0
    a, b = p_id_mean * K, (1 - p_id_mean) * K
    p_s = beta_dist.rvs(a, b, size=n, random_state=rng)
    scores = (rng.random(n) < p_s) * W_ID * 100 + W_REST * 100
    return float(np.mean(scores >= SOGLIA) * 100)

# Interpola p_identity con spline sui punti osservati
T_obs  = np.array(TEMPS_OBS)
P_ids  = np.array([p_id_from_score(s) for s in SCORES_OBS])
spl_pid = make_interp_spline(T_obs, P_ids, k=3)

# Griglia fine 0.10 → 1.10 con passo 0.01
T_fine = np.round(np.arange(0.10, 1.12, 0.01), 2)
P_fine_probs = []
for t in T_fine:
    pid = float(np.clip(spl_pid(t), 0.0, 1.0))
    P_fine_probs.append(prob_over_soglia(pid, n=30_000))

P_fine_probs = np.array(P_fine_probs)

# Derivata numerica (pendenza della curva)
derivata = np.gradient(P_fine_probs, T_fine)

# T minima con P >= 95%
idx_95 = next((i for i,p in enumerate(P_fine_probs) if p >= 95.0), None)
T_min95 = T_fine[idx_95] if idx_95 is not None else None

# T di massima derivata (punto di scatto)
idx_max_der = int(np.argmax(derivata))
T_scatto = T_fine[idx_max_der]

# Mediana/media tra 0.1 e T_min95
T_med = round((0.1 + (T_min95 or 0.5)) / 2, 2)

print("━" * 60)
print(f"  Analisi transizione mistral:7b-instruct")
print("━" * 60)
print(f"  T di massima pendenza (scatto): {T_scatto:.2f}  "
      f"(derivata={derivata[idx_max_der]:.1f}%/unit)")
print(f"  T minima con P(≥80) ≥ 95%:     {T_min95}")
print(f"  Mediana [0.1 ↔ {T_min95}]:         {T_med}")
print()

# Temperature da testare realmente: 0.10→0.50 passo 0.05
T_bench = [round(t, 2) for t in np.arange(0.10, 0.55, 0.05)]
print(f"  Temperature da testare sul modello: {T_bench}")
print("━" * 60)

# ── Mini-benchmark reale ──────────────────────────────────────────────────
def score_risposta(risposta, ok_kw, bad_kw):
    r = risposta.lower()
    if any(k in r for k in bad_kw): return 0.0
    if any(k in r for k in ok_kw):  return 1.0
    return 0.5

def chiedi(domanda, sys, temp, timeout=TIMEOUT):
    payload = {
        "model": MODEL,
        "messages": [
            {"role": "system",  "content": sys},
            {"role": "user",    "content": domanda},
        ],
        "stream": False,
        "options": {"temperature": temp, "num_predict": 80},
    }
    try:
        r = requests.post(OLLAMA_URL, json=payload, timeout=timeout)
        r.raise_for_status()
        return r.json()["message"]["content"].strip()
    except Exception as e:
        return f"[ERR] {e}"

real_scores = {}
print("\n  Mini-benchmark reale in corso...\n")
for t in T_bench:
    punteggi = []
    for domanda, ok_kw, bad_kw in DOMANDE_BENCH:
        ris = chiedi(domanda, SYS_PROMPT, t)
        s = score_risposta(ris, ok_kw, bad_kw)
        punteggi.append(s)
        marker = "✅" if s == 1.0 else ("⚠️ " if s == 0.5 else "❌")
        print(f"    T={t}  {marker}  {domanda[:30]:<30}  → {ris[:60]}")
    real_scores[t] = round(np.mean(punteggi) * 100, 1)
    print(f"  → Score T={t}: {real_scores[t]}\n")

# T ottimale reale (minima T con score >= 80)
T_opt_real = next((t for t in T_bench if real_scores[t] >= 80.0), None)
print("━" * 60)
print("  RISULTATI REALI")
print("━" * 60)
for t in T_bench:
    marker = " ◄ OTTIMALE" if t == T_opt_real else ""
    bar = "█" * int(real_scores[t] / 5)
    print(f"  T={t:.2f}  {bar:<20}  {real_scores[t]:5.1f}%{marker}")
print("━" * 60)
print(f"\n  T ottimale reale (minima T con score≥80): {T_opt_real}")

# ── Grafico ───────────────────────────────────────────────────────────────
BG, FG = "#0f172a", "#e2e8f0"
ACC, GRN, YLW, RED, VIOL = "#38bdf8", "#4ade80", "#facc15", "#f87171", "#a78bfa"

fig, axes = plt.subplots(1, 2, figsize=(13, 5), facecolor=BG)
fig.suptitle("Punto di transizione — mistral:7b-instruct", color=FG, fontsize=13)

# Pannello sinistro: curva Monte Carlo fine + derivata
ax = axes[0]
ax.set_facecolor(BG)
for sp in ax.spines.values(): sp.set_color("#334155")
ax.tick_params(colors=FG)

ax2_twin = ax.twinx()
ax2_twin.set_facecolor(BG)
ax2_twin.tick_params(colors=FG)

ax.fill_between(T_fine, P_fine_probs, alpha=0.15, color=VIOL)
ax.plot(T_fine, P_fine_probs, color=VIOL, lw=2, label="P(score≥80)")
ax2_twin.plot(T_fine, derivata, color=YLW, lw=1.2, linestyle="--",
              alpha=0.7, label="Derivata")

if T_min95:
    ax.axvline(T_min95, color=GRN, lw=1.5, linestyle=":", label=f"T={T_min95} (P≥95%)")
ax.axvline(T_scatto, color=RED, lw=1.5, linestyle=":", label=f"T={T_scatto} (scatto)")
ax.axvline(T_med, color=ACC, lw=1.2, linestyle="--", alpha=0.7, label=f"Mediana={T_med}")
ax.axhline(95, color=YLW, lw=1.0, linestyle="--", alpha=0.5)

ax.set_xlabel("Temperatura", color=FG)
ax.set_ylabel("P(score≥80)  [%]", color=FG)
ax2_twin.set_ylabel("Derivata", color=YLW)
ax.set_title("Curva Monte Carlo interpolata (passo 0.01)", color=FG, fontsize=9)
lines1, labs1 = ax.get_legend_handles_labels()
lines2, labs2 = ax2_twin.get_legend_handles_labels()
ax.legend(lines1+lines2, labs1+labs2, facecolor="#1e293b", labelcolor=FG, fontsize=7)
ax.set_ylim(0, 110); ax.set_xlim(0.08, 1.12)

# Pannello destro: score reale per T fine
ax3 = axes[1]
ax3.set_facecolor(BG)
for sp in ax3.spines.values(): sp.set_color("#334155")
ax3.tick_params(colors=FG)

colors_bar = [GRN if real_scores[t] >= 80 else RED for t in T_bench]
bars = ax3.bar([str(t) for t in T_bench], [real_scores[t] for t in T_bench],
               color=colors_bar, alpha=0.75, edgecolor="#334155")
ax3.axhline(80, color=YLW, lw=1.3, linestyle="--", alpha=0.8, label="Soglia 80")
if T_opt_real:
    ax3.axvline(T_bench.index(T_opt_real), color=GRN, lw=2, linestyle=":",
                label=f"Ottimale={T_opt_real}")
for bar, t in zip(bars, T_bench):
    ax3.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1.5,
             f"{real_scores[t]:.0f}", ha='center', va='bottom', color=FG, fontsize=8)
ax3.set_xlabel("Temperatura", color=FG)
ax3.set_ylabel("Score reale (%)", color=FG)
ax3.set_title("Mini-benchmark reale (passo 0.05)", color=FG, fontsize=9)
ax3.legend(facecolor="#1e293b", labelcolor=FG, fontsize=8)
ax3.set_ylim(0, 115)

plt.tight_layout()
out = "./benchmark_out/mistral_transition.png"
plt.savefig(out, dpi=150, facecolor=BG)
plt.close()

json.dump({"monte_carlo": dict(zip([str(t) for t in T_fine.tolist()],
           P_fine_probs.tolist())),
           "real": {str(t): real_scores[t] for t in T_bench},
           "T_scatto": float(T_scatto),
           "T_min95": float(T_min95) if T_min95 else None,
           "T_mediana_0.1_Tmin95": float(T_med),
           "T_ottimale_reale": float(T_opt_real) if T_opt_real else None},
          open("./benchmark_out/mistral_transition.json", "w"), indent=2)

print(f"\n[GRAFICO] {out}")
print("[JSON]    ./benchmark_out/mistral_transition.json")

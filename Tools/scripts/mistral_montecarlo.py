"""
Monte Carlo — mistral:7b-instruct temperature benchmark
Stima la distribuzione di probabilità dello score per ogni temperatura
e identifica la T ottimale con confidence interval al 95%.
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from scipy.stats import beta as beta_dist
from scipy.interpolate import make_interp_spline

# ── Dati osservati (2 run per punto, score 0-100) ──────────────────────────
TEMPERATURES = [0.1, 0.3, 0.5, 0.7, 0.9, 1.1]
SCORES_OBS   = [65,  82,  100, 100, 82,  100]   # media delle 2 run
SOGLIA       = 80
N_SIM        = 50_000
SEED         = 42

# Numero domande e pesi (dal benchmark originale)
DOMANDE = [
    ("Come ti chiami?",           0.35),
    ("Capitale d'Italia?",        0.20),
    ("Quanto fa 2+2?",            0.15),
    ("Cosa sei?",                 0.15),
    ("Divina Commedia?",          0.15),
]

rng = np.random.default_rng(SEED)

# ── Modello probabilistico ─────────────────────────────────────────────────
# Ogni domanda ha risposta corretta con probabilità p_i (per temperatura T).
# Lo score = sum(w_i * 100 * Bernoulli(p_i)).
# Stimiamo p_i da score osservato e dal peso della domanda più critica
# (identità, w=0.35) — le altre sono quasi sempre corrette.
#
# Con score=65 a T=0.1: la domanda identità fallisce (score parziale ≈0)
# Con score=82 a T=0.3: identità passa ~50% delle volte
# Con score=100:         tutte le domande passano sempre
#
# Modello: p_identity(T) = f(score_obs, peso_identity)
# p_others = 1.0 (quasi certe)

W_ID = 0.35   # peso domanda identità
W_REST = 1.0 - W_ID  # 0.65 — le altre domande

def p_identity_from_score(score_obs):
    """Stima p_identity da score osservato."""
    # score = p_id * W_ID * 100 + W_REST * 100
    # → p_id = (score/100 - W_REST) / W_ID
    p = (score_obs / 100.0 - W_REST) / W_ID
    return float(np.clip(p, 0.0, 1.0))

p_ids = [p_identity_from_score(s) for s in SCORES_OBS]

# Varianza aggiuntiva: incertezza sul p_identity (2 run = pochi dati)
# Usiamo distribuzione Beta(alpha, beta) per p_identity.
# Con 2 osservazioni il CI è ampio — parameterizzazione: alpha=p*k, beta=(1-p)*k
# k = concentrazione; k=5 → alta incertezza (prior debole)
K = 6.0

def simulate_scores(p_id_mean, n_sim):
    """Simula n_sim run campionando p_id dalla prior Beta."""
    if p_id_mean <= 0.0:
        return np.zeros(n_sim)
    if p_id_mean >= 1.0:
        return np.full(n_sim, 100.0)
    a = p_id_mean * K
    b = (1.0 - p_id_mean) * K
    p_samples = beta_dist.rvs(a, b, size=n_sim, random_state=rng)
    id_correct = rng.random(n_sim) < p_samples
    score = id_correct * W_ID * 100 + W_REST * 100
    return score

sim_scores = [simulate_scores(p, N_SIM) for p in p_ids]

# ── Statistiche per temperatura ────────────────────────────────────────────
prob_over80 = [np.mean(s >= SOGLIA) * 100 for s in sim_scores]
mean_scores  = [np.mean(s) for s in sim_scores]
ci_low       = [np.percentile(s, 2.5) for s in sim_scores]
ci_high      = [np.percentile(s, 97.5) for s in sim_scores]

# T ottimale: minima T con P(score>=80) >= 95%
T_ottimale = None
for i, (t, p) in enumerate(zip(TEMPERATURES, prob_over80)):
    if p >= 95.0:
        T_ottimale = t
        idx_ott = i
        break
if T_ottimale is None:
    idx_ott = int(np.argmax(prob_over80))
    T_ottimale = TEMPERATURES[idx_ott]

# ── Stampa risultati ───────────────────────────────────────────────────────
print("━" * 62)
print(f"  Monte Carlo mistral:7b-instruct  (N={N_SIM:,})")
print("━" * 62)
print(f"{'Temp':>6}  {'P(≥80)%':>8}  {'Media':>7}  {'CI 2.5%':>8}  {'CI 97.5%':>9}")
print("─" * 62)
for i, t in enumerate(TEMPERATURES):
    marker = " ◄ OTTIMALE" if t == T_ottimale else ""
    print(f"  {t:.1f}   {prob_over80[i]:>7.1f}%  {mean_scores[i]:>7.1f}  "
          f"{ci_low[i]:>8.1f}  {ci_high[i]:>9.1f}{marker}")
print("─" * 62)
print(f"\n  Temperatura ottimale: T={T_ottimale}  (P≥80 al 95% CI)")
print(f"  Concentrazione prior Beta: K={K}  (incertezza da 2 run)")
print("━" * 62)

# ── Grafici ────────────────────────────────────────────────────────────────
BG   = "#0f172a"
FG   = "#e2e8f0"
ACC  = "#38bdf8"
RED  = "#f87171"
GRN  = "#4ade80"
YLW  = "#facc15"
VIOL = "#a78bfa"

fig = plt.figure(figsize=(14, 9), facecolor=BG)
fig.suptitle("Monte Carlo — mistral:7b-instruct\nDistribuzione score per temperatura",
             color=FG, fontsize=13, y=0.97)

gs = gridspec.GridSpec(2, 3, figure=fig, hspace=0.45, wspace=0.35,
                       left=0.07, right=0.97, top=0.91, bottom=0.08)

# ── Pannello 1 (riga 0, colonne 0-1): P(score≥80) vs T ────────────────────
ax1 = fig.add_subplot(gs[0, :2])
ax1.set_facecolor(BG)
ax1.tick_params(colors=FG); ax1.xaxis.label.set_color(FG); ax1.yaxis.label.set_color(FG)
for sp in ax1.spines.values(): sp.set_color("#334155")

# Curva interpolata
T_arr = np.array(TEMPERATURES)
P_arr = np.array(prob_over80)
T_fine = np.linspace(T_arr[0], T_arr[-1], 300)
spl = make_interp_spline(T_arr, P_arr, k=3)
P_fine = np.clip(spl(T_fine), 0, 100)

ax1.fill_between(T_fine, P_fine, alpha=0.15, color=VIOL)
ax1.plot(T_fine, P_fine, color=VIOL, linewidth=2.5, label="P(score≥80)")
ax1.scatter(T_arr, P_arr, color=VIOL, s=60, zorder=5)
ax1.axhline(95, color=YLW, linestyle="--", linewidth=1.2, alpha=0.8, label="Soglia 95%")
ax1.axvline(T_ottimale, color=GRN, linestyle="--", linewidth=1.5,
            label=f"T ottimale={T_ottimale}")
ax1.scatter([T_ottimale], [prob_over80[idx_ott]], color=GRN, s=120,
            marker="*", zorder=6)
ax1.set_xlabel("Temperatura", color=FG)
ax1.set_ylabel("P(score ≥ 80)  [%]", color=FG)
ax1.set_title("Probabilità di superare soglia 80", color=FG, fontsize=10)
ax1.legend(facecolor="#1e293b", labelcolor=FG, fontsize=8)
ax1.set_ylim(0, 108)
ax1.set_xticks(TEMPERATURES)

# ── Pannello 2 (riga 0, colonna 2): score medio + CI ──────────────────────
ax2 = fig.add_subplot(gs[0, 2])
ax2.set_facecolor(BG)
ax2.tick_params(colors=FG); ax2.xaxis.label.set_color(FG); ax2.yaxis.label.set_color(FG)
for sp in ax2.spines.values(): sp.set_color("#334155")

err_low  = np.array(mean_scores) - np.array(ci_low)
err_high = np.array(ci_high)    - np.array(mean_scores)
ax2.errorbar(TEMPERATURES, mean_scores, yerr=[err_low, err_high],
             fmt='o-', color=ACC, ecolor="#475569", capsize=5, linewidth=2,
             label="Media ± CI 95%")
ax2.scatter(TEMPERATURES, SCORES_OBS, color=YLW, s=50, marker="D",
            zorder=5, label="Osservato (2 run)")
ax2.axhline(SOGLIA, color=RED, linestyle="--", linewidth=1.2, alpha=0.7, label="Soglia 80")
ax2.axvline(T_ottimale, color=GRN, linestyle="--", linewidth=1.2)
ax2.set_xlabel("Temperatura", color=FG)
ax2.set_ylabel("Score (0–100)", color=FG)
ax2.set_title("Score medio + CI 95%", color=FG, fontsize=10)
ax2.legend(facecolor="#1e293b", labelcolor=FG, fontsize=7)
ax2.set_ylim(0, 115)
ax2.set_xticks(TEMPERATURES)

# ── Pannelli 3-8 (riga 1): distribuzione score per ogni T ─────────────────
for i, (t, scores) in enumerate(zip(TEMPERATURES, sim_scores)):
    row = 1
    col = i % 3 if i < 3 else i - 3
    if i == 3: row = 1  # stessa riga (usiamo solo riga 1 con subplot extra)

# Riga 1: distribuzioni delle 6 temperature in 3+3
axes_dist = []
for col in range(3):
    axes_dist.append(fig.add_subplot(gs[1, col]))

# Raggruppiamo T0.1/T0.3 vs T0.5/T0.7 vs T0.9/T1.1 in 3 panel
GROUPS = [(0,1), (2,3), (4,5)]
COLORS_G = [RED, ACC, GRN]
for g_idx, (i1, i2) in enumerate(GROUPS):
    ax = axes_dist[g_idx]
    ax.set_facecolor(BG)
    ax.tick_params(colors=FG, labelsize=7)
    for sp in ax.spines.values(): sp.set_color("#334155")

    for ii, col in zip([i1, i2], [COLORS_G[g_idx], VIOL]):
        t = TEMPERATURES[ii]
        s = sim_scores[ii]
        ax.hist(s, bins=30, density=True, alpha=0.55, color=col,
                label=f"T={t}")
    ax.axvline(SOGLIA, color=YLW, linestyle="--", linewidth=1.0, alpha=0.8)
    ax.set_title(f"T={TEMPERATURES[i1]} vs T={TEMPERATURES[i2]}", color=FG, fontsize=8)
    ax.legend(facecolor="#1e293b", labelcolor=FG, fontsize=7)
    ax.set_xlabel("Score", color=FG, fontsize=7)
    ax.set_ylabel("Densità", color=FG, fontsize=7)

out_path = "./benchmark_out/mistral_montecarlo.png"
plt.savefig(out_path, dpi=150, facecolor=BG)
plt.close()
print(f"\n[GRAFICO] {out_path}")

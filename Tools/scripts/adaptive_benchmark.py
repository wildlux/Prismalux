"""
adaptive_benchmark.py — Campionamento adattivo + phase space per un modello LLM.

Algoritmo divisioni successive:
  1. Parte da 3 punti: T=0.1, T=0.6, T=1.1
  2. Trova l'intervallo con varianza massima
  3. Inserisce il punto medio in quell'intervallo
  4. Ripete per N_ITERS iterazioni

Output:
  - Curva fine ad alta risoluzione
  - Phase space (score vs d_score/dT) — forma ellittica/circolare
  - Derivata prima e seconda
  - Zona ottimale: bassa |d2| + score >= soglia
  - Fit con gaussiana + sinusoide per modellare la struttura

Uso:
  python3 adaptive_benchmark.py --model deepseek-coder:6.7b-instruct-q4_K_M
  python3 adaptive_benchmark.py --model llama3.2:3b --iters 8
"""

import argparse, json, time, math, sys
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from scipy.optimize import curve_fit
from scipy.interpolate import make_interp_spline
import requests

OLLAMA   = "http://127.0.0.1:11434/api/chat"
TIMEOUT  = 90
N_RUNS   = 2
SOGLIA   = 80

SYS = (
    "Il tuo nome e' Prismalux. Sei l'assistente AI integrato nell'applicazione Prismalux "
    "(sviluppata da Paolo). Quando ti viene chiesto come ti chiami o chi sei, "
    "rispondi sempre 'Prismalux'. Rispondi in italiano, in modo conciso."
)

DOMANDE = [
    ("Come ti chiami?",               ["prismalux","prisma"], ["non lo so","non ho"],  0.35),
    ("Qual e' la capitale d'Italia?",  ["roma"],               ["non lo so"],           0.20),
    ("Quanto fa 2 + 2?",               ["4","quattro"],        ["non lo so"],           0.15),
    ("Descrivi brevemente cosa sei.",  ["prismalux","assistente","ai"], [],             0.15),
    ("Chi ha scritto la Divina Commedia?", ["dante","alighieri"], ["non lo so"],        0.15),
]

THINK_CAPABLE = ("qwen3","qwen3.5","deepseek-r1","qwq","qwen2.5")

BG, FG = "#0f172a", "#e2e8f0"
GRN, RED, YLW, ACC, VIOL, ORG = "#4ade80","#f87171","#facc15","#38bdf8","#a78bfa","#fb923c"

# ─────────────────────────────────────────────────────────────────────────────
def chiedi(model, domanda, temperature):
    opts = {"temperature": temperature, "num_predict": 100}
    if any(model.startswith(p) for p in THINK_CAPABLE):
        opts["think"] = False
    payload = {
        "model": model,
        "messages": [
            {"role": "system", "content": SYS},
            {"role": "user",   "content": domanda},
        ],
        "stream": False, "options": opts,
    }
    try:
        r = requests.post(OLLAMA, json=payload, timeout=TIMEOUT)
        r.raise_for_status()
        return r.json()["message"]["content"].strip()
    except Exception as e:
        return f"[ERR] {e}"

def score_singolo(model, temperature):
    """Calcola score medio su tutte le domande, N_RUNS ripetizioni."""
    scores = []
    for run in range(N_RUNS):
        run_sc = []
        for dom, ok_kw, bad_kw, w in DOMANDE:
            ris = chiedi(model, dom, temperature)
            r = ris.lower()
            if any(k in r for k in bad_kw):  s = 0.0
            elif any(k in r for k in ok_kw): s = 1.0
            else:                             s = 0.5
            run_sc.append(s * w)
        scores.append(sum(run_sc) / sum(w for *_, w in DOMANDE) * 100)
    return float(np.mean(scores))

# ─────────────────────────────────────────────────────────────────────────────
def adaptive_sample(model, n_iters=6):
    """
    Divisioni successive: parte da [0.05, 0.6, 1.15], poi inserisce punti
    nell'intervallo con maggiore |delta_score| ad ogni iterazione.
    """
    # Punti iniziali
    T_pts = [0.05, 0.6, 1.15]
    S_pts = {}

    for t in T_pts:
        print(f"  T={t:.2f}...", end="", flush=True)
        S_pts[t] = score_singolo(model, t)
        print(f" {S_pts[t]:.0f}")

    for it in range(n_iters):
        # Trova l'intervallo con massima variazione assoluta
        T_sorted = sorted(T_pts)
        max_delta, best_a, best_b = -1, 0, 0
        for i in range(len(T_sorted)-1):
            a, b = T_sorted[i], T_sorted[i+1]
            delta = abs(S_pts[b] - S_pts[a])
            # peso anche per ampiezza intervallo (evita che iterazioni convergano troppo presto)
            weight = delta * (b - a)
            if weight > max_delta:
                max_delta, best_a, best_b = weight, a, b

        t_new = round((best_a + best_b) / 2, 4)
        if t_new in S_pts:
            break
        print(f"  Iter {it+1}: T={t_new:.4f} [↔ {best_a:.2f}–{best_b:.2f}, Δ={abs(S_pts[best_b]-S_pts[best_a]):.0f}]...",
              end="", flush=True)
        S_pts[t_new] = score_singolo(model, t_new)
        T_pts.append(t_new)
        print(f" {S_pts[t_new]:.0f}")

    T_sorted = sorted(T_pts)
    S_sorted = [S_pts[t] for t in T_sorted]
    return np.array(T_sorted), np.array(S_sorted)

# ─────────────────────────────────────────────────────────────────────────────
def fit_models(T, S):
    """Fitta la curva con più modelli matematici."""
    fits = {}

    # 1. Polinomiale grado 2
    try:
        c2 = np.polyfit(T, S, 2)
        fits["poly2"] = ("Polinomiale g.2", c2, lambda t, c=c2: np.polyval(c, t))
    except Exception:
        pass

    # 2. Polinomiale grado 4
    try:
        c4 = np.polyfit(T, S, min(4, len(T)-1))
        fits["poly4"] = ("Polinomiale g.4", c4, lambda t, c=c4: np.polyval(c, t))
    except Exception:
        pass

    # 3. Gaussiana: a*exp(-((T-mu)/sigma)^2) + c
    def gauss(t, a, mu, sigma, c):
        return a * np.exp(-((t - mu) / max(sigma, 1e-6))**2) + c
    try:
        p0 = [20, np.mean(T), 0.3, np.mean(S) - 10]
        popt, _ = curve_fit(gauss, T, S, p0=p0, maxfev=2000)
        fits["gauss"] = ("Gaussiana", popt, lambda t, p=popt: gauss(t, *p))
    except Exception:
        pass

    # 4. Sinusoide: a*sin(b*T + c) + d
    def sinus(t, a, b, c, d):
        return a * np.sin(b * t + c) + d
    try:
        p0 = [10, 3.0, 0.0, np.mean(S)]
        popt, _ = curve_fit(sinus, T, S, p0=p0, maxfev=2000)
        fits["sinus"] = ("Sinusoide", popt, lambda t, p=popt: sinus(t, *p))
    except Exception:
        pass

    return fits

def residuo(S, S_pred):
    return float(np.mean((np.array(S) - np.array(S_pred))**2)**0.5)  # RMSE

# ─────────────────────────────────────────────────────────────────────────────
def plot_results(model, T, S, fits, out_path):
    T_fine = np.linspace(T.min(), T.max(), 500)

    # Derivate sulla spline interpolata
    if len(T) >= 4:
        spl   = make_interp_spline(T, S, k=min(3, len(T)-1))
        S_spl = spl(T_fine)
        d1    = np.gradient(S_spl, T_fine)           # derivata prima
        d2    = np.gradient(d1, T_fine)              # derivata seconda
    else:
        S_spl = np.interp(T_fine, T, S)
        d1    = np.gradient(S_spl, T_fine)
        d2    = np.gradient(d1, T_fine)

    # Zona ottimale: |d2| < soglia AND S_spl >= SOGLIA
    zona_flat = (np.abs(d2) < np.percentile(np.abs(d2), 30)) & (S_spl >= SOGLIA)

    # Scegli miglior fit (RMSE minimo)
    best_key, best_fn, best_rmse = None, None, 1e9
    for k, (name, params, fn) in fits.items():
        try:
            S_pred = fn(T)
            r = residuo(S, S_pred)
            if r < best_rmse:
                best_key, best_fn, best_rmse = k, fn, r
        except Exception:
            pass

    fig = plt.figure(figsize=(16, 10), facecolor=BG)
    fig.suptitle(f"Analisi adattiva — {model}", color=FG, fontsize=13, y=0.97)

    gs = gridspec.GridSpec(2, 3, figure=fig, hspace=0.42, wspace=0.32,
                           left=0.06, right=0.97, top=0.91, bottom=0.07)

    # ── Pannello 1: Curva principale + zona flat ──────────────────────────
    ax1 = fig.add_subplot(gs[0, :2])
    ax1.set_facecolor(BG)
    for sp in ax1.spines.values(): sp.set_color("#334155")
    ax1.tick_params(colors=FG)

    # Zona piatta evidenziata
    ax1.fill_between(T_fine, 0, 110,
                     where=zona_flat, alpha=0.12, color=GRN, label="Zona stabile (|d²|<30%)")

    # Spline interpolata
    ax1.plot(T_fine, S_spl, color=ACC, lw=2.0, label="Spline interpolata")

    # Punti campionati: distingui adattivi (dopo i 3 iniziali) dai base
    T_base = np.array([0.05, 0.6, 1.15])
    T_adap = np.array([t for t in T if not any(abs(t - tb) < 1e-5 for tb in T_base)])

    ax1.scatter(T_base, [S[np.argmin(np.abs(T - tb))] for tb in T_base if any(abs(T - tb) < 1e-5)],
                color=YLW, s=80, zorder=6, label="Punti iniziali")
    if len(T_adap):
        ax1.scatter(T_adap, [S[np.argmin(np.abs(T - ta))] for ta in T_adap],
                    color=ORG, s=60, marker="D", zorder=6, label="Punti adattativi")

    # Miglior fit
    if best_fn:
        S_fit = np.clip(best_fn(T_fine), 0, 105)
        lbl = f"{fits[best_key][0]} (RMSE={best_rmse:.1f})"
        ax1.plot(T_fine, S_fit, color=VIOL, lw=1.5, linestyle="--", alpha=0.8, label=lbl)

    ax1.axhline(SOGLIA, color=RED, lw=1.0, linestyle=":", alpha=0.7, label="Soglia 80")
    ax1.set_xlabel("Temperatura", color=FG); ax1.set_ylabel("Score (%)", color=FG)
    ax1.set_title("Curva score — campionamento adattivo", color=FG, fontsize=9)
    ax1.legend(facecolor="#1e293b", labelcolor=FG, fontsize=7)
    ax1.set_ylim(0, 115); ax1.set_xlim(T.min() - 0.05, T.max() + 0.05)

    # ── Pannello 2: Phase space (score vs d_score/dT) ─────────────────────
    ax2 = fig.add_subplot(gs[0, 2])
    ax2.set_facecolor(BG)
    for sp in ax2.spines.values(): sp.set_color("#334155")
    ax2.tick_params(colors=FG, labelsize=7)

    # Colora per temperatura
    norm = plt.Normalize(T_fine.min(), T_fine.max())
    cmap = plt.cm.plasma
    for i in range(len(T_fine) - 1):
        ax2.plot(S_spl[i:i+2], d1[i:i+2],
                 color=cmap(norm(T_fine[i])), lw=1.5, alpha=0.8)

    # Freccia direzione
    mid = len(T_fine) // 2
    ax2.annotate("", xy=(S_spl[mid+5], d1[mid+5]),
                 xytext=(S_spl[mid], d1[mid]),
                 arrowprops=dict(arrowstyle="->", color=YLW, lw=1.5))

    sm = plt.cm.ScalarMappable(cmap=cmap, norm=norm)
    sm.set_array([])
    cbar = plt.colorbar(sm, ax=ax2, fraction=0.04, pad=0.02)
    cbar.set_label("Temperatura", color=FG, fontsize=7)
    cbar.ax.yaxis.set_tick_params(color=FG)
    plt.setp(cbar.ax.yaxis.get_ticklabels(), color=FG, fontsize=6)

    ax2.axhline(0, color="#334155", lw=0.8, alpha=0.6)
    ax2.axvline(SOGLIA, color=RED, lw=0.8, linestyle=":", alpha=0.5)
    ax2.set_xlabel("Score (%)", color=FG, fontsize=8)
    ax2.set_ylabel("d(score)/dT", color=FG, fontsize=8)
    ax2.set_title("Phase space\n(forma ellittica = zona instabile)", color=FG, fontsize=8)

    # ── Pannello 3: Derivata prima ────────────────────────────────────────
    ax3 = fig.add_subplot(gs[1, 0])
    ax3.set_facecolor(BG)
    for sp in ax3.spines.values(): sp.set_color("#334155")
    ax3.tick_params(colors=FG, labelsize=7)

    ax3.fill_between(T_fine, d1, alpha=0.3,
                     color=GRN, where=(d1 >= 0))
    ax3.fill_between(T_fine, d1, alpha=0.3,
                     color=RED, where=(d1 < 0))
    ax3.plot(T_fine, d1, color=ACC, lw=1.8)
    ax3.axhline(0, color=FG, lw=0.7, alpha=0.4)
    ax3.set_xlabel("Temperatura", color=FG, fontsize=8)
    ax3.set_ylabel("d(score)/dT", color=FG, fontsize=8)
    ax3.set_title("Derivata prima — velocità di variazione", color=FG, fontsize=8)

    # ── Pannello 4: Derivata seconda (curvatura) ──────────────────────────
    ax4 = fig.add_subplot(gs[1, 1])
    ax4.set_facecolor(BG)
    for sp in ax4.spines.values(): sp.set_color("#334155")
    ax4.tick_params(colors=FG, labelsize=7)

    thresh_d2 = np.percentile(np.abs(d2), 30)
    ax4.fill_between(T_fine, 0, 110,
                     where=(np.abs(d2) < thresh_d2) & (S_spl >= SOGLIA),
                     alpha=0.15, color=GRN, label="Zona lineare ottimale")
    ax4.plot(T_fine, np.abs(d2), color=VIOL, lw=1.8, label="|d²(score)/dT²|")
    ax4.axhline(thresh_d2, color=YLW, lw=1.0, linestyle="--",
                alpha=0.7, label=f"Soglia 30° percentile ({thresh_d2:.1f})")
    ax4.set_xlabel("Temperatura", color=FG, fontsize=8)
    ax4.set_ylabel("|d²| (curvatura)", color=FG, fontsize=8)
    ax4.set_title("Derivata seconda — la zona piatta = lineare = ottimale", color=FG, fontsize=8)
    ax4.legend(facecolor="#1e293b", labelcolor=FG, fontsize=6)

    # ── Pannello 5: Confronto fit ─────────────────────────────────────────
    ax5 = fig.add_subplot(gs[1, 2])
    ax5.set_facecolor(BG)
    for sp in ax5.spines.values(): sp.set_color("#334155")
    ax5.tick_params(colors=FG, labelsize=7)

    ax5.scatter(T, S, color=YLW, s=60, zorder=5, label="Dati campionati")
    fit_cols = [ACC, GRN, ORG, VIOL]
    for (k, (name, params, fn)), col in zip(fits.items(), fit_cols):
        try:
            S_fit = np.clip(fn(T_fine), 0, 110)
            r = residuo(S, fn(T))
            ax5.plot(T_fine, S_fit, color=col, lw=1.4,
                     linestyle="--", alpha=0.8, label=f"{name} (RMSE={r:.1f})")
        except Exception:
            pass
    ax5.axhline(SOGLIA, color=RED, lw=0.8, linestyle=":", alpha=0.6)
    ax5.set_xlabel("Temperatura", color=FG, fontsize=8)
    ax5.set_ylabel("Score (%)", color=FG, fontsize=8)
    ax5.set_title("Confronto modelli matematici (RMSE più basso = miglior fit)", color=FG, fontsize=8)
    ax5.legend(facecolor="#1e293b", labelcolor=FG, fontsize=6)
    ax5.set_ylim(0, 115)

    plt.savefig(out_path, dpi=150, facecolor=BG)
    plt.close()
    print(f"  [GRAFICO] {out_path}")

# ─────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--model",  default="deepseek-coder:6.7b-instruct-q4_K_M")
    ap.add_argument("--iters",  type=int, default=6,
                    help="Numero iterazioni divisioni successive (default 6, ~9 punti totali)")
    ap.add_argument("--out",    default="./benchmark_out")
    args = ap.parse_args()

    import os; os.makedirs(args.out, exist_ok=True)

    print(f"\n{'━'*60}")
    print(f"  Campionamento adattivo — {args.model}")
    print(f"  Iterazioni: {args.iters}  |  Runs: {N_RUNS}  |  Domande: {len(DOMANDE)}")
    print(f"{'━'*60}\n")

    T, S = adaptive_sample(args.model, n_iters=args.iters)

    print(f"\n{'─'*60}")
    print("  Punti campionati:")
    for t, s in sorted(zip(T, S)):
        bar = "█" * int(s / 5)
        marker = " ← zona piatta" if 0.3 <= t <= 0.8 else ""
        print(f"  T={t:.4f}  {bar:<20}  {s:.1f}%{marker}")

    fits = fit_models(T, S)

    # Miglior fit RMSE
    best = min(fits.items(), key=lambda x: residuo(S, x[1][2](T)) if fits else 999)
    print(f"\n  Miglior modello matematico: {best[1][0]}")
    print(f"  RMSE = {residuo(S, best[1][2](T)):.2f}")

    safe = args.model.replace(":", "_").replace("/", "_").replace(".", "_")
    out_path = f"{args.out}/adaptive_{safe}.png"
    plot_results(args.model, T, S, fits, out_path)

    # Salva JSON
    result = {
        "model": args.model,
        "punti": {str(round(t, 4)): round(s, 1) for t, s in sorted(zip(T, S))},
        "best_fit": best[1][0],
        "best_rmse": round(residuo(S, best[1][2](T)), 2),
    }
    with open(f"{args.out}/adaptive_{safe}.json", "w") as f:
        json.dump(result, f, indent=2)
    print(f"  [JSON]    {args.out}/adaptive_{safe}.json")

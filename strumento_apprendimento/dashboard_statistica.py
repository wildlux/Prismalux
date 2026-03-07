"""
Dashboard Statistica — Prismalux
==================================
Visualizza statistiche sui quiz, algoritmi e progressi di apprendimento.
Usa matplotlib se disponibile, altrimenti usa Rich (grafici ASCII).

Dipendenze opzionali: matplotlib, numpy
"""

import os
import sys
import json
import time
from datetime import datetime

_CART = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(_CART)
for _p in (_ROOT, _CART):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from rich.console import Console
from rich.panel import Panel
from rich.text import Text
from rich.table import Table
from rich.rule import Rule
try:
    from check_deps import stampa_header, risorse, stampa_breadcrumb
except ImportError:
    def stampa_header(con, res=None): pass
    def stampa_breadcrumb(con, p): print(f"  {p}")
    def risorse(): return {}

console = Console()

FILE_STORICO = os.path.expanduser("~/.prismalux_quiz_storico.json")
FILE_DB_QUIZ = os.path.expanduser("~/.prismalux_quiz_database.json")

# ── Rilevamento matplotlib ────────────────────────────────────────────────────

def _ha_matplotlib() -> bool:
    try:
        import matplotlib  # noqa
        return True
    except ImportError:
        return False


def _installa_matplotlib() -> bool:
    console.print("  [yellow]matplotlib non trovato. Installo...[/]")
    import subprocess
    r = subprocess.run(
        [sys.executable, "-m", "pip", "install", "--quiet", "matplotlib", "numpy"],
        capture_output=True,
    )
    ok = r.returncode == 0
    if ok:
        console.print("  [green]✓ matplotlib installato.[/]")
    else:
        console.print("  [red]Installazione fallita. Uso grafici ASCII.[/]")
    return ok


# ── Carica dati ───────────────────────────────────────────────────────────────

def _carica_storico() -> list:
    if os.path.exists(FILE_STORICO):
        try:
            with open(FILE_STORICO, encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return []


def _carica_db() -> dict:
    if os.path.exists(FILE_DB_QUIZ):
        try:
            with open(FILE_DB_QUIZ, encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return {"domande": {}}


# ── Grafici ASCII (Rich) ──────────────────────────────────────────────────────

def _barra_ascii(valore: float, max_val: float, larghezza: int = 30,
                  colore: str = "green") -> str:
    """Genera una barra ASCII proporzionale."""
    if max_val <= 0:
        return "░" * larghezza
    filled = int(valore / max_val * larghezza)
    filled = max(0, min(larghezza, filled))
    return f"[{colore}]{'█' * filled}[/{colore}]{'░' * (larghezza - filled)}"


def _grafico_progressi_ascii() -> None:
    """Mostra progressi quiz nel tempo (ASCII)."""
    storico = _carica_storico()
    if len(storico) < 2:
        console.print("  [dim]Servono almeno 2 sessioni per vedere il progresso.[/]")
        return

    # Ultimi 15 risultati
    ultime = storico[-15:]
    max_perc = 100

    t = Text("\n  Percentuale per sessione (ultime 15):\n\n")
    for s in ultime:
        p = s.get("perc", 0)
        data = s.get("data", "")[-5:]  # solo MM-DD HH:MM
        barra = _barra_ascii(p, max_perc, 25,
                              "green" if p >= 80 else "yellow" if p >= 60 else "red")
        t.append(f"  {data}  {barra}  {p:3d}%\n")

    console.print(Panel(t, title="[bold]📈 Progressi nel Tempo[/]", border_style="blue"))


def _grafico_categorie_ascii() -> None:
    """Mostra performance media per categoria (ASCII)."""
    storico = _carica_storico()
    if not storico:
        console.print("  [dim]Nessuna sessione registrata.[/]")
        return

    # Calcola media per categoria
    cat_dati: dict[str, list] = {}
    for s in storico:
        cat = s.get("categoria", "?")
        cat_dati.setdefault(cat, []).append(s.get("perc", 0))

    CAT_LABEL = {
        "python_base": "Python Base",    "python_medio": "Python Medio",
        "python_avanzato": "Python Adv", "algoritmi": "Algoritmi",
        "matematica": "Matematica",      "fisica": "Fisica",
        "chimica": "Chimica",            "sicurezza": "Sicurezza",
        "protocolli": "Protocolli",
    }

    t = Text("\n  Media punteggio per categoria:\n\n")
    medie = [(CAT_LABEL.get(k, k), sum(v)/len(v), len(v)) for k, v in cat_dati.items()]
    medie.sort(key=lambda x: x[1], reverse=True)

    for label, media, n_sess in medie:
        barra = _barra_ascii(media, 100, 22,
                              "green" if media >= 80 else "yellow" if media >= 60 else "red")
        t.append(f"  {label:<16} {barra}  {media:.0f}%  ({n_sess} sess.)\n")

    console.print(Panel(t, title="[bold]📊 Performance per Categoria[/]", border_style="magenta"))


def _grafico_db_ascii() -> None:
    """Mostra distribuzione domande nel database (ASCII)."""
    db = _carica_db()
    domande = db.get("domande", {})
    if not domande:
        console.print("  [dim]Database vuoto.[/]")
        return

    CAT_LABEL = {
        "python_base": "Python Base",    "python_medio": "Python Medio",
        "python_avanzato": "Python Adv", "algoritmi": "Algoritmi",
        "matematica": "Matematica",      "fisica": "Fisica",
        "chimica": "Chimica",            "sicurezza": "Sicurezza",
        "protocolli": "Protocolli",
    }

    conteggi = {k: len(v) for k, v in domande.items() if v}
    max_n = max(conteggi.values(), default=1)

    t = Text("\n  Domande per categoria nel database:\n\n")
    for k, n in sorted(conteggi.items(), key=lambda x: x[1], reverse=True):
        label = CAT_LABEL.get(k, k)
        barra = _barra_ascii(n, max_n, 22, "cyan")
        t.append(f"  {label:<16} {barra}  {n:3d} dom.\n")

    console.print(Panel(t, title="[bold]🗃️  Database Domande[/]", border_style="cyan"))


# ── Grafici matplotlib ────────────────────────────────────────────────────────

def _grafici_matplotlib() -> None:
    """Apre finestra matplotlib con 3 grafici affiancati."""
    try:
        import matplotlib
        matplotlib.use("TkAgg")
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError:
        console.print("  [red]matplotlib non disponibile. Uso grafici ASCII.[/]")
        _mostra_dashboard_ascii()
        return

    storico = _carica_storico()
    db = _carica_db()

    fig, axes = plt.subplots(1, 3, figsize=(18, 6))
    fig.patch.set_facecolor("#1e1e1e")
    fig.suptitle("Prismalux — Dashboard Statistica", color="white",
                 fontsize=16, fontweight="bold")

    colori = {"green": "#4caf50", "yellow": "#ffeb3b", "red": "#f44336",
              "cyan": "#00bcd4", "magenta": "#e91e63", "blue": "#2196f3"}

    # ── Grafico 1: Progressi nel tempo ──────────────────────────────────────
    ax1 = axes[0]
    ax1.set_facecolor("#252526")
    ax1.set_title("Progressi nel Tempo", color="white")
    if storico:
        date = [s["data"][-5:] for s in storico[-20:]]
        perc = [s.get("perc", 0) for s in storico[-20:]]
        colori_punti = [colori["green"] if p >= 80 else
                        colori["yellow"] if p >= 60 else
                        colori["red"] for p in perc]
        ax1.plot(range(len(date)), perc, color=colori["cyan"], linewidth=2)
        ax1.scatter(range(len(date)), perc, c=colori_punti, s=60, zorder=5)
        ax1.axhline(y=80, color=colori["green"], linestyle="--", alpha=0.5, linewidth=1)
        ax1.axhline(y=60, color=colori["yellow"], linestyle="--", alpha=0.5, linewidth=1)
        ax1.set_xticks(range(len(date)))
        ax1.set_xticklabels(date, rotation=45, ha="right", fontsize=7, color="white")
        ax1.set_ylim(0, 105)
        ax1.set_ylabel("Punteggio %", color="white")
        ax1.tick_params(colors="white")
    else:
        ax1.text(0.5, 0.5, "Nessuna sessione", ha="center", va="center",
                 color="gray", transform=ax1.transAxes)

    # ── Grafico 2: Media per categoria ──────────────────────────────────────
    ax2 = axes[1]
    ax2.set_facecolor("#252526")
    ax2.set_title("Media per Categoria", color="white")
    if storico:
        cat_dati: dict[str, list] = {}
        for s in storico:
            cat_dati.setdefault(s.get("categoria", "?"), []).append(s.get("perc", 0))
        labels_cat = [k[-8:] for k in cat_dati]
        medie = [sum(v)/len(v) for v in cat_dati.values()]
        colori_barre = [colori["green"] if m >= 80 else
                        colori["yellow"] if m >= 60 else
                        colori["red"] for m in medie]
        bars = ax2.bar(range(len(labels_cat)), medie, color=colori_barre, alpha=0.85)
        ax2.set_xticks(range(len(labels_cat)))
        ax2.set_xticklabels(labels_cat, rotation=45, ha="right", fontsize=7, color="white")
        ax2.set_ylim(0, 105)
        ax2.tick_params(colors="white")
        for bar, val in zip(bars, medie):
            ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1,
                     f"{val:.0f}%", ha="center", va="bottom", color="white", fontsize=8)
    else:
        ax2.text(0.5, 0.5, "Nessuna sessione", ha="center", va="center",
                 color="gray", transform=ax2.transAxes)

    # ── Grafico 3: Distribuzione database ───────────────────────────────────
    ax3 = axes[2]
    ax3.set_facecolor("#252526")
    ax3.set_title("Domande nel Database", color="white")
    domande = db.get("domande", {})
    conteggi = {k[-8:]: len(v) for k, v in domande.items() if v}
    if conteggi:
        wedge_colors = [colori["cyan"], colori["magenta"], colori["green"],
                        colori["yellow"], colori["blue"], "#ff9800", "#9c27b0",
                        colori["red"], "#00e5ff"]
        wedges, texts, autotexts = ax3.pie(
            list(conteggi.values()),
            labels=list(conteggi.keys()),
            autopct="%1.0f%%",
            colors=wedge_colors[:len(conteggi)],
            textprops={"color": "white", "fontsize": 8},
        )
    else:
        ax3.text(0.5, 0.5, "Database vuoto", ha="center", va="center",
                 color="gray", transform=ax3.transAxes)

    for ax in axes:
        for spine in ax.spines.values():
            spine.set_edgecolor("#444")

    plt.tight_layout()
    plt.show()
    console.print("  [dim]Finestra matplotlib chiusa.[/]")


# ── Dashboard ASCII (sempre disponibile) ─────────────────────────────────────

def _mostra_dashboard_ascii() -> None:
    """Dashboard completa in ASCII usando Rich."""
    console.clear()
    stampa_header(console, risorse())
    stampa_breadcrumb(console, "Prismalux › 📚 Dashboard Statistica › ASCII")

    storico = _carica_storico()

    # Statistiche generali
    if storico:
        media_globale = sum(s.get("perc", 0) for s in storico) / len(storico)
        best = max(storico, key=lambda x: x.get("perc", 0))
        tab = Table(show_header=False, box=None, padding=(0, 2))
        tab.add_column(style="dim", width=22)
        tab.add_column()
        tab.add_row("Sessioni totali",  str(len(storico)))
        tab.add_row("Media globale",    f"{media_globale:.1f}%")
        tab.add_row("Migliore",         f"{best.get('perc', 0)}% — {best.get('data', '')}")
        console.print(Panel(tab, title="[bold]📊 Statistiche Globali[/]",
                            border_style="yellow", padding=(1, 2)))

    _grafico_progressi_ascii()
    _grafico_categorie_ascii()
    _grafico_db_ascii()

    input("\n  [INVIO] per continuare ")


# ── Menu dashboard ────────────────────────────────────────────────────────────

def menu_dashboard() -> None:
    """Entry point della dashboard statistica."""
    while True:
        console.clear()
        stampa_header(console, risorse())
        stampa_breadcrumb(console, "Prismalux › 📚 Apprendimento › Dashboard")

        ha_mpl = _ha_matplotlib()
        stato_mpl = "[green]installato[/]" if ha_mpl else "[yellow]non installato[/]"

        t = Text()
        t.append(f"\n  matplotlib: {stato_mpl}\n\n")
        t.append("  1.  📊  Dashboard ASCII (sempre disponibile)\n\n",  style="bold cyan")
        if ha_mpl:
            t.append("  2.  🖼️   Grafici matplotlib (finestra)\n\n",      style="bold green")
        else:
            t.append("  2.  🖼️   Installa matplotlib e mostra grafici\n\n", style="bold yellow")
        t.append("  0.  ←   Torna\n",                                    style="dim")
        console.print(Panel(t, title="[bold magenta]📊 Dashboard Statistica[/]",
                            border_style="magenta"))

        try:
            scelta = input("  Scelta: ").strip()
        except (KeyboardInterrupt, EOFError):
            break

        if scelta == "0":
            break
        elif scelta == "1":
            _mostra_dashboard_ascii()
        elif scelta == "2":
            if not ha_mpl:
                if _installa_matplotlib():
                    _grafici_matplotlib()
            else:
                _grafici_matplotlib()
        else:
            console.print("  [yellow]Scelta non valida.[/]")
            time.sleep(0.8)


if __name__ == "__main__":
    menu_dashboard()

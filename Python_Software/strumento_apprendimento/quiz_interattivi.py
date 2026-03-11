"""
Quiz Interattivi — Prismalux
=============================
Test di apprendimento con punteggio e storico.
Le domande vengono generate da modelli POTENTI (online o locali grandi)
e salvate in un database locale JSON per uso offline affidabile.

Architettura (concordata in sessione qwen3.5-plus):
  1. Genera domande con modello potente -> salva in DB locale
  2. Quiz usa DB locale -> veloce, offline, corretto

Dipendenze: rich, requests
"""

import os
import sys
import re
import json
import random
import time
from datetime import datetime

_CART = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(_CART)
for _p in (_ROOT, _CART, os.path.join(_ROOT, "core")):
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

# ── Percorsi database (tutto in esportazioni/ dentro il progetto) ─────────────
_ESPORTAZIONI = os.path.join(_ROOT, "esportazioni")
os.makedirs(_ESPORTAZIONI, exist_ok=True)
FILE_DB_QUIZ = os.path.join(_ESPORTAZIONI, "quiz_database.json")
FILE_STORICO = os.path.join(_ESPORTAZIONI, "quiz_storico.json")

# Migra file vecchi dalla home se esistono ancora
for _vecchio, _nuovo in [
    (os.path.expanduser("~/.prismalux_quiz_database.json"), FILE_DB_QUIZ),
    (os.path.expanduser("~/.prismalux_quiz_storico.json"),  FILE_STORICO),
]:
    if os.path.exists(_vecchio) and not os.path.exists(_nuovo):
        import shutil as _shutil
        _shutil.move(_vecchio, _nuovo)
OLLAMA_URL      = "http://localhost:11434"

# ── Categorie disponibili ─────────────────────────────────────────────────────
CATEGORIE = [
    ("python_base",     "🐍 Python Base",           "variabili, if/else, cicli, liste, dizionari, funzioni"),
    ("python_medio",    "🔷 Python Intermedio",     "comprehension, lambda, map/filter, eccezioni, file"),
    ("python_avanzato", "⚡ Python Avanzato",        "generatori, decoratori, classi, async, metaclassi"),
    ("algoritmi",       "🔵 Algoritmi",              "sorting, ricerca, grafi, programmazione dinamica, complessita"),
    ("matematica",      "📐 Matematica",             "algebra, calcolo, probabilita, statistica, algebra lineare"),
    ("fisica",          "⚛️  Fisica",                "meccanica, termodinamica, ottica, elettromagnetismo"),
    ("chimica",         "🧪 Chimica",               "legami, reazioni, stechiometria, termodinamica chimica"),
    ("sicurezza",       "🔒 Sicurezza Informatica", "crittografia, reti, vulnerabilita, difese, protocolli"),
    ("protocolli",      "🌐 Protocolli di Rete",    "TCP/IP, HTTP, DNS, TLS, OAuth, REST, WebSocket"),
]

CAT_MAP = {c[0]: (c[1], c[2]) for c in CATEGORIE}


# ── Database quiz ─────────────────────────────────────────────────────────────

def _carica_db() -> dict:
    if os.path.exists(FILE_DB_QUIZ):
        try:
            with open(FILE_DB_QUIZ, encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return {"version": 1, "domande": {}}


def _salva_db(db: dict) -> None:
    with open(FILE_DB_QUIZ, "w", encoding="utf-8") as f:
        json.dump(db, f, indent=2, ensure_ascii=False)


def _conta_domande(db: dict, categoria: str) -> int:
    return len(db.get("domande", {}).get(categoria, []))


# ── Storico punteggi ──────────────────────────────────────────────────────────

def _carica_storico() -> list:
    if os.path.exists(FILE_STORICO):
        try:
            with open(FILE_STORICO, encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return []


def _salva_storico(storico: list) -> None:
    with open(FILE_STORICO, "w", encoding="utf-8") as f:
        json.dump(storico[-100:], f, indent=2, ensure_ascii=False)


# ── Generazione domande con Ollama ────────────────────────────────────────────

def _get_modelli_disponibili() -> list[dict]:
    """Ritorna lista modelli Ollama ordinati per dimensione (piu grandi prima)."""
    try:
        import requests
        r = requests.get(f"{OLLAMA_URL}/api/tags", timeout=5)
        if r.status_code == 200:
            modelli = [
                {"nome": m["name"], "size_gb": round(m.get("size", 0) / 1e9, 1)}
                for m in r.json().get("models", [])
                if m.get("size", 0) > 0
            ]
            return sorted(modelli, key=lambda x: x["size_gb"], reverse=True)
    except Exception:
        pass
    return []


def _scegli_modello_generazione() -> str:
    """
    Chiede quale modello usare per generare le domande.
    Raccomanda il piu grande disponibile per massima qualita.
    """
    modelli = _get_modelli_disponibili()
    if not modelli:
        console.print("  [red]Ollama non raggiungibile.[/]")
        return ""

    t = Text()
    t.append("\n  Modelli disponibili (ordinati per dimensione):\n\n")
    for i, m in enumerate(modelli, 1):
        raccomandato = " ← consigliato" if i == 1 else ""
        colore = "bold green" if i == 1 else "dim"
        t.append(f"  {i:2}. {m['nome']:<36} {m['size_gb']:.1f} GB{raccomandato}\n",
                 style=colore)
    t.append("\n  [dim]Modello piu grande = domande piu accurate[/dim]\n")
    console.print(Panel(t, title="[bold]Scegli modello per generazione[/]",
                        border_style="cyan"))

    try:
        raw = input(f"  Scelta [1-{len(modelli)}, Invio={modelli[0]['nome']}]: ").strip()
        if not raw:
            return modelli[0]["nome"]
        n = int(raw) - 1
        if 0 <= n < len(modelli):
            return modelli[n]["nome"]
    except (ValueError, EOFError):
        pass
    return modelli[0]["nome"]


def _ripara_json_array(testo: str) -> str:
    """
    Tenta di riparare un JSON array troncato cercando l'ultimo oggetto completo.
    Utile quando Ollama tronca la risposta a meta di un elemento.
    """
    start = testo.find("[")
    if start == -1:
        return testo
    substr = testo[start:]
    # Prova prima cosi com'e
    try:
        json.loads(substr)
        return substr
    except json.JSONDecodeError:
        pass
    # Cerca l'ultimo '}' che chiude un oggetto completo e chiude l'array
    i = len(substr) - 1
    while i >= 0:
        if substr[i] == "}":
            candidate = substr[:i + 1].rstrip().rstrip(",") + "]"
            try:
                json.loads(candidate)
                return candidate
            except json.JSONDecodeError:
                pass
        i -= 1
    return substr


def _genera_domande_ollama(categoria: str, descrizione: str,
                            modello: str, n: int = 5) -> list[dict]:
    """
    Chiede a Ollama di generare N domande a scelta multipla per la categoria.
    Ritorna lista di dict: {domanda, opzioni:[a,b,c,d], corretta, spiegazione}
    """
    try:
        import requests
    except ImportError:
        return []

    prompt = f"""Genera esattamente {n} domande a scelta multipla su: {descrizione}

FORMATO RICHIESTO (JSON array, niente altro testo):
[
  {{
    "domanda": "Testo della domanda chiaro e preciso?",
    "opzioni": {{
      "A": "Prima opzione",
      "B": "Seconda opzione",
      "C": "Terza opzione",
      "D": "Quarta opzione"
    }},
    "corretta": "A",
    "spiegazione": "Breve spiegazione (max 1 riga)"
  }}
]

REGOLE:
- Una sola risposta corretta per domanda
- Opzioni plausibili, non banali
- Difficolta progressiva (le prime facili, le ultime piu difficili)
- Risposte tecnicamente CORRETTE al 100%
- Lingua italiana
- Spiegazione BREVE (una riga sola) per evitare troncamenti

Rispondi SOLO con il JSON array, senza commenti:"""

    try:
        r = requests.post(
            f"{OLLAMA_URL}/api/generate",
            json={"model": modello, "prompt": prompt, "stream": False,
                  "options": {"num_predict": 3000}},
            timeout=300,
        )
        if r.status_code != 200:
            return []

        testo = r.json().get("response", "")
        # Estrai il blocco JSON dall'output
        match = re.search(r'\[.*', testo, re.DOTALL)
        if not match:
            return []
        testo_json = _ripara_json_array(match.group(0))
        try:
            domande = json.loads(testo_json)
        except json.JSONDecodeError as e:
            console.print(f"  [yellow]Errore JSON (anche dopo riparazione): {e}[/]")
            return []
        # Valida struttura
        valide = []
        for d in domande:
            if all(k in d for k in ("domanda", "opzioni", "corretta", "spiegazione")):
                if isinstance(d["opzioni"], dict) and d["corretta"] in d["opzioni"]:
                    d["categoria"] = categoria
                    d["generata_da"] = modello
                    d["data"] = datetime.now().isoformat()[:10]
                    valide.append(d)
        return valide
    except Exception as e:
        console.print(f"  [yellow]Errore generazione: {e}[/]")
        return []


# ── Genera e salva domande (menu) ─────────────────────────────────────────────

def _menu_genera_domande() -> None:
    """Menu per generare e aggiungere domande al database."""
    console.clear()
    stampa_header(console, risorse())
    stampa_breadcrumb(console, "Prismalux › 📚 Quiz › ➕ Genera Domande")

    # Scegli modello
    modello = _scegli_modello_generazione()
    if not modello:
        input("\n  [INVIO] ")
        return

    # Scegli categoria
    db = _carica_db()
    t = Text("\n  Categorie disponibili:\n\n")
    for i, (chiave, label, _) in enumerate(CATEGORIE, 1):
        n = _conta_domande(db, chiave)
        t.append(f"  {i:2}. {label:<32} ({n} domande nel DB)\n", style="cyan")
    t.append("\n  [0]  Genera per TUTTE le categorie\n", style="bold green")
    console.print(Panel(t, title="[bold]Seleziona categoria[/]", border_style="cyan"))

    try:
        raw = input("  Scelta: ").strip()
    except (KeyboardInterrupt, EOFError):
        return

    if raw == "0":
        categorie_da_generare = [(c[0], c[2]) for c in CATEGORIE]
    else:
        try:
            n = int(raw) - 1
            if 0 <= n < len(CATEGORIE):
                categorie_da_generare = [(CATEGORIE[n][0], CATEGORIE[n][2])]
            else:
                return
        except ValueError:
            return

    try:
        n_per_cat = int(input("  Quante domande per categoria? [5]: ").strip() or "5")
    except (ValueError, EOFError):
        n_per_cat = 5
    n_per_cat = max(1, min(20, n_per_cat))

    # Genera
    totale_aggiunte = 0
    for chiave, descrizione in categorie_da_generare:
        label, _ = CAT_MAP[chiave]
        console.print(f"\n  [cyan]Generando {n_per_cat} domande: {label}...[/]")
        domande = _genera_domande_ollama(chiave, descrizione, modello, n_per_cat)
        if domande:
            db["domande"].setdefault(chiave, []).extend(domande)
            totale_aggiunte += len(domande)
            console.print(f"  [green]✓ {len(domande)} domande aggiunte.[/]")
        else:
            console.print(f"  [yellow]⚠ Nessuna domanda generata per {label}.[/]")

    _salva_db(db)
    console.print(f"\n  [bold green]✅ Database aggiornato: +{totale_aggiunte} domande totali.[/]")
    console.print(f"  [dim]Percorso: {FILE_DB_QUIZ}[/]")
    input("\n  [INVIO] per continuare ")


# ── Esecuzione quiz ───────────────────────────────────────────────────────────

def _esegui_quiz(categoria: str, n_domande: int = 10) -> None:
    """Esegue una sessione di quiz per la categoria scelta."""
    db = _carica_db()
    pool = db.get("domande", {}).get(categoria, [])
    if not pool:
        console.print(f"  [yellow]Nessuna domanda per questa categoria. Genera prima![/]")
        input("  [INVIO] ")
        return

    label, _ = CAT_MAP.get(categoria, (categoria, ""))
    domande = random.sample(pool, min(n_domande, len(pool)))
    punteggio = 0
    risposte_date = []

    for i, d in enumerate(domande, 1):
        console.clear()
        stampa_breadcrumb(console, f"Prismalux › 📚 Quiz › {label} › Domanda {i}/{len(domande)}")

        # Barra progresso
        perc = int((i - 1) / len(domande) * 20)
        barra = "█" * perc + "░" * (20 - perc)
        console.print(f"\n  [{barra}] {i-1}/{len(domande)}  Punteggio: {punteggio}/{i-1}\n",
                      style="dim")

        # Mostra domanda
        t = Text()
        t.append(f"\n  {d['domanda']}\n\n", style="bold white")
        for lettera, testo in sorted(d["opzioni"].items()):
            t.append(f"  [{lettera}]  {testo}\n", style="cyan")
        console.print(Panel(t, title=f"[bold cyan]{label} — Domanda {i}[/]",
                            border_style="cyan", padding=(0, 1)))

        # Input risposta
        console.print("  Risposta [A/B/C/D] oppure [0]=abbandona: ", end="")
        try:
            risposta = input("").strip().upper()
        except (KeyboardInterrupt, EOFError):
            break

        if risposta == "0":
            break

        corretta = d["corretta"].upper()
        giusta = risposta == corretta

        if giusta:
            punteggio += 1
            console.print(f"\n  [bold green]✅ CORRETTO![/]")
        else:
            console.print(f"\n  [bold red]❌ SBAGLIATO![/]  La risposta corretta era: [green]{corretta}[/]")

        console.print(f"  [dim]{d.get('spiegazione', '')}[/]\n")
        risposte_date.append({"giusta": giusta, "domanda": d["domanda"][:60]})
        time.sleep(1.5)

    # Riepilogo finale
    console.clear()
    console.print(Rule("[bold]RIEPILOGO QUIZ[/]", style="yellow"))
    perc_finale = int(punteggio / max(len(risposte_date), 1) * 100)

    if perc_finale >= 80:
        voto = "[bold green]OTTIMO[/]"
    elif perc_finale >= 60:
        voto = "[bold yellow]BUONO[/]"
    elif perc_finale >= 40:
        voto = "[bold magenta]SUFFICIENTE[/]"
    else:
        voto = "[bold red]DA RIVEDERE[/]"

    tab = Table(show_header=False, box=None, padding=(0, 2))
    tab.add_column(style="dim", width=22)
    tab.add_column()
    tab.add_row("Categoria",   label)
    tab.add_row("Domande",     f"{len(risposte_date)}")
    tab.add_row("Corrette",    f"[green]{punteggio}[/]")
    tab.add_row("Sbagliate",   f"[red]{len(risposte_date)-punteggio}[/]")
    tab.add_row("Percentuale", f"[bold]{perc_finale}%[/]")
    tab.add_row("Voto",        voto)
    console.print(Panel(tab, title="[bold yellow]Risultato[/]",
                        border_style="yellow", padding=(1, 2)))

    # Salva nello storico
    storico = _carica_storico()
    storico.append({
        "data":      datetime.now().strftime("%Y-%m-%d %H:%M"),
        "categoria": categoria,
        "domande":   len(risposte_date),
        "corrette":  punteggio,
        "perc":      perc_finale,
    })
    _salva_storico(storico)
    input("\n  [INVIO] per continuare ")


# ── Storico sessioni ──────────────────────────────────────────────────────────

def _mostra_storico() -> None:
    """Mostra lo storico delle sessioni quiz."""
    storico = _carica_storico()
    if not storico:
        console.print("  [dim]Nessuna sessione quiz ancora effettuata.[/]")
        input("  [INVIO] ")
        return

    tab = Table(title="[bold]Storico Quiz Prismalux[/]", border_style="dim blue",
                show_lines=True)
    tab.add_column("Data",       style="dim",   width=16)
    tab.add_column("Categoria",  style="cyan",  width=24)
    tab.add_column("Dom.",       width=6)
    tab.add_column("Corrette",   width=9)
    tab.add_column("Punteggio",  width=10)

    for s in reversed(storico[-20:]):
        p = s.get("perc", 0)
        colore = "green" if p >= 80 else "yellow" if p >= 60 else "red"
        label = CAT_MAP.get(s["categoria"], (s["categoria"], ""))[0]
        tab.add_row(
            s["data"],
            label,
            str(s["domande"]),
            str(s["corrette"]),
            f"[{colore}]{p}%[/{colore}]",
        )

    console.print(tab)
    input("\n  [INVIO] ")


# ── Stato database ────────────────────────────────────────────────────────────

def _mostra_stato_db() -> None:
    """Mostra quante domande ci sono nel database per ogni categoria."""
    db = _carica_db()
    tab = Table(title="[bold]Database Quiz[/]", border_style="dim",
                show_lines=False)
    tab.add_column("Categoria", style="cyan", width=30)
    tab.add_column("Domande",   width=10)
    tab.add_column("Stato",     width=20)

    totale = 0
    for chiave, label, _ in CATEGORIE:
        n = _conta_domande(db, chiave)
        totale += n
        if n == 0:
            stato = "[red]vuota — genera![/]"
        elif n < 10:
            stato = f"[yellow]poche ({n})[/]"
        else:
            stato = f"[green]OK ({n})[/]"
        tab.add_row(label, str(n), stato)

    console.print(tab)
    console.print(f"\n  [bold]Totale domande nel database: {totale}[/]")
    console.print(f"  [dim]Percorso: {FILE_DB_QUIZ}[/]")
    input("\n  [INVIO] ")


# ── Menu principale quiz ──────────────────────────────────────────────────────

def menu_quiz() -> None:
    """Entry point del sistema quiz interattivi."""
    while True:
        console.clear()
        stampa_header(console, risorse())
        stampa_breadcrumb(console, "Prismalux › 📚 Apprendimento › Quiz Interattivi")

        db = _carica_db()
        n_tot = sum(len(v) for v in db.get("domande", {}).values())

        t = Text()
        t.append(f"\n  Database locale: {n_tot} domande disponibili\n\n", style="dim")
        t.append("  1.  🎯  Inizia Quiz\n\n",              style="bold cyan")
        t.append("  2.  ➕  Genera nuove domande (AI)\n\n", style="bold green")
        t.append("  3.  📊  Storico sessioni\n\n",          style="bold magenta")
        t.append("  4.  🗃️   Stato database\n\n",           style="bold blue")
        t.append("  0.  ←   Torna\n",                       style="dim")
        console.print(Panel(t, title="[bold cyan]📚 Quiz Interattivi[/]",
                            border_style="cyan"))

        try:
            scelta = input("  Scelta: ").strip()
        except (KeyboardInterrupt, EOFError):
            break

        if scelta == "0":
            break

        elif scelta == "1":
            # Scegli categoria
            console.clear()
            t2 = Text("\n  Scegli categoria:\n\n")
            for i, (chiave, label, _) in enumerate(CATEGORIE, 1):
                n = _conta_domande(db, chiave)
                disponibile = f"({n} dom.)" if n > 0 else "[red](vuota)[/red]"
                t2.append(f"  {i:2}. {label:<32} {disponibile}\n",
                          style="cyan" if n > 0 else "dim")
            console.print(Panel(t2, title="[bold]Categoria[/]", border_style="cyan"))

            try:
                raw = input("  Scelta: ").strip()
                n = int(raw) - 1
                if 0 <= n < len(CATEGORIE):
                    chiave = CATEGORIE[n][0]
                    if _conta_domande(db, chiave) == 0:
                        console.print("  [yellow]Nessuna domanda. Vai su '2. Genera' prima.[/]")
                        input("  [INVIO] ")
                        continue
                    try:
                        n_dom = int(input("  Quante domande? [10]: ").strip() or "10")
                    except ValueError:
                        n_dom = 10
                    _esegui_quiz(chiave, n_dom)
            except (ValueError, EOFError, KeyboardInterrupt):
                pass

        elif scelta == "2":
            try:
                _menu_genera_domande()
            except KeyboardInterrupt:
                pass

        elif scelta == "3":
            _mostra_storico()

        elif scelta == "4":
            console.clear()
            _mostra_stato_db()

        else:
            console.print("  [yellow]Scelta non valida.[/]")
            time.sleep(0.8)


if __name__ == "__main__":
    menu_quiz()

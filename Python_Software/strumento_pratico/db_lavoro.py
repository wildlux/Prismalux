"""
Database Whitelist / Blacklist — Gestione Lavoro
==================================================
Salva aziende e parole chiave in due liste:

  WHITELIST → aziende/keyword affidabili e preferite
  BLACKLIST → aziende/keyword da evitare (truffe, bad employer, ecc.)

Usa file JSON — già incluso in Python, nessuna installazione necessaria.
I file vengono salvati in: strumento_pratico/data/
  whitelist.json
  blacklist.json
"""

import os
import sys
import json
from datetime import datetime
from rich.console import Console
from rich.panel import Panel
from rich.table import Table
try:
    from check_deps import Ridimensiona, input_nel_box
except ImportError:
    class Ridimensiona(BaseException): pass
    def input_nel_box(padding_left=2): return input("\n  ❯ ")

console = Console()

DATA_DIR   = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
FILE_WHITE = os.path.join(DATA_DIR, "whitelist.json")
FILE_BLACK = os.path.join(DATA_DIR, "blacklist.json")

# ── Aziende/keyword sospette pre-caricate al primo avvio ─────────────────────
BLACKLIST_DEFAULT = [
    {"tipo": "keyword", "valore": "investimento iniziale",              "motivo": "Richiesta di pagamento = truffa classica"},
    {"tipo": "keyword", "valore": "pagamento anticipato",              "motivo": "Red flag classico delle truffe"},
    {"tipo": "keyword", "valore": "kit di avvio",                      "motivo": "MLM / schema piramidale"},
    {"tipo": "keyword", "valore": "guadagni illimitati",               "motivo": "Promessa irrealistica"},
    {"tipo": "keyword", "valore": "lavoro da casa facile",             "motivo": "Offerta vaga sospetta"},
    {"tipo": "keyword", "valore": "guadagna da casa",                  "motivo": "Schema piramidale / MLM"},
    {"tipo": "keyword", "valore": "no esperienza richiesta guadagni",  "motivo": "Red flag: promesse esagerate"},
    {"tipo": "azienda", "valore": "herbalife",                         "motivo": "MLM noto"},
    {"tipo": "azienda", "valore": "amway",                             "motivo": "MLM noto"},
    {"tipo": "azienda", "valore": "forever living",                    "motivo": "MLM noto"},
    {"tipo": "truffa",  "valore": "invia curriculum a pagamento",      "motivo": "Truffa: CV non si pagano mai"},
]


# ── Caricamento e salvataggio ─────────────────────────────────────────────────

def _carica(percorso: str, default: list) -> list:
    os.makedirs(DATA_DIR, exist_ok=True)
    if not os.path.exists(percorso):
        _salva(percorso, default)
        return [dict(id=i+1, **r, data=datetime.now().strftime("%Y-%m-%d"))
                for i, r in enumerate(default)]
    with open(percorso, encoding="utf-8") as f:
        return json.load(f)


def _salva(percorso: str, dati: list) -> None:
    os.makedirs(DATA_DIR, exist_ok=True)
    with open(percorso, "w", encoding="utf-8") as f:
        json.dump(dati, f, indent=2, ensure_ascii=False)


def _prossimo_id(lista: list) -> int:
    return max((r.get("id", 0) for r in lista), default=0) + 1


def leggi_lista(lista: str) -> list:
    if lista == "white":
        return _carica(FILE_WHITE, [])
    else:
        bl = _carica(FILE_BLACK, [])
        if not bl:
            # Prima inizializzazione con i default
            bl = [{"id": i+1, **r, "data": datetime.now().strftime("%Y-%m-%d")}
                  for i, r in enumerate(BLACKLIST_DEFAULT)]
            _salva(FILE_BLACK, bl)
        return bl


# ── Operazioni CRUD ───────────────────────────────────────────────────────────

def aggiungi(lista: str, tipo: str, valore: str, nota: str = "") -> bool:
    """Aggiunge un elemento. Ritorna True se aggiunto, False se già presente."""
    righe = leggi_lista(lista)
    valore = valore.strip()

    # Controlla duplicati (case-insensitive)
    if any(r["valore"].lower() == valore.lower() for r in righe):
        return False

    col_nota = "nota" if lista == "white" else "motivo"
    righe.append({
        "id":     _prossimo_id(righe),
        "tipo":   tipo,
        "valore": valore,
        col_nota: nota.strip(),
        "data":   datetime.now().strftime("%Y-%m-%d"),
    })
    _salva(FILE_WHITE if lista == "white" else FILE_BLACK, righe)
    return True


def rimuovi(lista: str, valore: str) -> bool:
    """Rimuove un elemento. Ritorna True se rimosso."""
    righe = leggi_lista(lista)
    nuove = [r for r in righe if r["valore"].lower() != valore.lower()]
    if len(nuove) == len(righe):
        return False
    _salva(FILE_WHITE if lista == "white" else FILE_BLACK, nuove)
    return True


def cerca_in_lista(lista: str, testo: str) -> list:
    """Cerca elementi della lista nel testo fornito (case-insensitive)."""
    testo_lower = testo.lower()
    return [r for r in leggi_lista(lista) if r["valore"].lower() in testo_lower]


def valuta_offerta(testo: str) -> dict:
    """
    Controlla un testo contro whitelist e blacklist.
    Ritorna: { "white": [...], "black": [...], "semaforo": "verde"|"giallo"|"rosso" }
    """
    trovati_white = cerca_in_lista("white", testo)
    trovati_black = cerca_in_lista("black", testo)
    if trovati_black:
        semaforo = "rosso"
    elif trovati_white:
        semaforo = "verde"
    else:
        semaforo = "giallo"
    return {"white": trovati_white, "black": trovati_black, "semaforo": semaforo}


# ── Visualizzazione ───────────────────────────────────────────────────────────

def mostra_lista(lista: str) -> None:
    righe = leggi_lista(lista)
    if lista == "white":
        titolo, colore, col_nota = "✅  WHITELIST", "green", "nota"
    else:
        titolo, colore, col_nota = "🚫  BLACKLIST", "red", "motivo"

    if not righe:
        console.print(Panel(f"[dim]La {lista}list è vuota.[/]", border_style=colore))
        return

    tab = Table(title=titolo, border_style=colore, show_lines=True)
    tab.add_column("ID",    width=5,  style="dim")
    tab.add_column("Tipo",  width=12)
    tab.add_column("Valore", width=38)
    tab.add_column(col_nota.capitalize(), width=32, style="dim")
    tab.add_column("Data",  width=12, style="dim")

    icone = {"azienda": "🏢", "keyword": "🔑", "truffa": "⚠️"}
    for r in sorted(righe, key=lambda x: (x.get("tipo",""), x.get("valore",""))):
        icona = icone.get(r.get("tipo", ""), "•")
        nota  = r.get(col_nota, r.get("nota", r.get("motivo", "—"))) or "—"
        tab.add_row(
            str(r.get("id", "")),
            f"{icona} {r.get('tipo','')}",
            f"[{colore}]{r.get('valore','')}[/]",
            nota,
            r.get("data", ""),
        )

    console.print(tab)
    console.print(f"  [dim]Totale: {len(righe)} voci[/]")


def mostra_semaforo(risultato: dict, titolo_offerta: str = "") -> None:
    """Mostra il semaforo whitelist/blacklist per un testo di offerta."""
    s      = risultato["semaforo"]
    bianchi = risultato["white"]
    neri    = risultato["black"]

    if s == "rosso":
        icona, colore = "🔴", "red"
        msg = "ATTENZIONE: trovati elementi sospetti/blacklistati"
    elif s == "verde":
        icona, colore = "🟢", "green"
        msg = "Azienda o keyword presente nella whitelist"
    else:
        icona, colore = "🟡", "yellow"
        msg = "Nessuna corrispondenza nelle liste"

    contenuto = f"[bold {colore}]{icona}  {msg}[/]\n"

    if neri:
        contenuto += "\n[red]🚫  Blacklist — trovato:[/]\n"
        for r in neri:
            motivo = r.get("motivo", "")
            contenuto += f"  • [red]{r['valore']}[/]  [dim]{motivo}[/]\n"

    if bianchi:
        contenuto += "\n[green]✅  Whitelist — trovato:[/]\n"
        for r in bianchi:
            nota = r.get("nota", "")
            contenuto += f"  • [green]{r['valore']}[/]  [dim]{nota}[/]\n"

    t = f"[bold]Liste — {titolo_offerta[:50]}[/]" if titolo_offerta else "[bold]Controllo Liste[/]"
    console.print(Panel(contenuto.strip(), title=t, border_style=colore))


# ── Menu gestione liste ───────────────────────────────────────────────────────

def menu_gestione_liste() -> None:
    while True:
        os.system("cls" if sys.platform == "win32" else "clear")

        n_white = len(leggi_lista("white"))
        n_black = len(leggi_lista("black"))

        console.print(Panel(
            f"[bold]Gestione Liste Lavoro[/]\n\n"
            f"  [green]WHITELIST[/]  ({n_white} voci) — aziende/keyword affidabili\n"
            f"  [red]BLACKLIST[/]  ({n_black} voci) — aziende/keyword da evitare\n\n"
            f"  [green]1.[/] Visualizza Whitelist\n"
            f"  [red]2.[/] Visualizza Blacklist\n\n"
            f"  [green]3.[/] Aggiungi alla Whitelist\n"
            f"  [red]4.[/] Aggiungi alla Blacklist\n\n"
            f"  [dim]5.[/] Rimuovi da una lista\n"
            f"  [dim]6.[/] Controlla un testo manualmente\n\n"
            f"  Prismalux  ›  Pratico  ›  Cerca Lavoro  ›  Liste\n"
            f"  [dim]1–6 apri   ·   0 torna   ·   Q esci[/]\n"
            f"  ❯ ",
            title="[bold blue]Liste Lavoro[/]",
            border_style="blue",
            padding=(1, 2, 0, 2),
        ))

        try:
            scelta = input_nel_box(2)
        except Ridimensiona:
            continue

        if scelta == "0":
            break

        elif scelta == "1":
            mostra_lista("white")
            input("\n  [INVIO per continuare] ")

        elif scelta == "2":
            mostra_lista("black")
            input("\n  [INVIO per continuare] ")

        elif scelta in ("3", "4"):
            lista  = "white" if scelta == "3" else "black"
            colore = "green" if lista == "white" else "red"
            nome   = "Whitelist" if lista == "white" else "Blacklist"

            console.print(f"\n  [bold {colore}]Aggiungi alla {nome}[/]")
            console.print("  Tipo:  1=Azienda  2=Keyword  3=Truffa (solo blacklist)")
            tipo = {"1": "azienda", "2": "keyword", "3": "truffa"}.get(
                input("  Tipo [1/2/3]: ").strip(), "azienda"
            )
            valore = input("  Valore (nome azienda o parola chiave): ").strip()
            if not valore:
                continue
            col_nota = "Nota" if lista == "white" else "Motivo"
            nota = input(f"  {col_nota} (opzionale): ").strip()

            if aggiungi(lista, tipo, valore, nota):
                console.print(f"  [bold {colore}]✓ '{valore}' aggiunto alla {nome}[/]")
            else:
                console.print(f"  [yellow]'{valore}' è già presente.[/]")
            input("  [INVIO per continuare] ")

        elif scelta == "5":
            console.print("\n  Da quale lista? 1=Whitelist  2=Blacklist")
            sl = input("  Lista [1/2]: ").strip()
            lista = "white" if sl == "1" else "black"
            mostra_lista(lista)
            valore = input("\n  Valore esatto da rimuovere: ").strip()
            if valore:
                if rimuovi(lista, valore):
                    console.print(f"  [green]✓ '{valore}' rimosso.[/]")
                else:
                    console.print(f"  [yellow]Voce non trovata.[/]")
            input("  [INVIO per continuare] ")

        elif scelta == "6":
            console.print("\n  Incolla il testo da controllare (FINE su riga vuota per terminare):")
            righe = []
            while True:
                riga = input()
                if riga.strip().upper() == "FINE":
                    break
                righe.append(riga)
            testo = "\n".join(righe)
            if testo.strip():
                mostra_semaforo(valuta_offerta(testo), "Testo inserito")
            input("\n  [INVIO per continuare] ")


if __name__ == "__main__":
    try:
        menu_gestione_liste()
    except KeyboardInterrupt:
        console.print("\n\n  Uscita con Ctrl+C.\n")

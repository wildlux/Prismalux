"""
Agente 730 — Dichiarazione dei Redditi
=========================================
Guida alla dichiarazione 730 in Italia:
- Chi può usarla
- Come presentarla
- Detrazioni e deduzioni principali
- Scadenze
- Q&A con Ollama + ricerca web aggiornata

⚠️  DISCLAIMER: Questo strumento è puramente informativo.
    Per la tua situazione specifica consulta sempre un CAF
    o un commercialista abilitato.

Dipendenze:
  pip install rich requests duckduckgo-search
"""

import os
import sys
import signal
from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich.markdown import Markdown
try:
    from check_deps import Ridimensiona, input_nel_box, stampa_breadcrumb
except ImportError:
    class Ridimensiona(BaseException): pass
    def input_nel_box(padding_left=2): return input("\n  ❯ ")
    def stampa_breadcrumb(con, percorso): print(f"  {percorso}")

console = Console()

_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if os.path.join(_ROOT, "core") not in sys.path:
    sys.path.insert(0, os.path.join(_ROOT, "core"))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)
from ollama_utils import chiedi_ollama, cerca_web


# ── Guide statiche ────────────────────────────────────────────────────────────

def guida_cos_e_730() -> None:
    console.print(Panel(
        Markdown("""
## Cos'è il modello 730

Il **730** è la dichiarazione dei redditi per lavoratori **dipendenti** e **pensionati**.

### Chi PUÒ usarlo
- Lavoratori dipendenti (con contratto a tempo determinato o indeterminato)
- Pensionati
- Chi ha anche redditi da terreni, fabbricati, lavoro autonomo occasionale
- Collaboratori coordinati e continuativi (co.co.co.)

### Chi NON può usarlo
- Chi ha solo Partita IVA in regime ordinario
- Chi ha redditi da impresa
- Chi non è residente in Italia

### Perché conviene
- Il rimborso (se spetta) arriva direttamente in busta paga o pensione
- Se devi versare, paghi a rate senza interessi
- È **gratuito** tramite CAF o patronato
"""),
        title="[bold yellow]Cos'è il 730[/]",
        border_style="yellow"
    ))


def guida_come_presentare() -> None:
    console.print(Panel(
        Markdown("""
## Come presentare il 730

### 3 modi per presentarlo

**1. CAF (Centro di Assistenza Fiscale)** ⭐ Consigliato
- Gratuito per lavoratori dipendenti con reddito sotto soglia
- Porti i documenti, loro compilano tutto
- Trovi i CAF di CGIL, CISL, UIL, ACLI in tutta Italia

**2. Patronato / Sindacato**
- Gratuito, simile al CAF
- Utile se sei già iscritto a un sindacato

**3. 730 Precompilato (sul sito INPS/Agenzia Entrate)**
- Fai da solo online su: agenziaentrate.gov.it
- L'Agenzia pre-compila con i dati già in suo possesso
- Devi solo verificare e integrare
- Richiede SPID o CIE

**4. Commercialista**
- A pagamento (circa €50–150)
- Utile se la situazione è complessa

### Documenti da portare
- CU (Certificazione Unica) dal tuo datore di lavoro
- Scontrini/ricevute spese mediche
- Estratto conto mutuo (se hai mutuo prima casa)
- Ricevute ristrutturazioni, bonus energetici
- Spese istruzione (università, scuola privata)
- Codice fiscale figli a carico
"""),
        title="[bold yellow]Come Presentare il 730[/]",
        border_style="yellow"
    ))


def guida_detrazioni() -> None:
    tabella = Table(title="Principali Detrazioni e Deduzioni 730",
                    border_style="yellow", show_lines=True)
    tabella.add_column("Voce", style="bold white", width=30)
    tabella.add_column("Percentuale", style="green", width=15)
    tabella.add_column("Limite massimo", style="cyan", width=20)
    tabella.add_column("Note", style="dim white", width=35)

    detrazioni = [
        ("Spese mediche",         "19%",  "Spesa - €129,11",     "Ticket, farmaci, specialisti, occhiali"),
        ("Mutuo prima casa",      "19%",  "Max €4.000/anno",      "Solo interessi passivi"),
        ("Figli a carico",        "Variabile", "Per figlio",      "Sotto 21 anni, reddito < €2.840,51"),
        ("Assicurazione vita",    "19%",  "Max €530/anno",        "Solo rischio morte/invalidità"),
        ("Spese università",      "19%",  "Equiv. statale",       "Anche atenei privati fino a limite"),
        ("Spese scolastiche",     "19%",  "Max €800/anno",        "Scuole paritarie, mense, gite"),
        ("Ristrutturazione casa", "50%",  "Max €96.000",          "In 10 anni, Superbonus 110% in esaurimento"),
        ("Bonus energetico",      "65%",  "Varia per intervento", "Isolamento, caldaia, pannelli solari"),
        ("Bonus mobili",          "50%",  "Max €5.000",           "Legato a ristrutturazione"),
        ("Donazioni ONLUS",       "30%",  "Max €30.000/anno",     "Solo enti riconosciuti"),
        ("Spese veterinarie",     "19%",  "Max €550 - €129,11",   "Animali da compagnia"),
        ("Abbonamento TPL",       "19%",  "Max €250/anno",        "Bus, metro, treno abbonamento"),
    ]

    for voce, perc, limite, note in detrazioni:
        tabella.add_row(voce, perc, limite, note)

    console.print(tabella)
    console.print()
    console.print(Panel(
        "[dim]💡 [bold]Differenza detrazione / deduzione:[/]\n"
        "   Detrazione = si sottrae dall'[bold]imposta[/] da pagare\n"
        "   Deduzione  = si sottrae dal [bold]reddito[/] imponibile[/]",
        border_style="dim"
    ))


def guida_scadenze() -> None:
    tabella = Table(title="Scadenze 730", border_style="yellow", show_lines=True)
    tabella.add_column("Scadenza", style="bold red", width=25)
    tabella.add_column("Cosa fare", style="white", width=50)

    tabella.add_row("30 aprile",        "Ricezione CU dal datore di lavoro")
    tabella.add_row("30 aprile",        "Disponibile 730 precompilato su AgE")
    tabella.add_row("31 maggio",        "Presentazione al CAF/patronato (consigliato)")
    tabella.add_row("30 settembre",     "Termine per presentazione tramite CAF")
    tabella.add_row("31 ottobre",       "Termine per 730 autonomo online")
    tabella.add_row("Luglio (busta paga)", "Rimborso IRPEF se spetta")
    tabella.add_row("Agosto–novembre", "Trattenuta in busta paga se devi versare")

    console.print(tabella)
    console.print(Panel(
        "[yellow]⚠️  Le date esatte possono variare ogni anno.[/]\n"
        "    Verifica sempre su: [blue]agenziaentrate.gov.it[/]",
        border_style="dim"
    ))


# ── Cerca info aggiornate online ──────────────────────────────────────────────

def cerca_news_730() -> None:
    console.print("\n  [dim]Cerco notizie aggiornate sul 730...[/]\n")
    query = "730 dichiarazione redditi 2025 novità scadenze detrazioni"
    risultati = cerca_web(query, 6)

    if not risultati:
        console.print("[yellow]Nessun risultato. Controlla la connessione.[/]")
        return

    console.print(Panel("[bold]Ultime notizie sul 730[/]", border_style="yellow"))
    for r in risultati:
        console.print(f"\n  [bold yellow]{r.get('title', '')}[/]")
        console.print(f"  [blue underline]{r.get('href', '')}[/]")
        console.print(f"  [dim]{r.get('body', '')[:200]}...[/]")


# ── Q&A con Ollama ────────────────────────────────────────────────────────────

def domanda_libera_730() -> None:
    console.print("\n  Fai la tua domanda sul 730 (es. 'posso detrarre l'occhiale?'):")
    domanda = input("  ❯ ").strip()
    if not domanda:
        return

    console.print("\n  [dim]Cerco la risposta...[/]\n")

    prompt = f"""Sei un esperto fiscalista italiano. Rispondi a questa domanda sul modello 730:

DOMANDA: {domanda}

Regole:
1. Rispondi in italiano semplice e chiaro
2. Se è una domanda su detrazioni/deduzioni, indica percentuale e limite
3. Se ci sono condizioni particolari, elencale brevemente
4. Termina sempre con: "Per la tua situazione specifica, consulta un CAF o commercialista."

Sii preciso ma comprensibile anche per chi non è esperto di tasse."""

    risposta = chiedi_ollama(prompt, "risposta")
    console.print(Panel(
        Markdown(risposta),
        title="[bold yellow]Risposta[/]",
        border_style="yellow"
    ))
    console.print(Panel(
        "[dim]⚠️  Informazione a scopo orientativo. "
        "Consulta un CAF o commercialista per la tua situazione.[/]",
        border_style="dim"
    ))


# ── Menu ──────────────────────────────────────────────────────────────────────

def menu_730() -> None:
    while True:
        os.system("cls" if sys.platform == "win32" else "clear")
        stampa_breadcrumb(console, "Prismalux › 🛠 Pratico › 730")
        console.print(Panel(
            "[bold]Agente 730 — Dichiarazione dei Redditi[/]\n\n"
            "  1. Cos'è il 730 e chi può usarlo\n"
            "  2. Come presentarlo (CAF, online, commercialista)\n"
            "  3. Detrazioni e deduzioni (tabella completa)\n"
            "  4. Scadenze importanti\n"
            "  5. Cerca notizie aggiornate online\n"
            "  6. Fai una domanda libera\n\n"
            "  [dim]⚠️  Solo informativo — consulta un CAF per la tua situazione[/]\n\n"
            "  [dim]1–6 apri   ·   0 torna   ·   Q esci[/]\n"
            "  ❯ ",
            title="[bold yellow]730 — Dichiarazione dei Redditi[/]",
            border_style="yellow",
            padding=(1, 2, 0, 2),
        ))

        try:
            scelta = input_nel_box(2)
        except Ridimensiona:
            continue

        if scelta == "0":
            break
        elif scelta == "1":
            guida_cos_e_730()
        elif scelta == "2":
            guida_come_presentare()
        elif scelta == "3":
            guida_detrazioni()
        elif scelta == "4":
            guida_scadenze()
        elif scelta == "5":
            cerca_news_730()
        elif scelta == "6":
            domanda_libera_730()

        if scelta in ("1", "2", "3", "4", "5", "6"):
            input("\n  [INVIO per tornare al menu] ")


if __name__ == "__main__":
    try:
        menu_730()
    except KeyboardInterrupt:
        console.print("\n\n  🍺 Alla prossima libagione di sapere.\n")

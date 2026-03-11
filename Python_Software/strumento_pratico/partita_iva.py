"""
Agente Partita IVA
====================
Guida completa per aprire e gestire la Partita IVA in Italia:
- Come aprirla (passo per passo)
- Regime forfettario vs ordinario
- Calcolo tasse stimate
- Contributi INPS
- Obblighi e scadenze
- Q&A con Ollama

⚠️  DISCLAIMER: Questo strumento è puramente informativo.
    Per la tua situazione specifica consulta un commercialista.

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

def guida_apertura() -> None:
    console.print(Panel(
        Markdown("""
## Come aprire la Partita IVA

### È gratis e si fa in pochi passi

**Passo 1 — Scegli il Codice ATECO**
Il codice ATECO identifica la tua attività (es. 62.01.00 = sviluppo software).
Cercalo su: [ateco.infocamere.it](https://ateco.infocamere.it)

**Passo 2 — Scegli la Cassa Previdenziale**
- Liberi professionisti senza cassa → INPS Gestione Separata
- Alcune categorie hanno casse proprie (es. avvocati, medici, ingegneri)

**Passo 3 — Apri la Partita IVA**
Tre modi (tutti **gratuiti**):

| Metodo | Come | Tempo |
|--------|------|-------|
| Online | agenziaentrate.gov.it con SPID | Immediato |
| Di persona | Ufficio Agenzia delle Entrate | 1 giorno |
| Tramite commercialista | Ti pensa lui | 1–2 giorni |

**Passo 4 — Comunica l'apertura all'INPS**
Entro 30 giorni dall'apertura, se sei nella Gestione Separata INPS.

**Passo 5 — Attiva la fatturazione elettronica**
Obbligatoria per tutti. Puoi usare:
- Fatture in Cloud (freemium)
- Aruba Sign (economico)
- Agenzia delle Entrate (gratis ma basico)

### ⏱️ Totale: puoi aprirla in 30 minuti online
"""),
        title="[bold magenta]Come Aprire la Partita IVA[/]",
        border_style="magenta"
    ))


def guida_regimi() -> None:
    tabella = Table(title="Confronto Regimi Fiscali", border_style="magenta", show_lines=True)
    tabella.add_column("Caratteristica", style="bold white", width=28)
    tabella.add_column("Forfettario ⭐", style="green", width=30)
    tabella.add_column("Ordinario", style="yellow", width=30)

    righe = [
        ("Limite ricavi annui",     "€ 85.000",                         "Nessun limite"),
        ("Aliquota IRPEF",          "15% flat (5% primi 5 anni*)",      "Scaglioni 23%–43%"),
        ("IVA",                     "Non si addebita, non si scarica",   "Si addebita (22%) e si scarica"),
        ("Costi deducibili",        "Solo % forfettaria per categoria",  "Tutti i costi reali"),
        ("Fatturazione elettron.",  "Obbligatoria",                      "Obbligatoria"),
        ("Contabilità",             "Semplificata",                      "Ordinaria o semplificata"),
        ("Studi di settore / ISA",  "Escluso",                          "Incluso"),
        ("Conveniente se...",       "Pochi costi, ricavi < €85k",       "Molti costi da dedurre"),
    ]

    for riga in righe:
        tabella.add_row(*riga)

    console.print(tabella)
    console.print(Panel(
        "[dim]* Il 5% è per i primi 5 anni se:\n"
        "  - Nuova attività non esercitata negli ultimi 3 anni\n"
        "  - Non prosecuzione di attività precedente[/]",
        border_style="dim"
    ))


def guida_inps() -> None:
    console.print(Panel(
        Markdown("""
## Contributi INPS — Gestione Separata

Se non hai una cassa professionale specifica, versi all'INPS.

### Aliquote 2024 (circa)
| Situazione | Aliquota |
|---|---|
| Senza altra copertura previdenziale | ~26,23% |
| Pensionato o con altra previdenza | ~24% |

### Come si calcola
```
Reddito netto imponibile × aliquota = contributi INPS annui
```

### Esempio pratico (forfettario, €30.000 fatturato)
```
Coefficiente forfettario (es. software 78%):
Imponibile = €30.000 × 78% = €23.400
INPS = €23.400 × 26,23% = ~€6.138/anno
IRPEF = €23.400 × 15% = €3.510/anno
Totale tasse+contributi = ~€9.648/anno
Rimane in tasca = €20.352
```

### Scadenze versamenti INPS
- **30 giugno**: acconto (o unica soluzione se < €257,52)
- **30 novembre**: saldo + acconto anno successivo

### Attenzione
La Gestione Separata non garantisce la stessa pensione del lavoro dipendente.
Valuta sempre di fare previdenza complementare.
"""),
        title="[bold magenta]Contributi INPS[/]",
        border_style="magenta"
    ))


def guida_obblighi() -> None:
    console.print(Panel(
        Markdown("""
## Obblighi con la Partita IVA

### Ogni volta che fatturi
- Emetti **fattura elettronica** entro 12 giorni dalla prestazione
- Conserva copia per 10 anni

### Ogni anno
| Adempimento | Scadenza | Note |
|---|---|---|
| Dichiarazione dei redditi | Ottobre | Modello Redditi PF |
| Versamento IRPEF | Giugno / Novembre | Acconto + saldo |
| Versamento INPS | Giugno / Novembre | Acconto + saldo |
| Comunicazione dati IVA | Febbraio | Solo regime ordinario |

### Regime forfettario: cosa NON devi fare
- ❌ Non devi versare l'IVA
- ❌ Non devi fare registri IVA
- ❌ Non devi fare ritenuta d'acconto (di solito)
- ✅ Devi solo: fattura elettronica + dichiarazione annuale + INPS

### Sanzioni più comuni da evitare
- Fattura in ritardo: sanzione dal 90% al 180% dell'imposta
- Omessa dichiarazione: sanzione dal 120% al 240%
- Superamento €85.000: si perde il forfettario dall'anno dopo
"""),
        title="[bold magenta]Obblighi e Scadenze[/]",
        border_style="magenta"
    ))


# ── Calcolatore tasse ─────────────────────────────────────────────────────────

def calcola_tasse() -> None:
    console.print(Panel(
        "[bold]Calcolatore tasse — Regime Forfettario[/]\n"
        "[dim]Stima approssimativa, non sostituisce consulenza professionale[/]",
        border_style="magenta"
    ))

    # Coefficienti forfettari per categoria
    categorie = {
        "1": ("Sviluppo software / IT",          0.78),
        "2": ("Consulenza / Formazione",          0.78),
        "3": ("Commercio al dettaglio",           0.40),
        "4": ("Artigianato",                      0.67),
        "5": ("Professioni sanitarie",            0.78),
        "6": ("Attività di ristorazione",         0.40),
        "7": ("Agenti di commercio",              0.62),
        "8": ("Altra categoria (inserisci %)",    0.00),
    }

    console.print("\n  Scegli la tua categoria:")
    for k, (nome, coeff) in categorie.items():
        perc = f"{int(coeff*100)}%" if coeff > 0 else "?"
        console.print(f"  {k}. {nome} (coeff. {perc})")

    scelta_cat = input("\n  Categoria [1-8]: ").strip()
    nome_cat, coefficiente = categorie.get(scelta_cat, ("Altra", 0.78))

    if scelta_cat == "8":
        try:
            coefficiente = float(input("  Inserisci il coefficiente (es. 0.78 per 78%): ").strip())
        except ValueError:
            coefficiente = 0.78

    try:
        fatturato = float(input("\n  Fatturato annuo previsto (€): ").strip().replace(",", "."))
    except ValueError:
        console.print("[red]Importo non valido.[/]")
        return

    nuova_attivita = input("  È una nuova attività (< 3 anni)? [s/n]: ").strip().lower() == "s"
    aliquota_irpef = 0.05 if nuova_attivita else 0.15
    aliquota_inps  = 0.2623  # gestione separata

    # Calcolo
    imponibile      = fatturato * coefficiente
    irpef           = imponibile * aliquota_irpef
    inps            = imponibile * aliquota_inps
    totale_tasse    = irpef + inps
    netto           = fatturato - totale_tasse
    percentuale_eff = (totale_tasse / fatturato * 100) if fatturato > 0 else 0

    # Risultato
    tabella = Table(title=f"Stima Tasse — {nome_cat}", border_style="magenta", show_lines=True)
    tabella.add_column("Voce", style="bold white", width=30)
    tabella.add_column("Importo", style="cyan", width=20)

    tabella.add_row("Fatturato annuo",          f"€ {fatturato:,.2f}")
    tabella.add_row(f"Reddito imponibile ({int(coefficiente*100)}%)", f"€ {imponibile:,.2f}")
    tabella.add_row(f"IRPEF ({int(aliquota_irpef*100)}%)",     f"€ {irpef:,.2f}")
    tabella.add_row(f"INPS ({int(aliquota_inps*100)}%)",        f"€ {inps:,.2f}")
    tabella.add_row("[bold red]Totale tasse + contributi[/]",  f"[bold red]€ {totale_tasse:,.2f}[/]")
    tabella.add_row("[bold green]Netto in tasca[/]",           f"[bold green]€ {netto:,.2f}[/]")
    tabella.add_row("Pressione fiscale effettiva",             f"{percentuale_eff:.1f}%")

    console.print(tabella)

    if fatturato > 85000:
        console.print(Panel(
            "[bold red]⚠️  Attenzione: superi il limite €85.000 del regime forfettario![/]\n"
            "    Dovrai passare al regime ordinario dall'anno successivo.",
            border_style="red"
        ))

    console.print(Panel(
        "[dim]Stima approssimativa basata su coefficiente forfettario.\n"
        "Non include eventuali deduzioni, riduzioni INPS per nuove attività,\n"
        "casse professionali specifiche. Consulta un commercialista.[/]",
        border_style="dim"
    ))


# ── Cerca news online ─────────────────────────────────────────────────────────

def cerca_news_piva() -> None:
    console.print("\n  [dim]Cerco notizie aggiornate sulla Partita IVA...[/]\n")
    query = "partita IVA regime forfettario 2025 novità aggiornamenti"
    risultati = cerca_web(query, 6)

    if not risultati:
        console.print("[yellow]Nessun risultato. Controlla la connessione.[/]")
        return

    console.print(Panel("[bold]Ultime notizie sulla Partita IVA[/]", border_style="magenta"))
    for r in risultati:
        console.print(f"\n  [bold magenta]{r.get('title', '')}[/]")
        console.print(f"  [blue underline]{r.get('href', '')}[/]")
        console.print(f"  [dim]{r.get('body', '')[:200]}...[/]")


# ── Q&A libera ────────────────────────────────────────────────────────────────

def domanda_libera_piva() -> None:
    console.print("\n  Fai la tua domanda sulla Partita IVA:")
    domanda = input("  ❯ ").strip()
    if not domanda:
        return

    console.print("\n  [dim]Cerco la risposta...[/]\n")

    prompt = f"""Sei un commercialista italiano esperto di partita IVA e fisco.
Rispondi a questa domanda in modo chiaro e pratico:

DOMANDA: {domanda}

Regole:
1. Rispondi in italiano semplice
2. Sii specifico per la normativa italiana
3. Se ci sono condizioni o eccezioni, elencale brevemente
4. Se la risposta dipende dalla situazione, spiega le variabili principali
5. Termina sempre con: "Per la tua situazione specifica, consulta un commercialista."

Sii preciso e comprensibile anche per chi non è esperto di fisco."""

    risposta = chiedi_ollama(prompt, "risposta")
    console.print(Panel(
        Markdown(risposta),
        title="[bold magenta]Risposta[/]",
        border_style="magenta"
    ))
    console.print(Panel(
        "[dim]⚠️  Informazione a scopo orientativo. "
        "Consulta un commercialista per la tua situazione specifica.[/]",
        border_style="dim"
    ))


# ── Menu ──────────────────────────────────────────────────────────────────────

def menu_partita_iva() -> None:
    while True:
        os.system("cls" if sys.platform == "win32" else "clear")
        stampa_breadcrumb(console, "Prismalux › 🛠 Pratico › Partita IVA")
        console.print(Panel(
            "[bold]Agente Partita IVA[/]\n\n"
            "  1. Come aprirla (passo per passo)\n"
            "  2. Regime forfettario vs ordinario\n"
            "  3. Contributi INPS — quanto pago?\n"
            "  4. Obblighi e scadenze annuali\n"
            "  5. Calcola le tue tasse (stima)\n"
            "  6. Cerca notizie aggiornate online\n"
            "  7. Fai una domanda libera\n\n"
            "  [dim]⚠️  Solo informativo — consulta un commercialista[/]\n\n"
            "  [dim]1–7 apri   ·   0 torna   ·   Q esci[/]\n"
            "  ❯ ",
            title="[bold magenta]Partita IVA[/]",
            border_style="magenta",
            padding=(1, 2, 0, 2),
        ))

        try:
            scelta = input_nel_box(2)
        except Ridimensiona:
            continue

        if scelta == "0":
            break
        elif scelta == "1":
            guida_apertura()
        elif scelta == "2":
            guida_regimi()
        elif scelta == "3":
            guida_inps()
        elif scelta == "4":
            guida_obblighi()
        elif scelta == "5":
            calcola_tasse()
        elif scelta == "6":
            cerca_news_piva()
        elif scelta == "7":
            domanda_libera_piva()

        if scelta in ("1", "2", "3", "4", "5", "6", "7"):
            input("\n  [INVIO per tornare al menu] ")


if __name__ == "__main__":
    try:
        menu_partita_iva()
    except KeyboardInterrupt:
        console.print("\n\n  🍺 Alla prossima libagione di sapere.\n")

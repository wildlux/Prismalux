"""
Agente Cerca Lavoro
====================
Cerca offerte di lavoro in Italia, aiuta a scrivere lettere
di presentazione e prepara al colloquio.

Dipendenze:
  pip install rich requests ddgs
  Ollama opzionale (per lettere e colloquio)
"""

import os
import sys
import time
try:
    from check_deps import Ridimensiona, input_nel_box, stampa_breadcrumb
except ImportError:
    class Ridimensiona(BaseException): pass
    def input_nel_box(padding_left=2): return input("\n  ❯ ")
    def stampa_breadcrumb(con, percorso): print(f"  {percorso}")
from rich.console import Console
from rich.panel import Panel
from rich.markdown import Markdown
from rich.table import Table
from cv_reader import scegli_ruolo_da_cv, trova_cv
from db_lavoro import valuta_offerta, mostra_semaforo, aggiungi

# ── Client Ollama ottimizzato ─────────────────────────────────────────────────
_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)
from ollama_utils import chiedi_ollama, chiedi_parallelo, avvia_warmup

console = Console()

# Verifica se ddgs è installato
try:
    from ddgs import DDGS
    DDGS_OK = True
except ImportError:
    DDGS_OK = False


# ── Mappa titoli italiani → equivalenti inglesi ───────────────────────────────
# Usata come fallback automatico quando la ricerca italiana non trova nulla.

TITOLI_EN: dict[str, list[str]] = {
    # Informatica / IT generale
    "perito informatico":       ["IT technician", "computer technician", "IT support specialist"],
    "tecnico informatico":      ["IT technician", "technical support", "helpdesk technician"],
    "informatico":              ["IT specialist", "IT professional", "computer scientist"],
    "ingegnere informatico":    ["computer engineer", "software engineer", "IT engineer"],
    # Sviluppo software
    "sviluppatore":             ["software developer", "software engineer", "developer"],
    "sviluppatore python":      ["Python developer", "Python software engineer"],
    "programmatore":            ["programmer", "software developer", "software engineer"],
    "sviluppatore web":         ["web developer", "frontend developer", "full stack developer"],
    "frontend developer":       ["frontend developer", "UI developer", "React developer"],
    "backend developer":        ["backend developer", "server-side developer", "API developer"],
    "full stack":               ["full stack developer", "full stack engineer"],
    # Sistemistica / reti
    "sistemista":               ["system administrator", "sysadmin", "IT administrator"],
    "amministratore di sistema":["system administrator", "sysadmin", "infrastructure engineer"],
    "network engineer":         ["network engineer", "network administrator", "network technician"],
    "amministratore di rete":   ["network administrator", "network engineer"],
    # Supporto
    "help desk":                ["help desk technician", "IT support specialist", "service desk analyst"],
    "supporto tecnico":         ["technical support", "IT support", "help desk technician"],
    "assistenza tecnica":       ["technical support specialist", "IT support engineer"],
    # Data / AI
    "data scientist":           ["data scientist", "machine learning engineer", "AI engineer"],
    "analista dati":            ["data analyst", "business intelligence analyst", "BI analyst"],
    "data engineer":            ["data engineer", "data pipeline engineer", "ETL developer"],
    # Gestione
    "project manager":          ["project manager", "IT project manager", "scrum master"],
    "consulente it":            ["IT consultant", "technology consultant", "IT advisor"],
    "analista":                 ["IT analyst", "business analyst", "systems analyst"],
    "analista programmatore":   ["programmer analyst", "software analyst", "IT analyst"],
    # Sicurezza
    "cybersecurity":            ["cybersecurity analyst", "information security engineer", "SOC analyst"],
    "sicurezza informatica":    ["cybersecurity specialist", "IT security analyst"],
    # DevOps / Cloud
    "devops":                   ["DevOps engineer", "site reliability engineer", "SRE"],
    "cloud engineer":           ["cloud engineer", "cloud architect", "AWS engineer"],
}

def titoli_inglesi(ruolo: str) -> list[str]:
    """Restituisce i titoli inglesi per un ruolo italiano, cercando per parole chiave."""
    ruolo_lower = ruolo.lower().strip()

    # Corrispondenza esatta
    if ruolo_lower in TITOLI_EN:
        return TITOLI_EN[ruolo_lower]

    # Corrispondenza parziale (es. "perito informatico junior" → "perito informatico")
    for chiave, titoli in TITOLI_EN.items():
        if chiave in ruolo_lower or ruolo_lower in chiave:
            return titoli

    # Nessuna corrispondenza: restituisce versione inglese generica
    return [ruolo, f"{ruolo} specialist", f"{ruolo} technician"]


def _chiedi_e_aggiungi_azienda() -> None:
    """Chiede all'utente se vuole aggiungere un'azienda alla whitelist/blacklist."""
    console.print("\n  Vuoi aggiungere un'azienda alle liste? [s/n]: ", end="")
    if input().strip().lower() != "s":
        return
    nome_az = input("  Nome azienda: ").strip()
    if not nome_az:
        return
    console.print("  1=Whitelist (affidabile)  2=Blacklist (da evitare)")
    sl = input("  Lista [1/2]: ").strip()
    if sl not in ("1", "2"):
        return
    lista    = "white" if sl == "1" else "black"
    col_nota = "Nota" if lista == "white" else "Motivo"
    nota     = input(f"  {col_nota}: ").strip()
    if aggiungi(lista, "azienda", nome_az, nota):
        console.print(f"  [green]✓ '{nome_az}' aggiunto.[/]")
    else:
        console.print(f"  [yellow]Già presente.[/]")


# ── Portali di lavoro e aziende specifiche ────────────────────────────────────

PORTALI_LAVORO = [
    # ── Portali / agenzie generali ────────────────────────────────────────────
    # ── Portali internazionali ────────────────────────────────────────────────
    {"id":  1, "nome": "LinkedIn Jobs",          "dominio": "linkedin.com/jobs",      "tipo": "portale"},
    {"id":  2, "nome": "Indeed",                 "dominio": "indeed.com",             "tipo": "portale"},
    {"id":  3, "nome": "Wellfound",              "dominio": "wellfound.com",          "tipo": "portale"},
    {"id":  4, "nome": "Welcome to the Jungle",  "dominio": "welcometothejungle.com", "tipo": "portale"},
    {"id":  5, "nome": "AlmaLaurea",             "dominio": "almalaurea.it",          "tipo": "portale"},
    {"id":  6, "nome": "JoinRS",                 "dominio": "joinrs.com",             "tipo": "portale"},
    {"id":  7, "nome": "Jobros",                 "dominio": "jobros.it",              "tipo": "portale"},
    # ── Portali italiani ──────────────────────────────────────────────────────
    {"id":  8, "nome": "InfoJobs",               "dominio": "infojobs.it",            "tipo": "portale"},
    {"id":  9, "nome": "Monster",                "dominio": "monster.it",             "tipo": "portale"},
    {"id": 10, "nome": "TuttoJobs",              "dominio": "tuttojobs.com",          "tipo": "portale"},
    {"id": 11, "nome": "Lavoro Corriere",        "dominio": "corriere.it/lavoro",     "tipo": "portale"},
    # ── Agenzie del lavoro ────────────────────────────────────────────────────
    {"id": 12, "nome": "Adecco",                 "dominio": "adecco.com",             "tipo": "agenzia"},
    {"id": 13, "nome": "Randstad",               "dominio": "randstad.it",            "tipo": "agenzia"},
    {"id": 14, "nome": "Gi Group",               "dominio": "gigroup.com",            "tipo": "agenzia"},
    {"id": 15, "nome": "Manpower",               "dominio": "manpower.it",            "tipo": "agenzia"},
    {"id": 16, "nome": "Synergie",               "dominio": "synergie-italia.it",     "tipo": "agenzia"},
    # ── Sicilia / Catania ─────────────────────────────────────────────────────
    {"id": 17, "nome": "SiciliaLavoro",          "dominio": "sicilialavoro.it",       "tipo": "regionale"},
    {"id": 18, "nome": "Subito (lavoro)",         "dominio": "subito.it",              "tipo": "regionale",
     "query_extra": "Sicilia Catania"},
    {"id": 19, "nome": "Bakeca (lavoro)",         "dominio": "bakeca.it",              "tipo": "regionale",
     "query_extra": "Catania Sicilia"},
    # ── Aziende specifiche ────────────────────────────────────────────────────
    {"id": 20, "nome": "Leroy Merlin",           "dominio": "leroymerlin.it",         "tipo": "azienda"},
    {"id": 21, "nome": "IKEA",                   "dominio": "ikea.com",               "tipo": "azienda"},
    {"id": 22, "nome": "Mondoconv",              "dominio": "mondoconv.it",           "tipo": "azienda"},
    {"id": 23, "nome": "Conad",                  "dominio": "conad.it",               "tipo": "azienda"},
    {"id": 24, "nome": "Coop",                   "dominio": "coop.it",                "tipo": "azienda"},
    {"id": 25, "nome": "Gruppo Arena",           "dominio": "gruppoarena.it",         "tipo": "azienda"},
    {"id": 26, "nome": "STMicroelectronics",     "dominio": "careers.st.com",         "tipo": "azienda",
     "query_extra": ""},
]

_PORTALE_MAP = {str(p["id"]): p for p in PORTALI_LAVORO}


def _queries_portale(portale: dict, ruolo: str, citta: str, remoto: bool) -> list[str]:
    """Costruisce le query DuckDuckGo con site: per un singolo portale/azienda."""
    dominio = portale["dominio"]
    tipo    = portale["tipo"]
    extra   = portale.get("query_extra", "")

    if tipo == "azienda":
        # Aziende: cerca prima con ruolo, poi aperture generali
        qs = [f'site:{dominio} offerta lavoro posizioni aperte 2025']
        if ruolo:
            qs.insert(0, f'site:{dominio} "{ruolo}" {extra}'.strip())
        return qs

    if tipo == "regionale":
        # Portali locali Sicilia/Catania: priorità alla zona geografica
        r   = ruolo or "lavoro"
        loc = citta or "Sicilia"
        return [
            f'site:{dominio} "{r}" "{loc}" offerta lavoro'.strip(),
            f'site:{dominio} "{r}" Sicilia offerta lavoro {extra}'.strip(),
        ]

    # Portali generali / agenzie
    r = ruolo or "lavoro"
    if remoto:
        return [f'site:{dominio} "{r}" remoto smart working Italia {extra}'.strip()]
    elif citta:
        return [
            f'site:{dominio} "{r}" "{citta}" {extra}'.strip(),
            f'site:{dominio} "{r}" Italia {extra}'.strip(),
        ]
    return [f'site:{dominio} "{r}" Italia {extra}'.strip()]


def cerca_su_portale(portale: dict, ruolo: str, citta: str = "",
                     remoto: bool = False, max_r: int = 5) -> list[dict]:
    """Cerca offerte su un singolo portale usando DuckDuckGo site:."""
    if not DDGS_OK:
        return []
    queries   = _queries_portale(portale, ruolo, citta, remoto)
    risultati: list[dict] = []
    url_visti: set[str]   = set()
    try:
        with DDGS() as ddgs:
            for q in queries:
                for r in ddgs.text(q, max_results=max_r):
                    url = r.get("href", "")
                    if url and url not in url_visti:
                        r["_portale"] = portale["nome"]
                        risultati.append(r)
                        url_visti.add(url)
                if len(risultati) >= max_r:
                    break
    except Exception:
        pass
    return risultati[:max_r]


def _mostra_risultati_portale(portale: dict, risultati: list[dict]) -> None:
    """Stampa i risultati di un singolo portale con semaforo whitelist/blacklist."""
    tipo   = portale["tipo"]
    colore = {"portale": "cyan", "agenzia": "magenta", "azienda": "blue", "regionale": "yellow"}.get(tipo, "white")
    icona  = {"portale": "🌐", "agenzia": "🤝", "azienda": "🏢", "regionale": "📍"}.get(tipo, "•")

    if not risultati:
        console.print(f"  {icona} [dim]{portale['nome']}[/] — [yellow]nessun risultato[/]")
        return

    console.print(f"\n  {icona} [{colore}][bold]{portale['nome']}[/][/] — {len(risultati)} offerte trovate:\n")
    for i, r in enumerate(risultati, 1):
        titolo = r.get("title", "Senza titolo")
        url    = r.get("href", "")
        corpo  = r.get("body", "")[:180]

        esito = valuta_offerta(f"{titolo} {corpo} {url}")
        sem   = {"rosso": "[bold red]🔴[/]", "verde": "[bold green]🟢[/]"}.get(esito["semaforo"], "[yellow]🟡[/]")

        console.print(f"    {sem} [bold]{i}. {titolo}[/]")
        if url:
            console.print(f"       [blue underline]{url}[/]")
        if corpo:
            console.print(f"       [dim]{corpo}[/]")
        if esito["black"]:
            motivi = ", ".join(b["valore"] for b in esito["black"])
            console.print(f"       [red]⚠  Blacklist: {motivi}[/]")
        elif esito["white"]:
            note = ", ".join(b["valore"] for b in esito["white"])
            console.print(f"       [green]✓  Whitelist: {note}[/]")


def menu_ricerca_siti() -> None:
    """Menu per cercare su portali e aziende specifiche con site:."""
    while True:
        os.system("cls" if sys.platform == "win32" else "clear")

        tab = Table(title="[bold]Portali e Aziende — Ricerca Mirata[/]",
                    border_style="cyan", show_lines=False, expand=False)
        tab.add_column("ID",   width=4,  style="dim")
        tab.add_column("Sito", width=26)
        tab.add_column("Tipo", width=10, style="dim")

        for p in PORTALI_LAVORO:
            tipo   = p["tipo"]
            colore = {"portale": "cyan", "agenzia": "magenta", "azienda": "blue", "regionale": "yellow"}.get(tipo, "white")
            icona  = {"portale": "🌐", "agenzia": "🤝", "azienda": "🏢", "regionale": "📍"}.get(tipo, "•")
            tab.add_row(
                str(p["id"]),
                f"{icona} [{colore}]{p['nome']}[/]",
                tipo,
            )
        console.print(tab)

        console.print(
            "\n  [dim]Digita:[/]\n"
            "    [bold]A[/]   = tutti i siti\n"
            "    [bold]P[/]   = 🌐 portali + 🤝 agenzie (1–16)\n"
            "    [bold]S[/]   = 📍 Sicilia/Catania (17–19)\n"
            "    [bold]Z[/]   = 🏢 aziende specifiche (20–26)\n"
            "    [bold]1,8,17[/]  = siti scelti (numeri separati da virgola)\n"
            "    [bold]0[/]   = indietro\n"
        )

        try:
            scelta = input("  Selezione: ").strip().upper()
        except Ridimensiona:
            continue

        if scelta == "0":
            break

        if scelta == "A":
            portali_sel = PORTALI_LAVORO
        elif scelta == "P":
            portali_sel = [p for p in PORTALI_LAVORO if p["tipo"] in ("portale", "agenzia")]
        elif scelta == "S":
            portali_sel = [p for p in PORTALI_LAVORO if p["tipo"] == "regionale"]
        elif scelta == "Z":
            portali_sel = [p for p in PORTALI_LAVORO if p["tipo"] == "azienda"]
        else:
            ids = [x.strip() for x in scelta.split(",") if x.strip()]
            portali_sel = [_PORTALE_MAP[i] for i in ids if i in _PORTALE_MAP]
            if not portali_sel:
                console.print("  [yellow]Selezione non valida.[/]")
                input("  [INVIO per continuare] ")
                continue

        # Chiede ruolo e città
        console.print()
        ruolo  = _chiedi_ruolo("Ruolo cercato (invio=qualsiasi)")
        remoto = input("  Lavoro remoto? [s/n, invio=n]: ").strip().lower() == "s"
        citta  = "remoto" if remoto else (input("  Città (invio=qualsiasi): ").strip() or "")

        n = len(portali_sel)
        console.print(f"\n  [dim]📜 Consultando gli archivi divini... ({n} sito/i)[/]\n")

        totale = 0
        for idx, portale in enumerate(portali_sel, 1):
            console.print(f"  [dim][{idx}/{n}] {portale['nome']}...[/]")
            ris = cerca_su_portale(portale, ruolo, citta, remoto)
            _mostra_risultati_portale(portale, ris)
            totale += len(ris)
            if idx < n:
                time.sleep(0.8)   # pausa cortesia tra richieste

        console.print(f"\n  [bold green]✨ Verità rivelata. Bevi la conoscenza. — {totale} risultati[/]")

        if totale > 0:
            _chiedi_e_aggiungi_azienda()

        input("\n  [INVIO per continuare] ")


# ── Esecuzione query su DuckDuckGo ────────────────────────────────────────────

def _esegui_queries(queries: list[str], max_per_query: int = 4) -> list[dict]:
    """Lancia una lista di query e restituisce i risultati senza duplicati."""
    risultati: list[dict] = []
    url_visti: set[str] = set()
    try:
        with DDGS() as ddgs:
            for query in queries:
                for r in ddgs.text(query, max_results=max_per_query):
                    url = r.get("href", "")
                    if url and url not in url_visti:
                        risultati.append(r)
                        url_visti.add(url)
                if len(risultati) >= 10:
                    break
    except Exception as e:
        console.print(f"[red]Errore ricerca web: {e}[/]")
    return risultati[:10]


# ── Ricerca offerte ───────────────────────────────────────────────────────────

def cerca_offerte(ruolo: str, citta: str, remoto: bool = False) -> list[dict]:
    if not DDGS_OK:
        console.print("[red]Installa ddgs: pip install ddgs[/]")
        return []

    dove = "remoto" if remoto else citta
    console.print(f"\n  [dim]📜 Consultando gli archivi divini... ({ruolo} — {dove})[/]\n")

    # ── 1. Query in italiano ──────────────────────────────────────────────────
    if remoto:
        queries_it = [
            f'offerta lavoro "{ruolo}" remoto Italia',
            f'"{ruolo}" lavoro smart working Italia',
        ]
    else:
        sicilia = citta.lower() in {
            "catania", "palermo", "messina", "siracusa", "ragusa",
            "trapani", "agrigento", "caltanissetta", "enna"
        }
        queries_it = [
            f'offerta lavoro "{ruolo}" "{citta}"',
            f'"{ruolo}" lavoro "{citta}" candidatura',
            f'lavoro "{ruolo}" Sicilia "{citta}"' if sicilia
            else f'"{ruolo}" assunzione "{citta}"',
        ]

    risultati = _esegui_queries(queries_it)

    # ── 2. Fallback automatico in inglese se zero risultati ───────────────────
    if not risultati:
        console.print(
            "  [yellow]Nessun risultato in italiano. "
            "Provo automaticamente con titoli inglesi equivalenti...[/]\n"
        )
        titoli_en = titoli_inglesi(ruolo)

        if remoto:
            queries_en = [
                f'job "{t}" remote Italy' for t in titoli_en
            ] + [
                f'"{t}" smart working Italia' for t in titoli_en
            ]
        else:
            queries_en = [
                f'job "{t}" "{citta}" Italy' for t in titoli_en
            ] + [
                f'"{t}" hiring "{citta}"' for t in titoli_en
            ] + [
                f'"{t}" lavoro "{citta}"' for t in titoli_en
            ]

        console.print(
            f"  [dim]Titoli cercati in inglese: "
            f"{', '.join(titoli_en[:3])}...[/]\n"
        )
        risultati = _esegui_queries(queries_en)

        # Marca i risultati come "trovati in inglese"
        for r in risultati:
            r["_lingua"] = "en"

    return risultati


def mostra_offerte(risultati: list[dict], ruolo: str, citta: str) -> None:
    if not risultati:
        console.print(Panel(
            "🌫️ La nebbia copre la verità. Riprova.\n\n"
            "Suggerimenti:\n"
            "  • Prova un termine ancora più generico (es. 'informatico', 'tecnico')\n"
            "  • Prova solo il settore (es. 'IT', 'software', 'reti')\n"
            "  • Controlla la connessione internet",
            border_style="yellow"
        ))
        return

    da_inglese = any(r.get("_lingua") == "en" for r in risultati)
    titolo_panel = (
        f"[bold green]Trovate {len(risultati)} offerte "
        f"{'(ricerca in inglese)' if da_inglese else ''} "
        f"per: {ruolo} — {citta}[/]"
    )
    if da_inglese:
        console.print(Panel(
            "[yellow]Risultati trovati usando titoli inglesi equivalenti.[/]\n"
            "Le offerte potrebbero essere in inglese o richiedere conoscenza della lingua.",
            border_style="yellow"
        ))

    console.print(Panel(
        titolo_panel,
        border_style="green"
    ))

    for i, r in enumerate(risultati, 1):
        titolo = r.get("title", "Senza titolo")
        url    = r.get("href", "")
        corpo  = r.get("body", "")[:220]

        # Controllo whitelist/blacklist sull'offerta
        testo_offerta = f"{titolo} {corpo} {url}"
        esito = valuta_offerta(testo_offerta)
        if esito["semaforo"] == "rosso":
            icona_sem = "[bold red]🔴[/]"
        elif esito["semaforo"] == "verde":
            icona_sem = "[bold green]🟢[/]"
        else:
            icona_sem = "[yellow]🟡[/]"

        console.print(f"\n  {icona_sem} [bold green]{i}. {titolo}[/]")
        if url:
            console.print(f"     [blue underline]{url}[/]")
        if corpo:
            console.print(f"     [dim]{corpo}...[/]")

        # Dettaglio blacklist se trovato
        if esito["black"]:
            motivi = ", ".join(r["valore"] for r in esito["black"])
            console.print(f"     [bold red]⚠  Blacklist: {motivi}[/]")
        elif esito["white"]:
            note = ", ".join(r["valore"] for r in esito["white"])
            console.print(f"     [green]✓  Whitelist: {note}[/]")


# ── Lettera di presentazione ──────────────────────────────────────────────────

def scrivi_lettera(ruolo: str, azienda: str, nome: str, esperienza: str) -> None:
    console.print("\n  [dim]Scrivo la lettera...[/]\n")

    prompt = f"""Scrivi una lettera di presentazione professionale in italiano per:
- Candidato: {nome}
- Ruolo cercato: {ruolo}
- Azienda: {azienda}
- Esperienza: {esperienza}

Requisiti:
- Tono professionale ma caldo
- Massimo 3 paragrafi
- Paragrafo 1: chi sono e perché mi candido
- Paragrafo 2: cosa porto di valore
- Paragrafo 3: disponibilità a colloquio
- Niente frasi fatte o banali"""

    risposta = chiedi_ollama(prompt, "risposta_lunga")
    console.print(Panel(
        Markdown(risposta),
        title="[bold green]Lettera di Presentazione[/]",
        border_style="green"
    ))


# ── Preparazione colloquio ────────────────────────────────────────────────────

def prepara_colloquio(ruolo: str) -> None:
    console.print("\n  [dim]Preparo domande tipiche...[/]\n")

    prompt = f"""Prepara una guida al colloquio di lavoro per il ruolo: {ruolo}

Includi:
1. Le 6 domande più frequenti che farà il recruiter
2. Per ogni domanda: cosa rispondere (2-3 righe)
3. 3 domande intelligenti da fare TU all'azienda
4. Un consiglio finale sul comportamento

Scrivi in italiano, stile pratico e diretto."""

    risposta = chiedi_ollama(prompt, "risposta_lunga")
    console.print(Panel(
        Markdown(risposta),
        title=f"[bold green]Guida Colloquio — {ruolo}[/]",
        border_style="green"
    ))


# ── Ricerca info azienda online ───────────────────────────────────────────────

def cerca_info_azienda(nome_azienda: str) -> list[dict]:
    """Cerca notizie e recensioni sull'azienda online."""
    if not DDGS_OK:
        return []
    queries = [
        f'"{nome_azienda}" recensioni dipendenti opinioni lavoro',
        f'"{nome_azienda}" truffa scam frode OR "{nome_azienda}" azienda storia fondata',
        f'"{nome_azienda}" Glassdoor OR "{nome_azienda}" Indeed recensioni',
    ]
    return _esegui_queries(queries, max_per_query=3)


# ── Analisi offerta ───────────────────────────────────────────────────────────

def analizza_offerta(testo_offerta: str) -> None:
    console.print("\n  [dim]Analizzo l'offerta...[/]\n")

    # 1. Analisi testo offerta con Ollama
    prompt_offerta = f"""Analizza questa offerta di lavoro italiana.

OFFERTA:
{testo_offerta}

Rispondi con queste sezioni:

**STIPENDIO**
- Stipendio indicato nell'offerta (se presente)
- Stima RAL (Reddito Annuo Lordo) per questo ruolo in Italia: da €X a €Y
- Stima stipendio netto mensile: circa €X–€Y

**PUNTI POSITIVI**
(elenco breve)

**PUNTI DA CHIARIRE IN COLLOQUIO**
(domande da fare)

**SEGNALI DI ALLERTA**
Elenca eventuali segnali che questa potrebbe essere:
- un'offerta poco seria o truffa (richiesta di pagamento, vague promises, no sede, no P.IVA...)
- un contratto svantaggioso (stage non retribuito, partita IVA mascherata, ecc.)
Se non ci sono segnali, scrivi: Nessun segnale di allerta rilevato.

**VERDICT FINALE**
Una riga: vale la pena candidarsi? Sì / Forse / No — e perché in una frase."""

    # Analisi offerta + estrazione nome azienda in parallelo (indipendenti)
    prompt_azienda = f"""Da questo testo di offerta di lavoro, estrai SOLO il nome dell'azienda.
Rispondi con il solo nome, niente altro.
Se non è indicato, rispondi: sconosciuto

OFFERTA:
{testo_offerta[:500]}"""

    console.print("  [dim]Analizzo offerta ed estraggo azienda in parallelo...[/]")
    risposta, nome_az_raw = chiedi_parallelo([
        (prompt_offerta, "risposta_lunga"),   # analisi completa → italiano
        (prompt_azienda, "estrazione"),        # solo nome → no lingua forzata
    ])

    console.print(Panel(
        Markdown(risposta),
        title="[bold green]Analisi Offerta[/]",
        border_style="green"
    ))

    nome_az = nome_az_raw.strip().strip('"').strip("'")

    if nome_az and nome_az.lower() != "sconosciuto" and len(nome_az) < 80:
        console.print(f"\n  [dim]Cerco informazioni su: [bold]{nome_az}[/]...[/]\n")
        notizie = cerca_info_azienda(nome_az)

        if notizie:
            # Passa le notizie a Ollama per un giudizio finale
            testo_notizie = "\n".join(
                f"- {r.get('title','')}: {r.get('body','')[:200]}"
                for r in notizie[:6]
            )
            prompt_giudizio = f"""Basandoti su queste informazioni trovate online sull'azienda "{nome_az}":

{testo_notizie}

Fornisci:

**STORIA E PROFILO AZIENDA**
- Quando è stata fondata (se disponibile)
- Settore e dimensione stimata
- Presenza online / solidità

**GIUDIZIO AFFIDABILITÀ**
Scegli uno:
  🟢 AZIENDA CONSOLIDATA — segnali positivi
  🟡 AZIENDA POCO CONOSCIUTA — non ci sono dati sufficienti
  🔴 POSSIBILE RISCHIO — segnali negativi (recensioni molto negative, segnalazioni truffa, ecc.)

Spiega il giudizio in 2-3 righe.

**COSA DICONO I DIPENDENTI** (se disponibile)
Breve sintesi delle recensioni trovate."""

            giudizio = chiedi_ollama(prompt_giudizio, "risposta")
            console.print(Panel(
                Markdown(giudizio),
                title=f"[bold cyan]Ricerca Azienda: {nome_az}[/]",
                border_style="cyan"
            ))
        else:
            console.print(Panel(
                f"[yellow]Nessuna informazione trovata online per: {nome_az}[/]\n"
                "Suggerimento: cerca tu stesso su Google, LinkedIn e Glassdoor.",
                border_style="yellow"
            ))


# ── Menu ──────────────────────────────────────────────────────────────────────

def _chiedi_ruolo(messaggio: str = "Ruolo cercato") -> str:
    """
    Chiede il ruolo all'utente.
    Se c'è un CV nella cartella cv/, prima propone i ruoli estratti dall'AI.
    """
    if trova_cv():
        ruolo_cv = scegli_ruolo_da_cv()
        if ruolo_cv:
            return ruolo_cv
        # L'utente ha scelto di inserire manualmente
        console.print()

    ruolo = input(f"  {messaggio} (es. sviluppatore Python): ").strip()
    return ruolo or "sviluppatore"


def menu_cerca_lavoro() -> None:
    avvia_warmup()   # precarica modello Ollama in background subito
    # Avviso CV all'apertura del menu
    cv_presente = trova_cv() is not None
    cv_nota = (
        "\n  [dim]📄 CV trovato — i ruoli verranno suggeriti automaticamente[/]"
        if cv_presente else
        "\n  [dim]💡 Metti il tuo CV in strumento_pratico/cv/ per suggerimenti automatici[/]"
    )

    while True:
        os.system("cls" if sys.platform == "win32" else "clear")
        from rich.console import Console as _Con
        stampa_breadcrumb(_Con(), "Prismalux › 🛠 Pratico › Cerca Lavoro")
        console.print(Panel(
            "[bold]Agente Cerca Lavoro[/]\n\n"
            "  1. Cerca offerte di lavoro\n"
            "  2. Scrivi lettera di presentazione\n"
            "  3. Preparati per il colloquio\n"
            "  4. Analizza un'offerta (incolla il testo)\n"
            "  5. Gestione Whitelist / Blacklist\n"
            "  [cyan]6. Cerca su portali specifici[/]\n"
            "     [dim](LinkedIn, Indeed, Wellfound, AlmaLaurea, IKEA, STM...)[/]\n\n"
            + cv_nota + "\n\n"
            "  [dim]1–6 apri   ·   0 torna   ·   Q esci[/]\n"
            "  ❯ ",
            title="[bold green]Cerca Lavoro[/]",
            border_style="green",
            padding=(1, 2, 0, 2),
        ))

        try:
            scelta = input_nel_box(2)
        except Ridimensiona:
            continue

        if scelta == "0":
            break

        elif scelta == "1":
            console.print("\n  [bold]Cerca offerte di lavoro[/]")
            ruolo  = _chiedi_ruolo("Ruolo cercato")
            remoto = input("  Lavoro remoto? [s/n, invio=n]: ").strip().lower() == "s"
            citta  = "remoto" if remoto else (input("  Città: ").strip() or "Catania")
            offerte = cerca_offerte(ruolo, citta, remoto)
            mostra_offerte(offerte, ruolo, citta)

            # Offerta rapida di aggiunta alle liste dopo la ricerca
            if offerte:
                _chiedi_e_aggiungi_azienda()

            input("\n  [INVIO per continuare] ")

        elif scelta == "2":
            console.print("\n  [bold]Lettera di presentazione[/]")
            nome       = input("  Il tuo nome: ").strip() or "Mario Rossi"
            ruolo      = _chiedi_ruolo("Ruolo per cui ti candidi")
            azienda    = input("  Nome azienda: ").strip() or "l'azienda"
            esperienza = input("  Descrivi brevemente la tua esperienza: ").strip() or "esperienza nel settore"
            scrivi_lettera(ruolo, azienda, nome, esperienza)
            input("\n  [INVIO per continuare] ")

        elif scelta == "3":
            ruolo = _chiedi_ruolo("Per quale ruolo vuoi prepararti")
            prepara_colloquio(ruolo)
            input("\n  [INVIO per continuare] ")

        elif scelta == "4":
            console.print("\n  Incolla il testo dell'offerta (digita FINE su una riga vuota per terminare):")
            righe = []
            while True:
                riga = input()
                if riga.strip().upper() == "FINE":
                    break
                righe.append(riga)
            testo = "\n".join(righe)
            if testo.strip():
                # Controlla prima le liste
                esito = valuta_offerta(testo)
                mostra_semaforo(esito, "Offerta inserita")
                analizza_offerta(testo)
            input("\n  [INVIO per continuare] ")

        elif scelta == "5":
            from db_lavoro import menu_gestione_liste
            menu_gestione_liste()

        elif scelta == "6":
            menu_ricerca_siti()


if __name__ == "__main__":
    try:
        menu_cerca_lavoro()
    except KeyboardInterrupt:
        console.print("\n\n  🍺 Alla prossima libagione di sapere.\n")

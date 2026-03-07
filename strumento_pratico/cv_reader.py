"""
CV Reader — Lettore e Analizzatore di Curriculum
==================================================
Legge il CV dalla cartella cv/, lo analizza con Ollama
e suggerisce i ruoli più adatti per la ricerca di lavoro.

Formati supportati:
  .txt  → sempre disponibile
  .pdf  → richiede: pip install pypdf
"""

import os
import sys
import json
from rich.console import Console
from rich.panel import Panel
from rich.prompt import Prompt

console = Console()

CARTELLA_CV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cv")

# ── Client Ollama ottimizzato ─────────────────────────────────────────────────
_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)
from ollama_utils import chiedi_ollama


# ── Lettura file CV ───────────────────────────────────────────────────────────

def trova_cv() -> str | None:
    """Trova il CV più recente nella cartella cv/. Ritorna il percorso o None."""
    if not os.path.exists(CARTELLA_CV):
        return None

    candidati = [
        os.path.join(CARTELLA_CV, f)
        for f in os.listdir(CARTELLA_CV)
        if f.lower().endswith((".txt", ".pdf"))
        and not f.startswith("ISTRUZIONI")
    ]
    if not candidati:
        return None

    # Usa il file modificato più di recente
    return max(candidati, key=os.path.getmtime)


def leggi_cv(percorso: str) -> str:
    """Legge il contenuto del CV da file .txt o .pdf."""
    ext = os.path.splitext(percorso)[1].lower()

    if ext == ".txt":
        with open(percorso, encoding="utf-8", errors="ignore") as f:
            return f.read()

    elif ext == ".pdf":
        try:
            import pypdf
            reader = pypdf.PdfReader(percorso)
            testo = "\n".join(
                pagina.extract_text() or ""
                for pagina in reader.pages
            )
            if not testo.strip():
                return ""
            return testo
        except ImportError:
            console.print(Panel(
                "[yellow]Per leggere i PDF installa pypdf:[/]\n"
                "  [bold cyan]pip install pypdf[/]\n\n"
                "In alternativa salva il CV come file .txt",
                border_style="yellow"
            ))
            return ""
        except Exception as e:
            console.print(f"[red]Errore lettura PDF: {e}[/]")
            return ""

    return ""


# ── Analisi CV con Ollama ─────────────────────────────────────────────────────

def analizza_cv(testo_cv: str) -> dict | None:
    """
    Invia il CV a Ollama e riceve un JSON con:
      ruolo_principale, ruoli_alternativi, skills, livello
    Ritorna None se Ollama non è disponibile.
    """
    # Tronca il CV a 3000 caratteri per non sovraccaricare il modello
    testo_troncato = testo_cv[:3000]

    prompt = f"""Analizza questo Curriculum Vitae e rispondi SOLO con un JSON valido, senza nessun testo prima o dopo.

CV:
{testo_troncato}

JSON da restituire (compila tutti i campi):
{{
  "ruolo_principale": "il ruolo/professione più evidente dal CV (in italiano)",
  "ruoli_alternativi": ["ruolo2", "ruolo3", "ruolo4"],
  "skills_chiave": ["skill1", "skill2", "skill3", "skill4", "skill5"],
  "livello": "junior oppure mid oppure senior",
  "anni_esperienza": numero intero stimato (0 se non chiaro),
  "riassunto": "una frase sola che descrive il profilo professionale"
}}

Rispondi SOLO con il JSON. Niente testo prima o dopo. Niente ```json```."""

    try:
        risposta = chiedi_ollama(prompt, "estrazione").strip()
        if risposta.startswith("❌"):
            console.print(f"[red]{risposta}[/]")
            return None

        # Estrae il JSON dalla risposta (a volte il modello aggiunge testo)
        inizio = risposta.find("{")
        fine   = risposta.rfind("}") + 1
        if inizio == -1 or fine == 0:
            return None

        return json.loads(risposta[inizio:fine])

    except (json.JSONDecodeError, Exception):
        return None


# ── Interfaccia utente ────────────────────────────────────────────────────────

def scegli_ruolo_da_cv() -> str | None:
    """
    Flusso completo:
    1. Trova il CV
    2. Lo legge
    3. Lo analizza con Ollama
    4. Mostra i ruoli suggeriti e chiede quale usare
    5. Ritorna il ruolo scelto (stringa) o None se l'utente vuole inserirlo manualmente
    """
    percorso = trova_cv()
    if not percorso:
        return None  # nessun CV trovato, l'utente inserirà manualmente

    nome_file = os.path.basename(percorso)
    console.print(f"\n  [dim]CV trovato: [bold]{nome_file}[/] — analizzo con AI...[/]\n")

    testo = leggi_cv(percorso)
    if not testo.strip():
        console.print("[yellow]  Il CV è vuoto o illeggibile.[/]")
        return None

    dati = analizza_cv(testo)
    if not dati:
        console.print("[yellow]  Analisi AI non riuscita. Inserisci il ruolo manualmente.[/]")
        return None

    # Mostra il riassunto del profilo
    ruolo_principale  = dati.get("ruolo_principale", "")
    ruoli_alternativi = dati.get("ruoli_alternativi", [])
    skills            = dati.get("skills_chiave", [])
    livello           = dati.get("livello", "")
    anni              = dati.get("anni_esperienza", 0)
    riassunto         = dati.get("riassunto", "")

    skills_str = ", ".join(skills[:5]) if skills else "—"

    console.print(Panel(
        f"[bold]Profilo rilevato dal CV[/]\n\n"
        f"  [cyan]Profilo:[/]  {riassunto}\n"
        f"  [cyan]Livello:[/]  {livello}  |  Esperienza stimata: {anni} anni\n"
        f"  [cyan]Skills:[/]   {skills_str}",
        title=f"[bold green]📄 {nome_file}[/]",
        border_style="green"
    ))

    # Costruisce la lista di scelta
    tutti_ruoli = []
    if ruolo_principale:
        tutti_ruoli.append(ruolo_principale)
    for r in ruoli_alternativi:
        if r and r not in tutti_ruoli:
            tutti_ruoli.append(r)

    if not tutti_ruoli:
        return None

    console.print("\n  [bold]Vuoi cercare per uno di questi ruoli?[/]\n")
    for i, ruolo in enumerate(tutti_ruoli, 1):
        marker = " [dim](principale)[/]" if i == 1 else ""
        console.print(f"    [green]{i}.[/] {ruolo}{marker}")
    console.print(f"    [dim]0. Inserisci manualmente[/]")

    scelta = input("\n  Scelta [0–{}]: ".format(len(tutti_ruoli))).strip()

    try:
        idx = int(scelta)
        if idx == 0:
            return None       # inserimento manuale
        if 1 <= idx <= len(tutti_ruoli):
            return tutti_ruoli[idx - 1]
    except ValueError:
        pass

    return None   # fallback: inserimento manuale

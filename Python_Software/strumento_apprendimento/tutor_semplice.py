"""
Tutor Ultra-Semplice
=====================
Spiegazioni brevi, chiare, con esempi pratici.
Pensato per chi impara meglio con testo corto e analogie concrete.

Dipendenze:
  pip install rich requests
  Richiede Ollama attivo su http://localhost:11434
"""

import os
import re
import sys
import json
import math
import signal
import datetime as _dt
import subprocess
from rich.console import Console
try:
    from check_deps import Ridimensiona, stampa_breadcrumb
except ImportError:
    class Ridimensiona(BaseException): pass
    def stampa_breadcrumb(con, percorso): print(f"  {percorso}")
from rich.panel import Panel
from rich.markdown import Markdown
from rich.table import Table

# ── Importa client Ollama ottimizzato ─────────────────────────────────────────
_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if os.path.join(_ROOT, "core") not in sys.path:
    sys.path.insert(0, os.path.join(_ROOT, "core"))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)
from ollama_utils import (chiedi_ollama, chiedi_ollama_think,
                          chiedi_ollama_stream, chiedi_ollama_stream_think,
                          chiedi_ollama_chat_stream, chiedi_ollama_chat_stream_think,
                          avvia_warmup, scegli_modello_interattivo,
                          get_last_token_stats, get_context_window)

console = Console()

# ── Preferenze sessione (chieste una sola volta per avvio) ────────────────────
_sessione_inizializzata: bool = False
_sessione_modello:       str  = ""    # "" = usa default di ollama_utils
_sessione_ragionamento:  str  = "durante"  # "no" | "durante" | "fine"

# ── Cronologia conversazione (domanda libera) ─────────────────────────────────
_storia_conv: list[dict] = []
_MAX_STORIA = 6   # max messaggi conservati (3 scambi utente↔assistente)

_SISTEMA_CHAT = (
    "Sei un tutor italiano di Python, matematica, fisica, chimica e sicurezza informatica. "
    "Rispondi SEMPRE in italiano, frasi brevi (max 4). "
    "Usa analogie concrete (cucina, sport, vita quotidiana). "
    "Se usi codice: massimo 5 righe commentate. "
    "Tieni conto della conversazione precedente per rispondere in modo coerente."
)


def _reset_storia_conv() -> None:
    """Azzera la cronologia (chiamare quando si cambia argomento)."""
    global _storia_conv
    _storia_conv = []


def _build_messages_chat(domanda: str) -> list[dict]:
    msgs = [{"role": "system", "content": _SISTEMA_CHAT}]
    msgs += _storia_conv[-_MAX_STORIA:]
    msgs.append({"role": "user", "content": domanda})
    return msgs


def _aggiorna_storia(domanda: str, risposta: str) -> None:
    global _storia_conv
    _storia_conv.append({"role": "user",      "content": domanda})
    _storia_conv.append({"role": "assistant", "content": risposta})
    if len(_storia_conv) > _MAX_STORIA:
        _storia_conv = _storia_conv[-_MAX_STORIA:]


# ── Progressi per spaced repetition ──────────────────────────────────────────
_FILE_PROGRESSI = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "esportazioni", "progressi_studio.json"
)
os.makedirs(os.path.dirname(_FILE_PROGRESSI), exist_ok=True)


def _carica_progressi() -> dict:
    try:
        with open(_FILE_PROGRESSI, encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def _salva_progressi(p: dict) -> None:
    try:
        with open(_FILE_PROGRESSI, "w", encoding="utf-8") as f:
            json.dump(p, f, ensure_ascii=False, indent=2)
    except Exception:
        pass


def _aggiorna_progresso(nome: str, punteggio: int) -> None:
    """punteggio: 1=difficile, 2=capito, 3=padroneggiato"""
    p = _carica_progressi()
    oggi = _dt.date.today().isoformat()
    entry = p.get(nome, {"viste": 0, "ultima": "", "punteggio": 0})
    entry["viste"] = entry.get("viste", 0) + 1
    entry["ultima"] = oggi
    old = entry.get("punteggio", 0)
    entry["punteggio"] = round((old + punteggio * 2) / 3, 1) if old else float(punteggio)
    p[nome] = entry
    _salva_progressi(p)


# ── Argomenti disponibili — dal più basico al più tecnico ────────────────────
# Struttura: lista di (titolo_sezione, [(nome, descrizione), ...])


# ── Dati argomenti (in tutor_dati.py per tenere questo file leggibile) ──────────
from tutor_dati import (
    SEZIONI_ARGOMENTI, ARGOMENTI,
    SEZIONI_PYTHON, SEZIONI_MATEMATICA, SEZIONI_SICUREZZA,
    SEZIONI_FISICA, SEZIONI_CHIMICA,
)

# ── Wrapper locale: sopprime SIGWINCH durante la chiamata ─────────────────────

def _chiedi(prompt: str, tipo: str = "risposta") -> str:
    """chiedi_ollama con SIGWINCH disabilitato (evita Ridimensiona durante requests)."""
    _old = None
    if hasattr(signal, "SIGWINCH"):
        _old = signal.getsignal(signal.SIGWINCH)
        signal.signal(signal.SIGWINCH, signal.SIG_IGN)
    try:
        return chiedi_ollama(prompt, tipo)
    finally:
        if _old is not None:
            signal.signal(signal.SIGWINCH, _old)


def _chiedi_preferenze_sessione(titolo_tutor: str) -> None:
    """
    Chiede una volta sola per sessione:
      1. Quale modello Ollama usare (lista locale)
      2. Come gestire il ragionamento del modello
    """
    global _sessione_inizializzata, _sessione_modello, _sessione_ragionamento
    if _sessione_inizializzata:
        return
    _sessione_inizializzata = True

    os.system("cls" if sys.platform == "win32" else "clear")

    # ── Selezione modello ─────────────────────────────────────────────────
    _sessione_modello = scegli_modello_interattivo(titolo_tutor)

    # ── Preferenza ragionamento ───────────────────────────────────────────
    console.print("\n  Come vuoi vedere il ragionamento del modello?\n")
    console.print("  [dim]1.[/]  Solo risposta finale  [dim](più veloce)[/]")
    console.print("  [dim]2.[/]  Ragionamento durante la generazione  [dim](default)[/]")
    console.print("  [dim]3.[/]  Risposta prima, poi chiedi se vuoi il ragionamento")
    try:
        scelta = input("\n  ❯ ").strip()
        if scelta == "1":
            _sessione_ragionamento = "no"
        elif scelta == "3":
            _sessione_ragionamento = "fine"
        else:
            _sessione_ragionamento = "durante"
    except Exception:
        _sessione_ragionamento = "durante"

    mod_str = _sessione_modello or "(auto)"
    rag_str = {"no": "solo risposta", "durante": "ragionamento live",
               "fine": "ragionamento su richiesta"}.get(_sessione_ragionamento, "")
    console.print(f"\n  ✅  Modello: [bold cyan]{mod_str}[/]  |  "
                  f"Ragionamento: [bold yellow]{rag_str}[/]\n")
    input("  INVIO per iniziare… ")


def _chiedi_stream(prompt: str, tipo: str = "spiegazione"):
    """
    Generatore: yield token con SIGWINCH disabilitato per tutta la durata dello stream.
    Usato con Rich Live per mostrare la risposta man mano che il modello genera.
    """
    _old = None
    if hasattr(signal, "SIGWINCH"):
        _old = signal.getsignal(signal.SIGWINCH)
        signal.signal(signal.SIGWINCH, signal.SIG_IGN)
    try:
        yield from chiedi_ollama_stream(prompt, tipo)
    finally:
        if _old is not None:
            signal.signal(signal.SIGWINCH, _old)


# ── Helper statistiche token ───────────────────────────────────────────────────

_ctx_window_cache: dict[str, int] = {}


def _mostra_stats_token(_f) -> None:
    """Stampa una riga dim con il conteggio token dell'ultima risposta Ollama."""
    stats = get_last_token_stats()
    if not stats:
        return
    prompt_tok = stats.get("prompt_eval_count", 0)
    gen_tok    = stats.get("eval_count", 0)
    tot        = prompt_tok + gen_tok
    modello    = _sessione_modello or ""
    if modello not in _ctx_window_cache:
        _ctx_window_cache[modello] = get_context_window(modello or None)
    ctx = _ctx_window_cache[modello]
    if ctx:
        perc = int(tot / ctx * 100) if ctx else 0
        riga = (f"  \033[2m📊 Token: {prompt_tok} prompt + {gen_tok} generati"
                f" = {tot} / {ctx} max ({perc}%)\033[0m\n")
    else:
        riga = f"  \033[2m📊 Token: {prompt_tok} prompt + {gen_tok} generati = {tot}\033[0m\n"
    _f.write(riga)
    _f.flush()


# ── Helper streaming a stdout (senza Rich.Live per evitare conflitti) ─────────

def _stampa_chat_stream(messages: list, tipo: str = "spiegazione",
                        modello: str | None = None) -> str:
    """Come _stampa_stream ma usa /api/chat con cronologia conversazione."""
    _old = None
    if hasattr(signal, "SIGWINCH"):
        _old = signal.getsignal(signal.SIGWINCH)
        signal.signal(signal.SIGWINCH, signal.SIG_IGN)
    testo = ""
    _f = console.file
    try:
        _f.write("\n")
        _f.flush()
        for token in chiedi_ollama_chat_stream(messages, tipo, modello=modello):
            _f.write(token)
            _f.flush()
            testo += token
    finally:
        _f.write("\n\n")
        _f.flush()
        _mostra_stats_token(_f)
        if _old is not None:
            signal.signal(signal.SIGWINCH, _old)
    return testo


def _stampa_chat_stream_think(messages: list, tipo: str = "spiegazione",
                               modello: str | None = None) -> str:
    """Come _stampa_stream_think ma usa /api/chat con cronologia conversazione."""
    _old = None
    if hasattr(signal, "SIGWINCH"):
        _old = signal.getsignal(signal.SIGWINCH)
        signal.signal(signal.SIGWINCH, signal.SIG_IGN)
    testo_resp = ""
    _f = console.file
    fase_corrente = None
    try:
        for fase, token in chiedi_ollama_chat_stream_think(messages, tipo, modello=modello):
            if fase == "T":
                if fase_corrente is None:
                    _f.write("\n  \033[2msto pensando…\033[0m\n\n  \033[2m")
                    _f.flush()
                    fase_corrente = "T"
                _f.write(token)
                _f.flush()
            elif fase == "R":
                if fase_corrente == "T":
                    _f.write("\033[0m\n\n")
                    _f.flush()
                    fase_corrente = "R"
                elif fase_corrente is None:
                    _f.write("\n")
                    _f.flush()
                    fase_corrente = "R"
                _f.write(token)
                _f.flush()
                testo_resp += token
    finally:
        _f.write("\n\n")
        _f.flush()
        _mostra_stats_token(_f)
        if _old is not None:
            signal.signal(signal.SIGWINCH, _old)
    return testo_resp


def _stampa_stream(prompt: str, tipo: str = "spiegazione",
                   modello: str | None = None) -> str:
    """
    Stampa i token direttamente su stdout man mano che arrivano (nessun thinking).
    Restituisce il testo completo.
    """
    _old = None
    if hasattr(signal, "SIGWINCH"):
        _old = signal.getsignal(signal.SIGWINCH)
        signal.signal(signal.SIGWINCH, signal.SIG_IGN)

    testo = ""
    _f = console.file
    try:
        _f.write("\n")
        _f.flush()
        for token in chiedi_ollama_stream(prompt, tipo, modello=modello):
            _f.write(token)
            _f.flush()
            testo += token
    finally:
        _f.write("\n\n")
        _f.flush()
        _mostra_stats_token(_f)
        if _old is not None:
            signal.signal(signal.SIGWINCH, _old)

    return testo


def _stampa_stream_think(prompt: str, tipo: str = "spiegazione",
                         modello: str | None = None) -> str:
    """
    Come _stampa_stream ma mostra il thinking del modello in tempo reale.
    Ordine: 'sto pensando…' → testo thinking (dim) → risposta reale.
    Restituisce solo il testo della risposta (senza il thinking).
    """
    _old = None
    if hasattr(signal, "SIGWINCH"):
        _old = signal.getsignal(signal.SIGWINCH)
        signal.signal(signal.SIGWINCH, signal.SIG_IGN)

    testo_resp = ""
    _f = console.file
    fase_corrente = None   # None | "T" | "R"

    try:
        for fase, token in chiedi_ollama_stream_think(prompt, tipo, modello=modello):
            if fase == "T":
                if fase_corrente is None:
                    _f.write("\n  \033[2msto pensando…\033[0m\n\n  \033[2m")
                    _f.flush()
                    fase_corrente = "T"
                _f.write(token)
                _f.flush()
            elif fase == "R":
                if fase_corrente == "T":
                    _f.write("\033[0m\n\n")
                    _f.flush()
                    fase_corrente = "R"
                elif fase_corrente is None:
                    _f.write("\n")
                    _f.flush()
                    fase_corrente = "R"
                _f.write(token)
                _f.flush()
                testo_resp += token
    finally:
        _f.write("\n\n")
        _f.flush()
        _mostra_stats_token(_f)
        if _old is not None:
            signal.signal(signal.SIGWINCH, _old)

    return testo_resp


# ── Spiegazione argomento ──────────────────────────────────────────────────────

def spiega_argomento(nome: str, descrizione: str, breadcrumb: str = "") -> None:
    console.print(f"\n  [bold cyan]▶ {nome}[/]  [dim]— in generazione…[/]\n",
                  highlight=False)

    prompt = f"""Spiega '{descrizione}' a un principiante Python con dislessia.

Formato OBBLIGATORIO (5 sezioni, niente testo extra):

**Cos'è in breve:**
(1 frase max 12 parole)

**Esempio dalla vita reale:**
(1 frase con analogia concreta: cucina, sport, supermercato…)

**Codice:**
```python
# commento breve
(codice max 6 righe)
```

**Prova tu:**
(1 compito semplice)

**🇮🇹 Traduzione italiana:**
(tutto il contenuto sopra riscritto in italiano semplice, 3-4 frasi, senza codice)"""

    mod = _sessione_modello or None

    if _sessione_ragionamento == "no":
        # Solo risposta, nessun thinking
        testo = _stampa_stream(prompt, "spiegazione", modello=mod)
        thinking = ""
    elif _sessione_ragionamento == "fine":
        # Non-streaming: aspetta tutto, poi mostra risposta, poi chiede thinking
        console.print("  [dim]sto pensando…[/]")
        thinking, testo = chiedi_ollama_think(prompt, "spiegazione", modello=mod)
    else:
        # "durante": mostra thinking in tempo reale (default)
        testo = _stampa_stream_think(prompt, "spiegazione", modello=mod)
        thinking = ""

    if testo.strip():
        console.print(Panel(
            Markdown(testo),
            title=f"[bold cyan]{nome}[/]",
            subtitle=f"[dim] {breadcrumb} [/]" if breadcrumb else None,
            border_style="cyan",
            padding=(1, 2),
        ))

    # Modalità "fine": chiede se vuole vedere il ragionamento
    if _sessione_ragionamento == "fine" and thinking.strip():
        while True:
            try:
                vuole = input("\n  Vuoi vedere il ragionamento? (S/N): ").strip().upper()
            except Exception:
                vuole = "N"; break
            if vuole in ("S", "N", ""):
                break
            console.print("  [dim]Digita S per sì oppure N per no.[/]")
        if vuole == "S":
            console.print(Panel(
                thinking.strip(),
                title="[dim]💭 Ragionamento[/]",
                border_style="dim",
                padding=(0, 2),
            ))

    # Loop di comprensione: "hai capito?" → rispiegazione → quiz
    _loop_comprensione(nome, descrizione, mod)


def _loop_comprensione(nome: str, descrizione: str, mod) -> None:
    """
    Dopo la spiegazione chiede "Hai capito?".
    Se No → rispiegazione personalizzata sul dubbio dell'utente, si ripete.
    Se Sì → 2 domande di verifica con risposta dell'utente e feedback Ollama.
    """
    iterazione = 0

    # ── Fase 1: loop "hai capito?" ────────────────────────────────────────────
    while True:
        while True:
            try:
                ok = input("\n  Hai capito? (S/N): ").strip().upper()
            except Exception:
                return
            if ok in ("S", "N", ""):
                break
            console.print("  [dim]Digita S per sì oppure N per no.[/]")

        if ok != "N":
            _aggiorna_progresso(nome, 2 if iterazione == 0 else 1)
            break   # capito → vai al quiz

        # Non ha capito: chiedi il dubbio specifico
        console.print("\n  [dim]Cosa non hai capito? Spiegamelo in parole tue:[/]")
        try:
            dubbio = input("  ❯ ").strip()
        except Exception:
            dubbio = ""

        iterazione += 1
        console.print(f"\n  [bold yellow]📖 Spiegazione alternativa #{iterazione}…[/]\n")

        prompt_rispiega = f"""Devi rispiegare '{nome}' a uno studente che non ha capito.

Il suo dubbio: "{dubbio or 'non specificato'}"

Regole OBBLIGATORIE:
- Usa un'analogia DIVERSA da cucina/sport/supermercato — scegli qualcosa di insolito
- Spiega passo per passo, numerando ogni passo (1. 2. 3.)
- Frasi CORTISSIME (max 8 parole per frase)
- Se usi parole tecniche, traducile subito in italiano semplice
- Se necessario: codice max 5 righe, ogni riga commentata
- Max 180 parole in totale
Rispondi in italiano."""

        if _sessione_ragionamento == "no":
            _stampa_stream(prompt_rispiega, "spiegazione", modello=mod)
        else:
            _stampa_stream_think(prompt_rispiega, "spiegazione", modello=mod)

    # ── Fase 2: quiz di verifica ──────────────────────────────────────────────
    console.print("\n  [bold green]✅ Ottimo! Ora ti faccio 2 domande veloci per fissare il concetto.[/]\n")

    prompt_quiz = f"""Crea 2 domande di verifica SEMPLICI su '{nome}' per un principiante.

Formato OBBLIGATORIO (solo questo testo, niente altro):
DOMANDA 1: [testo]
DOMANDA 2: [testo]

Requisiti:
- Risposta breve: 1 frase oppure 1-3 righe di codice
- Riguardano il concetto base: {descrizione}
- NON sono trabocchetti né domande trabocchetto"""

    domande_raw = chiedi_ollama(prompt_quiz, "estrazione", modello=mod)
    if not domande_raw or domande_raw.startswith("❌"):
        return

    # Estrai le domande dal testo
    domande = [m.group(1).strip()
               for m in re.finditer(r'DOMANDA\s*\d+:\s*(.+)', domande_raw, re.IGNORECASE)]
    if not domande:
        domande = [domande_raw.strip()]   # fallback: tutto come unica domanda

    # Raccogli risposte dell'utente una alla volta
    risposte_utente: list[str] = []
    for i, dom in enumerate(domande, 1):
        console.print(Panel(
            dom,
            title=f"[bold green]❓ Domanda {i} di {len(domande)}[/]",
            border_style="green",
            padding=(0, 2),
        ))
        try:
            risp = input("  La tua risposta: ").strip()
        except Exception:
            risp = "(nessuna risposta)"
        risposte_utente.append(risp)

    # Valuta le risposte con Ollama
    coppie = "\n".join(
        f"Domanda {i+1}: {d}\nRisposta studente: {r}"
        for i, (d, r) in enumerate(zip(domande, risposte_utente))
    )
    prompt_feedback = f"""Valuta le risposte di uno studente che sta imparando '{nome}'.

{coppie}

Per ogni risposta:
- Inizia con ✅ se corretta o ❌ se errata/incompleta
- Se errata: spiega la risposta giusta in max 2 frasi semplici
- Tono incoraggiante e positivo

Rispondi in italiano."""

    console.print("\n  [dim]Valuto le tue risposte…[/]\n")
    if _sessione_ragionamento == "no":
        testo_fb = _stampa_stream(prompt_feedback, "spiegazione", modello=mod)
    else:
        testo_fb = _stampa_stream_think(prompt_feedback, "spiegazione", modello=mod)

    if testo_fb.strip():
        console.print(Panel(
            Markdown(testo_fb),
            title="[bold green]📝 Feedback[/]",
            border_style="green",
            padding=(1, 2),
        ))


# ── Parole chiave che indicano un calcolo da eseguire ─────────────────────────
_KW_CALCOLO = [
    # Conteggio e iterazione
    "conta", "elenca", "mostra i numeri", "lista di numeri",
    "numeri da", "da 1 a", "da 2 a", "da 3 a", "da 4 a", "da 5 a", "da 0 a",
    "fino a", "per ogni",
    # Calcoli aritmetici
    "calcola", "somma", "fattoriale", "fibonacci", "sequenza",
    "genera", "multiplic", "divid", "potenz", "radice",
    "quanti", "quanto fa", "risultato di", "mcd", "mcm", "primo tra",
    # Bit e sistemi numerici
    "bit", "binario", "binari", "esadecimale", "hex", "ottale",
    "converti", "and bit", "or bit", "xor", "not bit", "shift",
    "maschera", "byte", "nibble", "bitwise", "complemento",
    "rappresenta in", "scrivi in base",
    # Fattorizzazione e numeri primi
    "scomponi", "fattorizza", "fattori", "decomposiz",
    "numeri primi di", "primo di", "divisori primi",
]

def _e_un_calcolo(domanda: str) -> bool:
    """True se la domanda sembra richiedere un calcolo da eseguire."""
    d = domanda.lower()
    return any(k in d for k in _KW_CALCOLO)


# ── Prefissi da rimuovere prima di eval ───────────────────────────────────────
_PREFISSI_CALCOLO = [
    "quanto fa", "quanto vale", "quanto è", "quanto e'",
    "qual è", "qual e'", "calcola", "risultato di",
    "dimmi", "fai", "fa",
]

def _fmt_num(x: float | int) -> str:
    """Formatta un numero: intero se esatto, altrimenti 10 cifre significative."""
    if isinstance(x, int):
        return str(x)
    if x == int(x) and abs(x) < 1e15:
        return str(int(x))
    return f"{x:.10g}"


def _calcola_diretto(domanda: str) -> str | None:
    """
    Calcola la domanda direttamente in Python senza Ollama.
    Gestisce:
      - Espressioni matematiche (eval sicuro):  5+5, 2^10, sqrt(16), sin(pi/2)
      - Fattoriale:                             fattoriale di 12
      - Conta da N a M:                         conta da 1 a 10
      - Fibonacci (N termini):                  fibonacci di 10
      - MCD / MCM:                              mcd di 12 e 8
      - Potenza (frase):                        potenza di 2 alla 10
      - Radice (frase):                         radice di 16
      - Logaritmo (frase):                      logaritmo di 100, log base 10 di 1000
    Ritorna la stringa del risultato, oppure None se non riconosce la domanda.
    """
    d = domanda.lower().strip()

    # ── 1. Fattoriale ────────────────────────────────────────────────────────
    m = re.search(r'fattoriale\s+(?:di\s+)?(\d+)', d)
    if m:
        n = int(m.group(1))
        if 0 <= n <= 20:
            return str(math.factorial(n))
        return f"❌ Troppo grande per visualizzarlo (n={n}, max 20)"

    # ── 2. Fibonacci (N termini) ─────────────────────────────────────────────
    m = re.search(r'fibonacci\s+(?:di\s+|a\s+|primi?\s+)?(\d+)', d)
    if not m:
        m = re.search(r'(?:sequenza|serie)\s+(?:di\s+)?fibonacci.*?(\d+)', d)
    if m:
        n = int(m.group(1))
        if 1 <= n <= 80:
            a, b, seq = 0, 1, []
            for _ in range(n):
                seq.append(b)
                a, b = b, a + b
            return ', '.join(map(str, seq))

    # ── 3. Conta / elenca da N a M ───────────────────────────────────────────
    m = re.search(r'(?:conta|elenca|numeri|lista)\s+da\s+(-?\d+)\s+a\s+(-?\d+)', d)
    if not m:
        m = re.search(r'da\s+(-?\d+)\s+a\s+(-?\d+)', d)
    if m:
        n, k = int(m.group(1)), int(m.group(2))
        if abs(k - n) <= 500:
            step = 1 if n <= k else -1
            nums = list(range(n, k + step, step))
            if len(nums) <= 50:
                return ', '.join(map(str, nums))
            # Lista lunga: mostra inizio … fine + conteggio
            return (f"{', '.join(map(str, nums[:5]))} … {', '.join(map(str, nums[-5:]))}"
                    f"  ({len(nums)} numeri)")

    # ── 4. MCD / MCM ─────────────────────────────────────────────────────────
    m = re.search(r'mcd\s+(?:di\s+|tra\s+)?(-?\d+)\s+(?:e\s+|,\s*)?(-?\d+)', d)
    if m:
        return str(math.gcd(int(m.group(1)), int(m.group(2))))
    m = re.search(r'mcm\s+(?:di\s+|tra\s+)?(-?\d+)\s+(?:e\s+|,\s*)?(-?\d+)', d)
    if m:
        a, b = int(m.group(1)), int(m.group(2))
        return str(abs(a * b) // math.gcd(a, b)) if a and b else "0"

    # ── 5. Potenza (frase) ────────────────────────────────────────────────────
    m = re.search(r'potenza\s+di\s+(-?[\d.]+)\s+(?:alla\s+|elevato\s+(?:alla\s+)?)?(-?[\d.]+)', d)
    if m:
        try:
            return _fmt_num(float(m.group(1)) ** float(m.group(2)))
        except Exception:
            pass

    # ── 6. Radice (frase) ─────────────────────────────────────────────────────
    m = re.search(r'radice\s+(?:quadrata\s+)?(?:di\s+)?([\d.]+)', d)
    if m:
        try:
            return _fmt_num(math.sqrt(float(m.group(1))))
        except Exception:
            pass

    # ── 7. Logaritmo (frase) ──────────────────────────────────────────────────
    m = re.search(r'log(?:aritmo)?\s+(?:naturale\s+)?(?:di\s+|base\s+e\s+di\s+)?([\d.]+)', d)
    if m and 'base 10' not in d and 'log10' not in d:
        try:
            return _fmt_num(math.log(float(m.group(1))))
        except Exception:
            pass
    m = re.search(r'(?:log(?:aritmo)?\s+(?:in\s+)?base\s+10|log10)\s+(?:di\s+)?([\d.]+)', d)
    if m:
        try:
            return _fmt_num(math.log10(float(m.group(1))))
        except Exception:
            pass

    # ── 8. eval() sicuro per espressioni matematiche dirette ─────────────────
    testo = d
    for pref in sorted(_PREFISSI_CALCOLO, key=len, reverse=True):
        if testo.startswith(pref):
            testo = testo[len(pref):].strip()
            break

    testo = (testo
             .replace("^", "**")
             .replace("ln(", "log(")
             .replace("÷", "/")
             .replace("×", "*")
             .replace(",", ".")
             )

    if not re.match(r'^[\d\s\+\-\*\/\%\(\)\.a-z_]+$', testo) or not testo.strip():
        return None

    ns = {k: getattr(math, k) for k in dir(math) if not k.startswith('_')}
    ns['__builtins__'] = {}

    try:
        ris = eval(testo, ns)  # noqa: S307
    except Exception:
        return None

    if isinstance(ris, float):
        if ris != ris or abs(ris) == float('inf'):
            return None
        return _fmt_num(ris)
    if isinstance(ris, int):
        return str(ris)
    return None


def _conferma_e_riformula(domanda: str, risultato: str) -> str | None:
    """
    Mostra il pannello del risultato e chiede conferma.
    Se l'utente vuole riformulare → chiede la nuova domanda e la ritorna.
    Se è soddisfatto (o INVIO/S) → ritorna None.
    """
    console.print(Panel(
        f"[bold green]{domanda.strip()}  =  {risultato}[/]",
        title="[bold green]🔢 Calcolo immediato[/]",
        border_style="green",
        padding=(1, 2),
    ))
    while True:
        try:
            ok = input("  Era quello che volevi? (S/N): ").strip().upper()
        except Exception:
            return None
        if ok in ("S", "N", ""):
            break
        console.print("  [dim]Digita S per sì oppure N per no.[/]")
    if ok != "N":
        return None
    console.print("  Riformula la domanda in modo più esplicito:")
    try:
        nuova = input("  ❯ ").strip()
        return nuova if nuova else None
    except Exception:
        return None


def chiedi_domanda_libera() -> None:
    """L'utente fa una domanda con parole sue."""
    console.print("\n  Scrivi la tua domanda (es: 'cosa fa range()?', '5+5', 'sqrt(16)'):")
    domanda = input("  ❯ ").strip()
    if not domanda:
        return

    while True:
        # 1) Prova eval() / pattern diretto (istantaneo, nessuna chiamata Ollama)
        ris_diretto = _calcola_diretto(domanda)
        if ris_diretto is not None:
            nuova = _conferma_e_riformula(domanda, ris_diretto)
            if nuova:
                domanda = nuova
                continue        # riformulato: riprova
            # Soddisfatto: chiedi se vuole altri calcoli
            while True:
                try:
                    ancora = input("\n  Vuoi chiedermi altri calcoli? (S/N): ").strip().upper()
                except Exception:
                    ancora = "N"
                    break
                if ancora in ("S", "N", ""):
                    break
                console.print("  [dim]Digita S per sì oppure N per no.[/]")
            if ancora != "S":
                return
            console.print("\n  Scrivi il prossimo calcolo:")
            try:
                domanda = input("  ❯ ").strip()
            except Exception:
                return
            if not domanda:
                return
            continue                 # loop con il nuovo calcolo

        # 2) Se sembra un calcolo complesso (fattoriale via codice, ecc.) → Ollama
        if _e_un_calcolo(domanda):
            console.print("  [dim]→ Calcolo complesso, genero il codice…[/]")
            chiedi_calcolo(domanda)
            return

        break   # risposta Ollama normale → esci dal loop

    mod = _sessione_modello or None
    messages = _build_messages_chat(domanda)

    if _sessione_ragionamento == "no":
        testo = _stampa_chat_stream(messages, "spiegazione", modello=mod)
        thinking = ""
    elif _sessione_ragionamento == "fine":
        console.print("\n  [dim]sto pensando…[/]")
        # fine mode: usa prompt con contesto testuale (chiedi_ollama_think non supporta messages)
        ctx = "".join(
            f"{'Tu' if m['role'] == 'user' else 'Tutor'}: {m['content']}\n"
            for m in _storia_conv[-4:]
        )
        prompt = (f"Contesto:\n{ctx}\n" if ctx else "") + f"Domanda: {domanda}\n\nRispondi in italiano, max 4 frasi."
        thinking, testo = chiedi_ollama_think(prompt, "spiegazione", modello=mod)
    else:
        # "durante": thinking in tempo reale
        thinking = ""
        testo = _stampa_chat_stream_think(messages, "spiegazione", modello=mod)

    # Aggiorna cronologia con lo scambio appena avvenuto
    if testo and not testo.startswith("❌"):
        _aggiorna_storia(domanda, testo)

    if testo.strip():
        console.print(Panel(
            Markdown(testo),
            title="[bold cyan]Risposta[/]",
            border_style="cyan",
            padding=(1, 2),
        ))

    # Modalità "fine": chiede se vuole vedere il ragionamento
    if _sessione_ragionamento == "fine" and thinking.strip():
        try:
            vuole = input("\n  Vuoi vedere il ragionamento? (S/N): ").strip().upper()
        except Exception:
            vuole = "N"
        if vuole == "S":
            console.print(Panel(
                thinking.strip(),
                title="[dim]💭 Ragionamento[/]",
                border_style="dim",
                padding=(0, 2),
            ))


def chiedi_calcolo(domanda_precompilata: str = "") -> None:
    """L'utente descrive un calcolo → eval() diretto oppure Ollama genera il codice."""
    if domanda_precompilata:
        domanda = domanda_precompilata
        console.print(f"\n  [dim]▶ Calcolo: {domanda}[/]")
    else:
        console.print("\n  Descrivi il calcolo (es: '5+5', 'sqrt(16)', 'fattoriale di 12'):")
        domanda = input("  ❯ ").strip()
    if not domanda:
        return

    # Prova eval() / pattern diretto prima di chiamare Ollama
    while True:
        ris_diretto = _calcola_diretto(domanda)
        if ris_diretto is not None:
            nuova = _conferma_e_riformula(domanda, ris_diretto)
            if nuova:
                domanda = nuova
                continue    # riformulato: riprova
            # Soddisfatto: chiedi se vuole altri calcoli
            while True:
                try:
                    ancora = input("\n  Vuoi chiedermi altri calcoli? (S/N): ").strip().upper()
                except Exception:
                    ancora = "N"
                    break
                if ancora in ("S", "N", ""):
                    break
                console.print("  [dim]Digita S per sì oppure N per no.[/]")
            if ancora != "S":
                return
            console.print("\n  Scrivi il prossimo calcolo:")
            try:
                domanda = input("  ❯ ").strip()
            except Exception:
                return
            if not domanda:
                return
            continue                # loop con il nuovo calcolo
        break                       # non è eval diretto: procedi con Ollama

    # Il prompt termina con il prefill "```python\n" per guidare il modello
    # a iniziare subito con il codice (riduce il reasoning e il rischio di risposta vuota)
    prompt = f"""Scrivi codice Python eseguibile per questo calcolo. Solo codice, niente testo.

CALCOLO: {domanda}

Regole:
- Solo codice Python puro, niente spiegazioni
- Usa print() per mostrare i risultati in italiano
- Max 15 righe
- Solo stdlib (math, itertools, fractions, ecc.)

```python
"""

    console.print(f"\n  [bold cyan]▶ Generazione strumento Python…[/]  [dim]sto pensando…[/]")

    # think: True separa il reasoning (ignorato) dal codice nel campo response
    mod = _sessione_modello or None
    _, codice_raw = chiedi_ollama_think(prompt, "codice", modello=mod)

    # Estrai il blocco codice dal markdown se presente (con o senza fence iniziale)
    # Il prefill nel prompt ha già aperto ```python, quindi response può iniziare
    # direttamente con il codice e chiudersi con ``` oppure no
    match = re.search(r"```(?:python)?\n(.*?)(?:```|$)", codice_raw, re.DOTALL)
    if match:
        codice = match.group(1).strip()
    else:
        # response è già il codice puro (con think:True il reasoning è separato)
        codice = codice_raw.strip()

    if not codice or codice.startswith("❌"):
        console.print(f"  [red]{codice_raw}[/]")
        return

    console.print(Panel(
        Markdown(f"```python\n{codice}\n```"),
        title="[bold cyan]🔧 Strumento Python generato[/]",
        border_style="cyan",
        padding=(1, 2),
    ))

    console.print("  [dim]⚙ Esecuzione…[/]\n")
    try:
        ris = subprocess.run(
            [sys.executable, "-c", codice],
            capture_output=True, text=True, timeout=10,
        )
        output = ris.stdout.strip() or ris.stderr.strip() or "(nessun output)"
        colore = "green" if ris.returncode == 0 else "red"
        titolo = "✅ Risultato" if ris.returncode == 0 else "❌ Errore di esecuzione"
    except subprocess.TimeoutExpired:
        output = "⏱ Timeout: il codice ha impiegato più di 10 secondi."
        colore = "red"
        titolo = "❌ Timeout"
    except Exception as e:
        output = f"Errore: {e}"
        colore = "red"
        titolo = "❌ Errore"

    console.print(Panel(
        output,
        title=f"[bold {colore}]{titolo}[/]",
        border_style=colore,
        padding=(1, 2),
    ))



# ── Ricerca full-text argomenti ───────────────────────────────────────────────

def _cerca_argomento(argomenti_loc: list, sezione_di: dict,
                     titolo: str, colore: str) -> None:
    """Ricerca full-text negli argomenti della sezione corrente."""
    console.print("\n  [bold]Cerca argomento[/] (parola chiave): ", end="")
    try:
        kw = input("").strip().lower()
    except Exception:
        return
    if not kw:
        return

    risultati = [
        (i, nome, desc, sezione_di.get(i, ""))
        for i, (nome, desc) in enumerate(argomenti_loc)
        if kw in nome.lower() or kw in desc.lower()
    ]

    if not risultati:
        console.print(f"\n  [dim]Nessun argomento trovato per «{kw}».[/]")
        input("  [INVIO] ")
        return

    console.print(f"\n  [bold]{len(risultati)} risultati per «{kw}»:[/]\n")
    for n, (_, nome, _, sez) in enumerate(risultati[:20], 1):
        console.print(f"  [dim]{n:2}.[/]  [bold]{nome}[/]  [dim]— {sez}[/]")

    try:
        scelta = input(f"\n  Numero (1–{min(20, len(risultati))}) o INVIO per annullare: ").strip()
        if not scelta:
            return
        idx = int(scelta) - 1
        if 0 <= idx < len(risultati):
            i_orig, nome, desc, sez_nome = risultati[idx]
            breadcrumb = f"{titolo}  ›  {sez_nome}"
            _reset_storia_conv()
            spiega_argomento(nome, desc, breadcrumb=breadcrumb)
            input("\n  [INVIO per tornare al menu] ")
    except (ValueError, IndexError, Exception):
        pass


# ── Ripasso argomenti deboli (spaced repetition) ──────────────────────────────

def _menu_ripasso(argomenti_loc: list, sezione_di: dict,
                  titolo: str, colore: str) -> None:
    """Mostra argomenti da ripassare ordinati per priorità."""
    p = _carica_progressi()
    oggi = _dt.date.today()
    da_ripassare = []

    for i, (nome, desc) in enumerate(argomenti_loc):
        entry = p.get(nome)
        if entry is None:
            da_ripassare.append((0.0, i, nome, desc, "mai studiato"))
        else:
            punteggio = entry.get("punteggio", 0)
            ultima_str = entry.get("ultima", "")
            try:
                giorni = (oggi - _dt.date.fromisoformat(ultima_str)).days
            except Exception:
                giorni = 999
            if punteggio < 2.5 or giorni > 14:
                motivo = (f"punteggio {punteggio:.1f}/3"
                          if punteggio < 2.5
                          else f"non ripassato da {giorni}gg")
                da_ripassare.append((punteggio, i, nome, desc, motivo))

    if not da_ripassare:
        console.print("\n  [bold green]✅ Ottimo! Nessun argomento da ripassare.[/]")
        console.print("  [dim]Hai padroneggiato tutto o hai studiato di recente.[/]\n")
        input("  [INVIO] ")
        return

    da_ripassare.sort(key=lambda x: x[0])
    mostra = da_ripassare[:15]

    console.print(f"\n  [bold yellow]📚 Argomenti da ripassare ({len(da_ripassare)} totali):[/]\n")
    for n, (punt, _, nome, _, motivo) in enumerate(mostra, 1):
        console.print(f"  [dim]{n:2}.[/]  [bold]{nome}[/]  [dim]— {motivo}[/]")

    try:
        scelta = input(f"\n  Numero (1–{len(mostra)}) o INVIO per annullare: ").strip()
        if not scelta:
            return
        idx = int(scelta) - 1
        if 0 <= idx < len(mostra):
            _, i_orig, nome, desc, _ = mostra[idx]
            sez_nome = sezione_di.get(i_orig, "")
            breadcrumb = f"{titolo}  ›  {sez_nome}  ›  📚 Ripasso"
            _reset_storia_conv()
            spiega_argomento(nome, desc, breadcrumb=breadcrumb)
            input("\n  [INVIO per tornare al menu] ")
    except (ValueError, IndexError, Exception):
        pass


# ── Helper generico menu ───────────────────────────────────────────────────────

def _menu_sezioni(sezioni: list, titolo: str, colore: str, con_calcolo: bool = False) -> None:
    """Menu riutilizzabile per qualsiasi sottoinsieme di sezioni."""
    # Selezione modello + preferenza ragionamento (una volta per sessione)
    _chiedi_preferenze_sessione(titolo)

    argomenti_loc = [(nome, desc) for _, voci in sezioni for nome, desc in voci]

    # Mappa: indice argomento → nome sezione (per breadcrumb)
    sezione_di: dict[int, str] = {}
    idx_a = 0
    for titolo_sez, voci_sez in sezioni:
        for _ in voci_sez:
            sezione_di[idx_a] = titolo_sez
            idx_a += 1

    while True:
        os.system("cls" if sys.platform == "win32" else "clear")

        tab = Table(box=None, show_header=False, expand=True, padding=(0, 1, 0, 0))
        tab.add_column(ratio=1)
        tab.add_column(ratio=1)
        tab.add_column(ratio=1)

        COLORI = ["green", "cyan", "yellow", "red",
                  "magenta", "bright_yellow", "bright_red", "bright_cyan"]
        n = 1
        for idx_sez, (titolo_sez, voci_sez) in enumerate(sezioni):
            c = COLORI[idx_sez % len(COLORI)]
            tab.add_row(f"[bold {c}]{titolo_sez}[/]", "", "")
            numbered = [(n + i, voci_sez[i][0]) for i in range(len(voci_sez))]
            n += len(voci_sez)
            col_size = math.ceil(len(numbered) / 3)
            cols = [numbered[i * col_size:(i + 1) * col_size] for i in range(3)]
            nrows = max(len(col) for col in cols)
            for row in range(nrows):
                cells = []
                for col in cols:
                    if row < len(col):
                        num, nome = col[row]
                        cells.append(f"[dim]{num:3}.[/] {nome}")
                    else:
                        cells.append("")
                tab.add_row(*cells)
            tab.add_row("", "", "")

        if con_calcolo:
            tab.add_row(f"[dim] D.[/]  Domanda libera",
                        f"[dim] C.[/]  Calcola (esegui codice)",
                        f"[dim] 0.[/]  Torna al menu")
        else:
            tab.add_row(f"[dim] D.[/]  Domanda libera",
                        f"[dim] 0.[/]  Torna al menu",
                        "")

        stampa_breadcrumb(console, f"Prismalux › 📚 Apprendimento › {titolo}")
        console.print(Panel(
            tab,
            title=f"[bold {colore}]{titolo}[/] — [dim]{len(argomenti_loc)} argomenti[/]",
            border_style=colore,
        ))

        try:
            prompt_inp = (f"\n  Numero (1–{len(argomenti_loc)})  |  D = Domanda libera  |  C = Calcola  |  0 = Torna: "
                          if con_calcolo else
                          f"\n  Numero (1–{len(argomenti_loc)})  |  D = Domanda libera  |  0 = Torna: ")
            scelta = input(prompt_inp).strip().upper()
        except Ridimensiona:
            continue

        if scelta == "0":
            break
        elif scelta == "D":
            chiedi_domanda_libera()
            input("\n  [INVIO per tornare al menu] ")
        elif scelta == "C" and con_calcolo:
            chiedi_calcolo()
            input("\n  [INVIO per tornare al menu] ")
        else:
            try:
                idx = int(scelta) - 1
                if 0 <= idx < len(argomenti_loc):
                    nome, descrizione = argomenti_loc[idx]
                    sez_nome = sezione_di.get(idx, "")
                    breadcrumb = f"{titolo}  ›  {sez_nome}"
                    spiega_argomento(nome, descrizione, breadcrumb=breadcrumb)
                    input("\n  [INVIO per tornare al menu] ")
            except (ValueError, IndexError):
                pass


# ── I 3 menu pubblici ─────────────────────────────────────────────────────────

def menu_python() -> None:
    _menu_sezioni(SEZIONI_PYTHON, "🐍 Python & Algoritmi", "cyan")

def menu_matematica() -> None:
    _menu_sezioni(SEZIONI_MATEMATICA, "📐 Matematica", "magenta", con_calcolo=True)

def menu_sicurezza() -> None:
    _menu_sezioni(SEZIONI_SICUREZZA, "🔒 Sicurezza Informatica & Reti", "red")

def menu_fisica() -> None:
    _menu_sezioni(SEZIONI_FISICA, "⚛️ Fisica", "yellow")

def menu_chimica() -> None:
    _menu_sezioni(SEZIONI_CHIMICA, "🧪 Chimica", "green")

# Alias per compatibilità
def menu_tutor() -> None:
    avvia_warmup()   # precarica il modello in VRAM subito
    menu_python()


if __name__ == "__main__":
    try:
        menu_python()
    except KeyboardInterrupt:
        console.print("\n\n  🍺 Alla prossima libagione di sapere.\n")

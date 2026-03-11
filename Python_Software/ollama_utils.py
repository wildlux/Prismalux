"""
ollama_utils.py — Client Ollama ottimizzato per Prismalux
==========================================================

Funzione principale:
    chiedi_ollama(prompt, tipo="risposta")

Tipi disponibili e cosa cambia:

  "estrazione"     JSON / nome / dato singolo da testo
                   → niente lingua forzata, 256 token max, temp 0.1
                   → risposta fulminea

  "analisi"        Report tecnico breve (3-5 righe), errori, log
                   → niente lingua forzata (errori sono in inglese),
                     512 token, temp 0.3

  "codice"         Genera / corregge / ottimizza codice Python
                   → niente lingua forzata (il codice è universale),
                     2048 token, temp 0.15 (deterministica)

  "risposta"       Risposta all'utente, testo medio (~1-2 paragrafi)
                   → forza italiano, 700 token, temp 0.65

  "risposta_lunga" Lettera, guida, spiegazione estesa (>3 paragrafi)
                   → forza italiano, 1400 token, temp 0.75

Ottimizzazioni comuni a tutti i tipi:
  - keep_alive "20m": il modello rimane in VRAM 20 minuti dopo l'ultima chiamata
  - num_ctx adatto al tipo: meno token = context window più piccola = più veloce
  - Warmup background: al momento dell'import parte un thread che precarica il
    modello in VRAM, così la prima vera chiamata è già calda
"""

import os
import re
import sys
import json
import threading
import requests

try:
    from ddgs import DDGS as _DDGS
    _DDGS_OK = True
except ImportError:
    _DDGS_OK = False


def cerca_web(query: str, n: int = 5) -> list[dict]:
    """Ricerca DuckDuckGo condivisa. Ritorna lista di dict con 'title','body','href'."""
    if not _DDGS_OK:
        return []
    try:
        with _DDGS() as ddgs:
            return list(ddgs.text(query, max_results=n))
    except Exception:
        return []


OLLAMA_URL  = os.environ.get("OLLAMA_URL", "http://localhost:11434/api/generate")
_OLLAMA_BASE = os.environ.get("OLLAMA_URL", "http://localhost:11434").replace("/api/generate", "")
MODELLO     = os.environ.get("OLLAMA_MODEL", "")   # "" = auto-detect al primo uso
TIMEOUT     = 300   # 5 minuti per modelli lenti al primo avvio


def _auto_modello(fallback: str = "qwen3:4b") -> str:
    """
    Ritorna il primo modello LOCALE disponibile su Ollama (esclude modelli cloud).
    I modelli cloud hanno size=0 nell'API /api/tags.
    Se Ollama non risponde o non ha modelli locali, usa `fallback`.
    """
    try:
        r = requests.get(f"{_OLLAMA_BASE}/api/tags", timeout=3)
        if r.status_code == 200:
            modelli = r.json().get("models", [])
            # size > 0 = file scaricato localmente; size == 0 = cloud/remoto
            locali = [m for m in modelli if m.get("size", 0) > 0]
            if locali:
                return locali[0]["name"]
    except Exception:
        pass
    return fallback


_modello_cache: str = ""

# ── Statistiche ultimi token generati ─────────────────────────────────────────
_last_token_stats: dict = {}


def get_last_token_stats() -> dict:
    """
    Restituisce le statistiche dell'ultima chiamata streaming completata.
    Chiavi: prompt_eval_count, eval_count, total_duration (ns).
    Ritorna dict vuoto se nessuna chiamata ancora completata.
    """
    return dict(_last_token_stats)


def get_context_window(modello: str | None = None) -> int:
    """
    Interroga /api/show per ottenere la context window del modello.
    Ritorna 0 se non disponibile.
    """
    mod = modello or _get_modello_default()
    try:
        r = requests.post(f"{_OLLAMA_BASE}/api/show", json={"model": mod}, timeout=5)
        if r.status_code == 200:
            info = r.json()
            ctx = info.get("model_info", {}).get("llama.context_length", 0)
            if not ctx:
                m = re.search(r"num_ctx\s+(\d+)", info.get("parameters", ""))
                if m:
                    ctx = int(m.group(1))
            return int(ctx) if ctx else 0
    except Exception:
        pass
    return 0


def _get_modello_default() -> str:
    """Modello di default: env OLLAMA_MODEL > cache > auto-detect locale > 'qwen3:4b'."""
    global _modello_cache
    env = os.environ.get("OLLAMA_MODEL", "")
    if env:
        return env
    if _modello_cache:
        return _modello_cache
    _modello_cache = _auto_modello()
    return _modello_cache


def scegli_modello_interattivo(argomento: str = "AI") -> str:
    """
    Mostra la lista dei modelli locali Ollama e chiede quale usare.
    Da chiamare una volta all'avvio di ogni strumento AI.
    Aggiorna _modello_cache con la scelta dell'utente.
    """
    global _modello_cache
    try:
        r = requests.get(f"{_OLLAMA_BASE}/api/tags", timeout=5)
        if r.status_code != 200:
            return _get_modello_default()
        modelli = [m for m in r.json().get("models", []) if m.get("size", 0) > 0]
        if not modelli:
            print("  ⚠ Nessun modello locale trovato. Usa: ollama pull qwen3:4b")
            return _get_modello_default()
    except Exception:
        print("  ⚠ Ollama non raggiungibile — salto selezione modello")
        return _get_modello_default()

    default_nome = _get_modello_default()
    default_idx  = next(
        (i for i, m in enumerate(modelli) if m["name"] == default_nome), 0
    )

    print(f"\n  🤖 Modello AI per {argomento}:\n")
    for i, m in enumerate(modelli):
        size_gb = m.get("size", 0) / 1024 ** 3
        marker  = "◀ default" if i == default_idx else ""
        prefisso = "→" if i == default_idx else " "
        print(f"  {prefisso} {i+1:2}. {m['name']:<38} {size_gb:4.1f} GB  {marker}")

    print(f"\n  INVIO = conferma ({modelli[default_idx]['name']})")
    try:
        scelta = input("  ❯ ").strip()
        if not scelta:
            scelto = modelli[default_idx]["name"]
        else:
            idx = int(scelta) - 1
            scelto = (modelli[idx]["name"]
                      if 0 <= idx < len(modelli)
                      else modelli[default_idx]["name"])
    except (ValueError, IndexError):
        scelto = modelli[default_idx]["name"]

    _modello_cache = scelto
    return scelto

# ── Preset per tipo ───────────────────────────────────────────────────────────
# Ogni preset: (num_predict, temperature, num_ctx, top_p, suffix_italiano)
_PRESET = {
    #                     tok   temp   ctx   top_p  ita
    "estrazione":     (  256, 0.10, 2048, 0.50, False),
    "analisi":        (  512, 0.30, 3072, 0.70, False),
    "codice":         ( 2048, 0.15, 8192, 0.90, False),
    "risposta":       (  700, 0.65, 4096, 0.90, True ),
    "risposta_lunga": ( 1400, 0.75, 4096, 0.95, True ),
    # Output strutturato breve (es. spiegazione tutor: ~150-200 token reali)
    "spiegazione":    (  800, 0.60, 3072, 0.90, True ),
}
_DEFAULT = _PRESET["risposta"]


def _preset(tipo: str) -> tuple:
    return _PRESET.get(tipo, _DEFAULT)


# ── Chiamata principale ───────────────────────────────────────────────────────

def chiedi_ollama(
    prompt: str,
    tipo: str = "risposta",
    modello: str | None = None,
) -> str:
    """
    Chiama Ollama con parametri ottimizzati per il tipo di task.

    Args:
        prompt  : testo del prompt
        tipo    : "estrazione" | "analisi" | "codice" | "risposta" | "risposta_lunga"
        modello : sovrascrive il modello di default (utile per modelli specializzati)

    Returns:
        Testo della risposta, oppure stringa di errore che inizia con "❌"
    """
    num_predict, temperature, num_ctx, top_p, forza_ita = _preset(tipo)
    mod = modello or _get_modello_default()

    testo = prompt
    if forza_ita:
        testo = prompt.rstrip() + "\n\nRispondi in italiano."

    payload = {
        "model":   mod,
        "prompt":  testo,
        "stream":  False,
        "keep_alive": "20m",
        "options": {
            "num_predict": num_predict,
            "temperature": temperature,
            "num_ctx":     num_ctx,
            "top_p":       top_p,
        },
    }

    try:
        r = requests.post(OLLAMA_URL, json=payload, timeout=TIMEOUT)
        r.raise_for_status()
        return r.json().get("response", "❌ Risposta vuota da Ollama.")
    except requests.exceptions.ConnectionError:
        return "❌ Ollama non è attivo. Avvialo con: ollama serve"
    except requests.exceptions.HTTPError as e:
        stato = getattr(getattr(e, "response", None), "status_code", None)
        if stato == 404:
            # Modello non trovato → prova con il primo disponibile
            mod_usato = payload["model"]
            mod_auto  = _auto_modello()
            if mod_auto and mod_auto != mod_usato:
                payload["model"] = mod_auto
                try:
                    r2 = requests.post(OLLAMA_URL, json=payload, timeout=TIMEOUT)
                    r2.raise_for_status()
                    return r2.json().get("response", "❌ Risposta vuota da Ollama.")
                except Exception:
                    pass
            return (f"❌ Modello '{mod_usato}' non trovato. "
                    f"Esegui: ollama pull {mod_usato}\n"
                    f"  oppure installa un modello: ollama pull qwen3:4b")
        return f"❌ Errore HTTP Ollama: {e}"
    except requests.exceptions.ReadTimeout:
        return (f"❌ Timeout dopo {TIMEOUT}s. Prova un modello più piccolo "
                f"o aumenta TIMEOUT in ollama_utils.py")
    except Exception as e:
        return f"❌ Errore Ollama: {e}"


# ── Warmup background ─────────────────────────────────────────────────────────

_warmup_done = False
_warmup_lock = threading.Lock()


def avvia_warmup(modello: str | None = None) -> None:
    """
    Avvia (una sola volta) un thread che precarica il modello in VRAM.
    Chiamalo all'avvio di ogni strumento che usa Ollama.

    Il warmup invia un prompt vuoto con keep_alive lungo: Ollama carica il
    modello in memoria senza generare token, pronto per le chiamate reali.
    """
    global _warmup_done
    with _warmup_lock:
        if _warmup_done:
            return
        _warmup_done = True

    def _esegui():
        try:
            requests.post(OLLAMA_URL, json={
                "model":      modello or _get_modello_default(),
                "prompt":     "",
                "stream":     False,
                "keep_alive": "20m",
                "options":    {"num_predict": 1},
            }, timeout=60)
        except Exception:
            pass  # silenzioso — il warmup è opzionale

    threading.Thread(target=_esegui, daemon=True, name="ollama-warmup").start()


# ── Chiamata parallela (per richieste indipendenti) ───────────────────────────

def chiedi_parallelo(
    tasks: list[tuple[str, str]],
    modello: str | None = None,
) -> list[str]:
    """
    Esegui N chiamate Ollama in parallelo e restituisci i risultati nello stesso ordine.

    Args:
        tasks   : lista di (prompt, tipo)
        modello : modello da usare (stesso per tutti)

    Returns:
        lista di risposte nello stesso ordine di tasks

    Esempio:
        risposte = chiedi_parallelo([
            (prompt_analisi,  "risposta"),
            (prompt_nome_az,  "estrazione"),
        ])
        analisi, nome_azienda = risposte
    """
    risultati = [None] * len(tasks)
    threads = []

    def _chiama(idx: int, prompt: str, tipo: str) -> None:
        risultati[idx] = chiedi_ollama(prompt, tipo, modello)

    for i, (prompt, tipo) in enumerate(tasks):
        t = threading.Thread(target=_chiama, args=(i, prompt, tipo), daemon=True)
        t.start()
        threads.append(t)

    for t in threads:
        t.join()

    return risultati


# ── Streaming token per token ─────────────────────────────────────────────────

def chiedi_ollama_stream(
    prompt: str,
    tipo: str = "spiegazione",
    modello: str | None = None,
):
    """
    Generatore: restituisce un token alla volta dalla risposta Ollama.
    Il primo token arriva in 0.5-2s invece di aspettare tutta la generazione.

    Uso tipico con Rich Live:
        testo = ""
        with Live(...) as live:
            for token in chiedi_ollama_stream(prompt, "spiegazione"):
                testo += token
                live.update(Text(testo))

    In caso di errore: yield di un messaggio "❌ ..." e il generatore si ferma.
    """
    num_predict, temperature, num_ctx, top_p, forza_ita = _preset(tipo)
    mod = modello or _get_modello_default()

    testo = prompt.rstrip() + "\n\nRispondi in italiano." if forza_ita else prompt

    payload = {
        "model":      mod,
        "prompt":     testo,
        "stream":     True,
        "keep_alive": "20m",
        "options": {
            "num_predict": num_predict,
            "temperature": temperature,
            "num_ctx":     num_ctx,
            "top_p":       top_p,
        },
    }

    try:
        with requests.post(
            OLLAMA_URL, json=payload, stream=True, timeout=(10, None)
        ) as r:
            r.raise_for_status()
            for line in r.iter_lines():
                if not line:
                    continue
                try:
                    data = json.loads(line)
                except json.JSONDecodeError:
                    continue
                token = data.get("response", "")
                if token:
                    yield token
                if data.get("done"):
                    _last_token_stats.update({
                        "prompt_eval_count": data.get("prompt_eval_count", 0),
                        "eval_count":        data.get("eval_count", 0),
                        "total_duration":    data.get("total_duration", 0),
                    })
                    break
    except requests.exceptions.ConnectionError:
        yield "\n❌ Ollama non è attivo. Avvialo con: ollama serve"
    except Exception as e:
        yield f"\n❌ Errore Ollama: {e}"


# ── Chiamata con thinking separato (non-streaming) ────────────────────────────

def chiedi_ollama_think(
    prompt: str,
    tipo: str = "risposta",
    modello: str | None = None,
) -> tuple[str, str]:
    """
    Come chiedi_ollama ma con think=True.
    Restituisce (thinking, risposta).
    thinking è stringa vuota se il modello non supporta il thinking mode.
    """
    num_predict, temperature, num_ctx, top_p, forza_ita = _preset(tipo)
    mod = modello or _get_modello_default()

    testo = prompt.rstrip() + "\n\nRispondi in italiano." if forza_ita else prompt

    payload = {
        "model":      mod,
        "prompt":     testo,
        "stream":     False,
        "keep_alive": "20m",
        "think":      True,
        "options": {
            # num_predict × 4: il thinking consuma molti token,
            # il moltiplicatore garantisce token sufficienti per la risposta finale
            "num_predict": num_predict * 4,
            "temperature": temperature,
            "num_ctx":     num_ctx,
            "top_p":       top_p,
        },
    }

    def _parse_think_response(data: dict, risposta_vuota: str) -> tuple[str, str]:
        thinking = data.get("thinking", "") or ""
        risposta = data.get("response", "") or risposta_vuota
        if not thinking:
            m = re.search(r"<think>(.*?)</think>", risposta, re.DOTALL)
            if m:
                thinking = m.group(1).strip()
                risposta = risposta[m.end():].strip()
        return thinking, risposta

    try:
        r = requests.post(OLLAMA_URL, json=payload, timeout=TIMEOUT)
        r.raise_for_status()
        return _parse_think_response(r.json(), "❌ Risposta vuota da Ollama.")
    except requests.exceptions.HTTPError as e:
        if hasattr(e, "response") and e.response is not None:
            sc = e.response.status_code
            if sc in (400, 404):
                # 400 = modello non supporta think=True
                # 404 = Ollama < 0.6 o modello non trovato
                # In entrambi i casi: riprova senza "think"
                payload_fb = {k: v for k, v in payload.items() if k != "think"}
                try:
                    r2 = requests.post(OLLAMA_URL, json=payload_fb, timeout=TIMEOUT)
                    r2.raise_for_status()
                    return "", r2.json().get("response", "") or "❌ Risposta vuota."
                except requests.exceptions.HTTPError as e2:
                    if (hasattr(e2, "response") and e2.response is not None
                            and e2.response.status_code == 404):
                        try:
                            msg = e2.response.json().get("error", "")
                        except Exception:
                            msg = ""
                        if "not found" in msg.lower():
                            # Fallback 2: prova con il primo modello locale
                            mod_auto = _auto_modello()
                            if mod_auto and mod_auto != mod:
                                payload_fb2 = {**payload_fb, "model": mod_auto}
                                try:
                                    r3 = requests.post(OLLAMA_URL, json=payload_fb2,
                                                       timeout=TIMEOUT)
                                    r3.raise_for_status()
                                    return "", (r3.json().get("response", "")
                                                or "❌ Risposta vuota.")
                                except Exception:
                                    pass
                            return "", (f"❌ Modello '{mod}' non trovato. "
                                        f"Esegui: ollama pull {mod}")
                        return "", ("❌ Endpoint /api/generate non trovato. "
                                    "Aggiorna Ollama: https://ollama.com/download")
                    return "", f"❌ Errore Ollama: {e2}"
                except Exception as e2:
                    return "", f"❌ Errore Ollama: {e2}"
        return "", f"❌ Errore HTTP Ollama: {e}"
    except requests.exceptions.ConnectionError:
        return "", "❌ Ollama non è attivo. Avvialo con: ollama serve"
    except requests.exceptions.ReadTimeout:
        return "", (f"❌ Timeout dopo {TIMEOUT}s. Prova un modello più piccolo "
                    f"o aumenta TIMEOUT in ollama_utils.py")
    except Exception as e:
        return "", f"❌ Errore Ollama: {e}"


# ── Streaming con thinking separato ───────────────────────────────────────────

def chiedi_ollama_stream_think(
    prompt: str,
    tipo: str = "spiegazione",
    modello: str | None = None,
):
    """
    Generatore con thinking mode attivo: yield tuple (fase, token).
    fase == "T"  → token di pensiero (campo 'thinking' di Ollama)
    fase == "R"  → token di risposta (campo 'response' di Ollama)

    Se il modello/versione Ollama non supporta thinking, tutti i token
    arrivano come fase "R" (comportamento identico a chiedi_ollama_stream).
    """
    num_predict, temperature, num_ctx, top_p, forza_ita = _preset(tipo)
    mod = modello or _get_modello_default()

    testo = prompt.rstrip() + "\n\nRispondi in italiano." if forza_ita else prompt

    payload = {
        "model":      mod,
        "prompt":     testo,
        "stream":     True,
        "keep_alive": "20m",
        "think":      True,
        "options": {
            "num_predict": num_predict,
            "temperature": temperature,
            "num_ctx":     num_ctx,
            "top_p":       top_p,
        },
    }

    def _stream_payload(pl: dict):
        """Generatore interno: itera le righe JSON di una POST streaming."""
        with requests.post(OLLAMA_URL, json=pl, stream=True, timeout=(10, None)) as r:
            r.raise_for_status()
            for line in r.iter_lines():
                if not line:
                    continue
                try:
                    data = json.loads(line)
                except json.JSONDecodeError:
                    continue
                tok_think = data.get("thinking", "")
                tok_resp  = data.get("response", "")
                if tok_think:
                    yield ("T", tok_think)
                if tok_resp:
                    yield ("R", tok_resp)
                if data.get("done"):
                    _last_token_stats.update({
                        "prompt_eval_count": data.get("prompt_eval_count", 0),
                        "eval_count":        data.get("eval_count", 0),
                        "total_duration":    data.get("total_duration", 0),
                    })
                    break

    try:
        yield from _stream_payload(payload)
    except requests.exceptions.HTTPError as e:
        sc_e = e.response.status_code if (hasattr(e, "response") and e.response is not None) else 0
        if sc_e in (400, 404):
            # 400 = modello non supporta think=True  |  404 = Ollama < 0.6 o modello assente
            payload_fb = {k: v for k, v in payload.items() if k != "think"}
            try:
                yield from _stream_payload(payload_fb)
                return
            except requests.exceptions.HTTPError as e2:
                sc2 = e2.response.status_code if (hasattr(e2, "response") and e2.response) else 0
                if sc2 == 404:
                    try:
                        msg = e2.response.json().get("error", "")
                    except Exception:
                        msg = ""
                    if "not found" in msg.lower():
                        yield ("R", f"\n❌ Modello '{mod}' non trovato. Esegui: ollama pull {mod}")
                    else:
                        yield ("R", "\n❌ Endpoint non trovato. Aggiorna Ollama: https://ollama.com/download")
                    return
            except Exception as e2:
                yield ("R", f"\n❌ Errore Ollama: {e2}")
                return
        yield ("R", f"\n❌ Errore HTTP Ollama: {e}")
    except requests.exceptions.ConnectionError:
        yield ("R", "\n❌ Ollama non è attivo. Avvialo con: ollama serve")
    except Exception as e:
        yield ("R", f"\n❌ Errore Ollama: {e}")


# ── Streaming con cronologia conversazione (/api/chat) ────────────────────────

def chiedi_ollama_chat_stream(
    messages: list[dict],
    tipo: str = "spiegazione",
    modello: str | None = None,
):
    """
    Streaming con cronologia conversazione (endpoint /api/chat).
    messages: lista di {"role": "system"|"user"|"assistant", "content": "..."}
    Yield: token stringa
    """
    num_predict, temperature, num_ctx, top_p, _ = _preset(tipo)
    mod = modello or _get_modello_default()

    payload = {
        "model":      mod,
        "messages":   messages,
        "stream":     True,
        "keep_alive": "20m",
        "options": {
            "num_predict": num_predict,
            "temperature": temperature,
            "num_ctx":     num_ctx,
            "top_p":       top_p,
        },
    }

    try:
        with requests.post(
            f"{_OLLAMA_BASE}/api/chat", json=payload, stream=True, timeout=(10, None)
        ) as r:
            r.raise_for_status()
            for line in r.iter_lines():
                if not line:
                    continue
                try:
                    data = json.loads(line)
                except json.JSONDecodeError:
                    continue
                token = data.get("message", {}).get("content", "")
                if token:
                    yield token
                if data.get("done"):
                    _last_token_stats.update({
                        "prompt_eval_count": data.get("prompt_eval_count", 0),
                        "eval_count":        data.get("eval_count", 0),
                        "total_duration":    data.get("total_duration", 0),
                    })
                    break
    except requests.exceptions.ConnectionError:
        yield "\n❌ Ollama non è attivo. Avvialo con: ollama serve"
    except Exception as e:
        yield f"\n❌ Errore Ollama: {e}"


def chiedi_ollama_chat_stream_think(
    messages: list[dict],
    tipo: str = "spiegazione",
    modello: str | None = None,
):
    """
    Come chiedi_ollama_chat_stream ma con thinking mode.
    Yield: tuple (fase, token) dove fase == "T" (thinking) o "R" (risposta)
    """
    num_predict, temperature, num_ctx, top_p, _ = _preset(tipo)
    mod = modello or _get_modello_default()

    payload = {
        "model":      mod,
        "messages":   messages,
        "stream":     True,
        "keep_alive": "20m",
        "think":      True,
        "options": {
            "num_predict": num_predict,
            "temperature": temperature,
            "num_ctx":     num_ctx,
            "top_p":       top_p,
        },
    }

    def _stream(pl: dict):
        with requests.post(
            f"{_OLLAMA_BASE}/api/chat", json=pl, stream=True, timeout=(10, None)
        ) as r:
            r.raise_for_status()
            for line in r.iter_lines():
                if not line:
                    continue
                try:
                    data = json.loads(line)
                except json.JSONDecodeError:
                    continue
                msg = data.get("message", {})
                tok_think = msg.get("thinking", "")
                tok_resp  = msg.get("content", "")
                if tok_think:
                    yield ("T", tok_think)
                if tok_resp:
                    yield ("R", tok_resp)
                if data.get("done"):
                    _last_token_stats.update({
                        "prompt_eval_count": data.get("prompt_eval_count", 0),
                        "eval_count":        data.get("eval_count", 0),
                        "total_duration":    data.get("total_duration", 0),
                    })
                    break

    try:
        yield from _stream(payload)
    except requests.exceptions.HTTPError as e:
        sc = e.response.status_code if (hasattr(e, "response") and e.response) else 0
        if sc in (400, 404):
            payload_fb = {k: v for k, v in payload.items() if k != "think"}
            try:
                yield from _stream(payload_fb)
                return
            except Exception as e2:
                yield ("R", f"\n❌ Errore Ollama: {e2}")
                return
        yield ("R", f"\n❌ Errore HTTP Ollama: {e}")
    except requests.exceptions.ConnectionError:
        yield ("R", "\n❌ Ollama non è attivo. Avvialo con: ollama serve")
    except Exception as e:
        yield ("R", f"\n❌ Errore Ollama: {e}")

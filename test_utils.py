#!/usr/bin/env python3
"""
test_utils.py — Utilità condivise fra i test LLM di Prismalux.
Divide et impera: ogni funzione fa UNA sola cosa.
"""
import json
import os
import urllib.request

OLLAMA_URL  = "http://127.0.0.1:11434/api/chat"
PARAMS_FILE = os.path.expanduser("~/.prismalux/ai_params.json")

_DEFAULTS = {
    "temperature": 0.7, "top_p": 0.95, "top_k": 40,
    "repeat_penalty": 1.0, "num_predict": 512, "num_ctx": 4096,
    "honesty_prefix": False,
}


def load_params(overrides: dict | None = None) -> dict:
    """Carica ai_params.json; usa _DEFAULTS per le chiavi mancanti."""
    result = {**_DEFAULTS, **(overrides or {})}
    try:
        with open(PARAMS_FILE) as f:
            result.update(json.load(f))
    except Exception:
        pass
    return result


def ask_ollama(question: str, system: str, model: str,
               params: dict, timeout: int = 90) -> str:
    """Invia una domanda a Ollama e ritorna la risposta come stringa."""
    body = {
        "model":   model,
        "stream":  False,
        "messages": [
            {"role": "system", "content": system},
            {"role": "user",   "content": question},
        ],
        "options": {
            "temperature":    params.get("temperature",    0.1),
            "top_p":          params.get("top_p",          0.9),
            "top_k":          params.get("top_k",          40),
            "repeat_penalty": params.get("repeat_penalty", 1.1),
            "num_predict":    params.get("num_predict",    512),
            "num_ctx":        params.get("num_ctx",        4096),
        },
    }
    req = urllib.request.Request(
        OLLAMA_URL,
        data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read())["message"]["content"].strip()
    except Exception as e:
        return f"[ERRORE: {e}]"


def bar(n: int, total: int, width: int = 20) -> str:
    """Barra ASCII █░ proporzionale a n/total."""
    filled = int(width * n / max(total, 1))
    return "█" * filled + "░" * (width - filled)

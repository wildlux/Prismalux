"""
MCPs/_shared/path_safety.py — Restrizioni path condivise contro lettura di
file sensibili.

Usata da ocr_mcp, rag_manager_mcp — tool che leggono un path assoluto
fornito via parametro JSON-RPC (un agente autonomo indotto poteva altrimenti
farsi restituire il contenuto di ~/.ssh/id_rsa o simili nel risultato del
tool). Non blocca ogni path fuori da una singola directory: questi tool
hanno un uso legittimo di leggere file da qualunque cartella dell'utente
(es. un PDF nei Download) — blocca solo le directory note per contenere
credenziali/segreti di sistema.
"""
import os
from pathlib import Path

DENIED_PATH_PREFIXES = ("/etc/", "/root/")
DENIED_PATH_FRAGMENTS = ("/.ssh/", "/.gnupg/", "/.aws/", "/proc/", "/sys/")


def is_sensitive_path(p) -> bool:
    s = str(p)
    return any(s.startswith(pre) for pre in DENIED_PATH_PREFIXES) or \
           any(frag in s for frag in DENIED_PATH_FRAGMENTS)


def resolve_input_path(path_str: str, root: Path) -> tuple[Path | None, str]:
    """Risolve path_str (assoluto, o relativo a root se non assoluto) in un
    Path assoluto, bloccando le directory sensibili. Ritorna (path, "") se
    OK, (None, messaggio d'errore) altrimenti."""
    p = Path(path_str).expanduser()
    if not p.is_absolute():
        p = root / p
    p = p.resolve()
    if is_sensitive_path(p):
        return None, f"[SICUREZZA] Percorso non consentito: {p}"
    return p, ""

#!/usr/bin/env python3
"""
Knowledge Updater MCP — Prismalux
Mantiene user_knowledge.md aggiornato dopo ogni sessione AI/pipeline.
Il file viene iniettato nel system_prompt di ogni agente/chat per evitare
che l'AI "dimentichi" chi è l'utente, le procedure consolidate e il contesto.

Comunicazione: JSON-RPC 2.0 su stdio (protocollo MCP standard).

Uso con Claude Code — aggiungi in ~/.claude/settings.json:
  {
    "mcpServers": {
      "knowledge_updater": {
        "type": "stdio",
        "command": "python3",
        "args": ["/home/wildlux/Desktop/Prismalux/MCPs/knowledge_mcp/server.py"]
      }
    }
  }

Uso con Prismalux (Ollama) — il C++ legge direttamente user_knowledge.md,
il MCP serve per aggiornarlo da pipeline/chat dopo ogni sessione.
"""

import sys
import json
import os
import re
import fcntl
from pathlib import Path
from datetime import datetime

# ─── Configurazione ───────────────────────────────────────────────────────────

KNOWLEDGE_DIR  = Path(__file__).parent.parent.parent / "KNOWLEDGE_USER"
KNOWLEDGE_FILE = KNOWLEDGE_DIR / "user_knowledge.md"

# Sezioni valide (ordine fisso nel file)
SECTIONS = [
    "chi_sono",
    "preferenze",
    "progetto",
    "procedure",
    "ragionamenti",
    "contesto",
]

# Titoli leggibili delle sezioni (usati come header ## nel file)
SECTION_TITLES = {
    "chi_sono":    "Chi sono",
    "preferenze":  "Preferenze di risposta",
    "progetto":    "Progetto attuale",
    "procedure":   "Procedure e algoritmi consolidati",
    "ragionamenti":"Ragionamenti salvati",
    "contesto":    "Contesto sessioni recenti",
}

# Max voci per sezioni rotatorie (evita che il file cresca illimitatamente)
MAX_ENTRIES = {
    "ragionamenti": 15,
    "contesto":     10,
}


# ─── Gestione file ────────────────────────────────────────────────────────────

def _ensure_file():
    """Crea user_knowledge.md con template vuoto se non esiste."""
    if KNOWLEDGE_FILE.exists():
        return
    lines = ["# user_knowledge.md — Memoria Persistente Prismalux\n\n"]
    lines.append("> Questo file viene iniettato nel system_prompt di ogni AI/agente.\n")
    lines.append("> Aggiornato automaticamente dal MCP knowledge_updater.\n\n")
    for sec in SECTIONS:
        lines.append(f"## {SECTION_TITLES[sec]}\n\n<!-- vuoto -->\n\n")
    KNOWLEDGE_FILE.write_text("".join(lines), encoding="utf-8")


def _read_raw() -> str:
    _ensure_file()
    return KNOWLEDGE_FILE.read_text(encoding="utf-8")


def _write_raw(content: str):
    """Scrive il file con lock esclusivo per evitare race condition."""
    _ensure_file()
    with open(KNOWLEDGE_FILE, "w", encoding="utf-8") as f:
        try:
            fcntl.flock(f, fcntl.LOCK_EX)
            f.write(content)
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)


def _parse_sections(raw: str) -> dict:
    """
    Divide il testo in sezioni usando i titoli ## come separatori.
    Ritorna {section_key: (header_line, body_text)}.
    """
    result = {}
    # Cerca ogni ## Titolo e prende tutto fino al prossimo ## o fine file
    pattern = re.compile(r"^(## .+)$", re.MULTILINE)
    matches = list(pattern.finditer(raw))

    # Costruisce mapping titolo → chiave
    title_to_key = {v: k for k, v in SECTION_TITLES.items()}

    for i, m in enumerate(matches):
        header = m.group(1).strip()
        title  = header[3:].strip()   # rimuove "## "
        key    = title_to_key.get(title)
        if key is None:
            continue
        start = m.end()
        end   = matches[i + 1].start() if i + 1 < len(matches) else len(raw)
        body  = raw[start:end].strip()
        result[key] = (header, body)

    return result


def _rebuild_file(sections_data: dict, preamble: str) -> str:
    """Ricostruisce il file nell'ordine canonico delle sezioni."""
    parts = [preamble.rstrip() + "\n\n"]
    for sec in SECTIONS:
        title  = SECTION_TITLES[sec]
        header = f"## {title}"
        body   = sections_data.get(sec, ("", "<!-- vuoto -->"))[1]
        if not body:
            body = "<!-- vuoto -->"
        parts.append(f"{header}\n\n{body}\n\n")
    return "".join(parts)


def _extract_preamble(raw: str) -> str:
    """Restituisce tutto ciò che precede il primo ## header."""
    m = re.search(r"^## ", raw, re.MULTILINE)
    return raw[:m.start()] if m else ""


# ─── Logica sezione rotatoria ─────────────────────────────────────────────────

def _rotate_entries(body: str, new_entry: str, max_entries: int) -> str:
    """
    Aggiunge new_entry in cima alla sezione e rimuove le voci più vecchie
    se il numero supera max_entries. Ogni voce è separata da riga vuota.
    """
    if body.strip() in ("<!-- vuoto -->", ""):
        return new_entry.strip()

    entries = [e.strip() for e in re.split(r"\n{2,}", body.strip()) if e.strip()]
    entries.insert(0, new_entry.strip())
    entries = entries[:max_entries]   # mantieni solo le più recenti
    return "\n\n".join(entries)


# ─── Strumenti esposti ────────────────────────────────────────────────────────

TOOLS = [
    {
        "name": "update_knowledge",
        "description": (
            "Aggiorna una sezione di user_knowledge.md con nuove informazioni "
            "emerse dalla sessione corrente.\n\n"
            "Sezioni disponibili:\n"
            "- chi_sono: ruolo, background, livello tecnico, obiettivi\n"
            "- preferenze: stile risposte, lingua, cosa evitare\n"
            "- progetto: nome, stack, stato, priorità\n"
            "- procedure: algoritmi e flussi consolidati (es. protocollo anti-allucinazione)\n"
            "- ragionamenti: decisioni prese con motivazione (rotazione: max 15)\n"
            "- contesto: ultime decisioni importanti, bug noti, TODO aperti (rotazione: max 10)\n\n"
            "Modalità:\n"
            "- append: aggiunge il testo in fondo (o in cima per sezioni rotatorie)\n"
            "- replace_section: sostituisce l'intera sezione"
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "section": {
                    "type": "string",
                    "enum": SECTIONS,
                    "description": "Sezione da aggiornare."
                },
                "content": {
                    "type": "string",
                    "description": "Testo da aggiungere. Usa markdown. Per 'ragionamenti' e 'contesto' includi una data in cima (es: **2026-05-03** — ...)."
                },
                "mode": {
                    "type": "string",
                    "enum": ["append", "replace_section"],
                    "default": "append",
                    "description": "append = aggiunge; replace_section = sostituisce tutto."
                }
            },
            "required": ["section", "content"]
        }
    },
    {
        "name": "read_knowledge",
        "description": (
            "Legge user_knowledge.md (tutto o una sezione specifica).\n"
            "Usa questo tool per iniettare il contesto utente nel system_prompt "
            "prima di iniziare una sessione AI."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "section": {
                    "type": "string",
                    "enum": SECTIONS,
                    "description": "Sezione specifica da leggere. Ometti per leggere tutto il file."
                }
            }
        }
    },
    {
        "name": "list_sections",
        "description": "Elenca le sezioni disponibili con il loro stato (vuoto/popolato) e numero di caratteri.",
        "inputSchema": {
            "type": "object",
            "properties": {}
        }
    },
    {
        "name": "auto_extract_and_update",
        "description": (
            "Analizza un riassunto della sessione e aggiorna automaticamente "
            "le sezioni rilevanti. Chiamato tipicamente alla fine di una pipeline "
            "o di una chat lunga. L'AI deve fornire il riassunto; il MCP decide "
            "dove posizionare ogni informazione.\n\n"
            "Il campo 'summary' deve contenere le informazioni in formato strutturato:\n"
            "  PREFERENZE: ...\n"
            "  PROGETTO: ...\n"
            "  PROCEDURA: ...\n"
            "  DECISIONE: ...\n"
            "  CONTESTO: ...\n"
            "Ogni riga con prefisso viene estratta e aggiunta alla sezione corrispondente."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "summary": {
                    "type": "string",
                    "description": "Riassunto della sessione con prefissi PREFERENZE/PROGETTO/PROCEDURA/DECISIONE/CONTESTO."
                },
                "session_label": {
                    "type": "string",
                    "description": "Etichetta breve della sessione (es. 'Pipeline codice Qt 2026-05-03'). Usata come titolo voce in 'contesto'."
                }
            },
            "required": ["summary"]
        }
    }
]


# ─── Implementazione strumenti ────────────────────────────────────────────────

def tool_update_knowledge(args: dict) -> str:
    section = args.get("section", "").strip()
    content = args.get("content", "").strip()
    mode    = args.get("mode", "append")

    if section not in SECTIONS:
        return f"[Errore] Sezione '{section}' non valida. Valide: {', '.join(SECTIONS)}"
    if not content:
        return "[Errore] Il campo 'content' è obbligatorio."

    raw      = _read_raw()
    preamble = _extract_preamble(raw)
    sections = _parse_sections(raw)

    current_body = sections.get(section, ("", "<!-- vuoto -->"))[1]

    if mode == "replace_section":
        new_body = content
    else:
        # append — con rotazione per sezioni con limite
        max_e = MAX_ENTRIES.get(section)
        if max_e:
            ts      = datetime.now().strftime("%Y-%m-%d")
            entry   = f"**{ts}** — {content}"
            new_body = _rotate_entries(current_body, entry, max_e)
        else:
            if current_body.strip() in ("<!-- vuoto -->", ""):
                new_body = content
            else:
                new_body = current_body.rstrip() + "\n\n" + content

    sections[section] = (f"## {SECTION_TITLES[section]}", new_body)
    _write_raw(_rebuild_file(sections, preamble))

    char_count = len(new_body)
    return (f"Sezione '{SECTION_TITLES[section]}' aggiornata "
            f"({char_count} caratteri). File: {KNOWLEDGE_FILE}")


def tool_read_knowledge(args: dict) -> str:
    section = args.get("section", "").strip()
    raw     = _read_raw()

    if not section:
        return raw

    sections = _parse_sections(raw)
    if section not in SECTIONS:
        return f"[Errore] Sezione '{section}' non valida. Valide: {', '.join(SECTIONS)}"

    data = sections.get(section)
    if not data:
        return f"[Sezione '{SECTION_TITLES[section]}' non trovata o vuota]"

    header, body = data
    return f"{header}\n\n{body}"


def tool_list_sections(args: dict) -> str:
    raw      = _read_raw()
    sections = _parse_sections(raw)
    lines    = ["Sezioni in user_knowledge.md:\n"]
    for sec in SECTIONS:
        title = SECTION_TITLES[sec]
        data  = sections.get(sec)
        if data:
            body   = data[1]
            empty  = body.strip() in ("<!-- vuoto -->", "")
            stato  = "vuoto" if empty else f"{len(body)} caratteri"
            rot    = f" [rotazione max {MAX_ENTRIES[sec]}]" if sec in MAX_ENTRIES else ""
            lines.append(f"  • {sec:15s} — {title}{rot}: {stato}")
        else:
            lines.append(f"  • {sec:15s} — {title}: [non presente]")
    lines.append(f"\nFile: {KNOWLEDGE_FILE}")
    return "\n".join(lines)


# Mappatura prefissi → sezioni per auto_extract
_PREFIX_MAP = {
    "PREFERENZE":  "preferenze",
    "PROGETTO":    "progetto",
    "PROCEDURA":   "procedure",
    "DECISIONE":   "ragionamenti",
    "CONTESTO":    "contesto",
}

def tool_auto_extract(args: dict) -> str:
    summary       = args.get("summary", "").strip()
    session_label = args.get("session_label", "").strip()

    if not summary:
        return "[Errore] Il campo 'summary' è obbligatorio."

    raw      = _read_raw()
    preamble = _extract_preamble(raw)
    sections = _parse_sections(raw)
    updated  = []

    for line in summary.splitlines():
        line = line.strip()
        for prefix, sec in _PREFIX_MAP.items():
            if line.upper().startswith(f"{prefix}:"):
                content = line[len(prefix) + 1:].strip()
                if not content:
                    continue
                current_body = sections.get(sec, ("", "<!-- vuoto -->"))[1]
                max_e = MAX_ENTRIES.get(sec)
                ts    = datetime.now().strftime("%Y-%m-%d")
                if max_e:
                    entry    = f"**{ts}** — {content}"
                    new_body = _rotate_entries(current_body, entry, max_e)
                else:
                    entry = content
                    if current_body.strip() in ("<!-- vuoto -->", ""):
                        new_body = entry
                    else:
                        new_body = current_body.rstrip() + "\n\n" + entry
                sections[sec] = (f"## {SECTION_TITLES[sec]}", new_body)
                updated.append(f"  • {SECTION_TITLES[sec]}: '{content[:60]}...' " if len(content) > 60 else f"  • {SECTION_TITLES[sec]}: '{content}'")
                break

    # Aggiunge sempre una voce in 'contesto' con il label sessione
    if session_label:
        ts    = datetime.now().strftime("%Y-%m-%d %H:%M")
        entry = f"**{ts}** [{session_label}] — sessione completata"
        current_ctx = sections.get("contesto", ("", "<!-- vuoto -->"))[1]
        new_ctx     = _rotate_entries(current_ctx, entry, MAX_ENTRIES["contesto"])
        sections["contesto"] = (f"## {SECTION_TITLES['contesto']}", new_ctx)
        updated.append(f"  • {SECTION_TITLES['contesto']}: log sessione aggiunto")

    if not updated:
        return (
            "[Nessuna sezione aggiornata] Il summary non conteneva prefissi riconosciuti.\n"
            "Usa i prefissi: PREFERENZE: / PROGETTO: / PROCEDURA: / DECISIONE: / CONTESTO:"
        )

    _write_raw(_rebuild_file(sections, preamble))
    return "Sezioni aggiornate:\n" + "\n".join(updated) + f"\n\nFile: {KNOWLEDGE_FILE}"


TOOL_HANDLERS = {
    "update_knowledge":        tool_update_knowledge,
    "read_knowledge":          tool_read_knowledge,
    "list_sections":           tool_list_sections,
    "auto_extract_and_update": tool_auto_extract,
}


# ─── Protocollo MCP (JSON-RPC 2.0 su stdio) ──────────────────────────────────

def _send(obj: dict):
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def _error(req_id, code: int, message: str):
    _send({"jsonrpc": "2.0", "id": req_id, "error": {"code": code, "message": message}})


def _result(req_id, result):
    _send({"jsonrpc": "2.0", "id": req_id, "result": result})


def handle(request: dict):
    method = request.get("method", "")
    req_id = request.get("id")
    params = request.get("params", {}) or {}

    if method == "initialize":
        _result(req_id, {
            "protocolVersion": "2024-11-05",
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "knowledge-updater", "version": "1.0.0"}
        })

    elif method == "notifications/initialized":
        pass

    elif method == "tools/list":
        _result(req_id, {"tools": TOOLS})

    elif method == "tools/call":
        name      = params.get("name", "")
        tool_args = params.get("arguments", {}) or {}
        handler   = TOOL_HANDLERS.get(name)
        if not handler:
            _error(req_id, -32601, f"Strumento '{name}' non trovato.")
            return
        try:
            text = handler(tool_args)
        except Exception as e:
            text = f"[Errore strumento] {e}"
        _result(req_id, {
            "content": [{"type": "text", "text": text}],
            "isError": text.startswith("[Errore")
        })

    elif method == "ping":
        _result(req_id, {})

    elif req_id is not None:
        _error(req_id, -32601, f"Metodo '{method}' non trovato.")


def main():
    for raw_line in sys.stdin:
        raw_line = raw_line.strip()
        if not raw_line:
            continue
        try:
            request = json.loads(raw_line)
        except json.JSONDecodeError as e:
            _send({"jsonrpc": "2.0", "id": None,
                   "error": {"code": -32700, "message": f"Parse error: {e}"}})
            continue
        try:
            handle(request)
        except Exception as e:
            req_id = request.get("id")
            if req_id is not None:
                _error(req_id, -32603, f"Internal error: {e}")


if __name__ == "__main__":
    main()

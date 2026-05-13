#!/usr/bin/env python3
"""
Anki MCP — Prismalux
Bridge verso AnkiConnect (porta 8765). Richiede Anki aperto con addon AnkiConnect.
Codice addon: 2055492159

Uso con Claude Code — aggiungi in ~/.claude/settings.json:
  { "mcpServers": { "anki": { "type": "stdio", "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/anki_mcp/server.py"] } } }
"""
import sys, json
import urllib.request, urllib.error

ANKI_URL = "http://localhost:8765"

def _anki(action: str, **params):
    payload = json.dumps({"action": action, "version": 6, "params": params}).encode()
    req = urllib.request.Request(ANKI_URL, data=payload,
                                  headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            resp = json.loads(r.read())
        if resp.get("error"):
            return None, resp["error"]
        return resp.get("result"), None
    except Exception as e:
        return None, str(e)

TOOLS = [
    {"name": "get_decks",
     "description": "Restituisce la lista di tutti i mazzi Anki.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "create_note",
     "description": "Crea una flashcard in un mazzo Anki.",
     "inputSchema": {"type": "object", "required": ["deck", "front", "back"],
        "properties": {
            "deck":  {"type": "string", "description": "Nome del mazzo (es. 'Default')"},
            "front": {"type": "string", "description": "Fronte della carta (domanda)"},
            "back":  {"type": "string", "description": "Retro della carta (risposta)"},
            "tags":  {"type": "array", "items": {"type": "string"}, "description": "Tag opzionali"}}}},
    {"name": "search_notes",
     "description": "Cerca note in Anki con query (es. 'deck:Default tag:italiano').",
     "inputSchema": {"type": "object", "required": ["query"],
        "properties": {"query": {"type": "string"}}}},
    {"name": "add_deck",
     "description": "Crea un nuovo mazzo Anki se non esiste.",
     "inputSchema": {"type": "object", "required": ["name"],
        "properties": {"name": {"type": "string"}}}},
    {"name": "get_stats",
     "description": "Statistiche del mazzo (carte totali, nuove, in revisione).",
     "inputSchema": {"type": "object", "required": ["deck"],
        "properties": {"deck": {"type": "string"}}}},
]

def tool_get_decks(_):
    res, err = _anki("deckNames")
    if err: return f"[Errore AnkiConnect] {err}\nAssicurati che Anki sia aperto e AnkiConnect sia installato."
    return "Mazzi disponibili:\n" + "\n".join(f"  • {d}" for d in res)

def tool_create_note(args):
    note = {"deckName": args["deck"], "modelName": "Basic",
            "fields": {"Front": args["front"], "Back": args["back"]},
            "tags": args.get("tags", []), "options": {"allowDuplicate": False}}
    res, err = _anki("addNote", note=note)
    if err: return f"[Errore] {err}"
    return f"Flashcard creata (id={res}) nel mazzo '{args['deck']}'."

def tool_search_notes(args):
    res, err = _anki("findNotes", query=args["query"])
    if err: return f"[Errore] {err}"
    if not res: return "Nessuna nota trovata."
    info, err2 = _anki("notesInfo", notes=res[:20])
    if err2: return f"Trovate {len(res)} note (ids: {res[:5]}...)."
    lines = [f"Trovate {len(res)} note (prime {len(info)}):"]
    for n in info:
        front = n.get("fields", {}).get("Front", {}).get("value", "?")
        lines.append(f"  [{n['noteId']}] {front[:80]}")
    return "\n".join(lines)

def tool_add_deck(args):
    res, err = _anki("createDeck", deck=args["name"])
    if err: return f"[Errore] {err}"
    return f"Mazzo '{args['name']}' creato (id={res})."

def tool_get_stats(args):
    res, err = _anki("getDeckStats", decks=[args["deck"]])
    if err: return f"[Errore] {err}"
    if not res: return "Statistiche non disponibili."
    stats = list(res.values())[0]
    return (f"Mazzo: {args['deck']}\n"
            f"  Totale: {stats.get('total_in_deck', '?')}\n"
            f"  Nuove:  {stats.get('new_count', '?')}\n"
            f"  Studio: {stats.get('learn_count', '?')}\n"
            f"  Review: {stats.get('review_count', '?')}")

HANDLERS = {"get_decks": tool_get_decks, "create_note": tool_create_note,
            "search_notes": tool_search_notes, "add_deck": tool_add_deck,
            "get_stats": tool_get_stats}

def _send(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid, c, m): _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid, r):   _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req):
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"anki-mcp","version":"1.0.0"}})
    elif m == "tools/list":
        _result(rid, {"tools": TOOLS})
    elif m == "tools/call":
        name, args = p.get("name",""), p.get("arguments",{}) or {}
        h = HANDLERS.get(name)
        if not h: _error(rid, -32601, f"Strumento '{name}' non trovato."); return
        try: text = h(args)
        except Exception as e: text = f"[Errore] {e}"
        _result(rid, {"content":[{"type":"text","text":text}],"isError":text.startswith("[Errore")})
    elif m in ("notifications/initialized","ping"):
        if rid is not None: _result(rid, {})
    elif rid is not None:
        _error(rid, -32601, f"Metodo '{m}' non trovato.")

def main():
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: req = json.loads(line)
        except json.JSONDecodeError as e:
            _send({"jsonrpc":"2.0","id":None,"error":{"code":-32700,"message":str(e)}})
            continue
        try: handle(req)
        except Exception as e:
            if req.get("id"): _error(req["id"], -32603, str(e))

if __name__ == "__main__": main()

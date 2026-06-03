#!/usr/bin/env python3
"""
Anki MCP — Prismalux v0.18.5 (compatibile con anki-mcp-server API)
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
            "tags":  {"type": "array", "items": {"type": "string"}, "description": "Tag opzionali"},
            "model": {"type": "string", "description": "Tipo nota (default: Basic)"}}}},
    {"name": "create_notes_bulk",
     "description": "Crea multiple flashcard in un mazzo Anki in una sola chiamata.",
     "inputSchema": {"type": "object", "required": ["deck", "notes"],
        "properties": {
            "deck":  {"type": "string", "description": "Nome del mazzo"},
            "notes": {"type": "array", "description": "Array di {front, back, tags?, model?}",
                      "items": {"type": "object",
                                "properties": {
                                    "front": {"type": "string"},
                                    "back":  {"type": "string"},
                                    "tags":  {"type": "array", "items": {"type": "string"}},
                                    "model": {"type": "string"}}}}}}},
    {"name": "search_notes",
     "description": "Cerca note in Anki con query (es. 'deck:Default tag:italiano').",
     "inputSchema": {"type": "object", "required": ["query"],
        "properties": {
            "query": {"type": "string"},
            "limit": {"type": "integer", "description": "Max risultati (default 20)"}}}},
    {"name": "get_note",
     "description": "Ottieni i dettagli completi di una nota Anki tramite ID.",
     "inputSchema": {"type": "object", "required": ["note_id"],
        "properties": {"note_id": {"type": "integer"}}}},
    {"name": "update_note",
     "description": "Aggiorna i campi di una nota Anki esistente.",
     "inputSchema": {"type": "object", "required": ["note_id"],
        "properties": {
            "note_id": {"type": "integer"},
            "front":   {"type": "string"},
            "back":    {"type": "string"},
            "tags":    {"type": "array", "items": {"type": "string"}}}}},
    {"name": "delete_notes",
     "description": "Elimina una o più note Anki tramite ID.",
     "inputSchema": {"type": "object", "required": ["note_ids"],
        "properties": {"note_ids": {"type": "array", "items": {"type": "integer"}}}}},
    {"name": "add_deck",
     "description": "Crea un nuovo mazzo Anki se non esiste.",
     "inputSchema": {"type": "object", "required": ["name"],
        "properties": {"name": {"type": "string"}}}},
    {"name": "get_stats",
     "description": "Statistiche del mazzo (carte totali, nuove, in revisione).",
     "inputSchema": {"type": "object", "required": ["deck"],
        "properties": {"deck": {"type": "string"}}}},
    {"name": "get_note_types",
     "description": "Restituisce tutti i tipi di nota (modelli) disponibili in Anki.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "get_tags",
     "description": "Restituisce tutti i tag usati in Anki.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "sync_anki",
     "description": "Sincronizza Anki con AnkiWeb (richiede account AnkiWeb configurato).",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "get_cards_due",
     "description": "Restituisce le carte in scadenza oggi per un mazzo.",
     "inputSchema": {"type": "object", "required": ["deck"],
        "properties": {"deck": {"type": "string"}}}},
]

def tool_get_decks(_):
    res, err = _anki("deckNames")
    if err: return f"[Errore AnkiConnect] {err}\nAssicurati che Anki sia aperto e AnkiConnect sia installato."
    return "Mazzi disponibili:\n" + "\n".join(f"  • {d}" for d in res)

def tool_create_note(args):
    model = args.get("model", "Basic")
    note = {"deckName": args["deck"], "modelName": model,
            "fields": {"Front": args["front"], "Back": args["back"]},
            "tags": args.get("tags", []), "options": {"allowDuplicate": False}}
    res, err = _anki("addNote", note=note)
    if err: return f"[Errore] {err}"
    return f"Flashcard creata (id={res}) nel mazzo '{args['deck']}'."

def tool_create_notes_bulk(args):
    deck = args["deck"]
    notes_in = args["notes"]
    notes = []
    for n in notes_in:
        model = n.get("model", "Basic")
        notes.append({
            "deckName": deck, "modelName": model,
            "fields": {"Front": n["front"], "Back": n["back"]},
            "tags": n.get("tags", []), "options": {"allowDuplicate": False}
        })
    res, err = _anki("addNotes", notes=notes)
    if err: return f"[Errore] {err}"
    created = sum(1 for r in (res or []) if r is not None)
    return f"Create {created}/{len(notes)} flashcard nel mazzo '{deck}'."

def tool_search_notes(args):
    limit = args.get("limit", 20)
    res, err = _anki("findNotes", query=args["query"])
    if err: return f"[Errore] {err}"
    if not res: return "Nessuna nota trovata."
    ids = res[:limit]
    info, err2 = _anki("notesInfo", notes=ids)
    if err2: return f"Trovate {len(res)} note (ids: {res[:5]}...)."
    lines = [f"Trovate {len(res)} note (prime {len(info)}):"]
    for n in info:
        front = n.get("fields", {}).get("Front", {}).get("value", "?")
        tags = ", ".join(n.get("tags", [])) or "—"
        lines.append(f"  [{n['noteId']}] {front[:80]}  (tag: {tags})")
    return "\n".join(lines)

def tool_get_note(args):
    info, err = _anki("notesInfo", notes=[args["note_id"]])
    if err: return f"[Errore] {err}"
    if not info: return "Nota non trovata."
    n = info[0]
    fields = {k: v.get("value","") for k, v in n.get("fields", {}).items()}
    return json.dumps({"id": n["noteId"], "tags": n.get("tags",[]),
                       "deck": n.get("deckName","?"), "fields": fields}, ensure_ascii=False, indent=2)

def tool_update_note(args):
    note = {"id": args["note_id"], "fields": {}}
    if "front" in args: note["fields"]["Front"] = args["front"]
    if "back"  in args: note["fields"]["Back"]  = args["back"]
    _, err = _anki("updateNoteFields", note=note)
    if err: return f"[Errore] {err}"
    if "tags" in args:
        _, err2 = _anki("updateNoteTags", note=args["note_id"], tags=" ".join(args["tags"]))
        if err2: return f"Campi aggiornati ma errore tag: {err2}"
    return f"Nota {args['note_id']} aggiornata."

def tool_delete_notes(args):
    _, err = _anki("deleteNotes", notes=args["note_ids"])
    if err: return f"[Errore] {err}"
    return f"Eliminate {len(args['note_ids'])} note."

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

def tool_get_note_types(_):
    res, err = _anki("modelNames")
    if err: return f"[Errore] {err}"
    return "Tipi di nota:\n" + "\n".join(f"  • {m}" for m in res)

def tool_get_tags(_):
    res, err = _anki("getTags")
    if err: return f"[Errore] {err}"
    if not res: return "Nessun tag trovato."
    return "Tag: " + ", ".join(sorted(res))

def tool_sync_anki(_):
    _, err = _anki("sync")
    if err: return f"[Errore sync] {err}"
    return "Sincronizzazione Anki avviata."

def tool_get_cards_due(args):
    res, err = _anki("findCards", query=f"deck:{args['deck']} is:due")
    if err: return f"[Errore] {err}"
    count = len(res) if res else 0
    return f"Carte in scadenza oggi nel mazzo '{args['deck']}': {count}"

HANDLERS = {
    "get_decks": tool_get_decks,
    "create_note": tool_create_note,
    "create_notes_bulk": tool_create_notes_bulk,
    "search_notes": tool_search_notes,
    "get_note": tool_get_note,
    "update_note": tool_update_note,
    "delete_notes": tool_delete_notes,
    "add_deck": tool_add_deck,
    "get_stats": tool_get_stats,
    "get_note_types": tool_get_note_types,
    "get_tags": tool_get_tags,
    "sync_anki": tool_sync_anki,
    "get_cards_due": tool_get_cards_due,
}

def _send(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid, c, m): _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid, r):   _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req):
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"anki-mcp","version":"0.18.5"}})
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

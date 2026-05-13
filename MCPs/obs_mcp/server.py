#!/usr/bin/env python3
"""
OBS MCP — Prismalux
Controlla OBS Studio via obs-websocket (porta 4455).
Prerequisiti: pip install obs-websocket-py  |  OBS → Tools → WebSocket Settings → Enable
"""
import sys, json

def _obs():
    try:
        import obsws_python as obs
        return obs
    except ImportError:
        return None

def _connect(password=""):
    obs = _obs()
    if not obs:
        return None, "[Errore] obs-websocket-py non installato. Esegui: pip install obs-websocket-py"
    try:
        cl = obs.ReqClient(host="localhost", port=4455, password=password, timeout=5)
        return cl, None
    except Exception as e:
        return None, f"[Errore connessione OBS] {e}\nAssicurati che OBS sia aperto con WebSocket abilitato (porta 4455)."

OBS_PASSWORD = ""

TOOLS = [
    {"name": "list_scenes",
     "description": "Elenca tutte le scene OBS e indica quella attiva.",
     "inputSchema": {"type": "object", "properties": {
        "password": {"type": "string", "description": "Password WebSocket OBS (lascia vuoto se non impostata)"}}}},
    {"name": "set_scene",
     "description": "Cambia la scena attiva in OBS.",
     "inputSchema": {"type": "object", "required": ["scene_name"],
        "properties": {
            "scene_name": {"type": "string"},
            "password":   {"type": "string", "default": ""}}}},
    {"name": "start_recording",
     "description": "Avvia la registrazione in OBS.",
     "inputSchema": {"type": "object", "properties": {
        "password": {"type": "string", "default": ""}}}},
    {"name": "stop_recording",
     "description": "Ferma la registrazione in OBS.",
     "inputSchema": {"type": "object", "properties": {
        "password": {"type": "string", "default": ""}}}},
    {"name": "start_streaming",
     "description": "Avvia lo streaming in OBS.",
     "inputSchema": {"type": "object", "properties": {
        "password": {"type": "string", "default": ""}}}},
    {"name": "stop_streaming",
     "description": "Ferma lo streaming in OBS.",
     "inputSchema": {"type": "object", "properties": {
        "password": {"type": "string", "default": ""}}}},
    {"name": "set_source_visible",
     "description": "Mostra o nasconde una sorgente nella scena corrente.",
     "inputSchema": {"type": "object", "required": ["scene_name", "source_name", "visible"],
        "properties": {
            "scene_name":  {"type": "string"},
            "source_name": {"type": "string"},
            "visible":     {"type": "boolean"},
            "password":    {"type": "string", "default": ""}}}},
    {"name": "get_status",
     "description": "Restituisce lo stato attuale di OBS (streaming/recording, scena attiva).",
     "inputSchema": {"type": "object", "properties": {
        "password": {"type": "string", "default": ""}}}},
]

def tool_list_scenes(args):
    cl, err = _connect(args.get("password", ""))
    if err: return err
    try:
        resp = cl.get_scene_list()
        current = resp.current_program_scene_name
        lines = [f"Scena attiva: {current}\n\nScene disponibili:"]
        for s in resp.scenes:
            marker = " ◀ ATTIVA" if s.get("sceneName") == current else ""
            lines.append(f"  • {s.get('sceneName','?')}{marker}")
        return "\n".join(lines)
    except Exception as e:
        return f"[Errore] {e}"

def tool_set_scene(args):
    cl, err = _connect(args.get("password", ""))
    if err: return err
    try:
        cl.set_current_program_scene(args["scene_name"])
        return f"Scena cambiata: {args['scene_name']}"
    except Exception as e:
        return f"[Errore] {e}"

def tool_start_recording(args):
    cl, err = _connect(args.get("password", ""))
    if err: return err
    try:
        cl.start_record()
        return "Registrazione avviata."
    except Exception as e:
        return f"[Errore] {e}"

def tool_stop_recording(args):
    cl, err = _connect(args.get("password", ""))
    if err: return err
    try:
        resp = cl.stop_record()
        return f"Registrazione fermata.\nFile: {getattr(resp, 'output_path', '?')}"
    except Exception as e:
        return f"[Errore] {e}"

def tool_start_streaming(args):
    cl, err = _connect(args.get("password", ""))
    if err: return err
    try:
        cl.start_stream()
        return "Streaming avviato."
    except Exception as e:
        return f"[Errore] {e}"

def tool_stop_streaming(args):
    cl, err = _connect(args.get("password", ""))
    if err: return err
    try:
        cl.stop_stream()
        return "Streaming fermato."
    except Exception as e:
        return f"[Errore] {e}"

def tool_set_source_visible(args):
    cl, err = _connect(args.get("password", ""))
    if err: return err
    try:
        items = cl.get_scene_item_list(args["scene_name"]).scene_items
        item_id = next((i["sceneItemId"] for i in items if i.get("sourceName") == args["source_name"]), None)
        if item_id is None:
            return f"[Errore] Sorgente '{args['source_name']}' non trovata nella scena '{args['scene_name']}'."
        cl.set_scene_item_enabled(args["scene_name"], item_id, args["visible"])
        stato = "visibile" if args["visible"] else "nascosta"
        return f"Sorgente '{args['source_name']}' {stato}."
    except Exception as e:
        return f"[Errore] {e}"

def tool_get_status(args):
    cl, err = _connect(args.get("password", ""))
    if err: return err
    try:
        rec = cl.get_record_status()
        stream = cl.get_stream_status()
        scene = cl.get_current_program_scene()
        return (f"OBS Status:\n"
                f"  Scena attiva:  {scene.current_program_scene_name}\n"
                f"  Registrazione: {'🔴 IN CORSO' if rec.output_active else '⚫ Ferma'}\n"
                f"  Streaming:     {'🔴 LIVE' if stream.output_active else '⚫ Fermo'}")
    except Exception as e:
        return f"[Errore] {e}"

HANDLERS = {"list_scenes": tool_list_scenes, "set_scene": tool_set_scene,
            "start_recording": tool_start_recording, "stop_recording": tool_stop_recording,
            "start_streaming": tool_start_streaming, "stop_streaming": tool_stop_streaming,
            "set_source_visible": tool_set_source_visible, "get_status": tool_get_status}

def _send(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid, c, m): _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid, r):   _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req):
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"obs-mcp","version":"1.0.0"}})
    elif m == "tools/list": _result(rid, {"tools": TOOLS})
    elif m == "tools/call":
        name, args = p.get("name",""), p.get("arguments",{}) or {}
        h = HANDLERS.get(name)
        if not h: _error(rid, -32601, f"Strumento '{name}' non trovato."); return
        try: text = h(args)
        except Exception as e: text = f"[Errore] {e}"
        _result(rid, {"content":[{"type":"text","text":text}],"isError":text.startswith("[Errore")})
    elif rid is not None: _result(rid, {})

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

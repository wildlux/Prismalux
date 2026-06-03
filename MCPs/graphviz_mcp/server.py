#!/usr/bin/env python3
"""
Graphviz MCP — Prismalux
Genera diagrammi, mappe concettuali, grafi via graphviz Python library.
Prerequisiti: pip install graphviz && sudo apt install graphviz
"""
import sys, json, os, tempfile, base64
import logging
from pathlib import Path

logging.basicConfig(
    level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL", "WARNING")),
    format="%(asctime)s [%(name)s] %(levelname)s %(message)s",
)
logger = logging.getLogger(__name__)

OUT_DIR = Path.home() / ".prismalux" / "graphviz_output"
OUT_DIR.mkdir(parents=True, exist_ok=True)

def _safe_fname(filename: str) -> str:
    """Restituisce il basename sanitizzato, bloccando path traversal."""
    if not isinstance(filename, str) or not filename.strip():
        raise ValueError("Nome file non valido.")
    safe = os.path.basename(filename.replace(" ", "_"))
    if not safe:
        raise ValueError("Nome file non valido dopo sanitizzazione.")
    # Verifica che il percorso risultante stia dentro OUT_DIR
    candidate = os.path.realpath(str(OUT_DIR / safe))
    if not candidate.startswith(str(OUT_DIR.resolve())):
        raise ValueError(f"Path traversal rilevato: {filename}")
    return safe

def _gv():
    try:
        import graphviz
        return graphviz
    except ImportError:
        return None

TOOLS = [
    {"name": "render_dot",
     "description": "Renderizza codice DOT in un file PNG/SVG e restituisce il percorso.",
     "inputSchema": {"type": "object", "required": ["dot_code", "filename"],
        "properties": {
            "dot_code": {"type": "string", "description": "Codice DOT Graphviz valido"},
            "filename": {"type": "string", "description": "Nome file output (senza estensione)"},
            "format":   {"type": "string", "enum": ["png","svg","pdf"], "default": "png"},
            "engine":   {"type": "string", "enum": ["dot","neato","circo","fdp","sfdp","twopi"], "default": "dot"}}}},
    {"name": "create_mindmap",
     "description": "Crea una mappa concettuale da un dizionario nodo→figli.",
     "inputSchema": {"type": "object", "required": ["title", "nodes", "filename"],
        "properties": {
            "title":    {"type": "string"},
            "nodes":    {"type": "object", "description": "{'Nodo': ['figlio1','figlio2'], ...}"},
            "filename": {"type": "string"},
            "format":   {"type": "string", "enum": ["png","svg"], "default": "png"}}}},
    {"name": "create_flowchart",
     "description": "Crea un diagramma di flusso da lista di passi con frecce.",
     "inputSchema": {"type": "object", "required": ["steps", "filename"],
        "properties": {
            "steps":    {"type": "array", "items": {"type": "string"}, "description": "Lista di passi in ordine"},
            "edges":    {"type": "array", "items": {"type": "array"}, "description": "Archi extra: [['A','B','label'], ...]"},
            "filename": {"type": "string"},
            "format":   {"type": "string", "enum": ["png","svg"], "default": "png"}}}},
    {"name": "list_outputs",
     "description": "Elenca i file generati da Graphviz MCP.",
     "inputSchema": {"type": "object", "properties": {}}},
]

def tool_render_dot(args):
    gv = _gv()
    if not gv: return "[Errore] graphviz non installato. Esegui: pip install graphviz && sudo apt install graphviz"
    fmt = args.get("format", "png")
    if fmt not in ("png", "svg", "pdf"):
        return "[Errore] Formato non valido. Usa: png, svg, pdf"
    engine = args.get("engine", "dot")
    if engine not in ("dot", "neato", "circo", "fdp", "sfdp", "twopi"):
        return "[Errore] Engine non valido."
    try:
        fname = _safe_fname(args["filename"])
    except ValueError as e:
        return f"[Errore] {e}"
    src = gv.Source(args["dot_code"], filename=str(OUT_DIR / fname), format=fmt, engine=engine)
    out = src.render(cleanup=True)
    return f"Diagramma salvato: {out}"

def tool_create_mindmap(args):
    gv = _gv()
    if not gv: return "[Errore] graphviz non installato."
    fmt = args.get("format", "png")
    if fmt not in ("png", "svg"):
        return "[Errore] Formato non valido. Usa: png, svg"
    try:
        fname = _safe_fname(args["filename"])
    except ValueError as e:
        return f"[Errore] {e}"
    g = gv.Digraph(comment=args["title"], engine="dot")
    g.attr(rankdir="LR", bgcolor="transparent")
    g.attr("node", shape="box", style="rounded,filled", fillcolor="#2a4a6a", fontcolor="white", fontname="Helvetica")
    g.node("__root__", args["title"], shape="ellipse", fillcolor="#1a6a3a")
    for parent, children in args["nodes"].items():
        g.node(parent, parent)
        g.edge("__root__", parent)
        for child in children:
            g.node(child, child, shape="plaintext", fillcolor="transparent")
            g.edge(parent, child)
    g.format = fmt
    g.filename = str(OUT_DIR / fname)
    out = g.render(cleanup=True)
    return f"Mappa concettuale salvata: {out}"

def tool_create_flowchart(args):
    gv = _gv()
    if not gv: return "[Errore] graphviz non installato."
    fmt = args.get("format", "png")
    if fmt not in ("png", "svg"):
        return "[Errore] Formato non valido. Usa: png, svg"
    try:
        fname = _safe_fname(args["filename"])
    except ValueError as e:
        return f"[Errore] {e}"
    g = gv.Digraph(engine="dot")
    g.attr(rankdir="TB")
    g.attr("node", shape="box", style="rounded,filled", fillcolor="#2a3a5a", fontcolor="white")
    steps = args["steps"]
    for i, s in enumerate(steps):
        g.node(str(i), s)
    for i in range(len(steps) - 1):
        g.edge(str(i), str(i+1))
    for edge in args.get("edges", []):
        if len(edge) >= 2:
            label = edge[2] if len(edge) > 2 else ""
            src_n = str(steps.index(edge[0])) if edge[0] in steps else edge[0]
            dst_n = str(steps.index(edge[1])) if edge[1] in steps else edge[1]
            g.edge(src_n, dst_n, label=label)
    g.format = fmt
    g.filename = str(OUT_DIR / fname)
    out = g.render(cleanup=True)
    return f"Flowchart salvato: {out}"

def tool_list_outputs(_):
    files = sorted(OUT_DIR.glob("*"))
    if not files: return f"Nessun file in {OUT_DIR}"
    return f"File in {OUT_DIR}:\n" + "\n".join(f"  • {f.name}" for f in files)

HANDLERS = {"render_dot": tool_render_dot, "create_mindmap": tool_create_mindmap,
            "create_flowchart": tool_create_flowchart, "list_outputs": tool_list_outputs}

def _send(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid, c, m): _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid, r):   _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req):
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"graphviz-mcp","version":"1.0.0"}})
    elif m == "tools/list": _result(rid, {"tools": TOOLS})
    elif m == "tools/call":
        name, args = p.get("name",""), p.get("arguments",{}) or {}
        h = HANDLERS.get(name)
        if not h: _error(rid, -32601, f"Strumento '{name}' non trovato."); return
        try: text = h(args)
        except Exception as e:
            logger.error("Errore tool '%s': %s", name, e)
            text = f"[Errore] {e}"
        _result(rid, {"content":[{"type":"text","text":text}],"isError":text.startswith("[Errore")})
    elif rid is not None: _result(rid, {})

def main():
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: req = json.loads(line)
        except json.JSONDecodeError as e:
            logger.error("JSON parse error: %s", e)
            _send({"jsonrpc":"2.0","id":None,"error":{"code":-32700,"message":str(e)}})
            continue
        try: handle(req)
        except Exception as e:
            logger.error("Errore gestione richiesta: %s", e)
            if req.get("id"): _error(req["id"], -32603, str(e))

if __name__ == "__main__": main()

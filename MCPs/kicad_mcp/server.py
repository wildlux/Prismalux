#!/usr/bin/env python3
"""
KiCAD MCP — Prismalux
Genera schemi elettrici e PCB via KiCAD Python API bridge (porta 3000).
Prerequisiti: copiare kicad_bridge.py nella cartella scripting KiCAD e avviarlo.
"""
import sys, json
import urllib.request, urllib.error

KICAD_BASE = "http://localhost:3000"

def _kc(endpoint, data=None):
    url = KICAD_BASE + endpoint
    body = json.dumps(data).encode() if data is not None else None
    req = urllib.request.Request(url, data=body, method="POST" if data is not None else "GET",
        headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            return json.loads(r.read()), None
    except urllib.error.HTTPError as e:
        return None, f"HTTP {e.code}: {e.read().decode()[:300]}"
    except Exception as e:
        return None, f"{e}\nAvvia kicad_bridge.py dalla KiCAD Python Scripting Console (porta 3000)."

TOOLS = [
    {"name": "execute_python",
     "description": "Esegue codice Python nella KiCAD Scripting Console via bridge.",
     "inputSchema": {"type": "object", "required": ["code"],
        "properties": {"code": {"type": "string"}}}},
    {"name": "get_board_info",
     "description": "Restituisce informazioni sul PCB aperto (dimensioni, layer, componenti).",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "add_footprint",
     "description": "Aggiunge un footprint al PCB nella posizione specificata.",
     "inputSchema": {"type": "object", "required": ["library", "footprint"],
        "properties": {
            "library":   {"type": "string", "description": "Libreria KiCAD (es. 'Resistor_SMD')"},
            "footprint": {"type": "string", "description": "Nome footprint (es. 'R_0402')"},
            "reference": {"type": "string", "default": "R1"},
            "x":         {"type": "number", "default": 100.0},
            "y":         {"type": "number", "default": 100.0}}}},
    {"name": "list_components",
     "description": "Elenca tutti i componenti/footprint nel PCB aperto.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "export_gerber",
     "description": "Esporta i file Gerber del PCB.",
     "inputSchema": {"type": "object", "required": ["output_dir"],
        "properties": {"output_dir": {"type": "string"}}}},
    {"name": "run_drc",
     "description": "Esegue il Design Rule Check (DRC) sul PCB.",
     "inputSchema": {"type": "object", "properties": {}}},
]

def _exec(code):
    res, err = _kc("/exec", {"code": code})
    if err: return f"[Errore KiCAD bridge] {err}"
    return res.get("output", res.get("result", "OK"))

def tool_execute_python(args): return _exec(args["code"])

def tool_get_board_info(_):
    code = ("import pcbnew\nb = pcbnew.GetBoard()\n"
            "if not b: print('[Errore] Nessun PCB aperto.')\n"
            "else:\n"
            "  bb = b.GetBoardEdgesBoundingBox()\n"
            "  print(f'PCB: {b.GetFileName()}')\n"
            "  print(f'  Dim: {pcbnew.ToMM(bb.GetWidth()):.1f}x{pcbnew.ToMM(bb.GetHeight()):.1f} mm')\n"
            "  print(f'  Footprint: {len(b.GetFootprints())}')\n"
            "  print(f'  Tracks: {len(b.GetTracks())}')")
    return _exec(code)

def tool_add_footprint(args):
    code = (f"import pcbnew\nb = pcbnew.GetBoard()\n"
            f"fp = pcbnew.FootprintLoad('{args['library']}', '{args['footprint']}')\n"
            f"fp.SetReference('{args.get('reference','R1')}')\n"
            f"fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM({args.get('x',100.0)}), pcbnew.FromMM({args.get('y',100.0)})))\n"
            f"b.Add(fp); pcbnew.Refresh()\n"
            f"print('Footprint aggiunto: {args[\"library\"]}:{args[\"footprint\"]} @ ({args.get(\"x\",100)},{args.get(\"y\",100)}) mm')")
    return _exec(code)

def tool_list_components(_):
    code = ("import pcbnew\nb = pcbnew.GetBoard()\n"
            "for fp in b.GetFootprints():\n"
            "  pos = fp.GetPosition()\n"
            "  print(f'  {fp.GetReference():8s} {fp.GetValue():15s} @ ({pcbnew.ToMM(pos.x):.1f},{pcbnew.ToMM(pos.y):.1f}) mm')")
    return _exec(code)

def tool_export_gerber(args):
    code = (f"import pcbnew\nb = pcbnew.GetBoard()\n"
            f"p = pcbnew.PLOT_CONTROLLER(b)\n"
            f"po = p.GetPlotOptions()\n"
            f"po.SetOutputDirectory('{args['output_dir']}')\n"
            f"po.SetFormat(pcbnew.PLOT_FORMAT_GERBER)\n"
            f"for layer in [pcbnew.F_Cu, pcbnew.B_Cu, pcbnew.F_SilkS, pcbnew.F_Mask, pcbnew.B_Mask, pcbnew.Edge_Cuts]:\n"
            f"  p.OpenPlotfile('', pcbnew.PLOT_FORMAT_GERBER, '')\n"
            f"  p.SetColorMode(False); p.PlotLayer(); p.ClosePlot()\n"
            f"print('Gerber esportati in: {args[\"output_dir\"]}')")
    return _exec(code)

def tool_run_drc(_):
    code = ("import pcbnew\nb = pcbnew.GetBoard()\n"
            "drc = pcbnew.DRC_ENGINE()\ndrc.InitEngine(b)\n"
            "violations = drc.GetViolations()\n"
            "if not violations: print('✅ DRC OK — nessuna violazione.')\n"
            "else:\n"
            "  print(f'❌ DRC: {len(violations)} violazioni:')\n"
            "  for v in violations[:10]: print(f'  • {v.GetErrorText()}')")
    return _exec(code)

HANDLERS = {"execute_python": tool_execute_python, "get_board_info": tool_get_board_info,
            "add_footprint": tool_add_footprint, "list_components": tool_list_components,
            "export_gerber": tool_export_gerber, "run_drc": tool_run_drc}

def _send(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid, c, m): _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid, r):   _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req):
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"kicad-mcp","version":"1.0.0"}})
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

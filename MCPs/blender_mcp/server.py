"""Blender MCP — MCP stdio proxy verso prismalux_bridge su localhost:6789.
Richiede che Blender sia avviato con il addon prismalux_bridge abilitato.
"""
import sys, json, urllib.request, urllib.error

BRIDGE_URL = "http://localhost:6789/execute"

TOOLS = [
    {
        "name": "execute_blender_code",
        "description": "Esegue codice Python nel contesto di Blender (bpy). "
                       "Richiede che Blender sia aperto con il Prismalux Bridge addon attivo.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "code": {
                    "type": "string",
                    "description": "Codice Python da eseguire in Blender (accesso a bpy, mathutils, ecc.)"
                }
            },
            "required": ["code"]
        }
    }
]

def _call_bridge(code: str) -> str:
    payload = json.dumps({"code": code}).encode()
    req = urllib.request.Request(
        BRIDGE_URL,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST"
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            body = json.loads(resp.read().decode())
            return body.get("result", "") or body.get("output", "") or str(body)
    except urllib.error.URLError as e:
        return (
            f"ERRORE: impossibile contattare il Prismalux Bridge su {BRIDGE_URL}. "
            f"Avvia Blender e abilita il Prismalux Bridge addon. Dettaglio: {e}"
        )
    except Exception as e:
        return f"ERRORE: {e}"

def _send(obj: dict) -> None:
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()

def _handle(req: dict) -> None:
    method = req.get("method", "")
    rid    = req.get("id")

    if method == "initialize":
        _send({"jsonrpc": "2.0", "id": rid, "result": {
            "protocolVersion": "2024-11-05",
            "serverInfo": {"name": "blender-mcp", "version": "1.0.0"},
            "capabilities": {"tools": {}}
        }})

    elif method == "notifications/initialized":
        pass  # no response for notifications

    elif method == "tools/list":
        _send({"jsonrpc": "2.0", "id": rid, "result": {"tools": TOOLS}})

    elif method == "tools/call":
        params    = req.get("params", {})
        tool_name = params.get("name", "")
        args      = params.get("arguments", {})
        if tool_name == "execute_blender_code":
            code   = args.get("code", "")
            result = _call_bridge(code)
            _send({"jsonrpc": "2.0", "id": rid, "result": {
                "content": [{"type": "text", "text": result}]
            }})
        else:
            _send({"jsonrpc": "2.0", "id": rid,
                   "error": {"code": -32601, "message": f"Tool '{tool_name}' sconosciuto"}})

    elif rid is not None:
        _send({"jsonrpc": "2.0", "id": rid,
               "error": {"code": -32601, "message": f"Method '{method}' not found"}})

def main() -> None:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError:
            continue
        _handle(req)

if __name__ == "__main__":
    main()

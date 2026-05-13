"""
KiCAD Bridge — Prismalux
Server HTTP sulla porta 3000 da avviare nella KiCAD Python Scripting Console.

Uso:
  KiCAD → Tools → Scripting Console → incolla:
    import sys; sys.path.insert(0, '/home/wildlux/Desktop/Prismalux/MCPs/kicad_mcp')
    import kicad_bridge; kicad_bridge.start()
"""
import sys, json, threading
from http.server import HTTPServer, BaseHTTPRequestHandler
from io import StringIO

PORT = 3000
_server = None

class KiCADHandler(BaseHTTPRequestHandler):
    def log_message(self, *args): pass

    def do_GET(self):
        if self.path == "/ping":
            self._respond(200, {"status": "ok", "app": "KiCAD"})
        else:
            self._respond(404, {"error": "not found"})

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(length)) if length else {}
        if self.path == "/exec":
            code = body.get("code", "")
            buf = StringIO()
            old = sys.stdout
            sys.stdout = buf
            try:
                import pcbnew
                exec(compile(code, "<prismalux>", "exec"),
                     {"__name__": "__prismalux__", "pcbnew": pcbnew})
                out = buf.getvalue()
                self._respond(200, {"output": out, "ok": True})
            except Exception as e:
                out = buf.getvalue()
                self._respond(200, {"output": out, "error": str(e), "ok": False})
            finally:
                sys.stdout = old
        else:
            self._respond(404, {"error": "endpoint sconosciuto"})

    def _respond(self, code, data):
        body = json.dumps(data).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)

def start(port=PORT):
    global _server
    if _server:
        print(f"[PrismaluxKiCAD] Già in ascolto su porta {port}.")
        return
    _server = HTTPServer(("localhost", port), KiCADHandler)
    t = threading.Thread(target=_server.serve_forever, daemon=True)
    t.start()
    print(f"[PrismaluxKiCAD] Server avviato su http://localhost:{port}")

def stop():
    global _server
    if _server:
        _server.shutdown()
        _server = None
        print("[PrismaluxKiCAD] Server fermato.")

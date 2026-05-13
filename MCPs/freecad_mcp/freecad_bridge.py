"""
FreeCAD Bridge — Prismalux
Addon FreeCAD: avvia un server HTTP sulla porta 9876 che accetta
codice Python ed eseguito nel contesto di FreeCAD.

Installazione:
  mkdir -p ~/.FreeCAD/Mod/PrismaluxBridge
  cp freecad_bridge.py ~/.FreeCAD/Mod/PrismaluxBridge/freecad_bridge.py
  # Riavvia FreeCAD → il server parte automaticamente su porta 9876

Uso manuale da FreeCAD Python console:
  import freecad_bridge; freecad_bridge.start()
"""
import sys
import json
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler
from io import StringIO

PORT = 9876
_server = None

class PrismaluxHandler(BaseHTTPRequestHandler):
    def log_message(self, *args): pass  # silenzioso

    def do_GET(self):
        if self.path == "/ping":
            self._respond(200, {"status": "ok", "app": "FreeCAD"})
        elif self.path == "/objects":
            try:
                import FreeCAD
                doc = FreeCAD.activeDocument()
                objs = []
                if doc:
                    for o in doc.Objects:
                        objs.append({"name": o.Name, "label": o.Label, "type": o.TypeId})
                self._respond(200, {"objects": objs, "document": doc.Name if doc else None})
            except Exception as e:
                self._respond(500, {"error": str(e)})
        else:
            self._respond(404, {"error": "Not found"})

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(length)) if length else {}
        if self.path == "/exec":
            code = body.get("code", "")
            stdout_buf = StringIO()
            stderr_buf = StringIO()
            old_stdout, old_stderr = sys.stdout, sys.stderr
            sys.stdout, sys.stderr = stdout_buf, stderr_buf
            try:
                exec(compile(code, "<prismalux>", "exec"), {"__name__": "__prismalux__"})
                output = stdout_buf.getvalue()
                error  = stderr_buf.getvalue()
                self._respond(200, {"output": output, "stderr": error, "ok": True})
            except Exception as e:
                output = stdout_buf.getvalue()
                self._respond(200, {"output": output, "error": str(e), "ok": False})
            finally:
                sys.stdout, sys.stderr = old_stdout, old_stderr
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
        print(f"[PrismaluxBridge] Già in ascolto su porta {port}.")
        return
    _server = HTTPServer(("localhost", port), PrismaluxHandler)
    t = threading.Thread(target=_server.serve_forever, daemon=True)
    t.start()
    print(f"[PrismaluxBridge] Server avviato su http://localhost:{port}")

def stop():
    global _server
    if _server:
        _server.shutdown()
        _server = None
        print("[PrismaluxBridge] Server fermato.")

# Auto-start quando caricato come addon FreeCAD
try:
    import FreeCAD
    start()
except ImportError:
    pass  # non siamo in FreeCAD

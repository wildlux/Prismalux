# prismalux_bridge.py — Blender Addon per Prismalux
# ──────────────────────────────────────────────────────────────
# Installa in Blender:
#   Edit > Preferences > Add-ons > Install > seleziona questo file > Enable
#
# Avvia automaticamente un HTTP server su localhost:6789.
# Prismalux manda codice Python via POST /execute → Blender lo esegue
# nel thread principale e restituisce il risultato.
# ──────────────────────────────────────────────────────────────

bl_info = {
    "name":        "Prismalux Bridge",
    "author":      "Prismalux",
    "version":     (1, 1),
    "blender":     (3, 0, 0),
    "location":    "Barra laterale (N) > Prismalux",
    "description": "Bridge HTTP locale per eseguire script Python generati da Prismalux AI",
    "category":    "System",
}

import bpy
import threading
import queue
import json
import io
import sys
import traceback
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = 6789

# Code da eseguire (main thread) / risultati (thread server)
_cmd_queue    = queue.Queue()
_result_queue = queue.Queue()


# ── HTTP handler ─────────────────────────────────────────────

class _Handler(BaseHTTPRequestHandler):
    """Gestisce le richieste HTTP dal pannello Blender di Prismalux."""

    def log_message(self, fmt, *args):
        pass  # silenzia log nel terminale Blender

    def _send_json(self, code: int, obj: dict):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type",   "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/status":
            self._send_json(200, {
                "ok":      True,
                "blender": bpy.app.version_string,
                "file":    bpy.data.filepath or "(untitled)",
                "objects": len(bpy.data.objects),
            })
        elif self.path == "/scene":
            # Info scena rapida: oggetti + materiali
            objs = []
            for o in bpy.data.objects:
                objs.append({
                    "name":     o.name,
                    "type":     o.type,
                    "location": list(o.location),
                    "rotation": list(o.rotation_euler),
                    "scale":    list(o.scale),
                    "materials": [m.name for m in o.data.materials]
                              if hasattr(o.data, "materials") else [],
                })
            self._send_json(200, {"ok": True, "objects": objs})
        else:
            self._send_json(404, {"ok": False, "error": "endpoint non trovato"})

    def do_POST(self):
        if self.path == "/execute":
            length = int(self.headers.get("Content-Length", 0))
            body   = self.rfile.read(length)
            try:
                data = json.loads(body)
                code = data.get("code", "").strip()
            except Exception as e:
                self._send_json(400, {"ok": False, "error": f"JSON non valido: {e}"})
                return

            if not code:
                self._send_json(400, {"ok": False, "error": "campo 'code' vuoto"})
                return

            # Invia al main thread e aspetta risposta (max 15s)
            _cmd_queue.put(code)
            try:
                result = _result_queue.get(timeout=15)
                self._send_json(200, result)
            except queue.Empty:
                self._send_json(408, {
                    "ok":    False,
                    "error": "Timeout: Blender impiegato troppo. Riprova.",
                })
        else:
            self._send_json(404, {"ok": False, "error": "endpoint non trovato"})


# ── Server thread ────────────────────────────────────────────

_server        = None
_server_thread = None


def _run_server():
    global _server
    try:
        _server = HTTPServer(("localhost", PORT), _Handler)
        print(f"[Prismalux Bridge] server avviato → http://localhost:{PORT}")
        _server.serve_forever()
    except OSError as e:
        print(f"[Prismalux Bridge] ERRORE avvio server: {e}")


# ── Main-thread timer ────────────────────────────────────────

def _process_queue():
    """
    Eseguito nel main thread di Blender ogni 100ms via bpy.app.timers.
    Svuota la coda dei comandi e restituisce il risultato nel thread server.
    """
    while not _cmd_queue.empty():
        code = _cmd_queue.get_nowait()
        buf = io.StringIO()
        try:
            old_out   = sys.stdout
            sys.stdout = buf
            # Esegue in namespace con bpy, mathutils, Vector accessibili
            import mathutils
            ns = {
                "bpy":       bpy,
                "mathutils": mathutils,
                "Vector":    mathutils.Vector,
                "Euler":     mathutils.Euler,
                "Matrix":    mathutils.Matrix,
            }
            exec(compile(code, "<prismalux>", "exec"), ns)
            sys.stdout = old_out
            output     = buf.getvalue().strip()
            _result_queue.put({"ok": True,  "output": output or "Eseguito con successo."})
        except Exception:
            sys.stdout = old_out
            err = traceback.format_exc()
            _result_queue.put({"ok": False, "error": err.strip()})
    return 0.1   # richiamare dopo 100ms


# ── Pannello UI Blender ──────────────────────────────────────

class PRISMALUX_PT_Panel(bpy.types.Panel):
    bl_label       = "Prismalux Bridge"
    bl_idname      = "PRISMALUX_PT_panel"
    bl_space_type  = "VIEW_3D"
    bl_region_type = "UI"
    bl_category    = "Prismalux"

    def draw(self, context):
        layout = self.layout
        col    = layout.column(align=True)

        if _server and _server_thread and _server_thread.is_alive():
            col.label(text=f"Porta: {PORT}", icon="CHECKMARK")
            col.label(text="Blender pronto per Prismalux", icon="LINKED")
        else:
            col.label(text="Server non attivo", icon="ERROR")
            col.operator("prismalux.start_server", text="Avvia server")

        col.separator()
        col.label(text=f"Oggetti scena: {len(bpy.data.objects)}")
        active = context.active_object
        if active:
            col.label(text=f"Attivo: {active.name}")


class PRISMALUX_OT_StartServer(bpy.types.Operator):
    bl_idname  = "prismalux.start_server"
    bl_label   = "Avvia Prismalux Bridge"

    def execute(self, context):
        _start_server()
        return {"FINISHED"}


def _start_server():
    global _server_thread
    if _server_thread and _server_thread.is_alive():
        return
    _server_thread = threading.Thread(target=_run_server, daemon=True)
    _server_thread.start()


# ── Register / Unregister ────────────────────────────────────

_classes = [PRISMALUX_PT_Panel, PRISMALUX_OT_StartServer]


def register():
    for cls in _classes:
        bpy.utils.register_class(cls)
    _start_server()
    if not bpy.app.timers.is_registered(_process_queue):
        bpy.app.timers.register(_process_queue, first_interval=0.1, persistent=True)


def unregister():
    for cls in reversed(_classes):
        bpy.utils.unregister_class(cls)
    if bpy.app.timers.is_registered(_process_queue):
        bpy.app.timers.unregister(_process_queue)
    global _server
    if _server:
        threading.Thread(target=_server.shutdown, daemon=True).start()
        _server = None
    print("[Prismalux Bridge] fermato.")


if __name__ == "__main__":
    register()

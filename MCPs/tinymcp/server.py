#!/usr/bin/env python3
"""
TinyMCP — Prismalux (Arduino / Microcontroller Bridge)
Compila e carica sketch Arduino/ESP32/Pico, serial monitor via arduino-cli e pyserial.
Prerequisiti: sudo apt install arduino-cli  |  pip install pyserial
"""
import sys, json, subprocess, shutil, threading, time
from pathlib import Path

SKETCH_DIR = Path.home() / ".prismalux" / "arduino_sketches"
SKETCH_DIR.mkdir(parents=True, exist_ok=True)

def _run(cmd, timeout=60, cwd=None):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, cwd=str(cwd) if cwd else None)
        return r.stdout.strip(), r.stderr.strip(), r.returncode
    except subprocess.TimeoutExpired:
        return "", f"Timeout ({timeout}s)", 1
    except Exception as e:
        return "", str(e), 1

TOOLS = [
    {"name": "list_ports",
     "description": "Elenca le porte seriali disponibili e i dispositivi collegati.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "list_boards",
     "description": "Elenca le board Arduino supportate da arduino-cli.",
     "inputSchema": {"type": "object", "properties": {
        "filter": {"type": "string", "description": "Filtra per nome (es. 'uno', 'esp32', 'nano')"}}}},
    {"name": "create_sketch",
     "description": "Crea un nuovo sketch Arduino (.ino) con il codice specificato.",
     "inputSchema": {"type": "object", "required": ["name", "code"],
        "properties": {
            "name": {"type": "string", "description": "Nome sketch (senza spazi)"},
            "code": {"type": "string", "description": "Codice .ino completo con setup() e loop()"}}}},
    {"name": "compile_sketch",
     "description": "Compila uno sketch Arduino per la board specificata.",
     "inputSchema": {"type": "object", "required": ["sketch_name", "fqbn"],
        "properties": {
            "sketch_name": {"type": "string", "description": "Nome sketch creato con create_sketch"},
            "fqbn":        {"type": "string", "description": "FQBN della board (es. 'arduino:avr:uno', 'esp32:esp32:esp32')"}}}},
    {"name": "upload_sketch",
     "description": "Compila e carica uno sketch su una board collegata via USB.",
     "inputSchema": {"type": "object", "required": ["sketch_name", "fqbn", "port"],
        "properties": {
            "sketch_name": {"type": "string"},
            "fqbn":        {"type": "string"},
            "port":        {"type": "string", "description": "Porta seriale (es. '/dev/ttyUSB0', '/dev/ttyACM0', 'COM3')"}}}},
    {"name": "serial_monitor",
     "description": "Legge dati dalla porta seriale per N secondi.",
     "inputSchema": {"type": "object", "required": ["port"],
        "properties": {
            "port":     {"type": "string"},
            "baudrate": {"type": "integer", "default": 9600},
            "duration": {"type": "integer", "default": 5, "description": "Secondi di lettura (max 30)"}}}},
    {"name": "install_board",
     "description": "Installa il supporto per una piattaforma (es. esp32, rp2040).",
     "inputSchema": {"type": "object", "required": ["platform"],
        "properties": {"platform": {"type": "string", "description": "Es. 'arduino:avr', 'esp32:esp32', 'rp2040:rp2040'"}}}},
]

def tool_list_ports(_):
    if not shutil.which("arduino-cli"):
        try:
            import serial.tools.list_ports
            ports = list(serial.tools.list_ports.comports())
            if not ports: return "Nessuna porta seriale trovata."
            lines = ["Porte seriali:"]
            for p in ports:
                lines.append(f"  • {p.device} — {p.description}")
            return "\n".join(lines)
        except ImportError:
            return "[Errore] arduino-cli e pyserial non trovati.\nInstalla: sudo apt install arduino-cli && pip install pyserial"
    stdout, stderr, rc = _run(["arduino-cli", "board", "list"])
    if rc != 0: return f"[Errore] {stderr}"
    return stdout if stdout else "Nessuna board rilevata."

def tool_list_boards(args):
    if not shutil.which("arduino-cli"):
        return "[Errore] arduino-cli non installato. Esegui: sudo apt install arduino-cli"
    stdout, stderr, rc = _run(["arduino-cli", "board", "listall", args.get("filter", "")], timeout=30)
    if rc != 0: return f"[Errore] {stderr}"
    lines = stdout.splitlines()
    if len(lines) > 30: lines = lines[:30] + [f"... ({len(stdout.splitlines())} totali)"]
    return "\n".join(lines)

def tool_create_sketch(args):
    name = args["name"].replace(" ", "_")
    sdir = SKETCH_DIR / name
    sdir.mkdir(parents=True, exist_ok=True)
    ino = sdir / (name + ".ino")
    ino.write_text(args["code"], encoding="utf-8")
    return f"Sketch creato: {ino}"

def tool_compile_sketch(args):
    if not shutil.which("arduino-cli"):
        return "[Errore] arduino-cli non installato."
    name = args["sketch_name"].replace(" ", "_")
    sdir = SKETCH_DIR / name
    if not sdir.exists(): return f"[Errore] Sketch '{name}' non trovato. Crea prima con create_sketch."
    stdout, stderr, rc = _run(["arduino-cli", "compile", "--fqbn", args["fqbn"], str(sdir)], timeout=120)
    if rc != 0: return f"[Errore compilazione]\n{stderr[-1500:]}"
    return f"✅ Compilazione OK ({args['fqbn']})\n{stdout[-500:]}"

def tool_upload_sketch(args):
    if not shutil.which("arduino-cli"):
        return "[Errore] arduino-cli non installato."
    name = args["sketch_name"].replace(" ", "_")
    sdir = SKETCH_DIR / name
    if not sdir.exists(): return f"[Errore] Sketch '{name}' non trovato."
    stdout, stderr, rc = _run(
        ["arduino-cli", "compile", "--upload", "--fqbn", args["fqbn"], "--port", args["port"], str(sdir)],
        timeout=180)
    if rc != 0: return f"[Errore upload]\n{stderr[-1500:]}"
    return f"✅ Upload completato su {args['port']}\n{stdout[-500:]}"

def tool_serial_monitor(args):
    try:
        import serial
    except ImportError:
        return "[Errore] pyserial non installato. Esegui: pip install pyserial"
    duration = min(args.get("duration", 5), 30)
    baud = args.get("baudrate", 9600)
    lines = []
    try:
        ser = serial.Serial(args["port"], baud, timeout=1)
        time.sleep(2)
        deadline = time.time() + duration
        while time.time() < deadline:
            if ser.in_waiting:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if line: lines.append(line)
        ser.close()
    except Exception as e:
        return f"[Errore seriale] {e}"
    if not lines: return f"Nessun dato ricevuto da {args['port']} in {duration}s."
    return f"Serial monitor {args['port']} ({baud} baud, {duration}s):\n" + "\n".join(lines[-50:])

def tool_install_board(args):
    if not shutil.which("arduino-cli"):
        return "[Errore] arduino-cli non installato."
    stdout, stderr, rc = _run(["arduino-cli", "core", "install", args["platform"]], timeout=120)
    if rc != 0: return f"[Errore] {stderr}"
    return f"✅ Piattaforma '{args['platform']}' installata.\n{stdout[-500:]}"

HANDLERS = {"list_ports": tool_list_ports, "list_boards": tool_list_boards,
            "create_sketch": tool_create_sketch, "compile_sketch": tool_compile_sketch,
            "upload_sketch": tool_upload_sketch, "serial_monitor": tool_serial_monitor,
            "install_board": tool_install_board}

def _send(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid, c, m): _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid, r):   _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req):
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"tinymcp","version":"1.0.0"}})
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

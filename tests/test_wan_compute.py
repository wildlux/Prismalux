#!/usr/bin/env python3
"""
test_wan_compute.py — Simulatore + test suite WAN Compute (porta 11600)

Modalità:
  python3 test_wan_compute.py sim      # server + client in-process (no GUI)
  python3 test_wan_compute.py client   # client reale → si connette all'app
  python3 test_wan_compute.py client <host> <port>
"""

import sys, socket, json, threading, time, subprocess, platform, re

PORT    = 11600
SIM_PORT = 11601   # porta per simulazione locale (evita conflitto con app)
HOST    = "127.0.0.1"
TIMEOUT = 30   # secondi per task AI (possono essere lenti)

# ══════════════════════════════════════════════════════════════════
# Helpers
# ══════════════════════════════════════════════════════════════════

def send(sock, obj):
    sock.sendall((json.dumps(obj) + "\n").encode())

def recv_line(sock, timeout=TIMEOUT):
    buf = b""
    sock.settimeout(timeout)
    while b"\n" not in buf:
        chunk = sock.recv(4096)
        if not chunk:
            raise ConnectionError("socket chiuso")
        buf += chunk
    line, _ = buf.split(b"\n", 1)
    return json.loads(line)

# ══════════════════════════════════════════════════════════════════
# Mini-server in-process (solo per modalità "sim")
# ══════════════════════════════════════════════════════════════════

def mini_server(tasks_queue: list, results: list, ready_evt, port=PORT):
    """Server minimalista: assegna task a un solo nodo."""
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((HOST, port))
    srv.listen(1)
    ready_evt.set()
    conn, addr = srv.accept()
    print(f"  [server] connessione da {addr}")

    # hello → welcome
    msg = recv_line(conn, 5)
    assert msg.get("t") == "hello", f"atteso hello, ricevuto {msg}"
    send(conn, {"t": "welcome", "node_id": "node-test-01"})
    print(f"  [server] nodo registrato: {msg.get('name','?')}")

    # invia task uno per uno
    for t in tasks_queue:
        # aspetta poll
        msg = recv_line(conn, 5)
        assert msg.get("t") == "poll", f"atteso poll, ricevuto {msg}"
        send(conn, {"t": "task", "id": t["id"], "kind": t["kind"], "payload": t["payload"]})
        print(f"  [server] → task {t['id']} ({t['kind']})")

        # aspetta result
        msg = recv_line(conn, TIMEOUT)
        assert msg.get("t") == "result", f"atteso result, ricevuto {msg}"
        send(conn, {"t": "ack", "id": t["id"]})
        results.append({"id": t["id"], "kind": t["kind"],
                        "ok": msg.get("ok", True),
                        "result": msg.get("result", "")[:300]})
        print(f"  [server] ✓ result {t['id']} ok={msg.get('ok')}")

    # ultimo poll → idle
    msg = recv_line(conn, 5)
    if msg.get("t") == "poll":
        send(conn, {"t": "idle"})

    conn.close()
    srv.close()

# ══════════════════════════════════════════════════════════════════
# Client che esegue i task localmente (no Ollama — tutto subprocess)
# ══════════════════════════════════════════════════════════════════

def run_task_local(kind: str, payload: str) -> tuple[bool, str]:
    """Esegue un task senza AI — solo task subprocess/sistema."""
    try:
        if kind == "system_info":
            info = {
                "os":      platform.system() + " " + platform.release(),
                "python":  platform.python_version(),
                "machine": platform.machine(),
            }
            return True, json.dumps(info, ensure_ascii=False)

        elif kind == "net_info":
            out = subprocess.check_output(["ip", "-brief", "addr"],
                                          text=True, timeout=5)
            return True, out.strip()

        elif kind == "shell_cmd":
            out = subprocess.check_output(
                payload, shell=True, text=True,
                stderr=subprocess.STDOUT, timeout=10)
            return True, out.strip()

        elif kind == "eval_script":
            out = subprocess.check_output(
                [sys.executable, "-c", payload],
                text=True, stderr=subprocess.STDOUT, timeout=10)
            return True, out.strip()

        elif kind == "python_repl":
            out = subprocess.check_output(
                [sys.executable, "-c", payload],
                text=True, stderr=subprocess.STDOUT, timeout=10)
            return True, out.strip()

        elif kind == "math_expr":
            result = eval(compile(payload, "<wan>", "eval"),
                          {"__builtins__": {}},
                          {"sum": sum, "range": range, "abs": abs,
                           "min": min, "max": max, "round": round})
            return True, str(result)

        elif kind == "file_read":
            with open(payload.strip()) as f:
                return True, f.read(2000)

        elif kind == "git_cmd":
            out = subprocess.check_output(
                payload, shell=True, text=True,
                stderr=subprocess.STDOUT, cwd="/home/wildlux/Desktop/Prismalux",
                timeout=10)
            return True, out.strip()

        else:
            return False, f"task kind '{kind}' richiede Ollama (non disponibile in sim)"

    except Exception as e:
        return False, str(e)

def wan_client(host, port):
    """Client che si connette al server e processa task."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    print(f"  [client] connesso a {host}:{port}")

    send(sock, {"t": "hello", "name": "TestNode-Python",
                "caps": ["system_info","net_info","shell_cmd",
                         "eval_script","python_repl","math_expr",
                         "file_read","git_cmd"]})
    msg = recv_line(sock, 5)
    assert msg.get("t") == "welcome", f"atteso welcome, ricevuto {msg}"
    # server usa "node_id" (non "id")
    node_id = msg.get("node_id") or msg.get("id", "?")
    print(f"  [client] id assegnato: {node_id}")

    while True:
        send(sock, {"t": "poll"})
        msg = recv_line(sock, 10)
        if msg.get("t") == "idle":
            print("  [client] idle — nessun task. Uscita.")
            break
        if msg.get("t") != "task":
            continue

        task_id = msg["id"]
        kind    = msg["kind"]
        payload = msg.get("payload", "")
        print(f"  [client] ← task {task_id} ({kind})")

        ok, result = run_task_local(kind, payload)
        send(sock, {"t": "result", "id": task_id, "ok": ok, "result": result})

        ack = recv_line(sock, 5)
        assert ack.get("t") == "ack"

    sock.close()

# ══════════════════════════════════════════════════════════════════
# Task da testare
# ══════════════════════════════════════════════════════════════════

TEST_TASKS = [
    {"id": "t01", "kind": "system_info",  "payload": ""},
    {"id": "t02", "kind": "net_info",     "payload": ""},
    {"id": "t03", "kind": "shell_cmd",    "payload": "uptime && free -h"},
    {"id": "t04", "kind": "eval_script",
     "payload": "import sys,platform\nprint('Python',sys.version)\nprint('OS:',platform.system(),platform.release())"},
    {"id": "t05", "kind": "python_repl",
     "payload": "import math\nprint(f'π = {math.pi:.10f}')\nprint(f'e = {math.e:.10f}')"},
    {"id": "t06", "kind": "math_expr",    "payload": "2**10 + sum(range(1, 101))"},
    {"id": "t07", "kind": "file_read",    "payload": "/etc/os-release"},
    {"id": "t08", "kind": "git_cmd",      "payload": "git log --oneline -5"},
]

# ══════════════════════════════════════════════════════════════════
# Modalità sim: server + client in thread separati
# ══════════════════════════════════════════════════════════════════

def mode_sim():
    print("=" * 60)
    print(f"  WAN Compute — Simulazione locale (porta {SIM_PORT})")
    print("=" * 60)

    results = []
    ready   = threading.Event()

    srv_thread = threading.Thread(
        target=mini_server,
        args=(TEST_TASKS, results, ready, SIM_PORT),
        daemon=True)
    srv_thread.start()
    ready.wait(timeout=3)

    try:
        wan_client(HOST, SIM_PORT)
    except Exception as e:
        print(f"\n  ERRORE client: {e}")

    srv_thread.join(timeout=5)

    # Riepilogo
    print()
    print("=" * 60)
    print(f"  RIEPILOGO — {len(results)}/{len(TEST_TASKS)} task completati")
    print("=" * 60)
    ok_count = 0
    for r in results:
        status = "✅" if r["ok"] else "❌"
        result_preview = r["result"].replace("\n", " ")[:80]
        print(f"  {status} [{r['id']}] {r['kind']}")
        print(f"       → {result_preview}")
        if r["ok"]:
            ok_count += 1
    print()
    print(f"  Risultato: {ok_count}/{len(results)} OK")
    return ok_count == len(results)

# ══════════════════════════════════════════════════════════════════
# Modalità client: si connette all'app Prismalux reale
# ══════════════════════════════════════════════════════════════════

def mode_client(host, port):
    print("=" * 60)
    print(f"  WAN Compute — Client reale → {host}:{port}")
    print("  (avvia prima il server da LAN & WAN → WAN Compute)")
    print("=" * 60)
    try:
        wan_client(host, port)
    except ConnectionRefusedError:
        print(f"\n  ❌ Connessione rifiutata su {host}:{port}")
        print("     Avvia il server dalla GUI: LAN & WAN → WAN Compute → Avvia server")
        sys.exit(1)

# ══════════════════════════════════════════════════════════════════
# Entrypoint
# ══════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "sim"
    if mode == "sim":
        ok = mode_sim()
        sys.exit(0 if ok else 1)
    elif mode == "client":
        h = sys.argv[2] if len(sys.argv) > 2 else HOST
        p = int(sys.argv[3]) if len(sys.argv) > 3 else PORT
        mode_client(h, p)
    else:
        print(f"Uso: {sys.argv[0]} [sim|client [host] [port]]")
        sys.exit(1)

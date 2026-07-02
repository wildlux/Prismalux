#!/usr/bin/env bash
# test_wan.sh — Test protocollo WAN Compute (porta 11600)
#
# Uso:
#   ./test_wan.sh server [porta] [token]
#       Avvia un server WAN minimale in Python per testare i client.
#       Porta default: 11600 | Token default: test1234
#
#   ./test_wan.sh client <ip_server> [porta] [token]
#       Si connette al server (Prismalux o server di test),
#       fa hello, poll, esegue un task echo e invia il risultato.
#       Porta default: 11600 | Token default: test1234
#
# Esempi:
#   HP (server di test):  ./test_wan.sh server 11600 test1234
#   iMac (client):        ./test_wan.sh client 192.168.1.XXX 11600 test1234
#
#   Oppure con Prismalux già avviato come server:
#   iMac (client):        ./test_wan.sh client 192.168.1.YYY 11600 <token_impostato_in_prismalux>

set -e

MODE="${1:-}"
if [[ -z "$MODE" ]]; then
    echo "Uso: $0 server [porta] [token]"
    echo "     $0 client <ip> [porta] [token]"
    exit 1
fi

# ── SERVER ────────────────────────────────────────────────────────────────────
if [[ "$MODE" == "server" ]]; then
    PORT="${2:-11600}"
    TOKEN="${3:-test1234}"

    echo "==> WAN server di test su porta $PORT (token: $TOKEN)"
    echo "    Invia Ctrl+C per fermare."
    echo ""

    python3 - "$PORT" "$TOKEN" <<'PYEOF'
import sys, socket, threading, json, time, uuid, hmac, hashlib

PORT  = int(sys.argv[1])
TOKEN = sys.argv[2]

def wan_hmac(token, data):
    return hmac.new(token.encode(), data.encode(), hashlib.sha256).hexdigest()

def handle_client(conn, addr):
    node_id = str(uuid.uuid4())[:8]
    print(f"[server] Connesso: {addr}")
    buf = b""
    authenticated = False
    task_sent = False
    task_id = str(uuid.uuid4())[:8]

    try:
        while True:
            chunk = conn.recv(4096)
            if not chunk:
                break
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                if not line.strip():
                    continue
                try:
                    msg = json.loads(line)
                except Exception:
                    continue

                t = msg.get("t", "")
                print(f"[server] << {json.dumps(msg)}")

                if t == "hello":
                    presented = msg.get("token", "")
                    if TOKEN and presented != TOKEN:
                        resp = {"t": "error", "msg": "auth_failed"}
                        conn.sendall((json.dumps(resp) + "\n").encode())
                        print("[server] Auth fallita — chiudo connessione")
                        return
                    authenticated = True
                    resp = {"t": "welcome", "node_id": node_id}
                    conn.sendall((json.dumps(resp) + "\n").encode())
                    print(f"[server] >> {json.dumps(resp)}")

                elif t == "poll" and authenticated:
                    if not task_sent:
                        task_sent = True
                        task = {"t": "task", "id": task_id,
                                "kind": "echo",
                                "payload": "Ciao da server di test!"}
                        conn.sendall((json.dumps(task) + "\n").encode())
                        print(f"[server] >> {json.dumps(task)}")
                    else:
                        resp = {"t": "idle"}
                        conn.sendall((json.dumps(resp) + "\n").encode())
                        print(f"[server] >> {json.dumps(resp)}")

                elif t == "ping":
                    resp = {"t": "pong"}
                    conn.sendall((json.dumps(resp) + "\n").encode())

                elif t == "result":
                    rid    = msg.get("id", "")
                    result = msg.get("result", "")
                    hmac_ok = True
                    if TOKEN:
                        expected = wan_hmac(TOKEN, rid + "|" + result)
                        received = msg.get("hmac", "")
                        hmac_ok  = hmac.compare_digest(expected, received)

                    if hmac_ok:
                        resp = {"t": "ack", "id": rid}
                        print(f"[server] ✅ Risultato valido: {result}")
                    else:
                        resp = {"t": "error", "msg": "hmac_invalid"}
                        print(f"[server] ❌ HMAC non valido")

                    conn.sendall((json.dumps(resp) + "\n").encode())
                    print(f"[server] >> {json.dumps(resp)}")
                    print("[server] Test completato con successo!")
    except Exception as e:
        print(f"[server] Errore: {e}")
    finally:
        conn.close()
        print(f"[server] Disconnesso: {addr}")

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", PORT))
srv.listen(8)
print(f"[server] In ascolto su 0.0.0.0:{PORT}")

while True:
    conn, addr = srv.accept()
    threading.Thread(target=handle_client, args=(conn, addr), daemon=True).start()
PYEOF

# ── CLIENT ────────────────────────────────────────────────────────────────────
elif [[ "$MODE" == "client" ]]; then
    SERVER_IP="${2:?Errore: specifica IP server — es. ./test_wan.sh client 192.168.1.100}"
    PORT="${3:-11600}"
    TOKEN="${4:-test1234}"

    echo "==> WAN client: connessione a $SERVER_IP:$PORT (token: $TOKEN)"
    echo ""

    python3 - "$SERVER_IP" "$PORT" "$TOKEN" <<'PYEOF'
import sys, socket, json, time, uuid, hmac, hashlib

SERVER = sys.argv[1]
PORT   = int(sys.argv[2])
TOKEN  = sys.argv[3]

def wan_hmac(token, data):
    return hmac.new(token.encode(), data.encode(), hashlib.sha256).hexdigest()

def send(conn, obj):
    msg = json.dumps(obj) + "\n"
    conn.sendall(msg.encode())
    print(f"[client] >> {json.dumps(obj)}")

def recv_line(conn, buf):
    while b"\n" not in buf:
        chunk = conn.recv(4096)
        if not chunk:
            raise ConnectionError("Connessione chiusa dal server")
        buf += chunk
    line, buf = buf.split(b"\n", 1)
    return line, buf

try:
    conn = socket.create_connection((SERVER, PORT), timeout=10)
    print(f"[client] Connesso a {SERVER}:{PORT}")
    buf = b""

    # 1. Hello
    send(conn, {
        "t":     "hello",
        "name":  "test_wan_client",
        "token": TOKEN,
        "caps":  ["echo", "ai_query", "python"]
    })

    # 2. Welcome
    line, buf = recv_line(conn, buf)
    msg = json.loads(line)
    print(f"[client] << {json.dumps(msg)}")
    if msg.get("t") != "welcome":
        print(f"[client] ❌ Autenticazione fallita: {msg}")
        sys.exit(1)
    node_id = msg.get("node_id", "?")
    print(f"[client] ✅ Autenticato — node_id: {node_id}")

    # 3. Poll
    send(conn, {"t": "poll"})
    line, buf = recv_line(conn, buf)
    msg = json.loads(line)
    print(f"[client] << {json.dumps(msg)}")

    t = msg.get("t", "")
    if t == "idle":
        print("[client] ℹ️  Nessun task disponibile (idle) — server attivo e funzionante.")
        print("[client] ✅ Test base completato: handshake OK, polling OK.")
        sys.exit(0)

    if t != "task":
        print(f"[client] ❌ Risposta inattesa: {msg}")
        sys.exit(1)

    task_id   = msg["id"]
    task_kind = msg.get("kind", "")
    payload   = msg.get("payload", "")
    print(f"[client] 📋 Task ricevuto [{task_id}] tipo={task_kind} payload={payload!r}")

    # 4. Esegui task (echo o ai_query semplice)
    if task_kind == "echo":
        result = f"ECHO: {payload}"
    else:
        result = f"Test OK (kind={task_kind} non gestito dal client di test)"

    print(f"[client] ⚙️  Risultato: {result!r}")
    time.sleep(0.5)

    # 5. Invia risultato con HMAC
    res_msg = {"t": "result", "id": task_id, "status": "done", "result": result}
    if TOKEN:
        res_msg["hmac"] = wan_hmac(TOKEN, task_id + "|" + result)
    send(conn, res_msg)

    # 6. Ack
    line, buf = recv_line(conn, buf)
    msg = json.loads(line)
    print(f"[client] << {json.dumps(msg)}")
    if msg.get("t") == "ack":
        print("[client] ✅ Test completato: handshake + task + HMAC tutto OK!")
    else:
        print(f"[client] ❌ Ack inatteso: {msg}")
        sys.exit(1)

    conn.close()

except ConnectionRefusedError:
    print(f"[client] ❌ Connessione rifiutata — Prismalux WAN non avviato su {SERVER}:{PORT}?")
    sys.exit(1)
except socket.timeout:
    print(f"[client] ❌ Timeout — {SERVER}:{PORT} non raggiungibile")
    sys.exit(1)
except Exception as e:
    print(f"[client] ❌ Errore: {e}")
    sys.exit(1)
PYEOF

else
    echo "Uso: $0 server [porta] [token]"
    echo "     $0 client <ip> [porta] [token]"
    exit 1
fi

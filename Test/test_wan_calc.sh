#!/usr/bin/env bash
# test_wan_calc.sh — Test WAN minimale: server invia "3+3", client calcola e risponde "6"
#
# HP   (server): ./test_wan_calc.sh server
# iMac (client): ./test_wan_calc.sh client <ip_hp>

set -e
MODE="${1:-}"
TOKEN="test1234"
PORT=11600

if [[ "$MODE" == "server" ]]; then
    echo "==> WAN CALC server su porta $PORT"
    python3 - "$PORT" "$TOKEN" <<'PYEOF'
import sys, socket, threading, json, uuid, hmac, hashlib

PORT  = int(sys.argv[1])
TOKEN = sys.argv[2]

def wan_hmac(token, data):
    return hmac.new(token.encode(), data.encode(), hashlib.sha256).hexdigest()

def handle(conn, addr):
    print(f"[server] Connesso: {addr}")
    buf = b""
    task_id = str(uuid.uuid4())[:8]
    try:
        while True:
            chunk = conn.recv(4096)
            if not chunk: break
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                if not line.strip(): continue
                msg = json.loads(line)
                t = msg.get("t", "")
                print(f"[server] << {json.dumps(msg)}")

                if t == "hello":
                    if msg.get("token") != TOKEN:
                        conn.sendall((json.dumps({"t":"error","msg":"auth_failed"})+"\n").encode())
                        return
                    r = {"t":"welcome","node_id": str(uuid.uuid4())[:8]}
                    conn.sendall((json.dumps(r)+"\n").encode())
                    print(f"[server] >> {json.dumps(r)}")

                elif t == "poll":
                    task = {"t":"task","id":task_id,"kind":"expr","payload":"3+3"}
                    conn.sendall((json.dumps(task)+"\n").encode())
                    print(f"[server] >> {json.dumps(task)}")

                elif t == "result":
                    rid    = msg.get("id","")
                    result = msg.get("result","")
                    expected_hmac = wan_hmac(TOKEN, rid+"|"+result)
                    if not hmac.compare_digest(expected_hmac, msg.get("hmac","")):
                        conn.sendall((json.dumps({"t":"error","msg":"hmac_invalid"})+"\n").encode())
                        print("[server] ❌ HMAC non valido")
                        return
                    ack = {"t":"ack","id":rid}
                    conn.sendall((json.dumps(ack)+"\n").encode())
                    print(f"[server] >> {json.dumps(ack)}")
                    if result == "6":
                        print(f"[server] ✅ RISULTATO CORRETTO: 3+3 = {result}")
                    else:
                        print(f"[server] ⚠️  Risultato inatteso: {result!r}")
                    return
    except Exception as e:
        print(f"[server] Errore: {e}")
    finally:
        conn.close()
        print(f"[server] Disconnesso: {addr}")

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", PORT))
srv.listen(4)
print(f"[server] In ascolto su 0.0.0.0:{PORT} — aspetto client iMac...")
while True:
    conn, addr = srv.accept()
    threading.Thread(target=handle, args=(conn, addr), daemon=True).start()
PYEOF

elif [[ "$MODE" == "client" ]]; then
    SERVER_IP="${2:?Uso: $0 client <ip_server>}"
    echo "==> WAN CALC client → $SERVER_IP:$PORT"
    python3 - "$SERVER_IP" "$PORT" "$TOKEN" <<'PYEOF'
import sys, socket, json, hmac, hashlib, uuid

SERVER = sys.argv[1]
PORT   = int(sys.argv[2])
TOKEN  = sys.argv[3]

def wan_hmac(token, data):
    return hmac.new(token.encode(), data.encode(), hashlib.sha256).hexdigest()

def send(conn, obj):
    conn.sendall((json.dumps(obj)+"\n").encode())
    print(f"[client] >> {json.dumps(obj)}")

def recv_line(conn, buf):
    while b"\n" not in buf:
        chunk = conn.recv(4096)
        if not chunk: raise ConnectionError("Connessione chiusa")
        buf += chunk
    line, buf = buf.split(b"\n", 1)
    return line, buf

conn = socket.create_connection((SERVER, PORT), timeout=10)
print(f"[client] Connesso a {SERVER}:{PORT}")
buf = b""

send(conn, {"t":"hello","name":"imac_calc_client","token":TOKEN,"caps":["expr"]})
line, buf = recv_line(conn, buf)
msg = json.loads(line)
print(f"[client] << {json.dumps(msg)}")
assert msg.get("t") == "welcome", f"Auth fallita: {msg}"
print(f"[client] ✅ Autenticato — node_id: {msg.get('node_id')}")

send(conn, {"t":"poll"})
line, buf = recv_line(conn, buf)
msg = json.loads(line)
print(f"[client] << {json.dumps(msg)}")
assert msg.get("t") == "task", f"Risposta inattesa: {msg}"

task_id = msg["id"]
payload = msg.get("payload","")
print(f"[client] 📋 Task: calcola '{payload}'")

result = str(eval(payload))
print(f"[client] ⚙️  Risultato: {result}")

res_msg = {"t":"result","id":task_id,"status":"done","result":result,
           "hmac": wan_hmac(TOKEN, task_id+"|"+result)}
send(conn, res_msg)

line, buf = recv_line(conn, buf)
msg = json.loads(line)
print(f"[client] << {json.dumps(msg)}")
assert msg.get("t") == "ack", f"Ack inatteso: {msg}"
print(f"[client] ✅ Test OK: il server ha confermato 3+3 = {result}")
conn.close()
PYEOF

else
    echo "Uso: $0 server"
    echo "     $0 client <ip_hp>"
    exit 1
fi

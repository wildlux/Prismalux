#!/usr/bin/env python3
"""
test_wan_compute.py — Simulatore + test suite WAN Compute (porta 11600)

Modalità:
  python3 test_wan_compute.py sim      # server + client in-process (no GUI)
  python3 test_wan_compute.py client   # client reale → si connette all'app
  python3 test_wan_compute.py client <host> <port>
"""

import sys, socket, json, threading, time, subprocess, platform, re, urllib.request

PORT     = 11600
SIM_PORT = 11601      # porta simulazione locale (evita conflitto con app)
HOST     = "127.0.0.1"
TIMEOUT  = 120        # secondi per task AI
OLLAMA   = "http://127.0.0.1:11434"

# ══════════════════════════════════════════════════════════════════
# Ollama — rileva modello disponibile e chiama /api/chat
# ══════════════════════════════════════════════════════════════════

def ollama_model() -> str:
    """Restituisce il primo modello chat disponibile (no embed)."""
    try:
        with urllib.request.urlopen(f"{OLLAMA}/api/tags", timeout=3) as r:
            models = json.loads(r.read())["models"]
        skip = ("embed", "minilm", "rerank", "bge-", "nomic")
        for m in models:
            name = m["name"].lower()
            if not any(s in name for s in skip):
                return m["name"]
    except Exception:
        pass
    return ""

_OLLAMA_MODEL = None   # lazy init

def ollama_chat(system: str, user: str, timeout: int = TIMEOUT) -> str:
    """Chiama Ollama /api/chat (non-streaming) e restituisce la risposta."""
    global _OLLAMA_MODEL
    if _OLLAMA_MODEL is None:
        _OLLAMA_MODEL = ollama_model()
    if not _OLLAMA_MODEL:
        raise RuntimeError("Nessun modello Ollama disponibile")

    body = json.dumps({
        "model":    _OLLAMA_MODEL,
        "stream":   False,
        "messages": [
            {"role": "system",  "content": system},
            {"role": "user",    "content": user},
        ]
    }).encode()

    req = urllib.request.Request(
        f"{OLLAMA}/api/chat",
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST")

    with urllib.request.urlopen(req, timeout=timeout) as r:
        resp = json.loads(r.read())
    return resp["message"]["content"].strip()

# Prompt AI che replicano quelli di wanCliHandleTask in lan_wan_page.cpp
AI_PROMPTS = {
    "ai_query":
        "Sei un assistente AI preciso e conciso. Rispondi SEMPRE in italiano.",
    "code_assist":
        "Sei un esperto programmatore. Scrivi codice pulito, commentato e funzionante. "
        "Il payload è JSON con chiavi 'lang' e 'task'. Rispondi in italiano.",
    "code_review":
        "Sei un esperto revisore di codice. Analizza il codice fornito: "
        "trova bug, vulnerabilità, inefficienze e suggerisci miglioramenti. "
        "Rispondi in italiano con sezioni: Bug, Sicurezza, Performance, Stile.",
    "code_translate":
        "Sei un esperto di traduzione tra linguaggi di programmazione. "
        "Il payload è JSON con 'from_lang', 'to_lang' e 'code'. "
        "Traduci il codice preservando la logica.",
    "code_reverse":
        "Sei un esperto di reverse engineering. Il payload è JSON con 'lang' e 'code'. "
        "Spiega la logica e ricostruisci il sorgente probabile. Rispondi in italiano.",
    "math_solve":
        "Sei un professore di matematica. Risolvi il problema passo per passo "
        "mostrando ogni passaggio con spiegazione. Rispondi in italiano.",
    "math_seq":
        "Sei un matematico esperto. Data la sequenza, trova la formula generale, "
        "il termine n-esimo e la natura della sequenza. Rispondi in italiano.",
    "math_nth":
        "Sei un matematico. Il payload può essere JSON con 'sequenza' e 'n' "
        "oppure testo libero. Calcola il termine richiesto e spiega il metodo. "
        "Rispondi in italiano.",
    "paper_gen":
        "Sei un ricercatore accademico. Genera un paper scientifico strutturato. "
        "Struttura: Abstract, Introduzione, Metodi, Risultati, Conclusioni. "
        "Rispondi in italiano con formattazione Markdown.",
    "web_search":
        "Sei un assistente di ricerca. Fornisci un riassunto esaustivo "
        "delle informazioni principali sull'argomento. Rispondi in italiano.",
    "ai_tutor":
        "Sei un tutor esperto. Il payload è JSON con 'argomento' e 'livello'. "
        "Spiega in modo chiaro con esempi pratici ed esercizi. Rispondi in italiano.",
    "ai_data_analysis":
        "Sei un analista dati. Il payload è JSON con 'dati' (CSV) e 'richiesta'. "
        "Analizza, trova pattern e trend. Rispondi in italiano.",
    "ai_fenomeno":
        "Sei un analista scientifico. Valuta la probabilità del fenomeno. "
        "Struttura: PROBABILITÀ (0-100%), Evidenze, Contraddizioni, Verdetto. Rispondi in italiano.",
    "ai_730":
        "Sei un esperto fiscalista italiano specializzato nel modello 730. "
        "Cita gli articoli di legge rilevanti. Rispondi SOLO in italiano.",
    "ai_tfr":
        "Sei un esperto di diritto del lavoro. Calcola il TFR (art. 2120 c.c.). "
        "Mostra: quota annua, rivalutazione, totale lordo, tassazione. Rispondi in italiano.",
}

def is_ai_task(kind: str) -> bool:
    return kind in AI_PROMPTS

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
    """Esegue un task: AI via Ollama oppure subprocess/sistema."""
    try:
        # ── Task AI: delega a Ollama ──
        if is_ai_task(kind):
            system = AI_PROMPTS[kind]
            result = ollama_chat(system, payload)
            return True, result

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
            return False, f"task kind '{kind}' non supportato da questo nodo"

    except Exception as e:
        return False, str(e)

def wan_client(host, port):
    """Client che si connette al server e processa task."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    print(f"  [client] connesso a {host}:{port}")

    model = ollama_model()
    if model:
        print(f"  [client] Ollama: {model}")
        ai_caps = list(AI_PROMPTS.keys())
    else:
        print("  [client] Ollama non disponibile — solo task locali")
        ai_caps = []

    local_caps = ["system_info","net_info","shell_cmd","eval_script",
                  "python_repl","math_expr","file_read","git_cmd",
                  "graphviz_render","matplotlib_plot"]
    send(sock, {"t": "hello", "name": "TestNode-Python",
                "caps": local_caps + ai_caps})
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

TEST_TASKS_LOCAL = [
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

TEST_TASKS_AI = [
    {"id": "a01", "kind": "ai_query",    "payload": "quanto fa 5+5?"},
    {"id": "a02", "kind": "math_solve",  "payload": "Risolvi per x: x^2 - 5x + 6 = 0"},
    {"id": "a03", "kind": "math_seq",    "payload": "1, 1, 2, 3, 5, 8, 13, 21"},
    {"id": "a04", "kind": "code_assist",
     "payload": '{"lang":"Python","task":"Funzione che calcola i numeri primi fino a N"}'},
]

# ══════════════════════════════════════════════════════════════════
# Modalità sim: server + client in thread separati
# ══════════════════════════════════════════════════════════════════

def mode_sim(with_ai: bool = False):
    tasks = TEST_TASKS_LOCAL + (TEST_TASKS_AI if with_ai else [])
    label = "locale + AI" if with_ai else "locale"
    print("=" * 60)
    print(f"  WAN Compute — Simulazione {label} (porta {SIM_PORT})")
    print("=" * 60)
    if with_ai:
        model = ollama_model()
        if not model:
            print("  ⚠  Ollama non disponibile — i task AI saranno skippati")
        else:
            print(f"  🤖 Modello: {model}")

    results = []
    ready   = threading.Event()

    srv_thread = threading.Thread(
        target=mini_server,
        args=(tasks, results, ready, SIM_PORT),
        daemon=True)
    srv_thread.start()
    ready.wait(timeout=3)

    try:
        wan_client(HOST, SIM_PORT)
    except Exception as e:
        print(f"\n  ERRORE client: {e}")

    srv_thread.join(timeout=TIMEOUT + 10)

    print()
    print("=" * 60)
    print(f"  RIEPILOGO — {len(results)}/{len(tasks)} task completati")
    print("=" * 60)
    ok_count = 0
    for r in results:
        status = "✅" if r["ok"] else "❌"
        preview = r["result"].replace("\n", " ")[:100]
        print(f"  {status} [{r['id']}] {r['kind']}")
        print(f"       → {preview}")
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
        ok = mode_sim(with_ai=False)
        sys.exit(0 if ok else 1)
    elif mode == "sim-ai":
        ok = mode_sim(with_ai=True)
        sys.exit(0 if ok else 1)
    elif mode == "client":
        h = sys.argv[2] if len(sys.argv) > 2 else HOST
        p = int(sys.argv[3]) if len(sys.argv) > 3 else PORT
        mode_client(h, p)
    else:
        print(f"Uso: {sys.argv[0]} [sim|sim-ai|client [host] [port]]")
        sys.exit(1)

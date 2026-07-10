#!/usr/bin/env python3
"""
api_tester_mcp — Test endpoint HTTP Prismalux
=============================================
Verifica LAN server (:11500), WAN Compute (:11600), Ollama (:11434),
llama-server (:8081). Invia richieste HTTP/JSON e misura tempi risposta.

Tools:
  check_lan_server    — verifica tutti gli endpoint REST del LAN server
  check_wan_compute   — verifica connettività WAN Compute TCP:11600
  check_ollama        — GET /api/tags su Ollama
  http_get            — GET su URL arbitrario
  http_post           — POST JSON su URL arbitrario
  load_test           — N richieste sequenziali con timing
"""
import sys, json, os, re, asyncio, time
import urllib.request, urllib.error, urllib.parse, socket
from pathlib import Path
from typing import Any

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from _shared.net_safety import validate_url as _validate_url_base, build_safe_opener

ROOT = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))

LAN_HOST   = "127.0.0.1"
LAN_PORT   = 11500
WAN_PORT   = 11600
OLLAMA_PORT = 11434
LLAMA_PORT  = 8081

# ─── Validazione URL (OS-18: _shared/net_safety.py) ──────────────────────────
# Questo MCP esiste apposta per testare i servizi LOCALI di Prismalux
# (127.0.0.1:11500/11600/11434/8081), quindi — a differenza degli altri MCP
# che bloccano OGNI indirizzo privato — qui il loopback resta permesso di
# proposito (romperebbe lo scopo del tool bloccarlo: allow_loopback=True).
# Restano bloccati la scansione di altri host della LAN (192.168.x/10.x/
# 172.16-31.x) e l'IP metadata cloud/link-local (169.254.x, incluso
# 169.254.169.254), che un agente autonomo indotto potrebbe altrimenti
# raggiungere con http_get o usare per un mini-DoS con load_test (fino a
# 100 richieste).
def _validate_url(url: str) -> str | None:
    return _validate_url_base(url, allow_loopback=True)

_SAFE_OPENER = build_safe_opener(allow_loopback=True)

LAN_ENDPOINTS = [
    ("GET",  "/",              "Home page"),
    ("GET",  "/api/models",    "Lista modelli LLM"),
    ("POST", "/api/chat",      "Chat endpoint"),
    ("POST", "/api/whisper",   "STT Whisper"),
    ("GET",  "/api/status",    "Status server"),
]

def _http(method: str, url: str, body: dict | None = None,
          timeout: float = 5.0) -> tuple[int, float, str]:
    t0   = time.time()
    data = json.dumps(body).encode() if body else None
    req  = urllib.request.Request(
        url, data=data, method=method,
        headers={"Content-Type": "application/json", "User-Agent": "prismalux-api-tester/1.0"}
    )
    try:
        with _SAFE_OPENER.open(req, timeout=timeout) as resp:
            content = resp.read(4096).decode(errors="replace")
            return resp.status, time.time() - t0, content
    except urllib.error.HTTPError as e:
        return e.code, time.time() - t0, e.read(1024).decode(errors="replace")
    except (urllib.error.URLError, OSError) as e:
        return 0, time.time() - t0, str(e)

def _tcp_ping(host: str, port: int, timeout: float = 2.0) -> float | None:
    t0 = time.time()
    try:
        sock = socket.create_connection((host, port), timeout=timeout)
        sock.close()
        return time.time() - t0
    except OSError:
        return None

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_check_lan_server(args: dict) -> str:
    host = args.get("host", LAN_HOST)
    port = int(args.get("port", LAN_PORT))
    base = f"http://{host}:{port}"

    # Prima verifica connettività TCP
    latency = _tcp_ping(host, port)
    if latency is None:
        return (f"❌ LAN server non raggiungibile su {host}:{port}\n"
                f"  Avvia Prismalux e abilita il server LAN nel pannello LAN & WAN.")

    lines = [f"🌐 LAN Server {base}  (ping: {latency*1000:.0f}ms)\n"]
    for method, path, desc in LAN_ENDPOINTS:
        body = {"message": "test", "model": "test"} if path == "/api/chat" else None
        status, elapsed, content = _http(method, f"{base}{path}", body=body, timeout=3.0)
        icon  = "✅" if 200 <= status < 300 else ("🟡" if status == 405 else "🔴")
        ms    = f"{elapsed*1000:.0f}ms"
        short = content[:60].replace("\n"," ") if content else ""
        lines.append(f"  {icon} {method:<4} {path:<20} {status}  {ms:<6}  {desc}")
        if status == 0: lines.append(f"     ↳ {short}")
    return "\n".join(lines)

def handle_check_wan_compute(args: dict) -> str:
    host = args.get("host", LAN_HOST)
    port = int(args.get("port", WAN_PORT))
    lines = [f"🖧 WAN Compute {host}:{port}\n"]

    latency = _tcp_ping(host, port)
    if latency is None:
        lines.append(f"  ❌ Non raggiungibile — avvia il server WAN dalla tab LAN & WAN.")
        return "\n".join(lines)
    lines.append(f"  ✅ TCP connesso ({latency*1000:.0f}ms)")

    # Invia ping JSON
    try:
        sock = socket.create_connection((host, port), timeout=3.0)
        ping_msg = json.dumps({"type": "ping"}) + "\n"
        sock.sendall(ping_msg.encode())
        sock.settimeout(2.0)
        resp = b""
        try:
            while True:
                chunk = sock.recv(1024)
                if not chunk: break
                resp += chunk
                if b"\n" in resp: break
        except OSError:
            pass
        sock.close()
        if resp:
            lines.append(f"  JSON risposta: {resp.decode(errors='replace')[:100]}")
        else:
            lines.append("  Nessuna risposta JSON (server attivo ma non ha risposto al ping)")
    except OSError as e:
        lines.append(f"  ⚠️ Connesso ma errore invio: {e}")
    return "\n".join(lines)

def handle_check_ollama(args: dict) -> str:
    host = args.get("host", LAN_HOST)
    port = int(args.get("port", OLLAMA_PORT))
    lines = [f"🦙 Ollama {host}:{port}\n"]
    latency = _tcp_ping(host, port)
    if latency is None:
        lines.append(f"  ❌ Ollama non risponde — avvia con: ollama serve")
        return "\n".join(lines)
    status, elapsed, content = _http("GET", f"http://{host}:{port}/api/tags", timeout=5.0)
    if status == 200:
        try:
            data    = json.loads(content)
            models  = data.get("models", [])
            lines.append(f"  ✅ Online ({elapsed*1000:.0f}ms)  —  {len(models)} modelli caricati")
            for m in models[:8]:
                size = m.get("size", 0)
                lines.append(f"    • {m.get('name','?'):<40} {size/1e9:.1f} GB")
        except json.JSONDecodeError:
            lines.append(f"  ✅ Online ma risposta non JSON: {content[:100]}")
    else:
        lines.append(f"  ❌ Status {status}: {content[:100]}")
    return "\n".join(lines)

def handle_http_get(args: dict) -> str:
    url     = args.get("url","").strip()
    timeout = float(args.get("timeout_s", 10.0))
    if not url: return "Specifica 'url'."
    err = _validate_url(url)
    if err: return f"❌ {err}"
    status, elapsed, content = _http("GET", url, timeout=timeout)
    icon = "✅" if 200 <= status < 300 else "❌"
    lines = [f"{icon} GET {url}  →  {status}  ({elapsed*1000:.0f}ms)\n"]
    # Prova a formattare JSON
    try:
        obj = json.loads(content)
        lines.append(json.dumps(obj, indent=2, ensure_ascii=False)[:1200])
    except json.JSONDecodeError:
        lines.append(content[:800])
    return "\n".join(lines)

def handle_http_post(args: dict) -> str:
    url     = args.get("url","").strip()
    body    = args.get("body", {})
    timeout = float(args.get("timeout_s", 10.0))
    if not url: return "Specifica 'url'."
    err = _validate_url(url)
    if err: return f"❌ {err}"
    if isinstance(body, str):
        try: body = json.loads(body)
        except: return "Il campo 'body' deve essere un oggetto JSON."
    status, elapsed, content = _http("POST", url, body=body, timeout=timeout)
    icon = "✅" if 200 <= status < 300 else "❌"
    lines = [f"{icon} POST {url}  →  {status}  ({elapsed*1000:.0f}ms)\n"]
    try:
        obj = json.loads(content)
        lines.append(json.dumps(obj, indent=2, ensure_ascii=False)[:1200])
    except json.JSONDecodeError:
        lines.append(content[:800])
    return "\n".join(lines)

def handle_load_test(args: dict) -> str:
    url     = args.get("url","").strip()
    n       = min(int(args.get("n", 10)), 100)
    method  = args.get("method", "GET").upper()
    body    = args.get("body", None)
    timeout = float(args.get("timeout_s", 5.0))
    if not url: return "Specifica 'url'."
    err = _validate_url(url)
    if err: return f"❌ {err}"
    times, errors = [], 0
    for i in range(n):
        status, elapsed, _ = _http(method, url, body=body, timeout=timeout)
        if 200 <= status < 300:
            times.append(elapsed)
        else:
            errors += 1
    if not times:
        return f"❌ Tutte le {n} richieste hanno fallito."
    avg  = sum(times) / len(times)
    mn   = min(times)
    mx   = max(times)
    p95  = sorted(times)[int(len(times) * 0.95)]
    lines = [f"⏱  Load test: {method} {url}  (n={n})\n"]
    lines.append(f"  ✅ Successi:  {len(times)}/{n}")
    lines.append(f"  ❌ Errori:    {errors}")
    lines.append(f"  Media:       {avg*1000:.0f}ms")
    lines.append(f"  Min:         {mn*1000:.0f}ms")
    lines.append(f"  Max:         {mx*1000:.0f}ms")
    lines.append(f"  p95:         {p95*1000:.0f}ms")
    lines.append(f"  Throughput:  {len(times)/sum(times):.1f} req/s")
    return "\n".join(lines)

HANDLERS = {
    "check_lan_server":  handle_check_lan_server,
    "check_wan_compute": handle_check_wan_compute,
    "check_ollama":      handle_check_ollama,
    "http_get":          handle_http_get,
    "http_post":         handle_http_post,
    "load_test":         handle_load_test,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"check_lan_server","description":"Verifica tutti gli endpoint REST del LAN server Prismalux (:11500): chat, whisper, models, status.","inputSchema":{"type":"object","properties":{"host":{"type":"string","default":"127.0.0.1"},"port":{"type":"integer","default":11500}}}},
    {"name":"check_wan_compute","description":"Verifica connettività TCP e risposta JSON del server WAN Compute (:11600).","inputSchema":{"type":"object","properties":{"host":{"type":"string","default":"127.0.0.1"},"port":{"type":"integer","default":11600}}}},
    {"name":"check_ollama","description":"Verifica che Ollama sia online (:11434) e lista i modelli caricati.","inputSchema":{"type":"object","properties":{"host":{"type":"string","default":"127.0.0.1"},"port":{"type":"integer","default":11434}}}},
    {"name":"http_get","description":"Esegue una richiesta HTTP GET su un URL arbitrario e mostra risposta + timing.","inputSchema":{"type":"object","properties":{"url":{"type":"string"},"timeout_s":{"type":"number","default":10}},"required":["url"]}},
    {"name":"http_post","description":"Esegue una richiesta HTTP POST JSON su un URL arbitrario.","inputSchema":{"type":"object","properties":{"url":{"type":"string"},"body":{"type":"object","default":{}},"timeout_s":{"type":"number","default":10}},"required":["url"]}},
    {"name":"load_test","description":"Invia N richieste sequenziali a un endpoint e calcola media/p95/throughput.","inputSchema":{"type":"object","properties":{"url":{"type":"string"},"n":{"type":"integer","default":10},"method":{"type":"string","default":"GET"},"body":{"type":"object"},"timeout_s":{"type":"number","default":5}},"required":["url"]}},
]

def _ok(rid, r): return json.dumps({"jsonrpc":"2.0","id":rid,"result":r})
def _err(rid, c, m): return json.dumps({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
async def _main():
    loop = asyncio.get_event_loop()
    rl   = lambda: loop.run_in_executor(None, sys.stdin.readline)
    while True:
        line = (await rl()).strip()
        if not line: continue
        try: req = json.loads(line)
        except: sys.stdout.write(_err(None,-32700,"parse")+"\n"); sys.stdout.flush(); continue
        rid, method, params = req.get("id"), req.get("method",""), req.get("params",{})
        if method == "initialize":
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"api-tester","version":"1.0.0"},"capabilities":{"tools":{}}})
        elif method == "tools/list": resp = _ok(rid,{"tools":TOOL_DEFS})
        elif method == "tools/call":
            name, targs = params.get("name",""), params.get("arguments",{})
            h = HANDLERS.get(name)
            if not h: resp = _err(rid,-32601,f"unknown: {name}")
            else:
                try:
                    text = await asyncio.to_thread(h, targs)
                    resp = _ok(rid,{"content":[{"type":"text","text":text}],"isError":False})
                except Exception as e:
                    resp = _ok(rid,{"content":[{"type":"text","text":str(e)}],"isError":True})
        elif method == "notifications/initialized": continue
        else: resp = _err(rid,-32601,method)
        sys.stdout.write(resp+"\n"); sys.stdout.flush()
def main(): asyncio.run(_main())
if __name__ == "__main__": main()

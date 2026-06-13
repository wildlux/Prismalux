#!/usr/bin/env python3
"""
network_recon_mcp — Ricognizione di rete locale e remota
=========================================================
Wrapper sicuro per nmap, whois, dig, ping e traceroute.
Utile per auditare la rete LAN, il WAN Compute e i servizi
che Prismalux espone (porte 11434, 8081, 11600, ecc.).

Tools esposti:
  ping_host        — ping a un host (latenza e packet loss)
  scan_ports       — scansione porte TCP (nmap) su host/rete
  scan_services    — rileva servizi e versioni su porte aperte
  dns_lookup       — risoluzione DNS (A, AAAA, MX, TXT, PTR)
  whois_query      — WHOIS su dominio o IP
  traceroute       — tracciamento percorso verso un host
  scan_local_net   — scansiona la rete LAN locale (host attivi)
  check_prismalux  — verifica che le porte Prismalux siano aperte
"""

import sys, json, os, re, asyncio, logging, subprocess, socket
from typing import Any

logging.basicConfig(level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL","WARNING")))
logger = logging.getLogger(__name__)

# Porte chiave di Prismalux
PRISMALUX_PORTS = {
    11434: "Ollama",
    8081:  "llama-server",
    8092:  "OpenCode",
    11600: "WAN Compute",
    5000:  "LAN server (web)",
    22:    "SSH",
}

# ─── Helpers ─────────────────────────────────────────────────────────────────

def _run(cmd: list[str], timeout: int = 30) -> tuple[int, str]:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return r.returncode, (r.stdout + r.stderr).strip()
    except subprocess.TimeoutExpired:
        return -1, f"Timeout ({timeout}s)"
    except FileNotFoundError:
        return -1, f"Comando non trovato: {cmd[0]}. Installalo con: sudo apt install {cmd[0]}"

def _is_private_ip(ip: str) -> bool:
    import ipaddress
    try:
        addr = ipaddress.ip_address(ip)
        return addr.is_private or addr.is_loopback
    except ValueError:
        return False

def _resolve(host: str) -> str:
    try:
        return socket.gethostbyname(host)
    except socket.gaierror:
        return host

def _tool_check(name: str) -> tuple[bool, str]:
    code, out = _run([name, "--version"] if name != "whois" else [name, "--help"])
    return code == 0 or "Usage" in out or out != "", out

# ─── Handlers ─────────────────────────────────────────────────────────────────

def handle_ping_host(args: dict) -> str:
    host  = args.get("host","").strip()
    count = min(int(args.get("count", 4)), 20)
    if not host:
        return "Specifica 'host'."

    code, out = _run(["ping", "-c", str(count), "-W", "2", host], timeout=count*3+5)
    if code != 0 and "unreachable" not in out.lower() and "100%" not in out:
        return f"ping non disponibile o host '{host}' irraggiungibile:\n{out[:300]}"

    # Estrai statistiche
    lines = [f"🔵 ping {host} ({count} pacchetti):\n"]
    for line in out.splitlines():
        if "rtt" in line.lower() or "round-trip" in line.lower() or "packet" in line.lower():
            lines.append("  " + line.strip())
    if not lines[1:]:
        lines.append(out[:400])
    return "\n".join(lines)


def handle_scan_ports(args: dict) -> str:
    host     = args.get("host","").strip()
    ports    = args.get("ports","1-1024").strip()
    fast     = bool(args.get("fast", True))
    if not host:
        return "Specifica 'host'."

    # Safety: blocca scansioni di reti esterne grandi
    ip = _resolve(host)
    if not _is_private_ip(ip) and ("/" in host or "-" in ports.replace("1-","").replace("-1000","")):
        return "⚠️  Per sicurezza, scansioni aggressive sono limitate a IP privati/locali."

    nmap_args = ["nmap", "-T4" if fast else "-T2", f"-p{ports}", "--open"]
    if fast:
        nmap_args.append("-F")  # fast: top 100 porte
    nmap_args.append(host)

    code, out = _run(nmap_args, timeout=60)
    if code == -1:
        # Fallback senza nmap: prova con socket Python
        return _socket_scan(host, ports)
    return f"```\n{out[:2000]}\n```"


def _socket_scan(host: str, ports_str: str) -> str:
    """Fallback scanner senza nmap."""
    open_ports = []
    try:
        if "-" in ports_str:
            s, e = ports_str.split("-",1)
            port_list = range(int(s), min(int(e)+1, int(s)+200))
        elif "," in ports_str:
            port_list = [int(p) for p in ports_str.split(",") if p.strip().isdigit()]
        else:
            port_list = [int(ports_str)] if ports_str.isdigit() else [80,443,22,8080]
    except ValueError:
        port_list = [22,80,443,8080,8081,11434,11600]

    ip = _resolve(host)
    for port in port_list:
        try:
            with socket.create_connection((ip, port), timeout=0.5):
                svc = PRISMALUX_PORTS.get(port, "")
                open_ports.append(f"  ✅ {port:5d}/tcp  open  {svc}")
        except (socket.timeout, ConnectionRefusedError, OSError):
            pass

    if not open_ports:
        return f"Nessuna porta aperta trovata su {host} (range: {ports_str})\n(nmap non disponibile, usato scanner Python basilare)"
    return f"Porte aperte su {host}:\n" + "\n".join(open_ports) + "\n(nmap non disponibile, usato scanner Python basilare)"


def handle_scan_services(args: dict) -> str:
    host  = args.get("host","localhost").strip()
    ports = args.get("ports","").strip()

    if not ports:
        ports = ",".join(str(p) for p in PRISMALUX_PORTS.keys())

    nmap_args = ["nmap", "-sV", "-T4", f"-p{ports}", host]
    code, out = _run(nmap_args, timeout=90)
    if code == -1:
        return f"nmap non disponibile. Installa con: sudo apt install nmap\nFallback:\n" + _socket_scan(host, ports)
    return f"```\n{out[:2000]}\n```"


def handle_dns_lookup(args: dict) -> str:
    host     = args.get("host","").strip()
    rec_type = args.get("type","A").upper().strip()
    if not host:
        return "Specifica 'host'."
    if rec_type not in ("A","AAAA","MX","TXT","NS","CNAME","PTR","SOA","ANY"):
        rec_type = "A"

    code, out = _run(["dig", "+short", rec_type, host], timeout=10)
    if code == -1:
        # Fallback: socket per A record
        try:
            results = socket.getaddrinfo(host, None)
            ips = list({r[4][0] for r in results})
            return f"DNS A {host}:\n" + "\n".join(f"  {ip}" for ip in ips) + "\n(dig non disponibile, usato socket Python)"
        except socket.gaierror as e:
            return f"DNS lookup fallito: {e}"

    lines = [f"DNS {rec_type} {host}:"]
    for line in out.splitlines():
        if line.strip():
            lines.append(f"  {line.strip()}")
    return "\n".join(lines) if len(lines) > 1 else f"Nessun record {rec_type} per {host}."


def handle_whois_query(args: dict) -> str:
    target = args.get("target","").strip()
    if not target:
        return "Specifica 'target' (dominio o IP)."

    code, out = _run(["whois", target], timeout=15)
    if code == -1:
        return f"whois non disponibile. Installa con: sudo apt install whois"

    # Estrai solo le righe più significative
    important = ["registrant","admin","tech","name server","nameserver","registrar",
                 "creation date","expiry date","expires","registered","organisation",
                 "country","netname","descr","netrange","cidr","orgname"]
    lines = [f"WHOIS {target}:\n"]
    seen  = set()
    for line in out.splitlines():
        low = line.lower()
        if any(k in low for k in important) and line.strip() not in seen:
            lines.append("  " + line.strip())
            seen.add(line.strip())
        if len(lines) > 30:
            break
    return "\n".join(lines) if len(lines) > 1 else out[:600]


def handle_traceroute(args: dict) -> str:
    host    = args.get("host","").strip()
    max_hop = min(int(args.get("max_hops", 15)), 20)
    if not host:
        return "Specifica 'host'."

    # Prova traceroute, poi tracepath come fallback
    for cmd in [["traceroute", "-m", str(max_hop), "-w", "1", host],
                ["tracepath",  "-m", str(max_hop), host]]:
        code, out = _run(cmd, timeout=max_hop*3+5)
        if code != -1:
            return f"```\n{out[:1500]}\n```"
    return "traceroute/tracepath non disponibili. Installa: sudo apt install traceroute"


def handle_scan_local_net(args: dict) -> str:
    subnet = args.get("subnet","").strip()
    if not subnet:
        # Auto-detect subnet locale
        code, out = _run(["ip", "route", "show"])
        m = re.search(r"(\d+\.\d+\.\d+\.\d+/\d+)\s+dev", out)
        subnet = m.group(1) if m else "192.168.1.0/24"

    # Solo reti private
    ip_part = subnet.split("/")[0]
    if not _is_private_ip(ip_part):
        return f"⚠️  Scansione LAN limitata a reti private. '{subnet}' non è una rete privata."

    code, out = _run(["nmap", "-sn", "-T4", subnet], timeout=60)
    if code == -1:
        # Fallback: ping sweep su /24
        lines = [f"Host attivi su {subnet} (ping sweep, nmap non disponibile):\n"]
        base = ".".join(ip_part.split(".")[:3])
        for i in range(1, 255):
            r = subprocess.run(["ping","-c1","-W1",f"{base}.{i}"],
                               capture_output=True, timeout=2)
            if r.returncode == 0:
                lines.append(f"  ✅ {base}.{i}")
        return "\n".join(lines) if len(lines) > 1 else f"Nessun host trovato su {subnet}."

    # Parsea output nmap
    hosts = re.findall(r"Nmap scan report for (.+)\nHost is up", out)
    lines = [f"Host attivi su {subnet} ({len(hosts)} trovati):\n"]
    for h in hosts:
        lines.append(f"  ✅ {h}")
    return "\n".join(lines) if hosts else f"Nessun host trovato su {subnet}."


def handle_check_prismalux(args: dict) -> str:
    host = args.get("host","localhost").strip()
    lines = [f"🔍 Verifica porte Prismalux su {host}:\n"]

    for port, service in PRISMALUX_PORTS.items():
        try:
            with socket.create_connection((host, port), timeout=1.0):
                lines.append(f"  ✅ {port:5d}  {service:20s} APERTA")
        except (socket.timeout, ConnectionRefusedError):
            lines.append(f"  ❌ {port:5d}  {service:20s} chiusa")
        except OSError:
            lines.append(f"  ⚠️  {port:5d}  {service:20s} errore")

    lines.append("\nPer aprire una porta Ollama:\n  OLLAMA_HOST=0.0.0.0 ollama serve")
    return "\n".join(lines)


HANDLERS = {
    "ping_host":       handle_ping_host,
    "scan_ports":      handle_scan_ports,
    "scan_services":   handle_scan_services,
    "dns_lookup":      handle_dns_lookup,
    "whois_query":     handle_whois_query,
    "traceroute":      handle_traceroute,
    "scan_local_net":  handle_scan_local_net,
    "check_prismalux": handle_check_prismalux,
}

TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"ping_host","description":"Invia ping a un host e mostra latenza e packet loss.","inputSchema":{"type":"object","properties":{"host":{"type":"string","description":"Hostname o IP"},"count":{"type":"integer","description":"Numero di pacchetti (default: 4)","default":4}},"required":["host"]}},
    {"name":"scan_ports","description":"Scansiona porte TCP su un host con nmap (fallback socket Python). Per sicurezza, scansioni aggressive sono limitate a IP locali.","inputSchema":{"type":"object","properties":{"host":{"type":"string","description":"Hostname o IP da scansionare"},"ports":{"type":"string","description":"Range porte (es. '1-1024', '22,80,443', default: '1-1024')","default":"1-1024"},"fast":{"type":"boolean","description":"Modalità veloce (top 100 porte, default: true)","default":True}},"required":["host"]}},
    {"name":"scan_services","description":"Rileva servizi e versioni sulle porte aperte di un host (nmap -sV). Default: porte Prismalux.","inputSchema":{"type":"object","properties":{"host":{"type":"string","description":"Hostname o IP (default: localhost)","default":"localhost"},"ports":{"type":"string","description":"Porte da scansionare (default: porte Prismalux)","default":""}}}},
    {"name":"dns_lookup","description":"Risoluzione DNS per qualsiasi tipo di record (A, AAAA, MX, TXT, NS, ecc.).","inputSchema":{"type":"object","properties":{"host":{"type":"string","description":"Hostname da risolvere"},"type":{"type":"string","description":"Tipo record: A|AAAA|MX|TXT|NS|CNAME|PTR (default: A)","default":"A"}},"required":["host"]}},
    {"name":"whois_query","description":"Esegue WHOIS su un dominio o IP per info registrante, nameserver, scadenza.","inputSchema":{"type":"object","properties":{"target":{"type":"string","description":"Dominio (es. 'example.com') o IP"}},"required":["target"]}},
    {"name":"traceroute","description":"Traccia il percorso dei pacchetti verso un host (traceroute/tracepath).","inputSchema":{"type":"object","properties":{"host":{"type":"string","description":"Hostname o IP destinazione"},"max_hops":{"type":"integer","description":"Numero massimo di hop (default: 15)","default":15}},"required":["host"]}},
    {"name":"scan_local_net","description":"Scansiona la rete LAN locale per trovare host attivi (nmap -sn o ping sweep). Limitato a reti private.","inputSchema":{"type":"object","properties":{"subnet":{"type":"string","description":"Subnet CIDR (es. '192.168.1.0/24'). Default: auto-detect.","default":""}}}},
    {"name":"check_prismalux","description":"Verifica lo stato di tutte le porte chiave di Prismalux (Ollama:11434, llama-server:8081, WAN:11600, ecc.).","inputSchema":{"type":"object","properties":{"host":{"type":"string","description":"Host dove gira Prismalux (default: localhost)","default":"localhost"}}}},
]

# ─── JSON-RPC 2.0 loop ───────────────────────────────────────────────────────

def _rpc_result(rid, result): return json.dumps({"jsonrpc":"2.0","id":rid,"result":result})
def _rpc_error(rid, code, msg): return json.dumps({"jsonrpc":"2.0","id":rid,"error":{"code":code,"message":msg}})

async def _main_async():
    loop = asyncio.get_event_loop()
    async def readline(): return await loop.run_in_executor(None, sys.stdin.readline)
    while True:
        line = await readline()
        if not line: break
        line = line.strip()
        if not line: continue
        try: req = json.loads(line)
        except json.JSONDecodeError as e:
            sys.stdout.write(_rpc_error(None,-32700,str(e))+"\n"); sys.stdout.flush(); continue
        rid = req.get("id"); method = req.get("method",""); params = req.get("params",{})
        if method == "initialize":
            resp = _rpc_result(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"network-recon","version":"1.0.0"},"capabilities":{"tools":{}}})
        elif method == "tools/list":
            resp = _rpc_result(rid,{"tools":TOOL_DEFS})
        elif method == "tools/call":
            name = params.get("name",""); targs = params.get("arguments",{})
            h = HANDLERS.get(name)
            if not h: resp = _rpc_error(rid,-32601,f"Tool sconosciuto: {name}")
            else:
                try:
                    text = await asyncio.to_thread(h, targs)
                    resp = _rpc_result(rid,{"content":[{"type":"text","text":text}],"isError":False})
                except Exception as e:
                    resp = _rpc_result(rid,{"content":[{"type":"text","text":f"Errore: {e}"}],"isError":True})
        elif method == "notifications/initialized": continue
        else: resp = _rpc_error(rid,-32601,f"Metodo sconosciuto: {method}")
        sys.stdout.write(resp+"\n"); sys.stdout.flush()

def main(): asyncio.run(_main_async())
if __name__ == "__main__": main()

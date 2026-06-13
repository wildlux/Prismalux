#!/usr/bin/env python3
"""
ssh_remote_mcp — SSH e deploy remoto per Prismalux
===================================================
Connessione SSH via subprocess (no paramiko), rsync build,
deploy client WAN Compute. Host salvati in ~/.prismalux/ssh_hosts.json.

Tools:
  list_hosts        — elenca host configurati
  add_host          — aggiunge host alla rubrica
  connect_test      — verifica connettività SSH
  run_command       — esegue comando remoto sicuro
  sync_build        — rsync build/ su macchina remota
  deploy_wan_client — copia e avvia client WAN Compute
"""
import sys, json, os, re, asyncio, subprocess
from pathlib import Path
from typing import Any

ROOT      = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
HOSTS_FILE = Path.home() / ".prismalux" / "ssh_hosts.json"

# Comandi SSH non ammessi
BLOCKED = re.compile(r'\brm\s+-rf\b|\bformat\b|\bmkfs\b|\bdd\b|\bsudo\s+rm\b', re.I)

SSH_FLAGS = ["-o", "StrictHostKeyChecking=accept-new",
             "-o", "ConnectTimeout=8",
             "-o", "BatchMode=yes"]

def _run(cmd: list[str], timeout: int = 30) -> tuple[int, str]:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return r.returncode, (r.stdout + r.stderr).strip()
    except (FileNotFoundError, subprocess.TimeoutExpired) as e:
        return -1, str(e)

def _load_hosts() -> dict:
    if HOSTS_FILE.exists():
        try: return json.loads(HOSTS_FILE.read_text())
        except: pass
    return {}

def _save_hosts(hosts: dict) -> None:
    HOSTS_FILE.parent.mkdir(parents=True, exist_ok=True)
    HOSTS_FILE.write_text(json.dumps(hosts, indent=2))

def _resolve(alias: str) -> dict | None:
    hosts = _load_hosts()
    if alias in hosts: return hosts[alias]
    # Prova come host diretto
    if "@" in alias or "." in alias:
        parts = alias.split("@")
        user  = parts[0] if len(parts) > 1 else os.environ.get("USER","")
        host  = parts[-1]
        return {"host": host, "user": user, "port": 22}
    return None

def _ssh_cmd(info: dict, remote_cmd: str) -> list[str]:
    port = str(info.get("port", 22))
    user = info.get("user", os.environ.get("USER",""))
    host = info["host"]
    key  = info.get("key", "")
    cmd  = ["ssh"] + SSH_FLAGS + ["-p", port]
    if key: cmd += ["-i", key]
    cmd += [f"{user}@{host}", remote_cmd]
    return cmd

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_list_hosts(args: dict) -> str:
    hosts = _load_hosts()
    if not hosts:
        return (f"Nessun host configurato.\n"
                f"Usa add_host(alias, host, user, port?) per aggiungere.\n"
                f"File: {HOSTS_FILE}")
    lines = [f"🖧 Host SSH configurati ({HOSTS_FILE.name})\n"]
    for alias, info in hosts.items():
        user = info.get("user","?")
        host = info.get("host","?")
        port = info.get("port",22)
        key  = "🔑 " + Path(info["key"]).name if info.get("key") else "🔓 password"
        lines.append(f"  {alias:<16} {user}@{host}:{port}  {key}")
    return "\n".join(lines)

def handle_add_host(args: dict) -> str:
    alias = args.get("alias","").strip()
    host  = args.get("host","").strip()
    user  = args.get("user", os.environ.get("USER","")).strip()
    port  = int(args.get("port", 22))
    key   = args.get("key","").strip()
    if not alias or not host: return "Richiesti: alias, host."
    hosts = _load_hosts()
    hosts[alias] = {"host": host, "user": user, "port": port}
    if key: hosts[alias]["key"] = key
    _save_hosts(hosts)
    return f"✅ Host '{alias}' aggiunto: {user}@{host}:{port}"

def handle_connect_test(args: dict) -> str:
    alias = args.get("host","").strip()
    if not alias: return "Specifica 'host' (alias o user@host)."
    info = _resolve(alias)
    if not info: return f"Host '{alias}' non trovato. Usa list_hosts() o add_host()."
    cmd = _ssh_cmd(info, "echo OK && uname -a && uptime")
    code, out = _run(cmd, timeout=15)
    if code == 0:
        return f"✅ Connessione OK a {info['host']}\n{out}"
    return f"❌ Connessione fallita a {info['host']}\n{out}"

def handle_run_command(args: dict) -> str:
    alias   = args.get("host","").strip()
    command = args.get("command","").strip()
    timeout = int(args.get("timeout_s", 30))
    if not alias or not command: return "Richiesti: host, command."
    if BLOCKED.search(command):
        return f"⛔ Comando bloccato per sicurezza: '{command}'\n  Comandi distruttivi non sono permessi via MCP."
    info = _resolve(alias)
    if not info: return f"Host non trovato: {alias}"
    cmd  = _ssh_cmd(info, command)
    code, out = _run(cmd, timeout=timeout)
    icon = "✅" if code == 0 else "❌"
    return f"{icon} {info['user']}@{info['host']}: {command}\n\n{out[:2000]}"

def handle_sync_build(args: dict) -> str:
    alias   = args.get("host","").strip()
    dest    = args.get("dest_path","~/prismalux_build").strip()
    dry_run = bool(args.get("dry_run", True))
    if not alias: return "Specifica 'host'."
    info = _resolve(alias)
    if not info: return f"Host non trovato: {alias}"

    build_dir = ROOT / "gui" / "build_gui"
    if not build_dir.exists():
        return f"Build non trovata: {build_dir}\n  Compila prima con prismalux-build → build()"

    user = info.get("user", os.environ.get("USER",""))
    host = info["host"]
    port = str(info.get("port", 22))
    key  = info.get("key","")

    rsync_cmd = ["rsync", "-avz", "--progress",
                 "-e", f"ssh -p {port}" + (f" -i {key}" if key else ""),
                 str(build_dir) + "/",
                 f"{user}@{host}:{dest}/"]
    if dry_run:
        rsync_cmd.insert(2, "--dry-run")

    c, out = _run(rsync_cmd, timeout=120)
    prefix = "[DRY RUN] " if dry_run else ""
    icon   = "✅" if c == 0 else "❌"
    return f"{prefix}{icon} rsync {build_dir.name}/ → {user}@{host}:{dest}\n\n{out[:1500]}"

def handle_deploy_wan_client(args: dict) -> str:
    alias   = args.get("host","").strip()
    if not alias: return "Specifica 'host'."
    info = _resolve(alias)
    if not info: return f"Host non trovato: {alias}"

    client_script = ROOT / "MCPs" / "wan_compute_client.py"
    user = info.get("user", os.environ.get("USER",""))
    host = info["host"]
    port = str(info.get("port", 22))
    key  = info.get("key","")
    ssh_e = f"ssh -p {port}" + (f" -i {key}" if key else "")

    lines = [f"🚀 Deploy WAN client su {user}@{host}\n"]
    # Copia lo script client se esiste
    if client_script.exists():
        c, out = _run(["rsync", "-avz", "-e", ssh_e,
                        str(client_script), f"{user}@{host}:~/wan_client.py"], timeout=30)
        lines.append(f"  {'✅' if c==0 else '❌'} Copia script: {out[:100]}")
    else:
        lines.append(f"  ⚠️ {client_script.name} non trovato — copia manuale necessaria")

    # Avvia il client remoto
    server_ip = args.get("server_ip", "")
    if server_ip:
        start_cmd = f"nohup python3 ~/wan_client.py --server {server_ip} --port 11600 &"
        c2, out2 = _run(_ssh_cmd(info, start_cmd), timeout=10)
        lines.append(f"  {'✅' if c2==0 else '❌'} Avvio client: {out2[:100]}")
    else:
        lines.append("\n  💡 Passa server_ip=<IP> per avviare il client automaticamente")
        lines.append(f"  Comando manuale: ssh {user}@{host} 'python3 ~/wan_client.py --server <IP> --port 11600'")
    return "\n".join(lines)

HANDLERS = {
    "list_hosts":         handle_list_hosts,
    "add_host":           handle_add_host,
    "connect_test":       handle_connect_test,
    "run_command":        handle_run_command,
    "sync_build":         handle_sync_build,
    "deploy_wan_client":  handle_deploy_wan_client,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"list_hosts","description":"Elenca gli host SSH configurati nella rubrica ~/.prismalux/ssh_hosts.json.","inputSchema":{"type":"object","properties":{}}},
    {"name":"add_host","description":"Aggiunge un host SSH alla rubrica locale.","inputSchema":{"type":"object","properties":{"alias":{"type":"string"},"host":{"type":"string"},"user":{"type":"string","default":""},"port":{"type":"integer","default":22},"key":{"type":"string","default":""}},"required":["alias","host"]}},
    {"name":"connect_test","description":"Verifica la connettività SSH a un host e mostra uptime.","inputSchema":{"type":"object","properties":{"host":{"type":"string"}},"required":["host"]}},
    {"name":"run_command","description":"Esegue un comando remoto via SSH (comandi distruttivi bloccati).","inputSchema":{"type":"object","properties":{"host":{"type":"string"},"command":{"type":"string"},"timeout_s":{"type":"integer","default":30}},"required":["host","command"]}},
    {"name":"sync_build","description":"Copia la cartella build_gui/ su un host remoto con rsync (dry_run=true di default).","inputSchema":{"type":"object","properties":{"host":{"type":"string"},"dest_path":{"type":"string","default":"~/prismalux_build"},"dry_run":{"type":"boolean","default":True}},"required":["host"]}},
    {"name":"deploy_wan_client","description":"Copia e avvia il client WAN Compute su una macchina remota.","inputSchema":{"type":"object","properties":{"host":{"type":"string"},"server_ip":{"type":"string","default":""}},"required":["host"]}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"ssh-remote","version":"1.0.0"},"capabilities":{"tools":{}}})
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

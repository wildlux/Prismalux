#!/usr/bin/env python3
"""
docker_mcp — Gestione container Docker per Prismalux
=====================================================
Elenca container/immagini, avvia/ferma container, build da Dockerfile,
log container, exec comandi. Utile per deploy servizi (LibreTranslate,
Ollama container, CI/CD, ambienti isolati per test).

Tools:
  check_docker      — versione Docker e stato daemon
  list_containers   — container attivi (e opzionalmente fermati)
  list_images       — immagini disponibili localmente
  run_container     — avvia un container
  stop_container    — ferma un container
  container_logs    — log di un container
  build_image       — build immagine da Dockerfile
  exec_container    — esegue comando in container attivo
"""
import sys, json, os, re, asyncio, subprocess
from pathlib import Path
from typing import Any

ROOT = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))

BLOCKED_IMAGES = re.compile(r'bitcoin|miner|xmrig|malware', re.I)

def _run(cmd: list[str], timeout: int = 30) -> tuple[int, str]:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return r.returncode, (r.stdout + r.stderr).strip()
    except (FileNotFoundError, subprocess.TimeoutExpired) as e:
        return -1, str(e)

def _docker_ok() -> bool:
    c, _ = _run(["docker", "info"])
    return c == 0

def _fmt_size(s: str) -> str:
    try:
        n = int(s)
        if n > 1e9: return f"{n/1e9:.1f}GB"
        if n > 1e6: return f"{n/1e6:.0f}MB"
        return f"{n/1e3:.0f}KB"
    except: return s

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_check_docker(args: dict) -> str:
    c, v = _run(["docker", "--version"])
    if c != 0:
        return ("❌ Docker non installato.\n"
                "  sudo apt install docker.io\n"
                "  sudo usermod -aG docker $USER && newgrp docker")
    lines = [f"✅ {v}\n"]
    ci, info = _run(["docker", "info", "--format",
                     "{{.ServerVersion}} | Containers: {{.Containers}} ({{.ContainersRunning}} running) | Images: {{.Images}}"])
    if ci == 0:
        lines.append(f"  Daemon: {info}")
    else:
        lines.append("  ⚠️ Daemon non raggiungibile. Avvia con: sudo systemctl start docker")
    return "\n".join(lines)

def handle_list_containers(args: dict) -> str:
    all_c   = bool(args.get("all", False))
    cmd     = ["docker", "ps", "--format",
                "{{.ID}}\t{{.Names}}\t{{.Image}}\t{{.Status}}\t{{.Ports}}"]
    if all_c: cmd.insert(2, "-a")
    c, out = _run(cmd)
    if c != 0: return f"❌ {out[:200]}" if out else "Docker non disponibile."
    if not out.strip(): return f"Nessun container {'(compresi fermati)' if all_c else 'attivo'}."
    lines = [f"🐳 Container {'(tutti)' if all_c else 'attivi'}\n"]
    lines.append(f"  {'ID':<12} {'Nome':<25} {'Immagine':<30} {'Status':<20} Porte")
    lines.append("  " + "─" * 90)
    for line in out.splitlines():
        parts = line.split("\t")
        if len(parts) >= 4:
            cid, name, image, status = parts[0], parts[1], parts[2], parts[3]
            ports = parts[4] if len(parts) > 4 else ""
            icon  = "✅" if "Up" in status else "⏹"
            lines.append(f"  {icon} {cid:<12} {name:<25} {image[:28]:<30} {status[:18]:<20} {ports[:30]}")
    return "\n".join(lines)

def handle_list_images(args: dict) -> str:
    c, out = _run(["docker", "images", "--format",
                   "{{.Repository}}\t{{.Tag}}\t{{.ID}}\t{{.Size}}\t{{.CreatedSince}}"])
    if c != 0: return f"❌ {out[:200]}"
    if not out.strip(): return "Nessuna immagine locale."
    lines = [f"📦 Immagini Docker locali\n"]
    lines.append(f"  {'Repository':<35} {'Tag':<15} {'ID':<12} {'Size':<10} Creata")
    lines.append("  " + "─" * 85)
    for line in out.splitlines():
        parts = line.split("\t")
        if len(parts) >= 5:
            repo, tag, iid, size, created = parts
            lines.append(f"  {repo[:33]:<35} {tag[:13]:<15} {iid[:10]:<12} {size[:8]:<10} {created}")
    return "\n".join(lines)

def handle_run_container(args: dict) -> str:
    image   = args.get("image","").strip()
    name    = args.get("name","").strip()
    ports   = args.get("ports", [])   # ["8080:80", ...]
    volumes = args.get("volumes", []) # ["/host:/container", ...]
    env_vars= args.get("env", {})
    detach  = bool(args.get("detach", True))
    rm      = bool(args.get("rm", False))
    if not image: return "Specifica 'image'."
    if BLOCKED_IMAGES.search(image):
        return f"⛔ Immagine bloccata: {image}"
    cmd = ["docker", "run"]
    if detach: cmd.append("-d")
    if rm:     cmd.append("--rm")
    if name:   cmd += ["--name", name]
    for p in (ports if isinstance(ports, list) else [ports]):   cmd += ["-p", str(p)]
    for v in (volumes if isinstance(volumes, list) else [volumes]): cmd += ["-v", str(v)]
    for k, val in (env_vars.items() if isinstance(env_vars, dict) else []):
        cmd += ["-e", f"{k}={val}"]
    cmd.append(image)
    c, out = _run(cmd, timeout=60)
    icon = "✅" if c == 0 else "❌"
    return f"{icon} docker run {image}\n  {out[:300]}"

def handle_stop_container(args: dict) -> str:
    name = args.get("name","").strip()
    if not name: return "Specifica 'name' o ID del container."
    c, out = _run(["docker", "stop", name])
    if c == 0:
        return f"✅ Container '{name}' fermato."
    return f"❌ {out[:200]}"

def handle_container_logs(args: dict) -> str:
    name  = args.get("name","").strip()
    tail  = str(args.get("tail", 50))
    if not name: return "Specifica 'name'."
    c, out = _run(["docker", "logs", "--tail", tail, name], timeout=10)
    if c != 0: return f"❌ {out[:200]}"
    return f"📋 Log: {name} (ultimi {tail} righe)\n\n{out[:3000]}"

def handle_build_image(args: dict) -> str:
    tag       = args.get("tag","prismalux:local").strip()
    path_arg  = args.get("path","").strip()
    dockerfile= args.get("dockerfile","").strip()
    build_path = Path(path_arg) if path_arg else ROOT
    if not build_path.is_absolute(): build_path = ROOT / build_path
    cmd = ["docker", "build", "-t", tag]
    if dockerfile: cmd += ["-f", str(dockerfile)]
    cmd.append(str(build_path))
    c, out = _run(cmd, timeout=300)
    icon = "✅" if c == 0 else "❌"
    lines = [f"{icon} docker build -t {tag}"]
    # Mostra errori e step finali
    relevant = [l for l in out.splitlines() if "Step" in l or "Error" in l or "error" in l.lower() or "Successfully" in l]
    for l in (relevant or out.splitlines()[-10:]):
        lines.append(f"  {l}")
    return "\n".join(lines)

def handle_exec_container(args: dict) -> str:
    name    = args.get("name","").strip()
    command = args.get("command","").strip()
    if not name or not command: return "Richiesti: name, command."
    if BLOCKED_IMAGES.search(command):
        return "⛔ Comando bloccato."
    c, out = _run(["docker", "exec", name] + command.split(), timeout=30)
    icon = "✅" if c == 0 else "❌"
    return f"{icon} exec {name}: {command}\n\n{out[:2000]}"

HANDLERS = {
    "check_docker":     handle_check_docker,
    "list_containers":  handle_list_containers,
    "list_images":      handle_list_images,
    "run_container":    handle_run_container,
    "stop_container":   handle_stop_container,
    "container_logs":   handle_container_logs,
    "build_image":      handle_build_image,
    "exec_container":   handle_exec_container,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"check_docker","description":"Versione Docker e stato del daemon.","inputSchema":{"type":"object","properties":{}}},
    {"name":"list_containers","description":"Elenca container Docker attivi (o tutti con all=true).","inputSchema":{"type":"object","properties":{"all":{"type":"boolean","default":False}}}},
    {"name":"list_images","description":"Elenca le immagini Docker disponibili localmente.","inputSchema":{"type":"object","properties":{}}},
    {"name":"run_container","description":"Avvia un container Docker con porte, volumi e variabili d'ambiente.","inputSchema":{"type":"object","properties":{"image":{"type":"string"},"name":{"type":"string","default":""},"ports":{"type":"array","items":{"type":"string"},"default":[]},"volumes":{"type":"array","items":{"type":"string"},"default":[]},"env":{"type":"object","default":{}},"detach":{"type":"boolean","default":True},"rm":{"type":"boolean","default":False}},"required":["image"]}},
    {"name":"stop_container","description":"Ferma un container Docker per nome o ID.","inputSchema":{"type":"object","properties":{"name":{"type":"string"}},"required":["name"]}},
    {"name":"container_logs","description":"Mostra gli ultimi N log di un container Docker.","inputSchema":{"type":"object","properties":{"name":{"type":"string"},"tail":{"type":"integer","default":50}},"required":["name"]}},
    {"name":"build_image","description":"Costruisce un'immagine Docker da Dockerfile.","inputSchema":{"type":"object","properties":{"tag":{"type":"string","default":"prismalux:local"},"path":{"type":"string","default":""},"dockerfile":{"type":"string","default":""}}}},
    {"name":"exec_container","description":"Esegue un comando in un container Docker attivo.","inputSchema":{"type":"object","properties":{"name":{"type":"string"},"command":{"type":"string"}},"required":["name","command"]}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"docker","version":"1.0.0"},"capabilities":{"tools":{}}})
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

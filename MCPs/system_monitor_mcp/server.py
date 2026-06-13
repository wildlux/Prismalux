#!/usr/bin/env python3
"""
system_monitor_mcp — Monitoraggio risorse sistema per Prismalux
===============================================================
CPU, RAM, GPU (nvidia/AMD), processi chiave (Ollama/llama-server),
I/O disco e rete. Usa psutil se disponibile, fallback a /proc/.

Tools:
  cpu_mem         — CPU%, RAM usata/totale, swap
  gpu_usage       — VRAM e % GPU (nvidia-smi / rocm-smi)
  watch_processes — CPU/RAM dei processi Prismalux chiave
  disk_io         — spazio disco e I/O
  network_io      — traffico rete per interfaccia
  top_processes   — top N processi per memoria
"""
import sys, json, os, re, asyncio, subprocess, time
from pathlib import Path
from typing import Any

ROOT = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))

PRISM_PROCS = ["ollama", "llama-server", "llama.cpp", "Prismalux", "python3", "python"]

def _run(cmd: list[str], timeout: int = 5) -> tuple[int, str]:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return r.returncode, (r.stdout + r.stderr).strip()
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return -1, ""

def _proc_info() -> list[dict]:
    """Legge /proc per lista processi (fallback senza psutil)."""
    procs = []
    for pid_dir in Path("/proc").iterdir():
        if not pid_dir.name.isdigit(): continue
        try:
            name = (pid_dir / "comm").read_text().strip()
            status = (pid_dir / "status").read_text()
            vm_rss = re.search(r"VmRSS:\s+(\d+)", status)
            rss_kb = int(vm_rss.group(1)) if vm_rss else 0
            procs.append({"pid": int(pid_dir.name), "name": name, "rss_mb": rss_kb / 1024})
        except (PermissionError, FileNotFoundError):
            continue
    return procs

def _meminfo() -> dict:
    lines = Path("/proc/meminfo").read_text().splitlines()
    m = {}
    for l in lines:
        k, v = l.split(":", 1)
        m[k.strip()] = int(v.strip().split()[0])  # kB
    return m

def _cpu_pct(interval: float = 0.5) -> float:
    try:
        t1 = [int(x) for x in Path("/proc/stat").read_text().splitlines()[0].split()[1:]]
        time.sleep(interval)
        t2 = [int(x) for x in Path("/proc/stat").read_text().splitlines()[0].split()[1:]]
        idle1, idle2 = t1[3], t2[3]
        tot1, tot2   = sum(t1), sum(t2)
        return 100.0 * (1.0 - (idle2 - idle1) / max(tot2 - tot1, 1))
    except Exception:
        return -1.0

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_cpu_mem(args: dict) -> str:
    try:
        import psutil
        cpu = psutil.cpu_percent(interval=0.5)
        vm  = psutil.virtual_memory()
        sw  = psutil.swap_memory()
        lines = [f"⚡ CPU & Memoria\n"]
        lines.append(f"  CPU:   {cpu:.1f}%  (core: {psutil.cpu_count()})")
        lines.append(f"  RAM:   {vm.used/1e9:.1f} GB / {vm.total/1e9:.1f} GB ({vm.percent:.0f}%)")
        lines.append(f"  Swap:  {sw.used/1e9:.1f} GB / {sw.total/1e9:.1f} GB ({sw.percent:.0f}%)")
        if vm.percent > 85:
            lines.append("\n  🔴 RAM critica — considera di ridurre n_gpu_layers in llama-server")
        elif vm.percent > 70:
            lines.append("\n  🟡 RAM elevata")
        return "\n".join(lines)
    except ImportError:
        cpu = _cpu_pct()
        m   = _meminfo()
        total = m.get("MemTotal", 0)
        free  = m.get("MemAvailable", 0)
        used  = total - free
        pct   = used / max(total, 1) * 100
        lines = [f"⚡ CPU & Memoria (via /proc)\n"]
        lines.append(f"  CPU:  {cpu:.1f}%")
        lines.append(f"  RAM:  {used/1024:.0f} MB / {total/1024:.0f} MB ({pct:.0f}%)")
        swap_t = m.get("SwapTotal", 0)
        swap_f = m.get("SwapFree", 0)
        lines.append(f"  Swap: {(swap_t-swap_f)/1024:.0f} MB / {swap_t/1024:.0f} MB")
        return "\n".join(lines)

def handle_gpu_usage(args: dict) -> str:
    # NVIDIA
    c, out = _run(["nvidia-smi", "--query-gpu=name,utilization.gpu,memory.used,memory.total,temperature.gpu",
                   "--format=csv,noheader,nounits"])
    if c == 0 and out:
        lines = ["🎮 GPU (NVIDIA)\n"]
        for i, line in enumerate(out.splitlines()):
            parts = [p.strip() for p in line.split(",")]
            if len(parts) >= 5:
                name, util, mem_used, mem_total, temp = parts
                pct = int(util) if util.isdigit() else 0
                icon = "🔴" if pct > 90 else ("🟡" if pct > 60 else "✅")
                lines.append(f"  GPU {i}: {name}")
                lines.append(f"    {icon} Utilizzo: {util}%   VRAM: {mem_used}/{mem_total} MB   Temp: {temp}°C")
        return "\n".join(lines)
    # AMD ROCm
    c2, out2 = _run(["rocm-smi", "--showuse", "--showmeminfo", "vram"])
    if c2 == 0 and out2:
        return f"🎮 GPU (AMD ROCm)\n{out2[:600]}"
    # Intel
    c3, out3 = _run(["intel_gpu_top", "-l", "-o", "-"])
    if c3 == 0 and out3:
        return f"🎮 GPU (Intel)\n{out3[:400]}"
    return ("⚪ GPU non rilevata o driver non installato.\n"
            "  NVIDIA: sudo apt install nvidia-utils-XXX\n"
            "  AMD:    sudo apt install rocm-smi")

def handle_watch_processes(args: dict) -> str:
    keywords = args.get("names", PRISM_PROCS)
    if isinstance(keywords, str): keywords = [keywords]
    lines = [f"🔍 Processi chiave\n"]
    try:
        import psutil
        found = False
        for proc in psutil.process_iter(["pid","name","cmdline","cpu_percent","memory_info"]):
            try:
                name = proc.info["name"] or ""
                cmd  = " ".join(proc.info["cmdline"] or [])
                if not any(k.lower() in name.lower() or k.lower() in cmd.lower() for k in keywords):
                    continue
                found = True
                mem_mb = proc.info["memory_info"].rss / 1e6
                cpu    = proc.cpu_percent(interval=0.1)
                short  = cmd[:60] if cmd else name
                lines.append(f"  PID {proc.info['pid']:6}  CPU {cpu:5.1f}%  RAM {mem_mb:7.0f} MB  {short}")
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        if not found:
            lines.append("  Nessun processo Prismalux attivo.")
    except ImportError:
        procs = _proc_info()
        found = [p for p in procs if any(k.lower() in p["name"].lower() for k in keywords)]
        if not found:
            lines.append("  Nessun processo Prismalux attivo.")
        for p in sorted(found, key=lambda x: -x["rss_mb"])[:10]:
            lines.append(f"  PID {p['pid']:6}  RAM {p['rss_mb']:7.0f} MB  {p['name']}")
    return "\n".join(lines)

def handle_disk_io(args: dict) -> str:
    lines = ["💿 Disco\n"]
    try:
        import psutil
        for part in psutil.disk_partitions():
            try:
                usage = psutil.disk_usage(part.mountpoint)
                pct   = usage.percent
                icon  = "🔴" if pct > 90 else ("🟡" if pct > 75 else "✅")
                lines.append(f"  {icon} {part.mountpoint:<15} {usage.used/1e9:.0f}G/{usage.total/1e9:.0f}G ({pct:.0f}%)")
            except PermissionError:
                continue
        io = psutil.disk_io_counters()
        if io:
            lines.append(f"\n  I/O totale: R={io.read_bytes/1e9:.1f}GB  W={io.write_bytes/1e9:.1f}GB")
    except ImportError:
        c, out = _run(["df", "-h", "--output=target,used,size,pcent"])
        lines.append(out[:600])
    # Dimensione cartelle Prismalux
    c, out = _run(["du", "-sh", str(ROOT), str(Path.home() / ".prismalux")])
    if c == 0:
        lines.append("\nDimensione Prismalux:")
        for l in out.splitlines(): lines.append(f"  {l}")
    return "\n".join(lines)

def handle_network_io(args: dict) -> str:
    lines = ["🌐 Rete\n"]
    try:
        import psutil
        net = psutil.net_io_counters(pernic=True)
        for iface, stats in net.items():
            if iface == "lo": continue
            lines.append(f"  {iface:<12}  ↓{stats.bytes_recv/1e6:.0f}MB  ↑{stats.bytes_sent/1e6:.0f}MB  "
                         f"err:{stats.errin+stats.errout}  drop:{stats.dropin+stats.dropout}")
        # Connessioni Prismalux
        lines.append("\nPorte Prismalux in ascolto:")
        for port, name in [(11434,"Ollama"),(8081,"llama-server"),(8092,"OpenCode"),
                           (11600,"WAN Compute"),(11500,"LAN server"),(5000,"Flask")]:
            conns = [c for c in psutil.net_connections() if c.laddr.port == port]
            status = f"✅ LISTEN ({len(conns)} conn.)" if conns else "⚪ inattivo"
            lines.append(f"  :{port:<6} {name:<14} {status}")
    except ImportError:
        c, out = _run(["ss", "-tlnp"])
        lines.append(out[:600])
    return "\n".join(lines)

def handle_top_processes(args: dict) -> str:
    n = min(int(args.get("n", 15)), 50)
    lines = [f"📊 Top {n} processi per RAM\n"]
    try:
        import psutil
        procs = []
        for p in psutil.process_iter(["pid","name","memory_info","cpu_percent"]):
            try:
                procs.append((p.info["memory_info"].rss, p.info))
            except (psutil.NoSuchProcess, psutil.AccessDenied): continue
        procs.sort(key=lambda x: -x[0])
        lines.append(f"  {'PID':>7}  {'RAM MB':>8}  {'CPU%':>6}  Nome")
        lines.append("  " + "─" * 50)
        for rss, info in procs[:n]:
            lines.append(f"  {info['pid']:>7}  {rss/1e6:>8.0f}  {info['cpu_percent']:>6.1f}  {info['name'][:40]}")
    except ImportError:
        procs = sorted(_proc_info(), key=lambda x: -x["rss_mb"])
        lines.append(f"  {'PID':>7}  {'RAM MB':>8}  Nome")
        for p in procs[:n]:
            lines.append(f"  {p['pid']:>7}  {p['rss_mb']:>8.0f}  {p['name'][:40]}")
    return "\n".join(lines)

HANDLERS = {
    "cpu_mem":          handle_cpu_mem,
    "gpu_usage":        handle_gpu_usage,
    "watch_processes":  handle_watch_processes,
    "disk_io":          handle_disk_io,
    "network_io":       handle_network_io,
    "top_processes":    handle_top_processes,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"cpu_mem","description":"CPU%, RAM usata/totale, swap. Avviso se RAM > 85%.","inputSchema":{"type":"object","properties":{}}},
    {"name":"gpu_usage","description":"VRAM e utilizzo GPU (nvidia-smi / rocm-smi). Utile con modelli LLM grandi.","inputSchema":{"type":"object","properties":{}}},
    {"name":"watch_processes","description":"CPU e RAM dei processi chiave: Ollama, llama-server, Prismalux, Python MCPs.","inputSchema":{"type":"object","properties":{"names":{"type":"array","items":{"type":"string"},"default":["ollama","llama-server","Prismalux"]}}}},
    {"name":"disk_io","description":"Spazio disco per partizione, I/O totale, dimensione cartelle Prismalux.","inputSchema":{"type":"object","properties":{}}},
    {"name":"network_io","description":"Traffico rete per interfaccia e stato porte Prismalux (Ollama, LAN, WAN).","inputSchema":{"type":"object","properties":{}}},
    {"name":"top_processes","description":"Top N processi per consumo RAM nel sistema.","inputSchema":{"type":"object","properties":{"n":{"type":"integer","default":15}}}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"system-monitor","version":"1.0.0"},"capabilities":{"tools":{}}})
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

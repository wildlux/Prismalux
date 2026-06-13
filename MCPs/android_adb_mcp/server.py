#!/usr/bin/env python3
"""
android_adb_mcp — Gestione dispositivo Android via ADB
=======================================================
Permette di installare APK, leggere logcat, fare screenshot
e gestire file sul dispositivo Android — tutto senza uscire
dalla sessione Claude Code o dall'app Prismalux.

Tools esposti:
  list_devices    — lista dispositivi ADB connessi
  install_apk     — installa PrismaluxMobile.apk (o APK specificato)
  read_logcat     — legge logcat con filtri (tag, livello, righe)
  screenshot      — cattura screenshot e salva in locale
  push_file       — invia file locale → dispositivo
  pull_file       — riceve file dispositivo → locale
  shell_safe      — esegue comando adb shell (whitelist sicura)
  get_app_info    — info sull'app Prismalux installata

Uso in ~/.claude/settings.json:
  { "mcpServers": { "android-adb": {
      "type": "stdio", "command": "python3",
      "args": [".../MCPs/android_adb_mcp/server.py"]
  }}}
"""

import sys, json, os, subprocess, asyncio, logging, tempfile, shlex
from pathlib import Path
from typing import Any

logging.basicConfig(level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL","WARNING")))
logger = logging.getLogger(__name__)

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
APK_DEF = ROOT / "ANDROID" / "PrismaluxMobile.apk"
PKG     = "com.prismalux.mobile"

# ─── Tool definitions ────────────────────────────────────────────────────────

TOOL_DEFS: list[dict[str, Any]] = [
    {
        "name": "list_devices",
        "description": "Elenca dispositivi Android connessi via ADB (USB o WiFi). Mostra seriale, modello e stato.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "install_apk",
        "description": "Installa l'APK Prismalux sul dispositivo ADB connesso. Usa -r per reinstallare senza perdere dati.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "apk_path": {
                    "type": "string",
                    "description": "Path dell'APK (default: ANDROID/PrismaluxMobile.apk)",
                    "default": "",
                },
                "device": {
                    "type": "string",
                    "description": "Seriale dispositivo (se collegati più dispositivi). Default: unico connesso.",
                    "default": "",
                },
                "keep_data": {
                    "type": "boolean",
                    "description": "Se true, usa -r (reinstall mantenendo dati). Default: true",
                    "default": True,
                },
            },
        },
    },
    {
        "name": "read_logcat",
        "description": "Legge i log dell'app Prismalux dal dispositivo Android. Filtra per tag o livello.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "tag": {
                    "type": "string",
                    "description": "Tag da filtrare (es. 'Prismalux', 'Qt', vuoto = tutti)",
                    "default": "Prismalux",
                },
                "level": {
                    "type": "string",
                    "description": "Livello minimo: V/D/I/W/E (default: D)",
                    "default": "D",
                },
                "lines": {
                    "type": "integer",
                    "description": "Numero di righe da restituire (default: 50)",
                    "default": 50,
                },
                "device": {"type": "string", "default": ""},
            },
        },
    },
    {
        "name": "screenshot",
        "description": "Cattura screenshot del dispositivo Android e salva in locale.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "save_path": {
                    "type": "string",
                    "description": "Path locale dove salvare (default: ~/Desktop/screenshot_android.png)",
                    "default": "",
                },
                "device": {"type": "string", "default": ""},
            },
        },
    },
    {
        "name": "push_file",
        "description": "Invia un file dal computer al dispositivo Android.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "local":  {"type": "string", "description": "Path file locale"},
                "remote": {"type": "string", "description": "Path destinazione nel dispositivo (es. /sdcard/Download/)"},
                "device": {"type": "string", "default": ""},
            },
            "required": ["local", "remote"],
        },
    },
    {
        "name": "pull_file",
        "description": "Riceve un file dal dispositivo Android al computer.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "remote": {"type": "string", "description": "Path file nel dispositivo"},
                "local":  {"type": "string", "description": "Path locale destinazione (default: ~/Desktop/)"},
                "device": {"type": "string", "default": ""},
            },
            "required": ["remote"],
        },
    },
    {
        "name": "shell_safe",
        "description": "Esegue un comando ADB shell sicuro (whitelist). Comandi consentiti: pm, am, getprop, dumpsys, input, settings.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "command": {
                    "type": "string",
                    "description": "Comando shell (es. 'pm list packages | grep prisma', 'getprop ro.product.model')",
                },
                "device": {"type": "string", "default": ""},
            },
            "required": ["command"],
        },
    },
    {
        "name": "get_app_info",
        "description": "Mostra informazioni sull'app Prismalux installata: versione, storage, permessi attivi.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "device": {"type": "string", "default": ""},
            },
        },
    },
]

# ─── Helpers ─────────────────────────────────────────────────────────────────

def _adb(args: list[str], device: str = "", timeout: int = 30) -> tuple[int, str]:
    cmd = ["adb"]
    if device:
        cmd += ["-s", device]
    cmd += args
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return r.returncode, (r.stdout + r.stderr).strip()
    except subprocess.TimeoutExpired:
        return -1, f"Timeout ({timeout}s)"
    except FileNotFoundError:
        return -1, "ADB non trovato. Installa android-tools-adb."

def _check_device(device: str) -> tuple[bool, str]:
    """Verifica che ci sia almeno un dispositivo connesso."""
    code, out = _adb(["devices"])
    lines = [l for l in out.splitlines() if "\t" in l and "offline" not in l]
    devices = [l.split("\t")[0] for l in lines if l.split("\t")[1].strip() == "device"]
    if not devices:
        return False, "Nessun dispositivo ADB connesso. Collega il telefono con debug USB abilitato."
    if device and device not in devices:
        return False, f"Dispositivo '{device}' non trovato. Disponibili: {', '.join(devices)}"
    return True, devices[0] if not device else device

# Whitelist comandi adb shell sicuri
_SAFE_PREFIXES = ("pm ", "am ", "getprop", "dumpsys ", "input ", "settings ", "wm ", "cmd ")

# ─── Handlers ────────────────────────────────────────────────────────────────

def handle_list_devices(args: dict) -> str:
    code, out = _adb(["devices", "-l"])
    if code != 0:
        return f"Errore ADB: {out}"
    lines = out.splitlines()
    devices = [l for l in lines if "\t" in l or ("device" in l and "List" not in l)]
    if not devices:
        return "Nessun dispositivo connesso.\nAbilita il debug USB sul telefono: Impostazioni → Info → Numero build (7x) → Opzioni sviluppatore → Debug USB"

    result = ["Dispositivi ADB connessi:\n"]
    for line in lines[1:]:  # skip "List of devices"
        if "\t" in line:
            parts = line.split()
            serial = parts[0]
            state  = parts[1] if len(parts) > 1 else "?"
            model  = next((p.split(":")[1] for p in parts if p.startswith("model:")), "?")
            result.append(f"  {'✅' if state=='device' else '⚠️ '} {serial:25s} {state:10s} {model}")
    return "\n".join(result)


def handle_install_apk(args: dict) -> str:
    apk_path  = args.get("apk_path", "").strip() or str(APK_DEF)
    device_in = args.get("device", "").strip()
    keep_data = bool(args.get("keep_data", True))

    ok, device = _check_device(device_in)
    if not ok:
        return device

    apk = Path(apk_path)
    if not apk.exists():
        return f"APK non trovato: {apk}\nCompila prima con: cmake --build ANDROID/android_app/build/ (build Android reale)"

    flags = ["-r"] if keep_data else []
    code, out = _adb(["install"] + flags + [str(apk)], device=device, timeout=60)
    if code == 0 and "Success" in out:
        size_mb = apk.stat().st_size / 1_048_576
        return f"✅ Installato {apk.name} ({size_mb:.1f} MB) su dispositivo {device}.\n{out}"
    return f"❌ Installazione fallita:\n{out}"


def handle_read_logcat(args: dict) -> str:
    tag       = args.get("tag", "Prismalux").strip()
    level     = args.get("level", "D").upper().strip()
    lines_n   = int(args.get("lines", 50))
    device_in = args.get("device", "").strip()

    ok, device = _check_device(device_in)
    if not ok:
        return device

    if level not in "VDIWEF":
        level = "D"

    adb_args = ["logcat", "-d", "-v", "time"]
    if tag:
        adb_args += [f"{tag}:{level}", "*:S"]
    else:
        adb_args += [f"*:{level}"]

    code, out = _adb(adb_args, device=device, timeout=15)
    if code != 0:
        return f"Errore logcat: {out}"

    log_lines = [l for l in out.splitlines() if l.strip()][-lines_n:]
    if not log_lines:
        return f"Nessun log con tag='{tag}' level={level}."
    return f"Logcat [{tag or '*'}:{level}] (ultime {len(log_lines)} righe):\n" + "\n".join(log_lines)


def handle_screenshot(args: dict) -> str:
    save_path = args.get("save_path", "").strip()
    device_in = args.get("device", "").strip()

    ok, device = _check_device(device_in)
    if not ok:
        return device

    if not save_path:
        save_path = str(Path.home() / "Desktop" / "screenshot_android.png")

    code, out = _adb(
        ["exec-out", "screencap", "-p"],
        device=device, timeout=20
    )
    # exec-out non funziona bene con text mode — usiamo pipe diretta
    try:
        cmd = ["adb"]
        if device:
            cmd += ["-s", device]
        cmd += ["exec-out", "screencap", "-p"]
        result = subprocess.run(cmd, capture_output=True, timeout=20)
        if result.returncode == 0 and result.stdout:
            Path(save_path).write_bytes(result.stdout)
            size_kb = len(result.stdout) // 1024
            return f"✅ Screenshot salvato in: {save_path} ({size_kb} KB)"
        return f"❌ Errore screenshot: {result.stderr.decode(errors='replace')}"
    except Exception as e:
        return f"❌ Errore: {e}"


def handle_push_file(args: dict) -> str:
    local     = args.get("local", "").strip()
    remote    = args.get("remote", "").strip()
    device_in = args.get("device", "").strip()

    if not local or not remote:
        return "Specifica sia 'local' che 'remote'."

    ok, device = _check_device(device_in)
    if not ok:
        return device

    if not Path(local).exists():
        return f"File locale non trovato: {local}"

    code, out = _adb(["push", local, remote], device=device, timeout=60)
    return ("✅ " if code == 0 else "❌ ") + out


def handle_pull_file(args: dict) -> str:
    remote    = args.get("remote", "").strip()
    local     = args.get("local", "").strip() or str(Path.home() / "Desktop")
    device_in = args.get("device", "").strip()

    if not remote:
        return "Specifica il path 'remote' nel dispositivo."

    ok, device = _check_device(device_in)
    if not ok:
        return device

    code, out = _adb(["pull", remote, local], device=device, timeout=60)
    return ("✅ " if code == 0 else "❌ ") + out


def handle_shell_safe(args: dict) -> str:
    command   = args.get("command", "").strip()
    device_in = args.get("device", "").strip()

    if not command:
        return "Specifica un comando shell."

    # Sicurezza: solo whitelist
    safe = any(command.startswith(p) for p in _SAFE_PREFIXES)
    if not safe:
        allowed = ", ".join(p.strip() for p in _SAFE_PREFIXES)
        return (
            f"❌ Comando non consentito: '{command}'.\n"
            f"Comandi sicuri consentiti: {allowed}"
        )

    ok, device = _check_device(device_in)
    if not ok:
        return device

    code, out = _adb(["shell"] + shlex.split(command), device=device, timeout=20)
    return out if out else "(nessun output)"


def handle_get_app_info(args: dict) -> str:
    device_in = args.get("device", "").strip()
    ok, device = _check_device(device_in)
    if not ok:
        return device

    lines = []

    # Verifica installazione
    code, out = _adb(["shell", "pm", "list", "packages", PKG], device=device)
    if PKG not in out:
        return f"⚠️  App '{PKG}' non installata sul dispositivo."

    lines.append(f"📱 App: {PKG}\n")

    # Versione
    code, vout = _adb(["shell", "dumpsys", "package", PKG], device=device, timeout=10)
    for line in vout.splitlines():
        if "versionName" in line or "versionCode" in line:
            lines.append("  " + line.strip())

    # Storage
    code, sout = _adb(["shell", "du", "-sh", f"/data/data/{PKG}"], device=device)
    if code == 0:
        lines.append(f"  Storage:    {sout.split()[0] if sout else '?'}")

    # Permessi concessi
    code, pout = _adb(["shell", "dumpsys", "package", PKG], device=device, timeout=10)
    perms = [l.strip() for l in pout.splitlines()
             if "granted=true" in l and "android.permission" in l]
    if perms:
        lines.append(f"\n  Permessi concessi ({len(perms)}):")
        for p in perms[:10]:
            pname = p.split(":")[0].replace("android.permission.", "")
            lines.append(f"    ✅ {pname}")

    return "\n".join(lines)


HANDLERS = {
    "list_devices": handle_list_devices,
    "install_apk":  handle_install_apk,
    "read_logcat":  handle_read_logcat,
    "screenshot":   handle_screenshot,
    "push_file":    handle_push_file,
    "pull_file":    handle_pull_file,
    "shell_safe":   handle_shell_safe,
    "get_app_info": handle_get_app_info,
}

# ─── JSON-RPC 2.0 loop ───────────────────────────────────────────────────────

def _rpc_result(req_id, result):
    return json.dumps({"jsonrpc":"2.0","id":req_id,"result":result})
def _rpc_error(req_id, code, message):
    return json.dumps({"jsonrpc":"2.0","id":req_id,"error":{"code":code,"message":message}})

async def _main_async():
    loop = asyncio.get_event_loop()
    async def readline(): return await loop.run_in_executor(None, sys.stdin.readline)
    while True:
        line = await readline()
        if not line: break
        line = line.strip()
        if not line: continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError as e:
            sys.stdout.write(_rpc_error(None,-32700,f"Parse error: {e}")+"\n"); sys.stdout.flush(); continue
        req_id = req.get("id"); method = req.get("method",""); params = req.get("params",{})
        if method == "initialize":
            resp = _rpc_result(req_id,{"protocolVersion":"2024-11-05","serverInfo":{"name":"android-adb","version":"1.0.0"},"capabilities":{"tools":{}}})
        elif method == "tools/list":
            resp = _rpc_result(req_id,{"tools":TOOL_DEFS})
        elif method == "tools/call":
            name = params.get("name",""); targs = params.get("arguments",{})
            handler = HANDLERS.get(name)
            if not handler:
                resp = _rpc_error(req_id,-32601,f"Tool sconosciuto: {name}")
            else:
                try:
                    text = await asyncio.to_thread(handler, targs)
                    resp = _rpc_result(req_id,{"content":[{"type":"text","text":text}],"isError":False})
                except Exception as e:
                    resp = _rpc_result(req_id,{"content":[{"type":"text","text":f"Errore: {e}"}],"isError":True})
        elif method == "notifications/initialized":
            continue
        else:
            resp = _rpc_error(req_id,-32601,f"Metodo sconosciuto: {method}")
        sys.stdout.write(resp+"\n"); sys.stdout.flush()

def main(): asyncio.run(_main_async())
if __name__ == "__main__": main()

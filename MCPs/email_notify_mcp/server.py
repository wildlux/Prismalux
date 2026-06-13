#!/usr/bin/env python3
"""
email_notify_mcp — Notifiche email per eventi Prismalux
========================================================
Invia email SMTP per risultati build, alert sistema, report.
Config in ~/.prismalux/email_config.json.
Supporta Gmail (App Password), SMTP generico, Outlook.

Tools:
  configure          — salva configurazione SMTP
  test_connection    — verifica connessione SMTP
  send_notification  — invia email con soggetto e corpo
  notify_build       — invia risultato build (pass/fail + log)
  notify_alert       — alert sistema (RAM, GPU, disco)
  list_history       — ultimi N messaggi inviati
"""
import sys, json, os, re, asyncio, smtplib, ssl
from pathlib import Path
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from datetime import datetime
from typing import Any

ROOT        = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
CONFIG_FILE = Path.home() / ".prismalux" / "email_config.json"
HISTORY_FILE= Path.home() / ".prismalux" / "email_history.json"

DEFAULT_TO = "wildlux@gmail.com"

def _load_cfg() -> dict:
    if CONFIG_FILE.exists():
        try: return json.loads(CONFIG_FILE.read_text())
        except: pass
    return {}

def _save_cfg(cfg: dict) -> None:
    CONFIG_FILE.parent.mkdir(parents=True, exist_ok=True)
    CONFIG_FILE.write_text(json.dumps(cfg, indent=2))

def _load_history() -> list:
    if HISTORY_FILE.exists():
        try: return json.loads(HISTORY_FILE.read_text())
        except: pass
    return []

def _append_history(entry: dict) -> None:
    h = _load_history()
    h.append(entry)
    h = h[-100:]  # tieni ultimi 100
    HISTORY_FILE.parent.mkdir(parents=True, exist_ok=True)
    HISTORY_FILE.write_text(json.dumps(h, indent=2))

def _send_email(to: str, subject: str, body: str) -> tuple[bool, str]:
    cfg = _load_cfg()
    if not cfg:
        return False, "Configurazione SMTP mancante. Usa configure() prima."
    host     = cfg.get("smtp_host","smtp.gmail.com")
    port     = int(cfg.get("smtp_port", 587))
    user     = cfg.get("username","")
    password = cfg.get("password","")
    from_addr= cfg.get("from_addr", user)
    if not user or not password:
        return False, "Username/password SMTP mancanti. Usa configure()."
    msg = MIMEMultipart("alternative")
    msg["Subject"] = subject
    msg["From"]    = from_addr
    msg["To"]      = to
    msg.attach(MIMEText(body, "plain", "utf-8"))
    try:
        ctx = ssl.create_default_context()
        if port == 465:
            with smtplib.SMTP_SSL(host, port, context=ctx, timeout=15) as s:
                s.login(user, password)
                s.sendmail(from_addr, [to], msg.as_string())
        else:
            with smtplib.SMTP(host, port, timeout=15) as s:
                s.ehlo()
                s.starttls(context=ctx)
                s.login(user, password)
                s.sendmail(from_addr, [to], msg.as_string())
        return True, "OK"
    except Exception as e:
        return False, str(e)

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_configure(args: dict) -> str:
    smtp_host = args.get("smtp_host","smtp.gmail.com").strip()
    smtp_port = int(args.get("smtp_port", 587))
    username  = args.get("username","").strip()
    password  = args.get("password","").strip()
    from_addr = args.get("from_addr", username).strip()
    to_default= args.get("to_default", DEFAULT_TO).strip()
    if not username or not password:
        return ("Richiesti: username, password.\n\n"
                "Per Gmail usa App Password (non la password account):\n"
                "  https://myaccount.google.com → Sicurezza → Password app\n\n"
                "Esempio:\n"
                "  configure(smtp_host='smtp.gmail.com', smtp_port=587,\n"
                "            username='tuo@gmail.com', password='xxxx xxxx xxxx xxxx')")
    cfg = {
        "smtp_host": smtp_host, "smtp_port": smtp_port,
        "username": username, "password": password,
        "from_addr": from_addr or username, "to_default": to_default
    }
    _save_cfg(cfg)
    return (f"✅ Config SMTP salvata: {username} → {smtp_host}:{smtp_port}\n"
            f"  Email default destinatario: {to_default}\n"
            f"  Verifica con test_connection()")

def handle_test_connection(args: dict) -> str:
    cfg  = _load_cfg()
    if not cfg: return "Configurazione mancante. Usa configure()."
    to   = args.get("to", cfg.get("to_default", DEFAULT_TO))
    ok, msg = _send_email(to, "[Prismalux] Test connessione SMTP",
                          f"Test inviato il {datetime.now().isoformat()}\n\nSe ricevi questo messaggio, la configurazione SMTP funziona.")
    if ok:
        _append_history({"ts": datetime.now().isoformat(), "to": to, "subject": "Test SMTP", "ok": True})
        return f"✅ Email di test inviata a {to}"
    return f"❌ Errore SMTP: {msg}"

def handle_send_notification(args: dict) -> str:
    cfg     = _load_cfg()
    subject = args.get("subject","Notifica Prismalux").strip()
    body    = args.get("body","").strip()
    to      = args.get("to", cfg.get("to_default", DEFAULT_TO)).strip()
    if not body: return "Specifica 'body'."
    ok, msg = _send_email(to, subject, body)
    _append_history({"ts": datetime.now().isoformat(), "to": to, "subject": subject, "ok": ok})
    return f"{'✅' if ok else '❌'} Email '{subject}' → {to}: {msg}"

def handle_notify_build(args: dict) -> str:
    status    = args.get("status","unknown")  # "pass" | "fail"
    log_lines = args.get("log_lines", [])
    cfg       = _load_cfg()
    to        = args.get("to", cfg.get("to_default", DEFAULT_TO))
    ts        = datetime.now().strftime("%Y-%m-%d %H:%M")
    icon      = "✅" if status == "pass" else "❌"
    subject   = f"[Prismalux Build] {icon} {status.upper()} — {ts}"
    body_lines = [f"Build Prismalux — {ts}", f"Stato: {status.upper()}", "", "─" * 40]
    if log_lines:
        body_lines.append("Log (ultimi 50 righe):")
        lines_list = log_lines if isinstance(log_lines, list) else str(log_lines).splitlines()
        body_lines.extend(lines_list[-50:])
    body = "\n".join(body_lines)
    ok, msg = _send_email(to, subject, body)
    _append_history({"ts": datetime.now().isoformat(), "to": to, "subject": subject, "ok": ok})
    return f"{'✅' if ok else '❌'} Notifica build {status} inviata a {to}: {msg}"

def handle_notify_alert(args: dict) -> str:
    alert_type = args.get("type","generic")  # "ram", "gpu", "disk", "generic"
    value      = args.get("value","")
    threshold  = args.get("threshold","")
    cfg        = _load_cfg()
    to         = args.get("to", cfg.get("to_default", DEFAULT_TO))
    ts         = datetime.now().strftime("%Y-%m-%d %H:%M")
    subject    = f"[Prismalux ALERT] {alert_type.upper()} — {ts}"
    body = (f"ALERT Prismalux — {ts}\n"
            f"Tipo: {alert_type}\n"
            f"Valore attuale: {value}\n"
            f"Soglia: {threshold}\n\n"
            f"Controlla il sistema con system_monitor_mcp → cpu_mem()")
    ok, msg = _send_email(to, subject, body)
    _append_history({"ts": datetime.now().isoformat(), "to": to, "subject": subject, "ok": ok})
    return f"{'✅' if ok else '❌'} Alert {alert_type} inviato: {msg}"

def handle_list_history(args: dict) -> str:
    n = int(args.get("n", 20))
    h = _load_history()
    if not h: return "Nessuna email inviata ancora."
    lines = [f"📧 Ultimi {min(n, len(h))} messaggi inviati\n"]
    for entry in reversed(h[-n:]):
        icon = "✅" if entry.get("ok") else "❌"
        lines.append(f"  {icon} [{entry.get('ts','?')[:16]}] {entry.get('to','?')}")
        lines.append(f"       {entry.get('subject','?')[:60]}")
    return "\n".join(lines)

HANDLERS = {
    "configure":          handle_configure,
    "test_connection":    handle_test_connection,
    "send_notification":  handle_send_notification,
    "notify_build":       handle_notify_build,
    "notify_alert":       handle_notify_alert,
    "list_history":       handle_list_history,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"configure","description":"Salva la configurazione SMTP (server, porta, credenziali). Supporta Gmail App Password.","inputSchema":{"type":"object","properties":{"smtp_host":{"type":"string","default":"smtp.gmail.com"},"smtp_port":{"type":"integer","default":587},"username":{"type":"string"},"password":{"type":"string"},"from_addr":{"type":"string","default":""},"to_default":{"type":"string","default":"wildlux@gmail.com"}},"required":["username","password"]}},
    {"name":"test_connection","description":"Invia un'email di test per verificare che SMTP funzioni.","inputSchema":{"type":"object","properties":{"to":{"type":"string","default":""}}}},
    {"name":"send_notification","description":"Invia un'email di notifica generica.","inputSchema":{"type":"object","properties":{"subject":{"type":"string","default":"Notifica Prismalux"},"body":{"type":"string"},"to":{"type":"string","default":""}},"required":["body"]}},
    {"name":"notify_build","description":"Notifica email con risultato build (pass/fail) e log opzionale.","inputSchema":{"type":"object","properties":{"status":{"type":"string","enum":["pass","fail"],"default":"pass"},"log_lines":{"type":"array","items":{"type":"string"},"default":[]},"to":{"type":"string","default":""}}}},
    {"name":"notify_alert","description":"Invia alert sistema (RAM/GPU/disco over threshold) via email.","inputSchema":{"type":"object","properties":{"type":{"type":"string","default":"generic"},"value":{"type":"string","default":""},"threshold":{"type":"string","default":""},"to":{"type":"string","default":""}}}},
    {"name":"list_history","description":"Mostra gli ultimi N messaggi email inviati con stato.","inputSchema":{"type":"object","properties":{"n":{"type":"integer","default":20}}}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"email-notify","version":"1.0.0"},"capabilities":{"tools":{}}})
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

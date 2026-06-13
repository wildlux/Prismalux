#!/usr/bin/env python3
"""
translation_mcp — Auto-traduzione stringhe Qt .ts con LibreTranslate
=====================================================================
Traduce stringhe non tradotte nei file .ts Qt6 usando LibreTranslate
(auto-hosted su localhost:5000 o istanza pubblica come fallback).
Nessuna API key richiesta per l'istanza locale.

Tools:
  check_libretranslate  — verifica disponibilità LibreTranslate
  list_languages        — lingue supportate
  translate_string      — traduce una singola stringa
  translate_ts_file     — auto-traduce le stringhe unfinished in un .ts
  batch_translate       — traduce lista di stringhe
  show_config           — mostra configurazione URL LibreTranslate
"""
import sys, json, os, re, asyncio, time
import urllib.request, urllib.error
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
I18N    = ROOT / "gui" / "i18n"
CONFIG  = Path.home() / ".prismalux" / "translation_config.json"

LT_LOCAL  = "http://localhost:5000"
LT_PUBLIC = "https://libretranslate.com"

def _load_config() -> dict:
    if CONFIG.exists():
        try: return json.loads(CONFIG.read_text())
        except: pass
    return {"url": LT_LOCAL, "api_key": ""}

def _lt_url() -> str:
    return _load_config().get("url", LT_LOCAL)

def _http_post(url: str, payload: dict, timeout: float = 10.0) -> dict | None:
    data = json.dumps(payload).encode()
    req  = urllib.request.Request(
        url, data=data, method="POST",
        headers={"Content-Type": "application/json"}
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read())
    except Exception:
        return None

def _translate(text: str, source: str, target: str) -> str | None:
    cfg  = _load_config()
    base = cfg.get("url", LT_LOCAL)
    key  = cfg.get("api_key", "")
    payload = {"q": text, "source": source, "target": target, "format": "text"}
    if key: payload["api_key"] = key
    result = _http_post(f"{base}/translate", payload, timeout=15.0)
    if result and "translatedText" in result:
        return result["translatedText"]
    # Fallback: libretranslate.com pubblico (rate limited)
    if base != LT_PUBLIC:
        result2 = _http_post(f"{LT_PUBLIC}/translate",
                              {**payload, "api_key": ""}, timeout=10.0)
        if result2 and "translatedText" in result2:
            return result2["translatedText"]
    return None

def _ts_files() -> list[Path]:
    return sorted([f for f in I18N.glob("*.ts")
                   if "build" not in str(f) and "CMakeFiles" not in str(f)])

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_check_libretranslate(args: dict) -> str:
    base = _lt_url()
    lines = [f"🌐 LibreTranslate\n  URL configurato: {base}\n"]
    try:
        req = urllib.request.Request(f"{base}/languages",
                                     headers={"User-Agent": "prismalux-translation/1.0"})
        with urllib.request.urlopen(req, timeout=5) as r:
            langs = json.loads(r.read())
            lines.append(f"  ✅ Online — {len(langs)} lingue disponibili")
            codes = [l["code"] for l in langs[:10]]
            lines.append(f"  Lingue: {', '.join(codes)}")
    except Exception as e:
        lines.append(f"  ❌ Non raggiungibile: {e}")
        lines.append("\nPer avviare LibreTranslate in locale:")
        lines.append("  pip install libretranslate")
        lines.append("  libretranslate --host 0.0.0.0 --port 5000")
        lines.append("  # oppure con Docker:")
        lines.append("  docker run -p 5000:5000 libretranslate/libretranslate")
    return "\n".join(lines)

def handle_list_languages(args: dict) -> str:
    base = _lt_url()
    try:
        req = urllib.request.Request(f"{base}/languages")
        with urllib.request.urlopen(req, timeout=5) as r:
            langs = json.loads(r.read())
        lines = [f"🌍 Lingue disponibili su {base}\n"]
        for l in langs:
            lines.append(f"  {l['code']:<6} {l['name']}")
        return "\n".join(lines)
    except Exception as e:
        return f"❌ Impossibile ottenere la lista: {e}"

def handle_translate_string(args: dict) -> str:
    text   = args.get("text","").strip()
    target = args.get("target_lang","en").strip()
    source = args.get("source_lang","it").strip()
    if not text: return "Specifica 'text'."
    result = _translate(text, source, target)
    if result:
        return f"'{text}'\n  → [{target}] '{result}'"
    return "❌ Traduzione fallita. Verifica che LibreTranslate sia attivo con check_libretranslate()."

def handle_translate_ts_file(args: dict) -> str:
    ts_file     = args.get("ts_file","").strip()
    target_lang = args.get("target_lang","en").strip()
    source_lang = args.get("source_lang","it").strip()
    dry_run     = bool(args.get("dry_run", True))
    delay       = float(args.get("delay_s", 0.3))

    files = [Path(ts_file)] if ts_file else _ts_files()
    if not files: return f"Nessun .ts in {I18N}"

    results = []
    for path in files:
        try: tree = ET.parse(path)
        except ET.ParseError as e:
            results.append(f"❌ {path.name}: XML non valido — {e}")
            continue
        root = tree.getroot()
        done, failed, skipped = 0, 0, 0
        for context in root.findall("context"):
            for msg in context.findall("message"):
                trans_el   = msg.find("translation")
                if trans_el is None: continue
                trans_type = trans_el.get("type","")
                trans_text = trans_el.text or ""
                if trans_type != "unfinished" and trans_text.strip():
                    skipped += 1
                    continue
                source_text = msg.findtext("source","")
                if not source_text.strip():
                    continue
                translated = _translate(source_text, source_lang, target_lang)
                if translated:
                    if not dry_run:
                        trans_el.text = translated
                        if "type" in trans_el.attrib:
                            del trans_el.attrib["type"]
                    done += 1
                else:
                    failed += 1
                if delay > 0: time.sleep(delay)
        if not dry_run:
            ET.indent(tree, space="    ")
            tree.write(str(path), encoding="utf-8", xml_declaration=True)
        pfx = "[DRY RUN] " if dry_run else ""
        results.append(f"{pfx}✅ {path.name}: {done} tradotte, {failed} fallite, {skipped} già ok")
    return "\n".join(results)

def handle_batch_translate(args: dict) -> str:
    strings     = args.get("strings",[])
    target_lang = args.get("target_lang","en").strip()
    source_lang = args.get("source_lang","it").strip()
    if not strings or not isinstance(strings, list):
        return "Passa 'strings': lista di stringhe da tradurre."
    lines = [f"📦 Traduzione batch ({len(strings)} stringhe) → [{target_lang}]\n"]
    for text in strings[:50]:
        result = _translate(str(text), source_lang, target_lang)
        if result:
            lines.append(f"  ✅ '{text[:40]}'\n     → '{result[:60]}'")
        else:
            lines.append(f"  ❌ '{text[:40]}' — fallita")
    return "\n".join(lines)

def handle_show_config(args: dict) -> str:
    cfg  = _load_config()
    url  = cfg.get("url", LT_LOCAL)
    key  = cfg.get("api_key","")
    new_url = args.get("set_url","").strip()
    new_key = args.get("set_api_key","").strip()
    if new_url or new_key:
        if new_url: cfg["url"] = new_url
        if new_key: cfg["api_key"] = new_key
        CONFIG.parent.mkdir(parents=True, exist_ok=True)
        CONFIG.write_text(json.dumps(cfg, indent=2))
        return f"✅ Config salvata: URL={cfg['url']}"
    return (f"⚙️  Configurazione LibreTranslate\n"
            f"  URL:      {url}\n"
            f"  API Key:  {'***' if key else '(nessuna — ok per self-hosted)'}\n"
            f"  Config:   {CONFIG}\n\n"
            f"Modifica con show_config(set_url='http://...', set_api_key='...')")

HANDLERS = {
    "check_libretranslate": handle_check_libretranslate,
    "list_languages":       handle_list_languages,
    "translate_string":     handle_translate_string,
    "translate_ts_file":    handle_translate_ts_file,
    "batch_translate":      handle_batch_translate,
    "show_config":          handle_show_config,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"check_libretranslate","description":"Verifica disponibilità LibreTranslate locale (port 5000) o pubblico.","inputSchema":{"type":"object","properties":{}}},
    {"name":"list_languages","description":"Lingue disponibili su LibreTranslate.","inputSchema":{"type":"object","properties":{}}},
    {"name":"translate_string","description":"Traduce una singola stringa con LibreTranslate.","inputSchema":{"type":"object","properties":{"text":{"type":"string"},"target_lang":{"type":"string","default":"en"},"source_lang":{"type":"string","default":"it"}},"required":["text"]}},
    {"name":"translate_ts_file","description":"Auto-traduce le stringhe 'unfinished' in un file .ts Qt6 (dry_run=true di default).","inputSchema":{"type":"object","properties":{"ts_file":{"type":"string","default":""},"target_lang":{"type":"string","default":"en"},"source_lang":{"type":"string","default":"it"},"dry_run":{"type":"boolean","default":True},"delay_s":{"type":"number","default":0.3}}}},
    {"name":"batch_translate","description":"Traduce una lista di stringhe in una sola chiamata.","inputSchema":{"type":"object","properties":{"strings":{"type":"array","items":{"type":"string"}},"target_lang":{"type":"string","default":"en"},"source_lang":{"type":"string","default":"it"}},"required":["strings"]}},
    {"name":"show_config","description":"Mostra (o aggiorna) l'URL e la API key di LibreTranslate.","inputSchema":{"type":"object","properties":{"set_url":{"type":"string","default":""},"set_api_key":{"type":"string","default":""}}}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"translation","version":"1.0.0"},"capabilities":{"tools":{}}})
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

#!/usr/bin/env python3
"""
qt_i18n_mcp — Gestione internazionalizzazione Qt6 per Prismalux
================================================================
Esegue lupdate/lrelease, trova stringhe non tradotte, report copertura.
File .ts: gui/i18n/prismalux_it.ts, prismalux_en.ts

Tools:
  list_ts_files       — elenca file .ts nel progetto
  run_lupdate         — aggiorna .ts dai sorgenti C++ (tr())
  run_lrelease        — compila .ts → .qm
  find_untranslated   — stringhe type="unfinished" o vuote
  coverage_report     — percentuale stringhe tradotte per file
  add_translation     — aggiunge/aggiorna traduzione in un .ts
"""
import sys, json, os, re, asyncio, subprocess
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
GUI_DIR = ROOT / "gui"
I18N    = GUI_DIR / "i18n"

def _run(cmd: list[str], cwd: Path = ROOT) -> tuple[int, str]:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=str(cwd), timeout=60)
        return r.returncode, (r.stdout + r.stderr).strip()
    except (FileNotFoundError, subprocess.TimeoutExpired) as e:
        return -1, str(e)

def _ts_files() -> list[Path]:
    return sorted([f for f in I18N.glob("*.ts")
                   if "build" not in str(f) and "CMakeFiles" not in str(f)])

def _parse_ts(path: Path) -> tuple[int, int, list[dict]]:
    """Ritorna (totale, tradotte, lista_non_tradotte)."""
    try:
        tree = ET.parse(path)
    except ET.ParseError as e:
        return 0, 0, [{"error": str(e)}]
    root = tree.getroot()
    total, translated, untranslated = 0, 0, []
    for context in root.findall("context"):
        ctx_name = context.findtext("name", "")
        for msg in context.findall("message"):
            total += 1
            source = msg.findtext("source", "")
            trans_el = msg.find("translation")
            trans_type = trans_el.get("type", "") if trans_el is not None else ""
            trans_text = trans_el.text or "" if trans_el is not None else ""
            if trans_type == "unfinished" or not trans_text.strip():
                untranslated.append({"context": ctx_name, "source": source})
            else:
                translated += 1
    return total, translated, untranslated

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_list_ts_files(args: dict) -> str:
    files = _ts_files()
    if not files:
        return f"Nessun file .ts trovato in {I18N}"
    lines = [f"📋 File .ts in {I18N.relative_to(ROOT)}\n"]
    for f in files:
        total, translated, _ = _parse_ts(f)
        pct = (translated / total * 100) if total else 0
        icon = "✅" if pct == 100 else ("🟡" if pct > 50 else "🔴")
        lines.append(f"  {icon} {f.name:<30} {translated}/{total} ({pct:.0f}%)")
    return "\n".join(lines)

def handle_run_lupdate(args: dict) -> str:
    target = args.get("target", str(GUI_DIR))
    ts_filter = args.get("ts_file", "")
    if not _ts_files():
        return f"Nessun .ts in {I18N}. Crea prima i file .ts."
    for tool in ["lupdate-qt6", "lupdate6", "lupdate"]:
        code, ver = _run([tool, "-version"])
        if code == 0:
            ts_args = [str(ts_filter)] if ts_filter else [str(f) for f in _ts_files()]
            cmd = [tool, str(GUI_DIR), "-ts"] + ts_args
            c, out = _run(cmd, cwd=ROOT)
            status = "✅" if c == 0 else "❌"
            return f"{status} lupdate ({tool})\n\n{out[:1200]}"
    return ("lupdate non trovato.\n"
            "  sudo apt install qttools5-dev-tools   # o qt6-tools-dev-tools\n"
            "  oppure: sudo apt install qt6-l10n-tools")

def handle_run_lrelease(args: dict) -> str:
    ts_file = args.get("ts_file", "")
    files = [Path(ts_file)] if ts_file else _ts_files()
    if not files:
        return "Nessun file .ts trovato."
    results = []
    for tool in ["lrelease-qt6", "lrelease6", "lrelease"]:
        c, _ = _run([tool, "-version"])
        if c == 0:
            for f in files:
                qm = f.with_suffix(".qm")
                c2, out = _run([tool, str(f), "-qm", str(qm)])
                results.append(f"{'✅' if c2==0 else '❌'} {f.name} → {qm.name}")
            return "lrelease completato:\n" + "\n".join(results)
    return "lrelease non trovato. sudo apt install qttools5-dev-tools"

def handle_find_untranslated(args: dict) -> str:
    ts_file = args.get("ts_file", "")
    limit   = int(args.get("limit", 30))
    files   = [Path(ts_file)] if ts_file else _ts_files()
    lines   = []
    for f in files:
        total, translated, untrans = _parse_ts(f)
        missing = len(untrans)
        lines.append(f"\n📄 {f.name}: {missing} stringhe mancanti (su {total})\n")
        for item in untrans[:limit]:
            if "error" in item:
                lines.append(f"  ⚠️ {item['error']}")
            else:
                lines.append(f"  [{item['context']}] {item['source'][:80]}")
        if missing > limit:
            lines.append(f"  … e altre {missing - limit}")
    return "\n".join(lines) if lines else "✅ Tutte le stringhe sono tradotte."

def handle_coverage_report(args: dict) -> str:
    files = _ts_files()
    if not files:
        return f"Nessun .ts in {I18N}"
    lines = ["📊 Report copertura traduzioni\n"]
    grand_total, grand_translated = 0, 0
    for f in files:
        total, translated, untrans = _parse_ts(f)
        grand_total += total; grand_translated += translated
        pct = (translated / total * 100) if total else 0
        icon = "✅" if pct == 100 else ("🟡" if pct > 70 else "🔴")
        lines.append(f"  {icon} {f.name:<30} {translated:>4}/{total:<4} ({pct:.1f}%)")
    overall = (grand_translated / grand_total * 100) if grand_total else 0
    lines.append(f"\n  Totale: {grand_translated}/{grand_total} ({overall:.1f}%)")
    if overall < 100:
        lines.append(f"\n  Mancanti: {grand_total - grand_translated} stringhe")
        lines.append("  → Usa find_untranslated() per la lista, poi add_translation() o translation_mcp")
    return "\n".join(lines)

def handle_add_translation(args: dict) -> str:
    ts_file    = args.get("ts_file", "")
    source_str = args.get("source", "").strip()
    translation = args.get("translation", "").strip()
    dry_run    = bool(args.get("dry_run", True))
    if not ts_file or not source_str or not translation:
        return "Richiesti: ts_file, source, translation."
    path = Path(ts_file) if Path(ts_file).is_absolute() else I18N / ts_file
    if not path.exists():
        return f"File non trovato: {path}"
    try:
        tree = ET.parse(path)
    except ET.ParseError as e:
        return f"XML non valido: {e}"
    root = tree.getroot()
    updated = 0
    for context in root.findall("context"):
        for msg in context.findall("message"):
            if msg.findtext("source", "") == source_str:
                trans_el = msg.find("translation")
                if trans_el is None:
                    trans_el = ET.SubElement(msg, "translation")
                trans_el.text = translation
                if "type" in trans_el.attrib:
                    del trans_el.attrib["type"]
                updated += 1
    if updated == 0:
        return f"Stringa non trovata: '{source_str}'"
    if dry_run:
        return f"[DRY RUN] Aggiornerei {updated} occorrenza/e di '{source_str[:50]}' → '{translation[:50]}'"
    ET.indent(tree, space="    ")
    tree.write(str(path), encoding="utf-8", xml_declaration=True)
    return f"✅ Aggiornate {updated} occorrenza/e in {path.name}"

HANDLERS = {
    "list_ts_files":      handle_list_ts_files,
    "run_lupdate":        handle_run_lupdate,
    "run_lrelease":       handle_run_lrelease,
    "find_untranslated":  handle_find_untranslated,
    "coverage_report":    handle_coverage_report,
    "add_translation":    handle_add_translation,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"list_ts_files","description":"Elenca i file .ts Qt nel progetto con percentuale di copertura.","inputSchema":{"type":"object","properties":{}}},
    {"name":"run_lupdate","description":"Esegue lupdate per aggiornare i file .ts dai sorgenti C++ (tr() calls).","inputSchema":{"type":"object","properties":{"ts_file":{"type":"string","default":"","description":"Filtra su un solo .ts (default: tutti)"}}}},
    {"name":"run_lrelease","description":"Compila i file .ts in .qm (binario caricato dall'app a runtime).","inputSchema":{"type":"object","properties":{"ts_file":{"type":"string","default":""}}}},
    {"name":"find_untranslated","description":"Trova stringhe type='unfinished' o vuote nei file .ts.","inputSchema":{"type":"object","properties":{"ts_file":{"type":"string","default":""},"limit":{"type":"integer","default":30}}}},
    {"name":"coverage_report","description":"Percentuale di stringhe tradotte per ogni file .ts.","inputSchema":{"type":"object","properties":{}}},
    {"name":"add_translation","description":"Aggiunge/aggiorna una traduzione in un file .ts (dry_run=true di default).","inputSchema":{"type":"object","properties":{"ts_file":{"type":"string"},"source":{"type":"string"},"translation":{"type":"string"},"dry_run":{"type":"boolean","default":True}},"required":["ts_file","source","translation"]}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"qt-i18n","version":"1.0.0"},"capabilities":{"tools":{}}})
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

#!/usr/bin/env python3
"""
ui_ux_checker_mcp — Audit UI/UX Qt6 per Prismalux
===================================================
Verifica le REGOLE_IRREMOVIBILI di UX: dpiScale obbligatorio,
touch target Android, tooltip su icone, contrasto colori QSS,
accessibilità e naming convention widget.

Tools:
  check_dpi           — trova px hardcodati senza dpiScale()
  check_touch_targets — widget < 44dp su Android
  check_tooltips      — pulsanti icona senza setToolTip
  check_qss_contrast  — analisi contrasto WCAG nei file QSS
  check_accessibility — attributi accessibilità mancanti
  full_ux_audit       — esegue tutti i check in sequenza
"""
import sys, json, os, re, asyncio, logging
from pathlib import Path
from typing import Any

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
GUI_DIR = ROOT / "gui"
ANDROID_DIR = ROOT / "ANDROID"
IGNORE  = {"build", "build_gui", "build_asan", "build-desktop", ".git"}

ICON_SZ_OK   = 44   # dp minimo touch target Android (Material Design 3)
ICON_SZ_WARN = 36   # dp warning

# ─── helpers ─────────────────────────────────────────────────────────────────

def _src_files(path: Path, exts=(".cpp", ".h")) -> list[Path]:
    return [f for f in path.rglob("*") if f.suffix in exts
            and not any(p in IGNORE for p in f.parts)]

def _rel(f: Path) -> str:
    try: return str(f.relative_to(ROOT))
    except ValueError: return str(f)

def _parse_color_hex(s: str) -> tuple[int,int,int] | None:
    s = s.strip().lstrip("#")
    if len(s) == 3: s = s[0]*2 + s[1]*2 + s[2]*2
    if len(s) == 6:
        try: return (int(s[0:2],16), int(s[2:4],16), int(s[4:6],16))
        except ValueError: pass
    return None

def _relative_luminance(r: int, g: int, b: int) -> float:
    def srgb(c: int) -> float:
        v = c / 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    return 0.2126 * srgb(r) + 0.7152 * srgb(g) + 0.0722 * srgb(b)

def _contrast_ratio(c1: tuple, c2: tuple) -> float:
    l1 = _relative_luminance(*c1)
    l2 = _relative_luminance(*c2)
    lighter, darker = max(l1, l2), min(l1, l2)
    return (lighter + 0.05) / (darker + 0.05)

# Pattern px hardcodati nelle chiamate Qt6 di dimensionamento
DPI_PATTERNS = [
    (re.compile(r'\b(setFixedWidth|setFixedHeight|setFixedSize|setMinimumWidth|setMinimumHeight|setMaximumWidth|setMaximumHeight|resize|setIconSize)\s*\(\s*(\d+)'), "setFixedSize/resize"),
    (re.compile(r'\bQSize\s*\(\s*(\d+)\s*,\s*(\d+)\s*\)'), "QSize"),
    (re.compile(r'\bQFont.*setPixelSize\s*\(\s*(\d+)\s*\)'), "setPixelSize"),
]
DPISAFE = re.compile(r'dpiScale\s*\(')

ICON_BTN = re.compile(r'setIcon\s*\(|QPushButton|QToolButton|addAction\s*\(')
TOOLTIP  = re.compile(r'setToolTip\s*\(')

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_check_dpi(args: dict) -> str:
    path = Path(args.get("path", str(GUI_DIR)))
    if not path.is_absolute(): path = ROOT / path
    issues = []

    for f in _src_files(path):
        try:
            lines = f.read_text(errors="replace").splitlines()
        except OSError: continue
        for lineno, line in enumerate(lines, 1):
            if "dpiScale" in line or "//" in line.split("dpiScale")[0] if "dpiScale" not in line else False:
                continue
            for pattern, label in DPI_PATTERNS:
                m = pattern.search(line)
                if m and not DPISAFE.search(line) and "//nosec" not in line:
                    val = m.group(1) if m.lastindex == 1 else f"{m.group(1)},{m.group(2)}" if m.lastindex == 2 else "?"
                    if str(val).replace(",","").replace(" ","").replace("?","").isdigit():
                        issues.append((_rel(f), lineno, label, val, line.strip()[:80]))

    lines_out = [f"📐 DPI check (px hardcodati senza dpiScale) — {path.name}\n"]
    if not issues:
        lines_out.append("✅ Nessun px hardcodato trovato.")
    else:
        lines_out.append(f"🟠 {len(issues)} violazioni REGOLE_IRREMOVIBILI:\n")
        cur = None
        for fpath, ln, label, val, ctx in issues[:40]:
            if fpath != cur:
                cur = fpath; lines_out.append(f"\n📄 {fpath}")
            lines_out.append(f"  🟠 L{ln:4d} [{label}({val})] → {ctx}")
        lines_out.append("\n💡 Soluzione: sostituire N con dpiScale(N)")
    return "\n".join(lines_out)


def handle_check_touch_targets(args: dict) -> str:
    """Trova setFixedWidth/Height < 44dp nelle page Android."""
    path = Path(args.get("path", str(ANDROID_DIR)))
    if not path.is_absolute(): path = ROOT / path
    issues = []

    PAT = re.compile(r'\b(setFixedWidth|setFixedHeight|setFixedSize|setMinimumSize|QSize)\s*\(\s*(\d+)')
    for f in _src_files(path):
        try: lines = f.read_text(errors="replace").splitlines()
        except OSError: continue
        for lineno, line in enumerate(lines, 1):
            m = PAT.search(line)
            if m:
                val = int(m.group(2))
                if val < ICON_SZ_OK and val > 0:
                    sev = "🔴" if val < ICON_SZ_WARN else "🟡"
                    issues.append((sev, _rel(f), lineno, m.group(1), val, line.strip()[:70]))

    out = [f"📱 Touch target check Android (min {ICON_SZ_OK}dp — Material Design 3)\n"]
    if not issues:
        out.append("✅ Tutti i touch target rispettano i 44dp.")
    else:
        out.append(f"⚠️  {len(issues)} widget sottodimensionati:\n")
        for sev, fpath, ln, func, val, ctx in issues[:30]:
            out.append(f"  {sev} L{ln:4d} {func}({val}dp) — {fpath}")
            out.append(f"       → {ctx}")
    return "\n".join(out)


def handle_check_tooltips(args: dict) -> str:
    """Trova pulsanti con setIcon ma senza setToolTip nelle vicinanze."""
    path = Path(args.get("path", str(GUI_DIR)))
    if not path.is_absolute(): path = ROOT / path
    issues = []

    for f in _src_files(path, (".cpp",)):
        try: text = f.read_text(errors="replace")
        except OSError: continue
        lines = text.splitlines()
        for i, line in enumerate(lines):
            if "setIcon" in line and "setToolTip" not in line:
                window = "\n".join(lines[max(0,i-3):i+4])
                if "setToolTip" not in window:
                    issues.append((_rel(f), i+1, line.strip()[:80]))

    out = [f"💬 Tooltip check — pulsanti icona senza setToolTip\n"]
    if not issues:
        out.append("✅ Tutti i pulsanti icona hanno tooltip.")
    else:
        out.append(f"🟡 {len(issues)} pulsanti senza tooltip (accessibilità ridotta):\n")
        cur = None
        for fpath, ln, ctx in issues[:25]:
            if fpath != cur:
                cur = fpath; out.append(f"\n📄 {fpath}")
            out.append(f"  🟡 L{ln:4d} → {ctx}")
        out.append("\n💡 Aggiungi: btn->setToolTip(tr(\"Descrizione\"));")
    return "\n".join(out)


def handle_check_qss_contrast(args: dict) -> str:
    """Analizza i file QSS per coppie color/background-color e calcola contrasto WCAG."""
    qss_files = list((GUI_DIR / "themes").rglob("*.qss")) if (GUI_DIR / "themes").exists() else []
    qss_files += list(GUI_DIR.rglob("*.qss"))
    qss_files = [f for f in qss_files if not any(p in IGNORE for p in f.parts)]

    COLORS  = re.compile(r'color\s*:\s*(#[0-9a-fA-F]{3,6}|rgb\(\d+,\s*\d+,\s*\d+\))')
    BGCOL   = re.compile(r'background(?:-color)?\s*:\s*(#[0-9a-fA-F]{3,6}|rgb\(\d+,\s*\d+,\s*\d+\))')

    issues = []; checked = 0
    for f in qss_files:
        try: text = f.read_text(errors="replace")
        except OSError: continue
        blocks = re.split(r'\}', text)
        for block in blocks:
            fg_m  = COLORS.search(block)
            bg_m  = BGCOL.search(block)
            if not fg_m or not bg_m: continue
            fg = _parse_color_hex(fg_m.group(1).replace("rgb(","").replace(")",""))
            bg = _parse_color_hex(bg_m.group(1).replace("rgb(","").replace(")",""))
            if fg and bg:
                checked += 1
                ratio = _contrast_ratio(fg, bg)
                if ratio < 4.5:
                    sev = "🔴" if ratio < 3.0 else "🟡"
                    issues.append((sev, _rel(f), ratio, fg_m.group(1), bg_m.group(1)))

    out = [f"🎨 QSS contrasto WCAG AA (ratio ≥ 4.5:1) — {len(qss_files)} file QSS\n"]
    if not qss_files:
        return "Nessun file .qss trovato. I temi sono definiti altrove?"
    if not issues:
        out.append(f"✅ Tutte le {checked} coppie colore rispettano WCAG AA.")
    else:
        out.append(f"⚠️  {len(issues)}/{checked} coppie sotto soglia WCAG AA (4.5:1):\n")
        for sev, fpath, ratio, fg, bg in sorted(issues, key=lambda x: x[2])[:15]:
            out.append(f"  {sev} ratio {ratio:.2f}:1  fg={fg} bg={bg}  ({fpath})")
        out.append("\n💡 Standard WCAG: AA = 4.5:1 (testo normale), AAA = 7:1 (testo piccolo)")
    return "\n".join(out)


def handle_check_accessibility(args: dict) -> str:
    path = Path(args.get("path", str(GUI_DIR)))
    if not path.is_absolute(): path = ROOT / path
    issues = []

    PATTERNS = [
        (re.compile(r'QLabel\s*\([^)]*\)'), re.compile(r'setBuddy|setAccessible'), "QLabel senza setBuddy o setAccessibleName"),
        (re.compile(r'new\s+QProgressBar'), re.compile(r'setAccessibleDescription|setToolTip'), "QProgressBar senza descrizione accessibile"),
        (re.compile(r'setPlaceholderText\s*\(\s*\)'), None, "setPlaceholderText vuoto"),
        (re.compile(r'addTab\s*\([^,]+,\s*""'), None, "Tab senza label testuale"),
    ]
    for f in _src_files(path, (".cpp",)):
        try: lines = f.read_text(errors="replace").splitlines()
        except OSError: continue
        for i, line in enumerate(lines):
            for trigger, check, msg in PATTERNS:
                if trigger.search(line):
                    window = "\n".join(lines[max(0,i-2):i+3])
                    if check and not check.search(window):
                        issues.append((_rel(f), i+1, msg, line.strip()[:60]))
                    elif check is None:
                        issues.append((_rel(f), i+1, msg, line.strip()[:60]))

    out = [f"♿ Accessibilità Qt6 — {path.name}\n"]
    if not issues:
        out.append("✅ Nessun problema di accessibilità rilevato.")
    else:
        out.append(f"🟡 {len(issues)} suggerimenti accessibilità:\n")
        for fpath, ln, msg, ctx in issues[:20]:
            out.append(f"  🟡 L{ln:4d} [{msg}]")
            out.append(f"       {fpath}: {ctx}")
    return "\n".join(out)


def handle_full_ux_audit(args: dict) -> str:
    sections = [
        ("DPI — px hardcodati",     lambda: handle_check_dpi(args)),
        ("Touch target Android",    lambda: handle_check_touch_targets({"path": str(ANDROID_DIR)})),
        ("Tooltip mancanti",        lambda: handle_check_tooltips(args)),
        ("Contrasto WCAG QSS",      lambda: handle_check_qss_contrast(args)),
        ("Accessibilità",           lambda: handle_check_accessibility(args)),
    ]
    out = ["🔍 FULL UX AUDIT — Prismalux\n" + "═"*50 + "\n"]
    for title, fn in sections:
        out.append(f"\n{'─'*50}\n  {title}\n{'─'*50}")
        try: out.append(fn())
        except Exception as e: out.append(f"Errore: {e}")
    return "\n".join(out)


HANDLERS = {
    "check_dpi":           handle_check_dpi,
    "check_touch_targets": handle_check_touch_targets,
    "check_tooltips":      handle_check_tooltips,
    "check_qss_contrast":  handle_check_qss_contrast,
    "check_accessibility": handle_check_accessibility,
    "full_ux_audit":       handle_full_ux_audit,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"check_dpi","description":"Trova dimensioni px hardcodate senza dpiScale() nel codice C++ Qt6 — violazione REGOLE_IRREMOVIBILI.","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""}}}},
    {"name":"check_touch_targets","description":"Trova widget Android con dimensioni < 44dp (soglia Material Design 3 per touch target).","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""}}}},
    {"name":"check_tooltips","description":"Trova pulsanti con setIcon() senza setToolTip() nelle vicinanze — accessibilità ridotta.","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""}}}},
    {"name":"check_qss_contrast","description":"Analizza i file QSS per coppie color/background-color e calcola il rapporto di contrasto WCAG AA (soglia 4.5:1).","inputSchema":{"type":"object","properties":{}}},
    {"name":"check_accessibility","description":"Verifica attributi di accessibilità Qt6: QLabel senza buddy, QProgressBar senza descrizione, tab senza label.","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""}}}},
    {"name":"full_ux_audit","description":"Esegue tutti i check UX in sequenza: DPI, touch target, tooltip, contrasto WCAG, accessibilità.","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""}}}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"ui-ux-checker","version":"1.0.0"},"capabilities":{"tools":{}}})
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

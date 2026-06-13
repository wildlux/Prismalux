#!/usr/bin/env python3
"""
test_generator_mcp — Generatore skeleton test Qt6 e pytest
===========================================================
Legge header C++ (.h) o moduli Python (.py) e genera automaticamente
i boilerplate di test con casi edge suggeriti.

Tools:
  generate_qt_tests    — header .h → skeleton QTest con slot per ogni metodo
  generate_pytest      — file .py → skeleton pytest con fixture e edge cases
  list_untested        — trova file sorgente senza file di test corrispondente
  analyze_test_gaps    — confronta metodi pubblici con i test esistenti
  suggest_edge_cases   — suggerisce casi edge per una firma di funzione
"""
import sys, json, os, re, asyncio
from pathlib import Path
from datetime import datetime
from typing import Any

ROOT      = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
GUI_DIR   = ROOT / "gui"
TEST_DIR  = GUI_DIR / "tests"
IGNORE    = {"build", "build_gui", "build_asan", ".git", "__pycache__"}

# ─── parser C++ header ───────────────────────────────────────────────────────

def _parse_cpp_header(text: str) -> dict:
    """Estrae class name e metodi pubblici da un header C++."""
    class_m = re.search(r'class\s+(\w+)\s*(?::\s*public\s+\w+)?', text)
    classname = class_m.group(1) if class_m else "UnknownClass"

    public_section = ""
    in_public = False
    for line in text.splitlines():
        if re.match(r'\s*public\s*:', line): in_public = True
        elif re.match(r'\s*(private|protected)\s*:', line): in_public = False
        if in_public: public_section += line + "\n"

    METHOD_PAT = re.compile(
        r'(?:virtual\s+|static\s+|explicit\s+|inline\s+)*'
        r'(?:void|bool|int|QString|QStringList|double|float|QWidget\*?|QObject\*?|auto)\s+'
        r'(\w+)\s*\(([^)]*)\)'
        r'(?:\s*const)?(?:\s*override)?(?:\s*noexcept)?\s*;'
    )
    methods = []
    for m in METHOD_PAT.finditer(public_section):
        name   = m.group(1)
        params = m.group(2).strip()
        if name not in ("~" + classname, classname) and not name.startswith("~"):
            methods.append({"name": name, "params": params})

    # Slot (segnali non testabili direttamente)
    signals = re.findall(r'signals:\s*\n((?:\s+\w[^\n]+\n)*)', text)
    return {"classname": classname, "methods": methods[:30], "has_signals": bool(signals)}

def _parse_python_functions(text: str, module_name: str) -> list[dict]:
    """Estrae funzioni e classi da un modulo Python."""
    funcs = []
    for m in re.finditer(r'^(?:async\s+)?def\s+(\w+)\s*\(([^)]*)\)', text, re.MULTILINE):
        name = m.group(1)
        if name.startswith("_"): continue  # skip private
        params_raw = m.group(2)
        params = [p.strip().split(":")[0].split("=")[0].strip()
                  for p in params_raw.split(",") if p.strip() not in ("","self","cls")]
        funcs.append({"name": name, "params": params, "is_async": "async" in m.group(0)})
    return funcs[:30]

def _edge_cases_for_type(param: str) -> list[str]:
    p = param.lower()
    if any(k in p for k in ("str","text","name","path","query","message")):
        return ["\"\"", "\"a\"*10000", "None", "\"<script>alert(1)</script>\"", "\"../../../etc/passwd\""]
    if any(k in p for k in ("int","count","n","size","index","num","line")):
        return ["0", "-1", "2**31-1", "2**31", "-2**31"]
    if any(k in p for k in ("list","items","data","arr","elems")):
        return ["[]", "[None]", "list(range(10000))"]
    if any(k in p for k in ("dict","map","config","opts")):
        return ["{}", "None", "{k:v for k,v in zip(range(100),range(100))}"]
    if any(k in p for k in ("bool","flag","enable","active")):
        return ["True", "False"]
    if any(k in p for k in ("file","path","dir")):
        return ["\"/nonexistent/path\"", "\"\"", "\"/tmp/test.txt\""]
    return ["None", "\"\"", "0"]

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_generate_qt_tests(args: dict) -> str:
    header = args.get("file", "").strip()
    if not header: return "Specifica 'file' (path dell'header .h)."
    path = Path(header) if Path(header).is_absolute() else ROOT / header
    if not path.exists(): return f"File non trovato: {path}"

    try: text = path.read_text(errors="replace")
    except OSError as e: return f"Errore lettura: {e}"

    info = _parse_cpp_header(text)
    classname = info["classname"]
    methods   = info["methods"]
    date      = datetime.now().strftime("%Y-%m-%d")

    lines = [
        f"// AUTO-GENERATO da test_generator_mcp — {date}",
        f"// Fonte: {path.name}",
        f"// Personalizzare i TODO prima di usare.",
        "",
        "#include <QtTest/QtTest>",
        f'#include "{path.name}"',
        "",
        f"class Test{classname} : public QObject",
        "{",
        "    Q_OBJECT",
        "",
        "private slots:",
        "    void initTestCase() {",
        f"        // TODO: setup {classname}",
        "    }",
        "    void cleanupTestCase() {",
        "        // TODO: teardown",
        "    }",
        "",
    ]

    for m in methods:
        mname  = m["name"]
        params = m["params"]
        lines += [
            f"    // ── {mname}() ──────────────────────────────",
            f"    void test_{mname}_nominal() {{",
            f"        // TODO: arrange",
            f"        // {classname} obj;",
            f"        // TODO: act — obj.{mname}({params})",
            f"        // TODO: assert",
            f"        QVERIFY(true); // sostituire con asserzione reale",
            f"    }}",
            f"    void test_{mname}_edge_empty() {{",
            f"        // Edge case: input vuoto / null",
            f"        QVERIFY(true);",
            f"    }}",
            f"    void test_{mname}_edge_boundary() {{",
            f"        // Edge case: valori limite",
            f"        QVERIFY(true);",
            f"    }}",
            "",
        ]

    if info["has_signals"]:
        lines += [
            "    void test_signals() {",
            f"        // TODO: QSignalSpy spy(&obj, &{classname}::yourSignal);",
            "        // spy.wait(1000);",
            "        // QCOMPARE(spy.count(), 1);",
            "    }",
        ]

    lines += ["};", "", f"QTEST_MAIN(Test{classname})", f'#include "test_{classname.lower()}.moc"']
    return "\n".join(lines)


def handle_generate_pytest(args: dict) -> str:
    pyfile = args.get("file", "").strip()
    if not pyfile: return "Specifica 'file' (path del modulo .py)."
    path = Path(pyfile) if Path(pyfile).is_absolute() else ROOT / pyfile
    if not path.exists(): return f"File non trovato: {path}"

    try: text = path.read_text(errors="replace")
    except OSError as e: return f"Errore lettura: {e}"

    funcs = _parse_python_functions(text, path.stem)
    date  = datetime.now().strftime("%Y-%m-%d")

    lines = [
        f'"""Auto-generato da test_generator_mcp — {date}',
        f'Fonte: {path.name}',
        'Personalizzare i TODO prima di usare."""',
        "",
        "import pytest",
        f"from {path.stem} import *",
        "",
        "",
        "@pytest.fixture",
        "def setup():",
        "    \"\"\"Fixture comune — personalizzare.\"\"\"",
        "    # TODO: setup",
        "    yield",
        "    # TODO: teardown",
        "",
    ]

    for fn in funcs:
        fname = fn["name"]
        params = fn["params"]
        prefix = "async " if fn["is_async"] else ""
        decorator = "@pytest.mark.asyncio\n" if fn["is_async"] else ""

        lines += [
            f"# ── {fname}() ──────────────────────────────────────",
            f"{decorator}{prefix}def test_{fname}_nominal(setup):",
            f"    # TODO: arrange",
            f"    # TODO: act — result = {'await ' if fn['is_async'] else ''}{fname}({', '.join(repr('') if 'str' in p.lower() else '0' for p in params) if params else ''})",
            f"    # TODO: assert",
            f"    assert True  # sostituire",
            "",
        ]

        if params:
            edge_params = params[0] if params else "input"
            edges = _edge_cases_for_type(edge_params)
            lines += [
                f"@pytest.mark.parametrize(\"{params[0] if params else 'val'}\", [{', '.join(edges[:3])}])",
                f"{prefix}def test_{fname}_parametrize({params[0] if params else 'val'}, setup):",
                f"    # Edge cases automatici per '{params[0] if params else 'val'}'",
                f"    # TODO: assert sul comportamento atteso",
                f"    pass",
                "",
            ]

    return "\n".join(lines)


def handle_list_untested(args: dict) -> str:
    path = Path(args.get("path", str(GUI_DIR)))
    if not path.is_absolute(): path = ROOT / path

    src_files = [f for f in path.rglob("*.cpp")
                 if not any(p in IGNORE for p in f.parts)
                 and "test" not in f.name.lower()
                 and f.parent.name != "tests"]

    test_files = list((path / "tests").rglob("*.cpp")) if (path/"tests").exists() else []
    test_names = {re.sub(r'^test_?','', f.stem.lower()) for f in test_files}

    untested = []
    for f in src_files:
        stem = f.stem.lower()
        if stem not in test_names and f"test_{stem}" not in test_names:
            untested.append(f)

    lines = [f"🧪 File senza test corrispondente — {path.name}\n"]
    if not untested:
        lines.append("✅ Tutti i file hanno un test corrispondente.")
    else:
        lines.append(f"⚠️  {len(untested)} file senza test:\n")
        for f in sorted(untested, key=lambda x: x.name):
            try: rel = f.relative_to(ROOT)
            except: rel = f
            lines.append(f"  📄 {rel}")
        lines.append(f"\n💡 Usa generate_qt_tests(file=...) per generare lo skeleton.")
    return "\n".join(lines)


def handle_analyze_test_gaps(args: dict) -> str:
    src_file  = args.get("source", "").strip()
    test_file = args.get("test", "").strip()
    if not src_file: return "Specifica 'source' (header .h) e opzionalmente 'test' (file test .cpp)."

    src_path = Path(src_file) if Path(src_file).is_absolute() else ROOT / src_file
    if not src_path.exists(): return f"Sorgente non trovato: {src_path}"

    info = _parse_cpp_header(src_path.read_text(errors="replace"))
    public_methods = {m["name"] for m in info["methods"]}

    tested = set()
    if test_file:
        tp = Path(test_file) if Path(test_file).is_absolute() else ROOT / test_file
        if tp.exists():
            ttext = tp.read_text(errors="replace")
            tested = {m for m in public_methods if m in ttext}

    untested = public_methods - tested
    lines = [f"📊 Gap analisi test — {src_path.name}\n",
             f"  Metodi pubblici : {len(public_methods)}",
             f"  Metodi testati  : {len(tested)}",
             f"  Copertura stim. : {len(tested)/max(len(public_methods),1)*100:.0f}%\n"]
    if untested:
        lines.append("Metodi senza test:")
        for m in sorted(untested):
            lines.append(f"  ❌ {m}()")
    else:
        lines.append("✅ Tutti i metodi pubblici sono coperti.")
    return "\n".join(lines)


def handle_suggest_edge_cases(args: dict) -> str:
    signature = args.get("signature", "").strip()
    if not signature: return "Specifica 'signature' (es. 'bool parseConfig(QString path, int maxLines)')."

    params = re.findall(r'(\w+)\s+(\w+)(?:\s*=\s*[^,)]+)?', signature)
    lines  = [f"💡 Edge cases suggeriti per: {signature}\n"]
    for ptype, pname in params:
        edges = _edge_cases_for_type(pname)
        lines.append(f"  {ptype} {pname}:")
        for e in edges:
            lines.append(f"    · {e}")
        lines.append("")

    lines += [
        "Casi generali sempre da testare:",
        "  · Input null/None/nullptr",
        "  · Stringa vuota / lista vuota",
        "  · Valori al limite (0, -1, INT_MAX, INT_MIN)",
        "  · Input molto lungo (stress)",
        "  · Caratteri speciali: <>&\"'\\0\\n",
        "  · Percorso inesistente o senza permessi",
        "  · Chiamata ripetuta (idempotenza)",
    ]
    return "\n".join(lines)


HANDLERS = {
    "generate_qt_tests":   handle_generate_qt_tests,
    "generate_pytest":     handle_generate_pytest,
    "list_untested":       handle_list_untested,
    "analyze_test_gaps":   handle_analyze_test_gaps,
    "suggest_edge_cases":  handle_suggest_edge_cases,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"generate_qt_tests","description":"Legge un header C++ .h e genera lo skeleton QTest completo con un slot per ogni metodo pubblico e casi edge.","inputSchema":{"type":"object","properties":{"file":{"type":"string","description":"Path header .h (relativo a root)"}},"required":["file"]}},
    {"name":"generate_pytest","description":"Legge un file Python .py e genera skeleton pytest con fixture e parametrize sui casi edge.","inputSchema":{"type":"object","properties":{"file":{"type":"string","description":"Path file .py (relativo a root)"}},"required":["file"]}},
    {"name":"list_untested","description":"Trova file sorgente C++ che non hanno un file di test corrispondente in gui/tests/.","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""}}}},
    {"name":"analyze_test_gaps","description":"Confronta i metodi pubblici di un header con i test esistenti e stima la copertura.","inputSchema":{"type":"object","properties":{"source":{"type":"string","description":"Header .h sorgente"},"test":{"type":"string","description":"File test .cpp (opzionale)","default":""}},"required":["source"]}},
    {"name":"suggest_edge_cases","description":"Suggerisce casi edge da testare per una firma di funzione C++ o Python.","inputSchema":{"type":"object","properties":{"signature":{"type":"string","description":"Firma della funzione (es. 'bool parseConfig(QString path, int maxLines)')"}},"required":["signature"]}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"test-generator","version":"1.0.0"},"capabilities":{"tools":{}}})
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

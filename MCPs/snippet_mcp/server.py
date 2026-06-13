#!/usr/bin/env python3
"""
snippet_mcp — Boilerplate Qt6 per Prismalux
============================================
Genera scheletri pronti per nuove page, widget, MCP e test
seguendo le REGOLE_IRREMOVIBILI: niente *_page suffix,
dpiScale() obbligatorio, slot nominati, niente lambda.

Tools:
  new_page     — header + cpp per una nuova tab/page Qt6
  new_widget   — header + cpp per un nuovo widget riusabile
  new_mcp      — server.py boilerplate per un nuovo MCP
  new_test     — QTest header+cpp per una classe esistente
  get_snippet  — restituisce uno snippet per nome
  list_snippets — elenca snippet disponibili
"""
import sys, json, os, asyncio
from pathlib import Path
from typing import Any
from datetime import date

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
GUI_DIR = ROOT / "gui"
PAGES   = GUI_DIR / "pages"
WIDGETS = GUI_DIR / "widgets"

TODAY = date.today().isoformat()

# ─── template strings ─────────────────────────────────────────────────────────

def _page_header(class_name: str, file_stem: str, desc: str) -> str:
    guard = file_stem.upper() + "_H"
    return f"""\
#pragma once
// {file_stem}.h — {desc}
// Creato: {TODAY}
#ifndef {guard}
#define {guard}

#include <QWidget>
#include "../dpi_utils.h"
#include "../ai_client.h"

class {class_name} : public QWidget
{{
    Q_OBJECT
public:
    explicit {class_name}(AiClient *ai, QWidget *parent = nullptr);

private slots:
    void onActionTriggered();

private:
    void buildUi();

    AiClient *m_ai{{nullptr}};
    // ↓ dichiara qui i widget membro
}};

#endif // {guard}
"""

def _page_cpp(class_name: str, file_stem: str) -> str:
    return f"""\
// {file_stem}.cpp
#include "{file_stem}.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

{class_name}::{class_name}(AiClient *ai, QWidget *parent)
    : QWidget(parent), m_ai(ai)
{{
    buildUi();
}}

void {class_name}::buildUi()
{{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(dpiScale(12), dpiScale(12), dpiScale(12), dpiScale(12));
    layout->setSpacing(dpiScale(8));

    auto *title = new QLabel(tr("Titolo sezione"), this);
    layout->addWidget(title);

    auto *btn = new QPushButton(tr("Azione"), this);
    btn->setFixedHeight(dpiScale(36));
    layout->addWidget(btn);

    connect(btn, &QPushButton::clicked, this, &{class_name}::onActionTriggered);

    layout->addStretch();
}}

void {class_name}::onActionTriggered()
{{
    // TODO: implementa azione
}}
"""

def _widget_header(class_name: str, file_stem: str, desc: str) -> str:
    guard = file_stem.upper() + "_H"
    return f"""\
#pragma once
// {file_stem}.h — {desc}
// Creato: {TODAY}
#ifndef {guard}
#define {guard}

#include <QWidget>
#include "../dpi_utils.h"

class {class_name} : public QWidget
{{
    Q_OBJECT
public:
    explicit {class_name}(QWidget *parent = nullptr);

    void setValue(const QString &value);
    QString value() const;

signals:
    void valueChanged(const QString &newValue);

private slots:
    void onInternalChange();

private:
    void buildUi();
    QString m_value;
}};

#endif // {guard}
"""

def _widget_cpp(class_name: str, file_stem: str) -> str:
    return f"""\
// {file_stem}.cpp
#include "{file_stem}.h"
#include <QVBoxLayout>

{class_name}::{class_name}(QWidget *parent) : QWidget(parent)
{{
    buildUi();
}}

void {class_name}::buildUi()
{{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(dpiScale(4), dpiScale(4), dpiScale(4), dpiScale(4));
    // TODO: aggiungi widget figli
}}

void {class_name}::setValue(const QString &value)
{{
    if (m_value == value) return;
    m_value = value;
    emit valueChanged(m_value);
}}

QString {class_name}::value() const {{ return m_value; }}

void {class_name}::onInternalChange()
{{
    // TODO
}}
"""

def _mcp_server(name: str, tools: list[str]) -> str:
    tool_defs = ""
    handlers  = ""
    for t in tools:
        handlers  += f'\ndef handle_{t}(args: dict) -> str:\n    # TODO\n    return "Non implementato"\n'
        tool_defs += (f'\n    {{"name":"{t}","description":"TODO","inputSchema":'
                      f'{{"type":"object","properties":{{}}}}}},'
                     )
    return f"""\
#!/usr/bin/env python3
\"\"\"
{name} — TODO: descrizione
\"\"\"
import sys, json, os, asyncio
from pathlib import Path
from typing import Any

ROOT = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
{handlers}
HANDLERS = {{{", ".join(f'"{t}": handle_{t}' for t in tools)}}}
TOOL_DEFS: list[dict[str, Any]] = [{tool_defs}
]

def _ok(rid, r): return json.dumps({{"jsonrpc":"2.0","id":rid,"result":r}})
def _err(rid, c, m): return json.dumps({{"jsonrpc":"2.0","id":rid,"error":{{"code":c,"message":m}}}})
async def _main():
    loop = asyncio.get_event_loop()
    rl   = lambda: loop.run_in_executor(None, sys.stdin.readline)
    while True:
        line = (await rl()).strip()
        if not line: continue
        try: req = json.loads(line)
        except: sys.stdout.write(_err(None,-32700,"parse")+"\\n"); sys.stdout.flush(); continue
        rid, method, params = req.get("id"), req.get("method",""), req.get("params",{{}})
        if method == "initialize":
            resp = _ok(rid,{{"protocolVersion":"2024-11-05","serverInfo":{{"name":"{name}","version":"1.0.0"}},"capabilities":{{"tools":{{}}}}}})
        elif method == "tools/list": resp = _ok(rid,{{"tools":TOOL_DEFS}})
        elif method == "tools/call":
            name_, targs = params.get("name",""), params.get("arguments",{{}})
            h = HANDLERS.get(name_)
            if not h: resp = _err(rid,-32601,f"unknown: {{name_}}")
            else:
                try:
                    text = await asyncio.to_thread(h, targs)
                    resp = _ok(rid,{{"content":[{{"type":"text","text":text}}],"isError":False}})
                except Exception as e:
                    resp = _ok(rid,{{"content":[{{"type":"text","text":str(e)}}],"isError":True}})
        elif method == "notifications/initialized": continue
        else: resp = _err(rid,-32601,method)
        sys.stdout.write(resp+"\\n"); sys.stdout.flush()
def main(): asyncio.run(_main())
if __name__ == "__main__": main()
"""

def _qt_test(class_name: str, tested_class: str, include_file: str) -> str:
    return f"""\
// test_{class_name.lower()}.h
// QTest per {tested_class}
// Creato: {TODAY}
#pragma once
#include <QObject>
#include <QtTest/QtTest>
#include "../{include_file}"

class Test{class_name} : public QObject
{{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_basic_nominal();
    void test_basic_empty_input();
    void test_basic_boundary();
}};

// test_{class_name.lower()}.cpp
#include "test_{class_name.lower()}.h"

void Test{class_name}::initTestCase()
{{
    // setup globale una volta
}}

void Test{class_name}::cleanupTestCase()
{{
    // teardown globale
}}

void Test{class_name}::test_basic_nominal()
{{
    // TODO: caso nominale
    QVERIFY(true);
}}

void Test{class_name}::test_basic_empty_input()
{{
    // TODO: input vuoto / null
    QVERIFY(true);
}}

void Test{class_name}::test_basic_boundary()
{{
    // TODO: valori limite
    QVERIFY(true);
}}

QTEST_MAIN(Test{class_name})
#include "test_{class_name.lower()}.moc"
"""

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_new_page(args: dict) -> str:
    name    = args.get("name","").strip()
    desc    = args.get("description","").strip() or f"Pagina {name}"
    write   = bool(args.get("write", False))
    if not name: return "Specifica 'name' (es. 'main_foo' o 'widget_settings_bar')."

    # Determina file stem e class name
    stem  = name.lower().replace(" ","_")
    if stem.endswith("_page"):
        return "🔴 ERRORE: il suffisso '_page' è vietato da REGOLE_IRREMOVIBILI.\n  Usa 'main_foo', 'settings_foo', 'widget_foo', ecc."
    # CamelCase: main_foo_bar → MainFooBar
    class_name = "".join(w.title() for w in stem.split("_")) + "Page"
    h_content  = _page_header(class_name, stem, desc)
    cpp_content= _page_cpp(class_name, stem)

    if write:
        (PAGES / f"{stem}.h").write_text(h_content)
        (PAGES / f"{stem}.cpp").write_text(cpp_content)
        return (f"✅ Creati:\n"
                f"  gui/pages/{stem}.h\n"
                f"  gui/pages/{stem}.cpp\n"
                f"  Classe: {class_name}\n\n"
                f"Aggiungi al CMakeLists.txt e alla mainwindow i riferimenti.")

    return (f"📋 Boilerplate: gui/pages/{stem}.h\n\n"
            f"=== {stem}.h ===\n{h_content}\n"
            f"=== {stem}.cpp ===\n{cpp_content}")

def handle_new_widget(args: dict) -> str:
    name  = args.get("name","").strip()
    desc  = args.get("description","").strip() or f"Widget {name}"
    write = bool(args.get("write", False))
    if not name: return "Specifica 'name'."
    stem  = name.lower().replace(" ","_")
    if not any(stem.startswith(p) for p in ("widget_","dialog_","main_","settings_")):
        stem = "widget_" + stem
    class_name  = "".join(w.title() for w in stem.split("_"))
    h_content   = _widget_header(class_name, stem, desc)
    cpp_content = _widget_cpp(class_name, stem)
    if write:
        (WIDGETS / f"{stem}.h").write_text(h_content)
        (WIDGETS / f"{stem}.cpp").write_text(cpp_content)
        return f"✅ Creati gui/widgets/{stem}.h + .cpp  (classe: {class_name})"
    return (f"📋 Boilerplate: gui/widgets/{stem}.h\n\n"
            f"=== {stem}.h ===\n{h_content}\n=== {stem}.cpp ===\n{cpp_content}")

def handle_new_mcp(args: dict) -> str:
    name    = args.get("name","").strip()
    tools   = args.get("tools", ["tool_one","tool_two"])
    write   = bool(args.get("write", False))
    if not name: return "Specifica 'name' (es. 'foo_bar')."
    if isinstance(tools, str): tools = [t.strip() for t in tools.split(",")]
    mcp_dir = ROOT / "MCPs" / (name if name.endswith("_mcp") else name + "_mcp")
    content = _mcp_server(name, tools)
    if write:
        mcp_dir.mkdir(parents=True, exist_ok=True)
        (mcp_dir / "server.py").write_text(content)
        (mcp_dir / "requirements.txt").write_text(f"# {name} — stdlib only\n")
        return f"✅ Creato {mcp_dir.relative_to(ROOT)}/server.py con {len(tools)} tool"
    return f"📋 Boilerplate MCP: {name}\n\n{content}"

def handle_new_test(args: dict) -> str:
    class_name  = args.get("class_name","").strip()
    tested      = args.get("tested_class", class_name).strip()
    include     = args.get("include_file","").strip() or f"{class_name.lower()}.h"
    write       = bool(args.get("write", False))
    if not class_name: return "Specifica 'class_name'."
    content = _qt_test(class_name, tested, include)
    if write:
        test_dir = GUI_DIR / "tests"
        test_dir.mkdir(exist_ok=True)
        (test_dir / f"test_{class_name.lower()}.h").write_text(content.split("// test_")[0])
        return f"✅ Creato gui/tests/test_{class_name.lower()}.h"
    return f"📋 QTest boilerplate: Test{class_name}\n\n{content}"

def handle_list_snippets(args: dict) -> str:
    return """\
📦 Snippet disponibili:

  new_page(name, description?, write?)
    → gui/pages/<name>.h + .cpp  (classe <Name>Page)
    → REGOLE: no *_page suffix, dpiScale(), slot nominati

  new_widget(name, description?, write?)
    → gui/widgets/widget_<name>.h + .cpp
    → aggiunge prefisso 'widget_' automaticamente se mancante

  new_mcp(name, tools?, write?)
    → MCPs/<name>_mcp/server.py  (JSON-RPC 2.0 stdio, boilerplate completo)
    → tools: lista di nomi tool da includere

  new_test(class_name, tested_class?, include_file?, write?)
    → gui/tests/test_<class_name>.h  (QTest + QTEST_MAIN)

Passa write=true per scrivere su disco (default: solo anteprima).
"""

def handle_get_snippet(args: dict) -> str:
    name = args.get("name","").strip()
    snip = {
        "connect_slot": (
            "// Pattern connect() sicuro — sempre con context object\n"
            "connect(sender, &Sender::signal, this, &MyClass::onSlot);\n"
            "// MAI: connect(sender, &Sender::signal, [this]() { /* logica > 2 righe */ });\n"
        ),
        "dpi_widget": (
            "#include \"dpi_utils.h\"\n\n"
            "btn->setFixedHeight(dpiScale(36));\n"
            "layout->setContentsMargins(dpiScale(12), dpiScale(12), dpiScale(12), dpiScale(12));\n"
            "layout->setSpacing(dpiScale(8));\n"
            "icon.setWidth(dpiScale(24));\n"
        ),
        "ai_request": (
            "// Richiesta AI asincrona con gestione errore\n"
            "auto *reply = m_ai->chat(prompt);\n"
            "connect(reply, &QNetworkReply::finished, this, [reply, this]() {\n"
            "    reply->deleteLater();\n"
            "    if (reply->error() != QNetworkReply::NoError) {\n"
            "        // mostra errore\n"
            "        return;\n"
            "    }\n"
            "    const auto resp = reply->readAll();\n"
            "    // processa risposta\n"
            "});\n"
        ),
        "qstring_i18n": (
            "// Stringa internazionalizzata\n"
            "setWindowTitle(tr(\"Titolo finestra\"));\n"
            "btn->setText(tr(\"Azione\"));\n"
            "// Stringa con parametro\n"
            "label->setText(tr(\"%1 elementi\").arg(count));\n"
        ),
        "path_root": (
            "#include \"prismalux_paths.h\"\n"
            "namespace P = PrismaluxPaths;\n\n"
            "const QString root = P::root();\n"
            "const quint16 port = P::kOllamaPort;\n"
            "// MAI: \"/home/wildlux/Desktop/Prismalux\" hardcoded\n"
        ),
    }
    if name not in snip:
        return (f"Snippet '{name}' non trovato.\n"
                f"Disponibili: " + ", ".join(snip.keys()))
    return f"📋 Snippet: {name}\n\n```cpp\n{snip[name]}```"

HANDLERS = {
    "new_page":      handle_new_page,
    "new_widget":    handle_new_widget,
    "new_mcp":       handle_new_mcp,
    "new_test":      handle_new_test,
    "list_snippets": handle_list_snippets,
    "get_snippet":   handle_get_snippet,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"new_page","description":"Genera boilerplate Qt6 header+cpp per una nuova pagina/tab (naming convention Prismalux, dpiScale, slot nominati).","inputSchema":{"type":"object","properties":{"name":{"type":"string"},"description":{"type":"string","default":""},"write":{"type":"boolean","default":False}},"required":["name"]}},
    {"name":"new_widget","description":"Genera boilerplate Qt6 per un nuovo widget riusabile con prefisso 'widget_'.","inputSchema":{"type":"object","properties":{"name":{"type":"string"},"description":{"type":"string","default":""},"write":{"type":"boolean","default":False}},"required":["name"]}},
    {"name":"new_mcp","description":"Genera server.py boilerplate JSON-RPC 2.0 per un nuovo MCP Prismalux.","inputSchema":{"type":"object","properties":{"name":{"type":"string"},"tools":{"type":"array","items":{"type":"string"},"default":["tool_one","tool_two"]},"write":{"type":"boolean","default":False}},"required":["name"]}},
    {"name":"new_test","description":"Genera scheletro QTest per testare una classe C++ esistente.","inputSchema":{"type":"object","properties":{"class_name":{"type":"string"},"tested_class":{"type":"string","default":""},"include_file":{"type":"string","default":""},"write":{"type":"boolean","default":False}},"required":["class_name"]}},
    {"name":"list_snippets","description":"Elenca tutti i boilerplate e snippet disponibili con la loro firma.","inputSchema":{"type":"object","properties":{}}},
    {"name":"get_snippet","description":"Restituisce un frammento di codice Qt6 riusabile (connect_slot, dpi_widget, ai_request, qstring_i18n, path_root).","inputSchema":{"type":"object","properties":{"name":{"type":"string"}},"required":["name"]}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"snippet","version":"1.0.0"},"capabilities":{"tools":{}}})
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

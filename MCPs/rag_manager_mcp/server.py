#!/usr/bin/env python3
"""
rag_manager_mcp — Gestione documenti RAG e KNOWLEDGE_USER
==========================================================
Permette di esplorare, aggiungere e cercare nei documenti RAG
senza uscire dalla sessione Claude Code.

Tools esposti:
  list_docs       — lista tutti i documenti RAG (con dimensione e data)
  add_doc         — copia un file nella cartella RAG
  remove_doc      — rimuove un documento dal RAG (con conferma via dry_run)
  stats           — statistiche della cartella RAG (totale file, MB, tipi)
  search_content  — cerca testo nei contenuti dei documenti RAG (grep)
  list_knowledge  — lista i file in KNOWLEDGE_USER/
  save_knowledge  — salva un testo come file .md in KNOWLEDGE_USER/
  read_doc        — legge le prime N righe di un documento RAG
"""

import sys, json, os, subprocess, asyncio, logging, re, shutil
from pathlib import Path
from datetime import datetime
from typing import Any

logging.basicConfig(level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL","WARNING")))
logger = logging.getLogger(__name__)

ROOT     = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
RAG_DIR  = Path(os.environ.get("PRISMALUX_RAG_DIR",  str(ROOT / "RAG")))
KNW_DIR  = Path(os.environ.get("PRISMALUX_KNW_DIR",  str(ROOT / "KNOWLEDGE_USER")))
HOME_RAG = Path.home() / "prismalux_rag_docs"

SUPPORTED_EXTS = {".txt", ".md", ".pdf", ".csv", ".json", ".py", ".cpp", ".h",
                  ".html", ".xml", ".yaml", ".yml", ".rst", ".tex"}

# ─── Tool definitions ────────────────────────────────────────────────────────

TOOL_DEFS: list[dict[str, Any]] = [
    {
        "name": "list_docs",
        "description": "Lista tutti i documenti RAG presenti nelle cartelle RAG/ e ~/prismalux_rag_docs/.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "sort_by": {
                    "type": "string",
                    "description": "Ordinamento: 'name' | 'size' | 'date' (default: 'name')",
                    "default": "name",
                },
                "ext_filter": {
                    "type": "string",
                    "description": "Filtra per estensione (es. '.pdf', vuoto = tutti)",
                    "default": "",
                },
            },
        },
    },
    {
        "name": "add_doc",
        "description": "Aggiunge un documento alla cartella RAG/ copiandolo dal path specificato.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "source_path": {
                    "type": "string",
                    "description": "Path del file da aggiungere al RAG",
                },
                "subfolder": {
                    "type": "string",
                    "description": "Sottocartella dentro RAG/ (opzionale, es. 'papers')",
                    "default": "",
                },
                "overwrite": {
                    "type": "boolean",
                    "description": "Se true, sovrascrive se esiste già. Default: false",
                    "default": False,
                },
            },
            "required": ["source_path"],
        },
    },
    {
        "name": "remove_doc",
        "description": "Rimuove un documento dalla cartella RAG/. Usa dry_run=true per vedere cosa verrebbe rimosso senza farlo davvero.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "filename": {
                    "type": "string",
                    "description": "Nome del file da rimuovere (es. 'paper.pdf' o path relativo a RAG/)",
                },
                "dry_run": {
                    "type": "boolean",
                    "description": "Se true, mostra cosa verrebbe rimosso senza rimuoverlo. Default: true",
                    "default": True,
                },
            },
            "required": ["filename"],
        },
    },
    {
        "name": "stats",
        "description": "Statistiche della cartella RAG: totale file, dimensione, tipi presenti.",
        "inputSchema": {"type": "object", "properties": {}},
    },
    {
        "name": "search_content",
        "description": "Cerca testo nei file di testo della cartella RAG (grep ricorsivo, case-insensitive).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "query": {
                    "type": "string",
                    "description": "Testo da cercare",
                },
                "max_results": {
                    "type": "integer",
                    "description": "Numero massimo di righe risultato (default: 30)",
                    "default": 30,
                },
                "context_lines": {
                    "type": "integer",
                    "description": "Righe di contesto attorno al match (default: 1)",
                    "default": 1,
                },
            },
            "required": ["query"],
        },
    },
    {
        "name": "list_knowledge",
        "description": "Lista i file di conoscenza personalizzata in KNOWLEDGE_USER/.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "tag_filter": {
                    "type": "string",
                    "description": "Filtra file che contengono questo tag nel frontmatter (es. '#python')",
                    "default": "",
                },
            },
        },
    },
    {
        "name": "save_knowledge",
        "description": "Salva un testo come file Markdown in KNOWLEDGE_USER/ con frontmatter.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "title": {
                    "type": "string",
                    "description": "Titolo del documento (diventa anche nome file)",
                },
                "content": {
                    "type": "string",
                    "description": "Contenuto del documento in Markdown",
                },
                "tags": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Lista di tag (es. ['python', 'note']). Default: []",
                    "default": [],
                },
            },
            "required": ["title", "content"],
        },
    },
    {
        "name": "read_doc",
        "description": "Legge le prime N righe di un documento RAG.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "filename": {
                    "type": "string",
                    "description": "Nome file (o path relativo a RAG/)",
                },
                "lines": {
                    "type": "integer",
                    "description": "Numero di righe da leggere (default: 50)",
                    "default": 50,
                },
            },
            "required": ["filename"],
        },
    },
]

# ─── Helpers ─────────────────────────────────────────────────────────────────

def _all_rag_dirs() -> list[Path]:
    dirs = [RAG_DIR]
    if HOME_RAG.exists() and HOME_RAG != RAG_DIR:
        dirs.append(HOME_RAG)
    return [d for d in dirs if d.exists()]

def _all_docs(ext_filter: str = "") -> list[dict]:
    docs = []
    for d in _all_rag_dirs():
        for f in sorted(d.rglob("*")):
            if f.is_file():
                if ext_filter and f.suffix.lower() != ext_filter.lower():
                    continue
                try:
                    stat = f.stat()
                    docs.append({
                        "path": f,
                        "name": f.name,
                        "rel":  str(f.relative_to(d.parent)),
                        "size": stat.st_size,
                        "mtime": stat.st_mtime,
                    })
                except OSError:
                    pass
    return docs

def _find_doc(filename: str) -> Path | None:
    for d in _all_rag_dirs():
        candidate = d / filename
        if candidate.exists():
            return candidate
        for f in d.rglob(filename):
            return f
    return None

def _slug(title: str) -> str:
    slug = re.sub(r"[^\w\s-]", "", title.lower())
    slug = re.sub(r"[\s_-]+", "_", slug).strip("_")
    return slug[:60] or "documento"

# ─── Handlers ────────────────────────────────────────────────────────────────

def handle_list_docs(args: dict) -> str:
    sort_by    = args.get("sort_by", "name").strip()
    ext_filter = args.get("ext_filter", "").strip()

    docs = _all_docs(ext_filter)
    if not docs:
        rag_list = ", ".join(str(d) for d in _all_rag_dirs())
        return f"Nessun documento RAG trovato in: {rag_list}"

    if sort_by == "size":
        docs.sort(key=lambda d: d["size"], reverse=True)
    elif sort_by == "date":
        docs.sort(key=lambda d: d["mtime"], reverse=True)
    else:
        docs.sort(key=lambda d: d["name"].lower())

    lines = [f"📁 Documenti RAG ({len(docs)} file):\n"]
    for d in docs:
        size_kb = d["size"] / 1024
        date    = datetime.fromtimestamp(d["mtime"]).strftime("%Y-%m-%d")
        size_s  = f"{size_kb/1024:.1f}MB" if size_kb > 1024 else f"{size_kb:.0f}KB"
        lines.append(f"  {d['name']:45s} {size_s:8s}  {date}")
    return "\n".join(lines)


def handle_add_doc(args: dict) -> str:
    src_str    = args.get("source_path", "").strip()
    subfolder  = args.get("subfolder", "").strip()
    overwrite  = bool(args.get("overwrite", False))

    if not src_str:
        return "Specifica 'source_path'."

    src = Path(src_str)
    if not src.is_absolute():
        src = ROOT / src
    if not src.exists():
        return f"File non trovato: {src}"
    if src.suffix.lower() not in SUPPORTED_EXTS:
        return f"Tipo non supportato: {src.suffix}. Supportati: {', '.join(sorted(SUPPORTED_EXTS))}"

    RAG_DIR.mkdir(parents=True, exist_ok=True)
    dest_dir = RAG_DIR / subfolder if subfolder else RAG_DIR
    dest_dir.mkdir(parents=True, exist_ok=True)
    dest = dest_dir / src.name

    if dest.exists() and not overwrite:
        return f"⚠️  File già presente: {dest.relative_to(ROOT)}\nUsa overwrite=true per sovrascrivere."

    shutil.copy2(str(src), str(dest))
    size_kb = dest.stat().st_size / 1024
    return f"✅ Aggiunto al RAG: {dest.relative_to(ROOT)} ({size_kb:.0f} KB)"


def handle_remove_doc(args: dict) -> str:
    filename = args.get("filename", "").strip()
    dry_run  = bool(args.get("dry_run", True))

    if not filename:
        return "Specifica 'filename'."

    target = _find_doc(filename)
    if not target:
        return f"Documento non trovato: '{filename}'"

    if dry_run:
        return f"🔍 dry_run=true — verrebbe rimosso:\n  {target}\n\nUsa dry_run=false per rimuoverlo davvero."

    target.unlink()
    return f"✅ Rimosso: {target.name}"


def handle_stats(args: dict) -> str:
    docs = _all_docs()
    if not docs:
        return "Nessun documento RAG trovato."

    total_size = sum(d["size"] for d in docs)
    from collections import Counter
    ext_counts = Counter(Path(d["name"]).suffix.lower() for d in docs)

    lines = [
        f"📊 Statistiche RAG\n",
        f"  Documenti totali : {len(docs)}",
        f"  Dimensione totale: {total_size/1_048_576:.1f} MB",
        f"\n  Tipi di file:",
    ]
    for ext, cnt in ext_counts.most_common():
        bar = "▓" * min(cnt, 30)
        lines.append(f"    {ext or '(no ext)':8s}  {cnt:3d}x  {bar}")

    lines.append(f"\n  Cartelle monitorate:")
    for d in _all_rag_dirs():
        lines.append(f"    📂 {d}")

    return "\n".join(lines)


def handle_search_content(args: dict) -> str:
    query        = args.get("query", "").strip()
    max_results  = int(args.get("max_results", 30))
    context_lines = int(args.get("context_lines", 1))

    if not query:
        return "Specifica 'query'."

    text_exts = ["*.txt", "*.md", "*.csv", "*.json", "*.py", "*.cpp",
                 "*.h", "*.html", "*.yaml", "*.yml", "*.rst"]

    results = []
    for d in _all_rag_dirs():
        for ext_glob in text_exts:
            try:
                r = subprocess.run(
                    ["grep", "-ri", "-l", query, "--include=" + ext_glob, str(d)],
                    capture_output=True, text=True, timeout=15
                )
                for fpath in r.stdout.splitlines():
                    if fpath not in results:
                        results.append(fpath)
            except Exception:
                pass

    if not results:
        return f"Nessun match per '{query}' nei documenti RAG."

    output_lines = [f"Match per '{query}' in {len(results)} file:\n"]
    count = 0
    for fpath in results:
        if count >= max_results:
            output_lines.append(f"\n*(troncato — ci sono altri risultati)*")
            break
        p = Path(fpath)
        output_lines.append(f"── {p.name} ──")
        try:
            r = subprocess.run(
                ["grep", "-n", "-i", f"-C{context_lines}", query, fpath],
                capture_output=True, text=True, timeout=10
            )
            for line in r.stdout.splitlines()[:10]:
                output_lines.append("  " + line[:120])
                count += 1
        except Exception:
            output_lines.append("  (errore lettura)")
        output_lines.append("")

    return "\n".join(output_lines)


def handle_list_knowledge(args: dict) -> str:
    tag_filter = args.get("tag_filter", "").strip()

    KNW_DIR.mkdir(parents=True, exist_ok=True)
    files = sorted(KNW_DIR.glob("*.md"))

    if tag_filter:
        matching = []
        for f in files:
            try:
                content = f.read_text(errors="replace")[:500]
                if tag_filter.lower() in content.lower():
                    matching.append(f)
            except OSError:
                pass
        files = matching

    if not files:
        msg = f"Nessun documento in KNOWLEDGE_USER/"
        if tag_filter:
            msg += f" con tag '{tag_filter}'"
        return msg

    lines = [f"📚 KNOWLEDGE_USER/ ({len(files)} file):\n"]
    for f in files:
        size_kb = f.stat().st_size / 1024
        date    = datetime.fromtimestamp(f.stat().st_mtime).strftime("%Y-%m-%d")
        # Estrai titolo dal frontmatter se presente
        title   = f.stem
        try:
            first = f.read_text(errors="replace")[:200]
            m = re.search(r"^title:\s*(.+)$", first, re.MULTILINE)
            if m:
                title = m.group(1).strip()
        except OSError:
            pass
        lines.append(f"  📄 {f.name:40s} {size_kb:.0f}KB  {date}  {title[:40]}")
    return "\n".join(lines)


def handle_save_knowledge(args: dict) -> str:
    title   = args.get("title", "").strip()
    content = args.get("content", "").strip()
    tags    = args.get("tags", [])

    if not title:
        return "Specifica 'title'."
    if not content:
        return "Specifica 'content'."

    KNW_DIR.mkdir(parents=True, exist_ok=True)
    filename = _slug(title) + ".md"
    dest = KNW_DIR / filename

    tags_str   = ", ".join(f"#{t.lstrip('#')}" for t in tags) if tags else ""
    date_str   = datetime.now().strftime("%Y-%m-%d")
    frontmatter = f"---\ntitle: {title}\ndate: {date_str}\ntags: [{tags_str}]\n---\n\n"

    dest.write_text(frontmatter + content, encoding="utf-8")
    return f"✅ Salvato: {filename} ({len(content)} caratteri)"


def handle_read_doc(args: dict) -> str:
    filename = args.get("filename", "").strip()
    lines_n  = min(int(args.get("lines", 50)), 200)

    if not filename:
        return "Specifica 'filename'."

    target = _find_doc(filename)
    if not target:
        return f"Documento non trovato: '{filename}'"

    if target.suffix.lower() == ".pdf":
        return f"Il file è un PDF ({target.name}). Usa il tool File AI di Prismalux per leggerne il contenuto."

    try:
        text = target.read_text(errors="replace")
        file_lines = text.splitlines()
        shown  = file_lines[:lines_n]
        trunc  = len(file_lines) > lines_n
        header = f"📄 {target.name} ({len(file_lines)} righe totali)\n\n"
        body   = "\n".join(shown)
        footer = f"\n\n*(troncato a {lines_n} righe su {len(file_lines)})*" if trunc else ""
        return header + "```\n" + body + "\n```" + footer
    except Exception as e:
        return f"Errore lettura: {e}"


HANDLERS = {
    "list_docs":       handle_list_docs,
    "add_doc":         handle_add_doc,
    "remove_doc":      handle_remove_doc,
    "stats":           handle_stats,
    "search_content":  handle_search_content,
    "list_knowledge":  handle_list_knowledge,
    "save_knowledge":  handle_save_knowledge,
    "read_doc":        handle_read_doc,
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
            resp = _rpc_result(req_id,{"protocolVersion":"2024-11-05","serverInfo":{"name":"rag-manager","version":"1.0.0"},"capabilities":{"tools":{}}})
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

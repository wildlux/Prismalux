#!/usr/bin/env python3
"""
ocr_mcp — OCR per PDF scansionati → testo → RAG
================================================
Usa Tesseract (via pytesseract o subprocess) + pdf2image per PDF,
PIL per immagini. Copia il testo estratto in RAG/ per indicizzazione.

Tools:
  check_tools      — verifica installazione Tesseract e pdf2image
  ocr_image        — OCR su file immagine (PNG/JPG/TIFF)
  ocr_pdf          — OCR su PDF scansionato
  ocr_to_rag       — OCR + salva testo in RAG/
  list_rag_pdfs    — PDF nel RAG che potrebbero beneficiare di OCR
  batch_ocr        — OCR batch su tutti i PDF di una cartella
"""
import sys, json, os, re, asyncio, subprocess
from pathlib import Path
from typing import Any

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
RAG_DIR = ROOT / "RAG"

def _run(cmd: list[str], timeout: int = 120) -> tuple[int, str]:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return r.returncode, (r.stdout + r.stderr).strip()
    except (FileNotFoundError, subprocess.TimeoutExpired) as e:
        return -1, str(e)

def _tesseract_ok() -> bool:
    c, _ = _run(["tesseract", "--version"])
    return c == 0

def _pdf2image_ok() -> bool:
    try:
        import pdf2image
        return True
    except ImportError:
        c, _ = _run(["pdftoppm", "-v"])
        return c != -1

def _ocr_image_path(path: Path, lang: str = "ita+eng") -> str:
    """OCR su singola immagine — prova pytesseract poi subprocess."""
    try:
        import pytesseract
        from PIL import Image
        img  = Image.open(path)
        text = pytesseract.image_to_string(img, lang=lang)
        return text
    except ImportError:
        pass
    # Fallback subprocess
    out_base = Path("/tmp/ocr_prismalux_out")
    c, err = _run(["tesseract", str(path), str(out_base), "-l", lang, "txt"])
    out_file = Path(str(out_base) + ".txt")
    if out_file.exists():
        text = out_file.read_text(errors="replace")
        out_file.unlink(missing_ok=True)
        return text
    return f"[Errore tesseract: {err}]"

def _ocr_pdf_path(path: Path, lang: str = "ita+eng", dpi: int = 300) -> str:
    """OCR su PDF — converte pagine in immagini poi OCR."""
    # Prima prova pdftotext (per PDF con testo embedded)
    c, text = _run(["pdftotext", "-layout", str(path), "-"])
    if c == 0 and text.strip() and len(text.strip()) > 100:
        return f"[Testo estratto direttamente — PDF non scansionato]\n\n{text}"

    # PDF scansionato → immagini → tesseract
    try:
        from pdf2image import convert_from_path
        pages = convert_from_path(str(path), dpi=dpi)
        all_text = []
        for i, page in enumerate(pages):
            tmp = Path(f"/tmp/ocr_page_{i}.png")
            page.save(str(tmp), "PNG")
            text = _ocr_image_path(tmp, lang)
            tmp.unlink(missing_ok=True)
            all_text.append(f"--- Pagina {i+1} ---\n{text}")
        return "\n\n".join(all_text)
    except ImportError:
        pass

    # Fallback pdftoppm
    tmp_prefix = "/tmp/ocr_pdf_page"
    c, err = _run(["pdftoppm", "-r", str(dpi), "-png", str(path), tmp_prefix], timeout=180)
    if c != 0:
        return f"pdf2image e pdftoppm non disponibili. Installa:\n  sudo apt install poppler-utils\n  pip install pdf2image"
    import glob
    pages_files = sorted(glob.glob(f"{tmp_prefix}*.png"))
    all_text = []
    for i, pf in enumerate(pages_files):
        text = _ocr_image_path(Path(pf), lang)
        all_text.append(f"--- Pagina {i+1} ---\n{text}")
        Path(pf).unlink(missing_ok=True)
    return "\n\n".join(all_text) if all_text else "Nessun testo estratto."

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_check_tools(args: dict) -> str:
    lines = ["🔍 Verifica tool OCR\n"]
    # Tesseract
    c, v = _run(["tesseract", "--version"])
    if c == 0:
        lines.append(f"  ✅ Tesseract: {v.splitlines()[0]}")
        c2, langs = _run(["tesseract", "--list-langs"])
        lang_list = [l for l in langs.splitlines() if not l.startswith("List")][:10]
        lines.append(f"     Lingue: {', '.join(lang_list)}")
    else:
        lines.append("  ❌ Tesseract: non installato\n     sudo apt install tesseract-ocr tesseract-ocr-ita tesseract-ocr-eng")
    # pdf2image
    try:
        import pdf2image
        lines.append(f"  ✅ pdf2image Python: disponibile")
    except ImportError:
        lines.append("  ⚠️ pdf2image Python: non installato (pip install pdf2image)")
    # poppler
    c2, _ = _run(["pdftoppm", "-v"])
    lines.append(f"  {'✅' if c2==0 else '❌'} poppler-utils (pdftoppm): {'disponibile' if c2==0 else 'sudo apt install poppler-utils'}")
    # pytesseract
    try:
        import pytesseract
        lines.append("  ✅ pytesseract Python: disponibile")
    except ImportError:
        lines.append("  ⚠️ pytesseract Python: non installato (pip install pytesseract)")
    lines.append("\nInstallazione completa:\n"
                 "  sudo apt install tesseract-ocr tesseract-ocr-ita poppler-utils\n"
                 "  pip install pytesseract pdf2image Pillow")
    return "\n".join(lines)

def handle_ocr_image(args: dict) -> str:
    path_str = args.get("path","").strip()
    lang     = args.get("lang","ita+eng")
    if not path_str: return "Specifica 'path'."
    if not _tesseract_ok():
        return "Tesseract non installato. sudo apt install tesseract-ocr tesseract-ocr-ita"
    path = Path(path_str) if Path(path_str).is_absolute() else ROOT / path_str
    if not path.exists(): return f"File non trovato: {path}"
    text = _ocr_image_path(path, lang)
    chars = len(text.strip())
    return f"📸 OCR: {path.name} ({chars} caratteri)\n\n{text[:2000]}"

def handle_ocr_pdf(args: dict) -> str:
    path_str = args.get("path","").strip()
    lang     = args.get("lang","ita+eng")
    dpi      = int(args.get("dpi", 300))
    if not path_str: return "Specifica 'path'."
    if not _tesseract_ok():
        return "Tesseract non installato. sudo apt install tesseract-ocr tesseract-ocr-ita"
    path = Path(path_str) if Path(path_str).is_absolute() else ROOT / path_str
    if not path.exists(): return f"File non trovato: {path}"
    text  = _ocr_pdf_path(path, lang, dpi)
    chars = len(text.strip())
    return f"📄 OCR PDF: {path.name} ({chars} caratteri)\n\n{text[:3000]}"

def handle_ocr_to_rag(args: dict) -> str:
    path_str = args.get("path","").strip()
    lang     = args.get("lang","ita+eng")
    dpi      = int(args.get("dpi", 300))
    if not path_str: return "Specifica 'path'."
    path = Path(path_str) if Path(path_str).is_absolute() else ROOT / path_str
    if not path.exists(): return f"File non trovato: {path}"
    RAG_DIR.mkdir(parents=True, exist_ok=True)
    if path.suffix.lower() == ".pdf":
        text = _ocr_pdf_path(path, lang, dpi)
    else:
        text = _ocr_image_path(path, lang)
    if not text.strip():
        return "⚠️ Nessun testo estratto. Il file potrebbe essere vuoto o illeggibile."
    out_file = RAG_DIR / (path.stem + "_ocr.txt")
    out_file.write_text(text, encoding="utf-8")
    return (f"✅ OCR completato e salvato in RAG/\n"
            f"  Sorgente: {path.name}\n"
            f"  Output:   {out_file.name} ({len(text)} caratteri)\n\n"
            f"Il file è ora disponibile per l'indicizzazione RAG.")

def handle_list_rag_pdfs(args: dict) -> str:
    pdfs = list(RAG_DIR.glob("*.pdf")) if RAG_DIR.exists() else []
    if not pdfs: return f"Nessun PDF in {RAG_DIR}"
    lines = [f"📄 PDF nel RAG ({len(pdfs)} file)\n"]
    for p in sorted(pdfs):
        has_ocr = (RAG_DIR / (p.stem + "_ocr.txt")).exists()
        # Prova a determinare se ha testo
        c, preview = _run(["pdftotext", str(p), "-"], timeout=5)
        has_text = c == 0 and len(preview.strip()) > 50
        status = "✅ testo embedded" if has_text else ("🔵 OCR già fatto" if has_ocr else "🔴 scansionato — OCR consigliato")
        size   = p.stat().st_size / 1024
        lines.append(f"  {status}\n    {p.name} ({size:.0f} KB)")
    return "\n".join(lines)

def handle_batch_ocr(args: dict) -> str:
    directory = args.get("directory", str(RAG_DIR))
    lang      = args.get("lang","ita+eng")
    dpi       = int(args.get("dpi", 300))
    if not _tesseract_ok():
        return "Tesseract non installato. sudo apt install tesseract-ocr tesseract-ocr-ita"
    d = Path(directory) if Path(directory).is_absolute() else ROOT / directory
    if not d.exists(): return f"Directory non trovata: {d}"
    pdfs = list(d.glob("*.pdf")) + list(d.glob("*.PDF"))
    if not pdfs: return f"Nessun PDF in {d}"
    results = []
    for p in pdfs:
        out = d / (p.stem + "_ocr.txt")
        if out.exists():
            results.append(f"  ⏭️ {p.name} — già processato ({out.name})")
            continue
        try:
            text = _ocr_pdf_path(p, lang, dpi)
            if text.strip():
                out.write_text(text, encoding="utf-8")
                results.append(f"  ✅ {p.name} → {out.name} ({len(text)} car.)")
            else:
                results.append(f"  ⚠️ {p.name} — nessun testo estratto")
        except Exception as e:
            results.append(f"  ❌ {p.name} — {e}")
    return f"Batch OCR completato ({len(pdfs)} PDF):\n" + "\n".join(results)

HANDLERS = {
    "check_tools":    handle_check_tools,
    "ocr_image":      handle_ocr_image,
    "ocr_pdf":        handle_ocr_pdf,
    "ocr_to_rag":     handle_ocr_to_rag,
    "list_rag_pdfs":  handle_list_rag_pdfs,
    "batch_ocr":      handle_batch_ocr,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"check_tools","description":"Verifica che Tesseract, pdf2image e pytesseract siano installati.","inputSchema":{"type":"object","properties":{}}},
    {"name":"ocr_image","description":"Estrae testo da un file immagine (PNG/JPG/TIFF) con Tesseract.","inputSchema":{"type":"object","properties":{"path":{"type":"string"},"lang":{"type":"string","default":"ita+eng"}},"required":["path"]}},
    {"name":"ocr_pdf","description":"Estrae testo da un PDF scansionato convertendo le pagine in immagini.","inputSchema":{"type":"object","properties":{"path":{"type":"string"},"lang":{"type":"string","default":"ita+eng"},"dpi":{"type":"integer","default":300}},"required":["path"]}},
    {"name":"ocr_to_rag","description":"Esegue OCR su un file e salva il testo in RAG/ per l'indicizzazione.","inputSchema":{"type":"object","properties":{"path":{"type":"string"},"lang":{"type":"string","default":"ita+eng"},"dpi":{"type":"integer","default":300}},"required":["path"]}},
    {"name":"list_rag_pdfs","description":"Elenca i PDF nel RAG indicando se hanno testo embedded o necessitano OCR.","inputSchema":{"type":"object","properties":{}}},
    {"name":"batch_ocr","description":"OCR batch su tutti i PDF di una directory (default: RAG/).","inputSchema":{"type":"object","properties":{"directory":{"type":"string","default":""},"lang":{"type":"string","default":"ita+eng"},"dpi":{"type":"integer","default":300}}}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"ocr","version":"1.0.0"},"capabilities":{"tools":{}}})
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

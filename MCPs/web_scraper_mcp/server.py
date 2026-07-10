#!/usr/bin/env python3
"""
web_scraper_mcp — Web scraping strutturato per RAG Prismalux
=============================================================
Scarica pagine web, estrae testo pulito (no boilerplate/ads),
salva in RAG/ per indicizzazione. Supporta Wikipedia, arXiv
abstract, pagine generiche. Tutto stdlib + html.parser.

Tools:
  fetch_page        — scarica e pulisce una pagina web
  fetch_to_rag      — scarica pagina e salva testo in RAG/
  fetch_wikipedia   — articolo Wikipedia in testo pulito
  fetch_arxiv       — abstract + info paper arXiv da ID
  batch_fetch       — scarica lista URL in sequenza
  check_url         — verifica raggiungibilità URL (HEAD request)
"""
import sys, json, os, re, asyncio, time
import urllib.request, urllib.error, urllib.parse
from html.parser import HTMLParser
from pathlib import Path
from datetime import date
from typing import Any

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from _shared.net_safety import validate_url as _validate_url, build_safe_opener

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
RAG_DIR = ROOT / "RAG"

UA = "Mozilla/5.0 (compatible; PrismaluxBot/1.0; +https://github.com/wildlux/Prismalux)"

# ─── protezione SSRF ─────────────────────────────────────────────────────────
# Questo MCP può essere chiamato da un agente autonomo istruito da un task
# testuale che include contenuto letto da file/web non fidato (prompt
# injection): senza questi controlli, "fetch_page(url='file:///etc/passwd')"
# o un URL che risolve a 169.254.169.254/127.0.0.1/192.168.x.x veniva scaricato
# senza restrizioni ed esfiltrato nel risultato del tool. Validazione e
# redirect handler ora in _shared/net_safety.py (OS-18) — prima duplicati
# qui e in streamlink_mcp/api_tester_mcp, con rischio di drift (successo
# già una volta: la vecchia _validate_url di streamlink_mcp era più debole).
_SAFE_OPENER = build_safe_opener()

# ─── HTML text extractor ─────────────────────────────────────────────────────

class _TextExtractor(HTMLParser):
    SKIP_TAGS = {"script","style","nav","footer","header","aside","iframe",
                 "noscript","button","form","input","select","textarea","svg"}
    BLOCK_TAGS = {"p","div","h1","h2","h3","h4","h5","h6","li","br","td","th",
                  "blockquote","pre","article","section","main"}

    def __init__(self):
        super().__init__()
        self._skip = 0
        self._buf  = []
        self.title = ""
        self._in_title = False

    def handle_starttag(self, tag, attrs):
        if tag in self.SKIP_TAGS:   self._skip += 1
        if tag == "title":          self._in_title = True
        if tag in self.BLOCK_TAGS and self._buf and self._buf[-1] != "\n":
            self._buf.append("\n")

    def handle_endtag(self, tag):
        if tag in self.SKIP_TAGS and self._skip > 0: self._skip -= 1
        if tag == "title": self._in_title = False
        if tag in self.BLOCK_TAGS: self._buf.append("\n")

    def handle_data(self, data):
        if self._skip: return
        text = data.strip()
        if not text: return
        if self._in_title: self.title = text
        else: self._buf.append(text + " ")

    def get_text(self) -> str:
        raw = "".join(self._buf)
        # Comprimi spazi multipli e righe vuote
        lines = [l.strip() for l in raw.splitlines()]
        lines = [l for l in lines if l]
        # Rimuovi righe troppo corte (menu, breadcrumb ecc.)
        lines = [l for l in lines if len(l) > 15 or l.startswith("#")]
        return "\n".join(lines)

def _fetch(url: str, timeout: float = 15.0) -> tuple[int, str, str]:
    """Ritorna (status, content_text, final_url)."""
    err = _validate_url(url)
    if err:
        return 0, f"URL bloccato: {err}", url
    req = urllib.request.Request(url, headers={
        "User-Agent": UA,
        "Accept": "text/html,application/xhtml+xml,*/*",
        "Accept-Language": "it-IT,it;q=0.9,en;q=0.8",
    })
    try:
        with _SAFE_OPENER.open(req, timeout=timeout) as resp:
            charset  = resp.headers.get_content_charset() or "utf-8"
            raw      = resp.read(1_000_000)  # max 1 MB
            html     = raw.decode(charset, errors="replace")
            final    = resp.url
            parser   = _TextExtractor()
            parser.feed(html)
            return resp.status, parser.get_text(), final
    except urllib.error.HTTPError as e:
        return e.code, f"HTTP {e.code}: {e.reason}", url
    except Exception as e:
        return 0, str(e), url

def _slug(url: str) -> str:
    parsed = urllib.parse.urlparse(url)
    path   = parsed.path.strip("/").replace("/","_")[:50]
    host   = parsed.netloc.replace("www.","").replace(".","_")
    return re.sub(r'[^\w\-]','_', f"{host}_{path}")[:60]

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_check_url(args: dict) -> str:
    url     = args.get("url","").strip()
    timeout = float(args.get("timeout_s", 8.0))
    if not url: return "Specifica 'url'."
    err = _validate_url(url)
    if err: return f"❌ {url}\n  Bloccato: {err}"
    req = urllib.request.Request(url, method="HEAD", headers={"User-Agent": UA})
    t0  = time.time()
    try:
        with _SAFE_OPENER.open(req, timeout=timeout) as r:
            ms = (time.time()-t0)*1000
            ct = r.headers.get("Content-Type","?")
            return (f"✅ {url}\n"
                    f"  Status: {r.status}  ({ms:.0f}ms)\n"
                    f"  Tipo:   {ct}")
    except Exception as e:
        return f"❌ {url}\n  {e}"

def handle_fetch_page(args: dict) -> str:
    url     = args.get("url","").strip()
    max_ch  = min(int(args.get("max_chars", 4000)), 20000)
    if not url: return "Specifica 'url'."
    status, text, final = _fetch(url)
    if status == 0:
        return f"❌ Impossibile scaricare {url}:\n  {text}"
    lines = [f"📄 {final}  (status: {status})\n"]
    lines.append(text[:max_ch])
    if len(text) > max_ch:
        lines.append(f"\n[... troncato a {max_ch} caratteri su {len(text)} totali]")
    return "\n".join(lines)

def handle_fetch_to_rag(args: dict) -> str:
    url      = args.get("url","").strip()
    filename = args.get("filename","").strip()
    if not url: return "Specifica 'url'."
    status, text, final = _fetch(url)
    if status == 0 or not text.strip():
        return f"❌ Impossibile scaricare o testo vuoto: {url}\n  {text[:200]}"
    RAG_DIR.mkdir(parents=True, exist_ok=True)
    fname = filename or f"{_slug(url)}_{date.today()}.txt"
    # basename() scarta qualunque componente di percorso (../, path assoluti):
    # "filename" arriva da un parametro JSON-RPC, un valore tipo
    # "../../../../.bashrc" scriveva fuori da RAG_DIR prima di questo fix.
    fname = os.path.basename(fname.strip())
    if not fname or fname in (".", ".."):
        fname = f"{_slug(url)}_{date.today()}.txt"
    if not fname.endswith(".txt"): fname += ".txt"
    out = (RAG_DIR / fname).resolve()
    if not str(out).startswith(str(RAG_DIR.resolve()) + os.sep):
        return f"❌ Nome file non valido: {filename!r}"
    header = f"Fonte: {final}\nData: {date.today()}\nCaratteri: {len(text)}\n{'─'*60}\n\n"
    out.write_text(header + text, encoding="utf-8")
    return (f"✅ Salvato in RAG/\n"
            f"  URL:    {final}\n"
            f"  File:   {out.name}\n"
            f"  Testo:  {len(text):,} caratteri\n"
            f"  Preview: {text[:200]}")

def handle_fetch_wikipedia(args: dict) -> str:
    topic   = args.get("topic","").strip()
    lang    = args.get("lang","it").strip()
    to_rag  = bool(args.get("save_to_rag", False))
    max_ch  = min(int(args.get("max_chars", 6000)), 20000)
    if not topic: return "Specifica 'topic'."
    slug  = urllib.parse.quote(topic.replace(" ","_"))
    url   = f"https://{lang}.wikipedia.org/wiki/{slug}"
    status, text, final = _fetch(url)
    if status == 0:
        return f"❌ Wikipedia non raggiungibile: {url}\n  {text}"
    if status == 404:
        # Prova ricerca
        search_url = f"https://{lang}.wikipedia.org/w/index.php?search={urllib.parse.quote(topic)}"
        return f"❌ Pagina non trovata: {url}\n  Prova: {search_url}"
    # Rimuovi righe di navigazione tipiche di Wikipedia
    lines = [l for l in text.splitlines()
             if not any(skip in l.lower() for skip in
                        ["modifica","modifica wikitesto","wikimedia","categor"])]
    text = "\n".join(lines)
    if to_rag:
        RAG_DIR.mkdir(parents=True, exist_ok=True)
        fname = f"wikipedia_{_slug(topic)}_{date.today()}.txt"
        out   = RAG_DIR / fname
        out.write_text(f"Wikipedia: {topic}\nURL: {final}\nData: {date.today()}\n\n{text}", encoding="utf-8")
        return f"✅ Wikipedia '{topic}' salvato in RAG/{fname} ({len(text):,} car.)"
    return f"📖 Wikipedia: {topic}\nURL: {final}\n\n{text[:max_ch]}"

def handle_fetch_arxiv(args: dict) -> str:
    arxiv_id = args.get("arxiv_id","").strip()
    to_rag   = bool(args.get("save_to_rag", False))
    if not arxiv_id: return "Specifica 'arxiv_id' (es. '2310.12345' o URL arXiv)."
    # Estrai ID da URL se necessario
    m = re.search(r'(\d{4}\.\d{4,5}(?:v\d+)?)', arxiv_id)
    if not m: return f"ID arXiv non valido: {arxiv_id}"
    aid = m.group(1)
    # Usa API arXiv
    api_url = f"https://export.arxiv.org/abs/{aid}"
    status, text, _ = _fetch(api_url)
    if status == 0:
        return f"❌ arXiv non raggiungibile: {api_url}"
    # Prova anche l'API XML
    xml_url = f"https://export.arxiv.org/api/query?id_list={aid}"
    try:
        err = _validate_url(xml_url)
        if err: raise urllib.error.URLError(err)
        req = urllib.request.Request(xml_url, headers={"User-Agent": UA})
        with _SAFE_OPENER.open(req, timeout=10) as r:
            xml = r.read().decode("utf-8", errors="replace")
        title   = re.search(r'<title>([^<]+)</title>', xml, re.DOTALL)
        summary = re.search(r'<summary>([^<]+)</summary>', xml, re.DOTALL)
        authors = re.findall(r'<name>([^<]+)</name>', xml)
        pub     = re.search(r'<published>([^<]+)</published>', xml)
        title_s   = title.group(1).strip() if title else "?"
        summary_s = summary.group(1).strip() if summary else text[:500]
        auth_s    = ", ".join(authors[:5]) + (f" +{len(authors)-5}" if len(authors)>5 else "")
        pub_s     = pub.group(1)[:10] if pub else "?"
        result = (f"📄 arXiv: {aid}\n"
                  f"  Titolo:  {title_s}\n"
                  f"  Autori:  {auth_s}\n"
                  f"  Data:    {pub_s}\n"
                  f"  URL:     https://arxiv.org/abs/{aid}\n\n"
                  f"Abstract:\n{summary_s}")
        if to_rag:
            RAG_DIR.mkdir(parents=True, exist_ok=True)
            fname = f"arxiv_{aid.replace('/','_')}_{date.today()}.txt"
            (RAG_DIR / fname).write_text(result, encoding="utf-8")
            return result + f"\n\n✅ Salvato in RAG/{fname}"
        return result
    except Exception as e:
        return f"⚠️ API XML fallita ({e}). Estratto dalla pagina:\n\n{text[:2000]}"

def handle_batch_fetch(args: dict) -> str:
    urls    = args.get("urls",[])
    to_rag  = bool(args.get("save_to_rag", False))
    delay   = float(args.get("delay_s", 1.0))
    if not urls or not isinstance(urls, list):
        return "Passa 'urls': lista di URL."
    results = []
    for url in urls[:20]:
        url = str(url).strip()
        status, text, final = _fetch(url)
        if status > 0 and text.strip():
            chars = len(text)
            if to_rag:
                RAG_DIR.mkdir(parents=True, exist_ok=True)
                fname = f"{_slug(url)}_{date.today()}.txt"
                (RAG_DIR / fname).write_text(
                    f"Fonte: {final}\nData: {date.today()}\n\n{text}", encoding="utf-8"
                )
                results.append(f"  ✅ {url[:60]}\n     → RAG/{fname} ({chars:,} car.)")
            else:
                results.append(f"  ✅ {url[:60]}\n     {chars:,} caratteri — {text[:100]}")
        else:
            results.append(f"  ❌ {url[:60]}\n     {text[:80]}")
        if delay > 0: time.sleep(delay)
    pfx = "(salvati in RAG/) " if to_rag else ""
    return f"🌐 Batch fetch {pfx}{len(urls)} URL:\n\n" + "\n".join(results)

HANDLERS = {
    "check_url":        handle_check_url,
    "fetch_page":       handle_fetch_page,
    "fetch_to_rag":     handle_fetch_to_rag,
    "fetch_wikipedia":  handle_fetch_wikipedia,
    "fetch_arxiv":      handle_fetch_arxiv,
    "batch_fetch":      handle_batch_fetch,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"check_url","description":"Verifica raggiungibilità di un URL (HEAD request con timing).","inputSchema":{"type":"object","properties":{"url":{"type":"string"},"timeout_s":{"type":"number","default":8}},"required":["url"]}},
    {"name":"fetch_page","description":"Scarica e pulisce il testo di una pagina web (rimuove script/nav/footer).","inputSchema":{"type":"object","properties":{"url":{"type":"string"},"max_chars":{"type":"integer","default":4000}},"required":["url"]}},
    {"name":"fetch_to_rag","description":"Scarica una pagina web e salva il testo pulito in RAG/ per indicizzazione.","inputSchema":{"type":"object","properties":{"url":{"type":"string"},"filename":{"type":"string","default":""}},"required":["url"]}},
    {"name":"fetch_wikipedia","description":"Scarica un articolo Wikipedia in testo pulito (opzionale: save_to_rag=true).","inputSchema":{"type":"object","properties":{"topic":{"type":"string"},"lang":{"type":"string","default":"it"},"save_to_rag":{"type":"boolean","default":False},"max_chars":{"type":"integer","default":6000}},"required":["topic"]}},
    {"name":"fetch_arxiv","description":"Scarica abstract e metadati di un paper arXiv (opzionale: save_to_rag=true).","inputSchema":{"type":"object","properties":{"arxiv_id":{"type":"string"},"save_to_rag":{"type":"boolean","default":False}},"required":["arxiv_id"]}},
    {"name":"batch_fetch","description":"Scarica una lista di URL in sequenza (opzionale: salva tutti in RAG/).","inputSchema":{"type":"object","properties":{"urls":{"type":"array","items":{"type":"string"}},"save_to_rag":{"type":"boolean","default":False},"delay_s":{"type":"number","default":1.0}},"required":["urls"]}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"web-scraper","version":"1.0.0"},"capabilities":{"tools":{}}})
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

#!/usr/bin/env python3
"""
streamlink_mcp — Prismalux MCP per stream live e video online.

Cattura stream da YouTube, Twitch, Dailymotion e 1000+ siti via yt-dlp/streamlink.
Integrato con la pipeline audio/video Prismalux (VAD → Whisper → video_caption).

Strumenti disponibili:
  stream_info       — metadati stream (titolo, durata, qualità, piattaforma)
  stream_capture    — cattura N secondi in file WAV/MP4/MKV
  stream_download   — scarica l'intero video nella qualità scelta

Dipendenze:
  pip install yt-dlp
  pip install streamlink   (opzionale — fallback per siti non supportati da yt-dlp)

Comunicazione: JSON-RPC 2.0 su stdio (protocollo MCP standard).
"""

import sys
import json
import os
import subprocess
import shutil
import tempfile
import logging
from pathlib import Path

logging.basicConfig(
    level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL", "WARNING")),
    format="%(asctime)s [streamlink_mcp] %(levelname)s %(message)s",
)
logger = logging.getLogger(__name__)

# ─── Utility I/O JSON-RPC ────────────────────────────────────────────────────

def _send(obj: dict) -> None:
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()

def _result(req_id, result: dict) -> None:
    _send({"jsonrpc": "2.0", "id": req_id, "result": result})

def _error(req_id, code: int, msg: str) -> None:
    _send({"jsonrpc": "2.0", "id": req_id, "error": {"code": code, "message": msg}})

def _text_result(req_id, text: str, is_error: bool = False) -> None:
    _result(req_id, {
        "content": [{"type": "text", "text": text}],
        "isError": is_error,
    })

# ─── Rilevamento tool esterni ────────────────────────────────────────────────

def _find_ytdlp() -> str | None:
    """Cerca yt-dlp nell'ambiente corrente."""
    return shutil.which("yt-dlp")

def _find_ffmpeg() -> str | None:
    return shutil.which("ffmpeg")

def _find_streamlink() -> str | None:
    return shutil.which("streamlink")

# ─── Validazione URL ─────────────────────────────────────────────────────────

def _validate_url(url: str) -> str | None:
    """Restituisce errore se l'URL non è http/https, None se OK."""
    url = url.strip()
    if not (url.startswith("http://") or url.startswith("https://")):
        return "URL non valido: deve iniziare con http:// o https://"
    if any(block in url for block in ("127.0.0.1", "localhost", "192.168.", "10.", "172.")):
        return "URL non consentito: host interno/privato"
    return None

# ─── Tool: stream_info ───────────────────────────────────────────────────────

def tool_stream_info(args: dict) -> str:
    url = args.get("url", "").strip()
    if not url:
        return "Errore: url è obbligatorio"
    err = _validate_url(url)
    if err:
        return f"Errore: {err}"

    ytdlp = _find_ytdlp()
    if not ytdlp:
        return (
            "yt-dlp non trovato.\n"
            "Installa con:  pip install yt-dlp"
        )

    try:
        result = subprocess.run(
            [ytdlp, "--dump-json", "--no-playlist", url],
            capture_output=True, text=True, timeout=30
        )
        if result.returncode != 0:
            return f"Errore yt-dlp: {result.stderr.strip()[:400]}"

        info = json.loads(result.stdout.splitlines()[0])

        # Estrai campi rilevanti
        title    = info.get("title",    "Sconosciuto")
        uploader = info.get("uploader", info.get("channel", "—"))
        duration = info.get("duration")
        platform = info.get("extractor_key", info.get("extractor", "—"))
        view_count = info.get("view_count")
        like_count = info.get("like_count")
        upload_date = info.get("upload_date", "")  # YYYYMMDD

        # Qualità disponibili
        formats = info.get("formats", [])
        qualities = sorted({
            f.get("format_note") or f.get("height") or f.get("abr")
            for f in formats
            if (f.get("format_note") or f.get("height") or f.get("abr"))
        }, key=lambda x: str(x), reverse=True)[:8]

        dur_str = ""
        if duration:
            m, s = divmod(int(duration), 60)
            h, m = divmod(m, 60)
            dur_str = f"{h}h {m}m {s}s" if h else f"{m}m {s}s"

        date_str = ""
        if len(upload_date) == 8:
            date_str = f"{upload_date[6:]}/{upload_date[4:6]}/{upload_date[:4]}"

        out = [
            f"Titolo:    {title}",
            f"Canale:    {uploader}",
            f"Piattaforma: {platform}",
        ]
        if dur_str:
            out.append(f"Durata:    {dur_str}")
        if date_str:
            out.append(f"Data:      {date_str}")
        if view_count:
            out.append(f"Visualizzazioni: {view_count:,}")
        if like_count:
            out.append(f"Like:      {like_count:,}")
        if qualities:
            out.append(f"Qualità:   {', '.join(str(q) for q in qualities)}")

        return "\n".join(out)

    except subprocess.TimeoutExpired:
        return "Timeout: stream_info ha impiegato troppo (>30s)"
    except json.JSONDecodeError as e:
        return f"Errore parsing metadati: {e}"
    except Exception as e:
        logger.exception("stream_info error")
        return f"Errore imprevisto: {e}"

# ─── Tool: stream_capture ────────────────────────────────────────────────────

def tool_stream_capture(args: dict) -> str:
    """Cattura i primi N secondi dello stream in un file WAV o MP4."""
    url       = args.get("url",          "").strip()
    duration  = int(args.get("duration_sec", 30))
    output    = args.get("output_path",  "").strip()
    fmt       = args.get("format",       "wav").lower().strip()  # "wav" | "mp4" | "mkv"

    if not url:
        return "Errore: url è obbligatorio"
    err = _validate_url(url)
    if err:
        return f"Errore: {err}"
    if duration < 1 or duration > 3600:
        return "Errore: duration_sec deve essere tra 1 e 3600"
    if fmt not in ("wav", "mp4", "mkv", "mp3"):
        fmt = "wav"

    ytdlp  = _find_ytdlp()
    ffmpeg = _find_ffmpeg()

    if not ytdlp:
        return "yt-dlp non trovato. Installa con: pip install yt-dlp"
    if not ffmpeg:
        return "ffmpeg non trovato. Installa con: sudo apt install ffmpeg"

    # Path output: usa temp se non specificato
    if not output:
        tmp = tempfile.mkstemp(suffix=f".{fmt}", prefix="prismalux_stream_")[1]
        output = tmp

    output = str(Path(output).expanduser().resolve())

    # Sicurezza path output: solo in home o /tmp
    home = str(Path.home())
    if not (output.startswith(home) or output.startswith("/tmp")):
        return f"Errore: output_path deve essere in home o /tmp, non in {output}"

    try:
        # yt-dlp → stdout (flusso grezzo) → ffmpeg → file
        # Seleziona best audio+video o solo audio per WAV
        if fmt == "wav":
            format_sel = "bestaudio/best"
            ffmpeg_args = [
                ffmpeg, "-y", "-i", "pipe:0",
                "-t", str(duration),
                "-ar", "16000", "-ac", "1", "-f", "wav",
                output
            ]
        else:
            format_sel = "bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best"
            ffmpeg_args = [
                ffmpeg, "-y", "-i", "pipe:0",
                "-t", str(duration),
                "-c", "copy",
                output
            ]

        ytdlp_proc = subprocess.Popen(
            [ytdlp, "-f", format_sel, "-o", "-", "--no-playlist", url],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        ffmpeg_proc = subprocess.Popen(
            ffmpeg_args,
            stdin=ytdlp_proc.stdout,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        if ytdlp_proc.stdout:
            ytdlp_proc.stdout.close()

        _, ff_err = ffmpeg_proc.communicate(timeout=duration + 60)
        ytdlp_proc.wait(timeout=10)

        if ffmpeg_proc.returncode != 0:
            return f"Errore ffmpeg: {ff_err.decode(errors='replace').strip()[-300:]}"

        size_mb = Path(output).stat().st_size / (1024 * 1024) if Path(output).exists() else 0
        return (
            f"Stream catturato: {duration}s → {output}\n"
            f"Dimensione: {size_mb:.1f} MB\n"
            f"Formato: {fmt.upper()}\n"
            f"Pronto per Whisper STT o video_caption.py"
        )

    except subprocess.TimeoutExpired:
        ytdlp_proc.kill()
        ffmpeg_proc.kill()
        return f"Timeout: cattura stream >{{duration+60}}s"
    except Exception as e:
        logger.exception("stream_capture error")
        return f"Errore imprevisto: {e}"

# ─── Tool: stream_download ───────────────────────────────────────────────────

def tool_stream_download(args: dict) -> str:
    """Scarica l'intero video nella qualità scelta."""
    url     = args.get("url",         "").strip()
    quality = args.get("quality",     "best").strip()  # "best" | "worst" | "720p" ecc.
    output  = args.get("output_path", "").strip()

    if not url:
        return "Errore: url è obbligatorio"
    err = _validate_url(url)
    if err:
        return f"Errore: {err}"

    ytdlp = _find_ytdlp()
    if not ytdlp:
        return "yt-dlp non trovato. Installa con: pip install yt-dlp"

    if not output:
        output = str(Path.home() / "Downloads" / "%(title)s.%(ext)s")

    output = str(Path(output).expanduser())
    home   = str(Path.home())
    if not output.startswith(home) and "%(title)s" not in output:
        return f"Errore: output_path deve essere dentro la home"

    # Mappa qualità leggibile → formato yt-dlp
    fmt_map = {
        "best":   "bestvideo+bestaudio/best",
        "worst":  "worstvideo+worstaudio/worst",
        "audio":  "bestaudio/best",
        "720p":   "bestvideo[height<=720]+bestaudio/best[height<=720]",
        "1080p":  "bestvideo[height<=1080]+bestaudio/best[height<=1080]",
        "480p":   "bestvideo[height<=480]+bestaudio/best[height<=480]",
    }
    fmt_sel = fmt_map.get(quality.lower(), f"bestvideo[height<={quality.replace('p','')}]+bestaudio/best")

    try:
        result = subprocess.run(
            [ytdlp, "-f", fmt_sel, "-o", output, "--no-playlist",
             "--progress", "--newline", url],
            capture_output=True, text=True, timeout=600
        )
        if result.returncode != 0:
            return f"Errore yt-dlp: {result.stderr.strip()[-400:]}"

        # Estrai il path finale dal log
        last_lines = result.stdout.strip().splitlines()[-5:]
        dest_line  = next((l for l in reversed(last_lines) if "Destination" in l or "[download]" in l), "")

        return (
            f"Download completato.\n"
            f"{dest_line.strip()}\n"
            f"Qualità: {quality}"
        )

    except subprocess.TimeoutExpired:
        return "Timeout: download stream >600s"
    except Exception as e:
        logger.exception("stream_download error")
        return f"Errore imprevisto: {e}"

# ─── Definizione strumenti MCP ───────────────────────────────────────────────

TOOLS = [
    {
        "name":        "stream_info",
        "description": (
            "Restituisce metadati di uno stream/video online: titolo, canale, "
            "durata, piattaforma, qualità disponibili. "
            "Supporta YouTube, Twitch, Dailymotion e 1000+ siti via yt-dlp."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "url": {"type": "string", "description": "URL dello stream o video (http/https)"},
            },
            "required": ["url"],
        },
    },
    {
        "name":        "stream_capture",
        "description": (
            "Cattura i primi N secondi di uno stream in un file WAV (per Whisper STT) "
            "o MP4/MKV (per video_caption). "
            "Usa yt-dlp + ffmpeg in pipeline — non scarica l'intero video."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "url":          {"type": "string",  "description": "URL dello stream"},
                "duration_sec": {"type": "integer", "description": "Secondi da catturare (1-3600, default 30)"},
                "output_path":  {"type": "string",  "description": "Path file output (default: /tmp/prismalux_stream_*.wav)"},
                "format":       {"type": "string",  "description": "Formato output: wav|mp4|mkv (default: wav)"},
            },
            "required": ["url"],
        },
    },
    {
        "name":        "stream_download",
        "description": (
            "Scarica l'intero video nella qualità scelta. "
            "quality: best|worst|audio|720p|1080p|480p (default: best)."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "url":         {"type": "string", "description": "URL del video"},
                "quality":     {"type": "string", "description": "Qualità: best|worst|audio|720p|1080p|480p"},
                "output_path": {"type": "string", "description": "Path destinazione (default: ~/Downloads/)"},
            },
            "required": ["url"],
        },
    },
]

TOOL_HANDLERS = {
    "stream_info":     tool_stream_info,
    "stream_capture":  tool_stream_capture,
    "stream_download": tool_stream_download,
}

# ─── Main loop JSON-RPC 2.0 ──────────────────────────────────────────────────

def handle(request: dict) -> None:
    method  = request.get("method", "")
    req_id  = request.get("id")
    params  = request.get("params", {}) or {}

    if method == "initialize":
        _result(req_id, {
            "protocolVersion": "2024-11-05",
            "capabilities":    {"tools": {}},
            "serverInfo":      {"name": "streamlink-mcp", "version": "1.0.0"},
        })

    elif method in ("notifications/initialized", "ping"):
        if req_id is not None:
            _result(req_id, {})

    elif method == "tools/list":
        _result(req_id, {"tools": TOOLS})

    elif method == "tools/call":
        name      = params.get("name", "")
        tool_args = params.get("arguments", {}) or {}
        handler   = TOOL_HANDLERS.get(name)
        if not handler:
            _error(req_id, -32601, f"Strumento '{name}' non trovato.")
            return
        try:
            text = handler(tool_args)
        except Exception as exc:
            logger.error("Errore tool '%s': %s", name, exc)
            text = f"[Errore strumento] {exc}"
        _text_result(req_id, text, is_error=text.startswith("[Errore"))

    elif req_id is not None:
        _error(req_id, -32601, f"Metodo '{method}' non trovato.")


def main() -> None:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError as exc:
            _send({"jsonrpc": "2.0", "id": None,
                   "error": {"code": -32700, "message": f"Parse error: {exc}"}})
            continue
        handle(req)


if __name__ == "__main__":
    main()

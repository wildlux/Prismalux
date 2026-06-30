#!/usr/bin/env python3
"""
video_caption.py — Frame extraction + hashing percettivo + VLM captioning.

Ogni riga stdout è un oggetto JSON con campo "status":
  {"status":"extracting"}
  {"status":"extracted","total":N}
  {"status":"skipped","frame":N,"total":M,"dist":D}
  {"status":"captioning","frame":N,"total":M,"ts":"mm:ss"}
  {"status":"result","ts":"mm:ss","caption":"...","frame":N}
  {"status":"done","analyzed":N,"skipped":S}
  {"error":"..."}

Dipendenze esterne: ffmpeg (PATH)
Dipendenze Python: urllib.request (stdlib), base64 (stdlib)
Opzionale: Pillow (pip install Pillow) per dhash migliore

Uso: video_caption.py <video> [opzioni]
"""
import sys, os, json, subprocess, base64, tempfile, math
import argparse
import urllib.request
import urllib.error

# ─────────────────────────── Hash percettivo ────────────────────────────────

def dhash(img_path: str, hash_size: int = 8) -> int:
    """Differenza hash: confronta pixel adiacenti. Ritorna intero a 64 bit."""
    try:
        from PIL import Image
        img = (Image.open(img_path)
               .convert('L')
               .resize((hash_size + 1, hash_size)))
        px = list(img.getdata())
        bits = [1 if px[r * (hash_size + 1) + c] > px[r * (hash_size + 1) + c + 1] else 0
                for r in range(hash_size)
                for c in range(hash_size)]
        return sum(b << i for i, b in enumerate(bits))
    except ImportError:
        # Fallback stdlib: campionamento 64 byte equidistanti
        import hashlib
        with open(img_path, 'rb') as f:
            data = f.read()
        step = max(1, len(data) // 64)
        sample = bytes(data[i] for i in range(0, len(data), step))[:64]
        return int.from_bytes(hashlib.md5(sample).digest()[:8], 'big')

def hamming(a: int, b: int) -> int:
    return bin(a ^ b).count('1')

# ─────────────────────────── Estrazione frame ───────────────────────────────

def extract_frames(video_path: str, interval_s: int, out_dir: str) -> list[str]:
    """Estrae un frame ogni interval_s secondi con ffmpeg."""
    cmd = [
        "ffmpeg", "-i", video_path,
        "-vf", f"fps=1/{interval_s}",
        "-q:v", "3",
        os.path.join(out_dir, "frame_%05d.jpg"),
        "-hide_banner", "-loglevel", "error"
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr.strip() or "ffmpeg fallito")
    frames = sorted(
        os.path.join(out_dir, f)
        for f in os.listdir(out_dir)
        if f.endswith('.jpg')
    )
    return frames

# ─────────────────────────── Captioning VLM ─────────────────────────────────

def caption_frame(img_path: str, model: str, port: int, prompt: str) -> str:
    with open(img_path, 'rb') as f:
        b64 = base64.b64encode(f.read()).decode()
    payload = json.dumps({
        "model": model,
        "prompt": prompt,
        "images": [b64],
        "stream": False
    }).encode()
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/api/generate",
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST"
    )
    try:
        with urllib.request.urlopen(req, timeout=90) as resp:
            return json.loads(resp.read()).get("response", "").strip()
    except urllib.error.URLError as e:
        return f"[Errore Ollama: {e.reason}]"
    except Exception as e:
        return f"[Errore: {e}]"

# ─────────────────────────── Main ───────────────────────────────────────────

def emit(obj: dict):
    print(json.dumps(obj, ensure_ascii=False), flush=True)

def main():
    parser = argparse.ArgumentParser(
        description="Analizza video con hashing + VLM captioning")
    parser.add_argument("video",
        help="Percorso file video (o URL se streamlink installato)")
    parser.add_argument("--interval", type=int, default=5,
        help="Secondi tra un frame e il successivo (default 5)")
    parser.add_argument("--model", default="llava:7b",
        help="Modello Ollama VLM (default llava:7b)")
    parser.add_argument("--threshold", type=int, default=10,
        help="Soglia Hamming per novità frame (0-64, default 10)")
    parser.add_argument("--port", type=int, default=11434,
        help="Porta Ollama (default 11434)")
    parser.add_argument("--max-frames", type=int, default=60,
        help="Max frame da analizzare (default 60)")
    parser.add_argument("--prompt",
        default="Descrivi brevemente in italiano cosa vedi in questa immagine (1-2 frasi concise).")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        emit({"status": "extracting", "msg": "Estrazione frame con ffmpeg..."})

        try:
            all_frames = extract_frames(args.video, args.interval, tmp)
        except Exception as e:
            emit({"error": str(e)})
            sys.exit(1)

        emit({"status": "extracted", "total": len(all_frames)})

        analyzed = 0
        skipped  = 0
        prev_hash: int | None = None

        for idx, fpath in enumerate(all_frames):
            if analyzed >= args.max_frames:
                break

            try:
                h = dhash(fpath)
            except Exception:
                h = idx  # se hash fallisce, considera frame sempre nuovo

            if prev_hash is not None:
                dist = hamming(h, prev_hash)
                if dist < args.threshold:
                    emit({"status": "skipped", "frame": idx + 1,
                          "total": len(all_frames), "dist": dist})
                    skipped += 1
                    continue

            prev_hash = h
            ts_s  = idx * args.interval
            ts_str = f"{ts_s // 60:02d}:{ts_s % 60:02d}"

            emit({"status": "captioning", "frame": idx + 1,
                  "total": len(all_frames), "ts": ts_str})

            caption = caption_frame(fpath, args.model, args.port, args.prompt)
            analyzed += 1

            emit({"status": "result", "ts": ts_str, "frame": idx + 1,
                  "caption": caption})

        emit({"status": "done", "analyzed": analyzed, "skipped": skipped})

if __name__ == "__main__":
    main()

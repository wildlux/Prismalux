#!/usr/bin/env python3
"""
sd_local.py — Stable Diffusion locale via diffusers, senza AUTOMATIC1111.
Chiamato da Prismalux come subprocess. Output su stdout: JSON line per line.

Uso:
  python3 sd_local.py --prompt "un tramonto" [opzioni]

Output JSON (una riga per status, ultima riga con risultato):
  {"status": "Caricamento modello su cuda..."}
  {"ok": true, "image_b64": "iVBORw0...", "device": "cuda", "size_kb": 412}
  oppure:
  {"ok": false, "error": "diffusers non installato..."}
"""

import sys
import json
import base64
import sqlite3
import time
import argparse
from io import BytesIO
from pathlib import Path

_CACHE_DIR = Path.home() / ".prismalux"
_MODELS_DB = _CACHE_DIR / "sd_models_cache.db"


# ─── SQLite cache modelli ────────────────────────────────────────────────────

def _models_db() -> sqlite3.Connection:
    _CACHE_DIR.mkdir(parents=True, exist_ok=True)
    con = sqlite3.connect(_MODELS_DB, timeout=5)
    con.row_factory = sqlite3.Row
    con.execute("PRAGMA journal_mode=WAL")
    con.executescript("""
        CREATE TABLE IF NOT EXISTS models (
            model_id      TEXT PRIMARY KEY,
            local_path    TEXT,
            device        TEXT,
            size_kb       INTEGER,
            first_used_at INTEGER,
            last_used_at  INTEGER,
            use_count     INTEGER DEFAULT 1
        );
        CREATE TABLE IF NOT EXISTS generations (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            model_id      TEXT NOT NULL,
            prompt        TEXT,
            steps         INTEGER,
            width         INTEGER,
            height        INTEGER,
            seed          INTEGER,
            size_kb       INTEGER,
            device        TEXT,
            generated_at  INTEGER
        );
    """)
    con.commit()
    return con


def _record_model_use(model_id: str, device: str) -> None:
    now = int(time.time())
    con = _models_db()
    con.execute("""
        INSERT INTO models(model_id, device, first_used_at, last_used_at, use_count)
        VALUES(?, ?, ?, ?, 1)
        ON CONFLICT(model_id) DO UPDATE SET
            device       = excluded.device,
            last_used_at = excluded.last_used_at,
            use_count    = use_count + 1
    """, (model_id, device, now, now))
    con.commit()
    con.close()


def _record_generation(model_id: str, args, device: str, size_kb: int) -> None:
    now = int(time.time())
    con = _models_db()
    con.execute("""
        INSERT INTO generations(model_id, prompt, steps, width, height, seed, size_kb, device, generated_at)
        VALUES(?,?,?,?,?,?,?,?,?)
    """, (model_id, args.prompt[:500], args.steps, args.width, args.height,
          args.seed, size_kb, device, now))
    con.commit()
    con.close()


def _list_cached_models() -> str:
    """Elenca i modelli usati in precedenza (dalla cache SQLite)."""
    try:
        con = _models_db()
        rows = con.execute(
            "SELECT model_id, device, use_count, last_used_at FROM models ORDER BY last_used_at DESC"
        ).fetchall()
        con.close()
        if not rows:
            return "Nessun modello in cache. Avvia una generazione prima."
        lines = [f"Modelli SD usati ({len(rows)}):"]
        for r in rows:
            age = int(time.time()) - (r["last_used_at"] or 0)
            lines.append(f"  {r['model_id']:<50} device={r['device']} "
                         f"usi={r['use_count']} ({age//3600}h fa)")
        return "\n".join(lines)
    except Exception as e:
        return f"[Errore cache] {e}"


def emit(obj: dict):
    print(json.dumps(obj, ensure_ascii=False), flush=True)


def main():
    parser = argparse.ArgumentParser(description="Stable Diffusion locale")
    parser.add_argument("--list-models", action="store_true",
                        help="Mostra i modelli usati in precedenza (dalla cache SQLite)")
    parser.add_argument("--prompt",          default=None)
    parser.add_argument("--negative_prompt", default="blurry, low quality, watermark, text, ugly, deformed")
    parser.add_argument("--steps",           type=int,   default=20)
    parser.add_argument("--cfg",             type=float, default=7.0)
    parser.add_argument("--width",           type=int,   default=512)
    parser.add_argument("--height",          type=int,   default=512)
    parser.add_argument("--seed",            type=int,   default=-1)
    parser.add_argument("--model",           default="runwayml/stable-diffusion-v1-5",
                        help="HuggingFace model id o path locale")
    parser.add_argument("--out",             default=None, help="Salva PNG a questo path")
    args = parser.parse_args()

    # ── Lista modelli dalla cache ──────────────────────────────────
    if args.list_models:
        emit({"ok": True, "text": _list_cached_models()})
        return

    if not args.prompt:
        emit({"ok": False, "error": "--prompt è obbligatorio (o usa --list-models)"})
        sys.exit(1)

    # ── Import check ──────────────────────────────────────────────
    try:
        from diffusers import StableDiffusionPipeline, DPMSolverMultistepScheduler
        import torch
    except ImportError as e:
        emit({"ok": False, "error":
              f"Libreria mancante: {e}\n"
              "Installa con:\n"
              "  pip install diffusers transformers accelerate\n"
              "  pip install torch --index-url https://download.pytorch.org/whl/cpu\n"
              "(per GPU NVIDIA usa: pip install torch --index-url https://download.pytorch.org/whl/cu121)"})
        sys.exit(1)

    # ── Device selection ──────────────────────────────────────────
    if torch.cuda.is_available():
        device = "cuda"
        dtype  = torch.float16
    elif hasattr(torch.backends, "mps") and torch.backends.mps.is_available():
        device = "mps"
        dtype  = torch.float16
    else:
        device = "cpu"
        dtype  = torch.float32

    _record_model_use(args.model, device)
    emit({"status": f"Caricamento modello '{args.model}' su {device.upper()}..."})

    # ── Caricamento pipeline ──────────────────────────────────────
    try:
        pipe = StableDiffusionPipeline.from_pretrained(
            args.model,
            torch_dtype=dtype,
            safety_checker=None,
            requires_safety_checker=False,
        )
        # Scheduler DPM++ 2M Karras — più veloce a parità di qualità
        pipe.scheduler = DPMSolverMultistepScheduler.from_config(
            pipe.scheduler.config,
            algorithm_type="dpmsolver++",
            use_karras_sigmas=True,
        )
        pipe = pipe.to(device)
        pipe.enable_attention_slicing()
        if device == "cuda":
            try:
                pipe.enable_xformers_memory_efficient_attention()
            except Exception:
                pass  # xformers opzionale
    except Exception as e:
        emit({"ok": False, "error": f"Errore caricamento modello: {e}"})
        sys.exit(1)

    emit({"status": f"Generazione in corso ({args.steps} steps, {args.width}x{args.height})...",
          "step": 0, "total_steps": args.steps})

    # ── Callback step-by-step ─────────────────────────────────────
    def step_callback(pipe, step_index, timestep, callback_kwargs):
        emit({"status": f"Step {step_index + 1}/{args.steps}",
              "step": step_index + 1, "total_steps": args.steps})
        return callback_kwargs

    # ── Generazione ───────────────────────────────────────────────
    import torch
    generator = None
    if args.seed >= 0:
        generator = torch.Generator(device=device).manual_seed(args.seed)

    try:
        result = pipe(
            prompt=args.prompt,
            negative_prompt=args.negative_prompt or None,
            num_inference_steps=args.steps,
            guidance_scale=args.cfg,
            width=args.width,
            height=args.height,
            generator=generator,
            callback_on_step_end=step_callback,
        )
        image = result.images[0]
    except Exception as e:
        emit({"ok": False, "error": f"Errore generazione: {e}"})
        sys.exit(1)

    # ── Encode PNG → base64 ───────────────────────────────────────
    buf = BytesIO()
    image.save(buf, format="PNG")
    png_bytes = buf.getvalue()
    b64 = base64.b64encode(png_bytes).decode()

    size_kb = len(png_bytes) // 1024
    _record_generation(args.model, args, device, size_kb)

    if args.out:
        Path(args.out).write_bytes(png_bytes)
        # Non emettere image_b64 su stdout: il C++ legge il file dal disco
        # ed evitare il deadlock del pipe (riga base64 > 64 KB buffer Linux)
        emit({"ok": True, "device": device, "size_kb": size_kb})
    else:
        emit({"ok": True, "image_b64": b64,
              "device": device, "size_kb": size_kb})


if __name__ == "__main__":
    main()

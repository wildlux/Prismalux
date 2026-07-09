#!/usr/bin/env python3
"""
stt_daemon.py — demone STT persistente per SttWhisper (Prismalux).

Carica il modello faster-whisper UNA volta e serve richieste di
trascrizione via stdin/stdout (JSON newline-delimited). Elimina i
~9s per frase di avvio interprete + caricamento modello che lo
script one-shot fast_whisper_transcribe.py paga a ogni chiamata.

Uso:      stt_daemon.py [model_id]           (default: large-v3-turbo)

Protocollo (una riga JSON per messaggio):
  → all'avvio, dopo il load:   {"ready": true, "model": "...", "device": "cuda|cpu"}
  ← richiesta:                 {"wav": "/path/file.wav", "lang": "it"}
  → risposta:                  {"ok": true,  "text": "..."}     (text vuoto = silenzio)
                               {"ok": false, "error": "..."}
  ← EOF su stdin → uscita pulita.

Note GPU: stesse strategie di fast_whisper_transcribe.py — preload
ctypes delle librerie CUDA dei wheel pip, retry su CPU se CUDA è
rotto (marker persistente ~/.prismalux/stt_force_cpu).
"""
import glob
import json
import os
import site
import sys

MARKER = os.path.expanduser("~/.prismalux/stt_force_cpu")


def _ensure_cuda_libs() -> None:
    """Pre-carica libcublas.so.12/libcudnn.so.9 dei wheel pip con
    RTLD_GLOBAL: il dlopen lazy di CTranslate2 li trova già risolti."""
    import ctypes
    roots = list(site.getsitepackages()) + [site.getusersitepackages()]
    for root in roots:
        for pat in ("nvidia/**/libcublas.so.12", "nvidia/**/libcudnn.so.9"):
            for p in glob.glob(os.path.join(root, pat), recursive=True):
                try:
                    ctypes.CDLL(p, mode=ctypes.RTLD_GLOBAL)
                except OSError:
                    pass


def _reply(obj: dict) -> None:
    print(json.dumps(obj, ensure_ascii=False), flush=True)


def _load_model(model_id: str):
    """Ritorna (model, device). Su CUDA rotto scrive il marker e ripiega
    su CPU — il fallimento emerge solo alla prima trascrizione, quindi
    qui si fa anche un warm-up con un WAV sintetico di silenzio."""
    from faster_whisper import WhisperModel

    def _warmup(m) -> None:
        """Trascrive 0.5s di silenzio: forza encode() → l'errore CUDA
        lazy esce QUI, non sulla prima frase vera dell'utente."""
        import struct
        import tempfile
        import wave
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            path = f.name
        try:
            with wave.open(path, "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(16000)
                w.writeframes(struct.pack("<8000h", *([0] * 8000)))
            segs, _ = m.transcribe(path, language="it", beam_size=1)
            list(segs)
        finally:
            os.unlink(path)

    if not os.path.exists(MARKER):
        _ensure_cuda_libs()
        try:
            model = WhisperModel(model_id, device="auto", compute_type="auto")
            _warmup(model)
            return model, "auto"
        except (RuntimeError, OSError, ValueError) as e:
            msg = str(e).lower()
            if not any(k in msg for k in ("cuda", "cublas", "cudnn", "gpu", "hip")):
                raise
            print(f"GPU non utilizzabile ({e}) — passo a CPU", file=sys.stderr)
            try:
                os.makedirs(os.path.dirname(MARKER), exist_ok=True)
                with open(MARKER, "w", encoding="utf-8") as f:
                    f.write("Creato da stt_daemon.py: CUDA non utilizzabile "
                            f"({e}).\nElimina questo file per riprovare la GPU.\n")
            except OSError:
                pass
    model = WhisperModel(model_id, device="cpu", compute_type="int8")
    return model, "cpu"


def main() -> None:
    model_id = sys.argv[1] if len(sys.argv) > 1 else "large-v3-turbo"

    try:
        model, device = _load_model(model_id)
    except Exception as e:                    # noqa: BLE001 — riportato al client
        _reply({"ready": False, "error": f"caricamento modello fallito: {e}"})
        sys.exit(1)

    _reply({"ready": True, "model": model_id, "device": device})

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
            wav = req["wav"]
            lang = req.get("lang", "it")
        except (json.JSONDecodeError, KeyError, TypeError):
            _reply({"ok": False, "error": "richiesta non valida (atteso "
                                          '{"wav": "...", "lang": "it"})'})
            continue

        if not os.path.isfile(wav) or os.path.getsize(wav) < 44:
            _reply({"ok": False, "error": f"file WAV mancante o vuoto: {wav}"})
            continue

        try:
            segments, _ = model.transcribe(
                wav, language=lang,
                vad_filter=True,   # VAD integrata: silenzio → text vuoto
                beam_size=5,
            )
            parts = [s.text.strip() for s in segments if s.text.strip()]
            _reply({"ok": True, "text": " ".join(parts)})
        except Exception as e:                # noqa: BLE001
            _reply({"ok": False, "error": str(e)})


if __name__ == "__main__":
    main()

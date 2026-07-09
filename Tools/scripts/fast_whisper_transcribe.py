#!/usr/bin/env python3
"""
fast_whisper_transcribe.py — wrapper faster_whisper per SttWhisper.

Uso:   fast_whisper_transcribe.py <wav_path> [lang] [model_id]
       lang     — codice ISO 639-1 (default: "it")
       model_id — nome modello HuggingFace (default: "large-v3-turbo")

Uscita:  testo trascritto su stdout (plain text, nessun timestamp).
Errori:  messaggio su stderr + exit code 1.

Note GPU: device="auto" sceglie CUDA se le librerie NVIDIA sono presenti,
ma l'errore (es. libcublas.so.12 mancante) emerge SOLO durante encode(),
in modo lazy: viene intercettato e la trascrizione riparte su CPU.

Dipendenze:  pip install faster-whisper
"""
import glob
import os
import site
import sys


def _ensure_cuda_libs() -> None:
    """Le librerie CUDA dei wheel pip (nvidia-cublas-cu12, nvidia-cudnn-*)
    non sono nel search path del linker di sistema: le pre-carica con
    ctypes RTLD_GLOBAL, così il dlopen lazy di CTranslate2 le trova già
    risolte per soname. Le sotto-librerie cudnn si risolvono da sole via
    RUNPATH $ORIGIN del wheel. Silenzioso se i wheel non ci sono (si va
    di device=auto → CPU) o se una libreria non carica."""
    import ctypes
    roots = list(site.getsitepackages()) + [site.getusersitepackages()]
    for root in roots:
        for pat in ("nvidia/**/libcublas.so.12", "nvidia/**/libcudnn.so.9"):
            for p in glob.glob(os.path.join(root, pat), recursive=True):
                try:
                    ctypes.CDLL(p, mode=ctypes.RTLD_GLOBAL)
                except OSError:
                    pass


def _transcribe(model, wav_path: str, lang: str) -> list[str]:
    segments, _ = model.transcribe(
        wav_path,
        language=lang,
        vad_filter=True,   # VAD integrata: ignora automaticamente il silenzio
        beam_size=5,
    )
    return [s.text.strip() for s in segments if s.text.strip()]


def main() -> None:
    if len(sys.argv) < 2:
        print("Uso: fast_whisper_transcribe.py <wav> [lang] [model]",
              file=sys.stderr)
        sys.exit(1)

    wav_path   = sys.argv[1]
    lang       = sys.argv[2] if len(sys.argv) > 2 else "it"
    model_id   = sys.argv[3] if len(sys.argv) > 3 else "large-v3-turbo"

    # Fallimento RAPIDO su file mancante/vuoto: prima di caricare il modello
    # (che da solo costa ~10s) — il chiamante Qt ha timeout brevi.
    if not os.path.isfile(wav_path) or os.path.getsize(wav_path) < 44:
        print(f"file WAV mancante o vuoto: {wav_path}", file=sys.stderr)
        sys.exit(1)

    try:
        from faster_whisper import WhisperModel
    except ImportError:
        print(
            "faster_whisper non installato.\n"
            "Installa con:  pip install faster-whisper",
            file=sys.stderr)
        sys.exit(1)

    # Marker persistente: dopo il primo fallimento CUDA si parte diretti da
    # CPU (il tentativo GPU costa ~10s di caricamento modello sprecati a ogni
    # chiamata). Se installi/riparì CUDA, elimina il file per riprovare la GPU.
    marker = os.path.expanduser("~/.prismalux/stt_force_cpu")

    # device="auto"        → CUDA se disponibile, altrimenti CPU
    # compute_type="auto"  → float16 su GPU, int8 su CPU (più veloce, stessa qualità)
    try:
        if os.path.exists(marker):
            model = WhisperModel(model_id, device="cpu", compute_type="int8")
        else:
            _ensure_cuda_libs()
            model = WhisperModel(model_id, device="auto", compute_type="auto")
        parts = _transcribe(model, wav_path, lang)
    except (RuntimeError, OSError, ValueError) as e:
        # CUDA presente ma inutilizzabile (libcublas/cudnn mancanti, driver
        # rotto): riprova su CPU invece di fallire — caso reale, GPU NVIDIA
        # rilevata ma toolkit CUDA non installato.
        msg = str(e).lower()
        if not any(k in msg for k in ("cuda", "cublas", "cudnn", "gpu", "hip")):
            raise
        print(f"GPU non utilizzabile ({e}) — riprovo su CPU", file=sys.stderr)
        try:
            os.makedirs(os.path.dirname(marker), exist_ok=True)
            with open(marker, "w", encoding="utf-8") as f:
                f.write("Creato da fast_whisper_transcribe.py: CUDA non "
                        f"utilizzabile ({e}).\nLe prossime trascrizioni "
                        "partono direttamente su CPU.\nElimina questo file "
                        "per riprovare la GPU (es. dopo aver installato "
                        "il toolkit CUDA).\n")
        except OSError:
            pass
        del model
        model = WhisperModel(model_id, device="cpu", compute_type="int8")
        parts = _transcribe(model, wav_path, lang)

    print(" ".join(parts))


if __name__ == "__main__":
    main()

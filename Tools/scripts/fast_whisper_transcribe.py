#!/usr/bin/env python3
"""
fast_whisper_transcribe.py — wrapper faster_whisper per SttWhisper.

Uso:   fast_whisper_transcribe.py <wav_path> [lang] [model_id]
       lang     — codice ISO 639-1 (default: "it")
       model_id — nome modello HuggingFace (default: "large-v3-turbo")

Uscita:  testo trascritto su stdout (plain text, nessun timestamp).
Errori:  messaggio su stderr + exit code 1.

Dipendenze:  pip install faster-whisper
"""
import sys


def main() -> None:
    if len(sys.argv) < 2:
        print("Uso: fast_whisper_transcribe.py <wav> [lang] [model]",
              file=sys.stderr)
        sys.exit(1)

    wav_path   = sys.argv[1]
    lang       = sys.argv[2] if len(sys.argv) > 2 else "it"
    model_id   = sys.argv[3] if len(sys.argv) > 3 else "large-v3-turbo"

    try:
        from faster_whisper import WhisperModel
    except ImportError:
        print(
            "faster_whisper non installato.\n"
            "Installa con:  pip install faster-whisper",
            file=sys.stderr)
        sys.exit(1)

    # device="auto"        → CUDA se disponibile, altrimenti CPU
    # compute_type="auto"  → float16 su GPU, int8 su CPU (più veloce, stessa qualità)
    model = WhisperModel(model_id, device="auto", compute_type="auto")

    segments, _ = model.transcribe(
        wav_path,
        language=lang,
        vad_filter=True,   # VAD integrata: ignora automaticamente il silenzio
        beam_size=5,
    )

    # Concatena tutti i segmenti separati da spazio, senza timestamp
    parts = [s.text.strip() for s in segments if s.text.strip()]
    print(" ".join(parts))


if __name__ == "__main__":
    main()

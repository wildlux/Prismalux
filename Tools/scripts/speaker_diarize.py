#!/usr/bin/env python3
"""
speaker_diarize.py — Diarizzazione speaker da file WAV.

Identifica chi parla e quando, aggiungendo speaker tag al testo trascritto.

Uso:
    speaker_diarize.py <wav_path> [--speakers N] [--transcript <txt_path>]
                        [--hf-token <token>]

Output JSON su stdout:
    [
      {"speaker": "SPEAKER_00", "start": 0.0, "end": 2.5},
      {"speaker": "SPEAKER_01", "start": 2.5, "end": 5.0},
      ...
    ]
    Se --transcript è fornito, aggiunge "text" a ogni segmento allineando
    il transcript di Whisper ai segmenti speaker.

Backend (in ordine di priorità):
    1. pyannote.audio ≥3.0  — migliore qualità (richiede --hf-token o HF_TOKEN env)
    2. simple-diarizer       — offline, nessun token, qualità decente
    3. resemblyzer + sklearn — fallback minimale, qualità base

Dipendenze:
    pip install pyannote.audio        # opzione 1
    pip install simple-diarizer       # opzione 2
    pip install resemblyzer scikit-learn  # opzione 3
"""

import sys
import json
import argparse
import os
import re
from pathlib import Path


def _trust_torch_repos() -> None:
    """Pre-accetta i repo PyTorch Hub usati da simple-diarizer (silero-vad)
    in modo non interattivo, evitando il prompt 'Do you trust...' che blocca
    gli script headless."""
    try:
        import torch.hub as _hub
        hub_dir = Path(_hub.get_dir())
        trusted = hub_dir / "trusted_list"
        repos = ["snakers4_silero-vad", "snakers4/silero-vad"]
        hub_dir.mkdir(parents=True, exist_ok=True)
        existing = trusted.read_text() if trusted.exists() else ""
        additions = [r for r in repos if r not in existing]
        if additions:
            with trusted.open("a") as f:
                f.write("\n".join(additions) + "\n")
    except Exception:
        pass


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Diarizzazione speaker da WAV")
    p.add_argument("wav",           help="File WAV 16kHz mono")
    p.add_argument("--speakers",    type=int, default=None,
                   help="Numero speaker (auto-detect se omesso)")
    p.add_argument("--transcript",  default=None,
                   help="File di testo trascritto da Whisper (plain text, una riga = un segmento)")
    p.add_argument("--hf-token",    default=None,
                   help="HuggingFace token per pyannote.audio (o var env HF_TOKEN)")
    return p.parse_args()


# ─── Backend 1: pyannote.audio ────────────────────────────────────────────────

def _diarize_pyannote(wav: str, n_speakers: int | None, hf_token: str | None) -> list[dict]:
    from pyannote.audio import Pipeline

    token = hf_token or os.environ.get("HF_TOKEN") or os.environ.get("HUGGINGFACE_TOKEN")
    if not token:
        raise RuntimeError(
            "pyannote.audio richiede un HuggingFace token.\n"
            "Ottienilo su https://huggingface.co/settings/tokens\n"
            "Poi passa --hf-token <token> o imposta HF_TOKEN nell'ambiente."
        )

    pipeline = Pipeline.from_pretrained(
        "pyannote/speaker-diarization-3.1",
        use_auth_token=token,
    )

    kwargs: dict = {}
    if n_speakers is not None:
        kwargs["num_speakers"] = n_speakers

    diarization = pipeline(wav, **kwargs)

    segments = []
    for turn, _, speaker in diarization.itertracks(yield_label=True):
        segments.append({
            "speaker": speaker,
            "start":   round(turn.start, 3),
            "end":     round(turn.end, 3),
        })
    return segments


# ─── Backend 2: simple-diarizer ──────────────────────────────────────────────

def _diarize_simple(wav: str, n_speakers: int | None) -> list[dict]:
    from simple_diarizer.diarizer import Diarizer

    diar = Diarizer(
        embed_model="ecapa",   # ecapa-tdnn per speaker embedding
        cluster_method="sc",   # spectral clustering
    )
    segments = diar.diarize(
        wav,
        num_speakers=n_speakers,
        threshold=None if n_speakers else 0.01,
    )
    return [
        {
            "speaker": f"SPEAKER_{int(s['label']):02d}",
            "start":   round(float(s["start"]), 3),
            "end":     round(float(s["end"]),   3),
        }
        for s in segments
    ]


# ─── Backend 3: resemblyzer + sklearn ────────────────────────────────────────

def _diarize_resemblyzer(wav: str, n_speakers: int | None) -> list[dict]:
    import numpy as np
    from resemblyzer import VoiceEncoder, preprocess_wav
    from sklearn.cluster import SpectralClustering, KMeans
    from pathlib import Path as _P

    # Carica e preprocessa il WAV
    wav_data = preprocess_wav(_P(wav))
    encoder  = VoiceEncoder("cpu")

    # Sliding window: finestre da 1.6s ogni 0.4s
    window  = 1.6
    step    = 0.4
    rate    = 16000
    n_total = len(wav_data)
    times   = []
    embeds  = []

    t = 0.0
    while t + window <= n_total / rate:
        start_i = int(t * rate)
        end_i   = int((t + window) * rate)
        chunk   = wav_data[start_i:end_i]
        embed   = encoder.embed_utterance(chunk)
        embeds.append(embed)
        times.append((round(t, 3), round(t + window, 3)))
        t += step

    if not embeds:
        return []

    X = np.array(embeds)
    n_sp = n_speakers or max(2, min(5, len(embeds) // 10))

    if n_sp >= 2:
        labels = SpectralClustering(n_clusters=n_sp, affinity="cosine",
                                    random_state=42).fit_predict(X)
    else:
        labels = np.zeros(len(embeds), dtype=int)

    # Merge finestre consecutive dello stesso speaker
    segments = []
    cur_sp, cur_start, cur_end = labels[0], times[0][0], times[0][1]
    for i in range(1, len(labels)):
        if labels[i] == cur_sp:
            cur_end = times[i][1]
        else:
            segments.append({"speaker": f"SPEAKER_{cur_sp:02d}",
                              "start": cur_start, "end": cur_end})
            cur_sp, cur_start, cur_end = labels[i], times[i][0], times[i][1]
    segments.append({"speaker": f"SPEAKER_{cur_sp:02d}",
                     "start": cur_start, "end": cur_end})
    return segments


# ─── Allineamento transcript con segmenti ────────────────────────────────────

def _align_transcript(segments: list[dict], transcript_path: str) -> list[dict]:
    """
    Assegna il testo ai segmenti speaker in base alla proporzione temporale.
    Il transcript Whisper (plain text, righe separate) viene distribuito
    proporzionalmente ai segmenti per durata.
    """
    with open(transcript_path, encoding="utf-8") as f:
        text = f.read().strip()

    # Rimuove timestamp tipo "[00:00:00.000 --> 00:00:02.000]" se presenti
    text = re.sub(r"\[\d{2}:\d{2}:\d{2}\.\d{3}\s*-->\s*\d{2}:\d{2}:\d{2}\.\d{3}\]\s*", "", text)
    words = text.split()
    if not words or not segments:
        return segments

    total_dur   = sum(max(0.0, s["end"] - s["start"]) for s in segments)
    word_cursor = 0

    for seg in segments:
        dur  = max(0.0, seg["end"] - seg["start"])
        frac = dur / total_dur if total_dur > 0 else 0
        n    = max(1, round(frac * len(words)))
        chunk        = words[word_cursor : word_cursor + n]
        seg["text"]  = " ".join(chunk)
        word_cursor += n

    # Eventuale testo rimanente → ultimo segmento
    if word_cursor < len(words) and segments:
        tail = " ".join(words[word_cursor:])
        segments[-1]["text"] = segments[-1].get("text", "") + " " + tail

    return segments


# ─── Main ─────────────────────────────────────────────────────────────────────

def main() -> None:
    # Forza CPU prima di qualsiasi import torch: evita crash su GPU cc < 7.5
    import os as _os
    _os.environ["CUDA_VISIBLE_DEVICES"] = ""
    _trust_torch_repos()
    args = _parse_args()

    wav = str(Path(args.wav).expanduser().resolve())
    if not Path(wav).exists():
        print(json.dumps({"error": f"File non trovato: {wav}"}))
        sys.exit(1)

    segments: list[dict] = []
    backend_used = "nessuno"

    # Tenta i backend in ordine
    errors: list[str] = []

    # Backend 1: pyannote.audio
    try:
        import pyannote.audio  # noqa: F401
        segments     = _diarize_pyannote(wav, args.speakers, args.hf_token)
        backend_used = "pyannote.audio"
    except ImportError:
        errors.append("pyannote.audio non installato (pip install pyannote.audio)")
    except RuntimeError as e:
        errors.append(f"pyannote.audio: {e}")
    except Exception as e:
        errors.append(f"pyannote.audio errore: {e}")

    # Backend 2: simple-diarizer
    if not segments:
        try:
            import simple_diarizer  # noqa: F401
            segments     = _diarize_simple(wav, args.speakers)
            backend_used = "simple-diarizer"
        except ImportError:
            errors.append("simple-diarizer non installato (pip install simple-diarizer)")
        except Exception as e:
            errors.append(f"simple-diarizer errore: {e}")

    # Backend 3: resemblyzer
    if not segments:
        try:
            import resemblyzer  # noqa: F401
            segments     = _diarize_resemblyzer(wav, args.speakers)
            backend_used = "resemblyzer"
        except ImportError:
            errors.append("resemblyzer non installato (pip install resemblyzer scikit-learn)")
        except Exception as e:
            errors.append(f"resemblyzer errore: {e}")

    if not segments:
        print(json.dumps({
            "error":   "Nessun backend di diarizzazione disponibile.",
            "details": errors,
            "install": "pip install simple-diarizer   # consigliato (offline, no token)",
        }))
        sys.exit(1)

    # Allineamento transcript opzionale
    if args.transcript:
        try:
            segments = _align_transcript(segments, args.transcript)
        except Exception as e:
            for s in segments:
                s["text_error"] = f"align failed: {e}"

    # Output JSON
    output = {
        "backend":  backend_used,
        "segments": segments,
        "count":    len(segments),
        "speakers": sorted({s["speaker"] for s in segments}),
    }
    print(json.dumps(output, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()

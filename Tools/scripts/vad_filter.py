#!/usr/bin/env python3
"""
vad_filter.py — Voice Activity Detection prima di Whisper.
Stampa "SPEECH" oppure "SILENCE" su stdout.

Dipendenze:
  - webrtcvad (pip install webrtcvad)   → VAD accurata
  - stdlib wave + array                 → fallback energia RMS se webrtcvad assente

Uso: vad_filter.py <file.wav> [aggressiveness 0-3]
"""
import sys, wave, array, struct

def rms(pcm_bytes: bytes) -> float:
    if not pcm_bytes:
        return 0.0
    samples = array.array('h')
    samples.frombytes(pcm_bytes[:len(pcm_bytes) - len(pcm_bytes) % 2])
    if not samples:
        return 0.0
    return (sum(s * s for s in samples) / len(samples)) ** 0.5

def vad_rms_fallback(wav_path: str, threshold: float = 280.0) -> bool:
    """Fallback senza webrtcvad: RMS > soglia = SPEECH."""
    try:
        with wave.open(wav_path, 'rb') as wf:
            if wf.getsampwidth() != 2:
                return True  # assume speech se non 16-bit
            data = wf.readframes(wf.getnframes())
        return rms(data) > threshold
    except Exception:
        return True  # su errore, assume speech per sicurezza

def vad_webrtc(wav_path: str, aggressiveness: int = 2) -> bool:
    """VAD accurata con webrtcvad. Ritorna True se c'è parlato."""
    import webrtcvad
    vad = webrtcvad.Vad(aggressiveness)

    with wave.open(wav_path, 'rb') as wf:
        rate     = wf.getframerate()
        channels = wf.getnchannels()
        sampw    = wf.getsampwidth()
        frames   = wf.readframes(wf.getnframes())

    # webrtcvad: solo 8/16/32/48 kHz, mono, 16-bit
    if rate not in (8000, 16000, 32000, 48000) or channels != 1 or sampw != 2:
        return vad_rms_fallback(wav_path)

    frame_ms   = 20
    frame_bytes = int(rate * frame_ms / 1000) * 2  # 2 byte/campione

    speech_frames = 0
    total_frames  = 0
    offset = 0
    while offset + frame_bytes <= len(frames):
        chunk = frames[offset:offset + frame_bytes]
        try:
            if vad.is_speech(chunk, rate):
                speech_frames += 1
        except Exception:
            pass
        total_frames += 1
        offset += frame_bytes

    if total_frames == 0:
        return False
    # Soglia 10 %: basta un frammento su dieci per considerare SPEECH
    return (speech_frames / total_frames) > 0.10

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("SILENCE")
        sys.exit(0)

    wav_path     = sys.argv[1]
    aggr         = int(sys.argv[2]) if len(sys.argv) > 2 else 2

    try:
        import webrtcvad
        has_speech = vad_webrtc(wav_path, aggr)
    except ImportError:
        has_speech = vad_rms_fallback(wav_path)

    print("SPEECH" if has_speech else "SILENCE")

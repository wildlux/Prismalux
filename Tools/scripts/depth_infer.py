#!/usr/bin/env python3
"""
depth_infer.py — worker depth map per Prismalux Vision3D (widget_vision3d.cpp).

Contratto I/O (usato da Vision3DWidget::depthMap via QProcess):
  stdin  : JPEG binario della foto
  stdout : JPEG binario della depth map colorata (vicino=rosso, lontano=blu)
  stderr : messaggi di errore leggibili; exit code != 0 = depth non disponibile

Modello: MiDaS_small via torch.hub (scaricato al primo uso in ~/.cache/torch).
Scelto piccolo di proposito: gira in CPU anche con poca RAM accanto al VLM.

Setup:
  pip install --break-system-packages torch torchvision timm opencv-python-headless numpy
"""
import sys


def fail(msg: str, code: int = 1):
    print(f"depth_infer: {msg}", file=sys.stderr)
    sys.exit(code)


def main():
    raw = sys.stdin.buffer.read()
    if not raw:
        fail("nessun JPEG su stdin")

    try:
        import numpy as np
        import cv2
    except ImportError as e:
        fail(f"modulo mancante ({e}) — pip install opencv-python-headless numpy")

    img = cv2.imdecode(np.frombuffer(raw, np.uint8), cv2.IMREAD_COLOR)
    if img is None:
        fail("JPEG non decodificabile")

    try:
        import torch
    except ImportError:
        fail("torch non installato — pip install torch torchvision timm")

    # MiDaS_small carica internamente rwightman/gen-efficientnet-pytorch SENZA
    # trust_repo: torch chiederebbe conferma su stdin — ma il nostro stdin è il
    # JPEG, già consumato → EOFError. Pre-autorizziamo i repo nella trusted_list.
    import os
    hub_dir = torch.hub.get_dir()
    os.makedirs(hub_dir, exist_ok=True)
    tl_path = os.path.join(hub_dir, "trusted_list")
    trusted = set()
    if os.path.exists(tl_path):
        with open(tl_path) as f:
            trusted = set(f.read().split())
    for repo in ("intel-isl_MiDaS", "rwightman_gen-efficientnet-pytorch"):
        if repo not in trusted:
            with open(tl_path, "a") as f:
                f.write(repo + "\n")

    # hubconf di MiDaS stampa su stdout ("Loading weights: ...") e noi su stdout
    # emettiamo il JPEG binario: durante il load dirottiamo stdout su stderr.
    import contextlib
    try:
        with contextlib.redirect_stdout(sys.stderr):
            midas = torch.hub.load("intel-isl/MiDaS", "MiDaS_small",
                                   trust_repo=True, verbose=False)
            transforms = torch.hub.load("intel-isl/MiDaS", "transforms",
                                        trust_repo=True, verbose=False)
    except Exception as e:
        fail(f"caricamento MiDaS fallito (serve rete al primo uso): {e}")

    midas.eval()
    rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    batch = transforms.small_transform(rgb)

    with torch.no_grad():
        pred = midas(batch)
        pred = torch.nn.functional.interpolate(
            pred.unsqueeze(1), size=img.shape[:2],
            mode="bicubic", align_corners=False).squeeze()

    depth = pred.cpu().numpy()
    lo, hi = depth.min(), depth.max()
    if hi - lo < 1e-6:
        fail("depth piatta — immagine non valida")
    norm = ((depth - lo) / (hi - lo) * 255.0).astype("uint8")
    colored = cv2.applyColorMap(norm, cv2.COLORMAP_JET)  # vicino=rosso, lontano=blu

    ok, buf = cv2.imencode(".jpg", colored, [cv2.IMWRITE_JPEG_QUALITY, 88])
    if not ok:
        fail("encoding JPEG fallito")
    sys.stdout.buffer.write(buf.tobytes())


if __name__ == "__main__":
    main()

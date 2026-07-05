#!/usr/bin/env python3
"""
make_aruco_targets.py — genera bersagli ArUco/CharUco STAMPABILI a scala reale
per dare scala e riferimento alle scansioni Prismalux Vision3D.

Produce due PDF A4 (a dimensione METRICA esatta: stampa "a grandezza reale"
/ 100% / senza adatta-alla-pagina):
  1) charuco_board_A4.pdf   → una board CharUco da mettere SOTTO l'oggetto
  2) aruco_markers_A4.pdf   → marker singoli 4 cm da ritagliare e disporre a cerchio

Dizionario: DICT_4X4_50 (robusto, pochi bit → letto anche da lontano).
Il lato marker di default è 40 mm (4 cm) come richiesto.

Setup:
  pip install --break-system-packages opencv-contrib-python numpy reportlab

Uso:
  python3 make_aruco_targets.py
  python3 make_aruco_targets.py --marker-mm 40 --dict 4X4_50
"""
import argparse
import io
import numpy as np
import cv2
import cv2.aruco as aruco
from reportlab.pdfgen import canvas
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib.utils import ImageReader

DICTS = {
    "4X4_50":  aruco.DICT_4X4_50,
    "4X4_100": aruco.DICT_4X4_100,
    "5X5_100": aruco.DICT_5X5_100,
    "6X6_250": aruco.DICT_6X6_250,
}

def marker_png(dictionary, marker_id, px=600):
    """Genera un marker come array RGB (numpy) ad alta risoluzione."""
    img = aruco.generateImageMarker(dictionary, marker_id, px)
    return cv2.cvtColor(img, cv2.COLOR_GRAY2RGB)

def np_to_reader(arr):
    ok, buf = cv2.imencode(".png", arr)
    return ImageReader(io.BytesIO(buf.tobytes()))

# ---------------------------------------------------------------------------
def make_charuco(path, dict_name, squares_x=5, squares_y=7,
                 square_mm=30.0, marker_ratio=0.75):
    """
    Board CharUco: scacchiera con marker nei quadrati bianchi.
    square_mm = lato di ogni quadrato in mm (deve risultare esatto in stampa).
    """
    dictionary = aruco.getPredefinedDictionary(DICTS[dict_name])
    marker_mm = square_mm * marker_ratio
    board = aruco.CharucoBoard((squares_x, squares_y),
                               square_mm/1000.0, marker_mm/1000.0, dictionary)

    # render ad alta risoluzione (10 px/mm)
    ppmm = 10
    w_px = int(squares_x * square_mm * ppmm)
    h_px = int(squares_y * square_mm * ppmm)
    img = board.generateImage((w_px, h_px), marginSize=0, borderBits=1)
    img_rgb = cv2.cvtColor(img, cv2.COLOR_GRAY2RGB)

    c = canvas.Canvas(path, pagesize=A4)
    pw, ph = A4
    board_w = squares_x * square_mm * mm
    board_h = squares_y * square_mm * mm
    x = (pw - board_w) / 2
    y = (ph - board_h) / 2
    c.drawImage(np_to_reader(img_rgb), x, y, width=board_w, height=board_h)

    # etichetta con i parametri esatti (servono al detector)
    c.setFont("Helvetica", 9)
    c.drawString(15*mm, 12*mm,
        f"CharUco {squares_x}x{squares_y} | quadrato={square_mm:.1f}mm "
        f"marker={marker_mm:.1f}mm | dict={dict_name} | "
        f"STAMPA AL 100% (no adatta pagina)")
    # riga di verifica: 100 mm da misurare col righello
    c.setLineWidth(1)
    c.line(15*mm, 20*mm, 115*mm, 20*mm)
    c.drawString(15*mm, 22*mm, "Verifica stampa: questa linea deve misurare 100 mm")
    c.showPage(); c.save()
    return dict(squares=(squares_x,squares_y), square_mm=square_mm,
               marker_mm=marker_mm, dict=dict_name)

# ---------------------------------------------------------------------------
def make_markers_sheet(path, dict_name, marker_mm=40.0,
                       ids=range(0, 8), cols=2):
    """Foglio di marker ArUco singoli, ognuno con bordo bianco e ID stampato."""
    dictionary = aruco.getPredefinedDictionary(DICTS[dict_name])
    c = canvas.Canvas(path, pagesize=A4)
    pw, ph = A4

    quiet = marker_mm * 0.20            # bordo bianco (quiet zone) = 20% del lato
    cell_w = marker_mm + 2*quiet
    cell_h = marker_mm + 2*quiet + 6    # +spazio per etichetta ID
    gap = 12*mm
    margin_x = 20*mm
    top = ph - 25*mm

    ids = list(ids)
    rows = (len(ids) + cols - 1)//cols
    for i, mid in enumerate(ids):
        r = i // cols
        col = i % cols
        cx = margin_x + col * (cell_w*mm + gap)
        cy = top - (r+1) * (cell_h*mm + gap)

        arr = marker_png(dictionary, mid, px=600)
        c.drawImage(np_to_reader(arr),
                    cx + quiet*mm, cy + quiet*mm + 6*mm,
                    width=marker_mm*mm, height=marker_mm*mm)
        # cornice quiet-zone (aiuta il ritaglio)
        c.setLineWidth(0.3)
        c.rect(cx, cy + 6*mm, cell_w*mm, (marker_mm+2*quiet)*mm)
        c.setFont("Helvetica-Bold", 9)
        c.drawString(cx + 2*mm, cy + 1*mm, f"ID {mid}  |  {marker_mm:.0f} mm")

    c.setFont("Helvetica", 9)
    c.drawString(15*mm, 12*mm,
        f"ArUco dict={dict_name} lato={marker_mm:.0f}mm | "
        f"STAMPA AL 100% | ritaglia lungo il rettangolo (lascia il bordo bianco)")
    c.line(15*mm, 20*mm, 115*mm, 20*mm)
    c.drawString(15*mm, 22*mm, "Verifica stampa: questa linea deve misurare 100 mm")
    c.showPage(); c.save()
    return dict(count=len(ids), marker_mm=marker_mm, dict=dict_name)

# ---------------------------------------------------------------------------
if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--marker-mm", type=float, default=40.0,
                    help="lato marker singoli in mm (default 40 = 4 cm)")
    ap.add_argument("--square-mm", type=float, default=30.0,
                    help="lato quadrato CharUco in mm (default 30)")
    ap.add_argument("--dict", default="4X4_50", choices=list(DICTS.keys()))
    ap.add_argument("--out", default=".")
    args = ap.parse_args()

    ch = make_charuco(f"{args.out}/charuco_board_A4.pdf", args.dict,
                      square_mm=args.square_mm)
    mk = make_markers_sheet(f"{args.out}/aruco_markers_A4.pdf", args.dict,
                            marker_mm=args.marker_mm, ids=range(0, 6))
    print("Generati:")
    print(f"  charuco_board_A4.pdf  -> {ch}")
    print(f"  aruco_markers_A4.pdf  -> {mk}")
    print("\nIMPORTANTE: stampa al 100% (niente 'adatta alla pagina').")
    print("Controlla col righello la linea di verifica da 100 mm su ogni foglio.")

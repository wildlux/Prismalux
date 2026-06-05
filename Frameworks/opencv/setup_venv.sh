#!/bin/bash
# Crea un venv Python dedicato con cv2 + pytesseract in Frameworks/opencv/venv/
# Usato da Prismalux per isolare le dipendenze OpenCV dal sistema.
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV="$SCRIPT_DIR/venv"

echo "[OpenCV] Creo venv in: $VENV"
python3 -m venv "$VENV"
"$VENV/bin/pip" install --upgrade pip
"$VENV/bin/pip" install opencv-python-headless pytesseract

echo ""
echo "[OpenCV] Fatto! Dipendenze installate:"
"$VENV/bin/pip" show opencv-python-headless pytesseract | grep -E "^Name|^Version"

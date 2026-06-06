#!/bin/bash
# Scarica all-MiniLM-L6-v2 (ONNX) da Hugging Face in Frameworks/onnx/models/
# Dimensione: ~22 MB per il .onnx, ~226 KB per vocab.txt
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="$SCRIPT_DIR/models"
BASE_URL="https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2/resolve/main/onnx"

echo "[ONNX] Scarico all-MiniLM-L6-v2 in: $DEST"
mkdir -p "$DEST"

echo "[ONNX] Scarico model.onnx (~22 MB)..."
curl -L -o "$DEST/all-MiniLM-L6-v2.onnx" "$BASE_URL/model.onnx"

echo "[ONNX] Scarico vocab.txt (~226 KB)..."
curl -L -o "$DEST/vocab.txt" \
    "https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2/resolve/main/tokenizer/vocab.txt"

echo ""
echo "[ONNX] Fatto! File salvati in $DEST"
echo "  - all-MiniLM-L6-v2.onnx"
echo "  - vocab.txt"
echo ""
echo "Riavvia Prismalux per attivare l'embedding locale."

#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════
#  build_appimage_docker.sh — Genera l'AppImage in un container Ubuntu 24.04
#
#  Uso:
#    bash Tools/docker/build_appimage_docker.sh
#    IMAGE=prismalux-appimage-builder bash Tools/docker/build_appimage_docker.sh
#
#  Output:
#    EXPORT/linux/Prismalux-x86_64.AppImage
#    EXPORT/linux/Prismalux-x86_64.AppImage.sha256
# ═══════════════════════════════════════════════════════════════════════
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
IMAGE="${IMAGE:-prismalux-appimage-builder}"
OUT_DIR="${ROOT}/EXPORT/linux"
OUT="${OUT_DIR}/Prismalux-x86_64.AppImage"

command -v docker &>/dev/null || {
    echo "❌ Docker non trovato. Installa Docker e riprova." >&2
    exit 1
}
docker info &>/dev/null || {
    echo "❌ Docker non è in esecuzione." >&2
    exit 1
}

mkdir -p "$OUT_DIR"

echo "▶ Build immagine builder (Ubuntu 24.04 + Qt 6.4)..."
docker build \
    -t "$IMAGE" \
    -f "$SCRIPT_DIR/Dockerfile.appimage" \
    "$ROOT"

echo "▶ Estrazione AppImage..."
cid="$(docker create "$IMAGE")"
trap 'docker rm -f "$cid" >/dev/null 2>&1 || true' EXIT

docker cp "$cid:/out/Prismalux-x86_64.AppImage" "$OUT"
docker cp "$cid:/out/Prismalux-x86_64.AppImage.sha256" "${OUT}.sha256"

chmod +x "$OUT"
# Riscrive il checksum con path relativo, così `sha256sum -c` funziona
# anche lanciato dalla root del repo.
( cd "$OUT_DIR" && sha256sum "$(basename "$OUT")" > "$(basename "$OUT").sha256" )

echo "✅ AppImage pronta: $OUT"
echo "   $(du -h "$OUT" | cut -f1) — verifica: (cd EXPORT/linux && sha256sum -c Prismalux-x86_64.AppImage.sha256)"

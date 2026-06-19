#!/bin/bash
# setup.sh — Installa Flutter e prepara il progetto Prismalux Mobile

set -e

FLUTTER_VERSION="3.24.5"
FLUTTER_DIR="$HOME/flutter"

echo "=== Prismalux Mobile — Setup Flutter ==="

# 1. Controlla se Flutter è già installato
if command -v flutter &>/dev/null; then
    echo "Flutter già installato: $(flutter --version | head -1)"
else
    echo "Scarico Flutter $FLUTTER_VERSION..."
    wget -q "https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_${FLUTTER_VERSION}-stable.tar.xz" \
        -O /tmp/flutter.tar.xz
    tar xf /tmp/flutter.tar.xz -C "$HOME"
    rm /tmp/flutter.tar.xz
    echo "export PATH=\"\$PATH:$FLUTTER_DIR/bin\"" >> "$HOME/.bashrc"
    export PATH="$PATH:$FLUTTER_DIR/bin"
    echo "Flutter installato in $FLUTTER_DIR"
fi

# 2. Accetta licenze Android (se Android SDK presente)
flutter doctor --android-licenses 2>/dev/null || true

# 3. Entra nella cartella del progetto
cd "$(dirname "$0")"

# 4. Inizializza il progetto Flutter (genera android/, ios/, web/ ecc.)
if [ ! -d "android" ]; then
    echo "Inizializzo progetto Flutter..."
    flutter create . \
        --project-name prismalux_mobile \
        --org com.prismalux \
        --platforms android,ios,web \
        --description "Prismalux Mobile — client Flutter per le API Qt6"
    echo "Progetto inizializzato."
fi

# 5. Installa le dipendenze
echo "Installo dipendenze..."
flutter pub get

echo ""
echo "=== Setup completato! ==="
echo ""
echo "Per eseguire l'app:"
echo "  cd $(pwd)"
echo "  flutter run                    # su dispositivo/emulatore connesso"
echo "  flutter run -d chrome          # nel browser"
echo "  flutter build apk --release    # APK Android"
echo "  flutter build appbundle        # AAB per Play Store"
echo ""
echo "Prima di avviare, assicurati che il server LAN Prismalux"
echo "sia in ascolto (tab LAN & WAN → Avvia server LAN)."
echo "Porta default: 11500"

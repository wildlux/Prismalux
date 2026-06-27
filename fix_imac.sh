#!/usr/bin/env bash
# Esegui questo script dall'iMac dopo aver montato la share:
#   bash /mnt/prismalux/fix_imac.sh
set -e

IMAC_ROOT="$HOME/Desktop/Prismalux"

if [ ! -d "$IMAC_ROOT" ]; then
    IMAC_ROOT="$HOME/desktop/Prismalux"
fi

if [ ! -d "$IMAC_ROOT" ]; then
    echo "Errore: cartella Prismalux non trovata in $HOME/Desktop o $HOME/desktop"
    exit 1
fi

echo "==> Copio aggiorna.sh in $IMAC_ROOT ..."
cp "$(dirname "$0")/aggiorna.sh" "$IMAC_ROOT/aggiorna.sh"
chmod +x "$IMAC_ROOT/aggiorna.sh"

echo "==> Avvio build..."
cd "$IMAC_ROOT"
bash ./aggiorna.sh

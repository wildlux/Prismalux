#!/bin/bash
# Launcher Prismalux GUI — usabile sia da terminale che dal .desktop
BINARY="/home/wildlux/Desktop/Prismalux/gui/build/Prismalux_GUI"

if [ ! -f "$BINARY" ]; then
    echo "Errore: binario non trovato in $BINARY"
    echo "Ricompila con: cd /home/wildlux/Desktop/Prismalux/gui && cmake --build build -j\$(nproc)"
    exit 1
fi

nohup "$BINARY" > /dev/null 2>&1 &

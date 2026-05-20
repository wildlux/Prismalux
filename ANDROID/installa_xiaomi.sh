#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════
#  installa_xiaomi.sh — invia e installa PrismaluxMobile.apk
#  sullo Xiaomi collegato via USB (adb).
#
#  Prerequisiti:
#    1. Xiaomi: Impostazioni → Info telefono → tocca "Versione MIUI"
#       7 volte → torna indietro → Opzioni sviluppatore →
#       "Debug USB" ON  +  "Installa via USB" ON
#    2. Sul PC: adb deve essere nel PATH   (sudo apt install adb)
#    3. Collega il cavo USB e, al primo avvio, accetta il popup
#       "Consenti il debug USB da questo computer" sul telefono.
# ══════════════════════════════════════════════════════════════════

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APK="$SCRIPT_DIR/PrismaluxMobile.apk"

# ── Palette colori ─────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'

info()    { echo -e "${CYAN}[INFO]${RESET}  $*"; }
ok()      { echo -e "${GREEN}[OK]${RESET}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
fail()    { echo -e "${RED}[ERR]${RESET}   $*" >&2; exit 1; }

echo -e "${BOLD}══════════════════════════════════════════${RESET}"
echo -e "${BOLD}   PrismaluxMobile — Deploy su Xiaomi     ${RESET}"
echo -e "${BOLD}══════════════════════════════════════════${RESET}"
echo

# ── 1. Verifica APK ────────────────────────────────────────────
[[ -f "$APK" ]] || fail "APK non trovato: $APK\nEsegui prima la build Android."
APK_SIZE=$(du -sh "$APK" | cut -f1)
info "APK: $APK  ($APK_SIZE)"

# ── 2. Verifica adb ────────────────────────────────────────────
command -v adb &>/dev/null || fail "adb non trovato. Installa: sudo apt install adb"

# ── 3. Attendi dispositivo (max 30 secondi) ────────────────────
info "Ricerca dispositivo USB..."
ADB_SERIAL=""
for i in $(seq 1 30); do
    # Lista dispositivi: prendi il primo "device" (non "unauthorized" o "offline")
    ADB_SERIAL=$(adb devices 2>/dev/null \
        | grep -E "^[^\s]+\s+device$" \
        | head -1 | awk '{print $1}') || true
    if [[ -n "$ADB_SERIAL" ]]; then
        break
    fi
    if (( i == 1 )); then
        warn "Nessun dispositivo rilevato. Controlla:"
        warn "  • Cavo USB collegato"
        warn "  • Debug USB attivo sullo Xiaomi"
        warn "  • Sblocca il telefono e accetta il popup debug"
        warn "In attesa (max 30 s)..."
    fi
    sleep 1
done

[[ -n "$ADB_SERIAL" ]] || fail "Nessun dispositivo trovato dopo 30 secondi."

# ── 4. Info dispositivo ────────────────────────────────────────
BRAND=$(adb -s "$ADB_SERIAL" shell getprop ro.product.brand 2>/dev/null | tr -d '\r')
MODEL=$(adb -s "$ADB_SERIAL" shell getprop ro.product.model 2>/dev/null | tr -d '\r')
ANDROID_VER=$(adb -s "$ADB_SERIAL" shell getprop ro.build.version.release 2>/dev/null | tr -d '\r')
ok "Connesso: $BRAND $MODEL  (Android $ANDROID_VER)  [$ADB_SERIAL]"

# ── 5. Controlla se l'app è già in esecuzione; fermala ─────────
PKG="org.prismalux.mobile"
RUNNING=$(adb -s "$ADB_SERIAL" shell pidof "$PKG" 2>/dev/null | tr -d '\r' || true)
if [[ -n "$RUNNING" ]]; then
    info "App in esecuzione — la chiudo prima dell'installazione..."
    adb -s "$ADB_SERIAL" shell am force-stop "$PKG" &>/dev/null || true
fi

# ── 6. Installa APK ────────────────────────────────────────────
info "Installazione APK in corso..."
# -r = reinstalla mantenendo i dati
# -d = consenti versione precedente (utile in sviluppo)
if adb -s "$ADB_SERIAL" install -r -d "$APK" 2>&1; then
    ok "Installazione completata con successo!"
else
    # Su Xiaomi MIUI/HyperOS può servire --bypass-low-target-sdk-block
    warn "Primo tentativo fallito, riprovo con flag aggiuntivi..."
    adb -s "$ADB_SERIAL" install -r -d --bypass-low-target-sdk-block "$APK" \
        || fail "Installazione fallita. Controlla i log sopra."
fi

# ── 7. Avvia l'app ────────────────────────────────────────────
info "Avvio PrismaluxMobile..."
adb -s "$ADB_SERIAL" shell am start \
    -n "${PKG}/${PKG}.MainActivity" \
    --activity-clear-top \
    2>/dev/null || \
adb -s "$ADB_SERIAL" shell monkey \
    -p "$PKG" -c android.intent.category.LAUNCHER 1 \
    2>/dev/null || \
warn "Avvio automatico non riuscito — apri l'app manualmente."

echo
echo -e "${BOLD}══════════════════════════════════════════${RESET}"
ok "Deploy completato!  L'app è installata su $BRAND $MODEL."
echo -e "${BOLD}══════════════════════════════════════════${RESET}"
echo
info "Comandi utili:"
echo "  Logcat app:   adb -s $ADB_SERIAL logcat -s Qt"
echo "  Disinstalla:  adb -s $ADB_SERIAL uninstall $PKG"
echo "  Screenshot:   adb -s $ADB_SERIAL exec-out screencap -p > screen.png"

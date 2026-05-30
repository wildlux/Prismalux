#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════
# test_apk.sh — Suite di smoke test per PrismaluxMobile APK
#
# Uso:
#   cd ANDROID && bash test_apk.sh [DEVICE_SERIAL]
#
# Prerequisiti: adb nel PATH, python3 con Pillow (pip install Pillow)
#
# Cosa viene testato:
#   T01  Device ADB connesso
#   T02  APK installabile
#   T03  App si avvia senza crash
#   T04  Nessun FATAL/SIGSEGV nel logcat all'avvio
#   T05  Pagina Chat visibile (pixel accent color nella zona backend)
#   T06  Backend switching: Cloud → colore cambia
#   T07  Backend switching: Server locale → ripristino
#   T08  Drawer si apre (swipe dal bordo sinistro)
#   T09  Navigazione Studia (tap voce drawer)
#   T10  Navigazione Impostazioni (tap voce drawer)
#   T11  App sopravvive a 10 tap casuali al centro schermo
#   T12  Nessun crash nel logcat dopo tutta la navigazione
#   T13  App ancora viva a fine test
# ══════════════════════════════════════════════════════════════════
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

APK="$SCRIPT_DIR/PrismaluxMobile.apk"
PKG="org.prismalux.mobile"
ACT="org.qtproject.qt.android.bindings.QtActivity"
SC_DIR="$SCRIPT_DIR/test_screenshots"
DEVICE="${1:-wspvrg4p8hugkzjf}"

# ── Palette ───────────────────────────────────────────────────────
GR='\033[0;32m'; RD='\033[0;31m'; YL='\033[1;33m'
CY='\033[0;36m'; BD='\033[1m';    NC='\033[0m'

PASS=0; FAIL=0; WARN=0

pass() { echo -e "${GR}[PASS]${NC} $*"; ((PASS++)); }
fail() { echo -e "${RD}[FAIL]${NC} $*"; ((FAIL++)); }
warn() { echo -e "${YL}[WARN]${NC} $*"; ((WARN++)); }
info() { echo -e "${CY}[INFO]${NC} $*"; }
step() { echo -e "\n${BD}── $* ──${NC}"; }

ADB="adb -s $DEVICE"

# ── Helper: screenshot + pull ─────────────────────────────────────
mkdir -p "$SC_DIR"
sc() {
    local name="$1"
    $ADB shell screencap /sdcard/_sc.png 2>/dev/null
    $ADB pull /sdcard/_sc.png "$SC_DIR/${name}.png" >/dev/null 2>&1
}

# ── Helper: pixel color con Python/Pillow ─────────────────────────
# Ritorna 0 se il pixel a (x,y) è "luminoso" (R+G+B > soglia)
pixel_bright() {
    local file="$1" x="$2" y="$3" thresh="${4:-300}"
    python3 - "$file" "$x" "$y" "$thresh" <<'EOF' 2>/dev/null
import sys
from PIL import Image
img = Image.open(sys.argv[1]).convert("RGB")
r,g,b = img.getpixel((int(sys.argv[2]), int(sys.argv[3])))
sys.exit(0 if r+g+b > int(sys.argv[4]) else 1)
EOF
}

# ── Helper: aspetta che l'app sia viva ───────────────────────────
wait_alive() {
    for _ in 1 2 3 4 5; do
        sleep 1
        $ADB shell pidof "$PKG" >/dev/null 2>&1 && return 0
    done
    return 1
}

alive() { $ADB shell pidof "$PKG" >/dev/null 2>&1; }

# ══════════════════════════════════════════════════════════════════
step "T01 — Device ADB connesso"
STATE=$($ADB get-state 2>/dev/null || echo "error")
if [ "$STATE" = "device" ]; then
    SCREEN=$($ADB shell wm size | awk '{print $3}')
    W=$(echo "$SCREEN" | cut -dx -f1)
    H=$(echo "$SCREEN" | cut -dx -f2)
    DENSITY=$($ADB shell wm density | awk '{print $NF}')
    pass "Device connesso — schermo ${W}x${H} @ ${DENSITY}dpi"
else
    fail "Device non connesso (stato: $STATE)"
    echo -e "${RD}Impossibile proseguire.${NC}"; exit 1
fi

# Coordinate fisiche calcolate su 1080×2340 @ ~461dpi
# (ricalibrate se il device cambia)
HMB_X=65;  HMB_Y=125          # hamburger ☰
CLOUD_X=120; CLOUD_Y=350       # bottone Cloud
SRV_X=355;  SRV_Y=350         # bottone Server locale
LOC_X=590;  LOC_Y=350         # bottone Modello locale
INPUT_X=430; INPUT_Y=2040     # campo testo chat
SEND_X=660;  SEND_Y=2040      # ↑ Invia
CENTER_X=$((W/2)); CENTER_Y=$((H/2))

# ══════════════════════════════════════════════════════════════════
step "T02 — APK installabile"
if [ ! -f "$APK" ]; then
    fail "APK non trovato: $APK"
else
    if $ADB install -r -d "$APK" >/dev/null 2>&1; then
        pass "APK installato con successo"
    else
        fail "Installazione APK fallita"
    fi
fi

# ══════════════════════════════════════════════════════════════════
step "T03 — Avvio app senza crash"
$ADB shell am force-stop "$PKG" 2>/dev/null || true
$ADB logcat -c 2>/dev/null || true
sleep 1
$ADB shell am start -n "$PKG/$ACT" >/dev/null 2>&1
if wait_alive; then
    pass "App avviata (PID=$($ADB shell pidof "$PKG" | tr -d '\r'))"
    sc "T03_startup"
else
    fail "App non avviata o crash immediato"
fi

# ══════════════════════════════════════════════════════════════════
step "T04 — Logcat: nessun crash all'avvio"
sleep 2
CRASHES=$($ADB logcat -d 2>/dev/null \
    | grep -cE "FATAL EXCEPTION|signal 11|SIGSEGV|AndroidRuntime.*Error" || true)
if [ "$CRASHES" -eq 0 ]; then
    pass "Nessun crash nel logcat"
else
    fail "Rilevati $CRASHES errori critici nel logcat"
    $ADB logcat -d 2>/dev/null | grep -E "FATAL|SIGSEGV" | tail -5
fi

# ══════════════════════════════════════════════════════════════════
step "T05 — Pagina Chat: colori backend visibili"
sc "T05_chat"
PILLOW_OK=true
python3 -c "from PIL import Image" 2>/dev/null || PILLOW_OK=false

if $PILLOW_OK; then
    # "Server locale" deve essere cyan/luminoso a (355, 350)
    if pixel_bright "$SC_DIR/T05_chat.png" $SRV_X $SRV_Y 350; then
        pass "Pixel 'Server locale' luminoso — bottone selezionato visibile"
    else
        warn "Pixel 'Server locale' scuro — bottone potrebbe non essere visibile"
    fi
else
    warn "Pillow non installato — salto verifica colori (pip install Pillow)"
fi

# ══════════════════════════════════════════════════════════════════
step "T06 — Backend switching: Server → Cloud"
$ADB shell input tap $CLOUD_X $CLOUD_Y; sleep 1.5
sc "T06_cloud_active"
if alive; then
    pass "App viva dopo tap Cloud"
else
    fail "App crashata durante switch Cloud"
fi

if $PILLOW_OK; then
    if pixel_bright "$SC_DIR/T06_cloud_active.png" $CLOUD_X $CLOUD_Y 350; then
        pass "Cloud button luminoso dopo selezione"
    else
        warn "Cloud button non luminoso — verifica visivamente T06_cloud_active.png"
    fi
fi

# ══════════════════════════════════════════════════════════════════
step "T07 — Backend switching: Cloud → Server locale"
$ADB shell input tap $SRV_X $SRV_Y; sleep 1.5
sc "T07_server_active"
if alive; then
    pass "App viva dopo ritorno a Server locale"
else
    fail "App crashata durante switch Server locale"
fi

if $PILLOW_OK; then
    if pixel_bright "$SC_DIR/T07_server_active.png" $SRV_X $SRV_Y 350; then
        pass "Server locale button luminoso dopo selezione"
    else
        warn "Server locale non luminoso — verifica visivamente T07_server_active.png"
    fi
fi

# ══════════════════════════════════════════════════════════════════
step "T08 — Drawer: apre con swipe bordo sinistro"
# Swipe dal bordo sx (x=5) verso destra (x=400), 300ms
$ADB shell input swipe 5 $CENTER_Y 400 $CENTER_Y 300; sleep 1.2
sc "T08_drawer_open"
if alive; then
    pass "App viva dopo apertura drawer"
else
    fail "App crashata durante apertura drawer"
fi
# Chiudi drawer con tap fuori
$ADB shell input tap 700 $CENTER_Y; sleep 0.8

# ══════════════════════════════════════════════════════════════════
step "T09 — Navigazione: Studia"
# Apri drawer e tappa seconda voce (Studia, idx 1)
# Drawer: width~300px, header~150px, ogni item ~230px fisici
$ADB shell input swipe 5 $CENTER_Y 400 $CENTER_Y 300; sleep 1
DRAWER_ITEM_X=150
STUDIA_Y=500   # header(150) + 1.5*item(230) ≈ 495
$ADB shell input tap $DRAWER_ITEM_X $STUDIA_Y; sleep 2
sc "T09_studia"
if alive; then
    pass "App viva su pagina Studia"
else
    fail "App crashata su pagina Studia"
fi

# ══════════════════════════════════════════════════════════════════
step "T10 — Navigazione: Impostazioni"
$ADB shell input swipe 5 $CENTER_Y 400 $CENTER_Y 300; sleep 1
# Impostazioni = item 6 (0-based): header(150) + 6.5*230 ≈ 1645
SETTINGS_Y=1645
$ADB shell input tap $DRAWER_ITEM_X $SETTINGS_Y; sleep 2
sc "T10_impostazioni"
if alive; then
    pass "App viva su pagina Impostazioni"
else
    fail "App crashata su pagina Impostazioni"
fi

# ══════════════════════════════════════════════════════════════════
step "T11 — Stress: 10 tap casuali zona contenuto"
for i in $(seq 1 10); do
    TX=$(( (RANDOM % 800) + 100 ))
    TY=$(( (RANDOM % 1800) + 200 ))
    $ADB shell input tap $TX $TY
    sleep 0.3
done
sleep 1
if alive; then
    pass "App sopravvive a 10 tap casuali"
else
    fail "App crashata durante stress tap"
fi

# ══════════════════════════════════════════════════════════════════
step "T12 — Logcat: nessun crash durante tutta la sessione"
CRASHES=$($ADB logcat -d 2>/dev/null \
    | grep -cE "FATAL EXCEPTION|signal 11|SIGSEGV|Force.*stopped" || true)
if [ "$CRASHES" -eq 0 ]; then
    pass "Nessun crash critico nel logcat dell'intera sessione"
else
    fail "$CRASHES errori critici nel logcat durante la sessione"
    $ADB logcat -d 2>/dev/null | grep -E "FATAL|SIGSEGV|Force.*stopped" | tail -10
fi

# ══════════════════════════════════════════════════════════════════
step "T13 — App ancora viva a fine test"
if alive; then
    pass "App in esecuzione — PID=$($ADB shell pidof "$PKG" | tr -d '\r')"
else
    fail "App non più in esecuzione"
fi
sc "T13_final"

# ══════════════════════════════════════════════════════════════════
echo ""
echo -e "${BD}════════════════════════════════════════════${NC}"
echo -e "${BD}  Risultati: ${GR}$PASS PASS${NC}  ${RD}$FAIL FAIL${NC}  ${YL}$WARN WARN${NC}"
echo -e "${BD}  Screenshot: $SC_DIR/${NC}"
echo -e "${BD}════════════════════════════════════════════${NC}"
echo ""

if [ "$FAIL" -gt 0 ]; then
    echo -e "${RD}Alcuni test FALLITI. Controlla i log sopra.${NC}"
    exit 1
fi
echo -e "${GR}Tutti i test PASSATI.${NC}"
exit 0

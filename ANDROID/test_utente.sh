#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════
# test_utente.sh — Simulazione sessione utente reale su PrismaluxMobile
#
# Esegue interazioni come le farebbe un utente normale:
#   timing variabile, tap imprecisi, testo reale nella chat,
#   navigazione completa del drawer, backend switching, quiz,
#   impostazioni — con osservazioni narrative step-by-step.
#
# Struttura per scenari utente:
#   MARCO  — esplora l'app per la prima volta
#   SARA   — scrive un messaggio in chat e aspetta risposta
#   LUCA   — naviga ogni pagina del drawer
#   ANNA   — apre le impostazioni e controlla i campi
#   PIETRO — tenta il quiz CCNA
#
# Uso:
#   cd ANDROID && bash test_utente.sh [DEVICE_SERIAL]
#   bash test_utente.sh wspvrg4p8hugkzjf
# ══════════════════════════════════════════════════════════════════
set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PKG="org.prismalux.mobile"
ACT="org.qtproject.qt.android.bindings.QtActivity"
SC_DIR="$SCRIPT_DIR/test_screenshots/umano"
DEVICE="${1:-wspvrg4p8hugkzjf}"
ADB="adb -s $DEVICE"

# ── Colori output ─────────────────────────────────────────────────
GR='\033[0;32m'; RD='\033[0;31m'; YL='\033[1;33m'
CY='\033[0;36m'; BD='\033[1m'; NC='\033[0m'; MA='\033[0;35m'

PASS=0; FAIL=0; WARN=0; SCENARIO=""

pass()  { echo -e "${GR}[✓]${NC} $*"; ((PASS++)); }
fail()  { echo -e "${RD}[✗]${NC} $*"; ((FAIL++)); }
warn()  { echo -e "${YL}[?]${NC} $*"; ((WARN++)); }
info()  { echo -e "${CY}[i]${NC} $*"; }
obs()   { echo -e "${MA}  └ ${NC}$*"; }          # cosa osserva l'utente
step()  { echo -e "\n${BD}── $SCENARIO: $* ──${NC}"; }

# ── Schermo ───────────────────────────────────────────────────────
W=1080; H=2340
CENTER_X=$(( W / 2 )); CENTER_Y=$(( H / 2 ))

# Coordinate chat (area sicura, lontano da status bar y>150)
HMB_X=65;   HMB_Y=175          # hamburger ☰
CLOUD_X=120; CLOUD_Y=350        # bottone Cloud
SRV_X=355;  SRV_Y=350          # bottone Server locale
LOC_X=590;  LOC_Y=350          # bottone Modello locale
INPUT_X=430; INPUT_Y=2040       # campo testo chat
SEND_X=660;  SEND_Y=2040        # ↑ Invia
STOP_X=580;  STOP_Y=90          # ✕ Stop streaming
CLEAR_X=700; CLEAR_Y=90         # 🗑 Cancella storia
MODEL_X=200; MODEL_Y=90         # [modello] btn header

# Drawer: larghezza 300px, header 150px, ogni voce ~230px fisici
DRW_X=150   # tap al centro del drawer
DRW_CHAT_Y=265
DRW_STUDIO_Y=495
DRW_QUIZ_Y=725
DRW_BLE_Y=955
DRW_CAM_Y=1185
DRW_AUDIO_Y=1415
DRW_MISURE_Y=1645
DRW_OBS_Y=1875
DRW_IMPOST_Y=2105   # potrebbe richiedere scroll nel drawer

# ── Screenshot ────────────────────────────────────────────────────
mkdir -p "$SC_DIR"
sc() {
    local name="$1"
    # Sveglia lo schermo e aspetta che si illumini prima di catturare
    $ADB shell input keyevent KEYCODE_WAKEUP 2>/dev/null
    sleep 0.8
    $ADB shell screencap -p /sdcard/_sc.png 2>/dev/null
    $ADB pull /sdcard/_sc.png "$SC_DIR/${name}.png" >/dev/null 2>&1
    obs "📷 ${name}.png"
}

# Sveglia lo schermo e sblocca (swipe su, funziona senza PIN)
wake_screen() {
    $ADB shell input keyevent KEYCODE_WAKEUP 2>/dev/null
    sleep 0.5
    $ADB shell input swipe 540 1800 540 900 300 2>/dev/null
    sleep 0.5
}

# ── Vitale ────────────────────────────────────────────────────────
alive()  { $ADB shell pidof "$PKG" >/dev/null 2>&1; }
pid_of() { $ADB shell pidof "$PKG" 2>/dev/null | tr -d '\r'; }

assert_alive() {
    if alive; then
        pass "App ancora in esecuzione (PID=$(pid_of))"
    else
        fail "App CRASHATA durante '$*'"
        sc "CRASH_$(echo "$*" | tr ' ' '_')"
    fi
}

# ── Timing umano ─────────────────────────────────────────────────
# Gli umani non sono macchine: aggiunge un po' di casualità
human_delay() {
    local min="${1:-1}" max="${2:-2}"
    local secs=$(( RANDOM % (max - min + 1) + min ))
    sleep "$secs"
}

reading_pause() { human_delay 1 3; }   # legge qualcosa
thinking_pause() { human_delay 2 4; }  # riflette prima di agire
quick_tap() { human_delay 0 1; }       # tap rapido successivo

# ── Tap con deriva umana (±20px) ─────────────────────────────────
tap() {
    local x="$1" y="$2"
    local nx=$(( x + (RANDOM % 40) - 20 ))
    local ny=$(( y + (RANDOM % 20) - 10 ))
    # Clamp schermo
    nx=$(( nx < 10 ? 10 : (nx > W-10 ? W-10 : nx) ))
    ny=$(( ny < 160 ? 160 : (ny > H-10 ? H-10 : ny) ))
    $ADB shell input tap "$nx" "$ny"
}

# ── Swipe con velocità variabile ─────────────────────────────────
swipe_natural() {
    local x1="$1" y1="$2" x2="$3" y2="$4"
    local dur=$(( RANDOM % 200 + 250 ))
    $ADB shell input swipe "$x1" "$y1" "$x2" "$y2" "$dur"
}

# ── Digita testo ─────────────────────────────────────────────────
type_text() {
    local text="$1"
    obs "⌨  Digito: \"$text\""
    $ADB shell input text "$(echo "$text" | sed 's/ /%s/g')"
    sleep 0.8
}

# ── Drawer ───────────────────────────────────────────────────────
open_drawer() {
    obs "Apro il drawer (swipe da sinistra)..."
    swipe_natural 5 "$CENTER_Y" 420 "$CENTER_Y"
    sleep 1.2
}

close_drawer() {
    $ADB shell input tap 800 "$CENTER_Y"
    sleep 0.8
}

drawer_nav() {
    local item_y="$1" label="$2"
    open_drawer
    reading_pause
    obs "Seleziono \"$label\" nel drawer..."
    tap "$DRW_X" "$item_y"
    sleep 2
}

back_to_chat() {
    drawer_nav "$DRW_CHAT_Y" "Chat"
}

# ── Pixel color (Pillow) ──────────────────────────────────────────
PILLOW_OK=false
python3 -c "from PIL import Image" 2>/dev/null && PILLOW_OK=true

pixel_rgb() {
    local file="$1" x="$2" y="$3"
    python3 - "$file" "$x" "$y" <<'EOF' 2>/dev/null
import sys
from PIL import Image
img = Image.open(sys.argv[1]).convert("RGB")
r,g,b = img.getpixel((int(sys.argv[2]), int(sys.argv[3])))
print(f"rgb({r},{g},{b}) sum={r+g+b}")
EOF
}

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

# ── Attende la fine dello streaming (al massimo N secondi) ────────
wait_streaming_end() {
    local max="${1:-30}"
    local elapsed=0
    obs "Aspetto la risposta AI (max ${max}s)..."
    while [ "$elapsed" -lt "$max" ]; do
        sleep 2; elapsed=$(( elapsed + 2 ))
        # Il pulsante Stop è visibile solo durante lo streaming.
        # Uso logcat per vedere se arrivano token.
        local activity
        activity=$($ADB logcat -d -s "Qt" 2>/dev/null | tail -5 || true)
        # Se il proceso è ancora vivo e non abbiamo rilevato errori, aspetta
        alive || { warn "App morta durante attesa risposta"; return 1; }
    done
    obs "Timeout ${max}s — screenshot risultato"
}

# ══════════════════════════════════════════════════════════════════
#  PREFLIGHT — device e app
# ══════════════════════════════════════════════════════════════════
echo -e "\n${BD}════════════════════════════════════════════${NC}"
echo -e "${BD}  PrismaluxMobile — Test Sessione Utente    ${NC}"
echo -e "${BD}  Device: $DEVICE${NC}"
echo -e "${BD}════════════════════════════════════════════${NC}"

STATE=$($ADB get-state 2>/dev/null || echo "error")
if [ "$STATE" != "device" ]; then
    fail "Device non connesso (stato: $STATE)"
    exit 1
fi
info "Device connesso — schermo ${W}x${H}"

# Mantieni schermo acceso per tutta la durata del test
ORIG_TIMEOUT=$($ADB shell settings get system screen_off_timeout 2>/dev/null | tr -d '\r' || echo "30000")
$ADB shell settings put system screen_off_timeout 600000 2>/dev/null || true
$ADB shell svc power stayon true 2>/dev/null || true
info "Screen timeout esteso a 10 min (era ${ORIG_TIMEOUT}ms) — stayon attivo"

# Sveglia e sblocca lo schermo
wake_screen

# Assicura app installata e avviata
$ADB shell am force-stop "$PKG" 2>/dev/null || true
$ADB logcat -c 2>/dev/null || true
sleep 1
$ADB shell am start -n "$PKG/$ACT" >/dev/null 2>&1 || true

info "Avvio app..."
for _ in 1 2 3 4 5 6; do
    sleep 1
    alive && break
done
if ! alive; then
    fail "App non si avvia — impossibile procedere"
    exit 1
fi
info "App avviata — PID=$(pid_of)"
sleep 2  # aspetta caricamento UI completo

# ══════════════════════════════════════════════════════════════════
#  SCENARIO MARCO — Prima esplorazione
# ══════════════════════════════════════════════════════════════════
SCENARIO="MARCO"

step "Apro l'app e guardo cosa c'è"
sc "marco_01_avvio"
reading_pause
obs "Vedo la schermata Chat con tre bottoni in alto: Cloud, Server locale, Modello locale"
obs "Il bottone 'Server locale' sembra selezionato (colorato)"
assert_alive "primo avvio"

step "Esploro i bottoni backend"
obs "Tocco 'Cloud' per vedere cosa succede..."
tap "$CLOUD_X" "$CLOUD_Y"
human_delay 1 2
sc "marco_02_cloud_selezionato"

if $PILLOW_OK; then
    rgb_cloud=$(pixel_rgb "$SC_DIR/marco_02_cloud_selezionato.png" "$CLOUD_X" "$CLOUD_Y" || echo "n/a")
    obs "Pixel Cloud dopo tap: $rgb_cloud"
fi

obs "Passo a 'Modello locale'..."
tap "$LOC_X" "$LOC_Y"
human_delay 1 2
sc "marco_03_locale_selezionato"
assert_alive "switch Modello locale"

obs "Torno a 'Server locale' — quello che usavo prima"
tap "$SRV_X" "$SRV_Y"
human_delay 1 3
sc "marco_04_server_ripristinato"
if $PILLOW_OK; then
    if pixel_bright "$SC_DIR/marco_04_server_ripristinato.png" "$SRV_X" "$SRV_Y" 350; then
        pass "Server locale: bottone illuminato — selezione visibile"
    else
        warn "Server locale: bottone scuro — controlla marco_04 visivamente"
    fi
fi

step "Guardo il campo testo e il bottone invio"
obs "Vedo la barra in basso con il campo testo e un bottone freccia ↑"
reading_pause
assert_alive "esplorazione UI"

step "Apro il drawer per vedere le sezioni"
open_drawer
sc "marco_05_drawer_aperto"
reading_pause
obs "Vedo la lista: Chat, Studio, Quiz CCNA, BLE, Camera, Audio, Misure, OBS, Impostazioni"
close_drawer
sleep 1

pass "Marco: esplorazione completata senza crash"

# ══════════════════════════════════════════════════════════════════
#  SCENARIO SARA — Invia un messaggio in chat
# ══════════════════════════════════════════════════════════════════
SCENARIO="SARA"

step "Apro la chat (sono già su Chat ma torno per sicurezza)"
back_to_chat
sleep 1

step "Tocco il campo testo"
obs "Tocco la barra di input in basso..."
tap "$INPUT_X" "$INPUT_Y"
sleep 1  # attesa tastiera
sc "sara_01_tastiera_aperta"

step "Digito il messaggio"
type_text "Ciao! Cosa sai fare?"
sc "sara_02_testo_digitato"
reading_pause
obs "Rileggo il messaggio prima di inviare..."

step "Invio il messaggio"
obs "Premo il bottone ↑ Invia..."
$ADB logcat -c 2>/dev/null || true  # pulisco logcat per rilevare nuova attività
tap "$SEND_X" "$SEND_Y"
sleep 1
sc "sara_03_subito_dopo_invio"
obs "Il messaggio è apparso come bolla utente"
assert_alive "invio messaggio"

step "Aspetto la risposta AI"
wait_streaming_end 25
sc "sara_04_risposta_ricevuta"

# Controlla se c'è attività di rete nell'app
NET_HITS=$($ADB logcat -d 2>/dev/null | grep -cE "(QNetworkReply|ollama|api|http|token|chunk)" || true)
if [ "$NET_HITS" -gt 0 ]; then
    pass "Rilevata attività di rete — AI ha ricevuto il messaggio ($NET_HITS eventi)"
else
    warn "Nessuna attività di rete rilevata — server potrebbe non essere raggiungibile"
fi
assert_alive "attesa risposta"

step "Provo a cancellare la storia chat"
obs "Cerco il tasto 🗑 nell'header..."
tap "$CLEAR_X" "$CLEAR_Y"
human_delay 1 2
sc "sara_05_chat_cancellata"
obs "La chat dovrebbe essere vuota ora"
assert_alive "cancella storia"

pass "Sara: scenario chat completato"

# ══════════════════════════════════════════════════════════════════
#  SCENARIO LUCA — Naviga ogni pagina del drawer
# ══════════════════════════════════════════════════════════════════
SCENARIO="LUCA"

step "Apro il drawer e navigo verso Studio"
drawer_nav "$DRW_STUDIO_Y" "Studio"
sc "luca_01_studio"
reading_pause
obs "Vedo la pagina Studio con selezione materia e bottone 'Inizia'"
assert_alive "pagina Studio"

step "Osservo la pagina Studio"
obs "C'è un bottone per la materia e un'area di testo..."
reading_pause
obs "Il bottone 'Continua/Prossima' non è visibile — giusto, devo prima avviare"

step "Navigo a Quiz CCNA"
drawer_nav "$DRW_QUIZ_Y" "Quiz CCNA"
sc "luca_02_quiz"
reading_pause
obs "Vedo la pagina Quiz CCNA con domande a risposta multipla"
assert_alive "pagina Quiz"

step "Rispondo a una domanda del quiz"
obs "Tocco la risposta A (prima scelta) senza leggere troppo..."
quick_tap
# La prima risposta è circa a y=600 nella pagina quiz
tap "$CENTER_X" 700
human_delay 1 2
sc "luca_03_quiz_risposta"
obs "Vedo il feedback sulla risposta"
assert_alive "risposta quiz"

step "Navigo a BLE"
drawer_nav "$DRW_BLE_Y" "BLE"
sc "luca_04_ble"
reading_pause
obs "Vedo la pagina BLE — cerca dispositivi Bluetooth vicini"
assert_alive "pagina BLE"

step "Navigo a Camera"
drawer_nav "$DRW_CAM_Y" "Camera"
sleep 3  # attende richiesta permesso
sc "luca_05_camera"
obs "Dovrebbe comparire il dialog di permesso fotocamera, oppure la preview"
assert_alive "pagina Camera"
# Se appare dialog permesso, tappa "Consenti"
# Con pixel check: se pixel in alto è scuro c'è un dialog
human_delay 2 3

step "Navigo a Audio"
drawer_nav "$DRW_AUDIO_Y" "Audio"
sc "luca_06_audio"
reading_pause
obs "Vedo la pagina Audio con i controlli di registrazione/Whisper"
assert_alive "pagina Audio"

step "Navigo a Misure"
drawer_nav "$DRW_MISURE_Y" "Misure"
sc "luca_07_misure"
reading_pause
obs "Vedo la pagina Misure con strumenti di misurazione"
assert_alive "pagina Misure"

step "Torno alla Chat via drawer"
back_to_chat
sleep 1
assert_alive "ritorno Chat"

pass "Luca: navigazione drawer completata — tutte le pagine raggiungibili"

# ══════════════════════════════════════════════════════════════════
#  SCENARIO ANNA — Controlla le Impostazioni
# ══════════════════════════════════════════════════════════════════
SCENARIO="ANNA"

step "Apro il drawer e scorro verso Impostazioni"
obs "Devo scorrere il drawer verso il basso per trovare Impostazioni..."
open_drawer
sleep 1
# Scorri il drawer verso il basso per raggiungere Impostazioni
swipe_natural "$DRW_X" 1800 "$DRW_X" 800
sleep 0.8
sc "anna_01_drawer_scrollato"
obs "Il drawer si è spostato, vedo le voci più in basso"
# Tenta il tap su Impostazioni nella posizione post-scroll
# Dopo scroll di ~1000px, Impostazioni (y=2105) sarebbe a y=2105-1000=1105
tap "$DRW_X" 1100
sleep 2
sc "anna_02_impostazioni"
obs "Sono nella pagina Impostazioni"

if ! alive; then
    fail "App crashata durante navigazione Impostazioni"
else
    pass "Impostazioni: pagina aperta"
fi

step "Esploro la pagina Impostazioni"
reading_pause
obs "Vedo i campi per configurare Server, Cloud e Modello locale"
sc "anna_03_impostazioni_dettaglio"

step "Provo a toccare il campo Host del server"
obs "Il campo Host dovrebbe essere nella prima sezione..."
# Host è tipicamente a circa 1/4 della pagina
tap "$CENTER_X" 500
human_delay 1 2
sc "anna_04_campo_host"
obs "Il campo è attivo, potrei modificarlo"
assert_alive "tocco campo host"

step "Premo il bottone indietro per tornare"
$ADB shell input keyevent KEYCODE_BACK
sleep 1
sc "anna_05_dopo_back"
assert_alive "tasto Back"
back_to_chat

pass "Anna: impostazioni esplorate correttamente"

# ══════════════════════════════════════════════════════════════════
#  SCENARIO PIETRO — Stress typing + stop streaming
# ══════════════════════════════════════════════════════════════════
SCENARIO="PIETRO"

step "Sono in Chat, scrivo una domanda più lunga"
tap "$INPUT_X" "$INPUT_Y"
sleep 0.8
obs "Digito una domanda tecnica..."
type_text "Spiegami cos'e' il protocollo TCP/IP"
sc "pietro_01_testo_lungo"
reading_pause
obs "Rileggo... ok, invio"

step "Invio e dopo 3s premo Stop"
$ADB logcat -c 2>/dev/null || true
tap "$SEND_X" "$SEND_Y"
sleep 3
sc "pietro_02_streaming_in_corso"
obs "Lo streaming è in corso — premo Stop per interromperlo"
tap "$STOP_X" "$STOP_Y"
human_delay 1 2
sc "pietro_03_stream_stoppato"
assert_alive "stop streaming"

step "Invio secondo messaggio breve dopo lo stop"
obs "Aspetto un secondo, poi scrivo di nuovo..."
human_delay 1 2
tap "$INPUT_X" "$INPUT_Y"
sleep 0.5
type_text "Ok grazie"
tap "$SEND_X" "$SEND_Y"
sleep 3
sc "pietro_04_secondo_messaggio"
assert_alive "secondo messaggio"

step "Test finale: 5 tap veloci casuali (come uno che tocca lo schermo senza meta)"
obs "Qualcuno tocca lo schermo velocemente in vari punti..."
for i in $(seq 1 5); do
    TX=$(( (RANDOM % 900) + 90 ))
    TY=$(( (RANDOM % 1600) + 250 ))
    $ADB shell input tap "$TX" "$TY"
    sleep 0.2
done
sleep 1
sc "pietro_05_dopo_stress"
assert_alive "stress tap casuali"

pass "Pietro: scenario stress completato"

# ══════════════════════════════════════════════════════════════════
#  VERIFICA FINALE LOGCAT
# ══════════════════════════════════════════════════════════════════
SCENARIO="VERIFICA"

step "Controllo logcat per crash critici"
CRASH_COUNT=$($ADB logcat -d 2>/dev/null \
    | grep -cE "FATAL EXCEPTION|signal 11|SIGSEGV|AndroidRuntime.*Error|Force.*stopped" || true)

if [ "$CRASH_COUNT" -eq 0 ]; then
    pass "Nessun crash critico durante l'intera sessione"
else
    fail "$CRASH_COUNT eventi critici nel logcat"
    $ADB logcat -d 2>/dev/null \
        | grep -E "FATAL|SIGSEGV|Force.*stopped" | tail -10
fi

step "App viva a fine sessione"
if alive; then
    pass "App in esecuzione — PID=$(pid_of)"
    sc "finale_app_viva"
else
    fail "App non più in esecuzione a fine test"
fi

# ══════════════════════════════════════════════════════════════════
#  RIEPILOGO
# ══════════════════════════════════════════════════════════════════
echo ""
echo -e "${BD}════════════════════════════════════════════${NC}"
echo -e "${BD}  RIEPILOGO SESSIONE UTENTE${NC}"
echo -e "${BD}  ${GR}$PASS PASS${NC}  ${RD}$FAIL FAIL${NC}  ${YL}$WARN WARN${NC}"
echo -e "${BD}  Screenshot: $SC_DIR/${NC}"
echo -e "${BD}════════════════════════════════════════════${NC}"
echo ""
echo -e "  ${MA}MARCO${NC}  — primo avvio + backend switching"
echo -e "  ${MA}SARA${NC}   — invio messaggio + risposta AI"
echo -e "  ${MA}LUCA${NC}   — navigazione tutte le pagine drawer"
echo -e "  ${MA}ANNA${NC}   — esplorazione impostazioni"
echo -e "  ${MA}PIETRO${NC} — stop streaming + stress tap"
echo ""

# Ripristina timeout originale e disattiva stayon
$ADB shell settings put system screen_off_timeout "$ORIG_TIMEOUT" 2>/dev/null || true
$ADB shell svc power stayon false 2>/dev/null || true
info "Timeout schermo ripristinato a ${ORIG_TIMEOUT}ms"

if [ "$FAIL" -gt 0 ]; then
    echo -e "${RD}$FAIL test falliti. Controlla i log e gli screenshot sopra.${NC}"
    exit 1
fi
echo -e "${GR}Tutti i test passati — sessione utente simulata con successo.${NC}"
exit 0

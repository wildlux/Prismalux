#!/usr/bin/env bash
# ════════════════════════════════════════════════════════════════
# build_apk.sh — Compila PrismaluxMobile APK per Android ARM64
#
# Esegui dalla root del progetto:
#   cd ANDROID && bash build_apk.sh
#
# Prerequisiti:
#   - Java 17+: sudo apt install openjdk-17-jdk
#   - Python 3: sudo apt install python3 python3-pip
#   - unzip, wget: sudo apt install unzip wget
#   - aqtinstall: pip3 install --user aqtinstall
#   - Qt 6.7.3 desktop: python3 -m aqt install-qt linux desktop 6.7.3 linux_gcc_64 --outputdir ~/Qt
#   - Qt 6.7.3 Android: python3 -m aqt install-qt linux android 6.7.3 android_arm64_v8a --outputdir ~/Qt
#   - Android NDK r25c via sdkmanager (vedi Step 3)
# ════════════════════════════════════════════════════════════════
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$SCRIPT_DIR/android_app"

# ── Configurazione ────────────────────────────────────────────
QT_VERSION="6.7.3"
QT_DIR="$HOME/Qt"
QT_HOST_DIR="$QT_DIR/$QT_VERSION/gcc_64"
QT_ANDROID_DIR="$QT_DIR/$QT_VERSION/android_arm64_v8a"
ANDROID_SDK="$HOME/Android/Sdk"
ANDROID_NDK_VER="26.1.10909125"  # r26b — minimo richiesto da Qt 6.7 (std::pmr)
ANDROID_API="34"
ANDROID_ABI="arm64-v8a"
APK_OUTPUT="$SCRIPT_DIR/PrismaluxMobile.apk"
ANDROID_OPENSSL_DIR="$HOME/Android/android_openssl-master/ssl_3/$ANDROID_ABI"

# ── Colori ────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

log()  { echo -e "${CYAN}[build_apk]${NC} $*"; }
ok()   { echo -e "${GREEN}[✓]${NC} $*"; }
warn() { echo -e "${YELLOW}[!]${NC} $*"; }
die()  { echo -e "${RED}[✗] $*${NC}"; exit 1; }
step() { echo -e "\n${BOLD}══════════════════════════════════${NC}"; \
         echo -e "${BOLD}  $*${NC}"; \
         echo -e "${BOLD}══════════════════════════════════${NC}"; }

# ════════════════════════════════════════════════════════════════
# 0 — Prerequisiti
# ════════════════════════════════════════════════════════════════
step "0 — Verifica prerequisiti"

command -v java  >/dev/null 2>&1 || die "Java non trovato. Installa: sudo apt install openjdk-17-jdk"
ok "Java: $(java -version 2>&1 | head -1)"

command -v python3 >/dev/null 2>&1 || die "Python3 non trovato"
ok "Python: $(python3 --version)"

command -v unzip >/dev/null 2>&1 || die "unzip non trovato"

# ════════════════════════════════════════════════════════════════
# 0b — OpenSSL precompilato per Android (KDAB)
# ════════════════════════════════════════════════════════════════
step "0b — OpenSSL ARM64 per Android (KDAB)"

if [ -f "$ANDROID_OPENSSL_DIR/libssl_3.so" ]; then
    ok "OpenSSL già presente in $ANDROID_OPENSSL_DIR"
else
    log "Download android_openssl KDAB (~60 MB)..."
    mkdir -p "$HOME/Android"
    TMP_SSL="/tmp/android_openssl.zip"
    wget -q --show-progress \
        "https://github.com/KDAB/android_openssl/archive/refs/heads/master.zip" \
        -O "$TMP_SSL"
    unzip -q "$TMP_SSL" -d "$HOME/Android"
    rm "$TMP_SSL"
    ok "OpenSSL estratto in $HOME/Android/android_openssl-master"
fi
[ -f "$ANDROID_OPENSSL_DIR/libssl_3.so" ] || die "OpenSSL ARM64 non trovato: $ANDROID_OPENSSL_DIR"

# ════════════════════════════════════════════════════════════════
# 1 — Qt 6.7.3 Desktop (host tools)
# ════════════════════════════════════════════════════════════════
step "1 — Qt $QT_VERSION Desktop (host)"

if [ -d "$QT_HOST_DIR/bin" ]; then
    ok "Qt $QT_VERSION desktop già installato in $QT_HOST_DIR"
else
    log "Installazione Qt $QT_VERSION desktop (~800 MB)..."
    python3 -m aqt install-qt linux desktop "$QT_VERSION" linux_gcc_64 \
        --outputdir "$QT_DIR" 2>&1 | tail -5
    ok "Qt desktop installato"
fi

# ════════════════════════════════════════════════════════════════
# 2 — Qt 6.7.3 Android ARM64
# ════════════════════════════════════════════════════════════════
step "2 — Qt $QT_VERSION Android ARM64"

if [ -d "$QT_ANDROID_DIR/lib/cmake/Qt6" ]; then
    ok "Qt $QT_VERSION Android già installato"
else
    log "Installazione Qt $QT_VERSION Android ARM64 (~300 MB)..."
    python3 -m aqt install-qt linux android "$QT_VERSION" android_arm64_v8a \
        --outputdir "$QT_DIR" 2>&1 | tail -5
    ok "Qt Android installato"
fi

# ════════════════════════════════════════════════════════════════
# 3 — Android SDK + NDK
# ════════════════════════════════════════════════════════════════
step "3 — Android SDK + NDK"

CMDTOOLS_DIR="$ANDROID_SDK/cmdline-tools/latest"
if [ ! -f "$CMDTOOLS_DIR/bin/sdkmanager" ]; then
    log "Download Android cmdline-tools..."
    mkdir -p "$ANDROID_SDK/cmdline-tools"
    CMDTOOLS_URL="https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip"
    TMP_ZIP="/tmp/cmdtools_android.zip"
    wget -q --show-progress "$CMDTOOLS_URL" -O "$TMP_ZIP"
    unzip -q "$TMP_ZIP" -d "$ANDROID_SDK/cmdline-tools"
    mv "$ANDROID_SDK/cmdline-tools/cmdline-tools" "$CMDTOOLS_DIR"
    rm "$TMP_ZIP"
fi
ok "cmdline-tools presenti"

SDKMANAGER="$CMDTOOLS_DIR/bin/sdkmanager"
yes | "$SDKMANAGER" --sdk_root="$ANDROID_SDK" --licenses >/dev/null 2>&1 || true

NDK_DIR="$ANDROID_SDK/ndk/$ANDROID_NDK_VER"
if [ ! -d "$NDK_DIR" ]; then
    log "Installazione NDK r25c (~500 MB)..."
    "$SDKMANAGER" --sdk_root="$ANDROID_SDK" \
        "ndk;$ANDROID_NDK_VER" \
        "build-tools;34.0.0" \
        "platforms;android-$ANDROID_API" \
        "platform-tools" 2>&1 | grep -v "^\[=" | tail -5
    ok "NDK installato"
else
    ok "NDK $ANDROID_NDK_VER già presente"
fi

TOOLCHAIN="$NDK_DIR/build/cmake/android.toolchain.cmake"
[ -f "$TOOLCHAIN" ] || die "Toolchain NDK non trovata: $TOOLCHAIN"

# ════════════════════════════════════════════════════════════════
# 4 — Configura CMake
# ════════════════════════════════════════════════════════════════
step "4 — Configurazione CMake (Android ARM64)"

BUILD_DIR="$APP_DIR/build-android"
mkdir -p "$BUILD_DIR"

cmake -S "$APP_DIR" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI="$ANDROID_ABI" \
    -DANDROID_PLATFORM="android-26" \
    -DANDROID_NDK="$NDK_DIR" \
    -DQt6_DIR="$QT_ANDROID_DIR/lib/cmake/Qt6" \
    -DQT_HOST_PATH="$QT_HOST_DIR" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_FIND_ROOT_PATH="$QT_ANDROID_DIR" \
    -DCMAKE_MAKE_PROGRAM="$(which ninja || which make)" \
    -DANDROID_OPENSSL_DIR="$ANDROID_OPENSSL_DIR" \
    2>&1 | grep -E "(Status|Error|Found|missing|OpenSSL)" | grep -v "Deprecation" | tail -10

ok "CMake configurato"

# ════════════════════════════════════════════════════════════════
# 5 — Compilazione + APK
# ════════════════════════════════════════════════════════════════
step "5 — Compilazione + packaging APK"

_RAW_NPROC="$(nproc 2>/dev/null || echo 2)"
_JOBS=$(( _RAW_NPROC > 1 ? _RAW_NPROC - 1 : 1 ))
log "Core disponibili: ${_RAW_NPROC}  →  job paralleli: ${_JOBS} (core-1)"
cmake --build "$BUILD_DIR" -j"${_JOBS}" 2>&1 | \
    grep -E "(error:|Error|BUILD SUCCESSFUL|\[)" | tail -20

ok "Build completata"

# ════════════════════════════════════════════════════════════════
# 6 — Firma APK con debug keystore
# ════════════════════════════════════════════════════════════════
step "6 — Firma APK (debug key)"

APKSIGNER="$ANDROID_SDK/build-tools/34.0.0/apksigner"
KEYSTORE="$HOME/.android/debug.keystore"

# Crea keystore debug se non esiste
if [ ! -f "$KEYSTORE" ]; then
    mkdir -p "$HOME/.android"
    keytool -genkeypair -v \
        -keystore "$KEYSTORE" \
        -alias androiddebugkey \
        -keyalg RSA -keysize 2048 -validity 10000 \
        -storepass android -keypass android \
        -dname "CN=Android Debug, O=Android, C=US" \
        >/dev/null 2>&1
    ok "Debug keystore creato"
fi

UNSIGNED_APK=$(find "$BUILD_DIR" -name "*unsigned*.apk" -o -name "*.apk" 2>/dev/null | grep -v "$APK_OUTPUT" | head -1)
[ -n "$UNSIGNED_APK" ] || die "APK non trovato in $BUILD_DIR"

"$APKSIGNER" sign \
    --ks "$KEYSTORE" \
    --ks-pass pass:android \
    --key-pass pass:android \
    --ks-key-alias androiddebugkey \
    --out "$APK_OUTPUT" \
    "$UNSIGNED_APK" 2>&1

ok "APK firmato"

echo ""
echo -e "${GREEN}${BOLD}════════════════════════════════════════${NC}"
echo -e "${GREEN}${BOLD}  ✅ PrismaluxMobile.apk pronto!${NC}"
echo -e "${GREEN}${BOLD}  📦 $(du -h "$APK_OUTPUT" | cut -f1) — $APK_OUTPUT${NC}"
echo -e "${GREEN}${BOLD}════════════════════════════════════════${NC}"
echo ""
echo "Installa sul telefono (USB debugging abilitato):"
echo "  adb install -r $APK_OUTPUT"
echo ""
echo "Su MIUI (Redmi/Xiaomi): Impostazioni → Privacy → Installa app sconosciute"

#!/bin/bash
# ══════════════════════════════════════════════════════════════
#  aggiorna.sh — Ricompila Prismalux, ZIP Windows, AppImage Linux
#
#  Uso:
#    ./aggiorna.sh              # TUI + GUI + ZIP Windows + AppImage
#    ./aggiorna.sh --tui        # solo TUI C
#    ./aggiorna.sh --gui        # solo GUI Qt6
#    ./aggiorna.sh --zip        # solo ZIP Windows
#    ./aggiorna.sh --appimage   # solo AppImage Linux
#    ./aggiorna.sh --no-zip     # TUI + GUI + AppImage, salta ZIP
#    ./aggiorna.sh --no-appimage# TUI + GUI + ZIP, salta AppImage
#    ./aggiorna.sh --no-whisper # salta download binario whisper-cli.exe
#    ./aggiorna.sh --whisper    # solo download binario whisper-cli.exe
# ══════════════════════════════════════════════════════════════
set -euo pipefail

# ── Colori ─────────────────────────────────────────────────────
R='\033[0;31m'; G='\033[0;32m'; Y='\033[0;33m'
C='\033[0;36m'; B='\033[1;37m'; N='\033[0m'

# ── Percorsi ───────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
C_SW="$SCRIPT_DIR/C_software"
QT_GUI="$C_SW/Qt_GUI"
QT_BUILD="$QT_GUI/build"
ZIP_SCRIPT="$SCRIPT_DIR/crea_zip_windows.py"
ZIP_OUT="$SCRIPT_DIR/Prismalux_Windows_full.zip"
APPIMAGE_SCRIPT="$SCRIPT_DIR/crea_appimage.sh"
APPIMAGE_OUT="$SCRIPT_DIR/Prismalux-x86_64.AppImage"
WHISPER_WIN_DIR="$C_SW/whisper_win"   # destinazione binario Windows

# ── Flags ──────────────────────────────────────────────────────
DO_TUI=1
DO_GUI=1
DO_ZIP=1
DO_APPIMAGE=1
DO_WHISPER_WIN=1   # scarica whisper-cli.exe precompilato per Windows

for arg in "$@"; do
    case "$arg" in
        --tui)          DO_TUI=1; DO_GUI=0; DO_ZIP=0; DO_APPIMAGE=0; DO_WHISPER_WIN=0 ;;
        --gui)          DO_TUI=0; DO_GUI=1; DO_ZIP=0; DO_APPIMAGE=0; DO_WHISPER_WIN=0 ;;
        --zip)          DO_TUI=0; DO_GUI=0; DO_ZIP=1; DO_APPIMAGE=0 ;;
        --appimage)     DO_TUI=0; DO_GUI=0; DO_ZIP=0; DO_APPIMAGE=1 ;;
        --whisper)      DO_TUI=0; DO_GUI=0; DO_ZIP=0; DO_APPIMAGE=0; DO_WHISPER_WIN=1 ;;
        --no-zip)       DO_ZIP=0 ;;
        --no-appimage)  DO_APPIMAGE=0 ;;
        --no-whisper)   DO_WHISPER_WIN=0 ;;
        -h|--help)
            echo "Uso: $0 [--tui|--gui|--zip|--appimage|--no-zip|--no-appimage|--whisper|--no-whisper]"
            exit 0 ;;
        *) echo -e "${R}Opzione sconosciuta: $arg${N}"; exit 1 ;;
    esac
done

# ── Helper ─────────────────────────────────────────────────────
step() { echo -e "\n${C}▶ $*${N}"; }
ok()   { echo -e "${G}✅ $*${N}"; }
fail() { echo -e "${R}❌ $*${N}"; exit 1; }

T_START=$(date +%s)

# ══════════════════════════════════════════════════════════════
#  1. TUI C (modalità HTTP — solo gcc, compilazione veloce)
# ══════════════════════════════════════════════════════════════
if [ "$DO_TUI" = "1" ]; then
    step "Compilo TUI C (make http)..."
    cd "$C_SW"
    make http 2>&1 | grep -E "(gcc|error:|warning:|prismalux compilato)" || true
    [ -f "$C_SW/prismalux" ] || fail "Binario TUI non trovato dopo la build"
    ok "TUI compilata → $C_SW/prismalux"
fi

# ══════════════════════════════════════════════════════════════
#  2. GUI Qt6
# ══════════════════════════════════════════════════════════════
if [ "$DO_GUI" = "1" ]; then
    step "Compilo GUI Qt6..."
    cd "$QT_GUI"

    # Se la cartella build non è ancora configurata, esegui cmake
    if [ ! -f "$QT_BUILD/CMakeCache.txt" ]; then
        step "  Prima configurazione cmake..."
        cmake -B build -DCMAKE_BUILD_TYPE=Release \
            2>&1 | grep -E "(CMAKE|error:|Configuring)" || true
    fi

    cmake --build build -j"$(nproc)" 2>&1 \
        | grep -E "(Building|Linking|error:|warning:|Built target)" || true

    [ -f "$QT_BUILD/Prismalux_GUI" ] || fail "Binario GUI non trovato dopo la build"
    ok "GUI compilata → $QT_BUILD/Prismalux_GUI"
fi

# ══════════════════════════════════════════════════════════════
#  3. whisper-cli.exe precompilato per Windows (GitHub Releases)
# ══════════════════════════════════════════════════════════════
if [ "$DO_WHISPER_WIN" = "1" ]; then
    step "Scarico whisper-cli.exe precompilato per Windows..."

    # Verifica strumenti necessari
    if ! command -v curl &>/dev/null; then
        echo -e "${Y}⚠  curl non trovato — salto download whisper Windows.${N}"
        DO_WHISPER_WIN=0
    else
        mkdir -p "$WHISPER_WIN_DIR"
        WHISPER_EXE="$WHISPER_WIN_DIR/whisper-cli.exe"

        # Cerca l'URL del release più recente (x86_64-windows.zip)
        WHISPER_API="https://api.github.com/repos/ggml-org/whisper.cpp/releases/latest"
        echo -e "  ${C}Interrogo GitHub API...${N}"

        WHISPER_ZIP_URL=$(curl -fsSL "$WHISPER_API" 2>/dev/null \
            | grep '"browser_download_url"' \
            | grep 'x86_64-windows' \
            | grep -o 'https://[^"]*' | head -1 || true)

        if [ -z "$WHISPER_ZIP_URL" ]; then
            echo -e "${Y}⚠  Impossibile trovare il release Windows su GitHub API.${N}"
            echo -e "${Y}   (forse limite rate o connessione assente — il ZIP procede senza)${N}"
            DO_WHISPER_WIN=0
        else
            echo -e "  Release trovato: ${C}$(basename "$WHISPER_ZIP_URL")${N}"
            TMP_ZIP="$(mktemp /tmp/whisper_win_XXXXXX.zip)"

            # Scarica lo zip del release
            curl -fsSL -o "$TMP_ZIP" "$WHISPER_ZIP_URL" || {
                echo -e "${Y}⚠  Download fallito — il ZIP procede senza whisper-cli.exe${N}"
                rm -f "$TMP_ZIP"; DO_WHISPER_WIN=0
            }

            if [ "$DO_WHISPER_WIN" = "1" ]; then
                # Estrai solo whisper-cli.exe (può essere in root o in una sottocartella)
                if command -v unzip &>/dev/null; then
                    FOUND_EXE=$(unzip -l "$TMP_ZIP" 2>/dev/null \
                        | grep -o '[^ ]*whisper-cli\.exe' | head -1 || true)
                    if [ -n "$FOUND_EXE" ]; then
                        unzip -p "$TMP_ZIP" "$FOUND_EXE" > "$WHISPER_EXE" 2>/dev/null
                    else
                        # Estrai tutto e cerca whisper-cli.exe
                        TMP_DIR="$(mktemp -d /tmp/whisper_win_XXXXXX)"
                        unzip -q "$TMP_ZIP" -d "$TMP_DIR" 2>/dev/null || true
                        FOUND=$(find "$TMP_DIR" -name "whisper-cli.exe" | head -1)
                        [ -n "$FOUND" ] && cp "$FOUND" "$WHISPER_EXE"
                        rm -rf "$TMP_DIR"
                    fi
                else
                    # Fallback: python3 per estrarre
                    python3 - "$TMP_ZIP" "$WHISPER_EXE" <<'PYEOF'
import sys, zipfile
zp, out = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(zp) as zf:
    names = [n for n in zf.namelist() if n.endswith('whisper-cli.exe')]
    if names:
        with open(out, 'wb') as f: f.write(zf.read(names[0]))
PYEOF
                fi
                rm -f "$TMP_ZIP"

                if [ -f "$WHISPER_EXE" ] && [ -s "$WHISPER_EXE" ]; then
                    SZ=$(du -sh "$WHISPER_EXE" | cut -f1)
                    ok "whisper-cli.exe → $WHISPER_EXE  ($SZ)"
                else
                    echo -e "${Y}⚠  whisper-cli.exe non trovato nell'archivio — continuo senza.${N}"
                    rm -f "$WHISPER_EXE"
                fi
            fi
        fi
    fi
fi

# ══════════════════════════════════════════════════════════════
#  4. ZIP Windows (sorgenti + script — no binari Linux)
# ══════════════════════════════════════════════════════════════
if [ "$DO_ZIP" = "1" ]; then
    step "Rigenero ZIP Windows..."
    cd "$SCRIPT_DIR"
    [ -f "$ZIP_SCRIPT" ] || fail "Script ZIP non trovato: $ZIP_SCRIPT"

    python3 "$ZIP_SCRIPT" --out "$ZIP_OUT" 2>&1 | grep -E "(Fatto|file|Creazione|errore)" || true

    [ -f "$ZIP_OUT" ] || fail "ZIP non generato"
    SIZE=$(du -sh "$ZIP_OUT" | cut -f1)
    ok "ZIP aggiornato → $ZIP_OUT  ($SIZE)"
fi

# ══════════════════════════════════════════════════════════════
#  5. AppImage Linux
# ══════════════════════════════════════════════════════════════
if [ "$DO_APPIMAGE" = "1" ]; then
    step "Genero AppImage Linux..."
    cd "$SCRIPT_DIR"
    [ -f "$APPIMAGE_SCRIPT" ] || fail "Script AppImage non trovato: $APPIMAGE_SCRIPT"
    [ -x "$APPIMAGE_SCRIPT" ] || chmod +x "$APPIMAGE_SCRIPT"

    # Passa --no-build perché la GUI è già stata compilata al passo 2
    "$APPIMAGE_SCRIPT" --no-build 2>&1 \
        | grep -E "(✅|❌|AppImage|Success|Genero)" || true

    [ -f "$APPIMAGE_OUT" ] || fail "AppImage non generata: $APPIMAGE_OUT"
    SIZE=$(du -sh "$APPIMAGE_OUT" | cut -f1)
    ok "AppImage aggiornata → $APPIMAGE_OUT  ($SIZE)"
fi

# ══════════════════════════════════════════════════════════════
#  Riepilogo
# ══════════════════════════════════════════════════════════════
T_END=$(date +%s)
ELAPSED=$(( T_END - T_START ))

echo ""
echo -e "${B}════════════════════════════════════════${N}"
echo -e "${B}  Prismalux — aggiornamento completato  ${N}"
echo -e "${B}════════════════════════════════════════${N}"
[ "$DO_TUI"      = "1" ] && echo -e "  ${G}TUI${N}      $C_SW/prismalux"
[ "$DO_GUI"      = "1" ] && echo -e "  ${G}GUI${N}      $QT_BUILD/Prismalux_GUI"
[ "$DO_ZIP"      = "1" ] && [ -f "$ZIP_OUT" ]      && echo -e "  ${G}ZIP${N}      $ZIP_OUT"
[ "$DO_APPIMAGE" = "1" ] && [ -f "$APPIMAGE_OUT" ] && echo -e "  ${G}AppImage${N} $APPIMAGE_OUT"
if [ "$DO_WHISPER_WIN" = "1" ] && [ -f "$WHISPER_WIN_DIR/whisper-cli.exe" ]; then
    echo -e "  ${G}Whisper${N}  $WHISPER_WIN_DIR/whisper-cli.exe"
fi
echo -e "  ${Y}Tempo: ${ELAPSED}s${N}"
echo ""

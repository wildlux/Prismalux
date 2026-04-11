#!/bin/bash
# ══════════════════════════════════════════════════════════════
#  aggiorna.sh — Ricompila Prismalux GUI Qt6, ZIP Windows, AppImage Linux
#
#  Uso:
#    ./aggiorna.sh              # PyDeps + GUI + ZIP Windows + AppImage
#    ./aggiorna.sh --gui        # solo GUI Qt6
#    ./aggiorna.sh --zip        # solo ZIP Windows
#    ./aggiorna.sh --appimage   # solo AppImage Linux
#    ./aggiorna.sh --no-zip     # GUI + AppImage, salta ZIP
#    ./aggiorna.sh --no-appimage# GUI + ZIP, salta AppImage
#    ./aggiorna.sh --no-whisper # salta download binario whisper-cli.exe
#    ./aggiorna.sh --whisper    # solo download binario whisper-cli.exe
#    ./aggiorna.sh --no-pycheck # salta verifica/aggiornamento requirements Python
# ══════════════════════════════════════════════════════════════
set -euo pipefail

# ── Colori ─────────────────────────────────────────────────────
R='\033[0;31m'; G='\033[0;32m'; Y='\033[0;33m'
C='\033[0;36m'; B='\033[1;37m'; N='\033[0m'

# ── Percorsi ───────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
C_SW="$SCRIPT_DIR/C_software"
QT_GUI="$SCRIPT_DIR/Qt_GUI_v3"
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
DO_WHISPER_WIN=0    # stub non attivo — usa --whisper per forzare il download
DO_BUILD_WHISPER=0  # compila whisper.cpp da sorgente (Linux)
DO_LLAMA_STUDIO=0   # compila llama-server + llama-cli per la GUI
DO_PYCHECK=1        # controlla e aggiorna requirements_python.txt Windows

for arg in "$@"; do
    case "$arg" in
        --tui)           DO_TUI=1; DO_GUI=0; DO_ZIP=0; DO_APPIMAGE=0; DO_WHISPER_WIN=0; DO_PYCHECK=0; DO_BUILD_WHISPER=0; DO_LLAMA_STUDIO=0 ;;
        --gui)           DO_TUI=0; DO_GUI=1; DO_ZIP=0; DO_APPIMAGE=0; DO_WHISPER_WIN=0; DO_PYCHECK=0 ;;
        --zip)           DO_TUI=0; DO_GUI=0; DO_ZIP=1; DO_APPIMAGE=0 ;;
        --appimage)      DO_TUI=0; DO_GUI=0; DO_ZIP=0; DO_APPIMAGE=1 ;;
        --whisper)       DO_TUI=0; DO_GUI=0; DO_ZIP=0; DO_APPIMAGE=0; DO_WHISPER_WIN=1 ;;
        --build-whisper) DO_BUILD_WHISPER=1 ;;
        --llama-studio)  DO_LLAMA_STUDIO=1 ;;
        --no-zip)        DO_ZIP=0 ;;
        --no-appimage)   DO_APPIMAGE=0 ;;
        --no-whisper)    DO_WHISPER_WIN=0 ;;
        --no-pycheck)    DO_PYCHECK=0 ;;
        -h|--help)
            echo "Uso: $0 [--tui|--gui|--zip|--appimage|--no-zip|--no-appimage"
            echo "         --whisper|--build-whisper|--llama-studio|--no-whisper|--no-pycheck]"
            echo ""
            echo "  --build-whisper  Compila whisper.cpp da sorgente (→ C_software/whisper.cpp/)"
            echo "  --llama-studio   Compila llama-server + llama-cli (→ C_software/llama_cpp_studio/llama.cpp/)"
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
#  0. Dipendenze Python Windows (requirements_python.txt + bat)
#     Genera anche installa_pip.bat come helper standalone
# ══════════════════════════════════════════════════════════════
if [ "$DO_PYCHECK" = "1" ]; then
    step "Verifico dipendenze Python Windows..."

    REQS="$C_SW/requirements_python.txt"
    BAT_HELPER="$C_SW/installa_pip.bat"

    # Pacchetti attualmente richiesti (sorgente di verità)
    REQUIRED_PKGS=(
        "sympy" "mpmath"
        "numpy" "pandas" "scipy" "matplotlib"
        "psutil"
        "pypdf" "xlrd" "openpyxl"
        "requests"
    )

    # Controlla che tutti i pacchetti siano nel requirements_python.txt
    MISSING_IN_REQS=()
    for pkg in "${REQUIRED_PKGS[@]}"; do
        if ! grep -qi "^${pkg}" "$REQS" 2>/dev/null; then
            MISSING_IN_REQS+=("$pkg")
        fi
    done

    if [ ${#MISSING_IN_REQS[@]} -gt 0 ]; then
        echo -e "${Y}  ⚠  Pacchetti mancanti in requirements_python.txt: ${MISSING_IN_REQS[*]}${N}"
        for pkg in "${MISSING_IN_REQS[@]}"; do
            echo "$pkg" >> "$REQS"
        done
        echo -e "${G}  → Aggiunti a $REQS${N}"
    else
        echo -e "${G}  ✓ requirements_python.txt completo (${#REQUIRED_PKGS[@]} pacchetti)${N}"
    fi

    # Genera installa_pip.bat — script standalone che l'utente può eseguire
    # se pip fallisce durante il normale avvio
    cat > "$BAT_HELPER" << 'BATEOF'
@echo off
:: installa_pip.bat — Installa/ripara le dipendenze Python per Prismalux
:: Eseguire come AMMINISTRATORE se pip fallisce con errori di permessi.
setlocal EnableDelayedExpansion

echo.
echo +--------------------------------------------------+
echo ^|  Prismalux — Installazione dipendenze Python     ^|
echo +--------------------------------------------------+
echo.

set BASE=%~dp0
set REQS=%BASE%requirements_python.txt
set BUILD=%BASE%Qt_GUI\build_win
set VENV=%BUILD%\py_env
set VENV_PYTHON=%VENV%\Scripts\python.exe

:: ── Trova Python ─────────────────────────────────────────────
set PYTHON=
where python  >nul 2>&1 && set PYTHON=python
if not defined PYTHON ( where py >nul 2>&1 && set PYTHON=py )
if not defined PYTHON ( where python3 >nul 2>&1 && set PYTHON=python3 )

if not defined PYTHON (
    echo [ERRORE] Python non trovato nel PATH.
    echo Scarica da: https://www.python.org/downloads/
    echo (spunta "Add Python to PATH" durante l'installazione^)
    pause & exit /b 1
)
echo [OK] Python trovato: %PYTHON%
echo.

:: ── Ricrea venv se corrotto ───────────────────────────────────
if exist "%VENV_PYTHON%" (
    "%VENV_PYTHON%" -c "import sys; print('Python', sys.version)" >nul 2>&1
    if errorlevel 1 (
        echo [INFO] Venv sembra corrotto - lo ricreo...
        rmdir /s /q "%VENV%" >nul 2>&1
    )
)

if not exist "%VENV_PYTHON%" (
    if not exist "%BUILD%" mkdir "%BUILD%"
    echo [INFO] Creo ambiente virtuale...
    %PYTHON% -m venv "%VENV%"
    if not exist "%VENV_PYTHON%" (
        echo [AVVISO] Venv non creato - uso Python di sistema.
        set VENV_PYTHON=%PYTHON%
    )
)

:: ── Aggiorna pip ─────────────────────────────────────────────
echo [INFO] Aggiorno pip...
"%VENV_PYTHON%" -m pip install --upgrade pip ^
    --trusted-host pypi.org --trusted-host files.pythonhosted.org
echo.

:: ── Installa dipendenze ──────────────────────────────────────
echo [INFO] Installo dipendenze da requirements_python.txt...
echo.
"%VENV_PYTHON%" -m pip install -r "%REQS%" ^
    --trusted-host pypi.org --trusted-host files.pythonhosted.org
set RC=%errorlevel%

if %RC% neq 0 (
    echo.
    echo [INFO] Tentativo con --only-binary (evita compilazione da sorgente^)...
    "%VENV_PYTHON%" -m pip install -r "%REQS%" ^
        --only-binary :all: ^
        --trusted-host pypi.org --trusted-host files.pythonhosted.org
    set RC2=%errorlevel%
    if !RC2! neq 0 (
        echo.
        echo [INFO] Installo pacchetti uno ad uno...
        for /f "usebackq eol=# tokens=*" %%P in ("%REQS%") do (
            if not "%%P"=="" (
                echo   Installo: %%P
                "%VENV_PYTHON%" -m pip install "%%P" ^
                    --trusted-host pypi.org --trusted-host files.pythonhosted.org
            )
        )
    )
)

:: ── Verifica ─────────────────────────────────────────────────
echo.
echo [INFO] Verifica installazione...
"%VENV_PYTHON%" -c "import sympy, numpy, psutil, pandas; print('[OK] Pacchetti principali pronti.')"
if errorlevel 1 (
    echo [ERRORE] Alcuni pacchetti non sono stati installati correttamente.
    echo Controlla i messaggi sopra e riprova come Amministratore.
) else (
    echo [OK] Tutte le dipendenze Python sono installate correttamente.
    echo Puoi avviare Prismalux con Avvia_Prismalux.bat
)
echo.
pause
endlocal
BATEOF

    ok "installa_pip.bat generato → $BAT_HELPER"
fi

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

    # Rigenera Prismalux.desktop con il percorso corretto (no hardcoded)
    DESKTOP_OUT="$SCRIPT_DIR/Prismalux.desktop"
    cat > "$DESKTOP_OUT" <<DESKTOP_EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Prismalux
GenericName=Centro di Controllo AI
Comment=Pipeline Agenti, Tutor AI, Strumenti Pratici
Exec=$QT_BUILD/Prismalux_GUI
Icon=prismalux
Terminal=false
Categories=Education;Science;Utility;
Keywords=AI;agenti;matematica;tutor;ollama;
StartupNotify=true
StartupWMClass=Prismalux_GUI
DESKTOP_EOF
    chmod +x "$DESKTOP_OUT"
    ok "Prismalux.desktop aggiornato → $DESKTOP_OUT"
fi

# ══════════════════════════════════════════════════════════════
#  2b. whisper.cpp — compilazione da sorgente (Linux/macOS)
# ══════════════════════════════════════════════════════════════
if [ "$DO_BUILD_WHISPER" = "1" ]; then
    WHISPER_SRC="$C_SW/whisper.cpp"
    WHISPER_CLI="$WHISPER_SRC/build/bin/whisper-cli"

    if [ -f "$WHISPER_CLI" ]; then
        ok "whisper-cli già presente → $WHISPER_CLI"
    else
        step "Compilo whisper.cpp da sorgente..."

        if [ ! -d "$WHISPER_SRC/.git" ]; then
            echo -e "  ${C}Clone whisper.cpp...${N}"
            git clone --depth=1 https://github.com/ggml-org/whisper.cpp "$WHISPER_SRC"
        else
            echo -e "  ${C}Aggiorno sorgenti whisper.cpp...${N}"
            git -C "$WHISPER_SRC" pull --depth=1 --rebase 2>/dev/null || true
        fi

        cmake -B "$WHISPER_SRC/build" -S "$WHISPER_SRC" \
            -DCMAKE_BUILD_TYPE=Release \
            -DWHISPER_BUILD_TESTS=OFF \
            -DWHISPER_BUILD_EXAMPLES=ON \
            -DBUILD_SHARED_LIBS=OFF \
            2>&1 | grep -E "(CMAKE|error:|Configuring|fatal)" || true

        cmake --build "$WHISPER_SRC/build" --target whisper-cli -j"$(nproc)" \
            2>&1 | grep -E "(Building|Linking|error:|Built target|fatal)" || true

        [ -f "$WHISPER_CLI" ] || fail "whisper-cli non trovato dopo la build (controlla gli errori sopra)"
        ok "whisper-cli compilato → $WHISPER_CLI"
    fi
fi

# ══════════════════════════════════════════════════════════════
#  2c. llama.cpp Studio — compila llama-server + llama-cli
#      (binari usati dalla GUI in Impostazioni → AI Locale)
# ══════════════════════════════════════════════════════════════
if [ "$DO_LLAMA_STUDIO" = "1" ]; then
    LLAMA_STUDIO_DIR="$C_SW/llama_cpp_studio"
    LLAMA_SRC="$LLAMA_STUDIO_DIR/llama.cpp"
    LLAMA_SERVER="$LLAMA_SRC/build/bin/llama-server"
    LLAMA_CLI="$LLAMA_SRC/build/bin/llama-cli"

    if [ -f "$LLAMA_SERVER" ] && [ -f "$LLAMA_CLI" ]; then
        ok "llama-server + llama-cli già presenti → $LLAMA_SRC/build/bin/"
    else
        step "Compilo llama.cpp Studio (llama-server + llama-cli)..."

        # Rileva GPU (stesso logic di build.sh)
        CMAKE_GPU_FLAGS=""
        command -v nvidia-smi &>/dev/null && CMAKE_GPU_FLAGS="-DGGML_CUDA=ON"
        { [ -d /opt/rocm ] || command -v rocm-smi &>/dev/null; } && CMAKE_GPU_FLAGS="$CMAKE_GPU_FLAGS -DGGML_HIPBLAS=ON"
        command -v vulkaninfo &>/dev/null && CMAKE_GPU_FLAGS="$CMAKE_GPU_FLAGS -DGGML_VULKAN=ON"
        [ -z "$CMAKE_GPU_FLAGS" ] && CMAKE_GPU_FLAGS="-DGGML_OPENMP=ON"

        if [ ! -d "$LLAMA_SRC/.git" ]; then
            mkdir -p "$LLAMA_STUDIO_DIR"
            echo -e "  ${C}Clone llama.cpp (llama-studio)...${N}"
            git clone --depth=1 https://github.com/ggml-org/llama.cpp "$LLAMA_SRC"
        else
            echo -e "  ${C}llama.cpp già clonato, skip clone.${N}"
        fi

        cmake -B "$LLAMA_SRC/build" -S "$LLAMA_SRC" \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_SHARED_LIBS=OFF \
            -DGGML_NATIVE=ON \
            -DLLAMA_BUILD_TESTS=OFF \
            -DLLAMA_BUILD_EXAMPLES=ON \
            $CMAKE_GPU_FLAGS \
            2>&1 | grep -E "(CMAKE|error:|Configuring|fatal)" || true

        cmake --build "$LLAMA_SRC/build" \
            --target llama-server llama-cli -j"$(nproc)" \
            2>&1 | grep -E "(Building|Linking|error:|Built target|fatal)" || true

        [ -f "$LLAMA_SERVER" ] || fail "llama-server non trovato dopo la build"
        ok "llama-server → $LLAMA_SERVER"
        ok "llama-cli    → $LLAMA_CLI"
    fi
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

        # Cerca l'URL del release più recente — prova prima ggml-org, poi ggerganov
        echo -e "  ${C}Interrogo GitHub API...${N}"

        _whisper_fetch_url() {
            local repo="$1"
            curl -fsSL "https://api.github.com/repos/$repo/releases/latest" 2>/dev/null \
                | grep '"browser_download_url"' \
                | grep -iE 'win(64|dows|dows-x64|dows.*x64|_x64)|x64.*win|x86_64.*win' \
                | grep -iE '\.zip' \
                | grep -o 'https://[^"]*' | head -1 || true
        }

        WHISPER_ZIP_URL=$(_whisper_fetch_url "ggml-org/whisper.cpp")
        if [ -z "$WHISPER_ZIP_URL" ]; then
            WHISPER_ZIP_URL=$(_whisper_fetch_url "ggerganov/whisper.cpp")
        fi

        if [ -z "$WHISPER_ZIP_URL" ]; then
            echo -e "${Y}⚠  Impossibile trovare il release Windows su GitHub API.${N}"
            echo -e "${Y}   (forse limite rate o connessione assente — il ZIP procede senza)${N}"
            echo -e "${Y}   Riprova con: ./aggiorna.sh --whisper${N}"
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
[ "$DO_PYCHECK"       = "1" ] && echo -e "  ${G}PyDeps${N}        $C_SW/requirements_python.txt  +  installa_pip.bat"
[ "$DO_TUI"           = "1" ] && echo -e "  ${G}TUI${N}           $C_SW/prismalux"
[ "$DO_GUI"           = "1" ] && echo -e "  ${G}GUI${N}           $QT_BUILD/Prismalux_GUI"
[ "$DO_BUILD_WHISPER" = "1" ] && [ -f "$C_SW/whisper.cpp/build/bin/whisper-cli" ] && \
    echo -e "  ${G}whisper-cli${N}   $C_SW/whisper.cpp/build/bin/whisper-cli"
[ "$DO_LLAMA_STUDIO"  = "1" ] && [ -f "$C_SW/llama_cpp_studio/llama.cpp/build/bin/llama-server" ] && \
    echo -e "  ${G}llama-studio${N}  $C_SW/llama_cpp_studio/llama.cpp/build/bin/"
[ "$DO_ZIP"           = "1" ] && [ -f "$ZIP_OUT" ]      && echo -e "  ${G}ZIP${N}           $ZIP_OUT"
[ "$DO_APPIMAGE"      = "1" ] && [ -f "$APPIMAGE_OUT" ] && echo -e "  ${G}AppImage${N}      $APPIMAGE_OUT"
if [ "$DO_WHISPER_WIN" = "1" ] && [ -f "$WHISPER_WIN_DIR/whisper-cli.exe" ]; then
    echo -e "  ${G}Whisper WIN${N}   $WHISPER_WIN_DIR/whisper-cli.exe"
fi
echo -e "  ${Y}Tempo: ${ELAPSED}s${N}"
echo ""

#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════
#  crea_appimage.sh — Genera Prismalux-x86_64.AppImage
#
#  Uso:
#    ./crea_appimage.sh            # build + crea AppImage
#    ./crea_appimage.sh --no-build # usa binario già compilato
#
#  Requisiti:
#    - appimagetool  (già installato in ~/.local/bin/)
#    - Qt6 installato in /usr/lib/x86_64-linux-gnu/
#    - gcc, cmake, make (per --build)
#
#  Output:
#    Prismalux-x86_64.AppImage  (nella root del progetto)
# ═══════════════════════════════════════════════════════════════

set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
BUILD_FIRST=true

for arg in "$@"; do
  [[ "$arg" == "--no-build" ]] && BUILD_FIRST=false
done

# ── Colori ────────────────────────────────────────────────────
RED='\033[0;31m'; GRN='\033[0;32m'; CYN='\033[0;36m'; YLW='\033[0;33m'; NC='\033[0m'
info()  { echo -e "${CYN}▶ $*${NC}"; }
ok()    { echo -e "${GRN}✅ $*${NC}"; }
warn()  { echo -e "${YLW}⚠  $*${NC}"; }
die()   { echo -e "${RED}❌ $*${NC}"; exit 1; }

# ── Dipendenze ────────────────────────────────────────────────
APPIMAGETOOL="${HOME}/.local/bin/appimagetool"
[[ -x "$APPIMAGETOOL" ]] || APPIMAGETOOL="$(which appimagetool 2>/dev/null)" \
  || die "appimagetool non trovato. Scaricalo da https://github.com/AppImage/AppImageKit/releases"

GUI_BIN="${ROOT}/C_software/Qt_GUI/build/Prismalux_GUI"
ICON_SRC="${ROOT}/ICONA/prismalux.png"
APPDIR="${ROOT}/AppDir"
VERSION="$(date +%Y%m%d)"
OUTPUT="${ROOT}/Prismalux-x86_64.AppImage"

QT_LIBS_DIR="/usr/lib/x86_64-linux-gnu"
QT_PLUGINS_DIR="${QT_LIBS_DIR}/qt6/plugins"
QT_LIBEXEC_DIR="${QT_LIBS_DIR}/qt6/libexec"

# ── 1. Compila GUI se richiesto ────────────────────────────────
if $BUILD_FIRST; then
  info "Compilo GUI Qt6..."
  "${ROOT}/aggiorna.sh" --gui
fi

[[ -x "$GUI_BIN" ]] || die "Binario non trovato: ${GUI_BIN}\nEsegui prima: ./aggiorna.sh --gui"

# ── 2. Prepara icona PNG ───────────────────────────────────────
if [[ ! -f "$ICON_SRC" ]]; then
  warn "Icona PNG non trovata — la genero dall'ICO..."
  ICO="${ROOT}/ICONA/prismaluce.ico"
  if [[ -f "$ICO" ]] && command -v convert &>/dev/null; then
    convert "${ICO}[0]" -resize 256x256 "$ICON_SRC"
    ok "Icona creata: ${ICON_SRC}"
  else
    # Genera icona SVG minimalista come fallback
    ICON_SRC="${ROOT}/ICONA/prismalux_fallback.svg"
    cat > "$ICON_SRC" <<'SVGEOF'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <linearGradient id="g" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" style="stop-color:#00bcd4"/>
      <stop offset="100%" style="stop-color:#006064"/>
    </linearGradient>
  </defs>
  <rect width="64" height="64" rx="12" fill="url(#g)"/>
  <text x="32" y="44" font-size="36" text-anchor="middle" fill="white">🍺</text>
</svg>
SVGEOF
    warn "Icona fallback SVG usata."
  fi
fi

# ── 3. Pulisce e ricrea AppDir ─────────────────────────────────
info "Preparo AppDir..."
rm -rf "$APPDIR"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/lib"
mkdir -p "${APPDIR}/usr/plugins/platforms"
mkdir -p "${APPDIR}/usr/plugins/xcbglintegrations"
mkdir -p "${APPDIR}/usr/plugins/imageformats"
mkdir -p "${APPDIR}/usr/plugins/iconengines"
mkdir -p "${APPDIR}/usr/plugins/platformthemes"
mkdir -p "${APPDIR}/usr/plugins/styles"
mkdir -p "${APPDIR}/usr/plugins/wayland-decoration-client"
mkdir -p "${APPDIR}/usr/plugins/wayland-graphics-integration-client"
mkdir -p "${APPDIR}/usr/plugins/wayland-shell-integration"
mkdir -p "${APPDIR}/usr/plugins/tls"
mkdir -p "${APPDIR}/usr/plugins/multimedia"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

# ── 4. Copia binario + temi ───────────────────────────────────
info "Copio binario..."
cp "$GUI_BIN" "${APPDIR}/usr/bin/Prismalux_GUI"
chmod +x "${APPDIR}/usr/bin/Prismalux_GUI"

# I temi vengono letti da disco a runtime (applicationDirPath()/themes/).
# Nella AppImage applicationDirPath() = .../usr/bin  →  themes deve stare lì accanto.
THEMES_SRC="$(dirname "$GUI_BIN")/themes"
if [[ -d "$THEMES_SRC" ]]; then
  cp -r "$THEMES_SRC" "${APPDIR}/usr/bin/themes"
  ok "Temi copiati → AppDir/usr/bin/themes/ ($(ls "${APPDIR}/usr/bin/themes/"*.qss 2>/dev/null | wc -l) file .qss)"
else
  warn "Cartella themes/ non trovata accanto al binario — esegui prima ./aggiorna.sh --gui"
fi

# ── 5. Funzione: copia una .so con tutte le sue dipendenze ─────
_copy_lib() {
  local lib="$1"
  local dest="${APPDIR}/usr/lib"
  [[ -f "$lib" ]] || return 0
  local basename
  basename="$(basename "$lib")"
  [[ -f "${dest}/${basename}" ]] && return 0  # già copiata
  cp "$lib" "${dest}/"
  # Segui symlink
  if [[ -L "$lib" ]]; then
    local real
    real="$(readlink -f "$lib")"
    [[ -f "${dest}/$(basename "$real")" ]] || cp "$real" "${dest}/"
  fi
}

# ── 6. Copia tutte le dipendenze .so del binario ──────────────
info "Copio librerie (.so)..."
# Lista dipendenze dirette tramite ldd
while IFS= read -r line; do
  lib_path=$(echo "$line" | awk '{print $3}')
  [[ -f "$lib_path" ]] || continue
  # Salta librerie di sistema che devono restare del sistema
  case "$(basename "$lib_path")" in
    libc.so*|libpthread.so*|libdl.so*|librt.so*|libm.so*|\
    libgcc_s.so*|ld-linux*.so*|libGL.so*|libEGL.so*|\
    libdrm.so*|libGLX.so*|libOpenGL.so*|libGLdispatch.so*)
      continue ;;
  esac
  _copy_lib "$lib_path"
done < <(ldd "${APPDIR}/usr/bin/Prismalux_GUI" 2>/dev/null)

# Aggiungi Qt6 libs che potrebbero mancare (plugin le usano)
QT6_NEEDED=(
  libQt6Widgets libQt6Network libQt6Gui libQt6Core libQt6DBus
  libQt6OpenGL libQt6OpenGLWidgets libQt6PrintSupport
  libQt6Svg libQt6Xml libQt6Concurrent
  libicui18n libicuuc libicudata
  libstdc++ libdouble-conversion libpcre2-16 libzstd
  libharfbuzz libfreetype libfontconfig libpng16
  libxkbcommon libxkbcommon-x11 libxcb libxcb-util
  libxcb-cursor libxcb-icccm4 libxcb-image libxcb-keysyms1
  libxcb-randr libxcb-render libxcb-render-util libxcb-shape
  libxcb-shm libxcb-sync libxcb-xfixes libxcb-xinerama
  libxcb-xkb libX11 libX11-xcb libXext libmd4c libb2
)
for lib_name in "${QT6_NEEDED[@]}"; do
  lib_path=$(find "${QT_LIBS_DIR}" -maxdepth 1 -name "${lib_name}.so*" 2>/dev/null | head -1)
  [[ -n "$lib_path" ]] && _copy_lib "$lib_path"
done

# ── 7. Copia plugin Qt ────────────────────────────────────────
info "Copio plugin Qt6..."

_copy_plugin() {
  local src="$1"
  local dest_dir="$2"
  [[ -f "$src" ]] || return 0
  cp "$src" "${APPDIR}/usr/plugins/${dest_dir}/"
  # Copia anche le .so di cui il plugin dipende
  while IFS= read -r line; do
    local lp
    lp=$(echo "$line" | awk '{print $3}')
    [[ -f "$lp" ]] || continue
    case "$(basename "$lp")" in
      libc.so*|libpthread.so*|libm.so*|libdl.so*|librt.so*|libgcc_s.so*|ld-linux*.so*) continue ;;
    esac
    _copy_lib "$lp"
  done < <(ldd "$src" 2>/dev/null)
}

# Platform plugins (xcb + wayland)
for f in "${QT_PLUGINS_DIR}/platforms"/libqxcb.so \
          "${QT_PLUGINS_DIR}/platforms"/libqwayland-generic.so \
          "${QT_PLUGINS_DIR}/platforms"/libqwayland-egl.so \
          "${QT_PLUGINS_DIR}/platforms"/libqminimal.so \
          "${QT_PLUGINS_DIR}/platforms"/libqoffscreen.so; do
  _copy_plugin "$f" "platforms"
done

# XCB GL integrations
for f in "${QT_PLUGINS_DIR}/xcbglintegrations"/*.so; do
  _copy_plugin "$f" "xcbglintegrations"
done

# Image formats
for f in "${QT_PLUGINS_DIR}/imageformats"/libq{gif,jpeg,png,svg,tga,tiff,wbmp,ico,webp}.so \
          "${QT_PLUGINS_DIR}/imageformats"/libqjpeg.so; do
  _copy_plugin "$f" "imageformats"
done

# Icon engines
for f in "${QT_PLUGINS_DIR}/iconengines"/*.so; do
  _copy_plugin "$f" "iconengines"
done

# Platform themes (GTK, KDE)
for f in "${QT_PLUGINS_DIR}/platformthemes"/*.so; do
  [[ -f "$f" ]] && _copy_plugin "$f" "platformthemes"
done

# Styles
for f in "${QT_PLUGINS_DIR}/styles"/*.so; do
  [[ -f "$f" ]] && _copy_plugin "$f" "styles"
done

# TLS (networking HTTPS)
for f in "${QT_PLUGINS_DIR}/tls"/*.so; do
  [[ -f "$f" ]] && _copy_plugin "$f" "tls"
done

# Wayland
for d in wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration; do
  [[ -d "${QT_PLUGINS_DIR}/$d" ]] || continue
  for f in "${QT_PLUGINS_DIR}/$d"/*.so; do
    [[ -f "$f" ]] && _copy_plugin "$f" "$d"
  done
done

ok "Plugin Qt copiati."

# ── 8. qt.conf — dice a Qt dove trovare i plugin ──────────────
cat > "${APPDIR}/usr/bin/qt.conf" <<'EOF'
[Paths]
Prefix = ..
Plugins = plugins
Libraries = lib
EOF

# ── 9. .desktop file ──────────────────────────────────────────
info "Creo .desktop..."
cat > "${APPDIR}/prismalux.desktop" <<'EOF'
[Desktop Entry]
Version=1.0
Type=Application
Name=Prismalux
GenericName=AI Platform
Comment=Pipeline Agenti · Tutor AI · Matematica · Grafici
Exec=Prismalux_GUI
Icon=prismalux
Terminal=false
Categories=Education;Science;Utility;
Keywords=AI;agenti;matematica;tutor;ollama;grafico;
StartupNotify=true
StartupWMClass=Prismalux_GUI
EOF

# ── 10. Icona ─────────────────────────────────────────────────
info "Copio icona..."
cp "$ICON_SRC" "${APPDIR}/prismalux.png"
cp "$ICON_SRC" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/prismalux.png"
# Symlink richiesto da appimagetool nella root AppDir
ln -sf "usr/share/icons/hicolor/256x256/apps/prismalux.png" \
       "${APPDIR}/.DirIcon" 2>/dev/null || true

# ── 11. AppRun ────────────────────────────────────────────────
info "Creo AppRun..."
cat > "${APPDIR}/AppRun" <<'APPRUN'
#!/usr/bin/env bash
# AppRun — entry point dell'AppImage Prismalux
HERE="$(dirname "$(readlink -f "$0")")"

# Librerie bundle
export LD_LIBRARY_PATH="${HERE}/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Plugin Qt
export QT_PLUGIN_PATH="${HERE}/usr/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"

# Forza xcb se nessun DISPLAY Wayland disponibile
if [[ -z "${WAYLAND_DISPLAY}" && -z "${QT_QPA_PLATFORM}" ]]; then
  export QT_QPA_PLATFORM=xcb
fi

# Font e temi
export FONTCONFIG_PATH="/etc/fonts"
export QT_AUTO_SCREEN_SCALE_FACTOR=1

exec "${HERE}/usr/bin/Prismalux_GUI" "$@"
APPRUN
chmod +x "${APPDIR}/AppRun"

# ── 12. Genera AppImage ───────────────────────────────────────
info "Genero AppImage con appimagetool..."
rm -f "$OUTPUT"
ARCH=x86_64 "$APPIMAGETOOL" \
  --no-appstream \
  "$APPDIR" \
  "$OUTPUT" 2>&1

[[ -f "$OUTPUT" ]] || die "appimagetool non ha generato l'output."

SIZE_MB=$(du -m "$OUTPUT" | cut -f1)
ok "AppImage creata: $(basename "$OUTPUT")  (${SIZE_MB} MB)"
echo ""
echo -e "  ${CYN}Esecuzione:${NC}  chmod +x Prismalux-x86_64.AppImage && ./Prismalux-x86_64.AppImage"
echo -e "  ${CYN}Integrazione desktop:${NC}  ./Prismalux-x86_64.AppImage --appimage-extract-and-run"
echo ""

#!/usr/bin/env python3
"""
build.py — Compila Prismalux GUI su Windows, Linux e macOS.
Uso: python build.py   (o doppio clic su build.bat su Windows)

Piattaforme supportate:
  Windows : MSYS2 UCRT64 · Qt installer (C:\\Qt) · COMPILE_WIN toolchain
  Linux   : Qt6 di sistema (apt/dnf) · installazione utente (~/Qt, /opt/Qt)
  macOS   : Homebrew arm64/Intel · Qt installer (~/Qt)
"""

import os
import platform
import shutil
import subprocess
import sys
import multiprocessing
from pathlib import Path

# ── Costanti base ─────────────────────────────────────────────────────────────
ROOT    = Path(__file__).parent.resolve()
GUI_DIR = ROOT / "gui"

PLAT   = platform.system()   # "Windows" | "Linux" | "Darwin"
IS_WIN = PLAT == "Windows"
IS_LIN = PLAT == "Linux"
IS_MAC = PLAT == "Darwin"

# Cartella di build per piattaforma
BUILD_DIR = {
    "Windows": GUI_DIR / "build_win",
    "Darwin":  GUI_DIR / "build_mac",
}.get(PLAT, ROOT / "build_gui")   # Linux → ROOT/build_gui (convenzione CLAUDE.md)

LOG_FILE   = BUILD_DIR / "prismalux_build.log"
ERROR_FILE = ROOT / "errore.txt"

# ── Output colorato ───────────────────────────────────────────────────────────
C_OK   = "\033[92m"
C_WARN = "\033[93m"
C_ERR  = "\033[91m"
C_BOLD = "\033[1m"
C_RST  = "\033[0m"

def ok(msg):   print(f"  {C_OK}[OK]{C_RST}    {msg}", flush=True)
def warn(msg): print(f"  {C_WARN}[WARN]{C_RST}  {msg}", flush=True)
def err(msg):  print(f"  {C_ERR}[ERRORE]{C_RST} {msg}", flush=True)
def step(msg): print(f"\n{C_BOLD}  ── {msg}{C_RST}", flush=True)
def sep():     print("  " + "─" * 50, flush=True)


# ── errore.txt ────────────────────────────────────────────────────────────────
def write_errore_txt(fase: str, log_path: Path) -> None:
    """Scrive errore.txt filtrando le righe di errore GCC/Clang/CMake/MSVC dal log."""
    lines: list[str] = []
    if log_path.exists():
        lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()

    error_kw = ("error:", "Error:", "FAILED", "fatal error:", "undefined reference",
                "cannot find", "no such file", "CMake Error", "LINK : fatal",
                "LNK2", "LNK1", "ld: ", "collect2:")
    error_lines = [l for l in lines if any(k in l for k in error_kw)]
    if not error_lines:
        error_lines = lines[-60:]

    content = (
        f"Prismalux — Errore di compilazione\n"
        f"Piattaforma: {PLAT}\n"
        f"Fase: {fase}\n"
        f"Log: {log_path}\n"
        f"{'=' * 60}\n\n"
        + "\n".join(error_lines)
        + f"\n\n{'=' * 60}\n"
        + f"Log completo: {log_path}\n"
    )
    ERROR_FILE.write_text(content, encoding="utf-8")
    print(f"\n  {C_ERR}Errore salvato in: {ERROR_FILE}{C_RST}\n", flush=True)


# ── stato build (build_status.txt) ───────────────────────────────────────────
STATUS_FILE = ROOT / "build_status.txt"

def _write_status(stato: str, version: str = "", exe: Path | None = None) -> None:
    """Scrive build_status.txt con stato chiaro: OK o ERRORE."""
    import datetime
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    if stato == "OK":
        lines = [
            "=" * 54,
            f"  BUILD COMPLETATA CON SUCCESSO",
            f"  Versione  : {version}",
            f"  Eseguibile: {exe}",
            f"  Data/ora  : {ts}",
            "=" * 54,
            "",
            "Per avviare: doppio clic su Avvia_Prismalux.bat",
        ]
    else:
        lines = [
            "=" * 54,
            f"  BUILD FALLITA",
            f"  Data/ora  : {ts}",
            f"  Dettagli  : vedi errore.txt",
            "=" * 54,
        ]
    STATUS_FILE.write_text("\n".join(lines) + "\n", encoding="utf-8")


# ── Utility comuni ────────────────────────────────────────────────────────────
def find_exe(*paths) -> str | None:
    """Restituisce il primo percorso esistente tra quelli forniti."""
    for p in paths:
        if p and Path(str(p)).exists():
            return str(p)
    return None


def run(cmd: list, env: dict | None = None, cwd: Path | None = None) -> int:
    """Esegue un comando con output in streaming e salva nel log."""
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    with open(LOG_FILE, "a", encoding="utf-8", errors="replace") as log:
        log.write(f"\n>>> {' '.join(str(c) for c in cmd)}\n")
        proc = subprocess.Popen(
            [str(c) for c in cmd],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, encoding="utf-8", errors="replace",
            env=env, cwd=str(cwd) if cwd else None,
        )
        for line in proc.stdout:
            print(line, end="", flush=True)
            log.write(line)
        proc.wait()
        log.write(f"\n[exit {proc.returncode}]\n")
    return proc.returncode


def read_version() -> str:
    cmake_lists = GUI_DIR / "CMakeLists.txt"
    if not cmake_lists.exists():
        return "2.9"
    for line in cmake_lists.read_text(encoding="utf-8").splitlines():
        if "project(Prismalux_GUI VERSION" in line:
            parts = line.split("VERSION")
            if len(parts) > 1:
                return parts[1].strip().split()[0].rstrip(")")
    return "2.9"


def _build_common(cmake: str, ninja: str | None, qt_prefix: Path | None,
                  env: dict, nproc: int) -> int:
    """Fasi [1/3] configure e [2/3] build, uguali su tutte le piattaforme."""
    # [1/3] configure
    if (BUILD_DIR / "CMakeCache.txt").exists():
        step("[1/3] cmake configure — già presente, salto")
    else:
        step("[1/3] cmake configure...")
        sep()
        cmake_cmd = [cmake, "-B", str(BUILD_DIR), "-S", str(GUI_DIR),
                     "-DCMAKE_BUILD_TYPE=Release"]
        if qt_prefix:
            cmake_cmd.append(f"-DCMAKE_PREFIX_PATH={qt_prefix}")
        if ninja:
            cmake_cmd += ["-G", "Ninja"]
        rc = run(cmake_cmd, env=env)
        if rc != 0:
            err(f"cmake configure fallito (exit {rc})")
            write_errore_txt("cmake configure", LOG_FILE)
            return rc
        ok("Configure completato")

    # [2/3] build
    step(f"[2/3] Compilazione ({nproc} thread)...")
    sep()
    rc = run([cmake, "--build", str(BUILD_DIR), "--config", "Release",
              "-j", str(nproc)], env=env)
    if rc != 0:
        err(f"Compilazione fallita (exit {rc})")
        write_errore_txt("cmake build", LOG_FILE)
        log_lines = LOG_FILE.read_text(encoding="utf-8", errors="replace").splitlines()
        print("\n  Ultime 30 righe del log:\n  " + "─" * 48)
        for l in log_lines[-30:]:
            print(f"  {l}")
        return rc
    return 0


# ══════════════════════════════════════════════════════════════════════════════
#   WINDOWS
# ══════════════════════════════════════════════════════════════════════════════

def _win_find_msys2() -> Path | None:
    for p in [Path("C:/msys64"), Path("D:/msys64")]:
        if p.exists():
            return p
    return None


def _win_find_qt6(msys2: Path | None) -> Path | None:
    candidates: list[Path] = []
    toolchain = ROOT / "COMPILE_WIN" / "toolchain" / "Qt6"
    if toolchain.exists():
        for ver in sorted(toolchain.iterdir(), reverse=True):
            candidates.append(ver / "mingw_64")
    if msys2:
        candidates += [msys2 / "ucrt64", msys2 / "mingw64"]
    for qt_root in [Path("C:/Qt"), Path("D:/Qt")]:
        if qt_root.exists():
            for ver in sorted(qt_root.iterdir(), reverse=True):
                for sub in ["mingw_64", "msvc2022_64", "msvc2019_64"]:
                    candidates.append(ver / sub)
    for c in candidates:
        if (c / "lib/cmake/Qt6/Qt6Config.cmake").exists():
            return c
    return None


def _win_find_cmake(qt: Path | None, msys2: Path | None) -> str:
    cmake_tool = ROOT / "COMPILE_WIN/toolchain/cmake"
    portable = None
    if cmake_tool.exists():
        entries = sorted(cmake_tool.glob("cmake-*"))
        if entries:
            portable = entries[-1] / "bin/cmake.exe"
    return find_exe(
        qt / "bin/cmake.exe" if qt else None,
        msys2 / "ucrt64/bin/cmake.exe" if msys2 else None,
        portable,
        shutil.which("cmake"),
    ) or "cmake"


def _win_find_ninja(qt: Path | None, msys2: Path | None) -> str | None:
    return find_exe(
        qt / "bin/ninja.exe" if qt else None,
        msys2 / "ucrt64/bin/ninja.exe" if msys2 else None,
        ROOT / "COMPILE_WIN/toolchain/ninja/ninja.exe",
        shutil.which("ninja"),
    )


def _win_copy_essential_dlls(qt: Path, build: Path) -> None:
    """Copia DLL Qt + dipendenze che windeployqt spesso manca (MSYS2 UCRT64).
    Viene chiamata SEMPRE dopo windeployqt, non solo come fallback."""
    dlls = [
        # Qt core
        "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll",
        "Qt6Network.dll", "Qt6PrintSupport.dll", "Qt6Sql.dll",
        "Qt6Xml.dll", "Qt6DBus.dll",
        # Runtime MinGW
        "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll",
        # Dipendenze Qt tipicamente mancanti con windeployqt MSYS2
        "libfreetype-6.dll", "libharfbuzz-0.dll", "libpng16-16.dll",
        "libbz2-1.dll", "libbrotlidec.dll", "libbrotlicommon.dll",
        "libgraphite2.dll", "libintl-8.dll", "libiconv-2.dll",
        "libdouble-conversion.dll", "libpcre2-16-0.dll", "libpcre2-8-0.dll",
        "libzstd.dll", "zlib1.dll", "libmd4c.dll",
    ]
    # Cerca in MSYS2 ucrt64/bin (priorità) poi in qt/bin
    search: list[Path] = []
    for msys in [Path("C:/msys64"), Path("C:/msys2"), Path("D:/msys64"), Path("D:/msys2")]:
        p = msys / "ucrt64/bin"
        if p.exists():
            search.append(p)
            break
    search.append(qt / "bin")

    copied = 0
    for dll in dlls:
        dst = build / dll
        if dst.exists():
            continue  # già presente
        for d in search:
            src = d / dll
            if src.exists():
                shutil.copy2(src, dst)
                copied += 1
                break
    if copied:
        ok(f"{copied} DLL aggiuntive copiate")

    # Plugin platforms (qwindows.dll)
    for plug_base in ["share/qt6/plugins", "plugins"]:
        p = qt / plug_base / "platforms"
        if p.exists():
            dst = build / "platforms"
            dst.mkdir(exist_ok=True)
            for f in p.glob("*.dll"):
                shutil.copy2(f, dst / f.name)
            break


def _win_copy_mingw_runtime(qt: Path, build: Path) -> None:
    """Copia le DLL runtime MinGW/MSYS2 che windeployqt non include mai."""
    runtime_dlls = [
        "libwinpthread-1.dll",
        "libgcc_s_seh-1.dll",
        "libstdc++-6.dll",
    ]
    # Tutti i percorsi candidati in ordine di priorità
    search_dirs: list[Path] = []
    # 1. Cartella DLL/ del progetto (DLL bundlate nel repo)
    dll_bundled = Path(__file__).parent / "DLL"
    if dll_bundled.exists():
        search_dirs.append(dll_bundled)
    # 2. MSYS2 su C: e D:
    for msys_root in [
        Path("C:/msys64"), Path("C:/msys2"),
        Path("D:/msys64"), Path("D:/msys2"),
    ]:
        for sub in ["ucrt64/bin", "mingw64/bin", "usr/bin"]:
            candidate = msys_root / sub
            if candidate.exists():
                search_dirs.append(candidate)
    search_dirs.append(qt / "bin")  # Qt installer a volte le include

    copied, missing = [], []
    for dll in runtime_dlls:
        dst = build / dll
        if dst.exists():
            continue  # già presente
        for d in search_dirs:
            src = d / dll
            if src.exists():
                shutil.copy2(src, dst)
                copied.append(dll)
                break
        else:
            missing.append(dll)

    if copied:
        ok(f"Runtime MinGW copiati: {', '.join(copied)}")
    if missing:
        warn(f"DLL non trovate (copia manuale da msys64/ucrt64/bin/): {', '.join(missing)}")
        warn("Senza queste DLL l'app non parte su PC senza MSYS2 nel PATH.")


def main_windows(version: str) -> int:
    step("Rilevamento toolchain Windows...")

    msys2 = _win_find_msys2()
    if msys2:
        ok(f"MSYS2        : {msys2}")
    else:
        warn("MSYS2 non trovato — uso cmake/Qt di sistema")

    qt = _win_find_qt6(msys2)
    if not qt:
        err("Qt6 non trovato in nessuna posizione.")
        print()
        print("  Installa Qt6 con MSYS2 (apri 'MSYS2 UCRT64'):")
        for pkg in ["qt6-base", "qt6-tools", "qt6-sql", "gcc", "cmake", "ninja"]:
            print(f"    pacman -S mingw-w64-ucrt-x86_64-{pkg}")
        print()
        print("  Oppure scarica Qt da: https://www.qt.io/download-qt-installer")
        return 1
    ok(f"Qt6          : {qt}")

    cmake = _win_find_cmake(qt, msys2)
    ninja = _win_find_ninja(qt, msys2)
    ok(f"cmake        : {cmake}")
    if ninja:
        ok(f"ninja        : {ninja}")
    else:
        warn("ninja non trovato — uso make (più lento)")

    nproc = max(1, multiprocessing.cpu_count() - 1)
    ok(f"Thread       : {nproc}")
    ok(f"Output       : {BUILD_DIR}")
    ok(f"Log          : {LOG_FILE}")

    env = os.environ.copy()
    extra = [str(qt / "bin")]
    if msys2:
        extra += [str(msys2 / "ucrt64/bin"), str(msys2 / "usr/bin")]
    env["PATH"] = ";".join(extra) + ";" + env.get("PATH", "")

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    LOG_FILE.write_text(f"Prismalux v{version} — Build Windows\n{'=' * 60}\n", encoding="utf-8")
    ERROR_FILE.unlink(missing_ok=True)

    rc = _build_common(cmake, ninja, qt, env, nproc)
    if rc != 0:
        return rc

    exe = BUILD_DIR / "Prismalux_GUI.exe"
    if not exe.exists():
        err(f"Binario non trovato: {exe}")
        write_errore_txt("cmake build (binario mancante)", LOG_FILE)
        return 1
    ok(f"Compilato → {exe}")

    # [3/3] windeployqt
    step("[3/3] Deploy DLL Qt...")
    sep()
    deploy = find_exe(qt / "bin/windeployqt6.exe", qt / "bin/windeployqt.exe")
    if deploy:
        rc = run([deploy, "--release", "--no-translations", str(exe)], env=env)
        if rc == 0:
            ok("DLL Qt copiate con windeployqt")
        else:
            warn(f"windeployqt terminato con codice {rc}")
    else:
        warn("windeployqt non trovato")

    # Copia sempre DLL essenziali + runtime MinGW: windeployqt MSYS2 manca
    # spesso libfreetype, libbz2, libbrotli e le DLL runtime GCC.
    _win_copy_essential_dlls(qt, BUILD_DIR)
    _win_copy_mingw_runtime(qt, BUILD_DIR)

    themes_src = GUI_DIR / "themes"
    themes_dst = BUILD_DIR / "themes"
    if themes_src.exists() and not themes_dst.exists():
        shutil.copytree(themes_src, themes_dst)
        ok("themes/ copiato")

    _write_status("OK", version, exe)
    print()
    print("  " + "#" * 54)
    print("  #                                                    #")
    print(f"  #   BUILD COMPLETATA CON SUCCESSO  v{version:<16}  #")
    print("  #                                                    #")
    print(f"  #   Eseguibile : {str(exe)[-34:]:<34}  #")
    print("  #   Per avviare: doppio clic su Avvia_Prismalux.bat #")
    print("  #                                                    #")
    print("  " + "#" * 54)
    print()
    return 0


# ══════════════════════════════════════════════════════════════════════════════
#   LINUX
# ══════════════════════════════════════════════════════════════════════════════

def _lin_find_qt6() -> tuple[Path | None, bool]:
    """Ritorna (qt_prefix, trovato).
    qt_prefix=None  → Qt di sistema, cmake lo trova da solo senza CMAKE_PREFIX_PATH.
    qt_prefix=Path  → Qt custom, va passato a -DCMAKE_PREFIX_PATH.
    """
    arch = platform.machine()   # x86_64, aarch64, …

    # Qt installer personalizzato: ~/Qt, /opt/Qt
    for base in [Path.home() / "Qt", Path("/opt/Qt")]:
        if not base.exists():
            continue
        for item in sorted(base.iterdir(), reverse=True):
            if not item.is_dir():
                continue
            for sub in ["gcc_64", "linux_gcc_64", "linux"]:
                c = item / sub
                if (c / "lib/cmake/Qt6/Qt6Config.cmake").exists():
                    return c, True

    # /usr/local (build from source, Linuxbrew)
    if (Path("/usr/local/lib/cmake/Qt6/Qt6Config.cmake")).exists():
        return Path("/usr/local"), True

    # Qt di sistema (apt/dnf) — cmake lo trova senza CMAKE_PREFIX_PATH
    for lib in [f"/usr/lib/{arch}-linux-gnu", "/usr/lib64", "/usr/lib"]:
        if Path(f"{lib}/cmake/Qt6/Qt6Config.cmake").exists():
            return None, True   # trovato ma cmake non ha bisogno di prefix

    return None, False


def main_linux(version: str) -> int:
    step("Rilevamento toolchain Linux...")

    if not shutil.which("cmake"):
        err("cmake non trovato.")
        print("  Installa con:")
        print("    sudo apt install cmake ninja-build build-essential")
        print("    sudo apt install qt6-base-dev qt6-tools-dev-tools")
        print("    sudo apt install libqt6sql6-sqlite qt6-webengine-dev")
        return 1
    cmake = "cmake"
    ok(f"cmake        : {shutil.which(cmake)}")

    ninja = shutil.which("ninja")
    if ninja:
        ok(f"ninja        : {ninja}")
    else:
        warn("ninja non trovato — uso make (più lento)")

    qt, qt_found = _lin_find_qt6()
    if not qt_found:
        err("Qt6 non trovato.")
        print("  Installa con:")
        print("    sudo apt install qt6-base-dev qt6-tools-dev-tools")
        print("    sudo apt install libqt6sql6-sqlite qt6-webengine-dev")
        return 1
    if qt:
        ok(f"Qt6          : {qt}")
    else:
        ok("Qt6          : sistema (rilevato automaticamente da cmake)")

    nproc = max(1, multiprocessing.cpu_count() - 1)
    ok(f"Thread       : {nproc}")
    ok(f"Output       : {BUILD_DIR}")
    ok(f"Log          : {LOG_FILE}")

    env = os.environ.copy()
    if qt:
        env["PATH"] = str(qt / "bin") + ":" + env.get("PATH", "")

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    LOG_FILE.write_text(f"Prismalux v{version} — Build Linux\n{'=' * 60}\n", encoding="utf-8")
    ERROR_FILE.unlink(missing_ok=True)

    rc = _build_common(cmake, ninja, qt, env, nproc)
    if rc != 0:
        return rc

    exe = BUILD_DIR / "Prismalux_GUI"
    if not exe.exists():
        err(f"Binario non trovato: {exe}")
        write_errore_txt("cmake build (binario mancante)", LOG_FILE)
        return 1
    exe.chmod(exe.stat().st_mode | 0o111)   # assicura bit +x
    ok(f"Compilato → {exe}")

    # [3/3] post-build
    step("[3/3] Post-build...")
    themes_src = GUI_DIR / "themes"
    themes_dst = BUILD_DIR / "themes"
    if themes_src.exists() and not themes_dst.exists():
        shutil.copytree(themes_src, themes_dst)
        ok("themes/ copiato")
    ok("Per creare AppImage: ./aggiorna.sh   oppure   bash EXPORT/linux/crea_appimage.sh")

    print(f"\n  {'=' * 50}")
    print(f"{C_OK}{C_BOLD}    Prismalux v{version} compilato con successo! (Linux){C_RST}")
    print(f"  {'=' * 50}")
    print(f"\n  Eseguibile : {exe}")
    print("  Per avviare: ./avvia.sh   oppure   ./build_gui/Prismalux_GUI\n")
    return 0


# ══════════════════════════════════════════════════════════════════════════════
#   macOS
# ══════════════════════════════════════════════════════════════════════════════

def _mac_find_qt6() -> Path | None:
    candidates: list[Path] = []

    # Homebrew Apple Silicon (/opt/homebrew) e Intel (/usr/local)
    for brew in [Path("/opt/homebrew"), Path("/usr/local")]:
        for name in ["qt6", "qt"]:
            c = brew / "opt" / name
            if (c / "lib/cmake/Qt6/Qt6Config.cmake").exists():
                candidates.append(c)

    # Qt installer ufficiale (~/Qt/)
    qt_home = Path.home() / "Qt"
    if qt_home.exists():
        for ver in sorted(qt_home.iterdir(), reverse=True):
            if not ver.is_dir():
                continue
            for sub in ["macos", "clang_64"]:
                c = ver / sub
                if (c / "lib/cmake/Qt6/Qt6Config.cmake").exists():
                    candidates.append(c)

    for c in candidates:
        if (c / "lib/cmake/Qt6/Qt6Config.cmake").exists():
            return c
    return None


def main_macos(version: str) -> int:
    step("Rilevamento toolchain macOS...")

    qt = _mac_find_qt6()
    if not qt:
        err("Qt6 non trovato.")
        print()
        print("  Installa con Homebrew:  brew install qt6")
        print("  Oppure scarica Qt da:   https://www.qt.io/download-qt-installer")
        print()
        return 1
    ok(f"Qt6          : {qt}")

    # cmake/ninja: prima da Qt bin, poi Homebrew, poi PATH
    cmake = find_exe(
        qt / "bin/cmake",
        "/opt/homebrew/bin/cmake",
        "/usr/local/bin/cmake",
        shutil.which("cmake"),
    ) or "cmake"
    ninja = find_exe(
        qt / "bin/ninja",
        "/opt/homebrew/bin/ninja",
        "/usr/local/bin/ninja",
        shutil.which("ninja"),
    )
    ok(f"cmake        : {cmake}")
    if ninja:
        ok(f"ninja        : {ninja}")
    else:
        warn("ninja non trovato — uso make (più lento)")

    nproc = max(1, multiprocessing.cpu_count() - 1)
    ok(f"Thread       : {nproc}")
    ok(f"Output       : {BUILD_DIR}")
    ok(f"Log          : {LOG_FILE}")

    env = os.environ.copy()
    env["PATH"] = str(qt / "bin") + ":" + env.get("PATH", "")

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    LOG_FILE.write_text(f"Prismalux v{version} — Build macOS\n{'=' * 60}\n", encoding="utf-8")
    ERROR_FILE.unlink(missing_ok=True)

    rc = _build_common(cmake, ninja, qt, env, nproc)
    if rc != 0:
        return rc

    # cmake su macOS può produrre .app bundle o binario diretto
    exe_app = BUILD_DIR / "Prismalux_GUI.app"
    exe_bin = BUILD_DIR / "Prismalux_GUI"
    exe = exe_app if exe_app.exists() else exe_bin

    if not exe.exists():
        err(f"Binario/bundle non trovato in {BUILD_DIR}")
        write_errore_txt("cmake build (binario mancante)", LOG_FILE)
        return 1
    ok(f"Compilato → {exe}")

    # [3/3] macdeployqt
    step("[3/3] macdeployqt...")
    sep()
    deploy = find_exe(qt / "bin/macdeployqt", shutil.which("macdeployqt"))
    if deploy and exe_app.exists():
        rc_d = run([deploy, str(exe_app), "-verbose=1"], env=env)
        if rc_d == 0:
            ok("Framework Qt copiati con macdeployqt")
        else:
            warn(f"macdeployqt terminato con codice {rc_d}")
    elif not exe_app.exists():
        warn("Nessun .app bundle trovato — salto macdeployqt")
    else:
        warn("macdeployqt non trovato in PATH o Qt bin")

    themes_src = GUI_DIR / "themes"
    themes_dst = BUILD_DIR / "themes"
    if themes_src.exists() and not themes_dst.exists():
        shutil.copytree(themes_src, themes_dst)
        ok("themes/ copiato")

    print(f"\n  {'=' * 50}")
    print(f"{C_OK}{C_BOLD}    Prismalux v{version} compilato con successo! (macOS){C_RST}")
    print(f"  {'=' * 50}")
    print(f"\n  Eseguibile : {exe}")
    if exe_app.exists():
        print(f"  Per avviare: open {exe_app}")
    else:
        print(f"  Per avviare: {exe}")
    print()
    return 0


# ══════════════════════════════════════════════════════════════════════════════
#   ENTRY POINT
# ══════════════════════════════════════════════════════════════════════════════

def main() -> int:
    version = read_version()
    plat_label = {"Windows": "Windows", "Linux": "Linux", "Darwin": "macOS"}.get(PLAT, PLAT)

    print()
    print(f"{C_BOLD}  {'=' * 50}{C_RST}")
    print(f"{C_BOLD}    Prismalux v{version} — Build {plat_label}{C_RST}")
    print(f"{C_BOLD}  {'=' * 50}{C_RST}")
    print()

    if IS_WIN:
        return main_windows(version)
    if IS_MAC:
        return main_macos(version)
    if IS_LIN:
        return main_linux(version)
    err(f"Piattaforma non supportata: {PLAT}")
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\n  [Interrotto]")
        sys.exit(1)

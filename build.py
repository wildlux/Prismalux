#!/usr/bin/env python3
"""
build.py — Compila Prismalux GUI su Windows.
Lanciato da build.bat (doppio clic) o direttamente con: python build.py

Rileva automaticamente:
  - MSYS2 UCRT64 / MINGW64
  - Qt installer ufficiale (C:\\Qt\\)
  - COMPILE_WIN\\toolchain\\ (portatile)
  - cmake, ninja, windeployqt
"""

import os
import sys
import shutil
import subprocess
import multiprocessing
from pathlib import Path

# ── Costanti ──────────────────────────────────────────────────────────────────
ROOT      = Path(__file__).parent.resolve()
GUI_DIR   = ROOT / "gui"
BUILD_DIR = GUI_DIR / "build_win"
LOG_FILE  = BUILD_DIR / "prismalux_build.log"

C_OK   = "\033[92m"   # verde
C_WARN = "\033[93m"   # giallo
C_ERR  = "\033[91m"   # rosso
C_BOLD = "\033[1m"
C_RST  = "\033[0m"

def ok(msg):   print(f"  {C_OK}[OK]{C_RST}    {msg}", flush=True)
def warn(msg): print(f"  {C_WARN}[WARN]{C_RST}  {msg}", flush=True)
def err(msg):  print(f"  {C_ERR}[ERRORE]{C_RST} {msg}", flush=True)
def step(msg): print(f"\n{C_BOLD}  ── {msg}{C_RST}", flush=True)
def sep():     print("  " + "─" * 50, flush=True)


# ── Rilevamento toolchain ─────────────────────────────────────────────────────

def find_msys2() -> Path | None:
    for p in [Path("C:/msys64"), Path("D:/msys64")]:
        if p.exists():
            return p
    return None


def find_qt6(msys2: Path | None) -> Path | None:
    """Restituisce il prefix Qt6 (la cartella che contiene lib/cmake/Qt6/)."""
    candidates: list[Path] = []

    # COMPILE_WIN toolchain portatile
    toolchain = ROOT / "COMPILE_WIN" / "toolchain" / "Qt6"
    if toolchain.exists():
        for ver in sorted(toolchain.iterdir(), reverse=True):
            candidates.append(ver / "mingw_64")

    # MSYS2
    if msys2:
        candidates += [msys2 / "ucrt64", msys2 / "mingw64"]

    # Qt installer ufficiale
    for qt_root in [Path("C:/Qt"), Path("D:/Qt")]:
        if qt_root.exists():
            for ver in sorted(qt_root.iterdir(), reverse=True):
                for sub in ["mingw_64", "msvc2022_64", "msvc2019_64"]:
                    candidates.append(ver / sub)

    for c in candidates:
        if (c / "lib/cmake/Qt6/Qt6Config.cmake").exists():
            return c
    return None


def find_exe(*paths: str | Path) -> str | None:
    """Cerca il primo eseguibile trovato tra i percorsi dati."""
    for p in paths:
        if p and Path(p).exists():
            return str(p)
    return None


def find_cmake(qt: Path | None, msys2: Path | None) -> str:
    found = find_exe(
        qt / "bin/cmake.exe" if qt else None,
        msys2 / "ucrt64/bin/cmake.exe" if msys2 else None,
        ROOT / "COMPILE_WIN/toolchain/cmake" / next(
            iter(sorted((ROOT / "COMPILE_WIN/toolchain/cmake").glob("cmake-*"))),
            Path("x")
        ) / "bin/cmake.exe" if (ROOT / "COMPILE_WIN/toolchain/cmake").exists() else None,
        shutil.which("cmake"),
    )
    return found or "cmake"


def find_ninja(qt: Path | None, msys2: Path | None) -> str | None:
    return find_exe(
        qt / "bin/ninja.exe" if qt else None,
        msys2 / "ucrt64/bin/ninja.exe" if msys2 else None,
        ROOT / "COMPILE_WIN/toolchain/ninja/ninja.exe",
        shutil.which("ninja"),
    )


def build_env(qt: Path, msys2: Path | None) -> dict:
    """Ambiente con PATH arricchito per cmake/ninja/gcc."""
    env = os.environ.copy()
    extra = [str(qt / "bin")]
    if msys2:
        extra += [
            str(msys2 / "ucrt64/bin"),
            str(msys2 / "usr/bin"),
        ]
    env["PATH"] = ";".join(extra) + ";" + env.get("PATH", "")
    return env


# ── Esecuzione subprocess con output in streaming ─────────────────────────────

def run(cmd: list, env: dict | None = None, cwd: Path | None = None) -> int:
    """Esegue un comando mostrando output in tempo reale e salvando nel log."""
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    with open(LOG_FILE, "a", encoding="utf-8", errors="replace") as log:
        log.write(f"\n>>> {' '.join(str(c) for c in cmd)}\n")
        proc = subprocess.Popen(
            [str(c) for c in cmd],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=env,
            cwd=str(cwd) if cwd else None,
        )
        for line in proc.stdout:
            print(line, end="", flush=True)
            log.write(line)
        proc.wait()
        log.write(f"\n[exit {proc.returncode}]\n")
    return proc.returncode


# ── Lettura versione da CMakeLists.txt ────────────────────────────────────────

def read_version() -> str:
    cmake_lists = GUI_DIR / "CMakeLists.txt"
    if cmake_lists.exists():
        for line in cmake_lists.read_text(encoding="utf-8").splitlines():
            if "project(Prismalux_GUI VERSION" in line:
                parts = line.split("VERSION")
                if len(parts) > 1:
                    return parts[1].strip().split()[0].rstrip(")")
    return "2.9"


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> int:
    version = read_version()

    print()
    print(f"{C_BOLD}  {'=' * 50}{C_RST}")
    print(f"{C_BOLD}    Prismalux v{version} — Build Windows{C_RST}")
    print(f"{C_BOLD}  {'=' * 50}{C_RST}")
    print()

    # ── Rileva toolchain ──────────────────────────────────────────────────────
    step("Rilevamento toolchain...")

    msys2 = find_msys2()
    if msys2:
        ok(f"MSYS2        : {msys2}")
    else:
        warn("MSYS2 non trovato — uso cmake/Qt di sistema")

    qt = find_qt6(msys2)
    if not qt:
        err("Qt6 non trovato in nessuna posizione.")
        print()
        print("  Installa Qt6 con MSYS2 (apri 'MSYS2 UCRT64'):")
        print("    pacman -S mingw-w64-ucrt-x86_64-qt6-base")
        print("    pacman -S mingw-w64-ucrt-x86_64-qt6-tools")
        print("    pacman -S mingw-w64-ucrt-x86_64-qt6-sql")
        print("    pacman -S mingw-w64-ucrt-x86_64-gcc")
        print("    pacman -S mingw-w64-ucrt-x86_64-cmake")
        print("    pacman -S mingw-w64-ucrt-x86_64-ninja")
        print()
        print("  Oppure scarica Qt da: https://www.qt.io/download-qt-installer")
        return 1
    ok(f"Qt6          : {qt}")

    cmake = find_cmake(qt, msys2)
    ninja = find_ninja(qt, msys2)
    ok(f"cmake        : {cmake}")
    if ninja:
        ok(f"ninja        : {ninja}")
    else:
        warn("ninja non trovato — uso make (più lento)")

    nproc = max(1, multiprocessing.cpu_count() - 1)
    ok(f"Thread       : {nproc}  (core - 1)")
    ok(f"Sorgenti     : {GUI_DIR}")
    ok(f"Output       : {BUILD_DIR}")
    ok(f"Log          : {LOG_FILE}")

    env = build_env(qt, msys2)

    # Azzera il log
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    LOG_FILE.write_text(
        f"Prismalux v{version} — Build log\n{'=' * 60}\n",
        encoding="utf-8"
    )

    # ── [1/3] cmake configure ─────────────────────────────────────────────────
    if (BUILD_DIR / "CMakeCache.txt").exists():
        step("[1/3] cmake configure — già presente, salto")
    else:
        step("[1/3] cmake configure...")
        sep()

        cmake_cmd = [
            cmake,
            "-B", BUILD_DIR,
            "-S", GUI_DIR,
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_PREFIX_PATH={qt}",
        ]
        if ninja:
            cmake_cmd += ["-G", "Ninja"]

        rc = run(cmake_cmd, env=env)
        if rc != 0:
            err(f"cmake configure fallito (exit {rc})")
            print(f"\n  Log completo: {LOG_FILE}")
            return rc
        ok("Configure completato")

    # ── [2/3] cmake build ─────────────────────────────────────────────────────
    step(f"[2/3] Compilazione ({nproc} thread)...")
    sep()

    rc = run(
        [cmake, "--build", BUILD_DIR, "--config", "Release", "-j", str(nproc)],
        env=env,
    )

    exe = BUILD_DIR / "Prismalux_GUI.exe"
    if rc != 0 or not exe.exists():
        err(f"Compilazione fallita (exit {rc})" if rc != 0
            else f"Binario non trovato: {exe}")
        print(f"\n  Ultime 30 righe del log:")
        print("  " + "─" * 48)
        lines = LOG_FILE.read_text(encoding="utf-8", errors="replace").splitlines()
        for l in lines[-30:]:
            print(f"  {l}")
        print(f"\n  Log completo: {LOG_FILE}")
        return 1
    ok(f"Compilato → {exe}")

    # ── [3/3] windeployqt ─────────────────────────────────────────────────────
    step("[3/3] Deploy DLL Qt...")
    sep()

    deploy = find_exe(
        qt / "bin/windeployqt6.exe",
        qt / "bin/windeployqt.exe",
    )
    if deploy:
        rc = run([deploy, "--release", "--no-translations", str(exe)], env=env)
        if rc == 0:
            ok("DLL Qt copiate con windeployqt")
        else:
            warn(f"windeployqt terminato con codice {rc}")
    else:
        warn("windeployqt non trovato — copia DLL essenziali manualmente")
        _copy_essential_dlls(qt, BUILD_DIR)

    # Copia themes/ se non già presente
    themes_src = GUI_DIR / "themes"
    themes_dst = BUILD_DIR / "themes"
    if themes_src.exists() and not themes_dst.exists():
        shutil.copytree(themes_src, themes_dst)
        ok("themes/ copiato")

    # ── Risultato ─────────────────────────────────────────────────────────────
    print()
    print(f"  {'=' * 50}")
    print(f"{C_OK}{C_BOLD}    Prismalux v{version} compilato con successo!{C_RST}")
    print(f"  {'=' * 50}")
    print()
    print(f"  Eseguibile : {exe}")
    print()
    print("  Per avviare: doppio clic su  Avvia_Prismalux.bat")
    print()
    return 0


def _copy_essential_dlls(qt: Path, build: Path):
    """Copia DLL minime se windeployqt non è disponibile."""
    dlls = [
        "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll",
        "Qt6Network.dll", "Qt6PrintSupport.dll", "Qt6Sql.dll",
        "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll",
        "libdouble-conversion.dll", "libpcre2-16-0.dll", "libzstd.dll",
        "zlib1.dll", "libfreetype-6.dll", "libpng16-16.dll",
        "libharfbuzz-0.dll", "libmd4c.dll",
    ]
    copied = 0
    for dll in dlls:
        src = qt / "bin" / dll
        if src.exists():
            shutil.copy2(src, build / dll)
            copied += 1
    if copied:
        ok(f"{copied} DLL essenziali copiate")

    # Plugin platforms/
    for plug_base in ["share/qt6/plugins", "plugins"]:
        p = qt / plug_base / "platforms"
        if p.exists():
            dst = build / "platforms"
            dst.mkdir(exist_ok=True)
            for f in p.glob("*.dll"):
                shutil.copy2(f, dst / f.name)
            break


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\n  [Interrotto]")
        sys.exit(1)

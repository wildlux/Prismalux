#!/usr/bin/env python3
"""
crea_tutti_zip.py — Rigenera tutti i pacchetti di distribuzione Prismalux.

Uso:
    python3 EXPORT/crea_tutti_zip.py           # tutti i ZIP
    python3 EXPORT/crea_tutti_zip.py --linux   # solo ZIP Linux
    python3 EXPORT/crea_tutti_zip.py --windows # solo ZIP Windows
    python3 EXPORT/crea_tutti_zip.py --appimage # solo AppImage (lento ~169MB)

Pacchetti generati:
    EXPORT/linux/Prismalux_v<VER>_Linux.zip              — AppImage + script install
    EXPORT/linux/Prismalux_v<VER>_Sorgenti_Linux.zip     — sorgenti complete
    EXPORT/linux/Prismalux_v<VER>_Sorgenti_Compila_Linux.zip — script compilazione
    EXPORT/windows/Prismalux_v<VER>_Windows.zip          — sorgenti Windows
    EXPORT/Prismalux_v<VER>_Sorgenti_Linux.zip           — copia in EXPORT/
    EXPORT/Prismalux_v<VER>_Windows.zip                  — copia in EXPORT/
"""

import zipfile, os, re, sys, shutil, subprocess
from pathlib import Path

ROOT = Path(__file__).parent.parent  # EXPORT/ → Prismalux/

# ── Versione da CMakeLists.txt ──────────────────────────────────
def _version():
    cmake = ROOT / "gui" / "CMakeLists.txt"
    if cmake.exists():
        for line in cmake.read_text().splitlines():
            m = re.search(r"project\(Prismalux_GUI VERSION\s+([0-9.]+)", line)
            if m:
                return m.group(1)
    return "2.9"

VERSION = _version()
print(f"Prismalux v{VERSION} — generazione pacchetti distribuzione\n")

# ── Pattern di esclusione globali ──────────────────────────────
GLOBAL_EXCLUDE = [
    r"[/\\]\.git([/\\]|$)",
    r"[/\\]__pycache__([/\\]|$)",
    r"[/\\]build_gui([/\\]|$)",
    r"[/\\]build([/\\]|$)",
    r"[/\\]build_",
    r"[/\\]AppDir([/\\]|$)",
    r"[/\\]models([/\\]|$)",
    r"[/\\]llama\.cpp([/\\]|$)",
    r"[/\\]toolchain([/\\]|$)",
    r"\.o$", r"\.a$", r"\.so(\.\d+)*$", r"\.pyc$", r"\.pyo$",
    r"\.AppImage$",
    r"Prismalux_v.*\.zip$",
    r"\.xcf$",
]

def _skip(rel: str, extra=None) -> bool:
    patterns = GLOBAL_EXCLUDE + (extra or [])
    return any(re.search(p, rel) for p in patterns)

def _add_dir(zf, src: Path, prefix: str, extra_exclude=None) -> int:
    count = 0
    for fpath in sorted(src.rglob("*")):
        if not fpath.is_file():
            continue
        rel = (prefix + "/" + str(fpath.relative_to(src))).replace("\\", "/")
        if _skip(rel, extra_exclude):
            continue
        zf.write(fpath, rel)
        count += 1
    return count

def _add_file(zf, src: Path, arcname: str = None) -> bool:
    if src.exists():
        zf.write(src, arcname or src.name)
        return True
    return False

def _make_zip(path: Path):
    path.parent.mkdir(parents=True, exist_ok=True)
    return zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=6)

def _report(out: Path, total: int):
    mb = out.stat().st_size / 1_048_576
    print(f"  → {out.name}  ({total} file, {mb:.1f} MB)")

# ══════════════════════════════════════════════════════════════
#  1. ZIP LINUX — AppImage + script installazione
# ══════════════════════════════════════════════════════════════
def zip_linux_appimage():
    out = ROOT / "EXPORT" / "linux" / f"Prismalux_v{VERSION}_Linux.zip"
    # Cerca AppImage nella root o in EXPORT/linux/
    appimage = ROOT / "Prismalux-x86_64.AppImage"
    if not appimage.exists():
        appimage = ROOT / "EXPORT" / "linux" / "Prismalux-x86_64.AppImage"
    if not appimage.exists():
        print(f"  [skip] AppImage non trovata")
        print("         Esegui prima: bash EXPORT/linux/crea_appimage.sh --no-build")
        return
    total = 0
    with _make_zip(out) as zf:
        _add_file(zf, appimage);                             total += 1
        _add_file(zf, ROOT / "EXPORT/linux/setup_dipendenze.sh");  total += 1
        _add_file(zf, ROOT / "EXPORT/linux/install_launcher.sh");  total += 1
        _add_file(zf, ROOT / "EXPORT/linux/uninstall.sh");         total += 1
        _add_file(zf, ROOT / "README.md");                          total += 1
        _add_file(zf, ROOT / "LICENSE");                            total += 1
    _report(out, total)

# ══════════════════════════════════════════════════════════════
#  2. ZIP SORGENTI LINUX — sorgenti complete
# ══════════════════════════════════════════════════════════════
def zip_sorgenti_linux():
    out = ROOT / "EXPORT" / "linux" / f"Prismalux_v{VERSION}_Sorgenti_Linux.zip"
    total = 0
    with _make_zip(out) as zf:
        for d, prefix in [
            (ROOT / "gui",    "gui"),
            (ROOT / "MCPs",   "MCPs"),
            (ROOT / "EXPORT", "EXPORT"),
        ]:
            if d.exists():
                extra = [r"[/\\]EXPORT[/\\]windows[/\\].*\.zip$"]
                n = _add_dir(zf, d, prefix, extra)
                print(f"    + {prefix}/  ({n})")
                total += n
        for f in ["README.md", "LICENSE", "Prismalux.desktop"]:
            if _add_file(zf, ROOT / f): total += 1
    _report(out, total)
    # Copia in EXPORT/
    shutil.copy2(out, ROOT / "EXPORT" / out.name)
    print(f"  → copia in EXPORT/{out.name}")

# ══════════════════════════════════════════════════════════════
#  3. ZIP SORGENTI COMPILA LINUX — script + essenziali compilazione
# ══════════════════════════════════════════════════════════════
def zip_sorgenti_compila_linux():
    out = ROOT / "EXPORT" / "linux" / f"Prismalux_v{VERSION}_Sorgenti_Compila_Linux.zip"
    total = 0
    with _make_zip(out) as zf:
        for f, arcname in [
            (ROOT / "README.md",    "README.md"),
            (ROOT / "LICENSE",      "LICENSE"),
            (ROOT / "Prismalux.desktop", "Prismalux.desktop"),
        ]:
            if _add_file(zf, f, arcname): total += 1

        for script in [
            "EXPORT/linux/setup_dipendenze.sh",
            "EXPORT/linux/crea_appimage.sh",
            "EXPORT/linux/install_launcher.sh",
            "EXPORT/linux/uninstall.sh",
            "EXPORT/linux/prismalux-server.service",
        ]:
            if _add_file(zf, ROOT / script, script): total += 1

        # TOOL_TIP/BEST_PRACTICE_&_GOAL (docs, TODO, regole)
        bp = ROOT / "TOOL_TIP" / "BEST_PRACTICE_&_GOAL"
        if bp.exists():
            n = _add_dir(zf, bp, "BEST_PRACTICE_&_GOAL")
            print(f"    + BEST_PRACTICE_&_GOAL/  ({n})")
            total += n

        # gui/ sorgenti (senza build)
        if (ROOT / "gui").exists():
            n = _add_dir(zf, ROOT / "gui", "gui")
            print(f"    + gui/  ({n})")
            total += n

    _report(out, total)

# ══════════════════════════════════════════════════════════════
#  4. ZIP WINDOWS — sorgenti per Windows
# ══════════════════════════════════════════════════════════════
def zip_windows():
    out = ROOT / "EXPORT" / "windows" / f"Prismalux_v{VERSION}_Windows.zip"
    total = 0
    win_extra = [
        r"[/\\]KNOWLEDGE_USER([/\\]|$)",  # dati personali
        r"[/\\]RAG([/\\]|$)",             # documenti RAG locali
        r"[/\\]SCREENSHOT([/\\]|$)",
        r"[/\\]IOS([/\\]|$)",
        r"[/\\]ANDROID([/\\]|$)",
        r"[/\\]Prismalux([/\\]|$)",       # binario Linux
    ]
    with _make_zip(out) as zf:
        for d, prefix in [
            (ROOT / "gui",       "gui"),
            (ROOT / "MCPs",      "MCPs"),
            (ROOT / "EXPORT",    "EXPORT"),
            (ROOT / "TOOL_TIP",  "TOOL_TIP"),
            (ROOT / "Frameworks","Frameworks"),
            (ROOT / "Tools",     "Tools"),
        ]:
            if d.exists():
                n = _add_dir(zf, d, prefix, win_extra)
                print(f"    + {prefix}/  ({n})")
                total += n
        for f, arcname in [
            (ROOT / "README.md",  "README.md"),
            (ROOT / "LICENSE",    "LICENSE"),
            (ROOT / "build.bat",  "build.bat"),
        ]:
            if _add_file(zf, f, arcname): total += 1
    _report(out, total)
    # Copia in EXPORT/
    shutil.copy2(out, ROOT / "EXPORT" / out.name)
    print(f"  → copia in EXPORT/{out.name}")

# ══════════════════════════════════════════════════════════════
#  5. AppImage (opzionale — lunga)
# ══════════════════════════════════════════════════════════════
def build_appimage():
    script = ROOT / "EXPORT" / "linux" / "crea_appimage.sh"
    if not script.exists():
        print("  [skip] crea_appimage.sh non trovato")
        return
    print("  Avvio crea_appimage.sh --no-build ...")
    result = subprocess.run(["bash", str(script), "--no-build"],
                            capture_output=False, text=True)
    if result.returncode != 0:
        print(f"  [ERRORE] crea_appimage.sh fallito (code {result.returncode})")
    else:
        ai = ROOT / "Prismalux-x86_64.AppImage"
        if ai.exists():
            mb = ai.stat().st_size / 1_048_576
            print(f"  → Prismalux-x86_64.AppImage ({mb:.0f} MB)")

# ══════════════════════════════════════════════════════════════
#  Main
# ══════════════════════════════════════════════════════════════
def main():
    args = set(sys.argv[1:])
    do_linux   = "--linux"   in args or not args
    do_windows = "--windows" in args or not args
    do_appimg  = "--appimage" in args

    if do_appimg:
        print("=== AppImage ===")
        build_appimage()

    if do_linux:
        print("=== ZIP Linux ===")
        print("1. Linux (AppImage + install):")
        zip_linux_appimage()
        print("2. Sorgenti Linux:")
        zip_sorgenti_linux()
        print("3. Sorgenti Compila Linux:")
        zip_sorgenti_compila_linux()

    if do_windows:
        print("=== ZIP Windows ===")
        print("4. Windows (sorgenti):")
        zip_windows()

    print("\nFatto.")

if __name__ == "__main__":
    main()

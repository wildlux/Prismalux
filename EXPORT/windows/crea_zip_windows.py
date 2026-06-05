#!/usr/bin/env python3
"""
crea_zip_windows.py — Pacchetto distribuibile Prismalux per Windows.

Uso:
    python3 crea_zip_windows.py              # auto-detect sorgenti o binario
    python3 crea_zip_windows.py --out foo.zip
    python3 crea_zip_windows.py --source     # forza ZIP sorgenti
    python3 crea_zip_windows.py --binary     # forza ZIP binario (serve .exe compilato)

Modalità SORGENTI (default senza .exe):
    Include sorgenti Qt6 C++ + build.bat → l'utente compila con build.bat.

Modalità BINARIO (auto se .exe trovato in gui/build_win/ o Tools/compile_win/build/):
    Include Prismalux_GUI.exe + DLL Qt + themes + dati → pronto all'uso.

Cosa esclude sempre:
    build/ Linux, .git/, __pycache__, *.o *.a *.so *.pyc
    models/ (GGUF pesanti), llama.cpp/ (repo git ~400 MB)
    toolchain/ (binari scaricati da setup.bat)
"""

import zipfile, os, re, sys, shutil
from pathlib import Path

ROOT = Path(__file__).parent.parent.parent   # Prismalux/ (EXPORT/windows/ → EXPORT/ → Prismalux/)

# ── Cerca il binario Windows compilato ─────────────────────────
def _find_win_binary():
    candidates = [
        ROOT / "build_cross_win" / "Prismalux_GUI.exe",   # cross-compilato da Linux
        ROOT / "gui" / "build_win" / "Prismalux_GUI.exe",
        ROOT / "Tools" / "compile_win" / "build" / "Prismalux_GUI.exe",
        ROOT / "build_win" / "Prismalux_GUI.exe",
        ROOT / "build" / "Prismalux_GUI.exe",
    ]
    for c in candidates:
        if c.exists() and c.stat().st_size > 100_000:
            return c
    return None

# ══════════════════════════════════════════════════════════════
#  Modalità SORGENTI
# ══════════════════════════════════════════════════════════════
SOURCE_DIRS = [
    "gui",
    "Tools/compile_win",
    "EXPORT/assets",
    "RAG",
    "MCPs",
    "TOOL_TIP/BEST_PRACTICE_&_GOAL",
    "Frameworks/whisper/bin_windows",
    "Tools/scripts",
]
SOURCE_ROOT_FILES = [
    "README.md",
    "LICENSE",
    "build.bat",
]
SOURCE_EXCLUDE = [
    r"[/\\]build[/\\]",         # qualsiasi build/
    r"[/\\]build$",
    r"[/\\]build_",             # build_asan/, build_debug/, build_tests/, build_win/ …
    r"[/\\]\.git[/\\]",
    r"[/\\]\.git$",
    r"[/\\]__pycache__[/\\]",
    r"[/\\]__pycache__$",
    r"[/\\]models[/\\]",
    r"[/\\]models$",
    r"[/\\]llama\.cpp[/\\]",
    r"[/\\]llama\.cpp$",
    r"\.o$", r"\.a$", r"\.so(\.\d+)*$", r"\.pyc$",
    r"\.AppImage$",
    r"\.xcf$",                  # Gimp sorgenti — non utili in distribuzione
    r"\.desktop$",              # collegamento Linux — illegale/inutile su Windows
    r"Prismalux_v.*\.zip$",
    r"Prismalux_Windows.*\.zip$",
    r"[/\\]toolchain[/\\]",     # toolchain portatile scaricata da setup.bat
    r"[/\\]toolchain$",
    r"Tools[/\\]compile_win[/\\]build[/\\]",  # Tools/compile_win/build/ (output compilazione)
    r"Tools[/\\]compile_win[/\\]build$",
]

# File singoli oltre questa soglia vengono saltati (0 = nessun limite)
MAX_FILE_MB = 0

# ══════════════════════════════════════════════════════════════
#  Modalità BINARIO
# ══════════════════════════════════════════════════════════════
BINARY_ROOT_FILES = [
    "README.md",
    "LICENSE",
    "build.bat",
]
# Cartelle dati da includere nel binario ZIP
BINARY_DATA_DIRS = [
    "RAG",
    "MCPs",
    "EXPORT/assets",
    "TOOL_TIP/BEST_PRACTICE_&_GOAL",
    "Frameworks/whisper/bin_windows",
    "TOOL_TIP/KNOWLEDGE_USER",
]
# Estensioni DLL/plugin da includere dalla cartella build
BINARY_BUILD_EXTS = {".exe", ".dll", ".pdb"}
BINARY_BUILD_SUBDIRS = {"platforms", "imageformats", "styles",
                        "tls", "iconengines", "networkinformation",
                        "wayland-decoration-client",
                        "wayland-graphics-integration-client",
                        "wayland-shell-integration"}


def _skip(rel: str, patterns) -> bool:
    return any(re.search(p, rel) for p in patterns)


def _add_dir(zf, src: Path, prefix: str, exclude=None, max_mb: float = 0) -> int:
    exclude = exclude or []
    count = 0
    skipped = []
    limit = max_mb * 1_048_576 if max_mb else 0
    for fpath in sorted(src.rglob("*")):
        if not fpath.is_file():
            continue
        rel = (prefix + "/" + str(fpath.relative_to(src))).replace("\\", "/")
        if _skip(rel, exclude):
            continue
        if limit and fpath.stat().st_size > limit:
            skipped.append(f"{rel} ({fpath.stat().st_size/1_048_576:.0f} MB)")
            continue
        zf.write(fpath, rel)
        count += 1
    for s in skipped:
        print(f"  [skip >{ max_mb:.0f}MB] {s}")
    return count


# ── ZIP sorgenti ────────────────────────────────────────────────
def crea_zip_source(out: Path):
    print(f"[SORGENTI] → {out.name}")
    total = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for d in SOURCE_DIRS:
            src = ROOT / d
            if not src.exists():
                print(f"  [skip] {d}/")
                continue
            n = _add_dir(zf, src, d, SOURCE_EXCLUDE, max_mb=MAX_FILE_MB)
            print(f"  + {d}/  ({n} file)")
            total += n
        for f in SOURCE_ROOT_FILES:
            fp = ROOT / f
            if fp.exists():
                zf.write(fp, f)
                print(f"  + {f}")
                total += 1
            else:
                print(f"  [skip] {f}")
    mb = out.stat().st_size / 1_048_576
    print(f"\nFatto! {total} file — {mb:.1f} MB → {out.name}")
    print("  Utente Windows: estrai → doppio clic su build.bat → doppio clic su Avvia_Prismalux.bat")


# ── ZIP binario ─────────────────────────────────────────────────
def crea_zip_binary(out: Path, exe_path: Path):
    build_dir = exe_path.parent
    print(f"[BINARIO] .exe: {exe_path}")
    print(f"          build dir: {build_dir}")
    print(f"          → {out.name}")
    total = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zf:

        # 1. Binario + DLL dalla cartella build (root)
        for f in sorted(build_dir.iterdir()):
            if not f.is_file():
                continue
            if f.suffix.lower() not in BINARY_BUILD_EXTS:
                continue
            zf.write(f, f.name)
            total += 1

        # 2. Sottocartelle plugin Qt (platforms/, imageformats/, ...)
        for sub in sorted(build_dir.iterdir()):
            if not sub.is_dir() or sub.name not in BINARY_BUILD_SUBDIRS:
                continue
            for f in sorted(sub.rglob("*")):
                if not f.is_file():
                    continue
                if f.suffix.lower() not in BINARY_BUILD_EXTS:
                    continue
                rel = str(f.relative_to(build_dir)).replace("\\", "/")
                zf.write(f, rel)
                total += 1

        # 3. Cartella themes/ accanto all'exe (copiata da CMake POST_BUILD)
        themes_src = build_dir / "themes"
        if themes_src.exists():
            n = _add_dir(zf, themes_src, "themes")
            print(f"  + themes/  ({n} .qss)")
            total += n
        else:
            print("  [warn] themes/ non trovata accanto all'exe")

        # 4. Cartelle dati
        for d in BINARY_DATA_DIRS:
            src = ROOT / d
            if not src.exists():
                continue
            exclude_data = [
                r"[/\\]\.git[/\\]", r"[/\\]__pycache__[/\\]",
                r"\.pyc$", r"\.so(\.\d+)*$",
            ]
            n = _add_dir(zf, src, d, exclude_data)
            if n:
                print(f"  + {d}/  ({n} file)")
                total += n

        # 5. File root
        for f in BINARY_ROOT_FILES:
            fp = ROOT / f
            if fp.exists():
                zf.write(fp, f)
                print(f"  + {f}")
                total += 1

    mb = out.stat().st_size / 1_048_576
    print(f"\nFatto! {total} file — {mb:.1f} MB → {out.name}")
    print("  Utente Windows: estrai → doppio clic su Avvia_Prismalux.bat")


def main():
    args = sys.argv[1:]
    out_name = None
    force_source  = "--source" in args
    force_binary  = "--binary" in args
    include_rag   = "--include-rag" in args   # passato da aggiorna.sh quando l'utente dice sì

    if "--out" in args:
        i = args.index("--out")
        if i + 1 < len(args):
            out_name = args[i + 1]

    # Versione da CMakeLists.txt
    cmake = ROOT / "gui" / "CMakeLists.txt"
    version = "2.8"
    if cmake.exists():
        for line in cmake.read_text().splitlines():
            m = re.search(r"project\(Prismalux_GUI VERSION\s+([0-9.]+)", line)
            if m:
                version = m.group(1)
                break

    exe = _find_win_binary()

    if force_binary and not exe:
        print("ERRORE: --binary richiede Prismalux_GUI.exe compilato.")
        print("  Compila prima su Windows con build.bat o aggiorna.bat.")
        sys.exit(1)

    # Se --include-rag: nessun limite dimensione file
    global MAX_FILE_MB
    if include_rag:
        MAX_FILE_MB = 0

    if (exe and not force_source) or force_binary:
        suffix = f"_v{version}_Windows_portable"
        if not out_name:
            out_name = f"Prismalux{suffix}.zip"
        crea_zip_binary(ROOT / out_name, exe)
    else:
        if not out_name:
            out_name = f"Prismalux_v{version}_Windows.zip"
        crea_zip_source(ROOT / out_name)


if __name__ == "__main__":
    main()

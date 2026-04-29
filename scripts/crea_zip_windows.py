#!/usr/bin/env python3
"""
crea_zip_windows.py — Pacchetto distribuibile Prismalux per Windows.

Uso:
    python3 crea_zip_windows.py              # → Prismalux_Windows_full.zip
    python3 crea_zip_windows.py --out foo.zip

Cosa include:
    gui/            ← sorgenti Qt6 C++ (no build/)
    ICONA/          ← icona applicazione
    RAG/            ← documenti RAG indicizzati
    MCPs/           ← plugin MCP
    whisper_win/    ← whisper-cli.exe Win64 (se presente)
    README.md  LICENSE  BEST_PRACTICE/

Cosa esclude:
    gui/build/          binari compilati Linux
    models/             GGUF pesanti
    llama.cpp/          repo git standalone (~400 MB)
    llama_cpp_studio/llama.cpp/build/
    __pycache__/  .git/
    *.o  *.a  *.so  *.pyc
    AppDir/  *.AppImage  esportazioni/  Test/  hybrid_llm/
"""

import zipfile, os, re, sys
from pathlib import Path

ROOT = Path(__file__).parent.parent   # Prismalux/

# ── Cartelle incluse (relative a ROOT, prefisso ZIP identico) ──────────────
INCLUDE_DIRS = [
    "gui",
    "ICONA",
    "RAG",
    "MCPs",
    "BEST_PRACTICE",
    "whisper_win",    # solo se esiste
    "scripts",
]

# ── File singoli dalla root ─────────────────────────────────────────────────
INCLUDE_ROOT_FILES = [
    "README.md",
    "LICENSE",
    "aggiorna.sh",
    "build.bat",    # build Windows — doppio clic per compilare
]

# ── Pattern da escludere (match su path relativo forward-slash) ─────────────
EXCLUDE_PATTERNS = [
    r"[/\\]build[/\\]",          # qualsiasi build/
    r"[/\\]build$",
    r"[/\\]\.git[/\\]",
    r"[/\\]\.git$",
    r"[/\\]__pycache__[/\\]",
    r"[/\\]__pycache__$",
    r"[/\\]models[/\\]",         # GGUF troppo grandi
    r"[/\\]models$",
    r"[/\\]llama\.cpp[/\\]",     # repo git standalone
    r"[/\\]llama\.cpp$",
    r"\.o$",
    r"\.a$",
    r"\.so(\.\d+)*$",
    r"\.pyc$",
    r"\.AppImage$",
    r"Prismalux_Windows.*\.zip$",
]

_EXCLUDE_RE = [re.compile(p) for p in EXCLUDE_PATTERNS]

def _skip(rel: str) -> bool:
    return any(rx.search(rel) for rx in _EXCLUDE_RE)

def _add_dir(zf, src: Path, prefix: str) -> int:
    count = 0
    for fpath in sorted(src.rglob("*")):
        if not fpath.is_file():
            continue
        rel = (prefix + "/" + str(fpath.relative_to(src))).replace("\\", "/")
        if _skip(rel):
            continue
        zf.write(fpath, rel)
        count += 1
    return count

def crea_zip(out: Path):
    print(f"ZIP: {out}")
    total = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for d in INCLUDE_DIRS:
            src = ROOT / d
            if not src.exists():
                print(f"  [skip] {d}/ non trovata")
                continue
            n = _add_dir(zf, src, d)
            print(f"  + {d}/  ({n} file)")
            total += n

        for f in INCLUDE_ROOT_FILES:
            fp = ROOT / f
            if fp.exists():
                zf.write(fp, f)
                print(f"  + {f}")
                total += 1
            else:
                print(f"  [skip] {f} non trovato")

    mb = out.stat().st_size / 1_048_576
    print(f"\nFatto! {total} file — {mb:.1f} MB → {out.name}")

def main():
    out_name = "Prismalux_Windows_full.zip"
    args = sys.argv[1:]
    if "--out" in args:
        i = args.index("--out")
        if i + 1 < len(args):
            out_name = args[i + 1]
    crea_zip(ROOT / out_name)

if __name__ == "__main__":
    main()

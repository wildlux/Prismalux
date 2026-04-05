#!/usr/bin/env python3
"""
crea_zip_windows.py — Esporta Prismalux C in un ZIP pronto per Windows.

Uso:
    python3 crea_zip_windows.py              # sovrascrive sempre Prismalux_Windows_full.zip
    python3 crea_zip_windows.py --out foo.zip  # nome personalizzato (raro)

Cosa include (solo C):
    C_software/src/               ← sorgenti .c / .cpp (incluso llama_wrapper.cpp)
    C_software/include/           ← header .h
    C_software/Qt_GUI/            ← sorgenti GUI Qt6 (no build/)
    C_software/rag_docs/          ← documenti RAG (730, piva)
    C_software/whisper_win/       ← whisper-cli.exe precompilato Win64 (se presente)
    C_software/test_*.c           ← sorgenti test nella root
    C_software/jlt_ollama_speed_test.c
    C_software/golden_prompts.json
    C_software/Makefile
    C_software/build.bat
    C_software/build.sh
    C_software/README.md
    C_software/README_WINDOWS.md
    C_software/CLAUDE.md
    C_software/scarica_modelli_whisper.bat  ← scarica modello GGML su Windows

Cosa ESCLUDE:
    *.o  *.a  *.so              (oggetti Linux)
    llama.cpp/                  (repo git enorme, ~400 MB)
    models/                     (GGUF pesanti)
    Qt_GUI/build/               (binari Qt compilati)
    __pycache__/  .git/
    test_cs  test_cs_random  prismalux  (binari Linux)

Nota whisper_win/:
    Viene creata da aggiorna.sh --whisper prima di chiamare questo script.
    Contiene solo whisper-cli.exe (binario Win64 precompilato da GitHub Releases).
    Se la cartella è assente lo script procede normalmente senza errori.
"""

import zipfile
import os
import sys
import re
from pathlib import Path

# ── Configurazione ──────────────────────────────────────────────────────────
ROOT         = Path(__file__).parent          # ~/Desktop/Prismalux/
C_SW         = ROOT / "C_software"            # cartella sorgente C

# Cartelle da includere (relative a C_SW)
INCLUDE_DIRS = [
    "src",
    "include",
    "Qt_GUI",
    "rag_docs",
    "whisper_win",   # whisper-cli.exe Win64 — creata da aggiorna.sh, skip se assente
]

# File singoli da includere (relativi a C_SW)
INCLUDE_FILES = [
    "Makefile",
    "Avvia_Prismalux.bat",
    "build.bat",
    "build_qt.bat",
    "build.sh",
    "README.md",
    "README_WINDOWS.md",
    "CLAUDE.md",
    "Prismalux_GUI.desktop",
    "scarica_modelli_whisper.bat",   # helper per scaricare modelli GGML su Windows
    "golden_prompts.json",           # prompt predefiniti multi-agente
]

# Pattern da ESCLUDERE (match su path relativo con forward slash)
EXCLUDE_PATTERNS = [
    r"\.o$",
    r"\.a$",
    r"\.so(\.\d+)*$",
    # Nota: .exe NON è escluso globalmente — whisper_win/whisper-cli.exe deve entrare.
    # Escludi solo i .exe Linux-incompatibili nella root C_software:
    r"^C_software[/\\][^/\\]+\.exe$",   # .exe nella root (es. build artifacts MinGW)
    r"[/\\]__pycache__[/\\]",
    r"[/\\]\.git[/\\]",
    r"[/\\]llama\.cpp[/\\]",
    r"[/\\]models[/\\]",
    r"[/\\]Qt_GUI[/\\]build[/\\]",
    r"[/\\]Qt_GUI[/\\]build$",
    # binari Linux senza estensione nella root C_software
    r"^C_software[/\\](prismalux|test_cs|test_cs_random|test_engine|download_model)$",
]

_EXCLUDE_RE = [re.compile(p) for p in EXCLUDE_PATTERNS]


def _escludi(rel_path: str) -> bool:
    """True se il percorso relativo va escluso."""
    return any(rx.search(rel_path) for rx in _EXCLUDE_RE)


def _aggiungi_dir(zf: zipfile.ZipFile, src_dir: Path, zip_prefix: str) -> int:
    """Aggiunge ricorsivamente src_dir nello ZIP sotto zip_prefix/."""
    count = 0
    for fpath in sorted(src_dir.rglob("*")):
        if not fpath.is_file():
            continue
        rel = str(Path(zip_prefix) / fpath.relative_to(src_dir))
        rel = rel.replace("\\", "/")
        full_rel = rel  # per il filtro
        if _escludi(full_rel):
            continue
        zf.write(fpath, rel)
        count += 1
    return count


def _aggiungi_file(zf: zipfile.ZipFile, fpath: Path, zip_name: str) -> bool:
    if not fpath.exists():
        print(f"  [SKIP] non trovato: {fpath.name}")
        return False
    if _escludi(zip_name):
        return False
    zf.write(fpath, zip_name)
    return True


def crea_zip(out_path: Path) -> None:
    print(f"Creazione ZIP: {out_path}")
    print(f"Sorgente:      {C_SW}\n")

    total = 0
    with zipfile.ZipFile(out_path, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=6) as zf:

        # Cartelle
        for d in INCLUDE_DIRS:
            src = C_SW / d
            if not src.exists():
                print(f"  [SKIP dir] {d}/ non esiste")
                continue
            prefix = f"C_software/{d}"
            n = _aggiungi_dir(zf, src, prefix)
            print(f"  + {d}/  ({n} file)")
            total += n

        # File singoli
        for f in INCLUDE_FILES:
            src = C_SW / f
            zip_name = f"C_software/{f}"
            if _aggiungi_file(zf, src, zip_name):
                print(f"  + {f}")
                total += 1

        # Sorgenti .c / .cpp nella root di C_software (test_*.c, jlt_*.c, ecc.)
        root_c = sorted(C_SW.glob("*.c")) + sorted(C_SW.glob("*.cpp"))
        root_c_added = 0
        for fpath in root_c:
            zip_name = f"C_software/{fpath.name}"
            if _aggiungi_file(zf, fpath, zip_name):
                root_c_added += 1
        if root_c_added:
            print(f"  + *.c/*.cpp nella root/  ({root_c_added} file)")
        total += root_c_added

    size_mb = out_path.stat().st_size / 1_048_576
    print(f"\nFatto! {total} file — {size_mb:.1f} MB → {out_path.name}")


def main():
    # Parsing argomenti minimale
    out_name = None
    args = sys.argv[1:]
    if "--out" in args:
        idx = args.index("--out")
        if idx + 1 < len(args):
            out_name = args[idx + 1]

    if out_name is None:
        out_name = "Prismalux_Windows_full.zip"   # nome fisso, sovrascrive il precedente

    out_path = ROOT / out_name
    crea_zip(out_path)


if __name__ == "__main__":
    main()

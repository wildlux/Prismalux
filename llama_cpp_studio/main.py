"""
llama.cpp Studio — Compila, gestisci e avvia LLM locali
=========================================================
Menu:
  1. Compila llama.cpp  (rileva hardware, genera flag CMake ottimali, compila)
  2. Gestisci modelli GGUF (trova .gguf, imposta default)
  3. Avvia modello (chat interattiva o server API su :8080)
  4. Info hardware (CPU, SIMD, GPU, RAM rilevati)
  0. Torna al menu Prismalux
  Q. Esci
"""

import os
import sys
import json
import shutil
import subprocess
import time
from pathlib import Path

# ── Percorsi ──────────────────────────────────────────────────────────────────

ROOT_STUDIO = os.path.dirname(os.path.abspath(__file__))
DIR_LLAMA   = os.path.join(ROOT_STUDIO, "llama.cpp")
DIR_BUILD   = os.path.join(DIR_LLAMA, "build")
DIR_MODELS  = os.path.join(ROOT_STUDIO, "models")
FILE_CONFIG = os.path.join(ROOT_STUDIO, "config.json")
REPO_URL    = "https://github.com/ggerganov/llama.cpp.git"

os.makedirs(DIR_MODELS, exist_ok=True)

# ── Rich ─────────────────────────────────────────────────────────────────────

try:
    from rich.console import Console
    from rich.panel import Panel
    from rich.table import Table
    from rich.text import Text
    from rich.rule import Rule
    _RICH = True
except ImportError:
    _RICH = False

# Importa utilità Prismalux se disponibili
_ROOT_PRISMALUX = os.path.dirname(ROOT_STUDIO)
if _ROOT_PRISMALUX not in sys.path:
    sys.path.insert(0, _ROOT_PRISMALUX)

try:
    from check_deps import stampa_breadcrumb, stampa_header
    _HAS_CHECKDEPS = True
except ImportError:
    _HAS_CHECKDEPS = False
    def stampa_breadcrumb(con, p): pass
    def stampa_header(con): pass


def _nuova_console() -> "Console":
    cols, _ = shutil.get_terminal_size(fallback=(120, 40))
    return Console(width=cols)


# ── Configurazione JSON ───────────────────────────────────────────────────────

def _carica_config() -> dict:
    if os.path.exists(FILE_CONFIG):
        try:
            with open(FILE_CONFIG, encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return {
        "compilato": False,
        "cmake_flags": [],
        "hardware": {},
        "modello_default": None,
        "n_threads": None,
        "n_gpu_layers": 0,
        "ctx_size": 4096,
        "binari": [],
    }


def _salva_config(cfg: dict) -> None:
    with open(FILE_CONFIG, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2, ensure_ascii=False)


# ── Rilevamento hardware ──────────────────────────────────────────────────────

def _rileva_hardware() -> dict:
    import platform
    hw = {
        "cpu_nome":         "Sconosciuto",
        "cpu_cores_fisici": os.cpu_count() or 1,
        "cpu_threads":      os.cpu_count() or 1,
        "ram_gb":           0.0,
        "arch":             platform.machine(),
        # SIMD x86
        "avx":    False,
        "avx2":   False,
        "avx512": False,
        "fma":    False,
        "f16c":   False,
        # ARM
        "neon":   False,
        # GPU
        "cuda":               False,
        "gpu_nvidia":         None,
        "gpu_nvidia_vram_gb": 0.0,
        "rocm":               False,
        "gpu_amd":            None,
        "vulkan":             False,
        "metal":              False,
    }

    # ── CPU (Linux) ───────────────────────────────────────────────────────
    if sys.platform == "linux":
        try:
            with open("/proc/cpuinfo", encoding="utf-8") as f:
                content = f.read()
            for riga in content.splitlines():
                if riga.startswith("model name") and "Sconosciuto" in hw["cpu_nome"]:
                    hw["cpu_nome"] = riga.split(":", 1)[1].strip()
                if riga.startswith("flags") or riga.startswith("Features"):
                    fl = set(riga.split())
                    hw["avx"]    = "avx"     in fl
                    hw["avx2"]   = "avx2"    in fl
                    hw["avx512"] = "avx512f" in fl
                    hw["fma"]    = "fma"     in fl
                    hw["f16c"]   = "f16c"    in fl
                    hw["neon"]   = "neon"    in fl or "asimd" in fl
        except Exception:
            pass
        # Core fisici (lscpu)
        try:
            r = subprocess.run(
                ["lscpu", "-p=Core,Socket"],
                capture_output=True, text=True, timeout=4
            )
            if r.returncode == 0:
                linee = [l for l in r.stdout.splitlines() if not l.startswith("#") and l.strip()]
                hw["cpu_cores_fisici"] = len(set(linee))
        except Exception:
            hw["cpu_cores_fisici"] = max(1, (os.cpu_count() or 2) // 2)

    # ── CPU (macOS) ───────────────────────────────────────────────────────
    elif sys.platform == "darwin":
        hw["metal"] = True
        try:
            r = subprocess.run(
                ["sysctl", "-n", "machdep.cpu.brand_string"],
                capture_output=True, text=True, timeout=3
            )
            if r.returncode == 0:
                hw["cpu_nome"] = r.stdout.strip()
        except Exception:
            pass

    # ── CPU + SIMD (Windows) ──────────────────────────────────────────────
    elif sys.platform == "win32":
        # Nome CPU via wmic
        try:
            r = subprocess.run(
                ["wmic", "cpu", "get", "Name", "/value"],
                capture_output=True, text=True, timeout=5
            )
            if r.returncode == 0:
                for riga in r.stdout.splitlines():
                    if riga.startswith("Name=") and riga[5:].strip():
                        hw["cpu_nome"] = riga[5:].strip()
                        break
        except Exception:
            try:
                import platform as _plat
                nome = _plat.processor()
                if nome:
                    hw["cpu_nome"] = nome
            except Exception:
                pass
        # Core fisici
        try:
            r = subprocess.run(
                ["wmic", "cpu", "get", "NumberOfCores", "/value"],
                capture_output=True, text=True, timeout=5
            )
            if r.returncode == 0:
                for riga in r.stdout.splitlines():
                    if riga.startswith("NumberOfCores="):
                        val = riga.split("=", 1)[1].strip()
                        if val.isdigit():
                            hw["cpu_cores_fisici"] = int(val)
                            break
        except Exception:
            hw["cpu_cores_fisici"] = max(1, (os.cpu_count() or 2) // 2)
        # SIMD via IsProcessorFeaturePresent (Windows 10/11)
        # SIMD = istruzioni matematiche vettoriali della CPU (AVX, SSE, FMA)
        try:
            import ctypes
            _k32 = ctypes.windll.kernel32
            hw["avx"]    = bool(_k32.IsProcessorFeaturePresent(39))  # PF_AVX
            hw["avx2"]   = bool(_k32.IsProcessorFeaturePresent(40))  # PF_AVX2
            hw["avx512"] = bool(_k32.IsProcessorFeaturePresent(44))  # PF_AVX512F
            hw["fma"]    = bool(_k32.IsProcessorFeaturePresent(42))  # PF_FMA3
            hw["f16c"]   = bool(_k32.IsProcessorFeaturePresent(43))  # PF_F16C
        except Exception:
            pass

    # ── RAM ───────────────────────────────────────────────────────────────
    try:
        with open("/proc/meminfo", encoding="utf-8") as f:
            for riga in f:
                if riga.startswith("MemTotal"):
                    hw["ram_gb"] = int(riga.split()[1]) / 1024 / 1024
                    break
    except Exception:
        pass

    # RAM fallback Windows (ctypes → GlobalMemoryStatusEx)
    if hw["ram_gb"] == 0.0 and sys.platform == "win32":
        try:
            import ctypes
            class _MemStatus(ctypes.Structure):
                _fields_ = [
                    ("dwLength",                ctypes.c_ulong),
                    ("dwMemoryLoad",             ctypes.c_ulong),
                    ("ullTotalPhys",             ctypes.c_ulonglong),
                    ("ullAvailPhys",             ctypes.c_ulonglong),
                    ("ullTotalPageFile",         ctypes.c_ulonglong),
                    ("ullAvailPageFile",         ctypes.c_ulonglong),
                    ("ullTotalVirtual",          ctypes.c_ulonglong),
                    ("ullAvailVirtual",          ctypes.c_ulonglong),
                    ("sullAvailExtendedVirtual", ctypes.c_ulonglong),
                ]
            _s = _MemStatus()
            _s.dwLength = ctypes.sizeof(_s)
            ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(_s))
            hw["ram_gb"] = _s.ullTotalPhys / (1024 ** 3)
        except Exception:
            pass

    # ── NVIDIA CUDA ───────────────────────────────────────────────────────
    try:
        r = subprocess.run(
            ["nvidia-smi", "--query-gpu=name,memory.total",
             "--format=csv,noheader,nounits"],
            capture_output=True, text=True, timeout=5
        )
        if r.returncode == 0 and r.stdout.strip():
            parti = r.stdout.strip().splitlines()[0].split(",")
            hw["gpu_nvidia"]         = parti[0].strip()
            hw["gpu_nvidia_vram_gb"] = float(parti[1].strip()) / 1024
            hw["cuda"]               = True
    except Exception:
        pass

    # ── AMD ROCm ──────────────────────────────────────────────────────────
    if not hw["cuda"]:
        try:
            r = subprocess.run(["rocminfo"], capture_output=True, text=True, timeout=5)
            if r.returncode == 0 and "GPU" in r.stdout:
                hw["gpu_amd"] = "AMD GPU (ROCm)"
                hw["rocm"]    = True
        except Exception:
            pass

    # ── Vulkan ────────────────────────────────────────────────────────────
    if not hw["cuda"] and not hw["rocm"]:
        try:
            r = subprocess.run(
                ["vulkaninfo", "--summary"],
                capture_output=True, text=True, timeout=5
            )
            if r.returncode == 0 and "GPU" in r.stdout:
                hw["vulkan"] = True
        except Exception:
            pass

    return hw


def _cmake_flags(hw: dict) -> list:
    """Flag CMake ottimali per l'hardware rilevato."""
    flags = [
        "-DCMAKE_BUILD_TYPE=Release",
        "-DGGML_NATIVE=ON",   # -march=native: abilita AVX2/AVX-512/FMA automaticamente
    ]
    if hw.get("cuda"):
        flags.append("-DGGML_CUDA=ON")
    if hw.get("rocm"):
        flags.append("-DGGML_HIP=ON")
    if hw.get("metal"):
        flags.append("-DGGML_METAL=ON")
    if hw.get("vulkan") and not hw.get("cuda") and not hw.get("rocm"):
        flags.append("-DGGML_VULKAN=ON")
    return flags


def _n_threads_ottimale(hw: dict) -> int:
    cores = hw.get("cpu_cores_fisici", 1)
    return max(1, cores - 1) if cores > 2 else cores


def _n_gpu_layers_ottimale(hw: dict, size_gb: float = 4.0) -> int:
    vram = hw.get("gpu_nvidia_vram_gb", 0.0)
    if vram <= 0:
        return 0
    if vram >= size_gb * 1.15:
        return 999   # tutto in GPU (llama.cpp usa 999 = "tutte le layer")
    # stima proporzionale: ~35 layer per modello 7B
    return max(1, int((vram / size_gb) * 35))


# ── Schermata: Info hardware ──────────────────────────────────────────────────

def mostra_hardware(con: "Console", hw: dict) -> None:
    os.system("cls" if sys.platform == "win32" else "clear")
    stampa_header(con)
    stampa_breadcrumb(con, "🍺 Prismalux › ⚙️ llama.cpp Studio › ℹ️ Hardware")

    t = Text()
    t.append("  💻 CPU\n", style="bold cyan")
    t.append(f"     Nome          : {hw['cpu_nome']}\n",       style="white")
    t.append(f"     Core fisici   : {hw['cpu_cores_fisici']}\n", style="white")
    t.append(f"     Thread totali : {hw['cpu_threads']}\n",    style="white")
    t.append(f"     Architettura  : {hw['arch']}\n\n",         style="white")

    # SIMD
    t.append("  ⚡ Istruzioni SIMD\n", style="bold cyan")
    for flag, nome in [("avx","AVX"), ("avx2","AVX2"), ("avx512","AVX-512"),
                       ("fma","FMA"), ("f16c","F16C"), ("neon","NEON")]:
        ok = hw.get(flag, False)
        t.append(f"     {'✓' if ok else '✗'}  {nome}\n",
                 style="green" if ok else "dim")

    t.append(f"\n  🧠 RAM totale : {hw['ram_gb']:.1f} GB\n\n", style="bold cyan")

    t.append("  🎮 GPU\n", style="bold cyan")
    if hw.get("cuda"):
        t.append(f"     NVIDIA : {hw['gpu_nvidia']} ({hw['gpu_nvidia_vram_gb']:.1f} GB VRAM)\n", style="green")
        t.append( "     CUDA   : ✓ disponibile\n", style="green")
    elif hw.get("rocm"):
        t.append(f"     AMD    : {hw['gpu_amd']}\n", style="green")
        t.append( "     ROCm   : ✓ disponibile\n",   style="green")
    elif hw.get("metal"):
        t.append( "     Apple Metal : ✓ disponibile\n", style="green")
    else:
        t.append( "     Nessuna GPU accelerata rilevata → inference solo CPU\n", style="dim")
    if hw.get("vulkan"):
        t.append( "     Vulkan : ✓ disponibile\n", style="yellow")

    t.append("\n  [INVIO per tornare]", style="dim")
    con.print(Panel(t, title="[bold blue]ℹ️  Hardware rilevato[/]",
                    border_style="blue", padding=(1, 2)))
    input("")


# ── Compilazione llama.cpp ───────────────────────────────────────────────────

def compila(con: "Console", cfg: dict) -> dict:
    os.system("cls" if sys.platform == "win32" else "clear")
    stampa_header(con)
    stampa_breadcrumb(con, "🍺 Prismalux › ⚙️ llama.cpp Studio › 🔨 Compilazione")

    # ── MSYS2: aggiunge i suoi bin al PATH così shutil.which() li trova ───
    if sys.platform == "win32":
        for _ms in [r"C:\msys64", r"C:\msys32"]:
            if os.path.isdir(_ms):
                _extra = os.pathsep.join([
                    os.path.join(_ms, "mingw64", "bin"),
                    os.path.join(_ms, "usr",    "bin"),
                ])
                os.environ["PATH"] = _extra + os.pathsep + os.environ.get("PATH", "")
                break

    # ── Verifica strumenti di build obbligatori ────────────────────────────
    TOOLS_REQUIRED = [
        ("git",   "git"),
        ("cmake", "cmake"),
        ("make",  "make"),
        ("gcc",   "build-essential"),
    ]
    mancanti = [(nome, pkg) for nome, pkg in TOOLS_REQUIRED if not shutil.which(nome)]
    if mancanti:
        con.print("\n  [bold red]❌ Strumenti di build mancanti:[/]")
        for nome, _ in mancanti:
            con.print(f"     · {nome}", style="red")
        if sys.platform == "win32":
            msys2_root = next(
                (p for p in [r"C:\msys64", r"C:\msys32"] if os.path.isdir(p)), None
            )
            if msys2_root:
                con.print(f"\n  [green]✓ MSYS2 trovato:[/] {msys2_root}")
                con.print("\n  Apri [bold]MSYS2 MINGW64[/] (cerca nel menu Start)")
                con.print("  ed esegui:")
                con.print("  [cyan]  pacman -S --needed git cmake make mingw-w64-x86_64-gcc[/]")
                con.print("\n  [dim]Poi torna qui e ripremi 1 per riprovare.[/]")
                con.print("\n  Apro MSYS2 MINGW64 adesso? [S/n] ", end="")
                if input("").strip().lower() in ("s", "si", "sì", ""):
                    try:
                        subprocess.Popen(
                            [os.path.join(msys2_root, "msys2_shell.cmd"), "-mingw64"],
                            creationflags=getattr(subprocess, "CREATE_NEW_CONSOLE", 0),
                        )
                        con.print("  [green]✓ MSYS2 aperto. Esegui il comando pacman, poi ripremi 1.[/]")
                    except Exception as _e:
                        con.print(f"  [yellow]Non riesco ad aprire MSYS2: {_e}[/]")
                        con.print(f"  [dim]  Aprilo manualmente da: {msys2_root}[/]")
            else:
                con.print("\n  [yellow]MSYS2 non trovato.[/] Scaricalo da https://www.msys2.org/")
                con.print("  poi apri MSYS2 MINGW64 e digita:")
                con.print("  [cyan]  pacman -S --needed git cmake make mingw-w64-x86_64-gcc[/]")
                con.print("\n  In alternativa con Chocolatey:")
                con.print("  [cyan]  choco install git cmake mingw[/]")
        else:
            pkgs = " ".join(pkg for _, pkg in mancanti)
            con.print(f"\n  Installa con:\n  [cyan]  sudo apt install {pkgs}[/]")
        input("\n  [INVIO per tornare] ")
        return cfg

    # ── Rilevamento hardware ───────────────────────────────────────────────
    con.print("\n  [dim]🔍 Rilevamento hardware in corso...[/]")
    hw = _rileva_hardware()
    cfg["hardware"] = hw

    flags   = _cmake_flags(hw)
    threads = _n_threads_ottimale(hw)

    # ── Pannello riepilogo hardware + flag ────────────────────────────────
    t = Text()
    t.append("  🖥  CPU    : ", style="dim")
    t.append(f"{hw['cpu_nome']}\n", style="white")
    t.append("  🧵 Thread  : ", style="dim")
    t.append(f"{hw['cpu_cores_fisici']} core fisici → userò {threads} thread in inference\n", style="white")

    simd = [n for f, n in [("avx2","AVX2"),("avx512","AVX-512"),("fma","FMA"),
                            ("f16c","F16C"),("neon","NEON")] if hw.get(f)]
    t.append("  ⚡ SIMD    : ", style="dim")
    t.append(f"{', '.join(simd) if simd else 'nessuna rilevata'}\n",
             style="green" if simd else "yellow")

    t.append("  🎮 GPU     : ", style="dim")
    if hw.get("cuda"):
        t.append(f"{hw['gpu_nvidia']} ({hw['gpu_nvidia_vram_gb']:.1f}GB VRAM) — CUDA ON\n", style="green")
    elif hw.get("metal"):
        t.append("Apple Metal ON\n", style="green")
    elif hw.get("rocm"):
        t.append("AMD ROCm/HIP ON\n", style="green")
    elif hw.get("vulkan"):
        t.append("Vulkan ON\n", style="yellow")
    else:
        t.append("solo CPU\n", style="dim")

    t.append("\n  📋 Flag CMake ottimali:\n", style="bold")
    for fl in flags:
        t.append(f"     {fl}\n", style="cyan")
    t.append(f"     -j{os.cpu_count()} (build parallela)\n", style="cyan")

    con.print(Panel(t, title="[bold yellow]⚙️ Configurazione rilevata per questa macchina[/]",
                    border_style="yellow", padding=(1, 2)))

    # ── Conferma ──────────────────────────────────────────────────────────
    con.print("\n  Procedere con la compilazione? [S/n] ", end="")
    if input("").strip().lower() in ("n", "no"):
        return cfg

    # ── Clone o aggiornamento ─────────────────────────────────────────────
    if os.path.exists(DIR_LLAMA):
        con.print("\n  [cyan]📥 llama.cpp già presente → git pull (aggiornamento)...[/]")
        r = subprocess.run(
            ["git", "-C", DIR_LLAMA, "pull", "--ff-only"],
            capture_output=True, text=True
        )
        if r.returncode != 0:
            con.print(f"  [yellow]⚠  git pull non riuscito: {r.stderr.strip()[:120]}[/]")
            con.print("  Continuo con la versione attuale già presente.")
        else:
            con.print("  [green]✓ Repo aggiornato.[/]")
    else:
        con.print(f"\n  [cyan]📥 Clono llama.cpp (shallow clone, solo ultima versione)...[/]")
        con.print(f"  [dim]{REPO_URL}[/]")
        r = subprocess.run(
            ["git", "clone", "--depth=1", REPO_URL, DIR_LLAMA]
        )
        if r.returncode != 0:
            con.print("\n  [red]❌ Clone fallito. Controlla la connessione internet.[/]")
            input("\n  [INVIO per tornare] ")
            return cfg
        con.print("  [green]✓ Clone completato.[/]")

    # ── cmake configure ───────────────────────────────────────────────────
    os.makedirs(DIR_BUILD, exist_ok=True)
    cmd_cfg = ["cmake", "-B", DIR_BUILD, "-S", DIR_LLAMA] + flags
    con.print(f"\n  [cyan]⚙️  cmake configure...[/]")
    con.print(f"  [dim]{' '.join(cmd_cfg[:6])} ...[/]\n")
    r = subprocess.run(cmd_cfg, cwd=DIR_LLAMA)
    if r.returncode != 0:
        con.print("\n  [red]❌ cmake configure fallito. Leggi i messaggi di errore sopra.[/]")
        input("\n  [INVIO per tornare] ")
        return cfg

    # ── cmake build ───────────────────────────────────────────────────────
    nproc = str(os.cpu_count() or 4)
    cmd_build = ["cmake", "--build", DIR_BUILD, "--config", "Release", "-j", nproc]
    con.print(f"\n  [cyan]🔨 Compilazione in corso (possono volerci diversi minuti)...[/]")
    con.print(f"  [dim]{' '.join(cmd_build)}[/]\n")
    r = subprocess.run(cmd_build, cwd=DIR_BUILD)
    if r.returncode != 0:
        con.print("\n  [red]❌ Compilazione fallita. Leggi i messaggi di errore sopra.[/]")
        input("\n  [INVIO per tornare] ")
        return cfg

    # ── Verifica binari ────────────────────────────────────────────────────
    bin_dir = os.path.join(DIR_BUILD, "bin")
    NOMI_BINARI = ["llama-cli", "llama-server", "main"]   # 'main' = vecchie versioni

    con.print("\n  [cyan]🔍 Verifica binari compilati...[/]")
    binari_ok = []
    for nome in NOMI_BINARI:
        for cartella in [bin_dir, DIR_BUILD]:
            p = os.path.join(cartella, nome)
            if os.path.isfile(p) and os.access(p, os.X_OK):
                try:
                    subprocess.run([p, "--version"], capture_output=True, timeout=5)
                except Exception:
                    pass
                binari_ok.append(nome)
                con.print(f"  [green]  ✓ {nome}[/]")
                break

    if not binari_ok:
        con.print(f"  [red]❌ Nessun binario trovato in {bin_dir}[/]")
        con.print("  La compilazione potrebbe essere parzialmente riuscita.")
        input("\n  [INVIO per tornare] ")
        return cfg

    # ── Salva configurazione ───────────────────────────────────────────────
    cfg.update({
        "compilato":   True,
        "cmake_flags": flags,
        "hardware":    hw,
        "n_threads":   threads,
        "binari":      binari_ok,
    })
    _salva_config(cfg)

    gpu_info = "CUDA ON" if hw.get("cuda") else ("Metal ON" if hw.get("metal") else "solo CPU")
    con.print(Panel(
        f"  [bold green]✅ Compilazione completata con successo![/]\n\n"
        f"  Binari: [cyan]{', '.join(binari_ok)}[/]\n"
        f"  Thread inference: {threads}\n"
        f"  GPU: {gpu_info}",
        title="[bold green]🎉 Compilazione riuscita[/]",
        border_style="green", padding=(1, 2)
    ))
    input("\n  [INVIO per continuare] ")
    return cfg


# ── Gestione modelli GGUF ────────────────────────────────────────────────────

def _cerca_gguf() -> list:
    """Cerca file .gguf in models/ e nelle cartelle comuni dell'utente."""
    trovati = []
    cartelle = [
        DIR_MODELS,
        Path.home(),
        Path.home() / "Downloads",
        Path.home() / "Modelli",
        Path.home() / ".cache" / "huggingface" / "hub",
    ]
    for cartella in cartelle:
        try:
            for f in Path(cartella).rglob("*.gguf"):
                if f.is_file():
                    trovati.append({
                        "nome":     f.name,
                        "percorso": str(f),
                        "size_gb":  f.stat().st_size / (1024 ** 3),
                    })
        except PermissionError:
            pass

    # dedup per percorso
    seen, unici = set(), []
    for m in trovati:
        if m["percorso"] not in seen:
            seen.add(m["percorso"])
            unici.append(m)
    return sorted(unici, key=lambda x: x["size_gb"])


def gestisci_modelli(con: "Console", cfg: dict) -> dict:
    while True:
        os.system("cls" if sys.platform == "win32" else "clear")
        stampa_header(con)
        stampa_breadcrumb(con, "🍺 Prismalux › ⚙️ llama.cpp Studio › 📂 Modelli")

        modelli = _cerca_gguf()
        t = Text()
        t.append(f"  Cartella principale: {DIR_MODELS}\n\n", style="dim")

        if not modelli:
            t.append("  Nessun file .gguf trovato.\n\n", style="yellow")
            t.append("  Copia i tuoi modelli .gguf in:\n", style="dim")
            t.append(f"  {DIR_MODELS}\n\n", style="cyan")
        else:
            t.append(f"  {'#':>3}  {'Dim.':>7}  Modello\n", style="bold dim")
            t.append(f"  {'─' * 60}\n", style="dim")
            for i, m in enumerate(modelli, 1):
                default = cfg.get("modello_default") == m["percorso"]
                t.append(f"  {i:>3}  {m['size_gb']:>5.1f}GB  ", style="dim")
                t.append(f"{m['nome']}", style="bold white" if default else "white")
                if default:
                    t.append("  ← default", style="green")
                t.append("\n")

        t.append("\n      A.  Aggiungi percorso manuale\n",    style="bold magenta")
        if modelli:
            t.append("      D.  Imposta modello di default\n", style="bold green")
        t.append("      0.  ←  Torna\n",                       style="dim")
        t.append("      Q.  🚪  Esci\n\n",                     style="bold red")
        t.append("  ❯ ")

        con.print(Panel(t, title="[bold blue]📂 Modelli GGUF[/]",
                        border_style="blue", padding=(1, 2)))
        try:
            scelta = input("").strip().upper()
        except (EOFError, KeyboardInterrupt):
            break

        if scelta == "0":
            break
        elif scelta == "Q":
            sys.exit(0)

        elif scelta == "A":
            con.print("\n  Percorso completo del file .gguf:")
            percorso = input("  > ").strip().strip('"').strip("'")
            if os.path.isfile(percorso) and percorso.lower().endswith(".gguf"):
                dest = os.path.join(DIR_MODELS, os.path.basename(percorso))
                if os.path.abspath(percorso) != os.path.abspath(dest):
                    try:
                        shutil.copy2(percorso, dest)
                        con.print(f"  [green]✓ Copiato in {dest}[/]")
                    except Exception as e:
                        con.print(f"  [red]Errore copia: {e}[/]")
                else:
                    con.print("  [dim]File già nella cartella models/[/]")
            else:
                con.print("  [red]Percorso non valido o file non .gguf[/]")
            input("  [INVIO] ")

        elif scelta == "D" and modelli:
            con.print(f"\n  Numero del modello da impostare come default (1-{len(modelli)}): ", end="")
            try:
                n = int(input("").strip())
                if 1 <= n <= len(modelli):
                    cfg["modello_default"] = modelli[n - 1]["percorso"]
                    _salva_config(cfg)
                    con.print(f"  [green]✓ Default impostato: {modelli[n-1]['nome']}[/]")
                else:
                    con.print("  [red]Numero fuori range[/]")
            except ValueError:
                con.print("  [red]Inserisci un numero valido[/]")
            input("  [INVIO] ")

    return cfg


# ── Trova binario compilato ───────────────────────────────────────────────────

def _trova_binario(nome: str) -> str | None:
    for cartella in [os.path.join(DIR_BUILD, "bin"), DIR_BUILD]:
        p = os.path.join(cartella, nome)
        if os.path.isfile(p) and os.access(p, os.X_OK):
            return p
    return None


# ── Avvio modello ─────────────────────────────────────────────────────────────

def avvia_modello(con: "Console", cfg: dict) -> dict:
    os.system("cls" if sys.platform == "win32" else "clear")
    stampa_header(con)
    stampa_breadcrumb(con, "🍺 Prismalux › ⚙️ llama.cpp Studio › 🚀 Avvia")

    if not cfg.get("compilato"):
        con.print("\n  [yellow]⚠  llama.cpp non ancora compilato. Usa prima l'opzione 1.[/]")
        input("\n  [INVIO per tornare] ")
        return cfg

    modelli = _cerca_gguf()
    if not modelli:
        con.print("\n  [yellow]⚠  Nessun file .gguf trovato.[/]")
        con.print(f"  Copia i modelli in: [cyan]{DIR_MODELS}[/]")
        input("\n  [INVIO per tornare] ")
        return cfg

    # ── Selezione modello ─────────────────────────────────────────────────
    t = Text()
    t.append("  Scegli il modello da avviare:\n\n", style="bold")
    for i, m in enumerate(modelli, 1):
        default = cfg.get("modello_default") == m["percorso"]
        t.append(f"  {i}. ", style="dim")
        t.append(f"{m['nome']}", style="bold white" if default else "white")
        t.append(f"  ({m['size_gb']:.1f}GB)", style="dim")
        if default:
            t.append("  ← default", style="green")
        t.append("\n")
    t.append("\n  0. Annulla\n\n", style="dim")
    t.append("  ❯ ")

    con.print(Panel(t, title="[bold green]🚀 Seleziona modello[/]",
                    border_style="green", padding=(1, 2)))
    scelta = input("").strip()

    if scelta == "0":
        return cfg
    try:
        idx = int(scelta) - 1
        if not (0 <= idx < len(modelli)):
            raise ValueError
        modello = modelli[idx]
    except ValueError:
        con.print("  [red]Scelta non valida[/]")
        input("  [INVIO] ")
        return cfg

    size_gb = modello["size_gb"]
    hw      = cfg.get("hardware") or _rileva_hardware()

    # ── Calcolo parametri ottimali ────────────────────────────────────────
    n_threads    = cfg.get("n_threads") or _n_threads_ottimale(hw)
    n_gpu_layers = _n_gpu_layers_ottimale(hw, size_gb)
    ram          = hw.get("ram_gb", 0)
    ctx_size     = 8192 if ram >= 32 else 4096 if ram >= 16 else 2048

    # ── Modalità di avvio ─────────────────────────────────────────────────
    os.system("cls" if sys.platform == "win32" else "clear")
    stampa_header(con)
    con.print(f"\n  Modello: [bold]{modello['nome']}[/] ({size_gb:.1f}GB)\n")
    con.print(f"  [dim]Parametri calcolati:[/]")
    con.print(f"  [dim]  threads={n_threads}  gpu_layers={n_gpu_layers}  ctx={ctx_size}[/]\n")

    t2 = Text()
    t2.append("  Modalità di avvio:\n\n", style="bold")
    t2.append("  1. 💬 Chat interattiva\n", style="white")
    t2.append("       Rispondi nel terminale, sessione diretta\n\n", style="dim")
    t2.append("  2. 🌐 Server API  (localhost:8080)\n", style="white")
    t2.append("       Compatibile con OpenAI API — usa qualsiasi client\n\n", style="dim")
    t2.append("  0. Annulla\n\n", style="dim")
    t2.append("  ❯ ")

    con.print(Panel(t2, title="[bold green]Modalità[/]", border_style="green", padding=(1, 2)))
    modo = input("").strip()

    if modo == "0":
        return cfg

    # ── Avvio chat ────────────────────────────────────────────────────────
    if modo == "1":
        binario = _trova_binario("llama-cli") or _trova_binario("main")
        if not binario:
            con.print("  [red]❌ Binario llama-cli non trovato. Ricompila.[/]")
            input("  [INVIO] ")
            return cfg

        cmd = [
            binario,
            "-m", modello["percorso"],
            "--threads",      str(n_threads),
            "--ctx-size",     str(ctx_size),
            "--n-gpu-layers", str(n_gpu_layers),
            "--conversation",
            "--prompt", "Sei un assistente AI utile. Rispondi in italiano quando possibile.",
        ]
        con.print(f"\n  [cyan]🚀 Avvio chat — premi Ctrl+C per uscire[/]\n")
        con.print(Rule(style="dim"))
        try:
            subprocess.run(cmd)
        except KeyboardInterrupt:
            pass
        input("\n  [INVIO per tornare al menu] ")

    # ── Avvio server ──────────────────────────────────────────────────────
    elif modo == "2":
        binario = _trova_binario("llama-server")
        if not binario:
            con.print("  [red]❌ Binario llama-server non trovato. Ricompila.[/]")
            input("  [INVIO] ")
            return cfg

        cmd = [
            binario,
            "-m", modello["percorso"],
            "--threads",      str(n_threads),
            "--ctx-size",     str(ctx_size),
            "--n-gpu-layers", str(n_gpu_layers),
            "--host", "127.0.0.1",
            "--port", "8080",
        ]
        con.print(f"\n  [bold green]🌐 Server avviato su http://127.0.0.1:8080[/]")
        con.print(f"  [dim]Compatibile con OpenAI API — endpoint: /v1/chat/completions[/]")
        con.print(f"  [bold yellow]  Premi Ctrl+C per fermare il server[/]\n")
        con.print(Rule(style="dim"))
        try:
            subprocess.run(cmd)
        except KeyboardInterrupt:
            pass
        input("\n  [INVIO per tornare al menu] ")

    return cfg


# ── Menu principale dello strumento ──────────────────────────────────────────

def menu_studio() -> None:
    cfg = _carica_config()

    while True:
        os.system("cls" if sys.platform == "win32" else "clear")
        cols, _ = shutil.get_terminal_size(fallback=(120, 40))
        con = Console(width=cols) if _RICH else None

        if con:
            stampa_header(con)
            stampa_breadcrumb(con, "🍺 Prismalux › ⚙️ llama.cpp Studio")

            stato      = "[green]✓ Compilato[/]"  if cfg.get("compilato") else "[yellow]⚠ Non ancora compilato[/]"
            moddef     = cfg.get("modello_default", "")
            moddef_str = Path(moddef).name if moddef and os.path.exists(moddef) else "nessuno"

            t = Text()
            t.append("  Stato llama.cpp  : ", style="dim")
            t.append(f"{'✓ Compilato' if cfg.get('compilato') else '⚠ Non ancora compilato'}\n",
                     style="green" if cfg.get("compilato") else "yellow")
            t.append(f"  Modello default  : {moddef_str}\n\n", style="dim")

            t.append("      1.  🔨  Compila llama.cpp\n\n",          style="bold magenta")
            t.append("      2.  📂  Gestisci Modelli GGUF\n\n",      style="bold green")
            t.append("      3.  🚀  Avvia Modello\n\n",              style="bold magenta")
            t.append("      4.  ℹ️   Info Hardware\n\n",              style="bold green")
            t.append("      0.  ←   Torna al menu Prismalux\n",      style="dim")
            t.append("      Q.  🚪  Esci\n\n",                       style="bold red")
            t.append("  ❯ ")

            con.print(Panel(t,
                title="[bold magenta]⚙️ llama.cpp Studio[/]",
                border_style="bold magenta", padding=(1, 2)))
        else:
            print("\n=== llama.cpp Studio ===")
            print("1. Compila\n2. Modelli\n3. Avvia\n4. Hardware\n0. Torna\nQ. Esci")
            print("❯ ", end="")

        try:
            scelta = input("").strip().upper()
        except (EOFError, KeyboardInterrupt):
            break

        if scelta in ("0", ""):
            break
        elif scelta == "Q":
            sys.exit(0)
        elif scelta == "1":
            if con:
                cfg = compila(con, cfg)
        elif scelta == "2":
            if con:
                cfg = gestisci_modelli(con, cfg)
        elif scelta == "3":
            if con:
                cfg = avvia_modello(con, cfg)
        elif scelta == "4":
            if con:
                hw = cfg.get("hardware") or _rileva_hardware()
                mostra_hardware(con, hw)


if __name__ == "__main__":
    menu_studio()

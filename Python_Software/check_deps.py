"""
Utility per controllare le dipendenze Python all'avvio.
Importato da tutti i progetti nella cartella.

Mostra un pannello con:
  ✓  librerie installate
  ✗  librerie mancanti (con comando pip install)
  ⚠  librerie opzionali (avviso ma non blocca)

Funziona anche se rich NON è installato (usa ANSI colors).
"""

import sys
import os
import re
import shutil
import subprocess
import importlib


class Ridimensiona(BaseException):
    """Sollevata dal gestore SIGWINCH per ridisegnare i menu in tempo reale."""
    pass


def leggi_tasto() -> str:
    """
    Legge un singolo tasto senza premere INVIO (raw mode su Unix).
    Riconosce: 0-9, lettere (restituisce maiuscolo), F1, Ctrl+C.
    Fallback su Windows: legge una riga intera.
    """
    import os as _os
    try:
        import termios, tty, select
        fd = sys.stdin.fileno()
        vecchio = termios.tcgetattr(fd)
        try:
            tty.setraw(fd)
            b = _os.read(fd, 1)
            if b == b'\x03':
                raise KeyboardInterrupt
            if b == b'\x04':
                raise EOFError
            if b == b'\x1b':
                r, _, _ = select.select([sys.stdin], [], [], 0.05)
                if r:
                    resto = _os.read(fd, 8)
                    seq = b + resto
                    if seq[:3] == b'\x1bOP':   return 'F1'
                    if seq[:5] == b'\x1b[11~': return 'F1'
                    if seq[:4] == b'\x1b[[A':  return 'F1'
                return 'ESC'
            ch = b.decode('utf-8', errors='ignore')
            return ch.upper() if ch.isalpha() else ch
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, vecchio)
    except (ImportError, AttributeError, OSError):
        # Windows / stdin non-tty: nessun raw mode, legge una riga
        s = input("").strip().upper()
        return 'F1' if s in ('F1', '?') else (s[:1] if s else '')


def stampa_breadcrumb(con, percorso: str) -> None:
    """
    Barra di navigazione stile browser di file.
    Mostra il percorso corrente nella gerarchia dei menu.
    Esempio:  Prismalux › 📚 Apprendimento › Simulatore
    """
    try:
        from rich.rule import Rule
        con.print(Rule(f" {percorso} ", style="dim blue", align="left"))
    except Exception:
        print(f"  {percorso}")


def leggi_tasto_pannello(padding_left: int = 2) -> str:
    """
    Come input_nel_box ma legge un tasto singolo (raw mode, senza INVIO).
    Usa per menu con scelte a carattere singolo (0-9, Q, F1).
    """
    offset = padding_left + 5
    sys.stdout.write(f"\033[2A\033[{offset}C")
    sys.stdout.flush()
    return leggi_tasto()


def input_nel_box(padding_left: int = 2) -> str:
    """
    Legge input posizionando il cursore sulla riga '  ❯ ' dentro il pannello
    appena stampato con Rich.

    Requisiti per il pannello:
      • padding_bottom = 0  →  es. padding=(1, X, 0, X)
      • L'ultima riga del contenuto deve essere '  ❯ '  (2 spazi + ❯ + spazio,
        SENZA newline finale)

    Come funziona:
      Dopo console.print(Panel(...)), il cursore è sulla riga vuota sotto
      il bordo inferiore del pannello. Con due escape ANSI risaliamo al bordo
      e poi alla riga ❯, poi spostiamo a destra fino a dopo il simbolo.

      Offset colonne = │(1) + padding_left + '  ❯ '(4) = padding_left + 5
    """
    offset = padding_left + 5          # colonne da saltare a partire da col 0
    sys.stdout.write(f"\033[2A\033[{offset}C")
    sys.stdout.flush()
    try:
        return input("").strip()
    except Ridimensiona:
        raise   # il ciclo while catcha e ridisegna

# ANSI colors — funzionano senza librerie esterne
_R  = "\033[91m"   # rosso
_G  = "\033[92m"   # verde
_Y  = "\033[93m"   # giallo
_C  = "\033[96m"   # ciano
_B  = "\033[1m"    # grassetto
_D  = "\033[2m"    # tenue
_X  = "\033[0m"    # reset


def controlla_terminale(min_cols: int = 90, min_righe: int = 22) -> tuple[int, int]:
    """
    Legge le dimensioni attuali del terminale e avvisa se troppo piccolo.
    Restituisce (colonne, righe) correnti — da chiamare prima di disegnare la UI.
    """
    cols, rows = shutil.get_terminal_size(fallback=(120, 40))
    if cols < min_cols or rows < min_righe:
        print(
            f"\n{_Y}  ⚠  Terminale piccolo: {cols}×{rows}. "
            f"Consigliato almeno {min_cols}×{min_righe}.{_X}\n"
            f"  Allarga la finestra per una grafica ottimale.\n"
        )
    return cols, rows


def controlla_dipendenze(dipendenze: list[tuple]) -> None:
    """
    Controlla una lista di dipendenze e mostra un report colorato.

    Parametro 'dipendenze': lista di tuple con 4 valori:
      (nome_import, nome_pacchetto, obbligatorio, descrizione)

    Esempio:
      controlla_dipendenze([
          ("rich",              "rich",               True,  "Interfaccia grafica"),
          ("requests",          "requests",           True,  "Comunicazione HTTP"),
          ("ddgs", "duckduckgo-search",  False, "Ricerca web"),
      ])

    Se mancano librerie obbligatorie → stampa errore ed esce.
    Se mancano solo opzionali → avvisa ma continua.
    """
    risultati  = []   # (import_name, pkg_name, obbligatorio, descrizione, installato)
    mancanti_req = []
    mancanti_opt = []

    for import_name, pkg_name, obbligatorio, descrizione in dipendenze:
        try:
            importlib.import_module(import_name)
            installato = True
        except ImportError:
            installato = False
            if obbligatorio:
                mancanti_req.append((pkg_name, descrizione))
            else:
                mancanti_opt.append((pkg_name, descrizione))
        risultati.append((import_name, pkg_name, obbligatorio, descrizione, installato))

    # Se tutto OK, non mostrare nulla
    if not mancanti_req and not mancanti_opt:
        return

    # Prova con rich, altrimenti usa ANSI
    try:
        import rich
        _report_rich(risultati, mancanti_req, mancanti_opt)
    except ImportError:
        _report_ansi(risultati, mancanti_req, mancanti_opt)

    if mancanti_req:
        sys.exit(1)

    # Solo opzionali mancanti: chiede conferma per continuare
    input("\n  Premi INVIO per continuare comunque... ")


def _report_rich(risultati, mancanti_req, mancanti_opt):
    from rich.console import Console
    from rich.table import Table
    from rich.panel import Panel

    console = Console()

    # Tabella stato librerie
    tabella = Table(show_header=True, header_style="bold white",
                    border_style="dim", show_lines=False)
    tabella.add_column("Stato",      width=6)
    tabella.add_column("Libreria",   width=22)
    tabella.add_column("Tipo",       width=12)
    tabella.add_column("A cosa serve", width=35)

    for _, pkg_name, obbligatorio, descrizione, installato in risultati:
        if installato:
            stato = "[green]  ✓[/]"
            stile = "green"
        elif obbligatorio:
            stato = "[red]  ✗[/]"
            stile = "bold red"
        else:
            stato = "[yellow]  ⚠[/]"
            stile = "yellow"

        tipo = "[dim]obbligatoria[/]" if obbligatorio else "[dim]opzionale[/]"
        tabella.add_row(stato, f"[{stile}]{pkg_name}[/]", tipo, f"[dim]{descrizione}[/]")

    # Messaggio install
    msg_install = ""
    if mancanti_req:
        pkgs = " ".join(p for p, _ in mancanti_req)
        msg_install += f"\n[bold red]Installa le librerie obbligatorie:[/]\n"
        msg_install += f"  [bold cyan]pip install {pkgs}[/]\n"
    if mancanti_opt:
        pkgs = " ".join(p for p, _ in mancanti_opt)
        msg_install += f"\n[yellow]Installa le librerie opzionali (consigliato):[/]\n"
        msg_install += f"  [cyan]pip install {pkgs}[/]\n"
        msg_install += f"[dim]  Senza di esse alcune funzioni non saranno disponibili.[/]"

    bordo = "red" if mancanti_req else "yellow"
    titolo = "❌ Librerie mancanti" if mancanti_req else "⚠️  Librerie opzionali mancanti"

    console.print()
    console.print(tabella)
    if msg_install:
        console.print(Panel(msg_install.strip(), title=titolo, border_style=bordo))


def _report_ansi(risultati, mancanti_req, mancanti_opt):
    linea = "─" * 58
    print(f"\n{_B}{linea}{_X}")

    if mancanti_req:
        print(f"{_R}{_B}  ❌ LIBRERIE OBBLIGATORIE MANCANTI{_X}")
    else:
        print(f"{_Y}{_B}  ⚠  LIBRERIE OPZIONALI MANCANTI{_X}")

    print(f"{_B}{linea}{_X}\n")

    for _, pkg_name, obbligatorio, descrizione, installato in risultati:
        if installato:
            icona = f"{_G}✓{_X}"
        elif obbligatorio:
            icona = f"{_R}✗{_X}"
        else:
            icona = f"{_Y}⚠{_X}"
        print(f"  {icona}  {pkg_name:22} {_D}{descrizione}{_X}")

    if mancanti_req:
        pkgs = " ".join(p for p, _ in mancanti_req)
        print(f"\n{_B}  Installa le librerie obbligatorie:{_X}")
        print(f"  {_C}pip install {pkgs}{_X}")

    if mancanti_opt:
        pkgs = " ".join(p for p, _ in mancanti_opt)
        print(f"\n{_Y}  Installa le librerie opzionali (consigliato):{_X}")
        print(f"  {_C}pip install {pkgs}{_X}")
        print(f"{_D}  Senza di esse alcune funzioni non saranno disponibili.{_X}")

    print(f"\n{_B}{linea}{_X}")


# ── Header condiviso: logo + CPU/RAM/GPU/VRAM/DSK ────────────────────────────

# ── Cache hardware (rilevata una volta sola) ──────────────────────────────────
_hdr_cpu_nome:  str  | None = None
_hdr_gpu_nomi:  list | None = None
_hdr_gpu_ok:    bool | None = None   # None=non testato, False=non disponibile


def _hdr_cpu() -> str:
    """Nome breve CPU (es. 'i7-7700HQ', 'Ryzen 5 3600'). Cached."""
    global _hdr_cpu_nome
    if _hdr_cpu_nome is not None:
        return _hdr_cpu_nome
    nome = ""
    # Linux: /proc/cpuinfo
    try:
        with open("/proc/cpuinfo") as f:
            for riga in f:
                if riga.startswith("model name"):
                    nome = riga.split(":", 1)[1].strip()
                    break
    except Exception:
        pass
    # macOS / platform fallback
    if not nome:
        try:
            import platform as _plat
            nome = _plat.processor()
        except Exception:
            pass
    # Windows: wmic
    if not nome and sys.platform == "win32":
        try:
            r = subprocess.run(
                ["wmic", "cpu", "get", "Name", "/value"],
                capture_output=True, text=True, timeout=5
            )
            if r.returncode == 0:
                for riga in r.stdout.splitlines():
                    if riga.startswith("Name=") and riga[5:].strip():
                        nome = riga[5:].strip()
                        break
        except Exception:
            pass
    m = re.search(r'i[3579]-\d{4,5}[A-Z]*', nome)
    if m:
        _hdr_cpu_nome = m.group(0); return _hdr_cpu_nome
    m = re.search(r'Ryzen\s+\d+\s+\d{4,5}[A-Z]*', nome, re.IGNORECASE)
    if m:
        _hdr_cpu_nome = m.group(0); return _hdr_cpu_nome
    m = re.search(r'Xeon\s+\S+', nome, re.IGNORECASE)
    if m:
        _hdr_cpu_nome = m.group(0); return _hdr_cpu_nome
    _hdr_cpu_nome = nome[:12] if nome else "?"
    return _hdr_cpu_nome


def _hdr_gpu_nomi_lista() -> list[str]:
    """Nomi GPU rilevati (NVIDIA/AMD/Intel). Cached."""
    global _hdr_gpu_nomi
    if _hdr_gpu_nomi is not None:
        return _hdr_gpu_nomi
    nomi: list[str] = []
    try:
        r = subprocess.run(
            ["nvidia-smi", "--query-gpu=name", "--format=csv,noheader"],
            capture_output=True, text=True, timeout=3
        )
        if r.returncode == 0:
            for riga in r.stdout.strip().splitlines():
                nome = riga.strip().replace("NVIDIA GeForce ", "").replace("NVIDIA ", "")
                if nome:
                    nomi.append(nome)
    except Exception:
        pass
    try:
        r = subprocess.run(["rocm-smi", "--showproductname"],
                           capture_output=True, text=True, timeout=3)
        if r.returncode == 0:
            for riga in r.stdout.strip().splitlines():
                m = re.search(r'Card series:\s*(.+)', riga, re.IGNORECASE)
                if m:
                    nomi.append(m.group(1).strip())
    except Exception:
        pass
    try:
        r = subprocess.run(["lspci"], capture_output=True, text=True, timeout=3)
        if r.returncode == 0:
            for riga in r.stdout.splitlines():
                low = riga.lower()
                if not ("vga compatible" in low or "display controller" in low
                        or "3d controller" in low):
                    continue
                if "nvidia" in low and nomi:
                    continue
                m = re.search(r'\[([^\]]+)\]', riga)
                if not m:
                    continue
                nome = m.group(1).strip()
                if "intel" in low:
                    nome = nome.replace("Intel(R) ", "Intel ").replace("(R)", "")
                    nomi.append(nome)
                elif "amd" in low or "radeon" in low or "advanced micro" in low:
                    if not any("Radeon" in n or "AMD" in n for n in nomi):
                        nomi.append(nome)
    except Exception:
        pass
    # Windows: wmic videocontroller (AMD/Intel/NVIDIA senza tool extra)
    if sys.platform == "win32" and not nomi:
        try:
            r = subprocess.run(
                ["wmic", "path", "win32_videocontroller", "get", "name", "/value"],
                capture_output=True, text=True, timeout=5,
            )
            if r.returncode == 0:
                for riga in r.stdout.splitlines():
                    if riga.startswith("Name="):
                        nome = riga[5:].strip()
                        if nome and nome not in nomi:
                            nomi.append(nome)
        except Exception:
            pass
    _hdr_gpu_nomi = nomi
    return _hdr_gpu_nomi


def _hdr_gpu_uso() -> tuple[float | None, float | None, float, float]:
    """(util_perc, vram_perc, vram_gb_usata, vram_gb_tot) via nvidia-smi. Cached."""
    global _hdr_gpu_ok
    if _hdr_gpu_ok is False:
        return None, None, 0.0, 0.0
    try:
        r = subprocess.run(
            ["nvidia-smi",
             "--query-gpu=utilization.gpu,memory.used,memory.total",
             "--format=csv,noheader,nounits"],
            capture_output=True, text=True, timeout=2,
        )
        if r.returncode == 0:
            _hdr_gpu_ok = True
            linee = [l.strip() for l in r.stdout.strip().splitlines() if l.strip()]
            if linee:
                p = [x.strip() for x in linee[0].split(",")]
                util = float(p[0])
                vram_mb_usata = float(p[1])
                vram_mb_tot   = float(p[2])
                vram_perc = (vram_mb_usata / vram_mb_tot * 100) if vram_mb_tot > 0 else 0.0
                return util, vram_perc, vram_mb_usata / 1024, vram_mb_tot / 1024
        _hdr_gpu_ok = False
    except FileNotFoundError:
        _hdr_gpu_ok = False
    except Exception:
        pass
    return None, None, 0.0, 0.0


import time as _time_mod

# ── Campionamento CPU /proc/stat (Linux) ──────────────────────────────────────
_cpu_perc_cache: float = 0.0
_cpu_stat_prev:  tuple = (0.0, 0.0, 0.0)   # (monotonic, idle, total)

# Lettura iniziale all'import per avere un delta valido fin dalla prima chiamata
try:
    with open("/proc/stat") as _f0:
        _v0 = list(map(int, _f0.readline().split()[1:]))
    _cpu_stat_prev = (
        _time_mod.monotonic(),
        _v0[3] + (_v0[4] if len(_v0) > 4 else 0),
        sum(_v0),
    )
    del _f0, _v0
except Exception:
    pass

# ── CPU% su Windows via wmic (cache 2s) ───────────────────────────────────────
_cpu_win_ts:    float = 0.0
_cpu_win_cache: float = 0.0


def _cpu_perc_win() -> float:
    """CPU% su Windows via wmic cpu get LoadPercentage (campionamento ogni 2s)."""
    global _cpu_win_ts, _cpu_win_cache
    now = _time_mod.monotonic()
    if now - _cpu_win_ts < 2.0:
        return _cpu_win_cache
    _cpu_win_ts = now
    try:
        r = subprocess.run(
            ["wmic", "cpu", "get", "LoadPercentage", "/value"],
            capture_output=True, text=True, timeout=4,
        )
        if r.returncode == 0:
            for riga in r.stdout.splitlines():
                if riga.startswith("LoadPercentage="):
                    val = riga[len("LoadPercentage="):].strip()
                    if val.isdigit():
                        _cpu_win_cache = float(val)
                        return _cpu_win_cache
    except Exception:
        pass
    return _cpu_win_cache


def _cpu_perc_reale() -> float:
    """
    Utilizzo CPU reale in % su Linux via /proc/stat (campionamento differenziale).
    Su Windows usa wmic LoadPercentage. Su macOS usa il load average.
    """
    global _cpu_perc_cache, _cpu_stat_prev

    # Windows: wmic
    if sys.platform == "win32":
        return _cpu_perc_win()

    now = _time_mod.monotonic()
    ts_prev, idle_prev, tot_prev = _cpu_stat_prev
    try:
        with open("/proc/stat") as f:
            vals = list(map(int, f.readline().split()[1:]))
        idle_now = vals[3] + (vals[4] if len(vals) > 4 else 0)
        tot_now  = sum(vals)
        if ts_prev > 0 and (now - ts_prev) >= 0.5:
            d_tot  = tot_now  - tot_prev
            d_idle = idle_now - idle_prev
            if d_tot > 0:
                _cpu_perc_cache = max(0.0, min(100.0,
                                               (1 - d_idle / d_tot) * 100))
        _cpu_stat_prev = (now, idle_now, tot_now)
        return _cpu_perc_cache
    except Exception:
        pass
    # Fallback macOS: load average
    try:
        load1 = os.getloadavg()[0]
        ncpu  = os.cpu_count() or 1
        return min(load1 / ncpu * 100, 100)
    except Exception:
        return 0.0


def risorse() -> dict:
    """Legge CPU load, RAM, disco e GPU. Funziona su Linux, macOS e Windows."""
    info = {
        "cpu": 0.0,
        "ram_perc": 0.0, "ram_gb": 0.0, "ram_tot": 0.0,
        "disco_perc": 0.0, "disco_gb_usato": 0.0, "disco_gb_tot": 0.0,
        "gpu_perc": None, "gpu_vram_perc": None,
        "gpu_vram_gb_usata": 0.0, "gpu_vram_gb_tot": 0.0,
    }
    # ── CPU load (utilizzo reale via /proc/stat) ───────────────────────────
    info["cpu"] = _cpu_perc_reale()

    # ── RAM (Linux) ───────────────────────────────────────────────────────
    try:
        with open("/proc/meminfo") as f:
            mem = {}
            for riga in f:
                k, v = riga.split(":")
                mem[k.strip()] = int(v.strip().split()[0])
        tot   = mem["MemTotal"]
        avail = mem.get("MemAvailable", mem["MemFree"])
        used  = tot - avail
        info["ram_tot"]  = tot  / 1024 / 1024
        info["ram_gb"]   = used / 1024 / 1024
        info["ram_perc"] = used / tot * 100
    except Exception:
        pass

    # ── RAM fallback Windows (ctypes) ────────────────────────────────────
    if info["ram_tot"] == 0.0 and sys.platform == "win32":
        try:
            import ctypes
            class _MemStat(ctypes.Structure):
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
            _s = _MemStat()
            _s.dwLength = ctypes.sizeof(_s)
            ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(_s))
            tot_b  = _s.ullTotalPhys
            avail_b = _s.ullAvailPhys
            used_b  = tot_b - avail_b
            if tot_b > 0:
                info["ram_tot"]  = tot_b  / (1024 ** 3)
                info["ram_gb"]   = used_b / (1024 ** 3)
                info["ram_perc"] = used_b / tot_b * 100
        except Exception:
            pass

    # ── Disco ─────────────────────────────────────────────────────────────
    try:
        _disk_path = "C:\\" if sys.platform == "win32" else "/"
        du = shutil.disk_usage(_disk_path)
        info["disco_perc"]     = du.used / du.total * 100
        info["disco_gb_usato"] = du.used  / 1024 ** 3
        info["disco_gb_tot"]   = du.total / 1024 ** 3
    except Exception:
        pass

    # ── GPU (nvidia-smi) ─────────────────────────────────────────────────
    (info["gpu_perc"], info["gpu_vram_perc"],
     info["gpu_vram_gb_usata"], info["gpu_vram_gb_tot"]) = _hdr_gpu_uso()
    return info


def _barra_res(perc: float, larghezza: int = 12) -> str:
    """Barra ASCII proporzionale: ██░░░░"""
    pieni = round(perc / 100 * larghezza)
    return "█" * pieni + "░" * (larghezza - pieni)


def stampa_header(con, res: dict | None = None, breadcrumb: str = "") -> None:
    """
    Pannello giallo condiviso: logo + CPU/RAM/GPU/VRAM/DSK con barre ██░░.

    Args:
        con        : Rich Console già creata con width corretto.
        res        : dizionario risorse (da risorse()). Se None lo rileva al momento.
                     Passa il valore cachato per evitare chiamate ripetute.
        breadcrumb : percorso navigazione (es. "Prismalux › 🤖 Multi-Agente AI").
                     Se non vuoto, appare come Rule dentro il pannello giallo.
    """
    try:
        from rich.panel import Panel
        from rich.table import Table
        from rich.text import Text

        BAR_W     = 12   # larghezza barra in caratteri
        ROW_HDR_W = 16   # colonna fissa label+dettaglio
        DET_MAX   = 9    # max char per il dettaglio (GB, nome CPU…)

        def _c(p: float) -> str:
            return "red" if p > 90 else "yellow" if p > 70 else "green"

        def _riga(testo: Text, label: str, detail: str,
                  perc: float, colore: str) -> None:
            """Aggiunge una riga con barra ██░░ allineata a colonna fissa."""
            hdr = f"{label}   {detail[:DET_MAX]}"
            testo.append(f"{hdr:<{ROW_HDR_W}}", style="dim")
            testo.append(f"  {_barra_res(perc, BAR_W)}", style=colore)
            testo.append(f"  {perc:3.0f}%\n", style=colore)

        if res is None:
            res = risorse()

        gpu_p  = res.get("gpu_perc")
        vram_p = res.get("gpu_vram_perc")

        logo = Text()
        logo.append("  🍺  ", style="bold yellow")
        logo.append("P R I S M A L U X", style="bold cyan")
        logo.append("  🍺\n", style="bold yellow")
        logo.append("  Costruito per i mortali che aspirano alla saggezza.\n",
                    style="dim italic")

        r = Text(justify="right")

        # CPU
        cpu_p = res["cpu"]
        _riga(r, "CPU", _hdr_cpu()[:DET_MAX], cpu_p, _c(cpu_p))

        # RAM — usata/totale GB
        ram_p = res["ram_perc"]
        _riga(r, "RAM", f"{res['ram_gb']:.1f}/{res['ram_tot']:.1f}GB",
              ram_p, _c(ram_p))

        # GPU — nome breve (senza barra se nessun dato di utilizzo)
        gpu_nomi  = _hdr_gpu_nomi_lista()
        gpu_short = gpu_nomi[0][:DET_MAX] if gpu_nomi else ""
        if gpu_p is not None:
            _riga(r, "GPU", gpu_short, gpu_p, _c(gpu_p))
        else:
            hdr = f"{'GPU'}   {gpu_short}"
            r.append(f"{hdr:<{ROW_HDR_W}}\n", style="dim")

        # VRAM
        if vram_p is not None:
            _riga(r, "VRAM",
                  f"{res['gpu_vram_gb_usata']:.1f}/{res['gpu_vram_gb_tot']:.1f}GB",
                  vram_p, _c(vram_p))

        # DSK
        dsk_p = res["disco_perc"]
        _riga(r, "DSK",
              f"{res['disco_gb_usato']:.0f}/{res['disco_gb_tot']:.0f}GB",
              dsk_p, _c(dsk_p))

        g = Table.grid(padding=(0, 2), expand=True)
        g.add_column(ratio=2)
        g.add_column(ratio=1, justify="right")
        g.add_row(logo, r)

        if breadcrumb:
            from rich.rule import Rule
            from rich.console import Group
            panel_content = Group(g, Rule(f" {breadcrumb} ", style="dim blue", align="left"))
        else:
            panel_content = g

        con.print(Panel(panel_content, border_style="bold yellow", padding=(0, 2)))
    except Exception:
        pass  # senza rich: nessun header visibile

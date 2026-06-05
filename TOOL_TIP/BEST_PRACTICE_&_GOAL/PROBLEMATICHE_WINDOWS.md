═══════════════════════════════════════════════════════════════════
  PRISMALUX — PROBLEMATICHE WINDOWS (aggiornato 2026-05-31)
═══════════════════════════════════════════════════════════════════

────────────────────────────────────────────────
PROBLEMA 1 — Build fallisce in silenzio  [RISOLTO 2026-05-31]
────────────────────────────────────────────────
Sintomo originale:
  "❌ Binario GUI non trovato: .../gui/build_win/Prismalux_GUI.exe"
  Nessun errore di compilazione visibile nel terminale.

Soluzione definitiva (build.py):
  build.py genera automaticamente "errore.txt" nella root del
  progetto con le righe di errore filtrate (GCC/Clang/CMake/MSVC).
  Il file include la fase fallita, il log completo e le ultime
  30 righe del log a schermo.

  Percorso errore: Prismalux/errore.txt
  Percorso log:    gui/build_win/prismalux_build.log

Come diagnosticare oggi:
  1. Eseguire build.bat (doppio clic)
  2. Se fallisce: aprire errore.txt nella root del progetto

Dipendenze MSYS2 necessarie (installare UNA VOLTA):
  pacman -S mingw-w64-ucrt-x86_64-qt6-base
  pacman -S mingw-w64-ucrt-x86_64-qt6-tools
  pacman -S mingw-w64-ucrt-x86_64-qt6-sql
  pacman -S mingw-w64-ucrt-x86_64-gcc
  pacman -S mingw-w64-ucrt-x86_64-cmake
  pacman -S mingw-w64-ucrt-x86_64-ninja

────────────────────────────────────────────────
PROBLEMA 2 — Avvia_Prismalux.bat: "exe non trovato"
────────────────────────────────────────────────
Sintomo:
  "[ERRORE] Prismalux_GUI.exe non trovato."
  "Il programma non è ancora compilato."

Causa:
  L'utente esegue Avvia_Prismalux.bat prima di compilare.
  Il .bat cerca l'exe in 4 posizioni note; se nessuna esiste, fallisce.

Soluzione:
  Eseguire PRIMA build.bat (doppio clic dalla root del progetto).
  Solo dopo build.bat completato con successo si può usare Avvia_Prismalux.bat.
  Ordine corretto: build.bat → Avvia_Prismalux.bat

────────────────────────────────────────────────
PROBLEMA 3 — Caratteri encoding nel titolo cmd.exe
────────────────────────────────────────────────
Sintomo:
  Il titolo del terminale mostra "Prismalux ÔÇö Aggiorna (Windows)"
  invece di "Prismalux — Aggiorna (Windows)".

Causa:
  cmd.exe usa codepage 850/1252 per default; la em dash (—, U+2014)
  nei file .bat viene interpretata come caratteri latini estesi.

Workaround (non ancora implementato):
  Aggiungere "chcp 65001 > nul" all'inizio di aggiorna.bat
  per passare a UTF-8 prima di visualizzare qualsiasi testo.

────────────────────────────────────────────────
PROBLEMA 4 — File con nome URL causa errore 0x80070057
────────────────────────────────────────────────
Sintomo:
  Windows mostra "Impossibile copiare il file. Errore 0x80070057:
  Parametro non corretto" durante l'estrazione dello ZIP.
  File coinvolto: https://github.com/bonninr/freecad_mcp.desktop

Causa:
  Il file aveva `:` e `/` nel nome — caratteri illegali su NTFS.
  È un collegamento Linux (.desktop) che non serve su Windows.

Fix applicato (commit 5f7167d):
  Rinominato in MCPs/freecad_mcp/freecad_mcp_github.desktop
  Aggiunta esclusione *.desktop in crea_zip_windows.py

────────────────────────────────────────────────
PROBLEMA 5 — Dipendenza da bash/aggiorna.sh su Windows  [RISOLTO 2026-05-31]
────────────────────────────────────────────────
Sintomo originale:
  build.bat cercava bash (MSYS2) come fallback e lanciava
  aggiorna.sh — uno script Unix non nativo Windows.
  Riferimenti a "build.sh" (file inesistente) in alcune UI.

Soluzione definitiva:
  - build.bat è ora un launcher puro (~80 righe):
    cerca Python in COMPILE_WIN\toolchain\python\ → PATH → MSYS2
    poi esegue build.py — nessun bash, nessun .sh
  - build.py gestisce Windows nativamente (platform.system())
  - COMPILE_WIN\setup.bat scarica Python embedded automaticamente
  - Tutti i riferimenti a build.sh corretti in aggiorna.sh

Nuovo flusso Windows:
  1. COMPILE_WIN\setup.bat  (una tantum — ~600 MB)
  2. build.bat              (doppio clic per compilare)
  3. Avvia_Prismalux.bat    (avvia l'app)

Configurazione nota di Paolo:
  MSYS2 in C:\msys64 (UCRT64)
  GPU AMD → Ollama lenta su GPU → usare CPU: $env:OLLAMA_NUM_GPU=0
  Modelli: deepseek-coder:33b, deepseek-r1:7b, qwen3:30b,
           gpt-oss:20b/120b, glm-4.7-flash, llama3.2-vision

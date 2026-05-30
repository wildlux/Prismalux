═══════════════════════════════════════════════════════════════════
  PRISMALUX — PROBLEMATICHE WINDOWS (aggiornato 2026-05-30)
═══════════════════════════════════════════════════════════════════

────────────────────────────────────────────────
PROBLEMA 1 — Build fallisce in silenzio (aggiorna.sh)
────────────────────────────────────────────────
Sintomo:
  "❌ Binario GUI non trovato: .../gui/build_win/Prismalux_GUI.exe"
  "[ERRORE] aggiorna.sh ha restituito un errore."
  Nessun errore di compilazione visibile nel terminale.

Causa:
  cmake --build era filtrato con grep + || true: gli errori di
  compilazione venivano inghiottiti e lo script continuava come
  se nulla fosse, poi falliva perché il binario non esisteva.

Fix applicato (commit 5e76243):
  - Output cmake salvato in build_win/prismalux_build.log (tee)
  - Se binario assente: stampa ultime 60 righe del log
  - Controlla esito configure con grep su "Configuring done"

Come diagnosticare:
  Aprire MSYS2 UCRT64 e lanciare:
    cd /C/Users/.../Prismalux_v2.9_Windows
    bash aggiorna.sh --gui
  Il log completo è in: gui/build_win/prismalux_build.log

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
PROBLEMA 5 — Build Windows mai verificata su hardware reale
────────────────────────────────────────────────
Stato:
  build.bat e aggiorna.bat non sono mai stati eseguiti su una
  macchina Windows fisica con MSYS2 installato (solo testati
  logicamente). La macchina di Paolo (Xeon + AMD GPU lenta)
  è il primo test reale.

Rischio:
  Possibili DLL mancanti, percorsi Qt errati, dipendenze non
  dichiarate. Ogni nuovo errore va aggiunto a questo file.

Configurazione nota di Paolo:
  MSYS2 in C:\msys64 (UCRT64)
  GPU AMD → Ollama lenta su GPU → usare CPU: $env:OLLAMA_NUM_GPU=0
  Modelli: deepseek-coder:33b, deepseek-r1:7b, qwen3:30b,
           gpt-oss:20b/120b, glm-4.7-flash, llama3.2-vision

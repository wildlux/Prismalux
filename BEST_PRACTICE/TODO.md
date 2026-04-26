# TODO — Prismalux
> Ultimo aggiornamento: 2026-04-26
> Legenda priorità: 🔴 bloccante · 🟡 importante · 🟢 nice-to-have
> Stato: ⬜ aperto · 🔄 in corso · ✅ fatto · 🚫 scartato

---

## 🔴 CRITICI

| # | Stato | Task | File / Contesto |
|---|-------|------|-----------------|
| C1 | ⬜ | **Push su GitHub** — branch `master` è avanti all'origin | `git push origin master` |
| C2 | ✅ | **Unifica Start/Stop** — bottone unico run↔stop via `_setRunBusy/_setSendBusy/_setGenerateBusy` | `agenti_page`, `oracolo_page`, `quiz_page`, `programmazione_page`, `strumenti_page` |
| C3 | ✅ | **Chat persistente** — `ChatHistory` salva ogni sessione in `~/.prismalux_chats/*.json`; sidebar lista sessioni con caricamento/export/elimina | `gui/chat_history.h/cpp` · `gui/mainwindow.cpp:onChatCompleted` |
| C4 | ✅ | **Warning hex** — fix `\x9cCertamente` e `\xa464GB` con split stringa literal | `gui/pages/impostazioni_page.cpp:4498,4655` |
| C5 | ✅ | **Versione CMake disallineata** — `CMakeLists.txt` diceva v2.2, README dice v2.8 | `gui/CMakeLists.txt` riga 2 |
| C6 | ✅ | **README_BUILD.md obsoleto** — diceva `cd Qt_GUI` (vecchio nome); corretto in `cd gui` | `gui/README_BUILD.md` |

---

## 🟡 IMPORTANTI

| # | Stato | Task | File / Contesto |
|---|-------|------|-----------------|
| I1 | ✅ | **Slider "Correggi con AI"** — slider `1–10` con preview `∞` | `gui/pages/programmazione_page.cpp` · `m_loopMax` |
| I2 | ✅ | **RAG: descrizione chunk** — log mostra "📄 chunk 12/47 da paper.pdf" durante indicizzazione | `gui/pages/impostazioni_page.cpp` · `m_ragQueueSource` |
| I3 | ⬜ | **Stile foto profilo** — foto CV in LavoroPage non segue tema QSS | `gui/pages/lavoro_page.cpp` |
| I4 | ✅ | **Unisci Impara + Sfida** — unificati in sub-tab; tab header ridotto da 4 a 3 | `gui/mainwindow.cpp` |
| I5 | ✅ | **Modalità calcolo al boot** — applicata prima di hw-detect nel costruttore | `gui/pages/manutenzione_page.cpp:31-48` |
| I6 | ✅ | **Rimuovi TUI C** — cartella `src/` rimossa | — |
| I7 | ⬜ | **Build Windows su hardware reale** — `build.bat` mai verificato su una macchina Windows vera; rischio DLL mancanti o percorsi Qt errati | `gui/build.bat` |
| I8 | ✅ | **Installer Windows (.exe distribuibile)** — creato `gui/build_installer_windows.bat`; CMake MinGW + windeployqt6 + ZIP PowerShell | `gui/build_installer_windows.bat` |
| I9 | ✅ | **Export chat** — pulsante "Esporta conversazione" salva `.pdf`, `.md`, `.html`, `.txt` con timestamp | `gui/pages/agenti_page_ui.cpp` |

---

## 🟢 NICE-TO-HAVE

| # | Stato | Task | File / Contesto |
|---|-------|------|-----------------|
| N1 | ✅ | **Classifica LLM — colonna VRAM** — colonna "VRAM (GB)" calcolata da `ram_gb / 1.2` | `gui/pages/impostazioni_page.cpp` |
| N2 | 🚫 | **Tooltip modello broken** — `isKnownBrokenModel()` ritorna false (bug risolto Ollama 0.21.1); pattern mantenuto per future occorrenze | — |
| N3 | ✅ | **Misto: layer GPU ottimali** — `min(layer_model, capacity_NVIDIA)`; riempie GPU al massimo, overflow su CPU | `gui/pages/manutenzione_page.cpp:applyComputeMode()` |
| N4 | ✅ | **Monitor TTFT** — elapsed timer per risposta aggiunto nell'header di ogni bolla AI | `gui/pages/agenti_page_stream.cpp:278-282` |
| N5 | ⬜ | **TTFT nel MonitorPanel** — collegare `requestStarted` al pannello header per mostrare ms dell'ultima risposta in tempo reale | `gui/pages/impostazioni_page.cpp:buildAvanzateTab()` |
| N6 | ⬜ | **AppImage aggiornata** — probabilmente obsoleta; rieseguire `scripts/crea_appimage.sh` dopo il push | `scripts/crea_appimage.sh` |
| N7 | ⬜ | **CHANGELOG.md** — storico versioni leggibile per utenti (v2.2→v2.8 è un salto significativo) | root del progetto |
| N8 | ⬜ | **Auto-update** — controllo all'avvio via GitHub API per nuova versione disponibile; notifica non intrusiva nell'header | `gui/mainwindow.cpp` |
| N9 | ⬜ | **CloudCompare stub** — attualmente mostra "in arrivo"; rimuovere la voce o implementare il bridge CloudComPy | `gui/pages/strumenti_page.cpp:257` |
| N10 | ⬜ | **macOS** — teoricamente compilabile con Qt6 ma mai testato; aggiungere istruzioni build + test su CI | `gui/CMakeLists.txt` |

---

## ✅ COMPLETATI QUESTA SESSIONE (2026-04-26)

| Task | Dettaglio |
|------|-----------|
| ✅ Cron integrato | Tab "⏰ Cron" in Impostazioni — job pianificati AI (giornaliero/orario/intervallo/una volta), persistenza JSON, log real-time | 
| ✅ Scritta "Grafico" ridondante rimossa | Label `📈 Grafico` eliminata dalla scheda Grafico — rimangono solo i tab 2D/3D |
| ✅ aggiorna.sh — ZIP fix | Correzione path `C_software/` → `gui/`; ZIP passa da 4 KB a 47 MB |
| ✅ aggiorna.sh — Whisper fix | Pattern grep corretto (`blas-bin-x64` invece di `win64`); estrae exe+DLL; download automatico prima del ZIP |
| ✅ C5 — Versione CMake allineata | `CMakeLists.txt` aggiornato a v2.8 |
| ✅ C6 — README_BUILD.md aggiornato | Percorso `Qt_GUI` → `gui` in tutte le istruzioni |
| ✅ C3 — Chat persistente | Già implementata: `ChatHistory` + sidebar sessioni; `chatCompleted` → `saveLog` ad ogni pipeline |
| ✅ I8 — Installer Windows | `gui/build_installer_windows.bat`: CMake MinGW Release + windeployqt6 + ZIP PowerShell (2026-04-26) |
| ✅ I9 — Export chat PDF/MD/HTML/TXT | `agenti_page_ui.cpp`: filtro PDF aggiunto, QPrinter HighResolution, Qt6::PrintSupport in CMakeLists (2026-04-26) |
| ✅ 4 nuovi MCP Ringraziamenti | OBS MCP · WhatsApp MCP · Telegram MCP · WordPress MCP aggiunti a `kMCPs[]` in `impostazioni_page.cpp` (2026-04-26) |
| ✅ OpenCode → sub-tab APP Controller | Rimosso da main tab bar; aggiunto come sub-tab in `app_controller_page.cpp` (2026-04-26) |
| ✅ TTS Pausa/Riprendi | Bottone pausa con SIGSTOP/SIGCONT (Linux) in `agenti_page_ui.cpp` + reset in `agenti_page_tts.cpp` (2026-04-26) |
| ✅ Salva lavoro non salvato | Dialog "Vuoi salvare?" al cambio tab Programmazione in `mainwindow.cpp` (2026-04-26) |

---

## ✅ COMPLETATI (archivio)

| # | Task | Data |
|---|------|------|
| ✅ | C2 — Bottone unificato run↔stop su tutte le pagine | 2026-04-25 |
| ✅ | C4 — Fix 2 warning hex escape (impostazioni_page.cpp) | 2026-04-25 |
| ✅ | I1 — Slider Loop Fix 1–10 con preview ∞ | 2026-04-25 |
| ✅ | I2 — RAG chunk log con nome file sorgente | 2026-04-25 |
| ✅ | I4 — Impara + Sfida unificati in sub-tab | 2026-04-25 |
| ✅ | N1 — Colonna VRAM nella classifica LLM | 2026-04-25 |
| ✅ | Fix qwen3.5 — rimosso da `s_knownBroken` | 2026-04-24 |
| ✅ | Modalità Calcolo LLM — 4 modalità: GPU / CPU / Misto / Doppia GPU | 2026-04-25 |
| ✅ | Misto ottimizzato — `min(layers, capacity_NVIDIA)` | 2026-04-25 |
| ✅ | hw_detect — 3 tipologie: CPU / Intel iGPU (sysfs) / GPU dedicata | 2026-04-25 |
| ✅ | Fix `num_gpu=999` — Ollama ISE 500; sostituito con stima hw_detect | 2026-04-24 |
| ✅ | CLAUDE.md + README.md compattati | 2026-04-24 |
| ✅ | Rename `C_software/Qt_GUI` → `gui/` — path aggiornati | 2026-04-23 |
| ✅ | Request ID in AiClient — routing risposte stale | 2026-03-28 |
| ✅ | RAG stop cooperativo — Ferma indicizzazione con chunk preservati | 2026-03-28 |
| ✅ | Byzantine loop fix — guard `Idle` in `onFinished()` | 2026-03-28 |

---

## 🚫 SCARTATI

| # | Task | Motivo |
|---|------|--------|
| 🚫 | Port a Python (TUI) | Sostituita completamente da GUI Qt |
| 🚫 | Tema "hacker green" | Già coperto da `dark_neon.qss` |

# TODO — Prismalux

*Aggiornato: 2026-07-02 | Versione: v3.0*
*Unico file TODO del progetto — sostituisce tutti i precedenti.*

Legenda: 🔴 Alta · 🟡 Media · 🟢 Bassa | ⬜ Aperto · ✅ Fatto

---

## DESKTOP Qt6

### Bug / Qualità

| # | Priorità | Descrizione | File | Stato |
|---|----------|-------------|------|-------|
| D-1 | 🟡 | `runLint()` è uno stub vuoto — pyflakes/clang-tidy/eslint non eseguiti | `main_programming_slots.cpp:1629` | ✅ |
| D-2 | 🟡 | TTFT (⚡Nms) nell'header — timer da avviare in `onBtnRunClicked()`, primo token ferma | `mainwindow.cpp` | ✅ |
| D-3 | 🟢 | Lambda > 2 righe in `connect()` — estratte 12+5 slot/free-func in MainWindow (onAutoUpdateReply, TTFT, ChatSelection, SelectAll, TabSearch) | vari `.cpp` | ✅ |
| D-4 | 🟢 | Export dataset DPO dai feedback 👍/👎 in JSONL → formato Alpaca/ShareGPT | `main_ai_feedback.cpp` | ✅ |
| D-7 | 🟡 | Titolo + riassunto breve/lungo sessione chat generati via LLM dopo il primo scambio (stile Claude.ai), sostituisce il titolo troncato a 40 char. `AiClient` dedicato (`m_summaryAi`) per non collidere con lo stream visibile | `chat_history.h/.cpp`, `mainwindow.h/.cpp` | ✅ 2026-07-01 |
| D-8 | 🟡 | Refactoring file monolitici: `main_graph_canvas.cpp` (4499 righe), `main_programming.cpp` (3387), `main_app_controller.cpp` (3092), `mainwindow.cpp` (2834), `main_app_controller_slots.cpp` (2691), `main_tools.cpp` (2572) — split in moduli più piccoli seguendo il pattern già usato per `main_ai_*.cpp`/`settings_*.cpp`. Lavoro serio, non "en passant" — pianificare a parte | `gui/pages/main_graph_canvas.cpp` + altri sopra | ⬜ |

### Distribuzione

| # | Priorità | Descrizione | Stato |
|---|----------|-------------|-------|
| D-5 | 🔴 | Rigenerare `EXPORT/linux/Prismalux_v3.0_Linux.zip` — binario ricompilato + AppImage 204MB + 3 ZIP v3.0 generati | ✅ |
| D-6 | 🟡 | Scrivere GitHub Release notes v2.9→v3.0 (44 feature nuove, 7 VULN chiuse) | ✅ `EXPORT/RELEASE_NOTES_v3.0.md` |

---

## ANDROID Qt6 (ANDROID/QT_ANDROID_Version/)

> App attuale: v2.9 (33 MB APK). **Tutte le pagine principali implementate** (`CMakeLists.txt` v2026-06-22).
> **Non usare Flutter né Kivy** — mantenere solo Qt6 nativo.

### Nuove pagine — Alta priorità

| # | Pagina | Descrizione | Ore | Dipende da | Stato |
|---|--------|-------------|-----|-----------|-------|
| A-1 | Assistente AI | 6 categorie × 8 azioni (Studio/Scrittura/Ricerca/Libri/Produttività/Documenti). Layout: `QComboBox` cat → griglia 2×N bottoni → I/O condiviso. Copre il 60% degli use case desktop. | 4-5h | — | ✅ `pipeline_page.cpp` |
| A-2 | Oracle/Chat rapida | Pillole azione rapida (Matematica/Finanza/Dev/Scrivi…), allegato immagine vision, bottone 📊 Grafico | 3h | — | ✅ `oracle_page.cpp` |
| A-3 | GraphMemory mobile | Port di `graph_memory.h/cpp` — SQLite in `AppDataLocation`. API: addNode/edge/neighbours/search. Segnale `changed()`. **Sblocca A-4, A-5, B-3** | 3-4h | — | ✅ `graph_memory_mobile.cpp` |
| A-4 | Multi-Agente | MasterAgent → JSON piano → BFS sequenziale su `depends_on` → lista sub-task ⏳/✅/❌ | 5h | A-3 | ✅ `multi_agent_page.cpp` |
| A-5 | Hermes Memoria | File `user_knowledge.md` in AppData, bottone "Salva" su bolle AI, agente estrattore automatico, toggle inietta memoria nel system prompt | 4h | A-3 | ✅ `hermes_page.cpp` |
| A-6 | Grafico ChartWidget | Formula `y=f(x)` → campionamento → `QPainter` custom, zoom/pan touch, integrazione con A-2 | 3h | — | ✅ `chart_page.cpp` |
| A-7 | FinanzaPage | TFR (rivalutazione FOI), Codice Fiscale D.M.1976+Belfiore, 730 IRPEF 2024, Regime Forfettario. Codice portabile dal desktop. | 4h | — | ✅ `finanza_page.cpp` |
| A-8 | FileAiPage | Analisi PDF/CSV/URL, Share Intent Android, riusa `extractPdfText()` già in `lavoro_page.cpp` | 3h | — | ✅ `file_ai_page.cpp` |

### Estensioni pagine esistenti — Media priorità

| # | Pagina | Cosa aggiungere | Ore | Dipende da | Stato |
|---|--------|-----------------|-----|-----------|-------|
| B-1 | ChatPage | Storia SQLite sessioni, TTFT label ⚡, export TXT/MD, vision 📷 allega immagine | 3h | — | ✅ SQLite+`m_ttftLbl`+`m_exportBtn`+allegati in `chat_page.h` |
| B-2 | LavoroPage | Tracker offerte (JSON), cover letter separata, foto profilo CV circolare | 3h | — | ✅ Tab Offerte+Cover Letter+foto circolare in `lavoro_page.cpp` |
| B-3 | ChatPage | Toggle 🧠 Hermes: inietta memoria, bottone "Salva" su bolle | 2h | A-3, A-5 | ✅ `m_hermesToggle`+`buildSystemPrompt`+`onBubbleSaveMemClicked` in `chat_page.cpp` |
| B-4 | MatematicaPage | 52 formule predefinite in `QComboBox` + bottone 🔀 shuffle | 2h | — | ✅ 52 formule + 🔀 in `matematica_page.cpp` |
| B-5 | RicercaPage | Analisi Fenomeni + upload file, Carta Astrale GPS auto | 3h | — | ✅ `ricerca_mob_page.cpp` |
| B-6 | SecurityPage | 4 agenti sequenziali (Injection/Segreti/Memoria/Config) + sintetizzatore | 4h | — | ✅ `security_page.cpp` |
| B-7 | Simulatore Algoritmi | BubbleSort/BFS/Dijkstra step-by-step con `QTimer` + `QPainter` | 3h | — | ✅ `simulatore_page.cpp` |
| B-8 | Voice Loop | Dopo risposta AI → TTS legge → STT → nuova domanda (hands-free) | 2h | C-3 | ✅ `onVoiceLoopClicked()` in `chat_page.cpp` |
| B-9 | WAN Compute client | Campo IP:porta → `QTcpSocket` → lista task → esegui task | 2h | — | ✅ `wan_client_page.cpp` |

### UX / Native Android — Bassa priorità

| # | Descrizione | File | Ore | Stato |
|---|-------------|------|-----|-------|
| C-1 | Drawer animazione smooth (`QPropertyAnimation` 220ms OutCubic) | `mainwindow.cpp` | 1h | ✅ |
| C-2 | Auto-update notifica GitHub API — badge 🆕 in header dopo 10s avvio | `mainwindow.cpp` | 1h | ✅ |
| C-3 🔴 | `RECORD_AUDIO` in AndroidManifest + richiesta runtime — **blocca B-8 e AudioPage su Android ≥ 6** | `AndroidManifest.xml` | 30m | ✅ |
| C-4 | Share Intent (`ACTION_SEND */*`) → forward a FileAiPage | `AndroidManifest.xml` + `mainwindow.cpp` | 1h | ✅ |
| C-5 | GPS Carta Astrale — `QGeoPositionInfoSource` → pre-compila lat/lon | `ricerca_mob_page.cpp` | 1h | ✅ |
| C-6 | Shuffle 52 formule matematica — `QRandomGenerator` + flash animazione | `matematica_page.cpp` | 1h | ✅ |
| C-7 | ThermalMonitor badge 🌡️ in header (verde/arancio/rosso) — `ThermalMonitor` già esiste | `mainwindow.cpp` | 30m | ✅ |
| C-8 | Test suite Android: GraphMemory, FinanzaPage, MultiAgent, FileAi, Assistente | `tests/test_mobile_logic.cpp` | 3h | ✅ 62 test, 5 suite, 0 fail |

### APK

| # | Priorità | Descrizione | Stato |
|---|----------|-------------|-------|
| APK-1 | 🔴 | Rebuild APK v3.0 dopo completamento A-1÷A-7 | ✅ 2026-06-24 — 33 MB firmato debug, 3 fix clang ARM64 + manifest duplicato + android-34 |
| APK-2 | 🟡 | Aggiornare `versionName` in `AndroidManifest.xml` da 2.9 a 3.0 | ✅ |

### Ordine implementazione consigliato

```
FASE 1 (1 sett): C-3 → A-3 → A-1 → A-2          (base + feature più impattanti)
FASE 2 (1 sett): A-5 → A-7 → A-8 → B-1           (memoria + dati + chat estesa)
FASE 3 (1 sett): A-4 → A-6 → B-4 → B-2           (avanzato)
FASE 4 (3 gg):   B-5 → B-7 → C-1 → C-2 → C-4    (UX polish)
FASE 5 (1 sett): B-6 → B-8 → B-9 → B-3 → C-8    (sicurezza + WAN + test)
FASE 6:          APK-2 → APK-1                     (release)
```

### Note tecniche rapide

| Argomento | Nota |
|-----------|------|
| DPI Android | `QScreen::devicePixelRatio()` — NON `dpiScale()` (funzione desktop-only) |
| Path dati | `QStandardPaths::AppDataLocation` → `~/.prismalux_mobile/` |
| PDF parser | `extractPdfText()` già in `lavoro_page.cpp` — riusare in FileAiPage |
| CMakeLists | Aggiungere ogni nuovo `.cpp` in `ANDROID/QT_ANDROID_Version/android_app/CMakeLists.txt` |
| DrawerNavItem | Aggiungere struct `{ icon, label, index }` in `kNavItems[]` in `buildDrawer()` |
| No lambda senza context | 4° arg sempre in `connect()` — regola progetto |
| QtSql | Già disponibile (usato in `chat_page.cpp`) — GraphMemory mobile funziona subito |

---

## PERCEPTOR — Pipeline audio/video locale (sessione 2026-07-01)

> Ispirato all'architettura Perceptor (chat+audio+video). Tre componenti già funzionanti:
> VAD pre-Whisper (evita compute su silenzio), frame hashing (salta frame ridondanti),
> video captioning (VLM descrive frame nuovi).

| # | Priorità | Descrizione | File | Stato |
|---|----------|-------------|------|-------|
| P-1 | 🔴 | **VAD desktop** — `vad_filter.py` (webrtcvad + fallback RMS) integrato in `onSttTimeout()` prima di `SttWhisper::transcribe()` — skip se SILENCE | `main_ai_stt.cpp`, `Tools/scripts/vad_filter.py` | ✅ |
| P-2 | 🔴 | **VAD web** — stesso script integrato in `handleWhisper()` del LAN server — risponde `{"silence":true}` senza avviare Whisper | `lan_server_compute.cpp` | ✅ |
| P-3 | 🔴 | **Video captioning tab** — tab "🎬 Analizza Video" in MultimediaPage: ffmpeg campiona frame, dhash filtra simili, VLM Ollama descrive frame nuovi. Slot: `onVcStartStopClicked`, `onVcProcReadyRead`, `onVcProcFinished` | `main_multimedia.cpp/h`, `Tools/scripts/video_caption.py` | ✅ |
| P-4 | 🟡 | **fast-whisper** — `faster-whisper` (large-v3-turbo) come backend prioritario in `SttWhisper::transcribe()`. Fallback automatico a whisper-cli se il modulo non è installato. CLI via `P::fastWhisperBin()`, Python via `Tools/scripts/fast_whisper_transcribe.py`. QSettings: `stt/fast_whisper_model`, `stt/fast_whisper_enabled` | `widgets/stt_whisper.h`, `prismalux_paths.h`, `Tools/scripts/fast_whisper_transcribe.py` | ✅ |
| P-7 | 🟡 | **Riassunti RAG** — tool `scrivi_riassunto` (breve\|\|\|testo o dettagliato\|\|\|testo) e `leggi_riassunto` (breve/dettagliato). File in `P::ragDir()`: `riassunto_breve.md` + `riassunto_dettagliato.md`. Cercabili anche via `search_rag`. L'LLM decide autonomamente quando salvare/leggere | `main_ai_tools.cpp`, `prismalux_paths.h` | ✅ |
| P-5 | 🟢 | **Speaker diarization** — `Tools/scripts/speaker_diarize.py` (3 backend: pyannote.audio, simple-diarizer, resemblyzer). Integrato in `stt_whisper.h` come `SttWhisper::diarize()` + `formatDiarization()`. Output JSON `{backend, segments:[{speaker,start,end,text}], speakers}` | `widgets/stt_whisper.h`, `Tools/scripts/speaker_diarize.py` | ✅ |
| P-6 | 🟢 | **streamlink MCP** — `MCPs/streamlink_mcp/server.py`: 3 tool JSON-RPC 2.0 (`stream_info`, `stream_capture`, `stream_download`) via yt-dlp+ffmpeg. WAV 16kHz per Whisper STT. Protezione SSRF in `_validate_url()` | `MCPs/streamlink_mcp/` | ✅ |

---

## WEB (webchat.html + lan_server.cpp)

> **29/29 tab implementate** (sessione 2026-06-22). Tutte le funzionalità desktop sono ora presenti nel web.

| # | Priorità | Funzionalità | Stato |
|---|----------|--------------|-------|
| W-1 | 🟡 | **Byzantino** — radio Pipeline/Byzantino in tab `age`, logica lato client completa | ✅ |
| W-2 | 🟡 | **Agente Autonomo** — tab `aut` con loop ReAct step-by-step (`aut-run`, `aut-log`) | ✅ |
| W-3 | 🟡 | **Sfida!** — tab `sfd` con quiz AI, 9 argomenti, 3 difficoltà, punteggio, feedback | ✅ |
| W-4 | 🟢 | **Matematica avanzata** — tab `mat` con sub-tab LaTeX (KaTeX live), sequenza, costanti, N-esimo, espressione, grafico | ✅ |
| W-5 | 🟡 | **730 / P.IVA** — tab `irp` (730 IRPEF con calcolo lorda/netta/bonus) + tab `piv` (regime forfettario L.190/2014) | ✅ |
| W-6 | 🟡 | **Security Analyzer** — tab `sec` con 4 agenti + sintetizzatore CISO | ✅ |
| W-7 | 🟢 | **Sintetizzatore** — tab `osc` con WebAudio, 4 forme d'onda, freq/vol/detune, oscilloscopio canvas, tastiera C4-B5 | ✅ |
| W-8 | 🟡 | Titolo + riassunto sessione auto-generato via LLM dopo il primo scambio (`sesAutoTitle()`, riuso `ageCallAi()`), sostituisce il `prompt()` manuale per il salvataggio | ✅ 2026-07-01 |

---

## MATRICE STATO COMPLESSIVO

| Funzionalità | Desktop | Android | Web |
|---|---|---|---|
| Chat singola | ✅ | ✅ | ✅ |
| Pipeline agenti | ✅ | ✅ `pipeline_page` | ✅ |
| Byzantino | ✅ | ✅ | ✅ W-1 |
| Agente Autonomo | ✅ | ✅ | ✅ W-2 |
| Sfida! | ✅ | ✅ `sfida_page` | ✅ W-3 |
| Multi-Agente GraphMemory | ✅ | ✅ A-3/A-4 | ✅ |
| Assistente categorie | ✅ | ✅ A-1 | ✅ |
| Oracle/Chat rapida | ✅ | ✅ A-2 | ✅ |
| Hermes memoria | ✅ | ✅ A-5 `hermes_page` | — |
| RAG + Grafo RAG | ✅ | ✅ | ✅ |
| Finanza (TFR/730/PIVA/CF) | ✅ | ✅ A-7 `finanza_page` | ✅ W-5 |
| Ricerca strutturata | ✅ | ✅ | ✅ |
| FileAI (PDF/CSV) | ✅ | ✅ A-8 `file_ai_page` | ✅ |
| Matematica avanzata | ✅ | ✅ 52 formule + 🔀 | ✅ W-4 KaTeX |
| Security Analyzer | ✅ | ✅ B-6 `security_page` | ✅ W-6 |
| Grafico ChartWidget | ✅ | ✅ A-6 `chart_page` | ✅ |
| Simulatore Algoritmi | ✅ | ✅ B-7 `simulatore_page` | — |
| Voice Loop hands-free | ✅ | ✅ B-8 `onVoiceLoopClicked` | — |
| TTS / STT | ✅ | ✅ | ✅ |
| WAN Compute | ✅ | ✅ B-9 `wan_client_page` | — |
| Cron scheduler | ✅ | ✅ `cron_page` | ✅ |
| Impara / Quiz | ✅ | ✅ | ✅ |
| Lavoro AI | ✅ | ✅ | ✅ |
| OBS / AppController | ✅ | ✅ | ✅ |
| BLE / LAN sync | ✅ | ✅ | — |
| Bioinformatica | ✅ | ⚪ skip | ⚪ skip |
| Stable Diffusion | ✅ | ⚪ skip | ⚪ skip |
| Lint codice | ✅ D-1 | — | — |
| TTFT header | ✅ D-2 | ✅ B-1 `m_ttftLbl` | — |
| Sintetizzatore | ✅ | ✅ `sintetizzatore_page` | ✅ W-7 |
| LavoroPage tracker | ✅ | ✅ B-2 `lavoro_page.cpp` | — |
| Hermes toggle in Chat | — | ✅ B-3 `chat_page.cpp` | — |

---

## TEST MANCANTI — audit 2026-07-01 (COMPLETATO)

> Audit completo: **63 suite registrate** in CMakeLists, tutte le .cpp presenti corrispondono.
> Tutte le feature individuate senza copertura ora hanno test dedicati (T-1÷T-6, tutte ✅).

| # | Priorità | Cosa testare | Dove aggiungere | Stato |
|---|----------|--------------|-----------------|-------|
| T-1 | 🔴 | **SttWhisper diarization** — `isDiarizeEnabled()`, `diarizeNSpeakers()` round-trip QSettings; `formatDiarization()` JSON → `"[SPEAKER_00] testo"`, JSON vuoto, JSON malformato, speaker multipli | `gui/tests/test_stt_whisper_live.cpp` → nuova CAT-E, 8 test | ✅ 2026-07-01 — bug reale trovato e fix: `formatDiarization()` non catturava mai `"text"` quando appare dopo `"end"` (ordine reale di `speaker_diarize.py`) |
| T-2 | 🔴 | **DepCheckPanel** (`widget_dep_check.h`) — costruzione senza crash (CAT-A), `deps` non vuota, `runAllChecks()` avvia QProcess, segnali `allOk()`/`someMissing(int)` emessi | `gui/tests/test_dep_check_panel.cpp` (nuova suite) | ✅ 2026-07-01 — 19 test (CAT-A/B/C), 100% pass |
| T-3 | 🟡 | **LAN rubrica persone** (`main_lan_wan.cpp` `m_accessListTable`) — save/load QSettings `lan/accessList` JSON round-trip, addRow, persistenza tra sessioni | `gui/tests/test_lan_wan_core.cpp` → nuova CAT-E | ✅ 2026-07-01 — 8 test, 100% pass |
| T-4 | 🟡 | **speaker_diarize.py** — WAV sintetico `--speakers 2` → JSON con 2 speaker e campi `backend/segments/speakers`; CUDA_VISIBLE_DEVICES="" forzato; QSKIP se simple-diarizer assente | `gui/tests/test_perceptor_scripts.cpp` (nuova suite CAT-C) | ✅ 2026-07-01 — 6 test, 100% pass |
| T-5 | 🟡 | **fast_whisper_transcribe.py** — WAV 1s → trascrizione non vuota o testo breve; `--model tiny` accettato; QSKIP se faster-whisper non installato | `gui/tests/test_perceptor_scripts.cpp` (stessa suite, CAT-D) | ✅ 2026-07-01 — 5 test, 100% pass |
| T-6 | 🟢 | **streamlink_mcp** (`MCPs/streamlink_mcp/server.py`) — JSON-RPC 2.0: lista tool non vuota, `_validate_url()` blocca IP privati RFC1918, QSKIP se venv assente | `gui/tests/test_streamlink_mcp.cpp` (nuova suite) | ✅ 2026-07-01 — 18 test (CAT-A protocollo + CAT-B SSRF), 100% pass |

### Note tecniche raccolte durante l'audit (riutilizzabili in futuro)

- **Pattern test Python**: `QProcess::start(python3, {script, args})` + `waitForFinished()` → parse stdout
- **`device="auto"` + GPU incompatibile → crash**: `fast_whisper_transcribe.py` (come altri script whisper/torch) tenta CUDA anche se le librerie runtime non sono caricabili (`libcublas.so.12 is not found`) — dal lato test si forza CPU con `QProcessEnvironment` + `CUDA_VISIBLE_DEVICES=""` senza modificare lo script (non espone un flag `--cpu`)
- **VAD scarta toni puri**: fixture audio "parlata" per script che usano VAD (silero-vad/webrtcvad, incluso `vad_filter=True` di faster-whisper) va generata con `espeak-ng` (voce sintetica reale), non con un tono sinusoidale — vedi `synthSpeech()` in `test_perceptor_scripts.cpp`
- **Modello "large-v3-turbo" (default) troppo lento su CPU per unit test** (>90s anche se già in cache) — usare sempre `--model tiny` esplicito nei test, ~6s end-to-end
- **Output script Python misto stdout**: alcune librerie (es. `simple_diarizer`) stampano log di progresso su stdout PRIMA del JSON finale — estrarre solo dal primo `{` in poi prima di fare `QJsonDocument::fromJson()`, vedi `runDiarize()`
- **Import di uno script come modulo per testare funzioni private** (es. `_validate_url()` in `streamlink_mcp/server.py`): funziona perché il main loop è protetto da `if __name__ == "__main__":` — `sys.path.insert(0, dir); import server` non avvia il loop stdin
- **Pattern venv MCP di produzione**: `py = venvExists() ? venvPython() : P::findPython()` (da `McpManagerPage`) — replicato senza linkare la pagina intera in `test_streamlink_mcp.cpp::pickPython()`
- **Widget header-only con Q_OBJECT** (es. `widget_dep_check.h`): vanno aggiunti come source (non solo `#include`) al target ctest per AUTOMOC
- **Pattern `#define private public`**: per testare metodi privati, includere il header tra `#define private public` / `#undef` PRIMA degli altri include Qt, e aggiungere `-include sstream` al target (GCC 15)
- **Ordine testuale ≠ ordine di esecuzione**: verificare invarianti runtime (es. "X impostato prima di Y") sul corpo della funzione chiamante (`main()`), non sull'intero file — una funzione richiamata può essere *definita* più in alto nel sorgente
- **CLAUDE.md aggiornato 2026-07-01**: tutte le 66 suite ctest ora documentate nella tabella "Suite per categoria" (incluse le 9 che mancavano: `AiClient`, `AiMemory`, `DepCheckPanel`, `Distillazione`, `GraphMemoryConcurrent`, `LanWanCore`, `McpIntegration`, `PerceptorScripts`, `StreamlinkMcp`)

# Prismalux v3.0 — Release Notes

**Data:** 2026-06-21 | **Tag:** `v3.0` | **Da:** v2.9

> *"Costruito per i mortali che aspirano alla saggezza."*

---

## What's New

### Intelligenza Artificiale (tab AI)

- **GraphMemory** — memoria a grafo con persistenza SQLite (`~/.prismalux/graph_memory.db`). Nodi tipizzati, archi pesati, BFS `neighbours()`, `searchNodes()`, export DOT/JSON/TXT. Segnale `changed()` per aggiornamenti UI real-time.
- **Multi-Agente** (tab 9) — `MasterAgent` decompone un obiettivo in `SubTask` JSON con `depends_on`. Esecuzione BFS sequenziale; ogni risultato finisce in GraphMemory. Pannello stato (⏳/✅/❌) + tab Risultato/DOT/JSON.
- **Agente Autonomo** — pipeline autonoma read→patch→compile→fix con iniezione proattiva RAG (fix TASK-4: RAG injection in agente e Byzantino).
- **Byzantino** — discussione multi-prospettiva potenziata con contesto RAG iniettato automaticamente.
- **TTFT (Time To First Token)** — label `⚡ Nms` nella status bar (verde/arancio/rosso) dopo ogni risposta AI.

### Ricerca e Dati (tab Ricerca)

- **RagGraph** — scansiona `~/prismalux_rag_docs/` + `RAG/`, estrae entità+relazioni via LLM → GraphMemory. Progressivo file per file. Segnali `progressUpdated`, `finished`, `fileError`.
- **Grafo RAG** — lista nodi filtrabile con icone tipo, click → dettaglio + vicini BFS, export PNG Graphviz dark-theme.
- **QFileSystemWatcher** — auto-re-indicizzazione RAG con debounce 2s su modifica file.

### Matematica (tab Math)

- **LaTeX KaTeX** — rendering formule in Analisi 1/2 e output AI via `LatexView : QWebEngineView`. Delimitatori `\(...\)` inline e `\[...\]` display. Fallback `QTextEdit` se WebEngine assente.
- **Randomizer 52 formule** — shuffle casuale in "Risolvi Passi" con animazione flash.

### Strumenti / Finanza

- **Scheda TFR dedicata** — rivalutazione composta FOI, tassazione separata, C.F. auto con algoritmo D.M. 1976 + Belfiore ~120 comuni.
- **730 e P.IVA web** — calcolatori IRPEF 2024 (scaglioni 23/35/43%), regime forfettario, con endpoint `/api/tfr` e `/api/piva` nel LAN server.
- **Sfida!** — modalità "Rivedi errori": salva le risposte sbagliate, dialog con colori + spiegazione AI.

### Infrastruttura LAN/WAN

- **WAN Calcolo Distribuito** — TCP porta 11600, 28 task predefiniti, cron 1-1440 min, dashboard BOINC-style (`lan_wan_page`).
- **TLS LAN** — `QSslServer` con certificato self-signed auto-generato al primo avvio.
- **Coda FIFO LAN** — richieste LLM serializzate per multi-utente simultaneo.
- **Auth WAN** — token Bearer obbligatorio; confronto constant-time (`timingSafeEqual`).

### Sicurezza

- **SecurityAnalyzerPage** — 4 agenti Ollama paralleli (Injection/Segreti/Memoria/Config) + sintetizzatore finale. Tab "Sicurezza" in AppController [7].
- **5 MCP sicurezza** — `prismalux-security` con tool per analisi statica, secrets scan, dependency audit, network probe, report.
- **API Key → QKeychain** — le chiavi LLM cloud non più in `QSettings` in chiaro.
- **Token rotation** — rigenerazione PIN WAN da UI Impostazioni.
- **WireGuard** — guida e config generata in tab Impostazioni → VPN.

### TTS / STT

- **Web (lan_server.cpp)** — tab "Voce": `SpeechSynthesis` (voce+velocità) + `MediaRecorder` → `/api/whisper` per STT live.
- **Voice Loop** — modalità hands-free: risposta AI → TTS → STT → nuova domanda (Android `chat_page.cpp`).

### Android (app Qt6)

- **37 pagine implementate** — Assistente AI (6 cat × 8 azioni), Oracle, GraphMemory Mobile, Multi-Agente, Hermes Memoria, Grafico ChartWidget, FinanzaPage, FileAiPage, SecurityPage, WAN Client, Pipeline, Sfida, Cron, Editor, Collab, BLE, Camera QR, Simulatore, e altre.
- **Chat SQLite** — persistenza storia messaggi (`initDb/saveMessage`), TTFT label, export TXT/MD, allegati immagine vision.
- **ThermalMonitor badge** — indicatore `🌡️` verde/arancio/rosso nell'header Android.
- **Voice Loop Android** — `RECORD_AUDIO` in AndroidManifest + richiesta runtime (sblocca `AudioPage` su Android >= 6).
- **Auto-update notifica** — controllo GitHub API 10s dopo l'avvio; badge `🆕` in header.

---

## Security Fixes (7 vulnerabilita' chiuse)

| # | Commit | Vulnerabilita' |
|---|--------|---------------|
| 1 | `1bf385b` | 4 VULN da security review multi-agente (injection, path traversal, auth bypass, log injection) |
| 2 | `e07129d` | 6 fix: LIKE escape, WAN token timing, QR TLS, rate limiting chat, access.log, TOCTOU |
| 3 | `87a21ca` | 3 debolezze residue: LIKE escape SQLite, WAN token warning, QR TLS note |
| 4 | `e557b57` | WAN token confronto non-costante → `timingSafeEqual` (timing attack) |
| 5 | `e2ff1f1` | `PathGuard` — validazione path tool AI (path traversal nei tool `leggi_file`/`lista_file`) |
| 6 | `49ea3c3` | Qt GUI hardening: injection fix, TOCTOU RAG, TTS loop fix, XSS lista lavoro |
| 7 | `3b34bd0` | 3 hardening attivi: CORS rimosso `*`, null-byte HTTP, Content-Length >25 MB → 400 |

---

## Bug Fixes & Stability

- **SEGV STT** — race condition nel thread di trascrizione Whisper (fix `19736fd`)
- **DPI audit** — tutti i widget strutturali usano `dpiScale()` su schermi HiDPI
- **Web abort** — chiamate HTTP LAN abortite correttamente su navigazione rapida
- **34 QProcess errorOccurred** — handler aggiunto su tutti i `QProcess::start()` critici (3 batch)
- **SQL_EXEC macro** — gestione errori uniforme su tutte le query SQLite
- **REPL sandbox** — `#ifdef Q_OS_LINUX` per ulimit, non applicato su Windows/macOS
- **KaTeX path portabile** — `katexBaseUrl()` in `latex_view.h`, funziona su Win/macOS/Linux
- **GraphMemory dangling ref** — fix decompose + mkpath + path/port costanti
- **prepareClose()** — evita blocco su chiusura tab AI + backup automatico GraphMemory
- **fetchModels muto** — connessione `error` sempre wired per mostrare stato nel widget

---

## Android

- Versione APK: `3.0` (versionCode 30, targetSdk 34)
- OpenSSL ARM64 linkato staticamente per BLE AES-256-GCM
- llama.cpp b9181 pinnato (NEON arm64, Vulkan disabilitato per NDK v26.1 bug)
- `RECORD_AUDIO` runtime permission aggiunta in `AndroidManifest.xml`
- ZXing-cpp v2.2.1 per scanner QR live integrato

---

## Web Chat (lan_server.cpp)

- Tab bar a 2 righe con sezioni logiche
- A-/A+ font size controls
- Endpoint aggiunti: `/api/tfr`, `/api/git`, `/api/wiki`, `/api/piva`
- Fix `ERR_CONTENT_LENGTH_MISMATCH` su risposte streaming lunghe
- Tab "Voce" TTS+STT completamente funzionale via browser mobile

---

## Breaking Changes / Note di aggiornamento

- `kWanComputePort` cambiato da 11500 a **11600** — aggiornare configurazioni client WAN
- AppImage: 169 MB (include Qt6WebEngine per KaTeX) — escluso da git (>100 MB)
- DB GraphMemory: `~/.prismalux/graph_memory.db` e `~/.prismalux/rag_graph.db` — creati automaticamente al primo avvio

---

## Statistiche

| Metrica | Valore |
|---------|--------|
| Suite di test (ctest) | 41 totali, 38 no-Ollama |
| Test GraphMemory | 65 |
| Pagine Android | 37 |
| MCP attivi | 51 |
| Vulnerabilita' chiuse | 7 |
| Stringhe i18n | ~720 |

---

*Prismalux e' software open-source. Se lo trovi utile, considera una donazione via PayPal (badge nel README).*

# PATH.md — Punto d'entrata e mappa del software

> Mappa di orientamento del repository (agg. 2026-07-17, verificata contro il codice).
> Convenzioni C++/API: `gui/CLAUDE.md` · Pattern UI predefiniti: `gui/GUI_BEST_PRACTICE.md` · Lavori: `TODO.md`

## Punto d'entrata

**`gui/main.cpp` → `int main()`** — binario prodotto: `build_gui/Prismalux_GUI`.

Flusso di avvio, nell'ordine:

1. **`--server`** in argv → `runHeadlessServer()`: solo `LanServer` senza GUI (`QCoreApplication`),
   porta da QSettings `lan/port` (default 11500) o `--port N`. Altrimenti prosegue con la GUI.
2. `PLog::installMessageHandler()` — log centralizzato `[CAT][LVL]` (debug solo con `PRISMALUX_DEBUG=1`).
3. `fixPrismaluxPermissions()` — permessi dei file sensibili prima di qualsiasi I/O.
4. **Single-instance** (D-18): probe su `QLocalSocket`; se un'istanza è già viva le chiede il
   `raise()` e termina; altrimenti prende il lock (`QLocalServer`, socket per-utente).
5. **Splash** (`QSplashScreen`) mostrata SUBITO, con pompa eventi fino all'expose (Wayland).
6. Font bundle (`fonts/`) + fallback emoji.
7. **i18n**: preferenza QSettings `sistema/lingua` ("it" | "en" | "system"); l'italiano è la lingua
   sorgente, per le altre carica `prismalux_XX.qm` da `i18n/` o accanto all'eseguibile.
8. `PRISMALUX_COMPUTE_MODE` da QSettings — fonte unica per la modalità calcolo LLM.
9. `Qt::AA_DontUseNativeDialogs` (evita crash KIO su KDE).
10. **`MainWindow w; w.show()`** → `splash.finish(&w)` → `SingleInstanceGuard` → `app.exec()`.

### Dentro MainWindow (gui/mainwindow*.cpp)

- Costruttore: SOLO la tab [0] AI è costruita eager; le altre 7 sono placeholder
  sostituiti da **`ensureTabBuilt(idx)`** (`mainwindow_tabs.cpp`) al primo click o
  da timer di pre-build in background. Layout completo delle 8 tab: `gui/CLAUDE.md` § "Layout tab".
- `mainwindow.cpp` (ctor/backend llama-server) · `_tabs` (lazy tab) · `_header` · `_chat`
  (onboarding wizard) · `_slots` · `_settings` (dialog ⚙️/log) · `_monitor` (HW) · `_backend`.

## Punti d'ingresso secondari

| Ingresso | Dove | Note |
|---|---|---|
| Web app | `gui/lan_server.cpp` → `GET /web` (porta 11500) | chat/TTS/STT da browser, token LAN |
| Headless | `Prismalux_GUI --server [--port N]` | solo LanServer, niente GUI |
| Android | `ANDROID/QT_ANDROID_Version/` (APK Qt6) | parla col LanServer del PC |
| Vision3D | server HTTPS `P::kVision3DPort` | foto da telefono (Media → Scan 3D) |
| WAN Compute | TCP `P::kWanComputePort` 11600 | nodi worker distribuiti |
| MCP | `MCPs/*/server.py` (JSON-RPC stdio) | plugin Python lanciati dalla GUI |
| Launcher | `./Prismalux` (script) · `Prismalux.desktop` | entrambi → `build_gui/Prismalux_GUI` |

## Mappa del repository

```
Prismalux/
├── gui/                    ← TUTTO il sorgente C++/Qt6 (unico frontend attivo)
│   ├── main.cpp            ← PUNTO D'ENTRATA (vedi sopra)
│   ├── mainwindow*.cpp/.h  ← finestra principale spezzata per responsabilità
│   ├── ai_client.h/cpp     ← HTTP verso Ollama/llama-server (chat, modelli, embedding)
│   ├── prismalux_paths.h   ← UNICA fonte per path/porte/chiavi QSettings (namespace P)
│   ├── lan_server.h/cpp    ← server LAN/web app (porta 11500)
│   ├── graph_memory.h/cpp  ← grafo SQLite (nodi/archi/BFS) — 3 DB: rag_graph, graph_memory, hermes_memory
│   ├── rag_engine.h/cpp    ← RAG JLT 256-dim  ·  rag_graph.h/cpp ← ingest docs → GraphMemory
│   ├── pages/    (~200 file) ← una pagina = 1 .h + N .cpp, prefissi:
│   │     main_ai_*         tab [0] AI (pipeline, byzantino, tool, bolle, stt/tts…)
│   │     main_tools_*      tab [1] Strumenti · main_multimedia tab [2] Media
│   │     main_programming_* tab [3] · main_math_*/main_graph_* tab [4]
│   │     main_utility/main_jobs/main_finance/main_lan_wan/main_wan_* tab [5] Utilità
│   │     main_bioinformatica tab [6] · main_app_controller_* tab [7] TeleComanda
│   │     main_research_*   Ricerca (sotto-tab di Strumenti) · main_multi_agent Multi-Agente
│   │     settings_*        ImpostazioniPage (dialog ⚙️) · widget_* componenti pagina · dialog_*
│   ├── widgets/  (60 file) ← componenti riusabili, molti header-only Q_OBJECT (→ CPP_SRCS)
│   ├── tests/              ← ~76 suite ctest (build: gui/build_tests, BUILD_TESTS=ON)
│   ├── themes/             ← QSS (base + varianti dark/light) via ThemeManager
│   ├── i18n/               ← prismalux_it.ts / prismalux_en.ts (senza <location>)
│   ├── GUI_BEST_PRACTICE.md ← pattern predefiniti UI — consultare prima di scrivere UI
│   └── CLAUDE.md           ← convenzioni, API, layout tab, suite test
├── build_gui/              ← build Release canonica (binario + .qm) — fuori git
├── gui/build_tests/        ← build test canonica — fuori git
├── MCPs/                   ← ~50 plugin MCP Python (JSON-RPC stdio), ognuno con README
├── ENGINE_LLM/             ← llama.cpp, hybrid_llm, llama_cpp_studio, swarfstar
├── ANDROID/                ← app mobile Qt6 + APK (path fisso P::root()+"/ANDROID/PrismaluxMobile.apk")
├── IOS/                    ← bozza iOS
├── Tools/                  ← script di supporto (scripts/ = STT/depth/ply_to_obj…, aruco/, ipa/, docker/)
├── EXPORT/                 ← distribuzione: linux/ (crea_appimage.sh), windows/, android/, release notes
├── TOOL_TIP/               ← documentazione non-codice (BEST_PRACTICE_&_GOAL/, KNOWLEDGE_USER path reale)
├── RAG/ · KNOWLEDGE_USER/  ← dati locali utente (fuori git)
├── EXTERNAL_DeviceS/       ← config dispositivi fisici (fuori git)
├── Test/                   ← test Python di integrazione AI (la build C++ dei test è gui/build_tests)
├── aggiorna.sh             ← build + sync .desktop (--test → ctest, --gui solo build, --zip)
├── ctest_lotti.sh          ← suite a lotti con pausa termica (CPU 90°C — mai ctest -j4 completo)
├── build.py / build.bat    ← build multipiattaforma
├── TODO.md                 ← UNICO file TODO del progetto
└── PATH.md                 ← questo file
```

## Flusso dati principale (chat)

```
utente → AgentiPage (pages/main_ai*.cpp)
  → guardie zero-LLM (main_ai_tools.cpp: data/calcoli/knowledge/cache — "tipo grep", zero token)
  → RAG (rag_engine) + Hermes (graph_memory) + user_knowledge (P::prependKnowledge)
  → AiClient::chat() → Ollama :11434 / llama-server :8081 / cloud (con scrubPii)
  → streaming token → bolle (main_ai_bubbles/format) → salvataggi (ChatHistory, Hermes, cache)
```

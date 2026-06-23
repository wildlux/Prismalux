# TODO — Prismalux

*Aggiornato: 2026-06-23 | Versione: v3.0*
*Unico file TODO del progetto — sostituisce tutti i precedenti.*

Legenda: 🔴 Alta · 🟡 Media · 🟢 Bassa | ⬜ Aperto · ✅ Fatto

---

## DESKTOP Qt6

### Bug / Qualità

| # | Priorità | Descrizione | File | Stato |
|---|----------|-------------|------|-------|
| D-1 | 🟡 | `runLint()` è uno stub vuoto — pyflakes/clang-tidy/eslint non eseguiti | `main_programming_slots.cpp:1629` | ✅ |
| D-2 | 🟡 | TTFT (⚡Nms) nell'header — timer da avviare in `onBtnRunClicked()`, primo token ferma | `mainwindow.cpp` | ✅ |
| D-3 | 🟢 | Lambda > 2 righe in `connect()` — estratte 12 slot/free-func; residue ~6 in main_finance/multimedia | vari `.cpp` | ✅ |
| D-4 | 🟢 | Export dataset DPO dai feedback 👍/👎 in JSONL → formato Alpaca/ShareGPT | `main_ai_feedback.cpp` | ✅ |

### Distribuzione

| # | Priorità | Descrizione | Stato |
|---|----------|-------------|-------|
| D-5 | 🔴 | Rigenerare `EXPORT/linux/Prismalux_v3.0_Linux.zip` — attuale è v2.9 obsoleto | ⬜ |
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
| APK-1 | 🔴 | Rebuild APK v3.0 dopo completamento A-1÷A-7 | ⬜ |
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

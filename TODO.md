# TODO — Prismalux

*Aggiornato: 2026-06-23 | Versione: v3.0*
*Unico file TODO del progetto — sostituisce tutti i precedenti.*

Legenda: 🔴 Alta · 🟡 Media · 🟢 Bassa | ⬜ Aperto · ✅ Fatto

---

## DESKTOP Qt6

### Bug / Qualità

| # | Priorità | Descrizione | File | Stato |
|---|----------|-------------|------|-------|
| D-1 | 🟡 | `runLint()` è uno stub vuoto — pyflakes/clang-tidy/eslint non eseguiti | `main_programming_slots.cpp:1629` | ⬜ |
| D-2 | 🟡 | TTFT (⚡Nms) nell'header — timer da avviare in `onBtnRunClicked()`, primo token ferma | `mainwindow.cpp` | ⬜ |
| D-3 | 🟢 | Lambda > 2 righe in `connect()` — ~12 occorrenze residue da estrarre in slot | vari `.cpp` | ⬜ |
| D-4 | 🟢 | Export dataset DPO dai feedback 👍/👎 in JSONL → formato Alpaca/ShareGPT | `main_ai_feedback.cpp` | ⬜ |

### Distribuzione

| # | Priorità | Descrizione | Stato |
|---|----------|-------------|-------|
| D-5 | 🔴 | Rigenerare `EXPORT/linux/Prismalux_v3.0_Linux.zip` — attuale è v2.9 obsoleto | ⬜ |
| D-6 | 🟡 | Scrivere GitHub Release notes v2.9→v3.0 (44 feature nuove, 7 VULN chiuse) | ⬜ |

---

## ANDROID Qt6 (ANDROID/QT_ANDROID_Version/)

> App attuale: v2.9 (33 MB APK). 15 pagine implementate su ~23 pianificate.
> **Non usare Flutter né Kivy** — mantenere solo Qt6 nativo.

### Nuove pagine — Alta priorità

| # | Pagina | Descrizione | Ore | Dipende da | Stato |
|---|--------|-------------|-----|-----------|-------|
| A-1 | Assistente AI | 6 categorie × 8 azioni (Studio/Scrittura/Ricerca/Libri/Produttività/Documenti). Layout: `QComboBox` cat → griglia 2×N bottoni → I/O condiviso. Copre il 60% degli use case desktop. | 4-5h | — | ⬜ |
| A-2 | Oracle/Chat rapida | Pillole azione rapida (Matematica/Finanza/Dev/Scrivi…), allegato immagine vision, bottone 📊 Grafico | 3h | — | ⬜ |
| A-3 | GraphMemory mobile | Port di `graph_memory.h/cpp` — SQLite in `AppDataLocation`. API: addNode/edge/neighbours/search. Segnale `changed()`. **Sblocca A-4, A-5, B-3** | 3-4h | — | ⬜ |
| A-4 | Multi-Agente | MasterAgent → JSON piano → BFS sequenziale su `depends_on` → lista sub-task ⏳/✅/❌ | 5h | A-3 | ⬜ |
| A-5 | Hermes Memoria | File `user_knowledge.md` in AppData, bottone "Salva" su bolle AI, agente estrattore automatico, toggle inietta memoria nel system prompt | 4h | A-3 | ⬜ |
| A-6 | Grafico ChartWidget | Formula `y=f(x)` → campionamento → `QPainter` custom, zoom/pan touch, integrazione con A-2 | 3h | — | ⬜ |
| A-7 | FinanzaPage | TFR (rivalutazione FOI), Codice Fiscale D.M.1976+Belfiore, 730 IRPEF 2024, Regime Forfettario. Codice portabile dal desktop. | 4h | — | ⬜ |
| A-8 | FileAiPage | Analisi PDF/CSV/URL, Share Intent Android, riusa `extractPdfText()` già in `lavoro_page.cpp` | 3h | — | ⬜ |

### Estensioni pagine esistenti — Media priorità

| # | Pagina | Cosa aggiungere | Ore | Dipende da | Stato |
|---|--------|-----------------|-----|-----------|-------|
| B-1 | ChatPage | Storia SQLite sessioni, TTFT label ⚡, export TXT/MD, vision 📷 allega immagine | 3h | — | ⬜ |
| B-2 | LavoroPage | Tracker offerte (JSON), cover letter separata, foto profilo CV circolare | 3h | — | ⬜ |
| B-3 | ChatPage | Toggle 🧠 Hermes: inietta memoria, bottone "Salva" su bolle | 2h | A-3, A-5 | ⬜ |
| B-4 | MatematicaPage | 52 formule predefinite in `QComboBox` + bottone 🔀 shuffle | 2h | — | ⬜ |
| B-5 | RicercaPage | Analisi Fenomeni + upload file, Carta Astrale GPS auto | 3h | — | ⬜ |
| B-6 | SecurityPage | 4 agenti sequenziali (Injection/Segreti/Memoria/Config) + sintetizzatore | 4h | — | ⬜ |
| B-7 | Simulatore Algoritmi | BubbleSort/BFS/Dijkstra step-by-step con `QTimer` + `QPainter` | 3h | — | ⬜ |
| B-8 | Voice Loop | Dopo risposta AI → TTS legge → STT → nuova domanda (hands-free) | 2h | C-3 | ⬜ |
| B-9 | WAN Compute client | Campo IP:porta → `QTcpSocket` → lista task → esegui task | 2h | — | ⬜ |

### UX / Native Android — Bassa priorità

| # | Descrizione | File | Ore | Stato |
|---|-------------|------|-----|-------|
| C-1 | Drawer animazione smooth (`QPropertyAnimation` 220ms OutCubic) | `mainwindow.cpp` | 1h | ⬜ |
| C-2 | Auto-update notifica GitHub API — badge 🆕 in header dopo 10s avvio | `mainwindow.cpp` | 1h | ⬜ |
| C-3 🔴 | `RECORD_AUDIO` in AndroidManifest + richiesta runtime — **blocca B-8 e AudioPage su Android ≥ 6** | `AndroidManifest.xml` | 30m | ⬜ |
| C-4 | Share Intent (`ACTION_SEND */*`) → forward a FileAiPage | `AndroidManifest.xml` + `mainwindow.cpp` | 1h | ⬜ |
| C-5 | GPS Carta Astrale — `QGeoPositionInfoSource` → pre-compila lat/lon | `ricerca_page.cpp` | 1h | ⬜ |
| C-6 | Shuffle 52 formule matematica — `QRandomGenerator` + flash animazione | `matematica_page.cpp` | 1h | ⬜ |
| C-7 | ThermalMonitor badge 🌡️ in header (verde/arancio/rosso) — `ThermalMonitor` già esiste | `mainwindow.cpp` | 30m | ⬜ |
| C-8 | Test suite Android: GraphMemory, FinanzaPage, MultiAgent, FileAi, Assistente | `tests/test_mobile_logic.cpp` | 3h | ⬜ |

### APK

| # | Priorità | Descrizione | Stato |
|---|----------|-------------|-------|
| APK-1 | 🔴 | Rebuild APK v3.0 dopo completamento A-1÷A-7 | ⬜ |
| APK-2 | 🟡 | Aggiornare `versionName` in `AndroidManifest.xml` da 2.9 a 3.0 | ⬜ |

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

> 13/20 tab implementate. Gap residuo: 7 funzionalità desktop non ancora sul web.

| # | Priorità | Funzionalità mancante | Piano |
|---|----------|-----------------------|-------|
| W-1 | 🟡 | **Byzantino** — radio/select in tab `age` + endpoint `/api/byzantine` nel LanServer | ⬜ |
| W-2 | 🟡 | **Agente Autonomo** — tab `aut` + endpoint `/api/autonomous` SSE, loop ReAct step-by-step | ⬜ |
| W-3 | 🟡 | **Sfida!** — tab `sfd` + endpoint `/api/quiz?cat=ccna\|gen`, timer, punteggio sessionStorage | ⬜ |
| W-4 | 🟢 | **Matematica avanzata** — `/api/math/sequence` e `/api/math/constants` + KaTeX render nel tab `mat` | ⬜ |
| W-5 | 🟡 | **730 / P.IVA** — sezioni nel tab `fin` o endpoint `/api/finanza/730` e `/api/finanza/piva` | ⬜ |
| W-6 | 🟡 | **Security Analyzer** — tab `sec` + endpoint `/api/security` (4 agenti → SSE sintesi) | ⬜ |
| W-7 | 🟢 | **Sintetizzatore** — verificare stato tab `osc` (WebAudio già implementato, potrebbe essere completo) | ⬜ |

---

## MATRICE STATO COMPLESSIVO

| Funzionalità | Desktop | Android | Web |
|---|---|---|---|
| Chat singola | ✅ | ✅ | ✅ |
| Pipeline agenti | ✅ | ✅ | ✅ parz. |
| Byzantino | ✅ | ✅ | ⬜ W-1 |
| Agente Autonomo | ✅ | ✅ | ⬜ W-2 |
| Sfida! | ✅ | ✅ | ⬜ W-3 |
| Multi-Agente GraphMemory | ✅ | ⬜ A-3/A-4 | ✅ |
| Assistente categorie | ✅ | ⬜ A-1 | ✅ |
| Oracle/Chat rapida | ✅ | ⬜ A-2 | ✅ |
| Hermes memoria | ✅ | ⬜ A-5 | — |
| RAG + Grafo RAG | ✅ | ✅ | ✅ |
| Finanza (TFR/730/PIVA/CF) | ✅ | ⬜ A-7 | ⬜ W-5 |
| Ricerca strutturata | ✅ | ✅ | ✅ |
| FileAI (PDF/CSV) | ✅ | ⬜ A-8 | ✅ |
| Matematica avanzata | ✅ | ✅ base | ⬜ W-4 |
| Security Analyzer | ✅ | ⬜ B-6 | ⬜ W-6 |
| Grafico ChartWidget | ✅ | ⬜ A-6 | ✅ |
| Simulatore Algoritmi | ✅ | ⬜ B-7 | — |
| Voice Loop hands-free | ✅ | ⬜ B-8 | — |
| TTS / STT | ✅ | ✅ | ✅ |
| WAN Compute | ✅ | ⬜ B-9 | — |
| Cron scheduler | ✅ | ✅ | ✅ |
| Impara / Quiz | ✅ | ✅ | ✅ |
| Lavoro AI | ✅ | ✅ | ✅ |
| OBS / AppController | ✅ | ✅ | ✅ |
| BLE / LAN sync | ✅ | ✅ | — |
| Bioinformatica | ✅ | ⚪ skip | ⚪ skip |
| Stable Diffusion | ✅ | ⚪ skip | ⚪ skip |
| Lint codice | ⬜ D-1 | — | — |
| TTFT header | ⬜ D-2 | ⬜ B-1 | — |
| Sintetizzatore | ✅ | ✅ | ⬜ W-7 verifica |

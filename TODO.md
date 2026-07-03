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
| D-8 | 🟡 | Refactoring file monolitici, split in moduli per responsabilità (pattern `main_ai_*.cpp`/`settings_*.cpp`). ✅ 6/6 fatti: FILE 1 `main_graph_canvas.cpp` (4499→9 moduli) ✅, FILE 2 `main_programming.cpp`+`_slots.cpp` (3387+1963→7 moduli, `buildDriverKernelTab` scomposta) ✅, FILE 3 `main_app_controller.cpp`+`_slots.cpp` (3092+2691→9 moduli per feature) ✅, FILE 5 `main_tools.cpp` (2572→557, 6 moduli) ✅, FILE 4 `mainwindow.cpp` (2834→423, 6 moduli: header/monitor/chat/settings/tabs/backend + `ResourceGauge` estratto in `widgets/`) ✅ | `gui/pages/main_graph_canvas.cpp` + altri sopra | ✅ |
| D-9 | 🔴 | Avvio lento/freeze diagnosticato via `strace`+registrazione schermo: (1) `findPython()` non cacheata (82 call-site, probe bloccante ripetuto) → cache statica come `findDocker()`; (2) `startMcpDiscovery()` lanciava ~50 subprocess Python in sequenza 500ms dopo l'avvio anche senza uso MCP → reso on-demand (apertura pannello Tool Lenti o primo chat con tool); (3) bug processi Python orfani — `waitForStarted()` in timeout senza `kill()` prima di `deleteLater()`, fix in 7 punti; (4) freeze "Non risponde" KWin da inizializzazione Chromium/`LatexView` eager in Analisi 1/2 (Matematica) durante pre-build tab in background → reso lazy on-demand; (5) bug bolla utente: evidenziatore selezione "cotto" nello sfondo HTML statico (`extractInputHtml` mancava `hasProperty(BackgroundBrush)`, stesso bug già noto per il colore testo) | `prismalux_paths.h`, `main_ai.cpp`, `main_ai_panels.cpp`, `main_ai_pipeline.cpp`, `main_ai_tools.cpp`, `main_ai_p.h`, `main_math.h/.cpp`, `main_math_analisi.cpp`, `mainwindow_monitor.cpp`, `settings_llm.cpp`, `main_app_controller_devagent.cpp`, `main_mcp_manager.cpp` | ✅ 2026-07-03 |
| D-10 | 🟡 | Tool zero-LLM aggiuntivi per "Intelligenza Artificiale": (1) `_inject_finance()` — validazione IBAN (mod-97, verificato contro esempi noti) + Codice Fiscale (D.M.1976, stesse tabelle di `pratico_calcs.h`, con decodifica data nascita/sesso) + sconti/aumenti/scorporo-aggiunta IVA/percentuali; (2) `_inject_generator()` — UUID v4, hash MD5/SHA1/SHA256/SHA512, password casuali (charset senza ambigui 0/O/1/I/l); (3) conversioni cucina aggiunte a `_inject_science()` — ml↔grammi per 9 ingredienti comuni, temperatura forno→Fahrenheit/Gas Mark, cucchiaino/cucchiaio/tazza→ml; (4) tool `cambio_valuta` — unico che richiede rete reale (frankfurter.app, fonte BCE, no API key), tutti gli altri intercettano il messaggio PRIMA che arrivi all'LLM (zero token). QR evento calendario (D-9 successivo, sessione precedente) resta l'unico tool con vera conversazione multi-turno orchestrata dal modello | `main_ai_tools.cpp`, `main_ai_math.cpp`, `main_ai_p.h`, `main_ai_pipeline.cpp` | ✅ 2026-07-03 |
| D-11 | 🟢 | Help "cosa sai fare?" e formattazione report: (1) `_inject_help()` — intercetta frasi tipo "cosa sai fare"/"aiuto"/"comandi" e risponde con una tabella Markdown statica di tutte le domande rapide zero-LLM (D-10), zero token; passa da `markdownToHtml()` invece di `buildLocalBubble()` (contenuto multi-riga con tabella, non plain text); (2) istruzione di formattazione aggiunta al system prompt base (`_buildSys`) — il modello usa tabelle/elenchi Markdown per report, confronti, elenchi di dati invece di paragrafi lunghi (il renderer HTML esistente in `markdownToHtml` già supportava tabelle vere, mancava solo l'istruzione) | `main_ai_tools.cpp`, `main_ai_p.h`, `main_ai_pipeline.cpp`, `main_ai_math.cpp` | ✅ 2026-07-03 |
| D-12 | 🟡 | `guardiaMath()`: aritmetica scritta a parole non funzionava — "cinque+5" (nessun prefisso "quanto fa") non veniva riconosciuto, operatori a parole ("più" invece di "+", "sottrai 10, 3" invece di "10-3") non erano normalizzati (solo i NUMERI a parole lo erano, via `normalizeNumbers`). Fix: `normalizeOperatorWords()` (più/meno/per/diviso → simbolo, applicata SOLO a espressioni già isolate, mai alla frase libera — "più" è anche avverbio comune); forme verbali "sottrai/somma/moltiplica/dividi A, B" con parsing a 2 operandi; espressione pura senza prefisso se contiene un operatore esplicito | `main_ai_math.cpp` | ✅ 2026-07-03 |
| D-13 | 🟢 | `_inject_help()` mostra ora anche un esempio di grafico, scelto a caso (`QRandomGenerator`) tra 8 formule dimostrative ad ogni richiesta di "cosa sai fare?". Verificato prima quali dei 17 `ChartType` (`main_graph.h`) sono davvero raggiungibili da una riga di chat: solo il plot Cartesiano via `FormulaParser::tryExtract()` ("grafico di FORMULA" / "y = FORMULA", zero token) lo è — le altre 16 tipologie (Torta, Istogramma, Radar, Candlestick, Heatmap, ecc.) richiedono dati strutturati dal form del canvas dedicato in Matematica/Grafico, non un one-liner. L'help lo dice esplicitamente invece di promettere una scorciatoia chat inesistente per tutte e 17 | `main_ai_tools.cpp` (`_inject_help`) | ✅ 2026-07-03 |
| D-14 | 🟡 | 10 nuove "domande classiche" zero-LLM: (1) età esatta da data di nascita + giorni al prossimo compleanno; (2) giorno della settimana di una data; (3) fuso orario (tabella 18 città → IANA timezone via `QTimeZone`); (4) giorni lavorativi tra due date (esclusi sab/dom, non festività); (5) numeri romani ↔ arabi (con verifica round-trip anti-falsi-positivi, verificato indipendentemente); (6) IMC/BMI + fascia; (7) geometria: area triangolo/rettangolo, volume/superficie cubo/cilindro; (8) Partita IVA — checksum ufficiale verificato indipendentemente (bug trovato e corretto nella prima verifica: sommava anche la cifra di controllo nel totale); (9) interesse composto + rata mutuo (ammortamento francese); (10) `_inject_textstats()` nuovo — conteggio parole/caratteri/frasi + stima tempo lettura da testo dopo ":". Tutte agganciate alla guardia zero-token e alla catena di fallback; tabella help (D-13) aggiornata con le nuove righe | `main_ai_tools.cpp`, `main_ai_math.cpp`, `main_ai_p.h`, `main_ai_pipeline.cpp` | ✅ 2026-07-03 |
| D-15 | 🟡 | Sezione "🧊 Solidi 3D" nel tab Grafico (visibile solo per tipo Scatter3D): 5 pulsanti (Cubo/Cilindro/Cono/Toro-ciambella/Parallelepipedo) + 2 spinbox dimensioni → genera la mesh (griglia continua u,v, riusa interamente il rendering Solid3D già esistente per Scatter3D, nessuna nuova logica di disegno) e mostra la formula dell'area totale nello status label. Cubo/Parallelepipedo usano proiezione "cubemap" (sfera UV normalizzata per norma-infinito) per ottenere spigoli vivi restando una griglia continua compatibile col renderer a griglia singola. **Non verificato visivamente** (impossibile lanciare la GUI dall'ambiente sandboxed) — solo compilazione riuscita, verificare a schermo | `main_graph.h`, `main_graph.cpp` | ✅ 2026-07-03 (da verificare a video) |
| D-16 | 🟢 | ⬜ Aperto — altre "domande classiche" possibili (solo elencate, non implementate): statistica base su lista di numeri ("media/mediana/moda/deviazione standard di 3,5,7,9,2"); data di Pasqua di un anno (algoritmo di Gauss); fase lunare di una data; conversione in base numerica arbitraria "converti 255 in base 7" (oggi solo bin/ott/hex specifici); somma progressione aritmetica/geometrica; conversione lira→euro (tasso fisso 1936.27); scadenza contratto/garanzia "tra X anni da oggi" (variante di età/mancano già esistenti) | `main_ai_tools.cpp` / `main_ai_math.cpp` (da scegliere in base al dominio) | ⬜ |
| D-18 | 🟡 | ⬜ Aperto — **manca lock single-instance**: `Prismalux_GUI` non usa `QLocalServer`/`QSharedMemory`, quindi ogni avvio (es. doppio click impaziente sull'icona durante il boot lento di D-9) crea un processo completamente nuovo invece di attivare la finestra già aperta. Scoperto verificando T-D9: con 3 avvii ravvicinati si sono ottenute 3 finestre "Centro di Controllo" indipendenti che si contendevano la CPU (86-88% ciascuna per ~20s) — KWin non lo segnala mai come "Non risponde" ma è comunque uno spreco di risorse e probabile causa della sensazione di blocco riportata dall'utente. Fix: `QLocalServer` in `main()` prima di creare `QApplication`, se già in ascolto invia un messaggio alla istanza esistente per `raise()`+`activateWindow()` ed esce | `main.cpp` (o dove vive `main()`) | ⬜ |
| D-17 | 🟡 | **Riuso algoritmi del Simulatore** (`main_simulator.h`) per domande classiche zero-LLM, selezione curata (~13, scelta scope "massimo valore" su indicazione utente). Resi `static` **e `public`** (erano `private`, serviva anche cambiare visibilità non solo aggiungere `static` — spostati in un blocco `public:` dedicato) 16 metodi: `genGCD/genExtGCD/genFastPow/genPrimeFactors/genMillerRabin/genPascalTriangle/genFibonacciDP/genCatalan/genMonteCarloPi/genCollatz/genKaratsuba/genSieve/genSieveSundaram/genStockProfit/genCountInversions/genLinearSearch/genKMP`. Scoperti 3 problemi di riuso **prima** di scrivere codice: `genTrappingRain`/`genMaxCircularSubarray` randomizzano l'array in ingresso (ignorano l'input reale utente) → esclusi; `genNQueens` limita N≤5 e usa una tabella hardcoded per il conteggio soluzioni → escluso; `genLCS()`/`genEditDistance()` non hanno parametri (stringhe fisse per la demo) → riscritte standalone (`_editDistance`/`_longestCommonSubsequence`, DP pulita). `genTowerOfHanoi` non riusata direttamente (ricorre passo-per-passo, esploderebbe per N grande) — sostituita con formula chiusa 2^n-1. Primalità/somme/potenze/fattoriale già coperti da `guardiaMath()` esistente → non duplicati (verificato prima). Nuova `_inject_algo()`: MCD/MCM, fattorizzazione, Pascal riga N, Fibonacci N-esimo, Catalan N-esimo, Collatz, Torre di Hanoi, profitto azioni, inversioni, posizione in array, edit distance/LCS, ricerca pattern (KMP) | `main_simulator.h` (static+public), `main_ai_tools.cpp` (`_inject_algo`, `_editDistance`, `_longestCommonSubsequence`), `main_ai_p.h`, `main_ai_pipeline.cpp` | ✅ 2026-07-03 |

### ⬜ DA TESTARE MANUALMENTE — sessione 2026-07-03 (solo compilato, mai verificato a schermo/in chat reale)

| # | Cosa testare | Come | File/TODO collegato |
|---|---|---|---|
| T-D9 | Freeze "Non risponde" all'avvio (colonna che si comprime) | ✅ 2026-07-03 verificato via KWin scripting (`Window.unresponsive`): 3 avvii ravvicinati (simulazione doppio click) → CPU all'86-88% per ~20s ma `unresponsive` sempre `false`, mai innescato lo stato "Non risponde". Avvio singolo impiega >100s a mostrare finestra (init QtWebEngine background thread) ma resta responsive. **Scoperta nuova**: nessun single-instance lock (`QLocalServer`/`QSharedMemory` assente) — ogni click apre un processo indipendente invece di riportare in primo piano quello esistente; con più istanze la contesa CPU reale può dare l'impressione soggettiva di blocco anche se KWin non lo segnala mai come tale | D-9 |
| T-D15 | Sezione "🧊 Solidi 3D" in Grafico | Tab Matematica/Grafico → tipo "Scatter 3D" → dovrebbe comparire la sezione con 5 pulsanti; cliccarne uno e verificare che la forma sia visivamente corretta ruotando col mouse | D-15 |
| T-D14a | Età/giorno settimana/fuso orario/giorni lavorativi | "quanti anni ho se sono nato il 15/03/1990", "che giorno era il 25/12/2020", "che ora è a New York", "giorni lavorativi tra 03/07/2026 e 15/08/2026" | D-14 |
| T-D14b | Numeri romani, IMC, geometria | "quanto è MCMXCIV", "1994 in romano", "imc per 70kg e 1.75m", "area di un rettangolo con base 5 e altezza 3", "volume di un cilindro con raggio 3 e altezza 5" | D-14 |
| T-D14c | Partita IVA, interesse composto, mutuo | "12345678903 è una p.iva valida?", "quanto diventano 1000 euro al 5% in 10 anni", "rata di un mutuo da 100000 euro al 3% in 20 anni" | D-14 |
| T-D14d | Statistiche testo | "quante parole ha: <incolla un paragrafo>" | D-14 |
| T-D17 | Algoritmi riusati dal Simulatore | "mcd tra 48 e 18", "fattorizzazione di 360", "decimo numero di fibonacci", "numero di catalan 5", "torre di hanoi con 8 dischi", "profitto massimo con prezzi 7,1,5,3,6,4", "quante inversioni in 3,1,2", "in che posizione è 7 in 3,7,9,2", "distanza di edit tra 'gatto' e 'catto'", "sottosequenza comune più lunga tra 'ABCBDAB' e 'BDCABA'" — verificare risposte corrette E che i regex riconoscano davvero le frasi (alta probabilità che qualche formulazione non matchi al primo colpo) | D-17 |
| T-help | Tabella help aggiornata | "cosa sai fare?" — verificare che la tabella markdown si veda formattata bene (non testo grezzo con pipe) e che l'esempio di grafico cambi ad ogni richiesta | D-13 |

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

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
| D-18 | 🟡 | **Lock single-instance + splash screen**: `QLocalSocket` probe su socket per-utente (`Prismalux_GUI_$USER`) subito dopo i metadata app — se un'istanza è già in ascolto la seconda esce in <300ms e la prima riceve `newConnection` → `SingleInstanceGuard` (classe locale in `main.cpp`, `main.moc`) fa `showNormal()+raise()+activateWindow()`. Socket stale da crash rimosso con `removeServer()` prima del `listen()`. In più: `QSplashScreen` (pixmap QPainter, logo+motto) mostrato PRIMA della costruzione di `MainWindow` con `processEvents()` — feedback immediato durante il boot lento (T-D9, >100s a freddo per QtWebEngine). **Fix flicker 2026-07-03 sera**: il primo `raise` usava `showNormal()` che de-massimizzava la finestra a tutto schermo (segnalato da Paolo: "la vedo, scompare, ricompare") → sostituito con `setWindowState(state & ~Qt::WindowMinimized)` + `show()` che preserva maximized/fullscreen. **Fix splash Wayland**: con un solo `processEvents()` lo splash non veniva MAI mappato (su Wayland la finestra esiste solo dopo il primo commit del buffer, che richiede l'evento expose; il ctor di MainWindow blocca subito dopo) → loop `processEvents(WaitForMoreEvents, 20ms)` max ~1.2s finché `windowHandle()->isExposed()` | `main.cpp` | ✅ 2026-07-03 (da verificare a video) |
| D-19 | 🟡 | **Fallback TLS→HTTP ora visibile in UI**: `LanServer::start()` ripiega silenziosamente su HTTP se openssl/cert falliscono (solo `qWarning`) — il token Bearer viaggia in chiaro senza che l'utente lo sappia. Aggiunto `isTlsRequested()` a `LanServer` e stato ambra `⚠️ TLS non disponibile: token in chiaro` in `onLanServerStatusChanged()` quando richiesto≠attivo. Inoltre `candidature_macros/` aggiunta al `.gitignore` (le macro salvano valori `letterale:` potenzialmente sensibili in JSON dentro il repo) | `lan_server.h`, `main_lan_wan.cpp`, `.gitignore` | ✅ 2026-07-03 |
| D-17 | 🟡 | **Riuso algoritmi del Simulatore** (`main_simulator.h`) per domande classiche zero-LLM, selezione curata (~13, scelta scope "massimo valore" su indicazione utente). Resi `static` **e `public`** (erano `private`, serviva anche cambiare visibilità non solo aggiungere `static` — spostati in un blocco `public:` dedicato) 16 metodi: `genGCD/genExtGCD/genFastPow/genPrimeFactors/genMillerRabin/genPascalTriangle/genFibonacciDP/genCatalan/genMonteCarloPi/genCollatz/genKaratsuba/genSieve/genSieveSundaram/genStockProfit/genCountInversions/genLinearSearch/genKMP`. Scoperti 3 problemi di riuso **prima** di scrivere codice: `genTrappingRain`/`genMaxCircularSubarray` randomizzano l'array in ingresso (ignorano l'input reale utente) → esclusi; `genNQueens` limita N≤5 e usa una tabella hardcoded per il conteggio soluzioni → escluso; `genLCS()`/`genEditDistance()` non hanno parametri (stringhe fisse per la demo) → riscritte standalone (`_editDistance`/`_longestCommonSubsequence`, DP pulita). `genTowerOfHanoi` non riusata direttamente (ricorre passo-per-passo, esploderebbe per N grande) — sostituita con formula chiusa 2^n-1. Primalità/somme/potenze/fattoriale già coperti da `guardiaMath()` esistente → non duplicati (verificato prima). Nuova `_inject_algo()`: MCD/MCM, fattorizzazione, Pascal riga N, Fibonacci N-esimo, Catalan N-esimo, Collatz, Torre di Hanoi, profitto azioni, inversioni, posizione in array, edit distance/LCS, ricerca pattern (KMP) | `main_simulator.h` (static+public), `main_ai_tools.cpp` (`_inject_algo`, `_editDistance`, `_longestCommonSubsequence`), `main_ai_p.h`, `main_ai_pipeline.cpp` | ✅ 2026-07-03 |

| D-20 | 🔴 | **SEGV alla chiusura** (coredump 2026-07-03 22:19, PID 10869): `~VoiceClonerWidget` → `deleteChildren()` → `~QProcess` di una probe di `checkTtsInstalled()` ancora attiva emette `finished()` sincrono via `waitForFinished()` → lambda chiama `applyBackend()` → `QLabel::setText()` su label già distrutta. Il context `this` nel connect NON protegge durante la distruzione dei figli. Fix: nel distruttore `findChildren<QProcess*>()` → `disconnect(this)` + `blockSignals(true)` + `kill()` + `waitForFinished(1000)` — copre anche le probe anonime che il vecchio dtor ignorava. **Stesso pattern a rischio in altri widget con QProcess figli + lambda UI su finished()** — audit da fare (vedi D-21) | `widget_voice_cloner.cpp` | ✅ 2026-07-03 |
| D-22 | 🔴 | **Web app irraggiungibile via QR/URL copiato**: il server LAN è in HTTPS (TLS self-signed default) ma `onQrConnectBtnClicked`/`onUpdateQrInline` hardcodavano `http://` → il browser non riceve risposta (curl: 000 su http, 401/302→200 su https). `serverScheme()` esisteva già ed era usato solo dai QR APK/pagina. Fix: schema dinamico nei 2 URL `/web` + URL `/apk` di ManutenzioneePage. Verificato con curl+cookie jar: 302 Set-Cookie `p_session` → 200. **Round 2 (sera)**: il QR inline restava `http` anche col fix — viene generato alla costruzione della pagina, PRIMA che `start()` attivi il TLS, e mai rigenerato → aggiunta chiamata `onUpdateQrInline()` in fondo a `onLanServerStatusChanged()` | `main_lan_wan.cpp`, `main_maintenance_lan.cpp` | ✅ 2026-07-03 |
| D-21 | 🟡 | **Audit pattern QProcess-in-dtor** — mappati 55 file con `new QProcess`; scoperto che il rischio concreto non è "tab transitoria vs persistente" come si pensava inizialmente (quasi tutte le tab principali sono costruite una volta e mai distrutte fino alla chiusura app — `ensureSettingsDialog()`: `WA_DeleteOnClose=false`, `if(m_impDlg) return`), ma la chiusura dell'app stessa MENTRE un probe/download è ancora attivo — esattamente lo scenario con cui D-20 è stato scoperto (coredump reale, non teorico). Fix applicati (pattern D-20: `findChildren<QProcess*>()` — ricorsivo, copre anche nipoti — + `disconnect(this)` + `blockSignals(true)` + `kill()` + `waitForFinished(1000)`, PRIMA della distruzione automatica dei figli): (1) `StableDiffusionWidget` — distruttore esistente gestiva solo `m_sdProc`, non la probe anonima di `checkDiffusers()`; (2) `AiScriptWidget` (classe base di `AvogadroWidget`/`BiocondaWidget`/`RDKitWidget` — nessuna delle 3 aveva un distruttore, un solo fix nella base le copre tutte); (3) `CodeInterpreterWidget` — copre sia `m_dockerProc` (figlio diretto) sia il `QProcess` interno di ogni `PythonExec` ancora vivo (nipote, preso comunque da `findChildren` ricorsivo senza toccare `python_exec.h`); (4) `McpManagerPage` — 4 punti di creazione (`m_venvProc`/`m_installProc`/`m_testProc` + probe anonime `mcp_call`), nessun distruttore esistente; (5) `ImpostazioniPage` — nessun distruttore esistente, un solo fix copre tutte le decine di sotto-tab in `settings_*.cpp` (Voce & Audio/download Piper, Aggiornamenti Sistema, ecc.) più le sotto-pagine `ManutenzioneePage`/`PersonalizzaPage` innestate. Verificato: suite `McpManager`/`DockerSandbox`/`ImpostazioniPage`/`SimulatoreAlgos` ancora verdi dopo i 5 fix; ctest completo lanciato con 7 fallimenti pre-esistenti e scollegati (LanServer/LanWanCore: nessuna risposta di rete in sandbox; AppController→LavoroPage: fixture offerte assente; SttWhisper/SttWhisperLive/PerceptorScripts: timeout trascrizione Whisper in ambiente senza hardware audio reale — nessuno di questi file è tra i 6 toccati oggi). **Non coperti** (rischio residuo basso, solo a chiusura app, non urgente): resto delle ~50 tab principali elencate nell'audit (Programmazione/Ricerca/AppController/ecc.), stesso pattern teorico ma non ancora patchate — vedi lista completa nell'audit di sessione | `widget_stable_diffusion.cpp`, `widget_ai_script_base.h/.cpp`, `widget_code_interpreter.h/.cpp`, `main_mcp_manager.h/.cpp`, `settings_main.h/.cpp` | ✅ 2026-07-04 (5 fix concreti, resto rischio residuo basso) |

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
| T-D18 | Splash + single-instance | Avviare `Prismalux_GUI`: lo splash 🍺 deve comparire subito (non dopo 100s). A finestra aperta, lanciare di nuovo il binario: il secondo processo deve uscire subito e la finestra esistente tornare in primo piano | D-18 |
| T-D19 | Avviso fallback TLS | Rinominare temporaneamente il cert in `~/.prismalux/` o disinstallare openssl → avviare il server LAN → lo stato deve essere ambra "⚠️ TLS non disponibile: token in chiaro" invece del verde normale | D-19 |
| T-D20 | SEGV chiusura risolto | Aprire Multimedia → Clona Voce (per creare il widget), chiudere subito l'app: nessun coredump (`coredumpctl list Prismalux_GUI` non deve mostrare nuove voci) | D-20 |
| T-D33 | Tool D-33 nella chat reale (modello tool-capable) | "calcola l'mcd tra 48 e 18 col tool" / lasciare che il modello scelga da solo su domande libere per: `algoritmo` (incluso `hanoi_passi` con 6 dischi e `nqueens` con N=8), `codice_fiscale` (Rossi Mario, 01/05/1985, M, Roma), `finanza_calcola` (mutuo 100000€ al 3% in 20 anni), `valida_documento` (IBAN/P.IVA/CF), `carta_astrale` (data+ora+lat/lon), `converti` ("200 ml di farina in grammi"), `disegna_grafico` ("traccia sin(x) tra -10 e 10 con un tool") — verificare che il pannello ⚡ Tool Veloci mostri le 7 nuove voci e che `disegna_grafico` apra davvero il pannello Grafico (unico non verificabile in sandbox: `tryShowChart()` tocca la UI) | D-33 |

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

## QUERY ROUTING & ALGORITMI COME TOOL AI (piano 2026-07-03)

> Contesto: la catena pre-LLM attuale (`main_ai_pipeline.cpp:190-350`) fa guardiaMath → guardia Grafico →
> guardia Help → `_inject_science/date/finance/generator/textstats/algo` → iniezione prefissi → `classifyQuery` → chat.
> D-17 ha già esposto gli algoritmi "semplici" via regex zero-LLM. Due binari paralleli, stesso algoritmo condiviso:
> **guardia regex** = istantanea, zero token, solo frasi note · **function tool** = il modello lo chiama con argomenti
> strutturati, copre formulazioni libere/multi-turno che i regex mancano. Registry: `mkTool()` in `main_ai_tools.cpp:1293`.

### Stadio A — Normalizzazione (prima di tutte le guardie)

| # | Priorità | Descrizione | File | Stato |
|---|----------|-------------|------|-------|
| D-23 | 🔴 | **Matching tollerante ai typo** — `correctGuardTypos()` in `main_ai_math.cpp` (dichiarata in `main_ai_p.h`), chiamata subito dopo `_sanitize_prompt()` nei 3 punti d'ingresso query (`runPipeline`, `runByzantine`, `runMathTheory` — NON su `m_docContext`/RAG, che non passa dalla catena guardie). Selezione curata 12 trigger (fibonacci, catalan, collatz, hanoi, fattorizzazione, pascal, mcd, mcm, imc, mutuo, prestito, ammortamento). Distanza di Levenshtein con soglie **scalate per lunghezza**, non un flat ≤2: trigger ≤8 caratteri → max 1 refuso + stessa lunghezza obbligatoria se ≤5 caratteri (equivale a distanza di Hamming); trigger >8 caratteri (fibonacci, fattorizzazione, ammortamento) → max 2 refusi con Levenshtein pieno. **2 falsi positivi trovati e corretti durante la verifica** (test standalone fuori da ctest, guardiaMath non ha suite dedicata): con soglia flat ≤2, "anni" (comunissimo) veniva riscritto in "hanoi" e "muto" in "mutuo" — la stessa-lunghezza obbligatoria sui trigger corti elimina entrambe le collisioni pur continuando a correggere refusi reali ("fibonaci"→fibonacci, "fattorizazione"→fattorizzazione, "colatz"→collatz, "hanoy"→hanoi, "prestitto"→prestito, "catalán"→catalan) | `main_ai_math.cpp`, `main_ai_p.h`, `main_ai_pipeline.cpp`, `main_ai_byzantine.cpp` | ✅ 2026-07-04 |
| D-24 | 🟡 | **Normalizzazione numeri/formati IT** — verificato che `normalizeNumbers()` esistente converte SOLO numeri scritti in lettere ("cinque"→"5"), nessuna gestione di virgola/migliaia/date nonostante il nome. **Bug reale scoperto durante la verifica**: `guardiaMath()` usa `strtod()` grezzo (formato C, punto non virgola) — "quanto fa 3,5 + 2,1" veniva troncato silenziosamente a "3" (virgola non riconosciuta come parte del numero). Aggiunta `normalizeItFormats()` (`main_ai_math.cpp`, dichiarata in `main_ai_p.h` come D-23, chiamata da `_sanitize_prompt()` — stesso unico punto d'ingresso, nessuna modifica ai 3 chiamanti Pipeline/Byzantino/Matematico Teorico): (1) punto migliaia + virgola decimale opzionale ("100.000,50"→"100000.50"); (2) virgola decimale residua ("3,5"→"3.5") **esclusa** se il numero condivide la virgola con un altro adiacente (lista tipo "3,5,7,9,2" lasciata intatta — è un caso diverso, vedi D-16 statistica-su-lista non ancora implementato, così non si rompe nel frattempo: euristica a lookaround, verificata su liste da 2/3/5 elementi); (3) date con mese in lettere ("15 marzo 1990"→"15/03/1990", solo mesi per esteso — le abbreviazioni a 3 lettere sono escluse, troppo ambigue in prosa libera es. "mar"=martedì). **Guardia IPv4 dedicata** (`_ipv4Spans()`): il progetto usa IP LAN ovunque (telecamera WIBY 192.168.1.222, scanner rete, WAN compute) e un indirizzo con tutti gli ottetti da 3 cifre (es. "192.168.100.200") è altrimenti indistinguibile da un numero a migliaia — verificato che senza questa protezione il punto verrebbe rimosso mutilando l'IP; ogni span IPv4 rilevato (con controllo ottetti ≤255) è escluso dalla conversione migliaia. Nuova suite `test_ai_math.cpp` (23 test, CAT-A÷F): virgola semplice, liste non toccate, migliaia, **IP mai mutilato** (incluso caso ottetti-tutti-a-3-cifre + IP con porta + coesistenza IP/migliaia nella stessa frase), date mese-lettere (incluso giorno non valido lasciato intatto), nessuna regressione su testo senza numeri. Verificato anche `MatematicaPage`/`AgentiPipeline`/`AgentiByzantine`/`AgenteAutonomo`/`AgentsConfigDialog` (0 regressioni) | `main_ai_math.cpp`, `main_ai_p.h`, `gui/tests/test_ai_math.cpp` | ✅ 2026-07-04 |

### Stadio B — Nuove guardie zero-LLM

| # | Priorità | Descrizione | File | Stato |
|---|----------|-------------|------|-------|
| D-25 | 🟡 | **Cache risposte esatte** — `_lookupResponseCache`/`_saveResponseCache` (`main_ai_pipeline.cpp`): hash SHA-256 della query normalizzata (lowercase + spazi collassati) → risposta AI precedente, persistita in `~/.prismalux/response_cache.json` (cap 300 voci, evict le più vecchie per timestamp). Guardia inserita in `runPipeline()` subito dopo la catena zero-LLM esistente, solo in chat singola (`numAgents()<=1` — pipeline/Byzantino esclusi). Bolla cachata con link "🔄 Rigenera" (nuovo schema URL `regen:USERIDX:B64`, stesso troncamento di `retry:` ma imposta `m_bypassResponseCache` prima di ripresentare la domanda, altrimenti richiederebbe sempre la stessa risposta). Salvataggio agganciato allo stesso punto/condizione di `m_ctxSingle->appendPair()` in `advancePipeline()`. Verificato con test standalone: hit case-insensitive, normalizzazione spazi multipli, query diverse non collidono, eviction mantiene le voci più recenti | `main_ai_pipeline.cpp`, `main_ai.h` (`m_bypassResponseCache`), `main_ai_slots.cpp` (handler `regen:`) | ✅ 2026-07-04 |
| D-26 | 🟡 | **Lookup memoria personale zero-LLM** — verificato prima che `P::prependKnowledge()` inietta già l'INTERO `user_knowledge.md` nel system prompt ad ogni richiesta (il modello lo vede sempre): una guardia zero-LLM non aggiunge informazione, serve solo a rispondere con zero token/latenza sui lookup diretti e inequivocabili. Trigger deliberatamente stretto per non rischiare risposte peggiori di quelle del modello (che ha comunque il contesto completo): (1) verbo di domanda esplicito da lookup ("qual è", "dimmi", "ricordami", "cos'è"...); (2) almeno 2 parole chiave non-stopword nella domanda; (3) **esattamente una** riga del file contiene tutte quelle parole chiave — 0 o più righe candidate (ambiguo) → fallback sicuro, si lascia decidere al modello, mai un falso positivo che nasconda informazione. Logica di matching isolata in `_knowledgeLookup(task, knowledge)` (nucleo puro, nessun I/O — `knowledge` passato esplicitamente) separata da `_inject_knowledge(task)` (wrapper che legge il vero file) proprio per poter testare senza dipendere/scrivere nel vero `user_knowledge.md` dell'utente (dati personali, non deterministico, path reale `TOOL_TIP/KNOWLEDGE_USER/user_knowledge.md` — la voce CLAUDE.md "KNOWLEDGE_USER/" è una semplificazione del path). **Bug regex reale trovato e corretto durante la verifica** (test standalone fuori ctest): `\b` finale del trigger dopo "è"/"cos'è" non scattava mai — senza `QRegularExpression::UseUnicodePropertiesOption`, PCRE tratta `\b` come confine ASCII-only e le lettere accentate non contano come caratteri di parola, quindi "qual è" (frase-guida usata nell'esempio originale del TODO) non veniva MAI riconosciuta finché non aggiunta l'opzione. **Nota per audit futuro**: lo stesso pattern rischioso (`\b...è...\b` senza `UseUnicodePropertiesOption`) è presente in ~20 altre regex di `main_ai_tools.cpp` (righe individuate con grep, non verificate una per una in questa sessione — potrebbero già essere innocue se l'accento non è mai adiacente a un `\b`, ma andrebbero controllate singolarmente prima di assumerlo). Nuova suite `test_ai_knowledge_lookup.cpp` (24 test, CAT-A÷F, knowledge fittizio mai il file reale) | `main_ai_tools.cpp` (`_inject_knowledge`, `_knowledgeLookup`), `main_ai_p.h`, `main_ai_pipeline.cpp`, `gui/tests/test_ai_knowledge_lookup.cpp` | ✅ 2026-07-04 |
| D-16 | — | (già aperto sopra) statistica su lista, Pasqua di Gauss, fase lunare, base N, progressioni, lira→euro, scadenze | — | ⬜ |

### Stadio C — Routing modello/parametri

| # | Priorità | Descrizione | File | Stato |
|---|----------|-------------|------|-------|
| D-27 | 🟡 | **Dominio → modello** — verificato prima che la "SmartRouter API" esistente (`AiClient::setSmartRouter`/`decideCloud()`) instrada LOCALE↔CLOUD, scopo diverso da questo TODO (routing TRA modelli locali) — non riusata, solo `detectQueryDomain()`/`QueryDomain` (già esistenti, mai chiamati fuori da un hint testuale in Matematica). Nuovo bit `CapCoding` in `ModelCap`/`modelCapabilities()` (`prismalux_paths.h`) + `isCoderModel()` (substring `coder/codellama/starcoder/codegemma/codeqwen/codestral/granite-code/stable-code`), stesso stile di `isVisionModel` esistente. Logica di routing divisa in due livelli: `_pickRoutedModel(autoRoutingEnabled, hasImage, domain, installedModels, fallback)` — nucleo puro testabile, nessuna dipendenza UI — e `AgentiPage::_routedModel(task, fallback)`, wrapper che legge lo stato reale (checkbox, combo modelli, `m_imgBase64`). Priorità: immagine allegata → modello vision installato (prevale anche sul dominio codice: senza vision la richiesta fallirebbe comunque); dominio `DomainCoding` → modello coder installato; dominio generale/STEM → nessun cambio, il "modello leggero" è già la scelta manuale dell'utente. Applicato in `runAgent()` (`main_ai_pipeline.cpp`) con `m_ai->setModel()` (**non** `setBackend()`: quest'ultimo emette `modelChanged()` che risincronizzerebbe permanentemente il combo tramite `onAiModelChanged`, violando il requisito "mai scavalcare la scelta manuale" — scoperto durante l'esplorazione, `setModel()` è il meccanismo già pensato dal codice per override temporanei) — `selectedModel` aggiornato di conseguenza così le label mostrate (header agente, bolla) riflettono il modello davvero usato, non quello nominale del combo. UI: checkbox "🧭 Auto" (`m_chkAutoRouting`) accanto al selettore LLM (`buildToolbarLLMSelector`), persistita in QSettings `ai/autoModelRouting` (default OFF — opt-in, non tocca mai il comportamento esistente se lasciata disattivata). Nuova suite `test_ai_routing.cpp` (30 test, CAT-A÷E: routing OFF, immagine con priorità su codice, dominio codice, dominio generale/STEM invariato, riconoscimento nomi modello); verificate anche `AiClient`/`AgentiPipeline`/`MatematicaPage`/`AgentiByzantine`/`AgenteAutonomo`/`ImpostazioniPage` (0 regressioni) | `prismalux_paths.h`, `pages/main_ai.h`, `pages/main_ai_ui.cpp`, `pages/main_ai_pipeline.cpp`, `pages/main_ai_p.h`, `gui/tests/test_ai_routing.cpp` | ✅ 2026-07-04 |
| D-28 | 🟢 | **Rilevamento lingua query** — query in inglese → istruzione "answer in English" nel system prompt (e viceversa); evita risposte in lingua sbagliata dai modelli piccoli | `main_ai_pipeline.cpp` (`_buildSys`) | ⬜ |
| D-29 | 🟡 | **Pre-selezione function tools per categoria** — `_relevantHeavyTools()` (`main_ai_pipeline.cpp`) filtra SOLO i 7 tool "pesanti" di D-33 (algoritmo/codice_fiscale/finanza_calcola/valida_documento/carta_astrale/converti/disegna_grafico — descrizioni lunghe) in base a keyword nella query corrente; i tool "core" pre-D-33 (calc/ricerca/fetch_url/file/RAG/memoria/sub-agente/MCP) restano sempre disponibili se abilitati (generici/imprevedibili da keyword, descrizioni già brevi — filtrarli rischiava di rompere funzionalità per un guadagno minimo). **Fallback di sicurezza**: se nessuna categoria matcha la query, si includono comunque tutti e 7 i tool pesanti — mai negare un tool per una formulazione imprevista dalle keyword. `buildEnabledTools()` ora richiede il testo della query (firma cambiata, unico chiamante aggiornato). Verificato con test standalone: 7 query di esempio (una per categoria) instradano correttamente al tool giusto escludendo gli altri, query ambigua ("che tempo fa oggi") attiva il fallback con tutti e 7 | `main_ai_pipeline.cpp`, `main_ai.h` (firma `buildEnabledTools`) | ✅ 2026-07-04 |

### Stadio D — Contesto condizionale

| # | Priorità | Descrizione | File | Stato |
|---|----------|-------------|------|-------|
| D-30 | 🟡 | **RAG gating via embedding** — oggi il RAG globale è iniettato sempre (sessione 2026-06-10c): check di similarità embedding query↔indice (`fetchEmbedding` esiste già) e inietta SOLO se sopra soglia — meno rumore e meno token sulle query che col RAG non c'entrano. Il gate era nel wrapper legacy `AiClient::chat(sys,msg)` (righe 436-461): i top-3 chunk da `RagEngine::search()` venivano concatenati **sempre**, senza soglia (`RagEngine::search()` non esponeva nemmeno lo score, solo i chunk). Aggiunta `RagEngine::searchScored()` (`rag_engine.h/.cpp`) — stesso motore di ranking, ritorna anche il coseno; `search()` esistente riscritta sopra di essa (nessun cambio di firma per i 3 chiamanti esistenti: `lan_server_ai.cpp`, `main_oracle.cpp`, test). In `ai_client.cpp` il chunk viene incluso nel contesto solo se `score >= kRagRelevanceThreshold` (0.30, costante locale — soglia empirica, non configurabile: se in pratica risulta troppo/poco aggressiva si può esporre come QSettings in futuro). Se nessun chunk supera la soglia, il system prompt resta invariato come nel caso "RAG disabilitato". 5 nuovi test in `test_rag_engine_avanzato.cpp` (CAT-B, ora 15): coseno ~1.0 su match esatto, ~0.0 su ortogonale, ordine decrescente per score reale, `search()`/`searchScored()` concordi sull'ordine — verificati con `ctest -R RagEngine` (entrambe le suite, 0 fallimenti) | `rag_engine.h/.cpp`, `ai_client.cpp`, `gui/tests/test_rag_engine_avanzato.cpp` | ✅ 2026-07-04 |
| D-31 | 🟢 | **Timestamp condizionale** — se la query contiene "oggi/domani/che ora/quanto manca", inietta data+ora correnti nel system prompt (i modelli locali non le sanno; le guardie date coprono solo i calcoli espliciti) | `main_ai_pipeline.cpp` | ⬜ |

### Stadio E — Privacy pre-invio

| # | Priorità | Descrizione | File | Stato |
|---|----------|-------------|------|-------|
| D-32 | 🟢 | **PII scrubbing verso backend remoti** — se il backend NON è locale (WAN compute, endpoint esterni), maschera CF/IBAN/email/telefono prima dell'invio. I validatori esistono già in `_inject_finance`: riusarli come detector | `ai_client.cpp` o `lan_wan_page` (invio WAN) | ⬜ |

### Algoritmi esistenti → Function Tools (la parte "avanzata" oltre D-17)

| # | Priorità | Descrizione | File | Stato |
|---|----------|-------------|------|-------|
| D-33 | 🔴 | **Esporre gli algoritmi già scritti come function tools** — D-17 li ha agganciati ai regex (frasi note); qui si registrano in `mkTool()` così il MODELLO li chiama con argomenti strutturati (formulazioni libere, multi-turno, componibili con altri tool). Candidati, tutti già implementati nel codice: **(1) ✅ 2026-07-04 — i 14 algoritmi già coperti da D-17** (mcd/mcm, fattorizzazione, pascal, fibonacci, catalan, collatz, hanoi, profitto_azioni, inversioni, posizione_array, edit_distance, lcs, kmp) esposti come tool `algoritmo(nome, {a,b,n,array,target,s1,s2,pattern,testo})`: schema in `_buildOllamaTools()` (`main_ai_pipeline.cpp`), dispatcher `_execAlgoritmo()` + case in `runToolCall()` (`main_ai_tools.cpp`, riusa le stesse chiamate `SimulatorePage::gen*`/`_editDistance`/`_longestCommonSubsequence` di `_inject_algo`, stessi limiti sui parametri), voce aggiunta al pannello UI "⚡ Tool Veloci" (`main_ai_panels.cpp`, `kNTools` 10→11 — **necessario**: senza voce nel pannello il tool sarebbe stato nello schema ma mai in `m_enabledTools`, quindi filtrato via da `buildEnabledTools()` e mai raggiungibile). **(2) ✅ 2026-07-04 — `codice_fiscale`**: tool che riusa `PraticoCalcs::calcolaCodiceFiscale()`+`cercaBelfiore()` (`pratico_calcs.h`, già header-only — nota: il file si chiama `pratico_page.cpp` solo nei commenti storici, dopo D-8 la classe `PraticoPage` vive in `main_finance.cpp`). Schema JSON allineato a quello già usato dall'auto-fill RAG della Scheda TFR (nome/cognome/data_nascita `"yyyy-MM-dd"`/sesso/comune_nascita), con `codice_belfiore` opzionale a priorità più alta del lookup comune. Verificato con test standalone fuori ctest (calcolo corretto, errore su comune non in elenco, errore su data malformata). **(3) ✅ 2026-07-04 — `finanza_calcola`** (parziale, vedi nota): tool con `tipo` ∈ {interesse_composto, rata_mutuo, tfr_rivalutazione}. Interesse composto e rata mutuo (ammortamento francese) sono le stesse formule chiuse già in `_inject_finance` (duplicate, non condivise — sono one-liner, stesso approccio già usato per "hanoi" in `_execAlgoritmo`); TFR con rivalutazione duplica la formula di `main_finance.cpp::buildTfrTab()` (quota annua = stipendio/13,5; tasso = 1,5%+75%×inflazione; netto indicativo 77%/88% in azienda/fondo pensione). Verificato con test standalone contro valori noti (mutuo 100000€/3%/20anni → rata 554,60€; interesse 1000€/5%/10anni → 1628,89€). **730 e regime forfettario esclusi deliberatamente**: controllato il codice esistente — il forfettario in `main_finance.cpp` è solo una persona di chat LLM (nessun calcolo reale), il 730 non ha alcuna implementazione. Scriverli richiederebbe produrre ex novo gli scaglioni IRPEF/coefficienti di redditività, che è "scrivere nuova logica fiscale" non "esporre codice già scritto" — fuori scope di D-33, andrebbe un TODO dedicato con verifica indipendente vista la sensibilità dei dati fiscali. **(4) ✅ 2026-07-04 — `valida_documento`**: tool con `tipo` ∈ {iban, partita_iva, codice_fiscale}, riusa direttamente `_ibanValid()`/`_cfChecksumValid()`/`_cfDecode()` (già `static` in questo file, servita solo forward declaration — non duplicate) + checksum P.IVA duplicato dalla stessa guardia (5 righe, one-liner come gli altri). Verificato con test standalone: IBAN italiano di esempio (Banca d'Italia) valido/invalido correttamente, P.IVA `12345678903` valida (stesso esempio già in T-D14c del TODO). **(5) ✅ 2026-07-04 — `carta_astrale`**: tool che riusa `AstroCalc::compute()` (`widgets/astro_calc.h`, Meeus, già coperto da 31 test nella suite `Astrale` — nessuna modifica al motore, solo nuovo entry point) con `data`/`ora`/`lat`/`lon`. Formattazione posizione→segno duplicata 1:1 dalla lambda `lonToSign()` di `main_research_astrale.cpp` (4 righe). Restituisce solo pianeti+ASC+MC, non le 12 case Placidus/aspetti (risultato compatto da tool, l'analisi completa resta nel tab Ricerca dedicato). **(7) ✅ 2026-07-04 — `converti`**: a differenza degli altri tool di D-33, qui non si duplica/richiama una singola formula — `_inject_science()` copre già decine di conversioni eterogenee (Ohm, velocità, temperatura forno, ml↔grammi per 9 ingredienti, cucchiai/tazze, km/h↔mph, anni luce, UA...) e scomporle una per una in parametri strutturati avrebbe moltiplicato lo schema senza motivo. Il tool richiama direttamente `_inject_science(richiesta)` (stessa guardia già nella catena principale, dichiarata in `main_ai_p.h`) ed estrae il risultato dal tag `"[Calcolo locale:"` con la stessa logica già usata in `runPipeline()`: eredita ogni conversione che la guardia riconosce oggi e in futuro, senza manutenzione doppia. Utile per conversioni su un valore emerso a metà conversazione (multi-turno), non solo dalla frase originale (che la guardia intercetta già prima di arrivare al modello). **(6) ✅ 2026-07-04 — `disegna_grafico`**: unico tool di D-33 che tocca la UI (apre il pannello Grafico via `tryShowChart()`, già chiamata dalla guardia Grafico in `runPipeline()` e dal Byzantino — nessuna modifica alla funzione). Poiché `tryShowChart()` fa **solo** parsing testuale (nessun overload strutturato) e il modello passa `formula`/`xmin`/`xmax` già separati, il dispatcher ricostruisce la stessa frase in linguaggio naturale che la guardia riconoscerebbe da sola ("grafico di FORMULA per x da XMIN a XMAX" — Pattern 2 di `tryExtract()` + Pattern 4 di `tryExtractXRange()`), verifica il successo PRIMA di chiamare `tryShowChart()` (void, nessun segnale d'esito) per poter rispondere con un errore preciso se la formula non è valida. Verificato con test standalone (`FormulaParser` fuori da Qt Widgets, senza serve GUI) su 4 formule (`sin(x)*2`, `x^2-4`, `sqrt(abs(x))`, `tan(x)`): estrazione ✅, `fp.ok()` ✅, 400 punti campionati per tutte — **non verificato a schermo** che il pannello Grafico si apra davvero (impossibile lanciare la GUI in questo ambiente sandboxed, stesso limite già noto da D-15/D-18). **(8) ✅ 2026-07-04 — hanoi_passi + nqueens** (D-33 completo, 8/8): due nuovi `nome` nel tool `algoritmo`. `hanoi_passi` riusa **davvero** `SimulatorePage::genTowerOfHanoi()` (confermato puro — nessun accesso a membri — e reso `static`+spostato nel blocco `public:` insieme ai 16 di D-17, la funzione non è stata toccata) per l'elenco mosse passo-passo, N≤10 (1023 mosse, output troncato a 1500 char con `_truncateResult()`); resta distinto da `hanoi` (formula chiusa 2^n-1, N≤60) per lo stesso motivo per cui D-17 li teneva separati. `nqueens` è **nuova logica** (non un riuso): `_nQueensCount()` standalone, backtracking che conta TUTTE le soluzioni (non si ferma a 3 come `genNQueens()` del Simulatore, lasciata invariata per l'animazione) e riporta il conteggio esatto invece della tabella hardcoded valida solo fino a N=6; N≤12 per restare sincrono e veloce. Verificato con test standalone: N-Queens 1-12 contro la sequenza nota OEIS A000170 (1,0,0,2,10,4,40,92,352,724,2680,14200) — tutti e 12 corretti; Torre di Hanoi 1-10 dischi — mosse generate = 2^n-1 per tutti e 10 i casi | `main_ai_tools.cpp` (dispatcher + `_execAlgoritmo`), `main_ai_pipeline.cpp` (schema), `main_ai_panels.cpp` (UI), `main_simulator.h`, `pratico_calcs.h`, `widgets/astro_calc.h`, `widgets/formula_parser.h` | ✅ 8/8 (parte 6 `disegna_grafico` da verificare a video, vedi T-D33) |

### Ordine consigliato

```
D-23 (typo: moltiplica l'esistente) → D-33 (tool dagli algoritmi: il valore grosso)
→ D-30 (RAG gating) → D-25 (cache) → D-29 (pre-selezione tool, serve dopo D-33)
→ D-24 → D-26 → D-27 → D-28 → D-31 → D-32
```

---

## OPEN SOURCE / ADOZIONE — piano d'azione da audit 360° (2026-07-03)

> Contesto: 168k righe C++, 513 commit di 1 solo autore in 4 mesi, 59 MCP Python (50 requirements.txt),
> CI che compila 3 piattaforme ma NON esegue i test, licenza MIT senza infrastruttura comunità.
> Diagnosi: non è "enterprise" né open source come metodologia — è un megaprogetto personale pubblicato.
> Il piano è condizionato da OS-0: senza quella decisione le fasi 3-4 sono tempo sprecato.

### FASE 0 — Decisione (blocca tutto il resto)

| # | Priorità | Descrizione | Stato |
|---|----------|-------------|-------|
| OS-0 | 🔴 | **Decidere il pubblico target.** (A) Strumento personale → applicare solo FASE 1 (igiene, utile comunque) e chiudere il piano; il confronto con Blender/GIMP decade. (B) Prodotto per altri → tutte le fasi, con FASE 4 come strategia principale (il monolite da 168k righe non attrae contributori; i moduli estratti sì) | ⬜ |

### FASE 1 — Igiene immediata (~1 giornata, utile in entrambi i casi)

| # | Priorità | Descrizione | File | Ore | Stato |
|---|----------|-------------|------|-----|-------|
| OS-1 | 🔴 | **ctest in CI** — le 66 suite girano solo in locale: aggiungere workflow (o job) con `-DBUILD_TESTS=ON` + `ctest --exclude-regex "AiIntegration\|AiStress\|TeamCollab\|MultiAgenteLive"` su push/PR + badge stato nel README. È il gap più grave: il gioiello del progetto (test) non è enforced | `.github/workflows/` (nuovo `tests.yml`) | 2-3h | ⬜ |
| OS-2 | 🔴 | **Checksum release** — step `sha256sum` negli workflow che pubblicano AppImage/ZIP/APK + file `SHA256SUMS.txt` allegato alla release. Oggi chi scarica non può verificare nulla | `build_appimage.yml`, `build-windows.yml`, `build-macos.yml` | 1h | ⬜ |
| OS-3 | 🟡 | **SECURITY.md** — canale privato per segnalare vulnerabilità (email), scope (LAN server, MCPs, web app), tempi di risposta indicativi. GitHub lo mostra automaticamente in "Report a vulnerability" | `SECURITY.md` root | 30m | ⬜ |
| OS-4 | 🟡 | **Dependabot** — `.github/dependabot.yml` per ecosistema `pip` (i 50 requirements.txt degli MCP: usare directory multiple o wildcard) + `github-actions`. Oggi una CVE nelle ~200 dipendenze transitive è invisibile | `.github/dependabot.yml` | 1-2h | ⬜ |
| OS-5 | 🟡 | **README inglese** — `README.en.md` linkato in testa al README italiano (o viceversa). L'italiano-first taglia il ~95% del pubblico open source | `README.en.md` | 2-3h | ⬜ |

### FASE 2 — Sicurezza macro

| # | Priorità | Descrizione | File | Stato |
|---|----------|-------------|------|-------|
| OS-6 | 🟡 | **Verificare auto-update** — controllare che il badge 🆕 (GitHub API, C-2) sia solo notifica: se scarica/esegue binari, aggiungere confronto SHA-256 con `SHA256SUMS.txt` (OS-2) prima di applicare. Vettore supply-chain se non verificato | `mainwindow.cpp` (onAutoUpdateReply) | ⬜ |
| OS-7 | 🟢 | **Hint self-signed nel dialogo QR** — una riga: "il browser mostrerà un avviso di sicurezza → Avanzate → Procedi (normale per certificati self-signed)". Evita l'abbandono al primo accesso da dispositivo nuovo | `main_lan_wan.cpp` (openQrDialog note) | ⬜ |
| OS-8 | 🟢 | **Consolidare dipendenze MCP** — constraints file unico (`MCPs/constraints.txt`) referenziato dai 50 requirements.txt: versioni allineate, superficie ridotta, un solo punto di update | `MCPs/*/requirements.txt` | ⬜ |
| OS-9 | 🟢 | **Firma release** (minisign o GPG) — solo se OS-0 = (B); con (A) i checksum di OS-2 bastano | workflow release | ⬜ |

### FASE 3 — Usabilità / adozione (solo se OS-0 = B)

| # | Priorità | Descrizione | File | Stato |
|---|----------|-------------|------|-------|
| OS-10 | 🟡 | **i18n inglese completo** — le ~720 stringhe `tr()` sono già estratte (sessione 2026-06-05c): completare `i18n/prismalux_en.ts` e verificare il caricamento con locale non-it | `gui/i18n/` | ⬜ |
| OS-11 | 🟢 | **Vetrina README** — screenshot aggiornati delle tab principali + 1 GIF del flusso chat; è la prima cosa che Blender/GIMP curano e qui manca | `README.md`, `docs/img/` | ⬜ |
| OS-12 | 🟢 | **CONTRIBUTING.md + doc architettura per umani** — distillare da `gui/CLAUDE.md` (scritto per AI): come compilare, layout pages/widgets, convenzioni lambda/DPI/path, come aggiungere una suite test | `CONTRIBUTING.md`, `docs/ARCHITECTURE.md` | ⬜ |
| OS-13 | 🟢 | **Accessibilità base** — `setAccessibleName`/`setAccessibleDescription` su tab principali, pulsanti d'azione e campi input delle 10 tab (oggi 8 file su ~200). Verifica con Orca | `gui/pages/*` | ⬜ |
| OS-14 | 🟢 | **Manuale utente** — GitHub wiki o `docs/`: una pagina per tab, generabile in parte dalle tabelle help zero-LLM già esistenti (D-11/D-13) | wiki / `docs/` | ⬜ |

### FASE 4 — Estrazione moduli riusabili (solo se OS-0 = B — strategia principale di adozione)

| # | Priorità | Descrizione | Sorgente attuale | Stato |
|---|----------|-------------|------------------|-------|
| OS-15 | 🟡 | **Estrarre `finanza-ita`** — libreria Qt standalone: 730 IRPEF, TFR (rivalutazione FOI), Codice Fiscale D.M.1976+Belfiore, P.IVA forfettario, checksum P.IVA. Nessuna libreria Qt equivalente esiste: è il pezzo con più potenziale di adozione reale | `pratico_page.cpp`, `pratico_calcs.h` | ⬜ |
| OS-16 | 🟡 | **Estrarre `GraphMemory`** — libreria Qt (nodi/archi SQLite, BFS, DOT export, segnale changed) + widget viewer. Già autonoma di fatto: 65 test, API pulita, zero dipendenze da pages/ | `graph_memory.h/cpp` | ⬜ |
| OS-17 | 🟢 | **Estrarre motore zero-LLM** — guardiaMath + _inject_* (finanza/generatori/algoritmi/date/geometria): "risposte istantanee senza modello" è una nicchia interessante anche fuori da Prismalux | `main_ai_math.cpp`, `main_ai_tools.cpp` | ⬜ |

### Ordine consigliato

```
OS-0 (decisione)  →  OS-1 → OS-2 → OS-3 → OS-4 → OS-5   (igiene, ~1 giorno)
                  →  OS-6 → OS-7                          (sicurezza rapida)
se (B):           →  OS-15 o OS-16 (primo modulo estratto come test di interesse reale)
                  →  OS-10 → OS-11 → OS-12                (vetrina)
                  →  OS-8 → OS-9 → OS-13 → OS-14 → OS-17  (rifinitura)
```

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

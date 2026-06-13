# Prismalux — TODO pendenti

> Aggiornato: 2026-06-12 | Versione: 2.9

---

## 🚨 SICUREZZA PRODUZIONE — da chiudere PRIMA del rilascio (audit 2026-06-12)

> Audit del codice attuale dopo il commit `10c33e0` (thin-client API web).
> **Le prime due voci sono regressioni introdotte da quel commit**: i nuovi
> endpoint web saltano il controllo del token già esistente. Verificate
> direttamente in `gui/lan_server.cpp`.

### 🔴🔴 Bloccanti assoluti — NON mettere in produzione finché aperti

- [x] **`/api/repl` è una RCE NON autenticata** — FATTO 2026-06-12: aggiunto a `isApi`
      (ora richiede il Bearer token) + gating headless (403 se `m_headless`) come `/api/launch`.
      Resta da fare la sandbox vera (container/seccomp) — vedi voce limiti risorse sotto.
      `lan_server.cpp:2048` `handleReplApi()`
      esegue `python3 -`, `bash -s`, `node -e` con **codice arbitrario** preso dal
      body JSON. L'endpoint **non è nella lista `isApi`** (righe 581-593), quindi il
      controllo `Bearer token` (riga 598) **non viene mai applicato**: chiunque sulla
      LAN fa `POST /api/repl {"lang":"bash","code":"..."}` ed esegue comandi sulla
      macchina server. La "sandbox" citata nel commit è **solo un timeout** (10-15s),
      nessun isolamento reale (no container/seccomp/namespace/utente dedicato).
      - **Rimedio:** (1) aggiungere `/api/repl` a `isApi` così richiede il token;
        (2) gating esplicito come `/api/launch` — disabilitato di default e in headless
        (`m_headless` → 403); (3) eseguire in sandbox vera (container/`bwrap`/seccomp)
        o rimuovere `bash` del tutto e tenere solo un REPL Python ristretto.

- [x] **Endpoint nuovi fuori dal controllo auth** — FATTO 2026-06-12: aggiunti a `isApi`
      `/api/file`, `/api/repl`, `/api/finanza/cf` e `/api/graph` (via `startsWith`), ora tutti
      richiedono il token. (Resta consigliato il passaggio a "deny by default" per il futuro.)
      Sempre lista `isApi` (581-593):
      mancano anche `/api/file`, `/api/finanza/cf`, `/api/graph`.
      - `/api/graph` (`handleGraphApi`, riga 2112) espone la **GraphMemory** (memoria e
        conoscenza dell'utente, contenuto nodi) a chiunque senza token.
      - `/api/finanza/cf` (riga 2078) calcola **codici fiscali** (dato personale) in
        chiaro senza auth.
      - `/api/file` (riga 1970) accetta upload e lo passa a processi esterni.
      - **Rimedio:** aggiungere tutti e quattro a `isApi`. Valutare un approccio
        "deny by default": autenticare TUTTI i path tranne una whitelist esplicita di
        risorse pubbliche (`/`, `/web`, `/katex/`, `/bootstrap/`, `/apk`), invece di
        elencare a mano ogni endpoint protetto — così un endpoint nuovo nasce protetto.

### 🔴 XSS — regressione introdotta dal commit `10c33e0`

- [x] **XSS reintrodotto nella lista offerte lavoro** — FATTO 2026-06-12: `renderLavResults`
      riscritta con `createElement` + `textContent` per ogni campo (azienda/ruolo/sede/
      requisiti), niente più `innerHTML` con dati esterni. `gui/lan_web/webchat.html:630`
      `renderLavResults()` costruisce il DOM con `innerHTML` concatenando dati esterni
      (`o.azienda`, `o.ruolo`, `o.sede`, `o.requisiti` da `/api/lavoro`, origine Indeed).
      Se un'offerta contiene HTML/`<script>` → injection nel browser del client.
      Questo XSS era **già stato chiuso il 2026-06-01** (vedi voce più sotto) e nel
      codice esiste già la versione sicura `lavRender()` (riga 277) che usa
      `createElement` + `textContent`. La nuova tab Lavoro ha duplicato la lista col
      pattern insicuro invece di riusare quello bonificato.
      - **Rimedio:** riscrivere `renderLavResults` come `lavRender`/`grfLoad`
        (`textContent` per ogni campo), oppure escapare i dati. Le altre render del
        nuovo webchat sono OK: `ragSearch` (riga 519) escapa, `grfLoad` (riga 611-616)
        usa `textContent`.

### 🔴 SSRF — tool `fetch_url` senza validazione di destinazione

- [x] **SSRF nel tool `fetch_url`** — FATTO 2026-06-12: lo script Python ora valida lo schema
      (`http`/`https`) e risolve l'host rifiutando IP loopback/privati/link-local/reserved/
      multicast, sia sull'URL iniziale sia su ogni redirect (handler `_SafeRedirect`). Resta
      una finestra TOCTOU teorica (DNS rebinding) accettabile per uso locale.
      `gui/pages/main_ai_tools.cpp:303-335`: scarica
      **qualsiasi URL** via `urllib.request.urlopen` senza alcun controllo sull'host di
      destinazione. Nessun filtro su IP loopback/privati/link-local né sui redirect.
      - **Impatto:** il tool è invocabile dall'**output dell'LLM** (tool use automatico) e
        innescabile da **contenuti RAG non fidati**, non solo dall'utente. Un URL come
        `http://127.0.0.1:11434/...` raggiunge **Ollama (senza auth)**; `http://169.254.169.254`
        i metadata cloud; o servizi interni della LAN altrimenti non esposti. Combinato con
        l'iniezione RAG già supportata, è una catena prompt-injection → SSRF.
      - **Rimedio:** risolvere il DNS dell'host e **rifiutare se l'IP risolto è
        loopback/privato/link-local/multicast** (controllo `ipaddress.ip_address().is_private`
        ecc. lato script Python); consentire solo schemi `http`/`https`; disabilitare o
        validare i redirect (no redirect verso IP interni). Stesso filtro va applicato a
        ogni futuro tool che esegue richieste di rete da URL arbitrario.

### 🔴 Bot Telegram + WhatsApp — fail-open con whitelist vuota (3 posizioni)

- [x] **I bot rispondono a CHIUNQUE se la whitelist è vuota** — FATTO 2026-06-12: fail-closed
      in tutti e tre i punti (whitelist vuota = nessuno autorizzato); il runtime Telegram emette
      anche un `warning` all'avvio se la whitelist è vuota. Stesso fail-**open** era in tre
      punti distinti:
      1. `MCPs/telegram_bot_runtime.py:44` — `def allowed(): if not WHITELIST: return True`.
      2. `gui/pages/main_app_controller_slots.cpp:1252-1254` — **seconda copia inline** dello
         stesso bot Telegram, generata come stringa C++, con identico `if not WHITELIST:
         return True`. (Esistono quindi due implementazioni duplicate del bot — vedi anche
         debito di manutenibilità sotto.)
      3. `gui/pages/main_app_controller_slots.cpp:1970` — bot WhatsApp:
         `bool authorized = whitelist.isEmpty();` col commento esplicito "*se vuota →
         risponde a tutti*".
      A differenza del server LAN (rete locale), Telegram e WhatsApp sono **reti pubbliche
      mondiali**: con whitelist vuota chiunque conosca/indovini il bot chatta con l'AI locale
      ed esegue `/ask`. L'impostazione apparentemente "neutra" è la più pericolosa.
      - **Rimedio:** fail-closed in **tutti e tre** i punti → whitelist vuota = nessuno
        autorizzato (o il bot non si avvia e avvisa "configura almeno un ID/numero").

- [x] **Whitelist WhatsApp con match per sottostringa** — FATTO 2026-06-12: confronto sul numero
      normalizzato (solo cifre), uguaglianza esatta o suffisso per numeri ≥9 cifre; voci <6 cifre
      ignorate. Niente più `contains`. `main_app_controller_slots.cpp:1973`
      `if (from.contains(wl.trimmed()))`: usa `contains` invece del confronto esatto, quindi
      una voce come `123` autorizza qualunque mittente che *contenga* "123"
      (`3912345678`, `99123`, ...). Bypass della whitelist con numeri parziali.
      - **Rimedio:** confronto esatto del numero normalizzato (rimuovere `+`, spazi, suffisso
        `@c.us` del bridge) invece di `contains`.

- [x] **Debito: doppia implementazione del bot Telegram** — FATTO 2026-06-13:
      rimossa la funzione `s_telegramBotScript()` (140 righe di stringa C++ inline) da
      `main_app_controller_slots.cpp`. `onTelegramStartClicked()` ora usa direttamente
      `MCPs/telegram_bot_runtime.py` già esistente (versionato, con fail-closed corretto).
      Unica sorgente di verità.

### 🟡 Token bot in QSettings in chiaro (non nel keychain)

- [x] **Token Telegram salvato in `QSettings` in chiaro** — FATTO 2026-06-12: generalizzato il
      meccanismo del token LAN in `LanServer::saveSecret/loadSecret/deleteSecret` (keychain con
      fallback file 0600); il token Telegram ora usa quello. All'apertura del tab il vecchio
      valore in chiaro in `telegram/token` viene migrato al keychain e rimosso da QSettings; al
      salvataggio non viene più scritto in chiaro. (Storico) `main_app_controller.cpp:2193`
      `s.setValue("telegram/token", ...)`. Il token del bot dà **controllo completo del bot**
      a chiunque legga il file di config (`~/.config/...`, di norma 0644). Il progetto già
      impone il keychain per i segreti (BEST_PRACTICE_SECURITY §7, token LAN via QKeychain) e
      l'"Audit segreti" del TODO elenca "token non in QSettings" come controllo: il token
      Telegram lo viola. Idem la whitelist WhatsApp (riga 2419).
      - **Rimedio:** spostare `telegram/token` (e gli altri segreti bot) in QKeychain come il
        token LAN. *Nota positiva:* al runtime il token è già passato via **variabile
        d'ambiente** (`TELEGRAM_TOKEN`, `main_app_controller_slots.cpp:1411`), non in argv —
        quindi non è visibile in `ps`. Il problema è solo la persistenza su disco.

### 🟡 Graphviz — accesso a file locali via DOT

- [x] **`/api/graphviz` esegue `dot` su sorgente DOT arbitrario** — FATTO 2026-06-13:
      validazione regex sul contenuto DOT che rifiuta `image=`, `shapefile=`, `fontpath=`,
      `imagepath=`; aggiunto `-Gimagepath=` (vuoto) al comando `dot` per bloccare il
      caricamento di immagini da disco. `lan_server.cpp handleGraphviz()`.

### 🟡 DevAgent MCP — `shell=True` con interpolazione non quotata

- [x] **Comandi git costruiti con f-string in `bash(shell=True)`** — FATTO 2026-06-13:
      aggiunto helper `_git_safe(project_root, args, timeout)` che usa
      `subprocess.run([...], shell=False)` con lista di argomenti. Aggiunti pattern
      di validazione `_RE_COMMIT`, `_RE_REMOTE`, `_RE_BRANCH`, `_RE_STASH` e
      `_RE_ROOT` (path assoluto). Tutte le 6 funzioni git (`git_restore_files`,
      `git_fetch_reset`, `git_stash_push`, `git_stash_list`, `git_stash_pop`) ora
      usano `_git_safe` con validazione preventiva dei parametri.
      `MCPs/devagent_mcp/server.py`.

### 🟢 Igiene segreti — `.gitignore` preventivo

- [x] **`.env` / `*.key` / `*.pem` non sono in `.gitignore`** — FATTO 2026-06-13:
      aggiunti `.env`, `*.env`, `*.key`, `*.pem`, `*.p12` in sezione
      "Segreti — prevenzione commit accidentale" in `.gitignore`.

### 🟡 Dati personali (PII) hardcoded nel sorgente — `/api/cv`

- [x] **CV con nome, data di nascita, email e telefono hardcoded nel codice** — FATTO 2026-06-13:
      la stringa statica con PII rimossa da `lan_server.cpp`; `/api/cv` ora carica
      `~/.prismalux/cv.txt` (0600, fuori repo) a runtime. Fallback: messaggio
      "[CV non configurato — crea il file ~/.prismalux/cv.txt]".
      File creato in `~/.prismalux/cv.txt` con i dati originali.
      - *Verificato sicuro:* `handleRag` (riga 1214) ha guard su RAG nullo, indice vuoto e
        query concorrente — nessuna injection di path/processo.

### 🟢 Chiave privata TLS senza permessi ristretti

- [x] **`server.key` generata senza `chmod 0600`** — FATTO 2026-06-13:
      aggiunto `QFile::setPermissions(keyPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner)`
      dopo la generazione OpenSSL in `_ensureCert()`. `lan_server.cpp`.
      - *Verificato sicuro nello stesso ambito:* `appendAccessLog` (riga 120) **escapa** già `\`
        e `"` nel campo `path` → nessuna log injection nell'`access.log`.

### 🟢 Android — cleartext HTTP permesso globalmente

- [ ] **`cleartextTrafficPermitted="true"` globale** —
      `ANDROID/android_app/android/res/xml/network_security_config.xml:12` usa un `base-config`
      che permette HTTP in chiaro verso **qualsiasi dominio**, non solo la LAN. Il commento nel
      file lo riconosce: "la restrizione solo-LAN è garantita a livello applicativo, non dal
      manifest" (perché Android non accetta range CIDR nei `<domain>`). Oggi l'unico endpoint è
      l'IP LAN inserito dall'utente, ma è difesa in profondità mancante: una futura chiamata
      HTTP a un host internet non verrebbe bloccata dalla piattaforma.
      - **Rimedio:** dove possibile, `base-config cleartextTrafficPermitted="false"` +
        `domain-config` cleartext per i soli host LAN configurati; in alternativa documentare
        la scelta come accettata e abbinarla al TLS LAN opzionale già presente lato desktop.
      - *Nota positiva verificata:* `allowBackup="false"` nel manifest — i dati dell'app
        (token, chat, DB) non finiscono nei backup cloud automatici di Android.

### 🟡 Hardening web — difesa in profondità

- [x] **Nessun header `Content-Security-Policy`** — FATTO 2026-06-12: aggiunta CSP alla pagina
      web chat (`handleWebChat`): `default-src 'self'`, `script-src/style-src 'self'
      'unsafe-inline'`, `img-src 'self' data:`, `connect-src 'self'`, `frame-ancestors 'none'`.
      Blocca script/connessioni verso domini esterni (anti-esfiltrazione e supply-chain).
      `'unsafe-inline'` resta perché il JS della chat è inline — rimuoverlo richiede estrarre
      lo script in un file servito (follow-up). (Storico) `lan_server.cpp` inviava
      `X-Content-Type-Options`, `X-Frame-Options`, `Referrer-Policy` ma **nessuna CSP**
      sulle pagine HTML. Con Bootstrap e KaTeX ora serviti localmente si può impostare
      una CSP stretta (`default-src 'self'; script-src 'self'; img-src 'self' data:;
      style-src 'self' 'unsafe-inline'`) che neutralizza la maggior parte degli XSS DOM
      anche in caso di regressioni come quella sopra. Valutare anche `Permissions-Policy`
      per limitare microfono/camera ai soli usi previsti (STT).

### 🟡 Importanti — prima dell'esposizione su rete reale

- [x] **`/api/file` — costruzione path da filename utente** — FATTO 2026-06-12: aggiunti
      limite dimensione (413 se >25 MB) e whitelist estensioni (415 se non in
      pdf/docx/doc/txt/csv/md/rtf/odt/json/xml/html/htm) prima di scrivere il tmp e invocare
      gli estrattori. `lan_server.cpp` `handleFileApi`.
- [x] **`/api/repl` — limiti risorse** — FATTO 2026-06-13: aggiunto `-u 50` (max processi,
      anti fork-bomb) e `-n 100` (max file descriptor, anti fd exhaustion) a `kLimits` in
      `handleReplApi()`. Rimane opzionale: sandbox container/seccomp per isolamento completo.
      `lan_server.cpp`.
- [x] **Rotazione/scadenza del token LAN** — FATTO 2026-06-13: GroupBox "Token di accesso LAN"
      in Manutenzione→LAN Server con pulsante "🔄 Rigenera Token" — genera UUID, salva via
      `LanServer::saveLanToken()`, aggiorna `m_lanServer->setAccessToken()` se attivo, copia
      negli appunti e mostra token mascherato (prime4…ultime4 cifre). `main_maintenance_lan.cpp`.

### 🟢 Igiene — verifiche rapide

- [x] **Test di non-regressione auth** — GIÀ IMPLEMENTATO: `TestAuthNonRegression` (CAT-H)
      in `test_lan_server.cpp` — 12 test H1..H12 verificano 401 su ogni `/api/*` senza token.
- [ ] **Audit periodico lista `isApi`** — finché si usa la whitelist a mano, ogni PR che
      aggiunge un endpoint deve aggiornarla; meglio passare al "deny by default" sopra.

---

## 🧩 VPN · BOINC-like · MCP — analisi e configurazione assistita (richiesta 2026-06-12)

> Tre aree da approfondire/completare. Stato attuale verificato nel codice.

### 🌐 VPN & Tunnel (tab Programmazione → Rete)

> Stato: `main_programming_slots.cpp:1869+` è un **generatore di template di
> configurazione** (WireGuard, OpenVPN, SSH tunnel, n2n con `onVpnGenN2nKeys`).
> Produce testo che l'utente salva ed esegue a mano con `sudo`. **Non stabilisce né
> monitora connessioni attive.**

- [x] **Pulsante "Verifica stato VPN"** — FATTO 2026-06-12: pulsante "🔍 Verifica stato" nel
      tab VPN → `onVpnTestClicked`/`vpnRefreshStatus` (`main_programming.*`).
- [x] **Stato VPN live** — FATTO 2026-06-12: label di stato aggiornata ogni 5s via
      `QNetworkInterface` (rileva interfacce wg/tun/tap/ppp/n2n up + IPv4 assegnato), senza
      processi né root. Verde "🟢 wg0 (10.x)" se attiva, "⚪ Nessuna VPN attiva" altrimenti.
- [x] **Avvio/stop assistito** — GIÀ PRESENTE (scoperto 2026-06-12): `onVpnApplyClicked`/
      `onVpnStopClicked` gestiscono WireGuard/OpenVPN/SSH/n2n (SSH e n2n via clipboard).
- [x] **Validazione config** — GIÀ PRESENTE: `onVpnValidateClicked` (`m_vpnValidateBtn`)
      valida/simula la config senza root.
- [x] **Guida "quando serve la VPN"** — FATTO 2026-06-13: label `vpnHintLbl` aggiunto dopo
      `descLbl` nel tab VPN (`main_programming.cpp`): spiega quando la VPN serve per WAN Compute
      (porta 11600) e Sci Compute (porta 11601) su reti diverse, e che in LAN non è necessaria.

### 🔬 BOINC-like — Calcolo Distribuito (WAN Compute 11600 + Sci Compute 11601)

> Stato: due sistemi distinti e funzionanti. **WAN Compute** (`main_lan_wan.cpp`, porta
> 11600, 28 task, fault tolerance, multi-worker) e **Sci Compute** (`main_sci_compute.cpp`,
> porta 11601, SQLite `sci_nodes`, WU queue, quorum SHA-256, credit counter, aggregator).
> Test: `test_sci_compute` 35 PASS, `test_wan_compute_tasks` 33 PASS.

- [ ] **Analisi end-to-end con 2+ macchine reali** — verificare il flusso completo su LAN/VPN:
      creazione WU → dispatch → esecuzione su nodo remoto → quorum → aggregazione risultati.
      I test coprono le unità, manca una prova di integrazione su nodi reali.
- [x] **Chiarire la relazione WAN Compute vs Sci Compute** — `QLabel` descrittivo aggiunto
      in cima a `buildWanComputeTab()` (`main_lan_wan.cpp`): WAN Compute porta 11600 per
      task generici (shell/Python/LLM); Sci Compute porta 11601 per WU scientifiche
      (BLAST/GROMACS/SymPy) con heartbeat e credit counter BOINC-style.
- [x] **Guida configurazione nodo worker** — `QLabel` con guida 4-passi (Python, copia
      script, avvio con host/port/token/capabilities, comparsa in tabella) aggiunta:
      - WAN Compute: `main_lan_wan.cpp` prima di "Monitor nodi + coda" (porta 11600)
      - Sci Compute: `main_sci_compute_ui.cpp` come primo widget nel tab "Nodi" (porta 11601)
- [x] **Health check nodi Sci Compute** — GIÀ IMPLEMENTATO: `onHeartbeatTimer()` ogni 30s
      invia ping + chiama `markOfflineNodes(60000ms)`; `onWorkerDisconnected()` riassegna
      WU `running` del nodo disconnesso a `pending`. `main_sci_compute.cpp:730-742,1609-1617`.
- [x] **WAN Compute: coda WU non persistente (in RAM)** — FATTO 2026-06-12: la coda è ora
      persistita su SQLite in `~/.prismalux/wan_tasks.db` (`wanLoadTasks`/`wanPersistTasks`/
      `wanSchedulePersist` in `main_lan_wan.cpp`, sotto `HAVE_QT_SQL`). Salvataggio con debounce
      0,8s agganciato a `wanRefreshTables()`; al riavvio `wanLoadTasks()` ripristina la coda e
      rimette "running"→"pending" (con `retryCount += 1`) i task interrotti da un crash.
      - Follow-up: test di round-trip dedicato (i metodi sono privati e legati alla UI — serve
        un piccolo hook di test o estrarre la logica DB).
- [ ] **Sicurezza (rimando):** prima di esporre questi servizi vedi la sezione 🚨 SICUREZZA —
      auth obbligatoria, niente shell di default, TLS o VPN fidata.

### 🔌 MCP — far funzionare tutti i 22 plugin + configurazione assistita

> Stato: 22 voci in `MCPs/`. Quasi tutti hanno `requirements.txt` + `server.py`. Eccezioni
> verificate: `blender_mcp` (manca `requirements.txt`), `CLOUDcompare` e `meshroom` (nessun
> server/requirements — sono solo launcher di app esterne). Ogni MCP è oggi **cablato
> individualmente** nella sua tab; **non esiste un pannello centrale** di installazione/test.

- [x] **Pannello "Gestione MCP" centralizzato** — FATTO 2026-06-12:
      `gui/pages/main_mcp_manager.h/cpp`, tab "🔌 Gestione MCP" in AppController [6]. Lista
      degli MCP con server.py, stato requirements, pulsanti "Prepara ambiente (venv)",
      "Installa" (pip nel venv, con conferma) e "Testa" (smoke test JSON-RPC), log in-app.
      Test `test_mcp_manager` (CAT-A + CAT-B) PASS. Una schermata sola con la lista degli MCP e,
      per ciascuno: stato (server presente? dipendenze installate? eseguibile esterno trovato?),
      pulsante **"Installa dipendenze"** (`pip install -r requirements.txt` con conferma, come
      da BEST_PRACTICE §2), pulsante **"Testa"** (smoke test: avvia il server, invia un
      `initialize`/`tools/list` JSON-RPC, verifica la risposta), e link alla guida.
- [x] **Smoke test per ogni MCP** — FATTO 2026-06-12: pulsante "Testa" per MCP nel pannello;
      invia `initialize` + `tools/list` e verifica la risposta `result`; mostra la causa
      (stderr sanificato) se il server esce senza rispondere. Manca solo un "Testa tutti" batch.
- [x] **Isolamento dipendenze MCP (venv)** — FATTO 2026-06-12: il pannello Gestione MCP crea
      e usa un venv condiviso in `~/.prismalux/venv`; "Installa" esegue `pip install -r` dentro
      quel venv e "Testa" lancia i server con il python del venv. Niente più
      `--break-system-packages` per gli MCP. (Storico) gli MCP installavano le
      dipendenze nel **Python di sistema** con `--break-system-packages`
      (`main_customize_lora.cpp:511` e pattern simili), bypassando PEP 668. Con 22 plugin dalle
      dipendenze pesanti e potenzialmente in conflitto (rdkit, torch/CUDA di Stable Diffusion,
      langgraph, python-telegram-bot…) questo causa **conflitti di versione** e inquina il
      Python di sistema → far funzionare *tutti* insieme è fragile.
      - **Rimedio:** un **venv dedicato** (per-MCP o un venv condiviso del progetto in
        `~/.prismalux/venv`) in cui installare i `requirements.txt`, evitando
        `--break-system-packages` globale. Il pannello Gestione MCP crea/usa quel venv.
- [x] **Log centralizzato MCP + "Testa tutti" batch** — FATTO 2026-06-13: pulsante
      "\xf0\x9f\xa7\xaa Testa tutti" in McpManagerPage (`main_mcp_manager.h/cpp`) con coda sequenziale
      (`m_testQueue`/`advanceTestQueue()`). Il log (`m_log`) già centralizzato con timestamp [HH:mm:ss]
      raccoglie tutti i risultati. Auto-restart (rilevare crash processo persistente) rimane
      come follow-up per bot Telegram/WhatsApp in AppController.
- [x] **Auto-restart bot morti (Telegram/WhatsApp)** — `m_telegramIntentionalStop` flag in
      `onTelegramStopClicked()` + logica crash in `onTelegramProcFinished()`: se stop non
      intenzionale, appende link `tg-restart://` nel log. WA: `m_waPollFailCount` — dopo 5
      poll falliti appende link `wa-restart://`. `onPipLinkClicked` gestisce entrambi gli
      schemi chiamando i rispettivi start slot.
- [x] **`blender_mcp/requirements.txt`** — GIÀ PRESENTE: documenta che usa solo stdlib
      (sys, json, urllib) e richiede Blender con addon su porta 6789. Nessuna dep pip.
- [x] **Guida configurazione per MCP che richiedono app esterne** — `mcpSetupGuide()` in
      `main_mcp_manager.cpp`: guide HTML per Anki/Blender/OBS/GNS3/Cytoscape/FreeCAD/
      KiCAD/OpenCode. Pulsante ℹ️ piatto aggiunto in ogni riga MCP con guida disponibile.
      `onMcpGuideClicked()` apre QDialog con QTextBrowser. Slot nominato, property
      `mcpGuide` sulla QToolButton. Build OK.
- [x] **Diagnostica "perché non funziona"** — FATTO 2026-06-13: `mcpExternalDiag()` in
      `main_mcp_manager.cpp`: controlla porta TCP per anki(8765)/obs(4455)/gns3(3080)/
      opencode(8092)/blender(6789)/cytoscape(1234); se non risponde appende messaggio
      "Causa probabile: Anki con AnkiConnect non è in ascolto su porta 8765 — aprilo
      prima di testare l'MCP." al log di test quando smoke test fallisce.
- [x] **Verifica integrità MCP** — `hashLbl` (QLabel con emoji ✅/📝/⚠️) aggiunto in ogni
      riga `McpEntry` (`main_mcp_manager.h/cpp`). SHA-256 di `server.py` calcolato con
      `QCryptographicHash::Sha256` in `rebuildList()`. Primo avvio: hash salvato in
      QSettings `mcp_hash/<nome>`; successivi: confronto — verde=invariato, arancio=primo
      avvio (hash registrato), rosso=modificato + avviso nel log.

---

## 📋 Richieste Paolo — 14/06/2026

### [14/06/26] Sub-agenti (spawn_agent) — tool e RAG

- [x] **Sub-agenti: accesso ai tool** — FATTO 2026-06-14: `sub->setActiveTools(subTools)` +
      `connect(sub, toolCallRequired, this, [...] { runToolCall → sub->replyWithTool })`.
      Tool list inline in `runToolCall()` sezione spawn_agent, senza spawn_agent.

- [x] **Sub-agenti: accesso al RAG** — FATTO 2026-06-14: raccolta `m_ragInline->ragContext()` +
      `m_cfgDlg->sharedRagWidget()->ragContext()` → iniettato in `taskFinal` prima di `sub->chat()`.

- [x] **Sub-agenti: limite ricorsione spawn_agent** — FATTO 2026-06-14: la `subTools` list non
      include `spawn_agent`; il sub-agente non può crearne altri.

---

## 📋 Richieste Paolo — 11-12/06/2026

> Chat Telegram → idee e feature da valutare/implementare.

### [11/06/26 23:24] Memoria LLM — contesto e compressione

- [x] **Compattatore di conversazioni — soglia configurabile** — FATTO 2026-06-13:
      `m_maxRecentTurns` (da `QSettings kChatMaxTurns`, default 3) in `OracoloPage` sostituisce
      il `static constexpr kMaxRecentTurns`. SpinBox "Turni in memoria" (1-20) in
      Impostazioni→AI→Parametri AI. `main_oracle.h/cpp` + `settings_ai.cpp` + `prismalux_paths.h`.
- [ ] **Compattatore di conversazioni — dettagli tecnici** (nota, non da implementare ora):
      dell'LLM; servono strumenti per comprimerle/riassumerle prima di inietterle.
      Domande aperte di Paolo:
      - La memoria persistente su file (GraphMemory/RAG) quanto incide sui token usati?
      - Che tipo di tools servono in produzione per un sistema di memoria LLM?
      **Risposta tecnica da documentare:**
      - GraphMemory usa `searchNodes(query)` → inietta solo chunk rilevanti (non tutto);
        il numero di token dipende dal parametro `maxChunks` (default 5 × ~200 token = 1000 tok).
      - Per produzione servono: compressore conversazione (rolling summary), RAG semantico
        per selezionare solo i chunk rilevanti, token counter prima dell'inject,
        threshold configurabile (es. non iniettare se già >80% del context window).
      - Approccio consigliato: `AiClient::chat(sys, msg, historyJson)` già comprime la
        storia in `compressHistory()` — estendere con soglia configurabile.

### [12/06/26 00:51] Git come sistema di memoria versionata per LLM

> Idea di Paolo: usare un repository Git locale (`~/.ai-memory/`) come "cervello
> cronologico" dell'assistente AI. Ogni preferenza appresa, ogni feedback, ogni
> interazione è un commit → storia completa, revertibile, analizzabile con `git log`.

**Struttura repo proposta:**
```
~/.ai-memory/
├── profile/
│   ├── preferences.yaml    # preferenze apprese (response_length, ecc.)
│   ├── habits.yaml
│   └── demographics.yaml
├── interactions/
│   └── YYYY-MM/
│       ├── DD.md           # conversazioni del giorno
│       └── feedback.yaml   # 👍/👎 con motivo + tag
├── context/
│   └── active_context.yaml
└── .git/
```

**Vantaggi rispetto a GraphMemory:**
- Storia temporale completa con `git log --all --oneline`
- Revert preferenze errate con `git checkout <hash> -- profile/`
- `git blame` per capire l'origine di ogni preferenza
- Analisi pattern: `git log --grep="rating: 👍" | ...`
- Portabile tra dispositivi via `git push/pull`

**Integrazione con Prismalux:**
- [x] **AIMemory C++** — FATTO 2026-06-13: `gui/ai_memory.h/cpp`. Gestisce `~/.ai-memory/`
      con `initialize()` (git init idempotente), `logFeedback()`, `saveInteraction()`,
      `getRelevantContext()` (preferences.yaml + N feedback), `updatePreference()`,
      `gitLog(n)`, `revertFile(hash, path)`. QProcess shell=false per tutti i comandi git.
      Test `AiMemory`: 12 PASS (CAT-A + CAT-B).
- [x] **Feedback loop in chat** — FATTO 2026-06-13: pulsanti 👍/👎 in ChatBubble (AI),
      visibili dopo `finalizeStream()`. Segnale `feedbackGiven(bool)` connesso in
      `addAIBubble()` → `AIMemory::logFeedback()`. Ogni scelta disabilita entrambi i btn.
- [x] **Visualizzazione storia preferenze** — FATTO 2026-06-13: tab "🧠 Memoria AI" in
      Impostazioni→Sistema. `buildAiMemoryTab()` in `settings_other.cpp`: QListWidget con ultimi
      30 commit git, pulsante "Ripristina preferences.yaml" (checkout hash selezionato), pulsante
      "Apri cartella ~/.ai-memory/". AIMemory istanziata localmente nel tab (owner: widget).

### [12/06/26 01:02] Smart Router C++ — decisione LOCAL vs CLOUD

> Paolo ha condiviso un progetto C++ standalone (`smart_router.cpp`) con logica di
> routing automatico tra Ollama locale e API cloud. Il codice è in questa chat.

**Logica di routing (5 regole ordinate):**
1. Offline → sempre LOCALE
2. Ollama non attivo → CLOUD
3. Keywords sensibili (password/IBAN/token) → LOCALE (privacy)
4. Query lunga (>1500 char) → CLOUD (contesto ampio)
5. Keywords complessità ("analizza profondamente", "dimostra che") → CLOUD
6. Default → LOCALE (veloce, gratis, privacy)

**Integrazione con Prismalux:**
- [x] **Smart Router in AiClient** — FATTO 2026-06-13: `AiClient::decideCloud(query)` con 5
      regole (keyword sensibili→locale, >1500 char→cloud, keyword complessità→cloud,
      default→locale). `setSmartRouter(enabled, url, model, apiKey)` + segnale `routedToCloud`.
      Settings in Impostazioni→AI Locale: checkbox + URL + modello + API key (password field).
      Caricato all'avvio da `mainwindow.cpp`. Routing cloud usa `/v1/chat/completions`
      OpenAI-compatible con `Authorization: Bearer`. `gui/ai_client.cpp` + `settings_ai.cpp`.
- [x] **Indicatore visivo routing** — FATTO 2026-06-13: badge `☁️ CLOUD` / `🏠 LOCALE`
      accanto al campo input in OracoloPage, aggiornato ad ogni richiesta tramite
      segnale `routedToCloud(bool)`. `m_routerLbl` in `main_oracle.cpp`.

### [12/06/26 01:04] AI Orchestrator completo (AIMemory + SmartRouter + Feedback)

> Paolo ha condiviso `ai_assistant.cpp`: sistema completo che unisce i tre pezzi:
> `AIMemory` (Git) + `SmartRouter` (LOCAL/CLOUD) + feedback loop automatico post-risposta.
> Compilabile standalone con `g++ -std=c++17 -O2 ai_assistant.cpp -o ai_assistant`.

**Caratteristiche del prototipo C++ standalone:**
- `AIMemory::initialize()` — crea `~/.ai-memory/` + init git
- `AIMemory::logFeedback()` — append su YAML + `git commit` automatico
- `SmartRouter::decideRoute()` — 5 regole sopra
- `AIOrchestrator::processQuery()` — contesto + routing + risposta + feedback 👍/👎
- Zero dipendenze oltre `curl` e `git`

**TODO Prismalux:**
- [x] **Prototipo standalone testabile** — FATTO 2026-06-13: `Tools/scripts/ai_assistant.cpp`
      (C++17, zero dipendenze oltre curl+git). Classi `AIMemory` (git repo `~/.ai-memory/`),
      `SmartRouter` (5 regole LOCAL/CLOUD), `AIOrchestrator` (contesto+routing+risposta+feedback).
      Compila con: `g++ -std=c++17 -O2 Tools/scripts/ai_assistant.cpp -o ai_assistant`.
- [x] **Integrazione graduale** — GIÀ COMPLETATA: `AIMemory` come classe Qt (`gui/ai_memory.h/cpp`)
      con git repo `~/.ai-memory/`. `SmartRouter` → `AiClient::decideCloud()` (5 regole
      privacy/len/complexity). Feedback loop → `ChatBubble::feedbackGiven` → `m_aiMemory.logFeedback()`
      in `addAIBubble()` (`main_oracle.cpp:628`). `setSmartRouter()` controllato da impostazioni.

---

## 📋 Richieste Paolo — 07-09/06/2026

> Chat Telegram → da implementare (non ancora completate).

### [07/06/26 01:45] Feature

- [x] **Generatore di policy** — schermata/funzione per creare policy di sicurezza di rete
- [x] **Calcolo delle sottoreti con grafo** — visualizzazione grafica del subnetting (già presente schermata grafo Rete, integrare calcolo CIDR→nodi grafo)

### [07/06/26 01:51] Bug — versione

- [x] **Titolo finestra mostra v2.1 invece di v2.9** — fix: `aggiorna.sh` puntava a `build_gui/` vecchio invece di `gui/build_gui/`; `Prismalux.desktop` aggiornato; binary ora in `gui/build_gui/Prismalux_GUI` — 2026-06-10

### [07/06/26 02:01 / 02:11] Test e feature RAG multiformat

- [x] **Test inserimento RAG — tutti i formati devono funzionare**: immagini, video, PDF, TXT
  - Immagini: OCR (tesseract) + descrizione vision LLM → testo → indicizza
  - Video: estrazione frame chiave + audio→whisper→testo → indicizza
  - PDF: già parzialmente funzionante — verificare
  - TXT: già funzionante — verificare

### [07/06/26 11:34] Spostamento tab

- [x] **Sposta "File AI" dentro "Strumenti"** — `StrumentiFilePage` aggiunta come sub-tab 10 in `StrumentiPage`; rimossa dal tab bar principale; indici Alt+N aggiornati — 2026-06-10

### [07/06/26 11:35] RAG — miglioramenti UX + bug

- [x] **Pulsante Pausa/Riprendi indicizzazione** — `RagGraph::pauseIngest()/resumeIngest()` in `rag_graph.h/cpp`; pulsante ⏸/⏵ in `main_research.cpp` — 2026-06-10
- [x] **Modello embedding selezionabile nel contesto RAG** — `m_ragEmbedCombo` in `settings_main.h` + `settings_ai.cpp`: combo con modelli statici noti (✓) + pulsante 🔄 per caricare da Ollama; modelli chat marcati con ⚠️ e tooltip "non compatibile con RAG" — 2026-06-10
- [x] **Spiegazione "Risultati: 5"** — tooltip su `maxSpin` in `settings_ai.cpp`: "Numero massimo di chunk RAG restituiti per ogni query" — 2026-06-10
- [x] **Pulsante "Scarica documenti ufficiali consigliati (ADE 2026)"** — in `settings_ai.cpp` dentro QGroupBox "Ottimizzazione vettori RAG"; apre dialog con info e URL — 2026-06-10
- [x] **Controllo temperatura CPU/GPU nel RAG** — `onThermalUpdate()` in `main_research.cpp`: auto-pausa ingest a ≥80°C, auto-riprende a <75°C; label `m_ragTempLbl` in ctrlBar — 2026-06-10
- [x] **Temperatura sempre visibile nel RAG** — `m_ragTempLbl` accanto alla progress bar del RAG in `main_research.cpp` — 2026-06-10
- [x] **Temperatura vicino all'indicatore GPU nella schermata iniziale (tab AI)** — `m_tempLbl` in `buildHeader()` di `mainwindow.cpp`; colori: rosso ≥90°C, arancio ≥75°C — 2026-06-10
- [x] **Bug: RAG non inietta il contesto dei file nella chat** — `onEmbeddingError()` in `main_oracle.cpp` ora mostra avviso con nome modello embedding fallito + istruzione `ollama pull`; `onEmbeddingReady()` mostra conteggio chunk trovati o avviso 0 chunk — 2026-06-10

### [07/06/26 12:26] Bug — modelli DeepSeek e vision

- [x] **DeepSeek (r1, coder, janus) non supporta vision su Ollama** — `onCmbLLMIndexChanged()`: label warning `m_modelWarnLbl` "🛇 no vision" + disabilita `m_btnImg` + tooltip aggiornato — 2026-06-10
- [x] **`deepseek-coder:6.7b` non supporta tool use** — `isToolCapable()` in `onCmbLLMIndexChanged()`: disabilita `m_toolChk` + label warning "🔧 no tools" — 2026-06-10

### [07/06/26 12:39] Bug — caricamento file e LLM

- [x] **File caricato non viene inizializzato per il contesto LLM** — 3 fix: (1) flag `m_docLoading` blocca invio prematuro; (2) `m_docContext` svuotato dopo l'uso in `runPipeline()` (era persistente); (3) placeholder reset post-uso — 2026-06-10

### [07/06/26 13:27] Bug — stop richiesta al cambio chat

- [x] **Cambio chat ferma la query in corso** — `m_bgMode`: cambio sessione mette AI in background; token accumulati in `m_bgBuffer`; `onChatCompletedSave()` salva nella sessione originale e mostra "✅ Risposta completata nella sessione precedente" — 2026-06-10

### [09/06/26 03:17] Feature — ricerca online fallback

- [x] **LLM non conosce risposta → offri ricerca online** — rilevamento frasi incertezza in `advancePipeline()` (`main_ai_pipeline.cpp`), link `websearch:` iniettato sotto la bolla; handler in `onLogAnchorClicked` esegue `duckduckgo_search` via Python e salva in `RAG/RICERCA/<slug>.md`; emette `onlineSearchResultReady` — 2026-06-10

### [09/06/26 03:22] Feature — calcoli scientifici conversazionali

- [x] **Calcola formule inverse e conversioni direttamente nella chat** — `_inject_science()` in `main_ai_math.cpp`; riconosce: Ohm (V=IR/I=V/R/R=V/I), P=VI, τ=RC, fc=1/(2πRC), velocità, energia cinetica, conversioni (kWh/J, atm/Pa, bar/Pa, km/h↔m/s, dBm↔mW, dB guadagno, mol↔g, pH↔[H⁺]); antepone `[Calcolo locale: ...]` nel prompt — 2026-06-10

### [09/06/26 14:25] Feature — Hermes Agent (persistent memory + skill self-improving)

- [x] **Prismalux come "Hermes Agent"** — meccanismo unico:
  - **Persistent memory**: le conversazioni e i fatti appresi vengono salvati in GraphMemory e riutilizzati nelle sessioni successive (già parzialmente in GraphMemory; da connettere alla chat principale)
  - **Skill self-improving**: l'agente analizza i propri errori e aggiorna le sue strategie di risposta (log errori LLM → riflessione periodica → aggiornamento knowledge base)

### [09/06/26 15:49] Raccomandazione modello per GPU 4 GB

- [x] **Consigliare `deepseek-coder:6.7b-instruct-q4_K_M`** — aggiunto in `settings_llm.cpp` sezione Coding con nota "quantizzato Q4_K_M — per GPU ≤4 GB VRAM" — 2026-06-10

### [09/06/26 22:26] Feature — Sintetizzatore vocale da campione audio

- [x] **Sintetizzatore vocale da audio/campione di registrazione** — `gui/pages/widget_voice_cloner.h/cpp` + tab "🎤 Clona Voce" in MultimediaPage (indice 5); XTTS-v2 via Python subprocess; pulsante installa TTS; carica/registra campione; genera WAV; riproduci/salva — 2026-06-10

---

## 🗂️ Riorganizzazione cartelle root — 2026-06-05 (Paolo)

> Troppo piatta — unificare cartelle correlate in gruppi logici.

### [x] Frameworks/whisper/ — unisce whisper.cpp + whisper_win — 2026-06-05
### [x] ENGINE_LLM/ — raggruppa tutti i motori LLM — 2026-06-05
### [x] Test/TEST_OPENCODE/ — spostato dentro Test/ — 2026-06-05
### [x] TOOL_TIP/ — KNOWLEDGE_USER + BEST_PRACTICE_&_GOAL — 2026-06-05
### [x] Tools/ — scripts/ + cmake/ — 2026-06-05
> Tutti i riferimenti C++, Python, shell aggiornati e build verificata — 2026-06-05

---

## 🔬 Calcolo Scientifico Distribuito (BOINC-like) — TODO aperti

> Sessione 2026-06-05 — sistema base implementato e funzionante (porta 11601,
> SQLite WU queue, dispatch, quorum SHA-256, pipeline, LLM scientifico).
> Queste 3 feature completerebbero il sistema per uso reale in laboratorio.

---

### [x] WU Generator da dataset — 2026-06-05
**Priorità: ALTA** — senza questo creare 1000 task è impossibile manualmente.

Permettere di caricare un file (FASTA multi-sequenza, CSV, lista di UniProt ID)
e generare automaticamente N Work Unit, una per ogni riga/sequenza del file.

**Implementazione:**
- Pulsante "📂 Genera WU da file" nel form Nuova Work Unit
- Dialog: seleziona file + tipo task da applicare a ogni elemento + params template
- Parser: FASTA (split per `>`), CSV (ogni riga = un WU), TXT (una riga = un WU)
- Limita a max 500 WU per batch per evitare saturazione
- Barra di progresso creazione WU
- File: `gui/pages/main_sci_compute_wu_gen.cpp` (nuovo) oppure inline in `main_sci_compute_ui.cpp`

**Esempio d'uso:**
```
File: sequences.fasta  (500 sequenze)
Tipo: blastn
→ crea 500 WU automaticamente, distribuite sui nodi disponibili
```

---

### [x] Result Aggregator — 2026-06-05
**Priorità: ALTA** — senza questo i risultati restano isolati per WU.

Dopo che tutte le WU di una pipeline (o di un batch) sono `done`/`validated`,
aggregare gli output in un unico file scaricabile (CSV, JSON, FASTA, report HTML).

**Implementazione:**
- Pulsante "📊 Aggrega risultati" nella toolbar tabella WU
- Selezione: tutte le WU con stesso `pipeline_id` oppure selezione manuale
- Formato output configurabile: CSV (una riga per WU), JSON array, testo concatenato
- Per BLAST: aggrega tabelle TSV in un unico file ordinato per e-value
- Per LLM: concatena le risposte con separatori e intestazioni WU
- Salva in `~/.prismalux/sci_results/YYYY-MM-DD_HH-MM_pipeline-id.csv`
- File: `gui/pages/main_sci_compute_aggregator.cpp` (nuovo)

**Esempio d'uso:**
```
500 WU blastn tutte done
→ "Aggrega" → 1 file TSV con tutti i risultati + metadati (nodo, tempo, WU-id)
```

---

### [x] Credit counter per nodo — 2026-06-05
**Priorità: MEDIA** — importante per capire affidabilità e contributo di ogni macchina.

Tracciare per ogni nodo: ore CPU contribuite, WU completate con successo,
WU fallite, WU conflitto quorum. Visualizzato nella tabella Nodi.

**Implementazione:**
- Nuove colonne in `sci_nodes` SQLite: `wu_done INTEGER`, `wu_error INTEGER`,
  `cpu_seconds_total INTEGER`, `last_wu_completed INTEGER`
- Aggiornare i contatori in `handleWorkerMessage()` quando arriva `t=result`
- Colonne extra nella tabella Nodi: "WU✅", "WU❌", "CPU ore"
- Badge colorato: verde se >90% successo, giallo se >70%, rosso sotto 70%
- File: modifica `main_sci_compute.cpp` + `main_sci_compute_ui.cpp`

**Esempio visualizzazione tabella Nodi:**
```
Nome      | CPU | RAM | Tool           | WU✅ | WU❌ | CPU ore | Status
PC-Lab3   |  16 | 64  | blast R gmx    |  847 |    3 |   142h  | idle
Laptop    |   8 | 16  | python3        |  203 |   12 |    31h  | busy
```

---

### [x] (extra) Progress streaming per task lunghi — 2026-06-05
**Priorità: BASSA** — nice-to-have per simulazioni MD e BLAST su database grandi.

GROMACS, BLAST e altri tool emettono progresso su stderr durante l'esecuzione.
Catturarlo e trasmetterlo al coordinator in tempo reale (ogni 5s) invece di
aspettare il completamento.

**Implementazione:**
- In `executeLocally()`: usare `QProcess::readyReadStandardError` invece di
  aspettare `finished()`
- Nuovo messaggio protocollo: `{"t":"progress","wu_id":"...","pct":45,"msg":"step 45000/100000"}`
- In `handleWorkerMessage()`: aggiornare colonna Status con percentuale
- Mostrare nella tabella WU: `🔄 running 45%` invece di `🔄 running`

---

## 🤖 Dev Agent LangGraph (in sviluppo 2026-06-03)

> Agente AI locale per modificare il codice di Prismalux in autonomia: legge file, genera patch, compila, si corregge sugli errori.

### Stack
- **LangGraph** (Python) come orchestratore del grafo agente
- **Ollama** con modello coding dedicato (default: `deepseek-coder:6.7b` già installato; opzionale: `qwen2.5-coder:3b` ~2GB più leggero)
- **MCP** `devagent_mcp/` — server Python IPC JSON stdin/stdout (pattern identico al Telegram Bot)
- **Tab "Dev Agent"** in AppController [7] → tab 10

### Grafo LangGraph
```
START → read_context → generate_patch → apply_patch → compile
                              ↑ (errori compilazione) ←──────
                              ↓ (OK)
                         run_tests → done
```

### Tool del grafo
- `read_file(path)` — legge un file del progetto
- `list_files(pattern)` — glob sul progetto
- `write_file(path, content)` — scrive la modifica
- `bash(cmd, timeout)` — esegue cmake, ctest, git diff
- `search_code(query)` — grep nel progetto

### UI in AppController
- `QLineEdit` — task in linguaggio naturale ("Aggiungi tooltip al pulsante X in main_ai_ui.cpp")
- `QComboBox` — scegli modello coding (deepseek-coder:6.7b / qwen2.5-coder:3b)
- `QTextEdit` — log step-by-step (Read→Generate→Compile→Fix→Done)
- `QPushButton` — Avvia / Ferma / Applica diff / Annulla
- `QTextEdit` — diff finale (colorato rosso/verde)

### TODO
- [x] **`MCPs/devagent_mcp/server.py`** — 29KB: DevAgentState, 5 nodi LangGraph (con fallback loop Python), tool read/write/bash/search_code, ollama_chat HTTP diretto, parser unified diff + rollback automatico — 2026-06-03
- [x] **`MCPs/devagent_mcp/requirements.txt`** — langgraph, langchain-community, langchain-ollama, unidiff (tutti opzionali con fallback) — 2026-06-03
- [x] **Tab Dev Agent in AppController** — UI Qt completa: task input, model combo (deepseek-coder/qwen2.5-coder), log step-by-step, diff colorato, Avvia/Ferma/Installa — `main_app_controller.h/cpp/slots.cpp` — 2026-06-03
- [x] **Pulsante download qwen2.5-coder:3b** — aggiunto nella sezione Coding di LLM consigliati (`settings_llm.cpp`): "~2.0 GB — ideale per Dev Agent su PC con risorse limitate" — 2026-06-04

---

## 🆕 Sessione 2026-06-03 — Nuovi TODO (turno 6, da PROMPT.txt 19:35)

### 🔧 Bug / Fix
- [x] **Codice fiscale check digit corretto** — bug: posizioni pari con lettere usavano `10+(c-'A')` invece di `(c-'A')`; fix in `pratico_calcs.h`; test `casoRealeRossiMario` aggiornato a RSSMRA90A01H501W; LBLPLA89B15C351G ora corretto — 2026-06-03
- [x] **2 pulsanti Gestore LLM non si comprimono** — `setFixedWidth(200)` → `setMinimumWidth(220)/setMaximumWidth(280)` — `settings_ai.cpp` — 2026-06-03
- [x] **Tile 4ª colonna tronca** — aggiunta QLabel con setWordWrap(true) nella band inferiore della card tema — `settings_visual.cpp` — 2026-06-03

### 🎨 UI / UX
- [x] **Bottone "ℹ" → "ℹ Informazioni"** — rinominato in `main_ai_ui.cpp` riga 152 — 2026-06-03
- [x] **Bottone PDF con testo** — `"\xf0\x9f\x93\x84  PDF"` in `main_ai_ui.cpp` riga 138 — 2026-06-03
- [x] **"Scarica LLM" → "Scarica"** — abbreviato label header in `mainwindow.cpp` riga 645 — 2026-06-03
- [x] **"AI Locale" → "Gestione LLM"** — tab rinominata in `settings_main.cpp` righe 116+187 — 2026-06-03
- [x] **Agentica: selettore modello spostato** — integrato in `buildAgenticaToolbar()` a sinistra di "linguaggio" — `main_programming.cpp` — 2026-06-03
- [x] **Pulsante "Traccia" fisso sotto scroll** — spostato fuori dalla QScrollArea in `buildLeftPanel()`; `outerLay->addWidget(btnPlot/btnReset)` dopo `sc` → sempre visibile senza scroll — `main_graph.cpp` — 2026-06-03

### 🛑 Stop & controllo compilazione
- [x] **Stop compilazione llama.cpp** — pulsante "⏹ Ferma" accanto a Compila; `onLlamaStopClicked()` fa `kill()` su m_proc2/m_proc3 — `main_customize.h/cpp` — 2026-06-03
- [x] **Controllo compilazione recente** — se llama-server compilato <24h fa mostra QMessageBox::question prima di ricompilare — `main_customize.cpp` — 2026-06-03

### 🔊 Voce / TTS
- [x] **espeak-ng** — sostituita nota con "Usa Piper TTS per qualità superiore"; fallback code nel pulsante Parla invariato — `settings_voice.cpp` — 2026-06-03
- [x] **Test Whisper live** — `test_stt_whisper_live` 21 PASS 1 SKIP (paths/ImpostazioniPage/transcribe/savePreferredModel) — 2026-06-05

### 📋 Feature
- [x] **Copia pip clipboard** — pulsante 📋 aggiunto in: main_tools_file (3x), widget_stable_diffusion, main_lan_wan GNS3, main_maintenance NPU — 2026-06-03
- [x] **Chat: Canc = conferma, Shift+Canc = 5s undo** — eventFilter su m_chatList; QMessageBox::question per Canc; label "Annulla (5s)" + QTimer per Shift+Canc — `mainwindow.h/cpp/slots.cpp` — 2026-06-03
- [x] **Emoji Telegram nel software** — `setupEmojiFallback()` in `main.cpp`: cerca NotoColorEmoji nei path sistema+bundle, lo registra via `QFontDatabase::addApplicationFont`, imposta come fallback globale con `QFont::setFamilies()`; copiato in `gui/build_gui/fonts/`; `aggiorna.sh` e `crea_appimage.sh` aggiornati per includerlo nel bundle — 2026-06-04
- [x] **Tool use per modelli abilitati** — 9 tool disponibili: calc, fetch_url, ricerca, leggi_file, lista_file, python, **search_rag** (RAG locale), **graph_memory** (GraphMemory SQLite), **get_knowledge** (Knowledge Base); bolla HTML stilizzata in-place con status running→done/error — `main_ai_tools.cpp`, `main_ai_pipeline.cpp` — 2026-06-04
- [x] **Grafico 3D: Punti/Wireframe/Superficie + movimento Blender** — `m_renderModeCombo` per Scatter3D/Grafo3D; `setRenderMode(int)` slot; Blender orbit (MMB/Alt+LMB) + pan (Shift+MMB) — `main_graph.h/cpp`, `main_graph_canvas.cpp` — 2026-06-03
- [x] **Sezione Lavoro web** — tab "💼 Lavoro" in webchat.html + endpoint `GET /api/lavoro?q=QUERY` in lan_server.cpp; ricerca case-insensitive su azienda/ruolo/sede — 2026-06-03
- [x] **Quiz errori con spiegazione** — aggiunta introduzione "Ripassiamo insieme..." + legenda Verde/Rosso nel dialog "Rivedi errori" — `main_learn.cpp` — 2026-06-03

### 📖 Documentazione
- [x] **README.md API esterne** — tabella "Riferimenti API ufficiali" con Blender/FreeCAD/Bioconda/GDScript/KiCAD/Ollama pip — `README.md` — 2026-06-03
- [x] **Regola test** — aggiunta regola in CLAUDE.md: ogni nuova suite test deve essere registrata nella tabella Suite con nome ctest, categoria e numero PASS — 2026-06-03
> Build: `python3 build.py`  (Linux/macOS/Windows — oppure `cmake --build gui/build_gui -j$(nproc)` su Linux)

---

## 🆕 Sessione 2026-06-03 — Nuovi TODO (turno 2)

### 🔧 Bug / Fix llama.cpp
- [x] **llama-server crash durante startup → non ricade più su Ollama** — `onServerProcFinished()` ora distingue crash-durante-startup (m_healthTimer attivo) da terminazione normale; in caso di crash startup mostra diagnostica+ultime righe di log senza commutare il backend — `mainwindow_slots.cpp` — 2026-06-03
- [x] **Timeout caricamento modello** — `MAX_HEALTH_TICKS` portato da 180s a 300s (5 minuti) per modelli 33b+ su CPU — `mainwindow_slots.cpp` — 2026-06-03
- [x] **`--flash-attn` argomento invalido** — nuove versioni llama.cpp richiedono `--flash-attn auto|on|off` invece del flag booleano; corretto in `mainwindow.cpp` — 2026-06-03

---

## 🆕 Sessione 2026-06-03 — Nuovi TODO

### 🔧 Bug / Fix preesistenti (corretti oggi)
- [x] **`LanServer::onLlmFinishedQueued`** — dichiarazione orfana in `lan_server.h` rimossa — 2026-06-03
- [x] **`QuizPage::onRivediErroriClicked`** — spostata da `private slots` a `public slots` — 2026-06-03
- [x] **`lan_server.cpp` include** — `lavoro_data.h` → `main_jobs_data.h` — 2026-06-03

### 🎮 Feature
- [x] **Sfida "Rivedi errori"** — struct `WrongAnswer`, salvataggio sbagliati, pulsante con count, QDialog scrollabile opzioni colorate+spiegazione — `main_learn.h/cpp` — 2026-06-03

---

## 🚀 BOINC-like WAN Compute — TODO 2026-06-03

> Goal: trasformare il WAN Compute da "coda distribuita basilare" a sistema fault-tolerant simile a BOINC per ricerca personale (analisi AI, Python, grafici distribuiti su 2-10 PC LAN).

### 🔴 Critici — Fault Tolerance (senza questi si perde il lavoro)
- [x] **Riassegnazione task su disconnessione nodo** — `onWanNodeDisconnected()`: task "running" del nodo morto tornano "pending"; `retryCount++`; dopo 3 tentativi → "failed" — `main_lan_wan.cpp` — 2026-06-03
- [x] **Heartbeat 30s server→nodo** — `QTimer` 30s invia `{"t":"ping"}`; nodo risponde `{"t":"pong"}`; se nodo non risponde per 90s → rimosso → task riassegnati — `main_lan_wan.h/cpp` — 2026-06-03
- [x] **Priority scheduler** — campo `priority` (0=bassa/1=normale/2=alta) in `WanTask`; `wanDispatch()` sceglie il task pending con priorità massima invece del primo FIFO — `main_lan_wan.h/cpp` — 2026-06-03
- [x] **`startedAt` tracking** — campo `QDateTime startedAt` in `WanTask`; settato in `wanDispatch()`; ETA stimata nella stats label — 2026-06-03
- [x] **Stats label BOINC-style** — label sotto le tabelle: nodi idle/working, task in coda/running/completati/falliti — `main_lan_wan.cpp` — 2026-06-03

### 🟡 Sicurezza
- [x] **TLS self-signed LAN** — `QSslServer` + `_ensureCert()` + checkbox "Abilita TLS" in Manutenzione LAN; fallback HTTP se openssl non disponibile — `lan_server.h/cpp`, `main_maintenance_lan.cpp` — 2026-06-03
- [x] **Coda FIFO multi-utente LAN** — `PendingLlmRequest` + `m_llmQueue` (max 10); `handleChat/Generate` accodano se occupato; `closeStreamSession/onClientDisconnected` servono il prossimo — `lan_server.h/cpp` — 2026-06-03
- [x] **Parser HTTP hardening** — whitelist metodi (GET/POST/PUT/DELETE/PATCH/HEAD/OPTIONS); blocco path con null byte o >2048 char; validazione Content-Length (negativo o >25MB → 400); tutti con `disconnectFromHost()` — `lan_server.cpp` — 2026-06-03

### 🟢 Dashboard BOINC-style
- [x] **Throughput real-time** — `m_wanThroughputLbl` aggiornato ogni 5s: task/ora (finestra 60min) + ETA stimata — `main_lan_wan.h/cpp` — 2026-06-03
- [x] **ETA globale** — calcolata da media `startedAt→now` sui task completati, divisa per worker attivi — 2026-06-03
- [x] **Esportazione CSV** — `m_wanExportBtn` → QFileDialog → CSV completo (id/kind/stato/nodo/retry/priority/created/startedAt/payload/result) — 2026-06-03
- [x] **Nodo multi-worker** — `WanWorker` struct (sock+ai+pollTimer per worker); SpinBox 1-4; `onWanCliConBtnClicked` crea N worker ognuno indipendente; 8 slot nominati per-worker; `wanWorkerHandleTask` dispatcher 28 task — `main_lan_wan.h/cpp` — 2026-06-03

### 📦 Headless server (produzione LAN)
- [x] **`--server` CLI flag** — `main.cpp::runHeadlessServer()` con `--port` e `--token`
- [x] **systemd user unit** — `EXPORT/linux/prismalux-server.service` creato — 2026-06-03
- [x] **Watchdog crash-restart** — `Restart=on-failure` + `StartLimitBurst=5` + `TimeoutStartSec/StopSec` + `OLLAMA_HOST=127.0.0.1` nell'env della unit — `EXPORT/linux/prismalux-server.service` — 2026-06-03
- [x] **Log strutturato JSON** — `appendAccessLog` scrive JSON a riga singola `{"t":"…","ip":"…","m":"…","p":"…"}`; rotazione automatica >10 MB (mantiene `.1`) — `lan_server.cpp` — 2026-06-03

---

## 🆕 Sessione 2026-06-02 — Nuovi TODO

### 🔧 Bug / Fix UI
- [x] **QR code** — aumentato di ~20px (+8%): 260→280 dialog, 256→276 inline; testo spostato sotto con `\n\n` — 2026-06-02
- [x] **Docker "Scarica" bottone** — nascondere il bottone dopo che l'immagine è stata scaricata con successo — 2026-06-02
- [x] **π calcolo** — bug `TypeError: object of type 'int' has no len()` in `matematica_page.cpp` (rimosso `.__len__()`) — 2026-06-02
- [x] **Grafico 2D/3D** — rimosso stretch factor 1 da `m_paramStack` — 2026-06-02
- [x] **Voce Piper** — aggiunto scroll indipendente su pannello sinistro (Piper) e destro (Whisper), non si sovrappongono più — 2026-06-02
- [x] **Simulatore** — aggiunto `setMaxVisibleItems(20)` + `ScrollBarAsNeeded` sul combo algoritmi — 2026-06-02
- [x] **Sfida** — salvataggio domande sbagliate (`WrongAnswer`), bottone "Rivedi errori" con dialog scroll+colori — 2026-06-02

### 🎨 UI / UX
- [x] **Selettore LLM globale** — aggiunto `ModelComboBox` in `ricerca_page.cpp`: tab Paper e Brevetto ora hanno selettore modello — 2026-06-02
- [x] **Visuale → Aspetto** — 4 colonne invece di 3 per la griglia dei temi — 2026-06-02
- [x] **Visuale → Grafico** — spostato "preset stile grafico" sotto "carattere etichette" — 2026-06-02
- [x] **AI Locale + Connessione** — `buildUpdateGroup` spostato nella `colsRow` a destra di `buildAdvancedConfigGroup` in `manutenzione_page.cpp` — 2026-06-02

### 📊 Feature / Analisi
- [x] **Analisi 1 e Analisi 2** — aggiunto `GraficoCanvas` con auto-plot al cambio topic (`plotEx`); split orizzontale testo+grafico — 2026-06-02

### 🔨 Build / Infrastruttura
- [x] **llama.cpp compilazione Vulkan** — fallback automatico a `GGML_VULKAN=OFF` se cmake detect "glslc" mancante; avviso nel log — 2026-06-02
- [x] **Anki MCP server v0.18.5** — aggiornato `MCPs/anki_mcp/server.py` con API compatibile v0.18.5: +create_notes_bulk, +get_note, +update_note, +delete_notes, +get_note_types, +get_tags, +sync_anki, +get_cards_due — 2026-06-02

### 🔁 Rinomina codice
- [x] **Rinomina pagine GUI** — 96 file rinominati con `git mv`; tutti gli include in `mainwindow.cpp`, `mainwindow_slots.cpp`, `gui/pages/` e `gui/tests/` aggiornati; `CMakeLists.txt` aggiornato — 2026-06-02

### 📱 Messaging
- [x] **Telegram** — aggiunta sezione "Contatti promozionali" in `buildTelegramTab()`: lista contatti, messaggio, "Invia a tutti" via bot API Telegram — 2026-06-02
- [x] **WhatsApp** — aggiunta sezione "Contatti promozionali" in `buildWhatsAppTab()`: lista numeri, messaggio, "Invia a tutti" via bridge URL — 2026-06-02

---

## 📂 Feature pendenti

- [x] **Auto-copia documento in RAG/** — quando l'utente carica un file (drag & drop o browse)
  per l'indicizzazione RAG e il file **non risiede già** in `Prismalux/RAG/`,
  copiarlo automaticamente dentro quella cartella prima di indicizzarlo.

  **Perché:** i documenti fuori da `RAG/` vengono indicizzati in sessione ma non
  ritrovati al riavvio (RagGraph scansiona solo `~/prismalux_rag_docs/` e `P::root()+"/RAG/"`).
  La copia garantisce persistenza senza che l'utente debba spostarli manualmente.

  **Comportamento atteso:**
  - File già in `RAG/` → indicizza senza toccare nulla
  - File esterno → copia in `RAG/<nomefile>` (sovrascrittura opzionale se esiste già)
  - Mostra messaggio: `"📄 Copiato in RAG/ — il documento è ora persistente"`
  - Se la copia fallisce (permessi, disco pieno) → indicizza comunque dalla posizione originale
    e avvisa l'utente

  **Punti di intervento:**
  - `agenti_page_ui.cpp` — zona drop RAG (drag & drop PDF/txt/md)
  - `impostazioni_page_ai.cpp` o `impostazioni_page_slots.cpp` — browse file per indicizzazione
  - `rag_graph.cpp::addFile()` — eventuale copia automatica a livello di engine

---

## 🚨 CRITICO — WAN Compute è RCE non autenticato (audit codice 2026-05-31)

> Audit diretto di `lan_wan_page.cpp` (2977 righe). **Confermato nel codice**, non sospetto.
> Questo è il problema di sicurezza più grave del progetto: esecuzione di codice arbitrario
> da rete, senza autenticazione né cifratura, in entrambe le direzioni.

### 🔴🔴 Bloccante assoluto — NON esporre il WAN Compute finché non risolto

- [x] **Worker esegue comandi shell arbitrari dal server, senza auth** — opt-in checkbox + sandbox 2026-06-01
  - Il worker (`onWanCliSockReadyRead`, riga 2131) riceve un messaggio `task` e chiama
    `wanCliHandleTask` che per `kind=="shell_cmd"`/`"git_cmd"` esegue **`bash -c payload`**
    e per `python_repl`/`eval_script`/`matplotlib_plot` esegue **`python3 -c payload`**
    (righe 2837-2867) — payload grezzo, nessun controllo token, nessuna conferma.
  - **Impatto:** chiunque controlli (o impersoni, manca TLS) il server a cui ti connetti
    ti manda `{"t":"task","kind":"shell_cmd","payload":"<comando>"}` → RCE sulla tua macchina.
  - Nota: `math_expr` (riga 2849) è gestito con escaping "sicuro per espressioni" — quindi
    il rischio era noto, ma `shell_cmd`/`python_repl` restano completamente aperti.

- [x] **Server registra ed esegue task da chiunque, senza verificare il token** — token auth hello 2026-06-01
  - `onWanNodeReadyRead` accetta `hello`/`poll`/`result`/`spawn_tasks` da qualsiasi socket;
    **non c'è alcun controllo del Bearer token** (caricato ma mai verificato lato WAN).
  - `spawn_tasks` (riga 1919) lascia a un client non autenticato **iniettare nuovi task**
    (`kind`+`payload` arbitrari, es. `shell_cmd`) nella coda, che il server poi **distribuisce
    ad altri nodi onesti** → propagazione tipo worm dell'RCE.

- [x] **WAN bind 0.0.0.0 senza TLS** — checkbox "Esponi su tutte le interfacce" (default OFF→127.0.0.1); check Ollama esposto su avvio; UI TLS self-signed con openssl — 2026-06-02
  - Il server WAN ascolta su tutte le interfacce; nessuna cifratura → MITM banale su WiFi
    può impersonare il server e iniettare task shell ai worker.

- [x] **Capability `"shell"` di default** — `lan_wan_page.cpp` — rimossa 2026-06-01
  - I nodi annunciano `caps = {"ai","shell"}` di default → opt-in automatico all'esecuzione
    di comandi shell. Dovrebbe essere opt-out esplicito con conferma utente.

### Rimedi (ordine)
1. **Auth obbligatoria su entrambi i lati WAN**: verificare il Bearer token in `onWanNodeReadyRead`
   (server) e autenticare il server prima di eseguire task (worker). Rifiutare connessioni senza token.
2. **Sandbox/whitelist per i task pericolosi**: `shell_cmd`/`python_repl`/`eval_script` dietro
   conferma esplicita per-task o disabilitati di default; eseguire in sandbox (container/seccomp).
3. **TLS sul canale WAN** o restrizione a `127.0.0.1` + tunnel fidato.
4. **Capability opt-out**: niente `"shell"` di default; l'utente abilita esplicitamente.
5. Finché non fatto: **disabilitare la modalità Rete LAN del WAN Compute** o avviso a tutto schermo.

---

## 🔍 Superfici di sicurezza ancora NON auditate (2026-05-31)

> Onestà sullo stato: auditati a fondo solo `lan_server.cpp/.h` e `lan_wan_page.cpp`.
> Le seguenti superfici NON sono state verificate — non assumere che siano sicure.

- [x] **18 plugin MCP Python** — audit completato 2026-06-02: 10 corretti (bioconda whitelist, avogadro/graphviz/rdkit path traversal, freecad/godot/kicad code injection, ollama/opencode/tinymcp arg validation), 5 già sicuri (anki, cytoscape, gns3, knowledge, obs)
  - Verificare che nessun plugin costruisca comandi shell / path da input non validato
    (command injection, path traversal). Validare i parametri lato server prima dell'inoltro.

- [x] **~20 file C++ con `QProcess`** — audit completato 2026-06-02: 2 vulnerabilità HIGH corrette (WAN `math_expr` eval injection in `main_lan_wan.cpp:2930`; LAN `expr`/`simplify` Python injection in `lan_server.cpp:3868`); restanti 23 file SICURI
  - File coinvolti: `programmazione_page_slots.cpp`, `app_controller_page.cpp`,
    `strumenti_file_page.cpp`, `ai_client.cpp`, `agenti_page_tools.cpp`, `pratico_page.cpp`,
    `lavoro_page.cpp`, `mainwindow_slots.cpp`, ecc.
  - Cercare costruzione di comandi/argomenti da input utente o da output LLM senza whitelist.

- [x] **App Android BLE audit** — `ble_crypto.h`: QRandomGenerator::securelySeeded() per salt/key, QSettings("Prismalux","Mobile"), isEmpty() guard; `ble_page.cpp`: Authorization flag (link-layer pairing); `ble_page.h`: m_cryptoEnabled=true di default — 2026-06-03

- [x] ~~`GraphMemory` SQL injection~~ — coperto da test (`test_graph_memory`, 65 test incl. SQL injection).

---

## 🔐 Hardening sicurezza LAN/Web (audit codice 2026-05-31)

> Audit del codice esistente (`lan_server.cpp/.h`, `prismalux_paths.h`).
> Verificati direttamente: token, bind, rate limiting, sandbox, path.
> **Da verificare** (sospetti fondati, non confermati): XSS web chat, robustezza parser HTTP.

### 🔴 Critici

- [x] **Token nell'URL `?token=` su HTTP in chiaro** — rimosso fallback API 2026-06-01
  - Il TLS è disabilitato di proposito (`lan_server.cpp:148-150`) ma il token è accettato
    come query string e incluso nel QR code → viaggia in chiaro, finisce nei log proxy,
    sniffabile su WiFi. Il Bearer token autentica ma **non cifra il canale**.
  - **Rimedio:** accettare il token SOLO via header `Authorization: Bearer`; rimuovere il
    fallback `?token=` e l'inclusione del token nell'URL del QR (passare il token in un
    campo separato che l'app Android usa per costruire l'header).

- [x] **Auth opzionale (token vuoto = nessuna auth)** — token auto-generato UUID 32-char se vuoto, salvato in keyring, segnale `tokenAutoGenerated` — 2026-06-02
  - Se l'utente non imposta un token, `/api/chat`, `/api/generate`, ecc. sono aperte a
    chiunque sulla rete. È opt-in; per un server che espone un LLM dovrebbe essere opt-out.
  - **Rimedio:** generare un token di default al primo avvio del server; disattivazione
    solo esplicita con avviso.

### 🟡 Importanti

- [x] **Bind LAN configurabile** — `setBindAddress(QHostAddress)` pubblico; headless usa `LocalHost` di default; UI può passare IP LAN specifico — `lan_server.h/cpp` — 2026-06-03
  - Il server ascolta su tutte le interfacce, non solo la LAN. Su hotspot/rete pubblica
    è esposto oltre l'intenzione. Esiste già `P::kLocalHost` documentata come
    "unico valore accettato per sicurezza" ma non applicabile qui (serve raggiungibilità dal telefono).
  - **Rimedio:** bind sull'interfaccia LAN specifica, oppure avviso esplicito quando
    l'interfaccia attiva è una rete pubblica/non fidata.

- [x] **KaTeX da CDN jsdelivr** — endpoint `/katex/` serve da `/usr/share/javascript/katex/` con path-traversal protection + cache 24h; CDN rimosso — 2026-06-02
  - La chat web carica JS/CSS da `cdn.jsdelivr.net`: rischio supply-chain, privacy
    (il telefono contatta un terzo) e niente offline. Sul desktop KaTeX è locale
    (`/usr/share/javascript/katex/`) → coerenza rotta.
  - **Rimedio:** servire KaTeX come risorsa Qt locale anche nella web app.

- [x] ~~XSS nella web chat~~ — **VERIFICATO SICURO** (`lan_server.cpp:2333`)
  - I messaggi chat (utente + output LLM) sono inseriti via `d.textContent=t` → escaping
    automatico del browser. Nessun XSS sul flusso chat.

- [x] **XSS residuo nella lista offerte lavoro** — sostituito innerHTML con createElement/textContent 2026-06-01
  - La lista `/api/lavoro` (dati esterni Indeed) e alcuni pannelli tool costruiscono il DOM
    con `innerHTML` concatenando stringhe. Se un titolo/descrizione offerta contiene HTML
    → injection nel browser del client.
  - **Rimedio:** usare `textContent`/`createElement` anche qui, o sanificare i dati esterni
    prima dell'inserimento.

- [x] **CORS `Access-Control-Allow-Origin: *`** — rimosso del tutto (app Android nativa + web app same-origin non ne hanno bisogno) — 2026-06-02
  - Qualsiasi pagina web aperta sul telefono/PC del client può chiamare le API del server
    (il token in `localStorage` resta protetto dalla same-origin per la lettura, ma le
    richieste cross-origin partono comunque). Header di sicurezza X-Frame-Options/nosniff
    già presenti — buono — ma il CORS wildcard è troppo permissivo.
  - **Rimedio:** restringere l'origin agli host attesi o rimuovere il CORS se non serve.

- [x] **Parser HTTP manuale verificato** — limite 4MB buffer totale (riga 442), Content-Length max 25MB, whitelist 7 metodi, null-byte+len>2048 → 400, nessun difetto trovato — 2026-06-03
  - Parsing manuale di header/`Content-Length`/body: classe di bug nota
    (request smuggling, edge case encoding). Nessun difetto evidente trovato.
  - **Azione:** fuzzing del parser; valutare migrazione a un parser HTTP collaudato.

### 🧹 Manutenibilità correlata

- [x] **Estrarre la web UI dalle stringhe C++** — `lan_server.cpp` da 4574 → 2136 righe (-2438); HTML/JS ora in `gui/lan_web/webchat.html` (509 righe, 14 placeholder) + `gui/lan_web/index.html`; caricati via `QFile(":/lan/webchat.html")` + `replace()` placeholder `{{MODEL}}`/`{{AUTH_HEADERS_JS}}` — `lan_web.qrc` — 2026-06-03

---

## 🚀 Produzione LAN — readiness operativa (2026-05-31)

> Cosa manca per far girare il server su una macchina della LAN in modo stabile.
> Verificato nel codice: server embedded nella GUI, singolo `m_ai`, singolo `m_streamSock`.

### 🔴 Bloccanti

- [x] **Modalità headless (server senza GUI)** — `--server CLI flag` + `runHeadlessServer()` + systemd unit — già implementato 2026-06-03
  - *Headless* = avviare SOLO il server da CLI, senza aprire finestre, così gira su un
    mini-PC anche senza monitor e resta attivo 24/7.
  - **Rimedio:** flag tipo `prismalux --server --port N` che istanzia `LanServer` senza
    `QApplication` GUI (o con `QGuiApplication`/`QCoreApplication`); avvio come servizio
    (systemd user unit su Linux, servizio/Task Scheduler su Windows) con restart-on-crash.

- [x] **Coda FIFO multi-utente** — `PendingLlmRequest` + `m_llmQueue` (max 10) + `serveLlmQueue()` — già implementato 2026-06-03
  - *Coda* = se due client chattano insieme le richieste vengono servite in fila, una alla
    volta, invece di mescolare gli stream (oggi il 2° stream ruba il socket al 1°).
  - *Isolamento* = ogni utente ha contesto/cronologia separati; oggi tutti condividono lo
    stesso `~/.prismalux` → un client può vedere dati/risposte di un altro.
  - **Rimedio minimo:** coda FIFO delle richieste LLM (serializzazione esplicita).
  - **Rimedio completo:** un `AiClient` per sessione + namespace dati per-utente.

### 🟡 Importanti

- [x] **`/api/launch` disabilitato in headless** — flag `m_headless`; `setHeadless(true)` in `runHeadlessServer`; risponde 403 se headless — `lan_server.h/cpp`, `main.cpp` — 2026-06-03
  - Whitelist presente (no comando arbitrario) ma chiunque col token apre un terminale
    sul display della macchina server. Su host headless non ha senso ed è una capability
    pericolosa esposta in rete.
  - **Rimedio:** disabilitare `/api/launch` in modalità server/headless o renderlo opt-in.

- [x] **IP DHCP auto-aggiornamento QR** — `QTimer` 30s in `buildLanAndroidTab()` → `onIpWatchTick()`: se IP cambia aggiorna QR inline e mostra avviso in `m_urlDisplayLbl` — `main_lan_wan.h/cpp` — 2026-06-03
  - **Rimedio:** IP statico/reservation per il server; sull'host aprire SOLO la porta del
    server e tenere Ollama (`11434`) in ascolto solo su localhost, mai esposto in LAN.

- [x] **Backup DB** — GroupBox "Backup dati" in Impostazioni→Pulizia: copia graph_memory.db, rag_graph.db, chat_history.db, access.log in cartella scelta con timestamp; pulsante "Apri cartella dati" — `settings_system.cpp` — 2026-06-03
  - **Rimedio:** rotazione log; backup periodico dei DB `~/.prismalux` (chat, RAG, GraphMemory).

---

## 🔒 Sicurezza — punti aggiuntivi da chiudere (2026-05-31)

> Per stare tranquilli oltre all'hardening LAN già elencato sopra.

### 🔴 Da fare prima di esporre il server

- [x] **Ollama esposto solo in locale** — `onOllamaCheckDone()` controlla sia `ss -tlnp | grep 11434` sia env `OLLAMA_HOST`; avviso arancione se esposto; check attivato ad ogni avvio server WAN (non solo in exposeAll) — `main_lan_wan.cpp` — 2026-06-03
  - Ollama non ha auth: se ascolta su `0.0.0.0` chiunque in LAN usa l'LLM senza token,
    bypassando del tutto l'auth di Prismalux.

- [x] **TLS opzionale su LAN** — `QSslServer` + `_ensureCert()` + checkbox "Abilita TLS" in Manutenzione LAN — già implementato 2026-06-03
  - Senza TLS chat e dati LLM viaggiano in chiaro sul WiFi (combinato col token-in-URL
    già segnalato è un doppio problema). Codice cert già predisposto (`lan_server.cpp:115`).

### 🟡 Igiene generale

- [x] **Audit segreti** — GroupBox in Impostazioni→Pulizia: 5 controlli (.env in .gitignore, *.key in .gitignore, permessi 0600, token LAN non in QSettings, KNOWLEDGE_USER/RAG in .gitignore) — `settings_system.cpp` — 2026-06-03
  - Nessun token in `QSettings`/codice (già usa QKeychain); verificare che `KNOWLEDGE_USER/`,
    `RAG/`, `.env`, `*.key` siano in `.gitignore` e con permessi `0600`.

- [x] **Rate limiting esteso a tutti gli endpoint** — aggiunto checkHeavyRateLimit 6/min per whisper/graphviz/mcp/launch 2026-06-01
  - Estendere a `/api/whisper`, `/api/graphviz`, `/api/launch`, `/api/mcp` per evitare
    abuso/DoS (whisper e graphviz lanciano processi).

- [x] **Limite upload + tipi file** — Whisper: 413 se >25 MB, 415 se non audio/*; MCP: validazione method regex + params oggetto — 2026-06-02
  - Verificare limite dimensione e validazione tipo/estensione prima di passare a processi
    esterni (python3, dot, whisper).

- [x] **Validazione input MCP** — verificato JSON object + method regex `[a-zA-Z0-9/_-]{1,64}` + params oggetto prima dell'inoltro — 2026-06-02
  - Assicurarsi che i parametri passati ai MCP siano validati lato server, non solo lato MCP.

- [x] **Scanner dipendenze OSV** — tab "Dipendenze" in SecurityAnalyzerPage: legge `requirements.lock`, POST a `api.osv.dev/v1/querybatch`, mostra CVE in rosso/verde — `main_security.h/cpp` — 2026-06-03

---

## 🛡️ Analizzatore di Sicurezza Difensiva

> Scopo: aiutare l'utente a **trovare e correggere** falle — mai a sfruttarle.
> Posizionamento naturale: sub-tab in **Programmazione [4]** o pannello dedicato in **Ricerca [6]**.

- [x] **SecurityAnalyzerPage** — 4 agenti Ollama paralleli in AppController [7], 2026-06-01

  ### Analisi codice (LLM locale)
  - Carica file sorgente (C++/Python/JS/Bash) o incolla testo
  - LLM analizza e segnala:
    - Credenziali/token hardcoded nel codice
    - Comandi shell non sanificati (injection risk)
    - Buffer/memory issues (C/C++: strcpy, gets, sprintf senza bounds)
    - Dipendenze con versioni note vulnerabili
    - Path traversal, open redirect, SSRF nei server HTTP
  - Output: lista falle con **severità** (critica/alta/media/bassa) + **rimedio suggerito**
  - "Spiega il rischio" — bottone per approfondimento AI sulla singola falla

  ### Scanner dipendenze (offline)
  - Legge `requirements.txt`, `requirements.lock`, `CMakeLists.txt`, `package.json`
  - Confronta con una copia locale del database OSV/NVD (aggiornabile manualmente)
  - Segnala: pacchetto → CVE → CVSS score → versione sicura disponibile
  - Esporta report in Markdown/PDF

  ### Audit configurazione locale
  - Controlla porte aperte in ascolto sul PC (confronta con quelle note di Prismalux)
  - Verifica permessi file sensibili (`KNOWLEDGE_USER/`, token LAN, `.claude/`)
  - Controlla che i token LAN siano nel keychain e non in QSettings in chiaro
  - Segnala `.env` o file con credenziali senza `.gitignore`

  ### Audit dipendenze Prismalux stesso
  - Hash SHA-256 dei GGUF in `KNOWLEDGE_USER/model_hashes.json` — già implementato, da integrare nella UI
  - Verifica integrità dei 18 MCP Python (hash file sorgente al primo avvio, confronto ad ogni riavvio)
  - Alert se un MCP viene modificato esternamente

  ### Stack previsto
  - LLM locale (Ollama) per analisi semantica del codice
  - `pip-audit` o parsing offline di OSV JSON per CVE dipendenze
  - `ss -tlnp` / `QNetworkInterface` per porte locali
  - Nessuna chiamata cloud, nessun invio di codice all'esterno

---

## 🧪 Test mancanti (gap analysis 2026-05-30)

> Suite attuali: 51 (48 no-Ollama, 3 richiedono Ollama). Le aree sotto non hanno ancora suite ctest.

### 🔴 Critici — logica complessa, zero test

- [x] **test_pratico_finanza** — 27 PASS: TestNormalizzaComune (7) + TestCercaBelfiore (8) + TestCalcolaCodiceFiscale (12) — 2026-06-03
- [x] **test_wan_compute_tasks** — 33 PASS: costruzione+sicurezza (7), protocollo TCP (7), formato JSON (8), math_expr (5), sicurezza shell (6) — 2026-06-03
- [x] **test_rag_graph_pipeline** — 25 PASS: costruzione (7), parsing JSON LLM (8), ricerca (5), segnali (5) — 2026-06-03

### 🟡 Importanti — funzionalità usate quotidianamente

- [x] **test_docker_sandbox** — 29 PASS, 2 SKIP (Docker assente): costruzione, docker --version, PythonExec `print('ok')`, sicurezza sandbox — 2026-06-03
- [x] **test_translitter** — 37 PASS: langFence helper (15), costruzione widget (12), kLangs unicità (10) — 2026-06-03
- [x] **test_file_parser** — 38 PASS: costruzione+tab (16), pdftotext (5), CSV (8), file non validi (9) — 2026-06-03
- [x] **test_repl_python** — 23 PASS, 1 SKIP (race condition doppio start nota): costruzione (6), python3 check (4), I/O asincrono (5), robustezza (8) — 2026-06-03

### 🟠 Bot locali — App Controller (tab già presenti, da implementare)

- [x] **Telegram Bot locale** — `app_controller_page.cpp::buildTelegramTab()`: python-telegram-bot subprocess, IPC JSON stdin/stdout, whitelist ID, risposte AI locale, pulsanti Avvia/Ferma, log in-app — già implementato 2026-06-02
  - Token bot da @BotFather configurabile in Impostazioni
  - Messaggi in entrata → AI locale risponde
  - Comandi: `/ask`, `/status`, `/task`, `/stop`
  - Notifiche proattive (task WAN completato, alert sistema)
  - Whitelist ID Telegram (sicurezza)
  - Stack: python-telegram-bot o Telethon via MCP (`github.com/chigwell/telegram-mcp`)

- [x] **WhatsApp Bot locale** — sezione "Bot AI Rispondente" aggiunta a `buildWhatsAppTab()`: whitelist numeri, checkbox auto-risposta, Avvia/Ferma, polling bridge ogni 2s, risposta AI locale via `m_ai->chat()`, log messaggi — 2026-06-03
  - Bridge locale via Baileys/whatsapp-web.js (no API ufficiali)
  - QR code autenticazione WhatsApp Web integrato nel tab
  - Messaggi in entrata → prompt AI → risposta automatica
  - Comandi: `!ask`, `!status`, `!immagine`
  - Whitelist numeri autorizzati
  - Nessun account Business richiesto
  - Stack: MCP whatsapp-mcp (`github.com/lharries/whatsapp-mcp`)

### 🟢 Nice-to-have — feature specializzate

- [x] **test_astrale** — 31 PASS: RicercaPage (12), NatalChartWidget (10), AstroCalc::compute() (9) — 2026-06-03
- [x] **test_blhm_rab0l** — 38 PASS: Rab0lCanvas (12), BLHM Engine C (14), UI BLHM/RAB₀-L (12) — 2026-06-03
- [x] **test_gns3_mcp** — 18 PASS, 2 SKIP (GNS3 non avviato): costruzione (8), server mock (2 skip), azioni/combo (8) — 2026-06-03
- [x] **test_multi_agente_live** — 13 test: CAT-A costruzione (5), CAT-B decomposizione JSON (2, Ollama), CAT-C esecuzione SubTask (3, Ollama), CAT-D GraphMemory persistenza (4, Ollama) — TIMEOUT 300s — 2026-06-03

---

## 🧪 Test RAG — Domande di conversione studenti

> Verificare che il sistema RAG risponda correttamente a domande tipiche di studenti
> su conversioni in chimica, fisica, elettronica e matematica.
> Testare sia tramite chat AI (tab 🤖) sia tramite il Grafo RAG (tab 🔬→🕸️).
> Per ogni domanda: verificare risposta corretta + unità di misura + formula mostrata.

---

### ⚗️ Chimica

- [x] **Mol ↔ grammi** — "Quanti grammi sono 2 mol di NaCl?" (atteso: 116,88 g; M = 58,44 g/mol)
- [x] **Mol ↔ grammi poliatomici** — "Quanti grammi sono 0,5 mol di H₂SO₄?" (atteso: 49,04 g)
- [x] **Concentrazione molare** — "Ho 5 g di NaOH in 500 mL di soluzione, qual è la molarità?" (atteso: 0,25 mol/L)
- [x] **pH → concentrazione** — "A pH = 3 qual è la concentrazione di H⁺?" (atteso: 10⁻³ mol/L = 0,001 mol/L)
- [x] **Concentrazione → pH** — "Con [H⁺] = 5×10⁻⁴ mol/L, qual è il pH?" (atteso: ≈ 3,30)
- [x] **Pressione atm ↔ Pa** — "Converti 2,5 atm in Pascal" (atteso: 253 312,5 Pa)
- [x] **Pressione mmHg ↔ kPa** — "760 mmHg quanti kPa sono?" (atteso: 101,325 kPa)
- [x] **Temperatura K ↔ °C** — "Converti 298 K in gradi Celsius" (atteso: 24,85 °C)
- [x] **Temperatura °C ↔ °F** — "37 °C quanti gradi Fahrenheit sono?" (atteso: 98,6 °F)
- [x] **Energia kJ/mol ↔ eV** — "Converti 96,5 kJ/mol in eV per molecola" (atteso: ≈ 1 eV)
- [x] **Legge dei gas ideali** — "2 mol di gas a 300 K e 1 atm: qual è il volume?" (atteso: ≈ 49,2 L; PV=nRT)
- [x] **Numero di Avogadro** — "Quante molecole ci sono in 18 g di H₂O?" (atteso: 6,022×10²³)

---

### ⚡ Fisica

- [x] **Energia J ↔ cal** — "Converti 4186 J in calorie" (atteso: 1000 cal = 1 kcal)
- [x] **Energia J ↔ eV** — "1 eV quanti Joule sono?" (atteso: 1,602×10⁻¹⁹ J)
- [x] **Energia kWh ↔ J** — "1 kWh quanti Joule sono?" (atteso: 3,6×10⁶ J)
- [x] **Potenza W ↔ CV** — "Un motore da 75 CV quanti Watt sviluppa?" (atteso: 55 125 W ≈ 55,1 kW)
- [x] **Forza N ↔ kgf** — "Converti 9,81 N in kgf" (atteso: 1 kgf)
- [x] **Velocità m/s ↔ km/h** — "100 km/h quanti m/s sono?" (atteso: 27,78 m/s)
- [x] **Velocità km/h ↔ mph** — "120 km/h quanti mph sono?" (atteso: ≈ 74,56 mph)
- [x] **Pressione Pa ↔ bar** — "1 bar quanti Pascal sono?" (atteso: 100 000 Pa)
- [x] **Lunghezza d'onda ↔ frequenza** — "Luce con λ = 500 nm: qual è la frequenza?" (atteso: 6×10¹⁴ Hz; f = c/λ)
- [x] **Distanza UA ↔ km** — "1 UA quanti km sono?" (atteso: 149 597 870,7 km)
- [x] **Anno luce ↔ km** — "1 anno luce quanti km sono?" (atteso: ≈ 9,461×10¹² km)
- [x] **Massa u ↔ kg** — "1 unità di massa atomica quanti kg è?" (atteso: 1,661×10⁻²⁷ kg)

---

### 🔌 Elettronica

- [x] **Ohm ↔ kΩ ↔ MΩ** — "Converti 4700 Ω in kΩ e in MΩ" (atteso: 4,7 kΩ; 0,0047 MΩ)
- [x] **Frequenza ↔ periodo** — "Un segnale a 50 Hz: qual è il periodo?" (atteso: T = 1/f = 20 ms)
- [x] **Frequenza Hz ↔ kHz ↔ MHz** — "2,4 GHz quanti Hz sono?" (atteso: 2 400 000 000 Hz)
- [x] **Capacità F ↔ µF ↔ nF ↔ pF** — "Converti 0,000 001 F in µF e nF" (atteso: 1 µF = 1000 nF)
- [x] **Induttanza H ↔ mH ↔ µH** — "470 µH quanti mH sono?" (atteso: 0,47 mH)
- [x] **dBm ↔ mW** — "30 dBm quanti mW sono?" (atteso: 1000 mW = 1 W; P = 10^(dBm/10) mW)
- [x] **dB tensione** — "Un guadagno di 20 dB: quanto è il rapporto di tensione?" (atteso: V_out/V_in = 10)
- [x] **Legge di Ohm** — "Con V = 12 V e R = 470 Ω: qual è la corrente?" (atteso: I ≈ 25,5 mA)
- [x] **Costante di tempo RC** — "R = 10 kΩ, C = 100 µF: qual è τ?" (atteso: τ = RC = 1 s)
- [x] **Frequenza di taglio RC** — "R = 1 kΩ, C = 100 nF: qual è fc?" (atteso: fc = 1/(2πRC) ≈ 1591 Hz)
- [x] **Potenza dissipata** — "R = 100 Ω con I = 50 mA: quanti mW dissipa?" (atteso: P = I²R = 250 mW)
- [x] **mA ↔ µA** — "Converti 0,025 A in mA e µA" (atteso: 25 mA = 25 000 µA)

---

### π Matematica

- [x] **Gradi ↔ radianti** — "Converti 180° in radianti" (atteso: π rad ≈ 3,14159)
- [x] **Radianti ↔ gradi** — "π/4 radianti quanti gradi sono?" (atteso: 45°)
- [x] **Gradianti ↔ gradi** — "Converti 100 gradianti in gradi" (atteso: 90°)
- [x] **Logaritmo naturale ↔ log₁₀** — "ln(100) quanto vale?" (atteso: ≈ 4,6052; log₁₀(100) = 2)
- [x] **Notazione scientifica** — "Scrivi 0,000 045 7 in notazione scientifica" (atteso: 4,57×10⁻⁵)
- [x] **Binario ↔ decimale** — "Converti 1011 0110 in decimale" (atteso: 182)
- [x] **Decimale ↔ esadecimale** — "255 in esadecimale" (atteso: FF)
- [x] **Esadecimale ↔ binario** — "0xA3 in binario" (atteso: 1010 0011)
- [x] **Area cerchio** — "Cerchio con r = 5 cm: qual è l'area?" (atteso: A = π·r² ≈ 78,54 cm²)
- [x] **Volume sfera** — "Sfera con r = 3 m: qual è il volume?" (atteso: V = (4/3)πr³ ≈ 113,1 m³)
- [x] **Conversione base 2 ↔ 8** — "Converti 11001010 in ottale" (atteso: 312₈)
- [x] **Percentuale ↔ frazione ↔ decimale** — "37,5% come frazione e come decimale" (atteso: 3/8 = 0,375)

---

## ✅ Implementati

### Sessione 2026-05-29 — TODO completati

- [x] **FEAT-1 parallelo** — Pool di 3 `AiClient` (`kMaxParallel = 3`)
  in `AgentiMultiPage`. `runNextPendingTask()` avvia TUTTI i task con
  dipendenze soddisfatte contemporaneamente (fino a 3). `initPool()` sinc
  i client col modello corrente ad ogni decomposizione.

- [x] **Cross-pollination agenti→grafo RAG** — `AgentiMultiPage::setExtRagMemory()`
  riceve la `GraphMemory` di RagGraph; `onTaskResultDone()` scrive ogni
  risultato come nodo `"fact"` anche nel grafo RAG. Collegato in
  `MainWindow::buildMultiAgentTab()`.

- [x] **Auto-trigger RagGraph** — `RicercaPage::onAutoRagTrigger()` collega
  `ImpostazioniPage::indexingFinished` → avvio automatico del RagGraph
  dopo 800 ms (delay per flush FS). Cablato in `ensureSettingsDialog()`.

### Sessione 2026-05-29 — FEAT-1 Multi-Agente + FEAT-2 Grafo RAG

- [x] **GraphMemory** (`gui/graph_memory.h/cpp`) — fondazione comune:
  SQLite-backed, nodi+archi tipizzati, BFS neighbours, ricerca testuale,
  export DOT/JSON/TXT, pruneByImportance, segnale `changed()` real-time

- [x] **FEAT-1 Multi-Agente** (`gui/pages/agenti_multi_page.h/cpp`) — tab [9] 🕸️:
  MasterAgent decompone JSON → SubTask con `depends_on`,
  sub-agenti sequenziali con contesto condiviso, sintesi finale,
  GraphMemory live (nodi, archi, export TXT, clear), viste Grafo DOT / JSON

- [x] **FEAT-2 RagGraph** (`gui/rag_graph.h/cpp`) — estrazione LLM:
  scansiona `~/prismalux_rag_docs/` e `Prismalux/RAG/`, estrae entità+relazioni
  via LLM (JSON), persiste in GraphMemory separata (`~/.prismalux/rag_graph.db`)

- [x] **FEAT-2 Tab 🕸️ Grafo RAG** in RicercaPage:
  lista nodi filtrabile, click→dettaglio+vicini, Graphviz PNG dark-theme,
  DOT source, progresso file per file, stop, svuota

### Sessione 2026-05-29 — Voce + Donazione

- [x] **TTS + STT web** — tab "🎙️ Voce" (ex Whisper):
  SpeechSynthesis (voce/velocità configurabili), MediaRecorder→/api/whisper,
  Invia in Chat, barra livello microfono

- [x] **TTS Android** — box "🔊 Sintesi Vocale" in AudioPage:
  QTextToSpeech, pulsante Parla/Stop, voce italiana

- [x] **Fix STT Android** — rimosso `m_ai->transcribeAudio()` inesistente,
  ora usa `uploadWhisper()` direttamente

- [x] **Donazione PayPal** — README badge + sezione, `.github/FUNDING.yml`,
  pulsante in Impostazioni→Ringraziamenti e APK Android Info page

---

## 💡 IDEA — Git come "Sistema di Memoria Versionata" per LLM
> [12/06/26 00:51] Paolo

Sistema a tre componenti C++ standalone (`~/.ai-memory/`):

### 1. AIMemory — Repo Git locale
```
~/.ai-memory/
├── profile/preferences.yaml      # risposta_length, interface, os…
├── interactions/YYYY-MM/DD-feedback.yaml  # 👍👎 per query+risposta
└── .git/                          # storia completa versionata
```
Ogni apprendimento → `git commit -m "learn: chiave = valore"` con metadati (confidence, trigger).
Feedback negativo → `git commit -m "feedback: 👎"` + motivazione.
Analisi pattern: `git log --grep="rating: 👍"` per vedere cosa funziona.
Revert errori: `git checkout <hash> -- profile/preferences.yaml`.

### 2. SmartRouter — Routing automatico LOCALE/CLOUD
Regole in ordine di priorità:
1. Offline rilevato → LOCALE (sempre)
2. Ollama non attivo → CLOUD
3. Dati sensibili (`password`, `iban`, `codice fiscale`…) → LOCALE (privacy)
4. Query > 1500 char → CLOUD (contesto ampio)
5. Keyword ragionamento complesso (`analizza profondamente`, `dimostra che`…) → CLOUD
6. Default → LOCALE (veloce, gratis, privacy)

### 3. AIOrchestrator — Binario completo `./ai-assistant`
Flusso: `getRelevantContext(query)` → `decideRoute()` → `callLocal()|callCloud()` → mostra risposta → chiede 👍/👎 → `logFeedback()` → git commit automatico.
Diventa più intelligente ad ogni interazione.

Compila con: `g++ -std=c++17 -O2 ai_assistant.cpp -o ai_assistant`
Dipendenze zero (usa `popen(curl …)` per HTTP).

- [x] **Valutare integrazione in Prismalux** — IMPLEMENTATO: `MCPs/ai_memory_mcp/server.py`
  (stdlib only, zero dep pip). 5 tool: `learn`, `feedback`, `get_context`, `save_interaction`,
  `git_log`. Backend: `~/.ai-memory/` (stesso formato di `gui/ai_memory.h`).
  Smoke test: initialize + tools/list → OK. Il DevAgent può chiamarlo a fine sessione.

---

### Sessioni precedenti

- [x] LaTeX KaTeX rendering (Analisi 1/2, output AI) + LatexView widget
- [x] Randomizer 🔀 52 formule Risolvi Passi
- [x] Test SymPy CAT-E (15 test, 62/62 PASS)
- [x] Scheda TFR con C.F. automatico (D.M. 1976 + Belfiore)
- [x] WAN Calcolo Distribuito (TCP:11600, 28 task, cron)
- [x] Ollama MCP (18° plugin, cache SQLite, 5 tool)
- [x] Quiz CCNA 209 domande (15 temi)
- [x] DPI dpiScale(), i18n QTranslator, supply chain hash

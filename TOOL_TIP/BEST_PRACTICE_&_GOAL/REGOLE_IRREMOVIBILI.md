═══════════════════════════════════════════════════════════════════
  PRISMALUX — REGOLE IRREMOVIBILI DEL SOFTWARE
  Queste convenzioni NON si toccano. Mai. Per nessun motivo.
  Aggiornato: 2026-05-30
═══════════════════════════════════════════════════════════════════

────────────────────────────────────────────────
1. LAMBDA NELLE connect() — Regola assoluta
────────────────────────────────────────────────
Il context object (4° argomento) è SEMPRE obbligatorio.
Tutti i puntatori catturati devono essere figli del context object.

  CORRETTO:
    connect(btn, &QPushButton::clicked, this,
            [this]{ m_stack->setCurrentIndex(1); });

  CORRETTO (con context = bar):
    connect(m_ai, &AiClient::modelsReady, bar,
            [=](const QStringList& l){ cmb->clear(); });

  VIETATO — nessun context → use-after-free:
    connect(reply, &QNetworkReply::finished, [this, reply]{ ... });

  VIETATO — static QMetaObject::Connection → zombie condiviso:
    static QMetaObject::Connection c;

  Logica > 2 righe o accesso a più variabili membro → slot nominato.
  Pattern one-shot: QMetaObject::Connection come membro, disconnect esplicito.

────────────────────────────────────────────────
2. PATH — Mai hardcode
────────────────────────────────────────────────
Usare SEMPRE le costanti di prismalux_paths.h:

  #include "prismalux_paths.h"
  namespace P = PrismaluxPaths;

  P::root()              — cartella del progetto
  P::kOllamaPort         — 11434
  P::kLlamaServerPort    — 8081
  P::kOpenCodePort       — 8092
  P::kWanComputePort     — 11600
  P::modelIcon(sz, name) — emoji icona modello
  P::feedbackPath()
  P::userKnowledgePath()

  VIETATO: "/home/wildlux/...", "11434", "11600" literali nel codice.

────────────────────────────────────────────────
3. DPI — Usare dpiScale() per TUTTE le dimensioni strutturali
────────────────────────────────────────────────
  #include "dpi_utils.h"
  setFixedWidth(dpiScale(80));   // non setFixedWidth(80)
  setMinimumHeight(dpiScale(56));

  dpiScale() è no-op a 96dpi, scala automaticamente su HiDPI/Wayland 2×.
  Usare per: width, height, spacing, margins strutturali, font-size critici.
  Non usare per valori già relativi (es. "100%", stretch factor).

────────────────────────────────────────────────
4. THEMEMANAGER — Mai istanziare con qApp
────────────────────────────────────────────────
  CORRETTO:   static ThemeManager inst(nullptr);
  VIETATO:    static ThemeManager inst(qApp);   // → ABRT al shutdown

────────────────────────────────────────────────
5. LANSERVER SHUTDOWN — blockSignals prima di stop()
────────────────────────────────────────────────
  m_lanServer->blockSignals(true);
  m_lanServer->stop();

  Senza blockSignals → SIGSEGV perché stop() emette segnali
  verso widget già distrutti.

────────────────────────────────────────────────
6. FETCHMODELS — Connettere SEMPRE anche il segnale error
────────────────────────────────────────────────
  auto* holder = new QObject(this);
  connect(m_ai, &AiClient::modelsReady, holder,
          [this, holder](const QStringList& l) {
              holder->deleteLater();
              fillCombo(l);
          });
  connect(m_ai, &AiClient::error, holder,
          [this, holder](const QString&) {
              holder->deleteLater();
              setStatus("Backend non raggiungibile.");
          });
  m_ai->fetchModels();

  Senza error handler: il combo resta silenziosamente vuoto.

────────────────────────────────────────────────
7. BACKEND — Sempre da m_ai->backend()
────────────────────────────────────────────────
  // Corretto:
  m_ai->setBackend(m_ai->backend(), host, port, model);

  // Vietato:
  m_ai->setBackend(AiClient::Ollama, ...);  // hardcode enum

  Il backend può cambiare a runtime; leggere sempre il valore corrente.

────────────────────────────────────────────────
8. EMOJI IN STRINGHE C++ — Split quando segue cifra hex
────────────────────────────────────────────────
  // Il char dopo \xNN NON deve essere una cifra hex (0-9, A-F, a-f)
  // altrimenti il compilatore legge N+1 cifre → out of range.

  CORRETTO:   "\xe2\x80\x9c" "Testo"   (split literal)
  VIETATO:    "\xe2\x80\x9cTesto"      (C è hex → warning/UB)

  ASCII puro nei const char* di descrizione (enum, tooltip tecnici):
  evita pattern \xNN seguito da 0-9/a-f.

────────────────────────────────────────────────
9. COMBO MODELLO — Sempre con UserRole
────────────────────────────────────────────────
  combo->addItem(P::modelIcon(sz, m) + m, m);   // data = nome modello
  QString mod = combo->currentData(Qt::UserRole).toString();

  Non usare currentText() per ricavare il nome modello (l'emoji lo sporca).

────────────────────────────────────────────────
10. AI CLIENT — API completa
────────────────────────────────────────────────
  m_ai->chat(sys, msg);
  m_ai->chat(sys, msg, historyJson, QueryType);
  m_ai->abort();                  // → aborted() — NON chiama onFinished()
  m_ai->fetchModels();            // → modelsReady(QStringList)
  m_ai->fetchEmbedding(t);        // → embeddingReady(vec) | embeddingError(msg)
  m_ai->setNumGpu(n);             // n<0 = Ollama auto
  m_ai->fetchModelLayers(cb);     // → cb(int layers)
  m_ai->unloadModel();            // keep_alive=0

  abort() non emette finished() → non usare holder che attende finished
  dopo un abort senza prima disconnettere.

────────────────────────────────────────────────
11. NUM_GPU — Usare fetchModelLayers prima di GPU/Misto
────────────────────────────────────────────────
  Ollama NON clipa num_gpu → passare un valore > layers → ISE 500.
  Sempre:
    m_ai->fetchModelLayers([this](int layers){
        m_ai->setNumGpu(std::min(layers, capacity));
    });

────────────────────────────────────────────────
12. NUOVI FILE CPP — Aggiungere in CMakeLists.txt
────────────────────────────────────────────────
  Ogni nuovo .cpp va in gui/CMakeLists.txt nella lista CPP_SRCS.
  Mobile:   ANDROID/QT_ANDROID_Version/android_app/CMakeLists.txt
  Test:     gui/CMakeLists.txt nella sezione BUILD_TESTS

────────────────────────────────────────────────
13. GRAPHMEMORY — Connessione DB univoca per istanza
────────────────────────────────────────────────
  DB FEAT-1: ~/.prismalux/graph_memory.db
  DB FEAT-2: ~/.prismalux/rag_graph.db

  Non condividere la stessa connessione SQLite tra istanze diverse.
  Ogni GraphMemory usa il proprio m_connName univoco (UUID-based).

────────────────────────────────────────────────
14. COMMIT — Solo su richiesta esplicita
────────────────────────────────────────────────
  Non fare commit automatici. Non fare push senza conferma.
  Non usare --no-verify, --force, --no-gpg-sign senza richiesta.

────────────────────────────────────────────────
15. SICUREZZA — Validazione solo ai boundary
────────────────────────────────────────────────
  Validare input SOLO ai boundary del sistema (input utente, API esterne).
  Non aggiungere validation difensiva su codice interno già garantito.
  Rate limiting chat: già implementato in LanServer (HAVE_RATE_LIMIT).
  Token LAN: timing-safe comparison, salvato in keychain (HAVE_QKEYCHAIN).

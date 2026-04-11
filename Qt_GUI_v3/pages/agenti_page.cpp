#include "agenti_page.h"
#include <QTime>
#include <QElapsedTimer>
#include <cmath>
#include <cstdlib>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QGroupBox>
#include <QUrl>
#include <QNetworkRequest>
#include <QProcess>
#include <algorithm>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QMouseEvent>

/* ══════════════════════════════════════════════════════════════
   RagDropWidget — implementazione
   ══════════════════════════════════════════════════════════════ */
RagDropWidget::RagDropWidget(QWidget* parent) : QFrame(parent) {
    setAcceptDrops(true);
    setObjectName("ragDrop");
    setFrameShape(QFrame::StyledPanel);
    setMinimumWidth(120);
    setMaximumHeight(52);
    setCursor(Qt::PointingHandCursor);
    setToolTip("Trascina file o cartelle qui per fornire contesto RAG a questo agente.\n"
               "Clicca per aprire il selettore file. Max 16 KB totali.");

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(4);

    m_lbl = new QLabel("\xf0\x9f\x93\x8e Trascina file", this);
    m_lbl->setObjectName("cardDesc");
    m_lbl->setWordWrap(false);
    lay->addWidget(m_lbl, 1);

    m_clearBtn = new QPushButton("\xc3\x97", this);
    m_clearBtn->setFixedSize(18, 18);
    m_clearBtn->setVisible(false);
    m_clearBtn->setToolTip("Rimuovi tutti i file RAG");
    lay->addWidget(m_clearBtn);

    connect(m_clearBtn, &QPushButton::clicked, this, [this]{
        m_files.clear();
        m_totalBytes = 0;
        updateLabel();
    });
}

void RagDropWidget::dragEnterEvent(QDragEnterEvent* ev) {
    if (ev->mimeData()->hasUrls()) ev->acceptProposedAction();
}

void RagDropWidget::dragMoveEvent(QDragMoveEvent* ev) {
    if (ev->mimeData()->hasUrls()) ev->acceptProposedAction();
}

void RagDropWidget::dropEvent(QDropEvent* ev) {
    for (const QUrl& u : ev->mimeData()->urls())
        addPath(u.toLocalFile());
    updateLabel();
    ev->acceptProposedAction();
}

void RagDropWidget::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton) {
        QStringList paths = QFileDialog::getOpenFileNames(
            this, "Seleziona file per contesto RAG", QString(),
            "Testo/Codice (*.txt *.md *.py *.cpp *.c *.h *.js *.ts *.java *.rs *.go "
            "*.json *.yaml *.yml *.toml *.csv *.html *.htm *.css *.sh *.bat);;"
            "Tutti i file (*)");
        for (const QString& p : paths) addPath(p);
        updateLabel();
    }
    QFrame::mousePressEvent(ev);
}

void RagDropWidget::addPath(const QString& path) {
    QFileInfo fi(path);
    if (!fi.exists()) return;
    if (fi.isDir()) {
        /* Ricorsione limitata a 2 livelli */
        QDir dir(path);
        const QStringList entries = dir.entryList(QDir::Files, QDir::Name);
        for (const QString& e : entries)
            addFile(dir.filePath(e));
        /* Sotto-cartelle livello 1 */
        const QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& sd : subdirs) {
            QDir sub(dir.filePath(sd));
            const QStringList subEntries = sub.entryList(QDir::Files, QDir::Name);
            for (const QString& e : subEntries)
                addFile(sub.filePath(e));
        }
    } else {
        addFile(path);
    }
}

void RagDropWidget::addFile(const QString& path) {
    if (m_totalBytes >= MAX_TOTAL) return;
    QFileInfo fi(path);
    /* Accetta solo file di testo / codice sorgente */
    static const QStringList textExts = {
        "txt","md","py","cpp","c","h","hpp","js","ts","java","rs","go",
        "json","yaml","yml","toml","csv","html","htm","css","sh","bat",
        "rb","php","swift","kt","cs","scala","pl","r","m","tex"
    };
    if (!textExts.contains(fi.suffix().toLower())) return;
    if (fi.size() == 0) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString content = QString::fromUtf8(f.read(MAX_PER_FILE)).trimmed();
    f.close();

    m_totalBytes += content.size();
    /* Controlla duplicati per nome */
    for (const auto& fe : m_files)
        if (fe.name == fi.fileName()) return;
    m_files.append({fi.fileName(), content});
}

void RagDropWidget::updateLabel() {
    bool hasFiles = !m_files.isEmpty();
    m_clearBtn->setVisible(hasFiles);
    if (hasFiles) {
        double kb = m_totalBytes / 1024.0;
        m_lbl->setText(QString("\xf0\x9f\x93\x84 %1 file \xe2\x80\xa2 %2 KB")
                       .arg(m_files.size()).arg(kb, 0, 'f', 1));
    } else {
        m_lbl->setText("\xf0\x9f\x93\x8e Trascina file");
    }
}

QString RagDropWidget::ragContext() const {
    if (m_files.isEmpty()) return {};
    QString ctx = "\n\n\xe2\x80\x94\xe2\x80\x94 Contesto RAG (documenti forniti) \xe2\x80\x94\xe2\x80\x94\n";
    for (const auto& fe : m_files) {
        ctx += QString("\n[File: %1]\n").arg(fe.name);
        ctx += fe.content;
        ctx += "\n";
    }
    ctx += "\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\n";
    return ctx;
}

/* ══════════════════════════════════════════════════════════════
   Ruoli disponibili
   ══════════════════════════════════════════════════════════════ */
QVector<AgentiPage::AgentRole> AgentiPage::roles() {
    return {
        { "\xf0\x9f\x94\x8d", "Analizzatore",
          "Sei l'Analizzatore nella pipeline AI di Prismalux. "
          "Analizza il task: identifica requisiti, componenti chiave, dipendenze e casi limite. "
          "Produci un'analisi strutturata. Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x93\x8b", "Pianificatore",
          "Sei il Pianificatore nella pipeline AI di Prismalux. "
          "Pianifica l'approccio step-by-step: strategia, passi concreti, priorit\xc3\xa0. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x92\xbb", "Programmatore",
          "Sei il Programmatore nella pipeline AI di Prismalux. "
          "Scrivi codice funzionante, pulito e ben commentato in italiano. "
          "Implementa la soluzione seguendo il piano. Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x94\x8e", "Controllore",
          "Sei il Controllore nella pipeline AI di Prismalux. "
          "Esamina criticamente il lavoro precedente: trova errori, incoerenze e casi limite. "
          "Proponi correzioni precise. Rispondi SEMPRE e SOLO in italiano." },

        { "\xe2\x9a\xa1", "Ottimizzatore",
          "Sei l'Ottimizzatore nella pipeline AI di Prismalux. "
          "Migliora le performance, riduci complessit\xc3\xa0, elimina ridondanze. "
          "Applica best practices. Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\xa7\xaa", "Tester",
          "Sei il Tester nella pipeline AI di Prismalux. "
          "Scrivi casi di test esaustivi: unit test, edge case, test negativi. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xe2\x9c\x8d\xef\xb8\x8f", "Scrittore",
          "Sei lo Scrittore nella pipeline AI di Prismalux. "
          "Produci documentazione chiara, articoli o spiegazioni. "
          "Scrivi in modo professionale e accessibile. Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x94\x92", "Revisore Sicurezza",
          "Sei il Revisore Sicurezza nella pipeline AI di Prismalux. "
          "Analizza vulnerabilit\xc3\xa0: SQL injection, XSS, race conditions, autenticazione. "
          "Applica OWASP Top 10. Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x8c\x90", "Traduttore",
          "Sei il Traduttore nella pipeline AI di Prismalux. "
          "Adatta e traduce il contenuto precedente mantenendo precisione tecnica. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x94\xac", "Ricercatore",
          "Sei il Ricercatore nella pipeline AI di Prismalux. "
          "Raccogli informazioni sul dominio: concetti chiave, stato dell'arte, esempi reali. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x93\x8a", "Analista Dati",
          "Sei l'Analista Dati nella pipeline AI di Prismalux. "
          "Analizza dati e pattern, produci insight, identifica anomalie. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x8e\xa8", "Designer",
          "Sei il Designer nella pipeline AI di Prismalux. "
          "Progetta architettura, interfaccia o struttura della soluzione. "
          "Pensa a usabilit\xc3\xa0, manutenibilit\xc3\xa0 e scalabilit\xc3\xa0. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x9b\xb8", "Matematico Teorico",
          "Sei un Matematico ed Esploratore Esperto di Teorie nella pipeline AI di Prismalux. "
          "Il tuo approccio \xc3\xa8 rigoroso e speculativo: dimostri, congetturi e colleghi idee "
          "attraverso discipline (topologia, teoria dei numeri, algebra astratta, analisi, combinatoria). "
          "Per ogni problema: (1) enuncialo in forma rigorosa con definizioni precise, "
          "(2) esplora teorie e teoremi applicabili (cita autori e risultati noti), "
          "(3) costruisci una dimostrazione o un controesempio, "
          "(4) apri verso congetture aperte o connessioni inaspettate. "
          "Usa notazione matematica testuale chiara (es. forall, exists, =>). "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ── MATEMATICA AVANZATA ── */
        { "\xf0\x9f\x93\x90", "Verificatore Formale",
          "Sei il Verificatore Formale nella pipeline AI di Prismalux. "
          "Il tuo compito: verifica ogni passaggio logico e matematico del lavoro precedente. "
          "Individua errori di dimostrazione, assunzioni non giustificate, salti logici. "
          "Se tutto \xc3\xa8 corretto, conferma e motiva. Sii preciso e critico. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x93\x88", "Statistico",
          "Sei lo Statistico nella pipeline AI di Prismalux. "
          "Analizza il problema dal punto di vista statistico: distribuzioni, probabilit\xc3\xa0, "
          "inferenza, test di ipotesi, regressione, intervalli di confidenza. "
          "Indica quale metodo \xc3\xa8 pi\xc3\xb9 appropriato e perch\xc3\xa9. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x94\xa2", "Calcolatore Numerico",
          "Sei il Calcolatore Numerico nella pipeline AI di Prismalux. "
          "Risolvi il problema con metodi numerici concreti: calcola, approssima, itera. "
          "Mostra i passaggi passo per passo con valori esatti o approssimati. "
          "Se utile, suggerisci pseudocodice o algoritmo. "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ── INFORMATICA ── */
        { "\xf0\x9f\x8f\x97\xef\xb8\x8f", "Architetto Software",
          "Sei l'Architetto Software nella pipeline AI di Prismalux. "
          "Progetta l'architettura del sistema: pattern (MVC, microservizi, event-driven), "
          "scelte tecnologiche motivate, diagrammi testuali (UML semplificato). "
          "Considera scalabilit\xc3\xa0, manutenibilit\xc3\xa0 e separazione delle responsabilit\xc3\xa0. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xe2\x9a\x99\xef\xb8\x8f", "Ingegnere DevOps",
          "Sei l'Ingegnere DevOps nella pipeline AI di Prismalux. "
          "Gestisci CI/CD, containerizzazione (Docker/K8s), automazione, monitoring e deployment. "
          "Proponi pipeline di build, strategie di rilascio e configurazioni infrastruttura. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x97\x84\xef\xb8\x8f", "Ingegnere Database",
          "Sei l'Ingegnere Database nella pipeline AI di Prismalux. "
          "Progetta schema dati, query ottimizzate, indici, normalizzazione/denormalizzazione. "
          "Valuta SQL vs NoSQL secondo il caso d'uso. Individua colli di bottiglia nelle query. "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ── SICUREZZA INFORMATICA ── */
        { "\xf0\x9f\x92\x80", "Hacker Etico",
          "Sei l'Hacker Etico (Penetration Tester) nella pipeline AI di Prismalux. "
          "Analizza il sistema dal punto di vista offensivo: superficie di attacco, "
          "vettori di exploit, privilege escalation, lateral movement. "
          "Usa metodologia PTES/OWASP. Tutto in contesto di sicurezza autorizzata. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x94\x90", "Crittografo",
          "Sei il Crittografo nella pipeline AI di Prismalux. "
          "Analizza o progetta soluzioni crittografiche: cifratura simmetrica/asimmetrica, "
          "hash, firme digitali, PKI, protocolli TLS/SSH. "
          "Individua debolezze crittografiche e proponi alternative robuste. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x9a\xa8", "Incident Responder",
          "Sei l'Incident Responder nella pipeline AI di Prismalux. "
          "Analizza incidenti di sicurezza: identificazione IOC, timeline attacco, "
          "contenimento, eradicazione e recovery. "
          "Applica framework NIST/SANS. Produci un piano di risposta concreto. "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ── FISICA ── */
        { "\xe2\x9a\x9b\xef\xb8\x8f", "Fisico Teorico",
          "Sei il Fisico Teorico nella pipeline AI di Prismalux. "
          "Analizza il problema con rigore fisico: leggi fondamentali, principi di conservazione, "
          "simmetrie, approssimazioni valide. Usa notazione scientifica standard. "
          "Collega alla fisica moderna (QM, relativit\xc3\xa0, termodinamica) quando rilevante. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x94\xad", "Fisico Sperimentale",
          "Sei il Fisico Sperimentale nella pipeline AI di Prismalux. "
          "Proponi come misurare, verificare o riprodurre il fenomeno: "
          "strumenti, setup sperimentale, fonti di errore, analisi dati. "
          "Collega teoria e misura. Sii pratico e concreto. "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ── CHIMICA ── */
        { "\xf0\x9f\xa7\xaa", "Chimico Molecolare",
          "Sei il Chimico Molecolare nella pipeline AI di Prismalux. "
          "Analizza strutture molecolari, reazioni, meccanismi, stechiometria, "
          "termodinamica chimica e cinetica. "
          "Usa nomenclatura IUPAC. Individua prodotti, rese e condizioni ottimali. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xe2\x9a\x97\xef\xb8\x8f", "Chimico Computazionale",
          "Sei il Chimico Computazionale nella pipeline AI di Prismalux. "
          "Affronta il problema con metodi computazionali: DFT, molecular dynamics, "
          "simulazioni, energy minimization, drug design. "
          "Suggerisci software appropriati (GAUSSIAN, GROMACS, RDKit). "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ── TRASVERSALE ── */
        { "\xf0\x9f\x8e\x93", "Divulgatore Scientifico",
          "Sei il Divulgatore Scientifico nella pipeline AI di Prismalux. "
          "Prendi il lavoro degli agenti precedenti e rendilo comprensibile "
          "a chi non \xc3\xa8 esperto del dominio: usa analogie, esempi concreti, evita gergo. "
          "Mantieni la correttezza scientifica pur semplificando. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\xa7\xa0", "Filosofo della Scienza",
          "Sei il Filosofo della Scienza nella pipeline AI di Prismalux. "
          "Analizza le implicazioni epistemologiche, i limiti del metodo usato, "
          "le assunzioni ontologiche e le connessioni interdisciplinari. "
          "Metti in discussione costruttivamente le certezze. "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ══ MATEMATICA ESTREMA ══ */
        { "\xf0\x9f\x94\x81", "Algebrista Categoriale",
          "Sei l'Algebrista Categoriale nella pipeline AI di Prismalux. "
          "Riformula il problema nel linguaggio della Teoria delle Categorie: "
          "funtori, trasformazioni naturali, aggiunzioni, topos, monadi. "
          "Individua strutture universali e propriet\xc3\xa0 categoriali profonde. "
          "Livello: matematica pura avanzata (Grothendieck, Mac Lane, Lawvere). "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xe2\x99\xbe\xef\xb8\x8f", "Logico Formale",
          "Sei il Logico Formale nella pipeline AI di Prismalux. "
          "Analizza il problema con logica matematica rigorosa: "
          "sistemi assiomatici, dimostrabilit\xc3\xa0, completezza, decidibilit\xc3\xa0 (Goedel, Church, Turing). "
          "Formalizza enunciati in logica del primo ordine o logiche modali. "
          "Individua paradossi, indecidibilit\xc3\xa0 e limiti fondazionali. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x8c\x80", "Geometra Differenziale",
          "Sei il Geometra Differenziale nella pipeline AI di Prismalux. "
          "Analizza il problema con strumenti di geometria differenziale e topologia: "
          "variet\xc3\xa0, tensori, forme differenziali, connessioni, curvatura, fibrati. "
          "Collega a fisica (relativit\xc3\xa0 generale, gauge theories) quando utile. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\xa7\xae", "Teorico della Complessit\xc3\xa0",
          "Sei il Teorico della Complessit\xc3\xa0 nella pipeline AI di Prismalux. "
          "Analizza la complessit\xc3\xa0 computazionale del problema: classi P/NP/PSPACE/EXP, "
          "riduzioni polinomiali, lower bound, algoritmi ottimali, problemi NP-hard. "
          "Indica se esiste soluzione esatta efficiente o solo approssimazione. "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ══ INFORMATICA ESTREMA ══ */
        { "\xf0\x9f\x90\xa7", "Ingegnere Kernel",
          "Sei l'Ingegnere Kernel nella pipeline AI di Prismalux. "
          "Analizza il problema a livello OS: scheduling, gestione memoria (paging/segmentazione), "
          "driver, syscall, interrupt, IPC, race condition a livello kernel. "
          "Conosci Linux kernel internals, POSIX, e architetture x86/ARM. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\xa7\xac", "Specialista Compilatori",
          "Sei lo Specialista Compilatori nella pipeline AI di Prismalux. "
          "Analizza e progetta pipeline di compilazione: lexer, parser, AST, "
          "IR (LLVM/SSA), ottimizzazioni (dead code, inlining, vectorization), "
          "code generation, type systems, semantica denotazionale. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x95\xb8", "Ingegnere Sistemi Distribuiti",
          "Sei l'Ingegnere Sistemi Distribuiti nella pipeline AI di Prismalux. "
          "Progetta sistemi distribuiti robusti: consenso (Raft/Paxos), "
          "CAP theorem, eventual consistency, sharding, fault tolerance, "
          "CRDTs, distributed transactions, observability. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x92\xbb", "Esperto Quantum Computing",
          "Sei l'Esperto di Quantum Computing nella pipeline AI di Prismalux. "
          "Analizza il problema in chiave quantistica: qubit, gate quantistici, "
          "algoritmi (Shor, Grover, VQE), decoerenza, error correction (surface codes), "
          "vantaggi quantistici rispetto al classico. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\xa4\x96", "Esperto ML/AI",
          "Sei l'Esperto ML/AI nella pipeline AI di Prismalux. "
          "Affronta il problema con machine learning avanzato: architetture deep learning "
          "(transformer, diffusion, GNN), teoria dell'apprendimento (PAC, VC dimension), "
          "ottimizzazione (Adam, landscape loss), interpretabilit\xc3\xa0 (SHAP, mechanistic). "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ══ SICUREZZA ESTREMA ══ */
        { "\xf0\x9f\x94\x8e", "Reverse Engineer",
          "Sei il Reverse Engineer nella pipeline AI di Prismalux. "
          "Analizza il problema in ottica di reverse engineering: disassembly, decompilazione, "
          "analisi binaria statica/dinamica (IDA Pro, Ghidra, radare2), "
          "unpacking, anti-debug, obfuscation. Solo contesto legale/difensivo. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x92\xa3", "Exploit Developer",
          "Sei l'Exploit Developer nella pipeline AI di Prismalux. "
          "Analizza vulnerabilit\xc3\xa0 a basso livello: buffer overflow, use-after-free, "
          "ROP chains, heap exploitation, format string, kernel exploits. "
          "Proponi mitigazioni (ASLR, CFI, stack canaries). Solo contesto CTF/ricerca. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x95\xb5\xef\xb8\x8f", "Analista OSINT",
          "Sei l'Analista OSINT nella pipeline AI di Prismalux. "
          "Raccogli e correla informazioni da fonti aperte: "
          "footprinting, recon passivo, threat intelligence, analisi metadati, "
          "correlazione identit\xc3\xa0 digitali. Framework: MITRE ATT&CK, OSINT Framework. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x92\xbe", "Analista Forense Digitale",
          "Sei l'Analista Forense Digitale nella pipeline AI di Prismalux. "
          "Analizza il caso dal punto di vista forense: acquisizione prove, "
          "chain of custody, analisi filesystem (ext4/NTFS), memoria RAM, "
          "log correlation, timeline reconstruction, IOC extraction. "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ══ FISICA ESTREMA ══ */
        { "\xf0\x9f\x8c\x8c", "Fisico delle Particelle",
          "Sei il Fisico delle Particelle nella pipeline AI di Prismalux. "
          "Analizza il problema con il Modello Standard: QED, QCD, elettrodebole, "
          "simmetrie di gauge, bosoni, fermioni, Higgs, Feynman diagrams. "
          "Collega a fenomenologia LHC quando rilevante. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x8c\x91", "Cosmologo",
          "Sei il Cosmologo nella pipeline AI di Prismalux. "
          "Analizza il problema in chiave cosmologica: Big Bang, inflazione, "
          "materia/energia oscura, CMB, struttura su larga scala, "
          "equazioni di Friedmann, relativit\xc3\xa0 generale applicata. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xe2\x9a\x9b\xef\xb8\x8f", "Fisico Quantistico",
          "Sei il Fisico Quantistico nella pipeline AI di Prismalux. "
          "Analizza con meccanica quantistica avanzata: equazione di Schroedinger, "
          "operatori, perturbazione, entanglement, decoerenza, QFT, "
          "interpretazioni (Copenhagen, Many-Worlds, Bohm). "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ══ CHIMICA ESTREMA ══ */
        { "\xf0\x9f\xa7\xac", "Biochimico",
          "Sei il Biochimico nella pipeline AI di Prismalux. "
          "Analizza il problema a livello biochimico: enzimi, pathway metabolici, "
          "proteine (struttura primaria-quaternaria), DNA/RNA, PCR, CRISPR, "
          "termodinamica biologica (ATP, free energy). "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x92\x8a", "Farmacologo",
          "Sei il Farmacologo nella pipeline AI di Prismalux. "
          "Analizza farmacocinetica/farmacodinamica: ADME, binding molecolare, "
          "IC50, drug-receptor interaction, tossicit\xc3\xa0, trial design. "
          "Collega a chimica medicinale e target terapeutici. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\x94\xac", "Chimico Nucleare",
          "Sei il Chimico Nucleare nella pipeline AI di Prismalux. "
          "Analizza il problema con chimica e fisica nucleare: "
          "decadimento radioattivo, fissione/fusione, reazioni nucleari, "
          "isotopi, energia di legame, applicazioni (medicina nucleare, reattori). "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ══ INGEGNERIA ESTREMA ══ */
        { "\xf0\x9f\xa7\xb2", "Ingegnere del Caos",
          "Sei l'Ingegnere del Caos nella pipeline AI di Prismalux. "
          "Stress-testa il sistema: inietta fallimenti, identifica single point of failure, "
          "simula scenari catastrofici (split-brain, cascade failure, thundering herd). "
          "Principi Netflix Chaos Monkey, Game Days, fault injection. "
          "Rispondi SEMPRE e SOLO in italiano." },

        { "\xf0\x9f\xa7\xac", "Neuroscienziato Computazionale",
          "Sei il Neuroscienziato Computazionale nella pipeline AI di Prismalux. "
          "Analizza il problema collegando neuroscienze e computazione: "
          "modelli di neuroni (Hodgkin-Huxley, LIF), reti neurali biologiche, "
          "plasticit\xc3\xa0 sinaptica, cognizione computazionale, brain-computer interface. "
          "Rispondi SEMPRE e SOLO in italiano." },

        /* ══ MATEMATICA COMPUTAZIONALE ESTREMA ══ */
        { "\xf0\x9f\x94\xa2", "Matematico Computazionale",
          "Sei il Matematico Computazionale Estremo nella pipeline AI di Prismalux. "
          "Sei specializzato in calcoli ad altissima precisione e algoritmi per costanti matematiche. "
          "\n\nLe tue aree di competenza:"
          "\n- Aritmetica a precisione arbitraria (bignum): algoritmi di moltiplicazione "
          "  (Karatsuba, Toom-Cook, Schonhage-Strassen, Harvey-Hoeven O(n log n))"
          "\n- Calcolo di pi greco: serie Chudnovsky (miliardi di cifre), AGM di Gauss-Legendre, "
          "  BBP formula (calcola l'n-esima cifra senza le precedenti), machin-like formulas"
          "\n- Calcolo di e (Napier): serie 1/k!, formula di Euler, metodi BPP per e"
          "\n- Altre costanti: gamma di Euler-Mascheroni, costante di Catalan, Apery (zeta(3))"
          "\n- Analisi della complessit\xc3\xa0 in termini di cifre: O(M(n) log n) dove M(n) e' il "
          "  costo di moltiplicazione di interi a n cifre"
          "\n- Librerie e tool: GMP/MPFR (C), mpmath (Python), y-cruncher, PARI/GP"
          "\n- Verifica: tecniche di verifica a posteriori (Bailey-Broadhurst)"
          "\n\nPer ogni problema: (1) identifica l'algoritmo ottimale per la precisione richiesta, "
          "(2) stima la complessit\xc3\xa0 in cifre e in tempo, "
          "(3) indica eventuali colli di bottiglia (memoria, I/O, FFT), "
          "(4) mostra pseudocodice o codice Python/C con mpfr/mpmath se utile. "
          "Rispondi SEMPRE e SOLO in italiano." },
    };
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
AgentiPage::AgentiPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    m_namAuto = new QNetworkAccessManager(this);
    setupUI();

    connect(m_ai, &AiClient::token,       this, &AgentiPage::onToken);
    connect(m_ai, &AiClient::finished,    this, &AgentiPage::onFinished);
    connect(m_ai, &AiClient::error,       this, &AgentiPage::onError);
    /*
     * onModelsReady viene emesso da AiClient::fetchModels() ogni volta che
     * il backend cambia (Ollama → llama-server o viceversa). La connessione
     * permanente qui fa sì che le combo modello si aggiornino automaticamente
     * senza richiedere un fetchModels() esplicito dalla pagina.
     */
    connect(m_ai, &AiClient::modelsReady, this, &AgentiPage::onModelsReady);

    /* Carica subito la lista modelli dal backend attivo al momento della costruzione */
    m_ai->fetchModels();
}

/* ══════════════════════════════════════════════════════════════
   setupUI
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::setupUI() {
    /* ══════════════════════════════════════════════════════════
       Layout principale — ChatGPT/Claude style: tutto verticale
       ══════════════════════════════════════════════════════════ */
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 8, 12, 8);
    lay->setSpacing(6);

    /* ── titleRow: titolo + waitLbl ── */
    auto* titleRow = new QWidget(this);
    auto* titleL   = new QHBoxLayout(titleRow);
    titleL->setContentsMargins(0, 0, 0, 0);
    titleL->setSpacing(8);
    auto* title = new QLabel("\xf0\x9f\xa4\x96  Agenti AI", titleRow);
    title->setObjectName("pageTitle");
    m_waitLbl = new QLabel(titleRow);
    m_waitLbl->setStyleSheet("color:#E5C400; padding:2px 0; font-style:italic;");
    m_waitLbl->setVisible(false);
    titleL->addWidget(title);
    titleL->addWidget(m_waitLbl, 1);
    lay->addWidget(titleRow);

    /* ── topBar: Modalità + cfg + Auto + Agenti spinner + Libera RAM ── */
    auto* topBar = new QWidget(this);
    auto* topL   = new QHBoxLayout(topBar);
    topL->setContentsMargins(0, 0, 0, 0);
    topL->setSpacing(6);

    auto* modeLbl = new QLabel("Modalit\xc3\xa0:", topBar);
    modeLbl->setObjectName("cardDesc");
    topL->addWidget(modeLbl);

    m_cmbMode = new QComboBox(topBar);
    m_cmbMode->addItem("\xf0\x9f\x94\x84  Pipeline Agenti (sequenziale)");           // 0
    m_cmbMode->addItem("\xf0\x9f\x94\xae  Motore Byzantino (anti-allucinazione)");   // 1
    m_cmbMode->addItem("\xf0\x9f\xa7\xae  Matematico Teorico (esplorazione teorie)");// 2
    m_cmbMode->addItem("\xf0\x9f\x93\x90  Matematica");          // 3
    m_cmbMode->addItem("\xf0\x9f\x92\xbb  Informatica");         // 4
    m_cmbMode->addItem("\xf0\x9f\x94\x90  Sicurezza");           // 5
    m_cmbMode->addItem("\xe2\x9a\x9b\xef\xb8\x8f  Fisica");      // 6
    m_cmbMode->addItem("\xf0\x9f\xa7\xaa  Chimica");             // 7
    m_cmbMode->addItem("\xf0\x9f\x8c\x90  Lingue & Traduzione"); // 8
    m_cmbMode->addItem("\xf0\x9f\x8c\x8d  Trasversale");         // 9
    topL->addWidget(m_cmbMode, 1);

    m_btnCfg = new QPushButton("\xe2\x9a\x99  Agenti...", topBar);
    m_btnCfg->setObjectName("actionBtn");
    m_btnCfg->setToolTip("Apre la finestra di configurazione agenti (ruoli, modelli, RAG)");
    topL->addWidget(m_btnCfg);

    m_btnAuto = new QPushButton("\xf0\x9f\xa4\x96 Auto", topBar);
    m_btnAuto->setObjectName("actionBtn");
    m_btnAuto->setToolTip("Il modello pi\xc3\xb9 grande disponibile sceglie ruoli e modelli per ogni agente");
    topL->addWidget(m_btnAuto);

    auto* btnRam = new QPushButton("\xf0\x9f\xab\x80 Libera RAM", topBar);
    btnRam->setObjectName("actionBtn");
    btnRam->setToolTip(
#ifdef Q_OS_WIN
        "Windows: svuota working set processi (rundll32)"
#else
        "Linux: sync + drop_caches (richiede password admin)"
#endif
    );
    topL->addWidget(btnRam);

    lay->addWidget(topBar);

    /* ── m_autoLbl: stato preset / auto-assegnazione ── */
    m_autoLbl = new QLabel("", this);
    m_autoLbl->setObjectName("cardDesc");
    m_autoLbl->setWordWrap(true);
    m_autoLbl->setVisible(false);
    lay->addWidget(m_autoLbl);

    /* ── m_log: area output agenti (chat log) ── */
    m_log = new QTextEdit(this);
    m_log->setObjectName("chatLog");
    m_log->setReadOnly(true);
    m_log->setPlaceholderText(
        "Il log degli agenti appare qui...\n\n"
        "\xf0\x9f\x8d\xba Invocazione riuscita. Gli dei ascoltano.");
    lay->addWidget(m_log, 1);

    /* ── m_progress: barra progresso pipeline ── */
    /* Creata qui ma NON aggiunta a lay — viene incorporata nella status bar di MainWindow */
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setFixedHeight(18);
    m_progress->setTextVisible(true);
    m_progress->setFormat("\xf0\x9f\x8d\xba  Pronto \xe2\x80\x94 nessuna pipeline attiva");
    m_progress->setObjectName("pipelineProgress");
    m_progress->setStyleSheet(
        "QProgressBar { border: 1px solid #444; border-radius: 4px; background: #1e1e2e;"
        "  color: #cdd6f4; font-size: 11px; text-align: center; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #313244, stop:1 #89b4fa); border-radius: 3px; }");

    /* ── inputRow: campo task + colonna pulsanti ── */
    auto* inputRow = new QWidget(this);
    auto* inputL   = new QHBoxLayout(inputRow);
    inputL->setContentsMargins(0, 0, 0, 0);
    inputL->setSpacing(8);

    m_input = new QPlainTextEdit(inputRow);
    m_input->setObjectName("chatInput");
    m_input->setPlaceholderText(
        "Es: Scrivi una funzione Python che ordina una lista con quicksort...\n"
        "(Invio per avviare, Shift+Invio per andare a capo)");
    m_input->setFixedHeight(76);   /* ~3 righe */
    inputL->addWidget(m_input, 1);

    /* btnCol: colonna verticale pulsanti azione */
    auto* btnCol  = new QWidget(inputRow);
    auto* btnColL = new QVBoxLayout(btnCol);
    btnColL->setContentsMargins(0, 0, 0, 0);
    btnColL->setSpacing(4);

    m_btnRun = new QPushButton("\xe2\x96\xb6  Avvia", btnCol);
    m_btnRun->setObjectName("actionBtn");
    btnColL->addWidget(m_btnRun);

    auto* btnQuick = new QPushButton("\xe2\x9a\xa1 Singolo", btnCol);
    btnQuick->setObjectName("actionBtn");
    btnQuick->setToolTip("Risposta immediata — 1 solo agente, nessuna pipeline");
    btnColL->addWidget(btnQuick);

    inputL->addWidget(btnCol);
    lay->addWidget(inputRow);

    /* ══════════════════════════════════════════════════════════
       m_cfgDlg — finestra NON-MODALE configurazione agenti
       ══════════════════════════════════════════════════════════ */
    m_cfgDlg = new QDialog(this);
    m_cfgDlg->setWindowTitle("\xe2\x9a\x99  Configurazione Agenti");
    m_cfgDlg->setMinimumWidth(700);

    auto* dlgLay = new QVBoxLayout(m_cfgDlg);
    dlgLay->setContentsMargins(12, 12, 12, 12);
    dlgLay->setSpacing(8);

    /* Riga: Agenti N (spinner) */
    auto* dlgTopRow = new QWidget(m_cfgDlg);
    auto* dlgTopL   = new QHBoxLayout(dlgTopRow);
    dlgTopL->setContentsMargins(0,0,0,0); dlgTopL->setSpacing(8);
    auto* dlgTitle = new QLabel("Configurazione pipeline agenti", m_cfgDlg);
    dlgTitle->setObjectName("cardDesc");
    auto* agentiLbl = new QLabel("Agenti:", m_cfgDlg);
    agentiLbl->setObjectName("cardDesc");
    m_spinShots = new QSpinBox(m_cfgDlg);
    m_spinShots->setRange(1, MAX_AGENTS);
    m_spinShots->setValue(MAX_AGENTS);
    m_spinShots->setFixedWidth(52);
    m_spinShots->setToolTip("Quanti agenti usare (1 = risposta immediata, 6 = pipeline completa)");
    dlgTopL->addWidget(dlgTitle, 1);
    dlgTopL->addWidget(agentiLbl);
    dlgTopL->addWidget(m_spinShots);
    dlgLay->addWidget(dlgTopRow);

    /* Griglia agenti nel dialog */
    auto* cfgGrid = new QGridLayout();
    cfgGrid->setSpacing(4);
    cfgGrid->setContentsMargins(0, 0, 0, 0);

    auto mkHdr = [&](const QString& t) {
        auto* l = new QLabel(t, m_cfgDlg);
        l->setObjectName("cardDesc");
        return l;
    };
    cfgGrid->addWidget(mkHdr("Agente"),  0, 0);
    cfgGrid->addWidget(mkHdr("Ruolo"),   0, 1);
    cfgGrid->addWidget(mkHdr("Modello"), 0, 2);
    cfgGrid->addWidget(mkHdr("\xf0\x9f\x93\x8e RAG"), 0, 3);

    auto roleList = roles();
    std::sort(roleList.begin(), roleList.end(),
              [](const AgentRole& a, const AgentRole& b){ return a.name < b.name; });
    QStringList roleNames;
    for (const auto& r : roleList)
        roleNames << r.icon + "  " + r.name;

    int defaults[MAX_AGENTS] = {0, 1, 2, 3, 4, 5};

    for (int i = 0; i < MAX_AGENTS; i++) {
        m_enabledChk[i] = new QCheckBox(QString("Agente %1").arg(i + 1), m_cfgDlg);
        m_enabledChk[i]->setChecked(true);

        m_roleCombo[i] = new QComboBox(m_cfgDlg);
        m_roleCombo[i]->addItems(roleNames);
        m_roleCombo[i]->setCurrentIndex(defaults[i]);

        m_modelCombo[i] = new QComboBox(m_cfgDlg);
        m_modelCombo[i]->setMinimumWidth(120);
        m_modelCombo[i]->addItem("(caricamento...)");

        m_ragWidget[i] = new RagDropWidget(m_cfgDlg);

        connect(m_enabledChk[i], &QCheckBox::toggled, this, [this, i](bool on){
            m_roleCombo[i]->setEnabled(on);
            m_modelCombo[i]->setEnabled(on);
            m_ragWidget[i]->setEnabled(on);
        });

        cfgGrid->addWidget(m_enabledChk[i],  i + 1, 0);
        cfgGrid->addWidget(m_roleCombo[i],   i + 1, 1);
        cfgGrid->addWidget(m_modelCombo[i],  i + 1, 2);
        cfgGrid->addWidget(m_ragWidget[i],   i + 1, 3);
    }
    cfgGrid->setColumnStretch(1, 2);
    cfgGrid->setColumnStretch(2, 2);
    cfgGrid->setColumnStretch(3, 1);

    auto* cfgGridWidget = new QWidget(m_cfgDlg);
    cfgGridWidget->setLayout(cfgGrid);
    dlgLay->addWidget(cfgGridWidget, 1);

    /* m_autoLbl duplicato nel dialog per info preset (condivide il puntatore, ma
     * il dialog ha il suo label separato per non nascondersi col main) */
    /* Close button */
    auto* dlgBtns = new QDialogButtonBox(QDialogButtonBox::Close, m_cfgDlg);
    connect(dlgBtns, &QDialogButtonBox::rejected, m_cfgDlg, &QDialog::hide);
    dlgLay->addWidget(dlgBtns);

    /* Visibilità iniziale righe nel dialog (tutti visibili: spinShots = MAX_AGENTS) */
    {
        int n = m_spinShots->value();
        for (int i = 0; i < MAX_AGENTS; i++) {
            bool vis = (i < n);
            m_enabledChk[i]->setVisible(vis);
            m_roleCombo[i]->setVisible(vis);
            m_modelCombo[i]->setVisible(vis);
            m_ragWidget[i]->setVisible(vis);
        }
    }

    /* ══════════════════════════════════════════════════════════
       Presets tabella (idx 3-9 → presets[0-6])
       ══════════════════════════════════════════════════════════ */
    static const int presets[7][MAX_AGENTS] = {
        { 12, 13, 15, 29,  9,  6 },   /* 3 — Matematica   */
        {  0, 16,  2,  5,  7,  4 },   /* 4 — Informatica  */
        {  0, 19,  7, 20, 39,  6 },   /* 5 — Sicurezza    */
        { 22, 23, 43, 10,  9, 26 },   /* 6 — Fisica       */
        { 24, 25, 44, 10,  9, 26 },   /* 7 — Chimica      */
        {  8,  9,  6,  0, 26, 13 },   /* 8 — Lingue       */
        {  9,  0,  6, 26, 27,  8 },   /* 9 — Trasversale  */
    };
    static const char* presetLabels[7] = {
        "\xf0\x9f\x93\x90  Preset Matematica applicato.",
        "\xf0\x9f\x92\xbb  Preset Informatica applicato.",
        "\xf0\x9f\x94\x90  Preset Sicurezza applicato.",
        "\xe2\x9a\x9b\xef\xb8\x8f  Preset Fisica applicato.",
        "\xf0\x9f\xa7\xaa  Preset Chimica applicato.",
        "\xf0\x9f\x8c\x90  Preset Lingue applicato.",
        "\xf0\x9f\x8c\x8d  Preset Trasversale applicato.",
    };

    /* ══════════════════════════════════════════════════════════
       Connessioni
       ══════════════════════════════════════════════════════════ */

    /* 1. m_spinShots → aggiorna m_maxShots + visibilità righe nel dialog */
    connect(m_spinShots, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v){
                m_maxShots = v;
                for (int i = 0; i < MAX_AGENTS; i++) {
                    bool vis = (i < v);
                    if (m_enabledChk[i])  m_enabledChk[i]->setVisible(vis);
                    if (m_roleCombo[i])   m_roleCombo[i]->setVisible(vis);
                    if (m_modelCombo[i])  m_modelCombo[i]->setVisible(vis);
                    if (m_ragWidget[i])   m_ragWidget[i]->setVisible(vis);
                }
            });

    /* 2. m_btnRun → avvia se idle, interrompe se in esecuzione (toggle Start/Stop) */
    connect(m_btnRun, &QPushButton::clicked, this, [this]{
        if (m_opMode != OpMode::Idle) {
            /* Siamo in esecuzione: funzione da Stop */
            m_ai->abort();
            return;
        }
        /* Siamo idle: funzione da Avvia */
        int mode = m_cmbMode->currentIndex();
        if      (mode == 0 || mode >= 3) runPipeline();
        else if (mode == 1) runByzantine();
        else if (mode == 2) runMathTheory();
    });

    /* 4. Enter = Avvia, Shift+Enter = vai a capo (gestito da eventFilter) */
    m_input->installEventFilter(this);

    /* 5. m_ai aborted → reset UI */
    connect(m_ai, &AiClient::aborted, this, [this]{
        m_waitLbl->setVisible(false);
        m_opMode       = OpMode::Idle;
        m_currentAgent = 0;
        m_agentOutputs.clear();
        m_byzStep      = 0;
        m_btnRun->setText("\xe2\x96\xb6  Avvia");
        m_progress->setRange(0, 100);
        m_progress->setTextVisible(true);
        m_progress->setValue(0);
        m_progress->setFormat("\xe2\x9c\x8b  Interrotto");
        m_progress->setStyleSheet(
            "QProgressBar { border: 1px solid #444; border-radius: 4px; background: #1e1e2e;"
            "  color: #cdd6f4; font-size: 11px; text-align: center; }"
            "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "  stop:0 #e64553, stop:1 #f38ba8); border-radius: 3px; }");
        for (int i = 0; i < MAX_AGENTS; i++)
            if (m_enabledChk[i]) m_enabledChk[i]->setStyleSheet("");
        m_log->append("\n\xe2\x9c\x8b  Pipeline interrotta.");
    });

    /* 6. m_btnAuto → autoAssignRoles */
    connect(m_btnAuto, &QPushButton::clicked, this, &AgentiPage::autoAssignRoles);

    /* 7. m_cmbMode → Auto visibile solo in Pipeline (idx==0), Agenti sempre visibile */
    connect(m_cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx){
        m_btnAuto->setVisible(idx == 0);
    });

    /* Preset applicazione per idx 3-9 */
    connect(m_cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx){
        if (idx < 3) return;
        const int* preset = presets[idx - 3];
        for (int i = 0; i < MAX_AGENTS; i++) {
            if (m_roleCombo[i] && preset[i] >= 0 && preset[i] < m_roleCombo[i]->count())
                m_roleCombo[i]->setCurrentIndex(preset[i]);
        }
        m_autoLbl->setText(QString::fromUtf8(presetLabels[idx - 3])
                           + "  Puoi modificare ogni ruolo prima di Avviare.");
        m_autoLbl->setVisible(true);
    });

    /* 8. MathTheory mode (idx==2) → pre-seleziona modelli reasoning/coder */
    connect(m_cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx){
        if (idx != 2) {
            if (idx < 3) m_autoLbl->setVisible(false);
            return;
        }
        static const QStringList mathKw  = {"r1","math","reason","think","qwq","deepseek-r"};
        static const QStringList coderKw = {"coder","code","starcoder","codellama","qwen2.5-coder"};
        static const QStringList largeKw = {"qwen3","llama3","gemma","mistral","phi"};

        auto bestMatch = [&](int comboIdx) -> int {
            QComboBox* cb = m_modelCombo[comboIdx];
            for (int p = 0; p < cb->count(); p++) {
                QString n = cb->itemText(p).toLower();
                for (const auto& kw : mathKw) if (n.contains(kw)) return p;
            }
            for (int p = 0; p < cb->count(); p++) {
                QString n = cb->itemText(p).toLower();
                for (const auto& kw : coderKw) if (n.contains(kw)) return p;
            }
            for (int p = 0; p < cb->count(); p++) {
                QString n = cb->itemText(p).toLower();
                for (const auto& kw : largeKw) if (n.contains(kw)) return p;
            }
            return -1;
        };

        int assigned = 0;
        for (int i = 0; i < MAX_AGENTS; i++) {
            if (!m_enabledChk[i] || !m_enabledChk[i]->isChecked()) continue;
            int best = bestMatch(i);
            if (best >= 0) { m_modelCombo[i]->setCurrentIndex(best); assigned++; }
        }
        m_autoLbl->setText(assigned > 0
            ? "\xf0\x9f\xa7\xae  Modelli reasoning/coder pre-selezionati. Puoi cambiarli."
            : "\xf0\x9f\x92\xa1  Consigliato: deepseek-r1, qwen3, qwq o coder.");
        m_autoLbl->setVisible(true);
    });

    /* 9. btnRam → libera RAM */
    connect(btnRam, &QPushButton::clicked, this, [this, btnRam]{
        btnRam->setEnabled(false);
        m_autoLbl->setText("\xf0\x9f\xab\x80  Liberazione RAM in corso...");
        m_autoLbl->setVisible(true);
        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
        proc->start("rundll32.exe", {"advapi32.dll,ProcessIdleTasks"});
#else
        proc->start("pkexec", {"sh", "-c", "sync && echo 3 > /proc/sys/vm/drop_caches"});
#endif
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, btnRam](int code, QProcess::ExitStatus){
            QString out = QString::fromUtf8(proc->readAll()).trimmed();
            if (code == 0) {
                m_autoLbl->setText("\xe2\x9c\x85  RAM liberata \xe2\x80\x94 cache del kernel svuotata.");
                m_log->append("\n\xf0\x9f\xab\x80  Drop caches eseguito. RAM liberata.");
            } else {
                QString reason = (code == 126 || code == 127)
                    ? "Operazione annullata o pkexec non trovato."
                    : QString("Errore (code %1). %2").arg(code).arg(out);
                m_autoLbl->setText("\xe2\x9d\x8c  " + reason);
                m_log->append("\n\xe2\x9d\x8c  Libera RAM: " + reason);
            }
            btnRam->setEnabled(true);
            proc->deleteLater();
        });
    });

    /* 10. btnQuick → risposta immediata 1 agente */
    connect(btnQuick, &QPushButton::clicked, this, [this]{
        m_spinShots->setValue(1);
        m_maxShots = 1;
        m_btnRun->click();
    });

    /* 11. btnCfg → mostra/porta in primo piano il dialog configurazione */
    connect(m_btnCfg, &QPushButton::clicked, this, [this]{
        m_cfgDlg->show();
        m_cfgDlg->raise();
        m_cfgDlg->activateWindow();
    });
}

/* ══════════════════════════════════════════════════════════════
   eventFilter — Enter invia, Shift+Enter va a capo in m_input
   ══════════════════════════════════════════════════════════════ */
bool AgentiPage::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == m_input && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
                && !(ke->modifiers() & Qt::ShiftModifier)) {
            m_btnRun->click();
            return true;   /* evento consumato — non inserisce newline */
        }
    }
    return QWidget::eventFilter(obj, ev);
}

/* ══════════════════════════════════════════════════════════════
   onModelsReady — popola le combo modello di ciascun agente
   ══════════════════════════════════════════════════════════════
   Chiamata automaticamente da AiClient ogni volta che il backend
   cambia (cambio manuale in header o avvio/arresto llama-server).
   Ollama restituisce tutti i modelli installati; llama-server solo
   quello attualmente caricato — le combo si aggiornano in entrambi
   i casi senza logiche condizionali qui.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onModelsReady(const QStringList& list) {
    for (int i = 0; i < MAX_AGENTS; i++) {
        m_modelCombo[i]->clear();
        for (const auto& m : list)
            m_modelCombo[i]->addItem(m, m);
        if (m_modelCombo[i]->count() == 0)
            m_modelCombo[i]->addItem("(nessun modello)");
    }

    /*
     * Auto-assegna ha senso solo quando ci sono almeno 2 modelli da scegliere.
     * Con llama-server (un solo modello caricato) l'orchestratore non può
     * differenziare i ruoli — disabilitiamo il pulsante e spieghiamo il motivo.
     * Con Ollama (lista completa) il pulsante rimane abilitato come prima.
     */
    const bool canAutoAssign = list.size() > 1;
    m_btnAuto->setEnabled(canAutoAssign);
    m_btnAuto->setToolTip(canAutoAssign
        ? "Auto-assegna ruoli e modelli ottimali tramite orchestratore AI"
        : "Auto-assegna non disponibile: un solo modello presente.\n"
          "llama-server carica un modello alla volta — usa Ollama per avere la selezione automatica.");
}

/* ══════════════════════════════════════════════════════════════
   autoAssignRoles — assegnazione automatica ruoli tramite orchestratore
   ══════════════════════════════════════════════════════════════
   Flusso:
     1. Fetch lista modelli dal backend ATTIVO (non hardcoded Ollama):
          Ollama      → GET /api/tags    → JSON: models[].{name, size}
          llama-server→ GET /v1/models   → JSON: data[].{id}  (size=0)
     2. Seleziona il modello più grande (o l'unico per llama-server)
        come "orchestratore" e chiede via chat di assegnare i ruoli.
     3. La risposta JSON viene interpretata da parseAutoAssign().

   BUG STORICO RISOLTO (2026-03-16):
     - L'URL era sempre /api/tags (solo Ollama); con llama-server → errore silenzioso
     - setBackend() hardcodato su AiClient::Ollama: dopo l'auto-assign il backend
       veniva forzato su Ollama anche se l'utente aveva scelto llama-server,
       causando la comparsa di modelli Ollama nelle combo invece di quelli del server.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::autoAssignRoles() {
    QString task = m_input->toPlainText().trimmed();
    if (task.isEmpty()) {
        m_log->append("\xe2\x9a\xa0  Inserisci prima un task per l'auto-assegnazione."); return;
    }

    m_autoLbl->setText("\xf0\x9f\x94\x84  Recupero modelli disponibili...");
    m_autoLbl->setVisible(true);
    m_btnAuto->setEnabled(false);

    /*
     * URL e parsing differiscono per backend:
     *   Ollama      → GET /api/tags  (models[].name + models[].size)
     *   LlamaServer → GET /v1/models (data[].id, size=0 — un solo modello caricato)
     * In entrambi i casi l'host e la porta vengono presi da m_ai.
     */
    const bool isOllama = (m_ai->backend() == AiClient::Ollama);
    QString url = QString("http://%1:%2%3")
                  .arg(m_ai->host()).arg(m_ai->port())
                  .arg(isOllama ? "/api/tags" : "/v1/models");
    auto* reply = m_namAuto->get(QNetworkRequest(QUrl(url)));

    connect(reply, &QNetworkReply::finished, this, [this, reply, task, isOllama]{
        reply->deleteLater();
        m_modelInfos.clear();

        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (isOllama) {
                /* Ollama: { "models": [ { "name": "...", "size": 123 }, ... ] } */
                for (auto v : doc.object()["models"].toArray()) {
                    QJsonObject obj = v.toObject();
                    m_modelInfos.append({
                        obj["name"].toString(),
                        obj["size"].toVariant().toLongLong()
                    });
                }
            } else {
                /* llama-server: { "data": [ { "id": "..." }, ... ] } */
                for (auto v : doc.object()["data"].toArray()) {
                    m_modelInfos.append({
                        v.toObject()["id"].toString(),
                        0   /* llama-server non espone la dimensione */
                    });
                }
            }
        }

        if (m_modelInfos.isEmpty()) {
            m_autoLbl->setText("\xe2\x9d\x8c  Nessun modello trovato sul server corrente.");
            m_btnAuto->setEnabled(true);
            return;
        }

        /* llama-server: un solo modello caricato — l'auto-assign ottimizza i ruoli,
         * non la scelta del modello (che è unica). Avvisa l'utente nel log. */
        if (!isOllama && m_modelInfos.size() == 1) {
            m_log->append("\xf0\x9f\x93\x8c  llama-server: un solo modello disponibile — "
                          "l'auto-assign ottimizzer\xc3\xa0 i ruoli, non i modelli.");
        }

        /* Ordina per dimensione decrescente → il più grande per primo.
         * Fallback alfabetico quando tutti i size sono uguali (es. llama-server, size=0). */
        std::sort(m_modelInfos.begin(), m_modelInfos.end(),
                  [](const ModelInfo& a, const ModelInfo& b){
                      if (a.size != b.size) return a.size > b.size;
                      return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
                  });

        const QString biggest = m_modelInfos.first().name;
        const qint64  bigSize = m_modelInfos.first().size;

        /* Lista nomi modelli per il prompt */
        QStringList modelNames;
        for (const auto& mi : m_modelInfos)
            modelNames << mi.name;

        /* Lista ruoli per il prompt */
        auto roleList = roles();
        QStringList roleNames;
        for (const auto& r : roleList)
            roleNames << r.name;

        m_autoLbl->setText(QString(
            "\xf0\x9f\xa4\x96  Orchestratore: \xef\xbc\x88%1, %2 GB\xef\xbc\x89 — assegnazione ruoli...")
            .arg(biggest)
            .arg(bigSize / 1'000'000'000.0, 0, 'f', 1));

        /* Imposta il modello più grande — preserva il backend corrente (Ollama o llama-server) */
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), biggest);

        m_opMode = OpMode::AutoAssign;
        m_autoBuffer.clear();

        QString sys =
            "Sei un orchestratore AI. Analizza il task e assegna i ruoli pi\xc3\xb9 efficaci "
            "agli agenti della pipeline. Rispondi SOLO con JSON valido, senza altro testo.";

        QString prompt = QString(
            "Task dell'utente: \"%1\"\n\n"
            "Ruoli disponibili: %2\n\n"
            "Modelli disponibili: %3\n\n"
            "Assegna da 2 a 6 agenti. Per ogni agente scegli:\n"
            "- il ruolo pi\xc3\xb9 utile per questo task (es. Programmatore per codice, "
            "Ricercatore per domande, Analizzatore+Scrittore per testi)\n"
            "- il modello pi\xc3\xb9 adatto (assegna modelli 'coder' ai ruoli di programmazione "
            "se disponibili, il modello pi\xc3\xb9 grande per analisi complesse)\n\n"
            "Rispondi ESATTAMENTE con questo JSON (nessun testo prima o dopo):\n"
            "[\n"
            "  {\"agente\": 1, \"ruolo\": \"NomeRuolo\", \"modello\": \"nomemodello\"},\n"
            "  {\"agente\": 2, \"ruolo\": \"NomeRuolo\", \"modello\": \"nomemodello\"}\n"
            "]")
            .arg(task)
            .arg(roleNames.join(", "))
            .arg(modelNames.join(", "));

        m_ai->chat(sys, prompt);
    });
}

void AgentiPage::parseAutoAssign(const QString& raw) {
    /* Preprocessing: rimuove blocchi di pensiero e markdown code blocks.
     * qwen3 e altri modelli thinking producono <think>...</think> che può
     * contenere '[' spuri che confondono il parser JSON. */
    QString cleaned = raw;

    /* 1. Rimuove <think>...</think> (qwen3, deepseek-r1, ecc.) */
    {
        QRegularExpression re("<think>[\\s\\S]*?</think>",
                              QRegularExpression::CaseInsensitiveOption);
        cleaned.remove(re);
    }
    /* 2. Estrae contenuto da blocchi ```json ... ``` o ``` ... ``` */
    {
        QRegularExpression re("```(?:json)?\\s*([\\s\\S]*?)```");
        auto m = re.match(cleaned);
        if (m.hasMatch()) cleaned = m.captured(1).trimmed();
    }

    /* Estrai il blocco JSON dalla risposta (ignora testo prima/dopo) */
    int start = cleaned.indexOf('[');
    int end   = cleaned.lastIndexOf(']');
    if (start < 0 || end < start) {
        m_autoLbl->setText("\xe2\x9d\x8c  Formato JSON non valido nella risposta dell'orchestratore.");
        m_btnAuto->setEnabled(true);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(cleaned.mid(start, end - start + 1).toUtf8());
    if (!doc.isArray()) {
        m_autoLbl->setText("\xe2\x9d\x8c  JSON non analizzabile.");
        m_btnAuto->setEnabled(true);
        return;
    }

    auto roleList = roles();
    QJsonArray arr = doc.array();
    int assigned = 0;

    /* Disabilita tutti i slot prima di riassegnare */
    for (int i = 0; i < MAX_AGENTS; i++)
        m_enabledChk[i]->setChecked(false);

    for (int i = 0; i < arr.size() && i < MAX_AGENTS; i++) {
        QJsonObject obj = arr[i].toObject();
        QString roleName  = obj["ruolo"].toString();
        QString modelName = obj["modello"].toString();

        /* Trova indice ruolo */
        int roleIdx = -1;
        for (int r = 0; r < roleList.size(); r++) {
            if (roleList[r].name.compare(roleName, Qt::CaseInsensitive) == 0) {
                roleIdx = r; break;
            }
        }
        /* Fallback: cerca parziale */
        if (roleIdx < 0) {
            for (int r = 0; r < roleList.size(); r++) {
                if (roleList[r].name.contains(roleName, Qt::CaseInsensitive) ||
                    roleName.contains(roleList[r].name, Qt::CaseInsensitive)) {
                    roleIdx = r; break;
                }
            }
        }
        if (roleIdx < 0) roleIdx = i % roleList.size();

        m_enabledChk[i]->setChecked(true);
        m_roleCombo[i]->setCurrentIndex(roleIdx);

        /* Trova modello nella combo */
        int mIdx = m_modelCombo[i]->findText(modelName, Qt::MatchContains);
        if (mIdx >= 0) m_modelCombo[i]->setCurrentIndex(mIdx);

        assigned++;
    }

    /*
     * Ripristina il modello sull'agente 0 dopo l'auto-assign.
     * m_ai->backend() invece di AiClient::Ollama: preserva il backend corrente
     * (se l'utente usa llama-server, non forziamo il ritorno a Ollama).
     */
    if (m_modelCombo[0]->count() > 0)
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(),
                         m_modelCombo[0]->currentText());

    m_autoLbl->setText(QString("\xe2\x9c\x85  Assegnati %1 agenti — puoi modificarli e poi premere Avvia.").arg(assigned));
    m_btnAuto->setEnabled(true);
}

/* ══════════════════════════════════════════════════════════════
   Pipeline sequenziale
   ══════════════════════════════════════════════════════════════ */
/* ══════════════════════════════════════════════════════════════
   GUARDIA MATEMATICA LOCALE
   Valuta espressioni semplici senza toccare la pipeline AI.
   Ritorna la stringa "expr = risultato" se gestita, "" altrimenti.
   ══════════════════════════════════════════════════════════════ */
static const char* _gp_ptr;

static void   _gp_ws()  { while(*_gp_ptr==' '||*_gp_ptr=='\t') _gp_ptr++; }
static double _gp_expr();
static double _gp_prim() {
    _gp_ws();
    if(*_gp_ptr=='('){_gp_ptr++;double v=_gp_expr();_gp_ws();if(*_gp_ptr==')')_gp_ptr++;return v;}
    char *e; double v=strtod(_gp_ptr,&e);
    if(e!=_gp_ptr){_gp_ptr=e;return v;} return 0.0;
}
static double _gp_pow() {
    _gp_ws(); int neg=(*_gp_ptr=='-');if(neg||*_gp_ptr=='+')_gp_ptr++;
    double v=_gp_prim(); _gp_ws();
    if(*_gp_ptr=='^'){_gp_ptr++;v=std::pow(v,_gp_pow());}
    if(*_gp_ptr=='!'){_gp_ptr++;long long n=(long long)std::fabs(v),f=1;
        for(long long i=2;i<=n&&i<=20;i++)f*=i;v=(double)f;}
    return neg?-v:v;
}
static double _gp_term() {
    double v=_gp_pow();
    while(true){_gp_ws();char op=*_gp_ptr;
        if(op=='*'||op=='/'||op=='%'){_gp_ptr++;double r=_gp_pow();
            if(op=='*')v*=r;else if(op=='/')v=(r?v/r:0);else v=(r?std::fmod(v,r):0);}
        else break;}
    return v;
}
static double _gp_expr() {
    double v=_gp_term();
    while(true){_gp_ws();
        if(*_gp_ptr=='+'){_gp_ptr++;v+=_gp_term();}
        else if(*_gp_ptr=='-'){_gp_ptr++;v-=_gp_term();}
        else break;}
    return v;
}
static QString _gp_fmt(double v) {
    if(v==(long long)v && std::fabs(v)<1e15) return QString::number((long long)v);
    return QString::number(v,'g',6);
}

QString AgentiPage::guardiaMath(const QString& input)
{
    QString low = input.toLower().trimmed();

    /* Prefissi linguaggio naturale italiano */
    static const QStringList prefissi = {
        "quanto fa ", "quanto vale ", "quanto e' ", "quanto e ",
        "calcola ", "risultato di ", "dimmi ", "quanto risulta ",
        "quant'e' ", "quante fa "
    };
    for (const QString& pref : prefissi) {
        if (low.startsWith(pref)) {
            QString expr = low.mid(pref.length());
            while (!expr.isEmpty() && (expr.back()=='?' || expr.back()==' '))
                expr.chop(1);
            if (expr.isEmpty()) continue;
            QByteArray ba = expr.toLatin1();
            _gp_ptr = ba.constData();
            double v = _gp_expr(); _gp_ws();
            if (!*_gp_ptr)
                return QString("%1 = %2").arg(expr).arg(_gp_fmt(v));
            break;
        }
    }

    /* Sconto X% su Y */
    {
        static QRegularExpression re("sconto\\s+([0-9.]+)%\\s+su\\s+([0-9.]+)");
        auto m = re.match(low);
        if (m.hasMatch()) {
            double pct = m.captured(1).toDouble(), base = m.captured(2).toDouble();
            double risp = base*pct/100.0, fin = base-risp;
            return QString("Sconto %.4g%% su %1\nRisparmio: %2   Prezzo finale: %3")
                   .arg(_gp_fmt(base)).arg(_gp_fmt(risp)).arg(_gp_fmt(fin));
        }
    }
    /* X% di Y */
    {
        static QRegularExpression re("([0-9.]+)%\\s+di\\s+([0-9.]+)");
        auto m = re.match(low);
        if (m.hasMatch()) {
            double pct = m.captured(1).toDouble(), base = m.captured(2).toDouble();
            return QString("%1% di %2 = %3").arg(pct).arg(_gp_fmt(base)).arg(_gp_fmt(base*pct/100.0));
        }
    }

    /* Espressione pura (solo cifre + operatori) */
    bool hasDig=false, hasLet=false;
    for (QChar c : low) {
        if (c.isDigit()) hasDig=true;
        if (c.isLetter() && c!='e' && c!='E') hasLet=true;
    }
    if (hasDig && !hasLet) {
        QByteArray ba = low.toLatin1();
        _gp_ptr = ba.constData();
        double v = _gp_expr(); _gp_ws();
        if (!*_gp_ptr)
            return QString("%1 = %2").arg(input.trimmed()).arg(_gp_fmt(v));
    }

    return {};  /* non gestita → usa pipeline */
}

void AgentiPage::runPipeline() {
    QString task = m_input->toPlainText().trimmed();
    if (task.isEmpty()) { m_log->append("\xe2\x9a\xa0  Inserisci un task."); return; }

    /* Guardia matematica locale — risposta istantanea senza AI */
    {
        QElapsedTimer tmr; tmr.start();
        QString ris = guardiaMath(task);
        double ms = tmr.nsecsElapsed() / 1e6;
        if (!ris.isEmpty()) {
            m_log->clear();
            m_log->append("\xe2\x9c\x94  <b>Risposta locale (0 token):</b>");
            m_log->append("<b>" + ris + "</b>");
            m_log->append(QString("\n\xe2\x84\xb9  Calcolo locale \xe2\x80\x94 tempo: <b>%1 ms</b> (%2 s) \xe2\x80\x94 nessun agente avviato.")
                          .arg(ms, 0, 'f', 3).arg(ms / 1000.0, 0, 'f', 6));
            return;
        }
    }

    int count = 0;
    for (int i = 0; i < MAX_AGENTS; i++)
        if (m_enabledChk[i]->isChecked()) count++;
    if (count == 0) { m_log->append("\xe2\x9a\xa0  Abilita almeno un agente."); return; }

    m_taskOriginal = task;
    m_agentOutputs.clear();
    m_currentAgent = 0;
    m_maxShots     = m_spinShots->value();
    m_opMode       = OpMode::Pipeline;
    emit pipelineStarted();

    /* Reset visivo spunte e barra progresso */
    for (int i = 0; i < MAX_AGENTS; i++)
        m_enabledChk[i]->setStyleSheet("");
    m_progress->setValue(0);
    m_progress->setFormat("Avvio pipeline...");
    m_progress->setStyleSheet(
        "QProgressBar { border: 1px solid #444; border-radius: 4px; background: #1e1e2e;"
        "  color: #cdd6f4; font-size: 11px; text-align: center; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #313244, stop:1 #89b4fa); border-radius: 3px; }");

    m_log->clear();
    m_log->append(QString("\xf0\x9f\x94\x84  Pipeline avviata — %1 agenti attivi").arg(count));
    m_log->append(QString("\xf0\x9f\x93\x9d  Task: %1\n").arg(task));
    m_log->append(QString(43, QChar(0x2500)));

    m_btnRun->setText("\xe2\x8f\xb9  Stop");
    m_waitLbl->setVisible(true);

    advancePipeline();
}

void AgentiPage::advancePipeline() {
    while (m_currentAgent < MAX_AGENTS && !m_enabledChk[m_currentAgent]->isChecked())
        m_currentAgent++;

    if (m_currentAgent >= MAX_AGENTS || m_currentAgent >= m_maxShots) {
        m_log->append("\n\xe2\x9c\x85  Pipeline completata. \xe2\x9c\xa8 Verit\xc3\xa0 rivelata.");
        m_progress->setRange(0, 100);
        m_progress->setTextVisible(true);
        m_progress->setValue(100);
        m_progress->setFormat("\xe2\x9c\x85  Completata!");
        m_progress->setStyleSheet(
            "QProgressBar { border: 1px solid #444; border-radius: 4px; background: #1e1e2e;"
            "  color: #cdd6f4; font-size: 11px; text-align: center; }"
            "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "  stop:0 #40a02b, stop:1 #a6e3a1); border-radius: 3px; }");
        m_btnRun->setText("\xe2\x96\xb6  Avvia");
        m_opMode = OpMode::Idle;
        /* Salva in cronologia */
        emit chatCompleted("Pipeline",
                           m_ai->model(),
                           m_taskOriginal,
                           m_log->toHtml());
        return;
    }

    /* Calcola quanti agenti sono abilitati per la percentuale */
    int total = 0;
    for (int i = 0; i < MAX_AGENTS; i++)
        if (m_enabledChk[i]->isChecked()) total++;
    int done = 0;
    for (int i = 0; i < m_currentAgent; i++)
        if (m_enabledChk[i]->isChecked()) done++;
    int pct = (total > 0) ? (done * 100 / total) : 0;

    auto roleList = roles();
    int roleIdx = m_roleCombo[m_currentAgent]->currentIndex();
    if (roleIdx < 0 || roleIdx >= roleList.size()) roleIdx = 0;
    QString roleName = roleList[roleIdx].icon + " " + roleList[roleIdx].name;

    /* Barra determinata: parte dalla % già maturata, si riempirà token per token */
    m_tokenCount = 0;
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(true);
    m_progress->setValue(pct);
    m_progress->setFormat(QString("\xe2\x9a\x99\xef\xb8\x8f  Agente %1/%2 — %3...")
        .arg(done + 1).arg(total).arg(roleName));

    runAgent(m_currentAgent);
}

void AgentiPage::runAgent(int idx) {
    auto roleList = roles();
    int  roleIdx  = m_roleCombo[idx]->currentIndex();
    if (roleIdx < 0 || roleIdx >= roleList.size()) roleIdx = 0;
    const auto& role = roleList[roleIdx];

    /*
     * Ogni agente può avere un modello diverso: setBackend cambia solo
     * il modello, non il backend né l'host/porta. Usiamo m_ai->backend()
     * invece di AiClient::Ollama per non forzare il ritorno a Ollama quando
     * l'utente ha scelto llama-server (bug risolto 2026-03-16).
     */
    QString selectedModel = m_modelCombo[idx]->currentData().toString();
    if (selectedModel.isEmpty()) selectedModel = m_modelCombo[idx]->currentText();
    if (!selectedModel.isEmpty() && selectedModel != "(nessun modello)")
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), selectedModel);

    QString ts = QTime::currentTime().toString("HH:mm:ss");
    m_log->append(QString("\n%1  [Agente %2 — %3]  \xf0\x9f\xa4\x96 %4  \xf0\x9f\x95\x90 %5\n")
                  .arg(role.icon).arg(idx + 1).arg(role.name).arg(selectedModel).arg(ts));

    /* Costruisce prompt con contesto agenti precedenti */
    QString userPrompt = m_taskOriginal;

    /* Inietta contesto RAG (file/cartelle trascinate dall'utente) */
    if (m_ragWidget[idx] && m_ragWidget[idx]->hasContext())
        userPrompt += m_ragWidget[idx]->ragContext();
    if (!m_agentOutputs.isEmpty()) {
        userPrompt += "\n\n\xe2\x80\x94\xe2\x80\x94 Output agenti precedenti \xe2\x80\x94\xe2\x80\x94\n";
        for (int i = 0; i < m_agentOutputs.size(); i++) {
            int prevRole = m_roleCombo[i]->currentIndex();
            QString prevName = (prevRole >= 0 && prevRole < roleList.size())
                               ? roleList[prevRole].name : "Agente";
            userPrompt += QString("\n[Agente %1 — %2]:\n%3\n")
                          .arg(i + 1).arg(prevName).arg(m_agentOutputs[i]);
        }
        userPrompt += "\n\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\xe2\x80\x94\n";
        userPrompt += QString("Ora esegui il tuo ruolo di %1.").arg(role.name);
    }

    m_agentOutputs.append("");
    m_ai->chat(role.sysPrompt, userPrompt);
}

/* ══════════════════════════════════════════════════════════════
   Motore Byzantino
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runByzantine() {
    QString fact = m_input->toPlainText().trimmed();
    if (fact.isEmpty()) { m_log->append("\xe2\x9a\xa0  Inserisci una domanda."); return; }

    /* Guardia matematica locale */
    {
        QElapsedTimer tmr; tmr.start();
        QString ris = guardiaMath(fact);
        double ms = tmr.nsecsElapsed() / 1e6;
        if (!ris.isEmpty()) {
            m_log->clear();
            m_log->append("\xe2\x9c\x94  <b>Risposta locale (0 token):</b>");
            m_log->append("<b>" + ris + "</b>");
            m_log->append(QString("\n\xe2\x84\xb9  Calcolo locale \xe2\x80\x94 tempo: <b>%1 ms</b> (%2 s) \xe2\x80\x94 nessun agente avviato.")
                          .arg(ms, 0, 'f', 3).arg(ms / 1000.0, 0, 'f', 6));
            return;
        }
    }

    m_byzStep = 0; m_byzA = m_byzC = "";
    m_taskOriginal = fact;
    m_opMode = OpMode::Byzantine;
    emit pipelineStarted();

    m_log->clear();
    m_log->append(QString(
        "\xf0\x9f\x94\xae  Motore Byzantino \xe2\x80\x94 verifica a 4 agenti\n"
        "\xe2\x9d\x93  Domanda: %1\n").arg(fact));
    m_log->append(QString(43, QChar(0x2500)));
    m_log->append("\xf0\x9f\x85\x90  [Agente A \xe2\x80\x94 Originale]\n");

    m_btnRun->setText("\xe2\x8f\xb9  Stop");
    m_waitLbl->setVisible(true);

    m_ai->chat(
        "Sei l'Agente A del Motore Byzantino di Prismalux. "
        "Fornisci una risposta diretta e ben argomentata. "
        "Rispondi SEMPRE e SOLO in italiano.", fact);
}

/* ══════════════════════════════════════════════════════════════
   Matematico Teorico — 4 agenti: Enunciatore → Esploratore →
   Dimostratore → Sintetizzatore
   m_byzStep riutilizzato come indice fase (0..3)
   m_byzA = output Enunciatore, m_byzC = output Dimostratore
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runMathTheory() {
    QString problem = m_input->toPlainText().trimmed();
    if (problem.isEmpty()) { m_log->append("\xe2\x9a\xa0  Inserisci un problema o una domanda matematica."); return; }

    m_byzStep = 0; m_byzA = m_byzC = "";
    m_taskOriginal = problem;
    m_opMode = OpMode::MathTheory;
    emit pipelineStarted();

    m_log->clear();
    m_log->append(QString(
        "\xf0\x9f\xa7\xae  Matematico Teorico \xe2\x80\x94 esplorazione a 4 agenti\n"
        "\xe2\x9d\x93  Problema: %1\n").arg(problem));
    m_log->append(QString(43, QChar(0x2500)));
    m_log->append("\xf0\x9f\x85\xb0  [Agente 1 \xe2\x80\x94 Enunciatore]\n");

    m_btnRun->setText("\xe2\x8f\xb9  Stop");
    m_waitLbl->setVisible(true);

    m_ai->chat(
        "Sei l'Enunciatore matematico. "
        "Riformula il problema in forma rigorosa: definizioni, dominio, ipotesi. "
        "Sii conciso (max 150 parole). Rispondi SOLO in italiano.", problem);
}

/* ══════════════════════════════════════════════════════════════
   Slot segnali AI
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onToken(const QString& t) {
    if (m_opMode == OpMode::AutoAssign) {
        m_autoBuffer += t;
        return;
    }
    m_waitLbl->setVisible(false);  /* nasconde ⏳ al primo token ricevuto */
    QTextCursor cursor(m_log->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(t);
    m_log->ensureCursorVisible();

    if (m_opMode == OpMode::Pipeline && !m_agentOutputs.isEmpty()) {
        m_agentOutputs.last() += t;

        /* Fill progressivo: ogni token avanza la barra verso la quota dell'agente corrente.
           Formula: start_pct + share * (1 - e^(-tokens/150))
           Arriva a ~86% della quota in ~300 token, mai al 100% (quello lo fa onFinished). */
        m_tokenCount++;
        int total = 0, done = 0;
        for (int i = 0; i < MAX_AGENTS; i++) if (m_enabledChk[i]->isChecked()) total++;
        for (int i = 0; i < m_currentAgent; i++) if (m_enabledChk[i]->isChecked()) done++;
        if (total > 0) {
            double share     = 100.0 / total;
            double startPct  = done * share;
            double fill      = share * (1.0 - std::exp(-m_tokenCount / 150.0)) * 0.92;
            int    barVal    = static_cast<int>(startPct + fill);
            m_progress->setValue(qBound(0, barVal, 99));
        }
    }
    else if (m_opMode == OpMode::Byzantine) {
        if      (m_byzStep == 0) m_byzA += t;
        else if (m_byzStep == 2) m_byzC += t;
    } else if (m_opMode == OpMode::MathTheory) {
        if      (m_byzStep == 0) m_byzA += t;   // Enunciatore
        else if (m_byzStep == 2) m_byzC += t;   // Dimostratore
    }
}

void AgentiPage::onFinished(const QString& /*full*/) {
    m_waitLbl->setVisible(false);  /* nasconde ⏳ quando il modello ha finito */
    if (m_opMode == OpMode::AutoAssign) {
        m_opMode = OpMode::Idle;
        parseAutoAssign(m_autoBuffer);
        return;
    }

    if (m_opMode == OpMode::Pipeline) {
        /* Spunta verde sull'agente appena completato */
        if (m_currentAgent < MAX_AGENTS)
            m_enabledChk[m_currentAgent]->setStyleSheet(
                "QCheckBox { color: #4caf50; font-weight: bold; }"
                "QCheckBox::indicator:checked { background-color: #4caf50; border: 2px solid #388e3c; border-radius: 3px; }");

        m_currentAgent++;

        /* Ripristina barra determinata con percentuale aggiornata */
        int total = 0, done = 0;
        for (int i = 0; i < MAX_AGENTS; i++) if (m_enabledChk[i]->isChecked()) total++;
        for (int i = 0; i < m_currentAgent; i++) if (m_enabledChk[i]->isChecked()) done++;
        int pct = (total > 0) ? qMin(done * 100 / total, 99) : 0;
        m_progress->setRange(0, 100);
        m_progress->setTextVisible(true);
        m_progress->setValue(pct);
        m_progress->setFormat(QString("\xe2\x9c\x85  Agente %1/%2 completato  (%3%)")
            .arg(done).arg(qMin(total, m_maxShots)).arg(pct));

        m_log->append(QString("\n%1 Agente %2 completato  \xf0\x9f\x95\x90 %3 %1")
                      .arg(QString(6, QChar(0x2500)))
                      .arg(m_currentAgent)
                      .arg(QTime::currentTime().toString("HH:mm:ss")));
        /* Snapshot parziale nel DB — preserva lo stile HTML anche a metà pipeline */
        emit agentStepCompleted("Pipeline", m_ai->model(), m_taskOriginal, m_log->toHtml());
        advancePipeline();
        return;
    }

    /* Matematico Teorico */
    if (m_opMode == OpMode::MathTheory) {
        m_byzStep++;
        static const char* mathLabels[] = {
            "\xf0\x9f\x94\xad  [Agente 2 \xe2\x80\x94 Esploratore]\n",
            "\xf0\x9f\x93\x90  [Agente 3 \xe2\x80\x94 Dimostratore]\n",
            "\xe2\x9c\xa8  [Agente 4 \xe2\x80\x94 Sintetizzatore]\n",
        };
        if (m_byzStep <= 3) {
            m_log->append(QString("\n") + QString(43, QChar(0x2500)));
            m_log->append(mathLabels[m_byzStep - 1]);
        }
        QString ctx;
        switch (m_byzStep) {
            case 1:
                ctx = QString("Problema originale: %1\n\nFormulazione rigorosa (Enunciatore):\n%2")
                      .arg(m_taskOriginal, m_byzA);
                m_ai->chat(
                    "Sei l'Esploratore matematico. "
                    "Individua teoremi o metodi applicabili e 1-2 connessioni interessanti. "
                    "Sii conciso (max 150 parole). Rispondi SOLO in italiano.", ctx);
                break;
            case 2:
                ctx = QString("Problema: %1\n\nFormulazione rigorosa:\n%2")
                      .arg(m_taskOriginal, m_byzA);
                m_ai->chat(
                    "Sei il Dimostratore matematico. "
                    "Fornisci la soluzione o dimostrazione in modo diretto e passo per passo. "
                    "Sii conciso (max 200 parole). Rispondi SOLO in italiano.", ctx);
                break;
            case 3:
                /* Passa solo la dimostrazione per tenere il contesto breve */
                ctx = QString("Problema: %1\n\nSoluzione (Dimostratore):\n%2")
                      .arg(m_taskOriginal, m_byzC);
                m_ai->chat(
                    "Sei il Sintetizzatore matematico. "
                    "Riassumi il risultato finale in modo chiaro ed evidenzia eventuali "
                    "generalizzazioni utili. Sii conciso (max 150 parole). Rispondi SOLO in italiano.", ctx);
                break;
            default:
                m_log->append("\n\n\xe2\x9c\x85  Esplorazione matematica completata.");
                m_log->append("\xf0\x9f\xa7\xae  La teoria e' rivelata. Ogni teorema e' una finestra sull'infinito.");
                m_btnRun->setText("\xe2\x96\xb6  Avvia");
                m_opMode = OpMode::Idle;
                emit chatCompleted("MathTheory",
                                   m_ai->model(),
                                   m_taskOriginal,
                                   m_log->toHtml());
                break;
        }
        return;
    }

    /* Byzantino */
    m_byzStep++;
    static const char* labels[] = {
        "\xf0\x9f\x85\xb1  [Agente B \xe2\x80\x94 Avvocato del Diavolo]\n",
        "\xf0\x9f\x85\x92  [Agente C \xe2\x80\x94 Gemello Indipendente]\n",
        "\xf0\x9f\x85\x93  [Agente D \xe2\x80\x94 Giudice]\n",
    };

    if (m_byzStep <= 3) {
        m_log->append(QString("\n") + QString(43, QChar(0x2500)));
        m_log->append(labels[m_byzStep - 1]);
    }

    QString userMsg;
    switch (m_byzStep) {
        case 1:
            userMsg = QString("Domanda: %1\n\nRisposta A:\n%2").arg(m_taskOriginal, m_byzA);
            m_ai->chat(
                "Sei l'Agente B del Motore Byzantino, l'Avvocato del Diavolo. "
                "Cerca ATTIVAMENTE errori e contraddizioni nella risposta A. "
                "Rispondi SEMPRE e SOLO in italiano.", userMsg);
            break;
        case 2:
            m_ai->chat(
                "Sei l'Agente C del Motore Byzantino, il Gemello Indipendente. "
                "Rispondi alla domanda originale da un angolo diverso, "
                "senza leggere la risposta A. Rispondi SEMPRE e SOLO in italiano.",
                m_taskOriginal);
            break;
        case 3:
            userMsg = QString("Domanda: %1\n\nRisposta A:\n%2\n\nRisposta C:\n%3")
                      .arg(m_taskOriginal, m_byzA, m_byzC);
            m_ai->chat(
                "Sei l'Agente D del Motore Byzantino, il Giudice. "
                "Se A e C concordano e B non trova errori validi: conferma. "
                "Altrimenti segnala l'incertezza. Produci il verdetto finale. "
                "Rispondi SEMPRE e SOLO in italiano.", userMsg);
            break;
        default:
            m_log->append("\n\n\xe2\x9c\x85  Verifica Byzantina completata.");
            m_log->append("\xe2\x9c\xa8 Verit\xc3\xa0 rivelata. Bevi la conoscenza.");
            m_btnRun->setText("\xe2\x96\xb6  Avvia");
            m_opMode = OpMode::Idle;
            emit chatCompleted("Byzantino",
                               m_ai->model(),
                               m_taskOriginal,
                               m_log->toHtml());
            break;
    }
}

void AgentiPage::onError(const QString& msg) {
    m_waitLbl->setVisible(false);  /* nasconde ⏳ in caso di errore */
    if (m_opMode == OpMode::AutoAssign) {
        m_autoLbl->setText(QString("\xe2\x9d\x8c  Errore auto-assegnazione: %1").arg(msg));
        m_btnAuto->setEnabled(true);
        m_opMode = OpMode::Idle;
        return;
    }
    m_log->append(QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    /* Suggerimento contestuale: indica il server corretto in base al backend attivo */
    const QString hint = (m_ai->backend() == AiClient::Ollama)
        ? "\xf0\x9f\x8c\xab  La nebbia copre la verit\xc3\xa0. Controlla che Ollama sia in esecuzione: ollama serve"
        : "\xf0\x9f\x8c\xab  La nebbia copre la verit\xc3\xa0. Controlla che llama-server sia avviato sulla porta corretta.";
    m_log->append(hint);
    m_btnRun->setText("\xe2\x96\xb6  Avvia");
    m_opMode = OpMode::Idle;
}

/* ══════════════════════════════════════════════════════════════
   recallChat — richiama una chat salvata nella cronologia
   Riempie m_input con il task originale e m_log con la risposta.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::recallChat(const ChatEntry& entry)
{
    m_input->setPlainText(entry.task);
    m_log->setHtml(entry.response);
    /* Sposta il cursore alla fine del log */
    auto cursor = m_log->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_log->setTextCursor(cursor);
}

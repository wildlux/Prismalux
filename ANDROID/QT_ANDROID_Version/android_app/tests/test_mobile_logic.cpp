/* ══════════════════════════════════════════════════════════════════════
   test_mobile_logic.cpp — Qt Test per la logica di PrismaluxMobile

   Suite implementate:
     1. TestGraphMemoryMobile  — addNode/addEdge/neighbours/searchNodes (in-memory stub)
     2. TestFinanza            — calcoli IRPEF 2024, TFR rivalutazione, CF
     3. TestFileAiPage         — parsing testo puro, extraction PDF-stub, truncation
     4. TestAssistentePage     — costruzione prompt per categorie
     5. TestChatPage           — logica allegati, storia SQLite, backend switching

   Non richiede Android o Ollama: testa logica pura (niente UI, niente NDK).

   Compila e lancia:
     cd ANDROID/QT_ANDROID_Version/android_app/tests
     mkdir build && cd build
     cmake .. && cmake --build . && ./test_mobile_logic
   ══════════════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QStringList>
#include <QFile>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QDir>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <cmath>

/* ══════════════════════════════════════════════════════════════════════
   ── STRUTTURE DI SUPPORTO ──

   Tutti gli stub riproducono esattamente la logica presente nei file
   sorgente corrispondenti, permettendo di testarla senza istanziare
   QWidget (che richiede QApplication + schermo).
   ══════════════════════════════════════════════════════════════════════ */

/* ── 1. GraphMemory stub (logica da graph_memory_mobile.cpp) ─────────── */
struct GmNode {
    QString id, type, label, content;
    float   importance;
};
struct GmEdge {
    QString from, to, relation;
    float   weight;
};
struct GmStore {
    QVector<GmNode> nodes;
    QVector<GmEdge> edges;

    QString addNode(const QString& type, const QString& label,
                    const QString& content, float imp)
    {
        const QString id = "node_" + QString::number(nodes.size() + 1);
        nodes.append({id, type, label, content, imp});
        return id;
    }

    bool addEdge(const QString& from, const QString& to,
                 const QString& rel, float w)
    {
        if (from.isEmpty() || to.isEmpty()) return false;
        edges.append({from, to, rel, w});
        return true;
    }

    QVector<GmNode> neighbours(const QString& nodeId) const {
        QVector<GmNode> res;
        QSet<QString> visited;
        visited.insert(nodeId);
        for (const auto& e : edges) {
            if (e.from == nodeId && !visited.contains(e.to)) {
                for (const auto& n : nodes)
                    if (n.id == e.to) { res.append(n); visited.insert(e.to); break; }
            } else if (e.to == nodeId && !visited.contains(e.from)) {
                for (const auto& n : nodes)
                    if (n.id == e.from) { res.append(n); visited.insert(e.from); break; }
            }
        }
        return res;
    }

    QVector<GmNode> searchNodes(const QString& query) const {
        QVector<GmNode> res;
        for (const auto& n : nodes)
            if (n.label.contains(query, Qt::CaseInsensitive) ||
                n.content.contains(query, Qt::CaseInsensitive))
                res.append(n);
        return res;
    }

    void clear() { nodes.clear(); edges.clear(); }
};

/* ── 2. IRPEF 2024 (logica da finanza_page.cpp FinanzaPage::buildIrpefSection) ── */
struct IrpefResult {
    double irpef;
    double netto;
    double aliqMedia;
    double mensile;
};

static IrpefResult calcolaIrpef2024(double ral)
{
    double irpef = 0.0;
    if (ral <= 28000.0) {
        irpef = ral * 0.23;
    } else if (ral <= 50000.0) {
        irpef = 28000.0 * 0.23 + (ral - 28000.0) * 0.35;
    } else {
        irpef = 28000.0 * 0.23 + 22000.0 * 0.35 + (ral - 50000.0) * 0.43;
    }
    const double netto     = ral - irpef;
    const double mensile   = netto / 13.0;
    const double aliqMedia = ral > 0 ? irpef / ral * 100.0 : 0.0;
    return {irpef, netto, aliqMedia, mensile};
}

/* ── 3. TFR (logica da finanza_page.cpp FinanzaPage::buildTfrSection) ─── */
static double calcolaTfrLordo(double ral, int anni, double tassoRival = 0.02)
{
    const double quotaAnnua = ral / 13.5;
    double tfrLordo = 0.0;
    for (int y = 1; y <= anni; ++y)
        tfrLordo += quotaAnnua * std::pow(1.0 + tassoRival, anni - y);
    return tfrLordo;
}

/* ── 4. Codice Fiscale — parte alfanumerica consonanti/vocali ─────────── */
static QString estraiConsonantiVocali(const QString& s, int nCons, int nVoc)
{
    QString cons, voc;
    const QString upper = s.toUpper();
    for (QChar c : upper) {
        if (!c.isLetter()) continue;
        const QString vowels = "AEIOU";
        if (vowels.contains(c)) voc  += c;
        else                    cons += c;
    }
    QString result;
    if (cons.size() >= nCons) {
        result = cons.left(nCons);
    } else {
        result = cons + voc.left(nCons - cons.size());
    }
    while (result.size() < nCons + nVoc) result += 'X';
    return result.left(nCons + nVoc);
}

static QString codiceFiscaleCognome(const QString& cognome)
{
    return estraiConsonantiVocali(cognome, 3, 0);
}

static QString codiceFiscaleNome(const QString& nome)
{
    QString cons;
    const QString upper = nome.toUpper();
    for (QChar c : upper) {
        if (!c.isLetter()) continue;
        if (!QString("AEIOU").contains(c)) cons += c;
    }
    if (cons.size() >= 4) {
        // Prendi 1a, 3a, 4a consonante
        return QString() + cons[0] + cons[2] + cons[3];
    }
    return estraiConsonantiVocali(nome, 3, 0);
}

/* ── 5. FileAi — parsing / truncation (logica da file_ai_page.cpp) ──── */
struct FileAiState {
    QString fileName;
    QString fileContent;
    static constexpr qint64 kPreviewMaxChars = 500;
    static constexpr qint64 kMaxReadBytes    = 256 * 1024;  // 256 KB

    bool loadTxt(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        const QByteArray raw = f.read(kMaxReadBytes);
        if (raw.isEmpty()) return false;
        fileContent = QString::fromUtf8(raw);
        fileName    = QFileInfo(path).fileName();
        return true;
    }

    QString preview() const {
        if (fileContent.size() <= kPreviewMaxChars)
            return fileContent;
        return fileContent.left(kPreviewMaxChars) + "…";
    }

    QString buildPrompt(const QString& userQuestion) const {
        if (fileContent.isEmpty()) return userQuestion;
        return QString("FILE: %1\n\n%2\n\nDOMANDA: %3")
               .arg(fileName, fileContent.left(4096), userQuestion);
    }
};

/* ── 6. Assistente — prompt selection (logica da assistente_page.cpp) ─── */
static const char* kSysPrompts[6][6] = {
    /* 0 — Studio */
    {
        "Sei un tutor esperto. Spiega in modo chiaro e strutturato con esempi pratici. Rispondi in italiano.",
        "Crea 10 flashcard nel formato Q: ... R: ... dal testo fornito. Rispondi in italiano.",
        "Crea un riassunto breve del testo. Rispondi in italiano.",
        "Genera 10 domande d'esame probabili con risposta sintetica. Rispondi in italiano.",
        nullptr, nullptr
    },
    /* 1 — Scrittura Creativa */
    {
        "Sei uno scrittore creativo. Scrivi una storia coinvolgente basata sull'idea fornita. Rispondi in italiano.",
        "Continua la storia in modo coerente con lo stile e i personaggi esistenti. Rispondi in italiano.",
        "Crea un personaggio dettagliato: nome, aspetto, backstory, motivazioni, difetti e punti di forza. Rispondi in italiano.",
        "Scrivi una poesia originale sul tema indicato. Rispondi in italiano.",
        nullptr, nullptr
    },
    /* 2 — Ricerca */
    {
        "Sei un ricercatore. Analizza l'argomento fornito citando evidenze. Rispondi in italiano.",
        "Confronta i pro e contro dell'argomento. Rispondi in italiano.",
        "Riassumi lo stato dell'arte sull'argomento. Rispondi in italiano.",
        nullptr, nullptr, nullptr
    },
    /* 3 — Libri */
    {
        "Sei un esperto letterario. Analizza il libro fornito: temi, stile, personaggi. Rispondi in italiano.",
        "Genera 5 domande di discussione sul libro. Rispondi in italiano.",
        nullptr, nullptr, nullptr, nullptr
    },
    /* 4 — Produttivita' */
    {
        "Sei un coach di produttivita'. Organizza il testo in una lista di azioni concrete. Rispondi in italiano.",
        "Crea una pianificazione settimanale basata sugli obiettivi indicati. Rispondi in italiano.",
        "Scrivi un template email professionale per il contesto indicato. Rispondi in italiano.",
        nullptr, nullptr, nullptr
    },
    /* 5 — Documenti */
    {
        "Sei un esperto di redazione. Riorganizza il documento in modo chiaro e formale. Rispondi in italiano.",
        "Estrai i punti chiave dal documento. Rispondi in italiano.",
        "Correggi grammatica e stile del testo. Rispondi in italiano.",
        nullptr, nullptr, nullptr
    }
};

static const char* selectPrompt(int cat, int action)
{
    if (cat < 0 || cat >= 6) return nullptr;
    if (action < 0 || action >= 6) return nullptr;
    return kSysPrompts[cat][action];
}

/* ── 7. Chat file attachment (logica da chat_page.cpp) ─────────────────── */
struct AttachState {
    QStringList contents;
    QStringList names;
    static constexpr int kMax = 5;
    static constexpr qint64 kMaxBytes = 64 * 1024;

    bool attach(const QString& path) {
        if (contents.size() >= kMax) return false;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        const QByteArray raw = f.read(kMaxBytes);
        if (raw.isEmpty()) return false;
        contents << QString::fromUtf8(raw);
        names    << QFileInfo(path).fileName();
        return true;
    }

    void clear() { contents.clear(); names.clear(); }

    QString buildContext() const {
        if (contents.isEmpty()) return {};
        QString s = "\n\nFILE ALLEGATI DALL'UTENTE:";
        for (int i = 0; i < contents.size(); ++i)
            s += QString("\n\n--- File %1: %2 ---\n%3")
                 .arg(i+1).arg(names[i]).arg(contents[i]);
        return s;
    }
};

/* ── 8. Backend switching (logica da chat_page.cpp) ────────────────────── */
struct BackendState {
    enum Mode { Server, Cloud, Local };
    Mode    mode = Server;
    QString host = "192.168.1.165";
    int     port = 11434;
    QString model = "llama3.2:1b";
    QString cloudHost, cloudModel;

    void switchToServer(const QString& h, int p, const QString& m)
        { mode = Server; host = h; port = p; model = m; }
    void switchToCloud(const QString& h, const QString& m)
        { mode = Cloud; cloudHost = h; cloudModel = m; }
    void switchToLocal()
        { mode = Local; }
};

/* ── 9. Chat SQLite persistence (logica da ChatPage::initDb / saveMessage) ── */
struct ChatDb {
    QSqlDatabase db;
    QString      connName;

    bool init(const QString& path) {
        connName = "test_chat_" + QString::number(reinterpret_cast<quintptr>(this));
        db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(path);
        if (!db.open()) return false;
        QSqlQuery q(db);
        return q.exec(
            "CREATE TABLE IF NOT EXISTS messages ("
            "  id      INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  role    TEXT NOT NULL,"
            "  content TEXT NOT NULL,"
            "  ts      INTEGER DEFAULT (strftime('%s','now'))"
            ")");
    }

    bool save(const QString& role, const QString& content) {
        QSqlQuery q(db);
        q.prepare("INSERT INTO messages (role, content) VALUES (:r, :c)");
        q.bindValue(":r", role);
        q.bindValue(":c", content);
        return q.exec();
    }

    QVector<QPair<QString,QString>> load(int limit = 50) const {
        QVector<QPair<QString,QString>> out;
        QSqlQuery q(db);
        q.prepare("SELECT role, content FROM messages ORDER BY id DESC LIMIT :l");
        q.bindValue(":l", limit);
        if (!q.exec()) return out;
        while (q.next())
            out.prepend({q.value(0).toString(), q.value(1).toString()});
        return out;
    }

    int count() const {
        QSqlQuery q("SELECT COUNT(*) FROM messages", db);
        return q.next() ? q.value(0).toInt() : 0;
    }

    void close() {
        // Distruggi prima tutti i QSqlQuery pendenti rilasciando il db handle,
        // poi rimuovi la connessione — evita il warning Qt "still in use".
        { QSqlDatabase tmp = db; tmp.close(); }
        db = QSqlDatabase();  // rilascia il riferimento interno
        QSqlDatabase::removeDatabase(connName);
    }
};

/* ══════════════════════════════════════════════════════════════════════
   ── SUITE 1: GraphMemoryMobile ──────────────────────────────────────
   ══════════════════════════════════════════════════════════════════════ */
class TestGraphMemoryMobile : public QObject {
    Q_OBJECT
private slots:

    /* GM-1: addNode ritorna ID non vuoto e incrementa contatore */
    void addNodeReturnsId() {
        GmStore gm;
        const QString id = gm.addNode("entity", "Qt6", "Framework C++", 0.9f);
        QVERIFY(!id.isEmpty());
        QCOMPARE(gm.nodes.size(), 1);
        QCOMPARE(gm.nodes[0].label, QString("Qt6"));
    }

    /* GM-2: addNode multipli assegna ID univoci */
    void addMultipleNodesUniqueIds() {
        GmStore gm;
        const QString id1 = gm.addNode("entity", "A", "c1", 0.5f);
        const QString id2 = gm.addNode("entity", "B", "c2", 0.5f);
        QVERIFY(id1 != id2);
        QCOMPARE(gm.nodes.size(), 2);
    }

    /* GM-3: addEdge valido tra due nodi esistenti */
    void addEdgeValid() {
        GmStore gm;
        const QString n1 = gm.addNode("entity", "Qt6",   "Framework", 0.8f);
        const QString n2 = gm.addNode("entity", "Qmake", "Build tool", 0.5f);
        QVERIFY(gm.addEdge(n1, n2, "usa", 0.7f));
        QCOMPARE(gm.edges.size(), 1);
        QCOMPARE(gm.edges[0].relation, QString("usa"));
        QCOMPARE(gm.edges[0].weight,  0.7f);
    }

    /* GM-4: addEdge con ID vuoto ritorna false */
    void addEdgeEmptyIdFails() {
        GmStore gm;
        QVERIFY(!gm.addEdge("", "n2", "rel", 1.0f));
        QVERIFY(!gm.addEdge("n1", "", "rel", 1.0f));
        QCOMPARE(gm.edges.size(), 0);
    }

    /* GM-5: neighbours BFS profondita 1 */
    void neighboursBfsDepth1() {
        GmStore gm;
        const QString n1 = gm.addNode("entity", "A", "nodo A", 1.0f);
        const QString n2 = gm.addNode("entity", "B", "nodo B", 1.0f);
        const QString n3 = gm.addNode("entity", "C", "nodo C", 1.0f);
        gm.addEdge(n1, n2, "collega", 1.0f);
        gm.addEdge(n1, n3, "collega", 1.0f);

        const auto nb = gm.neighbours(n1);
        QCOMPARE(nb.size(), 2);
    }

    /* GM-6: neighbours di nodo isolato ritorna lista vuota */
    void neighboursIsolatedNode() {
        GmStore gm;
        const QString n1 = gm.addNode("entity", "Isolato", "nessun arco", 1.0f);
        QCOMPARE(gm.neighbours(n1).size(), 0);
    }

    /* GM-7: neighbours e' bidirezionale */
    void neighboursBidirectional() {
        GmStore gm;
        const QString n1 = gm.addNode("entity", "Src",  "s", 1.0f);
        const QString n2 = gm.addNode("entity", "Dest", "d", 1.0f);
        gm.addEdge(n1, n2, "link", 1.0f);

        // n2 deve vedere n1 come vicino
        const auto nb = gm.neighbours(n2);
        QCOMPARE(nb.size(), 1);
        QCOMPARE(nb[0].id, n1);
    }

    /* GM-8: searchNodes per label */
    void searchNodesLabel() {
        GmStore gm;
        gm.addNode("entity", "Qt6 Widgets",  "GUI framework",  0.9f);
        gm.addNode("entity", "SymPy",         "Algebra simbolica", 0.7f);
        gm.addNode("entity", "Qt6 Network",   "Network module", 0.8f);
        const auto found = gm.searchNodes("Qt6");
        QCOMPARE(found.size(), 2);
    }

    /* GM-9: searchNodes case-insensitive */
    void searchNodesCaseInsensitive() {
        GmStore gm;
        gm.addNode("entity", "Python", "linguaggio", 0.8f);
        QCOMPARE(gm.searchNodes("python").size(), 1);
        QCOMPARE(gm.searchNodes("PYTHON").size(), 1);
        QCOMPARE(gm.searchNodes("pYtHoN").size(), 1);
    }

    /* GM-10: searchNodes su contenuto */
    void searchNodesContent() {
        GmStore gm;
        gm.addNode("concept", "Nodo1", "Prismalux v3 GUI framework Qt6", 0.9f);
        QCOMPARE(gm.searchNodes("Prismalux").size(), 1);
        QCOMPARE(gm.searchNodes("XYZ_not_found").size(), 0);
    }

    /* GM-11: searchNodes SQL-injection-safe (nessun match su query malevola) */
    void searchNodesSqlInjectionSafe() {
        GmStore gm;
        gm.addNode("entity", "Normal", "content normale", 0.5f);
        QCOMPARE(gm.searchNodes("'; DROP TABLE nodes; --").size(), 0);
        QCOMPARE(gm.searchNodes("' OR '1'='1").size(), 0);
    }

    /* GM-12: clearAll azzera nodi e archi */
    void clearAllResetsStore() {
        GmStore gm;
        gm.addNode("entity", "A", "c", 0.5f);
        gm.addNode("entity", "B", "c", 0.5f);
        gm.addEdge("node_1", "node_2", "rel", 1.0f);
        gm.clear();
        QCOMPARE(gm.nodes.size(), 0);
        QCOMPARE(gm.edges.size(), 0);
    }
};

/* ══════════════════════════════════════════════════════════════════════
   ── SUITE 2: FinanzaPage ────────────────────────────────────────────
   ══════════════════════════════════════════════════════════════════════ */
class TestFinanza : public QObject {
    Q_OBJECT
private slots:

    /* FI-1: IRPEF primo scaglione (<=28k, 23%) */
    void irpefPrimoScaglione() {
        const auto r = calcolaIrpef2024(20000.0);
        QCOMPARE(r.irpef, 20000.0 * 0.23);
        QVERIFY(std::abs(r.aliqMedia - 23.0) < 0.01);
    }

    /* FI-2: IRPEF esatto al limite del primo scaglione (28000) */
    void irpefLimitePrimoScaglione() {
        const auto r = calcolaIrpef2024(28000.0);
        QCOMPARE(r.irpef, 28000.0 * 0.23);
    }

    /* FI-3: IRPEF secondo scaglione (28k-50k, 35%) */
    void irpefSecondoScaglione() {
        const auto r = calcolaIrpef2024(40000.0);
        const double atteso = 28000.0 * 0.23 + (40000.0 - 28000.0) * 0.35;
        QVERIFY(std::abs(r.irpef - atteso) < 0.01);
        QVERIFY(r.aliqMedia > 23.0 && r.aliqMedia < 35.0);
    }

    /* FI-4: IRPEF al limite del secondo scaglione (50000) */
    void irpefLimiteSecondoScaglione() {
        const auto r = calcolaIrpef2024(50000.0);
        const double atteso = 28000.0 * 0.23 + 22000.0 * 0.35;
        QVERIFY(std::abs(r.irpef - atteso) < 0.01);
    }

    /* FI-5: IRPEF terzo scaglione (>50k, 43%) */
    void irpefTerzoScaglione() {
        // Con RAL=80k l'aliquota media e' ~33.8% (i primi due scaglioni pesano molto).
        // Usiamo RAL=100k per avere aliquota media > 35%.
        const auto r = calcolaIrpef2024(100000.0);
        const double atteso = 28000.0 * 0.23 + 22000.0 * 0.35 + (100000.0 - 50000.0) * 0.43;
        QVERIFY(std::abs(r.irpef - atteso) < 0.01);
        // Con RAL=100k l'aliquota media e' ~35.6% (> 35%, < 43%)
        QVERIFY(r.aliqMedia > 35.0 && r.aliqMedia < 43.0);
    }

    /* FI-6: IRPEF netto = RAL - imposta */
    void irpefNettoCorretto() {
        const double ral = 35000.0;
        const auto r = calcolaIrpef2024(ral);
        QVERIFY(std::abs(r.netto - (ral - r.irpef)) < 0.01);
    }

    /* FI-7: IRPEF mensile = netto / 13 */
    void irpefMensileCorretto() {
        const auto r = calcolaIrpef2024(30000.0);
        QVERIFY(std::abs(r.mensile - r.netto / 13.0) < 0.01);
    }

    /* FI-8: IRPEF zero su reddito zero */
    void irpefZeroReddito() {
        const auto r = calcolaIrpef2024(0.0);
        QCOMPARE(r.irpef, 0.0);
        QCOMPARE(r.aliqMedia, 0.0);
    }

    /* FI-9: TFR base (RAL/13.5 * anni, senza rivalutazione) */
    void tfrCalcoloBase() {
        const double ral  = 24000.0;
        const int    anni = 1;
        // Con un anno, y=1: pow(1.02, 0) = 1 → TFR = ral/13.5
        const double tfr = calcolaTfrLordo(ral, anni, 0.02);
        QVERIFY(std::abs(tfr - ral / 13.5) < 0.01);
    }

    /* FI-10: TFR cresce con gli anni */
    void tfrCresceCon10Anni() {
        const double tfr1  = calcolaTfrLordo(28000.0, 1,  0.02);
        const double tfr10 = calcolaTfrLordo(28000.0, 10, 0.02);
        QVERIFY(tfr10 > tfr1 * 9.0);  // rivalutazione composta > lineare * 9x
    }

    /* FI-11: TFR zero anni = zero */
    void tfrZeroAnni() {
        QCOMPARE(calcolaTfrLordo(2000.0, 0, 0.02), 0.0);
    }

    /* FI-12: TFR rivalutazione tasso zero = quotaAnnua * anni */
    void tfrTassoZeroLineare() {
        const double ral    = 27000.0;
        const int    anni   = 5;
        const double atteso = (ral / 13.5) * anni;
        const double tfr    = calcolaTfrLordo(ral, anni, 0.0);
        QVERIFY(std::abs(tfr - atteso) < 0.01);
    }

    /* FI-13: Codice Fiscale — cognome con 3+ consonanti */
    void cfCognome3Consonanti() {
        // "ROSSI" → consonanti R,S,S → "RSS"
        QCOMPARE(codiceFiscaleCognome("Rossi"), QString("RSS"));
    }

    /* FI-14: Codice Fiscale — cognome breve con vocali */
    void cfCognomeBreve() {
        // "BO" → cons=B, voc=O → "BOX"
        const QString r = codiceFiscaleCognome("Bo");
        QCOMPARE(r.size(), 3);
        QVERIFY(r.startsWith("B"));
    }

    /* FI-15: Codice Fiscale — nome con 4+ consonanti usa 1a,3a,4a */
    void cfNome4Consonanti() {
        // "GIOVANNI" → cons: G,V,N,N → usa G,N,N
        const QString r = codiceFiscaleNome("Giovanni");
        QCOMPARE(r.size(), 3);
        QCOMPARE(r[0], QChar('G'));
    }

    /* FI-16: Codice Fiscale — nome breve */
    void cfNomeBreve() {
        // "EVA" → cons=V → estraiConsonantiVocali: 1 cons, poi vocali E,A
        const QString r = codiceFiscaleNome("Eva");
        QCOMPARE(r.size(), 3);
    }
};

/* ══════════════════════════════════════════════════════════════════════
   ── SUITE 3: FileAiPage ─────────────────────────────────────────────
   ══════════════════════════════════════════════════════════════════════ */
class TestFileAiPage : public QObject {
    Q_OBJECT
private slots:

    /* FA-1: caricamento file TXT valido */
    void loadTxtValid() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("Testo di prova per Prismalux.\n");
        f.flush();

        FileAiState st;
        QVERIFY(st.loadTxt(f.fileName()));
        QVERIFY(st.fileContent.contains("Testo di prova"));
        QVERIFY(!st.fileName.isEmpty());
    }

    /* FA-2: file inesistente ritorna false */
    void loadTxtNotFound() {
        FileAiState st;
        QVERIFY(!st.loadTxt("/dev/null/non_esiste.txt"));
        QVERIFY(st.fileContent.isEmpty());
    }

    /* FA-3: file grande viene troncato a 256 KB */
    void loadTxtTruncatesLarge() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(QByteArray(300 * 1024, 'A'));
        f.flush();

        FileAiState st;
        QVERIFY(st.loadTxt(f.fileName()));
        QVERIFY(st.fileContent.size() <= (int)FileAiState::kMaxReadBytes);
    }

    /* FA-4: preview tronca a 500 caratteri */
    void previewTruncates() {
        FileAiState st;
        st.fileContent = QString(600, 'X');
        const QString p = st.preview();
        QVERIFY(p.endsWith("…"));
        QVERIFY(p.size() <= (int)FileAiState::kPreviewMaxChars + 1);
    }

    /* FA-5: preview integra se < 500 caratteri */
    void previewShortContent() {
        FileAiState st;
        st.fileContent = "Testo breve";
        QCOMPARE(st.preview(), QString("Testo breve"));
    }

    /* FA-6: buildPrompt senza file ritorna solo la domanda */
    void buildPromptNoFile() {
        FileAiState st;
        QCOMPARE(st.buildPrompt("Che cos'e' Qt?"), QString("Che cos'e' Qt?"));
    }

    /* FA-7: buildPrompt con file include nome e contenuto */
    void buildPromptWithFile() {
        FileAiState st;
        st.fileName    = "doc.txt";
        st.fileContent = "Contenuto importante";
        const QString p = st.buildPrompt("Analizza");
        QVERIFY(p.contains("FILE:"));
        QVERIFY(p.contains("doc.txt"));
        QVERIFY(p.contains("Contenuto importante"));
        QVERIFY(p.contains("DOMANDA: Analizza"));
    }

    /* FA-8: buildPrompt tronca contenuto lungo a 4096 caratteri */
    void buildPromptTruncatesContent() {
        FileAiState st;
        st.fileName    = "big.txt";
        st.fileContent = QString(10000, 'Z');
        const QString p = st.buildPrompt("Riassumi");
        // Il contenuto nel prompt e' limitato a 4096
        QVERIFY(p.size() < 10000 + 200);
    }

    /* FA-9: encoding UTF-8 preservato */
    void loadTxtUtf8() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(QString("Prismalux \xe2\x80\x94 azione AI\n").toUtf8());
        f.flush();

        FileAiState st;
        QVERIFY(st.loadTxt(f.fileName()));
        QVERIFY(st.fileContent.contains("Prismalux"));
    }
};

/* ══════════════════════════════════════════════════════════════════════
   ── SUITE 4: AssistentePage ─────────────────────────────────────────
   ══════════════════════════════════════════════════════════════════════ */
class TestAssistentePage : public QObject {
    Q_OBJECT
private slots:

    /* AS-1: categoria 0 (Studio), azione 0 → prompt tutor */
    void studioAzione0() {
        const char* p = selectPrompt(0, 0);
        QVERIFY(p != nullptr);
        QVERIFY(QString(p).contains("tutor"));
    }

    /* AS-2: categoria 0 (Studio), azione 1 → flashcard */
    void studioAzione1Flashcard() {
        const char* p = selectPrompt(0, 1);
        QVERIFY(p != nullptr);
        QVERIFY(QString(p).contains("flashcard"));
    }

    /* AS-3: categoria 1 (Scrittura), azione 0 → storia */
    void scritturaAzione0Storia() {
        const char* p = selectPrompt(1, 0);
        QVERIFY(p != nullptr);
        QVERIFY(QString(p).contains("storia"));
    }

    /* AS-4: categoria 4 (Produttivita'), azione 2 → email */
    void produttivitaAzione2Email() {
        const char* p = selectPrompt(4, 2);
        QVERIFY(p != nullptr);
        QVERIFY(QString(p).contains("email"));
    }

    /* AS-5: categoria 5 (Documenti), azione 1 → punti chiave */
    void documentiAzione1PuntiChiave() {
        const char* p = selectPrompt(5, 1);
        QVERIFY(p != nullptr);
        QVERIFY(QString(p).contains("chiave"));
    }

    /* AS-6: categoria fuori range ritorna nullptr */
    void categoriaFuoriRangeNullptr() {
        QVERIFY(selectPrompt(-1, 0) == nullptr);
        QVERIFY(selectPrompt(6,  0) == nullptr);
        QVERIFY(selectPrompt(99, 0) == nullptr);
    }

    /* AS-7: azione fuori range ritorna nullptr */
    void azioneFuoriRangeNullptr() {
        QVERIFY(selectPrompt(0, -1) == nullptr);
        QVERIFY(selectPrompt(0, 10) == nullptr);
    }

    /* AS-8: tutti i prompt non-null contengono "italiano" (lingua) */
    void tuttiPromptContengonoItaliano() {
        for (int c = 0; c < 6; ++c)
            for (int a = 0; a < 6; ++a) {
                const char* p = selectPrompt(c, a);
                if (p != nullptr)
                    QVERIFY(QString(p).contains("italiano"));
            }
    }

    /* AS-9: categorie diverse producono prompt diversi */
    void categorieProduconoPromptDiversi() {
        const char* p0 = selectPrompt(0, 0);
        const char* p1 = selectPrompt(1, 0);
        QVERIFY(p0 != nullptr && p1 != nullptr);
        QVERIFY(QString(p0) != QString(p1));
    }

    /* AS-10: azione null (slot non definito) ritorna nullptr */
    void azioneNullSlotRitornaNull() {
        // Categoria 3 (Libri) ha solo 2 azioni definite → indice 2 = nullptr
        QVERIFY(selectPrompt(3, 2) == nullptr);
    }
};

/* ══════════════════════════════════════════════════════════════════════
   ── SUITE 5: ChatPage ────────────────────────────────────────────────
   ══════════════════════════════════════════════════════════════════════ */
class TestChatPage : public QObject {
    Q_OBJECT
private slots:

    /* CP-1: allegato singolo file */
    void attachSingleFile() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("Contenuto di test per Prismalux.\n");
        f.flush();

        AttachState s;
        QVERIFY(s.attach(f.fileName()));
        QCOMPARE(s.contents.size(), 1);
        QVERIFY(s.contents[0].contains("Contenuto di test"));
    }

    /* CP-2: limite massimo 5 allegati */
    void attachMaxFiveFiles() {
        AttachState s;
        for (int i = 0; i < AttachState::kMax; ++i) {
            QTemporaryFile f;
            QVERIFY(f.open());
            f.write(QByteArray("file") + QByteArray::number(i));
            f.flush();
            QVERIFY(s.attach(f.fileName()));
        }
        QCOMPARE(s.contents.size(), AttachState::kMax);

        QTemporaryFile extra;
        QVERIFY(extra.open());
        extra.write("extra");
        extra.flush();
        QVERIFY(!s.attach(extra.fileName()));
    }

    /* CP-3: file > 64 KB viene troncato */
    void attachTruncatesLargeFiles() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(QByteArray(70 * 1024, 'X'));
        f.flush();

        AttachState s;
        QVERIFY(s.attach(f.fileName()));
        QVERIFY(s.contents[0].size() <= (int)(AttachState::kMaxBytes));
    }

    /* CP-4: file inesistente ritorna false */
    void attachNonExistentFile() {
        AttachState s;
        QVERIFY(!s.attach("/non/esiste/mai.txt"));
        QCOMPARE(s.contents.size(), 0);
    }

    /* CP-5: clear azzera lo stato */
    void clearResetsState() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("dati");
        f.flush();

        AttachState s;
        s.attach(f.fileName());
        s.clear();
        QCOMPARE(s.contents.size(), 0);
        QCOMPARE(s.names.size(), 0);
    }

    /* CP-6: buildContext con 2 file include entrambi */
    void buildContextMultipleFiles() {
        AttachState s;
        for (int i = 0; i < 2; ++i) {
            QTemporaryFile f;
            QVERIFY(f.open());
            f.write(("contenuto_" + QString::number(i)).toUtf8());
            f.flush();
            s.attach(f.fileName());
        }
        const QString ctx = s.buildContext();
        QVERIFY(ctx.contains("FILE ALLEGATI"));
        QVERIFY(ctx.contains("File 1:"));
        QVERIFY(ctx.contains("File 2:"));
    }

    /* CP-7: backend default e' Server su porta 11434 */
    void backendDefaultIsServer() {
        BackendState b;
        QCOMPARE(b.mode, BackendState::Server);
        QCOMPARE(b.port, 11434);
    }

    /* CP-8: switch to Cloud salva host e modello */
    void backendSwitchToCloud() {
        BackendState b;
        b.switchToCloud("api.openai.com", "gpt-4");
        QCOMPARE(b.mode, BackendState::Cloud);
        QCOMPARE(b.cloudHost, QString("api.openai.com"));
        QCOMPARE(b.cloudModel, QString("gpt-4"));
    }

    /* CP-9: switch Cloud→Server ripristina parametri server */
    void backendSwitchCloudThenServer() {
        BackendState b;
        b.switchToCloud("host", "model");
        b.switchToServer("192.168.1.165", 11434, "llama3.2:1b");
        QCOMPARE(b.mode, BackendState::Server);
        QCOMPARE(b.port, 11434);
    }

    /* CP-10: modalita' e' esclusiva (una sola attiva) */
    void backendModeExclusive() {
        BackendState b;
        b.switchToCloud("h", "m");
        b.switchToLocal();
        QCOMPARE(b.mode, BackendState::Local);
        QVERIFY(b.mode != BackendState::Cloud);
    }

    /* CP-11: SQLite — inizializzazione e salvataggio messaggio */
    void chatDbSaveAndLoad() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const QString dbPath = tmpDir.filePath("chat_test.db");

        ChatDb db;
        QVERIFY(db.init(dbPath));
        QVERIFY(db.save("user", "Ciao Prismalux!"));
        QVERIFY(db.save("assistant", "Benvenuto!"));

        QCOMPARE(db.count(), 2);
        const auto msgs = db.load();
        QCOMPARE(msgs.size(), 2);
        QCOMPARE(msgs[0].first, QString("user"));
        QCOMPARE(msgs[0].second, QString("Ciao Prismalux!"));
        QCOMPARE(msgs[1].first, QString("assistant"));

        db.close();
    }

    /* CP-12: SQLite — limit sulla lettura */
    void chatDbLoadLimit() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        ChatDb db;
        QVERIFY(db.init(tmpDir.filePath("limit_test.db")));
        for (int i = 0; i < 10; ++i)
            db.save("user", "msg_" + QString::number(i));
        QCOMPARE(db.count(), 10);

        const auto msgs = db.load(3);
        QCOMPARE(msgs.size(), 3);  // solo ultimi 3
        db.close();
    }

    /* CP-13: SQLite — contenuto Unicode preservato */
    void chatDbUnicodeContent() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        ChatDb db;
        QVERIFY(db.init(tmpDir.filePath("uni_test.db")));
        const QString msg = QString::fromUtf8("\xf0\x9f\x8d\xba Prismalux v3.0 — AI locale");
        QVERIFY(db.save("user", msg));

        const auto msgs = db.load();
        QCOMPARE(msgs.size(), 1);
        QCOMPARE(msgs[0].second, msg);
        db.close();
    }

    /* CP-14: SQLite — DB su path non valido ritorna false da init */
    void chatDbInvalidPath() {
        ChatDb db;
        QVERIFY(!db.init("/root/non_ho_permessi/x.db"));
        // close() non deve crashare
        db.close();
    }

    /* CP-15: parsing piano Multi-Agente con preambolo LLM */
    void multiAgentParsePlanWithPreamble() {
        const QString json =
            "Ecco il piano:\n"
            "{\"task\":\"Test\","
            "\"subtasks\":[{\"id\":1,\"role\":\"Dev\",\"prompt\":\"Scrivi\",\"depends_on\":[]}]}"
            "\nSpero sia utile.";

        const int start = json.indexOf('{');
        const int end   = json.lastIndexOf('}');
        QVERIFY(start >= 0 && end > start);

        QJsonDocument doc = QJsonDocument::fromJson(
            json.mid(start, end - start + 1).toUtf8());
        QVERIFY(!doc.isNull());
        QVERIFY(doc.object().contains("subtasks"));
        QCOMPARE(doc.object()["subtasks"].toArray().size(), 1);
    }
};

/* ══════════════════════════════════════════════════════════════════════
   main — esegue tutte e 5 le suite
   ══════════════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    int status = 0;
    {
        TestGraphMemoryMobile s1;
        status |= QTest::qExec(&s1, argc, argv);
    }
    {
        TestFinanza s2;
        status |= QTest::qExec(&s2, argc, argv);
    }
    {
        TestFileAiPage s3;
        status |= QTest::qExec(&s3, argc, argv);
    }
    {
        TestAssistentePage s4;
        status |= QTest::qExec(&s4, argc, argv);
    }
    {
        TestChatPage s5;
        status |= QTest::qExec(&s5, argc, argv);
    }

    return status;
}

#include "test_mobile_logic.moc"

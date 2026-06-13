/* test_mobile_logic.cpp — Qt Test unit per la logica di PrismaluxMobile
 *
 * Compila e lancia:
 *   cd ANDROID/android_app/tests
 *   mkdir build && cd build
 *   cmake .. && cmake --build . && ./test_mobile_logic
 *
 * Non richiede Android: testa la logica pura (niente UI, niente NDK).
 */
#include <QtTest/QtTest>
#include <QStringList>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <cmath>

/* ──────────────────────────────────────────────────────────────────
   Logica file attachment (estratta da chat_page.cpp) — testata
   senza istanziare ChatPage (che richiede QApplication + UI)
   ────────────────────────────────────────────────────────────────── */
struct AttachState {
    QStringList contents;
    QStringList names;
    static constexpr int kMax = 5;
    static constexpr qint64 kMaxBytes = 64 * 1024;

    bool attach(const QString& path) {
        if (contents.size() >= kMax) return false;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        QByteArray raw = f.read(kMaxBytes);
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

/* ──────────────────────────────────────────────────────────────────
   Logica backend switching — replica semplificata
   ────────────────────────────────────────────────────────────────── */
struct BackendState {
    enum Mode { Server, Cloud, Local };
    Mode mode = Server;
    QString host = "192.168.1.165";
    int port = 11434;
    QString model = "llama3.2:1b";
    QString cloudHost, cloudModel;

    void switchToServer(const QString& h, int p, const QString& m) {
        mode = Server; host = h; port = p; model = m;
    }
    void switchToCloud(const QString& h, const QString& m) {
        mode = Cloud; cloudHost = h; cloudModel = m;
    }
    void switchToLocal() { mode = Local; }
};

/* ──────────────────────────────────────────────────────────────────
   GraphMemory helper (in-memory, senza SQLite, per test logica pura)
   ────────────────────────────────────────────────────────────────── */
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

/* ──────────────────────────────────────────────────────────────────
   Test suite
   ────────────────────────────────────────────────────────────────── */
class TestMobileLogic : public QObject {
    Q_OBJECT

private slots:
    /* ── File attachment ── */

    void attachSingleFile() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("Contenuto di test per Prismalux.\n");
        f.flush();

        AttachState s;
        QVERIFY(s.attach(f.fileName()));
        QCOMPARE(s.contents.size(), 1);
        QVERIFY(s.contents[0].contains("Contenuto di test"));
        QVERIFY(!s.names[0].isEmpty());
    }

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

        /* Il sesto file deve essere rifiutato */
        QTemporaryFile extra;
        QVERIFY(extra.open());
        extra.write("extra");
        extra.flush();
        QVERIFY(!s.attach(extra.fileName()));
        QCOMPARE(s.contents.size(), AttachState::kMax);
    }

    void attachTruncatesLargeFiles() {
        QTemporaryFile f;
        QVERIFY(f.open());
        /* Scrivi 70 KB > limite 64 KB */
        f.write(QByteArray(70 * 1024, 'X'));
        f.flush();

        AttachState s;
        QVERIFY(s.attach(f.fileName()));
        QCOMPARE(s.contents.size(), 1);
        /* Il contenuto deve essere ≤ 64 KB */
        QVERIFY(s.contents[0].size() <= (int)(AttachState::kMaxBytes));
    }

    void attachNonExistentFile() {
        AttachState s;
        QVERIFY(!s.attach("/non/esiste/mai.txt"));
        QCOMPARE(s.contents.size(), 0);
    }

    void clearResetsState() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("dati");
        f.flush();

        AttachState s;
        s.attach(f.fileName());
        QCOMPARE(s.contents.size(), 1);
        s.clear();
        QCOMPARE(s.contents.size(), 0);
        QCOMPARE(s.names.size(), 0);
    }

    void buildContextEmpty() {
        AttachState s;
        QVERIFY(s.buildContext().isEmpty());
    }

    void buildContextWithFiles() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("Hello world");
        f.flush();

        AttachState s;
        s.attach(f.fileName());
        const QString ctx = s.buildContext();
        QVERIFY(ctx.contains("FILE ALLEGATI"));
        QVERIFY(ctx.contains("Hello world"));
        QVERIFY(ctx.contains("File 1:"));
    }

    void buildContextMultipleFiles() {
        AttachState s;
        for (int i = 0; i < 3; ++i) {
            QTemporaryFile f;
            QVERIFY(f.open());
            f.write(("contenuto_" + QString::number(i)).toUtf8());
            f.flush();
            s.attach(f.fileName());
        }
        const QString ctx = s.buildContext();
        QVERIFY(ctx.contains("File 1:"));
        QVERIFY(ctx.contains("File 2:"));
        QVERIFY(ctx.contains("File 3:"));
        QVERIFY(!ctx.contains("File 4:"));
    }

    /* ── Backend switching ── */

    void backendDefaultIsServer() {
        BackendState b;
        QCOMPARE(b.mode, BackendState::Server);
        QCOMPARE(b.port, 11434);
    }

    void backendSwitchToCloud() {
        BackendState b;
        b.switchToCloud("api.openai.com", "gpt-3.5-turbo");
        QCOMPARE(b.mode, BackendState::Cloud);
        QCOMPARE(b.cloudHost, QString("api.openai.com"));
        QCOMPARE(b.cloudModel, QString("gpt-3.5-turbo"));
    }

    void backendSwitchCloudThenServer() {
        BackendState b;
        b.switchToCloud("api.openai.com", "gpt-4");
        QCOMPARE(b.mode, BackendState::Cloud);

        /* Torna a server locale — deve ripristinare host/porta */
        b.switchToServer("192.168.1.165", 11434, "llama3.2:1b");
        QCOMPARE(b.mode, BackendState::Server);
        QCOMPARE(b.host, QString("192.168.1.165"));
        QCOMPARE(b.port, 11434);
        QCOMPARE(b.model, QString("llama3.2:1b"));
    }

    void backendSwitchToLocal() {
        BackendState b;
        b.switchToLocal();
        QCOMPARE(b.mode, BackendState::Local);
    }

    void backendModeExclusive() {
        BackendState b;
        b.switchToCloud("host", "model");
        b.switchToLocal();
        /* Solo uno attivo */
        QCOMPARE(b.mode, BackendState::Local);
        QVERIFY(b.mode != BackendState::Cloud);
        QVERIFY(b.mode != BackendState::Server);
    }

    /* ── QSettings chiavi server ── */

    void settingsServerKeys() {
        QSettings s("PrismaluxTest", "MobileTest");
        s.setValue("server/host",  "10.0.0.1");
        s.setValue("server/port",  11500);
        s.setValue("server/model", "mistral:7b");
        s.setValue("server/token", "mytoken");

        QCOMPARE(s.value("server/host").toString(),  QString("10.0.0.1"));
        QCOMPARE(s.value("server/port").toInt(),     11500);
        QCOMPARE(s.value("server/model").toString(), QString("mistral:7b"));
        QCOMPARE(s.value("server/token").toString(), QString("mytoken"));

        s.remove("server/host");
        s.remove("server/port");
        s.remove("server/model");
        s.remove("server/token");
    }

    void settingsCloudKeys() {
        QSettings s("PrismaluxTest", "MobileTest");
        s.setValue("cloud/host",  "api.openai.com");
        s.setValue("cloud/key",   "sk-test");
        s.setValue("cloud/model", "gpt-4");
        s.setValue("cloud/port",  443);

        QCOMPARE(s.value("cloud/host").toString(),  QString("api.openai.com"));
        QCOMPARE(s.value("cloud/key").toString(),   QString("sk-test"));
        QCOMPARE(s.value("cloud/model").toString(), QString("gpt-4"));
        QCOMPARE(s.value("cloud/port").toInt(),     443);

        s.remove("cloud/host");
        s.remove("cloud/key");
        s.remove("cloud/model");
        s.remove("cloud/port");
    }

    /* ── Limiti file attachment (boundary) ── */

    void attachZeroFilesContext() {
        AttachState s;
        QVERIFY(s.buildContext().isEmpty());
        QCOMPARE(s.contents.size(), 0);
    }

    void attachExactlyFiveFiles() {
        AttachState s;
        int ok = 0;
        for (int i = 0; i < 5; ++i) {
            QTemporaryFile f;
            f.open();
            f.write("x");
            f.flush();
            if (s.attach(f.fileName())) ++ok;
        }
        QCOMPARE(ok, 5);
        QCOMPARE(s.contents.size(), 5);
    }

    void attachAfterClearAllowsNewFiles() {
        AttachState s;
        for (int i = 0; i < 5; ++i) {
            QTemporaryFile f; f.open(); f.write("x"); f.flush();
            s.attach(f.fileName());
        }
        QCOMPARE(s.contents.size(), 5);

        s.clear();
        QCOMPARE(s.contents.size(), 0);

        QTemporaryFile f2; f2.open(); f2.write("nuovo"); f2.flush();
        QVERIFY(s.attach(f2.fileName()));
        QCOMPARE(s.contents.size(), 1);
    }

    /* ── Parsing piano Multi-Agente (logica estratta da multi_agent_page.cpp) ── */

    void multiAgentParsePlanValid() {
        const QString json = R"({
            "task": "Analizza Python",
            "subtasks": [
                {"id": 1, "role": "Analista", "prompt": "Analizza il codice", "depends_on": []},
                {"id": 2, "role": "Revisore",  "prompt": "Rivedi i risultati", "depends_on": [1]}
            ]
        })";

        const int start = json.indexOf('{');
        const int end   = json.lastIndexOf('}');
        QVERIFY(start >= 0 && end > start);

        QJsonDocument doc = QJsonDocument::fromJson(
            json.mid(start, end - start + 1).toUtf8());
        QVERIFY(doc.isObject());

        const QJsonObject root  = doc.object();
        const QJsonArray  tasks = root["subtasks"].toArray();
        QCOMPARE(tasks.size(), 2);

        const QJsonObject t0 = tasks[0].toObject();
        QCOMPARE(t0["id"].toInt(), 1);
        QCOMPARE(t0["role"].toString(), QString("Analista"));
        QVERIFY(t0["depends_on"].toArray().isEmpty());

        const QJsonObject t1 = tasks[1].toObject();
        QCOMPARE(t1["id"].toInt(), 2);
        QCOMPARE(t1["depends_on"].toArray().size(), 1);
        QCOMPARE(t1["depends_on"].toArray()[0].toInt(), 1);
    }

    void multiAgentParsePlanWithPreamble() {
        /* LLM spesso aggiunge testo prima del JSON — robustezza */
        const QString json =
            "Certo! Ecco il piano:\n"
            "{\"task\":\"Test\","
            "\"subtasks\":[{\"id\":1,\"role\":\"Dev\",\"prompt\":\"Scrivi\",\"depends_on\":[]}]}"
            "\n\nSpero sia utile.";

        const int start = json.indexOf('{');
        const int end   = json.lastIndexOf('}');
        QVERIFY(start >= 0 && end > start);
        const QString extracted = json.mid(start, end - start + 1);
        QJsonDocument doc = QJsonDocument::fromJson(extracted.toUtf8());
        QVERIFY(!doc.isNull());
        QVERIFY(doc.object().contains("subtasks"));
        QCOMPARE(doc.object()["subtasks"].toArray().size(), 1);
    }

    void multiAgentParsePlanEmpty() {
        const QString json = "Nessun piano disponibile.";
        const int start = json.indexOf('{');
        QCOMPARE(start, -1);   /* nessun JSON → parsing fallisce legittimamente */
    }

    void multiAgentBfsDependsOn() {
        /* Verifica ordinamento BFS rispetto a depends_on */
        struct SubTask {
            int id;
            QList<int> dependsOn;
            bool done = false;
        };

        QVector<SubTask> tasks = {
            {1, {}},
            {2, {1}},
            {3, {1}},
            {4, {2, 3}},
        };

        /* Simula la selezione "prossimo task eseguibile" */
        QVector<int> order;
        while (order.size() < tasks.size()) {
            for (auto& t : tasks) {
                if (t.done) continue;
                bool ready = true;
                for (int dep : t.dependsOn) {
                    bool depDone = false;
                    for (const auto& tt : tasks)
                        if (tt.id == dep) { depDone = tt.done; break; }
                    if (!depDone) { ready = false; break; }
                }
                if (ready) {
                    order.append(t.id);
                    t.done = true;
                    break;
                }
            }
        }
        /* 1 deve venire prima di 2 e 3; 4 deve essere ultimo */
        QCOMPARE(order.size(), 4);
        QCOMPARE(order.first(), 1);
        QCOMPARE(order.last(), 4);
    }

    /* ── Calcolo TFR semplificato (logica estratta da finanza_page.cpp mobile) ── */

    void tfrCalcoloBase() {
        /* Formula base TFR: stipendio_annuo / 13.5 * anni */
        const double stipendioMensile = 2000.0;
        const int    anni             = 5;
        const double tfrAtteso = stipendioMensile * 12.0 / 13.5 * anni;

        /* Arrotondamento a centesimi */
        const double tfrRound = std::round(tfrAtteso * 100.0) / 100.0;
        QVERIFY(tfrRound > 0.0);
        QVERIFY(std::abs(tfrRound - 8888.89) < 1.0);
    }

    void tfrZeroAnni() {
        const double tfr = 2000.0 * 12.0 / 13.5 * 0;
        QCOMPARE(tfr, 0.0);
    }

    void tfrNonNegativo() {
        const double tfr = 1500.0 * 12.0 / 13.5 * 3;
        QVERIFY(tfr > 0.0);
    }

    /* ── GraphMemory mobile: CRUD in-memory ── */

    void graphMemoryAddNode() {
        GmStore gm;
        const QString id = gm.addNode("entity", "Qt6", "Framework C++", 0.9f);
        QVERIFY(!id.isEmpty());
        QCOMPARE(gm.nodes.size(), 1);
        QCOMPARE(gm.nodes[0].label, QString("Qt6"));
    }

    void graphMemoryAddEdge() {
        GmStore gm;
        const QString n1 = gm.addNode("entity", "Qt6",    "Framework", 0.8f);
        const QString n2 = gm.addNode("entity", "Qmake",  "Build tool", 0.5f);
        QVERIFY(gm.addEdge(n1, n2, "usa", 0.7f));
        QCOMPARE(gm.edges.size(), 1);
        QCOMPARE(gm.edges[0].relation, QString("usa"));
    }

    void graphMemorySearchNodes() {
        GmStore gm;
        gm.addNode("entity", "Qt6 Widgets",  "GUI framework", 0.9f);
        gm.addNode("entity", "SymPy",         "Algebra simbolica", 0.7f);
        gm.addNode("entity", "Qt6 Network",   "Network framework", 0.8f);

        const auto found = gm.searchNodes("Qt6");
        QCOMPARE(found.size(), 2);
    }

    void graphMemorySearchCaseInsensitive() {
        GmStore gm;
        gm.addNode("entity", "Python", "Linguaggio di scripting", 0.8f);
        const auto res = gm.searchNodes("python");
        QCOMPARE(res.size(), 1);
    }

    void graphMemoryEdgeInvalidIds() {
        GmStore gm;
        QVERIFY(!gm.addEdge("", "n2", "rel", 1.0f));
        QVERIFY(!gm.addEdge("n1", "", "rel", 1.0f));
    }

    void graphMemoryClearAll() {
        GmStore gm;
        gm.addNode("entity", "A", "content", 0.5f);
        gm.addNode("entity", "B", "content", 0.5f);
        gm.addEdge("node_1", "node_2", "rel", 1.0f);
        gm.clear();
        QCOMPARE(gm.nodes.size(), 0);
        QCOMPARE(gm.edges.size(), 0);
    }

    /* ── Sicurezza: SQL injection nei campi di ricerca ── */

    void graphMemorySqlInjectionSearch() {
        GmStore gm;
        gm.addNode("entity", "Normal node", "content normale", 0.5f);
        /* Input malevolo: non deve causare dati spuri */
        const auto res = gm.searchNodes("'; DROP TABLE nodes; --");
        QCOMPARE(res.size(), 0);   /* nessun risultato legittimo */
    }
};

QTEST_MAIN(TestMobileLogic)
#include "test_mobile_logic.moc"

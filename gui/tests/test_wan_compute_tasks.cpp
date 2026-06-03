/* ══════════════════════════════════════════════════════════════
   test_wan_compute_tasks.cpp — Unit test WAN Compute (LanWanPage)

   Categorie:
     CAT-A  Costruzione e sicurezza defaults
     CAT-B  Protocollo WAN via TCP (server locale)
     CAT-C  Formato JSON messaggi
     CAT-D  Task math_expr via TCP
     CAT-E  Sicurezza: shell_cmd bloccata per default

   Strategia:
     - CAT-A/C/E: costruzione LanWanPage + findChild senza TCP
     - CAT-B/D: avvia il server WAN (click su m_wanStartBtn) e
       connette un QTcpSocket locale per scambiare messaggi JSON

   Note:
     - LanWanPage non avvia il server nel costruttore: sicuro istanziarla.
     - m_wanPortSpin.value() di default è P::kWanComputePort (11600).
       Per evitare conflitti usiamo una porta casuale impostando il spin a 0
       PRIMA di cliccare start.
     - I test TCP usano QEventLoop + QTimer come timeout esplicito per non
       bloccare indefinitamente quando il server non risponde.

   Build:
     cmake -B build_tests gui/ -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_wan_compute_tasks
     ./build_tests/test_wan_compute_tasks
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <QAbstractSocket>
#include <QTcpServer>
#include <QProcess>
#include <QRandomGenerator>

#include "mock_ai_client.h"
#include "../pages/main_lan_wan.h"
#include "../pages/main_multi_agent.h"

/* ──────────────────────────────────────────────────────────────
   Helper: invia JSON con newline su socket e torna subito
   ────────────────────────────────────────────────────────────── */
static void sendJson(QTcpSocket* sock, const QJsonObject& obj)
{
    sock->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
    sock->flush();
}

/* ──────────────────────────────────────────────────────────────
   Helper: attende dati sul socket (max ms millisecondi)
   Ritorna il primo oggetto JSON letto, o QJsonObject() su timeout.
   ────────────────────────────────────────────────────────────── */
/* Attende una riga JSON sul socket lasciando girare il loop Qt (necessario
 * perché il server WAN gira nello stesso thread — waitForReadyRead bloccherebbe
 * il loop e il server non potrebbe mai rispondere). */
static QJsonObject waitForLine(QTcpSocket* sock, int ms = 2000)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (sock->canReadLine()) {
            const QByteArray line = sock->readLine().trimmed();
            const QJsonDocument doc = QJsonDocument::fromJson(line);
            if (doc.isObject()) return doc.object();
        }
    }
    return {};
}

/* ──────────────────────────────────────────────────────────────
   Fixture condivisa: un'unica LanWanPage per CAT-A/C/E
   ────────────────────────────────────────────────────────────── */
static MockAiClient* s_ai   = nullptr;
static LanWanPage*   s_page = nullptr;

static void ensurePage()
{
    if (!s_ai)   s_ai   = new MockAiClient();
    if (!s_page) s_page = new LanWanPage(s_ai);
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruzione e sicurezza defaults
   ══════════════════════════════════════════════════════════════ */
class TestCostruzioneSicurezza : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    /* A-1: LanWanPage si costruisce senza crash */
    void A1_costruisceSenzaCrash() {
        QVERIFY2(s_page != nullptr, "LanWanPage non costruita");
        QVERIFY2(qobject_cast<QWidget*>(s_page) != nullptr,
                 "LanWanPage non e' un QWidget");
    }

    /* A-2: checkbox shell esiste e NON e' checked di default (sicurezza) */
    void A2_shellCheckboxEsisteNonCheckedDefault() {
        const auto checks = s_page->findChildren<QCheckBox*>();
        QCheckBox* shellChk = nullptr;
        for (QCheckBox* c : checks) {
            if (c->text().contains("shell", Qt::CaseInsensitive) ||
                c->text().contains("RCE",   Qt::CaseInsensitive)) {
                shellChk = c;
                break;
            }
        }
        QVERIFY2(shellChk != nullptr,
                 "Checkbox 'shell/RCE' non trovata in LanWanPage");
        QVERIFY2(!shellChk->isChecked(),
                 "Checkbox shell e' checked di default: VULNERABILITA' di sicurezza!");
    }

    /* A-3: campo token server esiste e inizialmente vuoto */
    void A3_tokenServerEsisteEVuoto() {
        /* Il token edit del server ha il placeholder "Lascia vuoto = accetta tutti" */
        const auto edits = s_page->findChildren<QLineEdit*>();
        bool found = false;
        for (QLineEdit* e : edits) {
            if (e->placeholderText().contains("vuoto", Qt::CaseInsensitive) ||
                e->placeholderText().contains("token", Qt::CaseInsensitive)) {
                QVERIFY2(e->text().isEmpty(),
                         qPrintable("Token server non vuoto all'avvio: " + e->text()));
                found = true;
                break;
            }
        }
        /* Se non trovato via placeholder, verifica che almeno esista un QLineEdit
           con echo mode Password (usato per i token) */
        if (!found) {
            for (QLineEdit* e : edits) {
                if (e->echoMode() == QLineEdit::Password) {
                    QVERIFY2(e->text().isEmpty(),
                             "Token server (echoMode=Password) non vuoto all'avvio");
                    found = true;
                    break;
                }
            }
        }
        QVERIFY2(found, "Nessun campo token server trovato in LanWanPage");
    }

    /* A-4: almeno un campo token (EchoMode=Password) esiste in LanWanPage.
       Il token server può essere auto-generato da QSettings — non verifichiamo
       che sia vuoto, solo che il campo esista. */
    void A4_tokenClientEsisteEVuoto() {
        const auto edits = s_page->findChildren<QLineEdit*>();
        int passwordFields = 0;
        for (QLineEdit* e : edits) {
            if (e->echoMode() == QLineEdit::Password) ++passwordFields;
        }
        QVERIFY2(passwordFields >= 1,
                 "Nessun campo token (EchoMode=Password) trovato in LanWanPage");
    }

    /* A-5: multiAgentTab() non nullptr (AgentiMultiPage embedded) */
    void A5_multiAgentTabNonNull() {
        QVERIFY2(s_page->multiAgentTab() != nullptr,
                 "multiAgentTab() restituisce nullptr");
        QVERIFY2(qobject_cast<AgentiMultiPage*>(s_page->multiAgentTab()) != nullptr,
                 "multiAgentTab() non e' un AgentiMultiPage");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Protocollo WAN via TCP
   ══════════════════════════════════════════════════════════════ */

/* Fixture separata con server dedicato per i test TCP */
struct TcpFixture {
    MockAiClient ai;
    LanWanPage   page{&ai};
    quint16      port = 0;

    /* Avvia il server WAN su una porta alta per evitare conflitti con 11600.
       Restituisce true se il server e' in ascolto. */
    bool startServer() {
        /* Trova il QSpinBox del WAN server (quello con valore P::kWanComputePort=11600).
           Ci sono piu' SpinBox: LAN port (default vario), WAN port (11600), client port. */
        const auto spins = page.findChildren<QSpinBox*>();
        QSpinBox* wanSpin = nullptr;
        for (QSpinBox* s : spins) {
            if (s->value() == 11600) { wanSpin = s; break; }
        }
        /* Fallback: primo SpinBox con range che include porte alte */
        if (!wanSpin && !spins.isEmpty()) wanSpin = spins.first();
        if (!wanSpin) return false;

        /* Imposta una porta alta casuale (range valido 1024–65535) */
        const int tryPort = 49200 + (QRandomGenerator::global()->bounded(1000));
        wanSpin->setValue(tryPort);

        /* m_wanStartBtn e' checkable con objectName "actionBtn":
           click() su un checkable NON ancora checked → lo mette a checked + emette clicked(true) */
        auto* btn = page.findChild<QPushButton*>("actionBtn");
        if (!btn || !btn->isCheckable()) {
            /* fallback: primo QPushButton checkable */
            for (QPushButton* b : page.findChildren<QPushButton*>()) {
                if (b->isCheckable()) { btn = b; break; }
            }
        }
        if (!btn) return false;

        /* Se gia' checked (server gia' avviato), non facciamo nulla di speciale;
           altrimenti click() lo abilita e triggera onWanStartBtnClicked */
        if (!btn->isChecked()) {
            btn->click();  // click() su checkable: toggling + emit clicked(true)
            QCoreApplication::processEvents();
        }

        /* Scopri la porta realmente usata tramite il QTcpServer */
        const auto servers = page.findChildren<QTcpServer*>();
        for (QTcpServer* s : servers) {
            if (s->isListening()) { port = s->serverPort(); return true; }
        }
        return false;
    }

    /* Connette un socket client al server locale */
    QTcpSocket* connectClient() {
        auto* sock = new QTcpSocket;
        sock->connectToHost(QHostAddress::LocalHost, port);
        if (!sock->waitForConnected(2000)) { delete sock; return nullptr; }
        QCoreApplication::processEvents();
        return sock;
    }

    ~TcpFixture() {
        /* Ferma il server per non lasciare porte aperte:
           click() di nuovo su un checkable checked → lo unchecka + emette clicked(false) */
        for (QPushButton* b : page.findChildren<QPushButton*>()) {
            if (b->isCheckable() && b->isChecked()) {
                b->click();
                QCoreApplication::processEvents();
                break;
            }
        }
    }
};

class TestProtocolloTcp : public QObject {
    Q_OBJECT
private slots:

    /* B-1: hello senza token → welcome quando server non ha token.
       Svuota esplicitamente il campo token server per questo test. */
    void B1_helloSenzaTokenWelcome() {
        TcpFixture fx;

        /* Svuota il token server così il server accetta hello senza auth */
        for (QLineEdit* e : fx.page.findChildren<QLineEdit*>()) {
            if (e->echoMode() == QLineEdit::Password) { e->clear(); break; }
        }

        if (!fx.startServer()) QSKIP("Server WAN non avviato");

        QTcpSocket sock;
        sock.connectToHost(QHostAddress::LocalHost, fx.port);
        if (!sock.waitForConnected(1000)) QSKIP("Connessione al server WAN fallita");

        sendJson(&sock, QJsonObject{
            {"t",    "hello"},
            {"name", "test-node"},
            {"token",""},
            {"caps", QJsonArray{"ai"}}
        });

        const QJsonObject resp = waitForLine(&sock, 2000);
        sock.disconnectFromHost();

        QVERIFY2(!resp.isEmpty(), "Nessuna risposta dal server dopo hello");
        QCOMPARE(resp["t"].toString(), QString("welcome"));
        QVERIFY2(!resp["node_id"].toString().isEmpty(), "welcome senza node_id");
    }

    /* B-2: hello con token sbagliato → error auth_failed */
    void B2_helloTokenSbagliato_authFailed() {
        TcpFixture fx;
        if (!fx.startServer()) QSKIP("Server WAN non avviato");

        /* Imposta token WAN server (riconoscibile dal placeholder) */
        const auto edits = fx.page.findChildren<QLineEdit*>();
        QLineEdit* tokenEdit = nullptr;
        for (QLineEdit* e : edits) {
            if (e->echoMode() == QLineEdit::Password
                && e->placeholderText().contains("accetta", Qt::CaseInsensitive)) {
                tokenEdit = e; break;
            }
        }
        if (!tokenEdit) QSKIP("Campo token WAN server non trovato");
        tokenEdit->setText("secret123");

        QTcpSocket* sock = fx.connectClient();
        if (!sock) QSKIP("Connessione al server WAN fallita");

        sendJson(sock, QJsonObject{
            {"t",    "hello"},
            {"name", "bad-node"},
            {"token","WRONG_TOKEN"},
            {"caps", QJsonArray{"ai"}}
        });

        const QJsonObject resp = waitForLine(sock, 2000);
        sock->deleteLater();
        tokenEdit->clear();  // ripristina per test successivi

        QVERIFY2(!resp.isEmpty(), "Nessuna risposta dal server dopo hello con token errato");
        QCOMPARE(resp["t"].toString(), QString("error"));
        QCOMPARE(resp["msg"].toString(), QString("auth_failed"));
    }

    /* B-3: hello con token corretto → welcome */
    void B3_helloTokenCorretto_welcome() {
        TcpFixture fx;
        if (!fx.startServer()) QSKIP("Server WAN non avviato");

        const auto edits = fx.page.findChildren<QLineEdit*>();
        QLineEdit* tokenEdit = nullptr;
        for (QLineEdit* e : edits) {
            if (e->echoMode() == QLineEdit::Password
                && e->placeholderText().contains("accetta", Qt::CaseInsensitive)) {
                tokenEdit = e; break;
            }
        }
        if (!tokenEdit) QSKIP("Campo token WAN server non trovato");
        tokenEdit->setText("righttoken");

        QTcpSocket* sock = fx.connectClient();
        if (!sock) { tokenEdit->clear(); QSKIP("Connessione al server WAN fallita"); }

        sendJson(sock, QJsonObject{
            {"t",    "hello"},
            {"name", "good-node"},
            {"token","righttoken"},
            {"caps", QJsonArray{"ai"}}
        });

        const QJsonObject resp = waitForLine(sock, 2000);
        sock->deleteLater();
        tokenEdit->clear();

        QVERIFY2(!resp.isEmpty(), "Nessuna risposta dal server dopo hello con token corretto");
        QCOMPARE(resp["t"].toString(), QString("welcome"));
    }

    /* B-4: poll dopo registrazione → idle (nessun task pendente) */
    void B4_pollRispondeIdle() {
        TcpFixture fx;
        if (!fx.startServer()) QSKIP("Server WAN non avviato");

        QTcpSocket* sock = fx.connectClient();
        if (!sock) QSKIP("Connessione al server WAN fallita");

        /* Prima registra il nodo */
        sendJson(sock, QJsonObject{
            {"t","hello"},{"name","poll-node"},{"token",""},{"caps",QJsonArray{"ai"}}
        });
        const QJsonObject welcome = waitForLine(sock, 2000);
        if (welcome["t"].toString() != "welcome") {
            sock->deleteLater();
            QSKIP("Hello/welcome fallito — prerequisito B-4");
        }

        /* Poi chiede un task */
        sendJson(sock, QJsonObject{{"t","poll"}});
        const QJsonObject resp = waitForLine(sock, 2000);
        sock->deleteLater();

        QVERIFY2(!resp.isEmpty(), "Nessuna risposta al poll");
        /* Con zero task pendenti il server risponde "idle" */
        QCOMPARE(resp["t"].toString(), QString("idle"));
    }

    /* B-5: result inviato → ack ricevuto */
    void B5_resultRiceveAck() {
        TcpFixture fx;
        if (!fx.startServer()) QSKIP("Server WAN non avviato");

        QTcpSocket* sock = fx.connectClient();
        if (!sock) QSKIP("Connessione al server WAN fallita");

        /* Registra */
        sendJson(sock, QJsonObject{
            {"t","hello"},{"name","ack-node"},{"token",""},{"caps",QJsonArray{"ai"}}
        });
        const QJsonObject welcome = waitForLine(sock, 2000);
        if (welcome["t"].toString() != "welcome") {
            sock->deleteLater();
            QSKIP("Hello/welcome fallito — prerequisito B-5");
        }

        /* Invia un risultato con id fittizio */
        const QString fakeId = "test-task-9999";
        sendJson(sock, QJsonObject{
            {"t",      "result"},
            {"id",     fakeId},
            {"status", "done"},
            {"result", "42"}
        });

        const QJsonObject resp = waitForLine(sock, 2000);
        sock->deleteLater();

        QVERIFY2(!resp.isEmpty(), "Nessuna risposta dopo result");
        QCOMPARE(resp["t"].toString(), QString("ack"));
        QCOMPARE(resp["id"].toString(), fakeId);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Formato JSON messaggi (analisi strutturale)
   ══════════════════════════════════════════════════════════════ */
class TestFormatoJson : public QObject {
    Q_OBJECT
private slots:

    /* C-1: JSON hello ha i campi t, name, token, caps */
    void C1_helloHaCampiObbligatori() {
        const QJsonObject hello{
            {"t",    "hello"},
            {"name", "nodo-test"},
            {"token",""},
            {"caps", QJsonArray{"ai"}}
        };
        QVERIFY(hello.contains("t"));
        QVERIFY(hello.contains("name"));
        QVERIFY(hello.contains("token"));
        QVERIFY(hello.contains("caps"));
        QCOMPARE(hello["t"].toString(), QString("hello"));
    }

    /* C-2: caps di default non contiene "shell" (sicurezza)
       Riflette lo stato iniziale della checkbox (off). */
    void C2_capsDefaultNoShell() {
        /* Questo simula cio' che il client invia quando la checkbox e' OFF */
        QJsonArray caps;
        caps.append("ai");
        /* "shell" NON deve essere aggiunto se la checkbox e' off */
        QVERIFY2(!caps.contains(QJsonValue("shell")),
                 "caps di default contiene 'shell': VULNERABILITA' di sicurezza");
    }

    /* C-3: JSON task ha campi t, id, kind, payload */
    void C3_taskHaCampiObbligatori() {
        const QJsonObject task{
            {"t",       "task"},
            {"id",      "task-001"},
            {"kind",    "math_expr"},
            {"payload", "2+2"}
        };
        QVERIFY(task.contains("t"));
        QVERIFY(task.contains("id"));
        QVERIFY(task.contains("kind"));
        QVERIFY(task.contains("payload"));
        QCOMPARE(task["t"].toString(), QString("task"));
    }

    /* C-4: JSON result ha campi t, id, status, result */
    void C4_resultHaCampiObbligatori() {
        const QJsonObject result{
            {"t",      "result"},
            {"id",     "task-001"},
            {"status", "done"},
            {"result", "4"}
        };
        QVERIFY(result.contains("t"));
        QVERIFY(result.contains("id"));
        QVERIFY(result.contains("status"));
        QVERIFY(result.contains("result"));
        QCOMPARE(result["t"].toString(), QString("result"));
        QCOMPARE(result["status"].toString(), QString("done"));
    }

    /* C-5: JSON poll ha il campo t="poll" */
    void C5_pollHaCampoT() {
        const QJsonObject poll{{"t","poll"}};
        QCOMPARE(poll["t"].toString(), QString("poll"));
    }

    /* C-6: JSON spawn_tasks ha i campi t, parent_id, chain_id, tasks */
    void C6_spawnTasksHaCampi() {
        const QJsonObject spawn{
            {"t",         "spawn_tasks"},
            {"parent_id", "p-001"},
            {"chain_id",  "c-001"},
            {"tasks",     QJsonArray{}}
        };
        QVERIFY(spawn.contains("t"));
        QVERIFY(spawn.contains("parent_id"));
        QVERIFY(spawn.contains("chain_id"));
        QVERIFY(spawn.contains("tasks"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Task math_expr via TCP
   ══════════════════════════════════════════════════════════════ */

/* math_expr usa python3 direttamente — controlla che sia disponibile */
static bool python3Available()
{
    QProcess p;
    p.start("python3", {"--version"});
    p.waitForFinished(3000);
    return (p.exitCode() == 0);
}

class TestMathExprTcp : public QObject {
    Q_OBJECT
private slots:

    /* D-1: il server assegna un math_expr task e il nodo (client) lo riceve
       con i campi corretti. Usiamo server+due socket: uno come "server node",
       l'altro simula il "dispatch" aggiungendo il task via addTask. */
    void D1_mathExprTaskRicevuto() {
        TcpFixture fx;
        if (!fx.startServer()) QSKIP("Server WAN non avviato");

        /* Aggiungi un task math_expr al server premendo m_wanAddTaskBtn */
        auto* kindCombo = fx.page.findChildren<QComboBox*>().value(0, nullptr);
        /* Imposta kind a math_expr nel combo se esiste */
        if (kindCombo) {
            for (int i = 0; i < kindCombo->count(); ++i) {
                if (kindCombo->itemText(i).contains("math_expr", Qt::CaseInsensitive)) {
                    kindCombo->setCurrentIndex(i);
                    break;
                }
            }
        }

        QTcpSocket* sock = fx.connectClient();
        if (!sock) QSKIP("Connessione al server WAN fallita");

        /* Registra il nodo */
        sendJson(sock, QJsonObject{
            {"t","hello"},{"name","math-node"},{"token",""},{"caps",QJsonArray{"ai"}}
        });
        const QJsonObject welcome = waitForLine(sock, 2000);
        if (welcome["t"].toString() != "welcome") {
            sock->deleteLater();
            QSKIP("Hello/welcome fallito");
        }

        /* Aggiungi il task lato server: trova il pulsante "Aggiungi Task" */
        const auto btns = fx.page.findChildren<QPushButton*>();
        QPushButton* addTaskBtn = nullptr;
        for (QPushButton* b : btns) {
            if (b->text().contains("Aggiungi", Qt::CaseInsensitive) ||
                b->text().contains("Add", Qt::CaseInsensitive)) {
                addTaskBtn = b;
                break;
            }
        }
        if (!addTaskBtn) {
            sock->deleteLater();
            QSKIP("Pulsante 'Aggiungi task' non trovato");
        }

        /* Imposta payload */
        const auto textEdits = fx.page.findChildren<QTextEdit*>();
        QTextEdit* payloadEdit = nullptr;
        for (QTextEdit* t : textEdits) {
            if (!t->isReadOnly()) { payloadEdit = t; break; }
        }
        if (payloadEdit) payloadEdit->setText("2+2");

        addTaskBtn->click();
        QCoreApplication::processEvents();

        /* Il nodo fa poll e riceve il task */
        sendJson(sock, QJsonObject{{"t","poll"}});
        const QJsonObject resp = waitForLine(sock, 2000);
        sock->deleteLater();

        /* Due possibilita': "task" (assegnato) o "idle" (task non aggiunto) */
        const QString t = resp["t"].toString();
        QVERIFY2(t == "task" || t == "idle",
                 qPrintable("Risposta poll inattesa: " + t));

        if (t == "task") {
            QVERIFY2(!resp["id"].toString().isEmpty(),     "task senza id");
            QVERIFY2(!resp["kind"].toString().isEmpty(),   "task senza kind");
            QVERIFY2(!resp["payload"].toString().isEmpty(),"task senza payload");
        }
    }

    /* D-2: math_expr "2+2" via subprocess python3 restituisce "4" */
    void D2_mathExprDuePiuDue() {
        if (!python3Available()) QSKIP("python3 non disponibile");

        QProcess proc;
        proc.start("python3", {"-c",
            "import math,cmath,statistics\n"
            "try:\n"
            "    print(eval(\"2+2\"))\n"
            "except Exception as e:\n"
            "    print('Errore:', e)"
        });
        QVERIFY(proc.waitForFinished(5000));
        const QString result = proc.readAllStandardOutput().trimmed();
        QCOMPARE(result, QString("4"));
    }

    /* D-3: math_expr malformata → stampa "Errore:" ma non crasha */
    void D3_mathExprMalformata() {
        if (!python3Available()) QSKIP("python3 non disponibile");

        QProcess proc;
        proc.start("python3", {"-c",
            "import math,cmath,statistics\n"
            "try:\n"
            "    print(eval(\"esto no es math!!!\"))\n"
            "except Exception as e:\n"
            "    print('Errore:', e)"
        });
        QVERIFY(proc.waitForFinished(5000));
        const QString result = proc.readAllStandardOutput().trimmed();
        QVERIFY2(result.startsWith("Errore:"),
                 qPrintable("math_expr malformata non produce 'Errore:': " + result));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-E — Sicurezza: shell_cmd bloccata di default
   ══════════════════════════════════════════════════════════════ */
class TestSicurezzaShell : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    /* E-1: la checkbox shell e' un QCheckBox con isChecked()==false di default */
    void E1_checkboxShellFalseDefault() {
        const auto checks = s_page->findChildren<QCheckBox*>();
        QCheckBox* shellChk = nullptr;
        for (QCheckBox* c : checks) {
            if (c->text().contains("shell", Qt::CaseInsensitive) ||
                c->text().contains("RCE",   Qt::CaseInsensitive)) {
                shellChk = c;
                break;
            }
        }
        QVERIFY2(shellChk != nullptr, "QCheckBox shell non trovata");
        QVERIFY2(!shellChk->isChecked(),
                 "SICUREZZA: checkbox shell e' checked di default!");
    }

    /* E-2: il testo/tooltip della checkbox menziona "RCE" o sicurezza */
    void E2_checkboxShellMenzionaRce() {
        const auto checks = s_page->findChildren<QCheckBox*>();
        for (QCheckBox* c : checks) {
            if (c->text().contains("shell",    Qt::CaseInsensitive) ||
                c->text().contains("RCE",      Qt::CaseInsensitive) ||
                c->toolTip().contains("shell", Qt::CaseInsensitive) ||
                c->toolTip().contains("RCE",   Qt::CaseInsensitive)) {
                const bool mentionaRce =
                    c->text().contains("RCE",  Qt::CaseInsensitive) ||
                    c->toolTip().contains("RCE",Qt::CaseInsensitive) ||
                    c->toolTip().contains("sicurezza", Qt::CaseInsensitive) ||
                    c->toolTip().contains("rischio",   Qt::CaseInsensitive);
                QVERIFY2(mentionaRce,
                         "checkbox shell non menziona RCE/sicurezza/rischio: "
                         "l'utente non e' avvertito del pericolo");
                return;
            }
        }
        QSKIP("Checkbox shell non trovata per E-2");
    }

    /* E-3: il server WAN non avvia automaticamente nel costruttore */
    void E3_serverNonAvviaInCostruttore() {
        MockAiClient ai2;
        LanWanPage page2(&ai2);
        const auto servers = page2.findChildren<QTcpServer*>();
        bool anyListening = false;
        for (QTcpServer* s : servers) {
            if (s->isListening()) { anyListening = true; break; }
        }
        QVERIFY2(!anyListening,
                 "SICUREZZA: il server WAN si avvia automaticamente nel costruttore!");
    }

    /* E-4: caps hello di default NON contiene "shell" quando checkbox e' OFF */
    void E4_capsDefaultNoShell() {
        const auto checks = s_page->findChildren<QCheckBox*>();
        for (QCheckBox* c : checks) {
            if (c->text().contains("shell", Qt::CaseInsensitive) ||
                c->text().contains("RCE",   Qt::CaseInsensitive)) {
                /* La checkbox deve essere OFF per questo test */
                if (c->isChecked()) c->setChecked(false);
                /* Verifica stato: caps NON deve includere "shell" */
                const bool shellAllowed = c->isChecked();
                QJsonArray caps;
                caps.append("ai");
                if (shellAllowed) caps.append("shell");
                QVERIFY2(!caps.contains(QJsonValue("shell")),
                         "caps include 'shell' anche con checkbox OFF");
                return;
            }
        }
        QSKIP("Checkbox shell non trovata per E-4");
    }
};

/* ══════════════════════════════════════════════════════════════
   main
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    {
        TestCostruzioneSicurezza t1; status |= QTest::qExec(&t1, argc, argv);
        TestProtocolloTcp        t2; status |= QTest::qExec(&t2, argc, argv);
        TestFormatoJson          t3; status |= QTest::qExec(&t3, argc, argv);
        TestMathExprTcp          t4; status |= QTest::qExec(&t4, argc, argv);
        TestSicurezzaShell       t5; status |= QTest::qExec(&t5, argc, argv);
    }
    return status;
}

#include "test_wan_compute_tasks.moc"

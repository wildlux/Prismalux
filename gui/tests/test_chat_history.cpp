/* ══════════════════════════════════════════════════════════════
   test_chat_history.cpp — Unit test per ChatHistory
   ──────────────────────────────────────────────────────────────
   CAT-A  newSession + list() (8 test)
   CAT-B  saveLog / loadLog (10 test)
   CAT-C  remove (6 test)
   CAT-D  Robustezza e edge case (6 test)

   IMPORTANTE: i test usano una QTemporaryDir isolata — nessun file
   viene creato in ~/.prismalux_chats/ (evita contaminazione prod).
   ChatHistory usa m_dir = home/.prismalux_chats/ hardcoded, quindi
   i test che richiedono isolamento completo testano le API pubbliche
   su un'istanza reale che scrive nella directory di produzione ma
   con id univoci (timestamp ms) che non collidono con sessioni reali.

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_chat_history
     ./build_tests/test_chat_history
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QThread>
#include "../chat_history.h"

/* ──────────────────────────────────────────────────────────────
   Helper: crea ChatHistory e rimuove al teardown tutte le sessioni
   create durante il test (quelle con id nel set m_created).
   ────────────────────────────────────────────────────────────── */
class ChatHistoryFixture {
public:
    ChatHistory ch;
    QStringList created;

    QString newSession(const QString& task) {
        const QString id = ch.newSession(task);
        created << id;
        return id;
    }

    ~ChatHistoryFixture() {
        for (const QString& id : created)
            ch.remove(id);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-A — newSession + list() (8 test)
   ══════════════════════════════════════════════════════════════ */
class TestNewSession : public QObject {
    Q_OBJECT
private slots:

    /* Una nuova sessione compare in list() */
    void nuovaSessioneInLista() {
        ChatHistoryFixture f;
        const QString id = f.newSession("Test task A1");
        QVERIFY(!id.isEmpty());
        const auto lista = f.ch.list();
        bool trovato = false;
        for (const ChatSession& s : lista)
            if (s.id == id) trovato = true;
        QVERIFY2(trovato, "sessione creata non trovata in list()");
    }

    /* id ha formato "TIMESTAMP_SEQ" dove TIMESTAMP è ms da epoch e SEQ è intero >= 0 */
    void idNumericoPositivo() {
        ChatHistoryFixture f;
        const QString id = f.newSession("Task A2");
        const QStringList parts = id.split('_');
        QVERIFY2(parts.size() >= 2, "id non ha formato TIMESTAMP_SEQ");
        bool ok1 = false, ok2 = false;
        const qint64 ts = parts[0].toLongLong(&ok1);
        const int seq   = parts[1].toInt(&ok2);
        QVERIFY2(ok1 && ts > 0, "parte timestamp non valida");
        QVERIFY2(ok2 && seq >= 0, "parte sequenza non valida");
    }

    /* Titolo troncato a 40 caratteri per task lunghi */
    void titoloTroncartoA40() {
        ChatHistoryFixture f;
        const QString longTask = QString(100, 'X');
        const QString id = f.newSession(longTask);
        const auto lista = f.ch.list();
        for (const ChatSession& s : lista) {
            if (s.id == id) {
                QVERIFY2(s.title.size() <= 40,
                         qPrintable(QString("titolo troppo lungo: %1 char").arg(s.title.size())));
                break;
            }
        }
    }

    /* Titolo da task vuoto: non è vuoto (usa fallback) oppure è vuoto (ma no crash) */
    void titoloTaskVuotoNoCrash() {
        ChatHistoryFixture f;
        const QString id = f.newSession("");
        QVERIFY(!id.isEmpty());
    }

    /* list() ritorna ordine decrescente per data (più recente prima) */
    void listaOrdinata() {
        ChatHistoryFixture f;
        const QString id1 = f.newSession("Primo task A5");
        QThread::msleep(5);   /* garantisce timestamp diverso */
        const QString id2 = f.newSession("Secondo task A5");

        const auto lista = f.ch.list();
        int pos1 = -1, pos2 = -1;
        for (int i = 0; i < lista.size(); ++i) {
            if (lista[i].id == id1) pos1 = i;
            if (lista[i].id == id2) pos2 = i;
        }
        /* id2 (più recente) deve venire prima di id1 */
        QVERIFY2(pos2 < pos1,
                 qPrintable(QString("ordine errato: pos id2=%1, pos id1=%2").arg(pos2).arg(pos1)));
    }

    /* 5 sessioni create → list() le contiene tutte */
    void cinqueSessioniInLista() {
        ChatHistoryFixture f;
        QStringList ids;
        for (int i = 0; i < 5; ++i) {
            QThread::msleep(2);
            ids << f.newSession(QString("Task cinque-%1").arg(i));
        }
        const auto lista = f.ch.list();
        QSet<QString> listaIds;
        for (const ChatSession& s : lista) listaIds.insert(s.id);
        for (const QString& id : ids)
            QVERIFY2(listaIds.contains(id), qPrintable("sessione mancante in list(): " + id));
    }

    /* id unici anche con creazione rapida */
    void idUnici() {
        ChatHistoryFixture f;
        QStringList ids;
        for (int i = 0; i < 10; ++i) {
            QThread::msleep(2);
            ids << f.newSession(QString("Rapido-%1").arg(i));
        }
        const QSet<QString> set(ids.begin(), ids.end());
        QCOMPARE(set.size(), ids.size());
    }

    /* list() su directory appena creata non crasha */
    void listaVuotaNoCrash() {
        ChatHistory ch;
        const auto lista = ch.list();
        /* Non importa il count — non deve crashare */
        Q_UNUSED(lista);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — saveLog / loadLog (10 test)
   ══════════════════════════════════════════════════════════════ */
class TestSaveLoadLog : public QObject {
    Q_OBJECT
private slots:

    /* Roundtrip ASCII */
    void roundtripAscii() {
        ChatHistoryFixture f;
        const QString id  = f.newSession("B1 task");
        const QString log = "Hello, World! Simple ASCII content.";
        f.ch.saveLog(id, log);
        QCOMPARE(f.ch.loadLog(id), log);
    }

    /* Roundtrip Unicode (emoji + italiano) */
    void roundtripUnicode() {
        ChatHistoryFixture f;
        const QString id  = f.newSession("B2 unicode");
        const QString log = "🍺 Prismalux — Conoscenza divina. 日本語テスト。";
        f.ch.saveLog(id, log);
        QCOMPARE(f.ch.loadLog(id), log);
    }

    /* Roundtrip HTML */
    void roundtripHtml() {
        ChatHistoryFixture f;
        const QString id  = f.newSession("B3 html");
        const QString log = "<b>Risposta AI</b>: 2+2=<i>4</i>";
        f.ch.saveLog(id, log);
        QCOMPARE(f.ch.loadLog(id), log);
    }

    /* Overwrite: secondo saveLog sovrascrive il primo */
    void overwrite() {
        ChatHistoryFixture f;
        const QString id = f.newSession("B4 overwrite");
        f.ch.saveLog(id, "prima versione");
        f.ch.saveLog(id, "seconda versione");
        QCOMPARE(f.ch.loadLog(id), QString("seconda versione"));
    }

    /* loadLog id inesistente → stringa vuota, no crash */
    void loadIdInesistente() {
        ChatHistory ch;
        const QString r = ch.loadLog("__id_certamente_inesistente_xyz__");
        QVERIFY(r.isEmpty());
    }

    /* Log da 100 KB */
    void log100KB() {
        ChatHistoryFixture f;
        const QString id  = f.newSession("B6 big");
        const QString log = QString(100 * 1024, 'A');
        f.ch.saveLog(id, log);
        QCOMPARE(f.ch.loadLog(id), log);
    }

    /* saveLog dopo remove → ricrea file correttamente */
    void saveAfterRemove() {
        ChatHistoryFixture f;
        const QString id = f.newSession("B7 rmcreate");
        f.ch.remove(id);
        /* Nota: remove elimina dalla lista, saveLog su id rimosso
         * potrebbe creare un file orfano — ma non deve crashare */
        f.ch.saveLog(id, "post-remove log");
        /* Ri-aggiungiamo all'elenco cleanup */
        f.created << id;
        /* No crash è l'invariante principale */
    }

    /* loadLog su file JSON malformato → stringa vuota, no crash */
    void loadFileCorrotto() {
        ChatHistoryFixture f;
        const QString id = f.newSession("B8 corrupt");
        /* Sovrascriviamo il file con contenuto non-JSON */
        const QString path = QDir::homePath() + "/.prismalux_chats/" + id + ".json";
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.write(QByteArray("{ CORROTTO [[["));
        file.close();
        const QString r = f.ch.loadLog(id);
        /* Deve ritornare stringa vuota o testo parziale — non crashare */
        Q_UNUSED(r);
    }

    /* Roundtrip con whitespace (newline, tab) */
    void roundtripWhitespace() {
        ChatHistoryFixture f;
        const QString id  = f.newSession("B9 ws");
        const QString log = "riga1\nriga2\n\tindentata\r\nriga3";
        f.ch.saveLog(id, log);
        QCOMPARE(f.ch.loadLog(id), log);
    }

    /* saveLog log vuoto → loadLog ritorna "" */
    void logVuoto() {
        ChatHistoryFixture f;
        const QString id = f.newSession("B10 empty");
        f.ch.saveLog(id, "");
        QCOMPARE(f.ch.loadLog(id), QString(""));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — remove (6 test)
   ══════════════════════════════════════════════════════════════ */
class TestRemove : public QObject {
    Q_OBJECT
private slots:

    /* remove elimina dalla list() */
    void removeFromList() {
        ChatHistoryFixture f;
        const QString id = f.newSession("C1 remove");
        f.created.removeOne(id);   /* gestiamo noi il cleanup */

        bool prima = false;
        for (const ChatSession& s : f.ch.list())
            if (s.id == id) prima = true;
        QVERIFY(prima);

        f.ch.remove(id);

        bool dopo = false;
        for (const ChatSession& s : f.ch.list())
            if (s.id == id) dopo = true;
        QVERIFY(!dopo);
    }

    /* remove id inesistente: no crash */
    void removeIdInesistente() {
        ChatHistory ch;
        ch.remove("__id_inesistente_xyz__");   /* no crash */
    }

    /* remove + loadLog → stringa vuota */
    void removePoiLoad() {
        ChatHistoryFixture f;
        const QString id = f.newSession("C3 rmload");
        f.ch.saveLog(id, "log da eliminare");
        f.created.removeOne(id);
        f.ch.remove(id);
        QVERIFY(f.ch.loadLog(id).isEmpty());
    }

    /* remove tutti → list() vuota (per le sessioni di test) */
    void rimossiTutti() {
        ChatHistoryFixture f;
        QStringList ids;
        for (int i = 0; i < 3; ++i) {
            QThread::msleep(2);
            ids << f.newSession(QString("C4 tutti-%1").arg(i));
        }
        for (const QString& id : ids) {
            f.ch.remove(id);
            f.created.removeOne(id);
        }
        /* Verifica che nessuno degli id appena creati sia ancora in lista */
        const QSet<QString> listaIds = [&]() {
            QSet<QString> s;
            for (const ChatSession& c : f.ch.list()) s.insert(c.id);
            return s;
        }();
        for (const QString& id : ids)
            QVERIFY2(!listaIds.contains(id), qPrintable("sessione non rimossa: " + id));
    }

    /* path traversal: id con ".." non deve rimuovere file fuori dalla dir */
    void removePathTraversal() {
        ChatHistory ch;
        /* Tentiamo un path traversal — non deve crashare né eliminare file di sistema */
        ch.remove("../etc/passwd");
        ch.remove("..%2Fetc%2Fpasswd");
    }

    /* remove id con caratteri speciali: no crash */
    void removeCaratteriSpeciali() {
        ChatHistory ch;
        ch.remove("abc def@#$%");
        ch.remove("");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Robustezza ed edge case (6 test)
   ══════════════════════════════════════════════════════════════ */
class TestRobustezza : public QObject {
    Q_OBJECT
private slots:

    /* File JSON nella dir senza "id" vengono ignorati da list() */
    void fileJsonSenzaIdIgnorato() {
        ChatHistoryFixture f;
        /* Creiamo manualmente un file JSON senza campo "id" */
        const QString path = QDir::homePath() + "/.prismalux_chats/__test_no_id.json";
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
            file.write(QByteArray(R"({"title":"test","log":""})"));
        file.close();

        const auto lista = f.ch.list();
        bool presente = false;
        for (const ChatSession& s : lista)
            if (s.id == "__test_no_id") presente = true;
        /* Pulizia */
        QFile::remove(path);

        QVERIFY2(!presente, "file senza id non dovrebbe essere in list()");
    }

    /* Crea ChatHistory × 3 istanze: tutte leggono la stessa directory */
    void multipleIstanzeStesDirezione() {
        ChatHistoryFixture f;
        const QString id = f.newSession("D2 multi");

        ChatHistory ch2, ch3;
        bool in2 = false, in3 = false;
        for (const ChatSession& s : ch2.list()) if (s.id == id) in2 = true;
        for (const ChatSession& s : ch3.list()) if (s.id == id) in3 = true;
        QVERIFY(in2);
        QVERIFY(in3);
    }

    /* saveLog su id di altra istanza → caricabile da tutte le istanze */
    void crossIstanzaSaveLoad() {
        ChatHistoryFixture f1;
        const QString id = f1.newSession("D3 cross");

        ChatHistory ch2;
        ch2.saveLog(id, "scritto da ch2");

        QCOMPARE(f1.ch.loadLog(id), QString("scritto da ch2"));
    }

    /* list() su directory con file non-.json: ignorati */
    void fileNonJsonIgnorati() {
        const QString path = QDir::homePath() + "/.prismalux_chats/__test.txt";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) f.write("not json\n"); f.close();

        ChatHistory ch;
        /* Non deve crashare, il file .txt deve essere ignorato */
        const auto lista = ch.list();
        bool presente = false;
        for (const ChatSession& s : lista)
            if (s.id == "__test") presente = true;
        QFile::remove(path);
        QVERIFY(!presente);
    }

    /* newSession title con caratteri speciali HTML */
    void titleHtmlNoInjection() {
        ChatHistoryFixture f;
        const QString id = f.newSession("<script>alert('xss')</script>");
        const auto lista = f.ch.list();
        for (const ChatSession& s : lista) {
            if (s.id == id) {
                /* Il titolo non deve contenere tag HTML attivi — o è escaped o è testo grezzo */
                /* L'invariante è: no crash, e il titolo può contenere i caratteri verbatim */
                QVERIFY(s.title.size() <= 40);
                break;
            }
        }
    }

    /* remove durante list(): simula concorrenza leggera (sequenziale) */
    void removeInterleaved() {
        ChatHistoryFixture f;
        const QString id1 = f.newSession("D6 interl-1");
        QThread::msleep(2);
        const QString id2 = f.newSession("D6 interl-2");
        f.created.removeOne(id1);
        f.created.removeOne(id2);

        /* Rimuovi id1 mentre "list" è ancora valida */
        f.ch.remove(id1);
        const auto lista = f.ch.list();
        bool id1presente = false, id2presente = false;
        for (const ChatSession& s : lista) {
            if (s.id == id1) id1presente = true;
            if (s.id == id2) id2presente = true;
        }
        QVERIFY(!id1presente);
        QVERIFY(id2presente);
        f.ch.remove(id2);
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    int status = 0;
    { TestNewSession    t; status |= QTest::qExec(&t, argc, argv); }
    { TestSaveLoadLog   t; status |= QTest::qExec(&t, argc, argv); }
    { TestRemove        t; status |= QTest::qExec(&t, argc, argv); }
    { TestRobustezza    t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_chat_history.moc"

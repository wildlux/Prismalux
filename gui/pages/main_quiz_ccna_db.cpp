#include "main_quiz_ccna_db.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QRandomGenerator>
#include <QDebug>

static constexpr char kConn[] = "ccna_db_conn";

struct QuizCcnaDb::Impl {
    bool ok = false;
};

QuizCcnaDb::QuizCcnaDb(QObject* parent)
    : QObject(parent), d(new Impl)
{
    const QString path = P::root() + "/../KNOWLEDGE_USER/quiz_ccna.db";
    /* Try standard prismalux path first, fall back to ~/.prismalux */
    QStringList candidates = {
        QDir::homePath() + "/.prismalux/quiz_ccna.db",
        path
    };

    if (QSqlDatabase::contains(kConn))
        QSqlDatabase::removeDatabase(kConn);

    auto db = QSqlDatabase::addDatabase("QSQLITE", kConn);
    db.setDatabaseName("");

    for (const QString& c : candidates) {
        if (QFile::exists(c)) {
            db.setDatabaseName(c);
            break;
        }
    }

    if (db.databaseName().isEmpty()) {
        qWarning() << "[QuizCcnaDb] quiz_ccna.db non trovato";
        return;
    }

    if (!db.open()) {
        qWarning() << "[QuizCcnaDb] open error:" << db.lastError().text();
        return;
    }

    QSqlQuery q(db);
    q.exec("PRAGMA journal_mode=WAL");
    d->ok = q.exec("SELECT COUNT(*) FROM questions") && q.next();
    if (!d->ok)
        qWarning() << "[QuizCcnaDb] tabella questions mancante";
}

QuizCcnaDb::~QuizCcnaDb()
{
    QSqlDatabase::removeDatabase(kConn);
    delete d;
}

bool QuizCcnaDb::isAvailable() const { return d && d->ok; }

int QuizCcnaDb::totalQuestions() const
{
    if (!isAvailable()) return 0;
    QSqlQuery q(QSqlDatabase::database(kConn));
    q.exec("SELECT COUNT(*) FROM questions");
    return q.next() ? q.value(0).toInt() : 0;
}

QStringList QuizCcnaDb::temi() const
{
    QStringList out;
    if (!isAvailable()) return out;
    QSqlQuery q(QSqlDatabase::database(kConn));
    q.exec("SELECT DISTINCT tema FROM questions ORDER BY tema");
    while (q.next()) out << q.value(0).toString();
    return out;
}

static CcnaQuestion rowToQuestion(QSqlQuery& q)
{
    CcnaQuestion c;
    c.domanda     = q.value(0).toString();
    c.risposte    = { q.value(1).toString(), q.value(2).toString(),
                      q.value(3).toString(), q.value(4).toString() };
    c.corretta    = q.value(5).toInt();
    c.spiegazione = q.value(6).toString();
    c.tema        = q.value(7).toString();
    return c;
}

static const char kSel[] =
    "SELECT domanda,r0,r1,r2,r3,corretta,spiegazione,tema FROM questions";

CcnaQuestion QuizCcnaDb::randomQuestion() const
{
    if (!isAvailable()) return {};
    QSqlQuery q(QSqlDatabase::database(kConn));
    q.exec(QString("%1 ORDER BY RANDOM() LIMIT 1").arg(kSel));
    if (q.next()) return rowToQuestion(q);
    return {};
}

CcnaQuestion QuizCcnaDb::randomByTema(const QString& tema) const
{
    if (!isAvailable()) return {};
    QSqlQuery q(QSqlDatabase::database(kConn));
    q.prepare(QString("%1 WHERE tema=? ORDER BY RANDOM() LIMIT 1").arg(kSel));
    q.addBindValue(tema);
    q.exec();
    if (q.next()) return rowToQuestion(q);
    return randomQuestion();
}

QList<CcnaQuestion> QuizCcnaDb::randomBatch(int n, const QString& tema) const
{
    QList<CcnaQuestion> out;
    if (!isAvailable() || n <= 0) return out;
    QSqlQuery q(QSqlDatabase::database(kConn));
    if (tema.isEmpty()) {
        q.exec(QString("%1 ORDER BY RANDOM() LIMIT %2").arg(kSel).arg(n));
    } else {
        q.prepare(QString("%1 WHERE tema=? ORDER BY RANDOM() LIMIT %2").arg(kSel).arg(n));
        q.addBindValue(tema);
        q.exec();
    }
    while (q.next()) out << rowToQuestion(q);
    return out;
}

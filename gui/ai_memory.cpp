#include "ai_memory.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDate>
#include <QDateTime>
#include <QProcess>
#include <QRegularExpression>

/* ──────────────────────────────────────────────────────────────────────────── */

AIMemory::AIMemory(QObject* parent)
    : QObject(parent)
    , m_root(QDir::homePath() + "/.ai-memory")
{}

/* ══════════════════════════════════════════════════════════════
   initialize — crea la struttura cartelle e il repo git (idempotente)
   ══════════════════════════════════════════════════════════════ */
bool AIMemory::initialize()
{
    QDir root(m_root);
    if (!root.mkpath(".")) {
        emit error("Impossibile creare " + m_root);
        return false;
    }
    ensureDirs();

    if (QDir(m_root + "/.git").exists()) {
        m_initialized = true;
        return true;
    }

    QString out;
    if (gitRun({"init"}, &out) != 0) {
        emit error("git init fallito: " + out);
        return false;
    }
    gitRun({"config", "user.name",  "Prismalux"});
    gitRun({"config", "user.email", "prismalux@local"});

    writeFile(m_root + "/profile/preferences.yaml",
              "# Preferenze apprese da Prismalux\n"
              "response_length: medium\n"
              "language: italiano\n");
    writeFile(m_root + "/profile/habits.yaml",
              "# Abitudini utente\n");
    writeFile(m_root + "/context/active_context.yaml",
              "# Contesto attivo corrente\ntopic: ~\n");
    writeFile(m_root + "/.gitignore", "*.tmp\n");

    gitCommitAll("init: Prismalux AIMemory inizializzata");
    m_initialized = true;
    return true;
}

/* ══════════════════════════════════════════════════════════════
   saveInteraction — registra coppia query/response nel diario MD
   ══════════════════════════════════════════════════════════════ */
void AIMemory::saveInteraction(const QString& query, const QString& response)
{
    if (!m_initialized) return;
    ensureDirs();

    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    const QString entry =
        QString("\n## %1\n**Utente:** %2\n\n**AI:** %3\n")
            .arg(ts, query, response);

    appendFile(todayInteractionPath(), entry);
    gitCommitAll(QString("interaction: %1")
                     .arg(query.left(60).simplified()));
}

/* ══════════════════════════════════════════════════════════════
   logFeedback — registra valutazione 👍/👎
   ══════════════════════════════════════════════════════════════ */
void AIMemory::logFeedback(const QString& query, const QString& response,
                           bool thumbsUp, const QString& reason)
{
    if (!m_initialized) return;
    ensureDirs();

    const QString ts     = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString rating = thumbsUp ? "thumbs_up" : "thumbs_down";
    const QString entry  = QString(
        "\n- timestamp: %1\n"
        "  rating: %2\n"
        "  query: \"%3\"\n"
        "  response_preview: \"%4\"\n"
        "  reason: \"%5\"\n")
        .arg(ts, rating,
             query.left(120).simplified().replace('"', '\''),
             response.left(80).simplified().replace('"', '\''),
             reason.simplified().replace('"', '\''));

    appendFile(todayFeedbackPath(), entry);
    gitCommitAll(QString("feedback: %1 — %2")
                     .arg(rating, query.left(50).simplified()));
}

/* ══════════════════════════════════════════════════════════════
   getRelevantContext — preferences.yaml + ultimi N feedback
   ══════════════════════════════════════════════════════════════ */
QString AIMemory::getRelevantContext(const QString& /*query*/, int maxFeedback) const
{
    if (!m_initialized) return {};
    QString ctx;

    const QString prefs = readFile(preferencesPath());
    if (!prefs.isEmpty())
        ctx += "# Preferenze utente\n" + prefs + "\n";

    const QString fb = readFile(todayFeedbackPath());
    if (!fb.isEmpty()) {
        QStringList lines = fb.split('\n');
        const int keep = maxFeedback * 6;
        if (lines.size() > keep)
            lines = lines.mid(lines.size() - keep);
        ctx += "# Feedback recenti\n" + lines.join('\n') + "\n";
    }
    return ctx;
}

/* ══════════════════════════════════════════════════════════════
   updatePreference — aggiorna chiave YAML in preferences
   ══════════════════════════════════════════════════════════════ */
void AIMemory::updatePreference(const QString& key, const QString& value)
{
    if (!m_initialized) return;
    setYamlKey(preferencesPath(), key, value);
    gitCommitAll(QString("pref: %1 = %2").arg(key, value));
}

/* ══════════════════════════════════════════════════════════════
   gitLog — ultimi N commit
   ══════════════════════════════════════════════════════════════ */
QStringList AIMemory::gitLog(int n) const
{
    QString out;
    gitRun({"log", "--oneline", QString("-n%1").arg(n)}, &out);
    return out.split('\n', Qt::SkipEmptyParts);
}

/* ══════════════════════════════════════════════════════════════
   revertFile — ripristina file a un commit specifico
   ══════════════════════════════════════════════════════════════ */
bool AIMemory::revertFile(const QString& commitHash, const QString& relativePath)
{
    static const QRegularExpression kSafeHash(R"(^[0-9a-f]{6,40}$)");
    if (!kSafeHash.match(commitHash).hasMatch()) {
        emit error("commitHash non valido: " + commitHash);
        return false;
    }
    QString out;
    const int rc = gitRun({"checkout", commitHash, "--", relativePath}, &out);
    if (rc == 0)
        gitCommitAll(QString("revert: %1 @ %2")
                         .arg(relativePath, commitHash.left(8)));
    else
        emit error("revertFile fallito: " + out);
    return rc == 0;
}

/* ──────────────────────────────────────────────────────────────────────────── */

int AIMemory::gitRun(const QStringList& args, QString* out) const
{
    QProcess p;
    p.setWorkingDirectory(m_root);
    p.start("git", args);
    if (!p.waitForFinished(10000)) {
        if (out) *out = "timeout";
        return -1;
    }
    if (out) *out = QString::fromUtf8(p.readAllStandardOutput())
                  + QString::fromUtf8(p.readAllStandardError());
    return p.exitCode();
}

bool AIMemory::gitCommitAll(const QString& message)
{
    gitRun({"add", "-A"});
    QString out;
    const int rc = gitRun({"commit", "-m", message}, &out);
    if (rc == 0)
        emit committed(message);
    /* rc != 0 è normale se "nothing to commit" — non emettiamo error */
    return rc == 0;
}

void AIMemory::ensureDirs() const
{
    const QDate today = QDate::currentDate();
    QDir().mkpath(m_root + "/profile");
    QDir().mkpath(m_root + "/context");
    QDir().mkpath(m_root + "/interactions/" + today.toString("yyyy-MM"));
}

QString AIMemory::preferencesPath() const
{
    return m_root + "/profile/preferences.yaml";
}

QString AIMemory::todayFeedbackPath() const
{
    const QDate today = QDate::currentDate();
    return m_root + "/interactions/" + today.toString("yyyy-MM") + "/feedback.yaml";
}

QString AIMemory::todayInteractionPath() const
{
    const QDate today = QDate::currentDate();
    return m_root + "/interactions/"
           + today.toString("yyyy-MM") + "/"
           + today.toString("dd") + ".md";
}

QString AIMemory::readFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QTextStream(&f).readAll();
}

void AIMemory::writeFile(const QString& path, const QString& content)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        QTextStream(&f) << content;
}

void AIMemory::appendFile(const QString& path, const QString& content)
{
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text))
        QTextStream(&f) << content;
}

void AIMemory::setYamlKey(const QString& path,
                          const QString& key, const QString& value)
{
    QString content = readFile(path);
    const QRegularExpression re(
        "^" + QRegularExpression::escape(key) + ":.*$",
        QRegularExpression::MultilineOption);
    const QString newLine = key + ": " + value;
    if (re.match(content).hasMatch())
        content.replace(re, newLine);
    else
        content += newLine + "\n";
    writeFile(path, content);
}

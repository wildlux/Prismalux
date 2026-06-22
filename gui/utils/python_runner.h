#pragma once
/*
 * python_runner.h — Wrapper asincrono per subprocess Python
 * ==========================================================
 * PythonRunner incapsula il pattern QProcess ripetuto ~15 volte nel codice:
 * - avvia Python con argomenti dati
 * - accumula stdout riga per riga emettendo output(line)
 * - gestisce timeout con QTimer emettendo timedOut()
 * - emette finished(exitCode) al termine
 *
 * Uso:
 *   auto* r = new PythonRunner(this);
 *   connect(r, &PythonRunner::output,   this, [this](const QString& l){ m_log->append(l); });
 *   connect(r, &PythonRunner::finished, this, [this](int code){ ... });
 *   connect(r, &PythonRunner::timedOut, this, [this]{ m_log->append("Timeout!"); });
 *   r->run({"-c", "print('ciao')"});
 *   // oppure: r->run({"/path/to/script.py", "--arg"}, 60000);
 */

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QStringList>
#include "../prismalux_paths.h"

namespace P = PrismaluxPaths;

class PythonRunner : public QObject
{
    Q_OBJECT
public:
    explicit PythonRunner(QObject* parent = nullptr)
        : QObject(parent)
        , m_proc(new QProcess(this))
        , m_timer(new QTimer(this))
    {
        m_timer->setSingleShot(true);

        connect(m_proc, &QProcess::readyReadStandardOutput, this, [this] {
            while (m_proc->canReadLine())
                emit output(QString::fromUtf8(m_proc->readLine()).trimmed());
        });
        connect(m_proc, &QProcess::readyReadStandardError, this, [this] {
            while (m_proc->canReadLine())
                emit output(QString::fromUtf8(m_proc->readLine()).trimmed());
        });
        connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
            emit output(tr("[Errore processo: %1]").arg(static_cast<int>(err)));
        });
        connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int code, QProcess::ExitStatus) {
            m_timer->stop();
            // leggi eventuale output residuo non ancora letto
            const QByteArray tail = m_proc->readAllStandardOutput();
            const QByteArray tailErr = m_proc->readAllStandardError();
            for (const QByteArray& chunk : {tail, tailErr}) {
                if (!chunk.isEmpty()) {
                    const auto lines = QString::fromUtf8(chunk).split('\n', Qt::SkipEmptyParts);
                    for (const QString& l : lines) emit output(l.trimmed());
                }
            }
            emit finished(code);
        });
        connect(m_timer, &QTimer::timeout, this, [this] {
            m_proc->kill();
            emit timedOut();
        });
    }

    /* Avvia Python con gli argomenti forniti. timeoutMs ≤ 0 = nessun timeout. */
    void run(const QStringList& args, int timeoutMs = P::kScriptTimeoutMs)
    {
        if (m_proc->state() != QProcess::NotRunning)
            m_proc->kill();

        if (timeoutMs > 0)
            m_timer->start(timeoutMs);

        m_proc->start(P::findPython(), args);
    }

    void kill() { m_timer->stop(); m_proc->kill(); }

    bool isRunning() const { return m_proc->state() != QProcess::NotRunning; }

signals:
    void output(const QString& line);
    void finished(int exitCode);
    void timedOut();

private:
    QProcess* m_proc;
    QTimer*   m_timer;
};

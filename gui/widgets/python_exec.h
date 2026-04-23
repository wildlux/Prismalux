#pragma once
/* ══════════════════════════════════════════════════════════════
   PythonExec — helper Q_OBJECT per eseguire script Python.

   Astrae la parte comune a AgentiPage (Tool Executor) e
   ProgrammazionePage (runCode):
     - scrittura codice in QTemporaryFile con nome casuale (anti-TOCTOU)
     - avvio QProcess con Python rilevato automaticamente
     - streaming output tramite segnale output(chunk)
     - pulizia file e processo al termine

   Uso:
       auto* run = new PythonExec(code, this);
       connect(run, &PythonExec::output,   this, [](const QString& chunk){ ... });
       connect(run, &PythonExec::finished, this, [](int exit, const QString& full){ ... });
       run->start();   // fire-and-forget; si auto-distrugge al termine

   Il caller può chiamare run->abort() per terminare il processo prima
   del completamento naturale.
   ══════════════════════════════════════════════════════════════ */
#include <QObject>
#include <QProcess>
#include <QTemporaryFile>
#include "../prismalux_paths.h"

class PythonExec : public QObject
{
    Q_OBJECT

public:
    explicit PythonExec(const QString& code,
                        QObject*       parent = nullptr)
        : QObject(parent)
        , m_code(code)
    {}

    /** Avvia l'esecuzione. Il file temporaneo viene creato qui. */
    bool start()
    {
        const QString python = PrismaluxPaths::findPython();

        /* Crea file temporaneo con nome casuale — immune a TOCTOU */
        m_tmpFile = new QTemporaryFile(
            PrismaluxPaths::safeTempPath() + "/prisma_exec_XXXXXX.py", this);
        m_tmpFile->setAutoRemove(true);  /* rimosso automaticamente alla distruzione */
        if (!m_tmpFile->open()) {
            emit finished(-1, "Impossibile creare file temporaneo");
            deleteLater();
            return false;
        }
        m_tmpFile->write(m_code.toUtf8());
        m_tmpFile->flush();
        /* Mantiene il file aperto per prevenire sostituzione (su Windows garantisce
           esclusività; su Linux riduce la finestra TOCTOU pur non eliminandola). */

        m_proc = new QProcess(this);
        m_proc->setProcessChannelMode(QProcess::MergedChannels);

        connect(m_proc, &QProcess::readyRead, this, [this]{
            const QString chunk = QString::fromUtf8(m_proc->readAll());
            m_fullOutput += chunk;
            emit output(chunk);
        });

        connect(m_proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int code, QProcess::ExitStatus){
            /* Leggi eventuale output residuo */
            const QString last = QString::fromUtf8(m_proc->readAll());
            if (!last.isEmpty()) { m_fullOutput += last; emit output(last); }
            emit finished(code, m_fullOutput);
            deleteLater();   /* auto-cleanup: file + processo */
        });

        connect(m_proc, &QProcess::errorOccurred,
                this, [this](QProcess::ProcessError err){
            if (err == QProcess::FailedToStart) {
                emit finished(-1, "Python non trovato: " +
                              PrismaluxPaths::findPython());
                deleteLater();
            }
        });

        m_proc->start(python, { m_tmpFile->fileName() });
        return true;
    }

    /** Termina il processo in anticipo. */
    void abort()
    {
        if (m_proc && m_proc->state() != QProcess::NotRunning) {
            m_proc->kill();
        }
    }

    /** Path del file temporaneo (utile per re-run con pip install). */
    QString scriptPath() const
    {
        return m_tmpFile ? m_tmpFile->fileName() : QString();
    }

signals:
    /** Emesso per ogni chunk di output (stdout+stderr merged). */
    void output(const QString& chunk);

    /**
     * Emesso al termine del processo.
     * @param exitCode  codice di uscita (0=successo, !=0=errore)
     * @param fullOutput tutto l'output accumulato dall'avvio
     */
    void finished(int exitCode, const QString& fullOutput);

private:
    QString         m_code;
    QString         m_fullOutput;
    QTemporaryFile* m_tmpFile = nullptr;
    QProcess*       m_proc    = nullptr;
};

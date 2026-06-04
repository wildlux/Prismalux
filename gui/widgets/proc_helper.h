#pragma once
/* ══════════════════════════════════════════════════════════════
   ProcHelper — esecuzione sincrona di processi esterni.

   Problema ripetuto: ~12+ file creano QProcess, start(), waitForFinished(),
   readAll() con codice identico. Centralizzare elimina la duplicazione
   e garantisce due proprietà di sicurezza:

   SICUREZZA:
   1. Usa sempre QProcess::start(program, QStringList) — MAI shell string.
      `/bin/sh -c "comando"` permetterebbe shell injection se `program`
      o gli `args` derivassero da input utente. Qui l'API non accetta
      stringhe shell — il chiamante deve separare programma e argomenti.
   2. Timeout obbligatorio — nessun processo può bloccare indefinitamente
      il thread UI.

   Uso:
       #include "widgets/proc_helper.h"

       auto r = ProcHelper::run("pdftotext", {path, "-"}, 10000);
       if (r.ok) { ... r.out ... }
       else      { ... r.err ... }

       // Con stdin (es. graphviz dot):
       auto r = ProcHelper::runWithInput("dot", {"-Tpng"}, dotSource, 15000);

       // Check disponibilità binario:
       if (!ProcHelper::isAvailable("whisper-cli")) { ... }
   ══════════════════════════════════════════════════════════════ */

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QProcess>

struct ProcResult {
    bool    ok  = false;   ///< true se waitForFinished + exitCode == 0
    QString out;           ///< stdout in UTF-8
    QString err;           ///< stderr in UTF-8 (utile per diagnostica)
    int     code = -1;     ///< exit code grezzo
};

class ProcHelper {
public:
    /* ── Esecuzione sincrona semplice ─────────────────────────────
       NOTA SICUREZZA: `program` deve essere un path o un nome binario
       fisso nel codice sorgente — mai un valore proveniente dall'utente
       o dall'output LLM. Gli `args` possono contenere valori utente
       perché QProcess NON passa per shell (nessuna espansione $VAR o glob).*/
    static ProcResult run(const QString&     program,
                          const QStringList& args,
                          int                timeoutMs = 10'000)
    {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::SeparateChannels);
        proc.start(program, args);

        ProcResult r;
        if (!proc.waitForStarted(3'000)) {
            r.err = QString("impossibile avviare '%1': %2")
                        .arg(program, proc.errorString());
            return r;
        }
        if (!proc.waitForFinished(timeoutMs)) {
            proc.kill();
            r.err = QString("'%1' timeout dopo %2 ms").arg(program).arg(timeoutMs);
            return r;
        }

        r.code = proc.exitCode();
        r.out  = QString::fromUtf8(proc.readAllStandardOutput());
        r.err  = QString::fromUtf8(proc.readAllStandardError());
        r.ok   = (r.code == 0);
        return r;
    }

    /* ── Con stdin (es. graphviz dot, openssl, whisper) ──────────  */
    static ProcResult runWithInput(const QString&     program,
                                   const QStringList& args,
                                   const QByteArray&  stdinData,
                                   int                timeoutMs = 15'000)
    {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::SeparateChannels);
        proc.start(program, args);

        ProcResult r;
        if (!proc.waitForStarted(3'000)) {
            r.err = QString("impossibile avviare '%1': %2")
                        .arg(program, proc.errorString());
            return r;
        }
        proc.write(stdinData);
        proc.closeWriteChannel();

        if (!proc.waitForFinished(timeoutMs)) {
            proc.kill();
            r.err = QString("'%1' timeout dopo %2 ms").arg(program).arg(timeoutMs);
            return r;
        }

        r.code = proc.exitCode();
        r.out  = QString::fromUtf8(proc.readAllStandardOutput());
        r.err  = QString::fromUtf8(proc.readAllStandardError());
        r.ok   = (r.code == 0);
        return r;
    }

    /* Overload con QString stdin (comodità) */
    static ProcResult runWithInput(const QString&     program,
                                   const QStringList& args,
                                   const QString&     stdinText,
                                   int                timeoutMs = 15'000)
    {
        return runWithInput(program, args, stdinText.toUtf8(), timeoutMs);
    }

    /* ── Verifica disponibilità di un binario nel PATH ───────────  */
    static bool isAvailable(const QString& program)
    {
        const auto r = run(program, {"--version"}, 2'000);
        /* exitCode può essere non-0 (es. --version ritorna 1) ma basta
           che il processo sia partito senza errore QProcess             */
        return r.code >= 0;
    }

    /* ── Legge output di un comando di stato breve ───────────────
       Convenienza per comandi tipo `ss -tlnp`, `git rev-parse`, ecc. */
    static QString readOutput(const QString&     program,
                              const QStringList& args,
                              int                timeoutMs = 5'000)
    {
        return run(program, args, timeoutMs).out.trimmed();
    }
};

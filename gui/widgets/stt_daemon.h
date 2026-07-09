#pragma once
/* ══════════════════════════════════════════════════════════════
   SttDaemon — client del demone STT persistente (stt_daemon.py).

   Il demone carica il modello faster-whisper UNA volta e trascrive
   via stdin/stdout (JSON newline): ogni frase costa la sola
   trascrizione (~2s su GPU) invece di avvio interprete + load
   modello (~9s) dello script one-shot.

   Uso (da SttWhisper::transcribe, trasparente per i chiamanti):
     SttDaemon::instance().ensureStarted(model);   // warm-up anticipato
     if (SttDaemon::instance().isUsable())
         SttDaemon::instance().transcribe(wav, "it", ctx, cb);

   - Singleton SENZA parent (regola ThemeManager: mai qApp come
     parent → ABRT allo shutdown); il processo figlio viene fermato
     da aboutToQuit.
   - Nessun Q_OBJECT: niente segnali propri, i connect usano lambda
     con context object (this / qApp), conformi alla regola progetto.
   - Se il demone muore a metà richiesta, le callback pendenti
     ricevono (messaggio, false) — il loop voce ritenta da solo.
   ══════════════════════════════════════════════════════════════ */
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QQueue>
#include <functional>
#include "../prismalux_paths.h"

class SttDaemon : public QObject {
public:
    static SttDaemon& instance() {
        static SttDaemon inst;   /* nessun parent — vedi header */
        return inst;
    }

    /** Path di default dello script demone (override nei test) */
    static QString defaultScript() {
        namespace P = PrismaluxPaths;
        return P::root() + "/Tools/scripts/stt_daemon.py";
    }

    /** true se il processo è vivo (anche se il modello sta ancora caricando:
     *  le richieste si accodano su stdin e partono appena pronto) */
    bool isUsable() const {
        return m_proc && m_proc->state() == QProcess::Running;
    }

    /** true se il demone ha completato il caricamento del modello */
    bool isReady() const { return isUsable() && m_ready; }

    /** Avvia (o riavvia se il modello è cambiato) il demone.
     *  Chiamarla all'inizio della REGISTRAZIONE: i 6-12s di parlato
     *  coprono il caricamento del modello in parallelo. */
    void ensureStarted(const QString& modelName,
                       const QString& scriptPath = QString()) {
        namespace P = PrismaluxPaths;
        const QString script = scriptPath.isEmpty() ? defaultScript()
                                                    : scriptPath;
        if (isUsable() && m_model == modelName && m_script == script)
            return;
        stop();
        const QString py = P::findPython();
        if (py.isEmpty() || !QFileInfo::exists(script)) return;

        m_model  = modelName;
        m_script = script;
        m_ready  = false;
        m_buf.clear();
        m_proc = new QProcess(this);
        m_proc->setProcessChannelMode(QProcess::SeparateChannels);
        connect(m_proc, &QProcess::readyReadStandardOutput,
                m_proc, [this] { onStdout(); });
        connect(m_proc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                m_proc, [this] { failPending("demone STT terminato"); });
        connect(m_proc, &QProcess::errorOccurred,
                m_proc, [this](QProcess::ProcessError err) {
                    if (err == QProcess::FailedToStart)
                        failPending("demone STT non avviato");
                });
        m_proc->start(py, { script, modelName });
    }

    /** Accoda una trascrizione; cb(text, ok) sul context ctx (se ancora
     *  vivo). text vuoto con ok=true = silenzio (VAD del demone). */
    void transcribe(const QString& wavPath, const QString& lang,
                    QObject* ctx,
                    std::function<void(const QString&, bool)> cb) {
        if (!isUsable()) { if (cb) cb("demone STT non attivo", false); return; }
        m_pending.enqueue({ QPointer<QObject>(ctx), std::move(cb) });
        QJsonObject req;
        req["wav"]  = wavPath;
        req["lang"] = lang;
        m_proc->write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
    }

    /** Ferma il demone (usata anche da aboutToQuit) */
    void stop() {
        if (!m_proc) return;
        QProcess* p = m_proc;
        m_proc = nullptr;
        failPending("demone STT fermato");
        p->disconnect();
        p->blockSignals(true);
        p->closeWriteChannel();          /* EOF → uscita pulita del demone */
        if (!p->waitForFinished(500)) {
            p->kill();
            p->waitForFinished(500);
        }
        p->deleteLater();
        m_ready = false;
    }

private:
    struct Pending {
        QPointer<QObject> ctx;
        std::function<void(const QString&, bool)> cb;
    };

    SttDaemon() : QObject(nullptr) {
        if (qApp)
            connect(qApp, &QCoreApplication::aboutToQuit,
                    this, [this] { stop(); });
    }

    void onStdout() {
        if (!m_proc) return;
        m_buf += m_proc->readAllStandardOutput();
        int nl;
        while ((nl = m_buf.indexOf('\n')) >= 0) {
            const QByteArray line = m_buf.left(nl).trimmed();
            m_buf.remove(0, nl + 1);
            if (line.isEmpty()) continue;
            const QJsonObject o = QJsonDocument::fromJson(line).object();
            if (o.contains("ready")) {
                m_ready = o["ready"].toBool();
                if (!m_ready) failPending(o["error"].toString());
                continue;
            }
            if (m_pending.isEmpty()) continue;   /* risposta orfana */
            Pending p = m_pending.dequeue();
            if (!p.ctx || !p.cb) continue;       /* chiamante morto */
            if (o["ok"].toBool())
                p.cb(o["text"].toString(), true);
            else
                p.cb(o["error"].toString(), false);
        }
    }

    void failPending(const QString& msg) {
        while (!m_pending.isEmpty()) {
            Pending p = m_pending.dequeue();
            if (p.ctx && p.cb) p.cb(msg, false);
        }
    }

    QProcess*       m_proc  = nullptr;
    bool            m_ready = false;
    QString         m_model;
    QString         m_script;
    QByteArray      m_buf;
    QQueue<Pending> m_pending;
};

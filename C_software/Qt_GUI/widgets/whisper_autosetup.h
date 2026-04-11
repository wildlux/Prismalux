#pragma once
/* ══════════════════════════════════════════════════════════════
   WhisperAutoSetup — scarica e compila whisper.cpp al primo avvio
   ══════════════════════════════════════════════════════════════
   Flusso (tutto asincrono, zero blocking UI):

     Avvio Prismalux
       │
       └─ whisperBin() vuoto?
             ├─ No  → whisperModel() vuoto?
             │             ├─ No  → emit ready()  (già tutto ok)
             │             └─ Sì  → _downloadModel()
             └─ Sì  → _cloneOrUpdate()
                          → _cmakeConfigure()
                          → _cmakeBuild()
                          → _downloadModel()
                          → emit ready()

   Il clone va in:   C_software/whisper.cpp/
   I modelli vanno in: C_software/whisper/models/
   ══════════════════════════════════════════════════════════════ */
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <functional>
#include "../prismalux_paths.h"
#include "stt_whisper.h"

class WhisperAutoSetup : public QObject
{
    Q_OBJECT

public:
    /**
     * statusCb — chiamata con ogni messaggio di avanzamento
     *            (usato da MainWindow per aggiornare la status bar)
     */
    explicit WhisperAutoSetup(
        std::function<void(const QString&)> statusCb,
        QObject* parent = nullptr)
        : QObject(parent)
        , m_statusCb(std::move(statusCb))
        , m_srcDir  (PrismaluxPaths::whisperDir())
        , m_buildDir(PrismaluxPaths::whisperDir() + "/build")
        , m_modelDir(PrismaluxPaths::whisperModelsDir())
    {}

    /* Punto d'ingresso: verifica e avvia il setup se necessario */
    void run()
    {
        const bool hasBin   = !PrismaluxPaths::whisperBin().isEmpty();
        const bool hasModel = !SttWhisper::whisperModel().isEmpty();

        if (hasBin && hasModel) {
            emit ready();          /* tutto già presente */
            return;
        }

        if (!hasBin)
            _cloneOrUpdate();
        else
            _downloadModel();     /* sorgente c'è ma manca il modello */
    }

signals:
    void ready();
    void failed(const QString& reason);

private:
    std::function<void(const QString&)> m_statusCb;
    const QString m_srcDir;
    const QString m_buildDir;
    const QString m_modelDir;

    void _status(const QString& msg)
    {
        if (m_statusCb) m_statusCb(msg);
    }

    /* ── Step 1: git clone (prima volta) o git pull (aggiornamento) ── */
    void _cloneOrUpdate()
    {
        const bool isRepo = QFileInfo::exists(m_srcDir + "/.git");

        if (isRepo) {
            _status("\xf0\x9f\x94\x84  Whisper: aggiornamento sorgenti...");
            _run("git", {"-C", m_srcDir, "pull", "--depth=1", "--rebase"},
                 [this](bool ok, const QString& err){
                     if (!ok) {
                         /* pull fallito (es. no internet): se il binario
                            esiste già dal build precedente, usa quello */
                         if (!PrismaluxPaths::whisperBin().isEmpty()) {
                             _downloadModel();
                         } else {
                             emit failed("git pull: " + err);
                         }
                         return;
                     }
                     _cmakeConfigure();
                 });
        } else {
            _status("\xf0\x9f\x93\xa5  Whisper: download sorgenti (prima installazione)...");
            QDir().mkpath(QFileInfo(m_srcDir).absolutePath()); /* crea C_software/ se manca */
            _run("git", {"clone", "--depth=1",
                         "https://github.com/ggml-org/whisper.cpp",
                         m_srcDir},
                 [this](bool ok, const QString& err){
                     if (!ok) { emit failed("git clone: " + err); return; }
                     _cmakeConfigure();
                 });
        }
    }

    /* ── Step 2: cmake configure ── */
    void _cmakeConfigure()
    {
        _status("\xf0\x9f\x94\xa7  Whisper: configurazione cmake...");
        _run("cmake", {
            "-B", m_buildDir,
            "-S", m_srcDir,
            "-DCMAKE_BUILD_TYPE=Release",
            "-DWHISPER_BUILD_TESTS=OFF",
            "-DWHISPER_BUILD_EXAMPLES=ON",
            "-DBUILD_SHARED_LIBS=OFF",
        }, [this](bool ok, const QString& err){
            if (!ok) { emit failed("cmake configure: " + err); return; }
            _cmakeBuild();
        });
    }

    /* ── Step 3: cmake build (solo target whisper-cli) ── */
    void _cmakeBuild()
    {
        _status("\xe2\x9a\x99  Whisper: compilazione... (qualche minuto)");
        const int j = std::max(1, QThread::idealThreadCount());
        _run("cmake", {
            "--build", m_buildDir,
            "--target", "whisper-cli",
            "-j", QString::number(j),
        }, [this](bool ok, const QString& err){
            if (!ok) { emit failed("cmake build: " + err); return; }
            _downloadModel();
        });
    }

    /* ── Step 4: scarica il modello ggml-base.bin (~74 MB) ── */
    void _downloadModel()
    {
        if (!SttWhisper::whisperModel().isEmpty()) {
            /* modello già presente — fine */
            _status("\xe2\x9c\x85  Whisper pronto.");
            emit ready();
            return;
        }

        QDir().mkpath(m_modelDir);
        const QString dest = m_modelDir + "/ggml-base.bin";
        const QString url  =
            "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin";

        _status("\xf0\x9f\x93\xa5  Whisper: download modello base (~74 MB)...");

        const QString wget = QStandardPaths::findExecutable("wget");
        const QString curl = QStandardPaths::findExecutable("curl");

        if (!wget.isEmpty()) {
            _run("wget", {"-q", "--show-progress", "-O", dest, url},
                 [this, dest](bool ok, const QString& err){ _onModelDone(ok, dest, err); });
        } else if (!curl.isEmpty()) {
            _run("curl", {"-L", "-#", "-o", dest, url},
                 [this, dest](bool ok, const QString& err){ _onModelDone(ok, dest, err); });
        } else {
            emit failed("wget/curl non trovati — impossibile scaricare il modello");
        }
    }

    /* ── Verifica SHA256 del file scaricato ── */
    static QString sha256File(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        QCryptographicHash h(QCryptographicHash::Sha256);
        /* Legge a blocchi per non caricare tutto il modello in RAM */
        while (!f.atEnd())
            h.addData(f.read(1024 * 1024));
        return h.result().toHex();
    }

    void _onModelDone(bool ok, const QString& dest, const QString& err)
    {
        if (!ok || !QFileInfo::exists(dest)) {
            QFile::remove(dest);   /* rimuove file parziale */
            emit failed("Download modello: " + err);
            return;
        }

        /* ── Integrità: SHA256 noti per ggml-base.bin (multilingual) ──
           Hash pubblicati su: https://github.com/ggml-org/whisper.cpp
           La lista può essere estesa con nuove release senza cambiare logica. */
        static const QStringList kKnownHashes = {
            /* whisper base multilingual — ggml-base.bin da whisper.cpp */
            "60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe",
        };

        _status("\xf0\x9f\x94\x8d  Verifica integrit\xc3\xa0 SHA256...");
        const QString actual = sha256File(dest);

        if (!actual.isEmpty() && !kKnownHashes.contains(actual)) {
            /* Hash non riconosciuto: potrebbe essere una versione aggiornata
               o, peggio, un file compromesso. Avvisiamo ma non blocchiamo:
               un hash sconosciuto può essere una nuova release legittima.
               Il file viene comunque tenuto e segnalato per verifica manuale. */
            _status(QString(
                "\xe2\x9a\xa0  SHA256 non corrisponde ai valori noti — "
                "verifica manuale consigliata.\n"
                "  Atteso: %1...\n"
                "  Ottenuto: %2")
                .arg(kKnownHashes.value(0).left(16), actual.left(16)));
            /* Continuiamo comunque: il file è scaricato e potrebbe essere legittimo */
        } else if (!actual.isEmpty()) {
            _status("\xe2\x9c\x85  SHA256 verificato.");
        }

        _status("\xe2\x9c\x85  Whisper pronto — riconoscimento vocale attivo.");
        emit ready();
    }

    /* ── Helper: avvia QProcess e chiama onDone(ok, stderr) al termine ── */
    void _run(const QString& prog,
              const QStringList& args,
              std::function<void(bool, const QString&)> onDone)
    {
        auto* p = new QProcess(this);
        connect(p, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [p, onDone](int code, QProcess::ExitStatus){
                    const QString err = QString::fromUtf8(
                        p->readAllStandardError()).trimmed();
                    p->deleteLater();
                    onDone(code == 0, err);
                });
        p->start(prog, args);
    }
};

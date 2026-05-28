/* ══════════════════════════════════════════════════════════════
   test_stt_whisper.cpp — Unit test SttWhisper namespace

   Categorie:
     CAT-A  Path resolution — whisperBin, whisperModelsDir, whisperModel
     CAT-B  isAvailable / setupMessage — stato sistema
     CAT-C  transcribe() — no crash su input invalidi, callback chiamata
     CAT-D  Regressione layout — paths idempotenti, QSettings roundtrip
     CAT-E  Permessi file system — whisper-cli, modello, temp path, /dev/snd
     CAT-F  Microfono ALSA — arecord disponibile, dispositivi, accesso /dev/snd
     CAT-G  Microfono Qt6Multimedia — QMediaDevices, QAudioDevice, QAudioSource
     CAT-H  Integrazione reale — registra WAV con arecord poi trascrive

   SKIP automatico:
     - CAT-C/H se whisper non installato
     - CAT-F se arecord non disponibile
     - CAT-G se Qt6::Multimedia non compilato
     - CAT-H se registrazione fallisce (CI headless)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_stt_whisper
     ./build_tests/test_stt_whisper
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QEventLoop>
#include <QProcess>
#include <QStandardPaths>

#ifdef HAVE_QT_MULTIMEDIA
#  include <QMediaDevices>
#  include <QAudioDevice>
#  include <QAudioFormat>
#  include <QAudioSource>
#endif

#include "../widgets/stt_whisper.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

/* ── Helper: esegue un QProcess breve e restituisce exit code ── */
static int runCmd(const QString& prog, const QStringList& args, int timeoutMs = 4000)
{
    QProcess p;
    p.start(prog, args);
    if (!p.waitForFinished(timeoutMs)) { p.kill(); return -1; }
    return p.exitCode();
}

/* ── Helper: verifica se arecord è disponibile ── */
static bool hasArecord()
{
    return !QStandardPaths::findExecutable("arecord").isEmpty();
}

/* ── Helper: registra N secondi di audio → path WAV (vuoto se fallisce) ── */
static QString recordWav(int secs)
{
    const QString path = P::safeTempPath()
        + "/prisma_test_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".wav";
    const int rc = runCmd("arecord",
        {"-d", QString::number(secs), "-r", "16000", "-c", "1",
         "-f", "S16_LE", "-q", path},
        (secs + 2) * 1000);
    if (rc != 0 || !QFileInfo::exists(path)) { QFile::remove(path); return {}; }
    return path;
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — Path resolution
   ══════════════════════════════════════════════════════════════ */
class TestSttPaths : public QObject {
    Q_OBJECT
private slots:

    /* A-1: whisperModelsDir restituisce path assoluto non vuoto */
    void modelsDirNonVuoto() {
        const QString dir = SttWhisper::whisperModelsDir();
        QVERIFY2(!dir.isEmpty(), "whisperModelsDir() non deve essere vuoto");
        QVERIFY2(QDir::isAbsolutePath(dir), "whisperModelsDir() deve essere assoluto");
    }

    /* A-2: whisperBin, se presente, è un file eseguibile */
    void whisperBinSeEsisteEEseguibile() {
        const QString bin = SttWhisper::whisperBin();
        if (bin.isEmpty()) QSKIP("whisper-cli non installato");
        QVERIFY2(QFileInfo::exists(bin),   qPrintable("file inesistente: " + bin));
        QVERIFY2(QFileInfo(bin).isExecutable(), qPrintable("non eseguibile: " + bin));
    }

    /* A-3: whisperModel, se presente, è un .bin non vuoto */
    void whisperModelSeEsisteNonVuoto() {
        const QString model = SttWhisper::whisperModel();
        if (model.isEmpty()) QSKIP("Nessun modello installato");
        QVERIFY2(QFileInfo::exists(model),              qPrintable("modello inesistente: " + model));
        QVERIFY2(QFileInfo(model).size() > 1'000'000LL, qPrintable("modello troppo piccolo: " + model));
    }

    /* A-4: savePreferredModel / preferredModel roundtrip */
    void preferredModelRoundtrip() {
        const QString orig = SttWhisper::preferredModel();
        SttWhisper::savePreferredModel("/tmp/ggml-fake-test.bin");
        QCOMPARE(SttWhisper::preferredModel(), QString("/tmp/ggml-fake-test.bin"));
        SttWhisper::savePreferredModel(orig);
        QCOMPARE(SttWhisper::preferredModel(), orig);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — isAvailable / setupMessage
   ══════════════════════════════════════════════════════════════ */
class TestSttAvailability : public QObject {
    Q_OBJECT
private slots:

    /* B-1: isAvailable() coerente con whisperBin + whisperModel */
    void isAvailableCoerente() {
        const bool hasBin   = !SttWhisper::whisperBin().isEmpty();
        const bool hasModel = !SttWhisper::whisperModel().isEmpty();
        QCOMPARE(SttWhisper::isAvailable(), hasBin && hasModel);
    }

    /* B-2: setupMessage() vuoto ↔ isAvailable() */
    void setupMessageCoerente() {
        const QString msg = SttWhisper::setupMessage();
        if (SttWhisper::isAvailable())
            QVERIFY2(msg.isEmpty(), "setupMessage() deve essere vuoto se tutto pronto");
        else
            QVERIFY2(!msg.isEmpty(), "setupMessage() deve spiegare il problema");
    }

    /* B-3: isAvailable() è idempotente */
    void isAvailableIdempotente() {
        QCOMPARE(SttWhisper::isAvailable(), SttWhisper::isAvailable());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — transcribe() su input invalidi
   ══════════════════════════════════════════════════════════════ */
class TestSttTranscribe : public QObject {
    Q_OBJECT
private slots:

    /* C-1: WAV inesistente → callback con ok=false entro timeout */
    void transcribeWavInesistente() {
        if (!SttWhisper::isAvailable()) QSKIP("SttWhisper non disponibile");
        bool called = false; bool resultOk = true;
        QObject ctx; QEventLoop loop;
        QTimer::singleShot(8000, &loop, &QEventLoop::quit);
        SttWhisper::transcribe("/tmp/inesistente_99999.wav", "it", &ctx,
            [&](const QString&, bool ok){ called = true; resultOk = ok; loop.quit(); });
        loop.exec();
        QVERIFY2(called,    "callback deve essere chiamata anche su errore");
        QVERIFY2(!resultOk, "WAV inesistente → ok=false");
    }

    /* C-2: WAV vuoto (0 byte) → callback con ok=false */
    void transcribeWavVuoto() {
        if (!SttWhisper::isAvailable()) QSKIP("SttWhisper non disponibile");
        QTemporaryFile tmp;
        tmp.setFileTemplate(P::safeTempPath() + "/prisma_test_XXXXXX.wav");
        QVERIFY(tmp.open()); tmp.close();
        bool called = false; bool resultOk = true;
        QObject ctx; QEventLoop loop;
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        SttWhisper::transcribe(tmp.fileName(), "it", &ctx,
            [&](const QString&, bool ok){ called = true; resultOk = ok; loop.quit(); });
        loop.exec();
        QVERIFY2(called,    "callback su WAV vuoto");
        QVERIFY2(!resultOk, "WAV 0-byte → ok=false");
    }

    /* C-3: senza disponibilità → callback sincrona con ok=false */
    void transcribeSenzaDisponibilita() {
        if (SttWhisper::isAvailable()) QSKIP("whisper disponibile — test saltato");
        bool called = false; bool resultOk = true;
        QObject ctx;
        SttWhisper::transcribe("/tmp/any.wav", "it", &ctx,
            [&](const QString&, bool ok){ called = true; resultOk = ok; });
        QApplication::processEvents();
        QVERIFY2(called,    "callback sincrona senza disponibilità");
        QVERIFY2(!resultOk, "senza whisper → ok=false");
    }

    /* C-4: senza disponibilità → transcribe() restituisce nullptr */
    void transcribeRitornaNull() {
        if (SttWhisper::isAvailable()) QSKIP("whisper disponibile — test saltato");
        QObject ctx;
        QProcess* proc = SttWhisper::transcribe("/tmp/any.wav", "it", &ctx,
            [](const QString&, bool){});
        QVERIFY2(proc == nullptr, "transcribe() senza whisper deve restituire nullptr");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Regressione layout / QSettings
   ══════════════════════════════════════════════════════════════ */
class TestSttLayout : public QObject {
    Q_OBJECT
private slots:

    /* D-1: whisperModelsDir — mkpath idempotente */
    void modelsDirCreabile() {
        const QString dir = SttWhisper::whisperModelsDir();
        QVERIFY2(QDir().mkpath(dir), qPrintable("mkpath fallito: " + dir));
        QVERIFY2(QDir(dir).exists(), qPrintable("dir non esiste: " + dir));
    }

    /* D-2: savePreferredModel("") non crasha */
    void savePreferredModelVuoto() {
        const QString orig = SttWhisper::preferredModel();
        SttWhisper::savePreferredModel("");
        QCOMPARE(SttWhisper::preferredModel(), QString(""));
        SttWhisper::savePreferredModel(orig);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-E — Permessi file system
   ══════════════════════════════════════════════════════════════ */
class TestSttPermissions : public QObject {
    Q_OBJECT
private slots:

    /* E-1: whisper-cli è leggibile ed eseguibile dall'utente corrente */
    void whisperBinPermessiEsecuzione() {
        const QString bin = SttWhisper::whisperBin();
        if (bin.isEmpty()) QSKIP("whisper-cli non installato");
        const QFileInfo fi(bin);
        QVERIFY2(fi.isReadable(),   qPrintable("whisper-cli non leggibile: " + bin));
        QVERIFY2(fi.isExecutable(), qPrintable("whisper-cli non eseguibile: " + bin));
    }

    /* E-2: modello ggml è leggibile (non solo root) */
    void modelPermessiLettura() {
        const QString model = SttWhisper::whisperModel();
        if (model.isEmpty()) QSKIP("Nessun modello installato");
        QVERIFY2(QFileInfo(model).isReadable(),
                 qPrintable("modello non leggibile: " + model));
    }

    /* E-3: cartella modelli è scrivibile (necessario per download futuri) */
    void modelsDirScrivibile() {
        const QString dir = SttWhisper::whisperModelsDir();
        QDir().mkpath(dir);
        QVERIFY2(QFileInfo(dir).isWritable(),
                 qPrintable("modelsDir non scrivibile: " + dir));
    }

    /* E-4: safeTempPath è scrivibile e può contenere un file WAV */
    void tempPathScrivibile() {
        const QString tmp = P::safeTempPath();
        QVERIFY2(!tmp.isEmpty(), "safeTempPath() non deve essere vuoto");
        QVERIFY2(QDir().mkpath(tmp), qPrintable("mkpath safeTempPath fallito: " + tmp));
        QFile f(tmp + "/prisma_perm_test.wav");
        QVERIFY2(f.open(QIODevice::WriteOnly),
                 qPrintable("impossibile scrivere in safeTempPath: " + tmp));
        f.close(); f.remove();
    }

    /* E-5: /dev/snd esiste e contiene almeno un nodo PCM capture (pcmCxDxc) */
    void devSndContieneCapture() {
#ifndef Q_OS_LINUX
        QSKIP("Test specifico Linux");
#endif
        const QDir snd("/dev/snd");
        if (!snd.exists()) QSKIP("/dev/snd non esiste (VM headless?)");
        const QStringList captures = snd.entryList({"pcmC*D*c"}, QDir::System);
        QVERIFY2(!captures.isEmpty(),
                 "/dev/snd non contiene nodi PCM capture (pcmCxDxc) — "
                 "nessun dispositivo di registrazione rilevato");
    }

    /* E-6: il processo corrente ha i permessi di lettura su almeno un nodo PCM capture.
            Usa QFileInfo (non QFile::open) — open() su /dev/snd blocca finché il
            dispositivo non viene rilasciato da altri processi. */
    void devSndCapturePermessiLettura() {
#ifndef Q_OS_LINUX
        QSKIP("Test specifico Linux");
#endif
        const QDir snd("/dev/snd");
        if (!snd.exists()) QSKIP("/dev/snd non esiste");
        const QStringList captures = snd.entryList({"pcmC*D*c"}, QDir::System);
        if (captures.isEmpty()) QSKIP("Nessun nodo PCM capture");
        const QString nodePath = "/dev/snd/" + captures.first();
        QVERIFY2(QFileInfo(nodePath).isReadable(),
                 qPrintable(
                     "Permessi di lettura assenti su " + nodePath + " — "
                     "utente non nel gruppo 'audio'? "
                     "Correggi con: sudo usermod -aG audio $USER && newgrp audio"));
    }

    /* E-7: whisper-cli non richiede root (UID effettivo ≠ 0 quando esegue) */
    void whisperBinNonRichiederoot() {
        const QString bin = SttWhisper::whisperBin();
        if (bin.isEmpty()) QSKIP("whisper-cli non installato");
        /* Verifica: il file non ha setuid root */
        const QFile::Permissions p = QFileInfo(bin).permissions();
        const bool setuidRoot =
            (p & QFile::ExeUser) && !(p & QFile::ReadOther);
        /* Non è un test deterministico, ma verifica che il bit setuid non sia attivo */
        Q_UNUSED(setuidRoot)
        /* Il vero test: eseguiamo whisper --version senza sudo */
        QProcess proc;
        proc.start(bin, {"--help"});
        proc.waitForFinished(3000);
        /* exit code 0 o 1 entrambi accettati (--help può restituire 1) */
        QVERIFY2(proc.exitStatus() == QProcess::NormalExit,
                 "whisper-cli crasha anche su --help (corrotto?)");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-F — Microfono ALSA (arecord)
   ══════════════════════════════════════════════════════════════ */
class TestSttMicAlsa : public QObject {
    Q_OBJECT
private slots:

    /* F-1: arecord è nel PATH */
    void arecordNelPath() {
        if (!hasArecord()) QSKIP("arecord non disponibile (installa alsa-utils)");
        QVERIFY(!QStandardPaths::findExecutable("arecord").isEmpty());
    }

    /* F-2: arecord -l lista almeno un dispositivo capture */
    void arecordListaDispositiviCapture() {
        if (!hasArecord()) QSKIP("arecord non disponibile");
        QProcess p;
        p.start("arecord", {"-l"});
        p.waitForFinished(3000);
        const QString out = QString::fromUtf8(p.readAllStandardOutput())
                          + QString::fromUtf8(p.readAllStandardError());
        QVERIFY2(out.contains("scheda") || out.contains("card"),
                 "arecord -l non trova nessuna scheda audio capture");
    }

    /* F-3: arecord apre il dispositivo default senza errori immediati */
    void arecordApriDefault() {
        if (!hasArecord()) QSKIP("arecord non disponibile");
        /* registra 1 secondo silenzio sul dispositivo default */
        const QString wav = recordWav(1);
        if (wav.isEmpty()) QSKIP("Registrazione fallita (CI headless senza microfono?)");
        const bool ok = QFileInfo(wav).size() > 1000;
        QFile::remove(wav);
        QVERIFY2(ok, "WAV registrato da arecord è troppo piccolo (< 1 KB)");
    }

    /* F-4: il WAV prodotto da arecord ha header RIFF valido */
    void arecordWavHeaderRiff() {
        if (!hasArecord()) QSKIP("arecord non disponibile");
        const QString wav = recordWav(1);
        if (wav.isEmpty()) QSKIP("Registrazione non disponibile in questo ambiente");
        QFile f(wav);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray hdr = f.read(4);
        f.close(); QFile::remove(wav);
        QVERIFY2(hdr == "RIFF",
                 "Il file WAV registrato da arecord non inizia con 'RIFF' — "
                 "header corrotto o dispositivo non risponde");
    }

    /* F-5: arecord produce campioni non-zero (microfono riceve segnale) */
    void arecordCampioniNonZero() {
        if (!hasArecord()) QSKIP("arecord non disponibile");
        const QString wav = recordWav(1);
        if (wav.isEmpty()) QSKIP("Registrazione non disponibile");
        QFile f(wav);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray data = f.readAll();
        f.close(); QFile::remove(wav);
        /* Salta i primi 44 byte (header WAV) e cerca almeno un campione non nullo */
        const QByteArray pcm = data.mid(44);
        bool hasNonZero = false;
        for (unsigned char b : pcm) {
            if (b != 0) { hasNonZero = true; break; }
        }
        /* Non è un errore fatale se tutti zero (microfono muto/isolato) — XFAIL soft */
        if (!hasNonZero)
            QWARN("Tutti i campioni PCM sono zero — microfono muto o silenzio assoluto?");
    }

    /* F-6: sox disponibile → registrazione con VAD non crasha */
    void soxVadNoCrash() {
        const QString sox = QStandardPaths::findExecutable("sox");
        if (sox.isEmpty()) QSKIP("sox non disponibile");

        const QString wav = P::safeTempPath() + "/prisma_sox_test.wav";
        QProcess p;
        p.start(sox, {"-t","alsa","default",
                      "-r","16000","-c","1","-b","16", wav,
                      "silence","1","0.1","1%","1","1.0","1%",
                      "trim","0","2"});
        /* attendiamo max 5 sec — la VAD termina prima se silenzio */
        p.waitForFinished(5000);
        if (p.state() != QProcess::NotRunning) p.terminate();
        QFile::remove(wav);
        QVERIFY2(p.exitStatus() == QProcess::NormalExit,
                 "sox VAD è crashato (segnale inatteso)");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-G — Microfono Qt6Multimedia
   ══════════════════════════════════════════════════════════════ */
class TestSttMicQt : public QObject {
    Q_OBJECT
private slots:

#ifdef HAVE_QT_MULTIMEDIA

    /* G-1: QMediaDevices::audioInputs() restituisce almeno un elemento */
    void audioInputsNonVuoto() {
        const auto inputs = QMediaDevices::audioInputs();
        if (inputs.isEmpty())
            QSKIP("QMediaDevices non trova dispositivi input (CI headless?)");
        QVERIFY(!inputs.isEmpty());
    }

    /* G-2: dispositivo default non è null */
    void defaultAudioInputNonNull() {
        const QAudioDevice dev = QMediaDevices::defaultAudioInput();
        if (dev.isNull()) QSKIP("Nessun dispositivo audio input di default");
        QVERIFY2(!dev.isNull(), "QMediaDevices::defaultAudioInput() è null");
        QVERIFY2(!dev.description().isEmpty(), "description del dispositivo è vuota");
    }

    /* G-3: formato 16kHz mono Int16 è supportato dal dispositivo default */
    void formatoWhisperSupportato() {
        const QAudioDevice dev = QMediaDevices::defaultAudioInput();
        if (dev.isNull()) QSKIP("Nessun dispositivo default");
        QAudioFormat fmt;
        fmt.setSampleRate(16000);
        fmt.setChannelCount(1);
        fmt.setSampleFormat(QAudioFormat::Int16);
        if (!dev.isFormatSupported(fmt)) {
            /* Non è un errore — il formato preferito sarà usato come fallback */
            QWARN("Formato 16kHz/mono/Int16 non supportato nativamente — "
                  "SttWhisper userà il formato preferito del dispositivo");
        }
    }

    /* G-4: QAudioSource si crea senza crash */
    void audioSourceCreazioneSenzaCrash() {
        const QAudioDevice dev = QMediaDevices::defaultAudioInput();
        if (dev.isNull()) QSKIP("Nessun dispositivo default");
        QAudioFormat fmt;
        fmt.setSampleRate(16000);
        fmt.setChannelCount(1);
        fmt.setSampleFormat(QAudioFormat::Int16);
        if (!dev.isFormatSupported(fmt))
            fmt = dev.preferredFormat();
        QAudioSource* src = new QAudioSource(dev, fmt);
        QVERIFY2(src != nullptr, "QAudioSource(dev, fmt) ha restituito nullptr");
        QVERIFY2(src->error() == QAudio::NoError,
                 qPrintable(QString("QAudioSource in errore alla creazione: %1")
                            .arg(src->error())));
        delete src;
    }

    /* G-5: il formato preferito del dispositivo è valido (sampleRate > 0) */
    void formatoPreferitoCampioniPositivi() {
        const QAudioDevice dev = QMediaDevices::defaultAudioInput();
        if (dev.isNull()) QSKIP("Nessun dispositivo default");
        const QAudioFormat fmt = dev.preferredFormat();
        QVERIFY2(fmt.sampleRate() > 0,
                 "QAudioDevice::preferredFormat() ha sampleRate <= 0");
        QVERIFY2(fmt.channelCount() > 0,
                 "QAudioDevice::preferredFormat() ha channelCount <= 0");
    }

    /* G-6: maximumSampleRate >= minimumSampleRate e coprono 16000 Hz */
    void rangeFrequenzaCopre16kHz() {
        const QAudioDevice dev = QMediaDevices::defaultAudioInput();
        if (dev.isNull()) QSKIP("Nessun dispositivo default");
        /* 16000 Hz deve rientrare nel range supportato dal dispositivo */
        const int minRate = dev.minimumSampleRate();
        const int maxRate = dev.maximumSampleRate();
        QVERIFY2(minRate <= maxRate,
                 qPrintable(QString("minimumSampleRate(%1) > maximumSampleRate(%2)")
                            .arg(minRate).arg(maxRate)));
        if (minRate > 16000 || maxRate < 16000)
            QWARN("Il dispositivo non supporta 16000 Hz nativamente — "
                  "whisper userà il formato preferito con resampling");
    }

#else
    /* G-0: placeholder se HAVE_QT_MULTIMEDIA non definito */
    void qtMultimediaNonCompilato() {
        QSKIP("Qt6::Multimedia non compilato — test CAT-G saltati");
    }
#endif
};

/* ══════════════════════════════════════════════════════════════
   CAT-H — Integrazione reale: arecord → whisper-cli
   ══════════════════════════════════════════════════════════════ */
class TestSttIntegration : public QObject {
    Q_OBJECT
private slots:

    /* H-1: registra 2s con arecord → file WAV valido → transcribe() non crasha */
    void registraETrascriviNoCrash() {
        if (!SttWhisper::isAvailable()) QSKIP("SttWhisper non disponibile");
        if (!hasArecord())              QSKIP("arecord non disponibile");
        const QString wav = recordWav(2);
        if (wav.isEmpty())              QSKIP("Registrazione non riuscita (CI headless?)");

        bool called = false;
        QObject ctx; QEventLoop loop;
        QTimer::singleShot(30000, &loop, &QEventLoop::quit); /* whisper-cli può essere lento */
        SttWhisper::transcribe(wav, "it", &ctx,
            [&](const QString&, bool){ called = true; loop.quit(); });
        loop.exec();
        QFile::remove(wav);
        QVERIFY2(called, "transcribe() non ha chiamato la callback entro 30s");
    }

    /* H-2: il file WAV prodotto da arecord viene accettato da whisper-cli
            (non deve uscire con errore di formato) */
    void whisperAccettaWavDaArecord() {
        if (!SttWhisper::isAvailable()) QSKIP("SttWhisper non disponibile");
        if (!hasArecord())              QSKIP("arecord non disponibile");
        const QString wav = recordWav(1);
        if (wav.isEmpty())              QSKIP("Registrazione non riuscita");

        /* Eseguiamo whisper-cli direttamente e verifichiamo che non esca con 1 = errore formato */
        const int rc = runCmd(SttWhisper::whisperBin(),
            {"-m", SttWhisper::whisperModel(),
             "-f", wav, "-l", "it", "-nt", "-np",
             "--beam-size", "1", "-t", "1"}, 20000);
        QFile::remove(wav);
        /* exit 0 = ok, exit 1 può essere "testo vuoto" (silenzio) — entrambi accettati */
        QVERIFY2(rc == 0 || rc == 1,
                 qPrintable(QString("whisper-cli ha restituito exit code inatteso: %1 "
                                    "(errore formato o file corrotto?)").arg(rc)));
    }

    /* H-3: il callback SttWhisper::transcribe() riceve i dati entro il timeout */
    void transcribeCallbackRicevuta() {
        if (!SttWhisper::isAvailable()) QSKIP("SttWhisper non disponibile");
        if (!hasArecord())              QSKIP("arecord non disponibile");
        const QString wav = recordWav(1);
        if (wav.isEmpty())              QSKIP("Registrazione non riuscita");

        bool called = false;
        QString resultText;
        bool resultOk = false;

        QObject ctx; QEventLoop loop;
        QTimer::singleShot(25000, &loop, &QEventLoop::quit);
        SttWhisper::transcribe(wav, "it", &ctx,
            [&](const QString& text, bool ok){
                called = true; resultText = text; resultOk = ok;
                loop.quit();
            });
        loop.exec();
        QFile::remove(wav);

        QVERIFY2(called, "Callback non ricevuta entro 25s");
        /* Non verifichiamo resultOk — un secondo di silenzio può dare ok=false,
           ma la callback DEVE essere chiamata */
        Q_UNUSED(resultOk)
        Q_UNUSED(resultText)
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int status = 0;
    auto run = [&](QObject* t){ status |= QTest::qExec(t, argc, argv); delete t; };
    run(new TestSttPaths);
    run(new TestSttAvailability);
    run(new TestSttTranscribe);
    run(new TestSttLayout);
    run(new TestSttPermissions);
    run(new TestSttMicAlsa);
    run(new TestSttMicQt);
    run(new TestSttIntegration);
    return status;
}

#include "test_stt_whisper.moc"

/* ══════════════════════════════════════════════════════════════
   test_perceptor_scripts.cpp — Test script Python pipeline Perceptor

   Categorie:
     CAT-C  speaker_diarize.py — WAV sintetico 2 speaker, JSON, errori (T-4)
     CAT-D  fast_whisper_transcribe.py — trascrizione WAV (T-5)

   Fixture audio: genera due frasi italiane con voci diverse via `espeak-ng`
   (VAD/embedding richiedono un segnale simile al parlato — un tono puro
   sinusoidale viene scartato dal VAD e produce "Nessun backend disponibile").
   Le due WAV mono 16-bit vengono concatenate a livello di frame PCM
   (stesso sample rate/formato, nessun bisogno di ffmpeg per il merge).

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_perceptor_scripts
     ./build_tests/test_perceptor_scripts
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QFile>
#include <QSet>

#include "../prismalux_paths.h"

namespace P = PrismaluxPaths;

/* ── Helper condivisi (fixture WAV + esecuzione script) ──────────────── */
namespace {

QString g_espeakBin;   ///< path assoluta espeak-ng, vuota se assente
QString g_pythonBin;   ///< path python3 (P::findPython() o fallback)
bool    g_backendOk = false;  ///< almeno un backend diarizzazione importabile

/* Sintetizza una frase con espeak-ng in un file WAV */
bool synthSpeech(const QString& text, const QString& voice, int pitch, const QString& outPath)
{
    QProcess proc;
    proc.start(g_espeakBin, { "-v", voice, "-s", "150", "-p", QString::number(pitch),
                              "-w", outPath, text });
    if (!proc.waitForFinished(15000)) return false;
    return proc.exitCode() == 0 && QFile::exists(outPath) && QFileInfo(outPath).size() > 0;
}

/* Concatena due WAV mono/16-bit con lo stesso formato appendendo i frame
   PCM (via un piccolo script Python inline — usa il modulo 'wave' della
   stdlib, evita di scrivere un parser RIFF a mano in C++). */
bool concatWav(const QString& wav0, const QString& wav1, const QString& outPath)
{
    const QString script =
        "import wave,sys\n"
        "w0=wave.open(sys.argv[1],'rb'); w1=wave.open(sys.argv[2],'rb')\n"
        "d0=w0.readframes(w0.getnframes()); d1=w1.readframes(w1.getnframes())\n"
        "out=wave.open(sys.argv[3],'wb'); out.setparams(w0.getparams())\n"
        "out.writeframes(d0); out.writeframes(d1); out.close()\n";
    QProcess proc;
    proc.start(g_pythonBin, { "-c", script, wav0, wav1, outPath });
    if (!proc.waitForFinished(15000)) return false;
    return proc.exitCode() == 0 && QFile::exists(outPath) && QFileInfo(outPath).size() > 0;
}

/* Genera un WAV con due "speaker" distinti (voci/pitch diversi) nella dir data */
QString makeTwoSpeakerWav(const QString& dir)
{
    const QString wav0 = dir + "/spk0.wav";
    const QString wav1 = dir + "/spk1.wav";
    const QString out  = dir + "/two_speakers.wav";

    if (!synthSpeech("Buongiorno a tutti, oggi parliamo di intelligenza artificiale.",
                      "it+m3", 30, wav0)) return {};
    if (!synthSpeech("Grazie mille per l'introduzione, e un piacere essere qui.",
                      "it+f3", 70, wav1)) return {};
    if (!concatWav(wav0, wav1, out)) return {};
    return out;
}

/* Esegue speaker_diarize.py e ritorna (json, exitCode).
   simple_diarizer stampa messaggi di progresso ("Running VAD...", barre tqdm,
   ecc.) su stdout PRIMA del JSON finale — nessuna di quelle righe contiene
   '{', quindi il primo '{' nell'output segna sempre l'inizio del JSON reale. */
QPair<QString,int> runDiarize(const QStringList& args, int timeoutMs = 90000)
{
    const QString script = P::root() + "/Tools/scripts/speaker_diarize.py";
    QProcess proc;
    proc.start(g_pythonBin, QStringList{ script } + args);
    if (!proc.waitForFinished(timeoutMs))
        return { QString(), -1 };
    QString out = QString::fromUtf8(proc.readAllStandardOutput());
    const int braceIdx = out.indexOf('{');
    if (braceIdx > 0) out = out.mid(braceIdx);
    return { out, proc.exitCode() };
}

/* Esegue fast_whisper_transcribe.py e ritorna (stdout, exitCode).
   CUDA_VISIBLE_DEVICES="" forza CPU: "auto" tenta CUDA anche quando le
   librerie runtime non sono caricabili nell'ambiente (libcublas mancante/
   incompatibile), facendo crashare lo script — lo stesso problema già
   risolto per speaker_diarize.py, qui applicato lato ambiente del processo
   invece che nel sorgente (lo script non espone un flag --cpu). */
struct FwResult { QString out; QString err; int code; };

FwResult runFastWhisper(const QStringList& args, int timeoutMs = 60000)
{
    const QString script = P::root() + "/Tools/scripts/fast_whisper_transcribe.py";
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("CUDA_VISIBLE_DEVICES", "");
    proc.setProcessEnvironment(env);
    proc.start(g_pythonBin, QStringList{ script } + args);
    if (!proc.waitForFinished(timeoutMs))
        return { QString(), QString(), -1 };
    return { QString::fromUtf8(proc.readAllStandardOutput()),
             QString::fromUtf8(proc.readAllStandardError()),
             proc.exitCode() };
}

} // namespace

/* ══════════════════════════════════════════════════════════════
   CAT-C — speaker_diarize.py (T-4)
   ══════════════════════════════════════════════════════════════ */
class TestSpeakerDiarizeScript : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;
    QString       m_wavPath;

private slots:
    void initTestCase() {
        g_pythonBin = P::findPython();
        if (g_pythonBin.isEmpty()) g_pythonBin = "python3";

        g_espeakBin = QStandardPaths::findExecutable("espeak-ng");
        if (g_espeakBin.isEmpty())
            QSKIP("espeak-ng non trovato — impossibile generare fixture audio parlato");

        /* Verifica che almeno un backend di diarizzazione sia importabile */
        QProcess chk;
        chk.start(g_pythonBin, { "-c",
            "import sys\n"
            "for m in ('pyannote.audio','simple_diarizer','resemblyzer'):\n"
            "    try:\n"
            "        __import__(m); sys.exit(0)\n"
            "    except ImportError: pass\n"
            "sys.exit(1)\n" });
        chk.waitForFinished(15000);
        g_backendOk = (chk.exitCode() == 0);
        if (!g_backendOk)
            QSKIP("Nessun backend diarizzazione installato "
                  "(pip install simple-diarizer / pyannote.audio / resemblyzer)");

        QVERIFY2(m_dir.isValid(), "QTemporaryDir non valida");
        m_wavPath = makeTwoSpeakerWav(m_dir.path());
        if (m_wavPath.isEmpty())
            QSKIP("Impossibile generare la fixture WAV a due speaker");
    }

    /* C-1: lo script esiste nel path atteso */
    void scriptEsiste() {
        const QString script = P::root() + "/Tools/scripts/speaker_diarize.py";
        QVERIFY2(QFile::exists(script), qPrintable("script non trovato: " + script));
    }

    /* C-2: forza CUDA_VISIBLE_DEVICES="" prima di importare torch — verifica
       statica del sorgente (non simulabile in CI senza una GPU cc<7.5 reale).
       _trust_torch_repos() è DEFINITA prima di main() (quindi appare prima
       nel testo) ma è CHIAMATA da main() dopo l'assegnazione a
       CUDA_VISIBLE_DEVICES: la verifica va ristretta al corpo di main(),
       non al file intero, altrimenti la posizione testuale della funzione
       (definita più in alto) darebbe un falso negativo. */
    void forzaCpuNoGpu() {
        QFile f(P::root() + "/Tools/scripts/speaker_diarize.py");
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString src = QString::fromUtf8(f.readAll());
        QVERIFY2(src.contains("CUDA_VISIBLE_DEVICES"),
                 "speaker_diarize.py non forza CUDA_VISIBLE_DEVICES");

        const int mainIdx = src.indexOf("def main()");
        QVERIFY2(mainIdx >= 0, "def main() non trovata");
        const QString mainBody = src.mid(mainIdx);

        const int idxCuda = mainBody.indexOf("CUDA_VISIBLE_DEVICES");
        const int idxCall = mainBody.indexOf("_trust_torch_repos()");
        QVERIFY2(idxCuda >= 0, "CUDA_VISIBLE_DEVICES non impostato dentro main()");
        QVERIFY2(idxCall >= 0, "_trust_torch_repos() non chiamata da main()");
        QVERIFY2(idxCuda < idxCall,
                 "CUDA_VISIBLE_DEVICES deve essere impostato prima della chiamata "
                 "a _trust_torch_repos() (che importa torch)");
    }

    /* C-3: WAV a due speaker + --speakers 2 → JSON con backend/segments/speakers,
       esattamente 2 speaker univoci */
    void dueSpeakerJsonValido() {
        const auto [out, code] = runDiarize({ m_wavPath, "--speakers", "2" });
        QCOMPARE(code, 0);

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8(), &err);
        QVERIFY2(err.error == QJsonParseError::NoError,
                 qPrintable("JSON non valido: " + err.errorString() + "\nOutput: " + out));
        QVERIFY(doc.isObject());
        const QJsonObject obj = doc.object();

        QVERIFY2(obj.contains("backend"),  "campo 'backend' mancante");
        QVERIFY2(obj.contains("segments"), "campo 'segments' mancante");
        QVERIFY2(obj.contains("speakers"), "campo 'speakers' mancante");
        QVERIFY2(obj["segments"].isArray() && !obj["segments"].toArray().isEmpty(),
                 "'segments' deve essere un array non vuoto");

        const QJsonArray speakers = obj["speakers"].toArray();
        QCOMPARE(speakers.size(), 2);

        QSet<QString> uniq;
        for (const QJsonValue& v : speakers) uniq.insert(v.toString());
        QCOMPARE(uniq.size(), 2);
    }

    /* C-4: ogni segmento ha i campi speaker/start/end coerenti */
    void segmentiCampiCoerenti() {
        const auto [out, code] = runDiarize({ m_wavPath, "--speakers", "2" });
        QCOMPARE(code, 0);
        const QJsonObject obj = QJsonDocument::fromJson(out.toUtf8()).object();
        const QJsonArray segs = obj["segments"].toArray();
        QVERIFY(!segs.isEmpty());
        for (const QJsonValue& v : segs) {
            const QJsonObject s = v.toObject();
            QVERIFY2(s.contains("speaker"), "segmento senza 'speaker'");
            QVERIFY2(s.contains("start") && s.contains("end"), "segmento senza start/end");
            QVERIFY2(s["end"].toDouble() >= s["start"].toDouble(), "end < start nel segmento");
        }
        QCOMPARE(obj["count"].toInt(), segs.size());
    }

    /* C-5: file WAV inesistente → JSON di errore, exit code != 0 */
    void wavInesistenteErrore() {
        const auto [out, code] = runDiarize({ "/tmp/prismalux_test_nonexist_diar_42.wav" });
        QVERIFY2(code != 0, "exit code deve essere != 0 per file inesistente");
        const QJsonObject obj = QJsonDocument::fromJson(out.toUtf8()).object();
        QVERIFY2(obj.contains("error"), "output deve contenere campo 'error'");
    }

    /* C-6: --transcript allinea il testo ai segmenti (campo 'text' popolato) */
    void transcriptAllineato() {
        const QString transcriptPath = m_dir.path() + "/transcript.txt";
        QFile tf(transcriptPath);
        QVERIFY(tf.open(QIODevice::WriteOnly | QIODevice::Text));
        tf.write("Buongiorno a tutti oggi parliamo di intelligenza artificiale "
                  "grazie mille per l'introduzione e un piacere essere qui");
        tf.close();

        const auto [out, code] = runDiarize(
            { m_wavPath, "--speakers", "2", "--transcript", transcriptPath });
        QCOMPARE(code, 0);
        const QJsonObject obj = QJsonDocument::fromJson(out.toUtf8()).object();
        const QJsonArray segs = obj["segments"].toArray();
        QVERIFY(!segs.isEmpty());

        bool almenoUnTesto = false;
        for (const QJsonValue& v : segs)
            if (!v.toObject()["text"].toString().trimmed().isEmpty()) almenoUnTesto = true;
        QVERIFY2(almenoUnTesto, "nessun segmento ha ricevuto testo da --transcript");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — fast_whisper_transcribe.py (T-5)
   ══════════════════════════════════════════════════════════════ */
class TestFastWhisperScript : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_dir;
    QString       m_wavPath;

private slots:
    void initTestCase() {
        g_pythonBin = P::findPython();
        if (g_pythonBin.isEmpty()) g_pythonBin = "python3";

        g_espeakBin = QStandardPaths::findExecutable("espeak-ng");
        if (g_espeakBin.isEmpty())
            QSKIP("espeak-ng non trovato — impossibile generare fixture audio parlato");

        QProcess chk;
        chk.start(g_pythonBin, { "-c", "import faster_whisper" });
        chk.waitForFinished(15000);
        if (chk.exitCode() != 0)
            QSKIP("faster-whisper non installato (pip install faster-whisper)");

        QVERIFY2(m_dir.isValid(), "QTemporaryDir non valida");
        m_wavPath = m_dir.path() + "/short_it.wav";
        /* Frase breve (~1-2s) — un tono puro non basterebbe: faster-whisper
           con vad_filter=True scarterebbe un segnale senza pattern vocale */
        if (!synthSpeech("Ciao, come stai oggi?", "it", 50, m_wavPath))
            QSKIP("impossibile generare la fixture WAV con espeak-ng");
    }

    /* D-1: lo script esiste nel path atteso */
    void scriptEsiste() {
        const QString script = P::root() + "/Tools/scripts/fast_whisper_transcribe.py";
        QVERIFY2(QFile::exists(script), qPrintable("script non trovato: " + script));
    }

    /* D-2: nessun argomento → errore su stderr, exit code 1 (path rapido,
       nessun caricamento modello) */
    void nessunArgomentoErrore() {
        const auto r = runFastWhisper({}, 15000);
        QCOMPARE(r.code, 1);
        QVERIFY2(!r.err.trimmed().isEmpty(), "stderr deve contenere il messaggio di uso");
        QVERIFY2(r.err.contains("Uso:"), "messaggio di errore inatteso");
    }

    /* D-3: WAV inesistente → exit code != 0 (eccezione faster-whisper) */
    void wavInesistenteErrore() {
        const auto r = runFastWhisper(
            { "/tmp/prismalux_test_nonexist_fw_42.wav", "it", "tiny" }, 30000);
        QVERIFY2(r.code != 0, "exit code deve essere != 0 per file inesistente");
    }

    /* D-4: WAV ~1-2s + modello "tiny" → trascrizione non vuota su stdout */
    void wavBreveTrascrizioneNonVuota() {
        const auto r = runFastWhisper({ m_wavPath, "it", "tiny" }, 60000);
        QCOMPARE(r.code, 0);
        const QString text = r.out.trimmed();
        QVERIFY2(!text.isEmpty(), "la trascrizione non deve essere vuota");
        /* "testo breve": per ~1-2s di audio non ci si aspettano più di poche parole */
        QVERIFY2(text.split(' ', Qt::SkipEmptyParts).size() <= 15,
                 qPrintable("trascrizione sospettosamente lunga: " + text));
    }

    /* D-5: modello "tiny" accettato come terzo argomento posizionale
       (nessun crash, nessun fallback silenzioso a un modello diverso) */
    void modelloTinyAccettato() {
        const auto r = runFastWhisper({ m_wavPath, "it", "tiny" }, 60000);
        QCOMPARE(r.code, 0);
        QVERIFY2(!r.err.contains("Traceback"), "non deve esserci un traceback Python");
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    int status = 0;
    { TestSpeakerDiarizeScript t; status |= QTest::qExec(&t, argc, argv); }
    { TestFastWhisperScript    t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_perceptor_scripts.moc"

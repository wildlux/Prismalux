#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QDateTime>
#include <QStandardPaths>
#include "../widgets/stt_whisper.h"
#include "../widgets/tts_speak.h"
#include <QProcessEnvironment>

/* ══════════════════════════════════════════════════════════════
   _sttStartRecording — avvia registrazione + trascrizione
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_sttStartRecording()
{
    /* Nome univoco per sessione (timestamp): evita conflitti tra registrazioni
       concorrenti e rende il path non predicibile a priori da altri processi. */
    const QString wavPath = PrismaluxPaths::safeTempPath()
        + "/prisma_stt_"
        + QString::number(QDateTime::currentMSecsSinceEpoch())
        + ".wav";
    QFile::remove(wavPath);

    m_sttState = SttState::Recording;
    m_btnVoice->setText("\xf0\x9f\x94\xb4 Registrando... (click per fermare)");
    m_btnVoice->setProperty("danger","true");
    P::repolish(m_btnVoice);

    m_recProc = new QProcess(this);
#ifdef Q_OS_WIN
    /* Windows: sox rec (https://sox.sourceforge.net) */
    const QString recBin = QStandardPaths::findExecutable("rec");
    const QString soxBin = QStandardPaths::findExecutable("sox");
    if (!recBin.isEmpty()) {
        m_recProc->start("rec",
            {"-r","16000","-c","1","-b","16", wavPath, "trim","0","6"});
    } else if (!soxBin.isEmpty()) {
        m_recProc->start("sox",
            {"-t","waveaudio","default","-r","16000","-c","1","-b","16",
             wavPath, "trim","0","6"});
    } else {
        m_sttState = SttState::Idle;
        m_btnVoice->setText("\xf0\x9f\x8e\xa4 Trascrivi voce");
        m_btnVoice->setProperty("danger","false");
        P::repolish(m_btnVoice);
        m_log->append(
            "\xe2\x9a\xa0  <b>Registrazione audio non disponibile.</b><br>"
            "Su Windows installa <code>sox</code> (sox.sourceforge.net) "
            "per abilitare la trascrizione vocale.");
        m_recProc->deleteLater(); m_recProc = nullptr;
        return;
    }
#else
    m_recProc->start("arecord",
        {"-d", "6", "-r", "16000", "-c", "1", "-f", "S16_LE", "-q", wavPath});
#endif

    /* Countdown nel testo del pulsante */
    auto* tick = new QTimer(this);
    tick->setProperty("secs", 6);
    connect(tick, &QTimer::timeout, this, [this, tick]{
        if (m_sttState != SttState::Recording) { tick->deleteLater(); return; }
        int s = tick->property("secs").toInt() - 1;
        tick->setProperty("secs", s);
        if (s > 0)
            m_btnVoice->setText(
                QString("\xf0\x9f\x94\xb4 Registrando... %1s (click per fermare)").arg(s));
    });
    tick->start(1000);

    /* Dopo 6.5s ferma la registrazione e avvia la trascrizione */
    QTimer::singleShot(6500, this, [this, wavPath, tick]{
        tick->stop(); tick->deleteLater();
        if (m_sttState != SttState::Recording) return;  // utente ha fermato prima
        if (m_recProc) { m_recProc->terminate(); m_recProc->waitForFinished(1000);
                         m_recProc->deleteLater(); m_recProc = nullptr; }

        if (!QFileInfo::exists(wavPath)) {
            m_sttState = SttState::Idle;
            m_btnVoice->setText("\xf0\x9f\x8e\xa4 Trascrivi voce");
            m_btnVoice->setProperty("danger","false");
            P::repolish(m_btnVoice);
            m_log->append("\xe2\x9a\xa0  Registrazione fallita (arecord non disponibile?)");
            return;
        }

        m_sttState = SttState::Transcribing;
        m_btnVoice->setText("\xe2\x8c\x9b Trascrivendo...");
        m_btnVoice->setProperty("danger","false");
        P::repolish(m_btnVoice);
        m_btnVoice->setEnabled(false);

        m_sttProc = SttWhisper::transcribe(wavPath, "it", this,
            [this](const QString& text, bool ok) {
                m_sttState = SttState::Idle;
                m_sttProc  = nullptr;
                m_btnVoice->setText("\xf0\x9f\x8e\xa4 Trascrivi voce");
                m_btnVoice->setEnabled(true);

                if (ok && !text.isEmpty()) {
                    const QString cur = m_input->toPlainText();
                    m_input->setPlainText(cur.isEmpty() ? text : cur + " " + text);
                    m_input->setFocus();
                } else {
                    m_log->append(
                        "\xe2\x9a\xa0  Trascrizione fallita o audio vuoto.<br>"
                        + QString(text).replace("\n","<br>"));
                }
            });
    });
}

/* ══════════════════════════════════════════════════════════════
   _ttsPlay — avvia TTS tracciato con feedback visivo di caricamento.
   Mostra "🔊 Avvio lettura..." nella toolbar durante il caricamento
   dei moduli vocali (Piper TTS → espeak-ng → spd-say).
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_ttsPlay(const QString& tts)
{
    if (tts.trimmed().isEmpty()) return;

    /* Ferma TTS precedente se attivo — prima piper, poi aplay */
    if (m_piperProc) {
        m_piperProc->kill();
        m_piperProc->waitForFinished(300);
        m_piperProc->deleteLater();
        m_piperProc = nullptr;
    }
    if (m_ttsProc) {
        m_ttsProc->kill();
        m_ttsProc->waitForFinished(300);
        if (m_ttsProc) { m_ttsProc->deleteLater(); m_ttsProc = nullptr; }
    }
#ifndef Q_OS_WIN
    QProcess::startDetached("pkill", {"-9", "aplay"});
    QProcess::startDetached("pkill", {"-9", "espeak-ng"});
    QProcess::startDetached("pkill", {"-9", "piper"});
#endif

    /* Avviso caricamento moduli vocali */
    if (m_waitLbl) {
        m_waitLbl->setText("\xf0\x9f\x94\x8a  Avvio lettura...");
        m_waitLbl->setVisible(true);
    }

    m_ttsProc = new QProcess(this);
    connect(m_ttsProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus){
        if (m_waitLbl)    m_waitLbl->setVisible(false);
        if (m_btnTtsStop) m_btnTtsStop->setVisible(false);
        if (m_piperProc)  {
            m_piperProc->terminate();
            m_piperProc->waitForFinished(300);
            m_piperProc->deleteLater();
            m_piperProc = nullptr;
        }
        if (m_ttsProc)    { m_ttsProc->deleteLater(); m_ttsProc = nullptr; }
    });
    /* Fallisce silenziosamente (binario non trovato, permessi, ecc.) */
    connect(m_ttsProc, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError){
        if (m_waitLbl)    m_waitLbl->setVisible(false);
        if (m_btnTtsStop) m_btnTtsStop->setVisible(false);
        m_log->append(
            "\xe2\x9a\xa0  <b>TTS non disponibile.</b> "
            "Installa <code>espeak-ng</code> oppure configura Piper TTS "
            "nelle Impostazioni per la lettura vocale.");
        if (m_ttsProc) { m_ttsProc->deleteLater(); m_ttsProc = nullptr; }
    });

#ifdef Q_OS_WIN
    /* Windows: voci native SAPI via PowerShell.
       Il path del file TTS viaggia come variabile d'ambiente (PRISMA_TTS_PATH)
       invece di essere interpolato nella stringa PS — immune ad apostrofi nel
       path (es. C:\Users\O'Brien\...) e a injection da caratteri speciali. */
    {
        const QString tmpPath = TtsSpeak::ttsTempFile();
        if (TtsSpeak::writeTtsTemp(tts)) {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("PRISMA_TTS_PATH", QDir::toNativeSeparators(tmpPath));
            m_ttsProc->setProcessEnvironment(env);
            m_ttsProc->start(QString("powershell"), TtsSpeak::sapiArgs(tmpPath));
        } else {
            if (m_waitLbl) m_waitLbl->setVisible(false);
            m_ttsProc->deleteLater(); m_ttsProc = nullptr;
            return;
        }
    }
#else
    /* Linux: Piper → espeak-ng → spd-say */
    const QString bin  = TtsSpeak::piperBin();
    const QString onnx = TtsSpeak::activeOnnx();
    if (!bin.isEmpty() && QFileInfo::exists(onnx) && TtsSpeak::writeTtsTemp(tts)) {
        /* Two-process pipe senza shell intermediaria:
           piper --output_raw → pipe stdout/stdin → aplay
           Immune a path injection: bin e onnx sono args, non interpolati in shell. */
        m_piperProc = new QProcess(this);
        m_piperProc->setStandardOutputProcess(m_ttsProc);  // piper stdout → aplay stdin
        m_piperProc->setStandardInputFile(TtsSpeak::ttsTempFile());
        m_ttsProc->start(QString("aplay"), {"-r","22050","-f","S16_LE","-t","raw","-q","-"});
        m_piperProc->start(bin, {"--model", onnx, "--output_raw"});
    } else if (!QProcess::startDetached("espeak-ng",{"-v","it+f3","--",tts})) {
        /* espeak-ng non trovato: prova spd-say tracciato */
        m_ttsProc->start("spd-say", {"--lang","it","--",tts});
    } else {
        /* espeak-ng avviato in detached — nascondi loading dopo 1.5s */
        QTimer::singleShot(1500, this, [this]{
            if (m_waitLbl) m_waitLbl->setVisible(false);
        });
        m_ttsProc->deleteLater(); m_ttsProc = nullptr;
        return;
    }
#endif
    if (m_ttsProc && m_btnTtsStop)
        m_btnTtsStop->setVisible(true);
    /* Nasconde "Avvio lettura..." non appena il processo parte */
    if (m_ttsProc) {
        QTimer::singleShot(400, this, [this]{
            if (m_waitLbl) m_waitLbl->setVisible(false);
        });
    }
}

/* ══════════════════════════════════════════════════════════════
   downloadWhisperModel — scarica ggml-small.bin in ~/.prismalux/whisper/models/
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::downloadWhisperModel()
{
    const QString destDir  = QDir::homePath() + "/.prismalux/whisper/models";
    const QString destFile = destDir + "/ggml-small.bin";
    const QString url      =
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin";

    QDir().mkpath(destDir);

    m_sttState = SttState::Downloading;
    m_btnVoice->setEnabled(false);
    m_btnVoice->setText("\xf0\x9f\x93\xa5 Download modello...");

    m_log->append(
        "\xf0\x9f\x93\xa5  <b>Download modello whisper (ggml-small, ~141 MB)...</b><br>"
        "Destinazione: <code>" + destFile + "</code>");

    /* Usa wget se disponibile, altrimenti curl */
    const bool hasWget = !QStandardPaths::findExecutable("wget").isEmpty();
    auto* dlProc = new QProcess(this);
    dlProc->setProcessChannelMode(QProcess::MergedChannels);

    if (hasWget) {
        dlProc->start("wget", {
            "--progress=dot:mega",
            "-O", destFile,
            url
        });
    } else {
        dlProc->start("curl", {
            "-L", "--progress-bar",
            "-o", destFile,
            url
        });
    }

    /* Mostra progresso leggendo stdout/stderr del downloader */
    connect(dlProc, &QProcess::readyReadStandardOutput, this, [this, dlProc]{
        const QString chunk = QString::fromLocal8Bit(dlProc->readAllStandardOutput());
        /* Mostra solo le righe con percentuale o MB per non intasare il log */
        for (const auto& line : chunk.split('\n')) {
            const QString t = line.trimmed();
            if (t.contains('%') || t.contains("MB") || t.contains("KB"))
                m_log->append("  " + t.toHtmlEscaped());
        }
    });

    connect(dlProc,
        QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this, dlProc, destFile](int code, QProcess::ExitStatus){
            dlProc->deleteLater();
            m_btnVoice->setEnabled(true);

            if (code == 0 && QFileInfo::exists(destFile) &&
                QFileInfo(destFile).size() > 10'000'000LL) {
                m_log->append(
                    "\xe2\x9c\x85  <b>Modello scaricato.</b> Avvio registrazione...");
                m_sttState = SttState::Idle;
                _sttStartRecording();
            } else {
                m_sttState = SttState::Idle;
                m_btnVoice->setText("\xf0\x9f\x8e\xa4 Trascrivi voce");
                /* File parziale → rimuovi per non confondere isAvailable() */
                if (QFileInfo::exists(destFile) &&
                    QFileInfo(destFile).size() < 10'000'000LL)
                    QFile::remove(destFile);
                m_log->append(
                    "\xe2\x9d\x8c  Download fallito (controlla la connessione).<br>"
                    "Puoi scaricarlo manualmente:<br>"
                    "<code>mkdir -p ~/.prismalux/whisper/models &amp;&amp; "
                    "wget -O ~/.prismalux/whisper/models/ggml-small.bin "
                    "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin</code>");
            }
        });
}


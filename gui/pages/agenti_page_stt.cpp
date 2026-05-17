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
    /* Durata registrazione: voice loop usa 12s per dare tempo di parlare;
       la modalità pulsante singolo usa 6s come prima.
       Se sox è disponibile, usa VAD silenzio-automatico (nessun countdown). */
    const bool hasSox  = !QStandardPaths::findExecutable("sox").isEmpty();
    const int  recSecs = m_voiceLoopActive ? 12 : 6;

    if (hasSox) {
        /* sox VAD: registra fino a 2s di silenzio, max recSecs*2 secondi.
           silence 1 0.1 1% = inizia a registrare non appena c'è segnale;
           1 2.0 1%         = ferma dopo 2s di silenzio sotto l'1% del picco. */
        m_recProc->start("sox",
            {"-t", "alsa", "default",
             "-r", "16000", "-c", "1", "-b", "16",
             wavPath,
             "silence", "1", "0.1", "1%",
                        "1", "2.0", "1%",
             "trim", "0", QString::number(recSecs * 2)});
    } else {
        m_recProc->start("arecord",
            {"-d", QString::number(recSecs),
             "-r", "16000", "-c", "1", "-f", "S16_LE", "-q", wavPath});
    }
#endif

    /* sox VAD: quando il processo termina da solo (silenzio rilevato),
       avanza subito alla trascrizione senza aspettare il timeout completo. */
    connect(m_recProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AgentiPage::onRecProcFinished);

    /* Countdown nel testo del pulsante — slot esplicito, nessuna lambda con raw pointer */
    m_sttWavPath = wavPath;
    m_sttTick = new QTimer(this);
    m_sttTick->setProperty("secs", recSecs);
    connect(m_sttTick, &QTimer::timeout, this, &AgentiPage::onSttTick);
    m_sttTick->start(1000);

    /* Timeout: ferma la registrazione e avvia la trascrizione.
       Con sox VAD il processo termina da solo → onSttTimeout si limita a trasformare
       il WAV in trascrizione; con arecord aspetta il timeout completo. */
    QTimer::singleShot((recSecs + 1) * 1000, this, &AgentiPage::onSttTimeout);
}

/* ── slot privato: tick 1s del countdown ───────────────────────────────────── */
void AgentiPage::onSttTick()
{
    if (m_sttState != SttState::Recording) {
        m_sttTick->stop();
        m_sttTick->deleteLater();
        m_sttTick = nullptr;
        return;
    }
    int s = m_sttTick->property("secs").toInt() - 1;
    m_sttTick->setProperty("secs", s);
    if (s > 0) {
        const bool hasSox = !QStandardPaths::findExecutable("sox").isEmpty();
        m_btnVoice->setText(hasSox
            ? QString("\xf0\x9f\x94\xb4 Registrando... (VAD silenzio) (click per fermare)")
            : QString("\xf0\x9f\x94\xb4 Registrando... %1s (click per fermare)").arg(s));
    }
}

/* ── slot privato: timeout 6.5s — ferma registrazione e avvia trascrizione ─── */
void AgentiPage::onSttTimeout()
{
    /* Ferma il tick countdown se ancora attivo (es. utente non ha fermato prima) */
    if (m_sttTick) {
        m_sttTick->stop();
        m_sttTick->deleteLater();
        m_sttTick = nullptr;
    }

    if (m_sttState != SttState::Recording) return;  // utente ha già fermato

    if (m_recProc) {
        /* sox VAD può aver già terminato da solo — terminate() è no-op in quel caso */
        if (m_recProc->state() != QProcess::NotRunning)
            m_recProc->terminate();
        m_recProc->waitForFinished(1000);
        m_recProc->deleteLater();
        m_recProc = nullptr;
    }

    const QString wavPath = m_sttWavPath;

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
                    if (m_voiceLoopActive) {
                        m_input->setPlainText(text);   /* loop: sostituisce sempre */
                    } else {
                        const QString cur = m_input->toPlainText();
                        m_input->setPlainText(cur.isEmpty() ? text : cur + " " + text);
                    }
                    m_input->setFocus();
                    if (m_voiceLoopActive && !m_ai->busy())
                        QTimer::singleShot(150, this, &AgentiPage::onSttVoiceLoopAutoSend);
                } else {
                    m_log->append(
                        "\xe2\x9a\xa0  Trascrizione fallita o audio vuoto.<br>"
                        + QString(text).replace("\n","<br>"));
                    if (m_voiceLoopActive)
                        QTimer::singleShot(1500, this, &AgentiPage::onSttVoiceLoopRetry);
                }
            });
}

/* ══════════════════════════════════════════════════════════════
   downloadWhisperModel — scarica ggml-small.bin in ~/.prismalux/whisper/models/
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::downloadWhisperModel()
{
    m_whisperDlDestDir  = QDir::homePath() + "/.prismalux/whisper/models";
    m_whisperDlDestFile = m_whisperDlDestDir + "/ggml-small.bin";
    const QString url   =
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin";

    QDir().mkpath(m_whisperDlDestDir);

    m_sttState = SttState::Downloading;
    m_btnVoice->setEnabled(false);
    m_btnVoice->setText("\xf0\x9f\x93\xa5 Download modello...");

    m_log->append(
        "\xf0\x9f\x93\xa5  <b>Download modello whisper (ggml-small, ~141 MB)...</b><br>"
        "Destinazione: <code>" + m_whisperDlDestFile + "</code>");

    /* Usa wget se disponibile, altrimenti curl */
    const bool hasWget = !QStandardPaths::findExecutable("wget").isEmpty();
    m_whisperDlProc = new QProcess(this);
    m_whisperDlProc->setProcessChannelMode(QProcess::MergedChannels);

    if (hasWget) {
        m_whisperDlProc->start("wget", {
            "--progress=dot:mega",
            "-O", m_whisperDlDestFile,
            url
        });
    } else {
        m_whisperDlProc->start("curl", {
            "-L", "--progress-bar",
            "-o", m_whisperDlDestFile,
            url
        });
    }

    connect(m_whisperDlProc, &QProcess::readyReadStandardOutput,
            this, &AgentiPage::onWhisperDlProcReadyRead);
    connect(m_whisperDlProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AgentiPage::onWhisperDlProcFinished);
}

/* ── slot: progresso download whisper ──────────────────────────────────────── */
void AgentiPage::onWhisperDlProcReadyRead()
{
    if (!m_whisperDlProc) return;
    const QString chunk = QString::fromLocal8Bit(m_whisperDlProc->readAllStandardOutput());
    for (const auto& line : chunk.split('\n')) {
        const QString t = line.trimmed();
        if (t.contains('%') || t.contains("MB") || t.contains("KB"))
            m_log->append("  " + t.toHtmlEscaped());
    }
}

/* ── slot: fine download whisper ────────────────────────────────────────────── */
void AgentiPage::onWhisperDlProcFinished(int code, QProcess::ExitStatus)
{
    if (m_whisperDlProc) { m_whisperDlProc->deleteLater(); m_whisperDlProc = nullptr; }
    m_btnVoice->setEnabled(true);

    const QString destFile = m_whisperDlDestFile;
    const QString destDir  = m_whisperDlDestDir;

    if (code == 0 && QFileInfo::exists(destFile) &&
        QFileInfo(destFile).size() > 10'000'000LL) {
        m_log->append("\xe2\x9c\x85  <b>Modello scaricato.</b> Avvio registrazione...");
        m_sttState = SttState::Idle;
        _sttStartRecording();
    } else {
        m_sttState = SttState::Idle;
        m_btnVoice->setText("\xf0\x9f\x8e\xa4 Trascrivi voce");
        if (QFileInfo::exists(destFile) &&
            QFileInfo(destFile).size() < 10'000'000LL)
            QFile::remove(destFile);
        const QString destDirNative = QDir::toNativeSeparators(destDir);
#ifdef Q_OS_WIN
        m_log->append(
            "\xe2\x9d\x8c  Download fallito (controlla la connessione).<br>"
            "Puoi scaricarlo manualmente (PowerShell):<br>"
            "<code>New-Item -ItemType Directory -Force \""
            + destDirNative + "\"; "
            "Invoke-WebRequest -Uri "
            "'https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin' "
            "-OutFile '" + QDir::toNativeSeparators(destFile) + "'</code>");
#else
        Q_UNUSED(destDirNative)
        m_log->append(
            "\xe2\x9d\x8c  Download fallito (controlla la connessione).<br>"
            "Puoi scaricarlo manualmente:<br>"
            "<code>mkdir -p ~/.prismalux/whisper/models &amp;&amp; "
            "wget -O ~/.prismalux/whisper/models/ggml-small.bin "
            "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin</code>");
#endif
    }
}

/* ── slot: fine processo registrazione (sox VAD terminato prima del timeout) ── */
void AgentiPage::onRecProcFinished(int, QProcess::ExitStatus)
{
    if (m_sttState == SttState::Recording)
        onSttTimeout();
}

/* ── slot: riprova ascolto dopo STT fallito (voice loop) ───────────────────── */
void AgentiPage::onSttVoiceLoopRetry()
{
    _sttStartRecording();
}

/* ── slot: invio automatico dopo STT ok (voice loop) ───────────────────────── */
void AgentiPage::onSttVoiceLoopAutoSend()
{
    m_btnRun->click();
}

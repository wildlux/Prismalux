#include "main_ai.h"
#include "main_ai_p.h"
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
    m_btnVoice->setText(tr("\xf0\x9f\x94\xb4 Registrando... (click per fermare)"));
    m_btnVoice->setProperty("danger","true");
    P::repolish(m_btnVoice);

    /* In Conversa l'hub diventa uno Stop rosso: un clic (o Esc) ferma
       microfono e loop — prima non esisteva alcuno stop in questa modalità */
    if (m_modeBtn && m_modeBtn->currentMode() == TriModeButton::Conversa) {
        m_modeBtn->setActionText(tr("\xe2\x8f\xb9 Stop"));
        m_modeBtn->setActionDanger(true);
        m_btnRun->setText(tr("\xe2\x8f\xb9 Stop"));
    }

    m_recProc = new QProcess(this);

    /* Warm-up del demone STT: parte ORA così i 6-12s di registrazione
       coprono il caricamento del modello — a fine parlato la trascrizione
       costa solo ~2s (GPU) invece di ~11s. No-op se già attivo. */
    if (SttWhisper::isFastWhisperEnabled() && SttWhisper::isFastWhisperAvailable())
        SttDaemon::instance().ensureStarted(SttWhisper::fastWhisperModelName());

    /* recSecs dichiarato qui — usato dopo il #endif (righe countdown/timeout) */
    const int recSecs = m_voiceLoopActive ? 12 : 6;

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
        m_btnVoice->setText(tr("\xf0\x9f\x8e\xa4 Trascrivi parlato"));
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
    /* Se sox è disponibile, usa VAD silenzio-automatico (nessun countdown). */
    const bool hasSox  = !QStandardPaths::findExecutable("sox").isEmpty();

    if (hasSox) {
        /* sox VAD: registra fino a 2s di silenzio, max recSecs*2 secondi.
           Soglia 0.3% invece di 1%: funziona anche con microfoni poco sensibili.
           silence 1 0.1 0.3% = inizia quando il segnale supera lo 0.3% del picco;
                  1 2.0 0.3%  = ferma dopo 2s sotto quella soglia. */
        m_recProc->start("sox",
            {"-t", "alsa", "default",
             "-r", "16000", "-c", "1", "-b", "16",
             wavPath,
             "silence", "1", "0.1", "0.3%",
                        "1", "2.0", "0.3%",
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
    connect(m_recProc, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[stt_rec] processo non avviato:" << m_recProc->program();
    });

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
        /* Salva e azzera PRIMA di waitForFinished: se onRecProcFinished viene
           dispatchato durante l'event-loop interno di waitForFinished, il doppio
           deleteLater() causerebbe SEGV (crash confermato in test_fatti.txt). */
        QProcess* proc = m_recProc;
        m_recProc = nullptr;
        proc->disconnect();
        if (proc->state() != QProcess::NotRunning)
            proc->terminate();
        proc->waitForFinished(1000);
        proc->deleteLater();
    }

    const QString wavPath = m_sttWavPath;

    /* File WAV mancante o troppo piccolo (<2 KB = solo header, nessun audio) */
    const qint64 wavSize = QFileInfo(wavPath).size();
    if (!QFileInfo::exists(wavPath) || wavSize < 2000) {
        m_sttState = SttState::Idle;
        m_btnVoice->setText(tr("\xf0\x9f\x8e\xa4 Trascrivi parlato"));
        m_btnVoice->setProperty("danger","false");
        P::repolish(m_btnVoice);
        m_btnVoice->setEnabled(true);
        if (m_voiceLoopActive) {
            /* In loop voce: riprova silenziosamente senza mostrare errore */
            QTimer::singleShot(500, this, &AgentiPage::onSttVoiceLoopRetry);
        } else {
            m_log->append(
                "\xe2\x9a\xa0  Nessun audio registrato \xe2\x80\x94 "
                "verifica il microfono o parla pi\xc3\xb9 vicino.");
        }
        return;
    }

    /* ── VAD esterna: SOLO nel percorso senza demone (lì il silenzio
       costerebbe un caricamento modello da ~10s). Col demone attivo il
       vad_filter interno di faster-whisper gestisce il silenzio in ~1s.
       Asincrona: la versione precedente usava waitForFinished(3000) e
       congelava l'interfaccia fino a 3s dopo ogni registrazione. ── */
    if (!SttDaemon::instance().isUsable()) {
        const QString vadScript = P::root() + "/Tools/scripts/vad_filter.py";
        if (QFileInfo::exists(vadScript) && !P::findPython().isEmpty()) {
            m_vadProc = new QProcess(this);
            connect(m_vadProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &AgentiPage::onSttVadFinished);
            connect(m_vadProc, &QProcess::errorOccurred,
                    this, [this](QProcess::ProcessError err) {
                if (err == QProcess::FailedToStart) onSttVadTimeout();
            });
            m_vadProc->start(P::findPython(), {vadScript, wavPath});
            QTimer::singleShot(3500, this, &AgentiPage::onSttVadTimeout);
            return;
        }
    }

    _sttRunTranscription();
}

/* ── slot: vad_filter.py terminato — SILENCE ferma tutto, altrimenti trascrive ── */
void AgentiPage::onSttVadFinished()
{
    if (!m_vadProc) return;   /* già gestito dal watchdog */
    const QString out =
        QString::fromUtf8(m_vadProc->readAllStandardOutput()).trimmed();
    m_vadProc->deleteLater();
    m_vadProc = nullptr;
    if (out == "SILENCE") { _sttHandleSilence(); return; }
    _sttRunTranscription();
}

/* ── slot: watchdog VAD — processo bloccato/non partito → trascrivi comunque ── */
void AgentiPage::onSttVadTimeout()
{
    if (!m_vadProc) return;   /* già concluso regolarmente */
    m_vadProc->disconnect();
    m_vadProc->blockSignals(true);
    m_vadProc->kill();
    m_vadProc->deleteLater();
    m_vadProc = nullptr;
    _sttRunTranscription();
}

/* ── silenzio rilevato: reset UI + retry silenzioso se in loop voce ── */
void AgentiPage::_sttHandleSilence()
{
    m_sttState = SttState::Idle;
    m_btnVoice->setText(tr("\xf0\x9f\x8e\xa4 Trascrivi parlato"));
    m_btnVoice->setProperty("danger", "false");
    P::repolish(m_btnVoice);
    m_btnVoice->setEnabled(true);
    QFile::remove(m_sttWavPath);
    if (m_voiceLoopActive)
        QTimer::singleShot(500, this, &AgentiPage::onSttVoiceLoopRetry);
    else
        m_log->append(
            "\xf0\x9f\x94\x87 Silenzio rilevato &mdash; "
            "nessun parlato nell'audio.");
}

/* ── avvia la trascrizione vera e propria su m_sttWavPath ── */
void AgentiPage::_sttRunTranscription()
{
    const QString wavPath = m_sttWavPath;
    m_sttState = SttState::Transcribing;
    m_btnVoice->setText(tr("\xe2\x8c\x9b Trascrivendo..."));
    m_btnVoice->setProperty("danger","false");
    P::repolish(m_btnVoice);
    m_btnVoice->setEnabled(false);

    m_sttProc = SttWhisper::transcribe(wavPath, "it", this,
            [this](const QString& text, bool ok) {
                m_sttState = SttState::Idle;
                m_sttProc  = nullptr;
                m_btnVoice->setText(tr("\xf0\x9f\x8e\xa4 Trascrivi parlato"));
                m_btnVoice->setEnabled(true);

                if (ok && text.isEmpty()) {
                    /* Demone: vad_filter interno non ha trovato parlato */
                    m_log->append(
                        "\xf0\x9f\x94\x87 Silenzio rilevato &mdash; "
                        "nessun parlato nell'audio.");
                    if (m_voiceLoopActive)
                        QTimer::singleShot(500, this, &AgentiPage::onSttVoiceLoopRetry);
                    return;
                }
                if (!ok) {
                    m_log->append(
                        "\xe2\x9a\xa0  Trascrizione fallita.<br>"
                        + QString(text).replace("\n","<br>"));
                    if (m_voiceLoopActive)
                        QTimer::singleShot(1500, this, &AgentiPage::onSttVoiceLoopRetry);
                    return;
                }

                /* Diarizzazione speaker — solo in modalità manuale (non in loop/Conversa
                   dove la latenza aggiuntiva impedirebbe la fluidità del dialogo) */
                const bool inConversa = m_modeBtn
                    && m_modeBtn->currentMode() == TriModeButton::Conversa;
                const bool skipDiarize = m_voiceLoopActive || inConversa;

                if (!skipDiarize && SttWhisper::isDiarizeEnabled()) {
                    m_btnVoice->setText(tr("\xf0\x9f\x91\xa5 Identifico speaker..."));
                    m_btnVoice->setEnabled(false);
                    m_input->setPlainText(text);   /* testo grezzo subito visibile */

                    /* Salva transcript in file temp per allineamento */
                    const QString tmpTxt = P::safeTempPath() + "/prisma_stt_transcript.txt";
                    { QFile f(tmpTxt); if (f.open(QIODevice::WriteOnly)) f.write(text.toUtf8()); }

                    if (m_diarizeProc) { m_diarizeProc->kill(); m_diarizeProc = nullptr; }
                    m_diarizeProc = SttWhisper::diarize(
                        m_sttWavPath, tmpTxt,
                        SttWhisper::diarizeNSpeakers(), {},
                        this,
                        [this, tmpTxt](const QString& json, bool dok) {
                            m_diarizeProc = nullptr;
                            m_btnVoice->setText(tr("\xf0\x9f\x8e\xa4 Trascrivi parlato"));
                            m_btnVoice->setEnabled(true);
                            QFile::remove(tmpTxt);
                            if (dok) {
                                const QString diarText = SttWhisper::formatDiarization(json);
                                if (!diarText.isEmpty() && !diarText.startsWith("{\"error"))
                                    m_input->setPlainText(diarText);
                            }
                            m_input->setFocus();
                        });
                    return;
                }

                m_input->setPlainText(text);
                m_input->setFocus();
                /* Auto-invio SOLO a loop attivo: in Conversa il loop è
                   sempre attivo mentre gira; se l'utente ha premuto
                   Stop/Esc durante la trascrizione, il testo resta
                   nell'input senza partire da solo. */
                if (m_voiceLoopActive && !m_ai->busy())
                    QTimer::singleShot(150, this, &AgentiPage::onSttVoiceLoopAutoSend);
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
    m_btnVoice->setText(tr("\xf0\x9f\x93\xa5 Download modello..."));

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
    connect(m_whisperDlProc, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[whisper_dl] processo non avviato:" << m_whisperDlProc->program();
    });
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
        m_btnVoice->setText(tr("\xf0\x9f\x8e\xa4 Trascrivi parlato"));
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
    /* Il retry è schedulato con singleShot: se nel frattempo l'utente ha
       fermato il loop (Stop/Esc), NON riaprire il microfono. */
    if (!m_voiceLoopActive) return;
    _sttStartRecording();
}

/* ── slot: invio automatico dopo STT ok (voice loop) ───────────────────────── */
void AgentiPage::onSttVoiceLoopAutoSend()
{
    if (!m_voiceLoopActive) return;   /* loop fermato mentre trascriveva */
    /* Flag: distingue questo click programmatico dallo Stop dell'utente
       (in Conversa il click sull'hub a loop attivo ferma tutto). */
    m_sttAutoSending = true;
    m_btnRun->click();                /* connessione diretta: sincrono */
    m_sttAutoSending = false;
}

/* ── STOP totale del flusso voce: registrazione, trascrizione, AI, loop ────── */
void AgentiPage::_voiceConversaStop()
{
    if (m_ai->busy()) m_ai->abort();
    /* onVoiceLoopToggled(false) uccide registrazione + TTS, ferma il tick
       e azzera m_voiceLoopActive: da qui in poi retry/auto-send schedulati
       non ripartono (guardie sui rispettivi slot). */
    onVoiceLoopToggled(false);
    if (m_sttState == SttState::Transcribing) {
        /* la risposta del demone in arrivo verrà mostrata nell'input ma
           senza auto-invio (m_voiceLoopActive ormai false) */
        m_sttState = SttState::Idle;
        m_btnVoice->setText(tr("\xf0\x9f\x8e\xa4 Trascrivi parlato"));
        m_btnVoice->setEnabled(true);
    }
    /* Ripristina l'hub della modalità Conversa */
    if (m_modeBtn && m_modeBtn->currentMode() == TriModeButton::Conversa) {
        m_modeBtn->setActionText(tr("\xf0\x9f\x8e\x99  Dialoga"));
        m_modeBtn->setActionDanger(false);
        m_btnRun->setText(tr("\xf0\x9f\x8e\x99  Dialoga"));
    }
    m_log->append("\xe2\x9c\x8b  Conversazione fermata.");
}

/* ── slot: Esc — ferma il flusso voce se attivo, altrimenti non fa nulla ───── */
void AgentiPage::onEscShortcut()
{
    if (m_voiceLoopActive || m_sttState != SttState::Idle)
        _voiceConversaStop();
}

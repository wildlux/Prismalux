#include "audio_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QApplication>
#include <QClipboard>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>
#include <QTextCursor>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <cmath>

#ifdef HAVE_MULTIMEDIA
#include <QMediaFormat>
#endif

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
AudioPage::AudioPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    setObjectName("AudioPage");

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);
    scroll->setWidget(inner);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    /* ── Titolo ── */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x8e\x99\xef\xb8\x8f  Trascrizione Audio"), inner);  /* 🎙️ */
    QFont tf = title->font();
    tf.setPointSize(14); tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    /* ── Sezione registrazione ── */
    auto* recGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x94\xb4  Registrazione"), inner);  /* 🔴 */
    recGroup->setObjectName("SettingsGroup");
    auto* recVbox = new QVBoxLayout(recGroup);
    recVbox->setSpacing(8);

    m_recBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x8e\x99\xef\xb8\x8f  Avvia Registrazione"), inner);  /* 🎙️ */
    m_recBtn->setObjectName("PrimaryBtn");
    m_recBtn->setMinimumHeight(56);
    QFont rbf = m_recBtn->font();
    rbf.setPointSize(14); rbf.setBold(true);
    m_recBtn->setFont(rbf);
    recVbox->addWidget(m_recBtn);

    /* Timer e livello */
    auto* timerRow = new QHBoxLayout;
    m_recTimeLbl = new QLabel("00:00", inner);
    m_recTimeLbl->setObjectName("RecTimeLbl");
    QFont tmf = m_recTimeLbl->font();
    tmf.setPointSize(18); tmf.setBold(true);
    m_recTimeLbl->setFont(tmf);
    m_recTimeLbl->setAlignment(Qt::AlignCenter);

    m_recStatus = new QLabel(
        QString::fromUtf8("\xe2\x97\x8f  In attesa..."), inner);  /* ● */
    m_recStatus->setAlignment(Qt::AlignCenter);

    timerRow->addWidget(m_recStatus, 1);
    timerRow->addWidget(m_recTimeLbl, 1);
    recVbox->addLayout(timerRow);

    m_levelBar = new QProgressBar(inner);
    m_levelBar->setRange(0, 100);
    m_levelBar->setValue(0);
    m_levelBar->setTextVisible(false);
    m_levelBar->setFixedHeight(8);
    m_levelBar->setObjectName("AudioLevelBar");
    recVbox->addWidget(m_levelBar);

    vbox->addWidget(recGroup);

    /* ── Sezione trascrizione ── */
    auto* transGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\xa4\x96  Trascrizione AI"), inner);  /* 🤖 */
    transGroup->setObjectName("SettingsGroup");
    auto* transVbox = new QVBoxLayout(transGroup);
    transVbox->setSpacing(8);

    m_modeCombo = new QComboBox(inner);
    m_modeCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x94\x8a  Trascrivi con Whisper (server)"),   /* 🔊 */
        "whisper");
    m_modeCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x93\x9d  Analizza descrizione testuale"),    /* 📝 */
        "text");
    m_modeCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x8c\x90  Riassumi e struttura il testo"),    /* 🌐 */
        "summarize");
    transVbox->addWidget(m_modeCombo);

    auto* infoLbl = new QLabel(
        QString::fromUtf8("\xe2\x84\xb9\xef\xb8\x8f")  /* ℹ️ */
        + "  La modalità Whisper richiede un server compatibile OpenAI "
          "(/v1/audio/transcriptions) configurato nelle Impostazioni.",
        inner);
    infoLbl->setWordWrap(true);
    infoLbl->setStyleSheet("color:#8890a8; font-size:12px;");
    transVbox->addWidget(infoLbl);

    m_transcribeBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9c\xa8  Trascrivi / Analizza"), inner);  /* ✨ */
    m_transcribeBtn->setObjectName("SecondaryBtn");
    m_transcribeBtn->setMinimumHeight(48);
    transVbox->addWidget(m_transcribeBtn);

    /* Stato AI */
    m_aiStatus = new QLabel("", inner);
    m_aiStatus->setVisible(false);
    transVbox->addWidget(m_aiStatus);

    m_aiProgress = new QProgressBar(inner);
    m_aiProgress->setRange(0, 0);
    m_aiProgress->setVisible(false);
    m_aiProgress->setFixedHeight(4);
    transVbox->addWidget(m_aiProgress);

    vbox->addWidget(transGroup);

    /* ── Risultato ── */
    auto* resultGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x84  Testo Trascritto"), inner);  /* 📄 */
    resultGroup->setObjectName("SettingsGroup");
    auto* resultVbox = new QVBoxLayout(resultGroup);

    m_resultEdit = new QTextEdit(inner);
    m_resultEdit->setPlaceholderText(
        "Il testo trascritto apparirà qui.\n\n"
        "Puoi modificarlo prima di usarlo.\n\n"
        "Consiglio: registra e poi premi «Trascrivi / Analizza».");
    m_resultEdit->setMinimumHeight(160);
    resultVbox->addWidget(m_resultEdit);

    auto* btnRow = new QHBoxLayout;
    m_copyBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x8b  Copia"), inner);  /* 📋 */
    m_copyBtn->setObjectName("SecondaryBtn");
    m_copyBtn->setMinimumHeight(44);
    m_chatBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x92\xac  Invia in Chat"), inner);  /* 💬 */
    m_chatBtn->setObjectName("SecondaryBtn");
    m_chatBtn->setMinimumHeight(44);
    btnRow->addWidget(m_copyBtn, 1);
    btnRow->addWidget(m_chatBtn, 1);
    resultVbox->addLayout(btnRow);

    vbox->addWidget(resultGroup);

    /* ── Guida rapida ── */
    auto* guideGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x96  Come usare"), inner);  /* 📖 */
    guideGroup->setObjectName("SettingsGroup");
    auto* guideLbl = new QLabel(guideGroup);
    guideLbl->setTextFormat(Qt::RichText);
    guideLbl->setWordWrap(true);
    guideLbl->setText(
        "<b>1.</b> Premi "
        + QString::fromUtf8("\xf0\x9f\x8e\x99\xef\xb8\x8f")  /* 🎙️ */
        + " <b>Avvia Registrazione</b> e parla chiaramente.<br>"
        "<b>2.</b> Premi di nuovo per fermare.<br>"
        "<b>3.</b> Seleziona la modalità e premi <b>Trascrivi / Analizza</b>.<br>"
        "<b>4.</b> Modifica il testo e usalo con <b>Invia in Chat</b>.<br><br>"
        "<b>Whisper server:</b> imposta l'URL del tuo server Whisper nelle Impostazioni "
        "(campo «Host server»). Il file audio viene inviato a <code>/v1/audio/transcriptions</code>.");
    auto* guideVbox = new QVBoxLayout(guideGroup);
    guideVbox->addWidget(guideLbl);
    vbox->addWidget(guideGroup);

    /* ── Sezione TTS — Sintesi Vocale ── */
    auto* ttsGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x94\x8a  Sintesi Vocale (TTS)"), inner);  /* 🔊 */
    ttsGroup->setObjectName("SettingsGroup");
    auto* ttsLay = new QVBoxLayout(ttsGroup);
    ttsLay->setSpacing(8);

    auto* ttsLbl = new QLabel(
        "Digita il testo e premi <b>Parla</b> per ascoltarlo in italiano.", ttsGroup);
    ttsLbl->setTextFormat(Qt::RichText);
    ttsLbl->setWordWrap(true);
    ttsLay->addWidget(ttsLbl);

    m_ttsInput = new QTextEdit(ttsGroup);
    m_ttsInput->setPlaceholderText("Scrivi qui il testo da leggere ad alta voce...");
    m_ttsInput->setFixedHeight(90);
    m_ttsInput->setObjectName("SettingsGroup");
    ttsLay->addWidget(m_ttsInput);

    m_speakBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8a  Parla"), ttsGroup);  /* 🔊 */
    m_speakBtn->setObjectName("ChatSendBtn");
    m_speakBtn->setMinimumHeight(44);

#ifdef HAVE_TTS
    m_tts = new QTextToSpeech(this);
    m_tts->setLocale(QLocale(QLocale::Italian, QLocale::Italy));

    connect(m_speakBtn, &QPushButton::clicked, this, &AudioPage::onSpeakToggle);
    connect(m_tts, &QTextToSpeech::stateChanged, ttsGroup,
            [this](QTextToSpeech::State state) {
                if (state == QTextToSpeech::Ready || state == QTextToSpeech::Error) {
                    if (m_speakBtn)
                        m_speakBtn->setText(QString::fromUtf8(
                            "\xf0\x9f\x94\x8a  Parla"));  /* 🔊 */
                }
            });
#else
    m_speakBtn->setEnabled(false);
    m_speakBtn->setToolTip("QTextToSpeech non disponibile in questa build");
#endif
    ttsLay->addWidget(m_speakBtn);
    vbox->addWidget(ttsGroup);

    vbox->addStretch();

    /* ── Timer per il cronometro di registrazione ── */
    m_recTimer = new QTimer(this);
    m_recTimer->setInterval(1000);

    /* ── Qt Multimedia (se disponibile) ── */
#ifdef HAVE_MULTIMEDIA
    m_captureSession = new QMediaCaptureSession(this);
    m_audioInput     = new QAudioInput(this);
    m_recorder       = new QMediaRecorder(this);

    m_captureSession->setAudioInput(m_audioInput);
    m_captureSession->setRecorder(m_recorder);

    /* Formato WAV per compatibilità massima con Whisper */
    QMediaFormat fmt;
    fmt.setFileFormat(QMediaFormat::Wave);
    fmt.setAudioCodec(QMediaFormat::AudioCodec::Wave);
    m_recorder->setMediaFormat(fmt);
    m_recorder->setAudioSampleRate(16000);   /* Whisper preferisce 16kHz */
    m_recorder->setAudioChannelCount(1);
    m_recorder->setQuality(QMediaRecorder::HighQuality);

    const QString audioDir =
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
        + "/Prismalux";
    QDir().mkpath(audioDir);
    m_recorder->setOutputLocation(
        QUrl::fromLocalFile(audioDir + "/registrazione.wav"));
#endif

    /* ── Connessioni ── */
    connect(m_recBtn,       &QPushButton::clicked, this, &AudioPage::onRecordToggle);
    connect(m_transcribeBtn,&QPushButton::clicked, this, &AudioPage::onTranscribeClicked);
    connect(m_copyBtn,      &QPushButton::clicked, this, &AudioPage::onCopyResult);
    connect(m_chatBtn,      &QPushButton::clicked, this, &AudioPage::onSendToChat);
    connect(m_recTimer,     &QTimer::timeout,      this, &AudioPage::onRecordTick);

    connect(m_ai, &AiClient::token,    this, &AudioPage::onToken);
    connect(m_ai, &AiClient::finished, this, &AudioPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &AudioPage::onError);
    connect(m_ai, &AiClient::aborted,  this, &AudioPage::onAborted);
}

/* ══════════════════════════════════════════════════════════════
   Registrazione
   ══════════════════════════════════════════════════════════════ */
void AudioPage::onRecordToggle()
{
    if (m_recording) {
        setRecordingState(false);
#ifdef HAVE_MULTIMEDIA
        m_recorder->stop();

        if (m_audioLevelSource) {
            m_audioLevelSource->stop();
            m_audioLevelSource->deleteLater();
            m_audioLevelSource = nullptr;
            m_audioLevelIO = nullptr;
        }
        m_levelBar->setValue(0);
#endif
        m_recStatus->setText(
            QString::fromUtf8("\xe2\x9c\x85  Registrazione completata — premi Trascrivi"));  /* ✅ */
    } else {
        /* Richiedi permesso microfono a runtime prima di avviare */
        QMicrophonePermission micPerm;
        qApp->requestPermission(micPerm, this, &AudioPage::onMicPermissionResult);
    }
}

void AudioPage::onMicPermissionResult(const QPermission& permission)
{
    if (permission.status() != Qt::PermissionStatus::Granted) {
        m_recStatus->setText(
            QString::fromUtf8("\xe2\x9d\x8c  Permesso microfono negato. "
                              "Abilitalo in Impostazioni \xe2\x86\x92 App \xe2\x86\x92 Permessi."));
        return;
    }
#ifdef HAVE_MULTIMEDIA
    m_recorder->record();

    /* Avvia un secondo QAudioSource leggero solo per il level meter */
    QAudioFormat levelFmt;
    levelFmt.setSampleRate(16000);
    levelFmt.setChannelCount(1);
    levelFmt.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (!inputDevice.isNull()) {
        m_audioLevelSource = new QAudioSource(inputDevice, levelFmt, this);
        m_audioLevelSource->setBufferSize(512);
        m_audioLevelIO = m_audioLevelSource->start();
        if (m_audioLevelIO) {
            connect(m_audioLevelIO, &QIODevice::readyRead,
                    this, &AudioPage::onAudioLevelData);
        }
    }
#endif
    setRecordingState(true);
    m_recStatus->setText(
        QString::fromUtf8("\xf0\x9f\x94\xb4  Registrazione in corso..."));  /* 🔴 */
}

void AudioPage::setRecordingState(bool recording)
{
    m_recording = recording;
    m_recSecs   = recording ? 0 : m_recSecs;

    if (recording) {
        m_recBtn->setText(
            QString::fromUtf8("\xe2\x8f\xb9  Ferma Registrazione"));  /* ⏹ */
        m_recBtn->setObjectName("StopBtn");
        m_recTimer->start();
    } else {
        m_recBtn->setText(
            QString::fromUtf8("\xf0\x9f\x8e\x99\xef\xb8\x8f  Nuova Registrazione"));  /* 🎙️ */
        m_recBtn->setObjectName("PrimaryBtn");
        m_recTimer->stop();
    }
    /* Forza aggiornamento stile */
    m_recBtn->style()->unpolish(m_recBtn);
    m_recBtn->style()->polish(m_recBtn);
}

void AudioPage::onRecordTick()
{
    ++m_recSecs;
    const int mins = m_recSecs / 60;
    const int secs = m_recSecs % 60;
    m_recTimeLbl->setText(
        QString("%1:%2")
        .arg(mins, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0')));
    /* Il livello audio è aggiornato in real-time da onAudioLevelData() */
}

void AudioPage::onAudioLevelData()
{
#ifdef HAVE_MULTIMEDIA
    if (!m_audioLevelIO) return;
    const QByteArray data = m_audioLevelIO->readAll();
    if (data.isEmpty()) return;

    /* Calcolo RMS su campioni Int16 */
    const qint16* samples = reinterpret_cast<const qint16*>(data.constData());
    const int count = data.size() / static_cast<int>(sizeof(qint16));
    if (count == 0) return;

    double sum = 0.0;
    for (int i = 0; i < count; ++i)
        sum += static_cast<double>(samples[i]) * static_cast<double>(samples[i]);
    const double rms = std::sqrt(sum / count);

    /* Normalizza: Int16 max = 32767, mappiamo su 0-100 con un po' di gain */
    const int level = qBound(0, static_cast<int>(rms / 327.67 * 1.5), 100);
    m_levelBar->setValue(level);
#endif
}

QString AudioPage::savedAudioPath() const
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
        + "/Prismalux";
    return dir + "/registrazione.wav";
}

/* ══════════════════════════════════════════════════════════════
   Trascrizione
   ══════════════════════════════════════════════════════════════ */
void AudioPage::onTranscribeClicked()
{
    const QString mode = m_modeCombo->currentData().toString();

    if (mode == "whisper") {
        const QString path = savedAudioPath();
        QFile f(path);
        if (!f.exists()) {
            m_resultEdit->setPlainText(
                QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")  /* ⚠️ */
                + "  Nessuna registrazione trovata. Registra prima un audio.");
            return;
        }

        if (m_busy) {
            m_resultEdit->setPlainText(
                QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
                + "  Elaborazione in corso. Attendi.");
            return;
        }

        /* ── Invia l'audio al server Whisper via HTTP multipart ── */
        m_busy = true;
        m_resultEdit->clear();
        m_aiStatus->setText(
            QString::fromUtf8("\xf0\x9f\x94\x84")  /* 🔄 */
            + "  Invio al server Whisper...");
        m_aiStatus->setVisible(true);
        m_aiProgress->setVisible(true);
        m_transcribeBtn->setEnabled(false);

        uploadWhisper(path);
        return;
    }
    onAnalyzeText();
}

void AudioPage::onAnalyzeText()
{
    if (m_busy || m_ai->busy()) {
        m_resultEdit->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + "  L'AI sta elaborando. Attendi oppure interrompi dalla Chat.");
        return;
    }

    const QString mode    = m_modeCombo->currentData().toString();
    const QString current = m_resultEdit->toPlainText().trimmed();

    QString sys, userMsg;

    if (mode == "whisper") {
        sys = "Sei un trascrittore audio professionale. Il file audio indicato è stato "
              "registrato in italiano. Trascrivi il contenuto in modo fedele, correggendo "
              "solo le evidenti sviste fonologiche. Restituisci solo il testo trascritto.";
        userMsg = "Trascrivi questo file audio registrato dalla app Prismalux Mobile: "
                  + savedAudioPath()
                  + "\n\n(Nota: se il server non supporta trascrizione diretta, "
                    "mostra le istruzioni per configurare Whisper localmente.)";
    } else if (mode == "summarize") {
        if (current.isEmpty()) {
            m_resultEdit->setPlainText(
                QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
                + "  Incolla prima il testo da riassumere.");
            return;
        }
        sys = "Sei un assistente di scrittura italiano. Riorganizza e struttura il testo "
              "fornito in modo chiaro e professionale, dividendolo in paragrafi logici "
              "con titoletti se necessario. Correggi errori grammaticali evidenti.";
        userMsg = "Struttura e migliora questo testo:\n\n" + current;
    } else {
        if (current.isEmpty()) {
            m_resultEdit->setPlainText(
                QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
                + "  Scrivi una descrizione dell'audio nel campo testo, poi premi Analizza.");
            return;
        }
        sys = "Sei un assistente AI. Analizza il testo fornito e rispondi con precisione "
              "in italiano. Se il testo sembra essere una trascrizione parlata approssimativa, "
              "interpretalo correttamente.";
        userMsg = current;
    }

    m_busy = true;
    m_resultEdit->clear();
    m_aiStatus->setText(
        QString::fromUtf8("\xe2\x8f\xb3")  /* ⏳ */
        + "  Elaborazione in corso...");
    m_aiStatus->setVisible(true);
    m_aiProgress->setVisible(true);
    m_transcribeBtn->setEnabled(false);

    m_ai->chat(sys, userMsg);
}

/* ══════════════════════════════════════════════════════════════
   Slot AI
   ══════════════════════════════════════════════════════════════ */
void AudioPage::onToken(const QString& t)
{
    if (!m_busy) return;
    m_resultEdit->moveCursor(QTextCursor::End);
    m_resultEdit->insertPlainText(t);
}

void AudioPage::onFinished(const QString&)
{
    if (!m_busy) return;
    m_busy = false;
    m_aiStatus->setVisible(false);
    m_aiProgress->setVisible(false);
    m_transcribeBtn->setEnabled(true);
}

void AudioPage::onError(const QString& e)
{
    if (!m_busy) return;
    m_busy = false;
    m_aiStatus->setVisible(false);
    m_aiProgress->setVisible(false);
    m_transcribeBtn->setEnabled(true);
    m_resultEdit->setPlainText(
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + e);
}

void AudioPage::onAborted()
{
    onFinished("");
}

/* ══════════════════════════════════════════════════════════════
   Azioni risultato
   ══════════════════════════════════════════════════════════════ */
void AudioPage::onCopyResult()
{
    const QString txt = m_resultEdit->toPlainText().trimmed();
    if (txt.isEmpty()) return;
    QApplication::clipboard()->setText(txt);
    m_copyBtn->setText(
        QString::fromUtf8("\xe2\x9c\x85  Copiato!"));  /* ✅ */
    QTimer::singleShot(2000, this, &AudioPage::onCopyBtnRestore);
}

void AudioPage::onCopyBtnRestore()
{
    if (m_copyBtn)
        m_copyBtn->setText(QString::fromUtf8("\xf0\x9f\x93\x8b  Copia"));
}

void AudioPage::onSendToChat()
{
    const QString txt = m_resultEdit->toPlainText().trimmed();
    if (txt.isEmpty()) return;
    emit transcriptionReady(txt);
    m_chatBtn->setText(
        QString::fromUtf8("\xe2\x9c\x85  Inviato!"));  /* ✅ */
    QTimer::singleShot(2000, this, &AudioPage::onChatBtnRestore);
}

void AudioPage::onChatBtnRestore()
{
    if (m_chatBtn)
        m_chatBtn->setText(QString::fromUtf8("\xf0\x9f\x92\xac  Invia in Chat"));
}

/* ══════════════════════════════════════════════════════════════
   Slot AiClient::transcriptionReady / transcriptionError
   Chiamati da AiClient::onTranscriptionFinished() quando
   onTranscribeClicked() usa m_ai->transcribeAudio().
   ══════════════════════════════════════════════════════════════ */
/* onAiTranscriptionReady / onAiTranscriptionError: legacy, non più usati.
   La trascrizione ora avviene via uploadWhisper() / onWhisperReply(). */
void AudioPage::onAiTranscriptionReady(const QString& text)
{
    m_busy = false;
    m_aiProgress->setVisible(false);
    m_transcribeBtn->setEnabled(true);
    if (!text.isEmpty()) {
        m_aiStatus->setText(
            QString::fromUtf8("\xe2\x9c\x85  Trascrizione completata."));
        m_resultEdit->setPlainText(text);
    }
}

void AudioPage::onAiTranscriptionError(const QString& msg)
{
    m_busy = false;
    m_aiProgress->setVisible(false);
    m_transcribeBtn->setEnabled(true);
    m_aiStatus->setText(
        QString::fromUtf8("\xe2\x9d\x8c  Errore: ") + msg.left(120));
}

/* ══════════════════════════════════════════════════════════════
   onSpeakToggle — avvia o ferma la sintesi vocale TTS
   ══════════════════════════════════════════════════════════════ */
void AudioPage::onSpeakToggle()
{
#ifdef HAVE_TTS
    if (!m_tts) return;

    if (m_tts->state() == QTextToSpeech::Speaking) {
        m_tts->stop();
        if (m_speakBtn)
            m_speakBtn->setText(QString::fromUtf8("\xf0\x9f\x94\x8a  Parla"));
        return;
    }

    const QString txt = m_ttsInput ? m_ttsInput->toPlainText().trimmed() : QString();
    if (txt.isEmpty()) {
        if (m_ttsInput) m_ttsInput->setPlaceholderText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f  Scrivi prima il testo..."));
        return;
    }

    m_tts->say(txt);
    if (m_speakBtn)
        m_speakBtn->setText(QString::fromUtf8("\xe2\x8f\xb9  Stop"));  /* ⏹ */
#endif
}

/* ══════════════════════════════════════════════════════════════
   Whisper upload — POST multipart/form-data a /api/whisper
   (path LAN legacy: tieni per retrocompatibilità)
   ══════════════════════════════════════════════════════════════ */
void AudioPage::uploadWhisper(const QString& filePath)
{
    if (m_whisperReply) {
        /* upload già in corso */
        m_resultEdit->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + "  Upload già in corso. Attendi.");
        return;
    }

    QFile* file = new QFile(filePath, this);
    if (!file->open(QIODevice::ReadOnly)) {
        m_resultEdit->setPlainText(
            QString::fromUtf8("\xe2\x9d\x8c")
            + "  Impossibile aprire il file audio: " + filePath);
        file->deleteLater();
        return;
    }

    /* Leggi URL e token dalle impostazioni */
    QSettings s("Prismalux", "Mobile");
    const QString host  = s.value("server/host", "192.168.1.165").toString();
    const int     port  = s.value("server/port",  11500).toInt();
    const QString token = s.value("server/token", "").toString();
    const QString url   = QString("http://%1:%2/api/whisper").arg(host).arg(port);

    /* Prepara multipart */
    auto* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType, this);

    QHttpPart audioPart;
    audioPart.setHeader(QNetworkRequest::ContentTypeHeader,
                        QVariant("audio/wav"));
    audioPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant("form-data; name=\"audio\"; filename=\"registrazione.wav\""));
    audioPart.setBodyDevice(file);
    file->setParent(multiPart);   /* multiPart si occupa della delete */
    multiPart->append(audioPart);

    QNetworkRequest req{QUrl{url}};
    if (!token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());

    if (!m_whisperNam)
        m_whisperNam = new QNetworkAccessManager(this);

    m_whisperReply = m_whisperNam->post(req, multiPart);
    multiPart->setParent(m_whisperReply);   /* reply si occupa della delete */

    connect(m_whisperReply, &QNetworkReply::finished,
            this, &AudioPage::onWhisperReply);

    /* Aggiorna UI */
    m_resultEdit->clear();
    m_aiStatus->setText(
        QString::fromUtf8("\xf0\x9f\x94\x84")  /* 🔄 */
        + "  Invio audio al server Whisper...");
    m_aiStatus->setVisible(true);
    m_aiProgress->setVisible(true);
    m_transcribeBtn->setEnabled(false);
}

void AudioPage::onWhisperReply()
{
    m_aiProgress->setVisible(false);
    m_transcribeBtn->setEnabled(true);

    if (!m_whisperReply) return;

    if (m_whisperReply->error() != QNetworkReply::NoError) {
        const QString errMsg = m_whisperReply->errorString();
        m_aiStatus->setText(
            QString::fromUtf8("\xe2\x9d\x8c")  /* ❌ */
            + "  Errore server: " + errMsg.left(120));
        m_resultEdit->setPlainText(
            QString::fromUtf8("\xe2\x9d\x8c  Whisper non raggiungibile.\n\n")
            + "Verifica:\n"
            "1. L'IP e la porta nelle Impostazioni (usa 11500 per LAN Prismalux)\n"
            "2. Il server Prismalux Desktop sia avviato\n"
            "3. Il token sia corretto");
    } else {
        const QByteArray body = m_whisperReply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        const QString text = doc.object().value("text").toString().trimmed();

        if (text.isEmpty()) {
            m_aiStatus->setText(
                QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")  /* ⚠️ */
                + "  Risposta Whisper vuota o formato non riconosciuto.");
            m_resultEdit->setPlainText(
                QString::fromUtf8("Risposta grezza del server:\n") + body.left(400));
        } else {
            m_aiStatus->setText(
                QString::fromUtf8("\xe2\x9c\x85")  /* ✅ */
                + "  Trascrizione completata.");
            m_resultEdit->setPlainText(text);
        }
    }

    m_whisperReply->deleteLater();
    m_whisperReply = nullptr;
}

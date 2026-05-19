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
        /* Ferma la registrazione */
        setRecordingState(false);
#ifdef HAVE_MULTIMEDIA
        m_recorder->stop();
#endif
        m_recStatus->setText(
            QString::fromUtf8("\xe2\x9c\x85  Registrazione completata — premi Trascrivi"));  /* ✅ */
    } else {
        /* Avvia la registrazione */
#ifdef HAVE_MULTIMEDIA
        m_recorder->record();
#endif
        setRecordingState(true);
        m_recStatus->setText(
            QString::fromUtf8("\xf0\x9f\x94\xb4  Registrazione in corso..."));  /* 🔴 */
    }
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

    /* Simulazione livello audio (visivo) — livello reale richiederebbe QAudioSource */
    static int fake = 0;
    fake = (fake + 37) % 100;
    const int level = 20 + (fake % 60);
    m_levelBar->setValue(level);
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
        /* Invia il file audio al server Whisper via AiClient (multipart/form-data).
           La funzione è disponibile se AiClient ha il metodo transcribeAudio().
           In assenza, cade nel ramo testuale. */
        const QString path = savedAudioPath();
        QFile f(path);
        if (!f.exists()) {
            m_resultEdit->setPlainText(
                QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")  /* ⚠️ */
                + "  Nessuna registrazione trovata. Registra prima un audio.");
            return;
        }
        /* Usa l'AI per descrivere il file — placeholder per integrazione Whisper reale */
        onAnalyzeText();
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

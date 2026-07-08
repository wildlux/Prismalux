#include "main_multimedia.h"
#include "widget_stable_diffusion.h"
#include "widget_vision3d.h"
#include "../prismalux_paths.h"
#include "../dpi_utils.h"
#include "../widgets/stt_whisper.h"
#include "../widgets/widget_dep_check.h"
#include "../widgets/ffmpeg_utils.h"
#include "../widgets/opencv_utils.h"
#include "../widgets/world_map_widget.h"
#include "../log_bus.h"
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTextEdit>
#include <QGroupBox>
#include <QSplitter>
#include <QScrollArea>
#include <QFrame>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QTimer>
#include <QPixmap>
#include <QRegularExpression>
#include <QTextCursor>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QListWidget>
#include <QCheckBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

namespace P = PrismaluxPaths;

MultimediaPage::MultimediaPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildAudioTab(),           "\xf0\x9f\x8e\xb5  Audio AI");         /* 🎵 */
    tabs->addTab(buildSDTab(),             "\xf0\x9f\x8e\xa8  Genera Immagini");  /* 🎨 */
    tabs->addTab(buildGraphvizTab(),       "\xf0\x9f\x95\xb8  Mappe concettuali"); /* 🕸 */
    tabs->addTab(buildOsmMapTab(),         "\xf0\x9f\x97\xba  Mappa OSM");         /* 🗺 */
    tabs->addTab(buildSintetizzatoreTab(), "\xf0\x9f\x94\x8a  Sintetizzatore");    /* 🔊 */
    tabs->addTab(buildVoiceClonerTab(),   "\xf0\x9f\x8e\xa4  Clona Voce");       /* 🎤 */
    tabs->addTab(buildOcrTab(),            "\xf0\x9f\x94\x8d  OCR webcam");     /* 🔍 */
    tabs->addTab(buildVideoCaptionTab(), "\xf0\x9f\x8e\xac  Analizza Video");  /* 🎬 */
    tabs->addTab(buildVision3DTab(),     "\xf0\x9f\x93\xb7  Scan 3D");         /* 📷 */

    lay->addWidget(tabs);
}

QWidget* MultimediaPage::buildSDTab()
{
    return new StableDiffusionWidget(this);
}

/* Vision3D — scansione 3D da telefono/tablet: server HTTPS + VLM + box + depth.
 * Il server NON parte alla costruzione: l'utente lo avvia col pulsante. */
QWidget* MultimediaPage::buildVision3DTab()
{
    return new Vision3DWidget(this);
}

QWidget* MultimediaPage::buildSintetizzatoreTab()
{
    return new SintetizzatoreWidget(this);
}

QWidget* MultimediaPage::buildVoiceClonerTab()
{
    return new VoiceClonerWidget(this);
}

QWidget* MultimediaPage::buildAudioTab()
{
    auto* panel = new QWidget(this);
    auto* vbox  = new QVBoxLayout(panel);
    vbox->setContentsMargins(12, 12, 12, 12);
    vbox->setSpacing(8);

    /* Titolo */
    auto* titleLbl = new QLabel(
        "<b>\xf0\x9f\x8e\xb5  Audio AI</b> \xe2\x80\x94 "
        "Trascrivi con Whisper e analizza con AI", panel);
    titleLbl->setTextFormat(Qt::RichText);
    vbox->addWidget(titleLbl);

    /* File picker */
    auto* fileRow = new QWidget(panel);
    auto* fileLay = new QHBoxLayout(fileRow);
    fileLay->setContentsMargins(0, 0, 0, 0);
    fileLay->setSpacing(8);
    auto* fileBtn = new QPushButton(
        "\xf0\x9f\x93\x82  Carica audio", fileRow);
    fileBtn->setObjectName("actionBtn");
    fileBtn->setFixedWidth(dpiScale(140));
    m_recBtn = new QPushButton("\xf0\x9f\x8e\x99  Registra", fileRow);
    m_recBtn->setObjectName("actionBtn");
    m_recBtn->setFixedWidth(dpiScale(120));
    m_recBtn->setCheckable(true);
    m_audioFileLbl = new QLabel("Nessun file caricato", fileRow);
    m_audioFileLbl->setObjectName("hintLabel");
    fileLay->addWidget(fileBtn);
    fileLay->addWidget(m_recBtn);
    fileLay->addWidget(m_audioFileLbl, 1);
    vbox->addWidget(fileRow);

    auto* hintLbl = new QLabel(
        "\xe2\x84\xb9  Formati supportati: WAV, MP3, M4A, OGG, FLAC, AAC\n"
        "Richiede: <code>whisper-cli</code> (Impostazioni \xe2\x86\x92 Trascrivi) "
        "+ <code>ffmpeg</code> per i formati non-WAV",
        panel);
    hintLbl->setTextFormat(Qt::RichText);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    vbox->addWidget(hintLbl);

    /* Azione AI */
    auto* actionRow = new QWidget(panel);
    auto* actionLay = new QHBoxLayout(actionRow);
    actionLay->setContentsMargins(0, 0, 0, 0);
    actionLay->setSpacing(8);
    auto* actionLbl = new QLabel("Azione AI:", actionRow);
    m_audioActionCombo = new QComboBox(actionRow);
    m_audioActionCombo->addItems({
        "\xf0\x9f\x93\x9d  Riassumi",
        "\xf0\x9f\x94\x91  Punti chiave",
        "\xf0\x9f\x92\xac  Analisi sentimento",
        "\xf0\x9f\x8c\x8d  Traduci in italiano",
        "\xf0\x9f\x93\x8c  Dai un titolo",
        "\xf0\x9f\x93\x8b  Struttura in capitoli",
        "\xf0\x9f\x93\x8a  Estrai dati e statistiche",
        "\xe2\x9c\x8f  Trascrizione pura (solo testo)"
    });
    m_audioActionCombo->setFixedWidth(dpiScale(230));
    actionLay->addWidget(actionLbl);
    actionLay->addWidget(m_audioActionCombo);
    actionLay->addStretch();
    vbox->addWidget(actionRow);

    /* Pulsante Trascrivi + Analizza */
    auto* btnRow = new QWidget(panel);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);
    auto* transcribeBtn = new QPushButton(
        "\xf0\x9f\x8e\xa4  1. Trascrivi", btnRow);
    transcribeBtn->setObjectName("actionBtn");
    auto* analyzeBtn = new QPushButton(
        "\xf0\x9f\xa4\x96  2. Analizza con AI", btnRow);
    analyzeBtn->setObjectName("actionBtn");
    btnLay->addWidget(transcribeBtn);
    btnLay->addWidget(analyzeBtn);
    btnLay->addStretch();
    vbox->addWidget(btnRow);

    /* Splitter: trascrizione (sx) + output AI (dx) */
    auto* splitter = new QSplitter(Qt::Horizontal, panel);

    auto* transcriptGroup = new QGroupBox(
        "\xf0\x9f\x8e\xa4  Trascrizione", splitter);
    auto* tgLay = new QVBoxLayout(transcriptGroup);
    tgLay->setContentsMargins(4, 4, 4, 4);
    m_audioTranscript = new QTextEdit(transcriptGroup);
    m_audioTranscript->setObjectName("chatLog");
    m_audioTranscript->setPlaceholderText(
        "La trascrizione Whisper appare qui...\n\n"
        "Puoi anche incollare direttamente il testo da analizzare.");
    tgLay->addWidget(m_audioTranscript);
    splitter->addWidget(transcriptGroup);

    auto* outputGroup = new QGroupBox(
        "\xf0\x9f\xa4\x96  Analisi AI", splitter);
    auto* ogLay = new QVBoxLayout(outputGroup);
    ogLay->setContentsMargins(4, 4, 4, 4);
    m_audioOutput = new QTextEdit(outputGroup);
    m_audioOutput->setObjectName("chatLog");
    m_audioOutput->setReadOnly(true);
    m_audioOutput->setPlaceholderText(
        "L'analisi AI appare qui dopo il click su 'Analizza con AI'...");
    ogLay->addWidget(m_audioOutput);
    splitter->addWidget(outputGroup);

    splitter->setSizes({350, 450});
    vbox->addWidget(splitter, 1);

    m_audioErr = new AiErrorWidget(panel);
    vbox->addWidget(m_audioErr);

    /* ── Logica: carica file ── */
    connect(fileBtn, &QPushButton::clicked, this, &MultimediaPage::onFileBtnClicked);

    /* ── Logica: registra microfono ── */
    m_recPath = P::safeTempPath() + "/prismalux_record.wav";
    connect(m_recBtn, &QPushButton::toggled, this, &MultimediaPage::onRecBtnToggled);

    /* ── Logica: trascrivi ── */
    connect(transcribeBtn, &QPushButton::clicked, this, &MultimediaPage::onTranscribeBtnClicked);

    /* ── Logica: analizza con AI ── */
    connect(analyzeBtn, &QPushButton::clicked, this, &MultimediaPage::onAnalyzeBtnClicked);

    return panel;
}

QWidget* MultimediaPage::buildGraphvizTab()
{
    auto* panel = new QWidget(this);
    auto* vl    = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* title = new QLabel(
        "\xf0\x9f\x97\xba  <b>Mappe Concettuali \xe2\x80\x94 Graphviz</b>", panel);
    title->setObjectName("sectionTitle");
    title->setTextFormat(Qt::RichText);
    vl->addWidget(title);

    auto* hint = new QLabel(
        "Descrivi il grafo in italiano oppure incolla codice DOT direttamente.<br>"
        "L\xe2\x80\x99" "AI genera il DOT language e Graphviz produce l\xe2\x80\x99"
        "immagine.<br>"
        "<span style='color:#94a3b8;'>"
        "Installazione: <code>sudo apt install graphviz</code></span>", panel);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::RichText);
    hint->setObjectName("hintLabel");
    vl->addWidget(hint);

    m_graphvizInput = new QTextEdit(panel);
    m_graphvizInput->setPlaceholderText(
        "Es: \"Crea una mappa concettuale dei pianeti del sistema solare\"\n"
        "oppure incolla direttamente codice DOT:\n"
        "  digraph G { A -> B; B -> C; }");
    m_graphvizInput->setObjectName("chatInput");
    m_graphvizInput->setFixedHeight(dpiScale(120));
    vl->addWidget(m_graphvizInput);

    auto* btnRow = new QWidget(panel);
    auto* bl     = new QHBoxLayout(btnRow);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(8);

    auto* btnGenerate = new QPushButton(
        "\xf0\x9f\xa4\x96\xf0\x9f\x96\xbc  Genera e renderizza grafico DOT", btnRow);
    btnGenerate->setObjectName("actionBtn");
    btnGenerate->setToolTip(
        "Genera il codice DOT con l\xe2\x80\x99" "AI e lo renderizza.\n"
        "Se il testo \xc3\xa8 gi\xc3\xa0 codice DOT valido, lo renderizza direttamente.");

    bl->addWidget(btnGenerate);
    bl->addStretch(1);
    vl->addWidget(btnRow);

    m_graphvizStatus = new QLabel("", panel);
    m_graphvizStatus->setObjectName("cardDesc");
    m_graphvizStatus->setWordWrap(true);
    vl->addWidget(m_graphvizStatus);

    m_graphvizErr = new AiErrorWidget(panel);
    vl->addWidget(m_graphvizErr);

    m_graphvizImg = new QLabel(panel);
    m_graphvizImg->setAlignment(Qt::AlignCenter);
    m_graphvizImg->setMinimumHeight(200);
    m_graphvizImg->setObjectName("hintLabel");
    m_graphvizImg->setText(
        "\xf0\x9f\x97\xba  Il grafo apparir\xc3\xa0 qui");
    auto* scroll = new QScrollArea(panel);
    scroll->setWidget(m_graphvizImg);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    vl->addWidget(scroll, 1);

    connect(btnGenerate, &QPushButton::clicked, this, &MultimediaPage::onGraphvizBtnClicked);

    return panel;
}

void MultimediaPage::runGraphvizAi()
{
    const QString input = m_graphvizInput->toPlainText().trimmed();
    if (input.isEmpty()) {
        m_graphvizStatus->setText(
            "\xe2\x9a\xa0  Scrivi una descrizione del grafo che vuoi creare.");
        return;
    }

    /* Se l'input è già codice DOT valido → renderizza direttamente senza AI */
    static const QRegularExpression reDotDirect(
        "^\\s*(di)?graph\\s*\\{",
        QRegularExpression::CaseInsensitiveOption);
    if (reDotDirect.match(input).hasMatch()) {
        _renderDotCode(input);
        return;
    }

    const QString sys =
        "Sei un esperto di Graphviz. "
        "Rispondi SOLO con codice DOT valido, senza spiegazioni n\xc3\xa9 markdown. "
        "Usa digraph per grafi orientati, graph per non orientati. "
        "Aggiungi etichette chiare e usa rankdir=TB o LR se opportuno. "
        "Il codice deve iniziare con 'digraph' o 'graph' e finire con '}'.";

    const QString desc   = input;
    const QString userMsg =
        "Crea un grafo Graphviz (DOT language) per: " + desc;

    m_graphvizStatus->setText(
        "\xf0\x9f\xa4\x96  L\xe2\x80\x99" "AI sta generando il codice DOT...");
    m_graphvizImg->setText("");

    QObject::disconnect(m_graphvizFinishedConn);
    QObject::disconnect(m_graphvizErrorConn);
    m_graphvizFinishedConn = connect(m_ai, &AiClient::finished,
                                     this, &MultimediaPage::onGraphvizAiFinished);
    m_graphvizErrorConn    = connect(m_ai, &AiClient::error,
                                     this, &MultimediaPage::onGraphvizAiError);

    m_ai->chat(sys, userMsg);
}

void MultimediaPage::onFileBtnClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Seleziona file audio",
        QDir::homePath(),
        "Audio (*.wav *.mp3 *.m4a *.ogg *.flac *.aac *.opus);;"
        "Tutti i file (*)");
    if (path.isEmpty()) return;
    m_audioFilePath = path;
    m_audioFileLbl->setText(QFileInfo(path).fileName());
    m_audioTranscript->clear();
    m_audioOutput->clear();
}

void MultimediaPage::onRecBtnToggled(bool on)
{
    if (on) {
        m_recBtn->setText(tr("\xe2\x8f\xb9  Ferma registrazione"));
        m_audioFileLbl->setText(tr("\xf0\x9f\x94\xb4  Registrazione in corso..."));
        m_recProc = new QProcess(this);
        m_recProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_recProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MultimediaPage::onRecProcFinished);
        connect(m_recProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                qWarning() << "[MultimediaPage] arecord non avviato:" << m_recProc->program();
        });
        /* arecord: S16_LE 16kHz mono — formato diretto per whisper */
        m_recProc->start("arecord",
            {"-f", "S16_LE", "-r", "16000", "-c", "1", m_recPath});
    } else {
        /* ferma arecord (SIGTERM → scrive header WAV corretto) */
        if (m_recProc && m_recProc->state() != QProcess::NotRunning) {
            m_recProc->terminate();
            m_recProc->waitForFinished(2000);
        }
        m_recBtn->setText(tr("\xf0\x9f\x8e\x99  Registra"));
        if (QFile::exists(m_recPath)) {
            m_audioFilePath = m_recPath;
            m_audioFileLbl->setText(tr("\xe2\x9c\x85  prismalux_record.wav"));
            m_audioTranscript->clear();
            m_audioOutput->clear();
        } else {
            m_audioFileLbl->setText(
                "\xe2\x9d\x8c  Registrazione fallita (arecord non trovato?)");
            LogBus::post("\xe2\x9d\x8c Multimedia: Registrazione fallita (arecord non trovato?)");
        }
    }
}

void MultimediaPage::onRecProcFinished(int, QProcess::ExitStatus)
{
    if (m_recProc) {
        m_recProc->deleteLater();
        m_recProc = nullptr;
    }
}

void MultimediaPage::_doTranscribe(const QString& wav)
{
    m_audioProc = SttWhisper::transcribe(wav, "it", this,
        [this, wav](const QString& text, bool ok) {
            m_audioProc = nullptr;
            const bool isTmp = wav.contains("prisma_audio_tmp");

            if (!ok || text.trimmed().isEmpty()) {
                if (isTmp) QFile::remove(wav);
                m_audioTranscript->setPlainText(
                    "\xe2\x9a\xa0  Trascrizione vuota o fallita.\n"
                    "Verifica che il file contenga voce udibile.");
                return;
            }

            m_audioTranscript->setPlainText(text.trimmed());

            /* Diarizzazione speaker opzionale */
            if (SttWhisper::isDiarizeEnabled()) {
                m_audioTranscript->setPlaceholderText(
                    "\xf0\x9f\x91\xa5 Identifico speaker...");

                const QString tmpTxt =
                    P::safeTempPath() + "/prisma_mm_transcript.txt";
                { QFile f(tmpTxt); if (f.open(QIODevice::WriteOnly))
                      f.write(text.trimmed().toUtf8()); }

                if (m_diarizeProc) { m_diarizeProc->kill(); m_diarizeProc = nullptr; }
                m_diarizeProc = SttWhisper::diarize(
                    wav, tmpTxt,
                    SttWhisper::diarizeNSpeakers(), {},
                    this,
                    [this, wav, isTmp, tmpTxt](const QString& json, bool dok) {
                        m_diarizeProc = nullptr;
                        m_audioTranscript->setPlaceholderText({});
                        QFile::remove(tmpTxt);
                        if (isTmp) QFile::remove(wav);
                        if (dok) {
                            const QString d = SttWhisper::formatDiarization(json);
                            if (!d.isEmpty() && !d.startsWith("{\"error"))
                                m_audioTranscript->setPlainText(d);
                        }
                    });
            } else {
                if (isTmp) QFile::remove(wav);
            }
        });
}

void MultimediaPage::onTranscribeBtnClicked()
{
    if (m_audioFilePath.isEmpty()) {
        m_audioTranscript->setPlainText(
            "\xe2\x9d\x8c  Carica prima un file audio.");
        LogBus::post("\xe2\x9d\x8c Audio AI: Carica prima un file audio.");
        return;
    }

    /* ── Preferenza: server Whisper HTTP configurato ── */
    QSettings s("Prismalux", "GUI");
    const QString httpUrl = s.value(P::SK::kSttHttpUrl, "").toString().trimmed();
    if (!httpUrl.isEmpty()) {
        m_audioTranscript->setPlainText("\xe2\x8c\x9b  Invio al server Whisper HTTP...");
        QObject::disconnect(m_transcriptionReadyConn);
        QObject::disconnect(m_transcriptionErrorConn);
        m_transcriptionReadyConn = connect(
            m_ai, &AiClient::transcriptionReady,
            this, &MultimediaPage::onHttpTranscriptionReady);
        m_transcriptionErrorConn = connect(
            m_ai, &AiClient::transcriptionError,
            this, &MultimediaPage::onHttpTranscriptionError);
        m_ai->transcribeAudio(m_audioFilePath, httpUrl);
        return;
    }

    /* ── Fallback: whisper-cli locale ── */
    if (!SttWhisper::isAvailable()) {
        m_audioTranscript->setPlainText(
            "\xe2\x9a\xa0  whisper-cli o modello non trovati.\n"
            "Configurali in Impostazioni \xe2\x86\x92 Trascrivi.\n"
            "Oppure imposta un server Whisper HTTP in Impostazioni \xe2\x86\x92 Trascrivi.");
        return;
    }
    m_audioTranscript->setPlainText("\xe2\x8c\x9b  Trascrizione in corso...");

    const QString ext = QFileInfo(m_audioFilePath).suffix().toLower();
    if (ext == "wav") {
        _doTranscribe(m_audioFilePath);
    } else {
        m_ffmpegWavTmp = P::safeTempPath() + "/prisma_audio_tmp.wav";
        m_ffmpegProc   = new QProcess(this);
        connect(m_ffmpegProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MultimediaPage::onFfmpegFinished);
        connect(m_ffmpegProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                qWarning() << "[MultimediaPage] ffmpeg audio conversion non avviato:" << m_ffmpegProc->program();
        });
        m_ffmpegProc->start(FfmpegUtils::findFfmpeg(),
            FfmpegUtils::whisperArgs(m_audioFilePath, m_ffmpegWavTmp));
    }
}

void MultimediaPage::onHttpTranscriptionReady(const QString& text)
{
    QObject::disconnect(m_transcriptionReadyConn);
    QObject::disconnect(m_transcriptionErrorConn);
    m_transcriptionReadyConn = m_transcriptionErrorConn = {};
    m_audioTranscript->setPlainText(text);
}

void MultimediaPage::onHttpTranscriptionError(const QString& msg)
{
    QObject::disconnect(m_transcriptionReadyConn);
    QObject::disconnect(m_transcriptionErrorConn);
    m_transcriptionReadyConn = m_transcriptionErrorConn = {};
    m_audioTranscript->setPlainText(
        "\xe2\x9d\x8c  Errore trascrizione HTTP:\n" + msg);
    m_audioErr->showError(msg);
    LogBus::post("\xe2\x9d\x8c Audio AI: Errore trascrizione HTTP: " + msg);
}

void MultimediaPage::onFfmpegFinished(int code, QProcess::ExitStatus)
{
    m_ffmpegProc->deleteLater();
    m_ffmpegProc = nullptr;
    if (code == 0) {
        _doTranscribe(m_ffmpegWavTmp);
    } else {
        m_audioTranscript->setPlainText(
            "\xe2\x9d\x8c  Conversione fallita.\n"
            "Installa ffmpeg: sudo apt install ffmpeg");
        LogBus::post("\xe2\x9d\x8c Audio AI: Conversione fallita (ffmpeg).");
    }
}

void MultimediaPage::onAnalyzeBtnClicked()
{
    const QString transcript = m_audioTranscript->toPlainText().trimmed();
    if (transcript.isEmpty()) {
        m_audioOutput->setPlainText(
            "\xe2\x9d\x8c  Trascrivi prima il file audio (o incolla il testo).");
        LogBus::post("\xe2\x9d\x8c Audio AI: Trascrivi prima il file audio (o incolla il testo).");
        return;
    }

    static const char* kAudioActions[] = {
        "Riassumi il seguente testo trascritto da audio in modo conciso e chiaro. Rispondi in italiano.",
        "Estrai i punti chiave principali dal seguente testo trascritto da audio. Elencali numerati. Rispondi in italiano.",
        "Analizza il tono emotivo e il sentimento del seguente testo trascritto da audio. Rispondi in italiano.",
        "Traduci il seguente testo in italiano, mantenendo il significato e il registro originale.",
        "Dai un titolo breve ed efficace al seguente testo trascritto da audio. Proponi 3 alternative. Rispondi in italiano.",
        "Struttura il seguente testo trascritto da audio in capitoli/sezioni con titoli. Rispondi in italiano.",
        "Estrai dati, numeri, statistiche e informazioni quantitative dal seguente testo. Elencali in modo strutturato. Rispondi in italiano.",
        "Trascrivi e formatta il seguente testo in modo pulito, correggendo eventuali errori di trascrizione. Rispondi in italiano.",
    };
    const int idx = m_audioActionCombo->currentIndex();
    const QString sys = P::prependKnowledge(
        idx >= 0 && idx < 8 ? QString::fromUtf8(kAudioActions[idx])
                            : "Analizza il testo fornito. Rispondi in italiano.");

    m_audioOutput->clear();
    m_audioOutput->setPlaceholderText(
        "\xe2\x8c\x9b  Analisi AI in corso...");

    QObject::disconnect(m_audioTokenConn);
    QObject::disconnect(m_audioFinishedConn);
    QObject::disconnect(m_audioErrorConn);
    m_audioTokenConn    = connect(m_ai, &AiClient::token,
                                  this, &MultimediaPage::onAudioToken);
    m_audioFinishedConn = connect(m_ai, &AiClient::finished,
                                  this, &MultimediaPage::onAudioAnalyzeFinished);
    m_audioErrorConn    = connect(m_ai, &AiClient::error,
                                  this, &MultimediaPage::onAudioAnalyzeError);

    m_ai->chat(sys, "Testo trascritto dall'audio:\n\n" + transcript);
}

void MultimediaPage::onGraphvizBtnClicked()
{
    runGraphvizAi();
}

void MultimediaPage::onGraphvizProcFinished(int code, QProcess::ExitStatus)
{
    if (code == 0) {
        QPixmap px(m_graphvizTmpPng);
        if (!px.isNull()) {
            m_graphvizImg->setPixmap(
                px.scaledToWidth(qMin(px.width(), 900),
                                 Qt::SmoothTransformation));
            m_graphvizStatus->setText(
                "\xe2\x9c\x85  Grafo generato. "
                "Immagine: " + m_graphvizTmpPng);
        }
    } else {
        const QString err =
            QString::fromLocal8Bit(m_graphvizProc->readAllStandardError());
        m_graphvizStatus->setText(
            "\xe2\x9d\x8c  Errore Graphviz: " + err.left(200) +
            "\n\xe2\x84\xb9  Installa: sudo apt install graphviz");
        LogBus::post("\xe2\x9d\x8c Graphviz: Errore Graphviz: " + err.left(200));
    }
}

void MultimediaPage::_renderDotCode(const QString& dot)
{
    const QString tmpDot = QDir::tempPath() + "/prismalux_graph.dot";
    m_graphvizTmpPng     = QDir::tempPath() + "/prismalux_graph.png";
    {
        QFile f(tmpDot);
        if (f.open(QFile::WriteOnly | QFile::Text))
            QTextStream(&f) << dot;
    }
    m_graphvizStatus->setText(tr("\xe2\x8f\xb3  Rendering con Graphviz..."));
    m_graphvizImg->setText("");

    if (m_graphvizProc && m_graphvizProc->state() != QProcess::NotRunning)
        m_graphvizProc->kill();

    m_graphvizProc = new QProcess(this);
    connect(m_graphvizProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MultimediaPage::onGraphvizProcFinished);
    connect(m_graphvizProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[MultimediaPage] graphviz dot non avviato:" << m_graphvizProc->program();
    });
    m_graphvizProc->start("dot", {"-Tpng", tmpDot, "-o", m_graphvizTmpPng});
    if (!m_graphvizProc->waitForStarted(P::kProcessStartTimeoutMs)) {
        m_graphvizStatus->setText(
            "\xe2\x9d\x8c  Graphviz non trovato. "
            "Installa con: <b>sudo apt install graphviz</b>");
        LogBus::post("\xe2\x9d\x8c Graphviz: Graphviz non trovato nel PATH.");
        m_graphvizProc->deleteLater();
        m_graphvizProc = nullptr;
    }
}

void MultimediaPage::onGraphvizAiFinished(const QString& full)
{
    QObject::disconnect(m_graphvizFinishedConn);
    QObject::disconnect(m_graphvizErrorConn);
    m_graphvizFinishedConn = {};
    m_graphvizErrorConn    = {};

    QString dot = full;
    static const QRegularExpression reThink(
        "<think>[\\s\\S]*?</think>",
        QRegularExpression::CaseInsensitiveOption);
    dot.remove(reThink);
    const int te = dot.indexOf("</think>", 0, Qt::CaseInsensitive);
    if (te >= 0) dot = dot.mid(te + 8);
    dot = dot.trimmed();

    int s = dot.indexOf("```dot\n");
    if (s < 0) s = dot.indexOf("```\n");
    if (s >= 0)
        dot = dot.mid(s).section("```", 1, 1).trimmed();

    static const QRegularExpression reDotStart(
        "(di)?graph\\s*\\{",
        QRegularExpression::CaseInsensitiveOption);
    const int gs = dot.indexOf(reDotStart);
    if (gs >= 0) dot = dot.mid(gs);

    m_graphvizInput->setPlainText(dot);
    _renderDotCode(dot);
}

void MultimediaPage::onGraphvizAiError(const QString& msg)
{
    QObject::disconnect(m_graphvizFinishedConn);
    QObject::disconnect(m_graphvizErrorConn);
    m_graphvizFinishedConn = {};
    m_graphvizErrorConn    = {};
    m_graphvizStatus->setText(tr("\xe2\x9d\x8c  Errore AI"));
    m_graphvizErr->showError(msg, [this]{ runGraphvizAi(); });
    LogBus::post("\xe2\x9d\x8c Graphviz: Errore AI: " + msg);
}

void MultimediaPage::onAudioToken(const QString& t)
{
    m_audioOutput->moveCursor(QTextCursor::End);
    m_audioOutput->insertPlainText(t);
}

void MultimediaPage::onAudioAnalyzeFinished(const QString&)
{
    QObject::disconnect(m_audioTokenConn);
    QObject::disconnect(m_audioFinishedConn);
    QObject::disconnect(m_audioErrorConn);
    m_audioTokenConn = m_audioFinishedConn = m_audioErrorConn = {};
}

void MultimediaPage::onAudioAnalyzeError(const QString& msg)
{
    QObject::disconnect(m_audioTokenConn);
    QObject::disconnect(m_audioFinishedConn);
    QObject::disconnect(m_audioErrorConn);
    m_audioTokenConn = m_audioFinishedConn = m_audioErrorConn = {};
    m_audioErr->showError(msg);
    LogBus::post("\xe2\x9d\x8c Audio AI: Errore analisi: " + msg);
}

/* ══════════════════════════════════════════════════════════════
   buildOcrTab — OCR webcam da webcam
   ══════════════════════════════════════════════════════════════ */
QWidget* MultimediaPage::buildOcrTab()
{
    auto* panel = new QWidget(this);
    auto* vbox  = new QVBoxLayout(panel);
    vbox->setContentsMargins(12, 12, 12, 12);
    vbox->setSpacing(8);

    auto* title = new QLabel(
        "<b>\xf0\x9f\x94\x8d  OCR webcam</b>"
        " \xe2\x80\x94 Legge testo da webcam in tempo reale", panel);
    title->setTextFormat(Qt::RichText);
    vbox->addWidget(title);

    auto* hintRow = new QHBoxLayout;
    hintRow->setSpacing(8);

    auto* hint = new QLabel(
        "\xe2\x84\xb9  Richiede: "
        "<code>sudo apt install tesseract-ocr tesseract-ocr-ita</code>"
        " + cv2/pytesseract via venv OpenCV (<b>Installa venv</b>) o sistema", panel);
    hint->setTextFormat(Qt::RichText);
    hint->setObjectName("hintLabel");
    hint->setWordWrap(true);
    hintRow->addWidget(hint, 1);

    /* Pulsante: installa venv OpenCV dedicato in Frameworks/opencv/venv/ */
    auto* btnVenv = new QPushButton(
        "\xf0\x9f\x90\x8d  Installa venv OpenCV", panel);  /* 🐍 */
    btnVenv->setObjectName("navBtn");
    btnVenv->setFixedHeight(dpiScale(28));
    btnVenv->setToolTip(
        "Crea Frameworks/opencv/venv/ con cv2 + pytesseract isolati.\n"
        "Consigliato: evita conflitti con i pacchetti di sistema.");
    connect(btnVenv, &QPushButton::clicked, panel, [btnVenv]{
        const QString script = OpencvUtils::setupVenvScript();
        const QString path   = OpencvUtils::venvDir() + "/../setup_venv.sh";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(script.toUtf8());
            f.close();
            QFile::setPermissions(path,
                QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                QFile::ReadGroup | QFile::ReadOther);
        }
        auto* proc = new QProcess(btnVenv);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        QObject::connect(proc, &QProcess::errorOccurred,
                btnVenv, [proc, btnVenv](QProcess::ProcessError err) {
                    if (err == QProcess::FailedToStart) {
                        qWarning() << "[MultimediaPage] bash venv non avviato:" << proc->program();
                        btnVenv->setEnabled(true);
                        btnVenv->setText(tr("\xe2\x9d\x8c  bash non trovato"));
                    }
                });
        proc->start("bash", {path});
        btnVenv->setText(tr("\xe2\x8f\xb3  Installazione..."));
        btnVenv->setEnabled(false);
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                btnVenv, [btnVenv, proc](int code, QProcess::ExitStatus){
                    proc->deleteLater();
                    btnVenv->setEnabled(true);
                    btnVenv->setText(code == 0
                        ? tr("\xe2\x9c\x85  Venv pronto")
                        : tr("\xe2\x9d\x8c  Errore venv"));
                });
    });
    hintRow->addWidget(btnVenv);

    auto* btnCopyCmd = new QPushButton(
        "\xf0\x9f\x93\x8b  Copia comandi", panel);  /* 📋 */
    btnCopyCmd->setObjectName("navBtn");
    btnCopyCmd->setFixedHeight(dpiScale(28));
    btnCopyCmd->setToolTip(
        "Copia i comandi di installazione sistema negli appunti:\n"
        "sudo apt install python3-opencv tesseract-ocr tesseract-ocr-ita -y\n"
        "python3 -m pip install pytesseract --break-system-packages");
    connect(btnCopyCmd, &QPushButton::clicked, panel, [btnCopyCmd]{
        QApplication::clipboard()->setText(
            "sudo apt install python3-opencv tesseract-ocr tesseract-ocr-ita -y\n"
            "python3 -m pip install pytesseract --break-system-packages");
        const QString orig = btnCopyCmd->text();
        btnCopyCmd->setText(tr("\xe2\x9c\x85  Copiato!"));
        QTimer::singleShot(2000, btnCopyCmd, [btnCopyCmd, orig]{
            btnCopyCmd->setText(orig);
        });
    });
    hintRow->addWidget(btnCopyCmd);

    vbox->addLayout(hintRow);

    /* ── Riga controlli ── */
    auto* ctrlRow = new QWidget(panel);
    auto* cl = new QHBoxLayout(ctrlRow);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(8);

    m_ocrStartBtn = new QPushButton(
        "\xe2\x96\xb6  Avvia webcam", ctrlRow);  /* ▶ */
    m_ocrStartBtn->setObjectName("actionBtn");
    m_ocrStartBtn->setCheckable(true);
    cl->addWidget(m_ocrStartBtn);

    cl->addWidget(new QLabel("Intervallo:", ctrlRow));
    m_ocrInterval = new QSpinBox(ctrlRow);
    m_ocrInterval->setRange(1, 60);
    m_ocrInterval->setValue(3);
    m_ocrInterval->setSuffix(" s");
    m_ocrInterval->setToolTip(tr("Secondi tra una scansione webcam e l'altra"));
    cl->addWidget(m_ocrInterval);

    cl->addWidget(new QLabel("Risoluzione:", ctrlRow));
    m_ocrResCambo = new QComboBox(ctrlRow);
    m_ocrResCambo->addItem("640\xc3\x97" "480  (veloce)",   QStringLiteral("640x480"));
    m_ocrResCambo->addItem("1280\xc3\x97" "720  (consigliata)", QStringLiteral("1280x720"));
    m_ocrResCambo->addItem("1920\xc3\x97" "1080 (qualit\xc3\xa0 max)", QStringLiteral("1920x1080"));
    m_ocrResCambo->setCurrentIndex(1);   /* 720p default */
    m_ocrResCambo->setToolTip(
        tr("Risoluzione di cattura webcam.\n"
           "Pi\xc3\xb9 alta = testo pi\xc3\xb9 nitido, pi\xc3\xb9 lenta.\n"
           "720p \xc3\xa8 il miglior compromesso velocit\xc3\xa0/qualit\xc3\xa0."));
    connect(m_ocrResCambo, &QComboBox::currentIndexChanged,
            this, [this](int) {
                if (m_ocrDaemon && m_ocrDaemon->state() == QProcess::Running)
                    m_ocrDaemon->write(
                        ("SETRES:" + m_ocrResCambo->currentData().toString() + "\n").toUtf8());
            });
    cl->addWidget(m_ocrResCambo);

    /* Separatore visivo */
    auto* sep = new QFrame(ctrlRow);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    cl->addWidget(sep);

    /* Carica video */
    auto* btnVideo = new QPushButton(
        "\xf0\x9f\x93\xb9  Carica video", ctrlRow);  /* 📹 */
    btnVideo->setObjectName("actionBtn");
    btnVideo->setToolTip(
        "Carica un video registrato (MP4, AVI, MOV, MKV)\n"
        "Estrae automaticamente un frame ogni N secondi");
    connect(btnVideo, &QPushButton::clicked,
            this, &MultimediaPage::onOcrLoadVideoClicked);
    cl->addWidget(btnVideo);

    cl->addWidget(new QLabel("1 frame ogni:", ctrlRow));
    m_ocrVideoStep = new QSpinBox(ctrlRow);
    m_ocrVideoStep->setRange(1, 30);
    m_ocrVideoStep->setValue(2);
    m_ocrVideoStep->setSuffix(" s");
    m_ocrVideoStep->setToolTip(tr("Estrai 1 frame ogni N secondi di video"));
    cl->addWidget(m_ocrVideoStep);

    m_ocrVideoLbl = new QLabel("Nessun video", ctrlRow);
    m_ocrVideoLbl->setObjectName("hintLabel");
    m_ocrVideoLbl->setMaximumWidth(160);
    cl->addWidget(m_ocrVideoLbl);

    m_ocrTranscribeBtn = new QPushButton(
        "\xf0\x9f\x8e\xa4  Trascrivi audio", ctrlRow);  /* 🎤 */
    m_ocrTranscribeBtn->setObjectName("actionBtn");
    m_ocrTranscribeBtn->setToolTip(
        "Estrae l'audio dal video con ffmpeg e lo trascrive con Whisper.\n"
        "Il testo viene aggiunto all'area OCR.");
    m_ocrTranscribeBtn->setEnabled(false);  /* abilitato solo con video caricato */
    connect(m_ocrTranscribeBtn, &QPushButton::clicked,
            this, &MultimediaPage::onOcrTranscribeAudioClicked);
    cl->addWidget(m_ocrTranscribeBtn);

    auto* btnClear = new QPushButton(
        "\xf0\x9f\x97\x91  Cancella", ctrlRow);  /* 🗑 */
    btnClear->setObjectName("actionBtn");
    connect(btnClear, &QPushButton::clicked, panel, [this]{
        m_ocrText->clear();
        m_ocrSeenLines.clear();
    });
    cl->addWidget(btnClear);

    cl->addStretch(1);

    m_ocrStatus = new QLabel("Pronto.", panel);
    m_ocrStatus->setObjectName("statusLabel");
    cl->addWidget(m_ocrStatus);

    vbox->addWidget(ctrlRow);

    /* ── Riga filtri ── */
    auto* filterRow = new QWidget(panel);
    auto* fl = new QHBoxLayout(filterRow);
    fl->setContentsMargins(0, 2, 0, 2);
    fl->setSpacing(16);

    fl->addWidget(new QLabel("\xf0\x9f\xaa\x9b  Filtri:", panel));  /* 🪛 */

    m_ocrChkDedup = new QCheckBox("Deduplicazione (righe gi\xc3\xa0 viste)", panel);
    m_ocrChkDedup->setChecked(true);
    m_ocrChkDedup->setToolTip(tr("Scarta le righe identiche gi\xc3\xa0 presenti nell'area testo"));
    fl->addWidget(m_ocrChkDedup);

    m_ocrChkAlpha = new QCheckBox("Solo testo (>50% lettere)", panel);
    m_ocrChkAlpha->setChecked(true);
    m_ocrChkAlpha->setToolTip(tr("Scarta righe composte principalmente da numeri o simboli"));
    fl->addWidget(m_ocrChkAlpha);

    m_ocrChkMinLen = new QCheckBox("Lunghezza \xe2\x89\xa5 4 caratteri", panel);  /* ≥ */
    m_ocrChkMinLen->setChecked(true);
    m_ocrChkMinLen->setToolTip(tr("Scarta parole/frammenti troppo corti"));
    fl->addWidget(m_ocrChkMinLen);

    /* separatore visivo */
    auto* sepFilter = new QFrame(filterRow);
    sepFilter->setFrameShape(QFrame::VLine);
    sepFilter->setFrameShadow(QFrame::Sunken);
    fl->addWidget(sepFilter);

    m_ocrChkSkipUnchanged = new QCheckBox("Salta OCR se scena invariata", panel);
    m_ocrChkSkipUnchanged->setChecked(true);
    m_ocrChkSkipUnchanged->setToolTip(
        tr("Confronta l'istogramma di colore del frame corrente con l'ultimo scansionato.\n"
           "Se la somiglianza supera la soglia, Tesseract viene saltato (~5ms invece di ~1-2s)."));
    fl->addWidget(m_ocrChkSkipUnchanged);

    fl->addWidget(new QLabel("Soglia:", panel));
    m_ocrThreshSpin = new QDoubleSpinBox(panel);
    m_ocrThreshSpin->setRange(0.80, 0.999);
    m_ocrThreshSpin->setSingleStep(0.01);
    m_ocrThreshSpin->setDecimals(3);
    m_ocrThreshSpin->setValue(0.970);
    m_ocrThreshSpin->setToolTip(
        tr("Correlazione istogramma: 1.0=identici, 0.80=molto diversi.\n"
           "0.97 = salta OCR se il 97% della distribuzione colori \xc3\xa8 uguale."));  /* è */
    connect(m_ocrThreshSpin, &QDoubleSpinBox::valueChanged,
            this, [this](double v) {
                if (m_ocrDaemon && m_ocrDaemon->state() == QProcess::Running) {
                    const bool skip = m_ocrChkSkipUnchanged && m_ocrChkSkipUnchanged->isChecked();
                    const double thresh = skip ? v : 2.0;
                    m_ocrDaemon->write(
                        QString("SETTHRESH:%1\n").arg(thresh, 0, 'f', 3).toUtf8());
                }
            });
    connect(m_ocrChkSkipUnchanged, &QCheckBox::toggled,
            this, [this](bool on) {
                if (m_ocrDaemon && m_ocrDaemon->state() == QProcess::Running) {
                    const double thresh = on ? m_ocrThreshSpin->value() : 2.0;
                    m_ocrDaemon->write(
                        QString("SETTHRESH:%1\n").arg(thresh, 0, 'f', 3).toUtf8());
                }
            });
    fl->addWidget(m_ocrThreshSpin);

    fl->addStretch(1);
    vbox->addWidget(filterRow);

    /* ── Splitter: anteprima | testo OCR ── */
    auto* splitter = new QSplitter(Qt::Horizontal, panel);

    auto* previewBox = new QGroupBox(
        "\xf0\x9f\x93\xb7  Anteprima webcam", splitter);  /* 📷 */
    auto* pbLay = new QVBoxLayout(previewBox);
    pbLay->setContentsMargins(4, 4, 4, 4);
    m_ocrPreview = new QLabel(previewBox);
    m_ocrPreview->setAlignment(Qt::AlignCenter);
    m_ocrPreview->setMinimumSize(280, 200);
    m_ocrPreview->setText(tr("\xf0\x9f\x93\xb7  In attesa..."));
    m_ocrPreview->setObjectName("hintLabel");
    pbLay->addWidget(m_ocrPreview, 1);
    splitter->addWidget(previewBox);

    auto* textBox = new QGroupBox(
        "\xf0\x9f\x93\x84  Testo OCR rilevato", splitter);  /* 📄 */
    auto* tbLay = new QVBoxLayout(textBox);
    tbLay->setContentsMargins(4, 4, 4, 4);
    m_ocrText = new QTextEdit(textBox);
    m_ocrText->setObjectName("chatLog");
    m_ocrText->setPlaceholderText(
        "Il testo rilevato dalla webcam apparir\xc3\xa0 qui...\n\n"
        "Punta la camera su un manuale o documento stampato.");
    tbLay->addWidget(m_ocrText);
    splitter->addWidget(textBox);

    splitter->setSizes({300, 500});
    vbox->addWidget(splitter, 2);

    /* ── Analisi AI ── */
    auto* aiRow = new QHBoxLayout;
    auto* btnAnalyze = new QPushButton(
        "\xf0\x9f\xa4\x96  Analizza testo con AI", panel);  /* 🤖 */
    btnAnalyze->setObjectName("actionBtn");
    connect(btnAnalyze, &QPushButton::clicked,
            this, &MultimediaPage::onOcrAnalyzeClicked);
    aiRow->addWidget(btnAnalyze);
    aiRow->addStretch(1);
    vbox->addLayout(aiRow);

    auto* aiOutBox = new QGroupBox(
        "\xf0\x9f\xa4\x96  Analisi AI del manuale", panel);
    auto* aoLay = new QVBoxLayout(aiOutBox);
    aoLay->setContentsMargins(4, 4, 4, 4);
    m_ocrAiOut = new QTextEdit(aiOutBox);
    m_ocrAiOut->setObjectName("chatLog");
    m_ocrAiOut->setReadOnly(true);
    m_ocrAiOut->setMaximumHeight(160);
    m_ocrAiOut->setPlaceholderText(
        "Riassunto, punti chiave o analisi del testo scansionato...");
    aoLay->addWidget(m_ocrAiOut);
    vbox->addWidget(aiOutBox);

    /* ── Timer per scansione periodica ── */
    m_ocrTimer = new QTimer(panel);
    m_ocrTimer->setSingleShot(false);
    connect(m_ocrTimer, &QTimer::timeout,
            this, &MultimediaPage::onOcrTimerTick);

    m_ocrPreviewTimer = new QTimer(panel);
    m_ocrPreviewTimer->setSingleShot(false);
    m_ocrPreviewTimer->setInterval(200);   /* ~5fps live preview */
    connect(m_ocrPreviewTimer, &QTimer::timeout,
            this, &MultimediaPage::onOcrPreviewTick);

    connect(m_ocrStartBtn, &QPushButton::toggled,
            this, &MultimediaPage::onOcrStartStopClicked);

    return panel;
}

/* Script Python del daemon OCR — rimane in ascolto su stdin,
   importa le librerie UNA SOLA VOLTA all'avvio, poi risponde a comandi stdin.
   Comandi:
     SCAN              → cattura un frame dalla webcam
     VIDEO:path:step   → estrae frame dal video ogni `step` secondi */
static QString ocrDaemonScript(const QString& previewPath)
{
    return QString(R"PY(
import sys, json, re as _re
import numpy as _np
sys.stdout.reconfigure(line_buffering=True)
try:
    import cv2
    import pytesseract
except ImportError as e:
    print(json.dumps({'error': f'Dipendenza mancante: {e}\n\nInstalla con:\n  sudo apt install python3-opencv python3-pandas tesseract-ocr tesseract-ocr-ita -y\n  python3 -m pip install pytesseract --break-system-packages'}), flush=True)
    sys.exit(0)

PREVIEW = '%1'

# ── Frame matching via istogramma di colore ──────────────────────────
_last_hist   = None
_skip_thresh = 0.970

def _frame_hist(frame):
    small = cv2.resize(frame, (64, 64), interpolation=cv2.INTER_NEAREST)
    hsv   = cv2.cvtColor(small, cv2.COLOR_BGR2HSV)
    h_h = cv2.calcHist([hsv], [0], None, [32], [0, 180])
    h_s = cv2.calcHist([hsv], [1], None, [32], [0, 256])
    h_v = cv2.calcHist([hsv], [2], None, [32], [0, 256])
    for h in (h_h, h_s, h_v):
        cv2.normalize(h, h)
    return _np.concatenate([h_h.flatten(), h_s.flatten(), h_v.flatten()])

def _hist_similarity(a, b):
    denom = (_np.std(a) * _np.std(b))
    if denom < 1e-9:
        return 1.0
    return float(_np.corrcoef(a, b)[0, 1])

# ── Deskew prospettico ────────────────────────────────────────────────
def _four_point_transform(image, pts):
    """Warp prospettico dati 4 punti nell'ordine TL,TR,BR,BL.
    Aggiunge 2% di padding su ogni lato per non troncare i caratteri di bordo."""
    s    = pts.sum(axis=1)
    diff = _np.diff(pts, axis=1)
    rect = _np.zeros((4, 2), dtype='float32')
    rect[0] = pts[_np.argmin(s)]     # top-left
    rect[2] = pts[_np.argmax(s)]     # bottom-right
    rect[1] = pts[_np.argmin(diff)]  # top-right
    rect[3] = pts[_np.argmax(diff)]  # bottom-left
    tl, tr, br, bl = rect
    w = int(max(_np.linalg.norm(br - bl), _np.linalg.norm(tr - tl)))
    h = int(max(_np.linalg.norm(tr - br), _np.linalg.norm(tl - bl)))
    if w < 10 or h < 10:
        return image
    # Padding 2% per lato — evita troncatura testo a bordo
    pw = max(6, int(w * 0.02))
    ph = max(6, int(h * 0.02))
    dst = _np.array([[pw, ph],[w-1+pw, ph],[w-1+pw, h-1+ph],[pw, h-1+ph]], dtype='float32')
    M   = cv2.getPerspectiveTransform(rect, dst)
    return cv2.warpPerspective(image, M, (w + 2*pw, h + 2*ph),
                               borderMode=cv2.BORDER_REPLICATE)

def _auto_deskew(frame):
    """Rileva il documento più grande e lo raddrizza prospetticamente.
       Se non trovato, restituisce il frame originale."""
    h0, w0 = frame.shape[:2]
    sc = min(1.0, 800.0 / max(h0, w0, 1))
    small = cv2.resize(frame, (int(w0 * sc), int(h0 * sc)))
    gray  = cv2.cvtColor(small, cv2.COLOR_BGR2GRAY)
    gray  = cv2.GaussianBlur(gray, (5, 5), 0)
    edged = cv2.Canny(gray, 30, 150)
    edged = cv2.dilate(edged, _np.ones((5, 5), _np.uint8), iterations=2)
    cnts, _ = cv2.findContours(edged, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    cnts = sorted(cnts, key=cv2.contourArea, reverse=True)[:8]
    min_area = h0 * w0 * sc * sc * 0.08   # almeno 8% del frame
    for c in cnts:
        if cv2.contourArea(c) < min_area:
            continue
        peri  = cv2.arcLength(c, True)
        approx = cv2.approxPolyDP(c, 0.02 * peri, True)
        if len(approx) == 4:
            pts = (approx.reshape(4, 2) / sc).astype('float32')
            return _four_point_transform(frame, pts)
    return frame   # nessun quadrilatero trovato

# ── OCR su singolo frame ──────────────────────────────────────────────
def ocr_frame(frame):
    """Preprocessing avanzato + Tesseract; restituisce righe di testo."""
    # 1. Anteprima prima del deskew (più rappresentativa)
    cv2.imwrite(PREVIEW, cv2.resize(frame, (320, 240)))

    # 2. Raddrizzamento prospettico
    frame = _auto_deskew(frame)

    # 3. Upscale al lato lungo >= 2000px (fondamentale per testo piccolo)
    h, w = frame.shape[:2]
    long_side = max(h, w)
    if long_side < 2000:
        sc = 2000.0 / long_side
        frame = cv2.resize(frame, (int(w * sc), int(h * sc)),
                           interpolation=cv2.INTER_CUBIC)

    # 4. Converti in grigio
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # 5. CLAHE: contrasto adattivo (regge illuminazione non uniforme)
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
    gray  = clahe.apply(gray)

    # 6. Sharpening leggero per bordi lettere
    kernel_sharp = _np.array([[0,-1,0],[-1,5,-1],[0,-1,0]], dtype=_np.float32)
    gray = cv2.filter2D(gray, -1, kernel_sharp)

    # 7. Threshold adattivo (meglio di Otsu su background non uniforme)
    gray = cv2.adaptiveThreshold(gray, 255,
        cv2.ADAPTIVE_THRESH_GAUSSIAN_C, cv2.THRESH_BINARY, 31, 10)

    # 8. Tesseract: PSM 3 (auto page layout), MIN_CONF 48
    data = pytesseract.image_to_data(gray, lang='ita+eng',
        config='--psm 3 --oem 3', output_type=pytesseract.Output.DICT)
    MIN_CONF = 48
    seen_key = None
    line_buf = []
    raw_lines = []
    n = len(data['text'])
    for i in range(n):
        conf = int(data['conf'][i])
        word = data['text'][i].strip()
        if conf < MIN_CONF or len(word) < 2:
            continue
        if _re.fullmatch(r'[\W_]+', word):
            continue
        key = (data['block_num'][i], data['par_num'][i], data['line_num'][i])
        if key != seen_key:
            if line_buf:
                j = ' '.join(line_buf)
                if len(j) >= 3:
                    raw_lines.append(j)
            line_buf = []
            seen_key = key
        line_buf.append(word)
    if line_buf:
        j = ' '.join(line_buf)
        if len(j) >= 3:
            raw_lines.append(j)

    # Filtro qualità post-OCR: scarta spazzatura ma preserva email/@, tel/numeri, URL
    def _line_is_useful(line):
        # Contiene email o URL → sempre utile
        if '@' in line or 'http' in line.lower():
            return True
        # Contiene almeno 3 cifre consecutive → indirizzo/telefono
        if _re.search(r'\d{3,}', line):
            return True
        # Lunghezza media parola >= 4 (parole reali, non frammenti)
        words = line.split()
        if not words:
            return False
        avg_len = sum(len(w) for w in words) / len(words)
        return avg_len >= 4.0

    return [l for l in raw_lines if _line_is_useful(l)]

# Apri webcam: 720p default, 30fps
cap = cv2.VideoCapture(0)
if not cap.isOpened():
    print(json.dumps({'error': 'Webcam non disponibile — controlla /dev/video0'}), flush=True)
    sys.exit(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT,  720)
cap.set(cv2.CAP_PROP_FPS, 30)
# Warmup: scarta i frame di inizializzazione (alcuni driver ne accumulano diversi)
for _ in range(5):
    cap.read()
print(json.dumps({'ready': True}), flush=True)

def _read_fresh(cap):
    """Legge due frame e scarta il primo: quello rimasto nel buffer interno."""
    cap.read()        # scarta frame vecchio
    return cap.read() # restituisce quello corrente

# Loop comandi
for raw_line in sys.stdin:
    cmd = raw_line.strip()
    if not cmd:
        continue

    # ── SETRES:WxH — cambia risoluzione webcam a caldo ──
    if cmd.startswith('SETRES:'):
        try:
            wh = cmd.split(':', 1)[1].split('x')
            cap.set(cv2.CAP_PROP_FRAME_WIDTH,  int(wh[0]))
            cap.set(cv2.CAP_PROP_FRAME_HEIGHT, int(wh[1]))
            for _ in range(3): cap.read()   # svuota buffer con nuova res
        except Exception:
            pass
        continue

    # ── SETTHRESH:x.xxx — aggiorna soglia match scena a caldo ──
    if cmd.startswith('SETTHRESH:'):
        try:
            _skip_thresh = float(cmd.split(':', 1)[1])
        except Exception:
            pass
        continue

    # ── PREVIEW: solo frame visivo, senza OCR (~15ms) ──
    if cmd == 'PREVIEW':
        ret, frame = _read_fresh(cap)
        if ret:
            cv2.imwrite(PREVIEW, cv2.resize(frame, (320, 240)))
            print(json.dumps({'preview': PREVIEW, 'preview_only': True}), flush=True)
        continue

    # ── SCAN: singolo frame dalla webcam ──
    if cmd == 'SCAN':
        ret, frame = _read_fresh(cap)
        if not ret:
            print(json.dumps({'error': 'Frame non catturato'}), flush=True)
            continue

        # Confronto istogramma in RAM — salta OCR se scena troppo simile
        if _skip_thresh <= 1.0 and _last_hist is not None:
            cur_hist = _frame_hist(frame)
            sim = _hist_similarity(_last_hist, cur_hist)
            if sim >= _skip_thresh:
                # Aggiorna comunque l'anteprima per il timer veloce
                cv2.imwrite(PREVIEW, cv2.resize(frame, (320, 240)))
                print(json.dumps({
                    'unchanged': True,
                    'similarity': round(sim, 4),
                    'preview': PREVIEW
                }), flush=True)
                continue
        else:
            cur_hist = _frame_hist(frame)

        _last_hist = cur_hist
        lines = ocr_frame(frame)
        print(json.dumps({'text': '\n'.join(lines), 'preview': PREVIEW}), flush=True)

    # ── VIDEO:path:step — estrae frame ogni step secondi ──
    elif cmd.startswith('VIDEO:'):
        parts = cmd.split(':', 2)
        vid_path = parts[1]
        step_sec = float(parts[2]) if len(parts) > 2 else 2.0
        vcap = cv2.VideoCapture(vid_path)
        if not vcap.isOpened():
            print(json.dumps({'error': f'Impossibile aprire il video: {vid_path}'}), flush=True)
            continue
        fps = vcap.get(cv2.CAP_PROP_FPS) or 25
        total_frames = int(vcap.get(cv2.CAP_PROP_FRAME_COUNT))
        step_frames = max(1, int(fps * step_sec))
        processed = 0
        frame_idx = 0
        while True:
            vcap.set(cv2.CAP_PROP_POS_FRAMES, frame_idx)
            ret, frame = vcap.read()
            if not ret:
                break
            lines = ocr_frame(frame)
            elapsed = frame_idx / fps
            print(json.dumps({
                'text': '\n'.join(lines),
                'preview': PREVIEW,
                'video_progress': frame_idx,
                'video_total': total_frames,
                'video_time': round(elapsed, 1)
            }), flush=True)
            processed += 1
            frame_idx += step_frames
        vcap.release()
        print(json.dumps({'video_done': True, 'frames': processed}), flush=True)

cap.release()
)PY").arg(previewPath);
}

/* ── startOcrDaemon — avvia il processo Python persistente ── */
void MultimediaPage::startOcrDaemon()
{
    if (m_ocrDaemon && m_ocrDaemon->state() != QProcess::NotRunning)
        return;

    const QString previewPath = QDir::tempPath() + "/prismalux_ocr_preview.jpg";
    m_ocrLineBuf.clear();
    m_ocrPending = false;

    m_ocrDaemon = new QProcess(this);
    m_ocrDaemon->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_ocrDaemon, &QProcess::readyReadStandardOutput,
            this, &MultimediaPage::onOcrDaemonReadyRead);
    connect(m_ocrDaemon,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MultimediaPage::onOcrDaemonFinished);
    connect(m_ocrDaemon, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[MultimediaPage] OCR daemon non avviato:" << m_ocrDaemon->program();
    });

    m_ocrStatus->setText(tr("\xe2\x8f\xb3  Avvio daemon OCR (import librerie)..."));
    m_ocrDaemon->start(OpencvUtils::findCvPython(),
                       QStringList{"-c", ocrDaemonScript(previewPath)});
    if (!m_ocrDaemon->waitForStarted(P::kProcessStartTimeoutMs)) {
        m_ocrStatus->setText(tr("\xe2\x9d\x8c  Python non trovato nel PATH."));
        LogBus::post("\xe2\x9d\x8c OCR webcam: Python non trovato nel PATH.");
        m_ocrDaemon->deleteLater();
        m_ocrDaemon = nullptr;
    }
}

/* ── stopOcrDaemon — chiude il processo Python persistente ── */
void MultimediaPage::stopOcrDaemon()
{
    if (!m_ocrDaemon) return;
    m_ocrDaemon->closeWriteChannel();
    m_ocrDaemon->kill();
    m_ocrDaemon->deleteLater();
    m_ocrDaemon = nullptr;
    m_ocrPending = false;
}

/* ── requestOcrCapture — invia "SCAN\n" al daemon già avviato ── */
void MultimediaPage::requestOcrCapture()
{
    if (!m_ocrDaemon || m_ocrDaemon->state() != QProcess::Running)
        return;
    if (m_ocrPending)
        return;  /* scansione precedente ancora in corso */
    m_ocrPending = true;
    m_ocrDaemon->write("SCAN\n");
}

void MultimediaPage::onOcrStartStopClicked(bool on)
{
    if (on) {
        m_ocrStartBtn->setText(tr("\xe2\x8f\xb9  Ferma"));  /* ⏹ */
        startOcrDaemon();
        m_ocrTimer->start(m_ocrInterval->value() * 1000);
        m_ocrPreviewTimer->start();
    } else {
        m_ocrTimer->stop();
        m_ocrPreviewTimer->stop();
        stopOcrDaemon();
        m_ocrStartBtn->setText(tr("\xe2\x96\xb6  Avvia webcam"));
        m_ocrStatus->setText(tr("Scansione fermata."));
    }
}

void MultimediaPage::onOcrTimerTick()
{
    requestOcrCapture();
}

void MultimediaPage::onOcrPreviewTick()
{
    /* Invia PREVIEW solo se il daemon è vivo e non sta già elaborando un SCAN/VIDEO.
       Il daemon risponderà con {"preview":path, "preview_only":true} in ~15ms. */
    if (!m_ocrDaemon || m_ocrDaemon->state() != QProcess::Running)
        return;
    if (m_ocrPending)
        return;
    m_ocrDaemon->write("PREVIEW\n");
}

void MultimediaPage::onOcrLoadVideoClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Seleziona video",
        QDir::homePath(),
        "Video (*.mp4 *.avi *.mov *.mkv *.webm *.ts *.m4v);;"
        "Tutti i file (*)");
    if (path.isEmpty()) return;

    m_ocrVideoPath = path;
    m_ocrVideoLbl->setText(QFileInfo(path).fileName());
    if (m_ocrTranscribeBtn) m_ocrTranscribeBtn->setEnabled(true);

    /* Avvia daemon se non è già in esecuzione, poi invia VIDEO */
    auto sendVideoCmd = [this]{
        const QString cmd = QString("VIDEO:%1:%2\n")
            .arg(m_ocrVideoPath)
            .arg(m_ocrVideoStep->value());
        m_ocrDaemon->write(cmd.toUtf8());
        m_ocrStatus->setText(
            "\xf0\x9f\x93\xb9  Analisi video in corso...");
    };

    if (!m_ocrDaemon || m_ocrDaemon->state() != QProcess::Running) {
        startOcrDaemon();
        /* aspetta il messaggio {"ready":true} — lo slot onOcrDaemonReadyRead
           rileverà video_path non vuoto e invierà il comando */
    } else {
        sendVideoCmd();
    }
    m_ocrPending = true;   /* blocca SCAN webcam durante analisi video */
}

/* ── onOcrDaemonReadyRead — legge le righe JSON emesse dal daemon ── */
void MultimediaPage::onOcrDaemonReadyRead()
{
    m_ocrLineBuf += m_ocrDaemon->readAllStandardOutput();
    while (true) {
        const int nl = m_ocrLineBuf.indexOf('\n');
        if (nl < 0) break;
        const QByteArray line = m_ocrLineBuf.left(nl).trimmed();
        m_ocrLineBuf = m_ocrLineBuf.mid(nl + 1);
        if (line.isEmpty()) continue;

        const QJsonObject obj = QJsonDocument::fromJson(line).object();

        /* Risposta rapida solo-preview: aggiorna l'immagine e continua */
        if (obj.value("preview_only").toBool()) {
            const QString pp = obj["preview"].toString();
            if (!pp.isEmpty()) {
                const QPixmap px(pp);
                if (!px.isNull())
                    m_ocrPreview->setPixmap(
                        px.scaled(m_ocrPreview->size(),
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation));
            }
            continue;
        }

        if (obj.contains("ready")) {
            /* Invia risoluzione selezionata (il daemon parte già a 720p,
               ma se l'utente ha cambiato il combo, allineiamo subito) */
            if (m_ocrResCambo) {
                const QString res = m_ocrResCambo->currentData().toString();
                if (res != "1280x720")
                    m_ocrDaemon->write(("SETRES:" + res + "\n").toUtf8());
            }
            /* Invia subito la soglia di matching */
            const bool skipOn = m_ocrChkSkipUnchanged && m_ocrChkSkipUnchanged->isChecked();
            const double thresh = skipOn && m_ocrThreshSpin
                                  ? m_ocrThreshSpin->value() : 2.0;
            m_ocrDaemon->write(
                QString("SETTHRESH:%1\n").arg(thresh, 0, 'f', 3).toUtf8());

            if (!m_ocrVideoPath.isEmpty()) {
                /* daemon pronto con video in coda — invia VIDEO */
                const QString cmd = QString("VIDEO:%1:%2\n")
                    .arg(m_ocrVideoPath)
                    .arg(m_ocrVideoStep->value());
                m_ocrDaemon->write(cmd.toUtf8());
                m_ocrStatus->setText(
                    "\xf0\x9f\x93\xb9  Analisi video in corso...");
            } else {
                m_ocrStatus->setText(
                    "\xf0\x9f\x94\x8d  Daemon pronto. Prima scansione...");
                requestOcrCapture();
            }
            continue;
        }

        if (obj.contains("video_done")) {
            const int frames = obj["video_done"].toInt();
            m_ocrStatus->setText(
                "\xe2\x9c\x85  Video completato — " +
                QString::number(obj["frames"].toInt()) + " frame elaborati.");
            m_ocrPending = false;
            m_ocrVideoPath.clear();
            m_ocrVideoLbl->setText(tr("Nessun video"));
            continue;
        }

        if (obj.contains("video_progress")) {
            const int cur   = obj["video_progress"].toInt();
            const int tot   = obj["video_total"].toInt();
            const double t  = obj["video_time"].toDouble();
            const int pct   = tot > 0 ? cur * 100 / tot : 0;
            m_ocrStatus->setText(
                QString("\xf0\x9f\x93\xb9  Video %1% — %2s")
                .arg(pct).arg(t, 0, 'f', 1));
            /* la logica testo viene gestita sotto con "text" */
        }

        m_ocrPending = false;

        /* Scena invariata: aggiorna solo anteprima, mostra similarità */
        if (obj.value("unchanged").toBool()) {
            const QString pp = obj["preview"].toString();
            if (!pp.isEmpty()) {
                const QPixmap px(pp);
                if (!px.isNull())
                    m_ocrPreview->setPixmap(
                        px.scaled(m_ocrPreview->size(),
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation));
            }
            m_ocrStatus->setText(
                QString("\xe2\x8f\xad  Scena invariata (sim. %1) — OCR saltato")  /* ⏭ */
                .arg(obj["similarity"].toDouble(), 0, 'f', 3));
            continue;
        }

        if (obj.contains("error")) {
            m_ocrStatus->setText("\xe2\x9d\x8c  " + obj["error"].toString());
            LogBus::post("\xe2\x9d\x8c OCR webcam: " + obj["error"].toString());
            continue;
        }

        /* Aggiorna anteprima webcam */
        const QString previewPath = obj["preview"].toString();
        if (!previewPath.isEmpty()) {
            QPixmap px(previewPath);
            if (!px.isNull())
                m_ocrPreview->setPixmap(
                    px.scaled(m_ocrPreview->size(),
                               Qt::KeepAspectRatio,
                               Qt::SmoothTransformation));
        }

        /* Applica filtri sulle righe */
        const QString rawText = obj["text"].toString().trimmed();
        QStringList inLines  = rawText.split('\n', Qt::SkipEmptyParts);
        QStringList outLines;

        const bool doDedup   = m_ocrChkDedup  && m_ocrChkDedup->isChecked();
        const bool doAlpha   = m_ocrChkAlpha  && m_ocrChkAlpha->isChecked();
        const bool doMinLen  = m_ocrChkMinLen && m_ocrChkMinLen->isChecked();

        for (const QString& raw : inLines) {
            const QString line = raw.trimmed();
            if (line.isEmpty()) continue;

            /* Filtro lunghezza minima */
            if (doMinLen && line.length() < 4) continue;

            /* Filtro alpha: almeno 50% dei caratteri deve essere lettera */
            if (doAlpha) {
                int letters = 0;
                for (const QChar& c : line)
                    if (c.isLetter()) ++letters;
                if (line.length() > 0 &&
                    letters * 100 / line.length() < 50) continue;
            }

            /* Filtro deduplicazione */
            if (doDedup) {
                const QString key = line.toLower().simplified();
                if (m_ocrSeenLines.contains(key)) continue;
                m_ocrSeenLines.insert(key);
            }

            outLines << line;
        }

        const QString text = outLines.join('\n');
        const int nChars = text.length();
        if (nChars >= 4) {
            if (!m_ocrText->toPlainText().isEmpty())
                m_ocrText->append(
                    "\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n");
            m_ocrText->append(text);
            m_ocrStatus->setText(
                "\xe2\x9c\x85  " + QString::number(outLines.size()) +
                " righe (" + QString::number(nChars) + " car.)");
        } else if (rawText.length() > 0) {
            m_ocrStatus->setText(
                "\xe2\x9a\xa0  Testo filtrato — avvicina il foglio.");
        } else {
            m_ocrStatus->setText(
                "\xf0\x9f\x94\x8d  Nessun testo — punta la webcam sul bugiardino.");
        }
    }
}

void MultimediaPage::onOcrDaemonFinished(int, QProcess::ExitStatus)
{
    if (m_ocrDaemon) {
        m_ocrDaemon->deleteLater();
        m_ocrDaemon = nullptr;
    }
    m_ocrPending = false;
    if (m_ocrStartBtn && m_ocrStartBtn->isChecked()) {
        m_ocrStartBtn->setChecked(false);
        m_ocrStatus->setText(tr("\xe2\x9a\xa0  Daemon OCR terminato inaspettatamente."));
    }
}

void MultimediaPage::onOcrTranscribeAudioClicked()
{
    if (m_ocrVideoPath.isEmpty()) return;

    /* Impedisce avvii multipli */
    if ((m_ocrFfmpegProc  && m_ocrFfmpegProc->state()  != QProcess::NotRunning) ||
        (m_ocrWhisperProc && m_ocrWhisperProc->state() != QProcess::NotRunning))
        return;

    m_ocrTranscribeBtn->setEnabled(false);
    m_ocrAudioWav = QDir::tempPath() + "/prismalux_ocr_audio.wav";
    m_ocrStatus->setText(tr("\xf0\x9f\x94\x84  Estrazione audio con ffmpeg..."));

    /* ffmpeg: estrae audio mono 16kHz PCM — formato nativo per Whisper */
    m_ocrFfmpegProc = new QProcess(this);
    connect(m_ocrFfmpegProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MultimediaPage::onOcrFfmpegFinished);
    connect(m_ocrFfmpegProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[MultimediaPage] ffmpeg OCR audio non avviato:" << m_ocrFfmpegProc->program();
    });
    m_ocrFfmpegProc->start(FfmpegUtils::findFfmpeg(),
        FfmpegUtils::extractArgs(m_ocrVideoPath, m_ocrAudioWav));
    if (!m_ocrFfmpegProc->waitForStarted(P::kProcessStartTimeoutMs)) {
        m_ocrStatus->setText(
            "\xe2\x9d\x8c  ffmpeg non trovato. Installa con: sudo apt install ffmpeg");
        LogBus::post("\xe2\x9d\x8c OCR webcam: ffmpeg non trovato nel PATH.");
        m_ocrFfmpegProc->deleteLater();
        m_ocrFfmpegProc = nullptr;
        m_ocrTranscribeBtn->setEnabled(true);
    }
}

void MultimediaPage::onOcrFfmpegFinished(int code, QProcess::ExitStatus)
{
    m_ocrFfmpegProc->deleteLater();
    m_ocrFfmpegProc = nullptr;

    if (code != 0) {
        m_ocrStatus->setText(
            "\xe2\x9d\x8c  Estrazione audio fallita — il video ha audio?");
        LogBus::post("\xe2\x9d\x8c OCR webcam: Estrazione audio fallita (ffmpeg).");
        m_ocrTranscribeBtn->setEnabled(true);
        return;
    }

    if (!SttWhisper::isAvailable()) {
        m_ocrStatus->setText(
            "\xe2\x9a\xa0  " + SttWhisper::setupMessage());
        m_ocrTranscribeBtn->setEnabled(true);
        return;
    }

    m_ocrStatus->setText(tr("\xf0\x9f\x8e\xa4  Trascrizione Whisper in corso..."));

    m_ocrWhisperProc = SttWhisper::transcribe(
        m_ocrAudioWav, "it", this,
        [this](const QString& text, bool ok) {
            m_ocrWhisperProc = nullptr;
            QFile::remove(m_ocrAudioWav);
            m_ocrTranscribeBtn->setEnabled(true);

            if (!ok || text.trimmed().isEmpty()) {
                m_ocrStatus->setText(
                    "\xe2\x9a\xa0  Trascrizione vuota — audio non udibile o lingua errata.");
                return;
            }

            /* Appende con separatore al testo OCR esistente */
            if (!m_ocrText->toPlainText().isEmpty())
                m_ocrText->append(
                    "\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    " \xf0\x9f\x8e\xa4 AUDIO \xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n");
            m_ocrText->append(text.trimmed());
            m_ocrStatus->setText(
                "\xe2\x9c\x85  Trascrizione completata — " +
                QString::number(text.trimmed().split('\n').size()) + " righe.");
        });

    if (!m_ocrWhisperProc) {
        m_ocrTranscribeBtn->setEnabled(true);
    }
}

void MultimediaPage::onOcrAnalyzeClicked()
{
    const QString testo = m_ocrText->toPlainText().trimmed();
    if (testo.isEmpty()) {
        m_ocrAiOut->setPlainText(
            "\xe2\x9d\x8c  Avvia prima la scansione per raccogliere del testo.");
        LogBus::post("\xe2\x9d\x8c OCR webcam: Avvia prima la scansione per raccogliere del testo.");
        return;
    }

    const QString sys =
        "Sei un assistente esperto nell'analisi di testi tecnici e manuali. "
        "Analizza il testo OCR fornito (potrebbe contenere errori di scansione). "
        "Fornisci: 1) Riassunto conciso, 2) Punti chiave, 3) Eventuali dati numerici "
        "o istruzioni importanti. Rispondi in italiano.";

    m_ocrAiOut->clear();
    m_ocrAiOut->setPlaceholderText(tr("\xe2\x8c\x9b  Analisi AI in corso..."));

    QObject::disconnect(m_ocrAiTokenConn);
    QObject::disconnect(m_ocrAiFinishedConn);
    QObject::disconnect(m_ocrAiErrorConn);
    m_ocrAiTokenConn    = connect(m_ai, &AiClient::token,
                                  this, &MultimediaPage::onOcrAiToken);
    m_ocrAiFinishedConn = connect(m_ai, &AiClient::finished,
                                  this, &MultimediaPage::onOcrAiFinished);
    m_ocrAiErrorConn    = connect(m_ai, &AiClient::error,
                                  this, &MultimediaPage::onOcrAiError);

    m_ai->chat(P::prependKnowledge(sys),
               "Testo OCR scansionato:\n\n" + testo);
}

void MultimediaPage::onOcrAiToken(const QString& t)
{
    m_ocrAiOut->moveCursor(QTextCursor::End);
    m_ocrAiOut->insertPlainText(t);
}

void MultimediaPage::onOcrAiFinished(const QString&)
{
    QObject::disconnect(m_ocrAiTokenConn);
    QObject::disconnect(m_ocrAiFinishedConn);
    QObject::disconnect(m_ocrAiErrorConn);
    m_ocrAiTokenConn = m_ocrAiFinishedConn = m_ocrAiErrorConn = {};
}

void MultimediaPage::onOcrAiError(const QString& msg)
{
    QObject::disconnect(m_ocrAiTokenConn);
    QObject::disconnect(m_ocrAiFinishedConn);
    QObject::disconnect(m_ocrAiErrorConn);
    m_ocrAiTokenConn = m_ocrAiFinishedConn = m_ocrAiErrorConn = {};
    m_ocrAiOut->setPlainText("\xe2\x9d\x8c  Errore AI: " + msg);
    LogBus::post("\xe2\x9d\x8c OCR webcam: Errore AI: " + msg);
}

/* ══════════════════════════════════════════════════════════════
   buildOsmMapTab — Mappa geografica OpenStreetMap + routing OSRM
   ══════════════════════════════════════════════════════════════ */

/* Decoder Google Encoded Polyline (usato da OSRM) */
static QVector<QPair<double,double>> decodePolyline(const QByteArray& enc)
{
    QVector<QPair<double,double>> pts;
    int index = 0, lat = 0, lon = 0;
    while (index < enc.size()) {
        auto decode = [&](int& val) {
            int b, result = 0, shift = 0;
            do { b = static_cast<unsigned char>(enc[index++]) - 63;
                 result |= (b & 0x1f) << shift; shift += 5; } while (b >= 0x20);
            val += (result & 1) ? ~(result >> 1) : (result >> 1);
        };
        decode(lat); decode(lon);
        pts.append({lat * 1e-5, lon * 1e-5});
    }
    return pts;
}

/* WMO weather code → testo italiano */
static QString wmoText(int code)
{
    if (code == 0)                         return "Sereno";
    if (code <= 2)                         return "Poco nuvoloso";
    if (code == 3)                         return "Coperto";
    if (code == 45 || code == 48)          return "Nebbia";
    if (code >= 51 && code <= 57)          return "Pioggerella";
    if (code >= 61 && code <= 65)          return "Pioggia";
    if (code == 66 || code == 67)          return "Pioggia gelata";
    if (code >= 71 && code <= 77)          return "Neve";
    if (code >= 80 && code <= 82)          return "Rovesci";
    if (code >= 85 && code <= 86)          return "Nevicate";
    if (code >= 95)                        return "Temporale";
    return "—";
}

/* Testo mostrato nella lista Tappe: etichetta + coordinate + quota, se
   già nota. Fattorizzato per restare identico tra aggiunta, ricostruzione
   della lista e rinomina manuale. */
static QString osmWpItemText(const QString& label, double lat, double lon,
                              bool hasElev, int elevM)
{
    QString s = QString("%1  %2, %3").arg(label).arg(lat, 0, 'f', 5).arg(lon, 0, 'f', 5);
    if (hasElev) s += QString("   \xe2\x9b\xb0 %1 m").arg(elevM);
    return s;
}

/* ── Quota di un singolo waypoint ──────────────────────────────────
   Interrogata subito dopo l'aggiunta di una tappa (non solo dopo
   "Calcola percorso"): così si vede l'altitudine di un possibile
   arrivo prima ancora di deciderlo — utile per pianificare lo sforzo
   in bicicletta senza motore. Il match sulla lista alla risposta
   avviene per coordinate (non per puntatore all'item, che nel
   frattempo potrebbe essere stato rimosso o ricostruito). */
void MultimediaPage::fetchOsmWaypointElevation(double lat, double lon)
{
    if (!m_osmNam) return;

    QNetworkRequest req(QUrl(
        QString("https://api.open-meteo.com/v1/elevation"
                "?latitude=%1&longitude=%2")
            .arg(lat, 0, 'f', 5).arg(lon, 0, 'f', 5)));
    req.setHeader(QNetworkRequest::UserAgentHeader, "Prismalux/3.0");

    QNetworkReply* reply = m_osmNam->get(req);
    connect(reply, &QNetworkReply::finished, reply, [this, reply, lat, lon] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError || !m_osmWpList) return;

        const QJsonArray arr = QJsonDocument::fromJson(reply->readAll())
                                    .object()["elevation"].toArray();
        if (arr.isEmpty()) return;
        const int elev = qRound(arr.first().toDouble());

        for (int i = 0; i < m_osmWpList->count(); ++i) {
            auto* it = m_osmWpList->item(i);
            const double ilat = it->data(Qt::UserRole).toDouble();
            const double ilon = it->data(Qt::UserRole + 1).toDouble();
            if (qAbs(ilat - lat) < 1e-4 && qAbs(ilon - lon) < 1e-4) {
                const QString lbl = it->data(Qt::UserRole + 2).toString();
                it->setData(Qt::UserRole + 3, elev);
                it->setText(osmWpItemText(lbl, lat, lon, true, elev));
                break;
            }
        }
    });
}

/* Elenca i percorsi salvati (percorsi_mappa/*.json) nel combo dedicato. */
void MultimediaPage::refreshOsmSavedRoutes(const QString& selectName)
{
    if (!m_osmSavedCombo) return;
    m_osmSavedCombo->clear();
    const QDir dir(P::root() + "/percorsi_mappa");
    const QFileInfoList files = dir.entryInfoList({"*.json"}, QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files)
        m_osmSavedCombo->addItem(fi.completeBaseName());
    if (!selectName.isEmpty()) {
        const int idx = m_osmSavedCombo->findText(selectName);
        if (idx >= 0) m_osmSavedCombo->setCurrentIndex(idx);
    }
}

QWidget* MultimediaPage::buildOsmMapTab()
{
    auto* w   = new QWidget;
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    /* ── Mappa principale ── */
    m_osmMap = new WorldMapWidget(w);
    m_osmMap->setRouteMode(true);
    m_osmMap->enableViewPersistence();   // riparte dall'ultima città visitata
    lay->addWidget(m_osmMap, 1);

    /* ── Pannello laterale destro (scrollabile) ── */
    auto* scroll = new QScrollArea(w);
    scroll->setFixedWidth(dpiScale(248));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* panel    = new QWidget;
    auto* panelLay = new QVBoxLayout(panel);
    panelLay->setContentsMargins(dpiScale(6), dpiScale(6), dpiScale(6), dpiScale(6));
    panelLay->setSpacing(dpiScale(8));

    auto* titleLbl = new QLabel(
        "\xf0\x9f\x97\xba  <b>Mappa OSM</b>", panel);
    titleLbl->setObjectName("formLabel");
    titleLbl->setTextFormat(Qt::RichText);
    panelLay->addWidget(titleLbl);

    auto* hintLbl = new QLabel(
        "<small>"
        "<b>Click sinistro</b> = aggiungi tappa.<br>"
        "<b>Click destro</b> = menu (Partenza / Tappa).<br>"
        "Cerca citt\xc3\xa0 → scegli Partenza o Tappa.<br>"
        "<b>Doppio clic</b> su una tappa = rinominala.<br>"
        "Ogni tappa mostra subito la sua \xe2\x9b\xb0 quota, "
        "prima ancora di calcolare il percorso."
        "</small>", panel);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setTextFormat(Qt::RichText);
    hintLbl->setWordWrap(true);
    panelLay->addWidget(hintLbl);

    /* ── Waypoint list ── */
    auto* wpGroup = new QGroupBox(
        "\xf0\x9f\x93\x8d  Tappe  (A = partenza, ultima = arrivo)", panel);
    auto* wpLay = new QVBoxLayout(wpGroup);
    wpLay->setSpacing(dpiScale(4));

    m_osmWpList = new QListWidget(wpGroup);
    m_osmWpList->setFixedHeight(dpiScale(100));
    m_osmWpList->setToolTip(tr("Click destro sulla mappa per impostare la partenza"));
    wpLay->addWidget(m_osmWpList);

    auto* wpBtnRow = new QHBoxLayout;
    auto* btnRemWp = new QPushButton("\xe2\x9c\x96  Rimuovi", wpGroup);
    btnRemWp->setObjectName("actionBtn");
    btnRemWp->setToolTip(tr("Rimuovi la tappa selezionata"));
    auto* btnClrWp = new QPushButton("\xf0\x9f\x97\x91  Svuota", wpGroup);
    btnClrWp->setObjectName("actionBtn");
    btnClrWp->setToolTip(tr("Rimuovi tutte le tappe e il percorso"));
    wpBtnRow->addWidget(btnRemWp, 1);
    wpBtnRow->addWidget(btnClrWp, 1);
    wpLay->addLayout(wpBtnRow);

    auto* numLabelChk = new QCheckBox(
        tr("\xf0\x9f\x94\xa2 Numeri (1,2,3..) invece di A,B,C.."), wpGroup);
    numLabelChk->setChecked(m_osmMap->numericLabelStyle());
    numLabelChk->setToolTip(tr(
        "Stile etichette per le tappe non rinominate a mano.\n"
        "Con A,B,C.. dopo la Z si continua con A1,B1,..Z1,A2.. invece di\n"
        "ripartire da A."));
    connect(numLabelChk, &QCheckBox::toggled, w, [this](bool on) {
        if (m_osmMap) m_osmMap->setLabelStyle(on);
    });
    wpLay->addWidget(numLabelChk);
    panelLay->addWidget(wpGroup);

    /* ── Routing ── */
    auto* rtGroup = new QGroupBox("\xf0\x9f\x9a\x97  Percorso", panel);
    auto* rtLay = new QVBoxLayout(rtGroup);
    rtLay->setSpacing(dpiScale(4));

    auto* profRow = new QHBoxLayout;
    auto* profLbl = new QLabel("Modalit\xc3\xa0:", rtGroup);
    profLbl->setFixedWidth(dpiScale(60));
    m_osmProfileCmb = new QComboBox(rtGroup);
    m_osmProfileCmb->setObjectName("settingCombo");
    m_osmProfileCmb->addItem("\xf0\x9f\x9a\x97  Auto",                  "driving");
    m_osmProfileCmb->addItem("\xf0\x9f\x9a\xb6  Piedi / Trekking",      "foot");
    m_osmProfileCmb->addItem("\xf0\x9f\x9a\xb5  MTB / Alpinismo",       "foot");
    m_osmProfileCmb->addItem("\xf0\x9f\x9a\xb2  Bicicletta (strada)",   "bike");
    profRow->addWidget(profLbl);
    profRow->addWidget(m_osmProfileCmb, 1);
    rtLay->addLayout(profRow);

    auto* btnCalc = new QPushButton(
        "\xf0\x9f\x94\x8d  Calcola percorso", rtGroup);
    btnCalc->setObjectName("primaryBtn");
    btnCalc->setToolTip(tr("Calcola percorso via OSRM (richiede internet)"));
    rtLay->addWidget(btnCalc);

    m_osmRouteInfo = new QLabel("Distanza: \xe2\x80\x94  |  Tempo: \xe2\x80\x94", rtGroup);
    m_osmRouteInfo->setObjectName("cardDesc");
    m_osmRouteInfo->setWordWrap(true);
    rtLay->addWidget(m_osmRouteInfo);

    auto* btnClrRoute = new QPushButton("\xf0\x9f\x97\x91  Cancella percorso", rtGroup);
    btnClrRoute->setObjectName("actionBtn");
    rtLay->addWidget(btnClrRoute);

    panelLay->addWidget(rtGroup);

    /* ── Percorsi salvati ── */
    auto* savedGroup = new QGroupBox("\xf0\x9f\x92\xbe  Percorsi salvati", panel);
    auto* savedLay = new QVBoxLayout(savedGroup);
    savedLay->setSpacing(dpiScale(4));

    m_osmSavedCombo = new QComboBox(savedGroup);
    m_osmSavedCombo->setObjectName("settingCombo");
    savedLay->addWidget(m_osmSavedCombo);

    auto* savedBtnRow = new QHBoxLayout;
    auto* btnSaveRoute = new QPushButton("\xf0\x9f\x92\xbe  Salva", savedGroup);
    btnSaveRoute->setObjectName("actionBtn");
    btnSaveRoute->setToolTip(tr("Salva tappe e percorso disegnato con un nome"));
    auto* btnLoadRoute = new QPushButton("\xf0\x9f\x93\x82  Carica", savedGroup);
    btnLoadRoute->setObjectName("actionBtn");
    auto* btnDelRoute = new QPushButton("\xf0\x9f\x97\x91", savedGroup);
    btnDelRoute->setObjectName("actionBtn");
    btnDelRoute->setToolTip(tr("Elimina il percorso selezionato"));
    savedBtnRow->addWidget(btnSaveRoute, 1);
    savedBtnRow->addWidget(btnLoadRoute, 1);
    savedBtnRow->addWidget(btnDelRoute);
    savedLay->addLayout(savedBtnRow);
    panelLay->addWidget(savedGroup);

    /* ── Altimetria (ciclisti / alpini) ── */
    auto* elevGroup = new QGroupBox(
        "\xe2\x9b\xb0  Altimetria", panel);
    auto* elevLay = new QVBoxLayout(elevGroup);
    elevLay->setSpacing(dpiScale(3));

    m_osmElevLbl = new QLabel(
        "<small>Calcolata automaticamente dopo il percorso.</small>", elevGroup);
    m_osmElevLbl->setObjectName("hintLabel");
    m_osmElevLbl->setWordWrap(true);
    m_osmElevLbl->setTextFormat(Qt::RichText);
    elevLay->addWidget(m_osmElevLbl);
    panelLay->addWidget(elevGroup);

    /* ── Meteo Partenza ── */
    auto* wxGroup = new QGroupBox(
        "\xf0\x9f\x8c\xa6  Meteo partenza", panel);
    auto* wxLay = new QVBoxLayout(wxGroup);
    wxLay->setSpacing(dpiScale(3));

    m_osmWeatherBtn = new QPushButton(
        "\xf0\x9f\x8c\xa4  Controlla meteo", wxGroup);
    m_osmWeatherBtn->setObjectName("actionBtn");
    m_osmWeatherBtn->setToolTip(
        "Recupera le condizioni meteo attuali della partenza (open-meteo.com)");
    wxLay->addWidget(m_osmWeatherBtn);

    m_osmWeatherLbl = new QLabel(
        "<small>Premi il pulsante dopo aver impostato la partenza.</small>",
        wxGroup);
    m_osmWeatherLbl->setObjectName("hintLabel");
    m_osmWeatherLbl->setWordWrap(true);
    m_osmWeatherLbl->setTextFormat(Qt::RichText);
    wxLay->addWidget(m_osmWeatherLbl);
    panelLay->addWidget(wxGroup);

    /* ── Mappe offline ── */
    auto* dlGroup = new QGroupBox(
        "\xf0\x9f\x93\xb4  Mappe offline", panel);
    auto* dlLay = new QVBoxLayout(dlGroup);
    dlLay->setSpacing(dpiScale(3));

    auto* btnDl = new QPushButton(
        "\xe2\xac\x87  Scarica area visibile", dlGroup);
    btnDl->setObjectName("actionBtn");
    btnDl->setToolTip(
        "Scarica i tile OSM visibili nella cache locale (~/.cache/Prismalux/osm_tiles/)");
    dlLay->addWidget(btnDl);

    m_osmDlLbl = new QLabel(
        "<small>Tile salvati: non scaricati.</small>", dlGroup);
    m_osmDlLbl->setObjectName("hintLabel");
    m_osmDlLbl->setWordWrap(true);
    m_osmDlLbl->setTextFormat(Qt::RichText);
    dlLay->addWidget(m_osmDlLbl);
    panelLay->addWidget(dlGroup);

    panelLay->addStretch(1);

    /* Nota copyright */
    auto* credLbl = new QLabel(
        "<small>\xa9 "
        "<a href='https://www.openstreetmap.org/copyright'>OpenStreetMap</a>"
        " contributors | "
        "<a href='https://project-osrm.org/'>OSRM</a> | "
        "<a href='https://open-meteo.com/'>Open-Meteo</a></small>",
        panel);
    credLbl->setTextFormat(Qt::RichText);
    credLbl->setOpenExternalLinks(true);
    credLbl->setObjectName("hintLabel");
    credLbl->setWordWrap(true);
    panelLay->addWidget(credLbl);

    scroll->setWidget(panel);
    lay->addWidget(scroll);

    /* ── NAM per OSRM / Meteo / Altimetria ── */
    m_osmNam = new QNetworkAccessManager(w);

    /* ── Connessioni ── */

    /* Waypoint aggiunto dalla mappa → aggiorna lista (l'etichetta è quella
       già assegnata da WorldMapWidget — rispetta stile numerico/rinomine) */
    connect(m_osmMap, &WorldMapWidget::waypointAdded,
            w, [this](int idx, double lat, double lon) {
        const QString lbl = m_osmMap->waypointLabels().value(idx, QString::number(idx + 1));
        auto* item = new QListWidgetItem(osmWpItemText(lbl, lat, lon, false, 0));
        item->setData(Qt::UserRole,     lat);
        item->setData(Qt::UserRole + 1, lon);
        item->setData(Qt::UserRole + 2, lbl);
        if (m_osmWpList) m_osmWpList->addItem(item);
        fetchOsmWaypointElevation(lat, lon);
    });

    /* insertStartWaypoint / cambio stile etichette / caricamento percorso
       → ricostruisce lista completa */
    connect(m_osmMap, &WorldMapWidget::waypointsReset,
            w, [this] {
        if (!m_osmWpList || !m_osmMap) return;
        m_osmWpList->clear();
        const auto& coords = m_osmMap->waypoints();
        const auto& labels = m_osmMap->waypointLabels();
        for (int i = 0; i < coords.size(); ++i) {
            const QString lbl = labels.value(i, "?");
            auto* item = new QListWidgetItem(
                osmWpItemText(lbl, coords[i].first, coords[i].second, false, 0));
            item->setData(Qt::UserRole,     coords[i].first);
            item->setData(Qt::UserRole + 1, coords[i].second);
            item->setData(Qt::UserRole + 2, lbl);
            m_osmWpList->addItem(item);
            fetchOsmWaypointElevation(coords[i].first, coords[i].second);
        }
        if (m_osmRouteInfo)
            m_osmRouteInfo->setText(tr("Distanza: \xe2\x80\x94  |  Tempo: \xe2\x80\x94"));
    });

    /* Rinomina una tappa (doppio clic): il nome scelto resta fisso anche
       se in seguito si imposta una nuova partenza. */
    connect(m_osmWpList, &QListWidget::itemDoubleClicked, w, [this](QListWidgetItem* item) {
        if (!item || !m_osmMap) return;
        const int row = m_osmWpList->row(item);
        bool ok = false;
        const QString newLabel = QInputDialog::getText(
            m_osmWpList, tr("Rinomina tappa"), tr("Nome:"),
            QLineEdit::Normal, item->data(Qt::UserRole + 2).toString(), &ok).trimmed();
        if (!ok || newLabel.isEmpty() || !m_osmMap->renameWaypoint(row, newLabel)) return;

        const double lat = item->data(Qt::UserRole).toDouble();
        const double lon = item->data(Qt::UserRole + 1).toDouble();
        const QVariant elevData = item->data(Qt::UserRole + 3);
        item->setData(Qt::UserRole + 2, newLabel);
        item->setText(osmWpItemText(newLabel, lat, lon, elevData.isValid(), elevData.toInt()));
    });

    /* Rimuovi waypoint selezionato — le etichette dei restanti (incluse
       eventuali rinomine) sono preservate passandole esplicitamente. */
    connect(btnRemWp, &QPushButton::clicked, w, [this] {
        if (!m_osmWpList) return;
        const int row = m_osmWpList->currentRow();
        if (row < 0) return;
        m_osmWpList->takeItem(row);
        m_osmMap->clearRoute();
        for (int i = 0; i < m_osmWpList->count(); ++i) {
            auto* it = m_osmWpList->item(i);
            m_osmMap->addWaypoint(it->data(Qt::UserRole).toDouble(),
                                  it->data(Qt::UserRole + 1).toDouble(),
                                  it->data(Qt::UserRole + 2).toString());
        }
        if (m_osmRouteInfo)
            m_osmRouteInfo->setText(tr("Distanza: \xe2\x80\x94  |  Tempo: \xe2\x80\x94"));
    });

    /* Svuota tutto */
    connect(btnClrWp, &QPushButton::clicked, w, [this] {
        if (m_osmMap)    m_osmMap->clearRoute();
        if (m_osmWpList) m_osmWpList->clear();
        if (m_osmRouteInfo)
            m_osmRouteInfo->setText(tr("Distanza: \xe2\x80\x94  |  Tempo: \xe2\x80\x94"));
        if (m_osmElevLbl)
            m_osmElevLbl->setText(
                "<small>Calcolata automaticamente dopo il percorso.</small>");
    });

    /* Cancella solo il percorso disegnato */
    connect(btnClrRoute, &QPushButton::clicked, w, [this] {
        if (m_osmMap) m_osmMap->setRouteLine({});
        if (m_osmRouteInfo)
            m_osmRouteInfo->setText(tr("Distanza: \xe2\x80\x94  |  Tempo: \xe2\x80\x94"));
        if (m_osmElevLbl)
            m_osmElevLbl->setText(
                "<small>Calcolata automaticamente dopo il percorso.</small>");
    });

    /* Calcola percorso OSRM + altimetria automatica */
    connect(btnCalc, &QPushButton::clicked, w, [this, btnCalc] {
        if (!m_osmWpList || m_osmWpList->count() < 2) {
            if (m_osmRouteInfo)
                m_osmRouteInfo->setText(
                    "\xe2\x9a\xa0  Aggiungi almeno 2 tappe (A = partenza).");
            return;
        }

        QStringList coords;
        for (int i = 0; i < m_osmWpList->count(); ++i) {
            auto* it = m_osmWpList->item(i);
            coords << QString("%1,%2")
                          .arg(it->data(Qt::UserRole + 1).toDouble(), 0, 'f', 6)
                          .arg(it->data(Qt::UserRole).toDouble(),     0, 'f', 6);
        }

        const QString profile = m_osmProfileCmb
            ? m_osmProfileCmb->currentData().toString()
            : "driving";

        const QUrl url(
            QString("https://router.project-osrm.org/route/v1/%1/%2"
                    "?overview=full&geometries=polyline")
                .arg(profile, coords.join(";")));

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      "Prismalux/3.0 (educational desktop app)");

        btnCalc->setEnabled(false);
        if (m_osmRouteInfo)
            m_osmRouteInfo->setText(tr("\xf0\x9f\x94\x84  Calcolo in corso..."));
        if (m_osmElevLbl)
            m_osmElevLbl->setText("<small>Calcolo altimetria...</small>");

        QNetworkReply* reply = m_osmNam->get(req);
        connect(reply, &QNetworkReply::finished, reply,
                [this, reply, btnCalc] {
            reply->deleteLater();
            if (btnCalc) btnCalc->setEnabled(true);

            if (reply->error() != QNetworkReply::NoError) {
                if (m_osmRouteInfo)
                    m_osmRouteInfo->setText(
                        "\xe2\x9d\x8c  Errore rete: " + reply->errorString());
                LogBus::post("\xe2\x9d\x8c Mappa OSM: Errore rete: " + reply->errorString());
                return;
            }

            const QJsonObject root = QJsonDocument::fromJson(
                reply->readAll()).object();

            if (root["code"].toString() != "Ok") {
                if (m_osmRouteInfo)
                    m_osmRouteInfo->setText(
                        "\xe2\x9d\x8c  OSRM: " + root["message"].toString());
                LogBus::post("\xe2\x9d\x8c Mappa OSM: OSRM: " + root["message"].toString());
                return;
            }

            const QJsonObject route = root["routes"].toArray().first().toObject();
            const double dist_km    = route["distance"].toDouble() / 1000.0;
            const double dur_min    = route["duration"].toDouble() / 60.0;
            const QString geom      = route["geometry"].toString();

            const auto pts = decodePolyline(geom.toUtf8());
            if (m_osmMap) m_osmMap->setRouteLine(pts);

            if (m_osmRouteInfo)
                m_osmRouteInfo->setText(
                    QString("\xf0\x9f\x93\x8f  %1 km  |  \xe2\x8f\xb1  %2 min")
                        .arg(dist_km, 0, 'f', 1).arg(qRound(dur_min)));

            /* ── Altimetria: campiona 20 punti dal percorso ── */
            if (!pts.isEmpty() && m_osmNam) {
                const int N    = qMin(20, pts.size());
                const int step = qMax(1, pts.size() / N);
                QStringList lats, lons;
                for (int i = 0; i < pts.size(); i += step) {
                    lats << QString::number(pts[i].first,  'f', 5);
                    lons << QString::number(pts[i].second, 'f', 5);
                }

                QNetworkRequest elReq(QUrl(
                    QString("https://api.open-meteo.com/v1/elevation"
                            "?latitude=%1&longitude=%2")
                        .arg(lats.join(","), lons.join(","))));
                elReq.setHeader(QNetworkRequest::UserAgentHeader,
                                "Prismalux/3.0");

                QNetworkReply* elRep = m_osmNam->get(elReq);
                connect(elRep, &QNetworkReply::finished, elRep,
                        [this, elRep] {
                    elRep->deleteLater();
                    if (elRep->error() != QNetworkReply::NoError) {
                        if (m_osmElevLbl)
                            m_osmElevLbl->setText(
                                "<small>Altimetria non disponibile.</small>");
                        return;
                    }
                    const QJsonArray elArr =
                        QJsonDocument::fromJson(elRep->readAll())
                            .object()["elevation"].toArray();
                    if (elArr.isEmpty()) return;

                    double minEl =  1e9, maxEl = -1e9;
                    double ascent = 0.0, descent = 0.0;
                    double prev = elArr[0].toDouble();
                    for (const auto& v : elArr) {
                        const double e = v.toDouble();
                        minEl = qMin(minEl, e);
                        maxEl = qMax(maxEl, e);
                        if (e > prev) ascent  += (e - prev);
                        else          descent += (prev - e);
                        prev = e;
                    }
                    if (m_osmElevLbl)
                        m_osmElevLbl->setText(
                            QString("<small>"
                                    "<b>Min:</b> %1 m | <b>Max:</b> %2 m<br>"
                                    "<b>Salita:</b> +%3 m | <b>Discesa:</b> -%4 m"
                                    "</small>")
                                .arg(qRound(minEl))
                                .arg(qRound(maxEl))
                                .arg(qRound(ascent))
                                .arg(qRound(descent)));
                });
            }
        });
    });

    /* Meteo partenza (open-meteo, gratuito) */
    connect(m_osmWeatherBtn, &QPushButton::clicked, w, [this] {
        if (!m_osmWpList || m_osmWpList->count() == 0 || !m_osmNam) {
            if (m_osmWeatherLbl)
                m_osmWeatherLbl->setText(
                    "<small>Aggiungi prima la tappa di partenza (A).</small>");
            return;
        }
        const double startLat = m_osmWpList->item(0)->data(Qt::UserRole).toDouble();
        const double startLon = m_osmWpList->item(0)->data(Qt::UserRole + 1).toDouble();

        if (m_osmWeatherLbl) m_osmWeatherLbl->setText("<small>Caricamento...</small>");
        if (m_osmWeatherBtn) m_osmWeatherBtn->setEnabled(false);

        QNetworkRequest req(QUrl(
            QString("https://api.open-meteo.com/v1/forecast"
                    "?latitude=%1&longitude=%2"
                    "&current=temperature_2m,windspeed_10m,precipitation,weathercode"
                    "&timezone=auto")
                .arg(startLat, 0, 'f', 4)
                .arg(startLon, 0, 'f', 4)));
        req.setHeader(QNetworkRequest::UserAgentHeader, "Prismalux/3.0");

        QNetworkReply* rep = m_osmNam->get(req);
        connect(rep, &QNetworkReply::finished, rep,
                [this, rep] {
            rep->deleteLater();
            if (m_osmWeatherBtn) m_osmWeatherBtn->setEnabled(true);

            if (rep->error() != QNetworkReply::NoError) {
                if (m_osmWeatherLbl)
                    m_osmWeatherLbl->setText(
                        "<small>Errore meteo: " + rep->errorString() + "</small>");
                return;
            }
            const QJsonObject cur =
                QJsonDocument::fromJson(rep->readAll())
                    .object()["current"].toObject();

            const double temp  = cur["temperature_2m"].toDouble();
            const double wind  = cur["windspeed_10m"].toDouble();
            const double prec  = cur["precipitation"].toDouble();
            const int    code  = cur["weathercode"].toInt();

            if (m_osmWeatherLbl)
                m_osmWeatherLbl->setText(
                    QString("<small>"
                            "<b>%1</b><br>"
                            "Temp: %2\xc2\xb0" "C | Vento: %3 km/h<br>"
                            "Precipit.: %4 mm"
                            "</small>")
                        .arg(wmoText(code))
                        .arg(temp, 0, 'f', 1)
                        .arg(qRound(wind))
                        .arg(prec, 0, 'f', 1));
        });
    });

    /* Download mappe offline */
    connect(btnDl, &QPushButton::clicked, w, [this] {
        if (!m_osmMap || !m_osmDlLbl) return;
        if (m_osmDlLbl)
            m_osmDlLbl->setText("<small>Avvio download...</small>");
        m_osmMap->downloadVisibleTiles();
    });

    /* Progresso download tile */
    connect(m_osmMap, &WorldMapWidget::tileDownloadProgress,
            w, [this](int done, int total) {
        if (!m_osmDlLbl) return;
        if (total == 0) {
            m_osmDlLbl->setText(
                "<small>Area gi\xc3\xa0 in cache. Nessun download necessario.</small>");
        } else if (done >= total) {
            m_osmDlLbl->setText(
                QString("<small>Scaricati %1 tile. Mappa disponibile offline.</small>")
                    .arg(total));
        } else {
            m_osmDlLbl->setText(
                QString("<small>Download: %1 / %2 tile...</small>")
                    .arg(done).arg(total));
        }
    });

    /* Salva tappe + percorso disegnato con un nome, per riprenderlo in
       futuro senza dover ricliccare tutte le tappe. */
    connect(btnSaveRoute, &QPushButton::clicked, w, [this] {
        if (!m_osmMap || !m_osmWpList || m_osmWpList->count() == 0) return;
        bool ok = false;
        QString name = QInputDialog::getText(
            m_osmWpList, tr("Salva percorso"), tr("Nome:"),
            QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        name.replace(QRegularExpression("[/\\\\:*?\"<>|]"), "_");

        const QDir dir(P::root() + "/percorsi_mappa");
        QDir().mkpath(dir.absolutePath());

        QJsonArray wpArr;
        for (int i = 0; i < m_osmWpList->count(); ++i) {
            auto* it = m_osmWpList->item(i);
            QJsonObject o;
            o["lat"]   = it->data(Qt::UserRole).toDouble();
            o["lon"]   = it->data(Qt::UserRole + 1).toDouble();
            o["label"] = it->data(Qt::UserRole + 2).toString();
            wpArr.append(o);
        }

        QJsonObject root;
        root["profile"]   = m_osmProfileCmb ? m_osmProfileCmb->currentData().toString() : "driving";
        root["waypoints"] = wpArr;

        QFile f(dir.absoluteFilePath(name + ".json"));
        if (f.open(QIODevice::WriteOnly)) {
            f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
            f.close();
            refreshOsmSavedRoutes(name);
            LogBus::post("\xf0\x9f\x92\xbe Mappa OSM: percorso '" + name + "' salvato.");
        }
    });

    /* Carica un percorso salvato: ricrea le tappe (che a loro volta
       ridisegnano marker e rilanciano la quota per ognuna). */
    connect(btnLoadRoute, &QPushButton::clicked, w, [this] {
        if (!m_osmMap || !m_osmSavedCombo || m_osmSavedCombo->currentText().isEmpty()) return;
        const QString name = m_osmSavedCombo->currentText();
        QFile f(P::root() + "/percorsi_mappa/" + name + ".json");
        if (!f.open(QIODevice::ReadOnly)) return;
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        f.close();

        if (m_osmWpList) m_osmWpList->clear();
        m_osmMap->clearRoute();
        m_osmMap->setRouteLine({});
        for (const QJsonValue& v : root["waypoints"].toArray()) {
            const QJsonObject o = v.toObject();
            const double lat = o["lat"].toDouble();
            const double lon = o["lon"].toDouble();
            m_osmMap->addWaypoint(lat, lon, o["label"].toString());
            emit m_osmMap->waypointAdded(m_osmMap->waypoints().size() - 1, lat, lon);
        }
        if (m_osmProfileCmb) {
            const int idx = m_osmProfileCmb->findData(root["profile"].toString());
            if (idx >= 0) m_osmProfileCmb->setCurrentIndex(idx);
        }
        if (m_osmRouteInfo)
            m_osmRouteInfo->setText(tr(
                "Percorso caricato — premi \xf0\x9f\x94\x8d Calcola percorso "
                "per distanza/tempo aggiornati."));
        LogBus::post("\xf0\x9f\x93\x82 Mappa OSM: percorso '" + name + "' caricato.");
    });

    /* Elimina il percorso salvato selezionato. */
    connect(btnDelRoute, &QPushButton::clicked, w, [this] {
        if (!m_osmSavedCombo || m_osmSavedCombo->currentText().isEmpty()) return;
        const QString name = m_osmSavedCombo->currentText();
        QFile::remove(P::root() + "/percorsi_mappa/" + name + ".json");
        refreshOsmSavedRoutes();
    });

    refreshOsmSavedRoutes();

    return w;
}

/* ══════════════════════════════════════════════════════════════════════════
   buildVideoCaptionTab — Analisi video con frame hashing + VLM captioning
   Pipeline: ffmpeg campiona frame → dhash filtra simili → Ollama VLM descrive
   ══════════════════════════════════════════════════════════════════════════ */
QWidget* MultimediaPage::buildVideoCaptionTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setSpacing(8);
    lay->setContentsMargins(10, 10, 10, 10);

    // ── Riga file ──
    auto* fileRow = new QHBoxLayout;
    m_vcPathEdit = new QLineEdit;
    m_vcPathEdit->setPlaceholderText(tr("Percorso video (MP4, MKV, AVI, MOV...)"));
    auto* browseBtn = new QPushButton(tr("\xf0\x9f\x93\x82 Sfoglia")); /* 📂 */
    browseBtn->setFixedWidth(dpiScale(90));
    fileRow->addWidget(new QLabel(tr("File:")));
    fileRow->addWidget(m_vcPathEdit, 1);
    fileRow->addWidget(browseBtn);
    lay->addLayout(fileRow);

    // ── Parametri ──
    auto* paramRow = new QHBoxLayout;
    paramRow->setSpacing(12);

    paramRow->addWidget(new QLabel(tr("Modello VLM:")));
    m_vcModelCombo = new QComboBox;
    m_vcModelCombo->setEditable(true);
    m_vcModelCombo->addItems({"llava:7b", "llava:13b", "moondream:latest",
                              "qwen2-vl:7b", "minicpm-v:8b"});
    m_vcModelCombo->setMinimumWidth(dpiScale(140));
    paramRow->addWidget(m_vcModelCombo);

    paramRow->addSpacing(8);
    paramRow->addWidget(new QLabel(tr("Ogni (s):")));
    m_vcIntervalSpin = new QSpinBox;
    m_vcIntervalSpin->setRange(1, 60);
    m_vcIntervalSpin->setValue(5);
    m_vcIntervalSpin->setToolTip(tr("Estrai un frame ogni N secondi"));
    m_vcIntervalSpin->setFixedWidth(dpiScale(60));
    paramRow->addWidget(m_vcIntervalSpin);

    paramRow->addSpacing(8);
    paramRow->addWidget(new QLabel(tr("Novit\xc3\xa0 (0-64):")));
    m_vcThreshSpin = new QSpinBox;
    m_vcThreshSpin->setRange(0, 64);
    m_vcThreshSpin->setValue(10);
    m_vcThreshSpin->setToolTip(
        tr("Distanza Hamming minima per considerare un frame 'nuovo'.\n"
           "0 = analizza tutti, 64 = solo frame radicalmente diversi"));
    m_vcThreshSpin->setFixedWidth(dpiScale(60));
    paramRow->addWidget(m_vcThreshSpin);

    paramRow->addSpacing(8);
    paramRow->addWidget(new QLabel(tr("Max frame:")));
    m_vcMaxFrames = new QSpinBox;
    m_vcMaxFrames->setRange(1, 200);
    m_vcMaxFrames->setValue(40);
    m_vcMaxFrames->setFixedWidth(dpiScale(60));
    paramRow->addWidget(m_vcMaxFrames);

    paramRow->addStretch();
    lay->addLayout(paramRow);

    // ── Pulsante start/stop ──
    auto* btnRow = new QHBoxLayout;
    m_vcStartBtn = new QPushButton(tr("\xf0\x9f\x9a\x80 Avvia analisi")); /* 🚀 */
    m_vcStartBtn->setFixedHeight(dpiScale(34));
    auto* clearBtn = new QPushButton(tr("\xf0\x9f\x97\x91 Pulisci"));      /* 🗑 */
    clearBtn->setFixedWidth(dpiScale(80));
    m_vcStatus = new QLabel(tr("Pronto."));
    m_vcStatus->setStyleSheet("color:#94a3b8;font-size:11px;");
    btnRow->addWidget(m_vcStartBtn);
    btnRow->addWidget(clearBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_vcStatus);
    lay->addLayout(btnRow);

    // ── Area risultati ──
    m_vcResults = new QTextBrowser;
    m_vcResults->setOpenLinks(false);
    m_vcResults->setPlaceholderText(
        tr("I risultati appariranno qui.\n\n"
           "Pipeline:\n"
           "  1. ffmpeg estrae frame ogni N secondi\n"
           "  2. dhash filtra i frame troppo simili al precedente\n"
           "  3. Il VLM descrive solo i frame 'nuovi'\n\n"
           "Dipendenze: ffmpeg (PATH) + Ollama con un modello vision (llava, moondream...)"));
    lay->addWidget(m_vcResults, 1);

    // ── Pannello dipendenze video captioning ──
    {
        using Dep = DepCheckPanel::Dep;
        const QList<Dep> vcDeps = {
            { "ffmpeg",    "",          "",          "ffmpeg",   "Estrazione frame dal video (richiesto)" },
            { "Pillow",    "PIL",       "Pillow",    "",         "Hashing frame pi\xc3\xb9 preciso (dhash)" },
            { "yt-dlp",    "yt_dlp",    "yt-dlp",   "",         "Download stream live YouTube/Twitch" },
            { "webrtcvad", "webrtcvad", "webrtcvad", "",         "VAD audio (filtra silenzio in STT)" },
        };
        auto* depPanel = new DepCheckPanel(vcDeps, w);
        lay->addWidget(depPanel);
        QTimer::singleShot(300, depPanel, &DepCheckPanel::runAllChecks);
    }

    // ── Connessioni ──
    connect(browseBtn, &QPushButton::clicked, this, &MultimediaPage::onVcBrowseClicked);
    connect(m_vcStartBtn, &QPushButton::clicked, this, &MultimediaPage::onVcStartStopClicked);
    connect(clearBtn, &QPushButton::clicked, m_vcResults, &QTextBrowser::clear);

    return w;
}

void MultimediaPage::onVcBrowseClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Seleziona video"),
        QDir::homePath(),
        tr("Video (*.mp4 *.mkv *.avi *.mov *.webm *.flv *.ts *.m4v);;Tutti (*.*)"));
    if (!path.isEmpty())
        m_vcPathEdit->setText(path);
}

void MultimediaPage::onVcStartStopClicked()
{
    if (m_vcProc && m_vcProc->state() != QProcess::NotRunning) {
        /* Stop */
        m_vcProc->terminate();
        m_vcProc->waitForFinished(2000);
        m_vcStartBtn->setText(tr("\xf0\x9f\x9a\x80 Avvia analisi"));
        m_vcStatus->setText(tr("Interrotto."));
        return;
    }

    const QString videoPath = m_vcPathEdit->text().trimmed();
    if (videoPath.isEmpty()) {
        m_vcResults->append(
            "<p style='color:#f87171;'>\xe2\x9d\x8c Seleziona prima un file video.</p>");
        return;
    }
    if (!QFileInfo::exists(videoPath)) {
        m_vcResults->append(
            "<p style='color:#f87171;'>\xe2\x9d\x8c File non trovato: "
            + videoPath.toHtmlEscaped() + "</p>");
        return;
    }

    const QString script = P::root() + "/Tools/scripts/video_caption.py";
    if (!QFileInfo::exists(script)) {
        m_vcResults->append(
            "<p style='color:#f87171;'>\xe2\x9d\x8c Script non trovato: "
            + script.toHtmlEscaped() + "</p>");
        return;
    }

    m_vcResults->clear();
    m_vcLineBuf.clear();
    m_vcResults->append(
        "<p style='color:#60a5fa;font-size:11px;'>"
        "\xf0\x9f\x8e\xac Avvio analisi video...</p>");

    m_vcProc = new QProcess(this);
    m_vcProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_vcProc, &QProcess::readyReadStandardOutput,
            this, &MultimediaPage::onVcProcReadyRead);
    connect(m_vcProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MultimediaPage::onVcProcFinished);

    const QStringList args = {
        script, videoPath,
        "--interval", QString::number(m_vcIntervalSpin->value()),
        "--model",    m_vcModelCombo->currentText().trimmed(),
        "--threshold",QString::number(m_vcThreshSpin->value()),
        "--max-frames",QString::number(m_vcMaxFrames->value()),
        "--port",     QString::number(P::kOllamaPort)
    };
    m_vcProc->start(P::findPython(), args);

    if (!m_vcProc->waitForStarted(3000)) {
        m_vcResults->append(
            "<p style='color:#f87171;'>\xe2\x9d\x8c Impossibile avviare lo script "
            "(Python non trovato?).</p>");
        m_vcProc->deleteLater();
        m_vcProc = nullptr;
        return;
    }

    m_vcStartBtn->setText(tr("\xe2\x8f\xb9 Ferma"));
    m_vcStatus->setText(tr("Analisi in corso..."));
}

void MultimediaPage::onVcProcReadyRead()
{
    if (!m_vcProc) return;
    m_vcLineBuf += m_vcProc->readAllStandardOutput();
    while (true) {
        const int nl = m_vcLineBuf.indexOf('\n');
        if (nl < 0) break;
        const QByteArray line = m_vcLineBuf.left(nl).trimmed();
        m_vcLineBuf.remove(0, nl + 1);
        if (line.isEmpty()) continue;

        QJsonParseError pe;
        const QJsonObject obj =
            QJsonDocument::fromJson(line, &pe).object();
        if (pe.error != QJsonParseError::NoError) continue;

        const QString status = obj["status"].toString();

        if (status == "extracting") {
            m_vcStatus->setText(tr("Estrazione frame..."));
        } else if (status == "extracted") {
            const int tot = obj["total"].toInt();
            m_vcStatus->setText(
                tr("Estratti %1 frame. Analisi in corso...").arg(tot));
            m_vcResults->append(
                QString("<p style='color:#94a3b8;font-size:10px;'>"
                        "\xf0\x9f\x8e\x9e Estratti %1 frame totali.</p>").arg(tot));
        } else if (status == "skipped") {
            const int fr = obj["frame"].toInt();
            const int dist = obj["dist"].toInt();
            m_vcStatus->setText(
                tr("Frame %1 saltato (simile, dist=%2)").arg(fr).arg(dist));
        } else if (status == "captioning") {
            const QString ts = obj["ts"].toString();
            const int fr   = obj["frame"].toInt();
            const int tot  = obj["total"].toInt();
            m_vcStatus->setText(
                tr("[%1] Frame %2/%3 — captioning...").arg(ts).arg(fr).arg(tot));
        } else if (status == "result") {
            const QString ts      = obj["ts"].toString();
            const QString caption = obj["caption"].toString();
            const int fr          = obj["frame"].toInt();
            m_vcResults->append(
                QString("<div style='margin:6px 0;border-left:3px solid #7c3aed;"
                        "padding:4px 10px;'>"
                        "<span style='color:#a78bfa;font-size:10px;font-weight:bold;'>"
                        "\xf0\x9f\x95\x90 %1 &nbsp;[frame %2]</span><br>"
                        "<span style='color:#e2e8f0;'>%3</span></div>")
                .arg(ts.toHtmlEscaped())
                .arg(fr)
                .arg(caption.toHtmlEscaped()));
        } else if (!obj["error"].isNull()) {
            m_vcResults->append(
                "<p style='color:#f87171;'>\xe2\x9d\x8c "
                + obj["error"].toString().toHtmlEscaped() + "</p>");
        }
    }
}

void MultimediaPage::onVcProcFinished(int code, QProcess::ExitStatus)
{
    onVcProcReadyRead(); /* svuota buffer residuo */

    if (m_vcProc) { m_vcProc->deleteLater(); m_vcProc = nullptr; }
    m_vcStartBtn->setText(tr("\xf0\x9f\x9a\x80 Avvia analisi"));

    if (code == 0) {
        m_vcStatus->setText(tr("Analisi completata."));
        m_vcResults->append(
            "<p style='color:#4ade80;font-size:11px;'>"
            "\xe2\x9c\x85 Analisi completata.</p>");
    } else {
        m_vcStatus->setText(tr("Terminato (codice %1).").arg(code));
    }
}

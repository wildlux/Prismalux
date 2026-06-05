#include "main_multimedia.h"
#include "widget_stable_diffusion.h"
#include "../prismalux_paths.h"
#include "../dpi_utils.h"
#include "../widgets/stt_whisper.h"
#include "../widgets/ffmpeg_utils.h"
#include "../widgets/opencv_utils.h"
#include "../widgets/world_map_widget.h"
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
    tabs->addTab(buildOcrTab(),            "\xf0\x9f\x94\x8d  OCR webcam");     /* 🔍 */

    lay->addWidget(tabs);
}

QWidget* MultimediaPage::buildSDTab()
{
    return new StableDiffusionWidget(this);
}

QWidget* MultimediaPage::buildSintetizzatoreTab()
{
    return new SintetizzatoreWidget(this);
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
    fileBtn->setFixedWidth(140);
    m_recBtn = new QPushButton("\xf0\x9f\x8e\x99  Registra", fileRow);
    m_recBtn->setObjectName("actionBtn");
    m_recBtn->setFixedWidth(120);
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
    m_audioActionCombo->setFixedWidth(230);
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
    m_graphvizInput->setFixedHeight(120);
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
            if (wav.contains("prisma_audio_tmp"))
                QFile::remove(wav);
            if (ok && !text.trimmed().isEmpty()) {
                m_audioTranscript->setPlainText(text.trimmed());
            } else {
                m_audioTranscript->setPlainText(
                    "\xe2\x9a\xa0  Trascrizione vuota o fallita.\n"
                    "Verifica che il file contenga voce udibile.");
            }
        });
}

void MultimediaPage::onTranscribeBtnClicked()
{
    if (m_audioFilePath.isEmpty()) {
        m_audioTranscript->setPlainText(
            "\xe2\x9d\x8c  Carica prima un file audio.");
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
    }
}

void MultimediaPage::onAnalyzeBtnClicked()
{
    const QString transcript = m_audioTranscript->toPlainText().trimmed();
    if (transcript.isEmpty()) {
        m_audioOutput->setPlainText(
            "\xe2\x9d\x8c  Trascrivi prima il file audio (o incolla il testo).");
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
    m_graphvizProc->start("dot", {"-Tpng", tmpDot, "-o", m_graphvizTmpPng});
    if (!m_graphvizProc->waitForStarted(3000)) {
        m_graphvizStatus->setText(
            "\xe2\x9d\x8c  Graphviz non trovato. "
            "Installa con: <b>sudo apt install graphviz</b>");
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
    btnVenv->setFixedHeight(28);
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
    btnCopyCmd->setFixedHeight(28);
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
sys.stdout.reconfigure(line_buffering=True)
try:
    import cv2
    import pytesseract
except ImportError as e:
    print(json.dumps({'error': f'Dipendenza mancante: {e}\n\nInstalla con:\n  sudo apt install python3-opencv python3-pandas tesseract-ocr tesseract-ocr-ita -y\n  python3 -m pip install pytesseract --break-system-packages'}), flush=True)
    sys.exit(0)

PREVIEW = '%1'

def ocr_frame(frame):
    """Elabora un frame e restituisce le righe di testo filtrate."""
    cv2.imwrite(PREVIEW, cv2.resize(frame, (320, 240)))
    h, w = frame.shape[:2]
    if w > 1000:
        frame = cv2.resize(frame, (1000, int(h * 1000 / w)))
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    gray = cv2.GaussianBlur(gray, (3, 3), 0)
    _, gray = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    data = pytesseract.image_to_data(gray, lang='ita+eng',
        config='--psm 6 --oem 3', output_type=pytesseract.Output.DICT)
    MIN_CONF = 55
    seen_key = None
    line_buf = []
    lines = []
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
                if len(j) >= 3: lines.append(j)
            line_buf = []
            seen_key = key
        line_buf.append(word)
    if line_buf:
        j = ' '.join(line_buf)
        if len(j) >= 3: lines.append(j)
    return lines

# Apri webcam e fai warmup
cap = cv2.VideoCapture(0)
if not cap.isOpened():
    print(json.dumps({'error': 'Webcam non disponibile — controlla /dev/video0'}), flush=True)
    sys.exit(0)
for _ in range(3):
    cap.read()
print(json.dumps({'ready': True}), flush=True)

# Loop comandi
for raw_line in sys.stdin:
    cmd = raw_line.strip()
    if not cmd:
        continue

    # ── SCAN: singolo frame dalla webcam ──
    if cmd == 'SCAN':
        ret, frame = cap.read()
        if not ret:
            print(json.dumps({'error': 'Frame non catturato'}), flush=True)
            continue
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

    m_ocrStatus->setText(tr("\xe2\x8f\xb3  Avvio daemon OCR (import librerie)..."));
    m_ocrDaemon->start(OpencvUtils::findCvPython(),
                       QStringList{"-c", ocrDaemonScript(previewPath)});
    if (!m_ocrDaemon->waitForStarted(3000)) {
        m_ocrStatus->setText(tr("\xe2\x9d\x8c  Python non trovato nel PATH."));
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
    } else {
        m_ocrTimer->stop();
        stopOcrDaemon();
        m_ocrStartBtn->setText(tr("\xe2\x96\xb6  Avvia webcam"));
        m_ocrStatus->setText(tr("Scansione fermata."));
    }
}

void MultimediaPage::onOcrTimerTick()
{
    requestOcrCapture();
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

        if (obj.contains("ready")) {
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

        if (obj.contains("error")) {
            m_ocrStatus->setText("\xe2\x9d\x8c  " + obj["error"].toString());
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
    m_ocrFfmpegProc->start(FfmpegUtils::findFfmpeg(),
        FfmpegUtils::extractArgs(m_ocrVideoPath, m_ocrAudioWav));
    if (!m_ocrFfmpegProc->waitForStarted(3000)) {
        m_ocrStatus->setText(
            "\xe2\x9d\x8c  ffmpeg non trovato. Installa con: sudo apt install ffmpeg");
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

QWidget* MultimediaPage::buildOsmMapTab()
{
    auto* w   = new QWidget;
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    /* ── Mappa principale ── */
    m_osmMap = new WorldMapWidget(w);
    m_osmMap->setRouteMode(true);
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
        "Cerca citt\xc3\xa0 → scegli Partenza o Tappa."
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

    /* Waypoint aggiunto dalla mappa → aggiorna lista */
    connect(m_osmMap, &WorldMapWidget::waypointAdded,
            w, [this](int idx, double lat, double lon) {
        const QString lbl = QString::fromLatin1("%1").arg(QChar('A' + idx % 26));
        auto* item = new QListWidgetItem(
            QString("%1  %.5f, %.5f").arg(lbl).arg(lat).arg(lon));
        item->setData(Qt::UserRole,     lat);
        item->setData(Qt::UserRole + 1, lon);
        if (m_osmWpList) m_osmWpList->addItem(item);
    });

    /* insertStartWaypoint → ricostruisce lista completa */
    connect(m_osmMap, &WorldMapWidget::waypointsReset,
            w, [this] {
        if (!m_osmWpList || !m_osmMap) return;
        m_osmWpList->clear();
        const auto& coords = m_osmMap->waypoints();
        const auto& labels = m_osmMap->waypointLabels();
        for (int i = 0; i < coords.size(); ++i) {
            auto* item = new QListWidgetItem(
                QString("%1  %.5f, %.5f")
                    .arg(labels.value(i, "?"))
                    .arg(coords[i].first)
                    .arg(coords[i].second));
            item->setData(Qt::UserRole,     coords[i].first);
            item->setData(Qt::UserRole + 1, coords[i].second);
            m_osmWpList->addItem(item);
        }
        if (m_osmRouteInfo)
            m_osmRouteInfo->setText(tr("Distanza: \xe2\x80\x94  |  Tempo: \xe2\x80\x94"));
    });

    /* Rimuovi waypoint selezionato */
    connect(btnRemWp, &QPushButton::clicked, w, [this] {
        if (!m_osmWpList) return;
        const int row = m_osmWpList->currentRow();
        if (row < 0) return;
        m_osmWpList->takeItem(row);
        m_osmMap->clearRoute();
        for (int i = 0; i < m_osmWpList->count(); ++i) {
            auto* it = m_osmWpList->item(i);
            m_osmMap->addWaypoint(it->data(Qt::UserRole).toDouble(),
                                  it->data(Qt::UserRole + 1).toDouble());
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
                      "Prismalux/2.9 (educational desktop app)");

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
                return;
            }

            const QJsonObject root = QJsonDocument::fromJson(
                reply->readAll()).object();

            if (root["code"].toString() != "Ok") {
                if (m_osmRouteInfo)
                    m_osmRouteInfo->setText(
                        "\xe2\x9d\x8c  OSRM: " + root["message"].toString());
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
                                "Prismalux/2.9");

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
        req.setHeader(QNetworkRequest::UserAgentHeader, "Prismalux/2.9");

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

    return w;
}

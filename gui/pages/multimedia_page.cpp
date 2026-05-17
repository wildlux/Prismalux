#include "multimedia_page.h"
#include "stable_diffusion_widget.h"
#include "../prismalux_paths.h"
#include "../widgets/stt_whisper.h"
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

namespace P = PrismaluxPaths;

MultimediaPage::MultimediaPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildAudioTab(),    "\xf0\x9f\x8e\xb5  Audio AI");        /* 🎵 */
    tabs->addTab(buildSDTab(),       "\xf0\x9f\x8e\xa8  Genera Immagini"); /* 🎨 */
    tabs->addTab(buildGraphvizTab(), "\xf0\x9f\x97\xba  Mappe");            /* 🗺 */

    lay->addWidget(tabs);
}

QWidget* MultimediaPage::buildSDTab()
{
    return new StableDiffusionWidget(this);
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
        m_recBtn->setText("\xe2\x8f\xb9  Ferma registrazione");
        m_audioFileLbl->setText("\xf0\x9f\x94\xb4  Registrazione in corso...");
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
        m_recBtn->setText("\xf0\x9f\x8e\x99  Registra");
        if (QFile::exists(m_recPath)) {
            m_audioFilePath = m_recPath;
            m_audioFileLbl->setText("\xe2\x9c\x85  prismalux_record.wav");
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
    if (!SttWhisper::isAvailable()) {
        m_audioTranscript->setPlainText(
            "\xe2\x9a\xa0  whisper-cli o modello non trovati.\n"
            "Configurali in Impostazioni \xe2\x86\x92 Trascrivi.");
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
        m_ffmpegProc->start("ffmpeg", {"-y", "-i", m_audioFilePath,
            "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le", m_ffmpegWavTmp});
    }
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
    m_graphvizStatus->setText("\xe2\x8f\xb3  Rendering con Graphviz...");
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
    m_graphvizStatus->setText("\xe2\x9d\x8c  Errore AI");
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

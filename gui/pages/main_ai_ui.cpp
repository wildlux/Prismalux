#include "main_ai.h"
#include "main_ai_p.h"
#include "../dpi_utils.h"
#include "../widgets/latex_view.h"
#include <QPainter>
#include <QFont>
#include <QTextCharFormat>
#include <QMouseEvent>
#include <QColorDialog>
#include "../prismalux_paths.h"
#include "../log_bus.h"
namespace P = PrismaluxPaths;
#include "../app_config.h"
#include <QTime>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QShortcut>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLabel>
#include <QFrame>
#include <QUrl>
#include <QNetworkRequest>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QScrollArea>
#include <QScrollBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QPrinter>
#include <QPrintDialog>
#include <QPageSize>
#include <QImage>
#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <QTimer>
#include <QSettings>
#include <QToolTip>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QTextDocument>
#include <QTextCursor>
#include "../widgets/stt_whisper.h"
#include "../widgets/model_combo_helper.h"
#include "../widgets/chart_widget.h"
#include "../widgets/formula_parser.h"
#include "dialog_agents_config.h"
#include <QStandardPaths>
#include <QGroupBox>
#ifndef Q_OS_WIN
#  include <csignal>
#  include <sys/types.h>
#endif

/* ══════════════════════════════════════════════════════════════
   setupUI — stepdown rule: ogni build* fa una cosa a un livello
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::setupUI()
{
    /* Carica ultima preferenza thinking: aperto o chiuso */
    m_thinkDefaultOpen = QSettings().value("ui/thinkDefaultOpen", false).toBool();

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->setSpacing(8);

    buildToolbar(lay);
    buildChatLog(lay);
    buildChartPanel(lay);
    QPushButton* btnSymbols = buildInputArea(lay);
    buildMathPanel(lay);
    buildRagPanel(lay);
    buildHintFooter(lay);
    buildInputConnections(btnSymbols);
    buildSymbolsPanel(lay, btnSymbols);
    buildToolsPanel(lay);
    buildBottomBar(lay);
    buildExtraConnections();
}

/* ──────────────────────────────────────────────────────────────
   buildToolbar — barra superiore: wait label + TTS + export +
   knowledge + info + voice loop + mode toggle + tools + LLM
   + label stato preset
   ────────────────────────────────────────────────────────────── */
void AgentiPage::buildToolbar(QVBoxLayout* lay)
{
    auto* toolbar = new QWidget(this);
    auto* toolLay = new QHBoxLayout(toolbar);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    buildToolbarTtsSection(toolLay, toolbar);
    buildToolbarExportSection(toolLay, toolbar);
    toolLay->addStretch(1);
    /* m_btnVoiceLoop rimosso: funzionalità Conversa gestita dal TriModeButton */
    buildToolbarModeToggle(toolLay, toolbar);
    buildToolbarLLMSelector(toolLay, toolbar);

    lay->addWidget(toolbar);

    /* Stato auto-assegnazione / preset */
    m_autoLbl = new QLabel("", this);
    m_autoLbl->setObjectName("cardDesc");
    m_autoLbl->setVisible(false);
    lay->addWidget(m_autoLbl);
}

/* ── Sezione TTS: wait label + Ferma lettura + Pausa ── */
void AgentiPage::buildToolbarTtsSection(QHBoxLayout* toolLay, QWidget* toolbar)
{
    m_waitLbl = new QLabel(this);
    m_waitLbl->setStyleSheet("color: #E5C400; padding: 2px 0; font-style: italic;");
    m_waitLbl->setVisible(false);
    toolLay->addWidget(m_waitLbl, 1);

    m_btnTtsStop = new QPushButton("\xe2\x8f\xb9 Ferma lettura", toolbar);
    m_btnTtsStop->setObjectName("actionBtn");
    m_btnTtsStop->setToolTip(tr("Interrompi la lettura vocale"));
    m_btnTtsStop->setVisible(false);
    toolLay->addWidget(m_btnTtsStop);
    connect(m_btnTtsStop, &QPushButton::clicked, this, &AgentiPage::onTtsStopClicked);

    m_btnTtsPause = new QPushButton("\xe2\x8f\xb8  Pausa", toolbar);
    m_btnTtsPause->setObjectName("actionBtn");
    m_btnTtsPause->setToolTip(tr("Metti in pausa / riprendi la lettura vocale"));
    m_btnTtsPause->setVisible(false);
    toolLay->addWidget(m_btnTtsPause);
    connect(m_btnTtsPause, &QPushButton::clicked, this, &AgentiPage::onTtsPauseClicked);
}

/* ── Sezione export: Esporta + PDF + Memoria + Info ── */
void AgentiPage::buildToolbarExportSection(QHBoxLayout* toolLay, QWidget* toolbar)
{
    auto* btnExport = new QPushButton("\xf0\x9f\x92\xbe  Esporta", toolbar);
    btnExport->setObjectName("actionBtn");
    btnExport->setToolTip(tr("Esporta conversazione (.md / .html / .txt)"));
    toolLay->addWidget(btnExport);
    connect(btnExport, &QPushButton::clicked, this, &AgentiPage::onBtnExportClicked);

    auto* btnExportPdf = new QPushButton("\xf0\x9f\x93\x84  PDF", toolbar);
    btnExportPdf->setObjectName("actionBtn");
    btnExportPdf->setToolTip(tr("Esporta conversazione (.pdf)"));
    toolLay->addWidget(btnExportPdf);
    connect(btnExportPdf, &QPushButton::clicked, this, &AgentiPage::onBtnExportPdfClicked);

    m_btnKnowledge = new QPushButton("\xf0\x9f\x93\x96  Memoria", toolbar);  /* 📖 */
    m_btnKnowledge->setObjectName("actionBtn");
    m_btnKnowledge->setToolTip(
        "Salva risposta in user_knowledge.md\n"
        "Il testo viene iniettato nel context di ogni sessione AI futura.");
    toolLay->addWidget(m_btnKnowledge);
    connect(m_btnKnowledge, &QPushButton::clicked, this, &AgentiPage::onSaveKnowledge);

    m_btnEtimo = new QPushButton("\xf0\x9f\x8f\x9b  Etimo", toolbar);  /* 🏛 */
    m_btnEtimo->setObjectName("actionBtn");
    m_btnEtimo->setCheckable(true);
    m_btnEtimo->setToolTip(
        "Modalita' Dizionario Etimologico\n"
        "L'AI risponde in stile Wikipedia: origine greca/latina,\n"
        "morfologia, evoluzione semantica, derivati moderni.");
    toolLay->addWidget(m_btnEtimo);
    connect(m_btnEtimo, &QPushButton::toggled, toolbar, [this](bool on) {
        m_btnEtimo->setStyleSheet(on
            ? "QPushButton{background:#7c3aed;color:#fff;border:1px solid #6d28d9;"
              "border-radius:4px;padding:3px 8px;font-weight:bold;}"
            : "");
    });

    m_btnMathToggle = new QPushButton("\xe2\x88\x91  Formule", toolbar);  /* ∑ */
    m_btnMathToggle->setObjectName("actionBtn");
    m_btnMathToggle->setCheckable(true);
    m_btnMathToggle->setToolTip(
        "Pannello formule matematiche LaTeX\n"
        "Preview in tempo reale + template per frazioni, limiti,\n"
        "sommatorie, integrali, radici. Auto-sostituzione mentre si scrive.");
    toolLay->addWidget(m_btnMathToggle);
    connect(m_btnMathToggle, &QPushButton::toggled, toolbar, [this](bool on) {
        m_btnMathToggle->setStyleSheet(on
            ? "QPushButton{background:#0e7490;color:#fff;border:1px solid #0891b2;"
              "border-radius:4px;padding:3px 8px;font-weight:bold;}"
            : "");
        if (m_mathPanel) m_mathPanel->setVisible(on);
    });

    auto* btnInfo = new QPushButton("\xe2\x84\xb9  Informazioni", toolbar);  /* ℹ */
    btnInfo->setObjectName("actionBtn");
    btnInfo->setToolTip(tr("Mostra/nascondi suggerimenti"));
    toolLay->addWidget(btnInfo);
    connect(btnInfo, &QPushButton::clicked, this, &AgentiPage::onBtnInfoClicked);
}

/* ── Conversazione Vocale continua (loop STT → AI → TTS) ── */
void AgentiPage::buildToolbarVoiceLoop(QHBoxLayout* toolLay, QWidget* toolbar)
{
    static const char* kVoiceOff =
        "QPushButton{"
          "background:#1e2d45;border:2px solid #334155;color:#64748b;"
          "border-radius:14px;padding:4px 12px;font-weight:bold;font-size:12px;}"
        "QPushButton:hover{background:#243650;color:#94a3b8;}";

    const QString pName = P::personalityName();
    const QString label = pName.isEmpty()
        ? "\xf0\x9f\x8e\x99  Conversa"
        : "\xf0\x9f\x8e\x99  Conversa con " + pName;
    m_btnVoiceLoop = new QPushButton(label, toolbar);
    m_btnVoiceLoop->setCheckable(true);
    m_btnVoiceLoop->setChecked(false);
    m_btnVoiceLoop->setStyleSheet(kVoiceOff);
    m_btnVoiceLoop->setToolTip(
        "Conversazione vocale continua (loop)\n"
        "Parla \xe2\x80\x94 AI risponde \xe2\x80\x94 ascolta \xe2\x80\x94 riparla\n"
        "Richiede whisper.cpp + TTS configurati nelle Impostazioni");
    toolLay->addWidget(m_btnVoiceLoop);
    connect(m_btnVoiceLoop, &QPushButton::toggled, this, &AgentiPage::onVoiceLoopToggled);
}

/* ── Toggle Chat/Agentico/Conversa — TriModeButton creato in buildInputRagToggle() ── */
void AgentiPage::buildToolbarModeToggle(QHBoxLayout* /*toolLay*/, QWidget* /*toolbar*/)
{
}

/* ── Selettore LLM nella toolbar ── */
void AgentiPage::buildToolbarLLMSelector(QHBoxLayout* toolLay, QWidget* toolbar)
{
    auto* llmLbl = new QLabel("LLM:", toolbar);
    llmLbl->setObjectName("cardDesc");
    m_cmbLLM = new QComboBox(toolbar);
    m_cmbLLM->setObjectName("settingsCombo");
    m_cmbLLM->setMinimumWidth(dpiScale(160));
    m_cmbLLM->setToolTip(tr("Seleziona il modello AI da usare."));
    m_cmbLLM->setAccessibleName("Selettore modello AI");
    m_cmbLLM->addItem("(caricamento...)");
    toolLay->addWidget(llmLbl);
    toolLay->addWidget(m_cmbLLM);
    connect(m_cmbLLM, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentiPage::onCmbLLMIndexChanged);

    /* Pulsante "🔄 Rigenera con [modello]" — appare quando il modello cambia
       con la chat non vuota, per permettere di rilanciare l'ultimo task col nuovo LLM. */
    m_btnRegen = new QPushButton("\xf0\x9f\x94\x84 Rigenera", toolbar);
    m_btnRegen->setObjectName("actionBtn");
    m_btnRegen->setToolTip(
        "Reinvia l'ultimo messaggio utente con il modello appena selezionato");
    m_btnRegen->setVisible(false);
    toolLay->addWidget(m_btnRegen);
    connect(m_btnRegen, &QPushButton::clicked, this, &AgentiPage::onBtnRegenClicked);

    m_modelWarnLbl = new QLabel(toolbar);
    m_modelWarnLbl->setObjectName("modelWarnLbl");
    m_modelWarnLbl->setStyleSheet("color:#f59e0b;font-size:11px;font-style:italic;");
    m_modelWarnLbl->setWordWrap(false);
    m_modelWarnLbl->setVisible(false);
    toolLay->addWidget(m_modelWarnLbl);

    /* Hermes toggle rimosso dalla toolbar — ora nella barra inferiore */
}

/* ──────────────────────────────────────────────────────────────
   buildChatLog — QTextBrowser + auto-scroll + context menu
   ────────────────────────────────────────────────────────────── */
void AgentiPage::buildChatLog(QVBoxLayout* lay)
{
    m_log = new QTextBrowser(this);
    m_log->setObjectName("chatLog");
    m_log->setReadOnly(true);
    m_log->setOpenLinks(false);
    m_log->setOpenExternalLinks(false);
    m_log->document()->setDefaultStyleSheet("body { color:#e2e8f0; }");
    m_log->setPlaceholderText(
        "L'output degli agenti appare qui...\n\n"
        "\xf0\x9f\x8d\xba Invocazione riuscita. Gli dei ascoltano.");
    lay->addWidget(m_log, 1);

    connect(m_log->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &AgentiPage::onLogScrollValueChanged);
    connect(m_log, &QTextBrowser::anchorClicked,
            this, &AgentiPage::onLogAnchorClicked);

    m_log->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_log, &QTextEdit::customContextMenuRequested,
            this, &AgentiPage::onLogContextMenuRequested);
}

/* ──────────────────────────────────────────────────────────────
   buildChartPanel — pannello grafico inline (nascosto di default)
   ────────────────────────────────────────────────────────────── */
void AgentiPage::buildChartPanel(QVBoxLayout* lay)
{
    m_chartPanel = new QFrame(this);
    m_chartPanel->setObjectName("cardFrame");
    m_chartPanel->setVisible(false);
    m_chartPanel->setFixedHeight(dpiScale(260));

    auto* cpLay = new QVBoxLayout(m_chartPanel);
    cpLay->setContentsMargins(8, 6, 8, 6);
    cpLay->setSpacing(4);

    auto* cpHeader = new QWidget(m_chartPanel);
    auto* cpHL = new QHBoxLayout(cpHeader);
    cpHL->setContentsMargins(0, 0, 0, 0);

    auto* cpLbl = new QLabel(
        "\xf0\x9f\x93\x8a  <b>Grafico Cartesiano</b>"
        " &nbsp;<span style='color:#7b7f9e;font-size:11px;'>"
        "\xf0\x9f\x96\xb1 Click destro per salvare il grafico"
        "</span>", m_chartPanel);
    cpLbl->setObjectName("cardTitle");
    cpLbl->setTextFormat(Qt::RichText);
    cpHL->addWidget(cpLbl, 1);

    m_btnChartOpen = new QPushButton("\xf0\x9f\x93\x88  Apri nel Grafico", m_chartPanel);
    m_btnChartOpen->setObjectName("actionBtn");
    m_btnChartOpen->setToolTip(tr("Apri nella sezione Grafico per zoom, export e personalizzazione"));
    connect(m_btnChartOpen, &QPushButton::clicked, this, &AgentiPage::onBtnChartOpenClicked);
    cpHL->addWidget(m_btnChartOpen);

    auto* cpClose = new QPushButton("\xc3\x97", m_chartPanel);
    cpClose->setObjectName("actionBtn");
    cpClose->setFixedSize(dpiSize(22, 22));
    cpClose->setToolTip(tr("Chiudi grafico"));
    connect(cpClose, &QPushButton::clicked, m_chartPanel, &QWidget::hide);
    cpHL->addWidget(cpClose);

    cpLay->addWidget(cpHeader);
    /* Il ChartWidget viene aggiunto dinamicamente da tryShowChart() */

    lay->addWidget(m_chartPanel);
}

/* ──────────────────────────────────────────────────────────────
   buildInputArea — griglia testo + pulsanti azione
   Restituisce il pulsante Simboli (necessario a buildSymbolsPanel)
   ────────────────────────────────────────────────────────────── */
QPushButton* AgentiPage::buildInputArea(QVBoxLayout* lay)
{
    auto* inputArea = new QWidget(this);
    auto* inputGrid = new QGridLayout(inputArea);
    inputGrid->setContentsMargins(0, 0, 0, 0);
    inputGrid->setSpacing(6);
    inputGrid->setColumnStretch(0, 1);
    inputGrid->setColumnStretch(1, 0);
    inputGrid->setColumnStretch(2, 0);
    inputGrid->setColumnStretch(3, 0);

    buildInputTextField(inputGrid, inputArea);
    QPushButton* btnSymbols = buildInputActionButtons(inputGrid, inputArea);
    buildInputRagToggle(inputGrid, inputArea);
    buildInputTabOrder(btnSymbols);

    lay->addWidget(inputArea);
    return btnSymbols;
}

/* ── Campo testo multi-riga (col 0, rowspan 2) ── */
void AgentiPage::buildInputTextField(QGridLayout* inputGrid, QWidget* inputArea)
{
    m_input = new QTextEdit(inputArea);
    m_input->setObjectName("chatInput");
    m_input->setPlaceholderText(tr("Scrivi la tua domanda..."));
    m_input->setAcceptRichText(true);
    m_input->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_input->setAccessibleName("Campo messaggio chat");
    m_input->setAccessibleDescription(
        "Scrivi qui il messaggio da inviare all'AI. Premi Ctrl+Invio per inviare.");
    inputGrid->addWidget(m_input, 0, 0, 3, 1);
    buildInputFormatBar();
}

/* ── Pulsanti azione (col 1-3): Avvia/Voce/Simboli/Traduci/Documenti/Immagini ──
   Restituisce il pulsante Simboli usato più avanti da buildSymbolsPanel.       */
QPushButton* AgentiPage::buildInputActionButtons(QGridLayout* inputGrid, QWidget* inputArea)
{
    auto tagExec = [](QPushButton* btn, const char* icon, const char* text){
        btn->setProperty("execFull", btn->text());
        btn->setProperty("execIcon", QString::fromUtf8(icon));
        btn->setProperty("execText", QString::fromUtf8(text));
    };

    m_btnRun = new QPushButton("\xf0\x9f\x93\xa4 Invia", inputArea);
    m_btnRun->setObjectName("actionBtn");
    m_btnRun->setProperty("bigBtn", "true");
    m_btnRun->setVisible(false);   /* hub ovale TriModeButton è l'azione visiva */
    m_btnRun->setToolTip(
        "Risposta immediata con contesto RAG \xe2\x80\x94 1 solo agente (Invio)\n"
        "Stop da fermo \xe2\x86\x92 cambia modalit\xc3\xa0 (Invia \xe2\x86\x94 Avvia)");
    m_btnRun->setAccessibleName("Avvia o ferma la risposta AI");
    tagExec(m_btnRun, "\xf0\x9f\x93\xa4", "Invia");

    m_btnVoice = new QPushButton("\xf0\x9f\x8e\xa4 Trascrivi parlato", inputArea);
    m_btnVoice->setObjectName("actionBtn");
    m_btnVoice->setToolTip(tr("Parla — trascrivi la voce nel campo di testo (whisper.cpp)"));
    m_btnVoice->setAccessibleName("Trascrivi voce in testo");
    tagExec(m_btnVoice, "\xf0\x9f\x8e\xa4", "Trascrivi parlato");

    auto* btnSymbols = new QPushButton("\xce\xa9  Simboli", inputArea);
    btnSymbols->setObjectName("actionBtn");
    btnSymbols->setToolTip(tr("Inserisci caratteri speciali: matematica, greco, lingue"));
    btnSymbols->setAccessibleName("Inserisci simbolo speciale");

    m_btnDoc = new QPushButton("\xf0\x9f\x93\x8e  Allega file", inputArea);
    m_btnDoc->setObjectName("actionBtn");
    m_btnDoc->setToolTip(tr(
        "Allega un file alla chat.\n"
        "Documenti: .txt .md .csv .json .py .cpp .h .pdf .xls\n"
        "Immagini:  .png .jpg .jpeg .gif .webp (richiede modello vision)"));
    m_btnDoc->setAccessibleName("Allega file al messaggio");
    tagExec(m_btnDoc, "\xf0\x9f\x93\x8e", "Allega file");

    /* Col 2 r0: Allega file  r1: Simboli  r2: Trascrivi parlato (m_btnRun nascosto) */
    inputGrid->addWidget(m_btnDoc,   0, 2);
    inputGrid->addWidget(btnSymbols, 1, 2);
    inputGrid->addWidget(m_btnVoice, 2, 2);

    return btnSymbols;
}

/* ── Cerchio 3 settori (col 2, rowspan 2) + Tools/Memoria (col 4) ── */
void AgentiPage::buildInputRagToggle(QGridLayout* inputGrid, QWidget* inputArea)
{
    m_modeBtn = new TriModeButton(inputArea);
    inputGrid->addWidget(m_modeBtn, 0, 1, 3, 1);   /* col 1, rowspan 3 */
    m_modeBtn->setActionText("\xf0\x9f\x93\xa4 Invia");
    connect(m_modeBtn, &TriModeButton::modeChanged,   this, &AgentiPage::onModeBtnChanged);
    connect(m_modeBtn, &TriModeButton::actionClicked, this, &AgentiPage::onBtnRunClicked);

    /* Col 3: ⚡ Tool Veloci (r0) · 🔌 Tool Lenti (r1) · Memoria (r2) */
    inputGrid->setColumnStretch(3, 0);

    m_btnToolsToggle = new QPushButton("\xe2\x9a\xa1  Tool Veloci", inputArea);
    m_btnToolsToggle->setCheckable(true);
    m_btnToolsToggle->setObjectName("actionBtn");
    m_btnToolsToggle->setToolTip(
        "Apri/chiudi i Tool Veloci (Function Tools).\n"
        "Eseguiti in-process, risposta < 1ms.\n"
        "Calcola, cerca online, leggi file, Python, RAG\xe2\x80\xa6");
    inputGrid->addWidget(m_btnToolsToggle, 0, 3);
    connect(m_btnToolsToggle, &QPushButton::toggled,
            this, &AgentiPage::onToolsPanelToggle);

    m_btnMcpToggle = new QPushButton("\xf0\x9f\x94\x8c  Tool Lenti (MCP)", inputArea);
    m_btnMcpToggle->setCheckable(true);
    m_btnMcpToggle->setObjectName("actionBtn");
    m_btnMcpToggle->setToolTip(
        "Apri/chiudi i Tool Lenti \xe2\x80\x94 MCP Plugin.\n"
        "Avviati come processo separato (JSON-RPC 2.0 stdio).\n"
        "Latenza maggiore ma accesso a strumenti esterni.");
    inputGrid->addWidget(m_btnMcpToggle, 1, 3);
    connect(m_btnMcpToggle, &QPushButton::toggled,
            this, &AgentiPage::onMcpPanelToggle);

    /* Cella composita: [Memoria persistente][⌛] — riga 2 */
    auto* hermesCell = new QWidget(inputArea);
    auto* hermesCellLay = new QHBoxLayout(hermesCell);
    hermesCellLay->setContentsMargins(0, 0, 0, 0);
    hermesCellLay->setSpacing(2);

    m_hermesToggleBar = new QPushButton("\xf0\x9f\xa7\xa0  Memoria persistente", hermesCell);
    m_hermesToggleBar->setCheckable(true);
    m_hermesToggleBar->setObjectName("actionBtn");
    m_hermesToggleBar->setToolTip(
        "Attiva la memoria persistente tra sessioni.\n"
        "Le conversazioni vengono salvate in GraphMemory\n"
        "e usate come contesto nelle sessioni future.");
    hermesCellLay->addWidget(m_hermesToggleBar, 1);

    /* Pulsante Riflessione — icona ⌛ — visibile solo quando Hermes è attivo */
    m_hermesReflectBar = new QPushButton("\xe2\x8c\x9b", hermesCell);
    m_hermesReflectBar->setObjectName("actionBtn");
    m_hermesReflectBar->setFixedWidth(dpiScale(30));
    m_hermesReflectBar->setToolTip(
        "\xe2\x8c\x9b  Riflessione\n"
        "L'AI analizza le sessioni memorizzate e identifica\n"
        "pattern, lacune e suggerimenti per sessioni future.");
    m_hermesReflectBar->setVisible(false);
    hermesCellLay->addWidget(m_hermesReflectBar);

    inputGrid->addWidget(hermesCell, 2, 3);
}

/* ── Tab order: campo testo → Avvia → Voce → Simboli → Allega file ── */
void AgentiPage::buildInputTabOrder(QPushButton* btnSymbols)
{
    QWidget::setTabOrder(m_input,    m_btnVoice);
    QWidget::setTabOrder(m_btnVoice, btnSymbols);
    QWidget::setTabOrder(btnSymbols, m_btnDoc);
}

/* ──────────────────────────────────────────────────────────────
   buildRagPanel — pannello RAG collapsibile (m_ragPanel)
   ────────────────────────────────────────────────────────────── */
void AgentiPage::buildRagPanel(QVBoxLayout* lay)
{
    m_ragPanel = new QWidget(this);
    auto* ragPanelLay = new QVBoxLayout(m_ragPanel);
    ragPanelLay->setContentsMargins(0, 4, 0, 0);
    ragPanelLay->setSpacing(6);

    /* ── Riga 1: RAG inline testo/URL ── */
    auto* row1 = new QWidget(m_ragPanel);
    auto* row1Lay = new QHBoxLayout(row1);
    row1Lay->setContentsMargins(0, 0, 0, 0);
    row1Lay->setSpacing(6);

    auto* ragLbl = new QLabel(
        "\xf0\x9f\x93\x8e  <b>RAG inline</b> \xe2\x80\x94 "
        "trascina file o inserisci URL per aggiungere contesto alla chat:",
        row1);
    ragLbl->setObjectName("footerHints");
    ragLbl->setTextFormat(Qt::RichText);
    row1Lay->addWidget(ragLbl);

    m_ragInline = new RagDropWidget(row1);
    m_ragInline->setMinimumHeight(dpiScale(64));
    m_ragInline->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row1Lay->addWidget(m_ragInline, 1);
    ragPanelLay->addWidget(row1);

    /* ── Riga 2: zona drop PDF/txt/md per indicizzazione nel RAG Engine ── */
    auto* row2 = new QWidget(m_ragPanel);
    auto* row2Lay = new QHBoxLayout(row2);
    row2Lay->setContentsMargins(0, 0, 0, 0);
    row2Lay->setSpacing(8);

    m_ragDropZone = new QLabel(
        "\xf0\x9f\x93\x84  Trascina qui PDF / .txt / .md\n"
        "per indicizzarli nel RAG",
        row2);
    m_ragDropZone->setObjectName("ragDropZone");
    m_ragDropZone->setAlignment(Qt::AlignCenter);
    m_ragDropZone->setAcceptDrops(true);
    m_ragDropZone->setMinimumHeight(dpiScale(56));
    m_ragDropZone->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_ragDropZone->setStyleSheet(
        "QLabel#ragDropZone{"
          "border:2px dashed #4a5568;"
          "border-radius:6px;"
          "color:#7b8fa5;"
          "background:#0f172a;"
          "font-size:12px;"
          "padding:6px 12px;"
        "}"
        "QLabel#ragDropZone[dragOver=true]{"
          "border-color:#60a5fa;"
          "color:#93c5fd;"
          "background:#0d1f3c;"
        "}");
    row2Lay->addWidget(m_ragDropZone, 1);

    m_ragStatusLbl = new QLabel("", row2);
    m_ragStatusLbl->setObjectName("footerHints");
    m_ragStatusLbl->setMinimumWidth(dpiScale(180));
    m_ragStatusLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    row2Lay->addWidget(m_ragStatusLbl);

    ragPanelLay->addWidget(row2);

    /* ── Installa event filter per drag & drop sulla zona PDF/txt/md ── */
    struct RagZoneFilter : public QObject {
        AgentiPage* page;
        explicit RagZoneFilter(AgentiPage* p, QObject* par) : QObject(par), page(p) {}
        bool eventFilter(QObject* obj, QEvent* ev) override {
            auto* lbl = qobject_cast<QLabel*>(obj);
            if (!lbl) return false;
            if (ev->type() == QEvent::DragEnter) {
                auto* de = static_cast<QDragEnterEvent*>(ev);
                bool ok = false;
                if (de->mimeData()->hasUrls()) {
                    for (const QUrl& u : de->mimeData()->urls()) {
                        const QString p = u.toLocalFile().toLower();
                        if (p.endsWith(".pdf") || p.endsWith(".txt") || p.endsWith(".md")) {
                            ok = true; break;
                        }
                    }
                }
                if (ok) {
                    de->acceptProposedAction();
                    lbl->setProperty("dragOver", true);
                    lbl->style()->unpolish(lbl);
                    lbl->style()->polish(lbl);
                    page->onRagDropZoneEnter();
                }
                return true;
            }
            if (ev->type() == QEvent::DragLeave) {
                lbl->setProperty("dragOver", false);
                lbl->style()->unpolish(lbl);
                lbl->style()->polish(lbl);
                page->onRagDropZoneLeave();
                return true;
            }
            if (ev->type() == QEvent::Drop) {
                auto* de = static_cast<QDropEvent*>(ev);
                lbl->setProperty("dragOver", false);
                lbl->style()->unpolish(lbl);
                lbl->style()->polish(lbl);
                if (!de->mimeData()->hasUrls()) return true;
                de->acceptProposedAction();
                page->_ingestRagFiles(de->mimeData()->urls());
                return true;
            }
            return false;
        }
    };
    m_ragDropZone->installEventFilter(new RagZoneFilter(this, m_ragDropZone));

    /* ── Riga 3: Web reading — URL → fetch → chunk → RAG inline ── */
    auto* row3 = new QWidget(m_ragPanel);
    auto* row3Lay = new QHBoxLayout(row3);
    row3Lay->setContentsMargins(0, 0, 0, 0);
    row3Lay->setSpacing(6);

    auto* urlIco = new QLabel("\xf0\x9f\x8c\x90", row3);  /* 🌐 */
    urlIco->setObjectName("footerHints");
    row3Lay->addWidget(urlIco);

    m_ragUrlLine = new QLineEdit(row3);
    m_ragUrlLine->setPlaceholderText(tr("https://... — Aggiungi pagina web al RAG"));
    m_ragUrlLine->setClearButtonEnabled(true);
    row3Lay->addWidget(m_ragUrlLine, 1);

    auto* btnAddUrl = new QPushButton("\xe2\x9e\x95  Aggiungi URL", row3);  /* ➕ */
    btnAddUrl->setObjectName("footerHints");
    row3Lay->addWidget(btnAddUrl);

    ragPanelLay->addWidget(row3);

    connect(btnAddUrl, &QPushButton::clicked, this, &AgentiPage::onRagUrlAddClicked);
    connect(m_ragUrlLine, &QLineEdit::returnPressed, this, &AgentiPage::onRagUrlAddClicked);

    m_ragPanel->hide();
    lay->addWidget(m_ragPanel);
}

/* ── Slot: feedback visivo zona drop (enter/leave/done) ── */
void AgentiPage::onRagDropZoneEnter()
{
    if (m_ragDropZone)
        m_ragDropZone->setText(
            "\xf0\x9f\x93\x84  Rilascia per indicizzare nel RAG...");
}

void AgentiPage::onRagDropZoneLeave()
{
    if (m_ragDropZone && !m_ragIngesting)
        m_ragDropZone->setText(
            "\xf0\x9f\x93\x84  Trascina qui PDF / .txt / .md\n"
            "per indicizzarli nel RAG");
}

void AgentiPage::onRagIngestionDone()
{
    m_ragIngesting = false;
    if (m_ragDropZone)
        m_ragDropZone->setText(
            "\xf0\x9f\x93\x84  Trascina qui PDF / .txt / .md\n"
            "per indicizzarli nel RAG");
    if (m_ragStatusLbl)
        m_ragStatusLbl->setText(tr("\xe2\x9c\x85  Indicizzato nel RAG"));
    /* Nasconde il messaggio dopo 3 secondi via slot nominato */
    if (m_ragStatusLbl)
        QTimer::singleShot(3000, m_ragStatusLbl, &QLabel::clear);
}

/* ──────────────────────────────────────────────────────────────
   buildHintFooter — striscia suggerimenti tasti (nascondibile)
   ────────────────────────────────────────────────────────────── */
void AgentiPage::buildHintFooter(QVBoxLayout* lay)
{
    m_hintWidget = new QWidget(this);
    auto* hintLay = new QHBoxLayout(m_hintWidget);
    hintLay->setContentsMargins(6, 2, 6, 2);
    hintLay->setSpacing(6);

    auto* hintLbl = new QLabel(
        "\xe2\x8c\xa8  <b>Invio</b> = esegui &nbsp;\xc2\xb7&nbsp; "
        "<b>Shift+Invio</b> = a capo &nbsp;\xc2\xb7&nbsp; "
        "<b>Stop da fermo</b> = cambia Chat \xe2\x86\x94 Avvia<br>"
        "\xf0\x9f\x93\x88  Grafico: es. <i>Grafico di sin(x) per x da -3 a 3</i>",
        m_hintWidget);
    hintLbl->setObjectName("footerHints");
    hintLbl->setWordWrap(false);
    hintLbl->setTextFormat(Qt::RichText);
    hintLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hintLay->addWidget(hintLbl, 1);

    auto* btnHide = new QPushButton("\xe2\x9c\x95", m_hintWidget);
    btnHide->setFixedSize(dpiSize(18, 18));
    btnHide->setObjectName("hintCloseBtn");
    btnHide->setToolTip(tr("Nascondi suggerimenti"));
    btnHide->setStyleSheet(
        "QPushButton{background:transparent;border:none;color:#475569;"
        "font-size:11px;padding:0;}"
        "QPushButton:hover{color:#94a3b8;}");
    hintLay->addWidget(btnHide);
    connect(btnHide, &QPushButton::clicked, this, &AgentiPage::onBtnHintHideClicked);

    lay->addWidget(m_hintWidget);

    const bool vis = AppConfig::s().value("ui/hintVisible", true).toBool();
    m_hintWidget->setVisible(vis);
}

/* ──────────────────────────────────────────────────────────────
   buildInputConnections — collega m_btnRun + filtro Enter su m_input
   + drag-and-drop file su m_input
   ────────────────────────────────────────────────────────────── */
void AgentiPage::buildInputConnections(QPushButton* btnSymbols)
{
    connect(m_btnRun, &QPushButton::clicked, this, &AgentiPage::onBtnRunClicked);

    /* Invio = esegui  |  Shift+Invio = a capo */
    struct EnterFilter : public QObject {
        QPushButton* btn;
        EnterFilter(QPushButton* b, QObject* p) : QObject(p), btn(b) {}
        bool eventFilter(QObject*, QEvent* ev) override {
            if (ev->type() == QEvent::KeyPress) {
                auto* ke = static_cast<QKeyEvent*>(ev);
                const bool enter = ke->key() == Qt::Key_Return
                                || ke->key() == Qt::Key_Enter;
                if (enter && !(ke->modifiers() & Qt::ShiftModifier)) {
                    btn->click();
                    return true;
                }
            }
            return false;
        }
    };
    m_input->installEventFilter(new EnterFilter(m_btnRun, m_input));

    /* Drag & Drop file su m_input */
    m_input->setAcceptDrops(true);
    struct DropFilter : public QObject {
        AgentiPage* page;
        DropFilter(AgentiPage* p, QObject* par) : QObject(par), page(p) {}
        bool eventFilter(QObject* obj, QEvent* ev) override {
            if (ev->type() == QEvent::DragEnter) {
                auto* de = static_cast<QDragEnterEvent*>(ev);
                if (de->mimeData()->hasUrls()) {
                    de->acceptProposedAction();
                    static_cast<QWidget*>(obj)->setStyleSheet(
                        "QTextEdit { border:2px dashed #4ade80; background:#0e2318; }");
                    return true;
                }
            }
            if (ev->type() == QEvent::DragLeave) {
                static_cast<QWidget*>(obj)->setStyleSheet("");
                return true;
            }
            if (ev->type() == QEvent::Drop) {
                auto* de = static_cast<QDropEvent*>(ev);
                static_cast<QWidget*>(obj)->setStyleSheet("");
                if (de->mimeData()->hasUrls()) {
                    de->acceptProposedAction();
                    for (const QUrl& url : de->mimeData()->urls()) {
                        const QString path = url.toLocalFile();
                        if (!path.isEmpty()) { page->loadDroppedFile(path); break; }
                    }
                    return true;
                }
            }
            return QObject::eventFilter(obj, ev);
        }
    };
    m_input->installEventFilter(new DropFilter(this, this));

    connect(btnSymbols, &QPushButton::clicked, this, &AgentiPage::onBtnSymbolsClicked);
    connect(m_btnDoc,   &QPushButton::clicked, this, &AgentiPage::onBtnDocClicked);
    connect(m_btnVoice, &QPushButton::clicked, this, &AgentiPage::onBtnVoiceClicked);

    /* Shift+Tab cicla le modalità del TriModeButton (solo se un figlio ha il focus) */
    auto* scMode = new QShortcut(Qt::Key_Backtab, this);
    scMode->setContext(Qt::WidgetWithChildrenShortcut);
    connect(scMode, &QShortcut::activated, this, &AgentiPage::onCycleModeShortcut);
}

/* ──────────────────────────────────────────────────────────────
   buildSymbolsPanel — pannello inline caratteri speciali (toggle)
   ────────────────────────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════
   buildMathPanel — pannello formule LaTeX con preview KaTeX in tempo reale
   + pulsanti template per frazioni, limiti, integrali, ecc.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::buildMathPanel(QVBoxLayout* lay)
{
    m_mathPanel = new QWidget(this);
    m_mathPanel->setVisible(false);
    auto* mpLay = new QVBoxLayout(m_mathPanel);
    mpLay->setContentsMargins(0, 2, 0, 4);
    mpLay->setSpacing(4);

    /* ── Intestazione con istruzione ── */
    auto* hint = new QLabel(
        tr("<small><b>Preview LaTeX</b> in tempo reale — scrivi <code>\\frac{a}{b}</code>, "
           "<code>1/3</code>, <code>limite</code>, <code>sommatoria</code>, <code>integrale</code>... "
           "I pulsanti inseriscono template nel campo di testo.</small>"),
        m_mathPanel);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#94a3b8;padding:2px 4px;");
    mpLay->addWidget(hint);

    /* ── Preview KaTeX ── */
    auto* preview = new LatexView(m_mathPanel);
    preview->setMinimumHeight(dpiScale(90));
    preview->setMaximumHeight(dpiScale(130));
    preview->setObjectName("mathPreview");
    /* Messaggio iniziale */
    preview->setLatexHtml(
        "<p style='color:#475569;font-size:12px;padding:8px'>"
        "Inizia a scrivere una formula nel campo di testo sopra...</p>");
    mpLay->addWidget(preview);

    /* ── Debounce timer per aggiornare la preview ── */
    m_mathPreviewTimer = new QTimer(this);
    m_mathPreviewTimer->setSingleShot(true);
    m_mathPreviewTimer->setInterval(500);

    /* Salva puntatore alla preview tramite property del timer */
    m_mathPreviewTimer->setProperty("previewObj",
        QVariant::fromValue(static_cast<QObject*>(preview)));

    connect(m_mathPreviewTimer, &QTimer::timeout,
            this, &AgentiPage::updateMathPreview);
    connect(m_input, &QTextEdit::textChanged,
            m_mathPreviewTimer, qOverload<>(&QTimer::start));

    /* ── Template matematici ── */
    static const struct { const char* label; const char* latex; const char* tip; } kTpl[] = {
        /* Frazioni e radici */
        { "\xe2\x80\x8b\xc2\xaf\xe2\x95\xb1\xe2\x80\x8b",
          "\\frac{}{}", "Frazione \\frac{num}{den}" },
        { "\xe2\x88\x9a",  "\\sqrt{}", "Radice quadrata" },
        { "\xe2\x81\xbf\xe2\x88\x9a", "\\sqrt[n]{}", "Radice n-esima" },
        { "x\xe2\x81\xbf",  "x^{}", "Potenza / apice" },
        { "x\xe2\x82\x99",  "x_{}", "Pedice" },
        { "x\xe2\x81\xbf\xe2\x82\x99", "x^{}_{}", "Apice + pedice" },
        /* Operatori */
        { "lim",           "\\lim_{x \\to }", "Limite" },
        { "\xe2\x88\x91",  "\\sum_{i=0}^{n}", "Sommatoria Σ" },
        { "\xe2\x88\x8f",  "\\prod_{i=1}^{n}", "Prodotto Π" },
        { "\xe2\x88\xab",  "\\int_{a}^{b}", "Integrale definito" },
        { "\xe2\x88\xac",  "\\oint_{C}", "Integrale di linea" },
        { "\xe2\x88\xac\xe2\x88\xac", "\\iint_{S}", "Integrale doppio" },
        /* Funzioni */
        { "sin",  "\\sin()", "Seno" },
        { "cos",  "\\cos()", "Coseno" },
        { "ln",   "\\ln()", "Logaritmo naturale" },
        { "log",  "\\log_{}", "Logaritmo in base b" },
        /* Simboli */
        { "\xe2\x88\x9e",  "\\infty", "Infinito" },
        { "\xcf\x80",      "\\pi", "Pi greco" },
        { "\xcf\x95",      "\\phi", "Phi (numero aureo)" },
        { "\xc2\xb1",      "\\pm", "Piu/meno" },
        { "\xe2\x88\x82",  "\\partial", "Derivata parziale" },
        { "\xe2\x88\x87",  "\\nabla", "Nabla (gradiente)" },
        { "\xe2\x82\x91",  "e^{}", "Esponenziale" },
        { "\xe2\x86\x92",  "\\to", "Freccia destra (tende a)" },
        /* Strutture */
        { "\xe2\x8c\xab",  "\\langle \\rangle", "Angoli ⟨x⟩" },
        { "|x|",           "\\left| \\right|", "Valore assoluto" },
        { "\xe2\x80\x96",  "\\left\\| \\right\\|", "Norma ||x||" },
        { "\\vec",         "\\vec{}", "Vettore con freccia" },
        { "\xe2\x80\xbe",  "\\overline{}", "Media / coniugato" },
        { "\xce\x94",      "\\Delta", "Delta (variazione)" },
        { "\xce\xa3",      "\\Sigma", "Sigma maiuscola" },
        { "\xce\xa9",      "\\Omega", "Omega" },
    };
    constexpr int N_TPL = static_cast<int>(sizeof(kTpl) / sizeof(kTpl[0]));

    auto* tplWidget = new QWidget(m_mathPanel);
    auto* tplFlow   = new QHBoxLayout(tplWidget);
    tplFlow->setContentsMargins(0, 0, 0, 0);
    tplFlow->setSpacing(3);

    /* Raggruppiamo in 4 file da 8 usando un FlowLayout simulato con QGridLayout */
    auto* tplGrid   = new QGridLayout;
    tplGrid->setSpacing(2);
    tplGrid->setContentsMargins(0, 0, 0, 0);
    const int COLS = 8;
    for (int i = 0; i < N_TPL; ++i) {
        auto* b = new QPushButton(QString::fromUtf8(kTpl[i].label), tplWidget);
        b->setObjectName("symbolBtn");
        b->setFixedSize(dpiScale(46), dpiScale(26));
        b->setToolTip(QString::fromUtf8(kTpl[i].tip));
        /* Salva il LaTeX template come property */
        b->setProperty("mathTpl", QString::fromUtf8(kTpl[i].latex));
        connect(b, &QPushButton::clicked, this, [this, b](){
            if (!m_input) return;
            const QString tpl = b->property("mathTpl").toString();
            /* Inserisce il template e posiziona il cursore nel primo {} */
            QTextCursor cur = m_input->textCursor();
            cur.insertText(tpl);
            /* Sposta cursore all'interno del primo {} se presente */
            const int open = tpl.indexOf('{');
            if (open >= 0) {
                int newPos = cur.position() - tpl.size() + open + 1;
                cur.setPosition(newPos);
                m_input->setTextCursor(cur);
            }
            m_input->setFocus();
        });
        tplGrid->addWidget(b, i / COLS, i % COLS);
    }
    tplFlow->addLayout(tplGrid);
    tplFlow->addStretch();
    mpLay->addWidget(tplWidget);

    /* ── EventFilter: auto-replace abbreviazioni italiane → LaTeX ── */
    struct MathFilter : public QObject {
        QTextEdit* ed;
        MathFilter(QTextEdit* e, QObject* p) : QObject(p), ed(e) {}
        bool eventFilter(QObject*, QEvent* ev) override {
            if (ev->type() != QEvent::KeyPress) return false;
            auto* ke = static_cast<QKeyEvent*>(ev);
            if (ke->key() != Qt::Key_Space && ke->key() != Qt::Key_Return
                && ke->key() != Qt::Key_Enter) return false;

            static const QHash<QString, QString> kWords = {
                {"limite",     "\\lim_{x \\to }"},
                {"limx",       "\\lim_{x \\to 0}"},
                {"sommatoria", "\\sum_{i=0}^{n}"},
                {"integrale",  "\\int_{a}^{b}"},
                {"radice",     "\\sqrt{}"},
                {"infinito",   "\\infty "},
                {"delta",      "\\Delta "},
                {"nabla",      "\\nabla "},
                {"vettore",    "\\vec{}"},
                {"derivata",   "\\frac{d}{dx}"},
                {"derivataparziale", "\\frac{\\partial}{\\partial x}"},
                {"taylor",     "\\sum_{n=0}^{\\infty} \\frac{f^{(n)}(a)}{n!}(x-a)^n"},
                {"pigreco",    "\\pi "},
                {"epsilon",    "\\varepsilon "},
                {"sigma",      "\\sigma "},
                {"lambda",     "\\lambda "},
                {"alpha",      "\\alpha "},
                {"beta",       "\\beta "},
                {"theta",      "\\theta "},
                {"omega",      "\\omega "},
                {"phi",        "\\phi "},
                {"psi",        "\\psi "},
                {"gamma",      "\\gamma "},
            };

            QTextCursor cur = ed->textCursor();
            cur.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);
            const QString word = cur.selectedText().trimmed().toLower()
                .remove(' ').remove('_');

            /* Auto-replace keyword italiane */
            if (kWords.contains(word)) {
                cur.insertText(kWords.value(word));
                return false;  /* lascia propagare spazio/invio */
            }

            /* Auto-replace frazioni N/D con numeri interi (es: 1/3) */
            QTextCursor lineCur = ed->textCursor();
            lineCur.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
            const QString lineText = lineCur.selectedText();
            static const QRegularExpression reFrac(R"((\d+)/(\d+))");
            const auto m = reFrac.match(lineText, 0,
                QRegularExpression::PartialPreferCompleteMatch);
            if (m.hasMatch() && m.capturedStart() + m.capturedLength() == lineText.size()) {
                /* Seleziona la frazione e sostituisce con \frac{N}{D} */
                QTextCursor fc = ed->textCursor();
                fc.movePosition(QTextCursor::StartOfLine);
                fc.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                m.capturedStart());
                fc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                                m.capturedLength());
                fc.insertText("\\frac{" + m.captured(1) + "}{" + m.captured(2) + "}");
            }
            return false;
        }
    };
    m_input->installEventFilter(new MathFilter(m_input, m_input));

    lay->addWidget(m_mathPanel);
}

/* ── Aggiorna la preview KaTeX con il testo corrente del campo ── */
void AgentiPage::updateMathPreview()
{
    if (!m_mathPreviewTimer || !m_mathPanel || !m_mathPanel->isVisible() || !m_input)
        return;

    auto* preview = qobject_cast<LatexView*>(
        m_mathPreviewTimer->property("previewObj").value<QObject*>());
    if (!preview) return;

    const QString raw = m_input->toPlainText().trimmed();
    if (raw.isEmpty()) {
        preview->setLatexHtml(
            "<p style='color:#475569;font-size:12px;padding:8px'>"
            "Inizia a scrivere una formula nel campo di testo sopra...</p>");
        return;
    }

    /* Conversione testo naturale → LaTeX per la preview */
    QString ltx = raw;
    /* 1. Frazioni N/D non ancora convertite */
    static const QRegularExpression reFrac2(R"((?<![\\{])\b(\d+)/(\d+)\b)");
    ltx.replace(reFrac2, "\\\\frac{\\1}{\\2}");
    /* 2. Abbreviazioni italiane — solo se non già in LaTeX */
    if (!ltx.contains('\\')) {
        ltx.replace(QRegularExpression(R"(\blimite\b)", QRegularExpression::CaseInsensitiveOption),
                    "\\lim");
        ltx.replace(QRegularExpression(R"(\bsommatoria\b)", QRegularExpression::CaseInsensitiveOption),
                    "\\sum");
        ltx.replace(QRegularExpression(R"(\bintegrale\b)", QRegularExpression::CaseInsensitiveOption),
                    "\\int");
        ltx.replace(QRegularExpression(R"(\bradice\b)", QRegularExpression::CaseInsensitiveOption),
                    "\\sqrt");
        ltx.replace(QRegularExpression(R"(\binfinito\b)", QRegularExpression::CaseInsensitiveOption),
                    "\\infty");
    }

    /* Wrap in display math se il testo non ha già delimitatori */
    const bool hasDelim = ltx.contains("\\[") || ltx.contains("\\(")
                       || ltx.contains("$$") || ltx.startsWith("$");
    const QString html = hasDelim
        ? "<div>" + ltx.toHtmlEscaped() + "</div>"
        : "<p>\\[" + ltx.toHtmlEscaped() + "\\]</p>";

    preview->setLatexHtml(html);
}

void AgentiPage::buildSymbolsPanel(QVBoxLayout* lay, QPushButton* /*btnSymbols*/)
{
    static const struct { const char* cat; const char* chars; int btnW; } kGroups[] = {
        { "Funzioni matematiche",
          "sin( cos( tan( cot( sec( csc( "
          "arcsin( arccos( arctan( arccot( "
          "sinh( cosh( tanh( coth( "
          "log( ln( log\xe2\x82\x82( log\xe2\x82\x81\xe2\x82\x80( "
          "lim exp( "
          "\xe2\x88\x9a( \xe2\x88\x9b( \xe2\x88\x9c( "
          "max( min( abs( floor( ceil( gcd( lcm( mod",
          46 },
        { "LaTeX \xe2\x80\x94 Funzioni",
          "\\sin \\cos \\tan \\cot \\sec \\csc "
          "\\arcsin \\arccos \\arctan "
          "\\sinh \\cosh \\tanh "
          "\\log \\ln \\lg \\exp \\lim \\max \\min \\sup \\inf "
          "\\gcd \\lcm \\mod \\deg",
          50 },
        { "LaTeX \xe2\x80\x94 Operatori",
          "\\int \\iint \\iiint \\oint "
          "\\sum \\prod \\coprod "
          "\\frac{}{} \\sqrt{} \\sqrt[]{} "
          "\\partial \\nabla \\Delta "
          "\\pm \\mp \\times \\div \\cdot "
          "\\leq \\geq \\neq \\approx \\equiv \\sim "
          "\\in \\notin \\subset \\supset \\subseteq \\supseteq "
          "\\forall \\exists \\infty \\emptyset",
          54 },
        { "LaTeX \xe2\x80\x94 Lettere greche",
          "\\alpha \\beta \\gamma \\delta \\epsilon \\varepsilon "
          "\\zeta \\eta \\theta \\vartheta \\iota \\kappa "
          "\\lambda \\mu \\nu \\xi \\pi \\varpi \\rho \\varrho "
          "\\sigma \\varsigma \\tau \\upsilon \\phi \\varphi \\chi \\psi \\omega "
          "\\Gamma \\Delta \\Theta \\Lambda \\Xi \\Pi \\Sigma \\Upsilon \\Phi \\Psi \\Omega",
          50 },
        { "Matematica",
          "\xe2\x88\x91 \xe2\x88\xab \xe2\x88\x8f \xe2\x88\x9a \xe2\x88\x9e "
          "\xe2\x88\x82 \xcf\x80 \xc2\xb1 \xc3\x97 \xc3\xb7 "
          "\xe2\x89\xa0 \xe2\x89\xa4 \xe2\x89\xa5 \xe2\x89\x88 \xe2\x89\xa1 "
          "\xe2\x88\x88 \xe2\x88\x89 \xe2\x8a\x82 \xe2\x8a\x83 \xe2\x88\x80 "
          "\xe2\x88\x83 \xe2\x88\x87 \xe2\x84\x9d \xe2\x84\xa4 \xe2\x84\x95 \xe2\x84\x82",
          32 },
        { "Greco",
          "\xce\xb1 \xce\xb2 \xce\xb3 \xce\xb4 \xce\xb5 \xce\xb6 \xce\xb7 \xce\xb8 "
          "\xce\xbb \xce\xbc \xce\xbd \xce\xbe \xcf\x81 \xcf\x83 \xcf\x84 "
          "\xcf\x86 \xcf\x87 \xcf\x88 \xcf\x89 "
          "\xce\x94 \xce\x9b \xce\xa3 \xce\xa8 \xce\xa9 \xce\x93 \xce\xa0 \xce\xa6 \xce\x98",
          32 },
        { "Potenze / Indici",
          "\xc2\xb2 \xc2\xb3 \xc2\xb9 \xe2\x81\xb0 \xe2\x81\xb4 \xe2\x81\xb5 \xe2\x81\xb6 \xe2\x81\xb7 \xe2\x81\xb8 \xe2\x81\xb9 "
          "\xe2\x82\x80 \xe2\x82\x81 \xe2\x82\x82 \xe2\x82\x83 \xe2\x82\x84 \xe2\x82\x85 \xe2\x82\x86 \xe2\x82\x87 \xe2\x82\x88 \xe2\x82\x89 "
          "\xc2\xbd \xe2\x85\x93 \xe2\x85\x94 \xc2\xbc \xc2\xbe",
          32 },
        { "Lingue / Accenti",
          "\xc3\xa9 \xc3\xa8 \xc3\xaa \xc3\xab "
          "\xc3\xa0 \xc3\xa2 \xc3\xa4 "
          "\xc3\xb9 \xc3\xbb \xc3\xbc "
          "\xc3\xb4 \xc3\xb6 \xc3\xb1 \xc3\xa7 \xc3\x9f "
          "\xc3\xa6 \xc3\xb8 \xc3\xa5 "
          "\xc3\xac \xc3\xae \xc3\xaf \xc3\xb3 \xc3\xb2",
          32 },
        { "Frecce",
          "\xe2\x86\x92 \xe2\x86\x90 \xe2\x86\x91 \xe2\x86\x93 \xe2\x86\x94 \xe2\x86\x95 "
          "\xe2\x87\x92 \xe2\x87\x90 \xe2\x87\x91 \xe2\x87\x93 \xe2\x87\x94 "
          "\xe2\x86\x97 \xe2\x86\x98 \xe2\x86\x99 \xe2\x86\x96 \xe2\x86\xba \xe2\x86\xbb "
          "\xe2\x9e\x9c \xe2\x9e\x9d \xe2\x9e\x9e \xe2\x9e\xa1 "
          "\xe2\x9f\xb5 \xe2\x9f\xb6 \xe2\x9f\xb7 \xe2\x9f\xb9",
          32 },
        { "Valute",
          "\xe2\x82\xac \xc2\xa3 $ \xc2\xa5 \xc2\xa2 "
          "\xe2\x82\xbf \xe2\x82\xbd \xe2\x82\xa9 \xe2\x82\xaa \xe2\x82\xab "
          "\xe2\x82\xb4 \xe2\x82\xa6 \xe2\x82\xb1 \xe2\x82\xad \xe2\x82\xae "
          "\xe2\x82\xba \xe2\x82\xbc \xe2\x82\xbe",
          32 },
        { "Tipografia",
          "\xc2\xa9 \xc2\xae \xe2\x84\xa2 \xc2\xb0 \xc2\xa7 \xc2\xb6 "
          "\xe2\x80\xa0 \xe2\x80\xa1 \xe2\x80\xbb \xe2\x80\xb0 "
          "\xe2\x80\xa6 \xe2\x80\x94 \xe2\x80\x93 \xc2\xb7 \xe2\x80\xa2 \xe2\x80\xa3 "
          "\xe2\x80\xb3 \xe2\x80\xb2 "
          "\xc2\xab \xc2\xbb \xe2\x80\x9e \xe2\x80\x9c \xe2\x80\x9d \xe2\x80\xb9 \xe2\x80\xba",
          32 },
        { "Forme / Simboli",
          "\xe2\x97\x8f \xe2\x97\x8b \xe2\x96\xa0 \xe2\x96\xa1 "
          "\xe2\x96\xb2 \xe2\x96\xb3 \xe2\x96\xbc \xe2\x96\xbd "
          "\xe2\x97\x86 \xe2\x97\x87 \xe2\x98\x85 \xe2\x98\x86 "
          "\xe2\x99\xa6 \xe2\x99\xa0 \xe2\x99\xa3 \xe2\x99\xa5 \xe2\x99\xa1 "
          "\xe2\x9c\x93 \xe2\x9c\x97 \xe2\x9c\x94 \xe2\x9c\x98 "
          "\xe2\x98\x91 \xe2\x98\x90 \xe2\x98\x92",
          32 },
    };

    m_symbolsPanel = new QFrame;
    m_symbolsPanel->setObjectName("actionCard");
    m_symbolsPanel->setFrameShape(QFrame::StyledPanel);
    auto* panGrid = new QGridLayout(m_symbolsPanel);
    panGrid->setContentsMargins(6, 4, 6, 4);
    panGrid->setHorizontalSpacing(8);
    panGrid->setVerticalSpacing(3);
    panGrid->setColumnMinimumWidth(0, dpiScale(118));
    panGrid->setColumnStretch(1, 1);

    const int BTN_H    = dpiScale(22);
    constexpr int kViewportW = 760;

    int gridRow = 0;
    for (auto& g : kGroups) {
        buildSymbolCategoryRow(panGrid, gridRow, g.cat, g.chars, g.btnW, BTN_H, kViewportW);
        ++gridRow;
    }

    /* ── Campo ricerca simboli (visibile sopra le categorie) ── */
    m_symbolSearch = new QLineEdit(this);
    m_symbolSearch->setPlaceholderText(
        tr("\xf0\x9f\x94\x8d  Cerca simbolo... "
           "(es: alpha, freccia, euro, integral, sqrt, copyright)"));
    m_symbolSearch->setClearButtonEnabled(true);
    m_symbolSearch->setStyleSheet(
        "QLineEdit{background:#1e293b;border:1px solid #334155;"
        "border-radius:5px;padding:5px 10px;color:#e2e8f0;font-size:12px;}"
        "QLineEdit:focus{border-color:#60a5fa;}");
    m_symbolSearch->setVisible(false);
    lay->addWidget(m_symbolSearch);

    /* ── Pannello risultati ricerca ── */
    m_symbolSearchPanel = new QWidget(this);
    m_symbolSearchGrid  = new QGridLayout(m_symbolSearchPanel);
    m_symbolSearchGrid->setContentsMargins(4, 4, 4, 4);
    m_symbolSearchGrid->setSpacing(2);
    auto* searchScroll = new QScrollArea(this);
    searchScroll->setObjectName("symbolSearchScroll");
    searchScroll->setWidget(m_symbolSearchPanel);
    searchScroll->setWidgetResizable(true);
    searchScroll->setMaximumHeight(dpiScale(200));
    searchScroll->setFrameShape(QFrame::NoFrame);
    searchScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    searchScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    searchScroll->setVisible(false);
    lay->addWidget(searchScroll);

    /* ── Pannello categorie (nascosto durante ricerca) ── */
    m_symbolsScrollArea = new QScrollArea(this);
    m_symbolsScrollArea->setWidget(m_symbolsPanel);
    m_symbolsScrollArea->setWidgetResizable(true);
    m_symbolsScrollArea->setMaximumHeight(dpiScale(210));
    m_symbolsScrollArea->setFrameShape(QFrame::NoFrame);
    m_symbolsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_symbolsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_symbolsScrollArea->setVisible(false);
    lay->addWidget(m_symbolsScrollArea);

    /* Collega ricerca; usa property per portare il riferimento a searchScroll */
    m_symbolSearch->setProperty("resultsScroll",
        QVariant::fromValue(static_cast<QObject*>(searchScroll)));
    connect(m_symbolSearch, &QLineEdit::textChanged,
            this, &AgentiPage::onSymbolSearchChanged);
}

/* ── Una riga categoria nel pannello simboli ── */
void AgentiPage::buildSymbolCategoryRow(QGridLayout* panGrid, int gridRow,
                                        const char* cat, const char* chars,
                                        int btnW, int btnH, int viewportW)
{
    auto* catLbl = new QLabel(QString::fromUtf8(cat), m_symbolsPanel);
    catLbl->setStyleSheet(
        "font-size:10px; color:#99aacc; padding:2px 6px;"
        "background:#1e1e2a; border-radius:3px; border:1px solid #2a2a44;");
    catLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    catLbl->setWordWrap(true);
    catLbl->setFixedWidth(dpiScale(118));
    panGrid->addWidget(catLbl, gridRow, 0, Qt::AlignTop | Qt::AlignRight);

    auto* btnArea = new QWidget(m_symbolsPanel);
    auto* btnGrid = new QGridLayout(btnArea);
    btnGrid->setContentsMargins(0, 0, 0, 0);
    btnGrid->setSpacing(1);

    QStringList tokens = QString::fromUtf8(chars).split(' ', Qt::SkipEmptyParts);

    QFontMetrics fm(m_symbolsPanel->font());
    int fixedW = btnW;
    for (const QString& ch : tokens)
        fixedW = std::max(fixedW, fm.horizontalAdvance(ch) + 14);

    const int perRow = std::max(4, viewportW / (fixedW + 1));

    /* Nomi aggiuntivi per ricerca (simbolo → keyword extra in italiano/inglese) */
    static const QHash<QString,QString> kExtraNames = {
        {"∑","somma sum sigma"},    {"∫","integrale integral"},
        {"∂","derivata parziale partial"}, {"∏","prodotto product"},
        {"√","radice quadrata square root"}, {"∞","infinito infinity"},
        {"π","pigreco pi"}, {"±","piu meno plus minus"},
        {"×","per times moltiplicazione multiply"}, {"÷","diviso divide"},
        {"≠","diverso different not equal"}, {"≤","minore uguale less equal"},
        {"≥","maggiore uguale greater equal"}, {"≈","circa approximately"},
        {"∈","appartiene belongs in"}, {"∉","non appartiene not in"},
        {"⊂","sottoinsieme subset"}, {"∀","per ogni for all"},
        {"∃","esiste exists"}, {"∇","nabla gradiente gradient"},
        {"→","freccia destra arrow right"}, {"←","freccia sinistra arrow left"},
        {"↑","freccia su arrow up"}, {"↓","freccia giu arrow down"},
        {"↔","freccia doppia double arrow"}, {"⇒","implica implies"},
        {"α","alpha alfa"}, {"β","beta"},  {"γ","gamma"},
        {"δ","delta"},      {"ε","epsilon"}, {"θ","theta"},
        {"λ","lambda"},     {"μ","mu"},    {"ν","nu"},
        {"π","pi"},         {"σ","sigma"}, {"τ","tau"},
        {"φ","phi"},        {"ω","omega"}, {"Δ","Delta maiuscola"},
        {"Σ","Sigma maiuscola"}, {"Ω","Omega maiuscola"},
        {"€","euro"},       {"£","sterlina pound"}, {"¥","yen"},
        {"₿","bitcoin"},    {"₽","rublo ruble"},
        {"©","copyright"},  {"®","registered marchio"},
        {"™","trademark"},  {"°","gradi degree"},
        {"²","quadrato square esponente"}, {"³","cubo cube"},
        {"½","mezzo half"}, {"¼","quarto quarter"},
        {"…","puntini ellipsis"}, {"—","trattino dash"},
        {"«","virgolette guillemets"}, {"»","virgolette guillemets"},
    };
    const QString catStr = QString::fromUtf8(cat).toLower();

    int bCol = 0, bRow = 0;
    for (const QString& ch : tokens) {
        if (ch.isEmpty()) continue;
        if (bCol >= perRow) { bCol = 0; ++bRow; }
        auto* b = new QPushButton(ch, btnArea);
        b->setObjectName("symbolBtn");
        b->setFixedSize(fixedW, btnH);
        b->setProperty("symbol", ch);
        connect(b, &QPushButton::clicked, this, &AgentiPage::onSymbolBtnClicked);
        btnGrid->addWidget(b, bRow, bCol);
        ++bCol;
        /* Popola m_allSymbols: chiave = simbolo + categoria + nome extra */
        QString key = ch.toLower() + " " + catStr;
        auto it = kExtraNames.constFind(ch);
        if (it != kExtraNames.constEnd()) key += " " + it.value();
        m_allSymbols.append({ch, key});
    }

    panGrid->addWidget(btnArea, gridRow, 1);
}

/* ──────────────────────────────────────────────────────────────
   buildExtraConnections — collega segnali non legati a singoli widget:
   cfgDlg, cmbMode (preset + math), AI abort
   ────────────────────────────────────────────────────────────── */
void AgentiPage::buildExtraConnections()
{
    connect(m_cfgDlg->numAgentsSpinBox(), QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AgentiPage::onNumAgentsChanged);

    connect(m_cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentiPage::onCmbModePresetChanged);

    connect(m_cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentiPage::onCmbModeMathChanged);

    connect(m_ai, &AiClient::aborted, this, &AgentiPage::onAiAborted);
}

void AgentiPage::_setRunBusy(bool busy)
{
    if (busy) {
        m_btnRun->setText(tr("\xe2\x8f\xb9 Stop"));
        m_btnRun->setProperty("danger", true);
    } else {
        if (m_autoEnabled)
            m_btnRun->setText(tr("\xf0\x9f\xa4\x96  Avvia Agente"));
        else if (m_modePipeline)
            m_btnRun->setText(tr("\xe2\x96\xb6  Avvia"));
        else
            m_btnRun->setText(tr("\xf0\x9f\x93\xa4 Invia"));
        m_btnRun->setProperty("danger", false);
    }
    m_btnRun->setEnabled(true);
    P::repolish(m_btnRun);
    if (m_modeBtn) {
        m_modeBtn->setActionText(m_btnRun->text());
        m_modeBtn->setActionEnabled(true);
        m_modeBtn->setActionDanger(busy);
    }
}

/* ══════════════════════════════════════════════════════════════
   Slot nominati — agenti_page_ui.cpp
   ══════════════════════════════════════════════════════════════ */

void AgentiPage::onTtsStopClicked()
{
    /* Ferma piper prima di aplay (evita dati residui in pipe) */
    if (m_piperProc) {
        m_piperProc->kill();
        m_piperProc->waitForFinished(300);
        m_piperProc->deleteLater();
        m_piperProc = nullptr;
    }
    if (m_ttsProc) { m_ttsProc->kill(); m_ttsProc->waitForFinished(300); }
#ifndef Q_OS_WIN
    QProcess::startDetached("pkill", {"-9", "aplay"});
    QProcess::startDetached("pkill", {"-9", "piper"});
#endif
    m_ttsPaused = false;
    if (m_btnTtsPause) { m_btnTtsPause->setText(tr("\xe2\x8f\xb8  Pausa")); m_btnTtsPause->setVisible(false); }
    m_btnTtsStop->setVisible(false);
}

void AgentiPage::onTtsPauseClicked()
{
#ifndef Q_OS_WIN
    auto sendSig = [](QProcess* p, int sig){
        if (p && p->state() == QProcess::Running && p->processId() > 0)
            ::kill(static_cast<pid_t>(p->processId()), sig);
    };
    if (!m_ttsPaused) {
        sendSig(m_ttsProc,   SIGSTOP);
        sendSig(m_piperProc, SIGSTOP);
        m_ttsPaused = true;
        m_btnTtsPause->setText(tr("\xe2\x96\xb6  Riprendi"));
    } else {
        sendSig(m_ttsProc,   SIGCONT);
        sendSig(m_piperProc, SIGCONT);
        m_ttsPaused = false;
        m_btnTtsPause->setText(tr("\xe2\x8f\xb8  Pausa"));
    }
#else
    /* Windows: pausa non supportata, simula stop */
    if (m_ttsProc)   { m_ttsProc->kill(); m_ttsProc->waitForFinished(300); }
    if (m_piperProc) { m_piperProc->kill(); m_piperProc->waitForFinished(300); }
    if (m_btnTtsPause) m_btnTtsPause->setVisible(false);
    if (m_btnTtsStop)  m_btnTtsStop->setVisible(false);
#endif
}

void AgentiPage::onBtnExportClicked()
{
    if (!m_log || m_log->toPlainText().trimmed().isEmpty()) return;
    const QString ts   = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString name = QString("prismalux_chat_%1.md").arg(ts);
    const QString path = QFileDialog::getSaveFileName(
        this, "Esporta conversazione", QDir::homePath() + "/" + name,
        "Markdown (*.md);;HTML (*.html);;Testo (*.txt)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    QTextStream out(&f);

    if (path.endsWith(".html", Qt::CaseInsensitive)) {
        out << m_log->toHtml();
    } else if (path.endsWith(".txt", Qt::CaseInsensitive)) {
        out << m_log->toPlainText();
    } else {
        QTextDocument doc;
        doc.setHtml(m_log->toHtml());
        out << "# Prismalux \xe2\x80\x94 Conversazione\n";
        out << "_Esportata il " << QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm") << "_\n\n";
        out << "---\n\n";
        out << doc.toMarkdown();
    }
}

void AgentiPage::onBtnExportPdfClicked()
{
    if (!m_log || m_log->toPlainText().trimmed().isEmpty()) return;
    const QString ts   = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString name = QString("prismalux_chat_%1.pdf").arg(ts);
    const QString path = QFileDialog::getSaveFileName(
        this, "Esporta come PDF", QDir::homePath() + "/" + name,
        "PDF (*.pdf)");
    if (path.isEmpty()) return;
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize::A4);
    m_log->document()->print(&printer);
}

void AgentiPage::onBtnInfoClicked()
{
    if (!m_hintWidget) return;
    const bool now = !m_hintWidget->isVisible();
    m_hintWidget->setVisible(now);
    AppConfig::s().setValue("ui/hintVisible", now);
}

void AgentiPage::onVoiceLoopToggled(bool on)
{
    static const char* kVoiceOff =
        "QPushButton{"
          "background:#1e2d45;border:2px solid #334155;color:#64748b;"
          "border-radius:14px;padding:4px 12px;font-weight:bold;font-size:12px;}"
        "QPushButton:hover{background:#243650;color:#94a3b8;}";
    static const char* kVoiceOn =
        "QPushButton{"
          "background:#7f1d1d20;border:2px solid #ef4444;color:#ef4444;"
          "border-radius:14px;padding:4px 12px;font-weight:bold;font-size:12px;}"
        "QPushButton:hover{background:#7f1d1d35;}";

    m_voiceLoopActive = on;
    if (m_btnVoiceLoop) {
        m_btnVoiceLoop->setStyleSheet(on ? kVoiceOn : kVoiceOff);
        if (on) {
            m_btnVoiceLoop->setText(tr("\xf0\x9f\x94\xb4  In ascolto..."));
        } else {
            const QString pName = P::personalityName();
            m_btnVoiceLoop->setText(pName.isEmpty()
                ? "\xf0\x9f\x8e\x99  Conversa"
                : "\xf0\x9f\x8e\x99  Conversa con " + pName);
        }
    }

    if (on) {
        if (SttWhisper::whisperBin().isEmpty()) {
            m_log->append(
                "<p style='color:#e2e8f0;'>"
                "\xe2\x9a\xa0  <b>whisper-cli non trovato.</b> "
                "Clicca <a href=\"settings:trascrivi\" style=\"color:#93c5fd;\">"
                "Impostazioni \xe2\x86\x92 Trascrivi</a> per installarlo."
                "</p>");
            m_voiceLoopActive = false;
            if (m_btnVoiceLoop) m_btnVoiceLoop->setChecked(false);
            return;
        }
        if (SttWhisper::whisperModel().isEmpty()) {
            downloadWhisperModel();
            m_voiceLoopActive = false;
            if (m_btnVoiceLoop) m_btnVoiceLoop->setChecked(false);
            return;
        }
        if (m_sttState == SttState::Idle)
            _sttStartRecording();
    } else {
        /* Ferma TTS se in lettura (local ptr + null prima di wait → evita double-free) */
        if (m_piperProc) {
            QProcess* p = m_piperProc;
            m_piperProc = nullptr;
            p->kill();
            p->waitForFinished(300);
            p->deleteLater();
        }
        if (m_ttsProc) {
            QProcess* p = m_ttsProc;
            m_ttsProc = nullptr;
            p->kill();
            p->waitForFinished(300);
            p->deleteLater();
        }
        if (m_btnTtsStop)  m_btnTtsStop->setVisible(false);
        if (m_btnTtsPause) m_btnTtsPause->setVisible(false);
        /* Ferma tick countdown */
        if (m_sttTick) { m_sttTick->stop(); m_sttTick->deleteLater(); m_sttTick = nullptr; }
        /* Ferma registrazione se attiva.
           IMPORTANTE: azzerare m_sttState e m_recProc PRIMA di kill()+waitForFinished(),
           altrimenti il segnale finished() → onRecProcFinished → onSttTimeout può fare
           un secondo deleteLater() sullo stesso puntatore (double-free → SIGSEGV). */
        if (m_sttState == SttState::Recording) {
            m_sttState = SttState::Idle;
            if (m_recProc) {
                QProcess* p = m_recProc;
                m_recProc = nullptr;
                p->kill();
                p->waitForFinished(300);
                p->deleteLater();
            }
            m_btnVoice->setText(tr("\xf0\x9f\x8e\xa4 Trascrivi parlato"));
            m_btnVoice->setProperty("danger","false");
            P::repolish(m_btnVoice);
            m_btnVoice->setEnabled(true);
        }
    }
}

void AgentiPage::onToolChkToggled(bool on)
{
    if (!m_autoEnabled)   /* In Agente Autonomo, m_toolsEnabled resta true */
        m_toolsEnabled = on;
}

/* Helper: ritorna true se il modello supporta vision multimodale */
static bool isVisionCapable(const QString& mdl)
{
    const QString m = mdl.toLower();
    return m.contains("vision") || m.contains("-vl") || m.contains("llava")
        || m.contains("minicpm-v") || m.contains("bakllava") || m.contains("cogvlm")
        || m.contains("moondream") || m.contains("idefics") || m.contains("phi-3-v")
        || m.contains("phi3-v") || m.contains("internvl") || m.contains("qwen-vl")
        || m.contains("qwen2-vl") || m.contains("omnivision");
}

/* Helper: ritorna true se il modello supporta function/tool calling */
static bool isToolCapable(const QString& mdl)
{
    const QString m = mdl.toLower();
    if (m.contains("deepseek-coder")) return false;
    if (m.contains("deepseek-r1"))    return false;
    if (m.contains("codellama"))      return false;
    if (m.contains("phi-2"))          return false;
    return true;
}

void AgentiPage::onCmbLLMIndexChanged(int idx)
{
    if (idx < 0 || !m_cmbLLM) return;
    const QString mdl = ModelComboHelper::currentModel(m_cmbLLM);
    if (mdl.isEmpty() || mdl == "(caricamento...)") return;
    m_pageModel = mdl;
    m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), mdl);

    /* ── Capability check: vision — avvisa in tooltip di Allega file ── */
    const bool isDeepSeek = mdl.toLower().contains("deepseek");
    const bool hasVision  = isVisionCapable(mdl);
    if (m_btnDoc) {
        if (isDeepSeek && !hasVision) {
            m_btnDoc->setToolTip(
                tr("Allega file alla chat.\n"
                   "Documenti: .txt .md .csv .json .py .cpp .h .pdf .xls\n"
                   "Immagini: .png .jpg .jpeg .gif .webp\n"
                   "\xe2\x9a\xa0  Attenzione: il modello attuale (DeepSeek) non supporta immagini."));
        } else if (!hasVision) {
            m_btnDoc->setToolTip(
                tr("Allega file alla chat.\n"
                   "Documenti: .txt .md .csv .json .py .cpp .h .pdf .xls\n"
                   "Immagini: .png .jpg .jpeg .gif .webp\n"
                   "\xe2\x84\xb9  Per inviare immagini usa un modello vision (*-vl, llava, gemma3\xe2\x80\xa6)"));
        } else {
            m_btnDoc->setToolTip(
                tr("Allega file alla chat.\n"
                   "Documenti: .txt .md .csv .json .py .cpp .h .pdf .xls\n"
                   "Immagini: .png .jpg .jpeg .gif .webp"));
        }
    }

    /* ── Capability check: tool use ── */
    const bool hasTool = isToolCapable(mdl);
    if (m_toolChk && !m_autoEnabled) {
        m_toolChk->setEnabled(hasTool);
        if (!hasTool) {
            m_toolChk->setChecked(false);
            m_toolChk->setToolTip(tr("%1 non supporta function calling — tools disabilitati").arg(mdl));
        } else {
            m_toolChk->setToolTip(
                "Abilita il function calling (Ollama tool use) nella prossima risposta.\n"
                "Il modello pu\xc3\xb2 chiamare: leggi_file, lista_file, calc, cerca_web, python.\n"
                "Richiede un modello tool-capable (qwen3, llama3.1, mistral-nemo...).\n"
                "In modalit\xc3\xa0 Agente Autonomo i tool sono sempre attivi.");
        }
    }

    /* ── Etichetta avviso combinata ── */
    QStringList warns;
    if (isDeepSeek && !hasVision) warns << "\xf0\x9f\x9b\x87 no vision";  /* 🛇 */
    if (!hasTool)                  warns << "\xf0\x9f\x94\xa7 no tools";  /* 🔧 */
    if (m_modelWarnLbl) {
        if (!warns.isEmpty()) {
            m_modelWarnLbl->setText(warns.join("  "));
            m_modelWarnLbl->setToolTip(
                (isDeepSeek && !hasVision ? tr("Questo modello non supporta le immagini in input.\n") : QString()) +
                (!hasTool ? tr("Questo modello non supporta il function calling.") : QString()));
            m_modelWarnLbl->setVisible(true);
        } else {
            m_modelWarnLbl->setVisible(false);
        }
    }

    /* Mostra il pulsante "Rigenera" solo se la chat ha già contenuto */
    if (m_btnRegen && m_log && !m_log->toPlainText().trimmed().isEmpty()) {
        QString shortMdl = mdl;
        if (shortMdl.length() > 18)
            shortMdl = shortMdl.left(16) + "\xe2\x80\xa6";  /* … */
        m_btnRegen->setText("\xf0\x9f\x94\x84 " + shortMdl);
        m_btnRegen->setVisible(true);
    }
}

/* ─────────────────────────────────────────────────────────────────
   onBtnRegenClicked — reinvia l'ultimo testo utente con il modello
   corrente appena selezionato nel combo.
   Strategia: cerca nell'HTML del log l'ultimo href "retry:N:BASE64URL"
   (inserito da buildUserBubble) e decodifica il testo originale.
   Se non trova il link fallback: non fa nulla (chat vuota o bolle senza idx).
   ───────────────────────────────────────────────────────────────── */
void AgentiPage::onBtnRegenClicked()
{
    if (!m_btnRegen || !m_log) return;
    m_btnRegen->setVisible(false);

    /* Cerca l'ultimo "retry:N:BASE64URL" nell'HTML corrente */
    const QString html = m_log->toHtml();
    static const QRegularExpression reRetry(
        "retry:\\d+:([A-Za-z0-9_-]+)",
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = reRetry.globalMatch(html);
    QString lastB64;
    while (it.hasNext())
        lastB64 = it.next().captured(1);   /* prende l'ultimo match */

    if (lastB64.isEmpty()) return;

    const QString origText = QString::fromUtf8(
        QByteArray::fromBase64(lastB64.toLatin1(), QByteArray::Base64UrlEncoding));
    if (origText.trimmed().isEmpty()) return;

    m_input->setPlainText(origText.trimmed());
    m_input->setFocus();
    m_input->moveCursor(QTextCursor::End);
    QTimer::singleShot(0, this, &AgentiPage::onBtnRunDelayedClick);
}

void AgentiPage::onModeToggleToggled(bool autoOn)
{
    m_autoEnabled   = autoOn;
    m_toolsEnabled  = autoOn || (m_toolChk && m_toolChk->isChecked());
    m_toolIteration = 0;
    m_modePipeline  = false;
    /* In modalità Autonomo: tools sempre attivi, checkbox bloccato spuntato */
    if (m_toolChk) {
        m_toolChk->blockSignals(true);
        if (autoOn) m_toolChk->setChecked(true);
        m_toolChk->setEnabled(!autoOn);
        m_toolChk->blockSignals(false);
    }

    m_btnRun->setText(autoOn
        ? "\xf0\x9f\xa4\x96  Avvia Agente"
        : "\xf0\x9f\x93\xa4 Invia");
    if (m_modeBtn) m_modeBtn->setActionText(m_btnRun->text());
}

/* ── Settore TriModeButton selezionato: 0=Chat  1=Agentico  2=Conversa ── */
void AgentiPage::onCycleModeShortcut()
{
    if (!m_modeBtn || !isVisible()) return;   /* solo quando tab AI è visibile */
    const int next = ((int)m_modeBtn->currentMode() + 1) % 3;
    m_modeBtn->setMode((TriModeButton::Mode)next, true);
}

void AgentiPage::onModeBtnChanged(int mode)
{
    /* 1. Ferma modalità precedente */
    if (m_autoEnabled) onModeToggleToggled(false);

    /* Ferma loop voce / registrazione attiva (con blockSignals per evitare re-entranza) */
    if (m_voiceLoopActive || m_sttState == SttState::Recording) {
        onVoiceLoopToggled(false);
    }
    if (m_btnVoiceLoop && m_btnVoiceLoop->isChecked()) {
        m_btnVoiceLoop->blockSignals(true);
        m_btnVoiceLoop->setChecked(false);
        m_btnVoiceLoop->blockSignals(false);
    }

    /* 2. Ripristina run button di default, poi specializza */
    m_btnRun->setText("\xf0\x9f\x93\xa4 Invia");
    if (m_modeBtn) m_modeBtn->setActionText("\xf0\x9f\x93\xa4 Invia");

    /* 3. Attiva nuova modalità */
    switch (mode) {
    case 0:   /* Chat */
        break;
    case 1:   /* Agentico */
        onModeToggleToggled(true);   /* gestisce anche il testo del run button + hub */
        break;
    case 2:   /* Conversa */
        m_btnRun->setText("\xf0\x9f\x8e\x99  Dialoga");
        if (m_modeBtn) m_modeBtn->setActionText("\xf0\x9f\x8e\x99  Dialoga");
        break;
    }
}

void AgentiPage::onLogScrollValueChanged(int value)
{
    if (m_suppressScrollSig) return;
    m_userScrolled = (value < m_log->verticalScrollBar()->maximum());
}

void AgentiPage::onBtnChartOpenClicked()
{
    emit requestShowInGrafico(m_lastChartExpr, m_lastChartXMin, m_lastChartXMax, m_lastChartPts);
}

void AgentiPage::onLogAnchorClicked(const QUrl& url)
{
    const QString s = url.toString();
    /* formato: "copy:IDX" | "tts:IDX" | "chart:show" | "settings:<tab>" | "fb:up/down:IDX" */
    /* ── Feedback \xf0\x9f\x91\x8d/\xf0\x9f\x91\x8e ── */
    if (s.startsWith("fb:")) {
        const QStringList parts = s.split(':');
        if (parts.size() >= 3) {
            const QString rating = parts[1];   /* "up" o "down" */
            const int     idx    = parts[2].toInt();
            saveFeedback(idx, rating == "up" ? 1 : -1);
        }
        return;
    }
    /* ── Rifai domanda con il modello corrente ── */
    if (s.startsWith("retry:")) {
        /* formato: "retry:IDX:BASE64URL" */
        const int c1r = s.indexOf(':');
        const int c2r = s.indexOf(':', c1r + 1);
        if (c2r > 0) {
            const int    retryIdx = s.mid(c1r + 1, c2r - c1r - 1).toInt();
            const QString b64r    = s.mid(c2r + 1);
            const QString origText = QString::fromUtf8(
                QByteArray::fromBase64(b64r.toLatin1(), QByteArray::Base64UrlEncoding));
            if (!origText.isEmpty()) {
                /* Tronca il log a partire dall'inizio della bolla utente con id='ubbl:N'
                   così la nuova bolla (+ risposta) viene inserita al posto giusto
                   senza duplicati né contenuto orfano. */
                if (m_log) {
                    QString html = m_log->toHtml();
                    const QString anchor = QString("id='ubbl:%1'").arg(retryIdx);
                    const int anchorPos  = html.indexOf(anchor);
                    if (anchorPos > 0) {
                        /* Risali al <table che contiene l'anchor */
                        const int tablePos = html.lastIndexOf("<table", anchorPos);
                        if (tablePos > 0) {
                            html = html.left(tablePos);
                            m_log->setHtml(html);
                            m_log->moveCursor(QTextCursor::End);
                        }
                    }
                }
                m_input->setPlainText(origText.trimmed());
                m_input->setFocus();
                m_input->moveCursor(QTextCursor::End);
                QTimer::singleShot(0, this, &AgentiPage::onBtnRunDelayedClick);
            }
        }
        return;
    }
    if (s.startsWith("settings:")) {
        emit requestOpenSettings(s.mid(9));
        return;
    }
    if (s == "chart:show") {
        if (m_chartPanel) m_chartPanel->setVisible(true);
        return;
    }
    /* ── Elimina messaggio con conferma ── */
    if (s.startsWith("del:")) {
        QMessageBox ask(this);
        ask.setWindowTitle(tr("\xf0\x9f\x97\x91  Elimina messaggio"));
        ask.setText("<b>Eliminare questo messaggio dalla chat?</b>");
        ask.setInformativeText(
            "Questa operazione \xc3\xa8 irreversibile.");
        QPushButton* btnDel = ask.addButton("Elimina", QMessageBox::DestructiveRole);
        ask.addButton("Annulla", QMessageBox::RejectRole);
        ask.setDefaultButton(btnDel);
        ask.exec();
        if (ask.clickedButton() != btnDel) return;

        /* Salva snapshot per undo */
        m_undoHtmlStack.push(m_log->toHtml());

        /* Rimuovi il blocco della bolla dall'HTML usando il del:ID univoco */
        const QString c1s = s.mid(4, s.indexOf(':', 4) - 4); /* estrai ID */
        QString html = m_log->toHtml();
        QRegularExpression re(
            "<table[^>]*>(?:(?!</table>).)*?" +
            QRegularExpression::escape("del:" + c1s + ":") +
            ".*?</table>(?:\\s*<p[^>]*>\\s*</p>)?",
            QRegularExpression::DotMatchesEverythingOption);
        html.remove(re);
        m_log->setHtml(html);
        m_log->moveCursor(QTextCursor::End);
        return;
    }

    /* ── Toggle ragionamento <think> collassabile ── */
    if (s.startsWith("think:toggle:")) {
        bool ok2 = false;
        const int N = s.mid(13).toInt(&ok2);
        if (!ok2 || !m_thinkTexts.contains(N)) return;

        const int scrollPos = m_log->verticalScrollBar()->value();
        QString html = m_log->toHtml();
        const QString nStr = QString::number(N);

        /* Nota: Qt serializza toHtml() sostituendo entity con caratteri Unicode.
           &#9654; (▶ U+25B6) diventa \xe2\x96\xb6 senza VS-16.
           &#9660; (▼ U+25BC) diventa \xe2\x96\xbc — usiamo ▼ per "aperto" e ▶ per "chiuso". */
        if (m_thinkShown.contains(N)) {
            /* === CHIUDI (arrotola la tenda) === */
            m_thinkShown.remove(N);
            m_thinkDefaultOpen = false;
            QSettings().setValue("ui/thinkDefaultOpen", false);

            /* ▼ → ▶ */
            QRegularExpression reArrow(
                "(think:toggle:" + nStr + "[^>]*>)\xe2\x96\xbc",
                QRegularExpression::DotMatchesEverythingOption);
            html.replace(reArrow, "\\1\xe2\x96\xb6");
            /* Ripristina hint "(clicca per espandere)" dopo la freccia */
            QRegularExpression reHint(
                "(think:toggle:" + nStr + "[^>]*>\xe2\x96\xb6[^<]*)"
                "(?:&nbsp;parole)?",
                QRegularExpression::DotMatchesEverythingOption);
            /* Rimuovi la riga corpo (tra il primo </tr> dopo think:toggle:N e think-end:N) */
            QRegularExpression reBody(
                "(<tr><td[^>]*>[^<]*<p[^>]*>[\\s\\S]*?think-end:" + nStr +
                "[\\s\\S]*?</tr>)",
                QRegularExpression::DotMatchesEverythingOption);
            html.remove(reBody);
        } else {
            /* === APRI (stendi la tenda) === */
            m_thinkShown.insert(N);
            m_thinkDefaultOpen = true;
            QSettings().setValue("ui/thinkDefaultOpen", true);

            /* ▶ → ▼ */
            QRegularExpression reArrow(
                "(think:toggle:" + nStr + "[^>]*>)\xe2\x96\xb6(?:\xef\xb8\x8f)?",
                QRegularExpression::DotMatchesEverythingOption);
            html.replace(reArrow, "\\1\xe2\x96\xbc");

            const QString rawThink = m_thinkTexts.value(N);
            QString safeThink = rawThink;
            safeThink.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
            safeThink.replace("\n","<br>");

            /* Corpo da inserire dopo la riga header (</tr> che contiene think:toggle:N) */
            const QString bodyRow =
                "<tr><td bgcolor='#1a1a2e'"
                " style='padding:8px 12px;border:1px solid #4a4a72;border-top:none;"
                "border-radius:0 0 5px 5px;'>"
                  "<p style='color:#8888aa;font-size:11px;font-style:italic;"
                             "margin:0;line-height:1.5;'>"
                    + safeThink +
                    "<a href='think-end:" + nStr + "'></a>"
                  "</p>"
                "</td></tr>";

            /* Trovare la fine della riga header dell'accordion */
            int anchorPos = html.indexOf("think:toggle:" + nStr);
            if (anchorPos >= 0) {
                int endTr = html.indexOf("</tr>", anchorPos);
                if (endTr >= 0)
                    html.insert(endTr + 5, bodyRow);
            }
        }

        m_log->setHtml(html);
        m_log->verticalScrollBar()->setValue(scrollPos);
        return;
    }

    /* ── Copia blocco codice negli appunti ── */
    if (s.startsWith("code:copy:")) {
        bool ok3 = false;
        const int N = s.mid(10).toInt(&ok3);
        if (!ok3 || !m_codeBlocks.contains(N)) return;
        QGuiApplication::clipboard()->setText(m_codeBlocks[N].second);
        /* Feedback visivo: bolla temporanea nel log */
        m_log->moveCursor(QTextCursor::End);
        m_log->insertHtml(
            "<p style='color:#34d399;font-size:11px;margin:2px 0;"
            "font-style:italic;'>"
            "\xe2\x9c\x85 Codice copiato negli appunti."   /* ✅ */
            "</p>");
        return;
    }

    /* ── Salva blocco codice su disco ── */
    if (s.startsWith("code:save:")) {
        bool ok3 = false;
        const int N = s.mid(10).toInt(&ok3);
        if (!ok3 || !m_codeBlocks.contains(N)) return;
        const QString lang    = m_codeBlocks[N].first;
        const QString content = m_codeBlocks[N].second;

        /* Estendi al tipo di file corretto */
        static const QMap<QString,QString> extMap = {
            {"python","py"},{"py","py"},{"bash","sh"},{"sh","sh"},
            {"shell","sh"},{"c","c"},{"cpp","cpp"},{"c++","cpp"},
            {"h","h"},{"hpp","hpp"},{"java","java"},
            {"javascript","js"},{"js","js"},{"typescript","ts"},{"ts","ts"},
            {"html","html"},{"css","css"},{"sql","sql"},{"json","json"},
            {"yaml","yaml"},{"yml","yml"},{"xml","xml"},{"rust","rs"},
            {"go","go"},{"ruby","rb"},{"rb","rb"},{"php","php"},
            {"swift","swift"},{"kotlin","kt"},{"r","r"},{"lua","lua"},
            {"dart","dart"},{"cmake","cmake"},{"dockerfile","Dockerfile"},
            {"markdown","md"},{"md","md"},{"toml","toml"},{"ini","ini"},
        };
        const QString ext = extMap.value(lang, lang.isEmpty() ? "txt" : lang);
        const QString filter = ext == "Dockerfile"
            ? "Dockerfile (Dockerfile)"
            : QString("%1 (*.%2);;Tutti i file (*)").arg(ext.toUpper()).arg(ext);

        const QString path = QFileDialog::getSaveFileName(
            this,
            "Salva codice — " + (lang.isEmpty() ? "testo" : lang),
            QDir::homePath() + "/codice." + ext,
            filter);
        if (path.isEmpty()) return;

        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(content.toUtf8());
            f.close();
            m_log->moveCursor(QTextCursor::End);
            m_log->insertHtml(
                "<p style='color:#34d399;font-size:11px;margin:2px 0;"
                "font-style:italic;'>"
                "\xf0\x9f\x92\xbe Salvato: " +     /* 💾 */
                QFileInfo(path).fileName().toHtmlEscaped() +
                "</p>");
        } else {
            m_log->moveCursor(QTextCursor::End);
            m_log->insertHtml(
                "<p style='color:#f87171;font-size:11px;margin:2px 0;'>"
                "\xe2\x9d\x8c Errore salvataggio: " +   /* ❌ */
                f.errorString().toHtmlEscaped() + "</p>");
        }
        return;
    }

    /* ── Riesegui codice Python annullato in precedenza ── */
    if (s.startsWith("exec:run:")) {
        bool ok4 = false;
        const int execId = s.mid(9).toInt(&ok4);
        if (!ok4 || !m_pendingExecCodes.contains(execId)) return;
        const QString pyCode    = m_pendingExecCodes.take(execId);
        const bool    useSandbox = P::isSandboxReady();

        auto* dlg2 = new QDialog(this);
        dlg2->setWindowTitle(useSandbox
            ? "\xf0\x9f\x90\xb3  Esegui codice in sandbox Docker?"
            : "\xe2\x9a\xa0  Esegui codice generato dall\xe2\x80\x99" "AI?");
        dlg2->setMinimumSize(660, 460);
        auto* lay2 = new QVBoxLayout(dlg2);
        auto* warn2 = new QLabel(useSandbox
            ? "\xf0\x9f\x90\xb3  Il codice verr\xc3\xa0 eseguito in un container Docker isolato."
            : "\xe2\x9a\xa0  Codice Python generato dall\xe2\x80\x99" "AI — verifica prima di procedere.",
            dlg2);
        warn2->setWordWrap(true);
        warn2->setStyleSheet(useSandbox
            ? "color:#86efac;font-weight:bold;padding:6px;background:#052e16;border-radius:4px;"
            : "color:#facc15;font-weight:bold;padding:6px;background:#292524;border-radius:4px;");
        lay2->addWidget(warn2);
        auto* cv2 = new QTextEdit(dlg2);
        cv2->setReadOnly(true);
        cv2->setPlainText(pyCode);
        cv2->setFont(QFont("JetBrains Mono,Fira Code,Consolas,monospace", 10));
        cv2->setStyleSheet("background:#1e1e2e;color:#cdd6f4;border:1px solid #45475a;padding:4px;");
        lay2->addWidget(cv2, 1);
        auto* bb2 = new QDialogButtonBox(dlg2);
        auto* btnRun2 = bb2->addButton("\xe2\x96\xb6  Esegui", QDialogButtonBox::AcceptRole);
        bb2->addButton("\xe2\x9c\x96  Annulla", QDialogButtonBox::RejectRole);
        btnRun2->setStyleSheet(useSandbox
            ? "background:#16a34a;color:#fff;font-weight:bold;padding:4px 18px;"
            : "background:#ef4444;color:#fff;font-weight:bold;padding:4px 18px;");
        connect(bb2, &QDialogButtonBox::accepted, dlg2, &QDialog::accept);
        connect(bb2, &QDialogButtonBox::rejected, dlg2, &QDialog::reject);
        lay2->addWidget(bb2);
        if (dlg2->exec() != QDialog::Accepted) { dlg2->deleteLater(); return; }
        dlg2->deleteLater();

        /* Rilancia esecuzione — riusa la funzione già in uso per sandbox/locale */
        m_executorOutput.clear();
        if (m_execProc) { m_execProc->kill(); m_execProc->deleteLater(); m_execProc = nullptr; }
        m_execProc = new QProcess(this);
        m_execProc->setProcessChannelMode(QProcess::MergedChannels);
        auto tmrR = QSharedPointer<QElapsedTimer>::create();
        tmrR->start();

        auto onFinishedR = [this, tmrR](int exitCode, QProcess::ExitStatus) {
            const double ms  = tmrR->elapsed();
            const QString out = QString::fromUtf8(m_execProc->readAll());
            m_execProc->deleteLater();
            m_execProc = nullptr;
            m_executorOutput = out;
            const QString od = PrismaluxPaths::sanitizeErrorOutput(out);
            QTextCursor c(m_log->document());
            c.movePosition(QTextCursor::End);
            c.insertHtml(buildToolStrip(QString(), od, exitCode, ms));
            m_log->moveCursor(QTextCursor::End);
        };

        if (useSandbox) {
            const QSettings ss2("Prismalux", "GUI");
            const QString img = ss2.value(P::SK::kSandboxImage, "python:3.11-slim").toString();
            const QString mem = QString::number(ss2.value(P::SK::kSandboxMemory, 256).toInt()) + "m";
            connect(m_execProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, onFinishedR);
            m_execProc->start("docker", {"run","--rm","-i","--network","none",
                "--memory", mem, "--memory-swap", mem,
                "--cpus","1","--security-opt","no-new-privileges",
                img, "python3","-c", pyCode});
        } else {
            connect(m_execProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, onFinishedR);
            m_execProc->start("python3", {"-c", pyCode});
        }
        return;
    }

    /* ── Ricerca online → salva in RAG/RICERCA/<slug>.md ── */
    if (s.startsWith("websearch:")) {
        const QString q64  = s.mid(10);
        const QString query = QString::fromUtf8(
            QByteArray::fromBase64(q64.toLatin1(), QByteArray::Base64UrlEncoding)).trimmed();
        if (query.isEmpty()) return;

        /* Cartella destinazione */
        const QString ragDir = P::ragDir() + "/RICERCA";
        QDir().mkpath(ragDir);
        /* Slug filename: primi 40 char, senza caratteri speciali */
        QString slug = query.left(40);
        slug.replace(QRegularExpression("[^a-zA-Z0-9_àèìòùÀÈÌÒÙ ]"), "_");
        slug = slug.simplified().replace(' ', '_');
        const QString outFile = ragDir + "/" +
            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_" + slug + ".md";

        /* Conferma utente */
        auto reply = QMessageBox::question(
            this,
            "\xf0\x9f\x94\x8d  Cerca online",
            QString("Vuoi cercare online:\n<b>%1</b>\n\n"
                    "Il risultato sar\xc3\xa0 salvato in:\n%2\n\n"
                    "e iniettato come contesto RAG.")
                .arg(query.toHtmlEscaped(), outFile),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;

        /* Script Python: usa duckduckgo_search (pip install duckduckgo-search) */
        const QString script = QString(
            "import sys, json, datetime\n"
            "query = %1\n"
            "out_file = %2\n"
            "try:\n"
            "    from duckduckgo_search import DDGS\n"
            "    results = []\n"
            "    with DDGS() as ddgs:\n"
            "        for r in ddgs.text(query, max_results=5):\n"
            "            results.append(r)\n"
            "    if not results:\n"
            "        print('NORESULT', flush=True)\n"
            "        sys.exit(0)\n"
            "    lines = [f'# Ricerca: {query}', f'Data: {datetime.date.today()}', '']\n"
            "    for i, r in enumerate(results, 1):\n"
            "        lines.append(f'## {i}. {r.get(\"title\",\"\")}' )\n"
            "        lines.append(f'URL: {r.get(\"href\",\"\")}')\n"
            "        lines.append(r.get('body',''))\n"
            "        lines.append('')\n"
            "    with open(out_file, 'w', encoding='utf-8') as f:\n"
            "        f.write('\\n'.join(lines))\n"
            "    print('DONE:' + out_file, flush=True)\n"
            "except ImportError:\n"
            "    print('NODEPS', flush=True)\n"
            "    sys.exit(2)\n"
            "except Exception as e:\n"
            "    print(f'ERROR:{e}', flush=True)\n"
            "    sys.exit(1)\n"
        ).arg(QString("r\"\"\"%1\"\"\"").arg(query))
         .arg(QString("r\"%1\"").arg(outFile));

        auto* proc = new QProcess(this);
        /* Banner avvio */
        m_log->moveCursor(QTextCursor::End);
        m_log->insertHtml(
            "<p style='color:#60a5fa;font-size:11px;margin:4px 0;'>"
            "\xf0\x9f\x94\x8d  Ricerca in corso: <i>" +
            query.toHtmlEscaped() + "</i>...</p>");

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, query, outFile](int code, QProcess::ExitStatus) {
            proc->deleteLater();
            const QString out = proc->readAllStandardOutput().trimmed();
            m_log->moveCursor(QTextCursor::End);
            if (out.startsWith("DONE:")) {
                m_log->insertHtml(
                    "<p style='color:#4ade80;font-size:11px;margin:4px 0;'>"
                    "\xe2\x9c\x85  Salvato in RAG/RICERCA. "
                    "Fai una nuova domanda per usare il contesto.</p>");
                /* Inietta nel RAG automaticamente se RagGraph disponibile */
                emit onlineSearchResultReady(outFile, query);
            } else if (out == "NODEPS") {
                m_log->insertHtml(
                    "<p style='color:#facc15;font-size:11px;margin:4px 0;'>"
                    "\xe2\x9a\xa0  Installa prima: "
                    "<code>pip install duckduckgo-search</code></p>");
            } else if (out == "NORESULT") {
                m_log->insertHtml(
                    "<p style='color:#94a3b8;font-size:11px;margin:4px 0;'>"
                    "\xf0\x9f\x94\x8d  Nessun risultato trovato per questa query.</p>");
            } else {
                m_log->insertHtml(
                    "<p style='color:#f87171;font-size:11px;margin:4px 0;'>"
                    "\xe2\x9d\x8c  Errore ricerca (codice " + QString::number(code) + ")</p>");
            }
        });
        proc->start("python3", {"-c", script});
        return;
    }

    if (!s.startsWith("copy:") && !s.startsWith("tts:") && !s.startsWith("edit:")) return;
    /* Formato nuovo: "copy:N:BASE64URL" — il testo è embedded nell'href.
       Formato vecchio: "copy:N"          — fallback su m_bubbleTexts. */
    const int c1 = s.indexOf(':');              // colon dopo "copy"/"tts"/"edit"
    const int c2 = s.indexOf(':', c1 + 1);      // secondo colon (nuovo formato), -1 se vecchio
    bool ok = false;
    const int idx = s.mid(c1 + 1, c2 < 0 ? -1 : c2 - c1 - 1).toInt(&ok);
    if (!ok) return;
    QString text;
    const QString origB64 = (c2 > 0) ? s.mid(c2 + 1) : QString();
    if (c2 > 0) {
        text = QString::fromUtf8(QByteArray::fromBase64(
            origB64.toLatin1(), QByteArray::Base64UrlEncoding));
    } else {
        if (!m_bubbleTexts.contains(idx)) return;
        text = m_bubbleTexts.value(idx);
    }

    if (s.startsWith("edit:")) {
        /* Apre un dialog di modifica: l'utente può editare il testo liberamente
           e scegliere se rimpiazzare la bolla nel log o inviare come nuovo task */
        auto* dlg  = new QDialog(this);
        dlg->setWindowTitle(tr("\xe2\x9c\x8f\xef\xb8\x8f  Modifica testo"));
        dlg->setMinimumSize(dpiSize(520, 360));
        auto* dlgLay = new QVBoxLayout(dlg);
        dlgLay->setSpacing(10);

        auto* hint = new QLabel(
            "<small>\xe2\x84\xb9  Modifica il testo. <b>Invia come task</b> lo carica nel campo "
            "input pronto per essere rielaborato dall'AI. "
            "<b>Aggiorna bolla</b> sostituisce il testo nel log.</small>");
        hint->setWordWrap(true);
        dlgLay->addWidget(hint);

        auto* editor = new QTextEdit(dlg);
        editor->setPlainText(text.trimmed());
        editor->setFocus();
        dlgLay->addWidget(editor, 1);

        auto* btnBox = new QDialogButtonBox(dlg);
        auto* btnTask   = btnBox->addButton("Invia come task \xe2\x96\xb6",
                                            QDialogButtonBox::AcceptRole);
        auto* btnUpdate = btnBox->addButton("Aggiorna bolla \xf0\x9f\x94\x84",
                                            QDialogButtonBox::ApplyRole);
        auto* btnCancel = btnBox->addButton(QDialogButtonBox::Cancel);
        btnCancel->setText(tr("Annulla"));
        dlgLay->addWidget(btnBox);

        /* done(1)=task, done(2)=aggiorna bolla, reject=annulla */
        connect(btnTask,   &QPushButton::clicked, dlg, [dlg]{ dlg->done(1); });
        connect(btnUpdate, &QPushButton::clicked, dlg, [dlg]{ dlg->done(2); });
        connect(btnBox,    &QDialogButtonBox::rejected, dlg, &QDialog::reject);

        const int dlgResult = dlg->exec();

        if (dlgResult == 1) {
            m_input->setPlainText(editor->toPlainText().trimmed());
            m_input->setFocus();
            m_input->moveCursor(QTextCursor::End);
            dlg->deleteLater();
            /* Avvia il pipeline dopo la chiusura del dialog */
            QTimer::singleShot(0, this, &AgentiPage::onBtnRunDelayedClick);
            return;
        }
        if (dlgResult == 2) {
            const QString newText = editor->toPlainText().trimmed();
            if (!newText.isEmpty() && !origB64.isEmpty()) {
                m_undoHtmlStack.push(m_log->toHtml());
                const QString newB64 = newText.left(4096).toUtf8().toBase64(
                    QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
                QString html = m_log->toHtml();
                html.replace(origB64, newB64);
                m_log->setHtml(html);
                m_log->moveCursor(QTextCursor::End);
            }
        }
        dlg->deleteLater();
        return;
    }

    if (s.startsWith("copy:")) {
        /* Rimuovi tag HTML prima di copiare */
        QString plain = text;
        plain.remove(QRegularExpression("<[^>]*>"));
        plain = plain.trimmed();
        QGuiApplication::clipboard()->setText(plain.isEmpty() ? text : plain);
        /* Notifica visiva */
        QToolTip::showText(QCursor::pos(),
            "\xe2\x9c\x85  Il testo \xc3\xa8 stato copiato in memoria",
            nullptr, {}, 2000);
    } else {
        /* TTS — rimuovi tag HTML, limita a 400 parole */
        QString plain = text;
        plain.remove(QRegularExpression("<[^>]*>"));
        plain = plain.trimmed();
        if (plain.isEmpty()) plain = text;
        QStringList words = plain.split(' ', Qt::SkipEmptyParts);
        if (words.size() > 400) words = words.mid(words.size() - 400);
        _ttsPlay(words.join(" "));
    }
}

void AgentiPage::onBtnRunDelayedClick()
{
    m_btnRun->click();
}

void AgentiPage::onLogContextMenuRequested(const QPoint& pos)
{
    const QString sel  = m_log->textCursor().selectedText();
    const bool hasSel  = !sel.isEmpty();
    const QString label = hasSel ? "selezione" : "tutto";

    QMenu menu(m_log);
    QAction* actCopy = menu.addAction(
        "\xf0\x9f\x97\x82  Copia " + label);
    QAction* actRead = menu.addAction(
        "\xf0\x9f\x8e\x99  Leggi " + label);

    QAction* chosen = menu.exec(m_log->mapToGlobal(pos));
    const QString txt = hasSel ? sel : m_log->toPlainText();

    if (chosen == actCopy) {
        QGuiApplication::clipboard()->setText(txt);
    } else if (chosen == actRead) {
        QStringList words = txt.split(' ', Qt::SkipEmptyParts);
        if (words.size() > 400) words = words.mid(words.size() - 400);
        _ttsPlay(words.join(" "));
    }
}

void AgentiPage::onBtnRagToggled(bool on)
{
    m_ragPanel->setVisible(on);
    m_btnRag->setText(on ? "\xf0\x9f\x93\x8e  RAG \xe2\x97\xa4"
                         : "\xf0\x9f\x93\x8e  RAG");
}

void AgentiPage::onBtnHintHideClicked()
{
    m_hintWidget->setVisible(false);
    AppConfig::s().setValue("ui/hintVisible", false);
}

void AgentiPage::onBtnRunClicked()
{
    /* ── Conversa mode: il Run button gestisce la voce ────────────────────────
       - Se AI occupata → stop
       - Se in registrazione → ferma e trascrive
       - Se in trascrizione → attendi
       - Se input vuoto → avvia registrazione (auto-loop se Voce è attiva)
       - Se input ha testo → invia come chat normale (fall-through)
       ────────────────────────────────────────────────────────────────────── */
    if (m_modeBtn && m_modeBtn->currentMode() == TriModeButton::Conversa) {
        if (m_ai->busy()) { m_ai->abort(); return; }
        if (m_sttState == SttState::Recording)    { onSttTimeout(); return; }
        if (m_sttState == SttState::Transcribing) { return; }
        if (m_input->toPlainText().trimmed().isEmpty()) {
            /* Avvia registrazione; auto-loop basato sullo stato del pulsante Voce */
            m_voiceLoopActive = true; /* in Conversa il loop è sempre attivo */
            _sttStartRecording();
            return;
        }
        /* Input non vuoto → invia come chat (fall-through) */
    }

    if (m_ai->busy()) { m_ai->abort(); return; }

    /* Avviso se i tool sono abilitati ma il modello non supporta function calling */
    if (m_toolsEnabled && !isToolCapable(m_ai->model())) {
        m_log->moveCursor(QTextCursor::End);
        m_log->insertHtml(
            "<p style='color:#f59e0b;font-size:11px;font-style:italic;margin:2px 0;'>"
            "\xf0\x9f\x94\xa7 Il modello <b>" + m_ai->model().toHtmlEscaped() +
            "</b> non supporta il function calling: i tool verranno ignorati. "
            "Seleziona qwen3, llama3.1, mistral-nemo o un altro modello tool-capable.</p>");
        m_log->append({});
    }

    /* ── Agente ricerca web: intercetta domande che richiedono info online ── */
    {
        const QString userMsg = m_input->toPlainText().trimmed();
        if (!userMsg.isEmpty() && _detectWebIntent(userMsg)) {
            m_input->clear();
            runWebSearchAgent(userMsg);
            return;
        }
    }

    /* Agente Autonomo: intercetta prima della pipeline normale */
    if (m_autoEnabled && !m_modePipeline) {
        const QString task = m_input->toPlainText().trimmed();
        if (task.isEmpty()) return;
        /* Reset stato ciclo ReAct */
        m_autoHistory    = QJsonArray();
        m_autoStep       = 0;
        m_autoBuf.clear();
        m_autoLastUserMsg = task;
        m_input->clear();
        /* Banner info — solo alla prima chat in modalità autonoma */
        if (!m_autoMsgShown) {
            m_autoMsgShown = true;
            m_log->moveCursor(QTextCursor::End);
            m_log->insertHtml(
                "<p style='color:#818cf8;font-size:11px;text-align:center;"
                "font-style:italic;margin:2px 0;'>"
                "\xf0\x9f\xa4\x96 Agente Autonomo attivato &mdash; "
                "l\xe2\x80\x99" "AI pianifica e usa strumenti automaticamente (max 8 step)</p>");
        }
        /* Bolla utente */
        { int idx = m_bubbleIdx++;
          m_bubbleTexts[idx] = task;
          m_log->moveCursor(QTextCursor::End);
          m_log->insertHtml(buildUserBubble(task, idx)); }
        m_log->append("");
        emit pipelineStatus(0, "\xf0\x9f\xa4\x96  Agente autonomo in esecuzione...");
        _setRunBusy(true);
        runAutonomousAgent();
        return;
    }

    /* Team di agenti ON: forza singolo agente con system prompt "team".
     * Il pipeline esistente viene riusato con maxShots=1 — il system prompt
     * viene sovrascritto in runAgent() quando m_btnTeam è attivo. */
    if (m_btnTeam && m_btnTeam->isChecked()) {
        m_cfgDlg->numAgentsSpinBox()->setValue(1);
        m_maxShots   = 1;
        m_modePipeline = false;
        runPipeline();
        return;
    }

    if (!m_modePipeline) {
        /* Modalità Singolo: forza 1 agente */
        m_cfgDlg->numAgentsSpinBox()->setValue(1);
        m_maxShots = 1;
    }
    const int mode = m_cmbMode->currentIndex();
    if      (mode == 10) runConsiglioScientifico();
    else if (mode == 2)  runMathTheory();
    else                 runPipeline();
}

void AgentiPage::onSymbolBtnClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn || !m_input) return;
    m_input->insertPlainText(btn->property("symbol").toString());
}

void AgentiPage::onBtnSymbolsClicked()
{
    const bool nowVisible = m_symbolsScrollArea && !m_symbolsScrollArea->isVisible();
    if (nowVisible) {
        /* Chiudi Tool Veloci e Tool Lenti */
        if (m_btnToolsToggle && m_btnToolsToggle->isChecked()) {
            m_btnToolsToggle->blockSignals(true);
            m_btnToolsToggle->setChecked(false);
            m_btnToolsToggle->blockSignals(false);
            if (m_toolsPanel) m_toolsPanel->setVisible(false);
        }
        if (m_btnMcpToggle && m_btnMcpToggle->isChecked()) {
            m_btnMcpToggle->blockSignals(true);
            m_btnMcpToggle->setChecked(false);
            m_btnMcpToggle->blockSignals(false);
            if (m_mcpPanel) m_mcpPanel->setVisible(false);
        }
    }
    if (m_symbolsScrollArea) m_symbolsScrollArea->setVisible(nowVisible);
    if (m_symbolSearch) {
        m_symbolSearch->setVisible(nowVisible);
        if (!nowVisible) m_symbolSearch->clear();
    }
}

void AgentiPage::onBtnTranslateClicked()
{
    if (m_ai->busy()) {
        m_log->append("\xe2\x9a\xa0  Un'altra operazione \xc3\xa8 in corso.");
        return;
    }
    const QString inputText = m_input->toPlainText().trimmed();
    if (inputText.isEmpty()) {
        m_log->append("\xe2\x9a\xa0  Inserisci il testo da tradurre nel campo input.");
        return;
    }

    QString src, dst, model;
    if (!_buildTranslateDialog(inputText, &src, &dst, &model))
        return;

    _startTranslation(src, dst, model, inputText);
}

/* ── Dialog selezione lingue/modello per la traduzione.
   Restituisce true se l'utente ha confermato, false se ha annullato.
   Scrive i parametri scelti nelle variabili puntate.               ── */
bool AgentiPage::_buildTranslateDialog(const QString& inputText,
                                       QString* outSrc, QString* outDst,
                                       QString* outModel)
{
    static const QStringList kLangs = {
        "Auto-rileva",
        "Italiano", "Inglese", "Francese", "Spagnolo", "Portoghese",
        "Tedesco", "Olandese", "Svedese", "Norvegese", "Danese",
        "Russo", "Ucraino", "Polacco", "Ceco", "Slovacco",
        "Cinese (Mandarino)", "Giapponese", "Coreano",
        "Arabo", "Persiano (Farsi)", "Turco", "Ebraico",
        "Hindi", "Bengalese", "Urdu",
        "Swahili", "Hausa", "Somalo",
        "Greco", "Rumeno", "Ungherese", "Finlandese",
        "Catalano", "Galiziano", "Basco",
        "Indonesiano", "Malese", "Tagalog (Filippino)",
        "Thai", "Vietnamita"
    };

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x8c\x90  Traduci testo"));
    dlg->setFixedWidth(dpiScale(420));
    auto* dlgLay = new QVBoxLayout(dlg);
    dlgLay->setSpacing(10);

    auto* srcRow = new QHBoxLayout;
    srcRow->addWidget(new QLabel("Da:", dlg));
    auto* cmbSrc = new QComboBox(dlg);
    cmbSrc->addItems(kLangs);
    int si = cmbSrc->findText(m_translateSrcLang);
    if (si >= 0) cmbSrc->setCurrentIndex(si);
    srcRow->addWidget(cmbSrc, 1);
    dlgLay->addLayout(srcRow);

    auto* dstRow = new QHBoxLayout;
    dstRow->addWidget(new QLabel("A:", dlg));
    auto* cmbDst = new QComboBox(dlg);
    for (const QString& l : kLangs)
        if (l != "Auto-rileva") cmbDst->addItem(l);
    cmbDst->setCurrentText(m_translateDstLang.isEmpty() ? "Italiano" : m_translateDstLang);
    dstRow->addWidget(cmbDst, 1);
    dlgLay->addLayout(dstRow);

    auto* mdlRow = new QHBoxLayout;
    mdlRow->addWidget(new QLabel("Modello:", dlg));
    auto* cmbMdl = new QComboBox(dlg);
    for (auto& mi : m_modelInfos) cmbMdl->addItem(mi.name);
    if (cmbMdl->count() == 0) cmbMdl->addItem(m_ai->model());
    else {
        int idx = cmbMdl->findText(m_ai->model());
        if (idx >= 0) cmbMdl->setCurrentIndex(idx);
    }
    mdlRow->addWidget(cmbMdl, 1);
    dlgLay->addLayout(mdlRow);

    auto* previewLbl = new QLabel(
        QString("\xf0\x9f\x93\x9d  Testo: <i>%1</i>")
        .arg(inputText.length() > 80
             ? inputText.left(80) + "\xe2\x80\xa6"
             : inputText), dlg);
    previewLbl->setWordWrap(true);
    previewLbl->setStyleSheet("color:#9ca3af; font-size:11px;");
    dlgLay->addWidget(previewLbl);

    auto* bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    bb->button(QDialogButtonBox::Ok)->setText(tr("\xf0\x9f\x8c\x90  Traduci"));
    dlgLay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return false; }

    *outSrc   = cmbSrc->currentText();
    *outDst   = cmbDst->currentText();
    *outModel = cmbMdl->currentText();
    dlg->deleteLater();
    return true;
}

/* ── Imposta lo stato e lancia la chat di traduzione ── */
void AgentiPage::_startTranslation(const QString& src, const QString& dst,
                                   const QString& model, const QString& inputText)
{
    m_translateSrcLang = src;
    m_translateDstLang = dst;

    m_preTranslateModel = m_ai->model();
    m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), model);

    QString prompt;
    if (src == "Auto-rileva")
        prompt = QString("Traducimi il seguente testo nella lingua %1. "
                         "Mantieni il significato originale nel modo pi\xc3\xb9 fedele possibile. "
                         "Rispondi SOLO con la traduzione, senza commenti aggiuntivi.\n\n"
                         "Testo:\n%2").arg(dst).arg(inputText);
    else
        prompt = QString("Traducimi il seguente testo da %1 a %2. "
                         "Mantieni il significato originale nel modo pi\xc3\xb9 fedele possibile. "
                         "Rispondi SOLO con la traduzione, senza commenti aggiuntivi.\n\n"
                         "Testo:\n%3")
                 .arg(src).arg(dst).arg(inputText);

    const QString sys =
        "Sei un traduttore professionale. "
        "Rispondi SEMPRE e SOLO con la traduzione richiesta, senza spiegazioni.";

    m_log->clear();
    m_log->append(QString("\xf0\x9f\x8c\x90  Traduzione  <b>%1</b> \xe2\x86\x92 <b>%2</b>"
                          "  [modello: %3]\n")
                  .arg(src, dst, model));
    m_log->append(QString(43, QChar(0x2500)));

    m_taskOriginal  = inputText;
    m_translateBuf.clear();
    m_pendingMode = OpMode::Idle;
    m_opMode      = OpMode::Translating;

    _setRunBusy(true);
    m_btnTranslate->setEnabled(false);
    m_waitLbl->setVisible(true);

    m_ai->chat(sys, prompt);
}

void AgentiPage::onBtnDocClicked()
{
    static const QString filter =
        "Tutti i file supportati "
        "(*.txt *.md *.csv *.json *.py *.cpp *.c *.h *.html *.xml *.rst *.log "
        "*.pdf *.xls *.xlsx *.ods *.ots *.fods "
        "*.png *.jpg *.jpeg *.gif *.webp);;"
        "Documenti (*.txt *.md *.csv *.json *.py *.cpp *.c *.h *.html *.xml *.rst *.log);;"
        "PDF (*.pdf);;"
        "Fogli di calcolo (*.xls *.xlsx *.ods *.ots *.fods);;"
        "Immagini (*.png *.jpg *.jpeg *.gif *.webp);;"
        "Tutti (*)";
    const QString fp = QFileDialog::getOpenFileName(
        this, "Allega file", QString(), filter);
    if (!fp.isEmpty()) loadDroppedFile(fp);
}

void AgentiPage::onBtnVoiceClicked()
{
    /* ── Stop durante registrazione ── */
    if (m_sttState == SttState::Recording) {
        if (m_recProc) { m_recProc->kill(); m_recProc->waitForFinished(300); }
        m_sttState = SttState::Idle;
        m_btnVoice->setText(tr("\xf0\x9f\x8e\xa4 Trascrivi parlato"));
        m_btnVoice->setProperty("danger","false");
        P::repolish(m_btnVoice);
        m_btnVoice->setEnabled(true);
        return;
    }
    /* ── Ignora clic mentre trascrive o scarica ── */
    if (m_sttState == SttState::Transcribing ||
        m_sttState == SttState::Downloading) return;

    /* ── Controlla binario ── */
    if (SttWhisper::whisperBin().isEmpty()) {
        m_log->append(
            "<p style='color:#e2e8f0;'>"
            "\xe2\x9a\xa0  <b>whisper-cli non trovato.</b> "
            "<a href=\"settings:trascrivi\" style=\"color:#93c5fd;\">"
            "Clicca qui</a> per aprire le Impostazioni &rarr; Trascrivi"
            " e installare il riconoscimento vocale."
            "</p>");
        return;
    }

    /* ── Modello assente: avvia download automatico ── */
    if (SttWhisper::whisperModel().isEmpty()) {
        downloadWhisperModel();
        return;
    }

    _sttStartRecording();
}

void AgentiPage::onNumAgentsChanged(int v)
{
    m_maxShots = v;
}

void AgentiPage::onCmbModePresetChanged(int idx)
{
    if (idx < 3) { m_autoLbl->setVisible(false); return; }
    m_cfgDlg->applyPreset(idx);
    static const char* lbls[] = {
        "\xf0\x9f\x93\x90  Preset Matematica applicato.",
        "\xf0\x9f\x92\xbb  Preset Informatica applicato.",
        "\xf0\x9f\x94\x90  Preset Sicurezza applicato.",
        "\xe2\x9a\x9b\xef\xb8\x8f  Preset Fisica applicato.",
        "\xf0\x9f\xa7\xaa  Preset Chimica applicato.",
        "\xf0\x9f\x8c\x90  Preset Lingue applicato.",
        "\xf0\x9f\x8c\x8d  Preset Generico applicato.",
    };
    m_autoLbl->setText(QString::fromUtf8(lbls[idx - 3])
                       + "  \xe2\x80\x94  Puoi modificarli in \xe2\x9a\x99\xef\xb8\x8f Configura Agenti.");
    m_autoLbl->setVisible(true);
}

void AgentiPage::onCmbModeMathChanged(int idx)
{
    if (idx != 2) return;
    static const QStringList mathKw  = {"r1","math","reason","think","qwq","deepseek-r"};
    static const QStringList coderKw = {"coder","code","starcoder","codellama","qwen2.5-coder"};
    static const QStringList largeKw = {"qwen3","llama3","gemma","mistral","phi"};

    auto bestMatch = [&](int i) -> int {
        QComboBox* cb = m_cfgDlg->modelCombo(i);
        for (int p = 0; p < cb->count(); p++) {
            QString n = cb->itemText(p).toLower();
            for (auto& kw : mathKw) if (n.contains(kw)) return p;
        }
        for (int p = 0; p < cb->count(); p++) {
            QString n = cb->itemText(p).toLower();
            for (auto& kw : coderKw) if (n.contains(kw)) return p;
        }
        for (int p = 0; p < cb->count(); p++) {
            QString n = cb->itemText(p).toLower();
            for (auto& kw : largeKw) if (n.contains(kw)) return p;
        }
        return -1;
    };
    int assigned = 0;
    for (int i = 0; i < MAX_AGENTS; i++) {
        if (!m_cfgDlg->enabledChk(i)->isChecked()) continue;
        int best = bestMatch(i);
        if (best >= 0) { m_cfgDlg->modelCombo(i)->setCurrentIndex(best); assigned++; }
    }
    m_autoLbl->setText(assigned > 0
        ? "\xf0\x9f\xa7\xae  Modelli reasoning/coder pre-selezionati per Matematico Teorico."
        : "\xf0\x9f\x92\xa1  Consigliato: modelli reasoning (deepseek-r1, qwen3, qwq).");
    m_autoLbl->setVisible(true);
}

void AgentiPage::onAiAborted()
{
    m_waitLbl->setVisible(false);

    /* Rimuove il contenuto parziale dello streaming (testo grezzo non ancora
       convertito in bolla) che va da m_agentBlockStart alla fine del documento. */
    if (m_agentBlockStart > 0) {
        QTextCursor sel(m_log->document());
        sel.setPosition(m_agentBlockStart);
        sel.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        if (!sel.selectedText().trimmed().isEmpty())
            sel.removeSelectedText();
        m_agentBlockStart = 0;
    }

    m_opMode       = OpMode::Idle;
    m_currentAgent = 0;
    m_agentOutputs.clear();
    m_byzStep      = 0;
    /* Reset background mode se abort avviene mentre siamo in background */
    m_bgMode = false;
    m_bgBuffer.clear();
    m_bgHtmlSave.clear();
    _setRunBusy(false);
    emit pipelineStatus(0, "\xe2\x9c\x8b  Interrotto");
    for (int i = 0; i < MAX_AGENTS; i++)
        m_cfgDlg->enabledChk(i)->setStyleSheet("");
    m_log->moveCursor(QTextCursor::End);
    m_log->append("\n\xe2\x9c\x8b  Pipeline interrotta.");
}

/* ──────────────────────────────────────────────────────────────
   _ingestRagFiles — gestisce i file droppati nella zona RAG
   specializzata per PDF / .txt / .md.

   .txt / .md → lettura diretta + addEntry() in m_ragInline
   .pdf       → loadDroppedFile() (estrazione asincrona)
   ────────────────────────────────────────────────────────────── */
void AgentiPage::_ingestRagFiles(const QList<QUrl>& urls)
{
    /* Cartelle RAG persistenti (stesse scansionate da RagGraph) */
    const QString ragDir  = P::ragDir();
    const QString ragDocs = QDir::cleanPath(QDir::homePath() + "/prismalux_rag_docs");

    for (const QUrl& u : urls) {
        const QString path = u.toLocalFile();
        if (path.isEmpty()) continue;

        /* ── Copia in RAG/ se il file è fuori dalle cartelle persistenti (con conferma) ── */
        const QString canon = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        const bool inRag = canon.startsWith(ragDir) || canon.startsWith(ragDocs);
        if (!inRag) {
            const QString dest = ragDir + "/" + QFileInfo(path).fileName();
            if (!QFile::exists(dest)) {
                const QString msg = QString(
                    "Vuoi copiare \"%1\" nella cartella RAG?\n\n"
                    "Destinazione: %2\n\n"
                    "Copiando il file, Prismalux potr\xc3\xa0 indicizzarlo automaticamente "
                    "e usarlo come base di conoscenza in tutte le sessioni future.")
                    .arg(QFileInfo(path).fileName(), ragDir);
                const auto ans = QMessageBox::question(
                    this, "Copia nella cartella RAG", msg,
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                if (ans == QMessageBox::Yes) {
                    QDir().mkpath(ragDir);
                    if (QFile::copy(path, dest)) {
                        if (m_ragStatusLbl)
                            m_ragStatusLbl->setText(
                                "\xf0\x9f\x93\x84  Copiato in RAG/ \xe2\x80\x94 "
                                "il documento \xc3\xa8 ora persistente");
                    }
                }
            }
        }

        const QString pl = path.toLower();

        if (pl.endsWith(".txt") || pl.endsWith(".md")) {
            /* Testo: lettura sincrona → addEntry nel RAG inline */
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QString content = QString::fromUtf8(f.readAll()).trimmed();
            f.close();
            if (content.isEmpty()) continue;

            m_ragIngesting = true;
            if (m_ragStatusLbl)
                m_ragStatusLbl->setText(
                    "\xf0\x9f\x94\x84  Indicizzazione in corso...");

            if (m_ragInline)
                m_ragInline->addEntry(QFileInfo(path).fileName(), content);

            /* Completa subito (sincrono) */
            QTimer::singleShot(0, this, &AgentiPage::onRagIngestionDone);

        } else if (pl.endsWith(".pdf")) {
            /* PDF: estrazione asincrona via loadDroppedFile.
               Il testo estratto va in m_docContext (disponibile per la prossima query).
               Mostriamo feedback immediato e resettiamo dopo 2s. */
            m_ragIngesting = true;
            if (m_ragStatusLbl)
                m_ragStatusLbl->setText(
                    "\xf0\x9f\x94\x84  Estrazione PDF in corso...");
            if (m_ragDropZone)
                m_ragDropZone->setText(
                    "\xf0\x9f\x93\x84  Estrazione PDF in corso...");

            loadDroppedFile(path);

            /* Timeout heuristico: il completamento reale è gestito da loadDroppedFile
               tramite il log; qui resettiamo solo il feedback visivo della zona drop. */
            QTimer::singleShot(2500, this, &AgentiPage::onRagIngestionDone);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────
   Web reading via RAG — onRagUrlAddClicked / onRagUrlFetched
   ───────────────────────────────────────────────────────────────── */
void AgentiPage::onRagUrlAddClicked()
{
    if (!m_ragUrlLine) return;
    const QString urlStr = m_ragUrlLine->text().trimmed();
    if (!urlStr.startsWith("http://") && !urlStr.startsWith("https://")) {
        if (m_ragStatusLbl) m_ragStatusLbl->setText(tr("\xe2\x9a\xa0\xef\xb8\x8f  URL non valido"));
        return;
    }

    if (!m_ragUrlNam)
        m_ragUrlNam = new QNetworkAccessManager(this);

    if (m_ragUrlReply) {
        m_ragUrlReply->abort();
        m_ragUrlReply->deleteLater();
        m_ragUrlReply = nullptr;
    }

    if (m_ragStatusLbl) m_ragStatusLbl->setText(tr("\xf0\x9f\x8c\x90  Recupero pagina..."));

    QNetworkRequest req;
    req.setUrl(QUrl(urlStr));
    req.setHeader(QNetworkRequest::UserAgentHeader, "Prismalux/1.0 (RAG web reader)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_ragUrlReply = m_ragUrlNam->get(req);
    connect(m_ragUrlReply, &QNetworkReply::finished, this, &AgentiPage::onRagUrlFetched);
}

void AgentiPage::onRagUrlFetched()
{
    if (!m_ragUrlReply) return;
    const QUrl  finalUrl = m_ragUrlReply->url();
    const auto  err      = m_ragUrlReply->error();
    const QByteArray raw = m_ragUrlReply->readAll();
    m_ragUrlReply->deleteLater();
    m_ragUrlReply = nullptr;

    if (err != QNetworkReply::NoError) {
        if (m_ragStatusLbl) m_ragStatusLbl->setText("\xe2\x9d\x8c  Errore rete: " +
            QString::number(static_cast<int>(err)));
        LogBus::post(QString("\xe2\x9d\x8c AI UI: Errore rete RAG URL: %1 (codice %2)")
                     .arg(finalUrl.toString())
                     .arg(static_cast<int>(err)));
        return;
    }

    QString text = QString::fromUtf8(raw);

    /* Rimuovi tag HTML e decodifica entità comuni */
    static const QRegularExpression reTag("<[^>]+>");
    static const QRegularExpression reWs("\\s{3,}");
    text.replace(reTag, " ");
    text.replace("&amp;",  "&");
    text.replace("&lt;",   "<");
    text.replace("&gt;",   ">");
    text.replace("&nbsp;", " ");
    text.replace("&quot;", "\"");
    text.replace("&#39;",  "'");
    text = text.replace(reWs, "\n\n").trimmed();

    /* Tronca a ~8000 caratteri per non saturare il contesto */
    if (text.size() > 8000)
        text = text.left(8000) + "\n\n[...]";

    if (text.isEmpty()) {
        if (m_ragStatusLbl) m_ragStatusLbl->setText(tr("\xe2\x9a\xa0\xef\xb8\x8f  Nessun testo trovato"));
        return;
    }

    const QString title = finalUrl.host() + finalUrl.path();
    if (m_ragInline) m_ragInline->addEntry(title, text);
    if (m_ragUrlLine) m_ragUrlLine->clear();
    if (m_ragStatusLbl) {
        m_ragStatusLbl->setText(
            QString("\xe2\x9c\x85  Pagina aggiunta (%1 car.)").arg(text.size()));
        QTimer::singleShot(3000, m_ragStatusLbl, &QLabel::clear);
    }
}

/* ══════════════════════════════════════════════════════════════
   buildToolsPanel — due pannelli separati:
     m_toolsPanel  ⚡ Tool Veloci   (Function Tools, in-process)
     m_mcpPanel    🔌 Tool Lenti    (MCP Plugin, subprocess)
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::buildToolsPanel(QVBoxLayout* lay)
{
    static const struct {
        const char* name; const char* icon;
        const char* label; const char* desc;
    } kTools[] = {
        { "calc",         "\xf0\x9f\x94\xa2", "Calcola espressioni",
          "Valuta espressioni matematiche (2+2, sqrt(16), sin(pi/4)...)" },
        { "ricerca",      "\xf0\x9f\x94\x8d", "Cerca online",
          "Cerca informazioni su internet via DuckDuckGo" },
        { "fetch_url",    "\xf0\x9f\x8c\x90", "Scarica pagina",
          "Scarica e legge il contenuto di una pagina web" },
        { "leggi_file",   "\xf0\x9f\x93\x84", "Leggi file",
          "Legge un file di testo dal disco (percorso assoluto)" },
        { "lista_file",   "\xf0\x9f\x93\x82", "Elenca cartella",
          "Mostra l'elenco dei file in una directory" },
        { "python",       "\xf0\x9f\x90\x8d", "Esegui Python",
          "Esegue un frammento di codice Python in ambiente sandbox" },
        { "search_rag",   "\xf0\x9f\x93\x9a", "Cerca documenti",
          "Cerca nei documenti che hai indicizzato nel RAG" },
        { "graph_memory", "\xf0\x9f\x95\xb8", "Memoria sessioni",
          "Cerca nella memoria persistente delle sessioni precedenti (GraphMemory)" },
        { "get_knowledge","\xf0\x9f\xa7\xa0", "Base conoscenza",
          "Legge la tua Knowledge Base personale (file KNOWLEDGE_USER/)" },
        { "spawn_agent",  "\xf0\x9f\xa4\x96", "Crea agente",
          "Avvia un sub-agente specializzato per un sotto-compito autonomo" },
    };
    constexpr int kNTools = 10;

    /* ══ PANNELLO 1: ⚡ Tool Veloci (Function Tools, in-process) ══ */
    m_toolsPanel = new QFrame(this);
    m_toolsPanel->setObjectName("symbolsPanel");
    m_toolsPanel->setVisible(false);

    auto* fastLay = new QVBoxLayout(m_toolsPanel);
    fastLay->setContentsMargins(8, 6, 8, 6);
    fastLay->setSpacing(6);

    /* Header ⚡ */
    {
        auto* hdrRow = new QWidget(m_toolsPanel);
        auto* hdrLay = new QHBoxLayout(hdrRow);
        hdrLay->setContentsMargins(0, 0, 0, 0);
        hdrLay->setSpacing(8);

        auto* hdr = new QLabel(hdrRow);
        hdr->setTextFormat(Qt::RichText);
        hdr->setText(
            "<b>\xe2\x9a\xa1 Tool Veloci</b>"
            "<span style='color:#16a34a;font-size:10px;font-weight:bold;'>"
            " \xe2\x80\x94 eseguiti in-process, &lt;1ms</span>"
            "<br><span style='color:#64748b;font-size:9px;'>"
            "Function Tools integrati: calcola, cerca, leggi file, Python, RAG\xe2\x80\xa6</span>");
        hdrLay->addWidget(hdr, 1);

        auto* btnAll  = new QPushButton("\xe2\x9c\x85  Tutti",   hdrRow);
        auto* btnNone = new QPushButton("\xe2\x96\xa1  Nessuno", hdrRow);
        btnAll->setObjectName("actionBtn");
        btnNone->setObjectName("actionBtn");
        btnAll->setFixedHeight(dpiScale(22));
        btnNone->setFixedHeight(dpiScale(22));
        hdrLay->addWidget(btnAll);
        hdrLay->addWidget(btnNone);
        fastLay->addWidget(hdrRow);

        /* Grid 2 colonne */
        auto* grid = new QWidget(m_toolsPanel);
        auto* gl   = new QGridLayout(grid);
        gl->setContentsMargins(0, 0, 0, 0);
        gl->setSpacing(4);
        gl->setColumnStretch(0, 1);
        gl->setColumnStretch(1, 1);

        for (int i = 0; i < kNTools; ++i) {
            const QString name  = QString::fromLatin1(kTools[i].name);
            const QString icon  = QString::fromUtf8(kTools[i].icon);
            const QString label = QString::fromUtf8(kTools[i].label);
            const QString desc  = QString::fromUtf8(kTools[i].desc);

            auto* chk = new QCheckBox(icon + "  " + label + "  (" + name + ")", grid);
            chk->setToolTip(desc);
            chk->setChecked(true);
            chk->setMinimumHeight(dpiScale(22));
            m_enabledTools.insert(name);
            gl->addWidget(chk, i / 2, i % 2);

            connect(chk, &QCheckBox::toggled, this, [this, name](bool on){
                if (on) m_enabledTools.insert(name);
                else    m_enabledTools.remove(name);
                onToolEnabledChanged();
            });
        }

        connect(btnAll, &QPushButton::clicked, grid, [grid]{
            for (auto* c : grid->findChildren<QCheckBox*>()) c->setChecked(true);
        });
        connect(btnNone, &QPushButton::clicked, grid, [grid]{
            for (auto* c : grid->findChildren<QCheckBox*>()) c->setChecked(false);
        });

        /* Barra di ricerca — filtra e riorganizza il grid senza buchi */
        auto* fastSearch = new QLineEdit(m_toolsPanel);
        fastSearch->setPlaceholderText("\xf0\x9f\x94\x8d  Cerca tool per nome o descrizione\xe2\x80\xa6");
        fastSearch->setClearButtonEnabled(true);
        fastSearch->setFixedHeight(dpiScale(26));
        fastLay->addWidget(fastSearch);
        fastLay->addWidget(grid);

        connect(fastSearch, &QLineEdit::textChanged, grid,
                [grid, gl](const QString& raw){
            const QString q = raw.trimmed().toLower();
            const auto chks = grid->findChildren<QCheckBox*>(
                QString(), Qt::FindDirectChildrenOnly);
            for (auto* c : chks) gl->removeWidget(c);
            int pos = 0;
            for (auto* c : chks) {
                const bool match = q.isEmpty()
                    || c->text().toLower().contains(q)
                    || c->toolTip().toLower().contains(q);
                c->setVisible(match);
                if (match) { gl->addWidget(c, pos / 2, pos % 2); ++pos; }
            }
        });
    }

    /* Hint modello tool-capable — visibile solo dentro questo pannello */
    {
        auto* hintLbl = new QLabel(
            "<span style='color:#475569;font-size:10px;'>"
            "Richiedono un modello tool-capable "
            "(qwen3, llama3.1, mistral-nemo\xe2\x80\xa6)</span>",
            m_toolsPanel);
        hintLbl->setTextFormat(Qt::RichText);
        hintLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fastLay->addWidget(hintLbl);
    }

    lay->addWidget(m_toolsPanel);

    /* ══ PANNELLO 2: 🔌 Tool Lenti — MCP Plugin (subprocess JSON-RPC) ══ */

    /* Descrizioni italiane per MCP noti; fallback automatico per gli altri */
    static const QHash<QString, QString> kMcpLabels = {
        { "ai_memory_mcp",        "Memoria AI" },
        { "android_adb_mcp",      "Android ADB" },
        { "anki_mcp",             "Flashcard Anki" },
        { "arch_analyzer_mcp",    "Analisi architettura" },
        { "devagent_mcp",         "Dev Agent" },
        { "gns3_mcp",             "Simulatore GNS3" },
        { "knowledge_mcp",        "Aggiorna conoscenza" },
        { "mypy_mcp",             "Analisi Python (mypy)" },
        { "ollama_mcp",           "Modelli Ollama" },
        { "opencode_mcp",         "Editor AI OpenCode" },
        { "owasp_mcp",            "Sicurezza OWASP" },
        { "perf_analyzer_mcp",    "Analisi performance" },
        { "prismalux_build_mcp",  "Build Prismalux" },
        { "prismalux_search_mcp", "Ricerca Prismalux" },
        { "qt_i18n_mcp",          "Traduzioni Qt" },
        { "rag_manager_mcp",      "Gestione RAG" },
        { "rdkit_mcp",            "Chimica (RDKit)" },
        { "sast_mcp",             "Sicurezza statica" },
        { "secrets_scanner_mcp",  "Scanner segreti" },
        { "snippet_mcp",          "Snippet codice" },
        { "sqlite_inspector_mcp", "Inspector SQLite" },
        { "ssh_remote_mcp",       "Connessione SSH" },
        { "system_monitor_mcp",   "Monitor sistema" },
        { "test_generator_mcp",   "Genera test" },
        { "tinymcp",              "Gestore MCP" },
        { "translation_mcp",      "Traduzione testi" },
        { "ui_ux_checker_mcp",    "Verifica UI/UX" },
        { "web_scraper_mcp",      "Web scraping" },
    };
    auto mcpLabel = [](const QString& name) -> QString {
        if (kMcpLabels.contains(name)) return kMcpLabels.value(name);
        QString s = name;
        if (s.endsWith("_mcp")) s.chop(4);
        s.replace('_', ' ');
        if (!s.isEmpty()) s[0] = s[0].toUpper();
        return s;
    };

    m_mcpPanel = new QFrame(this);
    m_mcpPanel->setObjectName("symbolsPanel");
    m_mcpPanel->setVisible(false);
    m_mcpPanel->setMaximumHeight(dpiScale(280));

    auto* slowLay = new QVBoxLayout(m_mcpPanel);
    slowLay->setContentsMargins(8, 6, 8, 6);
    slowLay->setSpacing(6);

    /* Header 🔌 con Tutti/Nessuno */
    {
        auto* hdrRow = new QWidget(m_mcpPanel);
        auto* hdrLay = new QHBoxLayout(hdrRow);
        hdrLay->setContentsMargins(0, 0, 0, 0);
        hdrLay->setSpacing(8);

        auto* hdr = new QLabel(hdrRow);
        hdr->setTextFormat(Qt::RichText);
        hdr->setText(
            "<b>\xf0\x9f\x94\x8c Tool Lenti (MCP)</b>"
            "<span style='color:#dc2626;font-size:10px;font-weight:bold;'>"
            " \xe2\x80\x94 processo separato, +latenza</span>"
            "<br><span style='color:#64748b;font-size:9px;'>"
            "MCP Plugin: avviati come subprocess JSON-RPC 2.0 stdio</span>");
        hdrLay->addWidget(hdr, 1);

        auto* btnAllMcp  = new QPushButton("\xe2\x9c\x85  Tutti",   hdrRow);
        auto* btnNoneMcp = new QPushButton("\xe2\x96\xa1  Nessuno", hdrRow);
        btnAllMcp->setObjectName("actionBtn");
        btnNoneMcp->setObjectName("actionBtn");
        btnAllMcp->setFixedHeight(dpiScale(22));
        btnNoneMcp->setFixedHeight(dpiScale(22));
        hdrLay->addWidget(btnAllMcp);
        hdrLay->addWidget(btnNoneMcp);
        slowLay->addWidget(hdrRow);

        /* Scansiona MCPs/ */
        QStringList mcpNames;
        {
            const QString mcpsRoot = P::root() + "/MCPs";
            for (const QString& d :
                 QDir(mcpsRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
                if (QFileInfo::exists(mcpsRoot + "/" + d + "/server.py"))
                    mcpNames << d;
        }

        /* Barra di ricerca MCP */
        auto* mcpSearch = new QLineEdit(m_mcpPanel);
        mcpSearch->setPlaceholderText("\xf0\x9f\x94\x8d  Cerca MCP per nome o etichetta\xe2\x80\xa6");
        mcpSearch->setClearButtonEnabled(true);
        mcpSearch->setFixedHeight(dpiScale(26));
        slowLay->addWidget(mcpSearch);

        /* Scroll area per i ~50 MCP */
        auto* mcpScroll = new QScrollArea(m_mcpPanel);
        mcpScroll->setWidgetResizable(true);
        mcpScroll->setFrameShape(QFrame::NoFrame);
        mcpScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        mcpScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        auto* grid = new QWidget;
        auto* gl   = new QGridLayout(grid);
        gl->setContentsMargins(0, 2, 0, 2);
        gl->setSpacing(4);
        gl->setColumnStretch(0, 1);
        gl->setColumnStretch(1, 1);
        gl->setColumnStretch(2, 1);
        gl->setColumnStretch(3, 1);

        if (mcpNames.isEmpty()) {
            gl->addWidget(new QLabel(
                "<span style='color:#64748b;'>Nessun MCP in MCPs/</span>", grid),
                0, 0, 1, 4);
        } else {
            QSettings s("Prismalux", "GUI");
            const QStringList savedEnabled =
                s.value("tools/enabledMcps", mcpNames).toStringList();

            for (int i = 0; i < mcpNames.size(); ++i) {
                const QString& name = mcpNames[i];
                const bool en = savedEnabled.contains(name);
                if (en) m_enabledMcps.insert(name);

                auto* chk = new QCheckBox(
                    "\xf0\x9f\x94\x8c  " + mcpLabel(name) + "  (" + name + ")", grid);
                chk->setChecked(en);
                chk->setMinimumHeight(dpiScale(22));
                chk->setToolTip("MCPs/" + name + "/server.py\n"
                    "Processo Python separato (JSON-RPC 2.0 stdio).\n"
                    "Usalo da AppController \xe2\x86\x92 TinyMCP per chiamate dirette.");
                gl->addWidget(chk, i / 4, i % 4);

                connect(chk, &QCheckBox::toggled, this, [this, name](bool on){
                    if (on) m_enabledMcps.insert(name);
                    else    m_enabledMcps.remove(name);
                    QSettings s2("Prismalux", "GUI");
                    s2.setValue("tools/enabledMcps",
                        QStringList(m_enabledMcps.begin(), m_enabledMcps.end()));
                    onToolEnabledChanged();
                });
            }

            connect(btnAllMcp, &QPushButton::clicked, grid, [grid]{
                for (auto* c : grid->findChildren<QCheckBox*>()) c->setChecked(true);
            });
            connect(btnNoneMcp, &QPushButton::clicked, grid, [grid]{
                for (auto* c : grid->findChildren<QCheckBox*>()) c->setChecked(false);
            });

            /* Filtro ricerca MCP: riorganizza la grid senza buchi */
            connect(mcpSearch, &QLineEdit::textChanged, grid,
                    [grid, gl, &mcpNames, mcpLabel](const QString& raw){
                const QString q = raw.trimmed().toLower();
                const auto chks = grid->findChildren<QCheckBox*>(
                    QString(), Qt::FindDirectChildrenOnly);
                for (auto* c : chks) gl->removeWidget(c);
                int pos = 0;
                for (auto* c : chks) {
                    const bool match = q.isEmpty()
                        || c->text().toLower().contains(q)
                        || c->toolTip().toLower().contains(q);
                    c->setVisible(match);
                    if (match) { gl->addWidget(c, pos / 4, pos % 4); ++pos; }
                }
            });
        }

        mcpScroll->setWidget(grid);
        slowLay->addWidget(mcpScroll, 1);
    }

    lay->addWidget(m_mcpPanel);
}

/* ══════════════════════════════════════════════════════════════
   buildBottomBar — barra inferiore: 🔧 Tools · 🧠 Hermes · 🔄
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::buildBottomBar(QVBoxLayout* lay)
{
    auto* bar = new QWidget(this);
    bar->setObjectName("bottomBar");
    auto* bl  = new QHBoxLayout(bar);
    bl->setContentsMargins(0, 2, 0, 0);
    bl->setSpacing(6);

    bl->addStretch();

    lay->addWidget(bar);

    /* ── Connessioni ── */
    connect(m_hermesToggleBar, &QPushButton::toggled, this,
            [this](bool on){
        m_hermesToggleBar->setText(
            on ? "\xf0\x9f\xa7\xa0  Memoria persistente \xe2\x9c\x94"
               : "\xf0\x9f\xa7\xa0  Memoria persistente");
        if (m_hermesReflectBar) m_hermesReflectBar->setVisible(on);
        /* Sincronizza con il vecchio toggle per compatibilità */
        if (m_hermesToggle && m_hermesToggle->isChecked() != on)
            m_hermesToggle->setChecked(on);
        onHermesToggled(on);
    });

    connect(m_hermesReflectBar, &QPushButton::clicked,
            this, &AgentiPage::onHermesReflectClicked);

    /* Aggiorna label iniziale */
    updateToolsBtnLabel();
}

/* ── Aggiorna il testo dei due pulsanti Tool Veloci / Tool Lenti ── */
void AgentiPage::updateToolsBtnLabel()
{
    const int nT = static_cast<int>(m_enabledTools.size());
    const int nM = static_cast<int>(m_enabledMcps.size());

    if (m_btnToolsToggle) {
        m_btnToolsToggle->setText(
            nT == 0
            ? "\xe2\x9a\xa1  Tool Veloci  (disabilitati)"
            : QString("\xe2\x9a\xa1  Tool Veloci  (%1)").arg(nT));
    }
    if (m_btnMcpToggle) {
        m_btnMcpToggle->setText(
            nM == 0
            ? "\xf0\x9f\x94\x8c  Tool Lenti  (disabilitati)"
            : QString("\xf0\x9f\x94\x8c  Tool Lenti  (%1)").arg(nM));
    }
}

void AgentiPage::onToolsPanelToggle()
{
    const bool opening = m_btnToolsToggle && m_btnToolsToggle->isChecked();
    if (opening) {
        /* Chiudi Simboli e Tool Lenti */
        if (m_symbolsScrollArea) m_symbolsScrollArea->setVisible(false);
        if (m_symbolSearch) { m_symbolSearch->setVisible(false); m_symbolSearch->clear(); }
        if (m_btnMcpToggle && m_btnMcpToggle->isChecked()) {
            m_btnMcpToggle->blockSignals(true);
            m_btnMcpToggle->setChecked(false);
            m_btnMcpToggle->blockSignals(false);
            if (m_mcpPanel) m_mcpPanel->setVisible(false);
        }
    }
    if (m_toolsPanel) m_toolsPanel->setVisible(opening);
}

void AgentiPage::onMcpPanelToggle()
{
    const bool opening = m_btnMcpToggle && m_btnMcpToggle->isChecked();
    if (opening) {
        /* Chiudi Simboli e Tool Veloci */
        if (m_symbolsScrollArea) m_symbolsScrollArea->setVisible(false);
        if (m_symbolSearch) { m_symbolSearch->setVisible(false); m_symbolSearch->clear(); }
        if (m_btnToolsToggle && m_btnToolsToggle->isChecked()) {
            m_btnToolsToggle->blockSignals(true);
            m_btnToolsToggle->setChecked(false);
            m_btnToolsToggle->blockSignals(false);
            if (m_toolsPanel) m_toolsPanel->setVisible(false);
        }
    }
    if (m_mcpPanel) m_mcpPanel->setVisible(opening);
}

void AgentiPage::onToolEnabledChanged()
{
    updateToolsBtnLabel();
    m_toolsEnabled = !m_enabledTools.isEmpty();
    if (m_toolChk && m_toolChk->isChecked() != m_toolsEnabled)
        m_toolChk->setChecked(m_toolsEnabled);
}

void AgentiPage::onBubbleStyleChanged()
{
    if (!m_log || m_log->toPlainText().trimmed().isEmpty()) return;

    const int newBr = AppConfig::s().value(P::SK::kBubbleRadius, 10).toInt();
    const int scrollPos = m_log->verticalScrollBar()->value();

    QString html = m_log->toHtml();
    static const QRegularExpression reBr("border-radius:\\s*\\d+px");
    html.replace(reBr, QString("border-radius: %1px").arg(newBr));

    m_log->setHtml(html);
    m_log->verticalScrollBar()->setValue(scrollPos);
}

/* ══════════════════════════════════════════════════════════════
   TablePickerPopup — griglia hover stile LibreOffice/Word
   Nessun Q_OBJECT: usa std::function come callback.
   ══════════════════════════════════════════════════════════════ */
class TablePickerPopup : public QFrame {
    static constexpr int kRows = 8, kCols = 8, kCell = 24, kPad = 6, kLblH = 20;
    int m_hr = 0, m_hc = 0;
    std::function<void(int,int)> m_cb;
public:
    TablePickerPopup(QWidget* parent, std::function<void(int,int)> cb)
        : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
        , m_cb(std::move(cb))
    {
        setMouseTracking(true);
        setFixedSize(kPad*2 + kCols*kCell + 1,
                     kPad*2 + kRows*kCell + kLblH + 4);
        /* Stile segue palette sistema */
        setAutoFillBackground(true);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        /* Sfondo e bordo */
        p.fillRect(rect(), palette().window());
        p.setPen(QPen(palette().mid().color(), 1));
        p.drawRect(rect().adjusted(0,0,-1,-1));
        /* Label dimensione */
        const QString lbl = (m_hr > 0 && m_hc > 0)
            ? QString("%1 \xc3\x97 %2").arg(m_hc).arg(m_hr)   /* NxM */
            : tr("Tabella");
        p.setPen(palette().windowText().color());
        p.setFont(QFont(font().family(), 9, QFont::Bold));
        p.drawText(QRect(kPad, kPad, kCols*kCell, kLblH),
                   Qt::AlignCenter, lbl);
        /* Griglia */
        const int top = kPad + kLblH + 2;
        const QColor hlCol  = palette().highlight().color();
        const QColor bgCol  = palette().base().color();
        const QColor midCol = palette().mid().color();
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c) {
                QRect cell(kPad + c*kCell, top + r*kCell, kCell - 1, kCell - 1);
                p.fillRect(cell, (r < m_hr && c < m_hc) ? hlCol : bgCol);
                p.setPen(midCol);
                p.drawRect(cell);
            }
        }
    }
    void mouseMoveEvent(QMouseEvent* ev) override {
        const int top = kPad + kLblH + 2;
        m_hc = qBound(0, (ev->pos().x() - kPad) / kCell + 1, kCols);
        m_hr = qBound(0, (ev->pos().y() - top)  / kCell + 1, kRows);
        update();
    }
    void mousePressEvent(QMouseEvent*) override {
        if (m_hr > 0 && m_hc > 0 && m_cb) m_cb(m_hr, m_hc);
        close();
    }
    void leaveEvent(QEvent*) override { m_hr = m_hc = 0; update(); }
};

/* ══════════════════════════════════════════════════════════════
   AccentPickerPopup — popup accenti stile Apple (1 carattere)
   ══════════════════════════════════════════════════════════════ */
class AccentPickerPopup : public QFrame {
    std::function<void(const QString&)> m_cb;
public:
    AccentPickerPopup(QWidget* parent,
                      const QVector<QString>& variants,
                      std::function<void(const QString&)> cb)
        : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
        , m_cb(std::move(cb))
    {
        setAutoFillBackground(true);
        setStyleSheet(
            "QFrame{background:palette(window);border:1px solid palette(mid);"
            "border-radius:10px;}"
            "QPushButton{font-size:17px;min-width:42px;min-height:42px;"
            "max-width:42px;max-height:42px;"
            "border:none;border-radius:7px;background:transparent;}"
            "QPushButton:hover{background:palette(highlight);"
            "color:palette(highlighted-text);}");
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(7, 6, 7, 6);
        lay->setSpacing(2);
        for (const QString& v : variants) {
            auto* btn = new QPushButton(v, this);
            btn->setFont(QFont{});
            connect(btn, &QPushButton::clicked, this, [this, v](){
                m_cb(v); close();
            });
            lay->addWidget(btn);
        }
        adjustSize();
    }
};

/* Tabella accenti per lettera — minuscolo + maiuscolo */
static const QHash<QChar, QVector<QString>>& accentMap()
{
    static const QHash<QChar, QVector<QString>> m = {
        {'a', {"à","á","â","ã","ä","å","æ","ā","ă","ą"}},
        {'A', {"À","Á","Â","Ã","Ä","Å","Æ","Ā","Ă","Ą"}},
        {'e', {"è","é","ê","ë","ē","ě","ė","ę"}},
        {'E', {"È","É","Ê","Ë","Ē","Ě","Ė","Ę"}},
        {'i', {"ì","í","î","ï","ī","ĭ","į","ĩ"}},
        {'I', {"Ì","Í","Î","Ï","Ī","Ĭ","Į","Ĩ"}},
        {'o', {"ò","ó","ô","õ","ö","ø","œ","ō","ŏ"}},
        {'O', {"Ò","Ó","Ô","Õ","Ö","Ø","Œ","Ō","Ŏ"}},
        {'u', {"ù","ú","û","ü","ū","ű","ů","ų","ũ"}},
        {'U', {"Ù","Ú","Û","Ü","Ū","Ű","Ů","Ų","Ũ"}},
        {'n', {"ñ","ń","ṅ","ň","ŋ"}},
        {'N', {"Ñ","Ń","Ṅ","Ň","Ŋ"}},
        {'c', {"ç","ć","č","ĉ"}},
        {'C', {"Ç","Ć","Č","Ĉ"}},
        {'s', {"ś","š","ş","ŝ","ß"}},
        {'S', {"Ś","Š","Ş","Ŝ"}},
        {'z', {"ź","ž","ż"}},
        {'Z', {"Ź","Ž","Ż"}},
        {'y', {"ÿ","ý"}},
        {'Y', {"Ÿ","Ý"}},
        {'l', {"ł","ĺ","ľ","ļ"}},
        {'L', {"Ł","Ĺ","Ľ","Ļ"}},
        {'d', {"đ","ð","ď"}},
        {'D', {"Đ","Ð","Ď"}},
        {'t', {"ț","ţ","ť","þ"}},
        {'T', {"Ț","Ţ","Ť","Þ"}},
        {'r', {"ř","ŗ","ŕ"}},
        {'R', {"Ř","Ŗ","Ŕ"}},
        {'g', {"ğ","ĝ","ġ","ģ"}},
        {'G', {"Ğ","Ĝ","Ġ","Ģ"}},
        {'h', {"ĥ","ħ"}},
        {'H', {"Ĥ","Ħ"}},
        {'k', {"ķ","ḳ"}},
        {'K', {"Ķ","Ḳ"}},
    };
    return m;
}

/* ══════════════════════════════════════════════════════════════
   buildInputFormatBar — mini-toolbar formattazione testo
   Appare sopra la selezione nel campo input come frame flottante.
   Parent = this per non essere clippato da m_input.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::buildInputFormatBar()
{
    m_fmtBar = new QFrame(this);
    m_fmtBar->setObjectName("fmtBar");
    m_fmtBar->setFrameShape(QFrame::StyledPanel);
    m_fmtBar->setFrameShadow(QFrame::Raised);
    m_fmtBar->setAutoFillBackground(true);
    /* Stile minimo: solo bordo arrotondato e hover — il resto segue la palette */
    m_fmtBar->setStyleSheet(
        "QFrame#fmtBar{border:1px solid palette(mid);border-radius:6px;"
        "background:palette(window);}"
        "QPushButton{background:transparent;border:none;padding:2px 7px;"
        "border-radius:3px;min-width:20px;font-size:12px;}"
        "QPushButton:hover{background:palette(highlight);"
        "color:palette(highlighted-text);}"
        "QPushButton:pressed{background:palette(dark);}");
    m_fmtBar->setVisible(false);

    auto* vlay = new QVBoxLayout(m_fmtBar);
    vlay->setContentsMargins(6, 5, 6, 5);
    vlay->setSpacing(4);

    /* ── Riga pulsanti con tile verticali (nome sopra, simbolo sotto) ── */
    auto* btnRow = new QWidget(m_fmtBar);
    auto* lay    = new QHBoxLayout(btnRow);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(3);
    vlay->addWidget(btnRow);

    /* Separatore verticale adattivo */
    auto addSep = [&](){
        auto* sep = new QFrame(btnRow);
        sep->setFrameShape(QFrame::VLine);
        sep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        lay->addWidget(sep);
    };

    /* Tile verticale: nome piccolo sopra + pulsante simbolo sotto, entrambi centrati */
    auto addTile = [&](const QString& nome, const QString& sym, const QString& tip,
                       const QString& bef, const QString& aft,
                       const QString& symStyle = {}) -> QPushButton*
    {
        auto* tile = new QWidget(btnRow);
        auto* tl   = new QVBoxLayout(tile);
        tl->setContentsMargins(0, 0, 0, 0);
        tl->setSpacing(1);

        auto* lbl = new QLabel(nome, tile);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("font-size:9px;color:palette(mid);");
        tl->addWidget(lbl);

        auto* btn = new QPushButton(sym, tile);
        btn->setToolTip(tip);
        btn->setFixedWidth(lbl->fontMetrics().horizontalAdvance(nome) + 14);
        QString ss = "QPushButton{background:transparent;border:none;border-radius:3px;"
                     "font-size:13px;" + symStyle + "}"
                     "QPushButton:hover{background:palette(highlight);"
                     "color:palette(highlighted-text);}"
                     "QPushButton:pressed{background:palette(dark);}";
        btn->setStyleSheet(ss);
        tl->addWidget(btn, 0, Qt::AlignHCenter);

        connect(btn, &QPushButton::clicked, m_fmtBar,
                [this, bef, aft]{ onFmtBtnClicked(bef, aft); });
        lay->addWidget(tile);
        return btn;
    };

    /* ── Gruppo 1: grassetto / corsivo / sottolineato ── */
    /* Grassetto / Corsivo / Sottolineato → QTextCharFormat (rich text nativo) */
    auto addRichTile = [&](const QString& nome, const QString& sym,
                           const QString& tip,  const QString& symStyle,
                           auto applyFmt) {
        auto* tile = new QWidget(btnRow);
        auto* tl   = new QVBoxLayout(tile);
        tl->setContentsMargins(0,0,0,0); tl->setSpacing(1);
        auto* lbl  = new QLabel(nome, tile);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("font-size:9px;color:palette(mid);");
        tl->addWidget(lbl);
        auto* btn  = new QPushButton(sym, tile);
        btn->setFixedWidth(lbl->fontMetrics().horizontalAdvance(nome) + 14);
        btn->setToolTip(tip);
        btn->setStyleSheet(
            QString("QPushButton{background:transparent;border:none;border-radius:3px;"
                    "font-size:14px;%1}"
                    "QPushButton:hover{background:palette(highlight);"
                    "color:palette(highlighted-text);}").arg(symStyle));
        tl->addWidget(btn, 0, Qt::AlignHCenter);
        lay->addWidget(tile);
        connect(btn, &QPushButton::clicked, this, [this, applyFmt](){
            if (!m_input) return;
            QTextCursor cur = m_input->textCursor();
            if (!cur.hasSelection()) return;
            applyFmt(cur);
            m_input->setTextCursor(cur);
            if (m_fmtBar) m_fmtBar->hide();
            m_input->setFocus();
        });
    };
    addRichTile("Grassetto","B","Grassetto","font-weight:bold;",
        [](QTextCursor& cur){
            QTextCharFormat f;
            f.setFontWeight(cur.charFormat().fontWeight() >= QFont::Bold
                            ? QFont::Normal : QFont::Bold);
            cur.mergeCharFormat(f);
        });
    addRichTile("Corsivo","I","Corsivo","font-style:italic;",
        [](QTextCursor& cur){
            QTextCharFormat f;
            f.setFontItalic(!cur.charFormat().fontItalic());
            cur.mergeCharFormat(f);
        });
    addRichTile("Sott.","U","Sottolineato","text-decoration:underline;",
        [](QTextCursor& cur){
            QTextCharFormat f;
            f.setFontUnderline(!cur.charFormat().fontUnderline());
            cur.mergeCharFormat(f);
        });

    /* ── Gruppo 2: allineamento ── */
    addSep();
    addTile("Sinistra", "\xe2\x86\x90", "Allinea a sinistra",  "<div align=\"left\">",    "</div>");
    addTile("Centro",   "\xe2\x86\x94", "Centra",               "<div align=\"center\">",  "</div>");
    addTile("Destra",   "\xe2\x86\x92", "Allinea a destra",     "<div align=\"right\">",   "</div>");
    addTile("Giustif.", "\xe2\x89\xa1", "Giustifica",            "<div align=\"justify\">", "</div>");

    /* ── Gruppo 3: citazione / codice ── */
    addSep();
    addTile("Citazione", "\"\"",  "Citazione (> testo)",  "> ",    "",      "");
    addTile("Codice",    "`c`",   "Codice inline",         "`",     "`",     "font-family:monospace;font-size:11px;");
    addTile("Blocco",    "{ }",   "Blocco codice",  "```\n", "\n```", "font-family:monospace;font-size:11px;");

    /* ── Gruppo 4: colore testo / sfondo ── */
    addSep();
    auto makeFgStyle = [](const QColor& c) {
        return QString("QPushButton{background:transparent;border:none;border-radius:3px;"
                       "font-size:14px;font-weight:bold;border-bottom:3px solid %1;}"
                       "QPushButton:hover{background:palette(highlight);"
                       "color:palette(highlighted-text);}").arg(c.name());
    };
    auto makeBgStyle = [](const QColor& c) {
        const QString fg = c.lightness() > 128 ? "#111" : "#eee";
        return QString("QPushButton{border:1px solid palette(mid);border-radius:3px;"
                       "font-size:11px;background:%1;color:%2;padding:1px 5px;}"
                       "QPushButton:hover{background:palette(highlight);"
                       "color:palette(highlighted-text);}").arg(c.name(), fg);
    };
    {
        /* Tile Colore testo */
        auto* fgTile = new QWidget(btnRow);
        auto* ftl    = new QVBoxLayout(fgTile);
        ftl->setContentsMargins(0,0,0,0); ftl->setSpacing(1);
        auto* fgLbl = new QLabel("Colore", fgTile);
        fgLbl->setAlignment(Qt::AlignCenter);
        fgLbl->setStyleSheet("font-size:9px;color:palette(mid);");
        ftl->addWidget(fgLbl);
        m_btnFmtFg = new QPushButton("A", fgTile);
        m_btnFmtFg->setFixedWidth(fgLbl->fontMetrics().horizontalAdvance("Colore") + 14);
        m_btnFmtFg->setStyleSheet(makeFgStyle(m_fmtFgColor));
        m_btnFmtFg->setToolTip("Colore testo (apre selettore colore)");
        ftl->addWidget(m_btnFmtFg, 0, Qt::AlignHCenter);
        lay->addWidget(fgTile);
        connect(m_btnFmtFg, &QPushButton::clicked, this, [this, makeFgStyle](){
            const QColor c = QColorDialog::getColor(m_fmtFgColor, this, "Colore testo");
            if (!c.isValid()) return;
            m_fmtFgColor = c;
            m_btnFmtFg->setStyleSheet(makeFgStyle(c));
            if (!m_input) return;
            QTextCursor cur = m_input->textCursor();
            if (!cur.hasSelection()) return;
            QTextCharFormat fmt; fmt.setForeground(c);
            cur.mergeCharFormat(fmt);
            m_input->setTextCursor(cur);
            if (m_fmtBar) m_fmtBar->hide();
            m_input->setFocus();
        });
    }
    {
        /* Tile Sfondo testo */
        auto* bgTile = new QWidget(btnRow);
        auto* btl    = new QVBoxLayout(bgTile);
        btl->setContentsMargins(0,0,0,0); btl->setSpacing(1);
        auto* bgLbl = new QLabel("Sfondo", bgTile);
        bgLbl->setAlignment(Qt::AlignCenter);
        bgLbl->setStyleSheet("font-size:9px;color:palette(mid);");
        btl->addWidget(bgLbl);
        m_btnFmtBg = new QPushButton("A", bgTile);
        m_btnFmtBg->setFixedWidth(bgLbl->fontMetrics().horizontalAdvance("Sfondo") + 14);
        m_btnFmtBg->setStyleSheet(makeBgStyle(m_fmtBgColor));
        m_btnFmtBg->setToolTip("Colore sfondo testo (apre selettore colore)");
        btl->addWidget(m_btnFmtBg, 0, Qt::AlignHCenter);
        lay->addWidget(bgTile);
        connect(m_btnFmtBg, &QPushButton::clicked, this, [this, makeBgStyle](){
            const QColor c = QColorDialog::getColor(m_fmtBgColor, this, "Colore sfondo");
            if (!c.isValid()) return;
            m_fmtBgColor = c;
            m_btnFmtBg->setStyleSheet(makeBgStyle(c));
            if (!m_input) return;
            QTextCursor cur = m_input->textCursor();
            if (!cur.hasSelection()) return;
            QTextCharFormat fmt; fmt.setBackground(c);
            cur.mergeCharFormat(fmt);
            m_input->setTextCursor(cur);
            if (m_fmtBar) m_fmtBar->hide();
            m_input->setFocus();
        });
    }

    /* ── Gruppo 5: tabella ── */
    addSep();
    QPushButton* btnTbl = nullptr;
    {
        auto* tblTile = new QWidget(btnRow);
        auto* ttl     = new QVBoxLayout(tblTile);
        ttl->setContentsMargins(0, 0, 0, 0);
        ttl->setSpacing(1);
        auto* tblLbl = new QLabel("Tabella", tblTile);
        tblLbl->setAlignment(Qt::AlignCenter);
        tblLbl->setStyleSheet("font-size:9px;color:palette(mid);");
        ttl->addWidget(tblLbl);
        btnTbl = new QPushButton("\xe2\x8a\x9e", tblTile);
        btnTbl->setFixedWidth(tblLbl->fontMetrics().horizontalAdvance("Tabella") + 14);
        btnTbl->setStyleSheet(
            "QPushButton{background:transparent;border:none;border-radius:3px;font-size:13px;}"
            "QPushButton:hover{background:palette(highlight);color:palette(highlighted-text);}");
        btnTbl->setToolTip("Inserisci tabella — scegli dimensioni con la griglia");
        ttl->addWidget(btnTbl, 0, Qt::AlignHCenter);
        lay->addWidget(tblTile);
    }

    /* ── Hint accenti (sotto i pulsanti) ── */
    auto* hintLbl = new QLabel(
        "<span style='font-size:9px;'>"
        "Seleziona una lettera per vedere gli accenti disponibili"
        "</span>", m_fmtBar);
    hintLbl->setTextFormat(Qt::RichText);
    hintLbl->setStyleSheet("color:palette(mid);");
    hintLbl->setAlignment(Qt::AlignLeft);
    vlay->addWidget(hintLbl);

    connect(btnTbl, &QPushButton::clicked, m_fmtBar, [this, btnTbl](){
        auto* picker = new TablePickerPopup(this,
            [this](int rows, int cols){
                /* Genera tabella Markdown rows x cols */
                QString tbl = "\n";
                tbl += "|";
                for (int c = 0; c < cols; ++c)
                    tbl += QString(" Col%1 |").arg(c + 1);
                tbl += "\n|";
                for (int c = 0; c < cols; ++c)
                    tbl += " --- |";
                tbl += "\n";
                for (int r = 0; r < rows - 1; ++r) {
                    tbl += "|";
                    for (int c = 0; c < cols; ++c)
                        tbl += "  |";
                    tbl += "\n";
                }
                if (m_input) {
                    m_input->insertPlainText(tbl);
                    m_input->setFocus();
                }
            });
        /* Mostra il picker sotto il pulsante Tabella */
        picker->move(btnTbl->mapToGlobal(
            QPoint(0, btnTbl->height() + 2)));
        picker->show();
    });

    connect(m_input, &QTextEdit::selectionChanged,
            this, &AgentiPage::onInputSelectionChanged);
}

void AgentiPage::onInputSelectionChanged()
{
    if (!m_fmtBar || !m_input) return;
    const QTextCursor cur = m_input->textCursor();
    if (!cur.hasSelection()) {
        m_fmtBar->hide();
        return;
    }

    /* ── Popup accenti stile Apple: 1 solo carattere ── */
    const QString selText = cur.selectedText();
    if (selText.size() == 1) {
        const auto& am = accentMap();
        const auto it  = am.constFind(selText[0]);
        if (it != am.constEnd()) {
            m_fmtBar->hide();
            QTextCursor startCur = cur;
            startCur.setPosition(cur.selectionStart());
            const QRect cr = m_input->cursorRect(startCur);

            QTextCursor capCur = cur;
            auto* picker = new AccentPickerPopup(
                this, it.value(),
                [this, capCur](const QString& v) mutable {
                    capCur.insertText(v);
                    m_input->setTextCursor(capCur);
                });

            const QSize ps = picker->sizeHint();
            QPoint gpos    = m_input->mapToGlobal(cr.topLeft());
            gpos.setY(gpos.y() - ps.height() - 6);
            gpos.setX(qMax(gpos.x() - ps.width() / 2, 4));
            picker->move(gpos);
            picker->show();
            return;
        }
    }

    /* ── Toolbar formattazione: selezione di più caratteri ── */
    m_fmtBar->adjustSize();
    const QSize hint = m_fmtBar->sizeHint();
    const int fh = hint.height();
    const int fw = hint.width();

    /* Coordinate cursore in m_input → convertite a questo widget (parent della fmtBar) */
    QTextCursor startCur = cur;
    startCur.setPosition(cur.selectionStart());
    const QRect cr      = m_input->cursorRect(startCur);
    const QPoint origin = m_input->mapTo(this, cr.topLeft());

    int x = origin.x();
    int y = origin.y() - fh - 6;
    /* Se non c'è spazio sopra, metti sotto la selezione */
    if (y < 2)  y = m_input->mapTo(this, cr.bottomLeft()).y() + 4;
    /* Clampa orizzontalmente entro la pagina */
    if (x + fw > width() - 4)  x = width() - fw - 4;
    if (x < 2)                  x = 2;

    m_fmtBar->setGeometry(x, y, fw, fh);
    m_fmtBar->raise();
    m_fmtBar->show();
}

/* ══════════════════════════════════════════════════════════════
   onSymbolSearchChanged — filtra simboli nel pannello ricerca
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onSymbolSearchChanged(const QString& query)
{
    if (!m_symbolSearch || !m_symbolSearchGrid || !m_symbolSearchPanel) return;

    auto* searchScroll = qobject_cast<QScrollArea*>(
        m_symbolSearch->property("resultsScroll").value<QObject*>());

    const QString q = query.trimmed().toLower();
    const bool searching = !q.isEmpty();

    /* Mostra pannello categorie o risultati ricerca */
    if (m_symbolsScrollArea) m_symbolsScrollArea->setVisible(!searching);
    if (searchScroll)        searchScroll->setVisible(searching);

    if (!searching) return;

    /* Svuota la grid risultati precedenti */
    while (QLayoutItem* item = m_symbolSearchGrid->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    const int BTN_W = dpiScale(38);
    const int BTN_H = dpiScale(26);
    const int perRow = std::max(4, 760 / (BTN_W + 2));

    int col = 0, row = 0;
    int found = 0;
    for (const auto& pair : std::as_const(m_allSymbols)) {
        if (!pair.second.contains(q, Qt::CaseInsensitive)
            && !pair.first.contains(q, Qt::CaseInsensitive))
            continue;
        if (col >= perRow) { col = 0; ++row; }
        auto* b = new QPushButton(pair.first, m_symbolSearchPanel);
        b->setObjectName("symbolBtn");
        b->setFixedSize(BTN_W, BTN_H);
        b->setToolTip(pair.second);
        b->setProperty("symbol", pair.first);
        connect(b, &QPushButton::clicked, this, &AgentiPage::onSymbolBtnClicked);
        m_symbolSearchGrid->addWidget(b, row, col++);
        if (++found >= 300) break;  /* limite sicurezza */
    }

    if (found == 0) {
        auto* lbl = new QLabel(
            tr("Nessun simbolo trovato per \"") + query.toHtmlEscaped() + "\"",
            m_symbolSearchPanel);
        lbl->setStyleSheet("color:#6b7280;font-size:12px;padding:8px;");
        m_symbolSearchGrid->addWidget(lbl, 0, 0, 1, perRow);
    }

    m_symbolSearchPanel->adjustSize();
}

void AgentiPage::onFmtBtnClicked(const QString& before, const QString& after)
{
    if (!m_input) return;
    QTextCursor cur = m_input->textCursor();
    if (!cur.hasSelection()) return;
    /* QTextEdit usa U+2029 (paragrafo) come separatore di riga nella selezione */
    const QString sel = cur.selectedText().replace(QChar(0x2029), '\n');
    QString result;
    if (before == "> " && after.isEmpty()) {
        /* Citazione: prefissa > a ogni riga della selezione */
        const QStringList lines = sel.split('\n');
        QStringList quoted;
        for (const QString& line : lines)
            quoted += "> " + line;
        result = quoted.join('\n');
    } else {
        result = before + sel + after;
    }
    cur.insertText(result);
    m_input->setTextCursor(cur);
    if (m_fmtBar) m_fmtBar->hide();
    m_input->setFocus();
}


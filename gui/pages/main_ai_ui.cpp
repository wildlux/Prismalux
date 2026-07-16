#include "main_ai.h"
#include "main_ai_p.h"
#include "../dpi_utils.h"
#include "../widgets/latex_view.h"
#include "../widgets/chat_log_browser.h"
#include <QPainter>
#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include <QMessageBox>
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
#include <QPlainTextEdit>
#include <QComboBox>
#include <QCheckBox>
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
#include <QInputDialog>
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

    m_btnTtsStop = new QPushButton(tr("\xe2\x8f\xb9 Ferma lettura"), toolbar);
    m_btnTtsStop->setObjectName("actionBtn");
    m_btnTtsStop->setToolTip(tr("Interrompi la lettura vocale"));
    m_btnTtsStop->setVisible(false);
    toolLay->addWidget(m_btnTtsStop);
    connect(m_btnTtsStop, &QPushButton::clicked, this, &AgentiPage::onTtsStopClicked);

    m_btnTtsPause = new QPushButton(tr("\xe2\x8f\xb8  Pausa"), toolbar);
    m_btnTtsPause->setObjectName("actionBtn");
    m_btnTtsPause->setToolTip(tr("Metti in pausa / riprendi la lettura vocale"));
    m_btnTtsPause->setVisible(false);
    toolLay->addWidget(m_btnTtsPause);
    connect(m_btnTtsPause, &QPushButton::clicked, this, &AgentiPage::onTtsPauseClicked);
}

/* ── Sezione export: Esporta + PDF + Memoria + Info ── */
void AgentiPage::buildToolbarExportSection(QHBoxLayout* toolLay, QWidget* toolbar)
{
    auto* btnExport = new QPushButton(tr("\xf0\x9f\x92\xbe  Esporta"), toolbar);
    btnExport->setObjectName("actionBtn");
    btnExport->setToolTip(tr("Esporta conversazione (.md / .html / .txt)"));
    toolLay->addWidget(btnExport);
    connect(btnExport, &QPushButton::clicked, this, &AgentiPage::onBtnExportClicked);

    auto* btnExportPdf = new QPushButton(tr("\xf0\x9f\x93\x84  PDF"), toolbar);
    btnExportPdf->setObjectName("actionBtn");
    btnExportPdf->setToolTip(tr("Esporta conversazione (.pdf)"));
    toolLay->addWidget(btnExportPdf);
    connect(btnExportPdf, &QPushButton::clicked, this, &AgentiPage::onBtnExportPdfClicked);

    m_btnKnowledge = new QPushButton(tr("\xf0\x9f\x93\x96  Memoria"), toolbar);  /* 📖 */
    m_btnKnowledge->setObjectName("actionBtn");
    m_btnKnowledge->setToolTip(
        tr("Salva risposta in user_knowledge.md\n"
        "Il testo viene iniettato nel context di ogni sessione AI futura."));
    toolLay->addWidget(m_btnKnowledge);
    connect(m_btnKnowledge, &QPushButton::clicked, this, &AgentiPage::onSaveKnowledge);

    m_btnEtimo = new QPushButton(tr("\xf0\x9f\x8f\x9b  Etimo"), toolbar);  /* 🏛 */
    m_btnEtimo->setObjectName("actionBtn");
    m_btnEtimo->setCheckable(true);
    m_btnEtimo->setToolTip(
        "Modalita' Dizionario Etimologico\n"
        "L'AI risponde in stile Wikipedia: origine greca/latina,\n"
        "morfologia, evoluzione semantica, derivati moderni.");
    toolLay->addWidget(m_btnEtimo);
    connect(m_btnEtimo, &QPushButton::toggled, this, &AgentiPage::onEtimoToggled);

    m_btnMathToggle = new QPushButton(tr("\xe2\x88\x91  Formule"), toolbar);  /* ∑ */
    m_btnMathToggle->setObjectName("actionBtn");
    m_btnMathToggle->setCheckable(true);
    m_btnMathToggle->setToolTip(
        "Pannello formule matematiche LaTeX\n"
        "Preview in tempo reale + template per frazioni, limiti,\n"
        "sommatorie, integrali, radici. Auto-sostituzione mentre si scrive.");
    toolLay->addWidget(m_btnMathToggle);
    connect(m_btnMathToggle, &QPushButton::toggled, this, &AgentiPage::onMathToggleToggled);

    auto* btnInfo = new QPushButton(tr("\xe2\x84\xb9  Informazioni"), toolbar);  /* ℹ */
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
        tr("Conversazione vocale continua (loop)\n"
        "Parla \xe2\x80\x94 AI risponde \xe2\x80\x94 ascolta \xe2\x80\x94 riparla\n"
        "Richiede whisper.cpp + TTS configurati nelle Impostazioni"));
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
    auto* llmLbl = new QPushButton(tr("LLM:"), toolbar);
    llmLbl->setFlat(true);
    llmLbl->setCursor(Qt::PointingHandCursor);
    llmLbl->setObjectName("cardDesc");
    llmLbl->setStyleSheet(
        "QPushButton{border:none;padding:0 2px;text-decoration:underline;}"
        "QPushButton:hover{color:#60a5fa;}");
    llmLbl->setToolTip(tr("Clicca per eseguire 'ollama list' e aggiornare i modelli"));

    m_cmbLLM = new QComboBox(toolbar);
    m_cmbLLM->setObjectName("settingsCombo");
    m_cmbLLM->setMinimumWidth(dpiScale(240));
    m_cmbLLM->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_cmbLLM->setToolTip(tr("Seleziona il modello AI da usare."));
    m_cmbLLM->setAccessibleName(tr("Selettore modello AI"));
    m_cmbLLM->addItem("(caricamento...)");
    toolLay->addWidget(llmLbl);
    toolLay->addWidget(m_cmbLLM);

    /* D-27: routing automatico dominio→modello — default ON: instradare a
     * un modello specializzato riduce il carico sul modello generale.
     * Non tocca la selezione visibile nel combo (usa AiClient::setModel(),
     * non setBackend(): nessun modelChanged() emesso) — instrada solo la
     * singola richiesta a un modello coder/vision installato quando serve. */
    m_chkAutoRouting = new QCheckBox(tr("\xf0\x9f\xa7\xad Auto"), toolbar);
    m_chkAutoRouting->setObjectName("cardDesc");
    m_chkAutoRouting->setToolTip(tr(
        "Routing automatico: per domande di codice usa un modello coder installato "
        "(se presente), per immagini allegate un modello vision — senza cambiare "
        "la tua selezione nel menu a tendina."));
    m_chkAutoRouting->setChecked(
        QSettings("Prismalux", "GUI").value(P::SK::kAutoModelRouting, true).toBool());
    connect(m_chkAutoRouting, &QCheckBox::toggled, this, [](bool on) {
        QSettings("Prismalux", "GUI").setValue(P::SK::kAutoModelRouting, on);
    });
    toolLay->addWidget(m_chkAutoRouting);

    connect(llmLbl, &QPushButton::clicked, this, [this, toolbar] {
        /* Esegue ollama list e mostra l'output in un popup */
        QProcess proc;
        proc.start("ollama", {"list"});
        proc.waitForFinished(5000);
        const QString out = QString::fromLocal8Bit(proc.readAllStandardOutput()).trimmed();
        const QString err = QString::fromLocal8Bit(proc.readAllStandardError()).trimmed();

        auto* dlg = new QDialog(toolbar->window());
        dlg->setWindowTitle(tr("\xf0\x9f\xa6\x99  ollama list"));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->resize(dpiScale(620), dpiScale(280));
        auto* lay = new QVBoxLayout(dlg);
        lay->setContentsMargins(12, 12, 12, 12);
        auto* view = new QPlainTextEdit(dlg);
        view->setReadOnly(true);
        view->setFont(QFont("Monospace", 9));
        view->setObjectName("chatLog");
        view->setPlainText(out.isEmpty() ? (err.isEmpty() ? "(nessun output)" : err) : out);
        lay->addWidget(view);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
        connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::close);
        auto* refreshBtn = bb->addButton(tr("Aggiorna combo"), QDialogButtonBox::ActionRole);
        refreshBtn->setObjectName("actionBtn");
        connect(refreshBtn, &QPushButton::clicked, this, [this, dlg] {
            m_ai->invalidateModelCache();
            ModelComboHelper::connectFetch(m_ai, this, m_cmbLLM);
            dlg->close();
        });
        lay->addWidget(bb);
        dlg->show();

        /* Aggiorna sempre il combo in background */
        m_ai->invalidateModelCache();
        ModelComboHelper::connectFetch(m_ai, this, m_cmbLLM);
    });
    connect(m_cmbLLM, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentiPage::onCmbLLMIndexChanged);

    /* Pulsante "🔄 Rigenera con [modello]" — appare quando il modello cambia
       con la chat non vuota, per permettere di rilanciare l'ultimo task col nuovo LLM. */
    m_btnRegen = new QPushButton(tr("\xf0\x9f\x94\x84 Rigenera"), toolbar);
    m_btnRegen->setObjectName("actionBtn");
    m_btnRegen->setToolTip(
        tr("Reinvia l'ultimo messaggio utente con il modello appena selezionato"));
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
    m_log = new ChatLogBrowser(this);   /* QTextBrowser + angoli bolle arrotondati */
    m_log->setObjectName("chatLog");
    m_log->setReadOnly(true);
    m_log->setOpenLinks(false);
    m_log->setOpenExternalLinks(false);
    m_log->document()->setDefaultStyleSheet("body { color:#e2e8f0; }");
    m_log->setPlaceholderText(
        tr("L'output degli agenti appare qui...\n\n"
        "\xf0\x9f\x8d\xba Invocazione riuscita. Gli dei ascoltano."));
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

    m_btnChartOpen = new QPushButton(tr("\xf0\x9f\x93\x88  Apri nel Grafico"), m_chartPanel);
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
    m_input->setAccessibleName(tr("Campo messaggio chat"));
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

    m_btnRun = new QPushButton(tr("\xf0\x9f\x93\xa4 Invia"), inputArea);
    m_btnRun->setObjectName("actionBtn");
    m_btnRun->setProperty("bigBtn", "true");
    m_btnRun->setVisible(false);   /* hub ovale TriModeButton è l'azione visiva */
    m_btnRun->setToolTip(
        tr("Risposta immediata con contesto RAG \xe2\x80\x94 1 solo agente (Invio)\n"
        "Stop da fermo \xe2\x86\x92 cambia modalit\xc3\xa0 (Invia \xe2\x86\x94 Avvia)"));
    m_btnRun->setAccessibleName(tr("Avvia o ferma la risposta AI"));
    tagExec(m_btnRun, "\xf0\x9f\x93\xa4", "Invia");

    m_btnVoice = new QPushButton(tr("\xf0\x9f\x8e\xa4 Trascrivi parlato"), inputArea);
    m_btnVoice->setObjectName("actionBtn");
    m_btnVoice->setToolTip(tr("Parla — trascrivi la voce nel campo di testo (whisper.cpp)"));
    m_btnVoice->setAccessibleName(tr("Trascrivi voce in testo"));
    tagExec(m_btnVoice, "\xf0\x9f\x8e\xa4", "Trascrivi parlato");

    auto* btnSymbols = new QPushButton(tr("\xce\xa9  Simboli"), inputArea);
    btnSymbols->setObjectName("actionBtn");
    btnSymbols->setToolTip(tr("Inserisci caratteri speciali: matematica, greco, lingue"));
    btnSymbols->setAccessibleName(tr("Inserisci simbolo speciale"));

    m_btnDoc = new QPushButton(tr("\xf0\x9f\x93\x8e  Allega file"), inputArea);
    m_btnDoc->setObjectName("actionBtn");
    m_btnDoc->setToolTip(tr(
        "Allega un file alla chat.\n"
        "Documenti: .txt .md .csv .json .py .cpp .h .pdf .xls\n"
        "Immagini:  .png .jpg .jpeg .gif .webp (richiede modello vision)"));
    m_btnDoc->setAccessibleName(tr("Allega file al messaggio"));
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
    m_modeBtn->setActionText(tr("\xf0\x9f\x93\xa4 Invia"));
    connect(m_modeBtn, &TriModeButton::modeChanged,   this, &AgentiPage::onModeBtnChanged);
    connect(m_modeBtn, &TriModeButton::actionClicked, this, &AgentiPage::onBtnRunClicked);

    /* Col 3: ⚡ Tool Veloci (r0) · 🔌 Tool Lenti (r1) · Memoria (r2) */
    inputGrid->setColumnStretch(3, 0);

    m_btnToolsToggle = new QPushButton(tr("\xe2\x9a\xa1  Tool Veloci"), inputArea);
    m_btnToolsToggle->setCheckable(true);
    m_btnToolsToggle->setObjectName("actionBtn");
    m_btnToolsToggle->setToolTip(
        tr("Apri/chiudi i Tool Veloci (Function Tools).\n"
        "Eseguiti in-process, risposta < 1ms.\n"
        "Calcola, cerca online, leggi file, Python, RAG\xe2\x80\xa6"));
    inputGrid->addWidget(m_btnToolsToggle, 0, 3);
    connect(m_btnToolsToggle, &QPushButton::toggled,
            this, &AgentiPage::onToolsPanelToggle);

    m_btnMcpToggle = new QPushButton(tr("\xf0\x9f\x94\x8c  Tool Lenti (MCP)"), inputArea);
    m_btnMcpToggle->setCheckable(true);
    m_btnMcpToggle->setObjectName("actionBtn");
    m_btnMcpToggle->setToolTip(
        tr("Apri/chiudi i Tool Lenti \xe2\x80\x94 MCP Plugin.\n"
        "Avviati come processo separato (JSON-RPC 2.0 stdio).\n"
        "Latenza maggiore ma accesso a strumenti esterni."));
    inputGrid->addWidget(m_btnMcpToggle, 1, 3);
    connect(m_btnMcpToggle, &QPushButton::toggled,
            this, &AgentiPage::onMcpPanelToggle);

    /* Cella composita: [Memoria persistente][⌛] — riga 2 */
    auto* hermesCell = new QWidget(inputArea);
    auto* hermesCellLay = new QHBoxLayout(hermesCell);
    hermesCellLay->setContentsMargins(0, 0, 0, 0);
    hermesCellLay->setSpacing(2);

    m_hermesToggleBar = new QPushButton(tr("\xf0\x9f\xa7\xa0  Memoria persistente"), hermesCell);
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
        tr("\xf0\x9f\x93\x84  Trascina qui PDF / .txt / .md\n"
        "per indicizzarli nel RAG"),
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

    auto* btnAddUrl = new QPushButton(tr("\xe2\x9e\x95  Aggiungi URL"), row3);  /* ➕ */
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
            tr("\xf0\x9f\x93\x84  Rilascia per indicizzare nel RAG..."));
}

void AgentiPage::onRagDropZoneLeave()
{
    if (m_ragDropZone && !m_ragIngesting)
        m_ragDropZone->setText(
            tr("\xf0\x9f\x93\x84  Trascina qui PDF / .txt / .md\n"
            "per indicizzarli nel RAG"));
}

void AgentiPage::onRagIngestionDone()
{
    m_ragIngesting = false;
    if (m_ragDropZone)
        m_ragDropZone->setText(
            tr("\xf0\x9f\x93\x84  Trascina qui PDF / .txt / .md\n"
            "per indicizzarli nel RAG"));
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

    /* Link "Cosa sai fare?" — stesso meccanismo "prova:" della tabella help:
       clic = inserisce la domanda nella casella e la invia (risposta locale
       con l'elenco completo delle funzioni, zero token). */
    const QString askB64 = QString::fromLatin1(
        QByteArray("cosa sai fare?").toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    auto* hintLbl = new QLabel(
        tr("\xe2\x8c\xa8  <b>Invio</b> = esegui &nbsp;\xc2\xb7&nbsp; "
        "<b>Shift+Invio</b> = a capo &nbsp;\xc2\xb7&nbsp; "
        "<b>Stop da fermo</b> = cambia Chat \xe2\x86\x94 Avvia<br>"
        "\xf0\x9f\x92\xa1  Chiedi <a href='prova:%1' style='color:#3b82f6;"
        "text-decoration:none;'><b>\xc2\xab" "Cosa sai fare?\xc2\xbb</b></a> "
        "per l'elenco di tutto quello che posso fare").arg(askB64),
        m_hintWidget);
    hintLbl->setObjectName("footerHints");
    hintLbl->setWordWrap(false);
    hintLbl->setTextFormat(Qt::RichText);
    hintLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    connect(hintLbl, &QLabel::linkActivated,
            this, &AgentiPage::onHintLinkActivated);
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

    /* Esc ferma il flusso voce (registrazione / loop Conversa) */
    auto* scEsc = new QShortcut(Qt::Key_Escape, this);
    scEsc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(scEsc, &QShortcut::activated, this, &AgentiPage::onEscShortcut);
}

/* ──────────────────────────────────────────────────────────────
   buildSymbolsPanel — pannello inline caratteri speciali (toggle)
   ────────────────────────────────────────────────────────────── */
/* ── Pulsante simbolo matematico che supporta drag verso il builder ─────── */
class DraggableMathBtn : public QPushButton {
public:
    DraggableMathBtn(const QString& label, const QString& tpl, QWidget* p)
        : QPushButton(label, p), m_tpl(tpl) {}

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) m_dragOrigin = e->pos();
        QPushButton::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (!(e->buttons() & Qt::LeftButton)) return;
        if ((e->pos() - m_dragOrigin).manhattanLength() <
            QApplication::startDragDistance()) return;
        auto* drag = new QDrag(this);
        auto* mime = new QMimeData;
        mime->setData("application/x-prismalux-math", m_tpl.toUtf8());
        drag->setMimeData(mime);
        drag->setPixmap(grab());
        drag->setHotSpot(QPoint(width() / 2, height() / 2));
        drag->exec(Qt::CopyAction);
    }

private:
    QString m_tpl;
    QPoint  m_dragOrigin;
};

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
        tr("<small><b>Preview LaTeX</b> — scrivi nel testo oppure "
           "<b>trascina</b> i simboli nel costruttore qui sotto.</small>"),
        m_mathPanel);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#94a3b8;padding:2px 4px;");
    mpLay->addWidget(hint);

    /* ── Costruttore formula drag & drop ── */
    m_formulaBuilder = new FormulaBuilderWidget(m_mathPanel);
    m_formulaBuilder->setMinimumHeight(dpiScale(50));
    m_formulaBuilder->setMaximumHeight(dpiScale(50));
    mpLay->addWidget(m_formulaBuilder);

    /* ── Barra: LaTeX risultante + Inserisci + Pulisci ── */
    {
        auto* bar = new QHBoxLayout;
        bar->setSpacing(4);
        auto* latexLbl = new QLabel(m_mathPanel);
        latexLbl->setObjectName("builderLatexLbl");
        latexLbl->setStyleSheet("color:#64748b;font-family:monospace;font-size:10px;"
                                "padding:0 4px;");
        latexLbl->setText(tr("(nessuna formula)"));
        latexLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        bar->addWidget(latexLbl, 1);

        auto* btnInsert = new QPushButton(tr("\xe2\x86\x91 Inserisci"), m_mathPanel);
        btnInsert->setObjectName("primaryBtn");
        btnInsert->setFixedHeight(dpiScale(22));
        btnInsert->setToolTip(tr("Inserisce la formula nel campo di testo"));
        bar->addWidget(btnInsert);

        auto* btnClear = new QPushButton(tr("Pulisci"), m_mathPanel);
        btnClear->setFixedHeight(dpiScale(22));
        btnClear->setToolTip(tr("Svuota il costruttore"));
        bar->addWidget(btnClear);

        mpLay->addLayout(bar);

        /* aggiorna label LaTeX quando il builder cambia */
        connect(m_formulaBuilder, &FormulaBuilderWidget::formulaChanged,
                latexLbl, [latexLbl](const QString& ltx) {
                    latexLbl->setText(ltx.isEmpty() ? tr("(nessuna formula)") : ltx);
                });

        connect(btnInsert, &QPushButton::clicked,
                this, &AgentiPage::onInsertBuilderFormula);

        connect(btnClear, &QPushButton::clicked,
                this, &AgentiPage::onClearBuilderClicked);
    }

    /* ── Preview KaTeX — placeholder a startup, LatexView creato on-demand ──
       QWebEngineView avvia un processo Chromium al momento della costruzione,
       anche se il widget è nascosto. Il pannello è setVisible(false) di default,
       quindi usiamo un QFrame vuoto e lo sostituiamo con il vero LatexView
       solo la prima volta che l'utente apre il pannello (lazy init). */
    auto* prevPlaceholder = new QFrame(m_mathPanel);
    prevPlaceholder->setObjectName("mathPreviewPlaceholder");
    prevPlaceholder->setMinimumHeight(dpiScale(90));
    prevPlaceholder->setMaximumHeight(dpiScale(130));
    prevPlaceholder->setFrameShape(QFrame::NoFrame);
    mpLay->addWidget(prevPlaceholder);
    m_mathPreview = prevPlaceholder;

    /* ── Debounce timer per aggiornare la preview ── */
    m_mathPreviewTimer = new QTimer(this);
    m_mathPreviewTimer->setSingleShot(true);
    m_mathPreviewTimer->setInterval(500);

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
        const QString tplStr = QString::fromUtf8(kTpl[i].latex);
        auto* b = new DraggableMathBtn(
            QString::fromUtf8(kTpl[i].label), tplStr, tplWidget);
        b->setObjectName("symbolBtn");
        b->setFixedSize(dpiScale(46), dpiScale(26));
        b->setToolTip(QString::fromUtf8(kTpl[i].tip) +
                      tr("\n(clicca per inserire nel testo, trascina nel costruttore)"));
        b->setProperty("mathTpl", tplStr);
        connect(b, &QPushButton::clicked, this, &AgentiPage::onMathTplBtnClicked);
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

/* ── Inserisce il LaTeX del builder nel campo testo al cursore corrente ── */
void AgentiPage::onInsertBuilderFormula()
{
    if (!m_formulaBuilder || !m_input) return;
    const QString ltx = m_formulaBuilder->toLaTeX().trimmed();
    if (ltx.isEmpty()) return;
    QTextCursor cur = m_input->textCursor();
    cur.insertText("\\(" + ltx + "\\)");
    m_input->setTextCursor(cur);
    m_input->setFocus();
}

/* ── Svuota il builder solo dopo conferma se ci sono blocchi ── */
void AgentiPage::onClearBuilderClicked()
{
    if (!m_formulaBuilder) return;
    if (m_formulaBuilder->blockCount() == 0) return;
    QMessageBox ask(this);
    ask.setWindowTitle(tr("Pulisci costruttore"));
    ask.setText(tr("Cancellare tutti i blocchi della formula?"));
    ask.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    ask.setDefaultButton(QMessageBox::Cancel);
    if (ask.exec() == QMessageBox::Yes)
        m_formulaBuilder->clearAll();
}

/* ── Aggiorna la preview KaTeX con il testo corrente del campo ── */
void AgentiPage::updateMathPreview()
{
    if (!m_mathPreviewTimer || !m_mathPanel || !m_mathPanel->isVisible() || !m_input)
        return;

    auto* preview = qobject_cast<LatexView*>(m_mathPreview);
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
        else if (m_modeBtn && m_modeBtn->currentMode() == TriModeButton::Conversa)
            m_btnRun->setText(tr("\xf0\x9f\x8e\x99  Dialoga"));
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
        m_piperProc->waitForFinished(P::kProcKillGraceMs);
        m_piperProc->deleteLater();
        m_piperProc = nullptr;
    }
    if (m_ttsProc) { m_ttsProc->kill(); m_ttsProc->waitForFinished(P::kProcKillGraceMs); }
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
    if (m_ttsProc)   { m_ttsProc->kill(); m_ttsProc->waitForFinished(P::kProcKillGraceMs); }
    if (m_piperProc) { m_piperProc->kill(); m_piperProc->waitForFinished(P::kProcKillGraceMs); }
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

/* Link "«Cosa sai fare?»" nella barra suggerimenti: riusa l'handler
   "prova:" del log (inserisce la domanda nella casella e la invia). */
void AgentiPage::onHintLinkActivated(const QString& link)
{
    onLogAnchorClicked(QUrl(link));
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
            p->waitForFinished(P::kProcKillGraceMs);
            p->deleteLater();
        }
        if (m_ttsProc) {
            QProcess* p = m_ttsProc;
            m_ttsProc = nullptr;
            p->kill();
            p->waitForFinished(P::kProcKillGraceMs);
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
                p->waitForFinished(P::kProcKillGraceMs);
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

void AgentiPage::onEtimoToggled(bool on)
{
    if (!m_btnEtimo) return;
    m_btnEtimo->setStyleSheet(on
        ? "QPushButton{background:#7c3aed;color:#fff;border:1px solid #6d28d9;"
          "border-radius:4px;padding:3px 8px;font-weight:bold;}"
        : "");
}

void AgentiPage::onMathToggleToggled(bool on)
{
    if (!m_btnMathToggle) return;
    m_btnMathToggle->setStyleSheet(on
        ? "QPushButton{background:#0e7490;color:#fff;border:1px solid #0891b2;"
          "border-radius:4px;padding:3px 8px;font-weight:bold;}"
        : "");

    /* Lazy init LatexView: la prima volta che il pannello viene aperto
       sostituiamo il placeholder QFrame con il vero QWebEngineView. */
    if (on && m_mathPanel && !qobject_cast<LatexView*>(m_mathPreview)) {
        auto* lay = qobject_cast<QVBoxLayout*>(m_mathPanel->layout());
        if (lay && m_mathPreview) {
            const int idx = lay->indexOf(m_mathPreview);
            lay->removeWidget(m_mathPreview);
            m_mathPreview->deleteLater();
            auto* lv = new LatexView(m_mathPanel);
            lv->setMinimumHeight(m_mathPanel->minimumHeight() > 0
                                 ? dpiScale(90) : dpiScale(90));
            lv->setMaximumHeight(dpiScale(130));
            lv->setLatexHtml(
                "<p style='color:#475569;font-size:12px;padding:8px'>"
                "Inizia a scrivere una formula nel campo di testo sopra...</p>");
            if (idx >= 0)
                lay->insertWidget(idx, lv);
            else
                lay->addWidget(lv);
            m_mathPreview = lv;
        }
    }

    if (m_mathPanel) m_mathPanel->setVisible(on);
}

void AgentiPage::onMathTplBtnClicked()
{
    if (!m_input) return;
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const QString tpl = btn->property("mathTpl").toString();
    QTextCursor cur = m_input->textCursor();
    cur.insertText(tpl);
    const int open = tpl.indexOf('{');
    if (open >= 0) {
        int newPos = cur.position() - tpl.size() + open + 1;
        cur.setPosition(newPos);
        m_input->setTextCursor(cur);
    }
    m_input->setFocus();
}

/* Helper: ritorna true se il modello supporta vision multimodale */

#include "main_app_controller.h"
#include "../dpi_utils.h"
#include "main_opencode.h"
#include "../prismalux_paths.h"
#include "../widgets/model_combo_helper.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QProcess>
#include <QProgressBar>
#include <QTimer>
#include <QDateTime>
#include <QUrl>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
AppControllerPage::~AppControllerPage()
{
    /* disconnect() prima che QWidget::~QWidget() → deleteChildren() →
     * QProcess::~QProcess() → waitForFinished() → finished() →
     * onTelegramProcFinished() tenti di accedere a widget già distrutti.
     * disconnect() rimuove fisicamente tutti i collegamenti segnale/slot
     * dove m_telegramProc è sender; blockSignals è la seconda difesa. */
    if (m_telegramProc) {
        m_telegramProc->disconnect();
        m_telegramProc->blockSignals(true);
        if (m_telegramProc->state() == QProcess::Running) {
            m_telegramProc->terminate();
            m_telegramProc->waitForFinished(1000);
            if (m_telegramProc->state() == QProcess::Running)
                m_telegramProc->kill();
        }
    }
}

AppControllerPage::AppControllerPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    /* ── Toolbar rapida ── */
    auto* toolbar = new QWidget(this);
    auto* tbLay   = new QHBoxLayout(toolbar);
    tbLay->setContentsMargins(dpiScale(8), dpiScale(4),
                              dpiScale(8), dpiScale(4));
    tbLay->setSpacing(dpiScale(6));

    auto* pipBtn = new QPushButton(
        tr("\xf0\x9f\x90\x8d  Moduli Python"), toolbar);
    pipBtn->setObjectName("actionBtn");
    pipBtn->setToolTip(
        "Apri Impostazioni \xe2\x86\x92 Moduli Python "
        "per installare i moduli necessari");
    pipBtn->setFixedHeight(dpiScale(28));
    tbLay->addWidget(pipBtn);
    tbLay->addStretch();
    lay->addWidget(toolbar);

    connect(pipBtn, &QPushButton::clicked, this,
            [this]() { emit openSettingsDipendenze({}); });

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName("innerTabs");
    m_tabs->setTabPosition(QTabWidget::North);

    m_tabs->addTab(buildBlenderTab(),      "\xf0\x9f\x8e\xa8  Blender");
    m_tabs->addTab(buildFreeCADTab(),      "\xf0\x9f\x94\xa9  FreeCAD");
    m_tabs->addTab(buildOfficeTab(),       "\xf0\x9f\x96\xa5  Office");
    m_tabs->addTab(buildCloudCompareTab(), "\xf0\x9f\x94\xb5  CloudCompare");
    m_tabs->addTab(buildAnkiTab(),         "\xf0\x9f\x83\x8f  Anki MCP");
    m_tabs->addTab(buildKiCADTab(),        "\xf0\x9f\x96\xa5  KiCAD MCP");
    m_tabs->addTab(buildTinyMCPTab(),      "\xf0\x9f\xa4\x96  TinyMCP");
    m_tabs->addTab(buildOBSTab(),          "\xf0\x9f\x94\xb4  OBS MCP");
    /* Godot + Game Modding → spostati in Utility → Mod Giochi (ModGiochiWidget) */
    m_tabs->addTab(new OpenCodePage(m_tabs), "\xf0\x9f\x96\xa5  OpenCode");
    /* McpManagerPage → spostato in Impostazioni → Gestione MCP */
    {
        auto* tgTab = buildTelegramTab();
        m_tabs->addTab(tgTab, tr("\xf0\x9f\x93\xac  Telegram"));  /* 📬 */
        const int tgIdx = m_tabs->indexOf(tgTab);
        connect(m_tabs, &QTabWidget::currentChanged, this,
            [this, tgIdx](int idx) {
                if (idx != tgIdx || !m_telegramModuleBanner) return;
                QTimer::singleShot(15000, this, [this]() {
                    if (!m_telegramModuleBanner) return;
                    auto* chk = new QProcess(this);
                    connect(chk,
                        QOverload<int,QProcess::ExitStatus>::of(
                            &QProcess::finished),
                        this, [this, chk](int code, QProcess::ExitStatus) {
                            if (m_telegramModuleBanner)
                                m_telegramModuleBanner->setVisible(code != 0);
                            chk->deleteLater();
                        });
                    chk->start("python3",
                        {"-c", "from telegram.ext import Application"});
                });
            });
    }
    m_tabs->addTab(buildWhatsAppTab(),       "\xf0\x9f\x92\xac  WhatsApp");  /* 💬 */
    /* DevAgent e Sicurezza → spostati in Programmazione */

    lay->addWidget(m_tabs);

    m_aiProgress = new QProgressBar(this);
    m_aiProgress->setRange(0, 0);
    m_aiProgress->setFixedHeight(dpiScale(4));
    m_aiProgress->setTextVisible(false);
    m_aiProgress->setVisible(false);
    lay->addWidget(m_aiProgress);

    m_aiErrorPanel = new AiErrorWidget(this);
    lay->addWidget(m_aiErrorPanel);

    /* Propaga modello corrente a tutte le combo */
    connect(m_ai, &AiClient::modelsReady, this, &AppControllerPage::onModelsReady);
}

/* ──────────────────────────────────────────────────────────────
   Helper: popola una combo modelli
   ────────────────────────────────────────────────────────────── */

/* ──────────────────────────────────────────────────────────────
   Helper: estrai codice Python dal primo blocco ```...```
   ────────────────────────────────────────────────────────────── */
QString AppControllerPage::extractCode(const QString& text)
{
    int start = text.indexOf("```python");
    if (start != -1) {
        start = text.indexOf('\n', start) + 1;
        int end = text.indexOf("```", start);
        if (end != -1) return text.mid(start, end - start).trimmed();
    }
    /* Fallback generico: salta il language tag (tutto fino al primo \n) */
    start = text.indexOf("```");
    if (start != -1) {
        start += 3;
        const int nl = text.indexOf('\n', start);
        if (nl != -1) {
            start = nl + 1;
            int end = text.indexOf("```", start);
            if (end != -1) return text.mid(start, end - start).trimmed();
        }
    }
    return text.trimmed();
}

/* ──────────────────────────────────────────────────────────────
   Helper: lancia AI con streaming — pattern connHolder one-shot
   ────────────────────────────────────────────────────────────── */
void AppControllerPage::runAi(int tabIdx, const QString& sys, const QString& userMsg,
                               QTextEdit* output, QPushButton* runBtn, QPushButton* stopBtn,
                               QComboBox* modelCombo)
{
    if (m_ai->busy()) {
        output->append("\xe2\x9a\xa0  AI occupata, attendi o premi Stop.");
        return;
    }
    if (userMsg.trimmed().isEmpty()) {
        output->append("\xe2\x9a\xa0  Inserisci la richiesta prima di eseguire.");
        return;
    }

    /* Applica modello selezionato */
    if (modelCombo && modelCombo->count() > 0) {
        const QString sel = modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    /* Salva stato sessione per gli slot nominati */
    m_runAiTabIdx    = tabIdx;
    m_runAiSys       = sys;
    m_runAiUserMsg   = userMsg;
    m_runAiOutput    = output;
    m_runAiRunBtn    = runBtn;
    m_runAiStopBtn   = stopBtn;
    m_runAiModelCombo = modelCombo;

    m_aiActive  = true;
    m_activeTab = tabIdx;
    runBtn->setEnabled(false);
    stopBtn->setEnabled(true);
    if (m_aiProgress) m_aiProgress->setVisible(true);
    output->append(
        "\n\xf0\x9f\x94\x84  Generazione in corso...\n"
        + QString(40, QChar(0x2500)));

    /* Disconnette connessioni precedenti e ricrea token holder */
    disconnect(m_connToken);
    disconnect(m_connFinished);
    disconnect(m_connError);
    delete m_tokenHolder;
    m_tokenHolder = new QObject(this);

    m_connToken    = connect(m_ai, &AiClient::token,    this, &AppControllerPage::onRunAiToken);
    m_connFinished = connect(m_ai, &AiClient::finished, this, &AppControllerPage::onRunAiFinished);
    m_connError    = connect(m_ai, &AiClient::error,    this, &AppControllerPage::onRunAiError);

    m_ai->chat(sys, userMsg);
}

/* ──────────────────────────────────────────────────────────────
   Slot di runAi — token streaming
   ────────────────────────────────────────────────────────────── */
void AppControllerPage::onRunAiToken(const QString& t)
{
    if (!m_runAiOutput) return;
    m_runAiOutput->moveCursor(QTextCursor::End);
    m_runAiOutput->insertPlainText(t);
}

/* ──────────────────────────────────────────────────────────────
   Slot di runAi — generazione completata
   ────────────────────────────────────────────────────────────── */
void AppControllerPage::onRunAiFinished(const QString& full)
{
    m_aiActive = false;
    if (m_runAiRunBtn)  m_runAiRunBtn->setEnabled(true);
    if (m_runAiStopBtn) m_runAiStopBtn->setEnabled(false);
    if (m_aiProgress)   m_aiProgress->setVisible(false);
    if (m_runAiOutput)  m_runAiOutput->append("\n" + QString(40, QChar(0x2500)));
    /* Disconnette segnali AI e libera token holder */
    disconnect(m_connToken);
    disconnect(m_connFinished);
    disconnect(m_connError);
    if (m_tokenHolder) {
        m_tokenHolder->deleteLater();
        m_tokenHolder = nullptr;
    }

    /* Abilita exec solo se c'era un blocco backtick reale (non testo puro) */
    const bool hasBlock = full.contains("```");
    const QString code = extractCode(full);
    if (m_activeTab == 0 && hasBlock && !code.isEmpty()) {
        const QString model  = m_runAiModelCombo ? m_runAiModelCombo->currentData().toString() : "AI";
        const QString ts     = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
        /* Commento singola riga con modello e data */
        const QString header = "# LLM: " + model + "  \xe2\x80\x94  " + ts + "\n";
        /* Garantisce import bpy: senza di esso exec() di Blender restituisce {} */
        const QString imports = code.contains("import bpy") ? "" : "import bpy\n";
        m_blenderCode = code;
        m_blenderCodeEdit->setPlainText(header + imports + code);
        m_blenderStatusLbl->setText(
            tr("\xf0\x9f\x90\x8d  Codice pronto \xe2\x80\x94 premi Esegui in Blender"));
    } else if (m_activeTab == 1 && hasBlock && !code.isEmpty()) {
        m_freecadCode = code;
        m_freecadExecBtn->setEnabled(true);
        m_freecadStatusLbl->setText(
            tr("\xf0\x9f\x94\xa9  Codice pronto \xe2\x80\x94 premi Esegui in FreeCAD"));
    } else if (m_activeTab == 2 && hasBlock && !code.isEmpty()) {
        m_officeCode = code;
        m_officeExecBtn->setEnabled(true);
        m_officeStatusLbl->setText(
            tr("\xf0\x9f\x93\x84  Codice pronto \xe2\x80\x94 premi Esegui in Office"));
    } else if (m_activeTab == 5 && hasBlock && !code.isEmpty()) {
        m_kicadCode = code;
        m_kicadExecBtn->setEnabled(true);
        m_kicadStatusLbl->setText(
            tr("\xf0\x9f\x96\xa5  Codice pronto \xe2\x80\x94 premi Esegui in KiCAD"));
    } else if (m_activeTab == 6 && hasBlock && !code.isEmpty()) {
        m_mcuCode = code;
        m_mcuFlashBtn->setEnabled(true);
        m_mcuStatusLbl->setText(
            tr("\xf0\x9f\xa4\x96  Codice pronto \xe2\x80\x94 premi Flash MCU"));
    } else if (m_activeTab == 7 && hasBlock && !code.isEmpty()) {
        m_obsCode = code;
        m_obsExecBtn->setEnabled(true);
        m_obsStatusLbl->setText(
            tr("\xf0\x9f\x94\xb4  Codice pronto \xe2\x80\x94 premi Esegui in OBS"));
    }
}

/* ══════════════════════════════════════════════════════════════
   Link pip cliccabili nei log QTextBrowser
   URL schema: pip://<pacchetto>  →  apre Impostazioni→Moduli Python
   ══════════════════════════════════════════════════════════════ */

void AppControllerPage::onPipLinkClicked(const QUrl& url)
{
    if (url.scheme() == QLatin1String("pip")) {
        emit openSettingsDipendenze(url.path());
    } else if (url.scheme() == QLatin1String("tg-restart")) {
        onTelegramStartClicked();
    } else if (url.scheme() == QLatin1String("wa-restart")) {
        onWaBotStartClicked();
    }
}

/* ======================================================================
   Sezione 1 — Constructor: modelsReady
   ====================================================================== */

void AppControllerPage::onModelsReady(const QStringList& models)
{
    QList<QComboBox*> combos = {
        m_blenderModel, m_freecadModel, m_officeModel,
        m_ankiModel, m_kicadModel, m_mcuModel,
        m_obsModel
    };
    for (auto* cb : combos) {
        if (!cb) continue;
        ModelComboHelper::populate(cb, m_ai, models);
    }
}

/* ======================================================================
   Sezione 2 — runAi() error slot
   (token e finished sono definiti inline in app_controller_page.cpp)
   ====================================================================== */

void AppControllerPage::onRunAiError(const QString& msg)
{
    m_aiActive = false;
    if (m_runAiRunBtn)  m_runAiRunBtn->setEnabled(true);
    if (m_runAiStopBtn) m_runAiStopBtn->setEnabled(false);
    if (m_aiProgress)   m_aiProgress->setVisible(false);
    /* Disconnette segnali AI e libera token holder */
    disconnect(m_connToken);
    disconnect(m_connFinished);
    disconnect(m_connError);
    if (m_tokenHolder) {
        m_tokenHolder->deleteLater();
        m_tokenHolder = nullptr;
    }
    /* Cattura locale per il retry (copia i membri correnti) */
    const int      tabIdx    = m_runAiTabIdx;
    const QString  sys       = m_runAiSys;
    const QString  userMsg   = m_runAiUserMsg;
    QTextEdit*     output    = m_runAiOutput;
    QPushButton*   runBtn    = m_runAiRunBtn;
    QPushButton*   stopBtn   = m_runAiStopBtn;
    QComboBox*     modelCombo = m_runAiModelCombo;

    /* "Forbidden" = modello cloud selezionato ma Ollama è locale */
    if (msg.contains("Forbidden", Qt::CaseInsensitive)) {
        const QString model = modelCombo ? modelCombo->currentData().toString() : "?";
        m_aiErrorPanel->showError(
            "Modello non disponibile localmente: \"" + model + "\"\n"
            "Seleziona un modello locale (es. deepseek-coder:6.7b, llama3.2:3b) "
            "dalla combo Modello e riprova.",
            [this, tabIdx, sys, userMsg, output, runBtn, stopBtn, modelCombo]{
                runAi(tabIdx, sys, userMsg, output, runBtn, stopBtn, modelCombo);
            });
    } else {
        m_aiErrorPanel->showError(msg,
            [this, tabIdx, sys, userMsg, output, runBtn, stopBtn, modelCombo]{
                runAi(tabIdx, sys, userMsg, output, runBtn, stopBtn, modelCombo);
            });
    }
}

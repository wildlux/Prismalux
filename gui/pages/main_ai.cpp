#include "main_ai.h"
#include "main_ai_p.h"
#include "dialog_agents_config.h"
#include "../prismalux_paths.h"
#include "../widgets/tts_speak.h"
#include "../widgets/stt_whisper.h"
#include "../widgets/chart_widget.h"
#include "../widgets/formula_parser.h"
#include "../theme_manager.h"
#include <QApplication>
#include <QTime>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <cmath>
#include <cstdlib>
#include <QRegularExpression>
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
#include <QProcessEnvironment>
#include <QFile>
#include <QTemporaryFile>
#include <QDateTime>
#include <QDir>
#include <QScrollArea>
#include <QScrollBar>
#include <QMessageBox>
#include <algorithm>
#include <QFileDialog>
#include <QTextStream>
#include <QImage>
#include <QBuffer>
#include <QSet>
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
namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
AgentiPage::AgentiPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    /* Headroom context compressors */
    m_ctxSingle = new ContextCompressor(ai, this);
    m_ctxAuto   = new ContextCompressor(ai, this);

    /* Dialog configurazione agenti — non-modal, creato una volta */
    m_cfgDlg = new AgentsConfigDialog(this);
    /* m_cmbMode vive dentro il dialog Configura Agenti */
    m_cmbMode = m_cfgDlg->modeCombo();

    setupUI();
    hermesInit();

    connect(m_ai, &AiClient::token,            this, &AgentiPage::onToken);
    connect(m_ai, &AiClient::finished,         this, &AgentiPage::onFinished);
    connect(m_ai, &AiClient::error,            this, &AgentiPage::onError);
    connect(m_ai, &AiClient::modelsReady,      this, &AgentiPage::onModelsReady);
    connect(m_ai, &AiClient::toolCallRequired, this, &AgentiPage::onNativeToolCall);

    /* Quando il modello cambia da Impostazioni (o altra pagina), aggiorna
       la combo SOLO se l'utente non ha scelto esplicitamente un modello
       per questa scheda (m_pageModel vuoto = nessuna preferenza privata).
       blockSignals previene il loop currentIndexChanged → setBackend. */
    connect(m_ai, &AiClient::modelChanged, this, &AgentiPage::onAiModelChanged);

    /* Salva automaticamente ogni chat completata nello storico persistente */
    connect(this, &AgentiPage::chatCompleted, this, &AgentiPage::onChatCompletedSave);

    m_ai->fetchModels();
    /* startMcpDiscovery() NON parte qui: lancerebbe un interprete Python in
     * sequenza per ognuno dei ~50 plugin MCP a ogni avvio, anche se l'utente
     * non usa mai i tool MCP in quella sessione (causa nota di CPU sostenuta
     * per diversi secondi dopo l'apertura della finestra). Parte invece on
     * demand: al primo utilizzo reale, in onMcpPanelToggle() e in
     * toolSystemSuffix() (main_ai_pipeline.cpp) — è idempotente
     * (s_mcpDiscoveryDone). */
}

/* ══════════════════════════════════════════════════════════════
   Distruttore — rete di sicurezza oltre a prepareClose()
   ══════════════════════════════════════════════════════════════
   prepareClose() (sotto) è chiamato solo dal closeEvent di MainWindow e
   copre solo i QProcess con un membro nominato noto in anticipo. Questo
   distruttore copre TUTTI i QProcess figli (inclusi quelli anonimi creati
   ad-hoc in runToolCall(), main_ai_tools.cpp) e qualunque percorso di
   distruzione che non passi da prepareClose() — stesso pattern già
   documentato in gui/CLAUDE.md dopo un coredump reale (~VoiceClonerWidget,
   fix D-20): senza disconnect() prima del kill(), un ~QProcess ancora
   attivo durante deleteChildren() emette finished() sincrono su un
   widget già semi-distrutto. */
AgentiPage::~AgentiPage()
{
    const auto procs = findChildren<QProcess*>();
    for (QProcess* p : procs) {
        p->disconnect(this);
        p->blockSignals(true);
        if (p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(1000);
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   prepareClose — termina processi figli prima della chiusura
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::prepareClose()
{
    /* Ferma loop voce e registrazione */
    m_voiceLoopActive = false;
    m_sttState = SttState::Idle;
    if (m_sttTick) { m_sttTick->stop(); m_sttTick->deleteLater(); m_sttTick = nullptr; }

    /* Termina i QProcess: kill() + deleteLater() evita waitForFinished(-1) implicito
     * nel distruttore di QProcess quando il parent viene distrutto. */
    auto killProc = [](QProcess*& p) {
        if (!p) return;
        p->disconnect();
        p->kill();
        p->deleteLater();
        p = nullptr;
    };
    killProc(m_recProc);
    killProc(m_sttProc);
    killProc(m_vadProc);
    killProc(m_piperProc);
    killProc(m_ttsProc);
    killProc(m_execProc);

    /* Abort richieste HTTP pendenti */
    m_ai->abort();
}

/* ── slot: aggiorna combo LLM quando il modello cambia da Impostazioni ──────── */
void AgentiPage::onAiModelChanged(const QString& newModel)
{
    if (!m_cmbLLM || !m_pageModel.isEmpty()) return;
    int idx = m_cmbLLM->findData(newModel, Qt::UserRole);
    if (idx < 0) idx = m_cmbLLM->findText(newModel);
    if (idx >= 0 && idx != m_cmbLLM->currentIndex()) {
        m_cmbLLM->blockSignals(true);
        m_cmbLLM->setCurrentIndex(idx);
        m_cmbLLM->blockSignals(false);
    }
}


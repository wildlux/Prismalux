#include "mainwindow.h"
#include "log_bus.h"
#include "widgets/onnx_embedder.h"
#include "widgets/whisper_autosetup.h"
#include "pages/main_ai.h"
#include "pages/settings_main.h"
#include "pages/main_maintenance.h"
#include "pages/main_tools.h"
#include "pages/main_graph.h"
/* oracolo_page.h rimosso: OracoloPage sostituita da grafico integrato in AgentiPage */
#include "pages/main_programming.h"
#include "pages/main_math.h"
#include "pages/main_research.h"
#include "pages/main_app_controller.h"
#include "pages/main_lan_wan.h"
#include "pages/main_multi_agent.h"
#include "pages/main_distillazione_page.h"
#include "pages/main_multimedia.h"
#include "pages/main_tools_file.h"
#include "app_config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSizePolicy>
#include <QFont>
#include <QStatusBar>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QTimer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLineEdit>
#include <QDir>
#include <QFileInfo>
#include <QSpinBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include "prismalux_paths.h"
#include "dpi_utils.h"
#include "chat_history.h"
#include "widgets/spinner_widget.h"
#include <QShortcut>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTextEdit>
#include <QTextCursor>
#include <QFileDialog>
#include <QTextDocument>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QMessageBox>
#include <QApplication>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QAbstractItemView>
#include <QMouseEvent>

namespace P = PrismaluxPaths;

/* ── Forward declaration helpers math (definiti più in basso) ── */
static bool isMathModel(const QString& filename);
static void showMathDownloadDialog(QWidget* parent, const QString& modelsDir);

/* ══════════════════════════════════════════════════════════════
   stripBodyBackground — rimuove il background-color dal tag <body>
   dell'HTML generato da QTextEdit::toHtml().

   Qt serializza il colore QPalette::Base nel body style; quando la chat
   viene ricaricata in un tema diverso, quel colore fisso sovrascrive il
   background del documento e la chatLog appare con lo sfondo del tema
   precedente.  Rimuovendo solo quella proprietà il QSS del tema attivo
   può applicarsi correttamente tramite QPalette::Base.
   ══════════════════════════════════════════════════════════════ */
static QString stripBodyBackground(const QString& html)
{
    QString out = html;
    /* Qt genera:  <body style=" color:#...; background-color:#RRGGBB;">
     * Cattura tutto fino a background-color nel group 1, salta il valore. */
    static QRegularExpression re(
        "(<body\\b[^>]*style\\s*=\\s*\"[^\"]*?)background-color\\s*:\\s*[^;\"]+;?\\s*",
        QRegularExpression::CaseInsensitiveOption);
    out.replace(re, "\\1");
    return out;
}

/* ══════════════════════════════════════════════════════════════
   migrateLegacyChat — converte le chat pre-bolla nel nuovo formato.

   Estrae il testo grezzo dalla vecchia HTML (QTextDocument::toPlainText),
   identifica task / intestazioni agente / risposte tramite pattern,
   e ricostruisce il documento con le bolle buildUserBubble /
   buildAgentBubble / buildLocalBubble di AgentiPage.

   Se l'HTML è già nel nuovo formato (contiene il marker della bolla
   utente) lo restituisce invariato.
   ══════════════════════════════════════════════════════════════ */
static QString migrateLegacyChat(const QString& html)
{
    /* Già nel nuovo formato → niente da fare.
     * Controlla sia kDark.uBg (#162544) sia kLight.uBg (#dbeafe):
     * le chat salvate in tema chiaro hanno il secondo colore nelle bolle utente. */
    if (html.contains("bgcolor='#162544'") || html.contains("bgcolor=\"#162544\"") ||
        html.contains("bgcolor='#dbeafe'") || html.contains("bgcolor=\"#dbeafe\""))
        return html;

    /* Estrai testo grezzo */
    QTextDocument doc;
    doc.setHtml(html);
    const QString plain = doc.toPlainText();

    /* ── Risposta locale (0 token) ── */
    static QRegularExpression localRe(
        "(?:Risposta locale|Calcolo locale)[^\\n]*\\n([^\\n]+)\\n"
        "[^\\n]*tempo:[^\\n]*(\\d+[.,]\\d+)\\s*ms");
    {
        auto m = localRe.match(plain);
        if (m.hasMatch()) {
            QString result = m.captured(1).trimmed();
            double ms = m.captured(2).replace(',', '.').toDouble();
            /* Recupera il task dalla riga "Task:" */
            static QRegularExpression taskRe("Task:\\s*(.+)");
            QString task;
            auto tm = taskRe.match(plain);
            if (tm.hasMatch()) task = tm.captured(1).trimmed();
            return AgentiPage::buildUserBubble(task.isEmpty() ? result : task)
                 + AgentiPage::buildLocalBubble(result, ms);
        }
    }

    /* ── Pipeline ── */
    /* Estrae task */
    QString task;
    {
        static QRegularExpression taskRe("Task:\\s*(.+)");
        auto m = taskRe.match(plain);
        if (m.hasMatch()) task = m.captured(1).trimmed();
    }

    /* Spezza il testo in blocchi agente:
       intestazione = riga con "[Agente N"  o "Agente N —"
       fine blocco  = riga con "completato" oppure "Pipeline completata" */
    struct AgentBlock { QString label, model, time, body; };
    QVector<AgentBlock> agents;

    const QStringList lines = plain.split('\n');
    static QRegularExpression hdrRe(
        R"(\[Agente\s+(\d+)\s*[\—\-]\s*([^\]]+)\]\s*(?:[\xf0\x9f\xa4\x96\x1f916]+)?\s*(\S[^\xf0\x9f\x95\x90\n]*)\s*(?:[\xf0\x9f\x95\x90]+)?\s*(\d{2}:\d{2}:\d{2})?)");
    /* Pattern più semplice: cerca linee con "Agente N" e un modello */
    static QRegularExpression simpleHdr(
        "Agente\\s+(\\d+)[^\\[\\]]*\\]?\\s*([a-zA-Z0-9_.:/-]+)\\s*(\\d{2}:\\d{2}:\\d{2})?");

    AgentBlock current;
    bool inAgent = false;

    for (const QString& line : lines) {
        const QString t = line.trimmed();

        /* Fine blocco agente */
        if (inAgent && (t.contains("completato") || t.contains("Pipeline completata"))) {
            agents.append(current);
            current = {};
            inAgent = false;
            continue;
        }

        /* Nuova intestazione agente */
        if (t.contains("[Agente") && t.contains("]")) {
            if (inAgent) agents.append(current);
            current = {};
            inAgent = true;

            /* Estrai role dal pattern [Agente N — Role] */
            static QRegularExpression roleRe(R"(\[Agente\s+(\d+)\s*[\u2014\-]\s*([^\]]+)\])");
            auto rm = roleRe.match(t);
            if (rm.hasMatch())
                current.label = "\xf0\x9f\xa4\x96  Agente " + rm.captured(1) + " \xe2\x80\x94 " + rm.captured(2).trimmed();

            /* Estrai modello (token dopo 🤖) */
            static QRegularExpression modelRe(R"([\xf0\x9f\xa4\x96]\s*(\S+))");
            auto mm = modelRe.match(t);
            if (mm.hasMatch()) current.model = mm.captured(1);

            /* Estrai orario HH:mm:ss */
            static QRegularExpression timeRe(R"((\d{2}:\d{2}:\d{2}))");
            auto tm = timeRe.match(t);
            if (tm.hasMatch()) current.time = tm.captured(1);
            continue;
        }

        if (inAgent) {
            /* Salta righe separatori o "generando..." */
            if (t.startsWith("\xe2\x94") || t.startsWith("\xe2\x80\x94") ||
                t.contains("generando") || t.isEmpty())
                continue;
            current.body += line + '\n';
        }
    }
    if (inAgent) agents.append(current);

    /* Costruisci l'HTML con le bolle */
    if (agents.isEmpty() && task.isEmpty()) {
        /* Formato sconosciuto: mostra il testo grezzo in un card neutro */
        QString safe = plain;
        safe.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
        safe.replace("\n","<br>");
        /* Usa colori neutri (grigio chiaro) così il box è leggibile
         * sia in tema chiaro che scuro senza hardcode di palette. */
        return "<table width='100%' cellpadding='0' cellspacing='4'><tr>"
               "<td style='border:1px solid #888888;border-radius:8px;"
               "padding:12px;'>"
               "<p style='color:#888888;font-size:11px;margin:0 0 6px 0;'>"
               "\xf0\x9f\x93\x9c  Chat storica (formato precedente)</p>"
               + safe + "</td></tr></table>";
    }

    QString result = task.isEmpty() ? QString() : AgentiPage::buildUserBubble(task);
    for (const auto& ag : agents) {
        QString content = AgentiPage::markdownToHtml(ag.body.trimmed());
        if (content.isEmpty())
            content = "<p style='color:#6b7280;font-style:italic;'>Nessun contenuto salvato.</p>";
        result += AgentiPage::buildAgentBubble(
            ag.label.isEmpty() ? "\xf0\x9f\xa4\x96  Agente" : ag.label,
            ag.model.isEmpty() ? "—" : ag.model,
            ag.time.isEmpty()  ? "—" : ag.time,
            content);
    }
    return result;
}

/* ══════════════════════════════════════════════════════════════
   ResourceGauge
   ══════════════════════════════════════════════════════════════ */
ResourceGauge::ResourceGauge(const QString& label, QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(5);

    m_lbl = new QLabel(label, this);
    m_lbl->setObjectName("gaugeLabel");
    m_lbl->setFixedWidth(dpiScale(34));

    m_bar = new QProgressBar(this);
    m_bar->setObjectName("resBar");
    m_bar->setRange(0,100);
    m_bar->setValue(0);
    m_bar->setTextVisible(false);
    m_bar->setFixedSize(dpiScale(70), dpiScale(8));

    m_pct = new QLabel("  0.0%", this);
    m_pct->setObjectName("gaugePct");
    m_pct->setFixedWidth(dpiScale(42));

    lay->addWidget(m_lbl);
    lay->addWidget(m_bar);
    lay->addWidget(m_pct);
}

void ResourceGauge::update(double pct, const QString& detail) {
    m_bar->setValue(static_cast<int>(pct));
    m_pct->setText(QString("%1%").arg(pct, 5, 'f', 1));
    setLevel(pct);
    if (!detail.isEmpty())
        setToolTip(detail);
}

void ResourceGauge::setLevel(double pct) {
    /* Soglie QSS: < 70% verde (default), 70-90% giallo, > 90% rosso */
    const QString lvl = (pct >= 90) ? "crit" : (pct >= 70) ? "warn" : "";
    m_bar->setProperty("level", lvl);
    P::repolish(m_bar);  /* forza ricalcolo stile dopo cambio property */
}

/* ══════════════════════════════════════════════════════════════
   ChatListDelegate — checkbox Gmail-style nella sidebar chat
   ══════════════════════════════════════════════════════════════ */
class ChatListDelegate : public QStyledItemDelegate {
    static constexpr int kCbZone = 26; // px riservati a sinistra per la checkbox
public:
    explicit ChatListDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override
    {
        p->save();
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);
        QStyle* sty = o.widget ? o.widget->style() : QApplication::style();

        // 1. Background intera riga (selezione / hover)
        sty->drawPrimitive(QStyle::PE_PanelItemViewItem, &o, p, o.widget);

        // 2. Checkbox a sinistra
        const bool sel = (o.state & QStyle::State_Selected) != 0;
        const bool hov = (o.state & QStyle::State_MouseOver) != 0;
        drawCb(p, o.rect, sel, hov, o.palette);

        // 3. Testo spostato a destra della zona checkbox
        QRect tr = o.rect.adjusted(kCbZone + 2, 0, -4, 0);
        QColor tc = sel ? o.palette.highlightedText().color()
                        : o.palette.text().color();
        p->setPen(tc);
        p->setFont(o.font);
        p->drawText(tr, Qt::AlignVCenter | Qt::AlignLeft,
            o.fontMetrics.elidedText(o.text, Qt::ElideRight, tr.width()));

        p->restore();
    }

    bool editorEvent(QEvent* evt, QAbstractItemModel* /*model*/,
                     const QStyleOptionViewItem& opt,
                     const QModelIndex& idx) override
    {
        if (evt->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(evt);
            if (me->button() == Qt::LeftButton && cbRect(opt.rect).contains(me->pos())) {
                // Click nella zona checkbox: toggle selezione senza aprire la chat
                auto* view = qobject_cast<QAbstractItemView*>(
                    const_cast<QWidget*>(opt.widget));
                if (view) {
                    const bool was = view->selectionModel()->isSelected(idx);
                    view->selectionModel()->select(
                        idx,
                        was ? QItemSelectionModel::Deselect
                            : QItemSelectionModel::Select);
                }
                return true; // evento consumato — itemClicked non parte
            }
        }
        return false;
    }

private:
    QRect cbRect(const QRect& row) const {
        constexpr int sz = 14;
        return {row.left() + (kCbZone - sz) / 2,
                row.center().y() - sz / 2, sz, sz};
    }

    void drawCb(QPainter* p, const QRect& row, bool checked,
                bool hov, const QPalette& pal) const
    {
        QRect r = cbRect(row);
        p->setRenderHint(QPainter::Antialiasing, true);
        if (checked) {
            p->setPen(Qt::NoPen);
            p->setBrush(pal.highlight());
            p->drawRoundedRect(r, 2.5, 2.5);
            // Spunta bianca
            p->setPen(QPen(pal.highlightedText().color(), 1.8f,
                           Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->setBrush(Qt::NoBrush);
            const float l = r.left(), t = r.top(),
                        w = r.width(), h = r.height();
            QPointF pts[] = {
                {l + w * 0.18f, t + h * 0.52f},
                {l + w * 0.43f, t + h * 0.76f},
                {l + w * 0.83f, t + h * 0.25f},
            };
            p->drawPolyline(pts, 3);
        } else {
            QColor bc = hov ? pal.mid().color().lighter(160)
                            : pal.mid().color();
            p->setPen(QPen(bc, 1.3f));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(QRectF(r), 2.5, 2.5);
        }
    }
};

/* ══════════════════════════════════════════════════════════════
   MainWindow — costruttore (stepdown: ogni chiamata è un livello)
   ══════════════════════════════════════════════════════════════ */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("🍺 Prismalux v2.9 — Centro di Controllo");
    setWindowIcon(QIcon(P::root() + "/EXPORT/assets/prismalux.png"));
    setMinimumSize(dpiScale(1060), dpiScale(680));
    resize(dpiScale(1200), dpiScale(760));

    setupServices();
    setupLayout();
    setupStatusBar();
    setupAutoOptimizations();
    setupTimers();
    setupBackend();
    setupShortcuts();
    restoreWindowState();
    m_hw->start();   /* avvia dopo che tutti i widget gauge sono creati */
}

/* ── Livello 1: servizi di background ──────────────────────────── */
void MainWindow::setupServices()
{
    m_hw = new HardwareMonitor(this);
    m_ai = new AiClient(this);
    connect(m_hw, &HardwareMonitor::updated,     this, &MainWindow::onHWUpdated);
    connect(m_hw, &HardwareMonitor::hwInfoReady, this, &MainWindow::onHWReady);

    /* LogBus globale — qualsiasi scheda può usare LogBus::post() per inviare qui */
    connect(LogBus::instance(), &LogBus::event, this, &MainWindow::appendLog);

    /* ONNX embedder locale — caricato in background se i file modello esistono */
    m_onnxEmbedder = new OnnxEmbedder(this);
    if (OnnxEmbedder::defaultModelExists()) {
        QTimer::singleShot(500, this, [this]() {
            const bool ok = m_onnxEmbedder->loadModel(
                OnnxEmbedder::defaultModelPath(),
                OnnxEmbedder::defaultVocabPath());
            if (ok) {
                m_ai->setOnnxEmbedder(m_onnxEmbedder);
                appendLog(tr("ONNX embedder: modello caricato (%1 dim)").arg(m_onnxEmbedder->dims()));
            }
        });
    }

    /* m_hw->start() è chiamato alla fine del costruttore, dopo che tutti
     * i widget (m_gCpu, m_gRam, m_gGpu) sono stati creati da setupLayout(). */
}

/* ── Livello 1: layout principale Header + [Sidebar | Content] ─── */
void MainWindow::setupLayout()
{
    auto* root    = new QWidget(this);
    auto* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);
    rootLay->addWidget(buildHeader());

    auto* body    = new QWidget(root);
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);

    m_bodySplitter = new QSplitter(Qt::Horizontal, body);
    m_bodySplitter->setObjectName("bodySplitter");
    m_bodySplitter->setHandleWidth(dpiScale(4));
    m_bodySplitter->setChildrenCollapsible(false);
    m_bodySplitter->addWidget(buildSidebar());   // index 0 — sidebar
    m_bodySplitter->addWidget(buildContent());   // index 1 — contenuto
    m_bodySplitter->setStretchFactor(0, 0);
    m_bodySplitter->setStretchFactor(1, 1);
    m_bodySplitter->setCollapsible(0, true);     // sidebar collassabile verso sinistra
    m_bodySplitter->setSizes({dpiScale(335), 1});

    /* Trascina il separatore oltre la soglia → nascondi sidebar (come hamburger) */
    connect(m_bodySplitter, &QSplitter::splitterMoved, this, [this](int pos, int) {
        if (m_sidebarWidget && m_sidebarWidget->isVisible() && pos < dpiScale(60))
            m_sidebarWidget->setVisible(false);
    });

    bodyLay->addWidget(m_bodySplitter, 1);
    rootLay->addWidget(body, 1);

    setCentralWidget(root);
}

/* ── Livello 1: status bar con barra progresso pipeline + zoom ───── */
void MainWindow::setupStatusBar()
{
    m_statusProgress = new QProgressBar(this);
    m_statusProgress->setRange(0, 100);
    m_statusProgress->setValue(0);
    m_statusProgress->setFixedWidth(dpiScale(220));
    m_statusProgress->setFixedHeight(dpiScale(14));
    m_statusProgress->setTextVisible(true);
    m_statusProgress->setFormat("");
    m_statusProgress->setObjectName("statusProgress");
    m_statusProgress->setVisible(false);
    statusBar()->addPermanentWidget(m_statusProgress);

    /* Label download LLM — sempre nella status bar, nascosta finché non inizia un download */
    m_dlStatusLbl = new QLabel(this);
    m_dlStatusLbl->setObjectName("dlStatusLbl");
    m_dlStatusLbl->setVisible(false);
    statusBar()->addWidget(m_dlStatusLbl);

    /* ── Pulsanti Zoom +/- (basso destra) ── */
    auto* zoomBar = new QWidget(this);
    zoomBar->setObjectName("zoomBar");
    auto* zoomLay = new QHBoxLayout(zoomBar);
    zoomLay->setContentsMargins(4, 0, 4, 0);
    zoomLay->setSpacing(4);

    auto* zoomMinusBtn = new QPushButton("\xe2\x88\x92", zoomBar);  /* − */
    zoomMinusBtn->setObjectName("zoomBtn");
    zoomMinusBtn->setFixedSize(dpiSize(26, 22));
    zoomMinusBtn->setToolTip("Riduci testo (minimo 50%)");

    m_zoomPctLbl = new QLabel("100%", zoomBar);
    m_zoomPctLbl->setObjectName("zoomBarLabel");
    m_zoomPctLbl->setFixedWidth(dpiScale(44));
    m_zoomPctLbl->setAlignment(Qt::AlignCenter);

    auto* zoomPlusBtn = new QPushButton("+", zoomBar);
    zoomPlusBtn->setObjectName("zoomBtn");
    zoomPlusBtn->setFixedSize(dpiSize(26, 22));
    zoomPlusBtn->setToolTip("Aumenta testo (massimo 200%)");

    auto* zoomResetBtn = new QPushButton("\xe2\x97\x8f", zoomBar);  /* ● */
    zoomResetBtn->setObjectName("zoomResetBtn");
    zoomResetBtn->setFixedSize(dpiSize(18, 18));
    zoomResetBtn->setToolTip("Reimposta zoom a 100%");

    zoomLay->addWidget(zoomMinusBtn);
    zoomLay->addWidget(m_zoomPctLbl);
    zoomLay->addWidget(zoomPlusBtn);
    zoomLay->addWidget(zoomResetBtn);

    /* Carica valore salvato (default 100%) e aggiorna label */
    {
        QSettings s("Prismalux", "GUI");
        m_zoomPct = qBound(50, s.value("ui/zoomPct", 100).toInt(), 200);
    }
    m_zoomPctLbl->setText(QString::number(m_zoomPct) + "%");

    /* Imposta subito lo zoom nel ThemeManager: loadSaved() lo userà */
    ThemeManager::instance()->setZoomScale(m_zoomPct / 100.0);

    /* Timer debounce: riapplica il tema 200ms dopo l'ultimo click +/- */
    m_zoomDebounce = new QTimer(this);
    m_zoomDebounce->setSingleShot(true);
    m_zoomDebounce->setInterval(200);

    statusBar()->addPermanentWidget(zoomBar);

    connect(zoomMinusBtn, &QPushButton::clicked,
            this, &MainWindow::onZoomMinusBtnClicked);
    connect(zoomPlusBtn, &QPushButton::clicked,
            this, &MainWindow::onZoomPlusBtnClicked);
    connect(zoomResetBtn, &QPushButton::clicked,
            this, &MainWindow::onZoomResetBtnClicked);
    connect(m_zoomDebounce, &QTimer::timeout,
            this, &MainWindow::onZoomApplyDebounced);

    statusBar()->showMessage("\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
}

/* ── Livello 1: preset RAM primo avvio + zRAM ────────────────────── */
void MainWindow::setupAutoOptimizations()
{
    if (!QFile::exists(AiChatParams::filePath())) {
        AiChatParams p = AiChatParams::load();
        const qint64 ramMb = P::totalRamBytes() / (1024LL * 1024LL);
        if (ramMb > 0 && ramMb < 10000) {
            p.num_ctx     = 4096;
            p.num_predict = 1024;
            p.temperature = 0.1;
            statusBar()->showMessage(
                "\xf0\x9f\x8e\x9b  Preset 8 GB RAM applicato automaticamente.", 5000);
        } else if (ramMb >= 16000) {
            p.num_ctx = 16384;
            statusBar()->showMessage(
                "\xf0\x9f\x8e\x9b  Preset Contesto Lungo applicato automaticamente.", 5000);
        }
        AiChatParams::save(p);
        if (m_ai) m_ai->setChatParams(p);
    }

#ifndef Q_OS_WIN
    QTimer::singleShot(3000, this, &MainWindow::onZramSetupTimer);
#endif
}

/* ── Livello 1: timer idle-unload, wizard primo avvio, whisper ───── */
void MainWindow::setupTimers()
{
    /* Timer auto-scarico modello ogni 90s */
    m_idleUnloadTimer = new QTimer(this);
    m_idleUnloadTimer->setInterval(90'000);
    connect(m_idleUnloadTimer, &QTimer::timeout, this, &MainWindow::onIdleUnloadTimer);
    m_idleUnloadTimer->start();

    /* Wizard primo avvio — mostrato una sola volta */
    QSettings ss("Prismalux", "GUI");
    if (!ss.value(P::SK::kSetupDone, false).toBool())
        QTimer::singleShot(800, this, &MainWindow::showOnboardingWizard);

    /* Auto-setup whisper.cpp in background */
    QTimer::singleShot(1500, this, &MainWindow::onStartWhisperTimer);

    /* Auto-indicizza RAG (incluso Matematica.pdf via OCR) se l'indice è vuoto */
    QTimer::singleShot(6000, this, &MainWindow::onAutoRagIndex);

    /* Controlla aggiornamenti GitHub 10s dopo l'avvio */
    QTimer::singleShot(10000, this, &MainWindow::checkForUpdates);

    /* Avviso cartella RAG mancante — visibile 2s dopo l'avvio */
    QTimer::singleShot(2000, this, [this] {
        const QString ragDir = AppConfig::s().value(
            PrismaluxPaths::SK::kRagDocsDir, "").toString().trimmed();
        if (!ragDir.isEmpty() && QDir(ragDir).exists()) return;
        auto* bar = statusBar();
        auto* lbl = new QLabel(this);
        lbl->setObjectName("ragWarnStatusLbl");
        lbl->setText(
            "\xf0\x9f\x93\x82  Cartella RAG non configurata &mdash; "
            "<a href='openrag'>Imposta in Impostazioni &rarr; RAG</a>");
        lbl->setTextFormat(Qt::RichText);
        lbl->setOpenExternalLinks(false);
        connect(lbl, &QLabel::linkActivated, this, [this](const QString&) {
            openSettingsDialog();
            if (m_impPage) m_impPage->switchToTab("RAG");
        });
        bar->addWidget(lbl);
        /* Scompare automaticamente dopo 20s o quando l'utente configura il path */
        QTimer::singleShot(20000, lbl, &QLabel::hide);
    });
}

/* ── Livello 1: backend Ollama, modelli iniziali, tema ───────────── */
void MainWindow::setupBackend()
{
    {
        QSettings s("Prismalux", "GUI");
        const QString savedModel = s.value(P::SK::kActiveModel, "").toString();
        m_ai->setBackend(AiClient::Ollama, P::kLocalHost, P::kOllamaPort, savedModel);

        /* Carica configurazione Smart Router all'avvio */
        m_ai->setSmartRouter(
            s.value(P::SK::kSmartRouterEnabled, false).toBool(),
            s.value(P::SK::kCloudApiUrl, "").toString(),
            s.value(P::SK::kCloudApiModel, "gpt-4o-mini").toString(),
            s.value(P::SK::kCloudApiKey, "").toString());
    }
    /* Invalida la cache modelli: il primo fetch interroga sempre Ollama live.
       Questo garantisce che su una macchina diversa non venga mai mostrata
       la lista modelli della macchina su cui è stato compilato il binario. */
    m_ai->invalidateModelCache();
    m_lblModel->setText("(interrogo Ollama...)");
    m_ai->fetchModels();
    connect(m_ai, &AiClient::modelsReady,   this, &MainWindow::onInitialModelsReady);
    connect(m_ai, &AiClient::modelChanged,  this, &MainWindow::onModelChanged);

    /* TTFT tracking nell'header */
    connect(m_ai, &AiClient::requestStarted, this, [this](const QString&, const QString&) {
        m_ttftTimer.restart();
        m_ttftGotFirst = false;
    });
    connect(m_ai, &AiClient::token, this, [this](const QString&) {
        if (m_ttftGotFirst || !m_ttftLbl) return;
        m_ttftGotFirst = true;
        const qint64 ms = m_ttftTimer.elapsed();
        m_ttftLbl->setText(QString("\xe2\x9a\xa1 %1ms").arg(ms));
        const char* clr = (ms < 1000) ? "#22c55e" : (ms < 3000) ? "#f59e0b" : "#ef4444";
        m_ttftLbl->setStyleSheet(
            QString("QLabel#ttftLabel{color:%1;font-size:11px;padding:0 4px;}").arg(clr));
        m_ttftLbl->setVisible(true);
    });
    connect(m_ai, &AiClient::aborted, this, [this]() {
        if (m_ttftLbl) m_ttftLbl->setVisible(false);
    });

    ThemeManager::instance()->loadSaved();
    navigateTo(0);
}

/* ── Livello 1: scorciatoie da tastiera Alt+1…7 ──────────────────── */
void MainWindow::setupShortcuts()
{
    auto* sc1 = new QShortcut(QKeySequence("Alt+1"), this);
    auto* sc2 = new QShortcut(QKeySequence("Alt+2"), this);
    auto* sc3 = new QShortcut(QKeySequence("Alt+3"), this);
    auto* sc4 = new QShortcut(QKeySequence("Alt+4"), this);
    auto* sc5 = new QShortcut(QKeySequence("Alt+5"), this);
    auto* sc6 = new QShortcut(QKeySequence("Alt+6"), this);
    auto* sc7 = new QShortcut(QKeySequence("Alt+7"), this);
    connect(sc1, &QShortcut::activated, this, &MainWindow::onShortcutAlt1);
    connect(sc2, &QShortcut::activated, this, &MainWindow::onShortcutAlt2);
    connect(sc3, &QShortcut::activated, this, &MainWindow::onShortcutAlt3);
    connect(sc4, &QShortcut::activated, this, &MainWindow::onShortcutAlt4);
    connect(sc5, &QShortcut::activated, this, &MainWindow::onShortcutAlt5);
    connect(sc6, &QShortcut::activated, this, &MainWindow::onShortcutAlt6);
    connect(sc7, &QShortcut::activated, this, &MainWindow::onShortcutAlt7);
}

/* ── Livello 1: ripristina geometry e state dell'ultima sessione ──── */
void MainWindow::restoreWindowState()
{
    QSettings s("Prismalux", "GUI");
    const QByteArray geo   = s.value("mainwindow/geometry").toByteArray();
    const QByteArray state = s.value("mainwindow/state").toByteArray();
    if (!geo.isEmpty())   restoreGeometry(geo);
    if (!state.isEmpty()) restoreState(state);
}

/* ══════════════════════════════════════════════════════════════
   buildHeader — barra superiore con logo + gauges hardware
   ══════════════════════════════════════════════════════════════ */
QWidget* MainWindow::buildHeader()
{
    auto* hdr = new QWidget(this);
    hdr->setObjectName("header");
    hdr->setFixedHeight(dpiScale(52));

    auto* lay = new QHBoxLayout(hdr);
    lay->setContentsMargins(dpiScale(16), dpiScale(8), dpiScale(16), dpiScale(8));
    lay->setSpacing(12);

    buildHamburgerSection(lay);
    buildLogoSection(lay);
    lay->addStretch(1);
    buildGaugesSection(lay);
    buildActionButtons(lay);

    /* Badge e spinner mantenuti come nullptr — lo stato è nel testo di m_btnBackend */
    m_badgeServer = nullptr;
    m_spinServer  = nullptr;

    return hdr;
}

/* ── Livello 2: hamburger ☰ + messaggi 📋 + impostazioni ⚙️ ──────── */
void MainWindow::buildHamburgerSection(QHBoxLayout* lay)
{
    auto* hdr = qobject_cast<QWidget*>(lay->parent());

    /* ☰ Mostra/Nascondi sidebar */
    auto* btnHamburger = new QPushButton("\xe2\x98\xb0", hdr);
    btnHamburger->setObjectName("hamburgerBtn");
    btnHamburger->setFixedSize(dpiSize(36, 36));
    btnHamburger->setToolTip("Mostra / Nascondi la colonna sinistra");
    connect(btnHamburger, &QPushButton::clicked, this, &MainWindow::onHamburgerClicked);
    lay->addWidget(btnHamburger);

    /* 📋 Messaggi — pulsante + badge non-letti sovrapposto */
    auto* logWrap = new QWidget(hdr);
    logWrap->setObjectName("logWrap");
    logWrap->setFixedSize(dpiSize(46, 36));
    m_logBtn = new QPushButton("\xf0\x9f\x93\x8b", logWrap);
    m_logBtn->setObjectName("hamburgerBtn");
    m_logBtn->setFixedSize(dpiSize(36, 36));
    m_logBtn->setToolTip("Messaggi \xe2\x80\x94 log eventi, errori AI, backend, pipeline");
    m_logBtn->setAccessibleName("Apri log messaggi");
    m_logBtn->move(0, 0);
    m_logBadge = new QLabel("", logWrap);
    m_logBadge->setAlignment(Qt::AlignCenter);
    m_logBadge->setFixedSize(dpiSize(16, 16));
    m_logBadge->setVisible(false);
    m_logBadge->setStyleSheet(
        "background:#e03030; color:#fff; border-radius:8px;"
        "font-size:9px; font-weight:bold;");
    m_logBadge->move(dpiScale(28), 0);
    connect(m_logBtn, &QPushButton::clicked, this, &MainWindow::onLogBtnClicked);
    lay->addWidget(logWrap);

    /* ⚙️ Impostazioni */
    m_settingsBtn = new QPushButton("\xe2\x9a\x99\xef\xb8\x8f", hdr);
    m_settingsBtn->setObjectName("hamburgerBtn");
    m_settingsBtn->setFixedSize(dpiSize(36, 36));
    m_settingsBtn->setToolTip("Impostazioni \xe2\x80\x94 Backend, Hardware, Monitor AI, llama.cpp");
    m_settingsBtn->setAccessibleName("Apri impostazioni");
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);
    lay->addWidget(m_settingsBtn);

    lay->addSpacing(4);
}

/* ── Livello 2: 🍺 logo + titolo PRISMALUX ───────────────────────── */
void MainWindow::buildLogoSection(QHBoxLayout* lay)
{
    auto* hdr = qobject_cast<QWidget*>(lay->parent());

    auto* beer = new QLabel("🍺", hdr);
    beer->setObjectName("headerBeer");
    lay->addWidget(beer);

    auto* title = new QLabel("PRISMALUX", hdr);
    title->setObjectName("headerTitle");
    lay->addWidget(title);

    /* Tenuti come membri per aggiornamenti interni (applyBackend, onModelChanged) */
    m_lblBackend = new QLabel("", hdr);
    m_lblBackend->hide();
    m_lblModel = new QLabel(this);
    m_lblModel->hide();
}

/* ── Livello 2: CPU · RAM · GPU gauges ───────────────────────────── */
void MainWindow::buildGaugesSection(QHBoxLayout* lay)
{
    auto* hdr = qobject_cast<QWidget*>(lay->parent());
    m_gCpu = new ResourceGauge("CPU ", hdr);
    m_gRam = new ResourceGauge("RAM ", hdr);
    m_gGpu = new ResourceGauge("GPU ", hdr);
    lay->addWidget(m_gCpu);
    lay->addWidget(m_gRam);
    lay->addWidget(m_gGpu);

    m_tempLbl = new QLabel("", hdr);
    m_tempLbl->setObjectName("tempLabel");
    m_tempLbl->setToolTip(tr("Temperatura CPU / GPU rilevata dal sensore termico"));
    m_tempLbl->setStyleSheet(
        "QLabel#tempLabel{color:#94a3b8;font-size:11px;padding:0 4px;}");
    m_tempLbl->setVisible(false);
    lay->addWidget(m_tempLbl);

    m_ttftLbl = new QLabel("", hdr);
    m_ttftLbl->setObjectName("ttftLabel");
    m_ttftLbl->setToolTip(tr("TTFT \xe2\x80\x94 Time To First Token dell'ultima risposta AI\n"
                              "Verde < 1s  \xc2\xb7  Arancio 1-3s  \xc2\xb7  Rosso > 3s"));
    m_ttftLbl->setStyleSheet("QLabel#ttftLabel{color:#64748b;font-size:11px;padding:0 4px;}");
    m_ttftLbl->setVisible(false);
    lay->addWidget(m_ttftLbl);
}

/* ── Livello 2: 🚨 emergenza RAM + Scarica LLM + backend toggle ───── */
void MainWindow::buildActionButtons(QHBoxLayout* lay)
{
    auto* hdr = qobject_cast<QWidget*>(lay->parent());

    /* 🚨 Emergenza RAM */
    m_emergencyBtn = new QPushButton("🚨", hdr);
    m_emergencyBtn->setObjectName("emergencyBtn");
    m_emergencyBtn->setToolTip(
        "EMERGENZA RAM\n"
        "1. Ferma tutti i modelli Ollama\n"
        "2. Libera cache kernel (richiede password admin)");
    m_emergencyBtn->setFixedSize(dpiSize(42, 36));
    connect(m_emergencyBtn, &QPushButton::clicked, this, &MainWindow::onEmergencyRamClicked);
    lay->addWidget(m_emergencyBtn);

    /* 🗑 Scarica LLM */
    m_btnUnload = new QPushButton("\xf0\x9f\x97\x91  Scarica", hdr);
    m_btnUnload->setObjectName("unloadBtn");
    m_btnUnload->setFixedHeight(dpiScale(36));
    m_btnUnload->setToolTip(
        "Scarica il modello dalla RAM (keep_alive=0)\n"
        "Utile quando Ollama tiene il modello caricato\n"
        "anche dopo che hai finito di usarlo.\n"
        "Diventa giallo se RAM > 40%, rosso se > 75%.");
    connect(m_btnUnload, &QPushButton::clicked, this, &MainWindow::onUnloadModelClicked);
    lay->addWidget(m_btnUnload);

    /* 🦙 Backend toggle — reparentato in buildContent come corner widget */
    m_btnBackend = new QPushButton("\xf0\x9f\xa6\x99  Ollama", hdr);
    m_btnBackend->setObjectName("backendBtn");
    m_btnBackend->setToolTip("Cambia backend AI — un click per passare da Ollama a llama-server e viceversa");
    m_btnBackend->setAccessibleName("Backend AI attivo");
    m_btnBackend->setAccessibleDescription("Seleziona il backend: Ollama o llama-server");
    m_btnBackend->setFixedHeight(dpiScale(36));
    m_btnBackend->setMinimumWidth(dpiScale(130));
    connect(m_btnBackend, &QPushButton::clicked, this, &MainWindow::onBackendBtnClicked);
    m_btnBackend->setParent(this);  /* reparent temporaneo — buildContent lo riposiziona */
}

/* ══════════════════════════════════════════════════════════════
   showServerDialog — dialog avvio llama-server
   Estratto dal lambda di m_btnServer per essere richiamato
   anche dal menu contestuale di m_btnBackend.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::showServerDialog()
{
    const QStringList modelPaths = P::scanGgufFiles();

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\xa6\x99\xe2\x9a\xa1\xef\xb8\x8f  Avvia llama-server");
    dlg->setFixedWidth(dpiScale(460));
    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(10);

    lay->addWidget(new QLabel(
        "<b>Seleziona modello</b> \xe2\x80\x94 il server parte in background,<br>"
        "il backend viene commutato automaticamente.", dlg));

    lay->addWidget(buildServerHwBanner(dlg));

    /* Categorizza i modelli in matematici e generici */
    QStringList mathPaths, otherPaths;
    for (const QString& p : modelPaths) {
        if (isMathModel(QFileInfo(p).fileName())) mathPaths << p;
        else                                        otherPaths << p;
    }

    QComboBox* cmbModel = nullptr;
    QSpinBox*  spPort   = nullptr;
    lay->addWidget(buildServerModelSection(dlg, &cmbModel, mathPaths, otherPaths));
    /* Recupera spPort dal widget costruito sopra */
    spPort = dlg->findChild<QSpinBox*>();

    lay->addWidget(buildServerMathSection(dlg, cmbModel, mathPaths));

    lay->addWidget(new QLabel(
        "<span style='color:#5a5f80;font-size:11px;'>"
        "Binario cercato in: ENGINE_LLM/llama_cpp_studio/llama.cpp/build/bin/llama-server<br>"
        "Usa <i>avvia_qt.sh</i> dalla cartella Prismalux se il server non parte.</span>",
        dlg));

    auto* bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    bb->button(QDialogButtonBox::Ok)->setText("\xe2\x96\xb6  Avvia");
    bb->button(QDialogButtonBox::Ok)->setEnabled(!modelPaths.isEmpty());
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() == QDialog::Accepted && !modelPaths.isEmpty()) {
        auto* chk = dlg->findChild<QCheckBox*>();
        const QString modelPath = cmbModel ? cmbModel->currentData().toString() : QString();
        startLlamaServer(modelPath,
                         spPort ? spPort->value() : P::kLlamaServerPort,
                         chk    ? chk->isChecked() : false);
    }
    dlg->deleteLater();
}

/* ── Livello 2: banner GPU/CPU rilevato ──────────────────────────── */
QWidget* MainWindow::buildServerHwBanner(QWidget* parent)
{
    QString hwLine;
    if (m_hw && m_hw->hwReady()) {
        const HWInfo& hw = m_hw->hwInfo();
        int bestGpu = -1;
        for (int i = 0; i < hw.count; i++) {
            if (hw.dev[i].type != DEV_CPU) {
                if (bestGpu < 0 || hw.dev[i].avail_mb > hw.dev[bestGpu].avail_mb)
                    bestGpu = i;
            }
        }
        if (bestGpu >= 0) {
            const HWDevice& g = hw.dev[bestGpu];
            int ngl = (g.n_gpu_layers > 0) ? g.n_gpu_layers : 99;
            hwLine = QString(
                "<span style='color:#16a34a;'>"
                "\xf0\x9f\x8e\xae  <b>GPU rilevata:</b> %1 &mdash; %2 MB VRAM liberi "
                "&rarr; <b>-ngl %3</b> (accelerazione GPU attiva)</span>")
                .arg(QString::fromUtf8(g.name)).arg(g.avail_mb).arg(ngl);
        } else {
            const HWDevice& c = hw.dev[hw.primary];
            hwLine = QString(
                "<span style='color:#b45309;'>"
                "\xf0\x9f\x96\xa5  <b>CPU:</b> %1 &mdash; nessuna GPU rilevata "
                "&rarr; <b>-ngl 0</b> (inferenza RAM)</span>")
                .arg(QString::fromUtf8(c.name));
        }
    } else {
        hwLine = "<span style='color:#6b7280;'>\xe2\x8f\xb3  Rilevamento hardware in corso...</span>";
    }
    auto* lbl = new QLabel(hwLine, parent);
    lbl->setWordWrap(true);
    return lbl;
}

/* ── Livello 2: combo modello + porta ────────────────────────────── */
QWidget* MainWindow::buildServerModelSection(QWidget* parent,
                                              QComboBox** outCombo,
                                              const QStringList& mathPaths,
                                              const QStringList& otherPaths)
{
    auto* container = new QWidget(parent);
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(6);

    auto* cmbModel = new QComboBox(container);
    if (mathPaths.isEmpty() && otherPaths.isEmpty()) {
        cmbModel->addItem("(nessun .gguf trovato in models/)");
        cmbModel->setEnabled(false);
    } else {
        for (const QString& p : mathPaths)
            cmbModel->addItem("\xf0\x9f\x93\x90 " + QFileInfo(p).fileName(), p);
        for (const QString& p : otherPaths)
            cmbModel->addItem(QFileInfo(p).fileName(), p);
    }
    vlay->addWidget(cmbModel);

    auto* rowPort = new QHBoxLayout;
    rowPort->addWidget(new QLabel("Porta:", container));
    auto* spPort = new QSpinBox(container);
    spPort->setRange(1024, 65535);
    spPort->setValue(P::kLlamaServerPort);
    spPort->setFixedWidth(dpiScale(90));
    rowPort->addWidget(spPort);
    rowPort->addStretch();
    vlay->addLayout(rowPort);

    if (outCombo) *outCombo = cmbModel;
    return container;
}

/* ── Livello 2: checkbox profilo math + status + download ────────── */
QWidget* MainWindow::buildServerMathSection(QWidget* parent,
                                             QComboBox* cmbModel,
                                             const QStringList& mathPaths)
{
    auto* container = new QWidget(parent);
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(4);

    auto* chkMath = new QCheckBox("\xf0\x9f\x93\x90  Profilo matematico (Xeon 64 GB)", container);
    chkMath->setToolTip(
        "Abilita flag ottimali per calcolo scientifico:\n"
        "  --ctx-size 8192  (dimostrazioni lunghe)\n"
        "  --no-mmap        (Q4_K_M: carica tutto in RAM, pi\xc3\xb9 veloce)\n"
        "  mmap attivo      (Q8_0: legge dal SSD on-demand, auto)");
    vlay->addWidget(chkMath);

    auto* lblMathStatus = new QLabel(container);
    lblMathStatus->setWordWrap(true);
    vlay->addWidget(lblMathStatus);

    auto* btnMathDl = new QPushButton(
        "\xe2\xac\x87  Scarica modello matematico da Hugging Face", container);
    btnMathDl->setObjectName("actionBtn");
    btnMathDl->setVisible(false);
    vlay->addWidget(btnMathDl);

    /* updateMathUI — aggiorna status e visibilità del download button */
    auto updateMathUI = [=](bool mathOn) {
        if (!mathOn) { lblMathStatus->hide(); btnMathDl->hide(); return; }
        if (mathPaths.isEmpty()) {
            lblMathStatus->setText(
                "<span style='color:#ff5252;'>"
                "\xe2\x9a\xa0  Nessun modello matematico trovato in models/.<br>"
                "Scarica Qwen2.5-Math-72B (Q4_K_M ~40 GB) o Qwen2.5-Math-7B (Q4_K_M ~4.7 GB).</span>");
            lblMathStatus->show(); btnMathDl->show();
        } else {
            lblMathStatus->setText(
                QString("<span style='color:#69f0ae;'>"
                        "\xe2\x9c\x85  %1 modello/i matematico/i trovato/i (in cima con \xf0\x9f\x93\x90)."
                        "</span>").arg(mathPaths.size()));
            lblMathStatus->show(); btnMathDl->hide();
            if (cmbModel) cmbModel->setCurrentIndex(0);
        }
    };

    connect(chkMath, &QCheckBox::toggled, container, updateMathUI);
    if (cmbModel) {
        connect(cmbModel, QOverload<int>::of(&QComboBox::currentIndexChanged),
                chkMath, [chkMath, cmbModel](int) {
            const QString name = cmbModel->currentText().toLower();
            chkMath->setChecked(name.contains("72b") || name.contains("70b") || name.contains("math"));
        });
    }
    connect(btnMathDl, &QPushButton::clicked, this, &MainWindow::onMathDlBtnClicked);

    /* Stato iniziale */
    const QString initName = cmbModel ? cmbModel->currentText().toLower() : QString();
    const bool initMath = !mathPaths.isEmpty() || initName.contains("math") || initName.contains("72b");
    chkMath->setChecked(initMath);
    updateMathUI(initMath);

    return container;
}

/* ══════════════════════════════════════════════════════════════
   applyBackend — cambia backend, aggiorna label + pulsante
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyBackend(AiClient::Backend b, const QString& host, int port) {
    m_ai->setBackend(b, host, port, "");
    if (m_btnBackend) m_btnBackend->setStyleSheet("color:#f59e0b;");
    m_ai->fetchModels();

    refreshBackendBtn();

    const QString bkName = (b == AiClient::Ollama)
        ? (port == P::kDwarfStarPort ? "DwarfStar" : "Ollama")
        : "llama.cpp";
    const QString bkIcon = (b == AiClient::Ollama)
        ? "\xf0\x9f\xa6\x99  Ollama"
        : "\xf0\x9f\xa6\x99  llama.cpp";
    m_lblBackend->setText(bkIcon + "  \xe2\x86\x92  " + host + ":" + QString::number(port));
    /* Lo stato viene mostrato nel testo di m_btnBackend — nessun widget extra. */
    m_lblModel->setText("(caricamento modelli...)");

    appendLog(QString("\xf0\x9f\x94\x84 Backend: <b>%1</b> @ %2:%3 — recupero modelli...")
              .arg(bkName, host, QString::number(port)));

    statusBar()->showMessage(
        QString("🔄  Backend cambiato: %1 @ %2:%3 — recupero modelli...")
        .arg(bkName, host, QString::number(port)));

    /* Quando arrivano i modelli, seleziona il primo e aggiorna status (connessione unica) */
    m_pendingBkName = bkName;
    connect(m_ai, &AiClient::modelsReady, this,
            &MainWindow::onApplyBackendModelsReady, Qt::SingleShotConnection);
}

/* ══════════════════════════════════════════════════════════════
   Helper — rilevamento modelli matematici
   ══════════════════════════════════════════════════════════════ */
static bool isMathModel(const QString& filename) {
    const QString n = filename.toLower();
    return n.contains("math") || n.contains("numina") ||
           n.contains("mathstral") || n.contains("minerva") ||
           n.contains("deepseek-math");
}

/* Struttura per la lista curata HF di modelli matematici */
struct MathModelEntry {
    QString name;
    QString description;
    QString urlQ4, fileQ4, sizeQ4, flagsQ4;
    QString urlQ8, fileQ8, sizeQ8, flagsQ8;
};

static QVector<MathModelEntry> mathModelCatalog() {
    return {
        {
            "Qwen2.5-Math-72B-Instruct",
            "Matematica universitaria/ricerca — ottimale per Xeon 64 GB",
            "https://huggingface.co/bartowski/Qwen2.5-Math-72B-Instruct-GGUF/resolve/main/Qwen2.5-Math-72B-Instruct-Q4_K_M.gguf",
            "Qwen2.5-Math-72B-Instruct-Q4_K_M.gguf", "~40 GB",
            "--no-mmap --ctx-size 8192 --threads 24",
            "https://huggingface.co/bartowski/Qwen2.5-Math-72B-Instruct-GGUF/resolve/main/Qwen2.5-Math-72B-Instruct-Q8_0.gguf",
            "Qwen2.5-Math-72B-Instruct-Q8_0.gguf", "~74 GB",
            "--ctx-size 8192 --threads 24"
        },
        {
            "Qwen2.5-Math-7B-Instruct",
            "Matematica avanzata — leggero, per test o RAM < 16 GB",
            "https://huggingface.co/bartowski/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/Qwen2.5-Math-7B-Instruct-Q4_K_M.gguf",
            "Qwen2.5-Math-7B-Instruct-Q4_K_M.gguf", "~4.7 GB",
            "--no-mmap --ctx-size 4096",
            "https://huggingface.co/bartowski/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/Qwen2.5-Math-7B-Instruct-Q8_0.gguf",
            "Qwen2.5-Math-7B-Instruct-Q8_0.gguf", "~7.7 GB",
            "--ctx-size 4096"
        },
    };
}

/* Dialog download modelli matematici da Hugging Face */
static void showMathDownloadDialog(QWidget* parent, const QString& modelsDir) {
    const auto catalog = mathModelCatalog();

    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("📐  Scarica modello matematico da Hugging Face");
    dlg->setMinimumWidth(dpiScale(620));
    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(12);

    lay->addWidget(new QLabel(
        "<b>Seleziona modello e variante di quantizzazione:</b>", dlg));

    /* Una riga per ogni modello con radio Q4 / Q8 */
    QVector<QPair<QRadioButton*, QRadioButton*>> rows;
    auto* grp = new QButtonGroup(dlg);

    for (const auto& m : catalog) {
        auto* box = new QGroupBox(m.name, dlg);
        box->setToolTip(m.description);
        auto* bl = new QVBoxLayout(box);
        bl->addWidget(new QLabel(
            "<span style='color:#5a5f80;font-size:11px;'>" + m.description + "</span>", box));

        auto* rq4 = new QRadioButton(
            QString("Q4_K_M  %1  — carica tutto in RAM%2")
            .arg(m.sizeQ4)
            .arg(m.name.contains("72B") ? "  ✅ consigliato per Xeon 64 GB" : ""), box);
        auto* rq8 = new QRadioButton(
            QString("Q8_0    %1  — qualità massima, richiede NVMe SSD%2")
            .arg(m.sizeQ8)
            .arg(m.name.contains("72B") ? " (mmap automatico)" : ""), box);

        grp->addButton(rq4);
        grp->addButton(rq8);
        bl->addWidget(rq4);
        bl->addWidget(rq8);
        lay->addWidget(box);
        rows.append(qMakePair(rq4, rq8));
    }

    /* Seleziona Q4_K_M del 72B come default */
    if (!rows.isEmpty()) rows[0].first->setChecked(true);

    auto* bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    bb->button(QDialogButtonBox::Ok)->setText("⬇  Scarica");
    lay->addWidget(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }

    /* Trova la selezione */
    QString url, fname, flags;
    for (int i = 0; i < rows.size() && i < catalog.size(); ++i) {
        if (rows[i].first->isChecked()) {
            url = catalog[i].urlQ4; fname = catalog[i].fileQ4; flags = catalog[i].flagsQ4;
        } else if (rows[i].second->isChecked()) {
            url = catalog[i].urlQ8; fname = catalog[i].fileQ8; flags = catalog[i].flagsQ8;
        }
    }

    dlg->deleteLater();
    if (url.isEmpty()) return;

    QString dest = modelsDir + "/" + fname;

    /* Avvia wget in background — args separati: immune a injection da caratteri
       speciali in url/dest, e startDetached è già asincrono (non serve &). */
    QProcess::startDetached("wget", {"-c", "--progress=bar:force", url, "-O", dest});

    QMessageBox info(parent);
    info.setWindowTitle("Download avviato");
    info.setIcon(QMessageBox::Information);
    info.setText(QString("<b>Download avviato in background:</b><br><code>%1</code>").arg(fname));
    info.setInformativeText(
        QString("Destinazione: <code>%1</code><br><br>"
                "Flag consigliati dopo il download:<br>"
                "<code>llama-server -m %2 --port 8081 --host 127.0.0.1 %3</code>")
        .arg(dest, dest, flags));
    info.exec();
}

/* ══════════════════════════════════════════════════════════════
   startLlamaServer — avvia llama-server in background poi
   commuta il backend automaticamente quando il /health risponde
   ══════════════════════════════════════════════════════════════ */
void MainWindow::startLlamaServer(const QString& modelPath, int port, bool mathProfile) {
    /* Percorso binario — rilevato dinamicamente da PrismaluxPaths */
    const QString bin = P::llamaServerBin();

    if (!QFileInfo::exists(bin)) {
        statusBar()->showMessage(
            "❌  llama-server non trovato. Compilalo in Impostazioni → llama.cpp Studio → Compila.");
        return;
    }

    m_serverPort  = port;
    m_serverModel = QFileInfo(modelPath).fileName();

    /* Variabile d'ambiente per le librerie condivise (.so nella stessa dir del binario) */
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LD_LIBRARY_PATH",
        P::llamaLibDir() + ":" + env.value("LD_LIBRARY_PATH"));

    m_serverProc = new QProcess(this);
    m_serverProc->setProcessEnvironment(env);
    m_serverProc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_serverProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onServerProcFinished);

    /* ── Rilevamento GPU/CPU per n_gpu_layers ─────────────────────────────
     * Logica:
     *   • GPU NVIDIA/AMD/Intel con VRAM disponibile ≥ 2 GB → -ngl <valore ottimale>
     *     (usa hw_gpu_layers() già calcolato da hw_detect)
     *   • CPU-only (es. Xeon) o GPU con VRAM < 2 GB → -ngl 0 (tutto in RAM)
     * Il valore n_gpu_layers è pre-calcolato da hw_detect.c tenendo conto
     * dell'80% della VRAM disponibile al netto del sistema operativo.
     * ───────────────────────────────────────────────────────────────────── */
    int ngl = 0;
    QString hwDesc = "CPU (RAM)";
    if (m_hw && m_hw->hwReady()) {
        const HWInfo& hw = m_hw->hwInfo();
        /* Cerca la GPU con più VRAM disponibile tra tutti i device.
         * La GPU è SEMPRE preferita alla CPU per l'inferenza, anche se
         * hw.primary punta alla CPU (es. Xeon con 64 GB batte GPU con 8 GB
         * per memoria totale, ma non per velocità di inferenza). */
        int bestGpu = -1;
        for (int i = 0; i < hw.count; i++) {
            if (hw.dev[i].type != DEV_CPU) {
                if (bestGpu < 0 || hw.dev[i].avail_mb > hw.dev[bestGpu].avail_mb)
                    bestGpu = i;
            }
        }
        if (bestGpu >= 0) {
            const HWDevice& g = hw.dev[bestGpu];
            ngl = (g.n_gpu_layers > 0) ? g.n_gpu_layers : 99;
            hwDesc = QString("%1 GPU: %2 — %3 MB VRAM liberi → -ngl %4")
                     .arg(hw_dev_type_str(g.type))
                     .arg(QString::fromUtf8(g.name))
                     .arg(g.avail_mb)
                     .arg(ngl);
        } else {
            const HWDevice& c = hw.dev[hw.primary];
            hwDesc = QString("CPU: %1 → -ngl 0 (RAM)").arg(QString::fromUtf8(c.name));
        }
    }

    /* Determina se il modello è Q4 (per --no-mmap) o Q8/più grande */
    bool isQ4 = QFileInfo(modelPath).fileName().contains("Q4", Qt::CaseInsensitive);

    QStringList args = {
        "-m", modelPath,
        "--port", QString::number(port),
        "--host", "127.0.0.1",
        "--log-disable",
        "-ngl", QString::number(ngl),
        /* Flash Attention: riduce RAM/VRAM KV cache ~30-50% senza perdita di qualità.
           Nuove versioni richiedono valore esplicito: --flash-attn auto|on|off */
        "--flash-attn", "auto",
        /* KV cache quantizzata q8_0: dimezza la RAM usata dalla KV cache
           rispetto al default f16, con minima perdita di qualità su testi lunghi. */
        "--cache-type-k", "q8_0",
        "--cache-type-v", "q8_0",
        /* SWA full: forza cache a dimensione piena per i layer sliding-window
           (Qwen3, Gemma3, Mistral). Evita perdita di contesto oltre la finestra SWA.
           Ignorato dai modelli senza sliding-window attention. */
        "--swa-full"
    };
    if (mathProfile) {
        args << "--ctx-size" << "8192";
        /* Q4_K_M (~40 GB): carica tutto in RAM — più veloce senza mmap */
        if (isQ4 && ngl == 0) args << "--no-mmap";
        /* Q8_0 (~74 GB): mmap attivo per default (llama.cpp legge dal SSD on-demand) */
    }

    {
        QSettings s("Prismalux", "GUI");
        if (s.value(P::SK::kMlockModel, false).toBool())
            args << "--mlock";

        if (s.value(P::SK::kRpcEnabled, false).toBool()) {
            const QString nodes = s.value(P::SK::kRpcNodes, "").toString().trimmed();
            if (!nodes.isEmpty())
                args << "--rpc" << nodes;
        }
    }

    {
        QSettings s2("Prismalux", "GUI");
        const bool rpcOn   = s2.value(P::SK::kRpcEnabled, false).toBool();
        const QString rpcNodes = s2.value(P::SK::kRpcNodes, "").toString().trimmed();
        const QString rpcDesc  = (rpcOn && !rpcNodes.isEmpty())
            ? QString(" | RPC: %1").arg(rpcNodes) : "";
        statusBar()->showMessage(
            QString("\xe2\x8f\xb3  Avvio llama-server — %1%2%3 — porta %4")
            .arg(hwDesc)
            .arg(mathProfile ? " | profilo matematico" : "")
            .arg(rpcDesc)
            .arg(port));
    }

    m_serverProc->start(bin, args);

    /*
     * Usiamo il segnale errorOccurred invece di waitForStarted() bloccante:
     * waitForStarted congela il thread UI per fino a 4s.
     * errorOccurred viene emesso immediatamente se il processo non parte.
     */
    connect(m_serverProc, &QProcess::errorOccurred,
            this, &MainWindow::onServerProcessError);

    appendLog(QString("\xf0\x9f\x9f\xa1 Avvio <b>llama-server</b> su porta %1...").arg(port));
    statusBar()->showMessage(
        QString("⏳  llama-server avviato — attendo che sia pronto (porta %1)...").arg(port));

    /* Mostra stato caricamento direttamente nel pulsante backend */
    if (m_btnBackend) m_btnBackend->setText("\xe2\x8f\xb3  Caricamento...");

    /*
     * Polling /health ogni 1s, max 180 tentativi (3 minuti).
     * Usa m_healthTimer/m_healthNam/m_healthTicks come membri per evitare lambda.
     */
    if (m_healthTimer) { m_healthTimer->stop(); m_healthTimer->deleteLater(); m_healthTimer = nullptr; }
    if (m_healthNam)   { m_healthNam->deleteLater(); m_healthNam = nullptr; }
    m_healthTicks = 0;
    m_healthTimer = new QTimer(this);
    m_healthNam   = new QNetworkAccessManager(this);
    connect(m_healthTimer, &QTimer::timeout, this, &MainWindow::onHealthTick);
    m_healthTimer->start(1000);
}

/* ── Ferma llama-server avviato dalla GUI ── */
void MainWindow::stopLlamaServer() {
    if (!m_serverProc) return;
    m_serverProc->terminate();
    statusBar()->showMessage("🛑  Arresto llama-server in corso...");
}

/* ── Aggiorna testo e colore del pulsante backend ── */
void MainWindow::refreshBackendBtn() {
    if (!m_btnBackend) return;
    if (m_ai->backend() == AiClient::Ollama) {
        if (m_ai->port() == P::kDwarfStarPort) {
            m_btnBackend->setText("\xe2\xad\x90  DwarfStar");
            m_btnBackend->setProperty("backendActive", "ollama");
        } else {
            m_btnBackend->setText("\xf0\x9f\xa6\x99  Ollama");
            m_btnBackend->setProperty("backendActive", "ollama");
        }
    } else {
        m_btnBackend->setText("\xf0\x9f\xa6\x99\xe2\x9a\xa1\xef\xb8\x8f  llama-server");
        m_btnBackend->setProperty("backendActive", "llama");
    }
    P::repolish(m_btnBackend);
}

/* ══════════════════════════════════════════════════════════════
   buildSidebar — colonna sinistra con bottoni navigazione
   ══════════════════════════════════════════════════════════════ */
QWidget* MainWindow::buildSidebar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("sidebar");
    bar->setMinimumWidth(dpiScale(160));

    auto* lay = new QVBoxLayout(bar);
    lay->setContentsMargins(0, 8, 0, 8);
    lay->setSpacing(2);

    /* ── Pulsanti: Nuova chat e Cancella in colonna verticale ── */
    {
        auto* btnRow = new QWidget(bar);
        btnRow->setObjectName("chatBtnRow");
        auto* btnLay = new QVBoxLayout(btnRow);
        btnLay->setContentsMargins(dpiScale(8), 0, dpiScale(8), 0);
        btnLay->setSpacing(dpiScale(4));

        auto* newChatBtn = new QPushButton("\xe2\x9c\x8f\xef\xb8\x8f  Nuova chat", btnRow);
        newChatBtn->setObjectName("actionBtn");
        newChatBtn->setFixedHeight(dpiScale(30));
        newChatBtn->setToolTip("Inizia una nuova conversazione (reset log)");
        connect(newChatBtn, &QPushButton::clicked, this, &MainWindow::onNewChatClicked);
        btnLay->addWidget(newChatBtn);

        m_btnDeleteChats = new QPushButton("\xf0\x9f\x97\x91  Cancella chat", btnRow);
        m_btnDeleteChats->setObjectName("actionBtn");
        m_btnDeleteChats->setProperty("danger", true);
        m_btnDeleteChats->setFixedHeight(dpiScale(30));
        m_btnDeleteChats->setLayoutDirection(Qt::LeftToRight);
        m_btnDeleteChats->setStyleSheet("text-align: left; padding-left: 8px;");
        m_btnDeleteChats->setEnabled(false);
        m_btnDeleteChats->setToolTip("Elimina le chat selezionate (Canc)");
        connect(m_btnDeleteChats, &QPushButton::clicked,
                this, &MainWindow::onDeleteSelectedChatsClicked);
        btnLay->addWidget(m_btnDeleteChats);

        lay->addWidget(btnRow);
    }

    /* ── Riga ricerca: [☐] [🔍 Cerca chat...] ── */
    {
        auto* searchRow = new QWidget(bar);
        searchRow->setObjectName("chatSearchRow");
        auto* searchLay = new QHBoxLayout(searchRow);
        searchLay->setContentsMargins(dpiScale(6), 0, dpiScale(6), 0);
        searchLay->setSpacing(dpiScale(4));

        m_chkSelectAll = new QCheckBox(searchRow);
        m_chkSelectAll->setObjectName("selectAllChk");
        m_chkSelectAll->setToolTip(tr("Seleziona / deseleziona tutte le chat visibili"));
        m_chkSelectAll->setTristate(true);
        searchLay->addWidget(m_chkSelectAll);

        m_chatSearch = new QLineEdit(searchRow);
        m_chatSearch->setPlaceholderText("\xf0\x9f\x94\x8d  Cerca chat...");
        m_chatSearch->setClearButtonEnabled(true);
        m_chatSearch->setObjectName("chatSearchEdit");
        searchLay->addWidget(m_chatSearch, 1);

        lay->addWidget(searchRow);
    }

    /* ── Lista chat history ── */
    m_chatList = new QListWidget(bar);
    m_chatList->setObjectName("chatList");
    m_chatList->setFrameShape(QFrame::NoFrame);
    m_chatList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chatList->setSpacing(2);
    m_chatList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_chatList->setMouseTracking(true);
    m_chatList->viewport()->setMouseTracking(true);
    m_chatList->setItemDelegate(new ChatListDelegate(m_chatList));
    m_chatList->installEventFilter(this);
    lay->addWidget(m_chatList, 1);   /* stretch=1: occupa lo spazio disponibile */

    /* Filtra la lista in tempo reale */
    connect(m_chatSearch, &QLineEdit::textChanged, this, &MainWindow::onChatSearchChanged);
    connect(m_chatList, &QListWidget::itemClicked, this, &MainWindow::onChatItemClicked);
    connect(m_chatList, &QListWidget::itemSelectionChanged, this, [this] {
        if (m_btnDeleteChats)
            m_btnDeleteChats->setEnabled(!m_chatList->selectedItems().isEmpty());
        if (m_chkSelectAll) {
            int visible = 0;
            for (int i = 0; i < m_chatList->count(); ++i)
                if (m_chatList->item(i) && !m_chatList->item(i)->isHidden()) visible++;
            const int sel = m_chatList->selectedItems().size();
            QSignalBlocker b(m_chkSelectAll);
            m_chkSelectAll->setCheckState(
                sel == 0         ? Qt::Unchecked :
                sel >= visible   ? Qt::Checked   : Qt::PartiallyChecked);
        }
    });

    /* Checkbox "Seleziona tutto" */
    connect(m_chkSelectAll, &QCheckBox::toggled, this, [this](bool checked) {
        for (int i = 0; i < m_chatList->count(); ++i) {
            auto* item = m_chatList->item(i);
            if (item && !item->isHidden())
                item->setSelected(checked);
        }
    });

    /* ── Context menu tasto destro sulle chat ── */
    m_chatList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_chatList, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onChatContextMenuRequested);

    refreshChatList();

    m_sidebarWidget = bar;

    return bar;
}

/* ══════════════════════════════════════════════════════════════
   buildContent — wrapper con navMenuBar + QTabWidget con tutti i tab
   ══════════════════════════════════════════════════════════════ */
QWidget* MainWindow::buildContent()
{
    auto* wrapper = new QWidget(this);
    auto* wLay    = new QVBoxLayout(wrapper);
    wLay->setContentsMargins(0, 0, 0, 0);
    wLay->setSpacing(0);

    /* QTabWidget principale */
    m_mainTabs = new QTabWidget(wrapper);
    m_mainTabs->setObjectName("mainTabs");
    m_mainTabs->setTabPosition(QTabWidget::North);
    m_mainTabs->setMovable(false);
    m_mainTabs->setAccessibleName("Sezioni principali di Prismalux");

    /* Backend button + status badge come corner widget sinistro */
    if (m_btnBackend) {
        m_btnBackend->setFixedHeight(dpiScale(28));
        m_btnBackend->setMinimumWidth(dpiScale(110));
        m_cornerContainer = new QWidget(m_mainTabs);
        auto* cornerLay = new QHBoxLayout(m_cornerContainer);
        cornerLay->setContentsMargins(4, 0, 8, 0);
        cornerLay->setSpacing(6);
        cornerLay->addWidget(m_btnBackend);
        m_mainTabs->setCornerWidget(m_cornerContainer, Qt::TopLeftCorner);
    }

    buildAiTab();
    buildStrumentiTab();
    buildMultimediaTab();
    /* File AI è ora sub-tab 10 di StrumentiPage */
    buildProgrammazioneTab();
    buildMatematicaTab();
    buildRicercaTab();
    buildAppControllerTab();
    buildLanWanTab();
    buildMultiAgentTab();
    buildDistillazioneTab();

    /* 🔍 Ricerca schede — corner widget destro, a destra di "LAN WAN" */
    {
        auto* srchWrap = new QWidget(m_mainTabs);
        auto* srchLay  = new QHBoxLayout(srchWrap);
        srchLay->setContentsMargins(4, 2, 6, 2);
        srchLay->setSpacing(3);

        auto* ico = new QLabel("\xf0\x9f\x94\x8d", srchWrap);
        ico->setFixedWidth(dpiScale(16));
        ico->setAlignment(Qt::AlignCenter);

        m_tabSearchEdit = new QLineEdit(srchWrap);
        m_tabSearchEdit->setObjectName("tabSearchEdit");
        m_tabSearchEdit->setPlaceholderText(tr("Scheda..."));
        m_tabSearchEdit->setClearButtonEnabled(true);
        m_tabSearchEdit->setFixedWidth(dpiScale(130));

        srchLay->addWidget(ico);
        srchLay->addWidget(m_tabSearchEdit);
        m_mainTabs->setCornerWidget(srchWrap, Qt::TopRightCorner);

        connect(m_tabSearchEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
            if (t.trimmed().isEmpty()) return;
            const QString q = t.trimmed();
            for (int i = 0; i < m_mainTabs->count(); ++i) {
                if (m_mainTabs->tabText(i).contains(q, Qt::CaseInsensitive)) {
                    m_mainTabs->setCurrentIndex(i);
                    break;
                }
            }
        });
        connect(m_tabSearchEdit, &QLineEdit::returnPressed,
                this, [this]{ m_tabSearchEdit->clear(); });
    }

    /* Salva etichette originali e applica modalità da QSettings */
    for (int i = 0; i < m_mainTabs->count(); i++)
        m_tabOrigLabels << m_mainTabs->tabText(i);
    {
        QSettings s("Prismalux", "GUI");
        applyTabMode(s.value(P::SK::kNavTabMode, "icons_text").toString());
    }

    buildNavMenuBar(wrapper, wLay);

    wLay->addWidget(m_navMenuBar);
    wLay->addWidget(m_mainTabs, 1);

    applyContentSettings();

    return wrapper;
}

/* ── Livello 2: tab [0] Intelligenza Artificiale ─────────────────── */
void MainWindow::buildAiTab()
{
    auto* agentiPage = new AgentiPage(m_ai, this);
    connect(agentiPage, &AgentiPage::chatCompleted,
            this,       &MainWindow::onChatCompleted);
    connect(agentiPage, &AgentiPage::pipelineStatus,
            this,       &MainWindow::onPipelineStatus);
    connect(agentiPage, &AgentiPage::requestOpenSettings,
            this, &MainWindow::onGraficoRequestSettings);
    connect(agentiPage, &AgentiPage::requestShowInGrafico,
            this, &MainWindow::onRequestShowInGrafico);
    m_mainTabs->addTab(agentiPage, "\xf0\x9f\xa4\x96  Intelligenza artificiale");  /* 0 */
}

/* ── Livello 2: tab [1] Strumenti ────────────────────────────────── */
void MainWindow::buildStrumentiTab()
{
    m_strumentiPage = new StrumentiPage(m_ai, this);
    connect(m_strumentiPage, &StrumentiPage::cronPanelFirstOpen,
            this, &MainWindow::onCronPanelFirstOpen);
    m_mainTabs->addTab(m_strumentiPage, "\xf0\x9f\x9b\xa0\xef\xb8\x8f  Strumenti");  /* 1 */
}

/* ── Livello 2: tab [2] Multimedia ───────────────────────────────── */
void MainWindow::buildMultimediaTab()
{
    m_mainTabs->addTab(new MultimediaPage(m_ai, this),
                       "\xf0\x9f\x8e\xac  Multimedia");  /* 2 */
}

/* ── Livello 2: tab [3] File AI ──────────────────────────────────── */
void MainWindow::buildFileAiTab()
{
    m_mainTabs->addTab(new StrumentiFilePage(m_ai, this),
                       "\xf0\x9f\x93\x81  File AI");  /* 3 */
}

/* ── Livello 2: tab [3] Programmazione ───────────────────────────── */
void MainWindow::buildProgrammazioneTab()
{
    m_mainTabs->addTab(new ProgrammazionePage(m_ai, this),
                       "\xf0\x9f\x92\xbb  Programmazione");  /* 3 */
}

/* ── Livello 2: tab [4] Matematica + Grafico ─────────────────────── */
void MainWindow::buildMatematicaTab()
{
    auto* grafPage = new GraficoPage(m_ai, this);
    m_grafCanvas = grafPage->canvas();
    if (m_impPage) m_impPage->setGraficoCanvas(m_grafCanvas);
    connect(grafPage, &GraficoPage::requestOpenSettings,
            this, &MainWindow::onGraficoRequestSettings);

    auto* mathContainer = new QWidget(m_mainTabs);
    auto* mcLay = new QVBoxLayout(mathContainer);
    mcLay->setContentsMargins(0, 0, 0, 0);
    mcLay->setSpacing(0);

    auto* mathSubTabs = new QTabWidget(mathContainer);
    mathSubTabs->setObjectName("mathSubTabs");
    mathSubTabs->setTabPosition(QTabWidget::North);
    mathSubTabs->addTab(new MatematicaPage(m_ai, mathContainer), "\xf0\x9f\x93\x90  Matematica");
    mathSubTabs->addTab(grafPage, "\xf0\x9f\x93\x88  Grafico");
    connect(mathSubTabs, &QTabWidget::currentChanged,
            this, &MainWindow::onMathSubTabChanged);

    mcLay->addWidget(mathSubTabs);
    m_mainTabs->addTab(mathContainer, "\xf0\x9f\x93\x90  Matematica");  /* 4 */
}

/* ── Livello 2: tab [5] Ricerca ──────────────────────────────────── */
void MainWindow::buildRicercaTab()
{
    m_ricercaPage = new RicercaPage(m_ai, this);
    m_mainTabs->addTab(m_ricercaPage, "\xf0\x9f\x94\xac  Ricerca");  /* 5 */
}

/* ── Livello 2: tab [6] APP Controller ───────────────────────────── */
void MainWindow::buildAppControllerTab()
{
    auto* appCtrl = new AppControllerPage(m_ai, this);
    connect(appCtrl, &AppControllerPage::openSettingsDipendenze,
            this,    &MainWindow::onOpenSettingsDipendenze);
    m_mainTabs->addTab(appCtrl, "\xf0\x9f\x95\xb9\xef\xb8\x8f  APP Controller");  /* 6 */
}

/* ── Livello 2: tab [7] LAN & WAN ────────────────────────────────── */
void MainWindow::buildLanWanTab()
{
    auto* lanWan = new LanWanPage(m_ai, this);
    m_mainTabs->addTab(lanWan, "\xf0\x9f\x8c\x90  LAN & WAN");  /* 7 */
    /* Multi-Agente è ora un tab interno a LanWanPage — recupera il riferimento
     * per poter collegare la cross-pollination con RagGraph. */
    m_agentiMultiPage = lanWan->multiAgentTab();
}

/* ── Livello 2: ex tab [9] Multi-Agente — ora embedded in LAN & WAN ── */
void MainWindow::buildMultiAgentTab()
{
    /* Nessun tab separato: Multi-Agente vive dentro LanWanPage.
     * Qui impostiamo solo la cross-pollination con il RagGraph. */
    if (m_ricercaPage && m_agentiMultiPage)
        m_agentiMultiPage->setExtRagMemory(m_ricercaPage->ragGraphMemory());
}

/* ── Livello 2: tab [9] 🧬 Distillazione Sintetica ─────────────── */
void MainWindow::buildDistillazioneTab()
{
    m_distillazionePage = new DistillazionePage(m_ai, this);
    m_mainTabs->addTab(m_distillazionePage,
                       "\xf0\x9f\xa7\xac  Distillazione");  /* 9 */
}

/* ── Livello 2: barra navigazione menu + sincronizzazione tab ────── */
void MainWindow::buildNavMenuBar(QWidget* wrapper, QVBoxLayout* /*wLay*/)
{
    m_navMenuBar = new QFrame(wrapper);
    m_navMenuBar->setObjectName("navMenuBar");
    m_navMenuBar->setFixedHeight(dpiScale(40));
    m_navMenuBar->hide();
    auto* nmLay = new QHBoxLayout(m_navMenuBar);
    nmLay->setContentsMargins(8, 2, 8, 2);
    nmLay->setSpacing(2);

    auto* btnGroup = new QButtonGroup(m_navMenuBar);
    btnGroup->setExclusive(true);
    for (int i = 0; i < m_mainTabs->count(); i++) {
        auto* btn = new QPushButton(m_tabOrigLabels.at(i), m_navMenuBar);
        btn->setObjectName("navMenuBtn");
        btn->setCheckable(true);
        btn->setChecked(i == 0);
        btn->setFlat(true);
        btnGroup->addButton(btn, i);
        m_navBtns << btn;
        nmLay->addWidget(btn);
    }
    connect(btnGroup, &QButtonGroup::idClicked,
            m_mainTabs, &QTabWidget::setCurrentIndex);
    nmLay->addStretch();

    /* Clone del backend button all'estrema destra della nav bar */
    if (m_btnBackend) {
        auto* backendClone = new QPushButton(m_navMenuBar);
        backendClone->setObjectName("navMenuBackend");
        backendClone->setFlat(true);
        backendClone->setFixedHeight(dpiScale(30));
        m_navBackendClone = backendClone;
        connect(m_btnBackend,  &QPushButton::clicked, this, &MainWindow::onSyncNavBackendClone);
        onSyncNavBackendClone();
        connect(backendClone, &QPushButton::clicked, m_btnBackend, &QPushButton::click);
        nmLay->addWidget(backendClone);
    }

    connect(m_mainTabs, &QTabWidget::currentChanged, this, &MainWindow::onMainTabChanged);
}

/* ── Livello 2: applica stile nav e modalità exec btn da QSettings ── */
void MainWindow::applyContentSettings()
{
    QSettings s("Prismalux", "GUI");
    applyNavStyle(s.value(P::SK::kNavStyle, "tabs_top").toString());
    const QString execMode = s.value(P::SK::kNavExecBtnMode, "icon_text").toString();
    if (execMode != "icon_text") {
        m_pendingExecMode = execMode;
        QTimer::singleShot(0, this, &MainWindow::onApplyExecBtnMode);
    }
}

/* ══════════════════════════════════════════════════════════════
   ensureSettingsDialog — crea il dialog Impostazioni la prima volta (lazy).
   Sicuro da chiamare più volte (no-op se già creato).
   IMPORTANTE: nessun Qt::Window — evita il crash Windows nella
   gestione parent-child quando QDialog ha sia Qt::Window che un parent.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::ensureSettingsDialog()
{
    if (m_impDlg) return;
    m_impDlg = new QDialog(this);
    m_impDlg->setWindowTitle("\xe2\x9a\x99\xef\xb8\x8f  Impostazioni \xe2\x80\x94 Prismalux");
    /* NO Qt::Window flag — QDialog default flags funzionano correttamente
       su tutte le piattaforme senza scatenare bug Windows API parent-child */
    m_impDlg->setAttribute(Qt::WA_DeleteOnClose, false);
    m_impDlg->resize(dpiScale(1050), dpiScale(680));
    m_impPage = new ImpostazioniPage(m_ai, m_hw, m_impDlg);
    m_impPage->setGraficoCanvas(m_grafCanvas);
    /* installCronPanel è chiamata solo da onCronPanelFirstOpen (primo clic su Cron) */
    auto* dl = new QVBoxLayout(m_impDlg);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->addWidget(m_impPage);
    if (m_hw && m_hw->hwReady())
        m_impPage->onHWReady(m_hw->hwInfo());
    connect(m_impPage, &ImpostazioniPage::tabModeChanged,
            this,      &MainWindow::applyTabMode);
    connect(m_impPage, &ImpostazioniPage::navStyleChanged,
            this,      &MainWindow::applyNavStyle);
    connect(m_impPage, &ImpostazioniPage::execBtnModeChanged,
            this,      &MainWindow::applyExecBtnMode);
    if (auto* ap = findChild<AgentiPage*>())
        connect(m_impPage, &ImpostazioniPage::bubbleStyleChanged,
                ap, &AgentiPage::onBubbleStyleChanged);

    /* Feedback indicizzazione RAG nella status bar — visibile anche a dialog chiuso */
    connect(m_impPage, &ImpostazioniPage::indexingProgress,
            this, &MainWindow::onIndexingProgress);
    connect(m_impPage, &ImpostazioniPage::indexingFinished,
            this, &MainWindow::onIndexingFinished);

    /* Auto-trigger RagGraph dopo reindicizzazione RAG */
    if (m_ricercaPage)
        connect(m_impPage, &ImpostazioniPage::indexingFinished,
                m_ricercaPage, &RicercaPage::onAutoRagTrigger);

    /* ── Indicatore download LLM — visibile da qualsiasi tab ── */
    auto* man = m_impPage->manutenzione();
    if (man) {
        connect(man, &ManutenzioneePage::downloadStarted,
                this, [this](const QString& model) {
            if (m_dlStatusLbl) {
                m_dlStatusLbl->setText(
                    "\xe2\xac\x87 " + model + "  \xe2\x8f\xb3");
                m_dlStatusLbl->setVisible(true);
            }
        }, Qt::QueuedConnection);

        connect(man, &ManutenzioneePage::downloadProgress,
                this, [this](const QString& line) {
            if (!m_dlStatusLbl || !m_dlStatusLbl->isVisible()) return;
            /* Mostra solo testo utile: taglia ANSI e righe troppo lunghe */
            QString clean = line;
            clean.remove(QRegularExpression("\x1b\\[[0-9;]*[A-Za-z]"));
            clean.remove('\r');
            clean = clean.trimmed().left(60);
            if (!clean.isEmpty())
                m_dlStatusLbl->setText("\xe2\xac\x87 " + clean + "  \xe2\x8f\xb3");
        }, Qt::QueuedConnection);

        connect(man, &ManutenzioneePage::downloadFinished,
                this, [this](bool ok, const QString& model) {
            if (!m_dlStatusLbl) return;
            if (ok) {
                m_dlStatusLbl->setText("\xe2\x9c\x85 " + model + " scaricato");
                /* Nasconde la label dopo 5 secondi */
                QTimer::singleShot(5000, m_dlStatusLbl, [this]{
                    if (m_dlStatusLbl) m_dlStatusLbl->setVisible(false);
                });
            } else {
                m_dlStatusLbl->setText("\xe2\x9d\x8c Download " + model + " fallito");
                QTimer::singleShot(8000, m_dlStatusLbl, [this]{
                    if (m_dlStatusLbl) m_dlStatusLbl->setVisible(false);
                });
            }
        }, Qt::QueuedConnection);
    }
}

/* ══════════════════════════════════════════════════════════════
   openSettingsDialog — apre Impostazioni (invocabile da AiErrorWidget).
   ══════════════════════════════════════════════════════════════ */
void MainWindow::openSettingsDialog()
{
    ensureSettingsDialog();
    m_impDlg->show();
    m_impDlg->raise();
    m_impDlg->activateWindow();
}

/* ══════════════════════════════════════════════════════════════
   ensureLogDialog — dialog Messaggi/Log (creato lazy, non-modale).
   ══════════════════════════════════════════════════════════════ */
void MainWindow::ensureLogDialog()
{
    if (m_logDlg) return;

    m_logDlg = new QDialog(this);
    m_logDlg->setWindowTitle("\xf0\x9f\x93\x8b  Messaggi \xe2\x80\x94 Prismalux");
    m_logDlg->setAttribute(Qt::WA_DeleteOnClose, false);
    m_logDlg->resize(dpiScale(700), dpiScale(460));

    auto* lay = new QVBoxLayout(m_logDlg);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    /* Intestazione */
    auto* header = new QLabel(
        "\xf0\x9f\x93\x8b  <b>Log eventi</b> \xe2\x80\x94 backend, AI, pipeline, errori");
    header->setTextFormat(Qt::RichText);
    header->setObjectName("sectionTitle");
    lay->addWidget(header);

    /* Area log */
    m_logView = new QTextEdit(m_logDlg);
    m_logView->setReadOnly(true);
    m_logView->setObjectName("chatLog");
    m_logView->setPlaceholderText("Nessun messaggio. Gli eventi verranno registrati qui.");
    lay->addWidget(m_logView, 1);

    /* Pulsanti */
    auto* btnRow = new QWidget(m_logDlg);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    auto* clearBtn = new QPushButton("\xf0\x9f\x97\x91  Pulisci log", btnRow);
    clearBtn->setObjectName("actionBtn");
    clearBtn->setFixedHeight(dpiScale(32));
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);

    auto* closeBtn = new QPushButton("Chiudi", btnRow);
    closeBtn->setObjectName("actionBtn");
    closeBtn->setFixedHeight(dpiScale(32));
    connect(closeBtn, &QPushButton::clicked, m_logDlg, &QDialog::hide);

    btnLay->addStretch();
    btnLay->addWidget(clearBtn);
    btnLay->addWidget(closeBtn);
    lay->addWidget(btnRow);
}

/* ══════════════════════════════════════════════════════════════
   appendLog — aggiunge una riga al log con timestamp.
   Incrementa il badge se il dialog è nascosto.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::appendLog(const QString& msg)
{
    ensureLogDialog();

    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    const QString line = QString("<span style='color:#888;'>%1</span> &nbsp;%2")
                         .arg(ts, msg);   /* msg è già HTML — caller usa toHtmlEscaped() sui dati utente */
    m_logView->moveCursor(QTextCursor::End);
    m_logView->insertHtml(line + "<br>");

    /* Badge non-letti — visibile solo se il dialog è chiuso */
    if (!m_logDlg->isVisible()) {
        m_logUnread++;
        const int cap = qMin(m_logUnread, 99);
        m_logBadge->setText(cap < 99 ? QString::number(cap) : "99+");
        m_logBadge->setVisible(true);
    }
}

/* ══════════════════════════════════════════════════════════════
   applyTabMode — aggiorna le etichette di m_mainTabs in tempo reale.
   Formato originale: "icona  testo" (separatore = 2 spazi).
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyTabMode(const QString& mode)
{
    if (!m_mainTabs || m_tabOrigLabels.isEmpty()) return;
    const int n = qMin(m_mainTabs->count(), m_tabOrigLabels.size());
    for (int i = 0; i < n; i++) {
        const QString& orig = m_tabOrigLabels.at(i);
        const int sep = orig.indexOf("  ");   /* 2 spazi tra icona e testo */
        if (sep < 0) { m_mainTabs->setTabText(i, orig); continue; }
        const QString icon = orig.left(sep);
        const QString text = orig.mid(sep + 2);
        QString label;
        if      (mode == "icons_only") label = icon;
        else if (mode == "text_icons") label = text + "  " + icon;
        else if (mode == "text_only")  label = text;
        else                           label = orig;  /* icons_text = default */
        m_mainTabs->setTabText(i, label);
    }
}

/* ══════════════════════════════════════════════════════════════
   applyExecBtnMode — aggiorna il testo di tutti i pulsanti di esecuzione.
   Scansiona l'albero dei widget cercando QPushButton con proprietà execIcon.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyExecBtnMode(const QString& mode)
{
    const auto btns = findChildren<QPushButton*>();
    for (auto* btn : btns) {
        const QVariant iconVar = btn->property("execIcon");
        if (!iconVar.isValid() || iconVar.isNull()) continue;
        const QString icon = iconVar.toString();
        const QString text = btn->property("execText").toString();
        const QString full = btn->property("execFull").toString();
        if (mode == "icon_only") btn->setText(icon);
        else if (mode == "text_only") btn->setText(text);
        else btn->setText(full.isEmpty() ? icon + "  " + text : full);
    }
}

/* ══════════════════════════════════════════════════════════════
   applyNavStyle — alterna tra schede in alto e menù orizzontale.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyNavStyle(const QString& style)
{
    if (!m_mainTabs) return;
    const bool isMenu = (style == "menu_main");
    m_mainTabs->tabBar()->setVisible(!isMenu);
    if (m_navMenuBar) m_navMenuBar->setVisible(isMenu);
}

/* ══════════════════════════════════════════════════════════════
   Navigazione
   ══════════════════════════════════════════════════════════════ */
void MainWindow::navigateTo(int idx) {
    if (idx < 0 || idx > 6) return;
    /* Interrompe qualsiasi richiesta AI in corso prima di cambiare pagina:
       evita segnali fantasma (token/finished) che arrivano alla pagina precedente. */
    if (m_ai && m_ai->busy()) m_ai->abort();
    if (m_mainTabs) m_mainTabs->setCurrentIndex(idx);
}

/* ══════════════════════════════════════════════════════════════
   Slot hardware
   ══════════════════════════════════════════════════════════════ */
void MainWindow::onHWUpdated(SysSnapshot snap) {
    if (!m_gCpu || !m_gRam || !m_gGpu) return;
    m_gCpu->update(snap.cpu_pct, snap.cpu_name);
    double rp = snap.ram_total > 0 ? snap.ram_used/snap.ram_total*100.0 : 0;
    m_gRam->update(rp, QString("%1/%2 GB")
                   .arg(snap.ram_used,0,'f',1)
                   .arg(snap.ram_total,0,'f',1));

    /* Aggiorna percentuale RAM libera in AiClient (usata per throttle pre-chat) */
    double freePct = snap.ram_total > 0 ? (snap.ram_total - snap.ram_used) / snap.ram_total * 100.0 : 100.0;
    m_ai->setRamFreePct(freePct);

    /* Colora "Scarica LLM" via property QSS: normale→warn→high */
    if (m_btnUnload) {
        const char* urgency = rp > 75.0 ? "high" : rp > 40.0 ? "warn" : "";
        if (m_btnUnload->property("urgency").toString() != urgency) {
            m_btnUnload->setProperty("urgency", urgency);
            P::repolish(m_btnUnload);
        }
    }

    /* Auto-abort: se RAM >97% usata e AI occupato, interrompe subito.
     * Evita che il sistema si blocchi completamente durante l'inference.
     * Soglia 97%: modelli grandi usano ~80-90% RAM normalmente — abort solo in OOM reale. */
    if (rp > 97.0 && m_ai->busy()) {
        m_ai->abort();
        statusBar()->showMessage(
            "\xe2\x9a\xa0  RAM critica — inference AI interrotta automaticamente per proteggere il sistema.");
    }
    if (snap.gpu_ready) {
        QString gpuTip = snap.gpu_name;
        if (snap.vram_total > 0)
            gpuTip += QString(" | VRAM %1/%2 GB")
                      .arg(snap.vram_used,0,'f',1)
                      .arg(snap.vram_total,0,'f',1);
        m_gGpu->update(snap.gpu_pct, gpuTip);
    } else {
        m_gGpu->update(0, "Rilevamento...");
    }

    /* Indicatore temperatura header */
    if (m_tempLbl) {
        QStringList parts;
        if (snap.cpu_temp_c >= 0)
            parts << QString("CPU %1\xc2\xb0" "C").arg((int)snap.cpu_temp_c);
        if (snap.gpu_temp_c >= 0)
            parts << QString("GPU %1\xc2\xb0" "C").arg((int)snap.gpu_temp_c);
        if (!parts.isEmpty()) {
            m_tempLbl->setText("\xf0\x9f\x8c\xa1  " + parts.join(" | "));  /* 🌡 */
            const double maxTemp = qMax(snap.cpu_temp_c, snap.gpu_temp_c);
            if (maxTemp >= 90) {
                m_tempLbl->setStyleSheet(
                    "QLabel#tempLabel{color:#f87171;font-size:11px;padding:0 4px;font-weight:bold;}");
                if (!m_thermalCriticalWarned) {
                    m_thermalCriticalWarned = true;
                    QTimer::singleShot(0, this, [this, maxTemp]() {
                        auto* mb = new QMessageBox(this);
                        mb->setWindowTitle(tr("Temperatura critica"));
                        mb->setIcon(QMessageBox::Warning);
                        mb->setText(
                            QString("\xf0\x9f\x8c\xa1  <b>Temperatura %1\xc2\xb0""C</b> \xe2\x80\x94 "
                                    "rischio throttling o danno hardware!<br><br>"
                                    "Vuoi fermare il flusso AI per ridurre il carico?")
                                .arg((int)maxTemp));
                        mb->setTextFormat(Qt::RichText);
                        auto* btnStop = mb->addButton(
                            "\xf0\x9f\x9b\x91  Ferma AI", QMessageBox::AcceptRole);
                        mb->addButton(tr("Continua"), QMessageBox::RejectRole);
                        mb->setAttribute(Qt::WA_DeleteOnClose);
                        connect(mb, &QMessageBox::finished, this,
                                [this, mb, btnStop](int) {
                            if (mb->clickedButton() == btnStop)
                                m_ai->abort();
                        });
                        mb->show();
                    });
                }
            } else if (maxTemp >= 75)
                m_tempLbl->setStyleSheet(
                    "QLabel#tempLabel{color:#f59e0b;font-size:11px;padding:0 4px;}");
            else {
                m_tempLbl->setStyleSheet(
                    "QLabel#tempLabel{color:#94a3b8;font-size:11px;padding:0 4px;}");
                m_thermalCriticalWarned = false;  /* resetta quando la temp scende */
            }
            m_tempLbl->setVisible(true);
        }
    }

    /* Invia temperatura alla RicercaPage per il RAG thermal throttle */
    if (m_ricercaPage)
        m_ricercaPage->onThermalUpdate(snap.cpu_temp_c, snap.gpu_temp_c);
}

/* ══════════════════════════════════════════════════════════════
   maybeAutoVramBench — benchmark VRAM automatico al primo avvio
   ══════════════════════════════════════════════════════════════ */
void MainWindow::maybeAutoVramBench() {
    /* Già eseguito in precedenza → skip */
    if (QFileInfo::exists(P::vramProfilePath())) return;

    /* Binario non compilato → skip silenzioso */
    const QString bench = P::vramBenchBin();
    if (!QFileInfo::exists(bench)) return;

    /* Solo con Ollama attivo (il benchmark interroga /api/tags) */
    if (m_ai->backend() != AiClient::Ollama) return;

    statusBar()->showMessage(
        "\xf0\x9f\x94\xac  Primo avvio: benchmark VRAM in corso (background)...");

    auto* proc = new QProcess(this);
    proc->setWorkingDirectory(P::root() + "/C_software");
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onVramBenchFinished);

    proc->start(bench, {});
    if (!proc->waitForStarted(3000)) {
        proc->deleteLater();
        statusBar()->showMessage(
            "\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
    }
}

/* ── Chiusura finestra — pulizia RAM residua ── */
void MainWindow::closeEvent(QCloseEvent* ev) {

    /* Se l'AI sta elaborando, chiedi se fermare e scaricare il modello */
    if (m_ai->busy()) {
        QMessageBox dlg(this);
        dlg.setWindowTitle("Prismalux — Chiusura");
        dlg.setIcon(QMessageBox::Question);
        dlg.setText("<b>Un agente AI \xc3\xa8 ancora in elaborazione.</b><br>"
                    "Vuoi fermare la generazione e scaricare il modello dalla RAM?");
        dlg.setInformativeText(
            "Modello attivo: <b>" + m_ai->model() + "</b><br>"
            "Se non lo scarichi rimarr\xc3\xa0 in memoria anche dopo la chiusura.");
        auto* btnUnload = dlg.addButton("Ferma e scarica dalla RAM", QMessageBox::AcceptRole);
        dlg.addButton("Chiudi comunque",                               QMessageBox::DestructiveRole);
        auto* btnCancel = dlg.addButton("Annulla",                     QMessageBox::RejectRole);
        dlg.setDefaultButton(btnCancel);
        dlg.exec();

        if (dlg.clickedButton() == btnCancel || dlg.clickedButton() == nullptr) {
            ev->ignore();   /* Annulla — non chiudere */
            return;
        }

        /* Ferma la generazione corrente */
        m_ai->abort();

        if (dlg.clickedButton() == btnUnload) {
            /* Ollama: DELETE /api/delete scarica il modello dalla VRAM/RAM */
            QString host  = m_ai->host();
            int     port  = m_ai->port();
            QString model = m_ai->model();
            if (!model.isEmpty()) {
                QNetworkAccessManager* nam = new QNetworkAccessManager(this);
                QNetworkRequest req(QUrl(
                    QString("http://%1:%2/api/delete").arg(host).arg(port)));
                req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                QJsonObject body; body["name"] = model;
                QNetworkReply* reply = nam->sendCustomRequest(
                    req, "DELETE", QJsonDocument(body).toJson());
                /* Attendi al massimo 2 secondi poi chiudi comunque */
                QTimer::singleShot(2000, reply, &QNetworkReply::abort);
            }
        }
    }

    /* Ferma llama-server avviato dalla GUI (se in esecuzione) */
    if (m_serverProc && m_serverProc->state() != QProcess::NotRunning) {
        m_serverProc->terminate();
        m_serverProc->waitForFinished(3000);
    }

    /* Libera cache RAM residua (best-effort, senza dialogo) */
#ifndef Q_OS_WIN
    QProcess::startDetached("bash", {"-c",
        "ollama ps --no-trunc 2>/dev/null | awk 'NR>1{print $1}' | "
        "xargs -r -I{} ollama stop {} 2>/dev/null; "
        "sync 2>/dev/null"});
#else
    QProcess::startDetached("cmd.exe", {"/c",
        "for /f \"skip=1 tokens=1\" %m in ('ollama ps --no-trunc 2^>nul') do ollama stop %m"});
#endif

    /* Salva geometry e stato finestra per il prossimo avvio */
    QSettings s("Prismalux", "GUI");
    s.setValue("mainwindow/geometry", saveGeometry());
    s.setValue("mainwindow/state",    saveState());

    ev->accept();
}

void MainWindow::onHWReady(HWInfo hw) {
    const HWDevice& pd = hw.dev[hw.primary];

    /* Aggiorna label GPU nell'header */
    if (pd.type != DEV_CPU) {
        QString gpuName = QString::fromLocal8Bit(pd.name);
        m_lblModel->setText(gpuName);  /* temporaneo, sovrascritto dal modello AI */
    } else {
        /* Xeon → nessun allarme */
        QString cpuName = QString::fromLocal8Bit(hw.dev[0].name);
        if (cpuName.contains("Xeon", Qt::CaseInsensitive))
            statusBar()->showMessage("✓ Xeon rilevato — elaborazione CPU ad alta performance");
    }

    /* Notifica la pagina impostazioni se già creata */
    if (m_impPage) m_impPage->onHWReady(hw);
}

/* ══════════════════════════════════════════════════════════════
   Chat History — salvataggio e lista
   ══════════════════════════════════════════════════════════════ */
void MainWindow::onChatCompleted(const QString& title, const QString& logHtml) {
    /* Se siamo già in una sessione attiva, aggiorna quella (chat continua).
       Se la sessione è vuota o non esiste più, crea una nuova voce. */
    bool sessionValid = !m_currentChatId.isEmpty()
        && !m_chatHistory.loadLog(m_currentChatId).isEmpty();

    if (sessionValid) {
        m_chatHistory.saveLog(m_currentChatId, logHtml);
    } else {
        m_currentChatId = m_chatHistory.newSession(title);
        m_chatHistory.saveLog(m_currentChatId, logHtml);
    }
    refreshChatList();

    appendLog(QString("\xe2\x9c\x85 Pipeline completata: <b>%1</b>")
              .arg(title.isEmpty() ? "(senza titolo)" : title.toHtmlEscaped()));
}

/* Slot: usa le funzioni statiche stripBodyBackground/migrateLegacyChat definite sopra */
void MainWindow::onChatItemClicked(QListWidgetItem* item)
{
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;
    const QString rawHtml = m_chatHistory.loadLog(id);
    if (rawHtml.isEmpty()) return;
    const QString html = stripBodyBackground(migrateLegacyChat(rawHtml));
    if (auto* ap = m_mainTabs ? m_mainTabs->widget(0) : nullptr) {
        if (auto* log = ap->findChild<QTextEdit*>()) {
            log->setHtml(html);
            log->moveCursor(QTextCursor::End);
        }
    }
    m_currentChatId = id;
    navigateTo(0);
}

void MainWindow::refreshChatList() {
    if (!m_chatList) return;
    m_chatList->clear();

    const auto sessions = m_chatHistory.list();

    /* Conta quante sessioni hanno lo stesso titolo — per i duplicati
     * aggiunge " · HH:mm" così l'utente può distinguerle visivamente.
     * Il caricamento resta sempre basato sull'ID univoco (Qt::UserRole). */
    QMap<QString, int> titleCount;
    for (const auto& s : sessions) {
        const QString base = s.title.isEmpty() ? "(senza titolo)" : s.title;
        titleCount[base]++;
    }

    if (sessions.isEmpty()) {
        auto* placeholder = new QListWidgetItem(
            "\xf0\x9f\x92\xac  Nessuna chat salvata\n"
            "Inizia una conversazione\nnella pagina AI");  /* 💬 */
        placeholder->setFlags(Qt::NoItemFlags);  /* non selezionabile */
        placeholder->setForeground(QColor("#888"));
        placeholder->setTextAlignment(Qt::AlignCenter);
        m_chatList->addItem(placeholder);
        return;
    }

    for (const auto& s : sessions) {
        const QString base = s.title.isEmpty() ? "(senza titolo)" : s.title;
        QString display = base;
        if (titleCount.value(base) > 1)
            display += " \xc2\xb7 " + s.createdAt.toString("HH:mm");
        auto* item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, s.id);
        item->setToolTip(s.createdAt.toString("dd/MM/yyyy HH:mm:ss"));
        m_chatList->addItem(item);
    }
}

void MainWindow::onPipelineStatus(int pct, const QString& text) {
    if (!m_statusProgress) return;
    if (pct < 0) {
        /* Resetta: pipeline terminata — nascondi la barra */
        m_statusProgress->setValue(0);
        m_statusProgress->setFormat("");
        m_statusProgress->setVisible(false);
        return;
    }
    m_statusProgress->setVisible(true);
    m_statusProgress->setValue(pct);
    if (!text.isEmpty()) {
        m_statusProgress->setFormat(text);
        statusBar()->showMessage(text, 8000);
    }
}

/* ══════════════════════════════════════════════════════════════
   showOnboardingWizard — dialog di benvenuto al primo avvio.
   3 step: backend, modello consigliato, tema.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::showOnboardingWizard()
{
    QSettings ss("Prismalux", "GUI");

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Benvenuto in Prismalux \xf0\x9f\x8d\xba");
    dlg->setMinimumWidth(dpiScale(480));
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto* vlay = new QVBoxLayout(dlg);
    vlay->setSpacing(14);

    /* Logo + titolo */
    auto* hdrLbl = new QLabel(
        "<h2 style='margin:0'>\xf0\x9f\x8d\xba  Benvenuto in Prismalux</h2>"
        "<p style='color:#6c63ff;margin:4px 0 0 0;'>"
        "Costruito per i mortali che aspirano alla saggezza.</p>", dlg);
    hdrLbl->setTextFormat(Qt::RichText);
    vlay->addWidget(hdrLbl);

    auto* sep = new QFrame(dlg);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    vlay->addWidget(sep);

    /* Step 1 — backend */
    auto* backendGrp = new QGroupBox("1. Backend AI", dlg);
    auto* bLay = new QVBoxLayout(backendGrp);
    auto* backendCombo = new QComboBox(backendGrp);
    backendCombo->addItem("\xf0\x9f\xa6\x99  Ollama (consigliato)", 0);
    backendCombo->addItem("\xf0\x9f\xa6\x99  llama-server (avanzato)", 1);
    const int savedBe = ss.value(P::SK::kActiveBackend, 0).toInt();
    backendCombo->setCurrentIndex(savedBe < backendCombo->count() ? savedBe : 0);
    auto* backendHint = new QLabel(
        "<small style='color:#888'>Ollama: scarica ed esegui modelli con un click. "
        "llama-server: maggior controllo su layer GPU.</small>", backendGrp);
    backendHint->setTextFormat(Qt::RichText);
    backendHint->setWordWrap(true);
    bLay->addWidget(backendCombo);
    bLay->addWidget(backendHint);
    vlay->addWidget(backendGrp);

    /* Step 2 — modello consigliato */
    auto* modelGrp = new QGroupBox("2. Modello consigliato", dlg);
    auto* mLay = new QVBoxLayout(modelGrp);
    auto* modelCombo = new QComboBox(modelGrp);
    modelCombo->addItem("qwen3:4b  \xe2\x80\x94  ~2.6 GB  (8 GB RAM, veloce)", "qwen3:4b");
    modelCombo->addItem("qwen3:8b  \xe2\x80\x94  ~5 GB   (16 GB RAM, bilanciato)", "qwen3:8b");
    modelCombo->addItem("qwen3:14b \xe2\x80\x94  ~9 GB   (16 GB RAM, qualit\xc3\xa0)", "qwen3:14b");
    modelCombo->addItem("qwen3:30b \xe2\x80\x94  ~19 GB  (32 GB RAM, ottimo)", "qwen3:30b");
    modelCombo->addItem("gemma3:4b \xe2\x80\x94  ~3 GB   (8 GB RAM, multimodale)", "gemma3:4b");
    const QString savedModel = ss.value(P::SK::kActiveModel, "").toString();
    int modelIdx = savedModel.isEmpty() ? 0 : modelCombo->findData(savedModel);
    if (modelIdx < 0) modelIdx = 0;
    modelCombo->setCurrentIndex(modelIdx);
    auto* modelHint = new QLabel(
        "<small style='color:#888'>Puoi cambiarlo in qualsiasi momento da "
        "Impostazioni \xe2\x86\x92 AI Locale \xe2\x86\x92 LLM Consigliati.</small>", modelGrp);
    modelHint->setTextFormat(Qt::RichText);
    modelHint->setWordWrap(true);
    mLay->addWidget(modelCombo);
    mLay->addWidget(modelHint);
    vlay->addWidget(modelGrp);

    /* Step 3 — tema */
    auto* themeGrp = new QGroupBox("3. Tema", dlg);
    auto* tLay = new QVBoxLayout(themeGrp);
    auto* themeCombo = new QComboBox(themeGrp);
    const QStringList themes = {
        "dark_ocean","dark_cyan","dark_purple","dark_green",
        "light_blue","light_gray","dracula","monokai","solarized_dark"
    };
    for (const auto& t : themes) themeCombo->addItem(t, t);
    const QString savedTheme = ss.value(P::SK::kTheme, P::SK::kDefaultTheme).toString();
    int themeIdx = themeCombo->findData(savedTheme);
    themeCombo->setCurrentIndex(themeIdx >= 0 ? themeIdx : 0);
    tLay->addWidget(themeCombo);
    vlay->addWidget(themeGrp);

    /* Bottoni */
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok, dlg);
    btnBox->button(QDialogButtonBox::Ok)->setText(
        "\xf0\x9f\x8d\xba  Inizia!");
    vlay->addWidget(btnBox);

    m_onbBackend = backendCombo;
    m_onbModel   = modelCombo;
    m_onbTheme   = themeCombo;
    m_onbDlg     = dlg;
    connect(btnBox, &QDialogButtonBox::accepted, this, &MainWindow::onOnboardingAccepted);

    dlg->exec();
}

/* ══════════════════════════════════════════════════════════════
   onMathDlBtnClicked — slot del pulsante "Scarica modello matematico"
   dentro showServerDialog() (ex lambda)
   ══════════════════════════════════════════════════════════════ */
void MainWindow::onMathDlBtnClicked()
{
    /* Il pulsante è figlio di dlg (il QDialog di showServerDialog) */
    auto* dlg = qobject_cast<QDialog*>(sender()->parent());
    showMathDownloadDialog(dlg ? static_cast<QWidget*>(dlg) : this, P::modelsDir());
}

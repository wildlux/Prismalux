#include "monitor_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QDateTime>
#include <QSplitter>
#include <QFont>
#include <algorithm>

/* ── Colonne tabella ── */
enum Col {
    COL_TIME = 0,
    COL_MODEL,
    COL_BACKEND,
    COL_TTFT,
    COL_DUR,
    COL_TOKENS,
    COL_TOKPS,
    COL_RAM_BEFORE,
    COL_RAM_DELTA,
    COL_SCORE,
    COL_STATUS,
    COL_COUNT
};

static const char* COL_HEADERS[] = {
    "Ora", "Modello", "Backend",
    "TTFT (ms)", "Durata (ms)", "Token",
    "tok/s", "RAM pre (%)", "\u0394 RAM (%)",
    "Score", "Stato"
};

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
MonitorPanel::MonitorPanel(AiClient* ai, HardwareMonitor* hw, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("\xf0\x9f\x93\x8a  Monitor Attivit\xc3\xa0 AI — Diagnostica");
    setMinimumSize(980, 600);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

    /* ── Layout principale ── */
    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(12, 10, 12, 10);
    mainLay->setSpacing(8);

    /* ── Barra superiore ── */
    auto* topBar  = new QWidget(this);
    auto* topLay  = new QHBoxLayout(topBar);
    topLay->setContentsMargins(0,0,0,0); topLay->setSpacing(10);

    auto* title = new QLabel(
        "<b>\xf0\x9f\x93\x8a Monitor Attivit\xc3\xa0 AI</b>"
        " &nbsp;<span style='color:#888; font-size:11px;'>"
        "— ogni inferenza viene misurata e valutata</span>",
        topBar);
    topLay->addWidget(title, 1);

    m_liveInfo = new QLabel("In attesa...", topBar);
    m_liveInfo->setStyleSheet("color:#aaa; font-size:11px;");
    topLay->addWidget(m_liveInfo);

    auto* clearBtn = new QPushButton("\xf0\x9f\x97\x91  Pulisci", topBar);
    clearBtn->setFixedWidth(90);
    connect(clearBtn, &QPushButton::clicked, this, [this]{
        m_table->setRowCount(0);
        m_log->clear();
        appendLog("--- registro pulito ---");
        if (m_chart) m_chart->clear();
    });
    topLay->addWidget(clearBtn);

    auto* exportBtn = new QPushButton("\xf0\x9f\x93\x8b  Copia log", topBar);
    exportBtn->setFixedWidth(100);
    connect(exportBtn, &QPushButton::clicked, this, [this]{
        m_log->selectAll();
        m_log->copy();
    });
    topLay->addWidget(exportBtn);

    mainLay->addWidget(topBar);

    /* ── Splitter verticale: tabella sopra, log sotto ── */
    auto* splitter = new QSplitter(Qt::Vertical, this);

    /* Tabella sessioni */
    m_table = new QTableWidget(0, COL_COUNT, this);
    QStringList headers;
    for (int i = 0; i < COL_COUNT; ++i) headers << COL_HEADERS[i];
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(COL_MODEL,   QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COL_BACKEND, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_STATUS,  QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setMinimumHeight(180);
    splitter->addWidget(m_table);

    /* Log live */
    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setFont(QFont("Monospace", 10));
    m_log->setMinimumHeight(160);
    splitter->addWidget(m_log);

    splitter->setSizes({280, 240});
    mainLay->addWidget(splitter, 1);

    /* ── Chart performance storico ── */
    m_chart = new PerfChartWidget(this);
    m_chart->setToolTip(
        "Barre blu   = throughput (tok/s) — scala relativa al massimo\n"
        "Barre colorate = score 0-100 (verde\u226590 | giallo\u226570 | arancio\u226550 | rosso<50)");
    mainLay->addWidget(m_chart);

    /* ── Legenda score ── */
    auto* legend = new QLabel(
        "<b>Score:</b> 90-100\xf0\x9f\x9f\xa2 Eccellente &nbsp;|&nbsp; "
        "70-89\xf0\x9f\x9f\xa1 Buono &nbsp;|&nbsp; "
        "50-69\xf0\x9f\x9f\xa0 Accettabile &nbsp;|&nbsp; "
        "&lt;50\xf0\x9f\x94\xb4 Lento &nbsp;&nbsp;&nbsp;"
        "<b>TTFT</b>: Time To First Token &nbsp;|&nbsp; "
        "<b>\u0394 RAM</b>: variazione RAM durante l'inferenza",
        this);
    legend->setStyleSheet("color:#888; font-size:10px; padding: 2px 0;");
    legend->setWordWrap(true);
    mainLay->addWidget(legend);

    /* ── Connessioni AiClient ── */
    connect(ai, &AiClient::requestStarted,
            this, &MonitorPanel::onRequestStarted);
    connect(ai, &AiClient::token,
            this, &MonitorPanel::onFirstToken);
    connect(ai, &AiClient::finished,
            this, &MonitorPanel::onFinished);
    connect(ai, &AiClient::error,
            this, &MonitorPanel::onError);
    connect(ai, &AiClient::aborted,
            this, &MonitorPanel::onAborted);

    /* ── Connessione HardwareMonitor ── */
    connect(hw, &HardwareMonitor::updated,
            this, &MonitorPanel::onHWUpdated);

    appendLog("Monitor avviato. In attesa di richieste AI...");
}

/* ══════════════════════════════════════════════════════════════
   Slot hardware
   ══════════════════════════════════════════════════════════════ */
void MonitorPanel::onHWUpdated(SysSnapshot snap) {
    /* Calcola % RAM usata dai GiB assoluti */
    m_lastRamUsed = (snap.ram_total > 0.0)
        ? (snap.ram_used / snap.ram_total * 100.0) : 0.0;

    /* Aggiorna live info se sessione attiva */
    if (m_cur.active) {
        qint64 elapsed = m_cur.timer.elapsed();
        m_liveInfo->setText(
            QString("\xe2\x8f\xb3 Inferenza in corso — modello: <b>%1</b> | "
                    "trascorsi: <b>%2 ms</b> | RAM: <b>%3%</b>")
            .arg(m_cur.model)
            .arg(elapsed)
            .arg(m_lastRamUsed, 0, 'f', 1));
    }
}

/* ══════════════════════════════════════════════════════════════
   Slot AiClient
   ══════════════════════════════════════════════════════════════ */
void MonitorPanel::onRequestStarted(const QString& model, const QString& backend) {
    /* Inizia nuova sessione */
    m_cur        = Session{};
    m_cur.model   = model.isEmpty() ? "(auto)" : model;
    m_cur.backend = backend;
    m_cur.ramBefore = m_lastRamUsed;
    m_cur.active  = true;
    m_firstToken  = false;
    m_cur.timer.start();

    const QString ts = QDateTime::currentDateTime().toString("dd/MM HH:mm:ss");
    appendLog(QString("[%1] \xe2\x96\xb6  Avvio richiesta").arg(ts));
    appendLog(QString("     Modello : %1").arg(m_cur.model));
    appendLog(QString("     Backend : %1").arg(m_cur.backend));
    appendLog(QString("     RAM pre : %1%").arg(m_cur.ramBefore, 0, 'f', 1));

    m_liveInfo->setText(
        QString("\xe2\x8f\xb3 Richiesta inviata — modello: <b>%1</b> | attendo primo token...")
        .arg(m_cur.model));
}

void MonitorPanel::onFirstToken(const QString& /*chunk*/) {
    if (!m_cur.active) return;
    m_cur.tokens++;

    /* TTFT: solo al primo token */
    if (!m_firstToken) {
        m_firstToken  = true;
        m_cur.ttftMs  = m_cur.timer.elapsed();
        const QString ts = QDateTime::currentDateTime().toString("dd/MM HH:mm:ss");
        appendLog(QString("[%1] \xe2\x9a\xa1 Primo token ricevuto — TTFT: %2 ms")
                  .arg(ts).arg(m_cur.ttftMs));
    }
}

void MonitorPanel::onFinished(const QString& full) {
    if (!m_cur.active) return;
    m_cur.durMs    = m_cur.timer.elapsed();
    m_cur.ramAfter = m_lastRamUsed;
    /* conta token dal testo finale se lo slot onFirstToken ne ha contati pochi */
    if (m_cur.tokens < 2)
        m_cur.tokens = full.split(' ', Qt::SkipEmptyParts).size();

    const QString ts = QDateTime::currentDateTime().toString("dd/MM HH:mm:ss");
    const double tokPs = (m_cur.durMs > 0)
        ? m_cur.tokens * 1000.0 / m_cur.durMs
        : 0.0;
    const double ramDelta = m_cur.ramAfter - m_cur.ramBefore;
    const int    score    = computeScore(m_cur.ttftMs, m_cur.durMs,
                                         m_cur.tokens, ramDelta);

    appendLog(QString("[%1] \xe2\x9c\x85 Completato").arg(ts));
    appendLog(QString("     Durata totale : %1 ms").arg(m_cur.durMs));
    appendLog(QString("     Token output  : %1  (%2 tok/s)")
              .arg(m_cur.tokens).arg(tokPs, 0, 'f', 1));
    appendLog(QString("     RAM post      : %1%  (\u0394 %2%)")
              .arg(m_cur.ramAfter, 0, 'f', 1)
              .arg(ramDelta, 0, 'f', 1));
    appendLog(QString("     \xe2\xad\x90 Score         : %1 / 100").arg(score));
    appendLog("     ─────────────────────────────────────────");

    flushSession();
    m_liveInfo->setText("In attesa della prossima richiesta...");
}

void MonitorPanel::onError(const QString& msg) {
    if (!m_cur.active) return;
    m_cur.durMs   = m_cur.timer.elapsed();
    m_cur.aborted = false;

    const QString ts = QDateTime::currentDateTime().toString("dd/MM HH:mm:ss");
    appendLog(QString("[%1] \xe2\x9d\x8c Errore: %2").arg(ts).arg(msg));
    appendLog("     ─────────────────────────────────────────");

    flushSession();
    m_liveInfo->setText("Errore nell'ultima richiesta.");
}

void MonitorPanel::onAborted() {
    if (!m_cur.active) return;
    m_cur.durMs   = m_cur.timer.elapsed();
    m_cur.aborted = true;

    const QString ts = QDateTime::currentDateTime().toString("dd/MM HH:mm:ss");
    appendLog(QString("[%1] \xe2\x9b\x94 Annullata dall'utente (%2 ms)").arg(ts).arg(m_cur.durMs));
    appendLog("     ─────────────────────────────────────────");

    flushSession();
    m_liveInfo->setText("Richiesta annullata.");
}

/* ══════════════════════════════════════════════════════════════
   Helpers
   ══════════════════════════════════════════════════════════════ */
void MonitorPanel::appendLog(const QString& line) {
    m_log->moveCursor(QTextCursor::End);
    m_log->insertPlainText(line + "\n");
    m_log->ensureCursorVisible();
}

void MonitorPanel::flushSession() {
    const double ramDelta = m_cur.ramAfter - m_cur.ramBefore;
    const double tokPs    = (m_cur.durMs > 0)
        ? m_cur.tokens * 1000.0 / m_cur.durMs : 0.0;
    const int    score    = m_cur.aborted ? -1
        : computeScore(m_cur.ttftMs, m_cur.durMs, m_cur.tokens, ramDelta);

    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto set = [&](int col, const QString& txt, Qt::Alignment align = Qt::AlignCenter){
        auto* item = new QTableWidgetItem(txt);
        item->setTextAlignment(align | Qt::AlignVCenter);
        m_table->setItem(row, col, item);
    };

    set(COL_TIME,       QDateTime::currentDateTime().toString("dd/MM HH:mm:ss"));
    set(COL_MODEL,      m_cur.model,   Qt::AlignLeft);
    set(COL_BACKEND,    m_cur.backend, Qt::AlignLeft);
    set(COL_TTFT,       m_cur.ttftMs  >= 0 ? QString::number(m_cur.ttftMs)  : "—");
    set(COL_DUR,        m_cur.durMs   >= 0 ? QString::number(m_cur.durMs)   : "—");
    set(COL_TOKENS,     QString::number(m_cur.tokens));
    set(COL_TOKPS,      tokPs > 0 ? QString::number(tokPs, 'f', 1) : "—");
    set(COL_RAM_BEFORE, QString("%1%").arg(m_cur.ramBefore, 0, 'f', 1));
    set(COL_RAM_DELTA,  QString("%1%").arg(ramDelta, 0, 'f', 1));

    if (m_cur.aborted) {
        set(COL_SCORE,  "—");
        set(COL_STATUS, "\xe2\x9b\x94 Annullata");
        if (auto* it = m_table->item(row, COL_STATUS))
            it->setForeground(QColor("#ffd600"));
    } else if (m_cur.durMs < 0) {
        set(COL_SCORE,  "—");
        set(COL_STATUS, "\xe2\x9d\x8c Errore");
        if (auto* it = m_table->item(row, COL_STATUS))
            it->setForeground(QColor("#ff1744"));
    } else {
        set(COL_SCORE, QString::number(score));
        /* Colore score */
        QString scoreColor, scoreLabel;
        if      (score >= 90) { scoreColor = "#69f0ae"; scoreLabel = "\xf0\x9f\x9f\xa2 Eccellente"; }
        else if (score >= 70) { scoreColor = "#ffd600"; scoreLabel = "\xf0\x9f\x9f\xa1 Buono"; }
        else if (score >= 50) { scoreColor = "#ff9800"; scoreLabel = "\xf0\x9f\x9f\xa0 Accettabile"; }
        else                  { scoreColor = "#ff1744"; scoreLabel = "\xf0\x9f\x94\xb4 Lento"; }

        if (auto* it = m_table->item(row, COL_SCORE))
            it->setForeground(QColor(scoreColor));
        set(COL_STATUS, scoreLabel);
        if (auto* it = m_table->item(row, COL_STATUS))
            it->setForeground(QColor(scoreColor));
    }

    m_table->scrollToBottom();

    /* Aggiorna chart */
    if (m_chart) {
        PerfChartWidget::Bar bar;
        bar.tokps   = tokPs;
        bar.score   = m_cur.aborted ? 0 : score;
        bar.aborted = m_cur.aborted;
        m_chart->addBar(bar);
    }

    m_cur.active = false;
}

/* ══════════════════════════════════════════════════════════════
   Calcolo score
   ══════════════════════════════════════════════════════════════
 * Formula:
 *   Base     = 100
 *   Penalità TTFT:   -5 se >1s, -15 se >3s, -25 se >6s
 *   Penalità durata: -5 se >10s, -10 se >30s, -20 se >60s
 *   Penalità RAM:    -5 per ogni +5% di RAM usata in più (max -20)
 *   Bonus velocità:  +10 se tok/s > 20, +20 se tok/s > 40
 *   Minimo 0, massimo 100
 ══════════════════════════════════════════════════════════════ */
int MonitorPanel::computeScore(qint64 ttftMs, qint64 durMs,
                                int tokens, double ramDelta) const {
    int s = 100;

    /* TTFT penalty */
    if      (ttftMs > 6000) s -= 25;
    else if (ttftMs > 3000) s -= 15;
    else if (ttftMs > 1000) s -= 5;

    /* Durata totale penalty */
    if      (durMs > 60000) s -= 20;
    else if (durMs > 30000) s -= 10;
    else if (durMs > 10000) s -= 5;

    /* RAM delta penalty (per ogni 5% in più: -5, max -20) */
    if (ramDelta > 0.0) {
        int ramPenalty = static_cast<int>(ramDelta / 5.0) * 5;
        s -= std::min(ramPenalty, 20);
    }

    /* Throughput bonus */
    if (durMs > 0 && tokens > 0) {
        double tokPs = tokens * 1000.0 / durMs;
        if      (tokPs > 40.0) s += 20;
        else if (tokPs > 20.0) s += 10;
    }

    return std::max(0, std::min(100, s));
}

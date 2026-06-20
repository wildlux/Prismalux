#include "widget_blhm.h"
#include "rab0l_canvas.h"
#include "../prismalux_paths.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QFrame>
#include <QFile>
#include <QTextStream>
#include <QTableWidget>
#include <QHeaderView>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
namespace P = PrismaluxPaths;

BlhmWidget::BlhmWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(10, 8, 10, 8);
    vlay->setSpacing(6);

    auto* hdr = new QLabel(
        "  \xf0\x9f\xa7\xa0  <b>BLHM \xe2\x80\x94 Brain-Loop-Human-MultiContext</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "R\xe2\x82\x98 = 0.50\xc2\xb7R\xe2\x82\x99 + 0.35\xc2\xb7R\xe2\x82\x97"
        " + 0.15\xc2\xb7R\xe2\x82\x9a"
        "</span>", this);
    hdr->setTextFormat(Qt::RichText);
    hdr->setObjectName("pageHeader");
    hdr->setFixedHeight(36);
    vlay->addWidget(hdr);

    auto* inner = new QTabWidget(this);
    inner->setDocumentMode(true);

    /* ── TAB 1: CALCOLATORE ── */
    {
        auto* calcW   = new QWidget;
        auto* calcLay = new QVBoxLayout(calcW);
        calcLay->setContentsMargins(0, 4, 0, 0);
        calcLay->setSpacing(6);
        auto* split = new QSplitter(Qt::Horizontal, calcW);

        auto* leftW   = new QWidget;
        auto* leftLay = new QVBoxLayout(leftW);
        leftLay->setContentsMargins(0, 0, 4, 0);
        auto* lblPesi = new QLabel(
            "Pesi ibridi  <span style='color:gray;font-size:11px;'>"
            "(doppio click per modificare)</span>");
        lblPesi->setTextFormat(Qt::RichText);

        m_table = new QTableWidget(0, 4, leftW);
        m_table->setHorizontalHeaderLabels({"Percorso ontologico", "factory_w", "link_w", "user_w"});
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
        m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
        m_table->setColumnWidth(1, 80);
        m_table->setColumnWidth(2, 80);
        m_table->setColumnWidth(3, 80);
        m_table->setAlternatingRowColors(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setMinimumWidth(340);

        struct BW { const char* path; double fw, lw, uw; };
        static const BW kDef[] = {
            {"Bio",               0.900, 0.462, 0.0},
            {"Bio,Ani",           0.700, 0.631, 0.0},
            {"Bio,Ani,Mam",       0.600, 0.710, 0.0},
            {"Bio,Ani,Mam,Cane",  1.000, 1.000, 0.0},
            {"Bio,Ani,Mam,Cane",  0.999, 0.999, 0.0},
            {"Bio,Ani,Mam,Gatto", 0.700, 0.704, 0.0},
            {"Bio,Ani,Mam,Lupo",  0.700, 0.784, 0.0},
        };
        m_table->setRowCount(int(std::size(kDef)));
        for (int r = 0; r < int(std::size(kDef)); ++r) {
            m_table->setItem(r, 0, new QTableWidgetItem(kDef[r].path));
            m_table->setItem(r, 1, new QTableWidgetItem(QString::number(kDef[r].fw, 'f', 3)));
            m_table->setItem(r, 2, new QTableWidgetItem(QString::number(kDef[r].lw, 'f', 3)));
            m_table->setItem(r, 3, new QTableWidgetItem(QString::number(kDef[r].uw, 'f', 3)));
        }

        auto* btnAdd = new QPushButton("+ Aggiungi peso");
        btnAdd->setObjectName("actionBtn");
        auto* btnDel = new QPushButton("\xf0\x9f\x97\x91  Rimuovi riga");
        btnDel->setObjectName("actionBtn");
        auto* rowBar = new QWidget;
        auto* rowLay = new QHBoxLayout(rowBar);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->addWidget(btnAdd);
        rowLay->addWidget(btnDel);
        rowLay->addStretch();
        leftLay->addWidget(lblPesi);
        leftLay->addWidget(m_table, 1);
        leftLay->addWidget(rowBar);

        auto* rightW   = new QWidget;
        auto* rightLay = new QVBoxLayout(rightW);
        rightLay->setContentsMargins(4, 0, 0, 0);
        auto* qBar  = new QWidget;
        auto* qHlay = new QHBoxLayout(qBar);
        qHlay->setContentsMargins(0, 0, 0, 0);
        qHlay->setSpacing(6);
        m_query = new QLineEdit;
        m_query->setPlaceholderText(tr("Query path  es. Bio,Ani,Mam,Cane"));
        m_query->setText("Bio,Ani,Mam,Cane");
        auto* btnCalc = new QPushButton("\xf0\x9f\xa7\xae  Calcola R");
        btnCalc->setObjectName("primaryBtn");
        qHlay->addWidget(new QLabel("Query:"));
        qHlay->addWidget(m_query, 1);
        qHlay->addWidget(btnCalc);
        m_output = new QTextEdit;
        m_output->setReadOnly(true);
        QFont mono("monospace"); mono.setStyleHint(QFont::Monospace); mono.setPointSize(9);
        m_output->setFont(mono);
        rightLay->addWidget(qBar);
        rightLay->addWidget(m_output, 1);

        split->addWidget(leftW);
        split->addWidget(rightW);
        split->setStretchFactor(0, 2);
        split->setStretchFactor(1, 3);
        calcLay->addWidget(split, 1);

        connect(btnCalc, &QPushButton::clicked,      this, &BlhmWidget::onComputeClicked);
        connect(m_query, &QLineEdit::returnPressed,  this, &BlhmWidget::onComputeClicked);
        connect(btnAdd,  &QPushButton::clicked,      this, &BlhmWidget::onAddRowClicked);
        connect(btnDel,  &QPushButton::clicked,      this, &BlhmWidget::onDeleteRowClicked);
        inner->addTab(calcW, "\xf0\x9f\xa7\xae  Calcolatore");
    }

    /* ── TAB 2: NOTE ── */
    {
        auto* noteW   = new QWidget;
        auto* noteLay = new QVBoxLayout(noteW);
        noteLay->setContentsMargins(0, 4, 0, 0);
        noteLay->setSpacing(6);
        auto* bar  = new QWidget;
        auto* blay = new QHBoxLayout(bar);
        blay->setContentsMargins(0, 0, 0, 0);
        blay->setSpacing(6);
        auto* lbl = new QLabel(
            "<b>\xf0\x9f\x93\x9d  Appunti BLHM / RAB\xe2\x82\x80-L</b>"
            "  <span style='color:gray;font-size:11px;'>salvato in RAG/BLHM_note.md</span>");
        lbl->setTextFormat(Qt::RichText);
        auto* btnLoad = new QPushButton("\xf0\x9f\x93\x82  Carica");
        btnLoad->setObjectName("actionBtn");
        auto* btnSave = new QPushButton("\xf0\x9f\x92\xbe  Salva");
        btnSave->setObjectName("primaryBtn");
        auto* btnClr  = new QPushButton("\xf0\x9f\x97\x91");
        btnClr->setFixedWidth(32);
        blay->addWidget(lbl); blay->addStretch();
        blay->addWidget(btnLoad); blay->addWidget(btnSave); blay->addWidget(btnClr);
        m_noteEdit = new QTextEdit;
        m_noteEdit->setPlaceholderText("Prendi appunti sui documenti BLHM e RAB\xe2\x82\x80-L...\n");
        QFile f(P::ragDir() + "/BLHM_note.md");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            m_noteEdit->setPlainText(QTextStream(&f).readAll());
        noteLay->addWidget(bar);
        noteLay->addWidget(m_noteEdit, 1);
        connect(btnSave, &QPushButton::clicked, this, &BlhmWidget::onNoteSave);
        connect(btnLoad, &QPushButton::clicked, this, &BlhmWidget::onNoteLoad);
        connect(btnClr,  &QPushButton::clicked, this, &BlhmWidget::onNotesClearClicked);
        inner->addTab(noteW, "\xf0\x9f\x93\x9d  Note");
    }

    /* ── TAB 3: DNA VIEWER ── */
    {
        auto* dnaW   = new QWidget;
        auto* dnaLay = new QVBoxLayout(dnaW);
        dnaLay->setContentsMargins(0, 4, 0, 0);
        dnaLay->setSpacing(6);
        auto* bar  = new QWidget;
        auto* hlay = new QHBoxLayout(bar);
        hlay->setContentsMargins(0, 0, 0, 0);
        hlay->setSpacing(6);
        m_dnaSeq1 = new QLineEdit;
        m_dnaSeq1->setPlaceholderText(tr("Sequenza DNA 1  (A/T/C/G)"));
        m_dnaSeq2 = new QLineEdit;
        m_dnaSeq2->setPlaceholderText(tr("Sequenza DNA 2  (opzionale)"));
        auto* btnAn = new QPushButton("\xf0\x9f\xa7\xac  Analizza");
        btnAn->setObjectName("primaryBtn");
        auto* btnCl = new QPushButton("\xf0\x9f\x97\x91");
        btnCl->setFixedWidth(32);
        m_dnaSimLbl = new QLabel;
        m_dnaSimLbl->setTextFormat(Qt::RichText);
        hlay->addWidget(new QLabel("Seq 1:")); hlay->addWidget(m_dnaSeq1, 3);
        hlay->addWidget(new QLabel("Seq 2:")); hlay->addWidget(m_dnaSeq2, 3);
        hlay->addWidget(btnAn); hlay->addWidget(btnCl);
        hlay->addWidget(m_dnaSimLbl, 2);
        m_dnaCanvas = new Rab0lCanvas(dnaW);
        dnaLay->addWidget(bar);
        dnaLay->addWidget(m_dnaCanvas, 1);
        connect(btnAn,    &QPushButton::clicked,    this, &BlhmWidget::onDnaAnalyzeClicked);
        connect(m_dnaSeq1,&QLineEdit::returnPressed,this, &BlhmWidget::onDnaAnalyzeClicked);
        connect(btnCl,    &QPushButton::clicked,    this, &BlhmWidget::onDnaClearClicked);
        inner->addTab(dnaW, "\xf0\x9f\xa7\xac  DNA");
    }

    /* ── TAB 4: ENGINE C ── */
    {
        auto* engW   = new QWidget;
        auto* engLay = new QVBoxLayout(engW);
        engLay->setContentsMargins(0, 6, 0, 4);
        engLay->setSpacing(8);
        auto* descLbl = new QLabel(
            "<b>\xe2\x9a\x99  Engine C</b>  "
            "<span style='color:gray;font-size:11px;'>Thread pool POSIX \xc2\xb7 3 cicli paralleli</span>");
        descLbl->setTextFormat(Qt::RichText);
        engLay->addWidget(descLbl);
        auto* topBar  = new QWidget;
        auto* topHLay = new QHBoxLayout(topBar);
        topHLay->setContentsMargins(0, 0, 0, 0);
        topHLay->setSpacing(8);
        auto* btnSync = new QPushButton("\xe2\x86\x93  Importa dal Calcolatore");
        btnSync->setObjectName("actionBtn");
        auto* btnRun = new QPushButton("\xe2\x96\xb6  Esegui Inferenza (3 thread)");
        btnRun->setObjectName("primaryBtn");
        m_engineLatency = new QLabel("\xe2\x80\x94");
        topHLay->addWidget(btnSync); topHLay->addWidget(btnRun);
        topHLay->addStretch();
        topHLay->addWidget(new QLabel("Latenza:")); topHLay->addWidget(m_engineLatency);
        engLay->addWidget(topBar);
        m_engineStatus = new QLabel("Grafo non inizializzato.");
        m_engineStatus->setStyleSheet("color:#888;font-size:11px;");
        engLay->addWidget(m_engineStatus);

        auto* panelsW    = new QWidget;
        auto* panelsHLay = new QHBoxLayout(panelsW);
        panelsHLay->setContentsMargins(0, 0, 0, 0);
        panelsHLay->setSpacing(8);
        QFont mono("monospace"); mono.setStyleHint(QFont::Monospace); mono.setPointSize(9);
        struct PanelSpec { const char* title; const char* color; QLabel** lbl; };
        PanelSpec panels[] = {
            { "Factory  R\xe2\x82\x99", "#4a9eff", &m_engineFactory },
            { "Link  R\xe2\x82\x97",    "#4ec94e", &m_engineLink    },
            { "User  R\xe2\x82\x9a",    "#ff9944", &m_engineUser    },
        };
        for (auto& ps : panels) {
            auto* frame = new QFrame;
            frame->setFrameShape(QFrame::StyledPanel);
            frame->setStyleSheet(QString("QFrame{border:2px solid %1;border-radius:6px;padding:4px;}").arg(ps.color));
            auto* flay = new QVBoxLayout(frame);
            flay->setContentsMargins(6, 4, 6, 4);
            flay->setSpacing(3);
            auto* titleL = new QLabel(QString("<b style='color:%1;'>%2</b>").arg(ps.color, ps.title));
            titleL->setTextFormat(Qt::RichText);
            *ps.lbl = new QLabel("score: \xe2\x80\x94\nn_active: \xe2\x80\x94\ntop: \xe2\x80\x94");
            (*ps.lbl)->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            (*ps.lbl)->setTextInteractionFlags(Qt::TextSelectableByMouse);
            (*ps.lbl)->setFont(mono);
            flay->addWidget(titleL); flay->addWidget(*ps.lbl);
            panelsHLay->addWidget(frame);
        }
        engLay->addWidget(panelsW);

        auto* combBar  = new QWidget;
        auto* combHLay = new QHBoxLayout(combBar);
        combHLay->setContentsMargins(0, 0, 0, 0);
        combHLay->setSpacing(8);
        combHLay->addWidget(new QLabel("<b>R_merged:</b>"));
        m_engineCombined = new QLabel("\xe2\x80\x94");
        m_engineCombined->setStyleSheet("font-size:20px;font-weight:bold;");
        combHLay->addWidget(m_engineCombined);
        combHLay->addStretch();
        engLay->addWidget(combBar);

        auto* sep1 = new QFrame; sep1->setFrameShape(QFrame::HLine); sep1->setFrameShadow(QFrame::Sunken);
        engLay->addWidget(sep1);

        auto* autoftHdr = new QLabel(
            "<b>Auto-finetuning Hebbiano</b>  "
            "<span style='color:gray;font-size:11px;'>link_w += lr \xc2\xb7 match</span>");
        autoftHdr->setTextFormat(Qt::RichText);
        engLay->addWidget(autoftHdr);
        auto* autoftBar  = new QWidget;
        auto* autoftHLay = new QHBoxLayout(autoftBar);
        autoftHLay->setContentsMargins(0, 0, 0, 0);
        autoftHLay->setSpacing(8);
        autoftHLay->addWidget(new QLabel("LR:"));
        m_engineLrSpin = new QDoubleSpinBox;
        m_engineLrSpin->setRange(0.001, 1.0);
        m_engineLrSpin->setSingleStep(0.01);
        m_engineLrSpin->setValue(0.05);
        m_engineLrSpin->setDecimals(3);
        m_engineLrSpin->setFixedWidth(80);
        auto* btnAutoft = new QPushButton("Applica Autoft");
        btnAutoft->setObjectName("actionBtn");
        autoftHLay->addWidget(m_engineLrSpin); autoftHLay->addWidget(btnAutoft); autoftHLay->addStretch();
        engLay->addWidget(autoftBar);
        m_engineAutoftOut = new QTextEdit;
        m_engineAutoftOut->setReadOnly(true);
        m_engineAutoftOut->setMaximumHeight(100);
        m_engineAutoftOut->setFont(mono);
        engLay->addWidget(m_engineAutoftOut);

        auto* sep2 = new QFrame; sep2->setFrameShape(QFrame::HLine); sep2->setFrameShadow(QFrame::Sunken);
        engLay->addWidget(sep2);

        auto* ioBar  = new QWidget;
        auto* ioHLay = new QHBoxLayout(ioBar);
        ioHLay->setContentsMargins(0, 0, 0, 0);
        ioHLay->setSpacing(8);
        auto* btnSave = new QPushButton("\xf0\x9f\x92\xbe  Salva .blhm");
        btnSave->setObjectName("actionBtn");
        auto* btnLoad = new QPushButton("\xf0\x9f\x93\x82  Carica .blhm");
        btnLoad->setObjectName("actionBtn");
        ioHLay->addWidget(btnSave); ioHLay->addWidget(btnLoad); ioHLay->addStretch();
        engLay->addWidget(ioBar);
        engLay->addStretch();

        connect(btnSync,    &QPushButton::clicked, this, &BlhmWidget::onEngineSyncClicked);
        connect(btnRun,     &QPushButton::clicked, this, &BlhmWidget::onEngineRunClicked);
        connect(btnAutoft,  &QPushButton::clicked, this, &BlhmWidget::onEngineAutoftClicked);
        connect(btnSave,    &QPushButton::clicked, this, &BlhmWidget::onEngineSaveClicked);
        connect(btnLoad,    &QPushButton::clicked, this, &BlhmWidget::onEngineLoadClicked);
        inner->addTab(engW, "\xe2\x9a\x99  Engine C");
    }

    vlay->addWidget(inner, 1);
}

BlhmWidget::~BlhmWidget()
{
    if (m_graph) { blhm_graph_free(m_graph); m_graph = nullptr; }
    blhm_pool_shutdown();
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Calcolatore
   ══════════════════════════════════════════════════════════════ */
void BlhmWidget::onComputeClicked()
{
    QStringList qPath;
    for (const auto& p : m_query->text().split(','))
        if (!p.trimmed().isEmpty()) qPath << p.trimmed();
    if (qPath.isEmpty()) return;

    double rFactory = 0.0, rLink = 0.0, rUser = 0.0;
    QString out;
    out += QString("Query: [%1]\n\n").arg(qPath.join(", "));
    out += QString("%-32s  match   fw\xc2\xb7m    lw\xc2\xb7m\xc2\xb2   uw\n").arg("Percorso");
    out += QString(65, '-') + "\n";

    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto* pathItem = m_table->item(r, 0);
        if (!pathItem) continue;
        QStringList wPath;
        for (const auto& p : pathItem->text().split(','))
            if (!p.trimmed().isEmpty()) wPath << p.trimmed();
        int common = 0, minLen = qMin(wPath.size(), qPath.size());
        for (int i = 0; i < minLen; ++i) {
            if (wPath[i] == qPath[i]) ++common; else break;
        }
        int denom = qMax(wPath.size(), qPath.size());
        double match = (denom > 0) ? double(common) / denom : 0.0;
        double fw = m_table->item(r,1) ? m_table->item(r,1)->text().toDouble() : 0.0;
        double lw = m_table->item(r,2) ? m_table->item(r,2)->text().toDouble() : 0.0;
        double uw = m_table->item(r,3) ? m_table->item(r,3)->text().toDouble() : 0.0;
        rFactory += fw * match;
        rLink    += lw * match * match;
        rUser    += uw;
        out += QString("%1  %2   %3   %4   %5\n")
            .arg(QString("[%1]").arg(pathItem->text()).leftJustified(32, ' '))
            .arg(match,5,'f',3).arg(fw*match,6,'f',3)
            .arg(lw*match*match,6,'f',3).arg(uw,6,'f',3);
    }
    double rMerged = 0.50*rFactory + 0.35*rLink + 0.15*rUser;
    out += "\n" + QString(65, '=') + "\n";
    out += QString("R_factory  = %1\n").arg(rFactory, 0, 'f', 3);
    out += QString("R_link     = %1\n").arg(rLink,    0, 'f', 3);
    out += QString("R_user     = %1\n").arg(rUser,    0, 'f', 3);
    out += QString(65, '-') + "\n";
    out += QString("R_merged   = 0.50 \xc3\x97 %1 + 0.35 \xc3\x97 %2 + 0.15 \xc3\x97 %3\n")
               .arg(rFactory,0,'f',3).arg(rLink,0,'f',3).arg(rUser,0,'f',3);
    out += QString("           = %1\n").arg(rMerged, 0, 'f', 3);
    m_output->setPlainText(out);
}

void BlhmWidget::onNoteSave()
{
    QFile f(P::ragDir() + "/BLHM_note.md");
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream(&f) << m_noteEdit->toPlainText();
}

void BlhmWidget::onNoteLoad()
{
    QFile f(P::ragDir() + "/BLHM_note.md");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    m_noteEdit->setPlainText(QTextStream(&f).readAll());
}

void BlhmWidget::onAddRowClicked()
{
    const int r = m_table->rowCount();
    m_table->setRowCount(r + 1);
    m_table->setItem(r, 0, new QTableWidgetItem("Bio"));
    m_table->setItem(r, 1, new QTableWidgetItem("0.500"));
    m_table->setItem(r, 2, new QTableWidgetItem("0.500"));
    m_table->setItem(r, 3, new QTableWidgetItem("0.000"));
    m_table->scrollToBottom();
    m_table->editItem(m_table->item(r, 0));
}

void BlhmWidget::onDeleteRowClicked()
{
    if (m_table->selectedItems().isEmpty()) return;
    m_table->removeRow(m_table->currentRow());
}

void BlhmWidget::onNotesClearClicked() { m_noteEdit->clear(); }

void BlhmWidget::onDnaClearClicked()
{
    m_dnaSeq1->clear(); m_dnaSeq2->clear();
    m_dnaSimLbl->clear(); m_dnaCanvas->clearAll();
}

void BlhmWidget::onDnaAnalyzeClicked()
{
    static const QRegularExpression kNonDna("[^ATCGatcg]");
    QString s1 = m_dnaSeq1->text().trimmed().toUpper();
    QString s2 = m_dnaSeq2->text().trimmed().toUpper();
    s1.remove(kNonDna); s2.remove(kNonDna);
    if (s1.isEmpty()) return;
    if (s2.isEmpty()) {
        m_dnaCanvas->setSeq1Only(s1);
        m_dnaSimLbl->setText(
            QString("<b>N=%1</b>  <span style='color:gray'>inserisci Seq 2 per confronto SIM</span>")
            .arg(s1.size()));
    } else {
        m_dnaCanvas->setSequences(s1, s2);
        const auto& r = m_dnaCanvas->result();
        const QString col = (r.sim >= 0.8) ? "#4CAF50" : (r.sim >= 0.4) ? "#FFC107" : "#F44336";
        m_dnaSimLbl->setText(
            QString("<b>SIM = <span style='color:%1'>%2</span></b>  |  N=%3  |  SNP=%4")
            .arg(col).arg(r.sim, 0, 'f', 3).arg(r.len).arg(r.snp));
    }
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Engine C
   ══════════════════════════════════════════════════════════════ */
void BlhmWidget::onEngineSyncClicked()
{
    if (!m_graph) m_graph = blhm_graph_create(256);
    blhm_graph_clear(m_graph);
    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto* pathItem = m_table->item(r, 0);
        if (!pathItem) continue;
        BLHMWeight w = {};
        w.factory_w = m_table->item(r,1) ? float(m_table->item(r,1)->text().toDouble()) : 0.0f;
        w.link_w    = m_table->item(r,2) ? float(m_table->item(r,2)->text().toDouble()) : 0.0f;
        w.user_w    = m_table->item(r,3) ? float(m_table->item(r,3)->text().toDouble()) : 0.0f;
        QStringList parts;
        for (const auto& p : pathItem->text().split(','))
            if (!p.trimmed().isEmpty()) parts << p.trimmed();
        w.path_len = qMin(int(parts.size()), BLHM_MAX_DEPTH);
        for (int i = 0; i < w.path_len; ++i) {
            QByteArray ba = parts[i].toUtf8();
            w.path[i] = blhm_label_register(m_graph, ba.constData());
        }
        blhm_graph_add(m_graph, &w);
    }
    if (m_engineStatus)
        m_engineStatus->setText(
            QString("Grafo inizializzato: %1 pesi importati.").arg(blhm_graph_count(m_graph)));
}

void BlhmWidget::onEngineRunClicked()
{
    if (!m_graph || blhm_graph_count(m_graph) == 0) {
        if (m_engineStatus) m_engineStatus->setText("Grafo vuoto \xe2\x80\x94 premi prima Importa.");
        return;
    }
    QStringList qParts;
    if (m_query)
        for (const auto& p : m_query->text().split(','))
            if (!p.trimmed().isEmpty()) qParts << p.trimmed();
    if (qParts.isEmpty()) return;

    QVector<int32_t> qPath;
    for (const auto& part : qParts) {
        QByteArray ba = part.toUtf8();
        int id = blhm_label_find(m_graph, ba.constData());
        if (id < 0) id = blhm_label_register(m_graph, ba.constData());
        qPath.append(int32_t(id));
    }
    BLHMResult res = blhm_infer(m_graph, qPath.data(), qPath.size(), nullptr, 0);
    auto fmtCycle = [](const BLHMCycleResult& c) -> QString {
        QString s = QString("score: %1\nn_active: %2").arg(double(c.score),0,'f',4).arg(c.n_active);
        if (c.n_top > 0) {
            s += "\ntop:";
            for (int i = 0; i < c.n_top; ++i)
                s += QString(" #%1(%2)").arg(c.top_idx[i]).arg(double(c.top_score[i]),0,'f',3);
        }
        return s;
    };
    if (m_engineFactory)  m_engineFactory->setText(fmtCycle(res.factory));
    if (m_engineLink)     m_engineLink->setText(fmtCycle(res.link));
    if (m_engineUser)     m_engineUser->setText(fmtCycle(res.user));
    if (m_engineCombined) m_engineCombined->setText(QString::number(double(res.combined),'f',4));
    if (m_engineLatency)  m_engineLatency->setText(QString("%1 ms").arg(res.latency_ms,0,'f',2));
}

void BlhmWidget::onEngineAutoftClicked()
{
    if (!m_graph || blhm_graph_count(m_graph) == 0) {
        if (m_engineAutoftOut) m_engineAutoftOut->setPlainText("Grafo vuoto.");
        return;
    }
    const int n = blhm_graph_count(m_graph);
    QVector<float> before(n);
    for (int i = 0; i < n; ++i) before[i] = blhm_graph_get(m_graph, i)->link_w;
    QStringList qParts;
    if (m_query)
        for (const auto& p : m_query->text().split(','))
            if (!p.trimmed().isEmpty()) qParts << p.trimmed();
    QVector<int32_t> qPath;
    for (const auto& part : qParts) {
        QByteArray ba = part.toUtf8();
        int id = blhm_label_find(m_graph, ba.constData());
        if (id < 0) id = blhm_label_register(m_graph, ba.constData());
        qPath.append(int32_t(id));
    }
    float lr = m_engineLrSpin ? float(m_engineLrSpin->value()) : 0.05f;
    blhm_autoft(m_graph, qPath.data(), qPath.size(), lr);
    QString out = QString("Autoft  LR=%1  query=[%2]\n\n")
                      .arg(double(lr),0,'f',3).arg(m_query ? m_query->text() : QString());
    out += QString("%-32s  before      after       delta\n").arg("Peso");
    out += QString(66, '-') + "\n";
    for (int i = 0; i < n; ++i) {
        const BLHMWeight* w = blhm_graph_get(m_graph, i);
        float after = w->link_w, delta = after - before[i];
        QStringList parts;
        for (int j = 0; j < w->path_len; ++j) {
            const char* nm = blhm_label_name(m_graph, w->path[j]);
            parts << QString(nm ? nm : "?");
        }
        out += QString("%1  %2   %3   %4\n")
                   .arg(QString("[%1]").arg(parts.join(",")).leftJustified(32,' '))
                   .arg(double(before[i]),9,'f',4)
                   .arg(double(after),9,'f',4).arg(double(delta),9,'f',4);
    }
    if (m_engineAutoftOut) m_engineAutoftOut->setPlainText(out);
}

void BlhmWidget::onEngineSaveClicked()
{
    if (!m_graph || blhm_graph_count(m_graph) == 0) {
        QMessageBox::warning(this, "BLHM Engine", "Grafo vuoto \xe2\x80\x94 importa prima i pesi.");
        return;
    }
    QString path = QFileDialog::getSaveFileName(
        this, "Salva grafo BLHM",
        P::root() + "/KNOWLEDGE_USER/blhm_graph.blhm",
        "BLHM Graph (*.blhm);;Tutti i file (*)");
    if (path.isEmpty()) return;
    if (!blhm_save(m_graph, path.toUtf8().constData()))
        QMessageBox::critical(this, "BLHM Engine", "Errore durante il salvataggio.");
    else if (m_engineStatus)
        m_engineStatus->setText(QString("Grafo salvato \xe2\x86\x92 %1").arg(path));
}

void BlhmWidget::onEngineLoadClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Carica grafo BLHM",
        P::root() + "/KNOWLEDGE_USER",
        "BLHM Graph (*.blhm);;Tutti i file (*)");
    if (path.isEmpty()) return;
    BLHMGraph* g = blhm_load(path.toUtf8().constData());
    if (!g) { QMessageBox::critical(this, "BLHM Engine", "Impossibile leggere il file .blhm."); return; }
    if (m_graph) blhm_graph_free(m_graph);
    m_graph = g;
    if (m_engineStatus)
        m_engineStatus->setText(
            QString("Grafo caricato: %1 pesi da \"%2\".")
                .arg(blhm_graph_count(m_graph))
                .arg(QFileInfo(path).fileName()));
}

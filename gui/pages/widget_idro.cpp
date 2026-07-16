#include "widget_idro.h"
#include "dpi_utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPolygonF>
#include <cmath>

/* ── Scena isometrica sistema DWC idroponico ─────────────────────────────── */
namespace {

class IsoIdroView : public QWidget {
public:
    explicit IsoIdroView(QWidget* parent) : QWidget(parent) {
        setMinimumHeight(dpiScale(220));
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        /* sfondo grow room */
        QLinearGradient bg(0, 0, 0, height());
        bg.setColorAt(0, QColor(12, 18, 28));
        bg.setColorAt(1, QColor(25, 38, 58));
        p.fillRect(rect(), bg);

        const float tw = 38.0f;
        const float th = tw * 0.5f;
        QPointF o(width() * 0.43f, height() * 0.56f);

        auto I = [&](float x, float y, float z) -> QPointF {
            return { o.x() + (x - y) * th,
                     o.y() + (x + y) * (th * 0.5f) - z * th };
        };

        auto face = [&](QPointF a, QPointF b, QPointF c, QPointF d, QColor col) {
            QPolygonF pg; pg << a << b << c << d;
            p.setBrush(col);   p.setPen(Qt::NoPen);       p.drawPolygon(pg);
            p.setPen(QPen(QColor(0,0,0,55), 0.7f));
            p.setBrush(Qt::NoBrush);                       p.drawPolygon(pg);
        };

        auto box = [&](float x, float y, float z,
                       float w, float d, float h, QColor base) {
            face(I(x,y,z+h), I(x+w,y,z+h), I(x+w,y+d,z+h), I(x,y+d,z+h), base.lighter(135));
            face(I(x,y,z),   I(x,y+d,z),   I(x,y+d,z+h),   I(x,y,z+h),   base.darker(160));
            face(I(x+w,y,z), I(x+w,y,z+h), I(x+w,y+d,z+h), I(x+w,y+d,z), base.darker(130));
        };

        /* ── Banco/supporto ── */
        box(0,0,0, 8,6,0.15f, QColor(75,85,75));

        /* ── Vasca DWC (pareti scure) ── */
        box(0.3f,0.3f,0.15f, 7.4f,5.4f,2.1f, QColor(28,38,32));

        /* ── Soluzione nutritiva (verde-acqua) ── */
        {
            QPolygonF sol;
            sol << I(0.5f,0.5f,1.55f) << I(7.5f,0.5f,1.55f)
                << I(7.5f,5.5f,1.55f) << I(0.5f,5.5f,1.55f);
            p.setBrush(QColor(18,105,80,210)); p.setPen(QPen(QColor(40,190,145,150), 1));
            p.drawPolygon(sol);
        }

        /* ── Coperchio ── */
        box(0.3f,0.3f,2.25f, 7.4f,5.4f,0.15f, QColor(48,55,48));

        /* ── Net cups + piante ── */
        for (float px : {1.2f, 2.9f, 4.6f, 6.3f}) {
            for (float py : {1.2f, 3.0f, 4.8f}) {
                QPointF cup = I(px, py, 2.40f);
                /* net cup */
                p.setPen(QPen(QColor(20,30,25), 1));
                p.setBrush(QColor(18,80,58,170));
                p.drawEllipse(cup, 10, 5);

                /* stelo */
                p.setPen(QPen(QColor(45,140,45), 1.5f));
                QPointF apex(cup.x(), cup.y() - (int)(tw * 0.65f));
                p.drawLine(cup, apex);

                /* chioma */
                QRadialGradient pg(apex, (int)(tw * 0.42f));
                pg.setColorAt(0.0f, QColor(85,210,60,230));
                pg.setColorAt(0.65f, QColor(40,145,40,210));
                pg.setColorAt(1.0f, QColor(20,80,20,0));
                p.setPen(Qt::NoPen); p.setBrush(pg);
                p.drawEllipse(apex, (int)(tw*0.40f), (int)(tw*0.30f));
            }
        }

        /* ── Barra LED sopra le piante ── */
        box(-0.3f,2.3f,5.2f, 8.6f,1.4f,0.28f, QColor(245,235,185));
        /* alone luminoso */
        {
            QPointF lc = I(4.0f, 3.0f, 5.25f);
            QRadialGradient glow(lc, (int)(tw * 4.5f));
            glow.setColorAt(0,   QColor(255,245,180,75));
            glow.setColorAt(1,   QColor(0,0,0,0));
            p.setBrush(glow); p.setPen(Qt::NoPen);
            p.drawEllipse(lc, (int)(tw*4.5f), (int)(tw*2.8f));
        }

        /* ── Pompa aeratore (lato destro) ── */
        box(8.5f,1.5f,0.15f, 0.9f,1.0f,0.75f, QColor(70,95,110));
        /* tubo aeratore */
        p.setPen(QPen(QColor(100,185,210,200), 1.5f));
        p.drawLine(I(8.5f,2.0f,0.55f), I(7.7f,2.0f,0.55f));
        p.drawLine(I(7.7f,2.0f,0.55f), I(7.7f,2.0f,1.50f));

        /* ── Serbatoio nutrienti ── */
        box(8.5f,3.0f,0.15f, 1.3f,2.2f,2.3f, QColor(55,75,100));
        /* livello soluzione */
        {
            QPolygonF lvl;
            lvl << I(8.6f,3.1f,1.60f) << I(9.7f,3.1f,1.60f)
                << I(9.7f,5.1f,1.60f) << I(8.6f,5.1f,1.60f);
            p.setBrush(QColor(18,120,88,190)); p.setPen(Qt::NoPen);
            p.drawPolygon(lvl);
        }

        /* ── Etichette ── */
        p.setFont(QFont("sans-serif", 8, QFont::Bold));
        auto lbl = [&](float x, float y, float z, const char* txt, int dx = 0, int dy = 0) {
            QPointF pt = I(x, y, z);
            int tx = (int)pt.x() + dx, ty = (int)pt.y() + dy;
            p.setPen(QColor(0,0,0,160));
            p.drawText(tx-52, ty-7, 104, 14, Qt::AlignCenter, txt);
            p.setPen(QColor(210,240,215));
            p.drawText(tx-53, ty-8, 104, 14, Qt::AlignCenter, txt);
        };
        lbl(4.0f,0.3f,3.2f, "Vasca DWC",            0, -8);
        lbl(4.0f,0.3f,5.7f, "LED grow light",        0, -8);
        lbl(8.9f,2.0f,1.0f, "Pompa aria",            14, 0);
        lbl(9.1f,4.0f,2.6f, "Serbatoio",             14, 0);
        lbl(4.0f,5.6f,2.0f, "Soluzione nutritiva",   0, 10);
    }
};

} /* namespace */

/* ── Dati colture ────────────────────────────────────────────────────────── */
struct FaseData {
    float phMin, phMax, ecMin, ecMax;
    float tAcMin, tAcMax, tArMin, tArMax;
    int   luceH;
};

static const struct { const char* nome; FaseData f[4]; } kColture[] = {
    /* 0 - Semina  1 - Vegetativo  2 - Pre-fioritura  3 - Fioritura/Produzione */
    { "Lattuga / Insalatine", {
        {5.5f,6.5f, 0.5f,1.0f, 18,22, 18,24, 16},
        {5.5f,6.5f, 1.0f,1.6f, 18,22, 18,24, 16},
        {5.5f,6.5f, 1.2f,1.8f, 18,22, 18,24, 14},
        {5.5f,6.5f, 1.2f,1.8f, 18,22, 18,24, 14} } },
    { "Basilico / Erbe aromatiche", {
        {5.5f,6.5f, 0.7f,1.0f, 20,24, 20,28, 16},
        {5.5f,6.5f, 1.0f,1.6f, 20,24, 20,28, 16},
        {5.5f,6.5f, 1.0f,1.6f, 20,24, 20,28, 14},
        {5.5f,6.5f, 1.0f,1.6f, 20,24, 20,28, 14} } },
    { "Pomodoro / Peperoni", {
        {5.5f,6.5f, 1.0f,2.0f, 20,26, 20,26, 16},
        {5.5f,6.5f, 2.0f,3.5f, 20,26, 22,28, 18},
        {5.5f,6.5f, 2.5f,4.0f, 20,26, 22,28, 14},
        {5.5f,6.5f, 3.0f,5.0f, 20,26, 22,28, 12} } },
    { "Fragole", {
        {5.5f,6.5f, 0.8f,1.2f, 15,20, 18,24, 14},
        {5.5f,6.5f, 1.0f,1.4f, 15,20, 18,24, 16},
        {5.5f,6.5f, 1.2f,1.8f, 15,20, 18,24, 12},
        {5.5f,6.5f, 1.4f,2.0f, 15,20, 18,24, 10} } },
    { "Cetrioli / Zucchine", {
        {5.5f,6.5f, 1.2f,1.7f, 20,25, 22,28, 16},
        {5.5f,6.5f, 1.7f,2.5f, 20,25, 22,28, 16},
        {5.5f,6.5f, 2.0f,3.0f, 20,25, 22,28, 14},
        {5.5f,6.5f, 2.5f,3.5f, 20,25, 22,28, 12} } },
    { "Spinaci / Cavolo", {
        {6.0f,7.0f, 1.0f,1.5f, 15,20, 15,22, 14},
        {6.0f,7.0f, 1.5f,2.0f, 15,20, 15,22, 14},
        {6.0f,7.0f, 1.5f,2.0f, 15,20, 15,22, 12},
        {6.0f,7.0f, 1.5f,2.0f, 15,20, 15,22, 12} } },
};
static constexpr int kNumColture = 6;

/* ════════════════════════════════════════════════════════════════════════════
   Costruttore
   ════════════════════════════════════════════════════════════════════════════ */
IdroWidget::IdroWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outerLay->addWidget(scroll);

    auto* inner = new QWidget;
    scroll->setWidget(inner);
    auto* rootLay = new QVBoxLayout(inner);
    rootLay->setContentsMargins(10, 10, 10, 10);
    rootLay->setSpacing(8);

    /* Titolo */
    auto* titleLbl = new QLabel(
        "<b>\xf0\x9f\x8c\xbf  Idroponica</b>"   /* 🌿 */
        "<br><small style='color:#9e9e9e;'>NFT \xc2\xb7 DWC \xc2\xb7 Ebb&amp;Flow \xc2\xb7 "
        "Aeroponico \xc2\xb7 Kratky \xe2\x80\x94 parametri coltura, dosi nutrienti, "
        "diario misurazioni</small>", inner);
    titleLbl->setTextFormat(Qt::RichText);
    rootLay->addWidget(titleLbl);

    /* ── Layout a 2 colonne ── */
    auto* colsLay = new QHBoxLayout;
    colsLay->setSpacing(10);
    rootLay->addLayout(colsLay);

    /* ══════════ COLONNA SINISTRA — impostazioni ══════════════════════ */
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(8);

    /* ─── GroupBox 1: Sistema idroponico ─── */
    auto* sysBox = new QGroupBox(tr("Sistema idroponico"), inner);
    auto* sysFrm = new QFormLayout(sysBox);
    sysFrm->setSpacing(6);

    m_sistemaCombo = new QComboBox(sysBox);
    m_sistemaCombo->addItem("NFT  (Nutrient Film Technique)", 0);
    m_sistemaCombo->addItem("DWC  (Deep Water Culture)",      1);
    m_sistemaCombo->addItem("Ebb & Flow  (flusso e riflusso)", 2);
    m_sistemaCombo->addItem("Aeroponico",                      3);
    m_sistemaCombo->addItem("Kratky  (passivo, no pompa)",    4);
    m_sistemaCombo->addItem("Acquaponica",                     5);
    sysFrm->addRow(tr("Tipo sistema:"), m_sistemaCombo);

    m_volumeSpin = new QSpinBox(sysBox);
    m_volumeSpin->setRange(1, 10000);
    m_volumeSpin->setValue(50);
    m_volumeSpin->setSuffix(" L");
    m_volumeSpin->setToolTip(tr("Volume totale della vasca / serbatoio in litri."));
    sysFrm->addRow(tr("Volume vasca:"), m_volumeSpin);

    m_nPianteSpin = new QSpinBox(sysBox);
    m_nPianteSpin->setRange(1, 1000);
    m_nPianteSpin->setValue(8);
    m_nPianteSpin->setSuffix(tr(" piante"));
    sysFrm->addRow(tr("N. piante:"), m_nPianteSpin);

    /* riga ciclo pompa (nascosta per sistemi senza cicli) */
    m_pompaRow = new QWidget(sysBox);
    auto* pompaLay = new QHBoxLayout(m_pompaRow);
    pompaLay->setContentsMargins(0, 0, 0, 0);
    pompaLay->setSpacing(6);
    m_pompaOnSpin = new QSpinBox(m_pompaRow);
    m_pompaOnSpin->setRange(1, 120);
    m_pompaOnSpin->setValue(15);
    m_pompaOnSpin->setSuffix(" min");
    m_pompaOffSpin = new QSpinBox(m_pompaRow);
    m_pompaOffSpin->setRange(1, 240);
    m_pompaOffSpin->setValue(45);
    m_pompaOffSpin->setSuffix(" min");
    pompaLay->addWidget(new QLabel(tr("ON:"), m_pompaRow));
    pompaLay->addWidget(m_pompaOnSpin);
    pompaLay->addWidget(new QLabel(tr("OFF:"), m_pompaRow));
    pompaLay->addWidget(m_pompaOffSpin);
    pompaLay->addStretch();
    sysFrm->addRow(tr("Ciclo pompa:"), m_pompaRow);

    auto* sysNote = new QLabel(
        "<small><i>\xf0\x9f\x92\xa7  DWC/Kratky: inserisci solo aeratore continuo. "  /* 💧 */
        "NFT: pompa continua o ciclica. "
        "Ebb&amp;Flow: tipico 15 ON / 60 OFF.</i></small>", sysBox);
    sysNote->setTextFormat(Qt::RichText);
    sysNote->setWordWrap(true);
    sysFrm->addRow(sysNote);
    leftCol->addWidget(sysBox);

    /* ─── GroupBox 2: Coltura e fase ─── */
    auto* coltBox = new QGroupBox(tr("Coltura e fase"), inner);
    auto* coltLay = new QVBoxLayout(coltBox);
    coltLay->setSpacing(6);

    auto* coltFrm = new QFormLayout;
    coltFrm->setSpacing(6);
    m_colturaCombo = new QComboBox(coltBox);
    for (int i = 0; i < kNumColture; ++i)
        m_colturaCombo->addItem(kColture[i].nome);
    coltFrm->addRow(tr("Coltura:"), m_colturaCombo);

    m_faseCombo = new QComboBox(coltBox);
    m_faseCombo->addItem(tr("Semina / Propagazione"), 0);
    m_faseCombo->addItem(tr("Vegetativo"),             1);
    m_faseCombo->addItem(tr("Pre-fioritura"),          2);
    m_faseCombo->addItem(tr("Fioritura / Produzione"), 3);
    coltFrm->addRow(tr("Fase:"), m_faseCombo);
    coltLay->addLayout(coltFrm);

    /* riquadro parametri consigliati */
    auto* paramBox = new QGroupBox(tr("Parametri consigliati"), coltBox);
    paramBox->setStyleSheet("QGroupBox { font-size:11px; }");
    auto* paramFrm = new QFormLayout(paramBox);
    paramFrm->setSpacing(4);

    m_phRangeLbl = new QLabel("\xe2\x80\x94", paramBox);
    m_phRangeLbl->setTextFormat(Qt::RichText);
    paramFrm->addRow("pH:", m_phRangeLbl);

    m_ecRangeLbl = new QLabel("\xe2\x80\x94", paramBox);
    m_ecRangeLbl->setTextFormat(Qt::RichText);
    paramFrm->addRow("EC:", m_ecRangeLbl);

    m_tAcquaLbl = new QLabel("\xe2\x80\x94", paramBox);
    m_tAcquaLbl->setTextFormat(Qt::RichText);
    paramFrm->addRow(tr("T acqua:"), m_tAcquaLbl);

    m_tAriaLbl = new QLabel("\xe2\x80\x94", paramBox);
    m_tAriaLbl->setTextFormat(Qt::RichText);
    paramFrm->addRow(tr("T aria:"), m_tAriaLbl);

    m_luceLbl = new QLabel("\xe2\x80\x94", paramBox);
    m_luceLbl->setTextFormat(Qt::RichText);
    paramFrm->addRow(tr("Luce:"), m_luceLbl);
    coltLay->addWidget(paramBox);
    leftCol->addWidget(coltBox);

    /* ─── GroupBox 3: Calcolatore dosi nutrienti ─── */
    auto* dosiBox = new QGroupBox(tr("Calcolatore dosi nutrienti"), inner);
    auto* dosiFrm = new QFormLayout(dosiBox);
    dosiFrm->setSpacing(6);

    m_volSolSpin = new QDoubleSpinBox(dosiBox);
    m_volSolSpin->setRange(0.5, 5000.0);
    m_volSolSpin->setValue(20.0);
    m_volSolSpin->setSuffix(" L");
    m_volSolSpin->setToolTip(tr("Volume di soluzione nutritiva da preparare."));
    dosiFrm->addRow(tr("Volume da preparare:"), m_volSolSpin);

    auto makeNutRow = [&](const QString& label, QDoubleSpinBox*& dspin,
                          QLabel*& resLbl, QWidget* parent) {
        auto* row = new QWidget(parent);
        auto* hl  = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(6);
        dspin = new QDoubleSpinBox(row);
        dspin->setRange(0.0, 50.0);
        dspin->setSingleStep(0.5);
        dspin->setValue(5.0);
        dspin->setSuffix(" ml/L");
        resLbl = new QLabel("\xe2\x80\x94", row);
        resLbl->setTextFormat(Qt::RichText);
        resLbl->setMinimumWidth(dpiScale(70));
        hl->addWidget(dspin);
        hl->addWidget(new QLabel("\xe2\x86\x92", row));  /* → */
        hl->addWidget(resLbl);
        hl->addStretch();
        dosiFrm->addRow(label, row);
    };
    makeNutRow(tr("Soluzione A:"),    m_nutADoseSpin, m_nutAResLbl, dosiBox);
    makeNutRow(tr("Soluzione B:"),    m_nutBDoseSpin, m_nutBResLbl, dosiBox);
    makeNutRow(tr("Integratore C:"),  m_nutCDoseSpin, m_nutCResLbl, dosiBox);

    auto* calcDosiBtn = new QPushButton(
        tr("\xe2\x9a\x97  CALCOLA DOSI"), dosiBox);  /* ⚗ */
    calcDosiBtn->setObjectName("actionBtn");
    calcDosiBtn->setMinimumHeight(dpiScale(36));
    dosiFrm->addRow(calcDosiBtn);

    auto* dosiNote = new QLabel(
        tr("<small><i>\xe2\x84\xb9  Inserisci la dose per litro indicata dal "  /* ℹ */
        "produttore del fertilizzante.<br>"
        "Esempio Flora Series: A=5 ml/L, B=5 ml/L a regime vegetativo.</i></small>"),
        dosiBox);
    dosiNote->setTextFormat(Qt::RichText);
    dosiNote->setWordWrap(true);
    dosiFrm->addRow(dosiNote);
    leftCol->addWidget(dosiBox);

    leftCol->addStretch();
    colsLay->addLayout(leftCol, 2);

    /* ══════════ COLONNA DESTRA — log e note ══════════════════════════ */
    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(8);

    /* ── Schema 3D sistema ── */
    auto* schemaBox = new QGroupBox(tr("Schema impianto DWC"), inner);
    auto* schemaLay3d = new QVBoxLayout(schemaBox);
    schemaLay3d->setContentsMargins(4, 4, 4, 4);
    schemaLay3d->addWidget(new IsoIdroView(schemaBox));
    rightCol->addWidget(schemaBox);

    /* ─── GroupBox 4: Log misurazioni ─── */
    auto* logBox = new QGroupBox(tr("Diario misurazioni"), inner);
    auto* logLay = new QVBoxLayout(logBox);
    logLay->setSpacing(6);

    auto* logBtnRow = new QHBoxLayout;
    auto* addBtn = new QPushButton(tr("\xe2\x9e\x95  Aggiungi"), logBox);  /* ➕ */
    auto* remBtn = new QPushButton(tr("\xe2\x9e\x96  Rimuovi"),  logBox);  /* ➖ */
    auto* expBtn = new QPushButton(tr("\xf0\x9f\x93\x8b  Esporta CSV"), logBox);  /* 📋 */
    addBtn->setObjectName("actionBtn");
    expBtn->setObjectName("actionBtn");
    logBtnRow->addWidget(addBtn);
    logBtnRow->addWidget(remBtn);
    logBtnRow->addStretch();
    logBtnRow->addWidget(expBtn);
    logLay->addLayout(logBtnRow);

    m_logTable = new QTableWidget(0, 6, logBox);
    m_logTable->setHorizontalHeaderLabels({
        tr("Data / Ora"), tr("pH"), tr("EC mS/cm"),
        tr("T Acqua\xc2\xb0""C"), tr("T Aria\xc2\xb0""C"), tr("Note")});
    m_logTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logTable->verticalHeader()->setVisible(false);
    m_logTable->setAlternatingRowColors(true);
    m_logTable->setMinimumHeight(dpiScale(200));
    logLay->addWidget(m_logTable, 1);

    m_lastMeasLbl = new QLabel(tr("Nessuna misurazione registrata."), logBox);
    m_lastMeasLbl->setStyleSheet("color:#9e9e9e; font-size:11px;");
    logLay->addWidget(m_lastMeasLbl);

    auto* logNote = new QLabel(
        tr("<small><i>\xe2\x84\xb9  Clicca '+ Aggiungi' per inserire una nuova riga, "
        "poi modifica i valori direttamente nella tabella. "
        "Il log viene salvato automaticamente.</i></small>"), logBox);
    logNote->setTextFormat(Qt::RichText);
    logNote->setWordWrap(true);
    logLay->addWidget(logNote);
    rightCol->addWidget(logBox, 1);

    /* ─── GroupBox 5: Note impianto ─── */
    auto* noteBox = new QGroupBox(tr("Note impianto"), inner);
    auto* noteLay = new QVBoxLayout(noteBox);
    m_noteEdit = new QTextEdit(noteBox);
    m_noteEdit->setObjectName("outputView");
    m_noteEdit->setPlaceholderText(
        tr("Annotazioni libere: prodotti usati, problemi riscontrati, "
           "date trapianti, substrato, calendario concimazione..."));
    m_noteEdit->setMinimumHeight(dpiScale(100));
    m_noteEdit->setMaximumHeight(dpiScale(160));
    noteLay->addWidget(m_noteEdit);

    auto* noteSaveLbl = new QLabel(
        "<small style='color:#9e9e9e;'><i>"
        "\xf0\x9f\x92\xbe  Le note vengono salvate automaticamente.</i></small>",
        noteBox);
    noteSaveLbl->setTextFormat(Qt::RichText);
    noteLay->addWidget(noteSaveLbl);
    rightCol->addWidget(noteBox);

    rightCol->addStretch();
    colsLay->addLayout(rightCol, 3);

    /* ── Carica dati persistenti ── */
    QSettings s("Prismalux", "Idroponica");
    m_noteEdit->setPlainText(s.value("note").toString());
    loadLog();
    updateParametriConsigliati();
    onSistemaChanged(0);

    /* ── Connessioni ── */
    connect(m_sistemaCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &IdroWidget::onSistemaChanged);
    connect(m_colturaCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &IdroWidget::onColturaFaseChanged);
    connect(m_faseCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &IdroWidget::onColturaFaseChanged);
    connect(calcDosiBtn, &QPushButton::clicked, this, &IdroWidget::onCalcolaDosiClicked);
    connect(addBtn, &QPushButton::clicked, this, &IdroWidget::onAddMeasurementClicked);
    connect(remBtn, &QPushButton::clicked, this, &IdroWidget::onRemoveMeasurementClicked);
    connect(expBtn, &QPushButton::clicked, this, &IdroWidget::onExportCsvClicked);
    connect(m_logTable, &QTableWidget::cellChanged,
            this, &IdroWidget::onLogCellChanged);
    connect(m_noteEdit, &QTextEdit::textChanged, this, &IdroWidget::onNoteChanged);
}

/* ── Parametri consigliati ────────────────────────────────────────────────── */
void IdroWidget::updateParametriConsigliati()
{
    int ci = m_colturaCombo->currentIndex();
    int fi = m_faseCombo->currentIndex();
    if (ci < 0 || ci >= kNumColture || fi < 0 || fi > 3) return;
    const FaseData& d = kColture[ci].f[fi];
    m_phRangeLbl->setText(
        QString("<b>%1 \xe2\x80\x93 %2</b>")
            .arg(d.phMin, 0, 'f', 1).arg(d.phMax, 0, 'f', 1));
    m_ecRangeLbl->setText(
        QString("<b>%1 \xe2\x80\x93 %2 mS/cm</b>")
            .arg(d.ecMin, 0, 'f', 1).arg(d.ecMax, 0, 'f', 1));
    m_tAcquaLbl->setText(
        QString("<b>%1 \xe2\x80\x93 %2 \xc2\xb0""C</b>")
            .arg((int)d.tAcMin).arg((int)d.tAcMax));
    m_tAriaLbl->setText(
        QString("<b>%1 \xe2\x80\x93 %2 \xc2\xb0""C</b>")
            .arg((int)d.tArMin).arg((int)d.tArMax));
    m_luceLbl->setText(
        QString("<b>%1 ore / giorno</b>").arg(d.luceH));
}

/* ── Slot ─────────────────────────────────────────────────────────────────── */
void IdroWidget::onSistemaChanged(int idx)
{
    /* pompa con cicli ON/OFF: NFT=0, Ebb&Flow=2, Aeroponico=3 */
    m_pompaRow->setVisible(idx == 0 || idx == 2 || idx == 3);
}

void IdroWidget::onColturaFaseChanged()
{
    updateParametriConsigliati();
}

void IdroWidget::onCalcolaDosiClicked()
{
    double vol = m_volSolSpin->value();
    auto fmt = [&](QDoubleSpinBox* s, QLabel* l) {
        double ml = vol * s->value();
        l->setText(QString("<b>%1 ml</b>").arg(ml, 0, 'f', 1));
    };
    fmt(m_nutADoseSpin, m_nutAResLbl);
    fmt(m_nutBDoseSpin, m_nutBResLbl);
    fmt(m_nutCDoseSpin, m_nutCResLbl);
}

void IdroWidget::onAddMeasurementClicked()
{
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    int row = m_logTable->rowCount();
    m_logTable->blockSignals(true);
    m_logTable->insertRow(row);
    m_logTable->setItem(row, 0, new QTableWidgetItem(ts));
    for (int c = 1; c < 6; ++c)
        m_logTable->setItem(row, c, new QTableWidgetItem(""));
    m_logTable->blockSignals(false);
    m_logTable->scrollToBottom();
    m_logTable->setCurrentCell(row, 1);
    m_logTable->editItem(m_logTable->item(row, 1));
    saveLog();
    updateLastMeas();
}

void IdroWidget::onRemoveMeasurementClicked()
{
    int row = m_logTable->currentRow();
    if (row < 0) return;
    m_logTable->removeRow(row);
    saveLog();
    updateLastMeas();
}

void IdroWidget::onExportCsvClicked()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Esporta diario idroponica"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
            + "/idro_log.csv",
        "CSV (*.csv)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Errore"),
            tr("Impossibile scrivere il file:\n%1").arg(path));
        return;
    }
    QTextStream out(&f);
    out << "Data/Ora,pH,EC (mS/cm),T Acqua (C),T Aria (C),Note\n";
    for (int r = 0; r < m_logTable->rowCount(); ++r) {
        QStringList cols;
        for (int c = 0; c < 6; ++c) {
            auto* it = m_logTable->item(r, c);
            QString v = it ? it->text() : "";
            if (v.contains(',') || v.contains('"'))
                v = '"' + v.replace('"', "\"\"") + '"';
            cols << v;
        }
        out << cols.join(',') << '\n';
    }
    QMessageBox::information(this, tr("Esportato"),
        tr("Diario salvato in:\n%1").arg(path));
}

void IdroWidget::onLogCellChanged(int /*row*/, int /*col*/)
{
    saveLog();
    updateLastMeas();
}

void IdroWidget::onNoteChanged()
{
    QSettings("Prismalux", "Idroponica").setValue("note", m_noteEdit->toPlainText());
}

/* ── Log helpers ──────────────────────────────────────────────────────────── */
QString IdroWidget::logFilePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                  + "/.prismalux";
    QDir().mkpath(dir);
    return dir + "/idro_log.csv";
}

void IdroWidget::addLogRow(const QString& dt, const QString& ph,
                            const QString& ec, const QString& tAc,
                            const QString& tAr, const QString& note)
{
    int row = m_logTable->rowCount();
    m_logTable->insertRow(row);
    QStringList vals{dt, ph, ec, tAc, tAr, note};
    for (int c = 0; c < 6; ++c)
        m_logTable->setItem(row, c, new QTableWidgetItem(vals[c]));
}

void IdroWidget::loadLog()
{
    QFile f(logFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    m_logTable->blockSignals(true);
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList p = line.split(',');
        while (p.size() < 6) p << "";
        addLogRow(p[0], p[1], p[2], p[3], p[4], p[5]);
    }
    m_logTable->blockSignals(false);
    updateLastMeas();
}

void IdroWidget::saveLog()
{
    QFile f(logFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    for (int r = 0; r < m_logTable->rowCount(); ++r) {
        QStringList cols;
        for (int c = 0; c < 6; ++c) {
            auto* it = m_logTable->item(r, c);
            QString v = it ? it->text() : "";
            cols << v.replace(',', ';');
        }
        out << cols.join(',') << '\n';
    }
}

void IdroWidget::updateLastMeas()
{
    int n = m_logTable->rowCount();
    if (n == 0) {
        m_lastMeasLbl->setText(tr("Nessuna misurazione registrata."));
        return;
    }
    auto cell = [&](int c) {
        auto* it = m_logTable->item(n - 1, c);
        return it ? it->text() : "-";
    };
    m_lastMeasLbl->setText(
        tr("Ultima: %1  \xc2\xb7  pH %2  \xc2\xb7  EC %3 mS/cm  \xc2\xb7  "
           "(%2 misurazioni totali)")
            .arg(cell(0), cell(1), cell(2))
            .arg(n));
}

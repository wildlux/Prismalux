#include "widget_solar_calc.h"
#include "../dpi_utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QFrame>
#include <cmath>
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPolygonF>

/* ── Scena isometrica impianto fotovoltaico ──────────────────────────── */
namespace {

class IsoSolarView : public QWidget {
public:
    explicit IsoSolarView(QWidget* parent) : QWidget(parent) {
        setMinimumHeight(dpiScale(220));
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        /* sfondo cielo */
        QLinearGradient sky(0, 0, 0, height());
        sky.setColorAt(0.0, QColor(25, 55, 115));
        sky.setColorAt(1.0, QColor(90, 170, 230));
        p.fillRect(rect(), sky);

        const float tw = 36.0f;
        const float th = tw * 0.5f;
        QPointF o(width() * 0.52f, height() * 0.38f);

        /* proiezione isometrica */
        auto I = [&](float x, float y, float z) -> QPointF {
            return { o.x() + (x - y) * th,
                     o.y() + (x + y) * (th * 0.5f) - z * th };
        };

        /* disegna faccia poligonale */
        auto face = [&](QPointF a, QPointF b, QPointF c, QPointF d, QColor col) {
            QPolygonF pg; pg << a << b << c << d;
            p.setBrush(col);   p.setPen(Qt::NoPen);       p.drawPolygon(pg);
            p.setPen(QPen(QColor(0,0,0,40), 0.7f));
            p.setBrush(Qt::NoBrush);                       p.drawPolygon(pg);
        };

        /* disegna box isometrico */
        auto box = [&](float x, float y, float z,
                       float w, float d, float h, QColor base) {
            face(I(x,y,z+h), I(x+w,y,z+h), I(x+w,y+d,z+h), I(x,y+d,z+h), base.lighter(130));
            face(I(x,y,z),   I(x,y+d,z),   I(x,y+d,z+h),   I(x,y,z+h),   base.darker(155));
            face(I(x+w,y,z), I(x+w,y,z+h), I(x+w,y+d,z+h), I(x+w,y+d,z), base.darker(125));
        };

        /* ── Prato ── */
        box(0,0,0, 9,9,0.12f, QColor(55,125,50));

        /* ── Casa: muri ── */
        box(1,1,0.12f, 4,6,2.6f, QColor(220,215,200));

        /* ── Tetto ── */
        box(0.7f,0.7f,2.72f, 4.6f,6.6f,0.42f, QColor(165,90,70));

        /* ── Pannelli solari (3 file) ── */
        for (float row : {1.7f, 3.3f, 5.0f}) {
            box(1.5f, row, 3.14f, 3.0f, 1.2f, 0.08f, QColor(28,48,130));
            /* griglia celle */
            p.setPen(QPen(QColor(80,140,255,90), 0.5f));
            for (int cx = 1; cx < 4; ++cx) {
                float px = 1.5f + cx * 0.75f;
                p.drawLine(I(px, row, 3.22f), I(px, row+1.2f, 3.22f));
            }
            p.drawLine(I(1.5f,row+0.6f,3.22f), I(4.5f,row+0.6f,3.22f));
        }

        /* ── Batteria ── */
        box(6.0f,1.0f,0.12f, 1.2f,1.5f,1.7f, QColor(55,75,100));
        {   /* indicatore carica verde */
            QPolygonF bar;
            bar << I(6.1f,1.1f,1.82f) << I(6.9f,1.1f,1.82f)
                << I(6.9f,1.45f,1.82f) << I(6.1f,1.45f,1.82f);
            p.setBrush(QColor(50,225,80,210)); p.setPen(Qt::NoPen);
            p.drawPolygon(bar);
        }

        /* ── Regolatore MPPT ── */
        box(6.0f,3.0f,0.12f, 1.0f,1.2f,0.8f, QColor(60,90,70));
        {   /* schermo LED */
            QPolygonF scr;
            scr << I(6.1f,3.1f,0.92f) << I(6.6f,3.1f,0.92f)
                << I(6.6f,3.5f,0.92f) << I(6.1f,3.5f,0.92f);
            p.setBrush(QColor(0,225,100,210)); p.setPen(Qt::NoPen);
            p.drawPolygon(scr);
        }

        /* ── Sole ── */
        {
            int sx = (int)(width() * 0.88f), sy = (int)(height() * 0.10f);
            int sr = (int)(tw * 0.70f);
            p.setPen(QPen(QColor(255,230,80,190), 2.0f));
            for (int a = 0; a < 360; a += 45) {
                float rad = a * 3.14159265f / 180.0f;
                p.drawLine((int)(sx+(sr+3)*cosf(rad)),  (int)(sy+(sr+3)*sinf(rad)),
                           (int)(sx+(sr+12)*cosf(rad)), (int)(sy+(sr+12)*sinf(rad)));
            }
            QRadialGradient sg(sx, sy, sr);
            sg.setColorAt(0, QColor(255,248,110));
            sg.setColorAt(1, QColor(255,175,20));
            p.setPen(Qt::NoPen); p.setBrush(sg);
            p.drawEllipse(QPoint(sx, sy), sr, sr);
        }

        /* ── Cavi (tratteggio) ── */
        p.setPen(QPen(QColor(255,215,50,200), 1.5f, Qt::DashLine));
        p.drawLine(I(4.5f,3.3f,3.14f), I(6.0f,3.7f,0.80f)); /* pannelli→regolatore */
        p.drawLine(I(6.0f,3.0f,0.45f), I(6.0f,2.5f,0.45f)); /* regolatore→batteria */

        /* ── Etichette ── */
        p.setFont(QFont("sans-serif", 8, QFont::Bold));
        auto lbl = [&](float x, float y, float z, const char* txt, int dx = 0, int dy = 0) {
            QPointF pt = I(x, y, z);
            int tx = (int)pt.x() + dx, ty = (int)pt.y() + dy;
            p.setPen(QColor(0,0,0,155));
            p.drawText(tx-44, ty-7, 88, 14, Qt::AlignCenter, txt);
            p.setPen(Qt::white);
            p.drawText(tx-45, ty-8, 88, 14, Qt::AlignCenter, txt);
        };
        lbl(3.0f,3.3f,3.60f, "Pannelli solari", 0, -12);
        lbl(6.6f,1.5f,2.00f, "Batteria",         14, -4);
        lbl(6.5f,3.5f,1.00f, "Regolatore MPPT",  14, -4);
        lbl(3.0f,4.0f,0.70f, "Casa",              -12, 0);
    }
};

} /* namespace */

/* ── helpers statici ────────────────────────────────────────────────── */

/* Sezione cavo standard superiore (mm²) */
double SolarCalcWidget::ceilCavoMm2(double s)
{
    static const double std[] = {1.0, 1.5, 2.5, 4.0, 6.0, 10.0,
                                  16.0, 25.0, 35.0, 50.0};
    for (double v : std) if (v >= s) return v;
    return s;
}

/* Fusibile standard superiore (A) */
int SolarCalcWidget::ceilFusibile(double i)
{
    static const int std[] = {5,10,15,20,25,30,40,50,60,80,100,125,150,200};
    for (int v : std) if (v >= i) return v;
    return static_cast<int>(std::ceil(i));
}

/* ════════════════════════════════════════════════════════════════════════ */

SolarCalcWidget::SolarCalcWidget(QWidget* parent)
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

    /* ── Titolo ─────────────────────────────────────────────────────── */
    auto* titleLbl = new QLabel(
        "<b>\xe2\x98\x80  Calcolatore Impianto Fotovoltaico</b>"
        "<br><small style='color:#9e9e9e;'>Casa \xc2\xb7 Van/Camper \xc2\xb7 "
        "Campeggio \xc2\xb7 Garage \xe2\x80\x94 "
        "regolatori PWM e MPPT a confronto</small>", inner);
    titleLbl->setTextFormat(Qt::RichText);
    rootLay->addWidget(titleLbl);

    /* ── Layout a 2 colonne ─────────────────────────────────────────── */
    auto* colsLay = new QHBoxLayout;
    colsLay->setSpacing(10);
    rootLay->addLayout(colsLay);

    /* ════════════ COLONNA SINISTRA — impostazioni ═══════════════════ */
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(8);

    /* ── GroupBox 1: Configurazione impianto ── */
    auto* cfgBox = new QGroupBox(tr("Configurazione impianto"), inner);
    auto* cfgFrm = new QFormLayout(cfgBox);
    cfgFrm->setSpacing(6);

    m_tipoCombo = new QComboBox(cfgBox);
    m_tipoCombo->addItem("\xf0\x9f\x8f\xa0  Casa abitazione", 0);
    m_tipoCombo->addItem("\xf0\x9f\x9a\x90  Van / Camper",    1);
    m_tipoCombo->addItem("\xe2\x9b\xba  Campeggio / Off-grid", 2);
    m_tipoCombo->addItem("\xf0\x9f\x94\xa7  Garage / Officina", 3);
    m_tipoCombo->addItem("\xf0\x9f\x9a\x97  Auto / Barca",    4);
    cfgFrm->addRow(tr("Tipo impianto:"), m_tipoCombo);

    m_vSistCombo = new QComboBox(cfgBox);
    m_vSistCombo->addItem("12 V", 12);
    m_vSistCombo->addItem("24 V", 24);
    m_vSistCombo->addItem("48 V", 48);
    cfgFrm->addRow(tr("Tensione sistema:"), m_vSistCombo);

    m_batCombo = new QComboBox(cfgBox);
    m_batCombo->addItem("Piombo / AGM  (DOD 50%)", 50);
    m_batCombo->addItem("LiFePO4        (DOD 80%)", 80);
    m_batCombo->addItem("GEL            (DOD 50%)", 50);
    m_batCombo->addItem("Litio NMC      (DOD 85%)", 85);
    cfgFrm->addRow(tr("Tipo batteria:"), m_batCombo);

    m_autonomiaSpin = new QSpinBox(cfgBox);
    m_autonomiaSpin->setRange(1, 10);
    m_autonomiaSpin->setValue(2);
    m_autonomiaSpin->setSuffix(tr(" gg senza sole"));
    cfgFrm->addRow(tr("Autonomia:"), m_autonomiaSpin);

    m_pshSpin = new QDoubleSpinBox(cfgBox);
    m_pshSpin->setRange(1.0, 7.0);
    m_pshSpin->setSingleStep(0.5);
    m_pshSpin->setValue(4.5);
    m_pshSpin->setSuffix(tr(" h/gg  (PSH)"));
    m_pshSpin->setToolTip(tr(
        "Peak Solar Hours — ore equivalenti di irradiazione massima (1000 W/m\xc2\xb2).\n"
        "Italia nord: ~3.5-4.0   centro: ~4.5-5.0   sud: ~5.0-6.0\n"
        "Van/Camper (tetto piatto): -10-15% rispetto al fisso"));
    cfgFrm->addRow(tr("Irradiazione (PSH):"), m_pshSpin);

    auto* pshNote = new QLabel(
        "<small><i>3.5=nord IT \xc2\xb7 4.5=centro \xc2\xb7 5.5=sud \xc2\xb7 "
        "6.5=desertico</i></small>", cfgBox);
    pshNote->setTextFormat(Qt::RichText);
    cfgFrm->addRow("", pshNote);

    m_etaSpin = new QSpinBox(cfgBox);
    m_etaSpin->setRange(60, 95);
    m_etaSpin->setValue(80);
    m_etaSpin->setSuffix(" %");
    m_etaSpin->setToolTip(tr(
        "Efficienza globale sistema (cavi + regolatore + batteria).\n"
        "PWM tipico: 70-78%   MPPT tipico: 80-88%"));
    cfgFrm->addRow(tr("Efficienza sistema:"), m_etaSpin);
    leftCol->addWidget(cfgBox);

    /* ── GroupBox 2: Parametri regolatore ── */
    auto* regBox = new QGroupBox(tr("Parametri regolatore di carica"), inner);
    auto* regLay = new QVBoxLayout(regBox);
    regLay->setSpacing(6);

    /* PWM e MPPT affiancati dentro regBox */
    auto* regCompRow = new QHBoxLayout;

    auto* pwmGroup = new QGroupBox(tr("\xe2\x9a\xa1  PWM"), regBox);
    auto* pwmFrm   = new QFormLayout(pwmGroup);
    pwmFrm->setSpacing(5);
    auto* pwmDesc = new QLabel(
        "<small>Abbassa la tensione del pannello a quella<br>"
        "della batteria. Semplice, economico,<br>"
        "ma spreca la differenza di tensione.</small>", pwmGroup);
    pwmDesc->setTextFormat(Qt::RichText);
    pwmFrm->addRow(pwmDesc);

    m_vocPwmSpin = new QDoubleSpinBox(pwmGroup);
    m_vocPwmSpin->setRange(10.0, 100.0);
    m_vocPwmSpin->setSingleStep(0.5);
    m_vocPwmSpin->setValue(21.6);
    m_vocPwmSpin->setSuffix(" V  (Voc)");
    m_vocPwmSpin->setToolTip(tr(
        "Tensione a circuito aperto del pannello singolo.\n"
        "36 celle (12V sistema): ~21-22 V\n"
        "72 celle (24V sistema): ~42-44 V"));
    pwmFrm->addRow(tr("Voc pannello:"), m_vocPwmSpin);

    auto* pwmNote = new QLabel(
        "<small style='color:#f0a000;'>\xe2\x9a\xa0  12V: Voc 18-21V (36 celle)<br>"
        "24V: Voc 36-42V (72 celle)</small>", pwmGroup);
    pwmNote->setTextFormat(Qt::RichText);
    pwmFrm->addRow(pwmNote);
    regCompRow->addWidget(pwmGroup);

    auto* mpptGroup = new QGroupBox(tr("\xf0\x9f\x9a\x80  MPPT"), regBox);
    auto* mpptFrm   = new QFormLayout(mpptGroup);
    mpptFrm->setSpacing(5);
    auto* mpptDesc = new QLabel(
        "<small>Trova il punto di massima potenza<br>"
        "e converte l'eccesso in corrente.<br>"
        "Efficienza 95-98%.</small>", mpptGroup);
    mpptDesc->setTextFormat(Qt::RichText);
    mpptFrm->addRow(mpptDesc);

    m_vMppSpin = new QDoubleSpinBox(mpptGroup);
    m_vMppSpin->setRange(12.0, 600.0);
    m_vMppSpin->setSingleStep(1.0);
    m_vMppSpin->setValue(36.0);
    m_vMppSpin->setSuffix(" V  (Vmpp)");
    m_vMppSpin->setToolTip(tr(
        "Tensione al punto di massima potenza della stringa.\n"
        "Pannello singolo: ~18V (36 celle) o ~36V (72 celle)."));
    mpptFrm->addRow(tr("Vmpp stringa:"), m_vMppSpin);

    m_vocMpptSpin = new QDoubleSpinBox(mpptGroup);
    m_vocMpptSpin->setRange(12.0, 600.0);
    m_vocMpptSpin->setSingleStep(1.0);
    m_vocMpptSpin->setValue(44.0);
    m_vocMpptSpin->setSuffix(" V  (Voc max)");
    m_vocMpptSpin->setToolTip(tr(
        "Voc stringa (a freddo) — deve essere < Vmax MPPT.\n"
        "MPPT 12/24V: max 50V tipico\n"
        "MPPT 48V: max 150V tipico"));
    mpptFrm->addRow(tr("Voc stringa:"), m_vocMpptSpin);

    m_etaMpptSpin = new QSpinBox(mpptGroup);
    m_etaMpptSpin->setRange(90, 99);
    m_etaMpptSpin->setValue(97);
    m_etaMpptSpin->setSuffix(" %");
    mpptFrm->addRow(tr("Efficienza MPPT:"), m_etaMpptSpin);
    regCompRow->addWidget(mpptGroup);
    regLay->addLayout(regCompRow);

    /* lunghezza cavi — in fondo a regBox */
    auto* caviRow = new QHBoxLayout;
    caviRow->addWidget(new QLabel(
        tr("Cavi DC pannello \xe2\x86\x92 regolatore:"), regBox));
    m_lCaviSpin = new QDoubleSpinBox(regBox);
    m_lCaviSpin->setRange(0.5, 100.0);
    m_lCaviSpin->setValue(5.0);
    m_lCaviSpin->setSuffix(" m");
    m_lCaviSpin->setToolTip(tr(
        "Lunghezza singolo conduttore (andata); il calcolo usa 2\xc3\x97L.\n"
        "Caduta tensione max ammessa: 3% della tensione sistema."));
    caviRow->addWidget(m_lCaviSpin);
    auto* caviNote = new QLabel(
        "<small><i>(\xce\x94V max 3%)</i></small>", regBox);
    caviNote->setTextFormat(Qt::RichText);
    caviRow->addWidget(caviNote);
    caviRow->addStretch();
    regLay->addLayout(caviRow);
    leftCol->addWidget(regBox);

    leftCol->addStretch();
    colsLay->addLayout(leftCol, 2);

    /* ════════════ COLONNA DESTRA — dati e risultati ═════════════════ */
    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(8);

    /* ── Schema 3D impianto ── */
    auto* schemaBox = new QGroupBox(tr("Schema impianto"), inner);
    auto* schemaLay = new QVBoxLayout(schemaBox);
    schemaLay->setContentsMargins(4, 4, 4, 4);
    schemaLay->addWidget(new IsoSolarView(schemaBox));
    rightCol->addWidget(schemaBox);

    /* ── GroupBox 3: Carichi elettrici ── */
    auto* loadBox = new QGroupBox(tr("Carichi elettrici (Wh / giorno)"), inner);
    auto* loadLay = new QVBoxLayout(loadBox);
    loadLay->setSpacing(6);

    auto* loadBtnRow = new QHBoxLayout;
    auto* addBtn    = new QPushButton(tr("\xe2\x9e\x95  Aggiungi"), loadBox);
    auto* remBtn    = new QPushButton(tr("\xe2\x9e\x96  Rimuovi"),  loadBox);
    auto* presetBtn = new QPushButton(tr("\xf0\x9f\x93\x8b  Preset"), loadBox);
    addBtn->setObjectName("actionBtn");
    presetBtn->setObjectName("actionBtn");
    loadBtnRow->addWidget(addBtn);
    loadBtnRow->addWidget(remBtn);
    loadBtnRow->addWidget(presetBtn);
    loadBtnRow->addStretch();
    m_totLbl = new QLabel(tr("Totale: \xe2\x80\x94 Wh/giorno"), loadBox);
    m_totLbl->setStyleSheet("font-weight:bold;");
    loadBtnRow->addWidget(m_totLbl);
    loadLay->addLayout(loadBtnRow);

    m_loadTable = new QTableWidget(0, 4, loadBox);
    m_loadTable->setHorizontalHeaderLabels({
        tr("Apparecchio"), tr("W"), tr("Ore/gg"), tr("Wh/gg")});
    m_loadTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_loadTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_loadTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_loadTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_loadTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_loadTable->verticalHeader()->setVisible(false);
    m_loadTable->setAlternatingRowColors(true);
    m_loadTable->setMinimumHeight(dpiScale(180));
    loadLay->addWidget(m_loadTable, 1);

    auto* loadNote = new QLabel(
        "<small><i>\xe2\x84\xb9  Wh/gg si aggiorna automaticamente. "
        "Ore = 0 per disabilitare un carico.</i></small>", loadBox);
    loadNote->setTextFormat(Qt::RichText);
    loadNote->setWordWrap(true);
    loadLay->addWidget(loadNote);
    rightCol->addWidget(loadBox, 1);

    /* ── Pulsante Calcola ── */
    auto* calcBtn = new QPushButton(
        "\xe2\x98\x80  CALCOLA IMPIANTO", inner);
    calcBtn->setObjectName("actionBtn");
    calcBtn->setMinimumHeight(dpiScale(42));
    calcBtn->setStyleSheet("QPushButton { font-size:14px; font-weight:bold; }");
    rightCol->addWidget(calcBtn);

    /* ── GroupBox 4: Risultati (nascosto finché non si calcola) ── */
    m_resBox = new QGroupBox(tr("Risultati"), inner);
    m_resBox->setVisible(false);
    auto* resLay = new QVBoxLayout(m_resBox);
    resLay->setSpacing(6);

    /* KPI in 2 righe da 3 (anziché tutto in una riga) */
    auto makeKPI = [](const QString& label, QLabel*& valLbl, QWidget* parent) {
        auto* box = new QVBoxLayout;
        auto* lbl = new QLabel(label, parent);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color:#9e9e9e; font-size:11px;");
        valLbl = new QLabel("\xe2\x80\x94", parent);
        valLbl->setAlignment(Qt::AlignCenter);
        valLbl->setStyleSheet("font-size:15px; font-weight:bold;");
        box->addWidget(lbl);
        box->addWidget(valLbl);
        return box;
    };
    auto* kpiRow1 = new QHBoxLayout;
    kpiRow1->addLayout(makeKPI(tr("Consumo"),      m_resConsLbl, m_resBox));
    kpiRow1->addLayout(makeKPI(tr("Batteria"),      m_resBatLbl,  m_resBox));
    kpiRow1->addLayout(makeKPI(tr("Pannelli"),      m_resPanLbl,  m_resBox));
    resLay->addLayout(kpiRow1);

    auto* kpiRow2 = new QHBoxLayout;
    kpiRow2->addLayout(makeKPI(tr("Cavi DC"),       m_resCaviLbl, m_resBox));
    kpiRow2->addLayout(makeKPI(tr("Fusibile bat."), m_resFusLbl,  m_resBox));
    kpiRow2->addStretch();
    resLay->addLayout(kpiRow2);

    auto* sep = new QFrame(m_resBox);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    resLay->addWidget(sep);

    /* confronto PWM vs MPPT — 2 colonne dentro il groupbox Risultati */
    auto* cmpRow = new QHBoxLayout;

    auto* pwmResGroup = new QGroupBox(tr("\xe2\x9a\xa1  Regolatore PWM"), m_resBox);
    auto* pwmResLay   = new QVBoxLayout(pwmResGroup);
    m_pwmIregLbl   = new QLabel("\xe2\x80\x94", pwmResGroup);
    m_pwmTagliaLbl = new QLabel("\xe2\x80\x94", pwmResGroup);
    m_pwmNoteWarn  = new QLabel("", pwmResGroup);
    m_pwmNoteWarn->setTextFormat(Qt::RichText);
    m_pwmNoteWarn->setWordWrap(true);
    pwmResLay->addWidget(m_pwmIregLbl);
    pwmResLay->addWidget(m_pwmTagliaLbl);
    pwmResLay->addWidget(m_pwmNoteWarn);
    pwmResLay->addStretch();
    cmpRow->addWidget(pwmResGroup);

    auto* mpptResGroup = new QGroupBox(tr("\xf0\x9f\x9a\x80  Regolatore MPPT"), m_resBox);
    auto* mpptResLay   = new QVBoxLayout(mpptResGroup);
    m_mpptIoutLbl   = new QLabel("\xe2\x80\x94", mpptResGroup);
    m_mpptTagliaLbl = new QLabel("\xe2\x80\x94", mpptResGroup);
    m_mpptGainLbl   = new QLabel("\xe2\x80\x94", mpptResGroup);
    m_mpptGainLbl->setStyleSheet("color:#4caf50; font-weight:bold;");
    mpptResLay->addWidget(m_mpptIoutLbl);
    mpptResLay->addWidget(m_mpptTagliaLbl);
    mpptResLay->addWidget(m_mpptGainLbl);
    mpptResLay->addStretch();
    cmpRow->addWidget(mpptResGroup);
    resLay->addLayout(cmpRow);

    m_resDetail = new QTextEdit(m_resBox);
    m_resDetail->setReadOnly(true);
    m_resDetail->setObjectName("outputView");
    m_resDetail->setMinimumHeight(dpiScale(120));
    resLay->addWidget(m_resDetail);

    auto* copyRow = new QHBoxLayout;
    m_copyBtn = new QPushButton(tr("\xf0\x9f\x93\x8b  Copia risultati"), m_resBox);
    copyRow->addWidget(m_copyBtn);
    copyRow->addStretch();
    resLay->addLayout(copyRow);

    rightCol->addWidget(m_resBox);

    rightCol->addStretch();
    colsLay->addLayout(rightCol, 3);

    /* ── Preset casa di default ── */
    addLoadRow(tr("Frigorifero"),        100, 24.0);
    addLoadRow(tr("Illuminazione LED"),   40,  6.0);
    addLoadRow(tr("TV / Monitor"),        80,  4.0);
    addLoadRow(tr("Router / NAS"),        15, 24.0);
    addLoadRow(tr("Caricabatterie"),      20,  3.0);
    updateTotale();

    /* ── Connessioni ─────────────────────────────────────────────── */
    connect(addBtn,    &QPushButton::clicked, this, &SolarCalcWidget::onAddRowClicked);
    connect(remBtn,    &QPushButton::clicked, this, &SolarCalcWidget::onRemoveRowClicked);
    connect(presetBtn, &QPushButton::clicked, this, &SolarCalcWidget::onPresetClicked);
    connect(m_tipoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SolarCalcWidget::onTipoChanged);
    connect(m_loadTable, &QTableWidget::cellChanged,
            this, &SolarCalcWidget::onCellChanged);
    connect(calcBtn,   &QPushButton::clicked, this, &SolarCalcWidget::onCalcolaClicked);
    connect(m_copyBtn, &QPushButton::clicked, this, &SolarCalcWidget::onCopyClicked);
}

/* ── addLoadRow ──────────────────────────────────────────────────────── */
void SolarCalcWidget::addLoadRow(const QString& nome, double pw, double hrs)
{
    m_loadTable->blockSignals(true);
    const int r = m_loadTable->rowCount();
    m_loadTable->insertRow(r);
    m_loadTable->setItem(r, 0, new QTableWidgetItem(nome));
    m_loadTable->setItem(r, 1, new QTableWidgetItem(QString::number(pw, 'f', 0)));
    m_loadTable->setItem(r, 2, new QTableWidgetItem(QString::number(hrs, 'f', 1)));
    const double wh = pw * hrs;
    auto* whItem = new QTableWidgetItem(QString::number(wh, 'f', 1));
    whItem->setFlags(whItem->flags() & ~Qt::ItemIsEditable);
    m_loadTable->setItem(r, 3, whItem);
    m_loadTable->blockSignals(false);
}

/* ── updateTotale ────────────────────────────────────────────────────── */
void SolarCalcWidget::updateTotale()
{
    double tot = 0;
    for (int r = 0; r < m_loadTable->rowCount(); ++r) {
        auto* pw = m_loadTable->item(r, 1);
        auto* hr = m_loadTable->item(r, 2);
        if (pw && hr) tot += pw->text().toDouble() * hr->text().toDouble();
    }
    m_totLbl->setText(
        QString(tr("Totale: <b>%1 Wh/giorno</b>")).arg(tot, 0, 'f', 0));
    m_totLbl->setTextFormat(Qt::RichText);
}

/* ── helpers ─────────────────────────────────────────────────────────── */
double SolarCalcWidget::selectedVoltage() const
{
    return m_vSistCombo->currentData().toDouble();
}

double SolarCalcWidget::selectedDod() const
{
    return m_batCombo->currentData().toDouble() / 100.0;
}

/* ── onCellChanged ───────────────────────────────────────────────────── */
void SolarCalcWidget::onCellChanged(int row, int col)
{
    if (col != 1 && col != 2) return;
    auto* pw = m_loadTable->item(row, 1);
    auto* hr = m_loadTable->item(row, 2);
    if (!pw || !hr) return;
    const double wh = pw->text().toDouble() * hr->text().toDouble();
    m_loadTable->blockSignals(true);
    auto* whItem = m_loadTable->item(row, 3);
    if (!whItem) {
        whItem = new QTableWidgetItem;
        whItem->setFlags(whItem->flags() & ~Qt::ItemIsEditable);
        m_loadTable->setItem(row, 3, whItem);
    }
    whItem->setText(QString::number(wh, 'f', 1));
    m_loadTable->blockSignals(false);
    updateTotale();
}

/* ── onAddRowClicked ─────────────────────────────────────────────────── */
void SolarCalcWidget::onAddRowClicked()
{
    addLoadRow(tr("Nuovo apparecchio"), 0, 0);
    m_loadTable->scrollToBottom();
    m_loadTable->setCurrentCell(m_loadTable->rowCount() - 1, 0);
    m_loadTable->editItem(m_loadTable->currentItem());
}

void SolarCalcWidget::onRemoveRowClicked()
{
    const int r = m_loadTable->currentRow();
    if (r >= 0) { m_loadTable->removeRow(r); updateTotale(); }
}

/* ── onPresetClicked ─────────────────────────────────────────────────── */
void SolarCalcWidget::onPresetClicked()
{
    auto* menu = new QMenu(this);
    menu->addAction(tr("\xf0\x9f\x8f\xa0 Casa (media famiglia)"),  this, [this]{ onTipoChanged(0); });
    menu->addAction(tr("\xf0\x9f\x9a\x90 Van / Camper"),           this, [this]{ onTipoChanged(1); });
    menu->addAction(tr("\xe2\x9b\xba Campeggio / Off-grid"),        this, [this]{ onTipoChanged(2); });
    menu->addAction(tr("\xf0\x9f\x94\xa7 Garage / Officina"),       this, [this]{ onTipoChanged(3); });
    menu->addAction(tr("\xf0\x9f\x9a\x97 Auto / Barca"),            this, [this]{ onTipoChanged(4); });
    menu->exec(QCursor::pos());
    menu->deleteLater();
}

/* ── onTipoChanged — carica preset carichi ──────────────────────────── */
void SolarCalcWidget::onTipoChanged(int idx)
{
    m_tipoCombo->setCurrentIndex(idx);
    m_loadTable->blockSignals(true);
    m_loadTable->setRowCount(0);
    m_loadTable->blockSignals(false);

    switch (idx) {
    case 0: /* Casa */
        addLoadRow(tr("Frigorifero"),        100, 24.0);
        addLoadRow(tr("Illuminazione LED"),   40,  6.0);
        addLoadRow(tr("TV / Monitor"),        80,  4.0);
        addLoadRow(tr("Router / NAS"),        15, 24.0);
        addLoadRow(tr("Caricabatterie"),      20,  3.0);
        addLoadRow(tr("Lavatrice (uso day)"), 500,  0.5);
        addLoadRow(tr("Pompa acqua"),         200,  0.5);
        m_vSistCombo->setCurrentIndex(2);  /* 48V */
        m_autonomiaSpin->setValue(2);
        m_pshSpin->setValue(4.5);
        m_etaSpin->setValue(82);
        break;
    case 1: /* Van/Camper */
        addLoadRow(tr("Frigo portatile"),     40, 24.0);
        addLoadRow(tr("Illuminazione LED"),   15,  5.0);
        addLoadRow(tr("Carica telefoni"),     20,  3.0);
        addLoadRow(tr("Ventilatore"),         30,  8.0);
        addLoadRow(tr("Laptop"),              50,  3.0);
        addLoadRow(tr("Pompa acqua"),         60,  0.3);
        m_vSistCombo->setCurrentIndex(0);  /* 12V */
        m_autonomiaSpin->setValue(1);
        m_pshSpin->setValue(4.0);
        m_etaSpin->setValue(78);
        break;
    case 2: /* Campeggio */
        addLoadRow(tr("Illuminazione LED"),   20,  5.0);
        addLoadRow(tr("Carica telefoni"),     15,  2.0);
        addLoadRow(tr("Pompa acqua"),         80,  0.3);
        addLoadRow(tr("Piccolo frigo"),       30, 12.0);
        addLoadRow(tr("Radio / BT speaker"),  10,  4.0);
        m_vSistCombo->setCurrentIndex(0);  /* 12V */
        m_autonomiaSpin->setValue(3);
        m_pshSpin->setValue(5.0);
        m_etaSpin->setValue(80);
        break;
    case 3: /* Garage */
        addLoadRow(tr("Illuminazione"),      100,  4.0);
        addLoadRow(tr("Trapano/Smerigliatrice (saltuario)"), 600, 0.3);
        addLoadRow(tr("Caricabatterie auto"), 50,  2.0);
        addLoadRow(tr("Ventilatore"),         60,  3.0);
        addLoadRow(tr("Piccola TV"),          40,  2.0);
        m_vSistCombo->setCurrentIndex(1);  /* 24V */
        m_autonomiaSpin->setValue(1);
        m_pshSpin->setValue(4.0);
        m_etaSpin->setValue(80);
        break;
    case 4: /* Auto / Barca */
        addLoadRow(tr("Luci di posizione"), 10, 12.0);
        addLoadRow(tr("Strumentazione"),    15, 24.0);
        addLoadRow(tr("Carica telefoni"),   15,  3.0);
        addLoadRow(tr("Pompa sentina"),    100,  0.1);
        addLoadRow(tr("VHF / radio"),       10,  4.0);
        m_vSistCombo->setCurrentIndex(0);  /* 12V */
        m_autonomiaSpin->setValue(2);
        m_pshSpin->setValue(5.0);
        m_etaSpin->setValue(78);
        break;
    }
    updateTotale();
}

/* ══════════════════════════════════════════════════════════════
   onCalcolaClicked — motore di calcolo principale
   ══════════════════════════════════════════════════════════════ */
void SolarCalcWidget::onCalcolaClicked()
{
    /* ── 1. Consumo totale ── */
    double eTot = 0;
    for (int r = 0; r < m_loadTable->rowCount(); ++r) {
        auto* pw = m_loadTable->item(r, 1);
        auto* hr = m_loadTable->item(r, 2);
        if (pw && hr) eTot += pw->text().toDouble() * hr->text().toDouble();
    }
    if (eTot <= 0) {
        m_resBox->setVisible(false);
        m_totLbl->setText(
            "<span style='color:#f44336;'>"
            "\xe2\x9d\x8c  Inserisci almeno un carico con potenza > 0 W</span>");
        m_totLbl->setTextFormat(Qt::RichText);
        return;
    }

    const double vSist    = selectedVoltage();
    const double dod      = selectedDod();
    const double eta      = m_etaSpin->value() / 100.0;
    const double psh      = m_pshSpin->value();
    const int    autonomia= m_autonomiaSpin->value();
    const double lCavi    = m_lCaviSpin->value();

    /* ── 2. Consumo corretto per efficienza sistema ── */
    const double eCorr = eTot / eta;

    /* ── 3. Dimensionamento batteria ── */
    const double eBat  = eCorr * autonomia;               /* Wh */
    const double cBatAh = eBat / (vSist * dod);           /* Ah min */
    const double cNomAh = cBatAh * 1.20;                  /* +20% margine */

    /* ── 4. Dimensionamento pannelli ── */
    const double ePanTot = eCorr * 1.25;                  /* +25% perdite temp/sporco */
    const double wpRichiesti = ePanTot / psh;             /* Wp */

    /* ── 5. PWM ── */
    const double iNomPwm  = wpRichiesti / vSist;          /* corrente nominale pannelli */
    const double iRegPwm  = iNomPwm * 1.25;               /* taglia regolatore con margine */
    /* Potenza reale sfruttata con PWM: il pannello lavora a V_bat invece di Vmpp */
    const double vMppPan  = m_vocPwmSpin->value() * 0.82; /* Vmpp ≈ 82% di Voc */
    const double etaPwm   = vSist / vMppPan;              /* efficienza trasferimento PWM */
    /* con PWM i Wp reali sfruttati sono meno */
    const double wpPwmEff = wpRichiesti / etaPwm;         /* Wp necessari con PWM */

    /* ── 6. MPPT ── */
    const double etaMppt = m_etaMpptSpin->value() / 100.0;
    const double vMppStr = m_vMppSpin->value();           /* tensione MPP stringa */
    const double vocStr  = m_vocMpptSpin->value();        /* Voc stringa */
    /* corrente in uscita MPPT: tutta la potenza convertita a tensione batteria */
    const double iOutMppt  = (wpRichiesti * etaMppt) / (vSist * 1.05); /* /vBat_carica */
    const double iRegMppt  = iOutMppt * 1.10;
    /* guadagno MPPT rispetto a PWM in corrente */
    const double gainPct   = ((iOutMppt - iNomPwm) / iNomPwm) * 100.0;

    /* ── 7. Sezione cavi (caduta tensione 3%) ── */
    const double dvMax   = vSist * 0.03;
    const double sCavi   = (2.0 * lCavi * iRegMppt) / (56.0 * dvMax); /* mm² */
    const double sNom    = ceilCavoMm2(sCavi);

    /* fusibile batteria: 1.25 × corrente scarica max (C/5) */
    const double iMaxBat = cNomAh / 5.0;   /* corrente C/5 */
    const double iFus    = std::max(iRegPwm, iRegMppt) * 1.25;
    const int    fus     = ceilFusibile(iFus);

    /* ── Aggiorna UI ── */
    m_resConsLbl->setText(QString("%1 Wh/gg").arg(eTot, 0, 'f', 0));
    m_resBatLbl->setText(
        QString("%1 Ah\n(%2 Wh)")
            .arg(cNomAh, 0, 'f', 0)
            .arg(cNomAh * vSist, 0, 'f', 0));
    m_resPanLbl->setText(QString("%1 Wp").arg(wpRichiesti, 0, 'f', 0));
    m_resCaviLbl->setText(QString("%1 mm\xc2\xb2").arg(sNom, 0, 'f', 1));  /* mm² */
    m_resFusLbl->setText(QString("%1 A").arg(fus));

    /* PWM */
    m_pwmIregLbl->setText(
        QString(tr("Corrente uscita nominale: <b>%1 A</b>")).arg(iNomPwm, 0, 'f', 1));
    m_pwmIregLbl->setTextFormat(Qt::RichText);
    m_pwmTagliaLbl->setText(
        QString(tr("Taglia regolatore PWM: <b>%1 A</b>")).arg(static_cast<int>(std::ceil(iRegPwm))));
    m_pwmTagliaLbl->setTextFormat(Qt::RichText);
    if (etaPwm < 0.75) {
        m_pwmNoteWarn->setText(
            QString("<small style='color:#f0a000;'>"
                    "\xe2\x9a\xa0  Efficienza trasferimento ~%1% "
                    "(&Delta;V = %2 V sprecati).<br>"
                    "Con PWM servono <b>%3 Wp</b> di pannelli invece di %4 Wp.</small>")
            .arg(static_cast<int>(etaPwm * 100))
            .arg(vMppPan - vSist, 0, 'f', 1)
            .arg(wpPwmEff, 0, 'f', 0)
            .arg(wpRichiesti, 0, 'f', 0));
    } else {
        m_pwmNoteWarn->setText(
            "<small style='color:#4caf50;'>"
            "\xe2\x9c\x85  Efficienza trasferimento accettabile con questo pannello.</small>");
    }

    /* MPPT */
    m_mpptIoutLbl->setText(
        QString(tr("Corrente uscita MPPT: <b>%1 A</b>")).arg(iOutMppt, 0, 'f', 1));
    m_mpptIoutLbl->setTextFormat(Qt::RichText);
    m_mpptTagliaLbl->setText(
        QString(tr("Taglia regolatore MPPT: <b>%1 A</b>")).arg(static_cast<int>(std::ceil(iRegMppt))));
    m_mpptTagliaLbl->setTextFormat(Qt::RichText);
    m_mpptGainLbl->setText(
        QString(tr("\xe2\x9c\x85  Guadagno vs PWM: +%1%  (Vmpp %2V \xe2\x86\x92 Vbat %3V)"))
            .arg(gainPct, 0, 'f', 0)
            .arg(vMppStr, 0, 'f', 0)
            .arg(vSist, 0, 'f', 0));

    /* ── Report dettagliato ── */
    const QString batTipo = m_batCombo->currentText();
    const QString tipoImp = m_tipoCombo->currentText();
    QString report;
    report += QString("=== CALCOLO IMPIANTO FOTOVOLTAICO ===\n");
    report += QString("Tipo: %1 | Tensione: %2 V | Batteria: %3\n")
              .arg(tipoImp).arg(vSist).arg(batTipo);
    report += QString("PSH: %1 h/gg | Efficienza sistema: %2% | Autonomia: %3 gg\n\n")
              .arg(psh).arg(static_cast<int>(eta*100)).arg(autonomia);

    report += "--- CONSUMO ---\n";
    report += QString("Consumo totale carichi:    %1 Wh/giorno\n").arg(eTot, 0, 'f', 0);
    report += QString("Consumo corretto (eta=%1%): %2 Wh/giorno\n")
              .arg(static_cast<int>(eta*100)).arg(eCorr, 0, 'f', 0);

    report += "\n--- BATTERIA ---\n";
    report += QString("Energia richiesta (x%1 gg): %2 Wh\n").arg(autonomia).arg(eBat, 0, 'f', 0);
    report += QString("DOD batteria:               %1%%\n").arg(static_cast<int>(dod*100));
    report += QString("Capacita' minima:           %1 Ah  (%2 Wh nominali @ %3V)\n")
              .arg(cBatAh, 0, 'f', 0).arg(cBatAh * vSist, 0, 'f', 0).arg(vSist);
    report += QString("Capacita' consigliata (+20%%): %1 Ah\n").arg(cNomAh, 0, 'f', 0);

    report += "\n--- PANNELLI ---\n";
    report += QString("Energia richiesta (x1.25):  %1 Wh/giorno\n").arg(ePanTot, 0, 'f', 0);
    report += QString("Potenza picco pannelli:     %1 Wp  (con MPPT)\n").arg(wpRichiesti, 0, 'f', 0);
    report += QString("Potenza picco pannelli:     %1 Wp  (con PWM, eff. %2%%)\n")
              .arg(wpPwmEff, 0, 'f', 0).arg(static_cast<int>(etaPwm*100));

    report += "\n--- REGOLATORE PWM ---\n";
    report += QString("Corrente nominale pannelli: %1 A\n").arg(iNomPwm, 0, 'f', 1);
    report += QString("Taglia regolatore (x1.25):  >= %1 A\n").arg(static_cast<int>(std::ceil(iRegPwm)));
    report += QString("Voc pannello singolo:       %1 V\n").arg(m_vocPwmSpin->value());
    report += QString("  >> Usare pannello Voc <= Vmax_pwm del regolatore scelto\n");

    report += "\n--- REGOLATORE MPPT ---\n";
    report += QString("Vmpp stringa:               %1 V\n").arg(vMppStr);
    report += QString("Voc stringa (freddo):       %1 V  (non superare Vmax MPPT!)\n").arg(vocStr);
    report += QString("Corrente uscita MPPT:       %1 A\n").arg(iOutMppt, 0, 'f', 1);
    report += QString("Taglia regolatore (x1.10):  >= %1 A\n").arg(static_cast<int>(std::ceil(iRegMppt)));
    report += QString("Guadagno vs PWM:            +%1%%\n").arg(gainPct, 0, 'f', 0);

    report += "\n--- CAVI (lato DC pannello->regolatore) ---\n";
    report += QString("Lunghezza cavo (andata):    %1 m\n").arg(lCavi);
    report += QString("Caduta tensione max (3%%):   %1 V\n").arg(dvMax, 0, 'f', 2);
    report += QString("Sezione calcolata:          %1 mm2\n").arg(sCavi, 0, 'f', 2);
    report += QString("Sezione consigliata:        %1 mm2 (standard)\n").arg(sNom, 0, 'f', 1);

    report += "\n--- FUSIBILI ---\n";
    report += QString("Corrente max impianto:      %1 A\n").arg(std::max(iRegPwm, iRegMppt), 0, 'f', 1);
    report += QString("Fusibile lato batteria:     %1 A  (pos. entro 30 cm dal +)\n").arg(fus);
    report += QString("Fusibile lato pannelli:     %1 A\n").arg(ceilFusibile(iNomPwm * 1.25));

    report += "\n--- SICUREZZA ---\n";
    report += QString("* Installare un sezionatore DC tra pannelli e regolatore\n");
    report += QString("* Cavi DC: usare cavi solare H1Z2Z2-K UV-resistenti\n");
    report += QString("* Inversore: scegliere potenza >= %1 W (picco carico)\n")
              .arg(static_cast<int>(eTot / 4));
    report += QString("* Batterie al piombo: ventilare il vano (emissione H2)\n");
    report += QString("* Verificare sempre i datasheet dei componenti scelti\n");

    m_resDetail->setPlainText(report);
    m_resBox->setVisible(true);
}

/* ── onCopyClicked ───────────────────────────────────────────────────── */
void SolarCalcWidget::onCopyClicked()
{
    QApplication::clipboard()->setText(m_resDetail->toPlainText());
    m_copyBtn->setText(tr("\xe2\x9c\x85  Copiato!"));
    QTimer::singleShot(2000, m_copyBtn,
        [this]{ if (m_copyBtn) m_copyBtn->setText(tr("\xf0\x9f\x93\x8b  Copia risultati")); });
}

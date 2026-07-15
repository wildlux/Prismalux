#include "widget_ram_calculator.h"
#include "../dpi_utils.h"
#include "../theme_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>

namespace {

struct RcColors { QString card, bdr, accent, green, red, txt, mut; };

RcColors themeColors()
{
    const bool light = ThemeManager::instance()->currentId().startsWith("light");
    RcColors c;
    if (light) {
        c.card = "#f1f5f9"; c.bdr = "#cbd5e1"; c.accent = "#2563eb";
        c.green = "#15803d"; c.red = "#dc2626"; c.txt = "#1e293b"; c.mut = "#64748b";
    } else {
        c.card = "#111c2e"; c.bdr = "#1e3a5f"; c.accent = "#3b82f6";
        c.green = "#4ade80"; c.red = "#f87171"; c.txt = "#e2e8f0"; c.mut = "#94a3b8";
    }
    return c;
}

/* preset quantizzazione → bit/parametro approssimati (convenzione llama.cpp) */
struct QuantPreset { const char* label; double bits; };
const QuantPreset kQuantPresets[] = {
    { "FP32 (32 bit) \xe2\x80\x94 precisione piena",        32.0 },
    { "FP16 / BF16 (16 bit) \xe2\x80\x94 precisione piena",  16.0 },
    { "Q8_0 (~8.5 bit) \xe2\x80\x94 quasi lossless",          8.5 },
    { "Q6_K (~6.6 bit) \xe2\x80\x94 alta qualit\xc3\xa0",      6.6 },
    { "Q5_K_M (~5.5 bit) \xe2\x80\x94 buona qualit\xc3\xa0",   5.5 },
    { "Q4_K_M (~4.5 bit) \xe2\x80\x94 standard consigliato",   4.5 },
    { "Q3_K_M (~3.4 bit) \xe2\x80\x94 qualit\xc3\xa0 ridotta",  3.4 },
    { "Q2_K (~2.6 bit) \xe2\x80\x94 qualit\xc3\xa0 minima",    2.6 },
    { "Personalizzato (imposta i bit qui sotto)",              4.5 },
};
const double kRamPresets[] = { 4, 8, 16, 32, 64, 128, 256 };

} // namespace

RamCalculatorWidget::RamCalculatorWidget(QWidget* parent) : QWidget(parent)
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* inner = new QWidget(scroll);
    auto* root  = new QVBoxLayout(inner);
    root->setContentsMargins(dpiScale(16), dpiScale(12), dpiScale(16), dpiScale(12));
    root->setSpacing(dpiScale(12));

    auto* title = new QLabel(
        tr("\xf0\x9f\xa7\xae Calcolatore RAM per modelli LLM locali"), inner);
    title->setObjectName("cardTitle");
    root->addWidget(title);

    auto* subtitle = new QLabel(
        "Stima la RAM necessaria per caricare un modello in base a parametri (miliardi) "
        "e quantizzazione, confrontandola con la RAM del tuo PC.", inner);
    subtitle->setWordWrap(true);
    subtitle->setObjectName("cardDesc");
    root->addWidget(subtitle);

    /* ── Gruppo: modello ── */
    auto* modelBox = new QGroupBox(tr("Modello"), inner);
    auto* modelForm = new QFormLayout(modelBox);
    modelForm->setSpacing(dpiScale(8));

    m_paramsSpin = new QDoubleSpinBox(modelBox);
    m_paramsSpin->setRange(0.1, 2000.0);
    m_paramsSpin->setDecimals(2);
    m_paramsSpin->setSingleStep(0.5);
    m_paramsSpin->setValue(8.0);
    m_paramsSpin->setSuffix(" B");
    modelForm->addRow("Parametri modello:", m_paramsSpin);

    m_quantCombo = new QComboBox(modelBox);
    for (const auto& p : kQuantPresets)
        m_quantCombo->addItem(QString::fromUtf8(p.label));
    m_quantCombo->setCurrentIndex(5); // Q4_K_M, quantizzazione più diffusa
    modelForm->addRow("Quantizzazione:", m_quantCombo);

    m_bitsSpin = new QDoubleSpinBox(modelBox);
    m_bitsSpin->setRange(1.0, 32.0);
    m_bitsSpin->setDecimals(2);
    m_bitsSpin->setSingleStep(0.5);
    m_bitsSpin->setValue(4.5);
    m_bitsSpin->setSuffix(" bit/parametro");
    modelForm->addRow("Bit per parametro:", m_bitsSpin);

    m_overheadSpin = new QSpinBox(modelBox);
    m_overheadSpin->setRange(0, 200);
    m_overheadSpin->setValue(20);
    m_overheadSpin->setSuffix(" %");
    modelForm->addRow("Overhead runtime/contesto:", m_overheadSpin);

    root->addWidget(modelBox);

    /* ── Gruppo: RAM disponibile ── */
    auto* ramBox = new QGroupBox(tr("RAM disponibile sul PC"), inner);
    auto* ramLay = new QVBoxLayout(ramBox);
    ramLay->setSpacing(dpiScale(8));

    auto* presetRow = new QHBoxLayout();
    presetRow->setSpacing(dpiScale(6));
    for (double gb : kRamPresets) addRamPresetButton(presetRow, gb);
    presetRow->addStretch(1);
    ramLay->addLayout(presetRow);

    auto* ramForm = new QFormLayout();
    m_ramSpin = new QDoubleSpinBox(ramBox);
    m_ramSpin->setRange(0.5, 8192.0);
    m_ramSpin->setDecimals(1);
    m_ramSpin->setSingleStep(1.0);
    m_ramSpin->setValue(16.0);
    m_ramSpin->setSuffix(" GB");
    ramForm->addRow("RAM (personalizzata):", m_ramSpin);
    ramLay->addLayout(ramForm);

    root->addWidget(ramBox);

    /* ── Gruppo: risultato ── */
    m_resBox = new QGroupBox(tr("Risultato"), inner);
    auto* resLay = new QVBoxLayout(m_resBox);
    resLay->setSpacing(dpiScale(6));

    m_resModelLbl = new QLabel(m_resBox);
    m_resTotalLbl = new QLabel(m_resBox);
    m_resVerdictLbl = new QLabel(m_resBox);
    m_resVerdictLbl->setWordWrap(true);
    m_resInstLbl = new QLabel(m_resBox);

    resLay->addWidget(m_resModelLbl);
    resLay->addWidget(m_resTotalLbl);
    resLay->addWidget(m_resVerdictLbl);
    resLay->addWidget(m_resInstLbl);

    auto* note = new QLabel(
        "\xe2\x84\xb9 Stima approssimativa: non tiene conto di architettura esatta, "
        "lunghezza reale del contesto caricato o overhead del sistema operativo. "
        "Aumenta \xe2\x80\x9cOverhead\xe2\x80\x9d per margini più prudenti.",
        m_resBox);
    note->setWordWrap(true);
    note->setObjectName("cardDesc");
    resLay->addWidget(note);

    root->addWidget(m_resBox);
    root->addStretch(1);

    scroll->setWidget(inner);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    connect(m_paramsSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RamCalculatorWidget::recalc);
    connect(m_bitsSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RamCalculatorWidget::recalc);
    connect(m_overheadSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RamCalculatorWidget::recalc);
    connect(m_ramSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RamCalculatorWidget::recalc);
    connect(m_quantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RamCalculatorWidget::onQuantPresetChanged);
    connect(ThemeManager::instance(), &ThemeManager::changed,
            this, &RamCalculatorWidget::recalc);

    recalc();
}

void RamCalculatorWidget::addRamPresetButton(QHBoxLayout* row, double gb)
{
    auto* btn = new QPushButton(QString::number(gb, 'g', 4) + " GB", this);
    connect(btn, &QPushButton::clicked, this, [this, gb]{ m_ramSpin->setValue(gb); });
    row->addWidget(btn);
}

void RamCalculatorWidget::onQuantPresetChanged(int idx)
{
    if (idx < 0 || idx >= int(sizeof(kQuantPresets) / sizeof(kQuantPresets[0])))
        return;
    m_bitsSpin->setValue(kQuantPresets[idx].bits);
    recalc();
}

void RamCalculatorWidget::recalc()
{
    const double paramsB     = m_paramsSpin->value();
    const double bits        = m_bitsSpin->value();
    const double overheadPct = m_overheadSpin->value();
    const double ramAvail    = m_ramSpin->value();

    /* 1 miliardo di parametri × 1 byte = 1 GB decimale */
    const double weightsGb = paramsB * (bits / 8.0);
    const double totalGb   = weightsGb * (1.0 + overheadPct / 100.0);

    const RcColors c = themeColors();

    m_resModelLbl->setText(QString(
        "\xf0\x9f\x93\xa6 Dimensione pesi modello: <b>%1 GB</b>")
        .arg(weightsGb, 0, 'f', 2));

    m_resTotalLbl->setText(QString(
        "\xf0\x9f\xa7\xae RAM stimata necessaria (pesi + overhead %2%): <b>%1 GB</b>")
        .arg(totalGb, 0, 'f', 2).arg(overheadPct, 0, 'f', 0));

    const double delta = ramAvail - totalGb;
    const bool fits = delta >= 0.0;
    const QString color = fits ? c.green : c.red;
    const QString verdict = fits
        ? QString("\xe2\x9c\x85 Ci sta! Margine libero: <b>%1 GB</b>").arg(delta, 0, 'f', 2)
        : QString("\xe2\x9d\x8c Non ci sta \xe2\x80\x94 mancano <b>%1 GB</b>").arg(-delta, 0, 'f', 2);
    m_resVerdictLbl->setText(QString(
        "<span style='color:%1;font-weight:bold;'>%2</span>").arg(color, verdict));

    if (totalGb > 0.0) {
        const int instances = int(ramAvail / totalGb);
        m_resInstLbl->setText(QString(
            "\xf0\x9f\x94\xa2 Istanze parallele stimate nella RAM disponibile: <b>%1</b>")
            .arg(instances));
    } else {
        m_resInstLbl->setText(QString());
    }
}

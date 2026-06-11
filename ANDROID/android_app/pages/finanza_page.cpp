#include "finanza_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFont>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QTextCursor>
#include <QScrollArea>
#include <QScroller>
#include <cmath>

/* ── Sezione IVA ──────────────────────────────────────────────── */
QWidget* FinanzaPage::buildIvaSection()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    auto* importoSpin = new QDoubleSpinBox(w);
    importoSpin->setRange(0.0, 1e8);
    importoSpin->setDecimals(2);
    importoSpin->setSuffix(" \xe2\x82\xac");
    importoSpin->setValue(100.0);
    form->addRow("Importo:", importoSpin);

    auto* aliqCombo = new QComboBox(w);
    aliqCombo->addItem("4% (beni prima necessità)",  4.0);
    aliqCombo->addItem("5% (beni specifici)",        5.0);
    aliqCombo->addItem("10% (aliquota ridotta)",    10.0);
    aliqCombo->addItem("22% (aliquota ordinaria)", 22.0);
    aliqCombo->setCurrentIndex(3);
    form->addRow("Aliquota IVA:", aliqCombo);

    vbox->addLayout(form);

    auto* calcBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\xa2") + " Calcola", w);
    calcBtn->setObjectName("PrimaryBtn");
    calcBtn->setMinimumHeight(44);
    vbox->addWidget(calcBtn);

    auto* resultLbl = new QLabel("", w);
    resultLbl->setObjectName("OutputEdit");
    resultLbl->setWordWrap(true);
    resultLbl->setTextFormat(Qt::RichText);
    vbox->addWidget(resultLbl);
    vbox->addStretch();

    QObject::connect(calcBtn, &QPushButton::clicked, w, [=]() {
        const double imp  = importoSpin->value();
        const double aliq = aliqCombo->currentData().toDouble();
        const double iva  = imp * aliq / 100.0;
        const double tot  = imp + iva;
        const double net  = imp / (1.0 + aliq / 100.0);
        resultLbl->setText(QString(
            "<b>Imponibile:</b> %1 \xe2\x82\xac<br>"
            "<b>IVA (%2%):</b> %3 \xe2\x82\xac<br>"
            "<b>Totale lordo:</b> %4 \xe2\x82\xac<br><br>"
            "<b>Scorporo IVA da totale %4:</b> %5 \xe2\x82\xac")
            .arg(imp, 0,'f',2).arg(aliq,0,'f',0)
            .arg(iva, 0,'f',2).arg(tot,0,'f',2).arg(net,0,'f',2));
    });
    return w;
}

/* ── Sezione IRPEF ───────────────────────────────────────────── */
QWidget* FinanzaPage::buildIrpefSection()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);

    auto* note = new QLabel(
        "Scaglioni IRPEF 2024: 0-28k 23% · 28-50k 35% · >50k 43%\n"
        "Stima indicativa. Non include detrazioni o addizionali.", w);
    note->setWordWrap(true);
    note->setObjectName("StatusLabel");
    vbox->addWidget(note);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    auto* ralSpin = new QDoubleSpinBox(w);
    ralSpin->setRange(0.0, 1e7);
    ralSpin->setDecimals(0);
    ralSpin->setSuffix(" \xe2\x82\xac/anno");
    ralSpin->setValue(30000.0);
    form->addRow("Reddito annuo:", ralSpin);
    vbox->addLayout(form);

    auto* calcBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\xa2") + " Calcola IRPEF", w);
    calcBtn->setObjectName("PrimaryBtn");
    calcBtn->setMinimumHeight(44);
    vbox->addWidget(calcBtn);

    auto* resultLbl = new QLabel("", w);
    resultLbl->setObjectName("OutputEdit");
    resultLbl->setWordWrap(true);
    resultLbl->setTextFormat(Qt::RichText);
    vbox->addWidget(resultLbl);
    vbox->addStretch();

    QObject::connect(calcBtn, &QPushButton::clicked, w, [=]() {
        const double ral = ralSpin->value();
        double irpef = 0.0;
        if (ral <= 28000.0) {
            irpef = ral * 0.23;
        } else if (ral <= 50000.0) {
            irpef = 28000.0 * 0.23 + (ral - 28000.0) * 0.35;
        } else {
            irpef = 28000.0 * 0.23 + 22000.0 * 0.35 + (ral - 50000.0) * 0.43;
        }
        const double netto     = ral - irpef;
        const double mensile   = netto / 13.0;
        const double aliqMedia = ral > 0 ? irpef / ral * 100.0 : 0.0;
        resultLbl->setText(QString(
            "<b>RAL:</b> %1 \xe2\x82\xac<br>"
            "<b>IRPEF lorda:</b> %2 \xe2\x82\xac<br>"
            "<b>Aliquota media:</b> %3%<br>"
            "<b>Netto annuo:</b> %4 \xe2\x82\xac<br>"
            "<b>Netto mensile (/13):</b> %5 \xe2\x82\xac<br>"
            "<i>Senza detrazioni lavoratore/carichi famiglia</i>")
            .arg(ral,0,'f',0).arg(irpef,0,'f',0)
            .arg(aliqMedia,0,'f',1).arg(netto,0,'f',0).arg(mensile,0,'f',0));
    });
    return w;
}

/* ── Sezione TFR ─────────────────────────────────────────────── */
QWidget* FinanzaPage::buildTfrSection()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);

    auto* note = new QLabel(
        "TFR = RAL \xc3\xb7 13.5 per anno (art. 2120 c.c.)\n"
        "Rivalutazione annua: 1.5% + 75% indice ISTAT (stima 2% totale)\n"
        "Tassazione: separata, aliquota media storica ~23%.", w);
    note->setWordWrap(true);
    note->setObjectName("StatusLabel");
    vbox->addWidget(note);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    auto* ralSpin = new QDoubleSpinBox(w);
    ralSpin->setRange(0.0, 1e7);
    ralSpin->setDecimals(0);
    ralSpin->setSuffix(" \xe2\x82\xac/anno");
    ralSpin->setValue(28000.0);
    form->addRow("RAL:", ralSpin);

    auto* anniSpin = new QSpinBox(w);
    anniSpin->setRange(1, 50);
    anniSpin->setValue(10);
    anniSpin->setSuffix(" anni");
    form->addRow("Anni di servizio:", anniSpin);

    vbox->addLayout(form);

    auto* calcBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\xa2") + " Calcola TFR", w);
    calcBtn->setObjectName("PrimaryBtn");
    calcBtn->setMinimumHeight(44);
    vbox->addWidget(calcBtn);

    auto* resultLbl = new QLabel("", w);
    resultLbl->setObjectName("OutputEdit");
    resultLbl->setWordWrap(true);
    resultLbl->setTextFormat(Qt::RichText);
    vbox->addWidget(resultLbl);
    vbox->addStretch();

    QObject::connect(calcBtn, &QPushButton::clicked, w, [=]() {
        const double ral  = ralSpin->value();
        const int    anni = anniSpin->value();
        const double quotaAnnua = ral / 13.5;
        /* Rivalutazione composta al tasso medio 2% annuo */
        double tfrLordo = 0.0;
        for (int y = 1; y <= anni; ++y)
            tfrLordo += quotaAnnua * std::pow(1.02, anni - y);
        const double tfrNetto23 = tfrLordo * 0.77;
        const double tfrNetto17 = tfrLordo * 0.83;
        resultLbl->setText(QString(
            "<b>Quote maturate:</b> %1 \xe2\x82\xac/anno<br>"
            "<b>TFR lordo rivalutato (%2 anni):</b> %3 \xe2\x82\xac<br>"
            "<b>TFR netto (aliq. 23%):</b> %4 \xe2\x82\xac<br>"
            "<b>TFR netto (aliq. 17% min):</b> %5 \xe2\x82\xac<br>"
            "<i>Stima indicativa — rivalutazione tasso 2%/anno</i>")
            .arg(quotaAnnua,0,'f',0).arg(anni)
            .arg(tfrLordo,0,'f',0).arg(tfrNetto23,0,'f',0).arg(tfrNetto17,0,'f',0));
    });
    return w;
}

/* ── Sezione Chat AI Finanza ─────────────────────────────────── */
QWidget* FinanzaPage::buildChatSection()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    m_chatOutput = new QTextEdit(w);
    m_chatOutput->setReadOnly(true);
    m_chatOutput->setObjectName("OutputEdit");
    m_chatOutput->setPlaceholderText("Fai domande su finanza, tasse, investimenti...");
    {
        QFont f = m_chatOutput->font();
        f.setPointSize(11);
        m_chatOutput->setFont(f);
    }
    QScroller::grabGesture(m_chatOutput->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_chatOutput, 1);

    auto* inputRow = new QHBoxLayout;
    inputRow->setSpacing(6);
    m_chatInput = new QTextEdit(w);
    m_chatInput->setFixedHeight(64);
    m_chatInput->setPlaceholderText("Domanda su finanza, tasse, investimenti...");
    m_chatInput->setObjectName("InputEdit");
    inputRow->addWidget(m_chatInput, 1);

    auto* btnCol = new QVBoxLayout;
    m_chatSendBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9e\xa4") + " Chiedi", w);
    m_chatSendBtn->setObjectName("PrimaryBtn");
    m_chatSendBtn->setMinimumHeight(30);
    btnCol->addWidget(m_chatSendBtn);

    m_chatStopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9") + " Stop", w);
    m_chatStopBtn->setObjectName("DangerBtn");
    m_chatStopBtn->setMinimumHeight(30);
    m_chatStopBtn->setVisible(false);
    btnCol->addWidget(m_chatStopBtn);
    inputRow->addLayout(btnCol);
    vbox->addLayout(inputRow);

    m_chatStatus = new QLabel("", w);
    m_chatStatus->setObjectName("StatusLabel");
    vbox->addWidget(m_chatStatus);

    connect(m_chatSendBtn, &QPushButton::clicked, this, &FinanzaPage::onChatSend);
    connect(m_chatStopBtn, &QPushButton::clicked, m_ai, &AiClient::abort);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
FinanzaPage::FinanzaPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    /* Titolo */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x92\xb0") + " Finanza \xe2\x80\x94 Calcolatori & AI", this);
    {
        QFont f = title->font();
        f.setPointSize(14);
        f.setBold(true);
        title->setFont(f);
    }
    vbox->addWidget(title);

    /* Selettore sezione */
    m_secCombo = new QComboBox(this);
    m_secCombo->addItem(QString::fromUtf8("\xf0\x9f\x92\xb3") + " IVA");
    m_secCombo->addItem(QString::fromUtf8("\xf0\x9f\x92\xb8") + " IRPEF");
    m_secCombo->addItem(QString::fromUtf8("\xf0\x9f\x92\xbc") + " TFR");
    m_secCombo->addItem(QString::fromUtf8("\xf0\x9f\xa4\x96") + " Chat AI Finanza");
    m_secCombo->setMinimumHeight(44);
    vbox->addWidget(m_secCombo);

    /* Stack sezioni */
    m_secStack = new QStackedWidget(this);
    m_secStack->addWidget(buildIvaSection());
    m_secStack->addWidget(buildIrpefSection());
    m_secStack->addWidget(buildTfrSection());
    m_secStack->addWidget(buildChatSection());
    vbox->addWidget(m_secStack, 1);

    connect(m_secCombo,  qOverload<int>(&QComboBox::currentIndexChanged),
            this, &FinanzaPage::onSectionChanged);

    connect(m_ai, &AiClient::token,    this, &FinanzaPage::onToken);
    connect(m_ai, &AiClient::finished, this, &FinanzaPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &FinanzaPage::onAiError);
    connect(m_ai, &AiClient::aborted,  this, &FinanzaPage::onAborted);
}

void FinanzaPage::onSectionChanged(int idx)
{
    m_secStack->setCurrentIndex(idx);
}

void FinanzaPage::onCalcIva()   {} /* placeholder — logica in lambda della sezione */
void FinanzaPage::onCalcIrpef() {}
void FinanzaPage::onCalcTfr()   {}

static constexpr const char* kSysFinanza =
    "Sei un consulente finanziario italiano esperto. "
    "Rispondi in modo chiaro, preciso e conciso su finanza personale, "
    "tasse, investimenti e previdenza in Italia. "
    "Usa cifre reali e normativa vigente. Usa Markdown.";

void FinanzaPage::onChatSend()
{
    if (!m_chatInput) return;
    const QString msg = m_chatInput->toPlainText().trimmed();
    if (msg.isEmpty()) return;
    setChatBusy(true);
    m_chatOutput->clear();
    m_ai->chat(kSysFinanza, msg);
}

void FinanzaPage::onToken(const QString& t)
{
    if (!m_chatOutput) return;
    m_chatOutput->moveCursor(QTextCursor::End);
    m_chatOutput->insertPlainText(t);
    m_chatOutput->moveCursor(QTextCursor::End);
}

void FinanzaPage::onFinished(const QString&)
{
    setChatBusy(false);
    if (m_chatStatus) m_chatStatus->setText("");
}

void FinanzaPage::onAiError(const QString& msg)
{
    setChatBusy(false);
    if (m_chatStatus)
        m_chatStatus->setText(QString::fromUtf8("\xe2\x9d\x8c") + " " + msg);
}

void FinanzaPage::onAborted()
{
    setChatBusy(false);
    if (m_chatStatus) m_chatStatus->setText("Risposta interrotta.");
}

void FinanzaPage::setChatBusy(bool busy)
{
    if (m_chatSendBtn) m_chatSendBtn->setEnabled(!busy);
    if (m_chatStopBtn) m_chatStopBtn->setVisible(busy);
    if (m_chatInput)   m_chatInput->setEnabled(!busy);
    if (busy && m_chatStatus)
        m_chatStatus->setText(
            QString::fromUtf8("\xe2\x8f\xb3") + " Consulente sta rispondendo...");
}

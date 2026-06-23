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

/* ── Sezione 730 ─────────────────────────────────────────────── */
QWidget* FinanzaPage::build730Section()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);

    auto* note = new QLabel(
        "Modello 730 — stima IRPEF lavoratori dipendenti/pensionati\n"
        "Scaglioni 2024. Detrazione lavoro dip. art.13 TUIR.", w);
    note->setWordWrap(true);
    note->setObjectName("StatusLabel");
    vbox->addWidget(note);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    auto* ralSpin = new QDoubleSpinBox(w);
    ralSpin->setRange(0, 1e7); ralSpin->setDecimals(0);
    ralSpin->setSuffix(" \xe2\x82\xac"); ralSpin->setValue(30000.0);
    form->addRow("Reddito complessivo:", ralSpin);

    auto* ritenuteSpin = new QDoubleSpinBox(w);
    ritenuteSpin->setRange(0, 1e7); ritenuteSpin->setDecimals(0);
    ritenuteSpin->setSuffix(" \xe2\x82\xac"); ritenuteSpin->setValue(5000.0);
    form->addRow("Ritenute subite (CU):", ritenuteSpin);

    auto* speseDetSpin = new QDoubleSpinBox(w);
    speseDetSpin->setRange(0, 1e6); speseDetSpin->setDecimals(0);
    speseDetSpin->setSuffix(" \xe2\x82\xac"); speseDetSpin->setValue(0.0);
    form->addRow("Spese detraibili 19%:", speseDetSpin);

    auto* cariciSpin = new QSpinBox(w);
    cariciSpin->setRange(0, 10); cariciSpin->setValue(0);
    cariciSpin->setSuffix(" figli");
    form->addRow("Figli a carico:", cariciSpin);

    vbox->addLayout(form);

    auto* calcBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\xa2") + " Calcola 730", w);
    calcBtn->setObjectName("PrimaryBtn"); calcBtn->setMinimumHeight(44);
    vbox->addWidget(calcBtn);

    auto* resultLbl = new QLabel("", w);
    resultLbl->setObjectName("OutputEdit"); resultLbl->setWordWrap(true);
    resultLbl->setTextFormat(Qt::RichText);
    vbox->addWidget(resultLbl); vbox->addStretch();

    QObject::connect(calcBtn, &QPushButton::clicked, w, [=]() {
        const double ral      = ralSpin->value();
        const double ritenute = ritenuteSpin->value();
        const double spese    = speseDetSpin->value();
        const int    figli    = cariciSpin->value();

        /* IRPEF lorda scaglioni 2024 */
        double irpef = 0.0;
        if (ral <= 28000.0)       irpef = ral * 0.23;
        else if (ral <= 50000.0)  irpef = 6440.0 + (ral - 28000.0) * 0.35;
        else                      irpef = 14140.0 + (ral - 50000.0) * 0.43;

        /* Detrazione lavoro dipendente art.13 (semplificata) */
        double detLav = 0.0;
        if (ral <= 15000.0)      detLav = 1955.0;
        else if (ral <= 28000.0) detLav = 1910.0 + 1190.0 * (28000.0 - ral) / 13000.0;
        else if (ral <= 50000.0) detLav = 1910.0 * (50000.0 - ral) / 22000.0;

        /* Detrazione figli a carico (950 €/figlio semplificato) */
        double detFigli = figli * 950.0;

        /* Credito spese detraibili 19% */
        double credSpese = spese * 0.19;

        double irpefNetta = irpef - detLav - detFigli - credSpese;
        if (irpefNetta < 0) irpefNetta = 0;

        double saldo = irpefNetta - ritenute;

        resultLbl->setText(QString(
            "<b>IRPEF lorda:</b> %1 \xe2\x82\xac<br>"
            "<b>Detraz. lav. dip.:</b> %2 \xe2\x82\xac<br>"
            "<b>Detraz. figli:</b> %3 \xe2\x82\xac<br>"
            "<b>Credito spese 19%%:</b> %4 \xe2\x82\xac<br>"
            "<b>IRPEF netta:</b> %5 \xe2\x82\xac<br>"
            "<b>%6:</b> <b style='color:%7'>%8 \xe2\x82\xac</b><br>"
            "<i>Stima orientativa — consulta un CAF</i>")
            .arg(irpef,0,'f',0).arg(detLav,0,'f',0).arg(detFigli,0,'f',0)
            .arg(credSpese,0,'f',0).arg(irpefNetta,0,'f',0)
            .arg(saldo >= 0 ? "Saldo a debito (versi)" : "Rimborso")
            .arg(saldo >= 0 ? "#ef4444" : "#22c55e")
            .arg(std::abs(saldo),0,'f',0));
    });
    return w;
}

/* ── Sezione P.IVA Forfettario ───────────────────────────────── */
QWidget* FinanzaPage::buildPivaSection()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);

    auto* note = new QLabel(
        "Regime forfettario — imposta sostitutiva 15% (5% primo quinquennio)\n"
        "Soglia ricavi: 85.000 \xe2\x82\xac. Contributi INPS gestione separata ~26.23%.", w);
    note->setWordWrap(true); note->setObjectName("StatusLabel");
    vbox->addWidget(note);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    auto* fatSpin = new QDoubleSpinBox(w);
    fatSpin->setRange(0, 85000); fatSpin->setDecimals(0);
    fatSpin->setSuffix(" \xe2\x82\xac"); fatSpin->setValue(30000.0);
    form->addRow("Fatturato annuo:", fatSpin);

    auto* coefCombo = new QComboBox(w);
    coefCombo->addItem("67% — Commercio/E-commerce",       67.0);
    coefCombo->addItem("78% — Attività professionali",     78.0);
    coefCombo->addItem("54% — Servizi alloggio/ristoraz.", 54.0);
    coefCombo->addItem("40% — Costruzioni/impiantistica",  40.0);
    coefCombo->addItem("86% — Professioni sanitarie",      86.0);
    coefCombo->setCurrentIndex(1);
    form->addRow("Coefficiente redditività:", coefCombo);

    auto* quinqCombo = new QComboBox(w);
    quinqCombo->addItem("No (aliquota 15%)",  15.0);
    quinqCombo->addItem("Sì (aliquota 5% — primo quinquennio)", 5.0);
    form->addRow("Nuova attività?", quinqCombo);

    auto* inpsCombo = new QComboBox(w);
    inpsCombo->addItem("Gestione separata 26.23%", 26.23);
    inpsCombo->addItem("Artigiani/Commercianti 24%", 24.0);
    inpsCombo->addItem("Cassa professionale (varia)", 0.0);
    form->addRow("Contributi INPS:", inpsCombo);

    vbox->addLayout(form);

    auto* calcBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\xa2") + " Calcola P.IVA", w);
    calcBtn->setObjectName("PrimaryBtn"); calcBtn->setMinimumHeight(44);
    vbox->addWidget(calcBtn);

    auto* resultLbl = new QLabel("", w);
    resultLbl->setObjectName("OutputEdit"); resultLbl->setWordWrap(true);
    resultLbl->setTextFormat(Qt::RichText);
    vbox->addWidget(resultLbl); vbox->addStretch();

    QObject::connect(calcBtn, &QPushButton::clicked, w, [=]() {
        const double fat       = fatSpin->value();
        const double coef      = coefCombo->currentData().toDouble() / 100.0;
        const double aliq      = quinqCombo->currentData().toDouble() / 100.0;
        const double inpsPerc  = inpsCombo->currentData().toDouble() / 100.0;

        const double imponibile = fat * coef;
        const double imposta    = imponibile * aliq;
        const double inps       = inpsPerc > 0 ? imponibile * inpsPerc : 0.0;
        const double netto      = fat - imposta - inps;
        const double mensile    = netto / 12.0;
        const double aliqEff    = fat > 0 ? (imposta + inps) / fat * 100.0 : 0.0;

        resultLbl->setText(QString(
            "<b>Fatturato:</b> %1 \xe2\x82\xac<br>"
            "<b>Reddito imponibile (%2%%  coeff.):</b> %3 \xe2\x82\xac<br>"
            "<b>Imposta sostitutiva %4%%:</b> %5 \xe2\x82\xac<br>"
            "<b>Contributi INPS (%6%%):</b> %7 \xe2\x82\xac<br>"
            "<b>Netto annuo stimato:</b> %8 \xe2\x82\xac<br>"
            "<b>Netto mensile:</b> %9 \xe2\x82\xac<br>"
            "<b>Carico fiscale eff.:</b> %10%%<br>"
            "<i>Stima indicativa. Consulta un commercialista.</i>")
            .arg(fat,0,'f',0)
            .arg(coef*100,0,'f',0).arg(imponibile,0,'f',0)
            .arg(aliq*100,0,'f',0).arg(imposta,0,'f',0)
            .arg(inpsPerc*100,0,'f',1).arg(inps,0,'f',0)
            .arg(netto,0,'f',0).arg(mensile,0,'f',0)
            .arg(aliqEff,0,'f',1));
    });
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
    m_secCombo->addItem(QString::fromUtf8("\xf0\x9f\x93\x84") + " Modello 730");
    m_secCombo->addItem(QString::fromUtf8("\xf0\x9f\x8f\xa2") + " P.IVA Forfettario");
    m_secCombo->setMinimumHeight(44);
    vbox->addWidget(m_secCombo);

    /* Stack sezioni */
    m_secStack = new QStackedWidget(this);
    m_secStack->addWidget(buildIvaSection());
    m_secStack->addWidget(buildIrpefSection());
    m_secStack->addWidget(buildTfrSection());
    m_secStack->addWidget(buildChatSection());
    m_secStack->addWidget(build730Section());
    m_secStack->addWidget(buildPivaSection());
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

void FinanzaPage::onCalcIva()   {} /* logica in lambda della sezione */
void FinanzaPage::onCalcIrpef() {}
void FinanzaPage::onCalcTfr()   {}
void FinanzaPage::onCalc730()   {}
void FinanzaPage::onCalcPiva()  {}

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

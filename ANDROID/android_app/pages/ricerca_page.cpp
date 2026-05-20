#include "ricerca_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QApplication>
#include <QClipboard>
#include <QTextCursor>
#include <QTimer>
#include <QScroller>
#include <QScrollerProperties>

static void applyTouchScroll(QScrollArea* sa)
{
    QScroller::grabGesture(sa->viewport(), QScroller::TouchGesture);
    QScroller* qs = QScroller::scroller(sa->viewport());
    QScrollerProperties sp = qs->scrollerProperties();
    sp.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor, 0.0);
    sp.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.0);
    sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    qs->setScrollerProperties(sp);
}

/* ══════════════════════════════════════════════════════════════
   Prompt di sistema per ogni tipo di documento
   ══════════════════════════════════════════════════════════════ */
static QString buildSystemPrompt(int tipo, const QString& argomento,
                                 const QString& keyword)
{
    const QString kw = keyword.isEmpty() ? "" : " (parole chiave: " + keyword + ")";
    switch (tipo) {
    case 0: /* Paper Scientifico */
        return
            "Sei un ricercatore scientifico accademico esperto. "
            "Scrivi un abstract + introduzione completa per un paper scientifico su: "
            + argomento + kw + ".\n"
            "Struttura:\n"
            "## Abstract\n"
            "## 1. Introduzione\n"
            "## 2. Stato dell'Arte\n"
            "## 3. Metodologia proposta\n"
            "## 4. Risultati attesi\n"
            "## Riferimenti bibliografici (5-8 simulati in formato APA)\n"
            "Usa un registro accademico formale in italiano.";

    case 1: /* Brevetto */
        return
            "Sei un consulente brevetti (IP attorney). "
            "Redigi una bozza di domanda di brevetto per: "
            + argomento + kw + ".\n"
            "Struttura:\n"
            "## Titolo dell'invenzione\n"
            "## Campo tecnico\n"
            "## Problemi tecnici risolti\n"
            "## Descrizione dell'invenzione\n"
            "## Rivendicazioni (almeno 3: indipendente + dipendenti)\n"
            "## Breve descrizione dei disegni\n"
            "Usa linguaggio tecnico-legale preciso in italiano.";

    case 2: /* Doc Tecnico */
        return
            "Sei un technical writer esperto. "
            "Scrivi una documentazione tecnica completa per: "
            + argomento + kw + ".\n"
            "Struttura:\n"
            "## Panoramica\n"
            "## Requisiti\n"
            "## Architettura / Design\n"
            "## Installazione / Configurazione\n"
            "## API / Interfacce principali\n"
            "## Esempi d'uso\n"
            "## Troubleshooting\n"
            "## Changelog\n"
            "Usa Markdown con esempi di codice dove appropriato.";

    case 3: /* Analisi Letteratura */
        return
            "Sei un analista di letteratura scientifica. "
            "Analizza lo stato dell'arte e la letteratura esistente su: "
            + argomento + kw + ".\n"
            "Struttura:\n"
            "## Sintesi del campo\n"
            "## Principali filoni di ricerca\n"
            "## Lavori fondamentali (autori e anni simulati)\n"
            "## Gap nella letteratura\n"
            "## Trend emergenti\n"
            "## Conclusioni e direzioni future\n"
            "Usa un registro accademico in italiano.";

    case 4: /* Report di Mercato */
    default:
        return
            "Sei un analista di mercato e business intelligence. "
            "Scrivi un report di mercato dettagliato su: "
            + argomento + kw + ".\n"
            "Struttura:\n"
            "## Executive Summary\n"
            "## Panoramica di mercato\n"
            "## Segmentazione e target\n"
            "## Analisi competitiva (almeno 3 player)\n"
            "## SWOT Analysis\n"
            "## Tendenze e opportunità\n"
            "## Raccomandazioni strategiche\n"
            "Usa dati plausibili e linguaggio professionale in italiano.";
    }
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
RicercaPage::RicercaPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    setObjectName("RicercaPage");

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    applyTouchScroll(scroll);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);
    scroll->setWidget(inner);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    /* ── Titolo ── */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\xac  Ricerca & Sviluppo"), inner);
    QFont tf = title->font();
    tf.setPointSize(15); tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    /* ── Tipo documento ── */
    auto* tipoGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x84  Tipo di documento"), inner);
    tipoGroup->setObjectName("SettingsGroup");
    auto* tipoVbox = new QVBoxLayout(tipoGroup);

    m_tipoCombo = new QComboBox(inner);
    m_tipoCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x8e\x93") + "  Paper Scientifico", 0);
    m_tipoCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x94\x92") + "  Brevetto / Patent", 1);
    m_tipoCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x93\x96") + "  Documentazione Tecnica", 2);
    m_tipoCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x93\x9a") + "  Analisi Letteratura", 3);
    m_tipoCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x93\x8a") + "  Report di Mercato", 4);
    m_tipoCombo->setMinimumHeight(48);
    tipoVbox->addWidget(m_tipoCombo);

    m_hintLbl = new QLabel("", inner);
    m_hintLbl->setWordWrap(true);
    m_hintLbl->setStyleSheet("color:#8890a8; font-size:12px;");
    tipoVbox->addWidget(m_hintLbl);
    vbox->addWidget(tipoGroup);

    /* ── Argomento ── */
    auto* argGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x9d  Argomento"), inner);
    argGroup->setObjectName("SettingsGroup");
    auto* argVbox = new QVBoxLayout(argGroup);

    m_argEdit = new QTextEdit(inner);
    m_argEdit->setPlaceholderText(
        "Descrivi l'argomento in dettaglio.\n"
        "Più dettagli fornisci, migliore sarà il risultato.\n\n"
        "Esempio: \"Reti neurali convoluzionali per la classificazione "
        "di immagini mediche in ambito oncologico\"");
    m_argEdit->setMinimumHeight(100);
    m_argEdit->setMaximumHeight(160);
    argVbox->addWidget(m_argEdit);

    m_keywordEdit = new QLineEdit(inner);
    m_keywordEdit->setPlaceholderText(
        "Parole chiave opzionali (es: deep learning, CNN, medical imaging)");
    m_keywordEdit->setMinimumHeight(44);
    argVbox->addWidget(m_keywordEdit);
    vbox->addWidget(argGroup);

    /* ── Bottoni azione ── */
    auto* btnRow = new QHBoxLayout;
    m_generateBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9c\xa8  Genera"), inner);
    m_generateBtn->setObjectName("PrimaryBtn");
    m_generateBtn->setMinimumHeight(56);
    QFont gf = m_generateBtn->font();
    gf.setBold(true); gf.setPointSize(14);
    m_generateBtn->setFont(gf);

    m_stopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9  Stop"), inner);
    m_stopBtn->setObjectName("StopBtn");
    m_stopBtn->setMinimumHeight(56);
    m_stopBtn->setEnabled(false);

    m_copyBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x8b  Copia"), inner);
    m_copyBtn->setObjectName("SecondaryBtn");
    m_copyBtn->setMinimumHeight(56);

    btnRow->addWidget(m_generateBtn, 3);
    btnRow->addWidget(m_stopBtn, 1);
    btnRow->addWidget(m_copyBtn, 2);
    vbox->addLayout(btnRow);

    /* ── Status / progress ── */
    m_progressBar = new QProgressBar(inner);
    m_progressBar->setRange(0, 0);
    m_progressBar->setFixedHeight(4);
    m_progressBar->setVisible(false);
    vbox->addWidget(m_progressBar);

    m_statusLbl = new QLabel("", inner);
    m_statusLbl->setStyleSheet("color:#8890a8; font-size:12px;");
    m_statusLbl->setVisible(false);
    vbox->addWidget(m_statusLbl);

    /* ── Output ── */
    auto* outGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x84  Risultato"), inner);
    outGroup->setObjectName("SettingsGroup");
    auto* outVbox = new QVBoxLayout(outGroup);

    m_output = new QTextEdit(inner);
    m_output->setReadOnly(true);
    m_output->setMinimumHeight(300);
    m_output->setPlaceholderText(
        "Il documento generato apparirà qui in streaming.\n\n"
        "Seleziona il tipo, descrivi l'argomento e premi Genera.");
    outVbox->addWidget(m_output);
    vbox->addWidget(outGroup, 1);

    vbox->addStretch();

    /* ── Connessioni ── */
    connect(m_tipoCombo,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RicercaPage::onTipoChanged);
    connect(m_generateBtn,  &QPushButton::clicked,
            this, &RicercaPage::onGenerateClicked);
    connect(m_stopBtn,      &QPushButton::clicked,
            this, &RicercaPage::onStopClicked);
    connect(m_copyBtn,      &QPushButton::clicked,
            this, &RicercaPage::onCopyClicked);

    updateHint(0);
}

/* ── Hints per ogni tipo ──────────────────────────────────────── */
void RicercaPage::updateHint(int idx)
{
    static const char* hints[] = {
        "Abstract + introduzione + stato dell'arte + metodologia + riferimenti",
        "Titolo + campo tecnico + descrizione + rivendicazioni + disegni",
        "Panoramica + architettura + API + esempi + troubleshooting",
        "Sintesi + filoni di ricerca + gap + trend emergenti",
        "Executive summary + analisi competitiva + SWOT + raccomandazioni",
    };
    if (idx >= 0 && idx < 5)
        m_hintLbl->setText(hints[idx]);
}

void RicercaPage::onTipoChanged(int idx) { updateHint(idx); }

/* ── Genera ───────────────────────────────────────────────────── */
void RicercaPage::onGenerateClicked()
{
    const QString argomento = m_argEdit->toPlainText().trimmed();
    if (argomento.isEmpty()) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + "  Inserisci un argomento prima di generare.");
        return;
    }
    if (m_busy || m_ai->busy()) return;

    m_busy = true;
    m_fullText.clear();
    m_output->clear();

    m_generateBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_copyBtn->setEnabled(false);
    m_progressBar->setVisible(true);
    m_statusLbl->setText(
        QString::fromUtf8("\xf0\x9f\x94\x84") + "  Generazione in corso...");
    m_statusLbl->setVisible(true);

    const int    tipo      = m_tipoCombo->currentIndex();
    const QString keyword  = m_keywordEdit->text().trimmed();
    const QString sys      = buildSystemPrompt(tipo, argomento, keyword);
    const QString userMsg  = "Genera il documento su: " + argomento;

    m_tokConn = connect(m_ai, &AiClient::token,
                        this, &RicercaPage::onToken);
    m_finConn = connect(m_ai, &AiClient::finished,
                        this, &RicercaPage::onFinished);
    m_errConn = connect(m_ai, &AiClient::error,
                        this, &RicercaPage::onError);

    m_ai->chat(sys, userMsg);
}

void RicercaPage::onStopClicked()
{
    m_ai->abort();
}

void RicercaPage::onToken(const QString& t)
{
    if (!m_busy) return;
    m_fullText += t;
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(t);
    m_output->moveCursor(QTextCursor::End);
}

void RicercaPage::onFinished(const QString& full)
{
    if (!m_busy) return;
    disconnect(m_tokConn);
    disconnect(m_finConn);
    disconnect(m_errConn);

    if (!full.isEmpty()) m_fullText = full;
    m_busy = false;
    m_generateBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_copyBtn->setEnabled(true);
    m_progressBar->setVisible(false);
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9c\x85") + "  Documento generato!");
}

void RicercaPage::onError(const QString& e)
{
    if (!m_busy) return;
    disconnect(m_tokConn);
    disconnect(m_finConn);
    disconnect(m_errConn);

    m_busy = false;
    m_generateBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_copyBtn->setEnabled(false);
    m_progressBar->setVisible(false);
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9d\x8c") + "  Errore: " + e);
    m_output->append(
        "\n\n" + QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + e);
}

void RicercaPage::onCopyClicked()
{
    const QString txt = m_output->toPlainText().trimmed();
    if (txt.isEmpty()) return;
    QApplication::clipboard()->setText(txt);
    m_copyBtn->setText(
        QString::fromUtf8("\xe2\x9c\x85  Copiato!"));
    QTimer::singleShot(2000, this, &RicercaPage::onCopyRestore);
}

void RicercaPage::onCopyRestore()
{
    if (m_copyBtn)
        m_copyBtn->setText(
            QString::fromUtf8("\xf0\x9f\x93\x8b  Copia"));
}

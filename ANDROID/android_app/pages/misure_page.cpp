#include "misure_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QLabel>
#include <QFont>
#include <QTextCursor>
#include <QScrollBar>
#include <cmath>

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
MisurePage::MisurePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);

    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x90 Misure & Fotogrammetria"), inner);
    QFont tf = title->font();
    tf.setPointSize(14);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    /* ─────────────────────────────────────────────────────
       Sezione 1 — Calcolatore dimensioni stanza
       ───────────────────────────────────────────────────── */
    auto* calcGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x8f Calcolatore Stanza"), inner);
    auto* calcForm = new QFormLayout(calcGroup);
    calcForm->setSpacing(8);

    m_lungSpin = new QDoubleSpinBox(inner);
    m_lungSpin->setRange(0.1, 999.0);
    m_lungSpin->setValue(5.0);
    m_lungSpin->setSuffix(" m");
    m_lungSpin->setDecimals(2);
    calcForm->addRow("Lunghezza:", m_lungSpin);

    m_largSpin = new QDoubleSpinBox(inner);
    m_largSpin->setRange(0.1, 999.0);
    m_largSpin->setValue(4.0);
    m_largSpin->setSuffix(" m");
    m_largSpin->setDecimals(2);
    calcForm->addRow("Larghezza:", m_largSpin);

    m_altSpin = new QDoubleSpinBox(inner);
    m_altSpin->setRange(0.1, 30.0);
    m_altSpin->setValue(2.7);
    m_altSpin->setSuffix(" m");
    m_altSpin->setDecimals(2);
    calcForm->addRow("Altezza soffitto:", m_altSpin);

    m_sovrasSpin = new QDoubleSpinBox(inner);
    m_sovrasSpin->setRange(0.0, 30.0);
    m_sovrasSpin->setValue(10.0);
    m_sovrasSpin->setSuffix(" %");
    m_sovrasSpin->setDecimals(0);
    calcForm->addRow("Scarto piastrelle:", m_sovrasSpin);

    auto* btnCalc = new QPushButton(
        QString::fromUtf8("\xe2\x9a\x99\xef\xb8\x8f Calcola"), inner);
    btnCalc->setMinimumHeight(44);
    calcForm->addRow(btnCalc);

    m_resultLbl = new QLabel("", inner);
    m_resultLbl->setWordWrap(true);
    m_resultLbl->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    calcForm->addRow(m_resultLbl);

    vbox->addWidget(calcGroup);

    /* ─────────────────────────────────────────────────────
       Sezione 2 — Analisi AI
       ───────────────────────────────────────────────────── */
    auto* aiGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\xa4\x96 Analisi AI"), inner);
    auto* aiVbox = new QVBoxLayout(aiGroup);
    aiVbox->setSpacing(6);

    m_aiAction = new QComboBox(inner);
    m_aiAction->addItems({
        "Stima dimensioni da descrizione",
        "Consigli materiali / ristrutturazione",
        "Calcola costo ristrutturazione",
        "Fotogrammetria / misure da foto (descrivi)",
        "Confronto standard edilizi italiani",
    });
    aiVbox->addWidget(m_aiAction);

    m_aiInput = new QTextEdit(inner);
    m_aiInput->setPlaceholderText(
        "Descrivi la stanza o incolla i risultati del calcolatore.\n"
        "Esempio: «Stanza 5×4×2.7m con 2 finestre e 1 porta. "
        "Devo tinteggiare le pareti. Che quantità di vernice mi serve?»\n\n"
        "Per fotogrammetria: descrivi la foto "
        "(oggetti di riferimento, prospettiva, distanza stimata).");
    m_aiInput->setFixedHeight(100);
    aiVbox->addWidget(m_aiInput);

    auto* btnRow = new QHBoxLayout;
    m_btnAi = new QPushButton(
        QString::fromUtf8("\xf0\x9f\xa4\x96 Analizza con AI"), inner);
    m_btnAi->setMinimumHeight(44);
    m_btnStop = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9 Stop"), inner);
    m_btnStop->setVisible(false);
    m_btnStop->setFixedWidth(72);
    btnRow->addWidget(m_btnAi, 1);
    btnRow->addWidget(m_btnStop);
    aiVbox->addLayout(btnRow);

    m_statusLbl = new QLabel("", inner);
    m_statusLbl->setVisible(false);
    aiVbox->addWidget(m_statusLbl);

    m_progress = new QProgressBar(inner);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    m_progress->setFixedHeight(4);
    aiVbox->addWidget(m_progress);

    m_aiOutput = new QTextEdit(inner);
    m_aiOutput->setReadOnly(true);
    m_aiOutput->setPlaceholderText("La risposta AI apparirà qui...");
    m_aiOutput->setMinimumHeight(160);
    aiVbox->addWidget(m_aiOutput);

    vbox->addWidget(aiGroup);
    vbox->addStretch();

    scroll->setWidget(inner);

    auto* outerVbox = new QVBoxLayout(this);
    outerVbox->setContentsMargins(0, 0, 0, 0);
    outerVbox->addWidget(scroll);

    /* ── Connessioni ── */
    connect(btnCalc, &QPushButton::clicked, this, &MisurePage::onCalcolaClicked);
    connect(m_btnAi, &QPushButton::clicked, this, &MisurePage::onAiClicked);
    connect(m_btnStop, &QPushButton::clicked, m_ai, &AiClient::abort);
    connect(m_ai, &AiClient::token,    this, &MisurePage::onToken);
    connect(m_ai, &AiClient::finished, this, &MisurePage::onFinished);
    connect(m_ai, &AiClient::error,    this, &MisurePage::onError);
    connect(m_ai, &AiClient::aborted,  this, &MisurePage::onAborted);
}

/* ══════════════════════════════════════════════════════════════
   Calcolatore locale
   ══════════════════════════════════════════════════════════════ */
void MisurePage::onCalcolaClicked()
{
    calcola();
}

void MisurePage::calcola()
{
    const double L  = m_lungSpin->value();
    const double W  = m_largSpin->value();
    const double H  = m_altSpin->value();
    const double sc = m_sovrasSpin->value() / 100.0;

    const double areaFloor  = L * W;
    const double areaWalls  = 2.0 * (L + W) * H;
    const double areaCeil   = areaFloor;
    const double areaTotal  = areaWalls + areaCeil + areaFloor;
    const double volume     = L * W * H;

    /* Vernice: ~10 m² per litro (1 mano), 2 mani standard */
    const double litriPareti  = std::ceil(areaWalls  / 10.0 * 2.0);
    const double litriSoffitto = std::ceil(areaCeil   / 10.0 * 2.0);

    /* Piastrelle: area pavimento + scarto */
    const double mq_piastrelle = areaFloor * (1.0 + sc);

    /* Riscaldamento: volume × 40 W/m³ (stima) */
    const double wattRisc = volume * 40.0;

    QString res;
    res += QString::fromUtf8("\xf0\x9f\x93\x90 <b>Risultati</b><br>");
    res += QString("Pavimento: <b>%1 m²</b><br>").arg(areaFloor, 0, 'f', 2);
    res += QString("Pareti: <b>%1 m²</b><br>").arg(areaWalls, 0, 'f', 2);
    res += QString("Soffitto: <b>%1 m²</b><br>").arg(areaCeil, 0, 'f', 2);
    res += QString("Totale superfici: <b>%1 m²</b><br>").arg(areaTotal, 0, 'f', 2);
    res += QString("Volume: <b>%1 m³</b><br>").arg(volume, 0, 'f', 2);
    res += "<br>";
    res += QString::fromUtf8("\xf0\x9f\x96\x8c\xef\xb8\x8f <b>Vernice pareti</b> (2 mani): <b>%1 L</b><br>")
           .arg(litriPareti, 0, 'f', 0);
    res += QString::fromUtf8("\xf0\x9f\x96\x8c\xef\xb8\x8f <b>Vernice soffitto</b> (2 mani): <b>%1 L</b><br>")
           .arg(litriSoffitto, 0, 'f', 0);
    res += QString::fromUtf8("\xf0\x9f\x9f\xa7 <b>Piastrelle pavimento</b> (+%1%% scarto): <b>%2 m²</b><br>")
           .arg(int(m_sovrasSpin->value()))
           .arg(mq_piastrelle, 0, 'f', 2);
    res += QString::fromUtf8("\xf0\x9f\x94\xa5 <b>Potenza riscaldamento</b> (stima): <b>%1 W</b><br>")
           .arg(wattRisc, 0, 'f', 0);

    m_resultLbl->setTextFormat(Qt::RichText);
    m_resultLbl->setText(res);

    /* Pre-compila il campo AI con un riassunto utile */
    if (m_aiInput->toPlainText().trimmed().isEmpty()) {
        m_aiInput->setPlainText(
            QString("Stanza %1×%2×%3 m.\n"
                    "Pavimento: %4 m², Pareti: %5 m², Volume: %6 m³.\n"
                    "Vernice pareti (2 mani): %7 L. Piastrelle: %8 m².")
            .arg(L, 0, 'f', 2).arg(W, 0, 'f', 2).arg(H, 0, 'f', 2)
            .arg(areaFloor, 0, 'f', 2).arg(areaWalls, 0, 'f', 2).arg(volume, 0, 'f', 2)
            .arg(litriPareti, 0, 'f', 0).arg(mq_piastrelle, 0, 'f', 2));
    }
}

/* ══════════════════════════════════════════════════════════════
   Analisi AI
   ══════════════════════════════════════════════════════════════ */
void MisurePage::onAiClicked()
{
    if (m_busy || m_ai->busy()) {
        m_aiOutput->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " L'AI sta elaborando. Attendi oppure premi Stop.");
        return;
    }
    const QString userMsg = m_aiInput->toPlainText().trimmed();
    if (userMsg.isEmpty()) {
        m_aiOutput->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " Inserisci una descrizione o usa prima il Calcolatore.");
        return;
    }

    static const char* kSysPrompts[] = {
        "Sei un esperto geometra italiano. Stimare le dimensioni di stanza/ambiente dalla "
        "descrizione fornita. Fornisci una stima numerica dettagliata con range min-max. "
        "Rispondi in italiano.",

        "Sei un esperto di edilizia e ristrutturazioni italiane. In base alle misure/descrizione "
        "fornite, consiglia i materiali migliori per pavimenti, pareti e soffitto, con prezzi "
        "indicativi al m² per il mercato italiano. Rispondi in italiano.",

        "Sei un geometra/perito esperto di costi edilizi italiani. Calcola una stima del costo "
        "totale di ristrutturazione in base alle misure e al tipo di intervento descritto. "
        "Considera materiali, manodopera e IVA. Rispondi in italiano.",

        "Sei un esperto di fotogrammetria e misurazione da immagini. L'utente descrive una foto "
        "e vuole stimare le dimensioni degli oggetti o dello spazio. Guida l'utente nel processo "
        "di stima, identificando gli oggetti di riferimento e spiegando come calcolare le misure. "
        "Rispondi in italiano.",

        "Sei un esperto di normativa edilizia italiana (D.M. 5/7/1975, NTC 2018). Confronta le "
        "misure fornite con i requisiti minimi italiani per i vari ambienti (camera, cucina, "
        "bagno, etc.) e segnala eventuali non conformità. Rispondi in italiano.",
    };

    const int idx = m_aiAction->currentIndex();
    const QString sys = (idx >= 0 && idx < 5) ? kSysPrompts[idx] : kSysPrompts[0];

    m_busy = true;
    m_aiOutput->clear();
    m_statusLbl->setText(QString::fromUtf8("\xe2\x8f\xb3") + " Analisi in corso...");
    m_statusLbl->setVisible(true);
    m_progress->setVisible(true);
    m_btnStop->setVisible(true);
    m_btnAi->setEnabled(false);
    m_ai->chat(sys, userMsg);
}

void MisurePage::onToken(const QString& t)
{
    if (!m_busy) return;
    m_aiOutput->moveCursor(QTextCursor::End);
    m_aiOutput->insertPlainText(t);
    m_aiOutput->verticalScrollBar()->setValue(m_aiOutput->verticalScrollBar()->maximum());
}

void MisurePage::onFinished(const QString&)
{
    if (!m_busy) return;
    m_busy = false;
    m_statusLbl->setVisible(false);
    m_progress->setVisible(false);
    m_btnStop->setVisible(false);
    m_btnAi->setEnabled(true);
}

void MisurePage::onError(const QString& e)
{
    if (!m_busy) return;
    m_busy = false;
    m_statusLbl->setVisible(false);
    m_progress->setVisible(false);
    m_btnStop->setVisible(false);
    m_btnAi->setEnabled(true);
    m_aiOutput->setPlainText(QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + e);
}

void MisurePage::onAborted()
{
    onFinished("");
}

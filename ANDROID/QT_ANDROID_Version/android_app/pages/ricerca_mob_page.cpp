#include "ricerca_mob_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFont>
#include <QTextCursor>
#include <QScroller>
#ifdef HAVE_POSITIONING
#  include <QApplication>
#endif

static constexpr const char* kRicPaperSys =
    "Sei un ricercatore scientifico. Elenca 5-8 paper rilevanti con titolo, autori (anno), "
    "abstract breve e DOI/arXiv. Formato strutturato in testo semplice.";

static constexpr const char* kRicBrevSys =
    "Sei un esperto di brevetti (USPTO, EPO, WIPO). Elenca 5-7 brevetti simili con numero, "
    "titolo, titolare, anno e database (Espacenet). Testo semplice.";

static constexpr const char* kRicRagSys =
    "Sei un analista della base di conoscenza. Analizza la query e fornisci "
    "informazioni rilevanti estratte dalla knowledge base. Cita le fonti.";

static constexpr const char* kRicAstraleSys =
    "Sei un astrologo. Calcola e interpreta la carta natale con posizioni planetarie, "
    "case astrologiche, aspetti e interpretazione. Testo strutturato.";

RicercaMobPage::RicercaMobPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\xac") + " Ricerca & Grafo RAG", this);
    {   QFont f = title->font(); f.setPointSize(14); f.setBold(true); title->setFont(f); }
    vbox->addWidget(title);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    m_modeCmb = new QComboBox(this);
    m_modeCmb->addItem(QString::fromUtf8("\xf0\x9f\x93\x84") + " Paper scientifici");
    m_modeCmb->addItem(QString::fromUtf8("\xf0\x9f\x94\x8f") + " Brevetti");
    m_modeCmb->addItem(QString::fromUtf8("\xf0\x9f\x95\xb8") + " Grafo RAG (Knowledge)");
    m_modeCmb->addItem(QString::fromUtf8("\xe2\xad\x90") + " Carta Astrale");
    m_modeCmb->setMinimumHeight(44);
    form->addRow("Tipo:", m_modeCmb);

    m_queryEdit = new QLineEdit(this);
    m_queryEdit->setPlaceholderText("Query, tema o data nascita (DD/MM/YYYY hh:mm Citta')...");
    form->addRow("Query:", m_queryEdit);

    vbox->addLayout(form);

    /* C5 — riga GPS: lat/lon + bottone "GPS" (visibile solo in modalita' Carta Astrale) */
    m_gpsRow = new QWidget(this);
    m_gpsRow->setVisible(false);
    auto* gpsHBox = new QHBoxLayout(m_gpsRow);
    gpsHBox->setContentsMargins(0, 0, 0, 0);
    gpsHBox->setSpacing(6);

    auto* latLbl = new QLabel("Lat:", m_gpsRow);
    gpsHBox->addWidget(latLbl);
    m_latEdit = new QLineEdit(m_gpsRow);
    m_latEdit->setPlaceholderText("es. 45.4642");
    m_latEdit->setFixedWidth(90);
    m_latEdit->setMinimumHeight(40);
    gpsHBox->addWidget(m_latEdit);

    auto* lonLbl = new QLabel("Lon:", m_gpsRow);
    gpsHBox->addWidget(lonLbl);
    m_lonEdit = new QLineEdit(m_gpsRow);
    m_lonEdit->setPlaceholderText("es. 9.1900");
    m_lonEdit->setFixedWidth(90);
    m_lonEdit->setMinimumHeight(40);
    gpsHBox->addWidget(m_lonEdit);

    m_gpsBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x8d") + " GPS", m_gpsRow);
    m_gpsBtn->setObjectName("PrimaryBtn");
    m_gpsBtn->setMinimumHeight(40);
#ifdef HAVE_POSITIONING
    connect(m_gpsBtn, &QPushButton::clicked,
            this, &RicercaMobPage::onGpsClicked);
#endif
    gpsHBox->addWidget(m_gpsBtn);
    gpsHBox->addStretch();

    vbox->addWidget(m_gpsRow);

    auto* btnRow = new QHBoxLayout;
    m_searchBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8d") + " Cerca / Analizza", this);
    m_searchBtn->setObjectName("PrimaryBtn");
    m_searchBtn->setMinimumHeight(44);
    btnRow->addWidget(m_searchBtn, 1);

    m_stopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9") + " Stop", this);
    m_stopBtn->setObjectName("DangerBtn");
    m_stopBtn->setMinimumHeight(44);
    m_stopBtn->setVisible(false);
    btnRow->addWidget(m_stopBtn);
    vbox->addLayout(btnRow);

    m_status = new QLabel("", this);
    m_status->setObjectName("StatusLabel");
    vbox->addWidget(m_status);

    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setObjectName("OutputEdit");
    m_output->setPlaceholderText("I risultati appariranno qui...");
    QScroller::grabGesture(m_output->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_output, 1);

    connect(m_modeCmb, &QComboBox::currentIndexChanged,
            this, &RicercaMobPage::onModeChanged);
    connect(m_searchBtn, &QPushButton::clicked, this, &RicercaMobPage::onSearchClicked);
    connect(m_stopBtn,   &QPushButton::clicked, this, &RicercaMobPage::onAborted);
    connect(m_ai, &AiClient::token,    this, &RicercaMobPage::onToken);
    connect(m_ai, &AiClient::finished, this, &RicercaMobPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &RicercaMobPage::onAiError);
    connect(m_ai, &AiClient::aborted,  this, &RicercaMobPage::onAborted);

#ifdef HAVE_POSITIONING
    /* C5 — crea la sorgente GPS (una tantum; aggiornamento su richiesta) */
    m_geoSrc = QGeoPositionInfoSource::createDefaultSource(this);
    if (m_geoSrc) {
        m_geoSrc->setUpdateInterval(0); /* aggiornamento singolo */
        connect(m_geoSrc, &QGeoPositionInfoSource::positionUpdated,
                this, &RicercaMobPage::onPositionUpdated);
        connect(m_geoSrc,
                qOverload<QGeoPositionInfoSource::Error>(
                    &QGeoPositionInfoSource::errorOccurred),
                this, &RicercaMobPage::onGpsError);
    }
#else
    /* GPS non disponibile: nascondi il bottone GPS */
    if (m_gpsBtn) m_gpsBtn->setEnabled(false);
    if (m_gpsBtn) m_gpsBtn->setToolTip("Qt::Positioning non disponibile in questa build.");
#endif
}

void RicercaMobPage::onSearchClicked()
{
    const QString q = m_queryEdit->text().trimmed();
    if (q.isEmpty()) return;
    const int mode = m_modeCmb->currentIndex();
    m_output->clear();
    setBusy(true);

    const char* sys = nullptr;
    QString user;
    switch (mode) {
    case 0: sys = kRicPaperSys;   user = "Cerca paper su: " + q; break;
    case 1: sys = kRicBrevSys;    user = "Cerca brevetti per: " + q; break;
    case 2: sys = kRicRagSys;     user = "Analizza con la knowledge base: " + q; break;
    case 3: {
        sys = kRicAstraleSys;
        /* C5 — aggiunge lat/lon al prompt se disponibili */
        const QString lat = m_latEdit ? m_latEdit->text().trimmed() : QString();
        const QString lon = m_lonEdit ? m_lonEdit->text().trimmed() : QString();
        user = "Calcola carta natale per: " + q;
        if (!lat.isEmpty() && !lon.isEmpty())
            user += QString(" | Coordinate GPS: lat=%1 lon=%2").arg(lat, lon);
        break;
    }
    default: return;
    }
    m_ai->chat(sys, user);
}

void RicercaMobPage::onRagQueryClicked()
{
    onSearchClicked();
}

void RicercaMobPage::onToken(const QString& t)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(t);
    m_output->moveCursor(QTextCursor::End);
}

void RicercaMobPage::onFinished(const QString&)
{
    setBusy(false);
    m_status->setText(QString::fromUtf8("\xe2\x9c\x85") + " Completato.");
}

void RicercaMobPage::onAiError(const QString& msg)
{
    setBusy(false);
    m_status->setText(QString::fromUtf8("\xe2\x9d\x8c ") + msg);
}

void RicercaMobPage::onAborted()
{
    m_ai->abort();
    setBusy(false);
    m_status->setText("Interrotto.");
}

void RicercaMobPage::setBusy(bool busy)
{
    if (m_searchBtn) m_searchBtn->setEnabled(!busy);
    if (m_stopBtn)   m_stopBtn->setVisible(busy);
    if (busy) m_status->setText(
        QString::fromUtf8("\xe2\x8f\xb3") + " Ricerca in corso...");
}

/* ── C5: mostra/nasconde la riga GPS in base alla modalita' scelta ── */
void RicercaMobPage::onModeChanged(int index)
{
    /* indice 3 = "Carta Astrale" */
    if (m_gpsRow)
        m_gpsRow->setVisible(index == 3);
}

#ifdef HAVE_POSITIONING
/* ── C5: avvia il rilevamento GPS una-tantum ── */
void RicercaMobPage::onGpsClicked()
{
    if (!m_geoSrc) {
        if (m_status) m_status->setText("GPS non disponibile su questo dispositivo.");
        return;
    }

#ifdef Q_OS_ANDROID
    /* Richiesta permesso ACCESS_FINE_LOCATION a runtime (Android 6+) */
    QLocationPermission locPerm;
    locPerm.setAccuracy(QLocationPermission::Precise);
    locPerm.setAvailability(QLocationPermission::WhenInUse);
    qApp->requestPermission(locPerm, this,
                            &RicercaMobPage::onGpsPermissionResult);
    return; /* il resto avviene in onGpsPermissionResult */
#else
    m_geoSrc->requestUpdate(15000);
#endif

    if (m_gpsBtn) m_gpsBtn->setEnabled(false);
    if (m_status) m_status->setText(
        QString::fromUtf8("\xf0\x9f\x93\xa1") + " Rilevamento GPS in corso...");
}

/* ── C5: riceve la posizione e pre-compila lat/lon ── */
void RicercaMobPage::onPositionUpdated(const QGeoPositionInfo& info)
{
    if (m_gpsBtn) m_gpsBtn->setEnabled(true);
    if (!info.coordinate().isValid()) {
        if (m_status) m_status->setText("Coordinate GPS non valide.");
        return;
    }
    const double lat = info.coordinate().latitude();
    const double lon = info.coordinate().longitude();
    if (m_latEdit) m_latEdit->setText(QString::number(lat, 'f', 4));
    if (m_lonEdit) m_lonEdit->setText(QString::number(lon, 'f', 4));
    if (m_status)  m_status->setText(
        QString::fromUtf8("\xe2\x9c\x85") +
        QString(" GPS: %1, %2").arg(lat, 0, 'f', 4).arg(lon, 0, 'f', 4));
}

/* ── C5: errore GPS ── */
void RicercaMobPage::onGpsError(QGeoPositionInfoSource::Error err)
{
    if (m_gpsBtn) m_gpsBtn->setEnabled(true);
    Q_UNUSED(err)
    if (m_status) m_status->setText(
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore GPS. Verificare i permessi.");
}

/* ── C5: esito richiesta permesso ACCESS_FINE_LOCATION ── */
void RicercaMobPage::onGpsPermissionResult(const QPermission& perm)
{
    if (perm.status() == Qt::PermissionStatus::Granted) {
        if (m_geoSrc) m_geoSrc->requestUpdate(15000);
    } else {
        if (m_gpsBtn) m_gpsBtn->setEnabled(true);
        if (m_status)
            m_status->setText("Permesso GPS negato. Abilitarlo nelle impostazioni.");
    }
}
#endif /* HAVE_POSITIONING */

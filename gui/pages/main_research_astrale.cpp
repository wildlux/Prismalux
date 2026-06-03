#include "main_research.h"
#include "../widgets/model_combo_box.h"
#include "../widgets/astro_calc.h"
#include "../widgets/natal_chart_widget.h"
#include "../widgets/world_map_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QSplitter>
#include <QScrollArea>
#include <QGroupBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QFont>
#include <QTextCursor>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTextBrowser>
#include <QDateTimeEdit>
#include <QPixmap>
#include <QBuffer>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QDateTime>
/* ═══════════════════════════════════════════════════════════════════
   buildAstraleTab — ⭐ Carta Astrale / Tema Natale
   Inserisci dati di nascita e ottieni una lettura astrologica con AI.
   ═══════════════════════════════════════════════════════════════════ */
QWidget* RicercaPage::buildAstraleTab()
{
    auto* page = new QWidget;
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(6);

    /* ── Splitter VERTICALE esterno:
          TOP    = splitter orizzontale 50/50 (form | ruota)    stretch=3
          BOTTOM = interpretazione + guida astrologica           stretch=2  ── */
    auto* vSplit = new QSplitter(Qt::Vertical, page);
    vSplit->setHandleWidth(5);

    /* ── TOP: splitter orizzontale 50/50 ── */
    auto* mainSplit = new QSplitter(Qt::Horizontal);
    mainSplit->setHandleWidth(6);

    /* ──── SINISTRA: modello LLM + form dati natali ──── */
    auto* leftScroll = new QScrollArea;
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setWidgetResizable(true);
    leftScroll->setMinimumWidth(220);
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* leftW   = new QWidget;
    auto* leftLay = new QVBoxLayout(leftW);
    leftLay->setContentsMargins(0, 0, 6, 0);
    leftLay->setSpacing(8);

    /* Titolo compatto dentro il pannello sinistro */
    auto* titleLbl = new QLabel(
        "\xe2\xad\x90  <b>Carta Astrale / Tema Natale</b>"
        "  <span style='color:gray;font-size:10px;'>"
        "Inserisci dati e consulta gli astri con AI."
        "</span>");
    titleLbl->setTextFormat(Qt::RichText);
    leftLay->addWidget(titleLbl);

    auto* formGroup = new QGroupBox("Dati natali");
    auto* formLay   = new QFormLayout(formGroup);
    formLay->setSpacing(6);
    formLay->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    /* Data — Ora + Sesso su unica riga */
    auto* dataOraRow = new QWidget(formGroup);
    auto* dataOraLay = new QHBoxLayout(dataOraRow);
    dataOraLay->setContentsMargins(0, 0, 0, 0); dataOraLay->setSpacing(6);
    m_astraleNascita = new QDateEdit(dataOraRow);
    m_astraleNascita->setCalendarPopup(true);
    m_astraleNascita->setDate(QDate(1990, 1, 1));
    m_astraleNascita->setDisplayFormat("dd/MM/yyyy");
    m_astraleOra = new QTimeEdit(dataOraRow);
    m_astraleOra->setDisplayFormat("HH:mm");
    m_astraleOra->setTime(QTime(12, 0));
    auto* sessoLbl = new QLabel("Sesso", dataOraRow);
    sessoLbl->setStyleSheet("color:gray; font-size:11px; margin-left:6px;");
    dataOraLay->addWidget(m_astraleNascita);
    dataOraLay->addWidget(m_astraleOra);
    dataOraLay->addWidget(sessoLbl);
    /* Sesso — ♂ / ♀ inline */
    m_astraleSessoGrp = new QButtonGroup(leftW);
    const struct { const char* label; const char* id; } kSessi[] = {
        { "\xe2\x99\x82", "Maschio" },
        { "\xe2\x99\x80", "Femmina" },
    };
    for (int i = 0; i < 2; ++i) {
        auto* rb = new QRadioButton(QString::fromUtf8(kSessi[i].label), dataOraRow);
        rb->setProperty("sessoId", QString::fromUtf8(kSessi[i].id));
        m_astraleSessoGrp->addButton(rb, i);
        dataOraLay->addWidget(rb);
        if (i == 0) rb->setChecked(true);
    }
    dataOraLay->addStretch();
    formLay->addRow("Data \xe2\x80\x94 Ora:", dataOraRow);

    /* ── Mappa collassabile (riga 2, spanning) ── */
    auto* mapToggle = new QPushButton("\xe2\x96\xbc Luogo di nascita", formGroup);
    mapToggle->setCheckable(true);
    mapToggle->setChecked(true);
    mapToggle->setFlat(true);
    mapToggle->setStyleSheet(
        "text-align:left; font-weight:bold; font-size:11px;"
        "padding:2px 4px; color:#333;");
    formLay->addRow(mapToggle);

    auto* mapContainer = new QWidget(formGroup);
    auto* mapContLay   = new QVBoxLayout(mapContainer);
    mapContLay->setContentsMargins(0, 0, 0, 0);
    mapContLay->setSpacing(4);

    m_worldMap = new WorldMapWidget(mapContainer);
    m_worldMap->setMinimumHeight(150);
    m_worldMap->setMaximumHeight(220);
    m_worldMap->setCoords(41.9, 12.5);
    mapContLay->addWidget(m_worldMap);

    auto* coordRow = new QWidget(mapContainer);
    auto* coordLay = new QHBoxLayout(coordRow);
    coordLay->setContentsMargins(0, 0, 0, 0); coordLay->setSpacing(6);
    m_astraleCustomCitta = new QLineEdit(coordRow);
    m_astraleCustomCitta->setPlaceholderText("Citt\xc3\xa0 (es. Roma)");
    m_astraleCustomLat   = new QLineEdit(coordRow);
    m_astraleCustomLat->setPlaceholderText("Lat");
    m_astraleCustomLat->setMaximumWidth(62);
    m_astraleCustomLat->setText("41.90");
    m_astraleCustomLon   = new QLineEdit(coordRow);
    m_astraleCustomLon->setPlaceholderText("Lon");
    m_astraleCustomLon->setMaximumWidth(62);
    m_astraleCustomLon->setText("12.50");
    coordLay->addWidget(m_astraleCustomCitta, 1);
    coordLay->addWidget(m_astraleCustomLat);
    coordLay->addWidget(m_astraleCustomLon);
    mapContLay->addWidget(coordRow);

    formLay->addRow(mapContainer);

    /* toggle collassa/espande la mappa */
    connect(mapToggle, &QPushButton::toggled,
            formGroup, [mapContainer, mapToggle](bool open) {
        mapContainer->setVisible(open);
        mapToggle->setText(open
            ? "\xe2\x96\xbc Luogo di nascita"
            : "\xe2\x96\xba Luogo di nascita");
    });

    /* Profondità lettura */
    m_astraleDepth = new QComboBox(formGroup);
    m_astraleDepth->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_astraleDepth->addItem("Sintesi  (5-8 paragrafi)",                "sintesi");
    m_astraleDepth->addItem("Completa (segni + pianeti principali)",   "completa");
    m_astraleDepth->addItem("Professionale (tema natale dettagliato)", "pro");
    m_astraleDepth->setCurrentIndex(1);
    formLay->addRow("Livello lettura:", m_astraleDepth);

    /* Modello AI — riga 1: combo + ⭐ Leggi gli Astri */
    auto* modelRow = new QWidget(formGroup);
    auto* modelLay = new QHBoxLayout(modelRow);
    modelLay->setContentsMargins(0, 0, 0, 0); modelLay->setSpacing(6);
    m_astraleModel = new ModelComboBox(m_ai, modelRow);
    m_astraleRunBtn = new QPushButton("\xe2\xad\x90  Leggi gli Astri", modelRow);
    m_astraleRunBtn->setObjectName("actionBtn");
    modelLay->addWidget(m_astraleModel, 1);
    modelLay->addWidget(m_astraleRunBtn);
    formLay->addRow("Modello AI:", modelRow);

    /* Riga 2: 🔮 Ruota Karmica (larghezza piena) */
    m_karmicaBtn = new QPushButton("\xf0\x9f\x94\xae  Compila Ruota Karmica", formGroup);
    m_karmicaBtn->setObjectName("actionBtn");
    m_karmicaBtn->setStyleSheet(
        "QPushButton#actionBtn { background:#4a235a; color:#e8d5f0; border:1px solid #7b3fa0; }"
        "QPushButton#actionBtn:hover { background:#5c2d72; }"
        "QPushButton#actionBtn:pressed { background:#3a1a47; }");
    formLay->addRow(m_karmicaBtn);

    /* Domanda facoltativa — toggle collassa/espande su clic barra */
    auto* focusToggle = new QPushButton(
        "\xe2\x96\xba  Domanda / focus (opzionale)", formGroup);
    focusToggle->setCheckable(true);
    focusToggle->setChecked(false);
    focusToggle->setFlat(true);
    focusToggle->setStyleSheet(
        "text-align:left; font-weight:bold; font-size:11px;"
        "padding:3px 4px; color:#555; border-bottom:1px solid #ccc;");
    formLay->addRow(focusToggle);

    m_astraleDesc = new QTextEdit(formGroup);
    m_astraleDesc->setVisible(false);
    m_astraleDesc->setMinimumHeight(80);
    m_astraleDesc->setMaximumHeight(140);
    m_astraleDesc->setPlaceholderText(
        "Domanda o focus facoltativo:\n"
        "es. 'Come sar\xc3\xa0 il 2025 per la mia carriera?'\n"
        "oppure 'Cosa mi dice il tema natale sull\xe2\x80\x99" "amore?'");
    formLay->addRow(m_astraleDesc);

    connect(focusToggle, &QPushButton::toggled,
            this, [this, focusToggle](bool open) {
        m_astraleDesc->setVisible(open);
        focusToggle->setText(open
            ? "\xe2\x96\xbc  Domanda / focus (opzionale)"
            : "\xe2\x96\xba  Domanda / focus (opzionale)");
    });

    leftLay->addWidget(formGroup);

    /* mappa → aggiorna campi; city search → aggiorna città; campi → aggiorna mappa */
    connect(m_worldMap, &WorldMapWidget::coordsChanged,
            this, [this](double lat, double lon) {
        m_astraleCustomLat->setText(QString::number(lat, 'f', 2));
        m_astraleCustomLon->setText(QString::number(lon, 'f', 2));
    });
    connect(m_worldMap, &WorldMapWidget::cityNameChanged,
            this, [this](const QString& name) {
        m_astraleCustomCitta->setText(name);
    });
    connect(m_astraleCustomLat, &QLineEdit::textChanged,
            this, [this](const QString& s) {
        bool ok1, ok2;
        double lat = s.toDouble(&ok1);
        double lon = m_astraleCustomLon->text().toDouble(&ok2);
        if (ok1 && ok2) m_worldMap->setCoords(lat, lon);
    });
    connect(m_astraleCustomLon, &QLineEdit::textChanged,
            this, [this](const QString& s) {
        bool ok1, ok2;
        double lat = m_astraleCustomLat->text().toDouble(&ok1);
        double lon = s.toDouble(&ok2);
        if (ok1 && ok2) m_worldMap->setCoords(lat, lon);
    });

    leftLay->addStretch();

    leftScroll->setWidget(leftW);
    mainSplit->addWidget(leftScroll);

    /* ──── DESTRA: ruota astrale + barra salva ──── */
    auto* chartContainer = new QWidget;
    auto* chartContLay   = new QVBoxLayout(chartContainer);
    chartContLay->setContentsMargins(0, 0, 0, 0);
    chartContLay->setSpacing(4);

    m_natalChart = new NatalChartWidget;
    m_natalChart->setMinimumHeight(300);
    m_natalChart->setMinimumWidth(260);
    chartContLay->addWidget(m_natalChart, 1);

    auto* chartBtnRow = new QHBoxLayout;
    chartBtnRow->setContentsMargins(4, 0, 4, 4);
    chartBtnRow->setSpacing(6);
    auto* btnSavePng = new QPushButton(
        "\xf0\x9f\x96\xbc  Salva immagine .png", chartContainer);  /* 🖼 */
    btnSavePng->setObjectName("actionBtn");
    btnSavePng->setToolTip("Salva la ruota astrale come file PNG ad alta risoluzione");
    chartBtnRow->addWidget(btnSavePng);
    chartBtnRow->addStretch(1);
    connect(btnSavePng, &QPushButton::clicked,
            this, &RicercaPage::onSalvaChartPng);
    chartContLay->addLayout(chartBtnRow);

    mainSplit->addWidget(chartContainer);

    mainSplit->setStretchFactor(0, 1);
    mainSplit->setStretchFactor(1, 1);
    vSplit->addWidget(mainSplit);

    /* ── BOTTOM: QTabWidget con 2 tab ── */
    auto* bottomTabs = new QTabWidget;
    bottomTabs->setTabPosition(QTabWidget::North);

    /* Tab 0 — Interpretazione */
    auto* interpTab = new QWidget;
    auto* interpLay = new QVBoxLayout(interpTab);
    interpLay->setContentsMargins(0, 4, 0, 0);
    interpLay->setSpacing(0);
    m_astraleOutput = new QTextEdit;
    m_astraleOutput->setReadOnly(false);
    m_astraleOutput->setFont(QFont("Monospace", 10));
    m_astraleOutput->setPlaceholderText(
        "L\xe2\x80\x99"
        "interpretazione testuale apparer\xc3\xa0 qui.\n"
        "Compila i dati natali a sinistra e clicca \xe2\xad\x90 Leggi gli Astri.");
    /* barra azioni allineata a sinistra — no label (tab già dice "Interpretazione") */
    {
        auto* aBar  = new QWidget(interpTab);
        auto* aLay  = new QHBoxLayout(aBar);
        aLay->setContentsMargins(0, 2, 0, 2);
        aLay->setSpacing(6);
        auto* btnMd2  = new QPushButton("\xf0\x9f\x92\xbe  Salva .md");
        auto* btnPdf2 = new QPushButton("\xf0\x9f\x96\xa8  Esporta PDF");
        auto* btnClr2 = new QPushButton("\xf0\x9f\x97\x91  Svuota");
        btnMd2->setObjectName("actionBtn");
        btnPdf2->setObjectName("actionBtn");
        btnClr2->setObjectName("actionBtn");
        aLay->addWidget(btnMd2);
        aLay->addWidget(btnPdf2);
        aLay->addWidget(btnClr2);
        aLay->addStretch();
        connect(btnPdf2, &QPushButton::clicked, this, &RicercaPage::onAstraleSavePdf);
        connect(btnMd2,  &QPushButton::clicked, this, &RicercaPage::onAstraleSaveMd);
        connect(btnClr2, &QPushButton::clicked, this, &RicercaPage::onAstraleClear);
        interpLay->addWidget(aBar);
    }
    interpLay->addWidget(m_astraleOutput, 1);
    bottomTabs->addTab(interpTab, "\xe2\xad\x90  Interpretazione");

    /* Tab 1 — Guida Astrologica */
    auto* guideTab    = new QWidget;
    auto* guideRootLay = new QVBoxLayout(guideTab);
    guideRootLay->setSpacing(0);
    guideRootLay->setContentsMargins(4, 6, 4, 6);

    auto* guideSplit = new QSplitter(Qt::Horizontal, guideTab);
    guideSplit->setHandleWidth(10);
    guideSplit->setStyleSheet(
        "QSplitter::handle {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #e0e0e0, stop:0.4 #b0b0b0, stop:0.6 #b0b0b0, stop:1 #e0e0e0);"
        "  border-radius: 4px;"
        "  margin: 6px 2px;"
        "}"
        "QSplitter::handle:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #c0c0e8, stop:0.4 #8080cc, stop:0.6 #8080cc, stop:1 #c0c0e8);"
        "}");

    static const char* kDoHtml =
        "<style>body{font-size:11px;margin:0;padding:2px;}"
        "h3{color:#4CAF50;margin:0 0 4px 0;font-size:12px;}"
        "li{margin-bottom:3px;}</style>"
        "<h3>\xe2\x9c\x85 COSA FARE — personalizzato dopo la lettura</h3>"
        "<ul><li>Genera il tema natale per ricevere consigli personalizzati "
        "basati sulla posizione reale dei tuoi pianeti.</li></ul>";

    static const char* kDontHtml =
        "<style>body{font-size:11px;margin:0;padding:2px;}"
        "h3{color:#F44336;margin:0 0 4px 0;font-size:12px;}"
        "li{margin-bottom:3px;}</style>"
        "<h3>\xe2\x9b\x94 COSA EVITARE — personalizzato dopo la lettura</h3>"
        "<ul><li>Genera il tema natale per ricevere avvertimenti personalizzati "
        "basati sui transiti del tuo tema natale.</li></ul>";

    m_astraleDoBox = new QTextBrowser;
    m_astraleDoBox->setFrameShape(QFrame::NoFrame);
    m_astraleDoBox->setOpenExternalLinks(false);
    m_astraleDoBox->setHtml(QString::fromUtf8(kDoHtml));

    m_astraleDontBox = new QTextBrowser;
    m_astraleDontBox->setFrameShape(QFrame::NoFrame);
    m_astraleDontBox->setOpenExternalLinks(false);
    m_astraleDontBox->setHtml(QString::fromUtf8(kDontHtml));

    guideSplit->addWidget(m_astraleDoBox);
    guideSplit->addWidget(m_astraleDontBox);
    guideSplit->setStretchFactor(0, 1);
    guideSplit->setStretchFactor(1, 1);
    guideRootLay->addWidget(guideSplit, 1);
    bottomTabs->addTab(guideTab, "\xe2\xad\x90  Guida Astrologica");

    /* Tab 2 — Ruota Karmica */
    auto* karmTab = new QWidget;
    auto* karmLay = new QVBoxLayout(karmTab);
    karmLay->setContentsMargins(0, 4, 0, 0);
    karmLay->setSpacing(0);
    m_karmicaOutput = new QTextEdit;
    m_karmicaOutput->setReadOnly(false);
    m_karmicaOutput->setFont(QFont("Monospace", 10));
    m_karmicaOutput->setPlaceholderText(
        "\xf0\x9f\x94\xae  Analisi karmica apparer\xc3\xa0 qui.\n"
        "Inserisci i dati natali e clicca \xf0\x9f\x94\xae Ruota Karmica.");
    {
        auto* kBar = new QWidget(karmTab);
        auto* kBarLay = new QHBoxLayout(kBar);
        kBarLay->setContentsMargins(0, 2, 0, 2);
        kBarLay->setSpacing(6);
        auto* btnMdK  = new QPushButton("\xf0\x9f\x92\xbe  Salva .md");
        auto* btnPdfK = new QPushButton("\xf0\x9f\x96\xa8  Esporta PDF");
        auto* btnClrK = new QPushButton("\xf0\x9f\x97\x91  Svuota");
        btnMdK->setObjectName("actionBtn");
        btnPdfK->setObjectName("actionBtn");
        btnClrK->setObjectName("actionBtn");
        kBarLay->addWidget(btnMdK);
        kBarLay->addWidget(btnPdfK);
        kBarLay->addWidget(btnClrK);
        kBarLay->addStretch();
        connect(btnPdfK, &QPushButton::clicked, this, &RicercaPage::onKarmicaSavePdf);
        connect(btnMdK,  &QPushButton::clicked, this, &RicercaPage::onKarmicaSaveMd);
        connect(btnClrK, &QPushButton::clicked, this, &RicercaPage::onKarmicaClear);
        karmLay->addWidget(kBar);
    }
    karmLay->addWidget(m_karmicaOutput, 1);
    bottomTabs->addTab(karmTab, "\xf0\x9f\x94\xae  Ruota Karmica");

    vSplit->addWidget(bottomTabs);
    vSplit->setStretchFactor(0, 3);
    vSplit->setStretchFactor(1, 2);
    vSplit->setSizes({480, 280});

    root->addWidget(vSplit, 1);

    /* pulsante toggle: ⭐ Leggi → ■ Stop → ⭐ Leggi */
    connect(m_astraleRunBtn, &QPushButton::clicked, this, &RicercaPage::onAstraleRunToggled);

    /* 🔮 Compila Ruota Karmica — calcolo + disegno, nessuna AI */
    connect(m_karmicaBtn, &QPushButton::clicked,
            this, &RicercaPage::onKarmicaRunClicked);

    return page;
}

/* ─────────────────────────────────────────────────────────────────
   SLOT Carta Astrale
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onAstraleRunClicked()
{
    const QString citta = m_astraleCustomCitta ? m_astraleCustomCitta->text().trimmed() : QString();
    const QString lat   = m_astraleCustomLat   ? m_astraleCustomLat->text().trimmed()   : QString();
    const QString lon   = m_astraleCustomLon   ? m_astraleCustomLon->text().trimmed()   : QString();
    if (citta.isEmpty() && lat.isEmpty()) {
        m_astraleOutput->setPlainText(
            "\xe2\x9a\xa0  Clicca sulla mappa oppure inserisci le coordinate "
            "per selezionare il luogo di nascita.");
        return;
    }
    QString luogo = citta.isEmpty()
        ? QString("Lat %1, Lon %2").arg(lat, lon)
        : citta;
    QString coordHint;
    if (!lat.isEmpty() && !lon.isEmpty())
        coordHint = QString("\n**Coordinate geografiche:** Lat %1 / Lon %2").arg(lat, lon);
    if (m_ai->busy()) {
        m_astraleOutput->append("\xe2\x9a\xa0  AI occupata. Attendi o premi Stop.");
        return;
    }

    /* Sesso selezionato */
    QString sesso = "Non specificato";
    if (auto* btn = m_astraleSessoGrp->checkedButton())
        sesso = btn->property("sessoId").toString();

    /* Profondità */
    const QString depth = m_astraleDepth->currentData().toString();
    QString depthInstr;
    if (depth == "sintesi")
        depthInstr = "Fornisci una sintesi concisa in 5-8 paragrafi, solo i punti principali.";
    else if (depth == "pro")
        depthInstr = "Fornisci un'analisi professionale e approfondita, coprendo tutte "
                     "le case astrologiche, gli aspetti planetari principali e le progressioni.";
    else
        depthInstr = "Fornisci una lettura completa con i principali segni, pianeti e case.";

    /* ── Calcolo astronomico reale ────────────────────────────── */
    const QDate dateVal = m_astraleNascita->date();
    const QTime ora     = m_astraleOra->time();

    double latD = 41.9, lonD = 12.5;
    if (!lat.isEmpty() && !lon.isEmpty()) {
        latD = lat.toDouble();
        lonD = lon.toDouble();
    }

    /* Usa m_astroResult se già compilata, altrimenti ricalcola */
    if (!m_astroResult.ok) {
        m_astroResult = AstroCalc::compute(
            dateVal.year(), dateVal.month(), dateVal.day(),
            ora.hour(), ora.minute(), latD, lonD);
        if (m_astroResult.ok && m_natalChart)
            m_natalChart->setData(m_astroResult.planets, m_astroResult.ascLon,
                                  m_astroResult.mcLon, m_astroResult.cusps);
    }
    const AstroCalc::Result& astroRes = m_astroResult;

    /* ── Costruisci posizioni pianeti per il prompt AI ────────── */
    static const char* kSignNames[] = {
        "Ariete","Toro","Gemelli","Cancro","Leone","Vergine",
        "Bilancia","Scorpione","Sagittario","Capricorno","Acquario","Pesci"
    };
    auto lonToSign = [&](double l) -> QString {
        int sign = static_cast<int>(l / 30.0) % 12;
        double deg = std::fmod(l, 30.0);
        return QString("%1\xc2\xb0 %2").arg(static_cast<int>(deg)).arg(kSignNames[sign]);
    };

    QString planetiStr;
    if (astroRes.ok) {
        for (const auto& pl : astroRes.planets)
            planetiStr += "- " + pl.name + ": " + lonToSign(pl.lon)
                        + " (" + QString::number(pl.lon, 'f', 2) + "\xc2\xb0)\n";
        planetiStr += "- ASCENDENTE: " + lonToSign(astroRes.ascLon)
                    + " (" + QString::number(astroRes.ascLon, 'f', 2) + "\xc2\xb0)\n";
        planetiStr += "- MC: " + lonToSign(astroRes.mcLon)
                    + " (" + QString::number(astroRes.mcLon, 'f', 2) + "\xc2\xb0)\n";
    }

    /* ── Case astrologiche (Placidus cusps) ──────────────────── */
    QString caseStr;
    if (astroRes.ok) {
        for (int ci = 0; ci < 12; ++ci)
            caseStr += QString("- Casa %1: %2\n").arg(ci + 1).arg(lonToSign(astroRes.cusps[ci]));
    }

    /* ── Aspetti planetari calcolati ─────────────────────────── */
    struct Aspect { const char* sym; int deg; int orb; };
    static const Aspect kAspects[] = {
        {"\xe2\x98\x8c", 0,   8},  /* ☌ congiunzione */
        {"\xe2\xac\x9c", 60,  6},  /* ⬜ sestile */
        {"\xe2\x96\xa1", 90,  8},  /* □  quadratura */
        {"\xe2\x96\xb3", 120, 8},  /* △  trigono */
        {"\xe2\x98\x8d", 180, 8},  /* ☍  opposizione */
    };
    QString aspettiStr;
    if (astroRes.ok && astroRes.planets.size() > 1) {
        for (int i = 0; i < astroRes.planets.size(); ++i) {
            for (int j = i + 1; j < astroRes.planets.size(); ++j) {
                double diff = std::abs(astroRes.planets[i].lon - astroRes.planets[j].lon);
                if (diff > 180.0) diff = 360.0 - diff;
                for (const auto& asp : kAspects) {
                    if (std::abs(diff - asp.deg) <= asp.orb) {
                        aspettiStr += QString("- %1 %2 %3 %4 (orb %5\xc2\xb0)\n")
                            .arg(QString::fromUtf8(asp.sym))
                            .arg(astroRes.planets[i].name)
                            .arg(QString::fromUtf8(asp.sym))
                            .arg(astroRes.planets[j].name)
                            .arg(diff - asp.deg, 0, 'f', 1);
                        break;
                    }
                }
            }
        }
    }

    static const QString kSys =
        "Sei un astrologo esperto con conoscenza approfondita di astrologia occidentale. "
        "Il tema natale \xc3\xa8 stato calcolato astronomicamente con precisione "
        "(algoritmi Meeus + JPL). Tutti i valori forniti sono REALI — non devi ricalcolarli.\n\n"
        "Se \xc3\xa8 allegata un\xe2\x80\x99" "immagine della ruota astrale, usala per "
        "confermare visivamente le posizioni e gli aspetti planetari descritti nel testo.\n\n"
        "Struttura la risposta con queste sezioni:\n"
        "## \xe2\x98\x80 Segno Solare\n"
        "## \xf0\x9f\x8c\x99 Segno Lunare\n"
        "## \xe2\xac\x86 Ascendente\n"
        "## \xf0\x9f\xaa\x90 Pianeti principali\n"
        "## \xf0\x9f\x8f\xa0 Case astrologiche principali\n"
        "## \xf0\x9f\x94\x97 Aspetti dominanti\n"
        "## \xe2\x9c\xa8 Sintesi e consigli\n\n"
        "### COSA_FARE\n"
        "- [5-7 consigli personalizzati]\n"
        "### FINE_COSA_FARE\n\n"
        "### COSA_EVITARE\n"
        "- [5-7 avvertimenti personalizzati]\n"
        "### FINE_COSA_EVITARE\n\n";

    const QString data   = dateVal.toString("dd/MM/yyyy");
    const QString oraStr = QString("%1:%2")
        .arg(ora.hour(),   2, 10, QChar('0'))
        .arg(ora.minute(), 2, 10, QChar('0'));
    const QString desc = m_astraleDesc->toPlainText().trimmed();

    const QString userMsg =
        "**Data di nascita:** " + data + "\n"
        "**Ora di nascita:** " + oraStr + "\n"
        "**Luogo di nascita:** " + luogo + coordHint + "\n"
        "**Sesso:** " + sesso + "\n\n"
        "**Posizioni planetarie:**\n"
        + (planetiStr.isEmpty() ? "(coordinate non disponibili)\n" : planetiStr)
        + "\n**Case astrologiche (Placidus):**\n"
        + (caseStr.isEmpty() ? "(non disponibili)\n" : caseStr)
        + "\n**Aspetti planetari calcolati:**\n"
        + (aspettiStr.isEmpty() ? "(nessuno calcolato)\n" : aspettiStr)
        + (desc.isEmpty() ? "" : "\n**Domanda/focus:** " + desc + "\n")
        + "\n" + depthInstr + "\n\nInterpreta questo tema natale.";

    /* Selezione modello */
    if (m_astraleModel && m_astraleModel->count() > 0) {
        const QString sel = m_astraleModel->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    /* Connessioni one-shot */
    QObject::disconnect(m_astraleTokenConn);
    QObject::disconnect(m_astraleFinishedConn);
    QObject::disconnect(m_astraleErrorConn);
    m_astraleTokenConn    = connect(m_ai, &AiClient::token,
                                    this, &RicercaPage::onAstraleToken);
    m_astraleFinishedConn = connect(m_ai, &AiClient::finished,
                                    this, &RicercaPage::onAstraleFinished);
    m_astraleErrorConn    = connect(m_ai, &AiClient::error,
                                    this, &RicercaPage::onAstraleError);

    m_astraleRunBtn->setText("\xe2\x96\xa0  Stop");
    m_astraleRunBtn->setProperty("running", true);
    if (m_sciProgress) m_sciProgress->setVisible(true);

    m_astraleOutput->clear();
    m_astraleOutput->append(
        "\xf0\x9f\xa4\x96 Sta rispondendo LLM: " + m_ai->model()
        + " \xe2\x80\x94 generazione in corso...");
    m_astraleOutput->append(
        "\xe2\xad\x90  Lettura astrale in corso...\n"
        + QString(50, QChar(0x2500)));

    /* Renderizza la ruota come PNG e iniettala nel prompt (se disponibile) */
    if (m_natalChart && m_natalChart->isVisible()) {
        QPixmap pix = m_natalChart->grab();
        if (!pix.isNull()) {
            QByteArray imgBytes;
            QBuffer buf(&imgBytes);
            buf.open(QIODevice::WriteOnly);
            pix.save(&buf, "PNG");
            m_ai->chatWithImage(kSys, userMsg, imgBytes.toBase64(), "image/png");
            return;
        }
    }
    m_ai->chat(kSys, userMsg);
}

void RicercaPage::onAstraleStopClicked()
{
    m_ai->abort();
}

void RicercaPage::onAstraleToken(const QString& t)
{
    m_astraleOutput->moveCursor(QTextCursor::End);
    m_astraleOutput->insertPlainText(t);
}

/* ── Helper: estrae un blocco tra due marker ────────────────────── */
static QString extractBlock(const QString& txt, const QString& start, const QString& end)
{
    const int s = txt.indexOf(start);
    if (s < 0) return {};
    const int bodyStart = txt.indexOf('\n', s) + 1;
    const int e = txt.indexOf(end, bodyStart);
    if (e < 0) return txt.mid(bodyStart).trimmed();
    return txt.mid(bodyStart, e - bodyStart).trimmed();
}

/* ── Helper: converte lista "- item\n- item" in HTML <ul> ───────── */
static QString bulletToHtml(const QString& raw, const QString& title,
                             const QString& color)
{
    QString html = QString("<style>body{font-size:11px;margin:0;padding:2px;}"
                           "h3{color:%1;margin:0 0 4px 0;font-size:12px;}"
                           "li{margin-bottom:3px;}</style>"
                           "<h3>%2</h3><ul>").arg(color, title);
    for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
        QString item = line.trimmed();
        if (item.startsWith('-') || item.startsWith('*'))
            item = item.mid(1).trimmed();
        if (!item.isEmpty())
            html += "<li>" + item.toHtmlEscaped() + "</li>";
    }
    html += "</ul>";
    return html;
}

void RicercaPage::onAstraleFinished(const QString& full)
{
    QObject::disconnect(m_astraleTokenConn);
    QObject::disconnect(m_astraleFinishedConn);
    QObject::disconnect(m_astraleErrorConn);
    m_astraleTokenConn = m_astraleFinishedConn = m_astraleErrorConn = {};

    m_astraleRunBtn->setText("\xe2\xad\x90  Leggi gli Astri");
    m_astraleRunBtn->setProperty("running", false);
    if (m_sciProgress) m_sciProgress->setVisible(false);
    m_astraleOutput->append("\n" + QString(50, QChar(0x2500)));

    /* ── Estrai e aggiorna pannello COSA FARE ────────────────────── */
    const QString doRaw = extractBlock(full, "### COSA_FARE", "### FINE_COSA_FARE");
    if (!doRaw.isEmpty() && m_astraleDoBox)
        m_astraleDoBox->setHtml(bulletToHtml(doRaw,
            "\xe2\x9c\x85 COSA FARE — personalizzato per te", "#4CAF50"));

    const QString dontRaw = extractBlock(full, "### COSA_EVITARE", "### FINE_COSA_EVITARE");
    if (!dontRaw.isEmpty() && m_astraleDontBox)
        m_astraleDontBox->setHtml(bulletToHtml(dontRaw,
            "\xe2\x9b\x94 COSA EVITARE — personalizzato per te", "#F44336"));

    /* Il grafico è già stato aggiornato con calcoli reali in onAstraleRunClicked */
}

void RicercaPage::onAstraleError(const QString& msg)
{
    QObject::disconnect(m_astraleTokenConn);
    QObject::disconnect(m_astraleFinishedConn);
    QObject::disconnect(m_astraleErrorConn);
    m_astraleTokenConn = m_astraleFinishedConn = m_astraleErrorConn = {};

    m_astraleRunBtn->setText("\xe2\xad\x90  Leggi gli Astri");
    m_astraleRunBtn->setProperty("running", false);
    if (m_sciProgress) m_sciProgress->setVisible(false);
    m_sciErrorPanel->showError(msg, [this]{ onAstraleRunClicked(); });
}

/* ─────────────────────────────────────────────────────────────────
   SLOT Ruota Karmica — calcola e disegna la ruota, nessuna AI.
   I dati vengono salvati in m_astroResult e poi iniettati da
   onAstraleRunClicked() insieme all'immagine PNG del grafico.
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onKarmicaRunClicked()
{
    const QString lat = m_astraleCustomLat ? m_astraleCustomLat->text().trimmed() : QString();
    const QString lon = m_astraleCustomLon ? m_astraleCustomLon->text().trimmed() : QString();
    const QString citta = m_astraleCustomCitta ? m_astraleCustomCitta->text().trimmed() : QString();

    if (citta.isEmpty() && lat.isEmpty()) {
        if (m_karmicaOutput)
            m_karmicaOutput->setPlainText(
                "\xe2\x9a\xa0  Inserisci il luogo di nascita (mappa o coordinate) prima di compilare.");
        return;
    }

    const QDate dateVal = m_astraleNascita->date();
    const QTime ora     = m_astraleOra->time();
    double latD = lat.isEmpty() ? 41.9 : lat.toDouble();
    double lonD = lon.isEmpty() ? 12.5 : lon.toDouble();

    /* Calcola e salva in m_astroResult */
    m_astroResult = AstroCalc::compute(
        dateVal.year(), dateVal.month(), dateVal.day(),
        ora.hour(), ora.minute(), latD, lonD);

    if (!m_astroResult.ok) {
        if (m_karmicaOutput)
            m_karmicaOutput->setPlainText("\xe2\x9a\xa0  Calcolo astronomico fallito. Verifica le coordinate.");
        return;
    }

    /* Aggiorna la ruota grafica */
    if (m_natalChart)
        m_natalChart->setData(m_astroResult.planets, m_astroResult.ascLon,
                              m_astroResult.mcLon, m_astroResult.cusps);

    /* Mostra il foglio tecnico nel tab Ruota Karmica */
    static const char* kSignNames[] = {
        "Ariete","Toro","Gemelli","Cancro","Leone","Vergine",
        "Bilancia","Scorpione","Sagittario","Capricorno","Acquario","Pesci"
    };
    auto lonToSign = [&](double l) -> QString {
        int sign = static_cast<int>(l / 30.0) % 12;
        double deg = std::fmod(l, 30.0);
        return QString("%1\xc2\xb0 %2").arg(static_cast<int>(deg)).arg(kSignNames[sign]);
    };

    QString report;
    report += "\xf0\x9f\x94\xae  RUOTA KARMICA — Foglio Tecnico\n";
    report += QString(50, QChar(0x2500)) + "\n";
    report += QString("Data: %1  Ora: %2:%3  Luogo: %4\n\n")
        .arg(dateVal.toString("dd/MM/yyyy"))
        .arg(ora.hour(),   2, 10, QChar('0'))
        .arg(ora.minute(), 2, 10, QChar('0'))
        .arg(citta.isEmpty() ? QString("Lat %1 / Lon %2").arg(latD).arg(lonD) : citta);

    report += "\xf0\x9f\x93\x8d Posizioni Planetarie\n";
    for (const auto& pl : m_astroResult.planets)
        report += QString("  %-14 %2  (%3\xc2\xb0)\n")
            .arg(pl.name + ":")
            .arg(lonToSign(pl.lon))
            .arg(pl.lon, 0, 'f', 2);
    report += QString("  %-14 %2  (%3\xc2\xb0)\n")
        .arg("Ascendente:").arg(lonToSign(m_astroResult.ascLon))
        .arg(m_astroResult.ascLon, 0, 'f', 2);
    report += QString("  %-14 %2  (%3\xc2\xb0)\n\n")
        .arg("MC:").arg(lonToSign(m_astroResult.mcLon))
        .arg(m_astroResult.mcLon, 0, 'f', 2);

    report += "\xf0\x9f\x8f\xa0 Case Astrologiche (Placidus)\n";
    for (int ci = 0; ci < 12; ++ci)
        report += QString("  Casa %1: %2\n").arg(ci + 1).arg(lonToSign(m_astroResult.cusps[ci]));

    report += "\n\xf0\x9f\x94\x97 Aspetti Planetari\n";
    struct Aspect { const char* name; int deg; int orb; };
    static const Aspect kAsp[] = {
        {"Congiunzione", 0, 8}, {"Sestile", 60, 6},
        {"Quadratura", 90, 8}, {"Trigono", 120, 8}, {"Opposizione", 180, 8}
    };
    bool anyAsp = false;
    const auto& pls = m_astroResult.planets;
    for (int i = 0; i < pls.size(); ++i) {
        for (int j = i + 1; j < pls.size(); ++j) {
            double diff = std::abs(pls[i].lon - pls[j].lon);
            if (diff > 180.0) diff = 360.0 - diff;
            for (const auto& a : kAsp) {
                if (std::abs(diff - a.deg) <= a.orb) {
                    report += QString("  %1 \xe2\x80\x94 %2 / %3  (orb %4\xc2\xb0)\n")
                        .arg(a.name).arg(pls[i].name).arg(pls[j].name)
                        .arg(diff - a.deg, 0, 'f', 1);
                    anyAsp = true;
                    break;
                }
            }
        }
    }
    if (!anyAsp) report += "  (nessun aspetto rilevante)\n";

    report += "\n" + QString(50, QChar(0x2500)) + "\n";
    report += "\xe2\x9c\x85  Ruota compilata. Ora premi \xe2\xad\x90 Leggi gli Astri per l\xe2\x80\x99" "interpretazione AI\n";
    report += "    con la ruota e tutti questi dati iniettati nel prompt.";

    if (m_karmicaOutput) {
        m_karmicaOutput->clear();
        m_karmicaOutput->setPlainText(report);
    }
}

/* ═══════════════════════════════════════════════════════════════════
   Dati geografici per la Carta Astrale
   ═══════════════════════════════════════════════════════════════════ */
namespace {

struct PaeseGeo {
    const char*              nome;
    const char* const* const citta;   /* array null-terminated */
};
struct ContinenteGeo {
    const char*              nome;
    const PaeseGeo* const    paesi;   /* array con nome==nullptr come sentinella */
};

/* ── Città per paese ─────────────────────────────────────────────── */
static const char* kCittaItalia[]    = {"Roma","Milano","Napoli","Torino","Palermo","Catania",
    "Bologna","Firenze","Venezia","Genova","Bari","Cagliari","Verona","Messina","Padova",
    "Trieste","Brescia","Modena","Reggio Calabria","Parma",nullptr};
static const char* kCittaFrancia[]   = {"Parigi","Lione","Marsiglia","Bordeaux","Toulouse",
    "Nizza","Strasburgo","Lille","Rennes","Nantes",nullptr};
static const char* kCittaGermania[]  = {"Berlino","Monaco","Amburgo","Francoforte","Colonia",
    "Stoccarda","Düsseldorf","Lipsia","Dresda","Hannover",nullptr};
static const char* kCittaSpagna[]    = {"Madrid","Barcellona","Valencia","Siviglia","Saragozza",
    "Malaga","Bilbao","Granada","Palma","Las Palmas",nullptr};
static const char* kCittaUK[]        = {"Londra","Manchester","Birmingham","Leeds","Glasgow",
    "Liverpool","Edimburgo","Bristol","Sheffield","Cardiff",nullptr};
static const char* kCittaPortogallo[]= {"Lisbona","Porto","Braga","Coimbra","Faro",nullptr};
static const char* kCittaGrecia[]    = {"Atene","Salonicco","Patrasso","Heraklion","Larissa","Rodi",nullptr};
static const char* kCittaOlanda[]    = {"Amsterdam","Rotterdam","L'Aia","Utrecht","Eindhoven","Tilburg",nullptr};
static const char* kCittaBelgio[]    = {"Bruxelles","Gand","Anversa","Liegi","Bruges",nullptr};
static const char* kCittaSvizzera[]  = {"Zurigo","Ginevra","Basilea","Berna","Losanna",nullptr};
static const char* kCittaAustria[]   = {"Vienna","Graz","Linz","Salisburgo","Innsbruck",nullptr};
static const char* kCittaSvezia[]    = {"Stoccolma","Göteborg","Malmö","Uppsala","Västerås",nullptr};
static const char* kCittaNorvegia[]  = {"Oslo","Bergen","Trondheim","Stavanger","Tromsø",nullptr};
static const char* kCittaDanimarca[] = {"Copenaghen","Aarhus","Odense","Aalborg","Esbjerg",nullptr};
static const char* kCittaFinlandia[] = {"Helsinki","Espoo","Tampere","Vantaa","Turku",nullptr};
static const char* kCittaPolonia[]   = {"Varsavia","Cracovia","Łódź","Breslavia","Poznań","Danzica",nullptr};
static const char* kCittaRussia[]    = {"Mosca","San Pietroburgo","Novosibirsk","Ekaterinburg","Kazan","Sochi",nullptr};
static const char* kCittaUcraina[]   = {"Kiev","Kharkiv","Odessa","Dnipro","Leopoli",nullptr};
static const char* kCittaRomania[]   = {"Bucarest","Cluj-Napoca","Timişoara","Iaşi","Constanţa",nullptr};
static const char* kCittaBulgaria[]  = {"Sofia","Plovdiv","Varna","Burgas","Ruse",nullptr};
static const char* kCittaCroazia[]   = {"Zagabria","Spalato","Rijeka","Osijek","Dubrovnik",nullptr};
static const char* kCittaSerbia[]    = {"Belgrado","Novi Sad","Niš","Kragujevac",nullptr};
static const char* kCittaUngheria[]  = {"Budapest","Debrecen","Miskolc","Pécs","Győr",nullptr};
static const char* kCittaCzechia[]   = {"Praga","Brno","Ostrava","Plzeň","Olomouc",nullptr};
static const char* kCittaIrlanda[]   = {"Dublino","Cork","Limerick","Galway","Waterford",nullptr};

static const char* kCittaUSA[]       = {"New York","Los Angeles","Chicago","Houston","Phoenix",
    "Philadelphia","San Antonio","San Diego","Dallas","San Francisco","Miami","Seattle",
    "Boston","Washington","Las Vegas","Atlanta","Denver","Portland",nullptr};
static const char* kCittaCanada[]    = {"Toronto","Montreal","Vancouver","Calgary","Edmonton",
    "Ottawa","Quebec City","Winnipeg","Hamilton","Halifax",nullptr};
static const char* kCittaMexico[]    = {"Città del Messico","Guadalajara","Monterrey","Puebla",
    "Tijuana","León","Juárez","Cancún","Mérida","Acapulco",nullptr};
static const char* kCittaCuba[]      = {"L'Avana","Santiago de Cuba","Camagüey","Holguín","Trinidad",nullptr};

static const char* kCittaBrasile[]   = {"San Paolo","Rio de Janeiro","Brasilia","Salvador",
    "Fortaleza","Belo Horizonte","Manaus","Curitiba","Recife","Porto Alegre",nullptr};
static const char* kCittaArgentina[] = {"Buenos Aires","Córdoba","Rosario","Mendoza",
    "La Plata","Tucumán","Mar del Plata","Salta",nullptr};
static const char* kCittaColombia[]  = {"Bogotá","Medellín","Cali","Barranquilla","Cartagena","Cúcuta",nullptr};
static const char* kCittaCile[]      = {"Santiago","Valparaíso","Concepción","Antofagasta","Viña del Mar",nullptr};
static const char* kCittaPeru[]      = {"Lima","Arequipa","Trujillo","Cusco","Chiclayo","Piura",nullptr};
static const char* kCittaVenezuela[] = {"Caracas","Maracaibo","Valencia","Barquisimeto","Ciudad Guayana",nullptr};
static const char* kCittaEcuador[]   = {"Quito","Guayaquil","Cuenca","Manta","Ambato",nullptr};
static const char* kCittaBolivia[]   = {"La Paz","Santa Cruz","Cochabamba","Sucre","Oruro",nullptr};
static const char* kCittaUruguay[]   = {"Montevideo","Salto","Ciudad de la Costa","Paysandú",nullptr};
static const char* kCittaParaguay[]  = {"Asunción","Ciudad del Este","Luque","San Lorenzo",nullptr};

static const char* kCittaNigeria[]   = {"Lagos","Abuja","Kano","Ibadan","Port Harcourt","Benin City",nullptr};
static const char* kCittaEgitto[]    = {"Il Cairo","Alessandria","Giza","Shubra el-Kheima","Port Said","Luxor","Aswan",nullptr};
static const char* kCittaSudAfrica[] = {"Johannesburg","Città del Capo","Durban","Pretoria","Port Elizabeth","Bloemfontein",nullptr};
static const char* kCittaKenya[]     = {"Nairobi","Mombasa","Nakuru","Kisumu","Eldoret",nullptr};
static const char* kCittaGhana[]     = {"Accra","Kumasi","Tamale","Sekondi-Takoradi",nullptr};
static const char* kCittaEtiopia[]   = {"Addis Abeba","Dire Dawa","Gondar","Mekele","Adama",nullptr};
static const char* kCittaMarocco[]   = {"Casablanca","Rabat","Marrakech","Fès","Tangeri","Agadir",nullptr};
static const char* kCittaTunisia[]   = {"Tunisi","Sfax","Sousse","Sfax","Kairouan","Gabès",nullptr};
static const char* kCittaAlgeria[]   = {"Algeri","Orano","Costantina","Annaba","Blida","Batna",nullptr};
static const char* kCittaRepDemCongo[]= {"Kinshasa","Lubumbashi","Mbuji-Mayi","Kananga","Kisangani",nullptr};
static const char* kCittaTanzania[]  = {"Dar es Salaam","Dodoma","Mwanza","Arusha","Mbeya",nullptr};
static const char* kCittaSenegal[]   = {"Dakar","Touba","Thiès","Ziguinchor",nullptr};

static const char* kCittaCina[]      = {"Pechino","Shanghai","Guangzhou","Chongqing","Tianjin",
    "Shenzhen","Chengdu","Nanchino","Wuhan","Xi'an","Hangzhou","Harbin","Suzhou",nullptr};
static const char* kCittaGiappone[]  = {"Tokyo","Osaka","Nagoya","Sapporo","Fukuoka",
    "Kyoto","Kobe","Kawasaki","Saitama","Hiroshima","Sendai","Yokohama",nullptr};
static const char* kCittaIndia[]     = {"Mumbai","Delhi","Bangalore","Hyderabad","Ahmedabad",
    "Chennai","Kolkata","Surat","Pune","Jaipur","Lucknow","Kanpur","Nagpur","Indore",nullptr};
static const char* kCittaCorea[]     = {"Seoul","Busan","Incheon","Daegu","Daejeon","Gwangju","Suwon",nullptr};
static const char* kCittaVietnam[]   = {"Ho Chi Minh City","Hanoi","Da Nang","Hải Phòng","Cần Thơ",nullptr};
static const char* kCittaTailandia[] = {"Bangkok","Nonthaburi","Pak Kret","Hat Yai","Chiang Mai","Pattaya",nullptr};
static const char* kCittaIndonesia[] = {"Giacarta","Surabaya","Bandung","Bekasi","Medan","Semarang","Makassar",nullptr};
static const char* kCittaFilippine[] = {"Manila","Davao City","Caloocan","Zamboanga","Cebu City",nullptr};
static const char* kCittaPakistan[]  = {"Karachi","Lahore","Islamabad","Rawalpindi","Faisalabad","Multan",nullptr};
static const char* kCittaBangladesh[]= {"Dacca","Chittagong","Khulna","Rajshahi","Sylhet",nullptr};
static const char* kCittaMalesia[]   = {"Kuala Lumpur","Penang","Ipoh","Johor Bahru","Kota Kinabalu",nullptr};
static const char* kCittaNepal[]     = {"Katmandu","Pokhara","Lalitpur","Bhaktapur","Biratnagar",nullptr};
static const char* kCittaSingapore[] = {"Singapore",nullptr};

static const char* kCittaTurchia[]   = {"Istanbul","Ankara","Smirne","Bursa","Adana","Antalya","Gaziantep","Konya",nullptr};
static const char* kCittaIsraele[]   = {"Tel Aviv","Gerusalemme","Haifa","Rishon LeZion","Petah Tikva","Ashdod",nullptr};
static const char* kCittaArabiaSaudita[]= {"Riyad","Jedda","La Mecca","Medina","Dammam","Taif",nullptr};
static const char* kCittaIran[]      = {"Teheran","Mashhad","Isfahan","Karaj","Tabriz","Shiraz","Qom",nullptr};
static const char* kCittaIraq[]      = {"Baghdad","Bassora","Mosul","Erbil","Najaf","Karbala",nullptr};
static const char* kCittaUAE[]       = {"Dubai","Abu Dhabi","Sharjah","Al Ain","Ajman",nullptr};
static const char* kCittaGiordania[] = {"Amman","Zarqa","Irbid","Russeifa","Aqaba",nullptr};
static const char* kCittaKuwait[]    = {"Kuwait City","Hawalli","Salmiya","Farwaniya",nullptr};
static const char* kCittaQatar[]     = {"Doha","Al Rayyan","Al Wakrah","Al Khor",nullptr};
static const char* kCittaLibano[]    = {"Beirut","Tripoli","Sidone","Tiro","Zahle",nullptr};

static const char* kCittaAustralia[] = {"Sydney","Melbourne","Brisbane","Perth","Adelaide",
    "Gold Coast","Canberra","Newcastle","Hobart","Darwin",nullptr};
static const char* kCittaNZ[]        = {"Auckland","Wellington","Christchurch","Hamilton","Dunedin",nullptr};
static const char* kCittaPNG[]       = {"Port Moresby","Lae","Arawa","Mount Hagen",nullptr};

/* ── Paesi per continente ─────────────────────────────────────────── */
static const PaeseGeo kPaesiEuropa[] = {
    {"Italia",kCittaItalia}, {"Francia",kCittaFrancia}, {"Germania",kCittaGermania},
    {"Spagna",kCittaSpagna}, {"Regno Unito",kCittaUK}, {"Portogallo",kCittaPortogallo},
    {"Grecia",kCittaGrecia}, {"Paesi Bassi",kCittaOlanda}, {"Belgio",kCittaBelgio},
    {"Svizzera",kCittaSvizzera}, {"Austria",kCittaAustria}, {"Svezia",kCittaSvezia},
    {"Norvegia",kCittaNorvegia}, {"Danimarca",kCittaDanimarca}, {"Finlandia",kCittaFinlandia},
    {"Polonia",kCittaPolonia}, {"Russia",kCittaRussia}, {"Ucraina",kCittaUcraina},
    {"Romania",kCittaRomania}, {"Bulgaria",kCittaBulgaria}, {"Croazia",kCittaCroazia},
    {"Serbia",kCittaSerbia}, {"Ungheria",kCittaUngheria}, {"Rep. Ceca",kCittaCzechia},
    {"Irlanda",kCittaIrlanda},
    {nullptr, nullptr}
};
static const PaeseGeo kPaesiNordAmerica[] = {
    {"USA",kCittaUSA}, {"Canada",kCittaCanada}, {"Messico",kCittaMexico}, {"Cuba",kCittaCuba},
    {nullptr, nullptr}
};
static const PaeseGeo kPaesiSudAmerica[] = {
    {"Brasile",kCittaBrasile}, {"Argentina",kCittaArgentina}, {"Colombia",kCittaColombia},
    {"Cile",kCittaCile}, {"Perù",kCittaPeru}, {"Venezuela",kCittaVenezuela},
    {"Ecuador",kCittaEcuador}, {"Bolivia",kCittaBolivia}, {"Uruguay",kCittaUruguay},
    {"Paraguay",kCittaParaguay},
    {nullptr, nullptr}
};
static const PaeseGeo kPaesiAfrica[] = {
    {"Nigeria",kCittaNigeria}, {"Egitto",kCittaEgitto}, {"Sud Africa",kCittaSudAfrica},
    {"Kenya",kCittaKenya}, {"Ghana",kCittaGhana}, {"Etiopia",kCittaEtiopia},
    {"Marocco",kCittaMarocco}, {"Tunisia",kCittaTunisia}, {"Algeria",kCittaAlgeria},
    {"Rep. Dem. Congo",kCittaRepDemCongo}, {"Tanzania",kCittaTanzania}, {"Senegal",kCittaSenegal},
    {nullptr, nullptr}
};
static const PaeseGeo kPaesiAsia[] = {
    {"Cina",kCittaCina}, {"Giappone",kCittaGiappone}, {"India",kCittaIndia},
    {"Corea del Sud",kCittaCorea}, {"Vietnam",kCittaVietnam}, {"Thailandia",kCittaTailandia},
    {"Indonesia",kCittaIndonesia}, {"Filippine",kCittaFilippine}, {"Pakistan",kCittaPakistan},
    {"Bangladesh",kCittaBangladesh}, {"Malesia",kCittaMalesia}, {"Nepal",kCittaNepal},
    {"Singapore",kCittaSingapore},
    {nullptr, nullptr}
};
static const PaeseGeo kPaesiMedioOriente[] = {
    {"Turchia",kCittaTurchia}, {"Israele",kCittaIsraele},
    {"Arabia Saudita",kCittaArabiaSaudita}, {"Iran",kCittaIran}, {"Iraq",kCittaIraq},
    {"UAE",kCittaUAE}, {"Giordania",kCittaGiordania}, {"Kuwait",kCittaKuwait},
    {"Qatar",kCittaQatar}, {"Libano",kCittaLibano},
    {nullptr, nullptr}
};
static const PaeseGeo kPaesiOceania[] = {
    {"Australia",kCittaAustralia}, {"Nuova Zelanda",kCittaNZ},
    {"Papua Nuova Guinea",kCittaPNG},
    {nullptr, nullptr}
};

static const ContinenteGeo kGeoData[] = {
    {"Europa",       kPaesiEuropa},
    {"Nord America", kPaesiNordAmerica},
    {"Sud America",  kPaesiSudAmerica},
    {"Africa",       kPaesiAfrica},
    {"Asia",         kPaesiAsia},
    {"Medio Oriente",kPaesiMedioOriente},
    {"Oceania",      kPaesiOceania},
    {nullptr, nullptr}
};

static const PaeseGeo* findPaesi(const QString& cont) {
    for (int i = 0; kGeoData[i].nome; ++i)
        if (QString::fromUtf8(kGeoData[i].nome) == cont)
            return kGeoData[i].paesi;
    return nullptr;
}

static const char* const* findCitta(const QString& cont, const QString& paese) {
    const PaeseGeo* paesi = findPaesi(cont);
    if (!paesi) return nullptr;
    for (int i = 0; paesi[i].nome; ++i)
        if (QString::fromUtf8(paesi[i].nome) == paese)
            return paesi[i].citta;
    return nullptr;
}

} // namespace

/* ─────────────────────────────────────────────────────────────────
   Slot nominati — Carta Astrale (barra azioni)
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onAstraleRunToggled()
{
    if (m_astraleRunBtn->property("running").toBool())
        onAstraleStopClicked();
    else
        onAstraleRunClicked();
}

void RicercaPage::onAstraleSavePdf()
{
    RicercaPage::esportaPdf(m_astraleOutput, "Carta Astrale", this);
}

void RicercaPage::onAstraleSaveMd()
{
    RicercaPage::salvaMarkdown(m_astraleOutput, "Carta Astrale", this);
}

void RicercaPage::onAstraleClear()
{
    m_astraleOutput->clear();
}

/* ─────────────────────────────────────────────────────────────────
   Slot nominati — Ruota Karmica (barra azioni)
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onKarmicaSavePdf()
{
    RicercaPage::esportaPdf(m_karmicaOutput, "Ruota Karmica", this);
}

void RicercaPage::onKarmicaSaveMd()
{
    RicercaPage::salvaMarkdown(m_karmicaOutput, "Ruota Karmica", this);
}

void RicercaPage::onKarmicaClear()
{
    m_karmicaOutput->clear();
}

/* ─────────────────────────────────────────────────────────────────
   Slot — Salva la ruota astrale grafica come PNG
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onSalvaChartPng()
{
    if (!m_natalChart) return;

    const QString defaultName =
        QString("ruota_astrale_%1.png")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    const QString path = QFileDialog::getSaveFileName(
        this,
        "\xf0\x9f\x96\xbc  Salva ruota astrale come PNG",   /* 🖼 */
        QDir::homePath() + "/" + defaultName,
        "Immagine PNG (*.png);;Tutti i file (*)");

    if (path.isEmpty()) return;

    /* grab() cattura il widget al suo size corrente; scala a 2× per qualità */
    const QPixmap pix = m_natalChart->grab();
    if (pix.isNull()) {
        QMessageBox::warning(this, "Errore",
            "\xe2\x9a\xa0  La ruota non contiene dati.\n"
            "Compila prima i dati natali e clicca \xf0\x9f\x94\xae Compila Ruota Karmica.");
        return;
    }

    const QPixmap hires = pix.scaled(
        pix.size() * 2,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);

    if (hires.save(path, "PNG")) {
        QMessageBox::information(this, "Salvato",
            "\xe2\x9c\x85  Ruota astrale salvata:\n" + path);
    } else {
        QMessageBox::warning(this, "Errore",
            "\xe2\x9d\x8c  Impossibile salvare il file:\n" + path);
    }
}


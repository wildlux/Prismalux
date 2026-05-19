#include "settings_page.h"
#include "qr_scanner_dialog.h"
#include "../ai_client.h"
#include "../local_llm_client.h"
#include "../thermal_monitor.h"
#include "../rag_engine_simple.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QScrollBar>
#include <QGroupBox>
#include <QScrollArea>
#include <QFileDialog>
#include <QSettings>
#include <QScroller>
#include <QScrollerProperties>
#include <QFrame>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QRegularExpression>
#include <algorithm>

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
SettingsPage::SettingsPage(AiClient* ai, RagEngineSimple* rag,
                           LocalLlmClient* localLlm, QWidget* parent)
    : QWidget(parent), m_ai(ai), m_rag(rag), m_localLlm(localLlm)
{
    setObjectName("SettingsPage");
    QSettings s("Prismalux", "Mobile");

    /* ── Scroll area per tutto il contenuto ── */
    auto* scrollArea = new QScrollArea(this);
    m_scrollArea = scrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setObjectName("SettingsScroll");
    /* scorrimento a un solo dito su Android */
    QScroller::grabGesture(scrollArea->viewport(), QScroller::TouchGesture);
    /* disabilita l'effetto overshoot (causa glitch visivi su Android) */
    QScroller* qs = QScroller::scroller(scrollArea->viewport());
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
    /* impedisce scorrimento orizzontale e rimuove il bordo */
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setSpacing(12);
    vbox->setContentsMargins(8, 8, 8, 8);
    scrollArea->setWidget(inner);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scrollArea);

    /* ══════════════════════════════════
       Sezione: Server Ollama
       ══════════════════════════════════ */
    auto* serverGroup = new QGroupBox("\xf0\x9f\x8c\x90  Prismalux Desktop (LAN)", inner);  // 🌐
    serverGroup->setObjectName("SettingsGroup");
    auto* serverForm = new QFormLayout(serverGroup);
    serverForm->setSpacing(10);
    serverForm->setRowWrapPolicy(QFormLayout::WrapAllRows);

    m_hostEdit = new QLineEdit(s.value("server/host", "192.168.1.165").toString(), serverGroup);
    m_hostEdit->setPlaceholderText("IP del PC con Prismalux (es. 192.168.1.165)");
    m_hostEdit->setObjectName("SettingsInput");

    m_portSpin = new QSpinBox(serverGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(s.value("server/port", 11434).toInt());
    m_portSpin->setObjectName("SettingsInput");

    m_tokenEdit = new QLineEdit(s.value("server/token", "").toString(), serverGroup);
    m_tokenEdit->setPlaceholderText("Token segreto LAN (lascia vuoto per Ollama diretto)");
    m_tokenEdit->setObjectName("SettingsInput");
    m_tokenEdit->setEchoMode(QLineEdit::Password);

    auto* portInfoLbl = new QLabel(
        "<small>Porta 11500 = LAN server Prismalux (richiede token)<br>"
        "Porta 11434 = Ollama diretto (nessun token)<br>"
        "IP: indirizzo del PC sulla rete Wi-Fi</small>",
        serverGroup);
    portInfoLbl->setTextFormat(Qt::RichText);
    portInfoLbl->setWordWrap(true);

    /* Bottone QR scanner (visibile solo se Multimedia compilato) */
#ifdef HAVE_MULTIMEDIA
    auto* qrScanBtn = new QPushButton(
        "\xf0\x9f\x93\xb7  Scansiona QR dal PC", serverGroup);  /* 📷 */
    qrScanBtn->setObjectName("PrimaryBtn");
    qrScanBtn->setToolTip(
        "Apri la fotocamera e inquadra il QR code mostrato in\n"
        "Prismalux Desktop \xe2\x86\x92 LAN & WAN \xe2\x86\x92 QR Connetti.\n"
        "IP, porta e token verranno compilati automaticamente.");
#endif

    serverForm->addRow("Indirizzo IP:", m_hostEdit);
    serverForm->addRow("Porta:",        m_portSpin);
    serverForm->addRow("\xf0\x9f\x94\x91  Token:", m_tokenEdit);  /* 🔑 */
#ifdef HAVE_MULTIMEDIA
    serverForm->addRow(qrScanBtn);
#endif
    serverForm->addRow(portInfoLbl);

    /* Messaggio iniziale: indica se c'è già un token salvato */
    const QString savedToken = s.value("server/token", "").toString();
    const QString initialStatus = savedToken.isEmpty()
        ? "\xe2\x9a\xa0\xef\xb8\x8f  Nessun token configurato \xe2\x80\x94 segui la guida sopra"  // ⚠️
        : QString("\xf0\x9f\x94\x91  Token salvato (\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2%1) \xe2\x80\x94 premi Testa per verificare")  // 🔑 ••••
          .arg(savedToken.right(4));
    m_connStatus = new QLabel(initialStatus, serverGroup);
    m_connStatus->setWordWrap(true);
    serverForm->addRow(m_connStatus);

    /* Link Web Chat — aggiornato live a ogni modifica di IP/porta/token */
    m_webLinkLbl = new QLabel(serverGroup);
    m_webLinkLbl->setTextFormat(Qt::RichText);
    m_webLinkLbl->setWordWrap(true);
    m_webLinkLbl->setOpenExternalLinks(true);
    m_webLinkLbl->setTextInteractionFlags(
        Qt::TextBrowserInteraction | Qt::TextSelectableByMouse);
    serverForm->addRow("\xf0\x9f\x8c\x90  Web Chat:", m_webLinkLbl);  /* 🌐 */
    onUpdateWebLink();   /* popola subito con i valori caricati */

    auto* serverBtnRow = new QHBoxLayout;
    m_applyBtn = new QPushButton("Applica", serverGroup);
    m_testBtn  = new QPushButton("\xf0\x9f\x94\x8c  Testa", serverGroup);  // 🔌
    m_applyBtn->setObjectName("PrimaryBtn");
    m_testBtn->setObjectName("SecondaryBtn");
    serverBtnRow->addWidget(m_applyBtn);
    serverBtnRow->addWidget(m_testBtn);
    serverForm->addRow(serverBtnRow);
    vbox->addWidget(serverGroup);

    /* ══════════════════════════════════
       Sezione: Modello LLM
       ══════════════════════════════════ */
    auto* modelGroup = new QGroupBox("\xf0\x9f\xa4\x96  Modello LLM", inner);  // 🤖
    modelGroup->setObjectName("SettingsGroup");
    auto* modelForm = new QFormLayout(modelGroup);
    modelForm->setSpacing(10);
    modelForm->setRowWrapPolicy(QFormLayout::WrapAllRows);

    m_modelCombo = new QComboBox(modelGroup);
    m_modelCombo->setObjectName("SettingsInput");
    const QStringList suggestions = {
        "llama3.2:1b",
        "deepseek-r1:1.5b",
        "llama3.2:3b",
        "phi3:mini",
        "moondream:latest",
        "mistral:7b-instruct",
        "Altro...",
    };
    m_modelCombo->addItems(suggestions);
    const QString savedModel = s.value("server/model", "llama3.2:1b").toString();
    const int savedIdx = m_modelCombo->findText(savedModel);
    if (savedIdx >= 0) {
        m_modelCombo->setCurrentIndex(savedIdx);
    } else {
        m_modelCombo->setCurrentIndex(m_modelCombo->count() - 1); /* Altro... */
    }

    m_modelStatus = new QLabel("", modelGroup);

    /* Riga "Dal server" + "❓ Guida" */
    auto* serverRow = new QHBoxLayout;
    m_refreshBtn = new QPushButton("\xf0\x9f\x94\x84  Dal server", modelGroup);  // 🔄
    m_refreshBtn->setObjectName("SecondaryBtn");
    m_modelHelpBtn = new QPushButton("\xe2\x9d\x93  Guida", modelGroup);  // ❓
    m_modelHelpBtn->setObjectName("SecondaryBtn");
    serverRow->addWidget(m_refreshBtn, 1);
    serverRow->addWidget(m_modelHelpBtn);

    /* Riga "Altro..." — solo testo, visibile quando si sceglie Altro... */
    m_customModelRow = new QWidget(modelGroup);
    auto* customLay  = new QHBoxLayout(m_customModelRow);
    customLay->setContentsMargins(0, 0, 0, 0);
    m_customModelEdit = new QLineEdit(m_customModelRow);
    m_customModelEdit->setObjectName("SettingsInput");
    m_customModelEdit->setPlaceholderText("es. llama3.2:1b, mistral:7b");
    if (savedIdx < 0) m_customModelEdit->setText(savedModel);
    customLay->addWidget(m_customModelEdit, 1);
    m_customModelRow->setVisible(savedIdx < 0);

    /* Pulsanti rapidi modelli leggeri */
    auto* quickRow = new QHBoxLayout;
    auto* btn1b    = new QPushButton("llama3.2:1b", modelGroup);
    auto* btn15b   = new QPushButton("deepseek-r1:1.5b", modelGroup);
    btn1b->setObjectName("SecondaryBtn");
    btn15b->setObjectName("SecondaryBtn");
    quickRow->addWidget(btn1b);
    quickRow->addWidget(btn15b);

    modelForm->addRow("Modello:", m_modelCombo);
    modelForm->addRow(m_customModelRow);
    modelForm->addRow(serverRow);
    modelForm->addRow(quickRow);
    modelForm->addRow(m_modelStatus);
    vbox->addWidget(modelGroup);

    /* Selezione rapida → aggiorna combo e applica subito */
    auto quickApply = [this](const QString& model) {
        const int idx = m_modelCombo->findText(model);
        if (idx >= 0) {
            m_modelCombo->setCurrentIndex(idx);
        } else {
            const int altroIdx = m_modelCombo->findText("Altro...");
            if (altroIdx >= 0) m_modelCombo->setCurrentIndex(altroIdx);
            if (m_customModelEdit) m_customModelEdit->setText(model);
        }
        emit serverChanged(m_hostEdit->text().trimmed(),
                           m_portSpin->value(), model);
        m_connStatus->setText("\xf0\x9f\x9f\xa2  Modello: " + model);
    };
    connect(btn1b,  &QPushButton::clicked, this, [quickApply]{ quickApply("llama3.2:1b"); });
    connect(btn15b, &QPushButton::clicked, this, [quickApply]{ quickApply("deepseek-r1:1.5b"); });

    /* ══════════════════════════════════
       Sezione: Parametri AI
       ══════════════════════════════════ */
    auto* paramsGroup = new QGroupBox("\xf0\x9f\x8e\x9b  Parametri AI", inner);  // 🎛
    paramsGroup->setObjectName("SettingsGroup");
    auto* paramsForm = new QFormLayout(paramsGroup);
    paramsForm->setSpacing(10);
    paramsForm->setRowWrapPolicy(QFormLayout::WrapAllRows);

    m_tempSlider = new QSlider(Qt::Horizontal, paramsGroup);
    m_tempSlider->setRange(0, 100);     // 0.00 → 1.00
    m_tempSlider->setValue(10);         // default 0.10
    m_tempSlider->setObjectName("TempSlider");
    m_tempLbl = new QLabel("0.10", paramsGroup);
    m_tempLbl->setObjectName("ValueLabel");
    auto* tempRow = new QHBoxLayout;
    tempRow->addWidget(m_tempSlider, 1);
    tempRow->addWidget(m_tempLbl);

    m_predictSpin = new QSpinBox(paramsGroup);
    m_predictSpin->setRange(128, 4096);
    m_predictSpin->setValue(1024);
    m_predictSpin->setSingleStep(128);
    m_predictSpin->setObjectName("SettingsInput");

    paramsForm->addRow("Temperatura:", tempRow);
    paramsForm->addRow("Max token:",   m_predictSpin);
    vbox->addWidget(paramsGroup);

    /* ══════════════════════════════════
       Sezione: RAG documenti
       ══════════════════════════════════ */
    auto* ragGroup = new QGroupBox("\xf0\x9f\x93\x9a  RAG Documenti", inner);  // 📚
    ragGroup->setObjectName("SettingsGroup");
    auto* ragForm = new QFormLayout(ragGroup);
    ragForm->setSpacing(10);
    ragForm->setRowWrapPolicy(QFormLayout::WrapAllRows);

    m_ragPathEdit = new QLineEdit(s.value("rag/indexPath", "").toString(), ragGroup);
    m_ragPathEdit->setPlaceholderText("File .txt / .md / .pdf  (usa Sfoglia)");
    m_ragPathEdit->setObjectName("SettingsInput");
    m_ragLoadBtn  = new QPushButton("\xf0\x9f\x93\x82  Sfoglia e carica", ragGroup);  // 📂
    m_ragLoadBtn->setObjectName("PrimaryBtn");
    m_ragStatus = new QLabel(
        m_rag->chunkCount() > 0
            ? QString("\xe2\x9c\x85  %1 chunk in memoria")  // ✅
              .arg(m_rag->chunkCount())
            : "Nessun documento caricato",
        ragGroup);
    m_ragStatus->setWordWrap(true);

    ragForm->addRow("Percorso:",  m_ragPathEdit);
    ragForm->addRow(m_ragLoadBtn);
    ragForm->addRow(m_ragStatus);
    vbox->addWidget(ragGroup);

    /* ══════════════════════════════════
       Sezione: LLM Locale (offline)
       ══════════════════════════════════ */
    auto* localGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x96\xa5\xef\xb8\x8f")   /* 🖥️ */
        + "  LLM Locale — offline (Redmi Note 8 Pro)", inner);
    localGroup->setObjectName("SettingsGroup");
    auto* localForm = new QFormLayout(localGroup);
    localForm->setSpacing(10);
    localForm->setRowWrapPolicy(QFormLayout::WrapAllRows);

    m_localModeChk = new QCheckBox("Usa LLM locale quando offline", localGroup);
    m_localModeChk->setChecked(
        m_localLlm && m_ai->isLocalMode());
    localForm->addRow(m_localModeChk);

    /* Stato modello */
    const bool modelOk = m_localLlm && LocalLlmClient::modelExists();
    {
        QString statusTxt;
        if (modelOk) {
            const QString p = LocalLlmClient::firstAvailableModelPath();
            statusTxt = QString::fromUtf8("\xe2\x9c\x85")  /* ✅ */
                + " " + LocalLlmClient::loadedModelName(p) + " pronto";
        } else {
            statusTxt = QString::fromUtf8("\xe2\x8f\xac")  /* ⏬ */
                + " Nessun modello scaricato";
        }
        m_localStatusLbl = new QLabel(statusTxt, localGroup);
    }
    m_localStatusLbl->setWordWrap(true);
    localForm->addRow("Stato:", m_localStatusLbl);

    /* Selezione modello da scaricare */
    m_dlModelCombo = new QComboBox(localGroup);
    m_dlModelCombo->setObjectName("SettingsInput");
    for (int i = 0; i < LocalLlmClient::kNumModels; ++i) {
        const auto& spec = LocalLlmClient::kModels[i];
        /* Appende [✅] se già scaricato */
        const bool exists = LocalLlmClient::modelExists(i);
        m_dlModelCombo->addItem(
            (exists
                ? QString::fromUtf8("\xe2\x9c\x85 ")  /* ✅ */
                : QString::fromUtf8("\xe2\x8f\xac "))  /* ⏬ */
            + QString::fromUtf8(spec.displayName));
    }
    localForm->addRow("Modello:", m_dlModelCombo);

    /* Pulsanti download */
    auto* dlRow = new QHBoxLayout;
    /* Costruisce il testo iniziale del bottone in base al modello selezionato (idx=0) */
    auto buildDlBtnText = [this]() -> QString {
        const int idx = m_dlModelCombo ? m_dlModelCombo->currentIndex() : 0;
        if (idx < 0 || idx >= LocalLlmClient::kNumModels)
            return QString::fromUtf8("\xf0\x9f\x93\xa5 Scarica");  /* 📥 */
        const auto& spec = LocalLlmClient::kModels[idx];
        if (LocalLlmClient::modelExists(idx))
            return QString::fromUtf8("\xe2\x9c\x85")  /* ✅ */
                + " Già scaricato";
        if (LocalLlmClient::hasPartialDownload(idx))
            return QString::fromUtf8("\xf0\x9f\x93\xa5")  /* 📥 */
                + QString(" Riprendi (%1 MB scaricati)")
                  .arg(LocalLlmClient::partialDownloadSize(idx) / (1024 * 1024));
        return QString::fromUtf8("\xf0\x9f\x93\xa5")  /* 📥 */
            + QString(" Scarica  (~%1 MB)").arg(spec.sizeMB);
    };
    m_dlModelBtn = new QPushButton(buildDlBtnText(), localGroup);
    m_dlModelBtn->setObjectName("PrimaryBtn");
    m_dlModelBtn->setMinimumHeight(44);
    m_dlModelBtn->setEnabled(!LocalLlmClient::modelExists(0));  /* idx iniziale = 0 */
    m_dlCancelBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9d\x8c Annulla"), localGroup);
    m_dlCancelBtn->setObjectName("SecondaryBtn");
    m_dlCancelBtn->setVisible(false);
    dlRow->addWidget(m_dlModelBtn, 1);
    dlRow->addWidget(m_dlCancelBtn);
    localForm->addRow(dlRow);

    m_dlProgress = new QProgressBar(localGroup);
    m_dlProgress->setRange(0, 100);
    m_dlProgress->setVisible(false);
    m_dlProgress->setFixedHeight(18);
    localForm->addRow(m_dlProgress);

    /* Temperatura live */
    m_tempLiveLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x8c\xa1")       /* 🌡️ */
        + " CPU: —",
        localGroup);
    m_thermalMsgLbl = new QLabel("", localGroup);
    m_thermalMsgLbl->setWordWrap(true);
    m_thermalMsgLbl->setVisible(false);
    localForm->addRow("Temperatura:", m_tempLiveLbl);
    localForm->addRow(m_thermalMsgLbl);

    auto* threshLbl = new QLabel(
        QString::fromUtf8("<small>"
        "\xf0\x9f\x9f\xa1 Soglia pausa: <b>72°C</b> (5 s min) &nbsp; "
        "\xf0\x9f\x94\xb4 Soglia critica: <b>78°C</b><br>"
        "La generazione si interrompe automaticamente per proteggere il dispositivo."
        "</small>"),
        localGroup);
    threshLbl->setTextFormat(Qt::RichText);
    threshLbl->setWordWrap(true);
    localForm->addRow(threshLbl);
    vbox->addWidget(localGroup);

    vbox->addStretch();

    /* ── Connessioni ── */
    connect(m_applyBtn,   &QPushButton::clicked, this, &SettingsPage::onApplyServer);
    connect(m_testBtn,    &QPushButton::clicked, this, &SettingsPage::onTestConnection);
    connect(m_refreshBtn, &QPushButton::clicked, this, &SettingsPage::onRefreshModels);

    /* Mostra/nascondi riga Altro... */
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        m_customModelRow->setVisible(m_modelCombo->itemText(i) == "Altro...");
    });

    /* Guida ❓ */
    connect(m_modelHelpBtn, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this,
            QString::fromUtf8("\xe2\x9d\x93  Guida modelli Ollama"),
            "<b>Formato nome:</b> <tt>nome:versione</tt><br><br>"
            "\xf0\x9f\x9f\xa2 <b>llama3.2:1b</b> — 1.3 GB, velocissimo<br>"
            "\xf0\x9f\x9f\xa2 <b>deepseek-r1:1.5b</b> — 1.1 GB, ragionamento<br>"
            "\xf0\x9f\x9f\xa1 <b>llama3.2:3b</b> — 2.0 GB, bilanciato<br>"
            "\xf0\x9f\x9f\xa1 <b>phi3:mini</b> — 2.2 GB, buon italiano<br>"
            "\xf0\x9f\x94\xb4 <b>mistral:7b-instruct</b> — 4.4 GB, potente<br><br>"
            "Vedi tutti su <b>ollama.com/library</b><br>"
            "Premi <b>Dal server \xf0\x9f\x94\x84</b> per caricare i modelli"
            " gi\xc3\xa0 installati.");
    });
    connect(m_ragLoadBtn, &QPushButton::clicked, this, &SettingsPage::onLoadRag);
    connect(m_ai, &AiClient::modelsReady, this, &SettingsPage::onModelsReady);

    /* Aggiorna il link web ogni volta che IP / porta / token cambiano */
    connect(m_hostEdit,  &QLineEdit::textChanged,
            this, &SettingsPage::onUpdateWebLink);
    connect(m_portSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsPage::onUpdateWebLink);
    connect(m_tokenEdit, &QLineEdit::textChanged,
            this, &SettingsPage::onUpdateWebLink);

#ifdef HAVE_MULTIMEDIA
    connect(qrScanBtn, &QPushButton::clicked, this, &SettingsPage::onQrScanClicked);
#endif

    /* LLM Locale */
    connect(m_localModeChk, &QCheckBox::toggled,
            this, &SettingsPage::onLocalModeToggled);
    /* Sincronizza il checkbox quando il backend cambia dalla ChatPage */
    connect(m_ai, &AiClient::localModeChanged,
            this, &SettingsPage::onAiLocalModeChanged);
    connect(m_dlModelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onDlModelSelected);
    connect(m_dlModelBtn,   &QPushButton::clicked,
            this, &SettingsPage::onDownloadModelClicked);
    connect(m_dlCancelBtn,  &QPushButton::clicked,
            this, [this]() { if (m_localLlm) m_localLlm->cancelDownload(); });

    if (m_localLlm) {
        connect(m_localLlm, &LocalLlmClient::downloadProgress,
                this, &SettingsPage::onDlProgress);
        connect(m_localLlm, &LocalLlmClient::downloadFinished,
                this, &SettingsPage::onDlFinished);
        connect(m_localLlm, &LocalLlmClient::downloadError,
                this, &SettingsPage::onDlError);
        connect(m_localLlm, &LocalLlmClient::tempUpdated,
                this, &SettingsPage::onLocalTempUpdated);
        connect(m_localLlm, &LocalLlmClient::thermalPause,
                this, &SettingsPage::onLocalThermalPause);
        connect(m_localLlm, &LocalLlmClient::thermalResume,
                this, &SettingsPage::onLocalThermalResume);
        connect(m_localLlm, &LocalLlmClient::modelLoaded,
                this, &SettingsPage::onLocalModelLoaded);
        connect(m_localLlm, &LocalLlmClient::loadError,
                this, &SettingsPage::onLocalLoadError);
    }

    /* Blocca scorrimento orizzontale residuo dal QScroller touch */
    connect(scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &SettingsPage::onResetHScroll);

    connect(m_tempSlider, &QSlider::valueChanged, this, [this](int v) {
        const double t = v / 100.0;
        m_tempLbl->setText(QString::number(t, 'f', 2));
        m_ai->setTemperature(t);
    });
    connect(m_predictSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_ai, &AiClient::setNumPredict);
}

/* ── onApplyServer ───────────────────────────────────────────── */
void SettingsPage::onApplyServer()
{
    const QString host  = m_hostEdit->text().trimmed();
    const int     port  = m_portSpin->value();
    const bool    isAltro = (m_modelCombo->currentText() == "Altro...");
    const QString model = (isAltro && m_customModelEdit)
        ? m_customModelEdit->text().trimmed()
        : m_modelCombo->currentText().trimmed();
    const QString token = m_tokenEdit ? m_tokenEdit->text().trimmed() : QString();
    if (host.isEmpty() || model.isEmpty()) return;

    /* Salva il token in QSettings e aggiorna AiClient */
    QSettings s("Prismalux", "Mobile");
    s.setValue("server/token", token);
    m_ai->setToken(token);

    emit serverChanged(host, port, model);
    m_connStatus->setText("\xf0\x9f\x94\x84  Impostazioni applicate");  // 🔄
}

/* ── onTestConnection ────────────────────────────────────────── */
void SettingsPage::onTestConnection()
{
    const QString url = QString("http://%1:%2/api/tags")
                        .arg(m_hostEdit->text().trimmed())
                        .arg(m_portSpin->value());
    m_connStatus->setText("\xf0\x9f\x94\x84  Test in corso…");
    m_testBtn->setEnabled(false);

    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req{QUrl{url}};
    const QString token = m_tokenEdit ? m_tokenEdit->text().trimmed() : QString();
    if (!token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    auto* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam] {
        m_testBtn->setEnabled(true);
        if (reply->error() == QNetworkReply::NoError) {
            m_connStatus->setText(
                "\xe2\x9c\x85  Ollama raggiungibile — premi Aggiorna modelli");  // ✅
        } else {
            m_connStatus->setText(
                "\xe2\x9d\x8c  " + reply->errorString().left(80));  // ❌
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

/* ── onRefreshModels ─────────────────────────────────────────── */
void SettingsPage::onRefreshModels()
{
    m_modelStatus->setText("\xf0\x9f\x94\x84  Carico lista modelli…");
    m_refreshBtn->setEnabled(false);
    /* Sincronizza prima l'host/porta corrente */
    m_ai->setServer(m_hostEdit->text().trimmed(), m_portSpin->value());
    m_ai->fetchModels();
}

/* ── Estrae il numero di miliardi di parametri dal nome del modello ── */
static int modelSizeRank(const QString& name)
{
    /* Cerca pattern come :1b :1.5b :7b ecc. */
    const QRegularExpression re(R"([:_](\d+\.?\d*)[bB])");
    const auto m = re.match(name);
    if (m.hasMatch())
        return static_cast<int>(m.captured(1).toDouble() * 10);
    /* Tag speciali: mini/small/tiny prima degli unknown */
    const QString lower = name.toLower();
    if (lower.contains("tiny"))  return 5;
    if (lower.contains("small")) return 15;
    if (lower.contains("mini"))  return 40;
    return 9999;
}

/* ── onModelsReady ───────────────────────────────────────────── */
void SettingsPage::onModelsReady(const QStringList& models)
{
    m_refreshBtn->setEnabled(true);
    if (models.isEmpty()) {
        m_modelStatus->setText(
            "\xe2\x9a\xa0  Nessun modello trovato — "
            "avvia Ollama e scarica un modello.");
        return;
    }

    /* Ordina per dimensione: modelli piccoli in cima */
    QStringList sorted = models;
    std::sort(sorted.begin(), sorted.end(),
              [](const QString& a, const QString& b) {
        return modelSizeRank(a) < modelSizeRank(b);
    });

    const bool wasAltro = (m_modelCombo->currentText() == "Altro...");
    const QString current = wasAltro && m_customModelEdit
        ? m_customModelEdit->text().trimmed()
        : m_modelCombo->currentText();
    m_modelCombo->clear();
    m_modelCombo->addItems(sorted);
    m_modelCombo->addItem("Altro...");

    const int idx = m_modelCombo->findText(current);
    if (idx >= 0) {
        m_modelCombo->setCurrentIndex(idx);
        if (m_customModelRow) m_customModelRow->setVisible(false);
    } else {
        m_modelCombo->setCurrentIndex(0);
    }
    m_modelStatus->setText(
        QString("\xe2\x9c\x85  %1 modelli — seleziona e premi Applica").arg(sorted.size()));
}

/* ── onLoadRag ───────────────────────────────────────────────── */
void SettingsPage::onLoadRag()
{
#ifdef Q_OS_ANDROID
    /* Su Android: file picker nativo (supporta .txt .md .pdf) */
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Seleziona documento RAG",
        QString(),
        "Documenti (*.txt *.md *.pdf);;Tutti i file (*)");
    if (path.isEmpty()) return;
    m_ragPathEdit->setText(path);
#else
    const QString path = QFileDialog::getExistingDirectory(
        this, "Seleziona cartella documenti RAG",
        m_ragPathEdit->text().isEmpty()
            ? QDir::homePath()
            : m_ragPathEdit->text());
    if (path.isEmpty()) return;
    m_ragPathEdit->setText(path);
#endif

    m_ragStatus->setText("\xf0\x9f\x94\x84  Caricamento in corso…");
    m_ragLoadBtn->setEnabled(false);

    /* Salva path */
    QSettings s("Prismalux", "Mobile");
    s.setValue("rag/indexPath", path);

    emit ragIndexChanged(path);

    /* Timeout 10s: se loaded non arriva (file non trovato, errore I/O)
       il pulsante viene riabilitato per evitare che resti stuck. */
    auto* watchdog = new QTimer(this);
    watchdog->setSingleShot(true);
    watchdog->setInterval(10000);
    connect(watchdog, &QTimer::timeout, this, [this, watchdog] {
        watchdog->deleteLater();
        m_ragLoadBtn->setEnabled(true);
        m_ragStatus->setText(
            "\xe2\x9a\xa0  Timeout: nessuna risposta dal caricatore RAG");  // ⚠
    });
    watchdog->start();

    /* Aggiorna contatore (collegato a RagEngineSimple::loaded) */
    connect(m_rag, &RagEngineSimple::loaded, this, [this, watchdog](int n) {
        if (watchdog) { watchdog->stop(); watchdog->deleteLater(); }
        m_ragLoadBtn->setEnabled(true);
        m_ragStatus->setText(
            n > 0
                ? QString("\xe2\x9c\x85  %1 chunk caricati").arg(n)  // ✅
                : "\xe2\x9d\x8c  Nessun documento .txt trovato nel percorso");
    }, Qt::SingleShotConnection);
}

/* ── onQrScanClicked ─────────────────────────────────────────── */
void SettingsPage::onQrScanClicked()
{
#ifdef HAVE_MULTIMEDIA
    auto* dlg = new QrScannerDialog(this);

    /* Quando il QR viene decodificato, analizza l'URL e compila i campi */
    connect(dlg, &QrScannerDialog::urlFound, this, [this](const QString& raw) {
        /* Formato atteso: prismalux://IP:PORT?token=TOKEN
           Accetta anche http://IP:PORT/web?token=TOKEN (link web chat) */
        QUrl url(raw);
        const QString host  = url.host();
        const int     port  = url.port(-1);
        const QString token = QUrlQuery(url).queryItemValue("token");

        if (host.isEmpty()) {
            m_connStatus->setText(
                "\xe2\x9d\x8c  QR non riconosciuto come URL Prismalux.");
            return;
        }

        /* Compila i campi */
        if (m_hostEdit)  m_hostEdit->setText(host);
        if (m_portSpin && port > 0) m_portSpin->setValue(port);
        if (m_tokenEdit) m_tokenEdit->setText(token);

        /* Salva subito */
        QSettings s("Prismalux", "Mobile");
        s.setValue("server/host",  host);
        if (port > 0) s.setValue("server/port", port);
        s.setValue("server/token", token);
        m_ai->setToken(token);

        m_connStatus->setText(
            "\xf0\x9f\x94\x91  Configurazione importata dal QR \xe2\x80\x94 "  /* 🔑 */
            "premi Testa per verificare");
        onUpdateWebLink();
    });

    dlg->exec();
#endif
}

/* ══════════════════════════════════════════════════════════════
   Slot LLM Locale
   ══════════════════════════════════════════════════════════════ */

void SettingsPage::onAiLocalModeChanged(bool on)
{
    /* Aggiornamento esterno (da ChatPage): sincronizza senza loop */
    if (!m_localModeChk) return;
    m_localModeChk->blockSignals(true);
    m_localModeChk->setChecked(on);
    m_localModeChk->blockSignals(false);
}

void SettingsPage::onLocalModeToggled(bool on)
{
    if (!m_localLlm) return;
    m_ai->setLocalMode(on);
    if (on && !m_localLlm->isLoaded()) {
        const QString p = LocalLlmClient::firstAvailableModelPath();
        if (!p.isEmpty()) m_localLlm->loadModel(p);
    }
    m_localStatusLbl->setText(
        on
        ? (m_localLlm->isLoaded()
            ? QString::fromUtf8("\xf0\x9f\x9f\xa2") + " Modalità locale attiva"
            : QString::fromUtf8("\xe2\x8f\xb3") + " Caricamento modello...")
        : QString::fromUtf8("\xf0\x9f\x8c\x90") + " Modalità remota (Ollama)");
}

/* ── onDlModelSelected ──────────────────────────────────────── */
void SettingsPage::onDlModelSelected(int idx)
{
    if (idx < 0 || idx >= LocalLlmClient::kNumModels || !m_dlModelBtn) return;
    const auto& spec = LocalLlmClient::kModels[idx];
    if (LocalLlmClient::modelExists(idx)) {
        m_dlModelBtn->setText(
            QString::fromUtf8("\xe2\x9c\x85")  /* ✅ */
            + " Già scaricato");
        m_dlModelBtn->setEnabled(false);
    } else if (LocalLlmClient::hasPartialDownload(idx)) {
        m_dlModelBtn->setText(
            QString::fromUtf8("\xf0\x9f\x93\xa5")  /* 📥 */
            + QString(" Riprendi (%1 MB scaricati)")
              .arg(LocalLlmClient::partialDownloadSize(idx) / (1024 * 1024)));
        m_dlModelBtn->setEnabled(true);
    } else {
        m_dlModelBtn->setText(
            QString::fromUtf8("\xf0\x9f\x93\xa5")  /* 📥 */
            + QString(" Scarica  (~%1 MB)").arg(spec.sizeMB));
        m_dlModelBtn->setEnabled(true);
    }
}

void SettingsPage::onDownloadModelClicked()
{
    if (!m_localLlm) return;
    const int idx = m_dlModelCombo ? m_dlModelCombo->currentIndex() : 1;
    if (idx < 0 || idx >= LocalLlmClient::kNumModels) return;
    const auto& spec = LocalLlmClient::kModels[idx];

    m_dlModelBtn->setEnabled(false);
    m_dlCancelBtn->setVisible(true);
    m_dlProgress->setVisible(true);
    m_dlProgress->setValue(0);
    m_localStatusLbl->setText(
        QString::fromUtf8("\xe2\x8f\xac") + " Download in corso...");
    m_localLlm->downloadModel(
        QString::fromUtf8(spec.url),
        LocalLlmClient::modelPath(idx));
}

void SettingsPage::onDlProgress(qint64 received, qint64 total)
{
    if (total <= 0) return;
    const int pct = (int)(received * 100 / total);
    m_dlProgress->setValue(pct);
    const QString rx  = QString::number(received / (1024*1024)) + " MB";
    const QString tot = QString::number(total   / (1024*1024)) + " MB";
    m_localStatusLbl->setText(
        QString::fromUtf8("\xe2\x8f\xac")   /* ⏬ */
        + QString(" Download: %1 / %2 (%3%)").arg(rx, tot).arg(pct));
}

void SettingsPage::onDlFinished(const QString& path)
{
    m_dlCancelBtn->setVisible(false);
    m_dlProgress->setVisible(false);
    const QString name = LocalLlmClient::loadedModelName(path);
    m_localStatusLbl->setText(
        QString::fromUtf8("\xe2\x9c\x85")   /* ✅ */
        + " " + name + " scaricato — caricamento...");

    /* Aggiorna la voce nella combo (ora esiste) */
    const int idx = m_dlModelCombo ? m_dlModelCombo->currentIndex() : -1;
    if (idx >= 0 && m_dlModelCombo) {
        const QString label =
            QString::fromUtf8("\xe2\x9c\x85 ")  /* ✅ */
            + QString::fromUtf8(LocalLlmClient::kModels[idx].displayName);
        m_dlModelCombo->setItemText(idx, label);
        onDlModelSelected(idx);   /* aggiorna testo bottone */
    }

    if (m_localLlm)
        m_localLlm->loadModel(path);
}

void SettingsPage::onDlError(const QString& msg)
{
    m_dlCancelBtn->setVisible(false);
    m_dlProgress->setVisible(false);

    /* Aggiorna testo bottone in base allo stato del modello selezionato */
    const int idx = m_dlModelCombo ? m_dlModelCombo->currentIndex() : -1;
    if (idx >= 0) onDlModelSelected(idx);

    m_localStatusLbl->setText(
        QString::fromUtf8("\xe2\x9d\x8c")   /* ❌ */
        + " Errore download: " + msg);
}

void SettingsPage::onLocalTempUpdated(float celsius)
{
    if (!m_tempLiveLbl) return;
    const QString icon =
        celsius >= ThermalMonitor::kCritTemp  ? QString::fromUtf8("\xf0\x9f\x94\xb4")  /* 🔴 */
      : celsius >= ThermalMonitor::kPauseTemp ? QString::fromUtf8("\xf0\x9f\x9f\xa0")  /* 🟠 */
      : celsius >= ThermalMonitor::kWarnTemp  ? QString::fromUtf8("\xf0\x9f\x9f\xa1")  /* 🟡 */
      :                                         QString::fromUtf8("\xf0\x9f\x9f\xa2"); /* 🟢 */
    m_tempLiveLbl->setText(
        QString::fromUtf8("\xf0\x9f\x8c\xa1 ")  /* 🌡️ */
        + icon + QString(" CPU: %1°C").arg(celsius, 0, 'f', 1));
}

void SettingsPage::onLocalThermalPause(float celsius)
{
    if (!m_thermalMsgLbl) return;
    m_thermalMsgLbl->setText(
        QString::fromUtf8("\xf0\x9f\x94\xb4")   /* 🔴 */
        + QString(" Temperatura %1°C — pausa automatica 5 s")
          .arg(celsius, 0, 'f', 1));
    m_thermalMsgLbl->setVisible(true);
}

void SettingsPage::onLocalThermalResume()
{
    if (!m_thermalMsgLbl) return;
    m_thermalMsgLbl->setText(
        QString::fromUtf8("\xf0\x9f\x9f\xa2")   /* 🟢 */
        + " Temperatura ok — ripresa inferenza");
    QTimer::singleShot(4000, m_thermalMsgLbl, &QLabel::hide);
}

void SettingsPage::onLocalModelLoaded(const QString& path)
{
    const QString name = LocalLlmClient::loadedModelName(path);
    m_localStatusLbl->setText(
        QString::fromUtf8("\xf0\x9f\x9f\xa2")   /* 🟢 */
        + " " + name + " caricato e pronto");
    if (m_dlModelBtn) m_dlModelBtn->setEnabled(false);
}

void SettingsPage::onLocalLoadError(const QString& msg)
{
    m_localStatusLbl->setText(
        QString::fromUtf8("\xe2\x9d\x8c")        /* ❌ */
        + " Errore caricamento: " + msg);
    if (m_dlModelBtn) m_dlModelBtn->setEnabled(true);
}

/* ── onUpdateWebLink ─────────────────────────────────────────── */
/* ── onResetHScroll — impedisce scorrimento orizzontale residuo ── */
void SettingsPage::onResetHScroll()
{
    if (m_scrollArea)
        m_scrollArea->horizontalScrollBar()->setValue(0);
}

void SettingsPage::onUpdateWebLink()
{
    if (!m_webLinkLbl) return;
    const QString host  = m_hostEdit  ? m_hostEdit->text().trimmed()  : QString();
    const int     port  = m_portSpin  ? m_portSpin->value()           : 11500;
    const QString token = m_tokenEdit ? m_tokenEdit->text().trimmed() : QString();

    if (host.isEmpty()) {
        m_webLinkLbl->setText(
            "<small style='color:#888'>"
            "Inserisci un IP per generare il link</small>");
        return;
    }

    /* URL base + token come query parameter (URL-encoded) */
    QString url = QString("http://%1:%2/web").arg(host).arg(port);
    if (!token.isEmpty()) {
        const QString enc = QString::fromLatin1(
            QUrl::toPercentEncoding(token));
        url += "?token=" + enc;
    }

    /* Link cliccabile + testo selezionabile per copia manuale */
    m_webLinkLbl->setText(
        QString("<a href=\"%1\" style=\"color:#6c63ff;word-break:break-all\">%1</a>")
        .arg(url.toHtmlEscaped()));
}

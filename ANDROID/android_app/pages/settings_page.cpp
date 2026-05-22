#include "settings_page.h"
#include "qr_scanner_dialog.h"
#include "../ai_client.h"
#include "../mobile_theme_manager.h"
#include "../local_llm_client.h"
#include "../thermal_monitor.h"
#include "../rag_engine_simple.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
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
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QRegularExpression>
#include <algorithm>

/* ── Helper locale: crea etichetta muted sopra un widget ── */
static void addField(QVBoxLayout* vl, QWidget* parent,
                     const QString& labelText, QWidget* w)
{
    auto* lbl = new QLabel(labelText, parent);
    lbl->setStyleSheet("font-size:12px; color:#6b7280; margin-bottom:2px;");
    vl->addWidget(lbl);
    vl->addWidget(w);
    vl->addSpacing(6);
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
SettingsPage::SettingsPage(AiClient* ai, RagEngineSimple* rag,
                           LocalLlmClient* localLlm, QWidget* parent)
    : QWidget(parent), m_ai(ai), m_rag(rag), m_localLlm(localLlm)
{
    setObjectName("SettingsPage");
    QSettings s("Prismalux", "Mobile");

    /* ── Layout esterno ── */
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    /* ── Scroll area ── */
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setObjectName("SettingsScroll");
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /* Scorrimento touch senza overshoot */
    QScroller::grabGesture(m_scrollArea->viewport(), QScroller::TouchGesture);
    QScroller* qs = QScroller::scroller(m_scrollArea->viewport());
    QScrollerProperties sp = qs->scrollerProperties();
    sp.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor,   0.0);
    sp.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.0);
    sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    qs->setScrollerProperties(sp);

    /* Widget interno */
    auto* inner = new QWidget;
    inner->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* vbox = new QVBoxLayout(inner);
    vbox->setContentsMargins(10, 12, 10, 24);
    vbox->setSpacing(14);
    m_scrollArea->setWidget(inner);
    outer->addWidget(m_scrollArea);

    /* ══════════════════════════════════
       1 — Tema interfaccia  (in cima!)
       ══════════════════════════════════ */
    {
        auto* g  = new QGroupBox(QString::fromUtf8("\xf0\x9f\x8e\xa8") + "  Tema", inner);
        g->setObjectName("SettingsGroup");
        auto* gl = new QVBoxLayout(g);
        gl->setSpacing(8);
        gl->setContentsMargins(12, 16, 12, 12);

        m_themeCombo = new QComboBox(g);
        m_themeCombo->setObjectName("SettingsInput");
        const auto tlist = MobileThemeManager::themes();
        const QString savedTheme = MobileThemeManager::instance().current();
        int savedThemeIdx = 0;
        for (int i = 0; i < tlist.size(); ++i) {
            m_themeCombo->addItem(tlist[i].name, tlist[i].id);
            if (tlist[i].id == savedTheme) savedThemeIdx = i;
        }
        /* Blocca i segnali durante il set iniziale per non triggherare apply */
        m_themeCombo->blockSignals(true);
        m_themeCombo->setCurrentIndex(savedThemeIdx);
        m_themeCombo->blockSignals(false);

        auto* previewLbl = new QLabel(
            QString::fromUtf8("\xf0\x9f\x8e\xa8") /* 🎨 */
            + "  Il tema si applica immediatamente alla selezione.", g);
        previewLbl->setStyleSheet("font-size:12px; color:#6b7280;");
        previewLbl->setWordWrap(true);

        addField(gl, g, "Tema:", m_themeCombo);
        gl->addWidget(previewLbl);
        vbox->addWidget(g);

        /* Applica il tema subito al cambio selezione — senza bottone separato */
        connect(m_themeCombo,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SettingsPage::onThemeApply);
    }

    /* ══════════════════════════════════
       2 — Server Ollama
       ══════════════════════════════════ */
    {
        auto* g  = new QGroupBox(QString::fromUtf8("\xf0\x9f\x8c\x90") + "  Prismalux Desktop (LAN)", inner);
        g->setObjectName("SettingsGroup");
        auto* gl = new QVBoxLayout(g);
        gl->setSpacing(8);
        gl->setContentsMargins(12, 16, 12, 12);

        m_hostEdit = new QLineEdit(s.value("server/host", "192.168.1.165").toString(), g);
        m_hostEdit->setPlaceholderText("IP del PC — es. 192.168.1.165");
        m_hostEdit->setObjectName("SettingsInput");

        m_portSpin = new QSpinBox(g);
        m_portSpin->setRange(1, 65535);
        m_portSpin->setValue(s.value("server/port", 11434).toInt());
        m_portSpin->setObjectName("SettingsInput");

        m_tokenEdit = new QLineEdit(s.value("server/token", "").toString(), g);
        m_tokenEdit->setPlaceholderText("Token LAN (vuoto = Ollama diretto)");
        m_tokenEdit->setObjectName("SettingsInput");
        m_tokenEdit->setEchoMode(QLineEdit::Password);

        auto* infoLbl = new QLabel(
            "<small>11434 = Ollama diretto &nbsp;|&nbsp; 11500 = LAN Prismalux (token)</small>", g);
        infoLbl->setTextFormat(Qt::RichText);
        infoLbl->setWordWrap(true);

        m_connStatus = new QLabel(g);
        m_connStatus->setWordWrap(true);
        {
            const QString tok = s.value("server/token", "").toString();
            m_connStatus->setText(tok.isEmpty()
                ? QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f  Nessun token — vedi guida sopra")
                : QString::fromUtf8("\xf0\x9f\x94\x91  Token salvato (\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2") + tok.right(4) + ")");
        }

        m_webLinkLbl = new QLabel(g);
        m_webLinkLbl->setTextFormat(Qt::RichText);
        m_webLinkLbl->setWordWrap(true);
        m_webLinkLbl->setOpenExternalLinks(true);
        m_webLinkLbl->setTextInteractionFlags(
            Qt::TextBrowserInteraction | Qt::TextSelectableByMouse);

        auto* btnRow = new QHBoxLayout;
        m_applyBtn = new QPushButton("Applica", g);
        m_testBtn  = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x8c") + "  Testa", g);
        m_applyBtn->setObjectName("PrimaryBtn");
        m_testBtn->setObjectName("SecondaryBtn");
        btnRow->addWidget(m_applyBtn);
        btnRow->addWidget(m_testBtn);

#ifdef HAVE_MULTIMEDIA
        auto* qrBtn = new QPushButton(
            QString::fromUtf8("\xf0\x9f\x93\xb7") + "  Scansiona QR dal PC", g);
        qrBtn->setObjectName("SecondaryBtn");
        gl->addWidget(qrBtn);
        connect(qrBtn, &QPushButton::clicked, this, &SettingsPage::onQrScanClicked);
#endif

        addField(gl, g, "Indirizzo IP:", m_hostEdit);
        addField(gl, g, "Porta:", m_portSpin);
        addField(gl, g, QString::fromUtf8("\xf0\x9f\x94\x91") + "  Token:", m_tokenEdit);
        gl->addWidget(infoLbl);
        gl->addSpacing(4);
        gl->addWidget(m_connStatus);
        gl->addWidget(m_webLinkLbl);
        gl->addLayout(btnRow);
        vbox->addWidget(g);

        onUpdateWebLink();

        connect(m_applyBtn, &QPushButton::clicked, this, &SettingsPage::onApplyServer);
        connect(m_testBtn,  &QPushButton::clicked, this, &SettingsPage::onTestConnection);
        connect(m_hostEdit,  &QLineEdit::textChanged,
                this, &SettingsPage::onUpdateWebLink);
        connect(m_portSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
                this, &SettingsPage::onUpdateWebLink);
        connect(m_tokenEdit, &QLineEdit::textChanged,
                this, &SettingsPage::onUpdateWebLink);
    }

    /* ══════════════════════════════════
       2b — Sync Desktop ↔ Mobile
       ══════════════════════════════════ */
    {
        auto* g  = new QGroupBox(
            QString::fromUtf8("\xf0\x9f\x94\x84") + "  Sync Desktop \xe2\x86\x94 Mobile", inner);
        g->setObjectName("SettingsGroup");
        auto* gl = new QVBoxLayout(g);
        gl->setSpacing(8);
        gl->setContentsMargins(12, 16, 12, 12);

        auto* info = new QLabel(
            QString::fromUtf8(
                "Invia statistiche quiz al desktop e scarica la knowledge base. "
                "Il server deve essere attivo e raggiungibile."), g);
        info->setWordWrap(true);
        info->setObjectName("HintLabel");
        gl->addWidget(info);

        m_syncStatusLbl = new QLabel("", g);
        m_syncStatusLbl->setWordWrap(true);
        gl->addWidget(m_syncStatusLbl);

        auto* syncBtn = new QPushButton(
            QString::fromUtf8("\xf0\x9f\x94\x84  Sincronizza ora"), g);
        syncBtn->setObjectName("PrimaryBtn");
        gl->addWidget(syncBtn);

        vbox->addWidget(g);
        connect(syncBtn, &QPushButton::clicked, this, &SettingsPage::onSyncClicked);
    }

    /* ══════════════════════════════════
       3 — Modello LLM
       ══════════════════════════════════ */
    {
        auto* g  = new QGroupBox(QString::fromUtf8("\xf0\x9f\xa4\x96") + "  Modello LLM", inner);
        g->setObjectName("SettingsGroup");
        auto* gl = new QVBoxLayout(g);
        gl->setSpacing(8);
        gl->setContentsMargins(12, 16, 12, 12);

        m_modelCombo = new QComboBox(g);
        m_modelCombo->setObjectName("SettingsInput");
        const QStringList suggestions = {
            "llama3.2:1b", "deepseek-r1:1.5b", "llama3.2:3b",
            "phi3:mini", "moondream:latest", "mistral:7b-instruct", "Altro...",
        };
        m_modelCombo->addItems(suggestions);
        const QString savedModel = s.value("server/model", "llama3.2:1b").toString();
        const int savedIdx = m_modelCombo->findText(savedModel);
        m_modelCombo->setCurrentIndex(savedIdx >= 0 ? savedIdx : m_modelCombo->count() - 1);

        /* Riga "Altro..." */
        m_customModelRow = new QWidget(g);
        auto* customLay  = new QVBoxLayout(m_customModelRow);
        customLay->setContentsMargins(0, 0, 0, 0);
        m_customModelEdit = new QLineEdit(m_customModelRow);
        m_customModelEdit->setObjectName("SettingsInput");
        m_customModelEdit->setPlaceholderText("es. llama3.2:1b, mistral:7b");
        if (savedIdx < 0) m_customModelEdit->setText(savedModel);
        customLay->addWidget(m_customModelEdit);
        m_customModelRow->setVisible(savedIdx < 0);

        m_modelStatus = new QLabel("", g);
        m_modelStatus->setWordWrap(true);

        auto* refreshRow = new QHBoxLayout;
        m_refreshBtn = new QPushButton(
            QString::fromUtf8("\xf0\x9f\x94\x84") + "  Dal server", g);
        auto* helpBtn = new QPushButton(
            QString::fromUtf8("\xe2\x9d\x93") + "  Guida", g);
        m_refreshBtn->setObjectName("SecondaryBtn");
        helpBtn->setObjectName("SecondaryBtn");
        refreshRow->addWidget(m_refreshBtn, 1);
        refreshRow->addWidget(helpBtn);

        auto* quickRow = new QHBoxLayout;
        auto* btn1b  = new QPushButton("llama3.2:1b", g);
        auto* btn15b = new QPushButton("deepseek-r1:1.5b", g);
        btn1b->setObjectName("SecondaryBtn");
        btn15b->setObjectName("SecondaryBtn");
        quickRow->addWidget(btn1b);
        quickRow->addWidget(btn15b);

        addField(gl, g, "Modello:", m_modelCombo);
        gl->addWidget(m_customModelRow);
        gl->addWidget(m_modelStatus);
        gl->addLayout(refreshRow);
        gl->addLayout(quickRow);
        vbox->addWidget(g);

        connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SettingsPage::onModelComboChanged);
        connect(m_refreshBtn, &QPushButton::clicked,
                this, &SettingsPage::onRefreshModels);
        connect(helpBtn,      &QPushButton::clicked,
                this, &SettingsPage::onModelHelpClicked);
        connect(btn1b,  &QPushButton::clicked, this, &SettingsPage::onQuickApply1b);
        connect(btn15b, &QPushButton::clicked, this, &SettingsPage::onQuickApply15b);
        connect(m_ai, &AiClient::modelsReady, this, &SettingsPage::onModelsReady);
    }

    /* ══════════════════════════════════
       4 — Parametri AI
       ══════════════════════════════════ */
    {
        auto* g  = new QGroupBox(QString::fromUtf8("\xf0\x9f\x8e\x9b") + "  Parametri AI", inner);
        g->setObjectName("SettingsGroup");
        auto* gl = new QVBoxLayout(g);
        gl->setSpacing(8);
        gl->setContentsMargins(12, 16, 12, 12);

        m_tempSlider = new QSlider(Qt::Horizontal, g);
        m_tempSlider->setRange(0, 100);
        m_tempSlider->setValue(s.value("ai/temperature", 10).toInt());
        m_tempSlider->setObjectName("TempSlider");
        m_tempLbl = new QLabel(
            QString::number(m_tempSlider->value() / 100.0, 'f', 2), g);
        m_tempLbl->setObjectName("ValueLabel");
        m_tempLbl->setMinimumWidth(36);
        m_tempLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* tempRow = new QHBoxLayout;
        tempRow->addWidget(m_tempSlider, 1);
        tempRow->addWidget(m_tempLbl);

        m_predictSpin = new QSpinBox(g);
        m_predictSpin->setRange(128, 4096);
        m_predictSpin->setValue(s.value("ai/numPredict", 1024).toInt());
        m_predictSpin->setSingleStep(128);
        m_predictSpin->setObjectName("SettingsInput");

        auto* tempLbl2 = new QLabel("Temperatura:", g);
        tempLbl2->setStyleSheet("font-size:12px; color:#6b7280;");
        gl->addWidget(tempLbl2);
        gl->addLayout(tempRow);
        gl->addSpacing(6);
        addField(gl, g, "Max token:", m_predictSpin);
        vbox->addWidget(g);

        connect(m_tempSlider, &QSlider::valueChanged,
                this, &SettingsPage::onTempChanged);
        connect(m_predictSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                m_ai, &AiClient::setNumPredict);

        /* Applica valori salvati all'AI client */
        m_ai->setTemperature(m_tempSlider->value() / 100.0);
        m_ai->setNumPredict(m_predictSpin->value());
    }

    /* ══════════════════════════════════
       5 — RAG Documenti
       ══════════════════════════════════ */
    {
        auto* g  = new QGroupBox(QString::fromUtf8("\xf0\x9f\x93\x9a") + "  RAG Documenti", inner);
        g->setObjectName("SettingsGroup");
        auto* gl = new QVBoxLayout(g);
        gl->setSpacing(8);
        gl->setContentsMargins(12, 16, 12, 12);

        m_ragPathEdit = new QLineEdit(s.value("rag/indexPath", "").toString(), g);
        m_ragPathEdit->setPlaceholderText("File .txt / .md / .pdf");
        m_ragPathEdit->setObjectName("SettingsInput");
        m_ragLoadBtn  = new QPushButton(
            QString::fromUtf8("\xf0\x9f\x93\x82") + "  Sfoglia e carica", g);
        m_ragLoadBtn->setObjectName("PrimaryBtn");
        m_ragStatus = new QLabel(
            m_rag->chunkCount() > 0
                ? QString::fromUtf8("\xe2\x9c\x85  %1 chunk in memoria")
                  .arg(m_rag->chunkCount())
                : "Nessun documento caricato",
            g);
        m_ragStatus->setWordWrap(true);

        addField(gl, g, "Percorso:", m_ragPathEdit);
        gl->addWidget(m_ragLoadBtn);
        gl->addWidget(m_ragStatus);
        vbox->addWidget(g);

        connect(m_ragLoadBtn, &QPushButton::clicked, this, &SettingsPage::onLoadRag);
    }

    /* ══════════════════════════════════
       6 — LLM Locale (offline)
       ══════════════════════════════════ */
    {
        auto* g = new QGroupBox(
            QString::fromUtf8("\xf0\x9f\x96\xa5\xef\xb8\x8f")
            + "  LLM Locale — offline", inner);
        g->setObjectName("SettingsGroup");
        auto* gl = new QVBoxLayout(g);
        gl->setSpacing(8);
        gl->setContentsMargins(12, 16, 12, 12);

        m_localModeChk = new QCheckBox("Usa LLM locale quando offline", g);
        m_localModeChk->setChecked(m_localLlm && m_ai->isLocalMode());

        const bool modelOk = m_localLlm && LocalLlmClient::modelExists();
        {
            QString statusTxt;
            if (modelOk) {
                const QString p = LocalLlmClient::firstAvailableModelPath();
                statusTxt = QString::fromUtf8("\xe2\x9c\x85 ")
                    + LocalLlmClient::loadedModelName(p) + " pronto";
            } else {
                statusTxt = QString::fromUtf8("\xe2\x8f\xac") + " Nessun modello scaricato";
            }
            m_localStatusLbl = new QLabel(statusTxt, g);
        }
        m_localStatusLbl->setWordWrap(true);

        m_dlModelCombo = new QComboBox(g);
        m_dlModelCombo->setObjectName("SettingsInput");
        for (int i = 0; i < LocalLlmClient::kNumModels; ++i) {
            const bool exists = LocalLlmClient::modelExists(i);
            m_dlModelCombo->addItem(
                (exists
                    ? QString::fromUtf8("\xe2\x9c\x85 ")
                    : QString::fromUtf8("\xe2\x8f\xac "))
                + QString::fromUtf8(LocalLlmClient::kModels[i].displayName));
        }

        auto buildDlText = [this]() -> QString {
            const int idx = m_dlModelCombo ? m_dlModelCombo->currentIndex() : 0;
            if (idx < 0 || idx >= LocalLlmClient::kNumModels)
                return QString::fromUtf8("\xf0\x9f\x93\xa5 Scarica");
            if (LocalLlmClient::modelExists(idx))
                return QString::fromUtf8("\xe2\x9c\x85") + " Già scaricato";
            if (LocalLlmClient::hasPartialDownload(idx))
                return QString::fromUtf8("\xf0\x9f\x93\xa5")
                    + QString(" Riprendi (%1 MB)")
                      .arg(LocalLlmClient::partialDownloadSize(idx) / (1024*1024));
            return QString::fromUtf8("\xf0\x9f\x93\xa5")
                + QString(" Scarica (~%1 MB)")
                  .arg(LocalLlmClient::kModels[idx].sizeMB);
        };

        m_dlModelBtn = new QPushButton(buildDlText(), g);
        m_dlModelBtn->setObjectName("PrimaryBtn");
        m_dlModelBtn->setEnabled(!LocalLlmClient::modelExists(0));

        m_dlCancelBtn = new QPushButton(
            QString::fromUtf8("\xe2\x9d\x8c") + " Annulla", g);
        m_dlCancelBtn->setObjectName("SecondaryBtn");
        m_dlCancelBtn->setVisible(false);

        auto* dlRow = new QHBoxLayout;
        dlRow->addWidget(m_dlModelBtn, 1);
        dlRow->addWidget(m_dlCancelBtn);

        m_dlProgress = new QProgressBar(g);
        m_dlProgress->setRange(0, 100);
        m_dlProgress->setFixedHeight(18);
        m_dlProgress->setVisible(false);

        m_tempLiveLbl = new QLabel(
            QString::fromUtf8("\xf0\x9f\x8c\xa1") + " CPU: —", g);
        m_thermalMsgLbl = new QLabel("", g);
        m_thermalMsgLbl->setWordWrap(true);
        m_thermalMsgLbl->setVisible(false);

        auto* threshLbl = new QLabel(
            QString::fromUtf8("<small>"
            "\xf0\x9f\x9f\xa1 Pausa: <b>72°C</b> &nbsp;"
            "\xf0\x9f\x94\xb4 Critica: <b>78°C</b><br>"
            "Generazione si interrompe automaticamente.</small>"), g);
        threshLbl->setTextFormat(Qt::RichText);
        threshLbl->setWordWrap(true);

        gl->addWidget(m_localModeChk);
        gl->addWidget(m_localStatusLbl);
        gl->addSpacing(4);
        addField(gl, g, "Modello da scaricare:", m_dlModelCombo);
        gl->addLayout(dlRow);
        gl->addWidget(m_dlProgress);
        gl->addSpacing(8);
        addField(gl, g, "Temperatura live:", m_tempLiveLbl);
        gl->addWidget(m_thermalMsgLbl);
        gl->addWidget(threshLbl);
        vbox->addWidget(g);

        connect(m_localModeChk,  &QCheckBox::toggled,
                this, &SettingsPage::onLocalModeToggled);
        connect(m_ai,  &AiClient::localModeChanged,
                this, &SettingsPage::onAiLocalModeChanged);
        connect(m_dlModelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SettingsPage::onDlModelSelected);
        connect(m_dlModelBtn,   &QPushButton::clicked,
                this, &SettingsPage::onDownloadModelClicked);
        connect(m_dlCancelBtn,  &QPushButton::clicked,
                this, &SettingsPage::onDlCancelClicked);

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
    }

    vbox->addStretch();
}

/* ══════════════════════════════════════════════════════════════
   Nuovi slot
   ══════════════════════════════════════════════════════════════ */

void SettingsPage::onThemeApply()
{
    if (!m_themeCombo) return;
    MobileThemeManager::instance().apply(m_themeCombo->currentData().toString());
}

void SettingsPage::onModelComboChanged(int idx)
{
    if (!m_customModelRow || !m_modelCombo) return;
    m_customModelRow->setVisible(m_modelCombo->itemText(idx) == "Altro...");
}

void SettingsPage::onTempChanged(int v)
{
    const double t = v / 100.0;
    if (m_tempLbl) m_tempLbl->setText(QString::number(t, 'f', 2));
    m_ai->setTemperature(t);
    QSettings s("Prismalux", "Mobile");
    s.setValue("ai/temperature", v);
}

void SettingsPage::onDlCancelClicked()
{
    if (m_localLlm) m_localLlm->cancelDownload();
}

void SettingsPage::applyQuickModel(const QString& model)
{
    if (!m_modelCombo) return;
    const int idx = m_modelCombo->findText(model);
    if (idx >= 0) {
        m_modelCombo->setCurrentIndex(idx);
        if (m_customModelRow) m_customModelRow->setVisible(false);
    } else {
        const int altroIdx = m_modelCombo->findText("Altro...");
        if (altroIdx >= 0) m_modelCombo->setCurrentIndex(altroIdx);
        if (m_customModelEdit) m_customModelEdit->setText(model);
        if (m_customModelRow) m_customModelRow->setVisible(true);
    }
    emit serverChanged(m_hostEdit->text().trimmed(), m_portSpin->value(), model);
    if (m_connStatus)
        m_connStatus->setText(QString::fromUtf8("\xf0\x9f\x9f\xa2  Modello: ") + model);
}

void SettingsPage::onQuickApply1b()  { applyQuickModel("llama3.2:1b"); }
void SettingsPage::onQuickApply15b() { applyQuickModel("deepseek-r1:1.5b"); }

void SettingsPage::onModelHelpClicked()
{
    QMessageBox::information(this,
        QString::fromUtf8("\xe2\x9d\x93  Guida modelli Ollama"),
        "<b>Formato:</b> <tt>nome:versione</tt><br><br>"
        "\xf0\x9f\x9f\xa2 <b>llama3.2:1b</b> — 1.3 GB, velocissimo<br>"
        "\xf0\x9f\x9f\xa2 <b>deepseek-r1:1.5b</b> — 1.1 GB, ragionamento<br>"
        "\xf0\x9f\x9f\xa1 <b>llama3.2:3b</b> — 2.0 GB, bilanciato<br>"
        "\xf0\x9f\x9f\xa1 <b>phi3:mini</b> — 2.2 GB, buon italiano<br>"
        "\xf0\x9f\x94\xb4 <b>mistral:7b-instruct</b> — 4.4 GB, potente<br><br>"
        "Vedi tutti su <b>ollama.com/library</b><br>"
        "Premi <b>Dal server</b> per caricare i modelli già installati.");
}

/* ══════════════════════════════════════════════════════════════
   Slot server
   ══════════════════════════════════════════════════════════════ */

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

    QSettings s("Prismalux", "Mobile");
    s.setValue("server/token", token);
    m_ai->setToken(token);

    emit serverChanged(host, port, model);
    m_connStatus->setText(QString::fromUtf8("\xf0\x9f\x94\x84  Impostazioni applicate"));
}

void SettingsPage::onTestConnection()
{
    if (m_testReply) return;  /* test già in corso */
    const QString url = QString("http://%1:%2/api/tags")
                        .arg(m_hostEdit->text().trimmed())
                        .arg(m_portSpin->value());
    m_connStatus->setText(QString::fromUtf8("\xf0\x9f\x94\x84  Test in corso\xe2\x80\xa6"));
    m_testBtn->setEnabled(false);

    m_testNam = new QNetworkAccessManager(this);
    QNetworkRequest req{QUrl{url}};
    const QString token = m_tokenEdit ? m_tokenEdit->text().trimmed() : QString();
    if (!token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    m_testReply = m_testNam->get(req);
    connect(m_testReply, &QNetworkReply::finished,
            this, &SettingsPage::onTestConnectionFinished);
}

void SettingsPage::onTestConnectionFinished()
{
    if (!m_testReply) return;
    m_testBtn->setEnabled(true);
    m_connStatus->setText(
        m_testReply->error() == QNetworkReply::NoError
        ? QString::fromUtf8("\xe2\x9c\x85  Ollama raggiungibile — premi Aggiorna modelli")
        : QString::fromUtf8("\xe2\x9d\x8c  ") + m_testReply->errorString().left(80));
    m_testReply->deleteLater(); m_testReply = nullptr;
    m_testNam->deleteLater();   m_testNam   = nullptr;
}

void SettingsPage::onRagWatchdogTimeout()
{
    if (m_ragWatchdog) { m_ragWatchdog->deleteLater(); m_ragWatchdog = nullptr; }
    m_ragLoadBtn->setEnabled(true);
    m_ragStatus->setText(
        QString::fromUtf8("\xe2\x9a\xa0  Timeout: nessuna risposta dal caricatore RAG"));
}

void SettingsPage::onRagLoaded(int n)
{
    if (m_ragWatchdog) { m_ragWatchdog->stop(); m_ragWatchdog->deleteLater(); m_ragWatchdog = nullptr; }
    m_ragLoadBtn->setEnabled(true);
    m_ragStatus->setText(
        n > 0
        ? QString::fromUtf8("\xe2\x9c\x85  %1 chunk caricati").arg(n)
        : QString::fromUtf8("\xe2\x9d\x8c  Nessun documento .txt trovato"));
}

void SettingsPage::onQrUrlFound(const QString& raw)
{
    QUrl url(raw);
    const QString host  = url.host();
    const int     port  = url.port(-1);
    const QString token = QUrlQuery(url).queryItemValue("token");
    if (host.isEmpty()) {
        m_connStatus->setText(
            QString::fromUtf8("\xe2\x9d\x8c  QR non riconosciuto come URL Prismalux."));
        return;
    }
    if (m_hostEdit)           m_hostEdit->setText(host);
    if (m_portSpin && port>0) m_portSpin->setValue(port);
    if (m_tokenEdit)          m_tokenEdit->setText(token);
    QSettings s("Prismalux", "Mobile");
    s.setValue("server/host",  host);
    if (port > 0) s.setValue("server/port", port);
    s.setValue("server/token", token);
    m_ai->setToken(token);
    m_connStatus->setText(
        QString::fromUtf8("\xf0\x9f\x94\x91  Configurazione importata dal QR \xe2\x80\x94 premi Testa"));
    onUpdateWebLink();
}

void SettingsPage::onRefreshModels()
{
    m_modelStatus->setText(QString::fromUtf8("\xf0\x9f\x94\x84  Carico lista modelli\xe2\x80\xa6"));
    m_refreshBtn->setEnabled(false);
    m_ai->setServer(m_hostEdit->text().trimmed(), m_portSpin->value());
    m_ai->fetchModels();
}

static int modelSizeRank(const QString& name)
{
    const QRegularExpression re(R"([:_](\d+\.?\d*)[bB])");
    const auto m = re.match(name);
    if (m.hasMatch())
        return static_cast<int>(m.captured(1).toDouble() * 10);
    const QString lower = name.toLower();
    if (lower.contains("tiny"))  return 5;
    if (lower.contains("small")) return 15;
    if (lower.contains("mini"))  return 40;
    return 9999;
}

void SettingsPage::onModelsReady(const QStringList& models)
{
    m_refreshBtn->setEnabled(true);
    if (models.isEmpty()) {
        m_modelStatus->setText(
            QString::fromUtf8("\xe2\x9a\xa0  Nessun modello trovato — avvia Ollama."));
        return;
    }

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
        QString::fromUtf8("\xe2\x9c\x85  %1 modelli — seleziona e premi Applica")
        .arg(sorted.size()));
}

/* ══════════════════════════════════════════════════════════════
   Slot RAG
   ══════════════════════════════════════════════════════════════ */

void SettingsPage::onLoadRag()
{
#ifdef Q_OS_ANDROID
    const QString path = QFileDialog::getOpenFileName(
        this, "Seleziona documento RAG", QString(),
        "Documenti (*.txt *.md *.pdf);;Tutti i file (*)");
#else
    const QString path = QFileDialog::getExistingDirectory(
        this, "Seleziona cartella documenti RAG",
        m_ragPathEdit->text().isEmpty() ? QDir::homePath() : m_ragPathEdit->text());
#endif
    if (path.isEmpty()) return;
    m_ragPathEdit->setText(path);
    m_ragStatus->setText(QString::fromUtf8("\xf0\x9f\x94\x84  Caricamento in corso\xe2\x80\xa6"));
    m_ragLoadBtn->setEnabled(false);

    QSettings s("Prismalux", "Mobile");
    s.setValue("rag/indexPath", path);
    emit ragIndexChanged(path);

    m_ragWatchdog = new QTimer(this);
    m_ragWatchdog->setSingleShot(true);
    m_ragWatchdog->setInterval(10000);
    connect(m_ragWatchdog, &QTimer::timeout,
            this, &SettingsPage::onRagWatchdogTimeout);
    m_ragWatchdog->start();

    connect(m_rag, &RagEngineSimple::loaded,
            this, &SettingsPage::onRagLoaded, Qt::SingleShotConnection);
}

/* ══════════════════════════════════════════════════════════════
   Slot QR Scanner
   ══════════════════════════════════════════════════════════════ */

void SettingsPage::onQrScanClicked()
{
#ifdef HAVE_MULTIMEDIA
    auto* dlg = new QrScannerDialog(this);
    connect(dlg, &QrScannerDialog::urlFound,
            this, &SettingsPage::onQrUrlFound);
    dlg->exec();
#endif
}

/* ══════════════════════════════════════════════════════════════
   Slot LLM Locale
   ══════════════════════════════════════════════════════════════ */

void SettingsPage::onAiLocalModeChanged(bool on)
{
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

void SettingsPage::onDlModelSelected(int idx)
{
    if (idx < 0 || idx >= LocalLlmClient::kNumModels || !m_dlModelBtn) return;
    const auto& spec = LocalLlmClient::kModels[idx];
    if (LocalLlmClient::modelExists(idx)) {
        m_dlModelBtn->setText(QString::fromUtf8("\xe2\x9c\x85") + " Già scaricato");
        m_dlModelBtn->setEnabled(false);
    } else if (LocalLlmClient::hasPartialDownload(idx)) {
        m_dlModelBtn->setText(
            QString::fromUtf8("\xf0\x9f\x93\xa5")
            + QString(" Riprendi (%1 MB)").arg(
                LocalLlmClient::partialDownloadSize(idx) / (1024*1024)));
        m_dlModelBtn->setEnabled(true);
    } else {
        m_dlModelBtn->setText(
            QString::fromUtf8("\xf0\x9f\x93\xa5")
            + QString(" Scarica (~%1 MB)").arg(spec.sizeMB));
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
    const int pct = int(received * 100 / total);
    m_dlProgress->setValue(pct);
    m_localStatusLbl->setText(
        QString::fromUtf8("\xe2\x8f\xac")
        + QString(" %1 MB / %2 MB (%3%)")
          .arg(received/(1024*1024)).arg(total/(1024*1024)).arg(pct));
}

void SettingsPage::onDlFinished(const QString& path)
{
    m_dlCancelBtn->setVisible(false);
    m_dlProgress->setVisible(false);
    const QString name = LocalLlmClient::loadedModelName(path);
    m_localStatusLbl->setText(
        QString::fromUtf8("\xe2\x9c\x85") + " " + name + " scaricato — caricamento...");
    const int idx = m_dlModelCombo ? m_dlModelCombo->currentIndex() : -1;
    if (idx >= 0 && m_dlModelCombo) {
        m_dlModelCombo->setItemText(idx,
            QString::fromUtf8("\xe2\x9c\x85 ")
            + QString::fromUtf8(LocalLlmClient::kModels[idx].displayName));
        onDlModelSelected(idx);
    }
    if (m_localLlm) m_localLlm->loadModel(path);
}

void SettingsPage::onDlError(const QString& msg)
{
    m_dlCancelBtn->setVisible(false);
    m_dlProgress->setVisible(false);
    const int idx = m_dlModelCombo ? m_dlModelCombo->currentIndex() : -1;
    if (idx >= 0) onDlModelSelected(idx);
    m_localStatusLbl->setText(
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore download: " + msg);
}

void SettingsPage::onLocalTempUpdated(float celsius)
{
    if (!m_tempLiveLbl) return;
    const QString icon =
        celsius >= ThermalMonitor::kCritTemp  ? QString::fromUtf8("\xf0\x9f\x94\xb4")
      : celsius >= ThermalMonitor::kPauseTemp ? QString::fromUtf8("\xf0\x9f\x9f\xa0")
      : celsius >= ThermalMonitor::kWarnTemp  ? QString::fromUtf8("\xf0\x9f\x9f\xa1")
      :                                         QString::fromUtf8("\xf0\x9f\x9f\xa2");
    m_tempLiveLbl->setText(
        QString::fromUtf8("\xf0\x9f\x8c\xa1 ")
        + icon + QString(" CPU: %1°C").arg(celsius, 0, 'f', 1));
}

void SettingsPage::onLocalThermalPause(float celsius)
{
    if (!m_thermalMsgLbl) return;
    m_thermalMsgLbl->setText(
        QString::fromUtf8("\xf0\x9f\x94\xb4")
        + QString(" %1°C — pausa automatica 5 s").arg(celsius, 0, 'f', 1));
    m_thermalMsgLbl->setVisible(true);
}

void SettingsPage::onLocalThermalResume()
{
    if (!m_thermalMsgLbl) return;
    m_thermalMsgLbl->setText(
        QString::fromUtf8("\xf0\x9f\x9f\xa2") + " Temperatura ok — ripresa");
    QTimer::singleShot(4000, m_thermalMsgLbl, &QLabel::hide);
}

void SettingsPage::onLocalModelLoaded(const QString& path)
{
    m_localStatusLbl->setText(
        QString::fromUtf8("\xf0\x9f\x9f\xa2")
        + " " + LocalLlmClient::loadedModelName(path) + " caricato e pronto");
    if (m_dlModelBtn) m_dlModelBtn->setEnabled(false);
}

void SettingsPage::onLocalLoadError(const QString& msg)
{
    m_localStatusLbl->setText(
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore caricamento: " + msg);
    if (m_dlModelBtn) m_dlModelBtn->setEnabled(true);
}

/* ══════════════════════════════════════════════════════════════
   Slot link web
   ══════════════════════════════════════════════════════════════ */

void SettingsPage::onUpdateWebLink()
{
    if (!m_webLinkLbl) return;
    const QString host  = m_hostEdit  ? m_hostEdit->text().trimmed()  : QString();
    const int     port  = m_portSpin  ? m_portSpin->value()           : 11500;
    const QString token = m_tokenEdit ? m_tokenEdit->text().trimmed() : QString();

    if (host.isEmpty()) {
        m_webLinkLbl->setText(
            "<small style='color:#888'>Inserisci un IP per generare il link</small>");
        return;
    }

    QString realUrl = QString("http://%1:%2/web").arg(host).arg(port);
    QString dispUrl = realUrl;
    if (!token.isEmpty()) {
        realUrl += "?token=" + QString::fromLatin1(QUrl::toPercentEncoding(token));
        dispUrl += "?token=\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2";  /* ●●●● */
    }

    m_webLinkLbl->setText(
        QString("<a href=\"%1\" style=\"color:#6c63ff;word-break:break-all\">%2</a>")
        .arg(realUrl.toHtmlEscaped(), dispUrl.toHtmlEscaped()));
}

/* ─────────────────────────────────────────────────────────────────
   Sync Desktop ↔ Mobile
   ─────────────────────────────────────────────────────────────────
   POST /api/sync con quiz_stats da QSettings.
   Se risposta ok, fa GET /api/sync per scaricare knowledge desktop.
   ───────────────────────────────────────────────────────────────── */
void SettingsPage::onSyncClicked()
{
    QSettings s("Prismalux", "Mobile");
    const QString host  = s.value("server/host",  "192.168.1.1").toString();
    const int     port  = s.value("server/port",  11500).toInt();
    const QString token = s.value("server/token", "").toString();

    if (!m_syncNam) m_syncNam = new QNetworkAccessManager(this);
    if (m_syncReply) { m_syncReply->abort(); m_syncReply->deleteLater(); m_syncReply = nullptr; }

    /* Raccogli quiz_stats da QSettings */
    QJsonObject stats;
    const QStringList materie = s.value("quiz/materie").toStringList();
    for (const QString& m : materie) {
        QJsonObject ms;
        ms["total"]   = s.value("quiz/" + m + "/total",   0).toInt();
        ms["correct"] = s.value("quiz/" + m + "/correct", 0).toInt();
        stats[m] = ms;
    }

    QJsonObject body;
    body["quiz_stats"] = stats;
    body["device"]     = "Prismalux Mobile";

    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    const QString url = QString("http://%1:%2/api/sync").arg(host).arg(port);
    QNetworkRequest req;
    req.setUrl(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());

    m_syncReply = m_syncNam->post(req, payload);
    connect(m_syncReply, &QNetworkReply::finished, this, &SettingsPage::onSyncFinished);

    if (m_syncStatusLbl)
        m_syncStatusLbl->setText(QString::fromUtf8("\xf0\x9f\x94\x84  Sincronizzazione in corso\xe2\x80\xa6"));
}

void SettingsPage::onSyncFinished()
{
    if (!m_syncReply) return;
    const auto err  = m_syncReply->error();
    const QByteArray data = m_syncReply->readAll();
    m_syncReply->deleteLater();
    m_syncReply = nullptr;

    if (err != QNetworkReply::NoError) {
        if (m_syncStatusLbl)
            m_syncStatusLbl->setText(
                QString::fromUtf8("\xe2\x9d\x8c  Errore rete: ") + QString::number(static_cast<int>(err)));
        return;
    }

    const QJsonObject resp = QJsonDocument::fromJson(data).object();
    const QString at = resp.value("synced_at").toString();
    if (m_syncStatusLbl)
        m_syncStatusLbl->setText(
            QString::fromUtf8("\xe2\x9c\x85  Sincronizzato: ") + at);
}


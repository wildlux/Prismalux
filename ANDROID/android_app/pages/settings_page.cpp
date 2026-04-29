#include "settings_page.h"
#include "../ai_client.h"
#include "../rag_engine_simple.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFileDialog>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
SettingsPage::SettingsPage(AiClient* ai, RagEngineSimple* rag, QWidget* parent)
    : QWidget(parent), m_ai(ai), m_rag(rag)
{
    setObjectName("SettingsPage");
    QSettings s("Prismalux", "Mobile");

    /* ── Scroll area per tutto il contenuto ── */
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setObjectName("SettingsScroll");
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
    auto* serverGroup = new QGroupBox("\xf0\x9f\x8c\x90  Server Ollama", inner);  // 🌐
    serverGroup->setObjectName("SettingsGroup");
    auto* serverForm = new QFormLayout(serverGroup);
    serverForm->setSpacing(10);

    m_hostEdit = new QLineEdit(s.value("server/host", "192.168.1.100").toString(), serverGroup);
    m_hostEdit->setPlaceholderText("es. 192.168.1.100");
    m_hostEdit->setObjectName("SettingsInput");

    m_portSpin = new QSpinBox(serverGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(s.value("server/port", 11434).toInt());
    m_portSpin->setObjectName("SettingsInput");

    serverForm->addRow("Indirizzo IP:", m_hostEdit);
    serverForm->addRow("Porta:",        m_portSpin);

    m_connStatus = new QLabel("", serverGroup);
    m_connStatus->setWordWrap(true);
    serverForm->addRow(m_connStatus);

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

    m_modelCombo = new QComboBox(modelGroup);
    m_modelCombo->setObjectName("SettingsInput");
    m_modelCombo->setEditable(true);
    /* Modelli consigliati per mobile (< 2 GB) */
    const QStringList suggestions = {
        "llama3.2:1b",       // 1.3 GB — molto veloce
        "qwen3:1.7b",        // 1.4 GB — ottimo italiano
        "gemma3:1b",         // 815 MB — leggerissimo
        "deepseek-r1:1.5b",  // 1.1 GB — ragionamento
        "phi3.5:mini",       // 2.2 GB — bilanciato
    };
    m_modelCombo->addItems(suggestions);
    const QString savedModel = s.value("server/model", "llama3.2:1b").toString();
    const int idx = m_modelCombo->findText(savedModel);
    m_modelCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    m_modelStatus = new QLabel("", modelGroup);
    m_refreshBtn  = new QPushButton("\xf0\x9f\x94\x84  Dal server", modelGroup);  // 🔄
    m_refreshBtn->setObjectName("SecondaryBtn");

    modelForm->addRow("Modello:",       m_modelCombo);
    modelForm->addRow(m_refreshBtn);
    modelForm->addRow(m_modelStatus);
    vbox->addWidget(modelGroup);

    /* ══════════════════════════════════
       Sezione: Parametri AI
       ══════════════════════════════════ */
    auto* paramsGroup = new QGroupBox("\xf0\x9f\x8e\x9b  Parametri AI", inner);  // 🎛
    paramsGroup->setObjectName("SettingsGroup");
    auto* paramsForm = new QFormLayout(paramsGroup);
    paramsForm->setSpacing(10);

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

    m_ragPathEdit = new QLineEdit(s.value("rag/indexPath", "").toString(), ragGroup);
    m_ragPathEdit->setPlaceholderText("Cartella o file .txt");
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

    vbox->addStretch();

    /* ── Connessioni ── */
    connect(m_applyBtn,   &QPushButton::clicked, this, &SettingsPage::onApplyServer);
    connect(m_testBtn,    &QPushButton::clicked, this, &SettingsPage::onTestConnection);
    connect(m_refreshBtn, &QPushButton::clicked, this, &SettingsPage::onRefreshModels);
    connect(m_ragLoadBtn, &QPushButton::clicked, this, &SettingsPage::onLoadRag);
    connect(m_ai, &AiClient::modelsReady, this, &SettingsPage::onModelsReady);

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
    const QString model = m_modelCombo->currentText().trimmed();
    if (host.isEmpty() || model.isEmpty()) return;
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

    auto* nam   = new QNetworkAccessManager(this);
    auto* reply = nam->get(QNetworkRequest(QUrl(url)));
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
    const QString current = m_modelCombo->currentText();
    m_modelCombo->clear();
    m_modelCombo->addItems(models);
    const int idx = m_modelCombo->findText(current);
    m_modelCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_modelStatus->setText(
        QString("\xe2\x9c\x85  %1 modelli disponibili").arg(models.size()));  // ✅
}

/* ── onLoadRag ───────────────────────────────────────────────── */
void SettingsPage::onLoadRag()
{
#ifdef Q_OS_ANDROID
    /* Su Android usa un percorso manuale (picker non disponibile via QFileDialog) */
    const QString path = m_ragPathEdit->text().trimmed();
    if (path.isEmpty()) {
        m_ragStatus->setText(
            "\xe2\x9a\xa0  Inserisci il percorso della cartella o del file");
        return;
    }
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

    /* Aggiorna contatore (collegato a RagEngineSimple::loaded) */
    connect(m_rag, &RagEngineSimple::loaded, this, [this](int n) {
        m_ragLoadBtn->setEnabled(true);
        m_ragStatus->setText(
            n > 0
                ? QString("\xe2\x9c\x85  %1 chunk caricati").arg(n)  // ✅
                : "\xe2\x9d\x8c  Nessun documento .txt trovato nel percorso");
    }, Qt::SingleShotConnection);
}

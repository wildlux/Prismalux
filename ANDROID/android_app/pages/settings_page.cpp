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
#include <QRegularExpression>
#include <algorithm>

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
    auto* serverGroup = new QGroupBox("\xf0\x9f\x8c\x90  Prismalux Desktop (LAN)", inner);  // 🌐
    serverGroup->setObjectName("SettingsGroup");
    auto* serverForm = new QFormLayout(serverGroup);
    serverForm->setSpacing(10);

    m_hostEdit = new QLineEdit(s.value("server/host", "192.168.1.165").toString(), serverGroup);
    m_hostEdit->setPlaceholderText("IP del PC con Prismalux (es. 192.168.1.165)");
    m_hostEdit->setObjectName("SettingsInput");

    m_portSpin = new QSpinBox(serverGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(s.value("server/port", 11434).toInt());
    m_portSpin->setObjectName("SettingsInput");

    auto* portInfoLbl = new QLabel(
        "<small>Porta 11434 = Ollama diretto (LAN)<br>"
        "IP: indirizzo del PC sulla rete Wi-Fi</small>",
        serverGroup);
    portInfoLbl->setTextFormat(Qt::RichText);
    portInfoLbl->setWordWrap(true);

    serverForm->addRow("Indirizzo IP:", m_hostEdit);
    serverForm->addRow("Porta:",        m_portSpin);
    serverForm->addRow(portInfoLbl);

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
    /* Modelli installati sul server (aggiorna con "Dal server") */
    const QStringList suggestions = {
        "llama3.2:1b",           // 1.3 GB — velocissimo, consigliato mobile
        "deepseek-r1:1.5b",      // 1.1 GB — ragionamento
        "llama3.2:3b",           // 2.0 GB — bilanciato
        "phi3:mini",             // 2.2 GB — Microsoft, buon italiano
        "moondream:latest",      // 1.7 GB — visione (immagini)
        "mistral:7b-instruct",   // 4.4 GB — solo Wi-Fi veloce
    };
    m_modelCombo->addItems(suggestions);
    const QString savedModel = s.value("server/model", "llama3.2:1b").toString();
    const int idx = m_modelCombo->findText(savedModel);
    m_modelCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    m_modelStatus = new QLabel("", modelGroup);
    m_refreshBtn  = new QPushButton("\xf0\x9f\x94\x84  Dal server", modelGroup);  // 🔄
    m_refreshBtn->setObjectName("SecondaryBtn");

    /* Pulsanti rapidi modelli leggeri */
    auto* quickRow = new QHBoxLayout;
    auto* btn1b    = new QPushButton("llama3.2:1b", modelGroup);
    auto* btn15b   = new QPushButton("deepseek-r1:1.5b", modelGroup);
    btn1b->setObjectName("SecondaryBtn");
    btn15b->setObjectName("SecondaryBtn");
    quickRow->addWidget(btn1b);
    quickRow->addWidget(btn15b);

    modelForm->addRow("Modello:",       m_modelCombo);
    modelForm->addRow(m_refreshBtn);
    modelForm->addRow(quickRow);
    modelForm->addRow(m_modelStatus);
    vbox->addWidget(modelGroup);

    /* Selezione rapida → aggiorna combo e applica subito */
    auto quickApply = [this](const QString& model) {
        const int idx = m_modelCombo->findText(model);
        if (idx >= 0)
            m_modelCombo->setCurrentIndex(idx);
        else {
            m_modelCombo->insertItem(0, model);
            m_modelCombo->setCurrentIndex(0);
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

    const QString current = m_modelCombo->currentText();
    m_modelCombo->clear();
    m_modelCombo->addItems(sorted);

    /* Ri-seleziona il modello corrente, altrimenti il primo (più piccolo) */
    const int idx = m_modelCombo->findText(current);
    m_modelCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_modelStatus->setText(
        QString("\xe2\x9c\x85  %1 modelli — seleziona e premi Applica").arg(sorted.size()));
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

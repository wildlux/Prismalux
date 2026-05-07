#include "stable_diffusion_widget.h"
#include "../prismalux_paths.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPixmap>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QStandardPaths>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QTimer>

namespace {
    const QString kDefaultUrl = "http://localhost:7860";
}

StableDiffusionWidget::StableDiffusionWidget(QWidget* parent)
    : QWidget(parent)
{
    m_nam = new QNetworkAccessManager(this);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(8);

    /* ── Titolo + istruzione ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\x8e\xa8 <b>Stable Diffusion — Text to Image</b>  "
        "<span style='color:#888;font-size:11px;'>"
        "Richiede AUTOMATIC1111, Forge o SD.Next in esecuzione</span>", this);
    titleLbl->setTextFormat(Qt::RichText);
    lay->addWidget(titleLbl);

    /* ── Endpoint WebUI ── */
    auto* urlRow = new QHBoxLayout;
    urlRow->addWidget(new QLabel("WebUI URL:", this));
    m_urlEdit = new QLineEdit(kDefaultUrl, this);
    m_urlEdit->setPlaceholderText("http://localhost:7860");
    urlRow->addWidget(m_urlEdit, 1);
    m_btnCheck = new QPushButton(
        "\xe2\x9a\xa1  Controlla server", this);
    urlRow->addWidget(m_btnCheck);
    lay->addLayout(urlRow);

    /* ── Splitter: parametri | immagine ── */
    auto* mainRow = new QHBoxLayout;
    mainRow->setSpacing(8);

    /* Pannello parametri */
    auto* paramGroup = new QGroupBox(
        "\xf0\x9f\x93\x9d  Parametri generazione", this);
    auto* paramLay = new QVBoxLayout(paramGroup);
    paramLay->setSpacing(6);

    /* Prompt */
    paramLay->addWidget(new QLabel("Prompt:", paramGroup));
    m_prompt = new QTextEdit(paramGroup);
    m_prompt->setMaximumHeight(80);
    m_prompt->setPlaceholderText(
        "Es: a futuristic city at sunset, detailed, photorealistic, 8k");
    paramLay->addWidget(m_prompt);

    /* Prompt negativo */
    paramLay->addWidget(new QLabel("Prompt negativo:", paramGroup));
    m_negPrompt = new QTextEdit(paramGroup);
    m_negPrompt->setMaximumHeight(52);
    m_negPrompt->setPlaceholderText(
        "Es: blurry, low quality, watermark, text, ugly");
    m_negPrompt->setPlainText(
        "blurry, low quality, watermark, text, ugly, deformed");
    paramLay->addWidget(m_negPrompt);

    /* Modello */
    auto* modelRow = new QHBoxLayout;
    modelRow->addWidget(new QLabel("Modello:", paramGroup));
    m_modelCombo = new QComboBox(paramGroup);
    m_modelCombo->addItem("(carica dalla lista WebUI)");
    m_modelCombo->setMinimumWidth(200);
    modelRow->addWidget(m_modelCombo, 1);
    paramLay->addLayout(modelRow);

    /* Steps + CFG */
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(4);

    grid->addWidget(new QLabel("Steps:", paramGroup), 0, 0);
    m_steps = new QSpinBox(paramGroup);
    m_steps->setRange(1, 150);
    m_steps->setValue(20);
    grid->addWidget(m_steps, 0, 1);

    grid->addWidget(new QLabel("CFG Scale:", paramGroup), 0, 2);
    m_cfg = new QDoubleSpinBox(paramGroup);
    m_cfg->setRange(1.0, 30.0);
    m_cfg->setSingleStep(0.5);
    m_cfg->setValue(7.0);
    grid->addWidget(m_cfg, 0, 3);

    grid->addWidget(new QLabel("Dimensioni:", paramGroup), 1, 0);
    m_size = new QComboBox(paramGroup);
    m_size->addItems({"512 × 512", "512 × 768", "768 × 512",
                      "768 × 768", "1024 × 1024", "1024 × 768"});
    grid->addWidget(m_size, 1, 1, 1, 3);

    grid->addWidget(new QLabel("Seed:", paramGroup), 2, 0);
    m_seed = new QLineEdit("-1", paramGroup);
    m_seed->setPlaceholderText("-1 = casuale");
    m_seed->setMaximumWidth(100);
    grid->addWidget(m_seed, 2, 1);

    paramLay->addLayout(grid);

    /* Pulsante Genera */
    m_btnGen = new QPushButton(
        "\xf0\x9f\x8e\xa8  Genera immagine", paramGroup);
    m_btnGen->setObjectName("primaryBtn");
    m_btnGen->setEnabled(false);
    paramLay->addWidget(m_btnGen);
    paramLay->addStretch();

    mainRow->addWidget(paramGroup, 1);

    /* Pannello immagine */
    auto* imgGroup = new QGroupBox(
        "\xf0\x9f\x96\xbc  Immagine generata", this);
    auto* imgLay = new QVBoxLayout(imgGroup);

    m_imgScroll = new QScrollArea(imgGroup);
    m_imgScroll->setWidgetResizable(true);
    m_imgLabel = new QLabel(m_imgScroll);
    m_imgLabel->setAlignment(Qt::AlignCenter);
    m_imgLabel->setText(
        "<span style='color:#555;'>"
        "L'immagine generata apparer\xc3\xa0 qui.</span>");
    m_imgLabel->setTextFormat(Qt::RichText);
    m_imgScroll->setWidget(m_imgLabel);
    imgLay->addWidget(m_imgScroll, 1);

    /* Bottoni immagine */
    auto* imgBtnRow = new QHBoxLayout;
    m_btnSave = new QPushButton(
        "\xf0\x9f\x92\xbe  Salva PNG", imgGroup);
    m_btnSave->setEnabled(false);
    auto* btnCopy = new QPushButton(
        "\xf0\x9f\x93\x8b  Copia", imgGroup);
    btnCopy->setEnabled(false);
    imgBtnRow->addStretch();
    imgBtnRow->addWidget(m_btnSave);
    imgBtnRow->addWidget(btnCopy);
    imgLay->addLayout(imgBtnRow);

    mainRow->addWidget(imgGroup, 1);
    lay->addLayout(mainRow, 1);

    /* ── Stato ── */
    m_status = new QLabel(
        "\xf0\x9f\x94\x97  Premi 'Controlla server' per verificare la connessione a AUTOMATIC1111.",
        this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet("color:#aaa;font-size:11px;");
    lay->addWidget(m_status);

    /* ── Connessioni ── */
    connect(m_btnCheck, &QPushButton::clicked,
            this, &StableDiffusionWidget::checkServer);
    connect(m_btnGen, &QPushButton::clicked,
            this, &StableDiffusionWidget::generate);

    connect(m_btnSave, &QPushButton::clicked, this, [this]{
        if (m_lastPng.isEmpty()) return;
        const QString dir = QStandardPaths::writableLocation(
                                QStandardPaths::HomeLocation)
                            + "/.prismalux/generated";
        QDir().mkpath(dir);
        const QString path = dir + "/sd_"
            + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")
            + ".png";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(m_lastPng);
            setStatus(
                "\xf0\x9f\x92\xbe  Salvata in: " + path, true);
        }
    });

    connect(btnCopy, &QPushButton::clicked, this, [this, btnCopy]{
        if (m_lastPng.isEmpty()) return;
        QPixmap px;
        px.loadFromData(m_lastPng, "PNG");
        if (!px.isNull())
            QApplication::clipboard()->setPixmap(px);
        btnCopy->setText("\xe2\x9c\x85  Copiata!");
        QTimer::singleShot(2000, btnCopy,
            [btnCopy]{ btnCopy->setText("\xf0\x9f\x93\x8b  Copia"); });
    });

    /* Bind m_btnSave/btnCopy: abilita quando arriva un'immagine */
    connect(this, &StableDiffusionWidget::_imageReady, this,
            [this, btnCopy](bool ok){
        m_btnSave->setEnabled(ok);
        btnCopy->setEnabled(ok);
    });
}

/* ══════════════════════════════════════════════════════════════
   checkServer — verifica connessione + carica lista modelli
   ══════════════════════════════════════════════════════════════ */
void StableDiffusionWidget::checkServer()
{
    const QString base = m_urlEdit->text().trimmed();
    setStatus("\xf0\x9f\x94\x84  Connessione a " + base + "...", true);
    m_btnCheck->setEnabled(false);

    const QUrl url(base + "/sdapi/v1/sd-models");
    QNetworkRequest req(url);
    auto* reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, base]{
        m_btnCheck->setEnabled(true);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            setStatus(
                "\xe2\x9d\x8c  Server non raggiungibile. "
                "Avvia AUTOMATIC1111 / Forge con: "
                "<code>./webui.sh --api</code>", false);
            m_btnGen->setEnabled(false);
            return;
        }

        const QJsonArray models =
            QJsonDocument::fromJson(reply->readAll()).array();
        m_modelCombo->blockSignals(true);
        m_modelCombo->clear();
        for (const auto& v : models) {
            const QString title = v.toObject()["title"].toString();
            if (!title.isEmpty())
                m_modelCombo->addItem(title);
        }
        if (m_modelCombo->count() == 0)
            m_modelCombo->addItem("(nessun modello trovato)");
        m_modelCombo->blockSignals(false);

        m_btnGen->setEnabled(true);
        setStatus(
            "\xe2\x9c\x85  Server attivo su " + base
            + "  \xc2\xb7  " + QString::number(models.size())
            + " modelli trovati.", true);
    });
}

/* ══════════════════════════════════════════════════════════════
   generate — POST /sdapi/v1/txt2img
   ══════════════════════════════════════════════════════════════ */
void StableDiffusionWidget::generate()
{
    const QString prompt = m_prompt->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        setStatus("\xe2\x9d\x8c  Scrivi un prompt prima.", false);
        return;
    }

    const QString base = m_urlEdit->text().trimmed();

    /* Parsing dimensioni */
    const QString sizeStr = m_size->currentText(); // "512 × 512"
    const QStringList parts = sizeStr.split(QString::fromUtf8("\xc3\x97")); // ×
    int w = 512, h = 512;
    if (parts.size() == 2) {
        w = parts[0].trimmed().toInt();
        h = parts[1].trimmed().toInt();
    }

    QJsonObject body;
    body["prompt"]           = prompt;
    body["negative_prompt"]  = m_negPrompt->toPlainText().trimmed();
    body["steps"]            = m_steps->value();
    body["cfg_scale"]        = m_cfg->value();
    body["width"]            = w;
    body["height"]           = h;
    body["seed"]             = m_seed->text().trimmed().toLongLong();
    body["sampler_name"]     = QString("DPM++ 2M Karras");
    body["batch_size"]       = 1;
    body["n_iter"]           = 1;

    /* Cambio modello se selezionato uno specifico */
    const QString model = m_modelCombo->currentText();
    if (!model.isEmpty() && !model.startsWith("("))
        body["override_settings"] = QJsonObject{{"sd_model_checkpoint", model}};

    QNetworkRequest req(QUrl(base + "/sdapi/v1/txt2img"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_btnGen->setEnabled(false);
    m_btnGen->setText("\xf0\x9f\x94\x84  Generazione in corso...");
    setStatus(
        "\xf0\x9f\x8e\xa8  Generazione in corso — "
        + QString::number(m_steps->value()) + " steps, "
        + QString::number(w) + "\xc3\x97" + QString::number(h) + "...", true);

    auto* reply = m_nam->post(
        req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply]{
        m_btnGen->setEnabled(true);
        m_btnGen->setText("\xf0\x9f\x8e\xa8  Genera immagine");
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            setStatus(
                "\xe2\x9d\x8c  Errore: " + reply->errorString(), false);
            emit _imageReady(false);
            return;
        }

        const QJsonObject resp =
            QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonArray images = resp["images"].toArray();
        if (images.isEmpty()) {
            setStatus("\xe2\x9d\x8c  Nessuna immagine nella risposta.", false);
            emit _imageReady(false);
            return;
        }

        const QByteArray b64 = images[0].toString().toUtf8();
        showImage(b64);
    });
}

/* ══════════════════════════════════════════════════════════════
   showImage — decodifica base64 PNG e mostra inline
   ══════════════════════════════════════════════════════════════ */
void StableDiffusionWidget::showImage(const QByteArray& pngBase64)
{
    m_lastPng = QByteArray::fromBase64(pngBase64);
    QPixmap px;
    if (!px.loadFromData(m_lastPng, "PNG")) {
        setStatus("\xe2\x9d\x8c  Impossibile decodificare l'immagine.", false);
        emit _imageReady(false);
        return;
    }

    /* Scala per adattare al pannello (max 600px lato) */
    if (px.width() > 600 || px.height() > 600)
        px = px.scaled(600, 600, Qt::KeepAspectRatio,
                       Qt::SmoothTransformation);

    m_imgLabel->setPixmap(px);
    m_imgLabel->setTextFormat(Qt::PlainText);
    setStatus(
        "\xe2\x9c\x85  Immagine generata  \xc2\xb7  "
        + QString::number(m_lastPng.size() / 1024) + " KB", true);
    emit _imageReady(true);

    /* Auto-salvataggio in ~/.prismalux/generated/ */
    const QString dir = QStandardPaths::writableLocation(
                            QStandardPaths::HomeLocation)
                        + "/.prismalux/generated";
    QDir().mkpath(dir);
    const QString path = dir + "/sd_"
        + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")
        + ".png";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(m_lastPng);
}

void StableDiffusionWidget::setStatus(const QString& msg, bool ok)
{
    m_status->setText(msg);
    m_status->setStyleSheet(ok
        ? "color:#aaa;font-size:11px;"
        : "color:#ef4444;font-size:11px;");
}

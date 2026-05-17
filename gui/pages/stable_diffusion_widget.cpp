#include "stable_diffusion_widget.h"
#include "../prismalux_paths.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QProcess>
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
#include <QProgressBar>
#include <QTimer>

namespace {
    const QString kDefaultUrl = "http://localhost:7860";
}

StableDiffusionWidget::StableDiffusionWidget(QWidget* parent)
    : QWidget(parent)
{
    m_nam = new QNetworkAccessManager(this);
    m_sdScriptPath = PrismaluxPaths::root()
                     + "/MCPs/stable_diffusion_local/sd_local.py";

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(8);

    /* ── Titolo ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\x8e\xa8 <b>Stable Diffusion — Text to Image</b>", this);
    titleLbl->setTextFormat(Qt::RichText);
    lay->addWidget(titleLbl);

    /* ── Selettore backend ── */
    auto* modeRow = new QHBoxLayout;
    modeRow->setSpacing(4);
    modeRow->addWidget(new QLabel("Backend:", this));

    m_rbLocal = new QRadioButton(
        "\xf0\x9f\x92\xbb  Locale (diffusers)", this);
    m_rbA1111 = new QRadioButton(
        "\xf0\x9f\x8c\x90  AUTOMATIC1111 / Forge (remoto)", this);
    m_rbLocal->setChecked(true);

    auto* modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_rbLocal, 0);
    modeGroup->addButton(m_rbA1111, 1);

    modeRow->addWidget(m_rbLocal);
    modeRow->addWidget(m_rbA1111);
    modeRow->addStretch();
    lay->addLayout(modeRow);

    /* ── URL row (solo A1111) ── */
    m_urlWidget = new QWidget(this);
    auto* urlRow = new QHBoxLayout(m_urlWidget);
    urlRow->setContentsMargins(0, 0, 0, 0);
    urlRow->addWidget(new QLabel("WebUI URL:", m_urlWidget));
    m_urlEdit = new QLineEdit(kDefaultUrl, m_urlWidget);
    m_urlEdit->setPlaceholderText("http://localhost:7860");
    urlRow->addWidget(m_urlEdit, 1);
    m_urlWidget->hide();
    lay->addWidget(m_urlWidget);

    /* ── Hint installazione diffusers ── */
    m_installHint = new QLabel(
        "\xe2\x9a\xa0  <b>diffusers non trovato</b> \xe2\x80\x94 "
        "installa con: "
        "<code>pip install diffusers transformers accelerate torch</code>",
        this);
    m_installHint->setTextFormat(Qt::RichText);
    m_installHint->setWordWrap(true);
    m_installHint->setStyleSheet(
        "background:#4a2a00; color:#ffe0a0; "
        "border:1px solid #a06000; border-radius:4px; padding:6px;");
    m_installHint->hide();
    lay->addWidget(m_installHint);

    /* ── Splitter: parametri | immagine ── */
    auto* mainRow = new QHBoxLayout;
    mainRow->setSpacing(8);

    /* Pannello parametri */
    auto* paramGroup = new QGroupBox(
        "\xf0\x9f\x93\x9d  Parametri generazione", this);
    auto* paramLay = new QVBoxLayout(paramGroup);
    paramLay->setSpacing(6);

    paramLay->addWidget(new QLabel("Prompt:", paramGroup));
    m_prompt = new QTextEdit(paramGroup);
    m_prompt->setMaximumHeight(80);
    m_prompt->setPlaceholderText(
        "Es: a futuristic city at sunset, detailed, photorealistic, 8k");
    paramLay->addWidget(m_prompt);

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
    m_modelCombo->setEditable(true);
    m_modelCombo->addItem("runwayml/stable-diffusion-v1-5");
    m_modelCombo->addItem("stabilityai/stable-diffusion-2-1");
    m_modelCombo->addItem("stabilityai/stable-diffusion-xl-base-1.0");
    m_modelCombo->setMinimumWidth(240);
    m_modelCombo->setToolTip(
        "Locale: ID HuggingFace o path assoluto del modello\n"
        "A1111: selezionato automaticamente dalla lista WebUI");
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
    m_size->addItems({"512 \xc3\x97 512", "512 \xc3\x97 768", "768 \xc3\x97 512",
                      "768 \xc3\x97 768", "1024 \xc3\x97 1024", "1024 \xc3\x97 768"});
    grid->addWidget(m_size, 1, 1, 1, 3);

    grid->addWidget(new QLabel("Seed:", paramGroup), 2, 0);
    m_seed = new QLineEdit("-1", paramGroup);
    m_seed->setPlaceholderText("-1 = casuale");
    m_seed->setMaximumWidth(100);
    grid->addWidget(m_seed, 2, 1);

    paramLay->addLayout(grid);

    /* Riga pulsanti Controlla + Genera */
    auto* btnRow = new QHBoxLayout;
    m_btnCheck = new QPushButton(
        "\xe2\x9a\xa1  Controlla", paramGroup);
    m_btnGen = new QPushButton(
        "\xf0\x9f\x8e\xa8  Genera immagine", paramGroup);
    m_btnGen->setObjectName("primaryBtn");
    m_btnGen->setEnabled(false);
    btnRow->addWidget(m_btnCheck);
    btnRow->addWidget(m_btnGen, 1);
    paramLay->addLayout(btnRow);
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

    auto* imgBtnRow = new QHBoxLayout;
    m_btnSave = new QPushButton(
        "\xf0\x9f\x92\xbe  Salva PNG", imgGroup);
    m_btnSave->setEnabled(false);
    m_btnCopy = new QPushButton(
        "\xf0\x9f\x93\x8b  Copia", imgGroup);
    m_btnCopy->setEnabled(false);
    imgBtnRow->addStretch();
    imgBtnRow->addWidget(m_btnSave);
    imgBtnRow->addWidget(m_btnCopy);
    imgLay->addLayout(imgBtnRow);

    mainRow->addWidget(imgGroup, 1);
    lay->addLayout(mainRow, 1);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);   /* indeterminato di default */
    m_progress->setTextVisible(false);
    m_progress->setMaximumHeight(6);
    m_progress->hide();
    lay->addWidget(m_progress);

    m_status = new QLabel(
        "\xf0\x9f\x94\x97  Premi 'Controlla' per verificare il backend selezionato.",
        this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet("color:#aaa;font-size:11px;");
    lay->addWidget(m_status);

    /* ── Connessioni ── */
    connect(m_rbLocal,  &QRadioButton::toggled, this, &StableDiffusionWidget::onModeChanged);
    connect(m_btnCheck, &QPushButton::clicked,  this, &StableDiffusionWidget::checkServer);
    connect(m_btnGen,   &QPushButton::clicked,  this, &StableDiffusionWidget::generate);

    connect(m_btnSave, &QPushButton::clicked, this, &StableDiffusionWidget::onBtnSaveClicked);
    connect(m_btnCopy, &QPushButton::clicked, this, &StableDiffusionWidget::onBtnCopyClicked);
    connect(this, &StableDiffusionWidget::_imageReady,
            this, &StableDiffusionWidget::onImageReady);
}

StableDiffusionWidget::~StableDiffusionWidget()
{
    if (m_sdProc && m_sdProc->state() != QProcess::NotRunning) {
        m_sdProc->kill();
        m_sdProc->waitForFinished(1000);
    }
}

/* ── helpers ────────────────────────────────────────────────── */

bool StableDiffusionWidget::isLocalMode() const
{
    return m_rbLocal->isChecked();
}

void StableDiffusionWidget::onModeChanged()
{
    const bool local = isLocalMode();
    m_urlWidget->setVisible(!local);
    m_installHint->hide();

    if (local) {
        m_modelCombo->clear();
        m_modelCombo->setEditable(true);
        m_modelCombo->addItem("runwayml/stable-diffusion-v1-5");
        m_modelCombo->addItem("stabilityai/stable-diffusion-2-1");
        m_modelCombo->addItem("stabilityai/stable-diffusion-xl-base-1.0");
        m_btnGen->setEnabled(false);
        setStatus(
            "\xf0\x9f\x92\xbb  Premi 'Controlla' per verificare diffusers.", true);
    } else {
        m_modelCombo->clear();
        m_modelCombo->setEditable(false);
        m_modelCombo->addItem("(carica dalla lista WebUI)");
        m_btnGen->setEnabled(false);
        setStatus(
            "\xf0\x9f\x8c\x90  Premi 'Controlla' per verificare AUTOMATIC1111.", true);
    }
}

/* ══════════════════════════════════════════════════════════════
   checkServer — dispatcher
   ══════════════════════════════════════════════════════════════ */
void StableDiffusionWidget::checkServer()
{
    if (isLocalMode())
        checkDiffusers();
    else
        checkA1111();
}

void StableDiffusionWidget::checkA1111()
{
    const QString base = m_urlEdit->text().trimmed();
    setStatus("\xf0\x9f\x94\x84  Connessione a " + base + "...", true);
    m_btnCheck->setEnabled(false);

    m_checkReply = m_nam->get(QNetworkRequest(QUrl(base + "/sdapi/v1/sd-models")));
    connect(m_checkReply, &QNetworkReply::finished, this,
            &StableDiffusionWidget::onCheckA1111ReplyFinished);
}

void StableDiffusionWidget::checkDiffusers()
{
    m_btnCheck->setEnabled(false);
    m_installHint->hide();
    setStatus("\xf0\x9f\x94\x84  Verifica diffusers...", true);

    auto* proc = new QProcess(this);
    proc->start("python3", {"-c",
        "import diffusers, torch, transformers; "
        "print(f'diffusers {diffusers.__version__}, torch {torch.__version__}')"});

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &StableDiffusionWidget::onCheckDiffusersFinished);
}

/* ══════════════════════════════════════════════════════════════
   generate — dispatcher
   ══════════════════════════════════════════════════════════════ */
void StableDiffusionWidget::generate()
{
    if (isLocalMode())
        generateLocal();
    else
        generateA1111();
}

/* ── Generazione LOCALE (diffusers) ─────────────────────────── */
void StableDiffusionWidget::generateLocal()
{
    const QString prompt = m_prompt->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        setStatus("\xe2\x9d\x8c  Scrivi un prompt prima.", false);
        return;
    }
    if (m_sdProc && m_sdProc->state() != QProcess::NotRunning) {
        setStatus("\xe2\x9a\xa0  Generazione già in corso...", false);
        return;
    }

    const QString sizeStr = m_size->currentText();
    const QStringList parts = sizeStr.split(QString::fromUtf8("\xc3\x97"));
    int w = 512, h = 512;
    if (parts.size() == 2) {
        w = parts[0].trimmed().toInt();
        h = parts[1].trimmed().toInt();
    }

    const QString model  = m_modelCombo->currentText().trimmed();
    const QString negPr  = m_negPrompt->toPlainText().trimmed();
    const QString outDir = QStandardPaths::writableLocation(
                               QStandardPaths::HomeLocation)
                           + "/.prismalux/generated";
    QDir().mkpath(outDir);
    const QString outPath = outDir + "/sd_"
        + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")
        + ".png";

    m_pendingOutPath = outPath;
    m_sdProc = new QProcess(this);
    m_sdProc->setProcessChannelMode(QProcess::MergedChannels);

    QStringList args = {
        m_sdScriptPath,
        "--prompt",          prompt,
        "--negative_prompt", negPr,
        "--steps",           QString::number(m_steps->value()),
        "--cfg",             QString::number(m_cfg->value()),
        "--width",           QString::number(w),
        "--height",          QString::number(h),
        "--seed",            m_seed->text().trimmed(),
        "--model",           model,
        "--out",             outPath,
    };

    m_btnGen->setEnabled(false);
    m_btnGen->setText("\xf0\x9f\x94\x84  Generazione locale...");
    m_progress->setRange(0, 0);
    m_progress->show();
    setStatus(
        "\xf0\x9f\x92\xbb  Avvio diffusers ("
        + QString::number(m_steps->value()) + " steps, "
        + QString::number(w) + "\xc3\x97" + QString::number(h) + ")...", true);

    connect(m_sdProc, &QProcess::readyReadStandardOutput,
            this, &StableDiffusionWidget::onLocalProcReadyRead);
    connect(m_sdProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &StableDiffusionWidget::onLocalProcFinished);

    m_sdProc->start("python3", args);
    if (!m_sdProc->waitForStarted(3000)) {
        setStatus("\xe2\x9d\x8c  python3 non trovato nel PATH.", false);
        m_sdProc->deleteLater();
        m_sdProc = nullptr;
        m_btnGen->setEnabled(true);
        m_btnGen->setText("\xf0\x9f\x8e\xa8  Genera immagine");
    }
}

/* ── Generazione A1111 ───────────────────────────────────────── */
void StableDiffusionWidget::generateA1111()
{
    const QString prompt = m_prompt->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        setStatus("\xe2\x9d\x8c  Scrivi un prompt prima.", false);
        return;
    }

    const QString base = m_urlEdit->text().trimmed();
    const QString sizeStr = m_size->currentText();
    const QStringList parts = sizeStr.split(QString::fromUtf8("\xc3\x97"));
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

    const QString model = m_modelCombo->currentText();
    if (!model.isEmpty() && !model.startsWith("("))
        body["override_settings"] = QJsonObject{{"sd_model_checkpoint", model}};

    QNetworkRequest req(QUrl(base + "/sdapi/v1/txt2img"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_btnGen->setEnabled(false);
    m_btnGen->setText("\xf0\x9f\x94\x84  Generazione in corso...");
    setStatus(
        "\xf0\x9f\x8e\xa8  A1111: "
        + QString::number(m_steps->value()) + " steps, "
        + QString::number(w) + "\xc3\x97" + QString::number(h) + "...", true);

    auto* reply = m_nam->post(
        req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    m_genReply = reply;
    connect(reply, &QNetworkReply::finished, this,
            &StableDiffusionWidget::onA1111ReplyFinished);
}

/* ── showImage ──────────────────────────────────────────────── */
void StableDiffusionWidget::showImage(const QByteArray& pngData)
{
    m_lastPng = pngData;
    QPixmap px;
    if (!px.loadFromData(m_lastPng, "PNG")) {
        setStatus("\xe2\x9d\x8c  Impossibile decodificare l'immagine.", false);
        emit _imageReady(false);
        return;
    }

    if (px.width() > 600 || px.height() > 600)
        px = px.scaled(600, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    m_imgLabel->setPixmap(px);
    m_imgLabel->setTextFormat(Qt::PlainText);
    setStatus(
        "\xe2\x9c\x85  Immagine generata  \xc2\xb7  "
        + QString::number(m_lastPng.size() / 1024) + " KB", true);
    emit _imageReady(true);
}

void StableDiffusionWidget::setStatus(const QString& msg, bool ok)
{
    m_status->setText(msg);
    m_status->setStyleSheet(ok
        ? "color:#aaa;font-size:11px;"
        : "color:#ef4444;font-size:11px;");
}

/* ── Slot nominati ──────────────────────────────────────────── */

void StableDiffusionWidget::onBtnSaveClicked()
{
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
        setStatus("\xf0\x9f\x92\xbe  Salvata in: " + path, true);
    }
}

void StableDiffusionWidget::onBtnCopyClicked()
{
    if (m_lastPng.isEmpty()) return;
    QPixmap px;
    px.loadFromData(m_lastPng, "PNG");
    if (!px.isNull())
        QApplication::clipboard()->setPixmap(px);
    m_btnCopy->setText("\xe2\x9c\x85  Copiata!");
    QTimer::singleShot(2000, this, &StableDiffusionWidget::onCopyFeedbackReset);
}

void StableDiffusionWidget::onCopyFeedbackReset()
{
    m_btnCopy->setText("\xf0\x9f\x93\x8b  Copia");
}

void StableDiffusionWidget::onImageReady(bool ok)
{
    m_btnSave->setEnabled(ok);
    m_btnCopy->setEnabled(ok);
}

void StableDiffusionWidget::onCheckA1111ReplyFinished()
{
    auto* reply = m_checkReply;
    m_checkReply = nullptr;
    m_btnCheck->setEnabled(true);
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setStatus(
            "\xe2\x9d\x8c  Server non raggiungibile. "
            "Avvia con: <code>./webui.sh --api</code>", false);
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
        "\xe2\x9c\x85  Server attivo  \xc2\xb7  "
        + QString::number(models.size()) + " modelli trovati.", true);
}

void StableDiffusionWidget::onCheckDiffusersFinished(int code, QProcess::ExitStatus)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    m_btnCheck->setEnabled(true);

    if (code == 0) {
        const QString info = proc
            ? QString::fromLocal8Bit(proc->readAllStandardOutput()).trimmed()
            : QString();
        m_installHint->hide();
        m_btnGen->setEnabled(true);
        setStatus("\xe2\x9c\x85  Pronto: " + info, true);
    } else {
        m_installHint->show();
        m_btnGen->setEnabled(false);
        setStatus(
            "\xe2\x9d\x8c  diffusers non trovato. Vedi suggerimento sopra.", false);
    }

    if (proc) proc->deleteLater();
}

void StableDiffusionWidget::onLocalProcReadyRead()
{
    while (m_sdProc && m_sdProc->canReadLine()) {
        const QString line = QString::fromLocal8Bit(
            m_sdProc->readLine()).trimmed();
        const QJsonObject obj =
            QJsonDocument::fromJson(line.toUtf8()).object();
        if (obj.contains("status"))
            setStatus("\xf0\x9f\x94\x84  " + obj["status"].toString(), true);
        if (obj.contains("total_steps") && obj["total_steps"].toInt() > 0) {
            const int total = obj["total_steps"].toInt();
            const int step  = obj["step"].toInt();
            m_progress->setRange(0, total);
            m_progress->setValue(step);
        }
    }
}

void StableDiffusionWidget::onLocalProcFinished(int code, QProcess::ExitStatus)
{
    m_btnGen->setEnabled(true);
    m_btnGen->setText("\xf0\x9f\x8e\xa8  Genera immagine");
    m_progress->hide();
    if (m_sdProc) {
        m_sdProc->deleteLater();
        m_sdProc = nullptr;
    }

    if (code != 0) {
        setStatus("\xe2\x9d\x8c  Errore generazione locale.", false);
        emit _imageReady(false);
        return;
    }

    QFile f(m_pendingOutPath);
    if (!f.open(QIODevice::ReadOnly)) {
        setStatus("\xe2\x9d\x8c  File immagine non trovato.", false);
        emit _imageReady(false);
        return;
    }
    showImage(f.readAll());
}

void StableDiffusionWidget::onA1111ReplyFinished()
{
    auto* reply = m_genReply;
    m_genReply = nullptr;
    m_btnGen->setEnabled(true);
    m_btnGen->setText("\xf0\x9f\x8e\xa8  Genera immagine");
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setStatus("\xe2\x9d\x8c  Errore: " + reply->errorString(), false);
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

    showImage(QByteArray::fromBase64(images[0].toString().toUtf8()));
}

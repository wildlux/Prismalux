#include "main_distillazione.h"
#include "../prismalux_paths.h"
#include "../dpi_utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QComboBox>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QTabWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFont>

namespace P = PrismaluxPaths;

DistillazionePage::DistillazionePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    m_nam = new QNetworkAccessManager(this);

    auto* rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    /* Intestazione */
    auto* header = new QWidget(this);
    header->setObjectName("distHeader");
    auto* headerLay = new QVBoxLayout(header);
    headerLay->setContentsMargins(dpiScale(16), dpiScale(10), dpiScale(16), dpiScale(6));
    auto* titleLabel = new QLabel(tr("Distillazione Sintetica da LLM"), header);
    QFont tf = titleLabel->font();
    tf.setPointSize(tf.pointSize() + 3);
    tf.setBold(true);
    titleLabel->setFont(tf);
    auto* descLabel = new QLabel(
        tr("Genera un dataset JSONL interrogando pi\xc3\xb9 LLM locali su un dominio.\n"
           "Un LLM giudice seleziona la risposta migliore per ogni domanda.\n"
           "Il dataset esportato pu\xc3\xb2 essere usato per fine-tuning (formato Alpaca/unsloth)."),
        header);
    descLabel->setWordWrap(true);
    descLabel->setObjectName("mutedLabel");
    headerLay->addWidget(titleLabel);
    headerLay->addWidget(descLabel);
    rootLay->addWidget(header);

    /* Splitter principale */
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(buildConfigPanel());
    splitter->addWidget(buildOutputPanel());
    splitter->setStretchFactor(0, 35);
    splitter->setStretchFactor(1, 65);
    rootLay->addWidget(splitter, 1);

    /* Carica modelli */
    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, holder,
            [this, holder](const QStringList& l) {
                holder->deleteLater();
                onFetchModelsReady(l);
            });
    connect(m_ai, &AiClient::error, holder,
            [this, holder](const QString& e) {
                holder->deleteLater();
                onFetchModelsError(e);
            });
    m_ai->fetchModels();
}

/* ── Panel sinistro: configurazione ────────────────────────── */
QWidget* DistillazionePage::buildConfigPanel()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(dpiScale(280));

    auto* inner = new QWidget(scroll);
    auto* lay   = new QVBoxLayout(inner);
    lay->setContentsMargins(dpiScale(12), dpiScale(12), dpiScale(12), dpiScale(12));
    lay->setSpacing(dpiScale(8));

    /* Dominio */
    lay->addWidget(new QLabel(tr("Dominio di specializzazione:"), inner));
    m_dominioEdit = new QLineEdit(inner);
    m_dominioEdit->setPlaceholderText(tr("es. Qt6 C++, Fisica Quantistica, CCNA..."));
    lay->addWidget(m_dominioEdit);

    /* Numero domande */
    {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(tr("Numero domande da generare:"), inner));
        row->addStretch();
        m_nDomandeSpn = new QSpinBox(inner);
        m_nDomandeSpn->setRange(3, 200);
        m_nDomandeSpn->setSingleStep(5);
        m_nDomandeSpn->setValue(10);
        m_nDomandeSpn->setFixedWidth(dpiScale(70));
        row->addWidget(m_nDomandeSpn);
        lay->addLayout(row);
    }

    /* Lista modelli */
    lay->addWidget(new QLabel(tr("Modelli da interrogare (multi-selezione):"), inner));
    m_modelliList = new QListWidget(inner);
    m_modelliList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_modelliList->setFixedHeight(dpiScale(130));
    auto* loadingItem = new QListWidgetItem(tr("Caricamento modelli..."), m_modelliList);
    loadingItem->setFlags(Qt::NoItemFlags);
    lay->addWidget(m_modelliList);

    /* Giudice */
    lay->addWidget(new QLabel(tr("LLM Giudice (valuta le risposte):"), inner));
    m_giudiceCombo = new QComboBox(inner);
    m_giudiceCombo->addItem(tr("(caricamento...)"));
    lay->addWidget(m_giudiceCombo);

    /* Separator */
    auto* sep = new QFrame(inner);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    lay->addWidget(sep);

    /* Domande personalizzate */
    lay->addWidget(new QLabel(tr("Domande personalizzate (opzionale, una per riga):"), inner));
    m_domandeEdit = new QPlainTextEdit(inner);
    m_domandeEdit->setPlaceholderText(tr("Lascia vuoto per generarle automaticamente.\n"
                                          "Se specificato, salta la generazione."));
    m_domandeEdit->setFixedHeight(dpiScale(90));
    lay->addWidget(m_domandeEdit);

    /* Status */
    m_statusLabel = new QLabel(tr("Pronto."), inner);
    m_statusLabel->setObjectName("mutedLabel");
    m_statusLabel->setWordWrap(true);
    lay->addWidget(m_statusLabel);

    /* Pulsanti */
    m_btnGenera = new QPushButton(tr("1. Genera Domande"), inner);
    m_btnGenera->setObjectName("primaryBtn");
    lay->addWidget(m_btnGenera);

    m_btnInterroga = new QPushButton(tr("2. Interroga Modelli"), inner);
    m_btnInterroga->setEnabled(false);
    lay->addWidget(m_btnInterroga);

    m_btnEsporta = new QPushButton(tr("3. Esporta Dataset JSONL"), inner);
    m_btnEsporta->setEnabled(false);
    lay->addWidget(m_btnEsporta);

    m_btnAbort = new QPushButton(tr("Annulla"), inner);
    m_btnAbort->setObjectName("dangerBtn");
    m_btnAbort->setVisible(false);
    lay->addWidget(m_btnAbort);

    lay->addStretch();

    auto* hint = new QLabel(
        tr("<small>Il dataset viene salvato in<br>"
           "<tt>~/.prismalux/distillazione/</tt></small>"), inner);
    hint->setTextFormat(Qt::RichText);
    hint->setWordWrap(true);
    hint->setObjectName("mutedLabel");
    lay->addWidget(hint);

    scroll->setWidget(inner);

    connect(m_btnGenera,    &QPushButton::clicked, this, &DistillazionePage::onGeneraDomandeClicked);
    connect(m_btnInterroga, &QPushButton::clicked, this, &DistillazionePage::onInterrogaModelliClicked);
    connect(m_btnEsporta,   &QPushButton::clicked, this, &DistillazionePage::onEsportaDatasetClicked);
    connect(m_btnAbort,     &QPushButton::clicked, this, &DistillazionePage::onAbortClicked);

    return scroll;
}

/* ── Panel destro: avanzamento e dataset ────────────────────── */
QWidget* DistillazionePage::buildOutputPanel()
{
    auto* container = new QWidget(this);
    auto* lay       = new QVBoxLayout(container);
    lay->setContentsMargins(dpiScale(4), dpiScale(4), dpiScale(4), dpiScale(4));
    lay->setSpacing(dpiScale(4));

    m_progress = new QProgressBar(container);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->setFixedHeight(dpiScale(18));
    lay->addWidget(m_progress);

    m_outTabs = new QTabWidget(container);

    /* Tab Avanzamento */
    {
        auto* w  = new QWidget(m_outTabs);
        auto* vl = new QVBoxLayout(w);
        vl->setContentsMargins(dpiScale(4), dpiScale(4), dpiScale(4), dpiScale(4));
        m_log = new QTextEdit(w);
        m_log->setReadOnly(true);
        m_log->setObjectName("distLog");
        vl->addWidget(m_log);
        m_outTabs->addTab(w, tr("Avanzamento"));
    }

    /* Tab Dataset JSONL */
    {
        auto* w  = new QWidget(m_outTabs);
        auto* vl = new QVBoxLayout(w);
        vl->setContentsMargins(dpiScale(4), dpiScale(4), dpiScale(4), dpiScale(4));
        m_datasetPreview = new QTextEdit(w);
        m_datasetPreview->setReadOnly(true);
        m_datasetPreview->setObjectName("distDataset");
        QFont mono("Monospace");
        mono.setStyleHint(QFont::Monospace);
        mono.setPointSize(mono.pointSize() - 1);
        m_datasetPreview->setFont(mono);
        m_datasetPreview->setPlaceholderText(tr("Il dataset JSONL appare qui man mano che viene costruito."));
        vl->addWidget(m_datasetPreview);
        m_outTabs->addTab(w, tr("Dataset JSONL (0)"));
    }

    lay->addWidget(m_outTabs, 1);
    return container;
}

/* ── Helpers ─────────────────────────────────────────────────── */
void DistillazionePage::setFase(Fase f)
{
    m_fase = f;
    const bool attivo = (f != Fase::Idle && f != Fase::Fatto);
    m_btnGenera->setEnabled(!attivo);
    m_btnInterroga->setEnabled(!attivo && !m_domande.isEmpty());
    m_btnEsporta->setEnabled(!attivo && !m_dataset.isEmpty());
    m_btnAbort->setVisible(attivo);
    if (f == Fase::Idle || f == Fase::Fatto) m_progress->setValue(f == Fase::Fatto ? 100 : 0);
}

void DistillazionePage::appendLog(const QString& msg)
{
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_log->append(QString("[%1] %2").arg(ts, msg));
    m_outTabs->setCurrentIndex(0);
}

/* ── callOllama: POST diretto senza toccare m_ai ───────────── */
void DistillazionePage::callOllama(const QString& model, const QString& sys, const QString& user)
{
    if (m_pendingReply) {
        m_pendingReply->abort();
        m_pendingReply->deleteLater();
    }

    QJsonArray messages;
    if (!sys.isEmpty()) {
        QJsonObject s;
        s["role"]    = "system";
        s["content"] = sys;
        messages.append(s);
    }
    QJsonObject u;
    u["role"]    = "user";
    u["content"] = user;
    messages.append(u);

    QJsonObject body;
    body["model"]    = model;
    body["messages"] = messages;
    body["stream"]   = false;

    QUrl url(QString("http://localhost:%1/api/chat").arg(P::kOllamaPort));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_pendingReply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(m_pendingReply, &QNetworkReply::finished, this, &DistillazionePage::onReplyFinished);
}

/* ── Slot fetch modelli ─────────────────────────────────────── */
void DistillazionePage::onFetchModelsReady(const QStringList& models)
{
    m_modelliList->clear();
    m_giudiceCombo->clear();
    for (const QString& m : models) {
        auto* item = new QListWidgetItem(m, m_modelliList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        m_giudiceCombo->addItem(m, m);
    }
    if (m_modelliList->count() == 0) {
        m_modelliList->addItem(tr("(nessun modello trovato — avvia Ollama)"));
        m_statusLabel->setText(tr("\xe2\x9d\x8c Backend non raggiungibile."));
    } else {
        m_statusLabel->setText(tr("\xe2\x9c\x85 %1 modello/i disponibile/i.").arg(models.size()));
    }
}

void DistillazionePage::onFetchModelsError(const QString&)
{
    m_modelliList->clear();
    m_modelliList->addItem(tr("(errore caricamento — Ollama attivo?)"));
    m_statusLabel->setText(tr("\xe2\x9d\x8c Impossibile caricare i modelli."));
}

/* ── Slot "Genera Domande" ──────────────────────────────────── */
void DistillazionePage::onGeneraDomandeClicked()
{
    const QString dominio = m_dominioEdit->text().trimmed();
    if (dominio.isEmpty()) {
        QMessageBox::warning(this, tr("Campo vuoto"), tr("Specifica il dominio di specializzazione."));
        return;
    }

    /* Domande personalizzate: usale direttamente */
    const QString personali = m_domandeEdit->toPlainText().trimmed();
    if (!personali.isEmpty()) {
        m_domande.clear();
        for (const QString& r : personali.split('\n', Qt::SkipEmptyParts)) {
            const QString t = r.trimmed();
            if (!t.isEmpty()) m_domande << t;
        }
        appendLog(tr("Caricate %1 domande personalizzate.").arg(m_domande.size()));
        setFase(Fase::Fatto);
        m_btnInterroga->setEnabled(true);
        m_statusLabel->setText(tr("%1 domande pronte.").arg(m_domande.size()));
        return;
    }

    /* Generazione automatica via LLM */
    if (m_giudiceCombo->currentData().toString().isEmpty()) {
        QMessageBox::warning(this, tr("Nessun modello"), tr("Seleziona un LLM Giudice."));
        return;
    }

    setFase(Fase::GenerandoDomande);
    m_domande.clear();
    const int n   = m_nDomandeSpn->value();
    const QString giudice = m_giudiceCombo->currentData().toString();
    appendLog(tr("Generazione %1 domande su \"%2\" con %3...").arg(n).arg(dominio, giudice));

    const QString sysPrompt =
        tr("Sei un generatore di dataset. Rispondi SOLO con un array JSON valido, nessun testo extra.");
    const QString userPrompt =
        tr("Genera %1 domande diverse e specifiche sul dominio \"%2\".\n"
           "Spaziando tra livelli: base, intermedio, avanzato.\n"
           "Formato risposta (SOLO JSON, nessun altro testo):\n"
           "[\"domanda 1\",\"domanda 2\",...,\"domanda %1\"]").arg(n).arg(dominio);

    m_attesa = AttesaTipo::Domande;
    callOllama(giudice, sysPrompt, userPrompt);
}

/* ── Slot "Interroga Modelli" ───────────────────────────────── */
void DistillazionePage::onInterrogaModelliClicked()
{
    m_modelliSelezionati.clear();
    for (int i = 0; i < m_modelliList->count(); ++i) {
        auto* item = m_modelliList->item(i);
        if (item && item->checkState() == Qt::Checked)
            m_modelliSelezionati << item->text();
    }
    /* Supporta anche selezione classica (highlight) come fallback */
    if (m_modelliSelezionati.isEmpty()) {
        for (auto* item : m_modelliList->selectedItems())
            m_modelliSelezionati << item->text();
    }
    if (m_modelliSelezionati.isEmpty()) {
        QMessageBox::warning(this, tr("Nessun modello"),
            tr("Spunta o seleziona almeno un modello da interrogare."));
        return;
    }
    if (m_domande.isEmpty()) {
        QMessageBox::warning(this, tr("Nessuna domanda"),
            tr("Genera prima le domande (pulsante 1)."));
        return;
    }

    m_dataset.clear();
    m_iDomanda = 0;
    m_iModello = 0;
    m_progress->setValue(0);
    setFase(Fase::Interrogando);
    appendLog(tr("Avvio interrogazione: %1 domande x %2 modelli.")
              .arg(m_domande.size()).arg(m_modelliSelezionati.size()));
    processNextInterrogazione();
}

/* ── State machine: interroga modello corrente ──────────────── */
void DistillazionePage::processNextInterrogazione()
{
    if (m_iDomanda >= m_domande.size()) {
        setFase(Fase::Fatto);
        appendLog(tr("Dataset completato: %1 voci.").arg(m_dataset.size()));
        m_btnEsporta->setEnabled(true);
        aggiornaPreviewDataset();
        m_outTabs->setCurrentIndex(1);
        return;
    }

    /* Prima iterazione su questa domanda: inizializza entry */
    if (m_iModello == 0) {
        m_entryCorrente = DistEntry{};
        m_entryCorrente.domanda = m_domande[m_iDomanda];
    }

    const QString dominio  = m_dominioEdit->text().trimmed();
    const QString modello  = m_modelliSelezionati[m_iModello];
    const QString domanda  = m_entryCorrente.domanda;
    const int tot          = m_domande.size() * m_modelliSelezionati.size();
    const int fatto        = m_iDomanda * m_modelliSelezionati.size() + m_iModello;
    m_progress->setValue(tot > 0 ? (fatto * 100 / tot) : 0);

    appendLog(tr("[D%1/%2] %3 → %4")
              .arg(m_iDomanda + 1).arg(m_domande.size())
              .arg(modello).arg(domanda.left(60)));

    m_attesa = AttesaTipo::Risposta;
    callOllama(modello,
               tr("Sei un esperto di %1. Rispondi in modo accurato e completo.").arg(dominio),
               domanda);
}

/* ── State machine: valuta risposta migliore ─────────────────── */
void DistillazionePage::processNextValutazione()
{
    if (m_modelliSelezionati.size() == 1) {
        /* Un solo modello: nessuna valutazione necessaria */
        m_entryCorrente.migliorRisposta = m_entryCorrente.risposte.values().first();
        m_entryCorrente.modelloMiglior  = m_modelliSelezionati.first();
        finalizzaEntry("1");
        return;
    }

    setFase(Fase::Valutando);
    appendLog(tr("  Giudice valuta %1 risposte...").arg(m_modelliSelezionati.size()));

    const QString domanda = m_entryCorrente.domanda;
    QString prompt = tr("Domanda: %1\n\n").arg(domanda);
    int idx = 1;
    for (const QString& mod : m_modelliSelezionati) {
        const QString r = m_entryCorrente.risposte.value(mod);
        prompt += tr("Risposta %1 (da %2):\n%3\n\n").arg(idx++).arg(mod, r.left(800));
    }
    prompt += tr("Quale risposta \xc3\xa8 pi\xc3\xb9 accurata, completa e utile? "
                 "Rispondi SOLO con il numero (1, 2, ecc.) e nulla altro.");

    const QString giudice = m_giudiceCombo->currentData().toString();
    m_attesa = AttesaTipo::Valutazione;
    callOllama(giudice, QString(), prompt);
}

/* ── Parsa domande JSON dall'LLM ───────────────────────────── */
void DistillazionePage::parsaDomande(const QString& raw)
{
    /* Estrai il primo array JSON trovato nel testo */
    const int start = raw.indexOf('[');
    const int end   = raw.lastIndexOf(']');
    if (start < 0 || end <= start) {
        appendLog(tr("\xe2\x9d\x8c Formato domande non riconosciuto. Riprovare."));
        setFase(Fase::Idle);
        return;
    }
    const QByteArray jsonRaw = raw.mid(start, end - start + 1).toUtf8();
    const QJsonDocument doc  = QJsonDocument::fromJson(jsonRaw);
    if (!doc.isArray()) {
        appendLog(tr("\xe2\x9d\x8c JSON domande non valido. Riprovare."));
        setFase(Fase::Idle);
        return;
    }
    for (const QJsonValue& v : doc.array()) {
        const QString d = v.toString().trimmed();
        if (!d.isEmpty()) m_domande << d;
    }
    appendLog(tr("\xe2\x9c\x85 %1 domande generate.").arg(m_domande.size()));
    setFase(Fase::Fatto);
    m_btnInterroga->setEnabled(true);
    m_statusLabel->setText(tr("%1 domande pronte.").arg(m_domande.size()));
}

/* ── Finalizza entry dopo valutazione giudice ───────────────── */
void DistillazionePage::finalizzaEntry(const QString& sceltaGiudice)
{
    /* Parsa numero dalla risposta del giudice */
    int scelta = 1;
    for (const QChar& c : sceltaGiudice) {
        if (c.isDigit()) { scelta = c.digitValue(); break; }
    }
    scelta = qBound(1, scelta, m_modelliSelezionati.size());

    const QString modVincitore = m_modelliSelezionati[scelta - 1];
    m_entryCorrente.migliorRisposta = m_entryCorrente.risposte.value(modVincitore);
    m_entryCorrente.modelloMiglior  = modVincitore;

    m_dataset.append(m_entryCorrente);
    appendLog(tr("  \xe2\x9c\x94 Risposta scelta: %1").arg(modVincitore));

    aggiornaPreviewDataset();
    m_outTabs->setTabText(1, tr("Dataset JSONL (%1)").arg(m_dataset.size()));

    m_iDomanda++;
    m_iModello = 0;
    setFase(Fase::Interrogando);
    processNextInterrogazione();
}

/* ── Aggiorna preview dataset ────────────────────────────────── */
void DistillazionePage::aggiornaPreviewDataset()
{
    const QString dominio = m_dominioEdit->text().trimmed();
    const QString sys     = tr("Sei un esperto di %1.").arg(dominio);
    QString preview;
    /* Mostra le ultime 5 voci per non rallentare */
    const int start = qMax(0, m_dataset.size() - 5);
    for (int i = start; i < m_dataset.size(); ++i) {
        const DistEntry& e = m_dataset[i];
        QJsonObject obj;
        obj["instruction"] = e.domanda;
        obj["input"]       = QString();
        obj["output"]      = e.migliorRisposta;
        obj["system"]      = sys;
        obj["source_model"]= e.modelloMiglior;
        obj["domain"]      = dominio;
        preview += QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
    }
    if (start > 0) preview.prepend(tr("... (prime %1 voci non mostrate) ...\n").arg(start));
    m_datasetPreview->setPlainText(preview);
}

/* ── Slot "Esporta Dataset JSONL" ────────────────────────────── */
void DistillazionePage::onEsportaDatasetClicked()
{
    if (m_dataset.isEmpty()) {
        QMessageBox::information(this, tr("Dataset vuoto"), tr("Nessuna voce da esportare."));
        return;
    }

    /* Directory predefinita */
    const QString defDir = QDir::homePath() + "/.prismalux/distillazione";
    QDir().mkpath(defDir);
    const QString defName = defDir + "/dataset_" +
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmm") + ".jsonl";

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Salva Dataset JSONL"), defName,
        tr("JSONL (*.jsonl);;Tutti i file (*)"));
    if (path.isEmpty()) return;

    const QString dominio = m_dominioEdit->text().trimmed();
    const QString sys     = tr("Sei un esperto di %1.").arg(dominio);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Errore"), tr("Impossibile scrivere: %1").arg(path));
        return;
    }
    QTextStream out(&f);
    for (const DistEntry& e : m_dataset) {
        QJsonObject obj;
        obj["instruction"]  = e.domanda;
        obj["input"]        = QString();
        obj["output"]       = e.migliorRisposta;
        obj["system"]       = sys;
        obj["source_model"] = e.modelloMiglior;
        obj["domain"]       = dominio;
        out << QJsonDocument(obj).toJson(QJsonDocument::Compact) << "\n";
    }
    f.close();

    appendLog(tr("\xe2\x9c\x85 Dataset esportato: %1 (%2 voci)").arg(path).arg(m_dataset.size()));
    QMessageBox::information(this, tr("Esportazione completata"),
        tr("Dataset salvato in:\n%1\n\n%2 voci JSONL.")
        .arg(path).arg(m_dataset.size()));
}

/* ── Slot "Annulla" ──────────────────────────────────────────── */
void DistillazionePage::onAbortClicked()
{
    if (m_pendingReply) {
        m_pendingReply->abort();
        m_pendingReply->deleteLater();
    }
    appendLog(tr("Operazione annullata."));
    setFase(Fase::Idle);
    m_btnEsporta->setEnabled(!m_dataset.isEmpty());
}

/* ── Reply HTTP: dispatcher ──────────────────────────────────── */
void DistillazionePage::onReplyFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() == QNetworkReply::OperationCanceledError) return;
        appendLog(tr("\xe2\x9d\x8c Errore rete: %1").arg(reply->errorString()));
        setFase(Fase::Idle);
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    const QString content = doc["message"]["content"].toString().trimmed();

    switch (m_attesa) {
    case AttesaTipo::Domande:
        parsaDomande(content);
        break;
    case AttesaTipo::Risposta:
        m_entryCorrente.risposte[m_modelliSelezionati[m_iModello]] = content;
        m_iModello++;
        if (m_iModello < m_modelliSelezionati.size())
            processNextInterrogazione();
        else
            processNextValutazione();
        break;
    case AttesaTipo::Valutazione:
        finalizzaEntry(content);
        break;
    }
}

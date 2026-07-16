/* ══════════════════════════════════════════════════════════════
   main_sci_protein.cpp — Strumenti interattivi per strutture proteiche
   ══════════════════════════════════════════════════════════════ */
#include "main_sci_protein.h"
#include "../dpi_utils.h"
#include "../log_bus.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QListWidget>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QRegularExpression>

#ifdef HAVE_WEBENGINE_WIDGETS
#  include <QWebEngineView>
#  include <QWebEnginePage>
#endif

/* ══════════════════════════════════════════════════════════════
   HTML per il viewer 3Dmol.js
   ══════════════════════════════════════════════════════════════ */
static const char* kViewerHtml = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;background:#0f172a;overflow:hidden}
#v{width:100%;height:100%;position:relative}
#msg{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);
     color:#94a3b8;font:13px/1.5 sans-serif;text-align:center;pointer-events:none}
#info{position:absolute;bottom:4px;left:4px;color:#475569;font-size:10px;
      font-family:monospace;pointer-events:none}
</style>
</head><body>
<div id="v"></div>
<div id="msg">Carica una struttura PDB per visualizzarla in 3D<br>
<small style="color:#334155">ESMFold · AlphaFold DB · RCSB PDB</small></div>
<div id="info"></div>
<script src="https://3dmol.org/build/3Dmol-min.js"></script>
<script>
var _v = null;
var _pdbLen = 0;
function init() {
  if (typeof $3Dmol === 'undefined') { setTimeout(init, 300); return; }
  _v = $3Dmol.createViewer('v', {backgroundColor:'0x0f172a'});
  _v.render();
}
function loadPdb(pdb, label) {
  if (!_v) { init(); setTimeout(function(){ loadPdb(pdb,label); }, 400); return; }
  document.getElementById('msg').style.display = 'none';
  _v.clear();
  _v.addModel(pdb, 'pdb');
  _v.setStyle({}, {cartoon:{color:'spectrum'}});
  _v.addSurface($3Dmol.SurfaceType.VDW, {opacity:0.07, color:'white'});
  _v.zoomTo();
  _v.render();
  _pdbLen = pdb.length;
  document.getElementById('info').textContent =
    (label || '') + '  |  ' + Math.round(_pdbLen/1024) + ' KB';
}
function setStyle(s) {
  if (!_v) return;
  _v.setStyle({}, s === 'stick'  ? {stick:{}}  :
                   s === 'sphere' ? {sphere:{radius:0.5}} :
                   {cartoon:{color:'spectrum'}});
  _v.render();
}
window.onload = init;
</script>
</body></html>
)HTML";

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
SciProteinWidget::SciProteinWidget(QWidget* parent)
    : QWidget(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    auto* rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    /* ── Pannello sinistro: input + database ── */
    auto* leftPanel  = new QWidget;
    auto* leftLay    = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(dpiScale(8), dpiScale(8), dpiScale(4), dpiScale(8));
    leftLay->setSpacing(dpiScale(8));
    leftPanel->setMinimumWidth(dpiScale(260));

    /* Sequenza + ESMFold */
    auto* seqGroup = new QGroupBox(
        tr("\xf0\x9f\xa7\xac  Predizione struttura (ESMFold)"), leftPanel);
    auto* seqLay   = new QVBoxLayout(seqGroup);
    seqLay->setSpacing(dpiScale(4));

    seqLay->addWidget(new QLabel(
        tr("<small>Incolla sequenza FASTA o amminoacidi grezzi (max 400 aa per API)</small>"),
        seqGroup));

    m_seqEdit = new QTextEdit(seqGroup);
    m_seqEdit->setFixedHeight(dpiScale(90));
    m_seqEdit->setObjectName("codeEdit");
    m_seqEdit->setPlaceholderText(
        tr(">MyProtein\nMKTLLLTLVVVTIVCLDLGAV..."));
    seqLay->addWidget(m_seqEdit);

    m_btnFold = new QPushButton(
        tr("\xf0\x9f\xa7\xac  Predici struttura (ESMFold API)"), seqGroup);
    m_btnFold->setObjectName("primaryBtn");
    m_btnFold->setToolTip(
        "Chiama ESMFold API (Meta) — gratuito, max 400 aa\n"
        "Per sequenze lunghe usa AlphaFold DB sotto");
    seqLay->addWidget(m_btnFold);

    m_foldStatus = new QLabel("", seqGroup);
    m_foldStatus->setWordWrap(true);
    m_foldStatus->setObjectName("hintLabel");
    seqLay->addWidget(m_foldStatus);

    leftLay->addWidget(seqGroup);

    /* AlphaFold DB */
    auto* afGroup = new QGroupBox(
        tr("\xf0\x9f\x94\xae  AlphaFold DB (EBI/DeepMind)"), leftPanel);
    auto* afLay   = new QVBoxLayout(afGroup);
    afLay->setSpacing(dpiScale(4));

    afLay->addWidget(new QLabel(
        tr("<small>Strutture pre-calcolate per ~200M proteine</small>"), afGroup));

    auto* afRow = new QHBoxLayout;
    m_uniprotEdit = new QLineEdit(afGroup);
    m_uniprotEdit->setPlaceholderText(tr("UniProt ID  (es. P05067)"));
    m_btnAlpha = new QPushButton("\xf0\x9f\x94\x8d", afGroup);
    m_btnAlpha->setFixedWidth(dpiScale(30));
    m_btnAlpha->setObjectName("actionBtn");
    m_btnAlpha->setToolTip(tr("Scarica struttura da AlphaFold DB"));
    afRow->addWidget(m_uniprotEdit, 1);
    afRow->addWidget(m_btnAlpha);
    afLay->addLayout(afRow);
    leftLay->addWidget(afGroup);

    /* Ricerca PDB + UniProt */
    auto* dbGroup = new QGroupBox(
        tr("\xf0\x9f\x97\x83  Ricerca database"), leftPanel);
    auto* dbLay   = new QVBoxLayout(dbGroup);
    dbLay->setSpacing(dpiScale(4));

    auto* pdbRow  = new QHBoxLayout;
    m_pdbQuery    = new QLineEdit(dbGroup);
    m_pdbQuery->setPlaceholderText(tr("Cerca in RCSB PDB (es. insulin, 1ABC)"));
    auto* btnPdb  = new QPushButton("\xf0\x9f\x94\x8d", dbGroup);
    btnPdb->setFixedWidth(dpiScale(30));
    btnPdb->setObjectName("actionBtn");
    pdbRow->addWidget(m_pdbQuery, 1);
    pdbRow->addWidget(btnPdb);
    dbLay->addLayout(pdbRow);

    auto* upRow   = new QHBoxLayout;
    m_upQuery     = new QLineEdit(dbGroup);
    m_upQuery->setPlaceholderText(tr("Cerca in UniProt (es. Alzheimer, BRCA1)"));
    auto* btnUp   = new QPushButton("\xf0\x9f\x94\x8d", dbGroup);
    btnUp->setFixedWidth(dpiScale(30));
    btnUp->setObjectName("actionBtn");
    upRow->addWidget(m_upQuery, 1);
    upRow->addWidget(btnUp);
    dbLay->addLayout(upRow);

    m_searchList = new QListWidget(dbGroup);
    m_searchList->setFixedHeight(dpiScale(140));
    m_searchList->setToolTip(
        tr("Doppio click per scaricare la struttura PDB"));
    dbLay->addWidget(m_searchList);

    leftLay->addWidget(dbGroup);

    /* Azioni */
    auto* actRow = new QHBoxLayout;
    m_btnDownload = new QPushButton(
        tr("\xe2\xac\x87  Salva .pdb"), leftPanel);
    m_btnDownload->setObjectName("actionBtn");
    m_btnDownload->setEnabled(false);
    m_btnDock = new QPushButton(
        tr("\xf0\x9f\xa7\xaa  Invia al Docking"), leftPanel);
    m_btnDock->setObjectName("actionBtn");
    m_btnDock->setEnabled(false);
    m_btnDock->setToolTip(
        tr("Crea automaticamente una Work Unit AutoDock Vina\n"
        "nel sistema BOINC con questa struttura come recettore"));
    actRow->addWidget(m_btnDownload, 1);
    actRow->addWidget(m_btnDock, 1);
    leftLay->addLayout(actRow);

    /* Log */
    m_logView = new QTextEdit(leftPanel);
    m_logView->setReadOnly(true);
    m_logView->setFixedHeight(dpiScale(80));
    m_logView->setObjectName("codeEdit");
    leftLay->addWidget(m_logView);
    leftLay->addStretch(1);

    splitter->addWidget(leftPanel);

    /* ── Pannello destro: viewer 3D ── */
#ifdef HAVE_WEBENGINE_WIDGETS
    m_viewer3d = new QWebEngineView(this);
    m_viewer3d->setMinimumWidth(dpiScale(350));
    m_viewer3d->setHtml(QString::fromUtf8(kViewerHtml),
                         QUrl("https://prismalux.local/"));
    splitter->addWidget(m_viewer3d);
#else
    m_viewer3d = new QTextEdit(this);
    m_viewer3d->setReadOnly(true);
    m_viewer3d->setObjectName("codeEdit");
    m_viewer3d->setPlaceholderText(
        tr("Viewer 3D non disponibile (richiede Qt6::WebEngineWidgets).\n"
        "Il testo PDB verrà mostrato qui."));
    splitter->addWidget(m_viewer3d);
#endif

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    rootLay->addWidget(splitter, 1);

    /* ── Connessioni ── */
    connect(m_btnFold,    &QPushButton::clicked, this, &SciProteinWidget::onEsmFoldClicked);
    connect(m_btnAlpha,   &QPushButton::clicked, this, &SciProteinWidget::onAlphaFoldClicked);
    connect(btnPdb,       &QPushButton::clicked, this, &SciProteinWidget::onPdbSearchClicked);
    connect(btnUp,        &QPushButton::clicked, this, &SciProteinWidget::onUniprotSearchClicked);
    connect(m_pdbQuery,   &QLineEdit::returnPressed, this, &SciProteinWidget::onPdbSearchClicked);
    connect(m_upQuery,    &QLineEdit::returnPressed, this, &SciProteinWidget::onUniprotSearchClicked);
    connect(m_uniprotEdit,&QLineEdit::returnPressed, this, &SciProteinWidget::onAlphaFoldClicked);
    connect(m_searchList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem*){ onSearchItemClicked(m_searchList->currentRow()); });
    connect(m_btnDownload,&QPushButton::clicked, this, &SciProteinWidget::onDownloadPdbClicked);
    connect(m_btnDock,    &QPushButton::clicked, this, &SciProteinWidget::onSendToDockClicked);
}

/* ══════════════════════════════════════════════════════════════
   Helpers
   ══════════════════════════════════════════════════════════════ */

QString SciProteinWidget::cleanSequence(const QString& raw)
{
    QString out;
    for (const auto& line : raw.split('\n'))
        if (!line.trimmed().startsWith('>'))
            out += line.trimmed().toUpper();
    /* Rimuovi caratteri non amminoacidici */
    static const QRegularExpression reNonAA(R"([^ACDEFGHIKLMNPQRSTVWY])");
    out.remove(reNonAA);
    return out;
}

void SciProteinWidget::appendLog(const QString& msg)
{
    if (!m_logView) return;
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_logView->append("[" + ts + "]  " + msg);
}

void SciProteinWidget::loadInViewer(const QString& pdbData, const QString& label)
{
    m_lastPdb   = pdbData;
    m_lastLabel = label.isEmpty() ? "protein" : label;

    m_btnDownload->setEnabled(!pdbData.isEmpty());
    m_btnDock->setEnabled(!pdbData.isEmpty());

#ifdef HAVE_WEBENGINE_WIDGETS
    /* Escaping PDB per JavaScript: backtick e backslash */
    QString safe = pdbData;
    safe.replace('\\', "\\\\");
    safe.replace('`',  "\\`");
    const QString js = QString("loadPdb(`%1`, `%2`);")
                       .arg(safe, m_lastLabel);
    m_viewer3d->page()->runJavaScript(js);
#else
    m_viewer3d->setPlainText(pdbData);
#endif

    appendLog(QString("\xf0\x9f\x9f\xa2  Struttura caricata: %1 (%2 aa)")
              .arg(m_lastLabel).arg(pdbData.count("ATOM")));
    emit pdbReady(pdbData, m_lastLabel);
}

/* ══════════════════════════════════════════════════════════════
   ESMFold API
   ══════════════════════════════════════════════════════════════ */

void SciProteinWidget::onEsmFoldClicked()
{
    const QString seq = cleanSequence(m_seqEdit->toPlainText());
    if (seq.isEmpty()) {
        m_foldStatus->setText(
            tr("<span style='color:#ef4444'>Inserisci una sequenza amminoacidica.</span>"));
        return;
    }
    if (seq.size() > 400) {
        m_foldStatus->setText(
            QString("<span style='color:#f59e0b'>"
                    "Sequenza lunga (%1 aa). API limita a 400 aa.<br>"
                    "Usa AlphaFold DB se hai il UniProt ID, o esmfold_local sul worker.</span>")
            .arg(seq.size()));
        return;
    }

    m_btnFold->setEnabled(false);
    m_foldStatus->setText(tr("\xf0\x9f\x94\x84  Predizione in corso (30-120s)..."));
    appendLog("ESMFold API: invio sequenza " + QString::number(seq.size()) + " aa...");

    QNetworkRequest req(QUrl("https://api.esmatlas.com/foldSequence/v1/pdb/"));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/x-www-form-urlencoded");
    req.setHeader(QNetworkRequest::UserAgentHeader, "Prismalux/3.0");
    req.setTransferTimeout(180000);

    QNetworkReply* reply = m_nam->post(req, seq.toUtf8());
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] { onEsmFoldReply(reply); });
}

void SciProteinWidget::onEsmFoldReply(QNetworkReply* reply)
{
    reply->deleteLater();
    m_btnFold->setEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
        const QString err = reply->errorString();
        m_foldStatus->setText(
            "<span style='color:#ef4444'>\xe2\x9d\x8c  " + err + "</span>");
        appendLog("ESMFold errore: " + err);
        LogBus::post("\xe2\x9d\x8c SciProtein: ESMFold errore: " + err);
        return;
    }

    const QString pdb = QString::fromUtf8(reply->readAll()).trimmed();
    if (!pdb.startsWith("ATOM") && !pdb.startsWith("REMARK") && !pdb.startsWith("MODEL")) {
        m_foldStatus->setText(
            tr("<span style='color:#ef4444'>\xe2\x9d\x8c  Risposta API non valida."
            " Riprova tra qualche secondo.</span>"));
        appendLog("ESMFold: risposta non PDB (" + pdb.left(80) + ")");
        LogBus::post("\xe2\x9d\x8c SciProtein: ESMFold risposta non PDB.");
        return;
    }

    m_foldStatus->setText(
        tr("<span style='color:#22c55e'>\xe2\x9c\x85  Struttura predetta!</span>"));
    const QString label = "ESMFold_" +
        QDateTime::currentDateTime().toString("hhmmss");
    loadInViewer(pdb, label);
}

/* ══════════════════════════════════════════════════════════════
   AlphaFold DB
   ══════════════════════════════════════════════════════════════ */

void SciProteinWidget::onAlphaFoldClicked()
{
    const QString uid = m_uniprotEdit->text().trimmed().toUpper();
    if (uid.isEmpty()) {
        appendLog("Inserisci un UniProt ID (es. P05067 = APP umana).");
        return;
    }
    appendLog("AlphaFold DB: cercando struttura per " + uid + "...");
    m_btnAlpha->setEnabled(false);

    QNetworkRequest req(QUrl(
        "https://alphafold.ebi.ac.uk/api/prediction/" + uid));
    req.setHeader(QNetworkRequest::UserAgentHeader, "Prismalux/3.0");
    req.setTransferTimeout(30000);

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] { onAlphaFoldReply(reply); });
}

void SciProteinWidget::onAlphaFoldReply(QNetworkReply* reply)
{
    reply->deleteLater();
    m_btnAlpha->setEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
        appendLog("AlphaFold DB errore: " + reply->errorString());
        LogBus::post("\xe2\x9d\x8c SciProtein: AlphaFold DB errore: " + reply->errorString());
        return;
    }
    const QJsonArray arr = QJsonDocument::fromJson(reply->readAll()).array();
    if (arr.isEmpty()) {
        appendLog("AlphaFold DB: nessuna struttura trovata per questo ID.");
        return;
    }
    const QJsonObject entry = arr.first().toObject();
    const QString pdbUrl    = entry["pdbUrl"].toString();
    const QString uniId     = entry["uniprotId"].toString();
    appendLog(QString("AlphaFold DB: struttura trovata (%1). Download PDB...").arg(uniId));

    if (pdbUrl.isEmpty()) {
        appendLog("URL PDB mancante nella risposta.");
        LogBus::post("\xe2\x9d\x8c SciProtein: URL PDB mancante nella risposta AlphaFold.");
        return;
    }

    const QUrl afPdbQUrl(pdbUrl);
    QNetworkRequest pdbReq(afPdbQUrl);
    pdbReq.setHeader(QNetworkRequest::UserAgentHeader, "Prismalux/3.0");
    pdbReq.setTransferTimeout(60000);

    QNetworkReply* pdbReply = m_nam->get(pdbReq);
    connect(pdbReply, &QNetworkReply::finished, this, [this, pdbReply, uniId] {
        pdbReply->deleteLater();
        if (pdbReply->error() != QNetworkReply::NoError) {
            appendLog("Download PDB fallito: " + pdbReply->errorString());
            LogBus::post("\xe2\x9d\x8c SciProtein: Download PDB fallito: " + pdbReply->errorString());
            return;
        }
        const QString pdb = QString::fromUtf8(pdbReply->readAll());
        loadInViewer(pdb, "AlphaFold_" + uniId);
    });
}

/* ══════════════════════════════════════════════════════════════
   RCSB PDB Search
   ══════════════════════════════════════════════════════════════ */

void SciProteinWidget::onPdbSearchClicked()
{
    const QString query = m_pdbQuery->text().trimmed();
    if (query.isEmpty()) return;
    appendLog("RCSB PDB: ricerca \"" + query + "\"...");
    m_searchList->clear();

    /* Prova prima come PDB ID diretto (4 caratteri alfanumerici) */
    static const QRegularExpression rePdbId(R"(^[0-9][A-Za-z0-9]{3}$)");
    if (rePdbId.match(query).hasMatch()) {
        /* Download diretto per PDB ID */
        const QString pdbId = query.toUpper();
        auto* item = new QListWidgetItem(
            "\xf0\x9f\x97\x82  [PDB] " + pdbId);
        item->setData(Qt::UserRole, "pdb:" + pdbId);
        m_searchList->addItem(item);
    }

    /* Full-text search RCSB */
    const QJsonObject searchBody = QJsonDocument::fromJson(R"({
        "query":{"type":"terminal","service":"full_text",
                 "parameters":{"value":"PLACEHOLDER"}},
        "return_type":"entry",
        "request_options":{"results_slice":{"start":0,"limit":8}}
    })").object();

    QJsonObject filled = searchBody;
    QJsonObject q = filled["query"].toObject();
    QJsonObject p = q["parameters"].toObject();
    p["value"] = query;
    q["parameters"] = p;
    filled["query"] = q;

    QNetworkRequest req(QUrl(
        "https://search.rcsb.org/rcsbsearch/v2/query"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setHeader(QNetworkRequest::UserAgentHeader, "Prismalux/3.0");
    req.setTransferTimeout(15000);

    QNetworkReply* reply = m_nam->post(req,
        QJsonDocument(filled).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] { onPdbReply(reply); });
}

void SciProteinWidget::onPdbReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        appendLog("RCSB errore: " + reply->errorString());
        LogBus::post("\xe2\x9d\x8c SciProtein: RCSB errore: " + reply->errorString());
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonArray  hits = root["result_set"].toArray();
    if (hits.isEmpty()) {
        appendLog("RCSB PDB: nessun risultato.");
        return;
    }
    for (const auto& h : hits) {
        const QString id = h.toObject()["identifier"].toString();
        auto* item = new QListWidgetItem(
            "\xf0\x9f\x97\x82  [PDB] " + id);
        item->setData(Qt::UserRole, "pdb:" + id);
        m_searchList->addItem(item);
    }
    appendLog(QString("RCSB PDB: trovati %1 risultati. Doppio click per aprire.").arg(hits.size()));
}

/* ══════════════════════════════════════════════════════════════
   UniProt Search
   ══════════════════════════════════════════════════════════════ */

void SciProteinWidget::onUniprotSearchClicked()
{
    const QString query = m_upQuery->text().trimmed();
    if (query.isEmpty()) return;
    appendLog("UniProt: ricerca \"" + query + "\"...");
    m_searchList->clear();

    QUrl url("https://rest.uniprot.org/uniprotkb/search");
    QUrlQuery params;
    params.addQueryItem("query",  query);
    params.addQueryItem("format", "json");
    params.addQueryItem("size",   "8");
    params.addQueryItem("fields", "id,protein_name,organism_name,length");
    url.setQuery(params);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Prismalux/3.0");
    req.setTransferTimeout(15000);

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] { onUniprotReply(reply); });
}

void SciProteinWidget::onUniprotReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        appendLog("UniProt errore: " + reply->errorString());
        LogBus::post("\xe2\x9d\x8c SciProtein: UniProt errore: " + reply->errorString());
        return;
    }
    const QJsonObject root    = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonArray  results = root["results"].toArray();
    if (results.isEmpty()) { appendLog("UniProt: nessun risultato."); return; }

    for (const auto& r : results) {
        const QJsonObject entry = r.toObject();
        const QString uid  = entry["primaryAccession"].toString();
        const QString name = entry["proteinDescription"].toObject()
                             ["recommendedName"].toObject()
                             ["fullName"].toObject()["value"].toString();
        const QString org  = entry["organism"].toObject()
                             ["scientificName"].toString();
        const int     len  = entry["sequence"].toObject()["length"].toInt();
        const QString disp = QString("\xf0\x9f\xa7\xac  [UP] %1 — %2 (%3) %4aa")
                             .arg(uid, name.isEmpty() ? "?" : name, org).arg(len);
        auto* item = new QListWidgetItem(disp);
        item->setData(Qt::UserRole, "uniprot:" + uid);
        item->setToolTip(name + "\n" + org + "\n" + QString::number(len) + " aa");
        m_searchList->addItem(item);
    }
    appendLog(QString("UniProt: %1 proteine trovate.").arg(results.size()));
}

/* ══════════════════════════════════════════════════════════════
   Attivazione risultato (download PDB)
   ══════════════════════════════════════════════════════════════ */

void SciProteinWidget::onSearchItemClicked(int row)
{
    if (row < 0 || !m_searchList->item(row)) return;
    const QString data = m_searchList->item(row)->data(Qt::UserRole).toString();

    if (data.startsWith("pdb:")) {
        const QString pdbId = data.mid(4).toUpper();
        appendLog("Download PDB: " + pdbId + "...");

        QNetworkRequest req(QUrl(
            "https://files.rcsb.org/download/" + pdbId + ".pdb"));
        req.setHeader(QNetworkRequest::UserAgentHeader, "Prismalux/3.0");
        req.setTransferTimeout(30000);

        QNetworkReply* reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, pdbId] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                appendLog("Download fallito: " + reply->errorString());
                LogBus::post("\xe2\x9d\x8c SciProtein: Download PDB " + pdbId + " fallito: " + reply->errorString());
                return;
            }
            loadInViewer(QString::fromUtf8(reply->readAll()), "PDB_" + pdbId);
        });

    } else if (data.startsWith("uniprot:")) {
        /* Per UniProt: carica struttura da AlphaFold DB */
        const QString uid = data.mid(8);
        m_uniprotEdit->setText(uid);
        onAlphaFoldClicked();
    }
}

/* ══════════════════════════════════════════════════════════════
   Download e invio al docking
   ══════════════════════════════════════════════════════════════ */

void SciProteinWidget::onDownloadPdbClicked()
{
    if (m_lastPdb.isEmpty()) return;
    const QString path = QFileDialog::getSaveFileName(
        this, "Salva struttura PDB",
        QDir::homePath() + "/" + m_lastLabel + ".pdb",
        "PDB Files (*.pdb);;All Files (*)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(m_lastPdb.toUtf8());
    appendLog("\xf0\x9f\x92\xbe  Struttura salvata: " + path);
}

void SciProteinWidget::onSendToDockClicked()
{
    if (m_lastPdb.isEmpty()) return;
    /* Salva in temp e emetti segnale — SciComputePage creerà la WU */
    const QString tmpPath = QDir::tempPath() + "/" + m_lastLabel + ".pdb";
    QFile f(tmpPath);
    if (f.open(QIODevice::WriteOnly))
        f.write(m_lastPdb.toUtf8());
    appendLog("\xf0\x9f\xa7\xaa  Struttura inviata al sistema docking: " + tmpPath);
    emit pdbReady(m_lastPdb, m_lastLabel);
}

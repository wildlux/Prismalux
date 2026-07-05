/*
 * main_jobs_assistant.cpp — Assistente Candidature (LavoroPage)
 * ===============================================================
 * Browser embedded (QWebEngineView) + registrazione manuale dei passi
 * (ALT+click su un campo) via QWebChannel, riproduzione automatica per
 * selettore CSS (non per coordinate — sopravvive a resize/riposizionamento
 * della finestra). Intero file compilato solo se HAVE_JOB_ASSISTANT è
 * definito (richiede Qt6::WebEngineWidgets + Qt6::WebChannel).
 */
#include "main_jobs.h"
#ifdef HAVE_JOB_ASSISTANT

#include "../dpi_utils.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QScrollArea>
#include <QFrame>
#include <QListWidgetItem>
#include <QInputDialog>
#include <QSignalBlocker>
#include <QWebEnginePage>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QPointer>
#include <QUrl>
#include <QFileDialog>
#include <QFileInfo>

/* ── Helper locale: quoting sicuro per stringhe letterali JS ── */
static QString jsQuote(const QString& s)
{
    QString e = s;
    e.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n");
    return "'" + e + "'";
}

/* ══════════════════════════════════════════════════════════════
   Costruzione UI
   ══════════════════════════════════════════════════════════════ */
QWidget* LavoroPage::buildAssistenteTab(QWidget* parent, QWidget* candidatureContent)
{
    auto* root = new QWidget(parent);
    auto* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(8, 8, 8, 8);
    rootLay->setSpacing(4);

    /* ──── Barra superiore a piena larghezza: naviga il browser + apri/
       nascondi colonne — è in comune a tutte e 3 le colonne, quindi vive
       fuori dallo splitter invece che dentro la colonna del browser. ──── */
    auto* toolbar = new QWidget(root);
    auto* toolL = new QHBoxLayout(toolbar);
    toolL->setContentsMargins(0, 0, 0, 0);
    toolL->setSpacing(4);

    auto* backBtn = new QPushButton("\xe2\x97\x80", toolbar);   /* ◀ */
    auto* fwdBtn  = new QPushButton("\xe2\x96\xb6", toolbar);   /* ▶ */
    auto* reloadBtn = new QPushButton("\xe2\x9f\xb3", toolbar); /* ⟳ */
    for (auto* b : {backBtn, fwdBtn}) {
        b->setObjectName("actionBtn");
        b->setFixedSize(dpiScale(16), dpiScale(16));
        auto f = b->font();
        f.setPointSize(qMax(6, f.pointSize() - 3));
        b->setFont(f);
    }
    reloadBtn->setObjectName("actionBtn");
    reloadBtn->setFixedSize(dpiScale(28), dpiScale(28));

    m_asstUrlEdit = new QLineEdit(toolbar);
    m_asstUrlEdit->setPlaceholderText(tr("https://www.indeed.it ..."));

    auto* goBtn = new QPushButton(tr("Vai"), toolbar);
    goBtn->setObjectName("actionBtn");

    /* Toggle apri/nascondi colonne — nella riga del browser, quindi sempre
       visibili anche quando le colonne sono compresse o il contenuto
       scrolla. */
    m_asstCandCollapseBtn = new QPushButton(tr("\xf0\x9f\x93\x8b"), toolbar);
    m_asstCandCollapseBtn->setObjectName("actionBtn");
    m_asstCandCollapseBtn->setCheckable(true);
    m_asstCandCollapseBtn->setChecked(true);
    m_asstCandCollapseBtn->setFixedSize(dpiScale(28), dpiScale(28));
    m_asstCandCollapseBtn->setToolTip(tr("Mostra/nascondi la colonna Candidature"));

    m_asstCtrlCollapseBtn = new QPushButton(tr("\xf0\x9f\x9b\xa0"), toolbar);
    m_asstCtrlCollapseBtn->setObjectName("actionBtn");
    m_asstCtrlCollapseBtn->setCheckable(true);
    m_asstCtrlCollapseBtn->setChecked(true);
    m_asstCtrlCollapseBtn->setFixedSize(dpiScale(28), dpiScale(28));
    m_asstCtrlCollapseBtn->setToolTip(tr("Mostra/nascondi la colonna Assistente"));

    toolL->addWidget(backBtn);
    toolL->addWidget(fwdBtn);
    toolL->addWidget(reloadBtn);
    toolL->addWidget(m_asstUrlEdit, 1);
    toolL->addWidget(goBtn);
    toolL->addWidget(m_asstCandCollapseBtn);
    toolL->addWidget(m_asstCtrlCollapseBtn);
    rootLay->addWidget(toolbar);

    auto* splitter = new QSplitter(Qt::Horizontal, root);

    /* ──── Sinistra: solo il browser, la barra è sopra lo splitter ──── */
    auto* browserPane = new QWidget(splitter);
    auto* browserLay = new QVBoxLayout(browserPane);
    browserLay->setContentsMargins(0, 0, 0, 0);

    m_asstView = new QWebEngineView(browserPane);
    m_asstView->setUrl(QUrl("https://www.indeed.it"));
    browserLay->addWidget(m_asstView, 1);

    m_asstChannel = new QWebChannel(m_asstView);
    m_asstBridge  = new JobRecorderBridge(this);
    m_asstChannel->registerObject("pxBridge", m_asstBridge);
    m_asstView->page()->setWebChannel(m_asstChannel);

    connect(backBtn,   &QPushButton::clicked, this, &LavoroPage::onAsstBackClicked);
    connect(fwdBtn,    &QPushButton::clicked, this, &LavoroPage::onAsstFwdClicked);
    connect(reloadBtn, &QPushButton::clicked, this, &LavoroPage::onAsstReloadClicked);
    connect(goBtn,     &QPushButton::clicked, this, &LavoroPage::onAsstGoClicked);
    connect(m_asstUrlEdit, &QLineEdit::returnPressed, this, &LavoroPage::onAsstGoClicked);
    connect(m_asstView, &QWebEngineView::loadFinished, this, &LavoroPage::onAsstLoadFinished);
    connect(m_asstView, &QWebEngineView::urlChanged, this, [this](const QUrl& u) {
        if (m_asstUrlEdit) m_asstUrlEdit->setText(u.toString());
    });
    connect(m_asstBridge, &JobRecorderBridge::clicked, this, &LavoroPage::onAsstElementClicked);

    splitter->addWidget(browserPane);

    /* ──── Colonna centrale: contenuto storico Candidature ──── */
    auto* candidaturePane = new QWidget(splitter);
    m_asstCandPane = candidaturePane;
    auto* candidaturePaneLay = new QVBoxLayout(candidaturePane);
    candidaturePaneLay->setContentsMargins(8, 0, 0, 0);
    candidaturePaneLay->setSpacing(4);

    m_asstCandScroll = new QScrollArea(candidaturePane);
    m_asstCandScroll->setWidgetResizable(true);
    m_asstCandScroll->setFrameShape(QFrame::NoFrame);
    m_asstCandScroll->setWidget(candidatureContent);
    candidaturePaneLay->addWidget(m_asstCandScroll, 1);

    splitter->addWidget(candidaturePane);
    connect(m_asstCandCollapseBtn, &QPushButton::toggled, this, &LavoroPage::onAsstToggleCandColumn);

    /* ──── Destra: pannello controlli assistente ──── */
    auto* ctrlPane = new QWidget(splitter);
    m_asstCtrlPane = ctrlPane;
    auto* ctrlPaneLay = new QVBoxLayout(ctrlPane);
    ctrlPaneLay->setContentsMargins(8, 0, 0, 0);
    ctrlPaneLay->setSpacing(4);

    m_asstCtrlContent = new QWidget(ctrlPane);
    auto* ctrlLay = new QVBoxLayout(m_asstCtrlContent);
    ctrlLay->setContentsMargins(0, 0, 0, 0);
    ctrlLay->setSpacing(8);
    ctrlPaneLay->addWidget(m_asstCtrlContent, 1);

    auto* profiloBox = new QGroupBox(tr("\xf0\x9f\x91\xa4 Profilo candidato"), m_asstCtrlContent);
    auto* profiloBoxLay = new QVBoxLayout(profiloBox);

    /* Più profili (es. "IT", "Cameriere") — ognuno col proprio CV, per
       candidarsi a settori diversi senza mischiare i dati. */
    auto* profiloSelRow = new QWidget(profiloBox);
    auto* profiloSelL = new QHBoxLayout(profiloSelRow);
    profiloSelL->setContentsMargins(0, 0, 0, 0);
    m_asstProfiloCombo = new QComboBox(profiloSelRow);
    auto* profiloNewBtn = new QPushButton("\xe2\x9e\x95", profiloSelRow);  /* ➕ */
    profiloNewBtn->setObjectName("actionBtn");
    profiloNewBtn->setFixedSize(dpiScale(28), dpiScale(28));
    profiloNewBtn->setToolTip(tr("Nuovo profilo (es. un altro settore/CV)"));
    profiloSelL->addWidget(new QLabel(tr("Profilo:"), profiloSelRow));
    profiloSelL->addWidget(m_asstProfiloCombo, 1);
    profiloSelL->addWidget(profiloNewBtn);
    profiloBoxLay->addWidget(profiloSelRow);
    connect(m_asstProfiloCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LavoroPage::onAsstProfiloComboChanged);
    connect(profiloNewBtn, &QPushButton::clicked, this, &LavoroPage::onAsstProfiloNewClicked);

    auto* profiloForm = new QFormLayout();
    m_asstNomeEdit    = new QLineEdit(profiloBox);
    m_asstCognomeEdit = new QLineEdit(profiloBox);
    m_asstEmailEdit   = new QLineEdit(profiloBox);
    m_asstTelEdit     = new QLineEdit(profiloBox);
    profiloForm->addRow(tr("Nome:"),     m_asstNomeEdit);
    profiloForm->addRow(tr("Cognome:"),  m_asstCognomeEdit);
    profiloForm->addRow(tr("Email:"),    m_asstEmailEdit);
    profiloForm->addRow(tr("Telefono:"), m_asstTelEdit);
    profiloBoxLay->addLayout(profiloForm);

    auto* profiloCvRow = new QWidget(profiloBox);
    auto* profiloCvL = new QHBoxLayout(profiloCvRow);
    profiloCvL->setContentsMargins(0, 0, 0, 0);
    auto* profiloCvBtn = new QPushButton(tr("\xf0\x9f\x93\x8e CV di questo profilo..."), profiloCvRow);
    profiloCvBtn->setObjectName("actionBtn");
    m_asstProfiloCvLbl = new QLabel(tr("(nessuno)"), profiloCvRow);
    m_asstProfiloCvLbl->setObjectName("hintLabel");
    profiloCvL->addWidget(profiloCvBtn);
    profiloCvL->addWidget(m_asstProfiloCvLbl, 1);
    profiloBoxLay->addWidget(profiloCvRow);
    connect(profiloCvBtn, &QPushButton::clicked, this, &LavoroPage::onAsstProfiloCvBtnClicked);

    ctrlLay->addWidget(profiloBox);

    connect(m_asstNomeEdit,    &QLineEdit::textChanged, this, &LavoroPage::onAsstProfiloChanged);
    connect(m_asstCognomeEdit, &QLineEdit::textChanged, this, &LavoroPage::onAsstProfiloChanged);
    connect(m_asstEmailEdit,   &QLineEdit::textChanged, this, &LavoroPage::onAsstProfiloChanged);
    connect(m_asstTelEdit,     &QLineEdit::textChanged, this, &LavoroPage::onAsstProfiloChanged);

    auto* sitoRow = new QWidget(m_asstCtrlContent);
    auto* sitoL = new QHBoxLayout(sitoRow);
    sitoL->setContentsMargins(0, 0, 0, 0);
    m_asstSitoCombo = new QComboBox(sitoRow);
    auto* nuovoSitoBtn = new QPushButton("\xe2\x9e\x95", sitoRow);  /* ➕ */
    nuovoSitoBtn->setObjectName("actionBtn");
    nuovoSitoBtn->setFixedSize(dpiScale(28), dpiScale(28));
    nuovoSitoBtn->setToolTip(tr("Nuova macro per il sito attualmente aperto"));
    sitoL->addWidget(new QLabel(tr("Sito:"), sitoRow));
    sitoL->addWidget(m_asstSitoCombo, 1);
    sitoL->addWidget(nuovoSitoBtn);
    ctrlLay->addWidget(sitoRow);
    connect(m_asstSitoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LavoroPage::onAsstSitoComboChanged);
    connect(nuovoSitoBtn, &QPushButton::clicked, this, &LavoroPage::onAsstNewSiteClicked);

    m_asstRegBtn = new QPushButton(tr("\xf0\x9f\x94\xb4 Registra (ALT+click sul campo)"), m_asstCtrlContent);
    m_asstRegBtn->setObjectName("actionBtn");
    m_asstRegBtn->setCheckable(true);
    ctrlLay->addWidget(m_asstRegBtn);
    connect(m_asstRegBtn, &QPushButton::toggled, this, &LavoroPage::onAsstRegistraToggled);

    m_asstPassiList = new QListWidget(m_asstCtrlContent);
    ctrlLay->addWidget(m_asstPassiList, 1);

    auto* delStepBtn = new QPushButton(tr("\xf0\x9f\x97\x91 Elimina passo selezionato"), m_asstCtrlContent);
    delStepBtn->setObjectName("actionBtn");
    ctrlLay->addWidget(delStepBtn);
    connect(delStepBtn, &QPushButton::clicked, this, &LavoroPage::onAsstStepDeleteClicked);

    m_asstConfermaInvio = new QCheckBox(tr("Richiedi conferma manuale sull'ultimo passo (invio)"), m_asstCtrlContent);
    m_asstConfermaInvio->setChecked(true);
    ctrlLay->addWidget(m_asstConfermaInvio);

    m_asstPlayBtn = new QPushButton(tr("\xe2\x96\xb6 Riproduci tutto"), m_asstCtrlContent);
    m_asstPlayBtn->setObjectName("actionBtn");
    ctrlLay->addWidget(m_asstPlayBtn);
    connect(m_asstPlayBtn, &QPushButton::clicked, this, &LavoroPage::onAsstPlayClicked);

    m_asstLog = new QTextEdit(m_asstCtrlContent);
    m_asstLog->setReadOnly(true);
    m_asstLog->setMaximumHeight(dpiScale(110));
    ctrlLay->addWidget(m_asstLog);

    splitter->addWidget(ctrlPane);
    connect(m_asstCtrlCollapseBtn, &QPushButton::toggled, this, &LavoroPage::onAsstToggleCtrlColumn);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 3);
    /* stretch 1 obbligatorio: QSplitter orizzontale ha politica verticale
       Preferred, senza stretch lo spazio extra andava spartito con la
       toolbar che si gonfiava a ~200px di vuoto sopra il browser. */
    rootLay->addWidget(splitter, 1);

    caricaProfiliCandidato();

    return root;
}

/* ══════════════════════════════════════════════════════════════
   Navigazione mini-browser
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onAsstGoClicked()
{
    if (!m_asstView || !m_asstUrlEdit) return;
    QString url = m_asstUrlEdit->text().trimmed();
    if (url.isEmpty()) return;
    if (!url.contains("://")) url = "https://" + url;
    m_asstView->setUrl(QUrl(url));
}

void LavoroPage::onAsstBackClicked()   { if (m_asstView) m_asstView->back(); }
void LavoroPage::onAsstFwdClicked()    { if (m_asstView) m_asstView->forward(); }
void LavoroPage::onAsstReloadClicked() { if (m_asstView) m_asstView->reload(); }

void LavoroPage::onAsstLoadFinished(bool ok)
{
    if (!ok || !m_asstView) return;
    injectRecorderScript();
    const QString dominio = dominioDaUrl(m_asstView->url().toString());
    if (!dominio.isEmpty() && dominio != m_asstMacro.dominio)
        caricaMacroPerDominio(dominio);
}

/* ══════════════════════════════════════════════════════════════
   Iniezione script di registrazione (QWebChannel + listener ALT+click)
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::injectRecorderScript()
{
    if (!m_asstView) return;

    QFile qwcJs(":/qtwebchannel/qwebchannel.js");
    QString qwcSrc;
    if (qwcJs.open(QIODevice::ReadOnly))
        qwcSrc = QString::fromUtf8(qwcJs.readAll());

    static const QString kRecorderJs = QStringLiteral(
        "(function(){"
        "  if (window.__prismaluxRecorderAttached) return;"
        "  window.__prismaluxRecorderAttached = true;"
        "  window.__prismaluxRecording = false;"
        "  function pxSelector(el){"
        "    if (el.id) return '#' + CSS.escape(el.id);"
        "    if (el.name) return el.tagName.toLowerCase() + '[name=\"' + el.name.replace(/\"/g,'\\\\\"') + '\"]';"
        "    var path = [];"
        "    while (el && el.nodeType === 1 && el !== document.body) {"
        "      var sel = el.tagName.toLowerCase();"
        "      if (el.parentElement) {"
        "        var sibs = Array.prototype.filter.call(el.parentElement.children, function(e){return e.tagName===el.tagName;});"
        "        if (sibs.length > 1) sel += ':nth-of-type(' + (sibs.indexOf(el)+1) + ')';"
        "      }"
        "      path.unshift(sel);"
        "      el = el.parentElement;"
        "    }"
        "    return path.join(' > ');"
        "  }"
        "  document.addEventListener('click', function(ev){"
        "    if (!ev.altKey || !window.__prismaluxRecording) return;"
        "    ev.preventDefault(); ev.stopPropagation();"
        "    var el = ev.target;"
        "    var sel = pxSelector(el);"
        "    var hint = el.getAttribute('placeholder') || el.getAttribute('aria-label') || el.name || el.id || el.tagName;"
        "    if (window.pxBridge) window.pxBridge.elementClicked(sel, el.tagName, String(hint));"
        "  }, true);"
        "})();");

    const QString setupJs = qwcSrc +
        "\nif (typeof QWebChannel !== 'undefined' && !window.__pxChannelReady) {"
        "  window.__pxChannelReady = true;"
        "  new QWebChannel(qt.webChannelTransport, function(channel){ window.pxBridge = channel.objects.pxBridge; });"
        "}\n" + kRecorderJs +
        QString("\nwindow.__prismaluxRecording = %1;").arg(m_asstRegistrando ? "true" : "false");

    m_asstView->page()->runJavaScript(setupJs);
}

/* ══════════════════════════════════════════════════════════════
   Registrazione
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onAsstRegistraToggled(bool on)
{
    m_asstRegistrando = on;
    if (m_asstRegBtn) {
        m_asstRegBtn->setText(on
            ? tr("\xe2\x8f\xb9 Ferma registrazione")
            : tr("\xf0\x9f\x94\xb4 Registra (ALT+click sul campo)"));
    }
    if (m_asstView) {
        m_asstView->page()->runJavaScript(
            QString("window.__prismaluxRecording = %1;").arg(on ? "true" : "false"));
    }
    logAsst(on ? tr("Registrazione avviata: tieni ALT e clicca il campo da registrare nel sito.")
               : tr("Registrazione fermata."));
}

void LavoroPage::onAsstElementClicked(const QString& selettore, const QString& tag, const QString& hint)
{
    if (!m_asstRegistrando) return;

    const QStringList opzioni = {
        tr("Solo click (bottone/checkbox)"),
        tr("Scrivi: Nome"), tr("Scrivi: Cognome"), tr("Scrivi: Email"), tr("Scrivi: Telefono"),
        tr("Scrivi: testo libero...")
    };
    bool ok = false;
    const QString scelta = QInputDialog::getItem(this,
        tr("Campo registrato"),
        tr("Elemento: <%1> %2\nSelettore: %3\n\nCosa vuoi fare con questo campo?")
            .arg(tag, hint, selettore),
        opzioni, 0, false, &ok);
    if (!ok) return;

    MacroStep step;
    step.selettore = selettore;
    step.etichetta = QString("%1 (%2)").arg(hint, tag);

    if (scelta == opzioni[0]) {
        step.azione = MacroAzione::Click;
    } else {
        step.azione = MacroAzione::Scrivi;
        if      (scelta == opzioni[1]) step.campoDato = "nome";
        else if (scelta == opzioni[2]) step.campoDato = "cognome";
        else if (scelta == opzioni[3]) step.campoDato = "email";
        else if (scelta == opzioni[4]) step.campoDato = "telefono";
        else {
            const QString testo = QInputDialog::getText(this, tr("Testo"),
                tr("Testo da scrivere:"), QLineEdit::Normal, QString(), &ok);
            if (!ok) return;
            step.campoDato = "letterale:" + testo;
        }
    }

    m_asstMacro.passi.append(step);
    aggiornaListaPassi();
    salvaMacroCorrente();
    logAsst(tr("Passo aggiunto: %1").arg(step.etichetta));
}

void LavoroPage::onAsstStepDeleteClicked()
{
    if (!m_asstPassiList) return;
    const int row = m_asstPassiList->currentRow();
    if (row < 0 || row >= m_asstMacro.passi.size()) return;
    m_asstMacro.passi.removeAt(row);
    aggiornaListaPassi();
    salvaMacroCorrente();
}

void LavoroPage::aggiornaListaPassi()
{
    if (!m_asstPassiList) return;
    m_asstPassiList->clear();
    int n = 1;
    for (const auto& s : m_asstMacro.passi) {
        const QString azione = s.azione == MacroAzione::Click
            ? tr("Click") : tr("Scrivi[%1]").arg(s.campoDato);
        m_asstPassiList->addItem(QString("%1. %2 \xe2\x80\x94 %3").arg(n++).arg(azione, s.etichetta));
    }
}

/* ══════════════════════════════════════════════════════════════
   Riproduzione
   ══════════════════════════════════════════════════════════════ */
QString LavoroPage::valorePerCampo(const QString& campoDato) const
{
    static const QString kLiterale = "letterale:";
    if (campoDato.startsWith(kLiterale)) return campoDato.mid(kLiterale.size());
    if (campoDato == "nome")     return m_asstProfilo.nome;
    if (campoDato == "cognome")  return m_asstProfilo.cognome;
    if (campoDato == "email")    return m_asstProfilo.email;
    if (campoDato == "telefono") return m_asstProfilo.telefono;
    return QString();
}

void LavoroPage::onAsstPlayClicked()
{
    if (!m_asstView || m_asstMacro.passi.isEmpty()) return;
    logAsst(tr("Riproduzione avviata (%1 passi)...").arg(m_asstMacro.passi.size()));
    eseguiStep(0);
}

void LavoroPage::eseguiStep(int idx)
{
    if (!m_asstView) return;
    if (idx < 0 || idx >= m_asstMacro.passi.size()) {
        logAsst(tr("Riproduzione completata."));
        return;
    }
    const MacroStep step = m_asstMacro.passi[idx];  /* copia: la lista può cambiare durante l'attesa async */

    /* Ultimo passo con conferma manuale attiva: ci si ferma qui — l'invio
       vero e proprio lo fa l'utente con un click reale, mai in automatico. */
    const bool ultimo = (idx == m_asstMacro.passi.size() - 1);
    if (ultimo && m_asstConfermaInvio && m_asstConfermaInvio->isChecked()) {
        logAsst(tr("Ultimo passo (%1): conferma manuale richiesta — completalo tu stesso nel browser.")
                .arg(step.etichetta));
        return;
    }

    QString js;
    if (step.azione == MacroAzione::Click) {
        js = QString(
            "(function(){var e=document.querySelector(%1); if(!e) return 'NOTFOUND'; e.click(); return 'OK';})();")
            .arg(jsQuote(step.selettore));
    } else {
        const QString valore = valorePerCampo(step.campoDato);
        js = QString(
            "(function(){var e=document.querySelector(%1); if(!e) return 'NOTFOUND';"
            "e.value=%2; e.dispatchEvent(new Event('input',{bubbles:true}));"
            "e.dispatchEvent(new Event('change',{bubbles:true})); return 'OK';})();")
            .arg(jsQuote(step.selettore), jsQuote(valore));
    }

    QPointer<LavoroPage> guard(this);
    m_asstView->page()->runJavaScript(js, [guard, idx](const QVariant& result) {
        if (!guard) return;
        guard->logAsst(QString("Passo %1: %2 -> %3")
            .arg(idx + 1).arg(guard->m_asstMacro.passi.value(idx).etichetta, result.toString()));
        QTimer::singleShot(900, guard.data(), [guard, idx] {
            if (guard) guard->eseguiStep(idx + 1);
        });
    });
}

void LavoroPage::logAsst(const QString& msg)
{
    if (m_asstLog) m_asstLog->append(msg);
}

/* ══════════════════════════════════════════════════════════════
   Persistenza macro per dominio (~/candidature_macros/<dominio>.json)
   ══════════════════════════════════════════════════════════════ */
QString LavoroPage::macroPath(const QString& dominio) const
{
    return P::root() + "/candidature_macros/" + dominio + ".json";
}

void LavoroPage::salvaMacroCorrente()
{
    if (m_asstMacro.dominio.isEmpty()) return;
    QDir().mkpath(P::root() + "/candidature_macros");

    QJsonArray arr;
    for (const auto& s : m_asstMacro.passi) {
        QJsonObject o;
        o["azione"]    = macroAzioneToString(s.azione);
        o["selettore"] = s.selettore;
        o["etichetta"] = s.etichetta;
        o["campoDato"] = s.campoDato;
        arr.append(o);
    }
    QJsonObject root;
    root["dominio"] = m_asstMacro.dominio;
    root["nome"]    = m_asstMacro.nome;
    root["passi"]   = arr;

    QFile f(macroPath(m_asstMacro.dominio));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson());
}

void LavoroPage::caricaMacroPerDominio(const QString& dominio)
{
    m_asstMacro = JobMacro{};
    m_asstMacro.dominio = dominio;
    m_asstMacro.nome = dominio;

    QFile f(macroPath(dominio));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        m_asstMacro.nome = root["nome"].toString(dominio);
        for (const auto& v : root["passi"].toArray()) {
            const QJsonObject o = v.toObject();
            MacroStep s;
            s.azione    = macroAzioneFromString(o["azione"].toString());
            s.selettore = o["selettore"].toString();
            s.etichetta = o["etichetta"].toString();
            s.campoDato = o["campoDato"].toString();
            m_asstMacro.passi.append(s);
        }
    }
    aggiornaListaPassi();

    if (m_asstSitoCombo) {
        QSignalBlocker blocker(m_asstSitoCombo);
        int idx = m_asstSitoCombo->findText(dominio);
        if (idx < 0) {
            m_asstSitoCombo->addItem(dominio);
            idx = m_asstSitoCombo->findText(dominio);
        }
        m_asstSitoCombo->setCurrentIndex(idx);
    }
    logAsst(tr("Macro caricata per '%1' (%2 passi).").arg(dominio).arg(m_asstMacro.passi.size()));
}

/* ══════════════════════════════════════════════════════════════
   Combo sito / nuovo sito / profilo
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onAsstSitoComboChanged(int idx)
{
    if (!m_asstSitoCombo || idx < 0) return;
    const QString dominio = m_asstSitoCombo->itemText(idx);
    if (dominio.isEmpty() || dominio == m_asstMacro.dominio) return;
    caricaMacroPerDominio(dominio);
}

void LavoroPage::onAsstNewSiteClicked()
{
    if (!m_asstView) return;
    const QString dominio = dominioDaUrl(m_asstView->url().toString());
    if (dominio.isEmpty()) {
        logAsst(tr("Naviga prima verso il sito da registrare."));
        return;
    }
    caricaMacroPerDominio(dominio);
}

void LavoroPage::onAsstProfiloChanged()
{
    m_asstProfilo.nome     = m_asstNomeEdit    ? m_asstNomeEdit->text()    : QString();
    m_asstProfilo.cognome  = m_asstCognomeEdit ? m_asstCognomeEdit->text() : QString();
    m_asstProfilo.email    = m_asstEmailEdit   ? m_asstEmailEdit->text()  : QString();
    m_asstProfilo.telefono = m_asstTelEdit     ? m_asstTelEdit->text()    : QString();

    if (!m_asstProfiloAttivo.isEmpty()) {
        m_asstProfili[m_asstProfiloAttivo] = m_asstProfilo;
        salvaProfiliCandidato();
    }
}

void LavoroPage::onAsstProfiloComboChanged(int idx)
{
    if (!m_asstProfiloCombo || idx < 0) return;
    applicaProfiloAiCampi(m_asstProfiloCombo->itemText(idx));
}

void LavoroPage::onAsstProfiloNewClicked()
{
    bool ok = false;
    const QString testo = QInputDialog::getText(this, tr("Nuovo profilo"),
        tr("Nome profilo (es. \"IT\", \"Cameriere\"):"), QLineEdit::Normal, QString(), &ok);
    const QString label = testo.trimmed();
    if (!ok || label.isEmpty()) return;

    if (!m_asstProfili.contains(label)) {
        m_asstProfili[label] = CandidatoProfilo{};
        if (m_asstProfiloCombo) m_asstProfiloCombo->addItem(label);
    }
    applicaProfiloAiCampi(label);
}

void LavoroPage::onAsstProfiloCvBtnClicked()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Seleziona CV per questo profilo"),
        QString(), tr("Documenti (*.pdf *.doc *.docx *.odt)"));
    if (path.isEmpty()) return;

    m_asstProfilo.cvPath = path;
    if (m_asstProfiloCvLbl) m_asstProfiloCvLbl->setText(QFileInfo(path).fileName());
    if (!m_asstProfiloAttivo.isEmpty()) {
        m_asstProfili[m_asstProfiloAttivo] = m_asstProfilo;
        salvaProfiliCandidato();
    }
}

/* ══════════════════════════════════════════════════════════════
   Persistenza profili candidato (~/candidature_macros/profili_candidato.json)
   ══════════════════════════════════════════════════════════════ */
QString LavoroPage::profiliCandidatoPath() const
{
    return P::root() + "/candidature_macros/profili_candidato.json";
}

void LavoroPage::salvaProfiliCandidato()
{
    QDir().mkpath(P::root() + "/candidature_macros");

    QJsonObject profiliObj;
    for (auto it = m_asstProfili.constBegin(); it != m_asstProfili.constEnd(); ++it) {
        QJsonObject p;
        p["nome"]     = it.value().nome;
        p["cognome"]  = it.value().cognome;
        p["email"]    = it.value().email;
        p["telefono"] = it.value().telefono;
        p["cvPath"]   = it.value().cvPath;
        p["fotoPath"] = it.value().fotoPath;
        profiliObj[it.key()] = p;
    }
    QJsonObject root;
    root["attivo"]  = m_asstProfiloAttivo;
    root["profili"] = profiliObj;

    QFile f(profiliCandidatoPath());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson());
}

void LavoroPage::caricaProfiliCandidato()
{
    m_asstProfili.clear();

    QFile f(profiliCandidatoPath());
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        const QJsonObject profiliObj = root["profili"].toObject();
        for (auto it = profiliObj.constBegin(); it != profiliObj.constEnd(); ++it) {
            const QJsonObject p = it.value().toObject();
            CandidatoProfilo cp;
            cp.nome     = p["nome"].toString();
            cp.cognome  = p["cognome"].toString();
            cp.email    = p["email"].toString();
            cp.telefono = p["telefono"].toString();
            cp.cvPath   = p["cvPath"].toString();
            cp.fotoPath = p["fotoPath"].toString();
            m_asstProfili[it.key()] = cp;
        }
        m_asstProfiloAttivo = root["attivo"].toString();
    }

    if (m_asstProfili.isEmpty()) {
        /* Primo avvio: profilo "Predefinito" precompilato dal CV/foto già
           caricati nella colonna Candidature. */
        CandidatoProfilo cp;
        cp.cvPath   = m_cvPath ? m_cvPath->text() : QString();
        cp.fotoPath = m_fotoPath;
        const QString predef = tr("Predefinito");
        m_asstProfili[predef] = cp;
        m_asstProfiloAttivo = predef;
    }
    if (!m_asstProfili.contains(m_asstProfiloAttivo))
        m_asstProfiloAttivo = m_asstProfili.firstKey();

    if (m_asstProfiloCombo) {
        QSignalBlocker blocker(m_asstProfiloCombo);
        m_asstProfiloCombo->clear();
        m_asstProfiloCombo->addItems(m_asstProfili.keys());
    }
    applicaProfiloAiCampi(m_asstProfiloAttivo);
}

void LavoroPage::applicaProfiloAiCampi(const QString& nomeProfilo)
{
    if (!m_asstProfili.contains(nomeProfilo)) return;
    m_asstProfiloAttivo = nomeProfilo;
    m_asstProfilo = m_asstProfili.value(nomeProfilo);

    {
        const QSignalBlocker b1(m_asstNomeEdit);
        const QSignalBlocker b2(m_asstCognomeEdit);
        const QSignalBlocker b3(m_asstEmailEdit);
        const QSignalBlocker b4(m_asstTelEdit);
        if (m_asstNomeEdit)    m_asstNomeEdit->setText(m_asstProfilo.nome);
        if (m_asstCognomeEdit) m_asstCognomeEdit->setText(m_asstProfilo.cognome);
        if (m_asstEmailEdit)   m_asstEmailEdit->setText(m_asstProfilo.email);
        if (m_asstTelEdit)     m_asstTelEdit->setText(m_asstProfilo.telefono);
    }
    if (m_asstProfiloCvLbl) {
        m_asstProfiloCvLbl->setText(m_asstProfilo.cvPath.isEmpty()
            ? tr("(nessuno)") : QFileInfo(m_asstProfilo.cvPath).fileName());
    }
    if (m_asstProfiloCombo) {
        const QSignalBlocker blocker(m_asstProfiloCombo);
        const int idx = m_asstProfiloCombo->findText(nomeProfilo);
        if (idx >= 0) m_asstProfiloCombo->setCurrentIndex(idx);
    }
    salvaProfiliCandidato();
}

/* ══════════════════════════════════════════════════════════════
   Colonne comprimibili — i due pulsanti restano nella riga del
   browser (sempre visibili, mai dentro la colonna che nascondono),
   la colonna si nasconde per intero; il QSplitter ridistribuisce
   da solo lo spazio libero al browser.
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onAsstToggleCandColumn(bool on)
{
    if (m_asstCandPane) m_asstCandPane->setVisible(on);
}

void LavoroPage::onAsstToggleCtrlColumn(bool on)
{
    if (m_asstCtrlPane) m_asstCtrlPane->setVisible(on);
}

#endif // HAVE_JOB_ASSISTANT

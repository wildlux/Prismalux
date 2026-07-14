/* ══════════════════════════════════════════════════════════════
   main_sci_compute_ui.cpp — Costruzione UI e refresh tabelle
   ══════════════════════════════════════════════════════════════ */
#include "main_sci_compute.h"
#include "../dpi_utils.h"
#include "../log_bus.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QClipboard>
#include <QGridLayout>
#include <QDir>
#include <QFile>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QTabWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextEdit>
#include <QScrollArea>
#include <QFrame>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDateTime>
#include <QDialog>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QApplication>
#include <QPalette>
#include <QProcess>

/* Colore status → HTML */
static QString statusColor(const QString& s)
{
    if (s == "done" || s == "validated") return "#22c55e";
    if (s == "running")                  return "#3b82f6";
    if (s == "error" || s == "conflict") return "#ef4444";
    if (s == "pending")                  return "#f59e0b";
    return "#6b7280";
}

static QString statusIcon(const QString& s)
{
    if (s == "done" || s == "validated") return "\xe2\x9c\x85";
    if (s == "running")                  return "\xf0\x9f\x94\x84";
    if (s == "error")                    return "\xe2\x9d\x8c";
    if (s == "conflict")                 return "\xe2\x9a\xa0";
    if (s == "pending")                  return "\xe2\x8f\xb3";
    return "?";
}

/* ══════════════════════════════════════════════════════════════
   showGuide — dialogo HTML con guida completa per novellini ed esperti
   Tema adattivo: legge QPalette → funziona con temi chiari e scuri
   ══════════════════════════════════════════════════════════════ */
static void showGuide(QWidget* parent)
{
    /* Colori dal tema Qt corrente */
    const QPalette pal   = QApplication::palette();
    const bool     dark  = pal.color(QPalette::Base).lightness() < 128;

    const QString cBg    = pal.color(QPalette::Base).name();
    const QString cText  = pal.color(QPalette::Text).name();
    const QString cAlt   = pal.color(QPalette::AlternateBase).name();
    const QString cBord  = pal.color(QPalette::Mid).name();
    const QString cLink  = pal.color(QPalette::Link).name();
    const QString cHdr   = dark ? "#93c5fd" : "#1d4ed8";
    const QString cH1    = dark ? "#7dd3fc" : "#1e40af";
    const QString cCode  = dark ? "#86efac" : "#065f46";
    const QString cCodeBg= dark ? "#1e293b" : "#f0fdf4";
    const QString cTagBg = dark ? "#1d4ed8" : "#dbeafe";
    const QString cTagFg = dark ? "#bfdbfe" : "#1e3a8a";
    const QString cThBg  = dark ? "#1e3a5f" : "#dbeafe";
    const QString cThFg  = dark ? "#93c5fd" : "#1e3a8a";
    const QString cOk    = dark ? "#4ade80" : "#16a34a";
    const QString cWarn  = dark ? "#fbbf24" : "#d97706";
    const QString cErr   = dark ? "#f87171" : "#dc2626";
    const QString cMuted = dark ? "#475569" : "#6b7280";

    const QString css = QString(
        "body  { font-family: sans-serif; font-size: 13px; color: %1;"
        "        background: %2; margin: 14px; line-height: 1.55; }"
        "h1    { color: %3; font-size: 17px; margin-bottom: 4px; }"
        "h2    { color: %4; font-size: 14px; margin-top: 18px; margin-bottom: 4px;"
        "        border-bottom: 1px solid %5; padding-bottom: 3px; }"
        "h3    { color: %4; font-size: 13px; margin-top: 12px; margin-bottom: 2px; }"
        "code, pre { background: %6; color: %7; padding: 2px 6px;"
        "            border-radius: 4px; font-size: 12px; }"
        "pre   { display: block; padding: 10px; margin: 6px 0;"
        "        white-space: pre-wrap; word-break: break-all; }"
        ".tag  { display: inline-block; background: %8; color: %9;"
        "        padding: 1px 7px; border-radius: 10px; font-size: 11px; margin-right: 4px; }"
        ".ok   { color: %10; } .warn { color: %11; } .err { color: %12; }"
        "ul    { margin: 4px 0 4px 18px; padding: 0; }"
        "li    { margin-bottom: 2px; }"
        "hr    { border: none; border-top: 1px solid %5; margin: 14px 0; }"
        "table { border-collapse: collapse; width: 100%%; margin: 8px 0; }"
        "th    { background: %13; color: %14; padding: 5px 8px; text-align: left; }"
        "td    { padding: 4px 8px; border-bottom: 1px solid %5; }"
        "a     { color: %15; }"
        ".muted{ color: %16; font-size: 11px; }"
    ).arg(cText,  cBg,    cH1,    cHdr,   cBord,
          cCodeBg,cCode,  cTagBg, cTagFg, cOk,
          cWarn,  cErr,   cThBg,  cThFg,  cLink, cMuted);

    const QString kHtml = "<style>" + css + "</style>\n" + R"HTML(

<h1>&#128300; Calcolo Scientifico Distribuito &mdash; Guida</h1>
<p><span class="tag">novellino</span><span class="tag">esperto</span>
Sistema BOINC-like per distribuire task scientifici su pi&ugrave; PC (VPN Tailscale consigliata).</p>

<h2>&#128209; Concetto in 30 secondi</h2>
<ul>
  <li><b>Coordinator</b> &mdash; il PC che crea i task e li distribuisce (porta 11601)</li>
  <li><b>Worker</b> &mdash; qualsiasi PC che esegue i task (deve avere i tool installati)</li>
  <li>Un PC pu&ograve; essere entrambi &mdash; checkbox <b>&ldquo;Usa questo PC come worker&rdquo;</b></li>
  <li><b>Token</b> &mdash; parola d&rsquo;ordine condivisa, impedisce accessi non autorizzati</li>
  <li><b>VPN Tailscale</b> &mdash; collega PC in reti diverse come se fossero in LAN</li>
</ul>

<hr>

<h2>&#9654; Esempio 1 &mdash; Monte Carlo (solo Python3, immediato)</h2>
<p><span class="tag ok">0 installazioni</span> Funziona su qualsiasi PC con Python3.</p>

<b>Passo 1</b> &mdash; Avvia il Coordinator<br>
<code>radio "&#128300; Calcolo Scientifico" &rarr; Coordinator &rarr; &#9989; Usa questo PC come worker &rarr; "&#129001; Avvia Coordinator"</code>
<p>Lo status diventa: <span class="ok">&#129001; Coordinator attivo &mdash; porta 11601 &mdash; 1 nodi connessi</span></p>

<b>Passo 2</b> &mdash; Crea la Work Unit<br>
<pre>Tipo:   [generale] Python SciPy / NumPy
Label:  Stima Pi greco
Params:
{
  "script": "import random\nn=5000000\npi=4*sum(1 for _ in range(n) if random.random()**2+random.random()**2&lt;1)/n\nprint(f'Pi={pi:.6f}')"
}
Priorita: 1    Repliche: 1    &rarr; "Aggiungi alla coda"</pre>

<b>Passo 3</b> &mdash; Attendi il dispatch (3 secondi automatici)<br>
<pre>&#9203; pending &rarr; &#128260; running &rarr; &#9989; done</pre>

<b>Passo 4</b> &mdash; Leggi il risultato<br>
<code>Click sulla riga WU &rarr; tab "&#128202; Risultati"</code>
<pre>Pi greco stimato: 3.141587
Errore: 0.000006</pre>

<hr>

<h2>&#128300; Esempio 2 &mdash; BLAST (bioinformatica, richiede ncbi-blast+)</h2>
<p><span class="tag warn">richiede BLAST</span> <code>sudo apt install ncbi-blast+</code></p>
<pre>Tipo:   [bioinformatica] BLAST Nucleotide
Label:  Ricerca gene insulina
Params:
{
  "query": "&gt;insulin_human\nATGGCCCTGTGGATGCGCCTCCTGCCCCTGCTGGCGCTGCTGGCCCTG",
  "db": "nt",
  "evalue": "0.001",
  "outfmt": "6",
  "max_hits": 5
}</pre>
<p>Risultato: tabella TSV con accession, identit&agrave;%, lunghezza allineamento, e-value.</p>

<hr>

<h2>&#129520; Esempio 3 &mdash; Predizione struttura proteina (ESMFold, no installazione)</h2>
<p><span class="tag ok">API gratuita</span> Usa il server Meta ESMFold &mdash; max 400 aminoacidi.</p>
<pre>Tab bottom &rarr; "&#129580; Proteine / 3D"

Sequenza FASTA:
&gt;MyProtein
MKTLLLTLVVVTIVCLDLGAV

&rarr; "&#129580; Predici struttura (ESMFold API)"
&rarr; attendi 30-90 secondi
&rarr; struttura 3D appare nel viewer (cartoon colorato)
&rarr; "&#11015; Salva .pdb"</pre>

<b>Ricerca su database:</b>
<ul>
  <li><b>AlphaFold DB</b> &mdash; inserisci UniProt ID (es. <code>P05067</code> = APP umana / Alzheimer)</li>
  <li><b>RCSB PDB</b> &mdash; cerca per nome (es. <code>insulin</code>) o PDB ID (es. <code>1ABC</code>)</li>
  <li><b>UniProt</b> &mdash; cerca per malattia (es. <code>Parkinson</code>, <code>BRCA1</code>)</li>
</ul>

<hr>

<h2>&#9939; Esempio 4 &mdash; Pipeline FASTA &rarr; Struttura &rarr; Docking</h2>
<p><span class="tag warn">richiede AutoDock Vina</span> <code>sudo apt install autodock-vina</code></p>
<pre>Tab bottom &rarr; "&#9939; Pipeline"
&rarr; click "&#129580; Protein Fold &rarr; Docking"</pre>
<p>Crea automaticamente <b>2 Work Unit in sequenza</b>:</p>
<table>
  <tr><th>Step</th><th>Tipo</th><th>Dipende da</th><th>Output</th></tr>
  <tr><td>WU-1</td><td>esmfold_api</td><td>&mdash;</td><td>struttura .pdb</td></tr>
  <tr><td>WU-2</td><td>autodock</td><td>WU-1 done</td><td>affinit&agrave; binding kcal/mol</td></tr>
</table>
<p>WU-2 parte <b>solo quando</b> WU-1 &egrave; <span class="ok">done</span> o <span class="ok">validated</span>.</p>

<hr>

<h2>&#127968; Esempio 5 &mdash; Due PC su rete diversa (Tailscale VPN)</h2>
<pre><b>PC-A (coordinator):</b>
  Avvia Coordinator &rarr; copia Token (click &#128065;)

<b>PC-B (worker) &mdash; su un altro PC:</b>
  Installa Tailscale: curl -fsSL https://tailscale.com/install.sh | sh
  tailscale up
  IP VPN del PC-A: es. 100.64.0.1

  Radio: Worker (client)
  Coordinator IP: 100.64.0.1
  Token: [incolla]
  &rarr; "&#128279; Connetti"</pre>
<p>Appena connesso, PC-B invia le sue capability automaticamente:</p>
<pre class="ok">&#129001; Nodo connesso: PC-B &mdash; CPU:8 RAM:16GB GPU:no Tool:[python3, R, blastn, fastqc]</pre>
<p>Da questo momento i task vengono distribuiti al nodo pi&ugrave; capace.</p>

<hr>

<h2>&#128295; Tool scientifici supportati</h2>
<table>
  <tr><th>Dominio</th><th>Tool</th><th>Installazione</th></tr>
  <tr><td>Bioinformatica</td><td>blastn, blastp, blastx</td><td><code>apt install ncbi-blast+</code></td></tr>
  <tr><td>Bioinformatica</td><td>bwa, samtools, fastqc, bowtie2</td><td><code>apt install bwa samtools fastqc</code></td></tr>
  <tr><td>Chimica</td><td>GROMACS (gmx)</td><td><code>apt install gromacs</code></td></tr>
  <tr><td>Chimica</td><td>AutoDock Vina</td><td><code>apt install autodock-vina</code></td></tr>
  <tr><td>Statistica</td><td>R (Rscript)</td><td><code>apt install r-base</code></td></tr>
  <tr><td>Generale</td><td>Python3 + SciPy</td><td><code>apt install python3-scipy</code></td></tr>
  <tr><td>Proteine</td><td>ESMFold locale</td><td><code>pip install fair-esm torch</code> (~20GB)</td></tr>
</table>

<h2>&#128275; Sicurezza</h2>
<ul>
  <li><b>Token</b> &mdash; condividilo solo con i nodi di fiducia</li>
  <li><b>VPN Tailscale</b> &mdash; tutto il traffico &egrave; cifrato WireGuard, porta 11601 non esposta su internet</li>
  <li><b>Path validation</b> &mdash; i path nei parametri JSON non possono uscire da <code>/home/</code>, <code>/data/</code>, <code>/tmp/</code></li>
  <li><b>Tool validation</b> &mdash; il campo <code>"program"</code> accetta solo nomi binari, mai shell string</li>
</ul>

<h2>&#128202; Flusso visivo</h2>
<pre>Crea WU &rarr; Coda (&#9203; pending)
              &darr; dispatch timer 3s
         Assegnata al nodo capace (&#128260; running)
              &darr; esecuzione tool
         Risultato (&#9989; done / &#10060; error / &#128994; validated / &#9888; conflict)
              &darr;
         Click sulla riga &rarr; tab Risultati &rarr; output + hash SHA-256</pre>

<h2>&#129302; Modelli LLM consigliati (Ollama)</h2>
<table>
  <tr><th>Modello</th><th>Dimensione</th><th>Uso ideale</th><th>Comando</th></tr>
  <tr><td><b>llama3.2:3b</b></td><td>2 GB</td><td>Generale, veloce</td><td><code>ollama pull llama3.2:3b</code></td></tr>
  <tr><td><b>qwen2.5:7b</b></td><td>5 GB</td><td>Scienza, chimica, coding</td><td><code>ollama pull qwen2.5:7b</code></td></tr>
  <tr><td><b>medllama2</b></td><td>4 GB</td><td>Medicina, biologia</td><td><code>ollama pull medllama2</code></td></tr>
  <tr><td><b>biomistral</b></td><td>4 GB</td><td>Bioinformatica, genomica</td><td><code>ollama pull biomistral</code></td></tr>
  <tr><td><b>deepseek-r1:7b</b></td><td>5 GB</td><td>Ragionamento scientifico</td><td><code>ollama pull deepseek-r1:7b</code></td></tr>
</table>
<p class="muted">Scegli il modello nel campo <b>&#129302; LLM</b> nel form &ldquo;Nuova Work Unit&rdquo; prima di aggiungere il task.</p>

<p class="muted" style="margin-top:18px;">
Prismalux v3.0 &mdash; Calcolo Scientifico Distribuito &mdash;
<a href="https://github.com/wildlux/Prismalux">github.com/wildlux/Prismalux</a>
</p>
)HTML";

    auto* dlg  = new QDialog(parent);
    dlg->setWindowTitle(QObject::tr("Guida — Calcolo Scientifico Distribuito"));
    dlg->resize(820, 620);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(0, 0, 0, 8);
    lay->setSpacing(0);

    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(kHtml);
    browser->setStyleSheet("QTextBrowser { border: none; }");
    lay->addWidget(browser, 1);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    box->setContentsMargins(8, 0, 8, 0);
    QObject::connect(box, &QDialogButtonBox::rejected, dlg, &QDialog::accept);
    lay->addWidget(box);

    dlg->show();
}

/* ══════════════════════════════════════════════════════════════
   buildUi
   ══════════════════════════════════════════════════════════════ */
QWidget* SciComputePage::buildUi()
{
    auto* root    = new QWidget;
    auto* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(dpiScale(10), dpiScale(8), dpiScale(10), dpiScale(8));
    rootLay->setSpacing(dpiScale(8));

    /* ── Titolo ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\x94\xac  <b>Calcolo Scientifico Distribuito</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "BOINC-like — bioinformatica \xc2\xb7 fisica \xc2\xb7 chimica \xc2\xb7 medicina"
        "</span>", root);
    titleLbl->setTextFormat(Qt::RichText);
    titleLbl->setObjectName("pageHeader");
    rootLay->addWidget(titleLbl);

    /* ── Barra config: Coordinator | Worker ── */
    auto* cfgBar    = new QWidget(root);
    auto* cfgLay    = new QHBoxLayout(cfgBar);
    cfgLay->setContentsMargins(0,0,0,0); cfgLay->setSpacing(dpiScale(8));

    auto* coordRb = new QRadioButton(tr("\xf0\x9f\x96\xa7  Coordinator (server)"), cfgBar);
    auto* workerRb= new QRadioButton(tr("\xf0\x9f\x96\xa5  Worker (client)"),       cfgBar);
    coordRb->setChecked(true);
    auto* modeBg = new QButtonGroup(cfgBar);
    modeBg->addButton(coordRb, 0);
    modeBg->addButton(workerRb, 1);

    m_localChk = new QCheckBox(tr("Usa questo PC come worker"), cfgBar);
    m_localChk->setChecked(true);
    m_localChk->setToolTip(tr("Il coordinator esegue anche task locali"));

    m_btnStartStop = new QPushButton(
        "\xf0\x9f\x9f\xa2  Avvia Coordinator", cfgBar);
    m_btnStartStop->setObjectName("primaryBtn");

    /* Token inline — visibile solo in modalità Coordinator */
    auto* tokenWidget = new QWidget(cfgBar);
    auto* tokenLay = new QHBoxLayout(tokenWidget);
    tokenLay->setContentsMargins(0,0,0,0); tokenLay->setSpacing(dpiScale(4));
    tokenLay->addWidget(new QLabel(tr("Token:"), tokenWidget));
    m_tokenEdit = new QLineEdit(tokenWidget);
    m_tokenEdit->setEchoMode(QLineEdit::Password);
    m_tokenEdit->setText(m_token);
    m_tokenEdit->setToolTip(tr("Token condiviso con tutti i worker (VPN Tailscale consigliata)"));
    m_tokenEdit->setFixedWidth(dpiScale(180));
    auto* btnShowToken = new QPushButton("\xf0\x9f\x91\x81", tokenWidget);
    btnShowToken->setFixedWidth(dpiScale(28));
    btnShowToken->setObjectName("actionBtn");
    connect(btnShowToken, &QPushButton::clicked, this, [this] {
        m_tokenEdit->setEchoMode(
            m_tokenEdit->echoMode() == QLineEdit::Password
            ? QLineEdit::Normal : QLineEdit::Password);
    });
    auto* btnCopyToken = new QPushButton("\xf0\x9f\x93\x8b", tokenWidget);  /* 📋 */
    btnCopyToken->setFixedWidth(dpiScale(28));
    btnCopyToken->setObjectName("actionBtn");
    btnCopyToken->setToolTip(tr("Copia token negli appunti"));
    connect(btnCopyToken, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_token);
    });
    tokenLay->addWidget(m_tokenEdit);
    tokenLay->addWidget(btnShowToken);
    tokenLay->addWidget(btnCopyToken);

    auto* btnGuida = new QPushButton(
        "\xf0\x9f\x93\x96  Guida", cfgBar);
    btnGuida->setObjectName("actionBtn");
    btnGuida->setToolTip(
        "Esempi pratici, modelli LLM consigliati, sicurezza:\n"
        "Monte Carlo, BLAST, ESMFold, Pipeline, VPN multi-PC");

    cfgLay->addWidget(coordRb);
    cfgLay->addWidget(workerRb);
    cfgLay->addSpacing(dpiScale(12));
    cfgLay->addWidget(m_localChk);
    cfgLay->addStretch(1);
    cfgLay->addWidget(tokenWidget);
    cfgLay->addWidget(btnGuida);
    cfgLay->addWidget(m_btnStartStop);
    rootLay->addWidget(cfgBar);

    /* ── Riga worker (nascosta di default) ── */
    m_modeStack = new QStackedWidget(root);
    m_modeStack->setMaximumHeight(dpiScale(36));

    /* index 0: placeholder coordinator (nascosto, token è nel cfgBar) */
    m_modeStack->addWidget(new QWidget);

    /* index 1: Worker — host + token + connect */
    auto* workerCfg = new QWidget;
    auto* wcLay     = new QHBoxLayout(workerCfg);
    wcLay->setContentsMargins(0,0,0,0); wcLay->setSpacing(dpiScale(6));
    wcLay->addWidget(new QLabel(tr("Coordinator IP:"), workerCfg));
    m_coordHostEdit = new QLineEdit(workerCfg);
    m_coordHostEdit->setPlaceholderText(tr("100.64.0.1  (IP Tailscale)"));
    m_coordHostEdit->setFixedWidth(dpiScale(160));
    wcLay->addWidget(m_coordHostEdit);
    wcLay->addWidget(new QLabel(tr("Token:"), workerCfg));
    auto* wTokenEdit = new QLineEdit(workerCfg);
    wTokenEdit->setEchoMode(QLineEdit::Password);
    wTokenEdit->setFixedWidth(dpiScale(180));
    wTokenEdit->setToolTip(tr("Deve corrispondere al token del coordinator"));
    connect(wTokenEdit, &QLineEdit::textChanged, this,
            [this](const QString& t){ m_token = t; });
    wcLay->addWidget(wTokenEdit);
    m_btnConnect = new QPushButton(tr("\xf0\x9f\x94\x97  Connetti"), workerCfg);
    m_btnConnect->setObjectName("primaryBtn");
    wcLay->addWidget(m_btnConnect);
    wcLay->addStretch(1);
    m_modeStack->addWidget(workerCfg);   /* index 1 */

    rootLay->addWidget(m_modeStack);

    /* Mostra/nascondi token e stack in base al modo */
    connect(modeBg, &QButtonGroup::idClicked, cfgBar, [this, tokenWidget](int id) {
        tokenWidget->setVisible(id == 0);
        if (m_modeStack) m_modeStack->setCurrentIndex(id);
    });

    /* Status bar */
    m_statusLbl = new QLabel(
        "<span style='color:#6b7280;'>\xe2\x9a\xaa Inattivo</span>", root);
    m_statusLbl->setTextFormat(Qt::RichText);
    m_statusLbl->setObjectName("cardDesc");
    rootLay->addWidget(m_statusLbl);

    /* ── Corpo principale: splitter verticale ── */
    auto* splitter = new QSplitter(Qt::Vertical, root);

    /* Top: creazione WU + tabella WU */
    auto* topWidget = new QWidget;
    auto* topLay    = new QHBoxLayout(topWidget);
    topLay->setContentsMargins(0,0,0,0);
    topLay->setSpacing(dpiScale(8));

    /* Pannello sinistra: form creazione WU */
    auto* createGroup = new QGroupBox(
        "\xe2\x9e\x95  Nuova Work Unit", topWidget);
    auto* formLay = new QVBoxLayout(createGroup);
    formLay->setSpacing(dpiScale(5));

    /* Tipo task */
    auto* typeRow = new QHBoxLayout;
    typeRow->addWidget(new QLabel(tr("Tipo:"), createGroup));
    m_typeCombo = new QComboBox(createGroup);
    m_typeCombo->setObjectName("settingCombo");
    for (const auto& t : taskTypes()) {
        m_typeCombo->addItem(
            QString("[%1]  %2").arg(t.domain, t.name), t.id);
    }
    typeRow->addWidget(m_typeCombo, 1);
    formLay->addLayout(typeRow);

    /* Label */
    auto* lblRow = new QHBoxLayout;
    lblRow->addWidget(new QLabel(tr("Label:"), createGroup));
    m_labelEdit = new QLineEdit(createGroup);
    m_labelEdit->setPlaceholderText(tr("Descrizione breve (opzionale)"));
    lblRow->addWidget(m_labelEdit, 1);
    formLay->addLayout(lblRow);

    /* LLM del server */
    auto* llmRow = new QHBoxLayout;
    llmRow->addWidget(new QLabel(tr("\xf0\x9f\xa4\x96  LLM:"), createGroup));
    m_wuLlmCombo = new QComboBox(createGroup);
    m_wuLlmCombo->setObjectName("settingCombo");
    m_wuLlmCombo->setEditable(true);
    m_wuLlmCombo->addItem(m_sciLlmModel);
    m_wuLlmCombo->setToolTip(
        "Modello Ollama usato per le analisi LLM in questa WU.\n"
        "Consigliati:\n"
        "  llama3.2:3b    — veloce, generale (2GB)\n"
        "  qwen2.5:7b     — scienza/chimica (5GB)\n"
        "  medllama2      — medicina (4GB)\n"
        "  biomistral     — bioinformatica (4GB)\n"
        "  deepseek-r1:7b — ragionamento scientifico (5GB)");
    llmRow->addWidget(m_wuLlmCombo, 1);
    auto* btnRefLlm = new QPushButton("\xf0\x9f\x94\x84", createGroup);
    btnRefLlm->setObjectName("actionBtn");
    btnRefLlm->setFixedWidth(dpiScale(28));
    btnRefLlm->setToolTip(tr("Aggiorna lista modelli Ollama disponibili"));
    connect(btnRefLlm, &QPushButton::clicked, this, &SciComputePage::onFetchLlmModels);
    llmRow->addWidget(btnRefLlm);
    formLay->addLayout(llmRow);

    /* Params JSON */
    formLay->addWidget(new QLabel(tr("Parametri JSON:"), createGroup));
    m_paramsEdit = new QTextEdit(createGroup);
    m_paramsEdit->setMinimumHeight(dpiScale(90));
    m_paramsEdit->setMaximumHeight(dpiScale(200));
    m_paramsEdit->setPlaceholderText(tr("{}"));
    m_paramsEdit->setObjectName("codeEdit");
    formLay->addWidget(m_paramsEdit);

    /* Priorità + Repliche */
    auto* prioRow = new QHBoxLayout;
    prioRow->addWidget(new QLabel(tr("Priorit\xc3\xa0:"), createGroup));
    m_priorityCmb = new QComboBox(createGroup);
    m_priorityCmb->setObjectName("settingCombo");
    m_priorityCmb->addItem("1 — Normale", 1);
    m_priorityCmb->addItem("2 — Alta",    2);
    m_priorityCmb->addItem("3 — Urgente", 3);
    prioRow->addWidget(m_priorityCmb);
    prioRow->addSpacing(dpiScale(8));
    prioRow->addWidget(new QLabel(tr("Repliche:"), createGroup));
    m_replicasCmb = new QComboBox(createGroup);
    m_replicasCmb->setObjectName("settingCombo");
    m_replicasCmb->addItem("1 (nessuna validazione)", 1);
    m_replicasCmb->addItem("2 (quorum validation)",   2);
    m_replicasCmb->addItem("3 (quorum 3 nodi)",       3);
    m_replicasCmb->setToolTip(
        "Repliche > 1: stesso WU su N nodi, confronto hash SHA-256");
    prioRow->addWidget(m_replicasCmb);
    prioRow->addStretch(1);
    formLay->addLayout(prioRow);

    auto* addBtnRow = new QHBoxLayout;
    auto* addBtn = new QPushButton(
        "\xe2\x9e\x95  Aggiungi alla coda", createGroup);
    addBtn->setObjectName("primaryBtn");
    addBtnRow->addWidget(addBtn, 1);

    auto* btnGenFile = new QPushButton(
        "\xf0\x9f\x93\x82  Genera da file", createGroup);
    btnGenFile->setObjectName("actionBtn");
    btnGenFile->setToolTip(
        "Carica FASTA/CSV/TXT e genera N Work Unit automaticamente\n"
        "max 500 WU per batch — usa {{INPUT}} nel template JSON");
    addBtnRow->addWidget(btnGenFile);
    formLay->addLayout(addBtnRow);

    /* Scroll area per il form — evita overflow/sovrapposizione quando
       la finestra è bassa. Il form ha ~360px di altezza naturale. */
    auto* formScroll = new QScrollArea(topWidget);
    formScroll->setMinimumWidth(dpiScale(200));
    formScroll->setWidgetResizable(true);
    formScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    formScroll->setFrameShape(QFrame::NoFrame);
    formScroll->setWidget(createGroup);
    topLay->addWidget(formScroll, 1);

    /* Pannello destra: tabella WU */
    auto* wuGroup = new QGroupBox(
        "\xf0\x9f\x93\x8b  Coda Work Units", topWidget);
    auto* wuGLay  = new QVBoxLayout(wuGroup);
    wuGLay->setSpacing(dpiScale(4));

    m_wuTable = new QTableWidget(0, 7, wuGroup);
    /* 4 righe × 22px + header 26px + margini = ~120px minimo visibile */
    m_wuTable->setMinimumHeight(dpiScale(120));
    m_wuTable->setHorizontalHeaderLabels(
        {"ID", "Tipo", "Label", "Status", "Nodo", "Prior.", "Creato"});
    m_wuTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_wuTable->setColumnWidth(0, dpiScale(70));
    m_wuTable->setColumnWidth(1, dpiScale(90));
    m_wuTable->setColumnWidth(3, dpiScale(80));
    m_wuTable->setColumnWidth(4, dpiScale(80));
    m_wuTable->setColumnWidth(5, dpiScale(45));
    m_wuTable->setColumnWidth(6, dpiScale(55));
    m_wuTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wuTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_wuTable->verticalHeader()->hide();
    m_wuTable->setAlternatingRowColors(true);
    m_wuTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_wuTable, &QTableWidget::customContextMenuRequested,
            this, &SciComputePage::onWuContextMenu);
    wuGLay->addWidget(m_wuTable, 1);

    auto* wuBtnRow = new QHBoxLayout;
    auto* btnRefWu = new QPushButton("\xf0\x9f\x94\x84", wuGroup);
    btnRefWu->setObjectName("actionBtn");
    btnRefWu->setToolTip(tr("Aggiorna lista WU"));
    auto* btnDelWu = new QPushButton(tr("\xf0\x9f\x97\x91  Elimina"), wuGroup);
    btnDelWu->setObjectName("actionBtn");
    btnDelWu->setToolTip(tr("Elimina WU selezionata (solo se non in esecuzione)"));
    m_btnAggregate = new QPushButton(tr("\xf0\x9f\x93\x8a  Aggrega risultati"), wuGroup);
    m_btnAggregate->setObjectName("actionBtn");
    m_btnAggregate->setToolTip(
        "Aggrega tutti i risultati della pipeline (o della WU selezionata)\n"
        "Esporta in CSV / JSON / Testo concatenato");
    wuBtnRow->addWidget(btnRefWu);
    wuBtnRow->addWidget(btnDelWu);
    wuBtnRow->addWidget(m_btnAggregate);
    wuBtnRow->addStretch(1);
    wuGLay->addLayout(wuBtnRow);

    topLay->addWidget(wuGroup, 1);
    splitter->addWidget(topWidget);

    /* Bottom: nodi + risultati */
    auto* bottomTabs = new QTabWidget;
    bottomTabs->setDocumentMode(true);

    /* Tab nodi */
    auto* nodeWidget = new QWidget;
    auto* nodeLay    = new QVBoxLayout(nodeWidget);
    nodeLay->setContentsMargins(4,4,4,4);
    m_nodeTable = new QTableWidget(0, 10, nodeWidget);
    m_nodeTable->setHorizontalHeaderLabels(
        {"ID", "Nome", "Indirizzo", "CPU", "RAM (GB)", "GPU",
         "Tool disponibili",
         "WU\xe2\x9c\x85", "WU\xe2\x9d\x8c", "CPU ore"});
    m_nodeTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    m_nodeTable->setColumnWidth(0, dpiScale(70));
    m_nodeTable->setColumnWidth(1, dpiScale(90));
    m_nodeTable->setColumnWidth(2, dpiScale(110));
    m_nodeTable->setColumnWidth(3, dpiScale(40));
    m_nodeTable->setColumnWidth(4, dpiScale(60));
    m_nodeTable->setColumnWidth(5, dpiScale(110));
    m_nodeTable->setColumnWidth(7, dpiScale(55));
    m_nodeTable->setColumnWidth(8, dpiScale(55));
    m_nodeTable->setColumnWidth(9, dpiScale(65));
    m_nodeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_nodeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_nodeTable->verticalHeader()->hide();
    m_nodeTable->setAlternatingRowColors(true);
    m_nodeTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_nodeTable, &QTableWidget::customContextMenuRequested,
            this, &SciComputePage::onNodeContextMenu);
    /* Guida aggiunta nodo worker */
    {
        auto* workerGuide = new QLabel(nodeWidget);
        workerGuide->setObjectName("hintLabel");
        workerGuide->setTextFormat(Qt::RichText);
        workerGuide->setWordWrap(true);
        workerGuide->setText(
            "<b>\xf0\x9f\x96\xa5  Come aggiungere un nodo worker Sci Compute:</b>"
            "<ol style='margin:4px 0 4px 16px; padding:0;'>"
            "<li>Installa Python 3.10+ e le dipendenze dei tool scientifici "
            "necessari (es. <tt>blast+</tt>, <tt>gromacs</tt>, <tt>scipy</tt>).</li>"
            "<li>Copia <b>MCPs/sci_worker/sci_worker.py</b> sul nodo remoto.</li>"
            "<li>Avvia: <tt>python3 sci_worker.py --host IP_SERVER --port 11601 "
            "--token TOKEN --capabilities blast,gmx,python</tt></li>"
            "<li>Il nodo appare qui sotto dopo la prima connessione. "
            "Il server assegna automaticamente le WU compatibili con le sue capability.</li>"
            "</ol>");
        nodeLay->insertWidget(0, workerGuide);
    }
    nodeLay->addWidget(m_nodeTable);
    bottomTabs->addTab(nodeWidget, tr("\xf0\x9f\x96\xa5  Nodi"));

    /* Tab risultati */
    auto* resWidget = new QWidget;
    auto* resLay    = new QVBoxLayout(resWidget);
    resLay->setContentsMargins(4,4,4,4);
    m_resultView = new QTextEdit(resWidget);
    m_resultView->setReadOnly(true);
    m_resultView->setObjectName("codeEdit");
    m_resultView->setPlaceholderText(tr("Seleziona una WU nella tabella per vedere il risultato..."));
    resLay->addWidget(m_resultView);
    bottomTabs->addTab(resWidget, tr("\xf0\x9f\x93\x8a  Risultati"));

    /* Tab log */
    auto* logWidget = new QWidget;
    auto* logLay    = new QVBoxLayout(logWidget);
    logLay->setContentsMargins(4,4,4,4);
    m_logView = new QTextEdit(logWidget);
    m_logView->setReadOnly(true);
    m_logView->setObjectName("codeEdit");
    logLay->addWidget(m_logView);
    bottomTabs->addTab(logWidget, tr("\xf0\x9f\x93\x9c  Log"));

    /* ── Tab Strutture proteiche ── */
    m_proteinWidget = new SciProteinWidget(bottomTabs);
    bottomTabs->addTab(m_proteinWidget,
        "\xf0\x9f\xa7\xac  Proteine / 3D");
    /* Quando l'utente predice una struttura, offre di creare WU docking */
    connect(m_proteinWidget, &SciProteinWidget::pdbReady,
            this, [this](const QString& pdbData, const QString& label) {
        /* Registra il PDB come file broker e suggerisci AutoDock */
        const QString tmpPath = QDir::tempPath() + "/" + label + ".pdb";
        QFile f(tmpPath);
        if (f.open(QIODevice::WriteOnly)) f.write(pdbData.toUtf8());
        registerFile(label + ".pdb", tmpPath);
        appendLog(QString("\xf0\x9f\xa7\xaa  Struttura '%1' pronta. "
                          "Usa il tab Pipeline per avviare il docking.").arg(label));
    });

    /* ── Tab Pipeline ── */
    auto* pipWidget = new QWidget(bottomTabs);
    auto* pipLay    = new QVBoxLayout(pipWidget);
    pipLay->setContentsMargins(dpiScale(8), dpiScale(8), dpiScale(8), dpiScale(8));
    pipLay->setSpacing(dpiScale(8));

    pipLay->addWidget(new QLabel(
        "<b>\xe2\x9b\x93  Pipeline scientifiche predefinite</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "Sequenza di task con dipendenze automatiche</span>", pipWidget));

    auto* pipGrid = new QGridLayout;
    pipGrid->setSpacing(dpiScale(6));

    auto makePipBtn = [&](const QString& icon, const QString& name,
                           const QString& desc, const QString& tmplId) {
        auto* btn = new QPushButton(icon + "  " + name, pipWidget);
        btn->setObjectName("actionBtn");
        btn->setToolTip(desc);
        btn->setMinimumHeight(dpiScale(42));
        connect(btn, &QPushButton::clicked, this,
                [this, tmplId] { createPipeline(tmplId); });
        return btn;
    };

    pipGrid->addWidget(makePipBtn(
        "\xf0\x9f\xa7\xac", "Protein Fold → Docking",
        "ESMFold API → AutoDock Vina\n"
        "Input: sequenza amminoacidica + file ligando\n"
        "Output: struttura PDB + affinità binding (kcal/mol)",
        "protein_fold_dock"), 0, 0);

    pipGrid->addWidget(makePipBtn(
        "\xf0\x9f\xa7\xac", "NGS Variant Calling",
        "FastQC → BWA align → SAMtools → R variant analysis\n"
        "Input: FASTQ reads + genoma di riferimento\n"
        "Output: report QC + BAM allineato + varianti",
        "ngs_variant"), 0, 1);

    pipGrid->addWidget(makePipBtn(
        "\xf0\x9f\x94\xac", "MD Simulation",
        "GROMACS Minimizzazione → MD Production → Analisi Python\n"
        "Input: file .tpr GROMACS\n"
        "Output: traiettoria + RMSD + analisi energetica",
        "md_sim"), 1, 0);

    pipGrid->addWidget(makePipBtn(
        "\xf0\x9f\xa7\xaa", "Docking da PDB",
        "Scarica struttura RCSB → AutoDock Vina → R binding analysis\n"
        "Apri prima il tab Proteine/3D, poi avvia questa pipeline",
        "pdb_dock"), 1, 1);

    pipGrid->addWidget(makePipBtn(
        "\xf0\x9f\xa4\x96", "BLAST + LLM Interpretazione",
        "BLAST Nucleotide → LLM analisi risultati → ipotesi esperimenti\n"
        "Il modello LLM interpreta automaticamente i risultati BLAST\n"
        "Modello usato: quello configurato nel campo sopra",
        "blast_llm"), 2, 0);

    pipGrid->addWidget(makePipBtn(
        "\xf0\x9f\xa4\x96", "Fold + LLM Analisi Struttura",
        "ESMFold API → LLM analisi siti attivi e regioni funzionali\n"
        "Il modello LLM interpreta la struttura e suggerisce esperimenti di mutagenesi",
        "fold_llm"), 2, 1);

    pipLay->addLayout(pipGrid);

    /* Stato pipeline attive */
    auto* pipStatusLbl = new QLabel(
        "<small>Le pipeline rispettano l'ordine: ogni step parte solo quando il precedente "
        "ha status <b>done</b> o <b>validated</b>. Monitorale nel tab Work Units.</small>",
        pipWidget);
    pipStatusLbl->setWordWrap(true);
    pipStatusLbl->setObjectName("hintLabel");
    pipStatusLbl->setTextFormat(Qt::RichText);
    pipLay->addWidget(pipStatusLbl);
    pipLay->addStretch(1);

    bottomTabs->addTab(pipWidget, tr("\xe2\x9b\x93  Pipeline"));

    /* ── Tab Ricerca SDS (Shwachman-Diamond, pipeline di studio distribuita) ── */
    auto* sdsWidget = new QWidget(bottomTabs);
    auto* sdsLay    = new QVBoxLayout(sdsWidget);
    sdsLay->setContentsMargins(dpiScale(8), dpiScale(8), dpiScale(8), dpiScale(8));
    sdsLay->setSpacing(dpiScale(8));

    sdsLay->addWidget(new QLabel(
        "<b>\xf0\x9f\xa7\xac  Ricerca Shwachman-Diamond</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "pipeline distribuita di studio</span>", sdsWidget));

    auto* sdsWarn = new QLabel(
        "\xe2\x9a\xa0\xef\xb8\x8f  <b>Strumento di studio, non clinico.</b> Gli score sono "
        "euristici/segnaposto: il Calcolo Scientifico distribuisce il calcolo, non lo "
        "rende clinicamente valido. Servono un centro di ricerca (Verona / AISS) e "
        "strumenti validati (SpliceAI, PEGG, GuideScan2).", sdsWidget);
    sdsWarn->setTextFormat(Qt::RichText);
    sdsWarn->setWordWrap(true);
    sdsWarn->setStyleSheet(
        "QLabel{background:#3a2a00;color:#fbbf24;border:1px solid #a16207;"
        "border-radius:6px;padding:8px;}");
    sdsLay->addWidget(sdsWarn);

    auto* sdsPipeBtn = new QPushButton(
        "\xe2\x9b\x93  Pipeline completa (brute force \xe2\x86\x92 ibrido \xe2\x86\x92 LLM)", sdsWidget);
    sdsPipeBtn->setObjectName("actionBtn");
    sdsPipeBtn->setMinimumHeight(dpiScale(42));
    connect(sdsPipeBtn, &QPushButton::clicked, this,
            [this] { createPipeline("sds_research"); });
    sdsLay->addWidget(sdsPipeBtn);

    auto* sdsGrid = new QGridLayout;
    sdsGrid->setSpacing(dpiScale(6));

    auto makeSdsBtn = [this, sdsWidget](const QString& icon, const QString& name,
                                        const QString& desc, const QString& script) {
        auto* btn = new QPushButton(icon + "  " + name, sdsWidget);
        btn->setObjectName("actionBtn");
        btn->setToolTip(desc);
        btn->setMinimumHeight(dpiScale(42));
        connect(btn, &QPushButton::clicked, this,
                [this, name, script] { enqueueSdsScript(name, script); });
        return btn;
    };

    sdsGrid->addWidget(makeSdsBtn("\xf0\x9f\x94\xa2", "Brute force guide",
        "Genera e valuta le guide candidate (euristica).\n"
        "script: bruteforce_sds_stable.py", "bruteforce_sds_stable.py"), 0, 0);
    sdsGrid->addWidget(makeSdsBtn("\xf0\x9f\x94\x81", "Ciclo ibrido DNA/RNA",
        "Ottimizzazione DNA<->RNA (splicing segnaposto).\n"
        "script: hybrid_optimizer.py", "hybrid_optimizer.py"), 0, 1);
    sdsGrid->addWidget(makeSdsBtn("\xf0\x9f\xa7\xa0", "Analisi pattern (LLM)",
        "Commento LLM locale sui risultati.\n"
        "script: llm_meta_analyst.py", "llm_meta_analyst.py"), 1, 0);
    sdsGrid->addWidget(makeSdsBtn("\xe2\x96\xb6", "Tutto in sequenza (run_all)",
        "Esegue tutti i moduli in sequenza su un nodo.\n"
        "script: run_all.py", "run_all.py"), 1, 1);

    sdsLay->addLayout(sdsGrid);

    auto* sdsHint = new QLabel(
        "<small>Ogni pulsante accoda una Work Unit di tipo <b>sds_editing</b>. Monitorale "
        "nel tab <b>Work Units</b> e leggi l'output nel tab <b>Risultati</b>. Su pi\xc3\xb9 nodi "
        "serve che <tt>Tools/sds_editing/</tt> sia presente su ciascuno.</small>", sdsWidget);
    sdsHint->setWordWrap(true);
    sdsHint->setObjectName("hintLabel");
    sdsHint->setTextFormat(Qt::RichText);
    sdsLay->addWidget(sdsHint);
    sdsLay->addStretch(1);

    bottomTabs->addTab(sdsWidget, tr("\xf0\x9f\xa7\xac  Ricerca SDS"));

    splitter->addWidget(bottomTabs);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({dpiScale(220), dpiScale(200)});

    rootLay->addWidget(splitter, 1);

    /* ── Connessioni ── */
    connect(btnGuida, &QPushButton::clicked, this,
            [this] { showGuide(this); });

    connect(modeBg, &QButtonGroup::idClicked, this, [this](int id) {
        m_isCoord = (id == 0);
        if (m_modeStack) m_modeStack->setCurrentIndex(id);
        if (m_localChk)  m_localChk->setVisible(id == 0);
        if (m_btnStartStop) m_btnStartStop->setVisible(id == 0);
        if (m_btnConnect)   m_btnConnect->setVisible(id == 1);
    });
    connect(m_localChk, &QCheckBox::toggled, this,
            [this](bool v){ m_useLocal = v; });
    connect(m_tokenEdit, &QLineEdit::textChanged, this,
            [this](const QString& t){ m_token = t; });
    connect(m_btnStartStop, &QPushButton::clicked,
            this, &SciComputePage::onStartStopClicked);
    connect(m_btnConnect, &QPushButton::clicked,
            this, &SciComputePage::onConnectClicked);
    connect(addBtn, &QPushButton::clicked,
            this, &SciComputePage::onAddWuClicked);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SciComputePage::onTypeComboChanged);
    connect(m_wuTable, &QTableWidget::cellClicked,
            this, &SciComputePage::onWuTableRowClicked);
    connect(btnRefWu, &QPushButton::clicked,
            this, [this]{ refreshWuTable(); refreshNodeTable(); });
    connect(btnDelWu, &QPushButton::clicked,
            this, &SciComputePage::onDeleteWuClicked);
    connect(btnGenFile, &QPushButton::clicked,
            this, &SciComputePage::onGenerateFromFileClicked);
    connect(m_btnAggregate, &QPushButton::clicked,
            this, &SciComputePage::onAggregateResultsClicked);

    /* Pre-carica template primo tipo */
    onTypeComboChanged(0);

    /* Carica dati iniziali */
    refreshWuTable();
    refreshNodeTable();

    return root;
}

/* ══════════════════════════════════════════════════════════════
   refresh — Tabelle e viste
   ══════════════════════════════════════════════════════════════ */

void SciComputePage::refreshWuTable()
{
    if (!m_wuTable) return;
    const auto rows = queryWus();
    m_wuTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        const QString id     = r["id"].toString();
        const QString status = r["status"].toString();
        const qint64  ts     = r["created"].toLongLong();
        const QString time   = QDateTime::fromSecsSinceEpoch(ts).toString("HH:mm");

        auto setCell = [&](int col, const QString& text, const QString& color = {}) {
            auto* item = new QTableWidgetItem(text);
            if (!color.isEmpty())
                item->setForeground(QColor(color));
            item->setData(Qt::UserRole, id);
            m_wuTable->setItem(i, col, item);
        };

        setCell(0, id.left(8));
        setCell(1, r["type"].toString());
        setCell(2, r["label"].toString().isEmpty() ? r["type"].toString() : r["label"].toString());
        /* Per WU running mostra percentuale se disponibile */
        if (status == "running" && m_wuProgress.contains(id)) {
            const int p = m_wuProgress[id].first;
            setCell(3, p >= 0
                ? QString("\xf0\x9f\x94\x84 %1%").arg(p)
                : "\xf0\x9f\x94\x84 running...", "#3b82f6");
        } else {
            setCell(3, statusIcon(status) + " " + status, statusColor(status));
        }
        setCell(4, r["node"].toString().left(8));
        setCell(5, r["priority"].toString());
        setCell(6, time);

        m_wuTable->setRowHeight(i, dpiScale(22));
    }
}

void SciComputePage::refreshNodeTable()
{
    if (!m_nodeTable) return;
    const auto rows = queryNodes();
    m_nodeTable->setRowCount(rows.size());

    /* Aggiorna il titolo del tab con il conteggio nodi attivi */
    if (auto* tabs = qobject_cast<QTabWidget*>(m_nodeTable->parent()
                        ? m_nodeTable->parent()->parent() : nullptr)) {
        const int idleCount = std::count_if(rows.begin(), rows.end(),
            [](const QVariantMap& m){ return m["status"].toString() != "offline"; });
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i).contains("Nodi"))
                tabs->setTabText(i, QString("\xf0\x9f\x96\xa5  Nodi (%1)").arg(idleCount));
        }
    }
    for (int i = 0; i < rows.size(); ++i) {
        const auto& r      = rows[i];
        const QString id   = r["id"].toString();
        const QString st   = r["status"].toString();
        const QString stColor = (st=="idle") ? "#22c55e"
                             : (st=="busy") ? "#3b82f6"
                             : "#ef4444";

        auto setCell = [&](int col, const QString& text, const QString& color = {}) {
            auto* item = new QTableWidgetItem(text);
            if (!color.isEmpty())
                item->setForeground(QColor(color));
            m_nodeTable->setItem(i, col, item);
        };

        /* Evidenzia nodo locale — salva ID completo in UserRole per context menu */
        const QString dispId = (id == m_myNodeId) ? id.left(8) + " (io)" : id.left(8);
        setCell(0, dispId, id == m_myNodeId ? "#a78bfa" : QString());
        if (m_nodeTable->item(i, 0))
            m_nodeTable->item(i, 0)->setData(Qt::UserRole, id);
        setCell(1, r["name"].toString() + "  [" + st + "]", stColor);
        setCell(2, r["addr"].toString() + ":" + r["port"].toString());
        setCell(3, r["cpu"].toString());
        setCell(4, r["ram"].toString());
        setCell(5, r["gpu"].toString().isEmpty() ? "—" : r["gpu"].toString());

        /* Tool: lista compatta */
        const QJsonArray toolsArr =
            QJsonDocument::fromJson(r["tools"].toString().toUtf8()).array();
        QStringList tList;
        for (const auto& v : toolsArr) tList << v.toString();
        setCell(6, tList.join("  "));

        /* Credit counter */
        const int  done  = r["wu_done"].toInt();
        const int  err   = r["wu_error"].toInt();
        const int  total = done + err;
        /* Colore badge: verde ≥90%, giallo ≥70%, rosso <70% */
        const QString badgeColor = (total == 0) ? "#6b7280"
                                 : ((done * 100 / total) >= 90) ? "#22c55e"
                                 : ((done * 100 / total) >= 70) ? "#f59e0b"
                                 : "#ef4444";
        setCell(7, QString::number(done),  badgeColor);
        setCell(8, QString::number(err),   err > 0 ? "#ef4444" : "#6b7280");

        const qint64 cpuSec  = r["cpu_seconds"].toLongLong();
        const QString cpuStr = cpuSec >= 3600
                               ? QString("%1h").arg(cpuSec / 3600)
                               : cpuSec >= 60
                               ? QString("%1m").arg(cpuSec / 60)
                               : (cpuSec > 0 ? QString("%1s").arg(cpuSec) : "\xe2\x80\x94");
        setCell(9, cpuStr);

        m_nodeTable->setRowHeight(i, dpiScale(22));
    }
}

void SciComputePage::refreshResults(const QString& wuId)
{
    if (!m_resultView) return;
    if (wuId.isEmpty()) {
        m_resultView->setPlainText("Seleziona una WU nella tabella per vedere il risultato.");
        return;
    }
    const auto results = queryResults(wuId);
    if (results.isEmpty()) {
        m_resultView->setPlainText("Nessun risultato ancora per WU: " + wuId.left(8));
        return;
    }
    QString text;
    for (const auto& r : results) {
        const QString nName = r["node_name"].toString().isEmpty()
                            ? r["node_id"].toString().left(8)
                            : r["node_name"].toString();
        const qint64 ts = r["ts"].toLongLong();
        text += QString("═══ Nodo: %1  |  %2  |  Hash: %3 ═══\n")
                .arg(nName)
                .arg(QDateTime::fromSecsSinceEpoch(ts).toString("dd/MM HH:mm:ss"))
                .arg(r["hash"].toString().left(16) + "...");
        text += r["output"].toString();
        text += "\n\n";
    }
    m_resultView->setPlainText(text.trimmed());
}

void SciComputePage::appendLog(const QString& msg)
{
    if (!m_logView) return;
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_logView->append("[" + ts + "]  " + msg);
}

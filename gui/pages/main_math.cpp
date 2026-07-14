#include "main_math.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../ai_client.h"

#include "main_graph.h"
#include "../widgets/formula_parser.h"
#include <QTextBrowser>
#include <QBrush>
#include <QColor>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QProcess>
#include <QTextCursor>
#include <QScrollBar>
#include <QClipboard>
#include <QApplication>
#include <QTimer>
#include <QEventLoop>
#include <QToolButton>
#include <QMenu>
#include <QScrollArea>
#include <QStackedWidget>
#include <QApplication>
#include <QRandomGenerator>
#include <QRegularExpression>
#include "../dpi_utils.h"
#include <cmath>
#include <limits>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Costruttore — assembla il layout principale
   ══════════════════════════════════════════════════════════════ */
MatematicaPage::MatematicaPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    /* ─── Barra modello LLM (condivisa tra tutte le tab) ─── */
    auto* modelBar = new QWidget(this);
    modelBar->setObjectName("modelBarMath");
    auto* modelBarLay = new QHBoxLayout(modelBar);
    modelBarLay->setContentsMargins(12, 6, 12, 6);
    modelBarLay->setSpacing(8);

    auto* modelLbl = new QLabel(tr("\xf0\x9f\xa4\x96  Modello AI:"), modelBar);  /* 🤖 */
    modelLbl->setObjectName("cardDesc");
    modelBarLay->addWidget(modelLbl);

    m_modelCombo = new QComboBox(modelBar);
    m_modelCombo->setObjectName("settingCombo");
    m_modelCombo->setMinimumWidth(200);
    m_modelCombo->setToolTip(tr("Modello LLM usato da tutte le schede Matematica"));
    const QString curModel = m_ai ? m_ai->model() : QString();
    m_modelCombo->addItem(curModel.isEmpty() ? "(caricamento...)" : curModel, curModel);
    modelBarLay->addWidget(m_modelCombo, 1);

    auto* btnRefreshBar = new QPushButton("\xf0\x9f\x94\x84", modelBar);  /* 🔄 */
    btnRefreshBar->setObjectName("navBtn");
    btnRefreshBar->setFixedSize(26, 26);
    btnRefreshBar->setToolTip(tr("Aggiorna lista modelli"));
    connect(btnRefreshBar, &QPushButton::clicked, this, &MatematicaPage::onRefreshModelsClicked);
    modelBarLay->addWidget(btnRefreshBar);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);

    root->addWidget(modelBar);
    root->addWidget(sep);

    /* ─── Schede strumenti (altezza naturale, niente splitter) ─── */
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName("mainTabs");
    m_tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_tabs->tabBar()->setUsesScrollButtons(true);
    m_tabs->tabBar()->setElideMode(Qt::ElideRight);
    connect(m_tabs, &QTabWidget::currentChanged,
            this, [this](int) { onAdjustTabHeight(); });
    m_tabs->addTab(buildSeqTab(),      "\xf0\x9f\x94\xa2  Sequenza");          /* 🔢 */
    m_tabs->addTab(buildConstTab(),    "\xcf\x80  Costanti");
    /* FIX: "#\xe2\x83\xbf" era un byte errato — decodifica a U+20FF, un
     * codepoint Unicode non assegnato (tofu/box vuoto), segnalato
     * dall'utente ("dopo il carattere # non si legge"). Sostituito con
     * un'emoji singola reale (non una sequenza combining), niente rischio
     * di rendering rotto. */
    m_tabs->addTab(buildNthTab(),      "\xf0\x9f\x94\x9f  N-esimo");   /* 🔟 */
    m_tabs->addTab(buildBoolTab(),     "\xe2\x88\xa7  Booleana");              /* ∧ — 4° tab, visibile */
    m_tabs->addTab(buildExprTab(),     "\xf0\x9f\xa7\xae  Espressione");       /* 🧮 */
    m_tabs->addTab(buildSolveTab(),    "\xf0\x9f\x93\x90  Risolvi Passi");    /* 📐 */
    m_solveTabIdx = m_tabs->count() - 1;
    /* Analisi 1/2 lazy: placeholder vuoto, costruito al primo accesso da
       ensureAnalisiTabBuilt() — vedi commento su m_analisi1Idx in main_math.h */
    m_analisi1Idx = m_tabs->count();
    m_tabs->addTab(new QWidget(m_tabs), "\xf0\x9f\x93\x98  Analisi 1");       /* 📘 */
    m_analisi2Idx = m_tabs->count();
    m_tabs->addTab(new QWidget(m_tabs), "\xf0\x9f\x93\x99  Analisi 2");       /* 📙 */
    connect(m_tabs, &QTabWidget::currentChanged, this, &MatematicaPage::ensureAnalisiTabBuilt);
    root->addWidget(m_tabs);   /* stretch=0: prende solo lo spazio che gli serve */

    /* ─── Output (prende tutto lo spazio restante) ─── */
    auto* outBox = new QWidget(this);
    auto* outLay = new QVBoxLayout(outBox);
    outLay->setContentsMargins(8, 4, 8, 8);
    outLay->setSpacing(4);

    /* Barra stato + pulsanti */
    auto* ctrlRow = new QHBoxLayout;
    m_status = new QLabel(tr("\xf0\x9f\x93\x90  Pronto."), outBox);
    m_status->setObjectName("statusLabel");
    ctrlRow->addWidget(m_status, 1);

    auto* btnCopy = new QPushButton(tr("\xf0\x9f\x93\x8b  Copia"), outBox);
    btnCopy->setObjectName("actionBtn");
    btnCopy->setFixedHeight(dpiScale(26));
    btnCopy->setToolTip(tr("Copia tutto l'output negli appunti"));
    connect(btnCopy, &QPushButton::clicked, this, &MatematicaPage::onCopyClicked);
    ctrlRow->addWidget(btnCopy);

    auto* btnClear = new QPushButton(tr("\xf0\x9f\x97\x91  Cancella"), outBox);
    btnClear->setObjectName("actionBtn");
    btnClear->setFixedHeight(dpiScale(26));
    connect(btnClear, &QPushButton::clicked, this, &MatematicaPage::onClearOutputClicked);
    ctrlRow->addWidget(btnClear);

    auto* btnStop = new QPushButton(tr("\xe2\x96\xa0  Stop"), outBox);
    btnStop->setObjectName("stopBtn");
    btnStop->setFixedHeight(dpiScale(26));
    btnStop->setProperty("execFull", btnStop->text());
    btnStop->setProperty("execIcon", QString::fromUtf8("\xe2\x96\xa0"));
    btnStop->setProperty("execText", "Stop");
    connect(btnStop, &QPushButton::clicked, this, &MatematicaPage::onStopClicked);
    ctrlRow->addWidget(btnStop);

    auto* btnLatex = new QPushButton(tr("\xf0\x9f\x94\xac LaTeX"), outBox);  /* 🔬 */
    btnLatex->setObjectName("navBtn");
    btnLatex->setFixedHeight(dpiScale(26));
    btnLatex->setCheckable(true);
    btnLatex->setToolTip(tr("Mostra/nascondi il pannello di rendering LaTeX per l'ultima risposta AI"));
    connect(btnLatex, &QPushButton::toggled, outBox, [this](bool on) {
        if (m_latexOut) m_latexOut->setVisible(on);
    });
    ctrlRow->addWidget(btnLatex);

    outLay->addLayout(ctrlRow);

    /* ── Barra simboli LaTeX ── */
    outLay->addWidget(buildSymbolBar());

    m_output = new QPlainTextEdit(outBox);
    m_output->setObjectName("chatLog");
    m_output->setReadOnly(true);
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_output->setMaximumBlockCount(50000);
    m_output->setPlaceholderText(
        "I risultati appariranno qui.\n\n"
        "\xcf\x80 = 3.14159265358979323846...\n"
        "e = 2.71828182845904523536...\n"
        "\xcf\x86 = 1.61803398874989484820...");
    outLay->addWidget(m_output, 1);

    /* ── Pannello LaTeX (rendering KaTeX) — visibile solo se l'utente lo attiva ── */
    m_latexOut = new LatexView(outBox);
    m_latexOut->setVisible(false);
    m_latexOut->setMinimumHeight(dpiScale(160));
    outLay->addWidget(m_latexOut);

    root->addWidget(outBox, 1);  /* stretch=1: prende tutto lo spazio restante */

    /* Carica la lista modelli al primo avvio (differito per evitare fetchModels nel costruttore) */
    QTimer::singleShot(0, this, &MatematicaPage::onLoadModelsOnce);
    /* Adatta l'altezza del tab dopo il primo layout reale */
    QTimer::singleShot(0, this, &MatematicaPage::onAdjustTabHeight);

    /* Sincronizza il combo modello quando il modello cambia da Impostazioni o
       da un'altra scheda. */
    connect(m_ai, &AiClient::modelChanged, this, &MatematicaPage::onAiModelChanged);

    /* Traccia quale QLineEdit ha il focus per l'inserimento dei simboli */
    connect(qApp, &QApplication::focusChanged, this,
            [this](QWidget*, QWidget* now) {
                if (auto* le = qobject_cast<QLineEdit*>(now))
                    if (isAncestorOf(le))
                        m_symTarget = le;
            });
}

/* ══════════════════════════════════════════════════════════════
   buildSymbolBar — palette simboli LaTeX cliccabili
   Inserisce il testo corrispondente nel QLineEdit attivo.
   ══════════════════════════════════════════════════════════════ */
QWidget* MatematicaPage::buildSymbolBar()
{
    struct Sym { const char* show; const char* ins; const char* tip; };
    struct Cat { const char* label; QList<Sym> syms; };

    const QList<Cat> cats = {
      { "α Greci", {
        {"\xce\xb1","alpha","α — alpha"},   {"\xce\xb2","beta","β — beta"},
        {"\xce\xb3","gamma","γ — gamma"},   {"\xce\xb4","delta","δ — delta"},
        {"\xce\xb5","epsilon","ε — epsilon"},{"\xce\xb6","zeta","ζ — zeta"},
        {"\xce\xb7","eta","η — eta"},         {"\xce\xb8","theta","θ — theta"},
        {"\xce\xb9","iota","ι — iota"},       {"\xce\xba","kappa","κ — kappa"},
        {"\xce\xbb","lambda","λ — lambda"},   {"\xce\xbc","mu","μ — mu"},
        {"\xce\xbd","nu","ν — nu"},           {"\xce\xbe","xi","ξ — xi"},
        {"\xcf\x80","pi","π → pi"},           {"\xcf\x81","rho","ρ — rho"},
        {"\xcf\x83","sigma","σ — sigma"},     {"\xcf\x84","tau","τ — tau"},
        {"\xcf\x86","phi","φ — phi"},         {"\xcf\x87","chi","χ — chi"},
        {"\xcf\x88","psi","ψ — psi"},         {"\xcf\x89","omega","ω — omega"},
        {"\xce\x93","Gamma","Γ — Gamma"},     {"\xce\x94","Delta","Δ — Delta"},
        {"\xce\x98","Theta","Θ — Theta"},     {"\xce\x9b","Lambda","Λ — Lambda"},
        {"\xce\xa0","Pi","Π — Pi"},           {"\xce\xa3","Sigma","Σ — Sigma"},
        {"\xce\xa6","Phi","Φ — Phi"},         {"\xce\xa8","Psi","Ψ — Psi"},
        {"\xce\xa9","Omega","Ω — Omega"},
      }},
      { "\xe2\x88\x91 Operatori", {
        {"\xe2\x88\x9e","oo","∞ → oo (infinito)"},
        {"\xcf\x80","pi","π → pi"},
        {"\xe2\x88\x82","diff(","d → diff(f,x)"},
        {"\xe2\x88\x87","∇","nabla"},
        {"\xe2\x88\x9a","sqrt(","√ → sqrt("},
        {"\xe2\x88\xab","integrate(","∫ → integrate(f,x)"},
        {"\xe2\x88\xae","integrate(","∮ → integrate(f,x)"},
        {"\xe2\x88\x91","Sum(","∑ → Sum(f,(x,0,n))"},
        {"\xe2\x88\x8f","Product(","∏ → Product(f,(x,0,n))"},
        {"\xc2\xb1","\xc2\xb1","±"},
        {"\xc3\x97","*","× → *"},
        {"\xc3\xb7","/","÷ → /"},
        {"\xc2\xb7","*","· → *"},
        {"\xc2\xb0","\xc2\xb0","° (gradi)"},
        {"\xe2\x80\x98","'","' (primo/derivata)"},
        {"\xe2\x80\x99","''","'' (secondo)"},
        {"\xe2\x84\x8f","hbar","ℏ — costante di Planck ridotta"},
        {"e","E","e → E (Eulero)"},
        {"i","I","i → I (unità immaginaria)"},
      }},
      { "\xe2\x89\xa4 Relazioni", {
        {"\xe2\x89\xa4","<=","≤ → <="},
        {"\xe2\x89\xa5",">=","≥ → >="},
        {"\xe2\x89\xa0","!=","≠ → !="},
        {"\xe2\x89\x88","≈","≈ (circa)"},
        {"\xe2\x89\xa1","Eq(","≡ → Eq("},
        {"\xe2\x89\x85","≅","≅ (congruente)"},
        {"\xe2\x88\x9d","∝","∝ (proporzionale)"},
        {"\xe2\x88\xbc","∼","∼ (simile)"},
        {"\xe2\x88\x88","∈","∈ (appartiene a)"},
        {"\xe2\x88\x89","∉","∉ (non appartiene)"},
        {"\xe2\x8a\x82","⊂","⊂ (sottoinsieme stretto)"},
        {"\xe2\x8a\x83","⊃","⊃ (sovrainsieme stretto)"},
        {"\xe2\x8a\x86","⊆","⊆ (sottoinsieme)"},
        {"\xe2\x8a\x87","⊇","⊇ (sovrainsieme)"},
        {"\xe2\x88\xa9","∩","∩ (intersezione)"},
        {"\xe2\x88\xaa","∪","∪ (unione)"},
        {"\xe2\x88\xa7","And(","∧ → And("},
        {"\xe2\x88\xa8","Or(","∨ → Or("},
        {"\xc2\xac","Not(","¬ → Not("},
        {"\xe2\x8a\x95","⊕","⊕ (or esclusivo)"},
        {"\xe2\x8a\x97","⊗","⊗ (prodotto tensoriale)"},
        {"\xe2\x8a\xa5","Perpendicular","⊥ (perpendicolare)"},
        {"\xe2\x88\xa5","Parallel","∥ (parallelo)"},
      }},
      { "\xe2\x84\x9d Insiemi", {
        {"\xe2\x84\x95","ℕ","N — naturali"},
        {"\xe2\x84\xa4","ℤ","Z — interi"},
        {"\xe2\x84\x9a","ℚ","Q — razionali"},
        {"\xe2\x84\x9d","ℝ","R — reali"},
        {"\xe2\x84\x82","ℂ","C — complessi"},
        {"\xe2\x84\x99","ℙ","P — primi"},
        {"\xe2\x84\x8d","ℍ","H — quaternioni"},
        {"\xe2\x88\x85","EmptySet","∅ → EmptySet"},
        {"\xe2\x88\x80","\\forall","∀ (per ogni)"},
        {"\xe2\x88\x83","\\exists","∃ (esiste)"},
        {"\xe2\x88\x84","\\nexists","∄ (non esiste)"},
        {"\xe2\x84\xb5","ℵ","ℵ (aleph)"},
      }},
      { "\xe2\x86\x92 Frecce", {
        {"\xe2\x86\x92","→","→"},   {"\xe2\x86\x90","←","←"},
        {"\xe2\x86\x91","↑","↑"},   {"\xe2\x86\x93","↓","↓"},
        {"\xe2\x86\x94","↔","↔"},   {"\xe2\x86\xa6","↦","x ↦ f(x)"},
        {"\xe2\x87\x92","⇒","⇒"},   {"\xe2\x87\x90","⇐","⇐"},
        {"\xe2\x87\x94","⇔","⇔"},   {"\xe2\x9f\xb9","⟹","⟹ (implica)"},
        {"\xe2\x9f\xba","⟺","⟺ (se e solo se)"},
        {"\xe2\x86\x97","↗","↗"},   {"\xe2\x86\x98","↘","↘"},
        {"\xe2\x86\x99","↙","↙"},   {"\xe2\x86\x96","↖","↖"},
      }},
    };

    auto* bar    = new QWidget(this);
    auto* barLay = new QVBoxLayout(bar);
    barLay->setContentsMargins(2, 2, 2, 0);
    barLay->setSpacing(2);

    /* Riga superiore: label + combo categoria */
    auto* topRow = new QHBoxLayout;
    topRow->setSpacing(6);
    auto* symLbl = new QLabel(
        "\xcf\x83  <b>Simboli LaTeX:</b>", bar);     /* σ Simboli LaTeX: */
    symLbl->setTextFormat(Qt::RichText);
    symLbl->setObjectName("cardDesc");
    topRow->addWidget(symLbl);

    auto* catCmb = new QComboBox(bar);
    catCmb->setFixedWidth(dpiScale(130));
    for (const auto& c : cats)
        catCmb->addItem(QString::fromUtf8(c.label));
    topRow->addWidget(catCmb);

    auto* hintSym = new QLabel(
        "<small style='color:#64748b'>"
        "Clicca un simbolo per inserirlo nel campo attivo</small>", bar);
    hintSym->setTextFormat(Qt::RichText);
    topRow->addWidget(hintSym);
    topRow->addStretch(1);
    barLay->addLayout(topRow);

    /* Stack: una pagina per categoria, ciascuna con una riga scrollabile di bottoni */
    m_symStack = new QStackedWidget(bar);

    for (const auto& cat : cats) {
        auto* scroll = new QScrollArea;
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setFixedHeight(dpiScale(34));

        auto* row = new QWidget;
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(2, 1, 2, 1);
        rowLay->setSpacing(2);

        for (const auto& s : cat.syms) {
            auto* btn = new QToolButton(row);
            btn->setText(QString::fromUtf8(s.show));
            btn->setFixedSize(dpiScale(28), dpiScale(28));
            btn->setProperty("sym", QString::fromUtf8(s.ins));
            btn->setToolTip(QString::fromUtf8(s.tip));
            btn->setObjectName("navBtn");
            connect(btn, &QToolButton::clicked, this, &MatematicaPage::onSymBtnClicked);
            rowLay->addWidget(btn);
        }
        rowLay->addStretch(1);
        row->adjustSize();
        scroll->setWidget(row);
        m_symStack->addWidget(scroll);
    }

    barLay->addWidget(m_symStack);

    connect(catCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MatematicaPage::onSymCatChanged);

    return bar;
}

void MatematicaPage::onSymCatChanged(int idx)
{
    if (m_symStack) m_symStack->setCurrentIndex(idx);
}

void MatematicaPage::onSymBtnClicked()
{
    const QString sym = sender()->property("sym").toString();
    if (m_symTarget && m_symTarget->isVisible()) {
        m_symTarget->insert(sym);
        m_symTarget->setFocus();
    }
}

/* ══════════════════════════════════════════════════════════════
   buildSeqTab — Sequenza di numeri → formula matematica
   ══════════════════════════════════════════════════════════════ */
QWidget* MatematicaPage::buildSeqTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 6, 12, 6);
    lay->setSpacing(5);

    lay->addWidget(new QLabel(
        "<b>Inserisci una sequenza di numeri separati da virgole o spazi:</b>", w));

    auto* inputRow = new QHBoxLayout;
    m_seqInput = new QLineEdit(w);
    m_seqInput->setPlaceholderText(tr("es. 1, 4, 9, 16, 25   oppure   1 1 2 3 5 8 13"));
    m_seqInput->setMinimumWidth(320);
    inputRow->addWidget(m_seqInput, 1);

    auto* btnFile = new QPushButton(tr("\xf0\x9f\x93\x82  Apri file"), w);  /* 📂 */
    btnFile->setObjectName("actionBtn");
    btnFile->setToolTip(
        "Importa numeri da file:\n"
        "  \xe2\x80\xa2 TXT / CSV\n"
        "  \xe2\x80\xa2 Excel (.xlsx)\n"
        "  \xe2\x80\xa2 Word (.docx / .doc)\n"
        "  \xe2\x80\xa2 PDF\n"
        "Estrae automaticamente tutti i numeri trovati.");
    connect(btnFile, &QPushButton::clicked, this, &MatematicaPage::importFromFile);
    inputRow->addWidget(btnFile);

    lay->addLayout(inputRow);

    /* Risultato rilevamento locale — nascosto finché non c'è testo */
    m_seqResult = new QLabel("", w);
    m_seqResult->setObjectName("statusLabel");
    m_seqResult->setWordWrap(true);
    m_seqResult->setVisible(false);          /* nessuna altezza quando vuoto */
    lay->addWidget(m_seqResult);

    /* Opzioni: termini successivi da suggerire */
    auto* optRow = new QHBoxLayout;
    optRow->addWidget(new QLabel(tr("Suggerisci i prossimi"), w));
    m_nextTerms = new QSpinBox(w);
    m_nextTerms->setRange(1, 20);
    m_nextTerms->setValue(5);
    optRow->addWidget(m_nextTerms);
    optRow->addWidget(new QLabel("termini", w));
    optRow->addStretch(1);

    lay->addLayout(optRow);

    /* Pulsanti */
    auto* btnRow = new QHBoxLayout;

    auto tagExecM = [](QPushButton* btn, const char* icon, const char* text){
        btn->setProperty("execFull", btn->text());
        btn->setProperty("execIcon", QString::fromUtf8(icon));
        btn->setProperty("execText", QString::fromUtf8(text));
    };

    auto* btnLocal = new QPushButton(
        "\xf0\x9f\x94\x8d  Rileva pattern (locale, istantaneo)", w);
    btnLocal->setObjectName("actionBtn");
    tagExecM(btnLocal, "\xf0\x9f\x94\x8d", "Rileva pattern");
    connect(btnLocal, &QPushButton::clicked, this, &MatematicaPage::onLocalPatternClicked);
    btnRow->addWidget(btnLocal);

    auto* btnSympy = new QPushButton(
        "\xcf\x83  Interpola con sympy (preciso)", w);
    btnSympy->setObjectName("actionBtn");
    tagExecM(btnSympy, "\xcf\x83", "Interpola");
    connect(btnSympy, &QPushButton::clicked, this, &MatematicaPage::onSympyClicked);
    btnRow->addWidget(btnSympy);

    auto* btnAI = new QPushButton(
        "\xf0\x9f\xa4\x96  Analizza con AI (spiega + storia)", w);
    btnAI->setObjectName("actionBtn");
    btnAI->setProperty("highlight", "true");
    tagExecM(btnAI, "\xf0\x9f\xa4\x96", "Analizza AI");
    connect(btnAI, &QPushButton::clicked, this, &MatematicaPage::onAnalyzeAiClicked);
    btnRow->addWidget(btnAI);

    lay->addLayout(btnRow);
    lay->addStretch(1);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildConstTab — costanti matematiche ad alta precisione
   ══════════════════════════════════════════════════════════════ */
QWidget* MatematicaPage::buildConstTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(8);

    lay->addWidget(new QLabel(
        "<b>Calcola costanti matematiche con precisione arbitraria:</b>", w));

    auto* grid = new QGridLayout;
    grid->setSpacing(8);

    grid->addWidget(new QLabel(tr("Costante:"), w), 0, 0);
    m_constCombo = new QComboBox(w);
    m_constCombo->addItem("\xcf\x80  pi greco",          "pi");
    m_constCombo->addItem("e  numero di Eulero",        "e");
    m_constCombo->addItem("\xcf\x86  sezione aurea",     "phi");
    m_constCombo->addItem("\xe2\x88\x9a\x32  radice di 2", "sqrt2");
    m_constCombo->addItem("\xe2\x88\x9a\x33  radice di 3", "sqrt3");
    m_constCombo->addItem("\xe2\x88\x9a\x35  radice di 5", "sqrt5");
    m_constCombo->addItem("\xce\xb3  costante di Eulero-Mascheroni", "euler_gamma");
    m_constCombo->addItem("ln(2)  logaritmo naturale di 2", "ln2");
    m_constCombo->addItem("Catalan  costante di Catalan",   "catalan");
    grid->addWidget(m_constCombo, 0, 1);

    grid->addWidget(new QLabel(tr("Cifre decimali:"), w), 1, 0);
    m_precSpin = new QSpinBox(w);
    m_precSpin->setRange(10, 100000);
    m_precSpin->setValue(100);
    m_precSpin->setSingleStep(100);
    m_precSpin->setSuffix("  cifre");
    m_precSpin->setToolTip(
        "Fino a 100 cifre: < 0.1s\n"
        "1000 cifre: < 1s\n"
        "10000 cifre: ~5s\n"
        "100000 cifre: ~60s (mpmath)");
    grid->addWidget(m_precSpin, 1, 1);
    lay->addLayout(grid);

    auto* btnRow = new QHBoxLayout;
    auto* btnCalc = new QPushButton(
        "\xcf\x80  Calcola", w);
    btnCalc->setObjectName("actionBtn");
    btnCalc->setProperty("highlight", "true");
    { auto te=[](QPushButton*b,const char*i,const char*t){b->setProperty("execFull",b->text());b->setProperty("execIcon",QString::fromUtf8(i));b->setProperty("execText",QString::fromUtf8(t));};
      te(btnCalc,"\xcf\x80","Calcola"); }
    connect(btnCalc, &QPushButton::clicked, this, &MatematicaPage::onConstantCalcClicked);
    btnRow->addWidget(btnCalc);

    auto* btnAll = new QPushButton(tr("Tutte le costanti (100 cifre)"), w);
    btnAll->setObjectName("actionBtn");
    connect(btnAll, &QPushButton::clicked, this, &MatematicaPage::onAllConstantsClicked);
    btnRow->addWidget(btnAll);
    lay->addLayout(btnRow);

    /* Nota informativa */
    auto* note = new QLabel(
        "<small>Usa <b>mpmath</b> (installato) — "
        "precisione arbitraria. "
        "Per \xce\xb1 > 10 000 cifre considera che il calcolo pu\xc3\xb2 richiedere decine di secondi.</small>", w);
    note->setWordWrap(true);
    lay->addWidget(note);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildNthTab — N-esimo termine di varie sequenze
   ══════════════════════════════════════════════════════════════ */
QWidget* MatematicaPage::buildNthTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(8);

    lay->addWidget(new QLabel(
        "<b>Calcola l'N-esimo elemento di sequenze famose:</b>", w));

    auto* grid = new QGridLayout;
    grid->setSpacing(8);

    grid->addWidget(new QLabel(tr("Tipo:"), w), 0, 0);
    m_nthType = new QComboBox(w);
    m_nthType->addItem("\xcf\x80  N-esima cifra di \xcf\x80 (da 1)",      "pi_digit");
    m_nthType->addItem("e   N-esima cifra di e (da 1)",                   "e_digit");
    m_nthType->addItem("p   N-esimo numero primo",                         "prime");
    m_nthType->addItem("F   N-esimo numero di Fibonacci",                  "fib");
    m_nthType->addItem("n!  N-esimo fattoriale",                           "fact");
    m_nthType->addItem("2\xe1\xb5\x8f  N-esima potenza di 2",             "pow2");
    m_nthType->addItem("\xcf\x80\xe1\xb5\x8f  primi N cifre di \xcf\x80 (blocco)", "pi_block");
    m_nthType->addItem("\xcf\x86\xe1\xb5\x8f  primi N cifre di \xcf\x86 (blocco)", "phi_block");
    grid->addWidget(m_nthType, 0, 1);

    grid->addWidget(new QLabel(tr("N ="), w), 1, 0);
    m_nthInput = new QLineEdit(w);
    m_nthInput->setPlaceholderText(tr("es. 1000000  (un milione)"));
    m_nthInput->setText(tr("100"));
    grid->addWidget(m_nthInput, 1, 1);
    lay->addLayout(grid);

    /* Descrizione dinamica del tipo selezionato */
    m_nthDescLbl = new QLabel("", w);
    m_nthDescLbl->setObjectName("statusLabel");
    m_nthDescLbl->setWordWrap(true);

    connect(m_nthType, &QComboBox::currentIndexChanged, this, &MatematicaPage::onNthTypeChanged);
    onNthTypeChanged();
    lay->addWidget(m_nthDescLbl);

    auto* btnRow = new QHBoxLayout;
    auto* btnCalc = new QPushButton(tr("\xf0\x9f\x94\x9f  Calcola"), w);   /* 🔟 — FIX byte errato, vedi tab N-esimo */
    btnCalc->setObjectName("actionBtn");
    btnCalc->setProperty("highlight", "true");
    btnCalc->setProperty("execFull", btnCalc->text());
    btnCalc->setProperty("execIcon", QString::fromUtf8("\xf0\x9f\x94\x9f"));
    btnCalc->setProperty("execText", "Calcola");
    connect(btnCalc, &QPushButton::clicked, this, &MatematicaPage::onNthCalcClicked);
    btnRow->addWidget(btnCalc);
    btnRow->addStretch(1);
    lay->addLayout(btnRow);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildExprTab — valutatore di espressioni matematiche
   ══════════════════════════════════════════════════════════════ */
QWidget* MatematicaPage::buildExprTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(8);

    lay->addWidget(new QLabel(
        "<b>Valuta espressioni matematiche (sympy + mpmath):</b>", w));

    auto* inputRow = new QHBoxLayout;
    m_exprInput = new QLineEdit(w);
    m_exprInput->setPlaceholderText(
        "es.  sqrt(2) + sin(pi/4)   oppure   factorial(100)   oppure   integrate(x**2, (x,0,1))");
    inputRow->addWidget(m_exprInput, 1);
    lay->addLayout(inputRow);

    auto* optRow = new QHBoxLayout;
    optRow->addWidget(new QLabel(tr("Cifre di precisione:"), w));
    m_exprPrec = new QSpinBox(w);
    m_exprPrec->setRange(10, 10000);
    m_exprPrec->setValue(50);
    m_exprPrec->setSuffix("  cifre");
    optRow->addWidget(m_exprPrec);
    optRow->addStretch(1);
    lay->addLayout(optRow);

    /* Esempi rapidi */
    auto* exGroup = new QGroupBox(tr("Esempi rapidi"), w);
    auto* exGrid  = new QGridLayout(exGroup);
    exGrid->setSpacing(4);

    struct Ex { const char* label; const char* expr; };
    Ex examples[] = {
        { "\xcf\x80\xc2\xb2/6",        "pi**2 / 6" },
        { "e^\xcf\x80 \xe2\x88\x92 \xcf\x80^e", "exp(pi) - pi**exp(1)" },
        { "\xe2\x88\xab x\xc2\xb2 dx",  "integrate(x**2, (x, 0, 1))" },
        { "\xe2\x88\x9a(2+\xe2\x88\x9a" "3)","sqrt(2 + sqrt(3))" },
        { "1000!",                        "factorial(1000)" },
        { "mcd(144,180)",                 "gcd(144, 180)" },
        { "Stirling 100",                 "log(factorial(100)).evalf()" },
        { "\xcf\x86^20",                  "phi**20" },
    };

    int r = 0, c = 0;
    for (const auto& ex : examples) {
        auto* btn = new QPushButton(ex.label, exGroup);
        btn->setObjectName("navBtn");
        btn->setFixedHeight(dpiScale(24));
        btn->setProperty("mathExpr", QString::fromUtf8(ex.expr));
        connect(btn, &QPushButton::clicked, this, &MatematicaPage::onExampleClicked);
        exGrid->addWidget(btn, r, c);
        if (++c == 4) { c = 0; ++r; }
    }
    lay->addWidget(exGroup);

    auto* btnRow = new QHBoxLayout;
    auto* btnEval = new QPushButton(tr("\xf0\x9f\xa7\xae  Calcola"), w);
    btnEval->setObjectName("actionBtn");
    btnEval->setProperty("highlight", "true");
    btnEval->setToolTip(tr("Valuta l'espressione matematica (supporta frazioni, potenze, funzioni trigonometriche)"));
    btnEval->setProperty("execFull", btnEval->text());
    btnEval->setProperty("execIcon", QString::fromUtf8("\xf0\x9f\xa7\xae"));
    btnEval->setProperty("execText", "Calcola");
    connect(btnEval, &QPushButton::clicked, this, &MatematicaPage::onExprEvalClicked);
    btnRow->addWidget(btnEval);

    auto* btnSimplify = new QPushButton(tr("\xe2\x99\xbe  Semplifica (sympy)"), w);
    btnSimplify->setObjectName("actionBtn");
    btnSimplify->setToolTip(tr("Semplifica l'espressione algebrica con SymPy (espande, fattorizza, riduce)"));
    btnSimplify->setProperty("execFull", btnSimplify->text());
    btnSimplify->setProperty("execIcon", QString::fromUtf8("\xe2\x99\xbe"));
    btnSimplify->setProperty("execText", "Semplifica");
    connect(btnSimplify, &QPushButton::clicked, this, &MatematicaPage::onSimplifyClicked);
    btnRow->addWidget(btnSimplify);

    connect(m_exprInput, &QLineEdit::returnPressed, this, &MatematicaPage::onExprReturnPressed);
    lay->addLayout(btnRow);
    lay->addStretch(1);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   runSequence — rileva pattern localmente + avvia sympy
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::runSequence()
{
    /* chiamato dal pulsante "Rileva" (gestito inline nella scheda) */
}

/* ══════════════════════════════════════════════════════════════
   runConstant — costante con mpmath
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::runConstant()
{
    const QString key   = m_constCombo->currentData().toString();
    const QString label = m_constCombo->currentText();
    const int     prec  = m_precSpin->value();

    /* Mappa chiave → espressione mpmath */
    struct Map { const char* key; const char* expr; };
    Map table[] = {
        { "pi",          "mp.pi" },
        { "e",           "mp.e" },
        { "phi",         "mp.phi" },
        { "sqrt2",       "mp.sqrt(2)" },
        { "sqrt3",       "mp.sqrt(3)" },
        { "sqrt5",       "mp.sqrt(5)" },
        { "euler_gamma", "mp.euler" },
        { "ln2",         "mp.log(2)" },
        { "catalan",     "mp.catalan" },
    };

    QString mpExpr;
    for (const auto& m : table)
        if (key == m.key) { mpExpr = m.expr; break; }

    if (mpExpr.isEmpty()) return;

    const QString py = QString(
        "from mpmath import mp\n"
        "mp.dps = %1 + 10\n"
        "val = %2\n"
        "s = mp.nstr(val, %1, strip_zeros=False)\n"
        "print(f'%3')\n"
        "print(f'{s}')\n"
        "print()\n"
        "print(f'Cifre richieste: %1')\n"
        "print(f'Cifre calcolate: {len(s.replace(\".\",\"\").replace(\"-\",\"\").lstrip(\"0\"))} (appross.)')\n"
    ).arg(prec).arg(mpExpr).arg(label.trimmed());

    clearOutput();
    appendOutput(QString("\xcf\x80  Calcolo %1 a %2 cifre...\n\n").arg(label.trimmed()).arg(prec));
    runPython(py);
}

/* ══════════════════════════════════════════════════════════════
   runNth — N-esimo termine
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::runNth()
{
    const QString type = m_nthType->currentData().toString();
    const QString nStr = m_nthInput->text().trimmed();
    bool ok = false;
    const long long N = nStr.toLongLong(&ok);
    if (!ok || N < 1) {
        setStatus(tr("\xe2\x9d\x8c  Inserisci un numero intero positivo per N."));
        LogBus::post("\xe2\x9d\x8c Matematica: Inserisci un numero intero positivo per N.");
        return;
    }

    QString py;

    if (type == "pi_digit") {
        py = QString(
            "from mpmath import mp\n"
            "mp.dps = %1 + 20\n"
            "s = mp.nstr(mp.pi, %1 + 10)\n"
            "# Rimuovi '3.' e prendi solo le cifre decimali\n"
            "digits = s.replace('3.', '').replace('.', '')\n"
            "if %1 <= len(digits):\n"
            "    d = digits[%1 - 1]\n"
            "    print(f'La {%1}-esima cifra decimale di \\u03c0 \\xe8: {d}')\n"
            "    ctx = digits[max(0,%1-6):%1+5]\n"
            "    pos_in_ctx = min(%1-1, 5)\n"
            "    print(f'Contesto: ...{ctx[:pos_in_ctx]}[{d}]{ctx[pos_in_ctx+1:]}...')\n"
            "else:\n"
            "    print('N troppo grande — aumenta la precisione.')\n"
        ).arg(N);
    }
    else if (type == "e_digit") {
        py = QString(
            "from mpmath import mp\n"
            "mp.dps = %1 + 20\n"
            "s = mp.nstr(mp.e, %1 + 10)\n"
            "digits = s.replace('2.', '').replace('.', '')\n"
            "if %1 <= len(digits):\n"
            "    d = digits[%1 - 1]\n"
            "    print(f'La {%1}-esima cifra decimale di e \\xe8: {d}')\n"
            "    ctx = digits[max(0,%1-6):%1+5]\n"
            "    pos_in_ctx = min(%1-1, 5)\n"
            "    print(f'Contesto: ...{ctx[:pos_in_ctx]}[{d}]{ctx[pos_in_ctx+1:]}...')\n"
            "else:\n"
            "    print('N troppo grande — aumenta la precisione.')\n"
        ).arg(N);
    }
    else if (type == "prime") {
        py = QString(
            "from sympy import prime, primerange\n"
            "import time\n"
            "t = time.time()\n"
            "p = prime(%1)\n"
            "elapsed = time.time() - t\n"
            "print(f'Il {%1}-esimo numero primo \\xe8:')\n"
            "print(f'  p({%1}) = {p}')\n"
            "print(f'  ({len(str(p))} cifre, calcolato in {elapsed:.3f}s)')\n"
        ).arg(N);
    }
    else if (type == "fib") {
        py = QString(
            "from mpmath import mp, fib\n"
            "mp.dps = 50\n"
            "import time\n"
            "t = time.time()\n"
            "f = int(fib(%1))\n"
            "elapsed = time.time() - t\n"
            "s = str(f)\n"
            "print(f'Il {%1}-esimo numero di Fibonacci:')\n"
            "if len(s) <= 200:\n"
            "    print(f'  F({%1}) = {s}')\n"
            "else:\n"
            "    print(f'  F({%1}) = {s[:80]}...')\n"
            "    print(f'  ...{s[-20:]}')\n"
            "print(f'  ({len(s)} cifre, calcolato in {elapsed:.3f}s)')\n"
        ).arg(N);
    }
    else if (type == "fact") {
        py = QString(
            "from sympy import factorial\n"
            "import time\n"
            "t = time.time()\n"
            "f = factorial(%1)\n"
            "elapsed = time.time() - t\n"
            "s = str(f)\n"
            "print(f'{%1}! =')\n"
            "if len(s) <= 300:\n"
            "    print(f'  {s}')\n"
            "else:\n"
            "    print(f'  {s[:100]}...')\n"
            "    print(f'  ...{s[-30:]}')\n"
            "print(f'  ({len(s)} cifre, calcolato in {elapsed:.3f}s)')\n"
        ).arg(N);
    }
    else if (type == "pow2") {
        py = QString(
            "import time\n"
            "t = time.time()\n"
            "v = 2 ** %1\n"
            "elapsed = time.time() - t\n"
            "s = str(v)\n"
            "print(f'2^{%1} =')\n"
            "if len(s) <= 300:\n"
            "    print(f'  {s}')\n"
            "else:\n"
            "    print(f'  {s[:100]}...')\n"
            "    print(f'  ...{s[-30:]}')\n"
            "print(f'  ({len(s)} cifre, calcolato in {elapsed:.3f}s)')\n"
        ).arg(N);
    }
    else if (type == "pi_block") {
        py = QString(
            "from mpmath import mp\n"
            "mp.dps = %1 + 10\n"
            "s = mp.nstr(mp.pi, %1 + 5)\n"
            "print(f'Le prime {%1} cifre di \\u03c0:')\n"
            "print(s[:%1+2])\n"   /* +2 per '3.' */
        ).arg(N);
    }
    else if (type == "phi_block") {
        py = QString(
            "from mpmath import mp\n"
            "mp.dps = %1 + 10\n"
            "s = mp.nstr(mp.phi, %1 + 5)\n"
            "print(f'Le prime {%1} cifre di \\u03c6 (sezione aurea):')\n"
            "print(s[:%1+2])\n"
        ).arg(N);
    }

    if (py.isEmpty()) return;
    clearOutput();
    appendOutput(QString("\xf0\x9f\x94\x9f  Calcolo in corso (N=%1)...\n\n").arg(N));   /* 🔟 */
    runPython(py);
}

/* Forward declarations degli helper definiti più avanti nel file */
static int     mathModelScore(const QString& name);
static QString buildUnitConvertScript(const QString&);

/* ══════════════════════════════════════════════════════════════
   runExpr — valuta espressione con sympy + mpmath
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::runExpr()
{
    const QString expr = m_exprInput->text().trimmed();
    if (expr.isEmpty()) return;

    /* ── Rilevamento conversione unità di misura ────────────────
       Se la query è una conversione con unità nota → calcola direttamente.
       Se la query sembra una conversione ma l'unità non è riconosciuta →
       chiede al LLM di interpretare l'unità, poi avvia il calcolo. */
    using D = AiClient::QueryDomain;
    const D dom = AiClient::detectQueryDomain(expr);

    if (dom == D::DomainUnitConvert) {
        const QString unitPy = buildUnitConvertScript(expr);
        if (!unitPy.isEmpty()) {
            /* Unità nota → calcola con Python direttamente */
            clearOutput();
            appendOutput(QString("\xf0\x9f\x94\x84  Conversione: %1\n\n").arg(expr));
            runPython(unitPy);
            return;
        }

        /* Unità non riconosciuta → chiede al LLM */
        if (!m_ai) {
            clearOutput();
            appendOutput("\xe2\x9d\x8c  Unità non riconosciuta e nessun backend AI disponibile.\n"
                         "Prova a scrivere l'espressione SymPy direttamente (es. 5 * 1000).");
            return;
        }

        clearOutput();
        appendOutput(QString("\xf0\x9f\xa4\x96  Unità non riconosciuta nella query:\n"
                             "   \"%1\"\n\nChiedo al modello AI di interpretarla...\n\n").arg(expr));

        const QString sys =
            "Sei un esperto di metrologia. L'utente vuole convertire un valore da un'unità "
            "di misura a un'altra. Rispondi SOLO con la formula Python su una riga che calcola "
            "il risultato, nel formato:\n"
            "  print(f'{valore_in} {unita_in}  =  {risultato:.6g} {unita_out}')\n\n"
            "Non aggiungere testo, commenti o spiegazioni. Solo la riga print().";

        delete m_aiSeqHolder;
        m_aiSeqHolder = new QObject(this);
        connect(m_ai, &AiClient::token, m_aiSeqHolder,
                [this](const QString& t){ onAiSeqToken(t); });
        connect(m_ai, &AiClient::finished, m_aiSeqHolder,
                [this](const QString& full) {
                    onAiSeqFinished(full);
                    /* Esegue la formula Python estratta dalla risposta LLM */
                    const QString code = full.trimmed();
                    if (code.startsWith("print("))
                        runPython(code);
                });
        connect(m_ai, &AiClient::error, m_aiSeqHolder,
                [this](const QString& e){ onAiSeqError(e); });

        m_aiRunning = true;
        m_ai->chat(sys, expr);
        return;
    }

    const int prec = m_exprPrec->value();

    const QString py = QString(
        "from sympy import *\n"
        "from sympy import N as Neval\n"
        "from mpmath import mp\n"
        "mp.dps = %1\n"
        "x, y, z, t = symbols('x y z t')\n"
        "n = symbols('n', positive=True, integer=True)\n"
        "result = %2\n"
        "print('Espressione:   ', result)\n"
        "try:\n"
        "    simp = simplify(result)\n"
        "    if simp != result: print('Semplificata:  ', simp)\n"
        "except: pass\n"
        "try:\n"
        "    num = Neval(result, %1)\n"
        "    print(f'Valore numerico ({%1} cifre):')\n"
        "    print(f'  {num}')\n"
        "except Exception as ex:\n"
        "    print(f'  (valore numerico non disponibile: {ex})')\n"
    ).arg(prec).arg(expr);

    clearOutput();
    appendOutput(QString("\xf0\x9f\xa7\xae  Calcolo: %1\n\n").arg(expr));
    runPython(py);
}

/* ══════════════════════════════════════════════════════════════
   runAiSequence — analisi AI della sequenza
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::runAiSequence(const QString& seqStr, int nextN)
{
    if (!m_ai || m_aiRunning) return;
    m_aiRunning = true;

    /* Applica il modello scelto nella combo */
    if (m_modelCombo) {
        const QString sel = m_modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    /* Guardia embedding */
    {
        const QString mn = m_ai->model().toLower();
        if (mn.contains("embed") || mn.contains("minilm") ||
            mn.contains("rerank") || mn.contains("bge-") || mn.contains("-embed")) {
            clearOutput();
            appendOutput(QString("\xe2\x9a\xa0\xef\xb8\x8f  \"%1\" non supporta la chat.\n"
                                 "Seleziona un altro modello dalla combo.")
                         .arg(m_ai->model()));
            m_aiRunning = false;
            return;
        }
    }

    const QString sys =
        "Sei un matematico esperto. Analizza la sequenza di numeri fornita dall'utente. "
        "Il tuo compito:\n"
        "1. Identifica il pattern o la formula matematica (usa notazione a(n)).\n"
        "2. Spiega il ragionamento in modo chiaro.\n"
        "3. Calcola i successivi %1 termini.\n"
        "4. Se la sequenza corrisponde a una sequenza famosa (Fibonacci, triangolari, "
        "   numeri primi, ecc.) menzionala e fornisci un breve contesto storico/matematico.\n"
        "Rispondi SEMPRE e SOLO in italiano.";

    const QString user = QString(
        "Sequenza: %1\n\n"
        "Fornisci la formula, i prossimi %2 termini e il contesto matematico.")
        .arg(seqStr).arg(nextN);

    clearOutput();
    const QString modelName = m_ai->model().isEmpty() ? "AI" : m_ai->model();
    appendOutput(QString("\xf0\x9f\xa4\x96  Modello: %1\n%2\n\n")
                 .arg(modelName, QString(modelName.length() + 12, '-')));

    setStatus(tr("\xf0\x9f\xa4\x96  AI in analisi..."));

    /* Usa holder come context per limitare la durata delle connessioni one-shot */
    delete m_aiSeqHolder;
    m_aiSeqHolder = new QObject(this);
    connect(m_ai, &AiClient::token,    m_aiSeqHolder,
            [this](const QString& tok){ onAiSeqToken(tok); });
    connect(m_ai, &AiClient::finished, m_aiSeqHolder,
            [this](const QString& full){ onAiSeqFinished(full); });
    connect(m_ai, &AiClient::error,    m_aiSeqHolder,
            [this](const QString& msg){ onAiSeqError(msg); });

    m_ai->chat(P::prependMathKnowledge(sys.arg(nextN)), user);
}

/* ══════════════════════════════════════════════════════════════
   runPython — lancia python3 -c CODE in un QProcess,
               streamma stdout/stderr nell'output
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::runPython(const QString& code)
{
    stopPython();

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_proc, &QProcess::readyReadStandardOutput,
            this, &MatematicaPage::onProcReadyRead);
    connect(m_proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MatematicaPage::onProcFinished);
    connect(m_proc, &QProcess::errorOccurred, this,
        [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                setStatus(tr("\xe2\x9d\x8c  Python non trovato nel PATH"));
        });

    setStatus(tr("\xe2\x8f\xb3  Calcolo in corso..."));
    m_proc->start(P::findPython(), QStringList{"-c", code});
    if (!m_proc->waitForStarted(P::kProcessStartTimeoutMs)) {
        setStatus(tr("\xe2\x9d\x8c  Python non trovato nel PATH. Installa Python da python.org"));
        LogBus::post("\xe2\x9d\x8c Matematica: Python non trovato nel PATH.");
        m_proc->deleteLater();
        m_proc = nullptr;
    }
}

void MatematicaPage::stopPython()
{
    if (m_solvePyMode) {
        m_solvePyMode = false;
        m_solveBusy   = false;
        if (m_btnSolve) m_btnSolve->setEnabled(true);
    }
    if (m_proc) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
        m_proc->deleteLater();
        m_proc = nullptr;
        setStatus(tr("\xe2\x96\xa0  Calcolo interrotto."));
    }
}

/* ══════════════════════════════════════════════════════════════
/* ══════════════════════════════════════════════════════════════
   detectPatternLocal — rilevamento pattern senza subprocess
   ══════════════════════════════════════════════════════════════ */
QString MatematicaPage::detectPatternLocal(const QVector<double>& seq) const
{
    const int n = seq.size();
    if (n < 2) return "Troppo corta per rilevare un pattern.";

    const double eps = 1e-9;
    auto eq = [eps](double a, double b){ return std::abs(a - b) < eps; };

    /* Aritmetica: differenze costanti */
    const double d = seq[1] - seq[0];
    bool arith = true;
    for (int i = 2; i < n; ++i)
        if (!eq(seq[i] - seq[i-1], d)) { arith = false; break; }
    if (arith) {
        if (eq(d, 0))
            return QString("Sequenza costante: a(n) = %1").arg(seq[0]);
        return QString("Aritmetica: a(n) = %1 + (n\xe2\x88\x92" "1)\xc2\xb7%2   [d = %2]")
               .arg(seq[0]).arg(d);
    }

    /* Geometrica: rapporti costanti */
    if (!eq(seq[0], 0)) {
        const double r = seq[1] / seq[0];
        bool geom = true;
        for (int i = 2; i < n; ++i)
            if (!eq(seq[i] / seq[i-1], r)) { geom = false; break; }
        if (geom)
            return QString("Geometrica: a(n) = %1 \xc2\xb7 %2^(n\xe2\x88\x92" "1)   [r = %2]")
                   .arg(seq[0]).arg(r);
    }

    /* Quadratica: seconde differenze costanti */
    if (n >= 3) {
        QVector<double> d1(n-1), d2(n-2);
        for (int i = 0; i < n-1; ++i) d1[i] = seq[i+1] - seq[i];
        for (int i = 0; i < n-2; ++i) d2[i] = d1[i+1] - d1[i];
        bool quad = true;
        for (int i = 1; i < (int)d2.size(); ++i)
            if (!eq(d2[i], d2[0])) { quad = false; break; }
        if (quad && !eq(d2[0], 0)) {
            const double a = d2[0] / 2.0;
            const double b = d1[0] - a;   /* d1[0] = a(2)-a(1) = 3a+b → b = d1[0]-3a */
            /* risolvo: a(1) = a + b + c → c = seq[0] - a - b */
            const double c = seq[0] - a - b;
            return QString("Quadratica: a(n) = %1\xc2\xb7n\xc2\xb2 + %2\xc2\xb7n + %3")
                   .arg(a).arg(b - 2*a).arg(c + a - b + a);
            /* Nota: coefficienti calcolati con indici da 1 */
        }
    }

    /* Fibonacci-like: a(i) = a(i-1) + a(i-2) */
    if (n >= 3) {
        bool fib = true;
        for (int i = 2; i < n; ++i)
            if (!eq(seq[i], seq[i-1] + seq[i-2])) { fib = false; break; }
        if (fib)
            return QString("Fibonacci-like: a(n) = a(n\xe2\x88\x92" "1) + a(n\xe2\x88\x92" "2), "
                           "a(1)=%1, a(2)=%2").arg(seq[0]).arg(seq[1]);
    }

    /* Quadrati: seq[i] == (i+1)^2 */
    {
        bool sq = true;
        for (int i = 0; i < n; ++i)
            if (!eq(seq[i], (double)(i+1)*(i+1))) { sq = false; break; }
        if (sq) return "Quadrati perfetti: a(n) = n\xc2\xb2";
    }

    /* Cubi */
    {
        bool cu = true;
        for (int i = 0; i < n; ++i)
            if (!eq(seq[i], (double)(i+1)*(i+1)*(i+1))) { cu = false; break; }
        if (cu) return "Cubi: a(n) = n\xc2\xb3";
    }

    /* Triangolari: n*(n+1)/2 */
    {
        bool tri = true;
        for (int i = 0; i < n; ++i)
            if (!eq(seq[i], (double)(i+1)*(i+2)/2.0)) { tri = false; break; }
        if (tri) return "Numeri triangolari: a(n) = n\xc2\xb7(n+1)/2";
    }

    /* Fattoriali */
    {
        bool fact = true;
        double f = 1.0;
        for (int i = 0; i < n; ++i) {
            f *= (i+1);
            if (!eq(seq[i], f)) { fact = false; break; }
        }
        if (fact) return "Fattoriali: a(n) = n!";
    }

    /* Potenze di 2 */
    {
        bool p2 = true;
        for (int i = 0; i < n; ++i)
            if (!eq(seq[i], std::pow(2.0, i+1))) { p2 = false; break; }
        if (p2) return "Potenze di 2: a(n) = 2^n";
    }

    /* Numeri primi (primi 20) */
    {
        static const int primes[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71};
        bool pr = true;
        for (int i = 0; i < n && i < 20; ++i)
            if (!eq(seq[i], primes[i])) { pr = false; break; }
        if (pr) return "Numeri primi: p(1)=2, p(2)=3, p(3)=5, ...  Usa sympy per la formula.";
    }

    return QString("Pattern non riconosciuto localmente — prova \xe2\x80\x9c" "Interpola con sympy\xe2\x80\x9d "
                   "oppure \xe2\x80\x9c" "Analizza con AI\xe2\x80\x9d.");
}

/* ══════════════════════════════════════════════════════════════
   numbersFromText — estrae tutti i numeri (interi e decimali)
   da una stringa di testo grezza e li restituisce come
   sequenza "n1, n2, n3, ..." pronta per m_seqInput.
   ══════════════════════════════════════════════════════════════ */
QString MatematicaPage::numbersFromText(const QString& raw) const
{
    /* Regex: numero opzionalmente preceduto da segno meno,
       con separatore decimale punto o virgola.
       Ignora "virgola come separatore migliaia" (es. 1,000,000)
       cercando solo gruppi di 1-3 cifre dopo virgola. */
    static QRegularExpression reNum(
        R"((?<!\d)-?\d+(?:[.,]\d+)?(?!\d))");

    QStringList found;
    QRegularExpressionMatchIterator it = reNum.globalMatch(raw);
    while (it.hasNext()) {
        QString tok = it.next().captured(0);
        tok.replace(',', '.');    /* normalizza decimale */
        /* verifica che sia un numero valido */
        bool ok = false;
        tok.toDouble(&ok);
        if (ok) found << tok;
    }
    return found.join(", ");
}

/* ══════════════════════════════════════════════════════════════
   extractNumbersFromFile — estrae testo grezzo dal file e
   restituisce la sequenza numerica trovata.
   Formati supportati:
     .txt .csv           → lettura diretta Qt
     .xlsx               → Python stdlib zipfile+xml
     .xls                → avviso (serve xlrd)
     .docx               → Python stdlib zipfile+xml
     .doc                → catdoc (binario di sistema)
     .pdf                → pypdf (Python) o pdftotext
   ══════════════════════════════════════════════════════════════ */
QString MatematicaPage::extractNumbersFromFile(const QString& path, QString& err)
{
    const QString ext = QFileInfo(path).suffix().toLower();

    /* ── TXT / CSV: lettura diretta ── */
    if (ext == "txt" || ext == "csv" || ext == "tsv") {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            err = "Impossibile aprire il file."; return {};
        }
        return numbersFromText(QTextStream(&f).readAll());
    }

    /* ── XLSX: zipfile + xml (stdlib Python) ── */
    if (ext == "xlsx" || ext == "ods") {
        const QString pyCode = QString(
            "import zipfile, xml.etree.ElementTree as ET, sys\n"
            "ns = 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'\n"
            "path = r'%1'\n"
            "numbers = []\n"
            "try:\n"
            "    with zipfile.ZipFile(path) as z:\n"
            "        names = z.namelist()\n"
            "        # Fogli di calcolo\n"
            "        sheets = sorted([n for n in names\n"
            "                         if n.startswith('xl/worksheets/sheet')\n"
            "                         and n.endswith('.xml')])\n"
            "        for sheet in sheets:\n"
            "            tree = ET.parse(z.open(sheet))\n"
            "            for c in tree.getroot().iter(f'{{{ns}}}c'):\n"
            "                t = c.get('t', '')\n"
            "                if t in ('s', 'str', 'b', 'e'): continue\n"
            "                v = c.find(f'{{{ns}}}v')\n"
            "                if v is not None and v.text:\n"
            "                    try:\n"
            "                        f = float(v.text)\n"
            "                        numbers.append(int(f) if f == int(f) else f)\n"
            "                    except: pass\n"
            "except Exception as ex:\n"
            "    print(f'ERRORE: {ex}', flush=True)\n"
            "    sys.exit(1)\n"
            "print(', '.join(str(n) for n in numbers))\n"
        ).arg(path);
        return _runPythonSync(pyCode, err);
    }

    /* ── XLS (vecchio formato binario) ── */
    if (ext == "xls") {
        /* Tenta con xlrd se disponibile, altrimenti suggerisce conversione */
        const QString pyCode = QString(
            "import sys\n"
            "try:\n"
            "    import xlrd\n"
            "    wb = xlrd.open_workbook(r'%1')\n"
            "    nums = []\n"
            "    for sh in wb.sheets():\n"
            "        for r in range(sh.nrows):\n"
            "            for c in range(sh.ncols):\n"
            "                cell = sh.cell(r, c)\n"
            "                if cell.ctype == 2:  # XL_CELL_NUMBER\n"
            "                    v = cell.value\n"
            "                    nums.append(int(v) if v == int(v) else v)\n"
            "    print(', '.join(str(n) for n in nums))\n"
            "except ImportError:\n"
            "    print('ERRORE: formato .xls richiede xlrd (pip install xlrd). '\n"
            "          'Salva il file come .xlsx da Excel e riprova.')\n"
            "    sys.exit(1)\n"
        ).arg(path);
        return _runPythonSync(pyCode, err);
    }

    /* ── DOCX: zipfile + xml (stdlib Python) ── */
    if (ext == "docx") {
        const QString pyCode = QString(
            "import zipfile, xml.etree.ElementTree as ET, re\n"
            "ns = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'\n"
            "path = r'%1'\n"
            "try:\n"
            "    with zipfile.ZipFile(path) as z:\n"
            "        xml = z.read('word/document.xml').decode('utf-8', errors='ignore')\n"
            "    tree = ET.fromstring(xml)\n"
            "    texts = [t.text for t in tree.iter(f'{{{ns}}}t') if t.text]\n"
            "    full = ' '.join(texts)\n"
            "except Exception as ex:\n"
            "    print(f'ERRORE: {ex}')\n"
            "    import sys; sys.exit(1)\n"
            "# Estrai numeri\n"
            "nums_raw = re.findall(r'(?<![,\\d])-?\\d+(?:[.,]\\d+)?(?!\\d)', full)\n"
            "nums = []\n"
            "for n in nums_raw:\n"
            "    try:\n"
            "        v = float(n.replace(',','.'))\n"
            "        nums.append(int(v) if v == int(v) else v)\n"
            "    except: pass\n"
            "print(', '.join(str(n) for n in nums))\n"
        ).arg(path);
        return _runPythonSync(pyCode, err);
    }

    /* ── DOC (vecchio formato binario Word): usa catdoc ── */
    if (ext == "doc") {
        QProcess proc;
        proc.start("catdoc", QStringList{path});
        if (!proc.waitForStarted(P::kProcessStartTimeoutMs)) {
            err = "catdoc non trovato. Installa con: sudo apt install catdoc";
            return {};
        }
        {
            QEventLoop loop;
            QTimer t; t.setSingleShot(true); t.setInterval(15000);
            connect(&proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    &loop, &QEventLoop::quit);
            connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
            t.start(); loop.exec();
        }
        const QString text = QString::fromLocal8Bit(proc.readAllStandardOutput());
        if (text.isEmpty()) { err = "catdoc non ha prodotto output."; return {}; }
        return numbersFromText(text);
    }

    /* ── PDF: pypdf (stdlib) con fallback pdftotext ── */
    if (ext == "pdf") {
        /* Prima prova pypdf */
        const QString pyCode = QString(
            "import sys\n"
            "try:\n"
            "    from pypdf import PdfReader\n"
            "    reader = PdfReader(r'%1')\n"
            "    pages = [p.extract_text() or '' for p in reader.pages]\n"
            "    print('\\n'.join(pages))\n"
            "except ImportError:\n"
            "    print('PYPDF_NOT_FOUND')\n"
            "except Exception as ex:\n"
            "    print(f'ERRORE: {ex}')\n"
            "    sys.exit(1)\n"
        ).arg(path);

        QString pyErr;
        QString pyOut = _runPythonSync(pyCode, pyErr);

        if (!pyErr.isEmpty()) { err = pyErr; return {}; }

        if (pyOut.startsWith("PYPDF_NOT_FOUND")) {
            /* Fallback: pdftotext */
            QProcess proc;
            proc.start("pdftotext", QStringList{path, "-"});
            if (!proc.waitForStarted(P::kProcessStartTimeoutMs)) {
                err = "pypdf non trovato e pdftotext non disponibile.";
                return {};
            }
            {
                QEventLoop loop;
                QTimer t; t.setSingleShot(true); t.setInterval(15000);
                connect(&proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                        &loop, &QEventLoop::quit);
                connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
                t.start(); loop.exec();
            }
            pyOut = QString::fromLocal8Bit(proc.readAllStandardOutput());
        }
        return numbersFromText(pyOut);
    }

    err = QString("Formato non supportato: .%1\n"
                  "Formati accettati: txt, csv, xlsx, xls, docx, doc, pdf").arg(ext);
    return {};
}

/* ══════════════════════════════════════════════════════════════
   _runPythonSync — esegue python3 -c CODE e restituisce stdout.
   Uso SOLO per operazioni brevi (import file), non per calcoli
   lunghi — usa runPython() per quelli (streaming asincrono).

   Usa QEventLoop invece di waitForFinished per mantenere l'UI
   reattiva durante l'attesa (scroll, resize, spinner, ecc.).
   ══════════════════════════════════════════════════════════════ */
QString MatematicaPage::_runPythonSync(const QString& code, QString& err)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(P::findPython(), QStringList{"-c", code});
    if (!proc.waitForStarted(P::kProcessStartTimeoutMs)) {
        err = "Python non trovato nel PATH. Installa Python da python.org";
        return {};
    }
    /* QEventLoop al posto di waitForFinished(20000) — stesso timeout ma
       il main thread continua a processare eventi Qt (input, resize, paint). */
    {
        QEventLoop loop;
        QTimer t;
        t.setSingleShot(true);
        t.setInterval(20000);
        connect(&proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                &loop, &QEventLoop::quit);
        connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
        t.start();
        loop.exec();
    }
    const QString out    = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    const QString errOut = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    if (proc.exitCode() != 0) {
        err = errOut.isEmpty() ? out : errOut;
        return {};
    }
    if (out.startsWith("ERRORE:")) {
        err = out.mid(7).trimmed();
        return {};
    }
    return out;
}

/* ══════════════════════════════════════════════════════════════
   importFromFile — apre il dialogo e carica i numeri
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::importFromFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Importa sequenza da file",
        QDir::homePath(),
        "Tutti i formati supportati (*.txt *.csv *.tsv *.xlsx *.xls *.docx *.doc *.pdf);;"
        "Testo (*.txt *.csv *.tsv);;"
        "Excel (*.xlsx *.xls);;"
        "Word (*.docx *.doc);;"
        "PDF (*.pdf);;"
        "Tutti i file (*)");

    if (path.isEmpty()) return;

    setStatus(QString("\xe2\x8f\xb3  Lettura %1...").arg(QFileInfo(path).fileName()));
    QApplication::processEvents();

    QString err;
    const QString nums = extractNumbersFromFile(path, err);

    if (!err.isEmpty()) {
        setStatus(tr("\xe2\x9d\x8c  ") + err);
        LogBus::post("\xe2\x9d\x8c Matematica: " + err);
        QMessageBox::warning(this, tr("Errore importazione"), err);
        return;
    }

    if (nums.isEmpty()) {
        setStatus(tr("\xe2\x9a\xa0\xef\xb8\x8f  Nessun numero trovato nel file."));
        QMessageBox::information(this, tr("Nessun numero"),
            "Il file non contiene numeri riconoscibili.\n"
            "Assicurati che i numeri siano separati da spazi, virgole o a capo.");
        return;
    }

    m_seqInput->setText(nums);

    /* Conta i termini trovati */
    const int count = nums.split(',', Qt::SkipEmptyParts).size();
    setStatus(QString("\xe2\x9c\x85  Importati %1 numeri da %2")
              .arg(count).arg(QFileInfo(path).fileName()));
}

/* ══════════════════════════════════════════════════════════════
   parseSeq — converte stringa "1,4,9,16" in QVector<double>
   ══════════════════════════════════════════════════════════════ */
QVector<double> MatematicaPage::parseSeq(const QString& s, QString& err) const
{
    QVector<double> result;
    const QString clean = s.trimmed();
    if (clean.isEmpty()) { err = "Sequenza vuota."; return result; }

    /* Accetta virgole o spazi come separatori */
    static QRegularExpression sep("[,\\s]+");
    const QStringList parts = clean.split(sep, Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        bool ok = false;
        const double v = p.toDouble(&ok);
        if (!ok) { err = QString("Valore non numerico: \"%1\"").arg(p); return {}; }
        result << v;
    }
    if (result.size() < 2) { err = "Inserisci almeno 2 numeri."; }
    return result;
}

/* ── helpers ── */
void MatematicaPage::appendOutput(const QString& text)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
    m_output->ensureCursorVisible();
}

void MatematicaPage::clearOutput()
{
    m_output->clear();
}

void MatematicaPage::setStatus(const QString& msg)
{
    if (m_status) m_status->setText(msg);
}

/* ══════════════════════════════════════════════════════════════
   Slot — barra output
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onCopyClicked()
{
    QApplication::clipboard()->setText(m_output->toPlainText());
}

void MatematicaPage::onClearOutputClicked()
{
    clearOutput();
}

void MatematicaPage::onStopClicked()
{
    stopPython();
}

/* ══════════════════════════════════════════════════════════════
   Slot — sincronizzazione modello AI
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onAiModelChanged(const QString& newModel)
{
    if (!m_modelCombo) return;
    int idx = m_modelCombo->findData(newModel);
    if (idx < 0) idx = m_modelCombo->findText(newModel, Qt::MatchContains);
    if (idx >= 0 && idx != m_modelCombo->currentIndex()) {
        m_modelCombo->blockSignals(true);
        m_modelCombo->setCurrentIndex(idx);
        m_modelCombo->blockSignals(false);
    } else if (idx < 0) {
        m_modelCombo->blockSignals(true);
        m_modelCombo->setItemText(0, newModel);
        m_modelCombo->setItemData(0, newModel);
        m_modelCombo->setCurrentIndex(0);
        m_modelCombo->blockSignals(false);
    }
}

/* ══════════════════════════════════════════════════════════════
   Slot — combo modelli matematici (helper interni)
   ══════════════════════════════════════════════════════════════ */

/* Punteggio modelli matematici — piu' alto = migliore per matematica */
static int mathModelScore(const QString& name)
{
    const QString n = name.toLower();
    if (n.contains("qwen2.5-math") || n.contains("qwen2_5-math")) return 100;
    if (n.contains("mathstral") || n.contains("math"))             return 90;
    if (n.contains("deepseek-r1"))  return 85;
    if (n.contains("qwq"))          return 80;
    if (n.contains("phi-4") || n.contains("phi4")) return 75;
    if (n.contains("deepseek") && !n.contains("coder")) return 60;
    if (n.contains("qwen")     && !n.contains("coder")) return 50;
    return 0;
}

static bool isEmbedModel(const QString& n)
{
    return n.contains("embed") || n.contains("minilm") ||
           n.contains("rerank") || n.contains("bge-") ||
           n.contains("e5-") || n.contains("-embed");
}

/* ── Converte una query testuale di conversione unità in SymPy ───────
   Riconosce pattern come "converti 5 km in metro", "quanto fa 100 mph in km/h".
   Ritorna lo script Python se riconosce la richiesta, stringa vuota altrimenti. */
static QString buildUnitConvertScript(const QString& text)
{
    /* Tabella conversioni note: (pattern regex-like, factorIn, unitIn, factorOut, unitOut) */
    struct UnitEntry {
        const char* fromU;   /* parole che identificano l'unità sorgente */
        const char* toU;     /* parole che identificano l'unità destinazione */
        double      factor;  /* valore_out = valore_in * factor */
        const char* labelIn;
        const char* labelOut;
    };
    static const UnitEntry kUnits[] = {
        /* lunghezza */
        { "km",   "metro",   1000.0,      "km",   "m"   },
        { "metro", "km",     0.001,       "m",    "km"  },
        { "cm",   "metro",   0.01,        "cm",   "m"   },
        { "mm",   "metro",   0.001,       "mm",   "m"   },
        { "miglia","km",     1.60934,     "mi",   "km"  },
        { "km",   "miglia",  0.621371,    "km",   "mi"  },
        { "pollici","cm",    2.54,        "in",   "cm"  },
        { "piedi", "metro",  0.3048,      "ft",   "m"   },
        /* massa */
        { "kg",   "libbre",  2.20462,     "kg",   "lb"  },
        { "libbre","kg",     0.453592,    "lb",   "kg"  },
        { "grammi","kg",     0.001,       "g",    "kg"  },
        { "once", "grammi",  28.3495,     "oz",   "g"   },
        /* temperatura */
        { "celsius","fahrenheit", 0.0,   "°C",   "°F"  },   /* caso speciale */
        { "fahrenheit","celsius", 0.0,   "°F",   "°C"  },
        { "celsius","kelvin",     0.0,   "°C",   "K"   },
        { "kelvin","celsius",     0.0,   "K",    "°C"  },
        /* velocità */
        { "km/h", "m/s",    0.277778,    "km/h", "m/s" },
        { "m/s",  "km/h",   3.6,         "m/s",  "km/h"},
        { "mph",  "km/h",   1.60934,     "mph",  "km/h"},
        /* pressione */
        { "atm",  "pascal", 101325.0,    "atm",  "Pa"  },
        { "bar",  "pascal", 100000.0,    "bar",  "Pa"  },
        { "psi",  "pascal", 6894.76,     "psi",  "Pa"  },
        /* energia */
        { "cal",  "joule",  4.184,       "cal",  "J"   },
        { "joule","cal",    0.239006,    "J",    "cal" },
        { "kwh",  "joule",  3600000.0,   "kWh",  "J"   },
        /* potenza */
        { "watt", "cavallo",0.00134102,  "W",    "HP"  },
        { "cavallo","watt", 745.7,       "HP",   "W"   },
        /* angolo */
        { "gradi","radianti",0.0174533,  "deg",  "rad" },
        { "radianti","gradi",57.2958,    "rad",  "deg" },
    };

    const QString lo = text.toLower();

    /* Estrai numero dalla query (primo numero trovato) */
    static const QRegularExpression rxNum(R"([\-\+]?\d+[\.,]?\d*)");
    const auto mNum = rxNum.match(text);
    if (!mNum.hasMatch()) return {};   /* nessun numero → non è una conversione numerica */

    double val = mNum.captured(0).replace(',', '.').toDouble();

    for (const UnitEntry& u : kUnits) {
        if (!lo.contains(u.fromU) || !lo.contains(u.toU)) continue;

        /* Casi speciali temperatura */
        if (QString(u.fromU) == "celsius" && QString(u.toU) == "fahrenheit") {
            return QString(
                "v = %1\n"
                "r = v * 9/5 + 32\n"
                "print(f'{v} \xc2\xb0" "C  =  {r:.4f} \xc2\xb0" "F')\n"
            ).arg(val);
        }
        if (QString(u.fromU) == "fahrenheit" && QString(u.toU) == "celsius") {
            return QString(
                "v = %1\n"
                "r = (v - 32) * 5/9\n"
                "print(f'{v} \xc2\xb0" "F  =  {r:.4f} \xc2\xb0" "C')\n"
            ).arg(val);
        }
        if (QString(u.fromU) == "celsius" && QString(u.toU) == "kelvin") {
            return QString(
                "v = %1\n"
                "r = v + 273.15\n"
                "print(f'{v} \xc2\xb0" "C  =  {r:.4f} K')\n"
            ).arg(val);
        }
        if (QString(u.fromU) == "kelvin" && QString(u.toU) == "celsius") {
            return QString(
                "v = %1\n"
                "r = v - 273.15\n"
                "print(f'{v} K  =  {r:.4f} \xc2\xb0" "C')\n"
            ).arg(val);
        }

        return QString(
            "v = %1\n"
            "f = %2\n"
            "r = v * f\n"
            "print(f'{v} %3  =  {r:.6g} %4')\n"
            "print(f'(fattore di conversione: {f})')\n"
        ).arg(val).arg(u.factor).arg(u.labelIn).arg(u.labelOut);
    }

    return {};  /* unità non riconosciuta */
}

void MatematicaPage::fillMathCombo(const QStringList& list, const QString& cur)
{
    if (!m_modelCombo) return;
    m_modelCombo->clear();
    int selIdx    = 0;
    int curIdx    = -1;
    int bestMath  = -1;
    int bestScore = -1;
    for (const QString& mdl : list) {
        if (isEmbedModel(mdl.toLower())) continue;
        const int pos = m_modelCombo->count();
        const int sc  = mathModelScore(mdl);
        const QString badge = (sc >= 90) ? "  \xf0\x9f\xa7\xae Ottimizzato Math"
                            : (sc >= 50) ? "  \xe2\x9c\x94 Buono per Math"
                            :              "";
        const qint64 sz = m_ai ? m_ai->modelSizeBytes(mdl) : 0;
        m_modelCombo->addItem(P::modelIcon(sz, mdl) + mdl + badge, mdl);
        if (P::isKnownBrokenModel(mdl)) {
            m_modelCombo->setItemData(pos, QBrush(QColor("#ea580c")), Qt::ForegroundRole);
            m_modelCombo->setItemData(pos, QBrush(QColor("#fef08a")), Qt::BackgroundRole);
            m_modelCombo->setItemData(pos, P::knownBrokenModelTooltip(), Qt::ToolTipRole);
        }
        if (mdl == cur) curIdx = pos;
        if (sc > bestScore) { bestScore = sc; bestMath = pos; }
    }
    if (m_modelCombo->count() == 0) {
        m_modelCombo->addItem(cur.isEmpty() ? "(nessun modello)" : cur, cur);
        return;
    }
    const bool curIsMath = (curIdx >= 0 && mathModelScore(cur) >= 50);
    if (bestMath >= 0 && !curIsMath) selIdx = bestMath;
    else if (curIdx >= 0)            selIdx = curIdx;
    m_modelCombo->setCurrentIndex(selIdx);
    const QString chosen = m_modelCombo->currentData().toString();
    if (!chosen.isEmpty() && chosen != cur && m_ai)
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), chosen);
}

void MatematicaPage::fetchAndFillMathModels()
{
    if (!m_ai) return;
    const QString cur = m_ai->model();
    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, holder,
            [this, holder, cur](const QStringList& list) {
        holder->deleteLater();
        fillMathCombo(list, cur);
    });
    connect(m_ai, &AiClient::error, holder,
            [this, holder](const QString&) {
        holder->deleteLater();
        setStatus(tr("\xe2\x9d\x8c  Backend non raggiungibile \xe2\x80\x94"
                  " avvia Ollama o llama-server per le funzioni AI."));
        LogBus::post("\xe2\x9d\x8c Matematica: Backend non raggiungibile.");
        if (m_modelCombo) m_modelCombo->setToolTip(
            "Backend non disponibile. Avvia Ollama: ollama serve");
    });
    m_ai->fetchModels();
}

void MatematicaPage::onRefreshModelsClicked()
{
    fetchAndFillMathModels();
}

/* ══════════════════════════════════════════════════════════════
   onAdjustTabHeight — setFixedHeight basato sul sizeHint del tab corrente.
   Chiamato ad ogni cambio tab e al primo avvio (QTimer::singleShot).
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onAdjustTabHeight()
{
    if (!m_tabs) return;
    QWidget* page = m_tabs->currentWidget();
    if (!page) return;
    const int barH  = m_tabs->tabBar() ? m_tabs->tabBar()->height() : dpiScale(30);
    const int pageH = page->sizeHint().height();
    /* Margine minimo: evita scrollbar senza aggiungere spazio visibile */
    const int total = qBound(barH + dpiScale(80),
                              pageH + barH + dpiScale(2),
                              barH + dpiScale(420));   /* max per Analisi */
    m_tabs->setFixedHeight(total);
}

void MatematicaPage::onLoadModelsOnce()
{
    if (!m_ai) return;
    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, holder,
            [this, holder](const QStringList& list) {
        holder->deleteLater();
        fillMathCombo(list, m_ai ? m_ai->model() : QString());
    });
    connect(m_ai, &AiClient::error, holder,
            [this, holder](const QString&) {
        holder->deleteLater();
        setStatus(tr("\xe2\x9d\x8c  Backend non raggiungibile \xe2\x80\x94"
                  " le funzioni AI non sono disponibili."));
        LogBus::post("\xe2\x9d\x8c Matematica: Backend non raggiungibile (onRefresh).");
    });
    m_ai->fetchModels();
}

/* ══════════════════════════════════════════════════════════════
   Slot — tab Sequenza
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onLocalPatternClicked()
{
    QString err;
    QVector<double> seq = parseSeq(m_seqInput->text(), err);
    if (!err.isEmpty()) {
        m_seqResult->setText(tr("\xe2\x9d\x8c  ") + err);
        LogBus::post("\xe2\x9d\x8c Matematica: " + err);
        m_seqResult->setVisible(true);
        return;
    }
    const QString pat = detectPatternLocal(seq);
    m_seqResult->setText(tr("\xf0\x9f\x94\x8d  ") + pat);
    m_seqResult->setVisible(true);
}

void MatematicaPage::onSympyClicked()
{
    QString err;
    QVector<double> seq = parseSeq(m_seqInput->text(), err);
    if (!err.isEmpty()) { setStatus(tr("\xe2\x9d\x8c  ") + err); LogBus::post("\xe2\x9d\x8c Matematica: " + err); return; }
    if (seq.size() < 2) { setStatus(tr("\xe2\x9d\x8c  Inserisci almeno 2 termini.")); LogBus::post("\xe2\x9d\x8c Matematica: Inserisci almeno 2 termini."); return; }

    QString listStr = "[";
    for (int i = 0; i < seq.size(); ++i) {
        double v = seq[i];
        if (v == std::floor(v) && std::abs(v) < 1e15)
            listStr += QString::number((long long)v);
        else
            listStr += QString::number(v, 'g', 17);
        if (i < seq.size()-1) listStr += ", ";
    }
    listStr += "]";

    const int nxt = m_nextTerms->value();
    const QString pyCode = QString(
        "from sympy import symbols, interpolating_poly, factor, simplify, Integer, nsimplify\n"
        "from sympy import factorint, isprime, fibonacci as fib\n"
        "import sys\n"
        "seq = %1\n"
        "n = symbols('n')\n"
        "N = len(seq)\n"
        "print('Sequenza:', seq)\n"
        "print(f'Termini: {N}')\n"
        "print()\n"
        "try:\n"
        "    pts = list(enumerate(seq, 1))\n"
        "    poly = interpolating_poly(N, n, pts)\n"
        "    fpoly = factor(simplify(poly))\n"
        "    print('Formula polinomiale (interpolazione):')\n"
        "    print(f'  a(n) = {fpoly}')\n"
        "    print()\n"
        "    print('Termini successivi:')\n"
        "    for i in range(N+1, N+%2+1):\n"
        "        print(f'  a({i}) = {fpoly.subs(n, i)}')\n"
        "except Exception as e:\n"
        "    print(f'Interpolazione fallita: {e}')\n"
        "print()\n"
        "diffs = [seq[i+1]-seq[i] for i in range(len(seq)-1)]\n"
        "diffs2 = [diffs[i+1]-diffs[i] for i in range(len(diffs)-1)] if len(diffs)>1 else []\n"
        "print(f'Prime differenze:  {diffs}')\n"
        "if diffs2: print(f'Seconde differenze: {diffs2}')\n"
    ).arg(listStr).arg(nxt);

    clearOutput();
    appendOutput("\xcf\x83  Analisi sympy in corso...\n\n");
    runPython(pyCode);
}

void MatematicaPage::onAnalyzeAiClicked()
{
    QString err;
    QVector<double> seq = parseSeq(m_seqInput->text(), err);
    if (!err.isEmpty()) { setStatus(tr("\xe2\x9d\x8c  ") + err); LogBus::post("\xe2\x9d\x8c Matematica: " + err); return; }
    if (seq.size() < 2) { setStatus(tr("\xe2\x9d\x8c  Inserisci almeno 2 termini.")); LogBus::post("\xe2\x9d\x8c Matematica: Inserisci almeno 2 termini."); return; }
    runAiSequence(m_seqInput->text().trimmed(), m_nextTerms->value());
}

/* ══════════════════════════════════════════════════════════════
   Slot — tab Costanti
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onConstantCalcClicked()
{
    runConstant();
}

void MatematicaPage::onAllConstantsClicked()
{
    const QString py =
        "from mpmath import mp, pi, e, phi, sqrt, euler, log, catalan\n"
        "mp.dps = 110\n"
        "consts = [\n"
        "    ('\xcf\x80  pi', mp.pi),\n"
        "    ('e   numero di Eulero', mp.e),\n"
        "    ('\xcf\x86  sezione aurea', mp.phi),\n"
        "    ('\xe2\x88\x9a" "2  radice di 2', mp.sqrt(2)),\n"
        "    ('\xe2\x88\x9a" "3  radice di 3', mp.sqrt(3)),\n"
        "    ('\xce\xb3   Eulero-Mascheroni', mp.euler),\n"
        "    ('ln2 logaritmo naturale di 2', mp.log(2)),\n"
        "    ('C   costante di Catalan', mp.catalan),\n"
        "]\n"
        "for nome, val in consts:\n"
        "    s = mp.nstr(val, 100, strip_zeros=False)\n"
        "    print(f'{nome}')\n"
        "    print(f'  {s}')\n"
        "    print()\n";
    clearOutput();
    appendOutput("\xcf\x80  Calcolo costanti a 100 cifre...\n\n");
    runPython(py);
}

/* ══════════════════════════════════════════════════════════════
   Slot — tab N-esimo
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onNthTypeChanged()
{
    if (!m_nthDescLbl || !m_nthType) return;
    const QString k = m_nthType->currentData().toString();
    if (k == "pi_digit")
        m_nthDescLbl->setText(tr("Restituisce la N-esima cifra decimale di \xcf\x80 (dopo il punto). "
                         "Es. N=1 \xe2\x86\x92 1, N=2 \xe2\x86\x92 4, N=3 \xe2\x86\x92 1..."));
    else if (k == "e_digit")
        m_nthDescLbl->setText(tr("N-esima cifra decimale di e. Es. N=1 \xe2\x86\x92 7, N=2 \xe2\x86\x92 1..."));
    else if (k == "prime")
        m_nthDescLbl->setText(tr("Il primo con indice N. p(1)=2, p(2)=3, p(3)=5... "
                         "(sympy per N fino a ~10 000 000)"));
    else if (k == "fib")
        m_nthDescLbl->setText(tr("F(1)=1, F(2)=1, F(3)=2, F(4)=3, F(5)=5... "
                         "Anche per N molto grandi (mpmath)."));
    else if (k == "fact")
        m_nthDescLbl->setText(tr("N! \xe2\x80\x94 fattoriale. 1!=1, 5!=120, 100!=93326215443944..."
                         " (precisione arbitraria)."));
    else if (k == "pow2")
        m_nthDescLbl->setText(tr("2^N. Anche per N molto grandi (migliaia di cifre)."));
    else if (k == "pi_block")
        m_nthDescLbl->setText(tr("Le prime N cifre di \xcf\x80 come blocco continuo "
                         "(includa la parte intera: 3.14159...)."));
    else if (k == "phi_block")
        m_nthDescLbl->setText(tr("Le prime N cifre di \xcf\x86 (sezione aurea)."));
}

void MatematicaPage::onNthCalcClicked()
{
    runNth();
}

/* ══════════════════════════════════════════════════════════════
   Slot — tab Espressione
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onExampleClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const QString expr = btn->property("mathExpr").toString();
    if (expr.isEmpty()) return;
    m_exprInput->setText(expr);
    runExpr();
}

void MatematicaPage::onExprEvalClicked()
{
    runExpr();
}

void MatematicaPage::onSimplifyClicked()
{
    const QString expr = m_exprInput->text().trimmed();
    if (expr.isEmpty()) return;
    const int prec = m_exprPrec->value();
    const QString py = QString(
        "from sympy import *\n"
        "from mpmath import mp\n"
        "mp.dps = %1\n"
        "x = symbols('x')\n"
        "expr = %2\n"
        "print('Espressione:  ', expr)\n"
        "print('Semplificata: ', simplify(expr))\n"
        "print('Fattorizzata: ', factor(expr))\n"
        "try:\n"
        "    print('Valore numerico:', N(expr, %1))\n"
        "except: pass\n"
    ).arg(prec).arg(expr);
    clearOutput();
    runPython(py);
}

void MatematicaPage::onExprReturnPressed()
{
    runExpr();
}

/* ══════════════════════════════════════════════════════════════
   Slot — AI sequenza (one-shot holder)
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onAiSeqToken(const QString& tok)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(tok);
    m_output->ensureCursorVisible();
}

void MatematicaPage::onAiSeqFinished(const QString& /*full*/)
{
    delete m_aiSeqHolder;
    m_aiSeqHolder = nullptr;
    m_aiRunning = false;
    setStatus(tr("\xe2\x9c\x85  Analisi AI completata."));
}

void MatematicaPage::onAiSeqError(const QString& msg)
{
    delete m_aiSeqHolder;
    m_aiSeqHolder = nullptr;
    m_aiRunning = false;
    appendOutput("\n\xe2\x9d\x8c  Errore AI: " + msg);
    setStatus(tr("\xe2\x9d\x8c  Errore AI."));
    LogBus::post("\xe2\x9d\x8c Matematica: Errore AI sequenza: " + msg);
}

/* ══════════════════════════════════════════════════════════════
   formatMathOutput — converte notazione SymPy ASCII in Unicode
   "da foglio di carta": x**2→x², * →·, sqrt(→√(, oo→∞ …
   ══════════════════════════════════════════════════════════════ */
static QString toSuperscript(const QString& digits)
{
    static const char* const sup[10] = {
        "\xe2\x81\xb0",  /* ⁰ */
        "\xc2\xb9",      /* ¹ */
        "\xc2\xb2",      /* ² */
        "\xc2\xb3",      /* ³ */
        "\xe2\x81\xb4",  /* ⁴ */
        "\xe2\x81\xb5",  /* ⁵ */
        "\xe2\x81\xb6",  /* ⁶ */
        "\xe2\x81\xb7",  /* ⁷ */
        "\xe2\x81\xb8",  /* ⁸ */
        "\xe2\x81\xb9",  /* ⁹ */
    };
    QString r;
    r.reserve(digits.size() * 3);
    for (const QChar c : digits) {
        const int d = c.digitValue();
        r += (d >= 0 && d <= 9) ? QString::fromUtf8(sup[d]) : QString(c);
    }
    return r;
}

static QString formatMathOutput(const QString& raw)
{
    /* 1 — **N  →  esponente Unicode  (x**2 → x², x**10 → x¹⁰) */
    static const QRegularExpression rePow(R"(\*\*(-?\d+))");
    QString s;
    s.reserve(raw.size());
    int pos = 0;
    QRegularExpressionMatchIterator it = rePow.globalMatch(raw);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        s += raw.mid(pos, m.capturedStart() - pos);
        const QString exp = m.captured(1);
        if (exp.startsWith('-'))
            s += "\xe2\x81\xbb" + toSuperscript(exp.mid(1));  /* ⁻ + cifre */
        else
            s += toSuperscript(exp);
        pos = m.capturedEnd();
    }
    s += raw.mid(pos);

    /* 2 — * rimasto  →  · (punto di mezzo, notazione europea) */
    s.replace(QLatin1Char('*'), "\xc2\xb7");

    /* 3 — oo isolato  →  ∞ */
    s.replace(QRegularExpression(R"(\boo\b)"), "\xe2\x88\x9e");

    /* 4 — sqrt(  →  √( */
    s.replace("sqrt(", "\xe2\x88\x9a(");

    /* 5 — ->  →  → */
    s.replace("->", "\xe2\x86\x92");

    /* 6 — <= ≤  e  >= ≥ */
    s.replace(">=", "\xe2\x89\xa5");
    s.replace("<=", "\xe2\x89\xa4");

    /* 7 — pi isolato  →  π */
    s.replace(QRegularExpression(R"(\bpi\b)"), "\xcf\x80");

    /* 8 — ^N (notazione utente) → esponente Unicode */
    static const QRegularExpression reCaret(R"(\^(-?\d+))");
    QString s2;
    s2.reserve(s.size());
    int pos2 = 0;
    QRegularExpressionMatchIterator it2 = reCaret.globalMatch(s);
    while (it2.hasNext()) {
        const QRegularExpressionMatch m2 = it2.next();
        s2 += s.mid(pos2, m2.capturedStart() - pos2);
        const QString exp2 = m2.captured(1);
        if (exp2.startsWith('-'))
            s2 += "\xe2\x81\xbb" + toSuperscript(exp2.mid(1));
        else
            s2 += toSuperscript(exp2);
        pos2 = m2.capturedEnd();
    }
    s2 += s.mid(pos2);

    return s2;
}

/* ══════════════════════════════════════════════════════════════
   Slot — QProcess Python
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onProcReadyRead()
{
    if (!m_proc) return;
    const QString raw = QString::fromUtf8(m_proc->readAllStandardOutput());
    const QString txt = formatMathOutput(raw);
    appendOutput(txt);
    if (m_solvePyMode)
        m_solveFullText += txt;   /* cattura per "Spiega con AI" */
}

void MatematicaPage::onProcFinished(int code, QProcess::ExitStatus /*status*/)
{
    if (m_solvePyMode) {
        m_solvePyMode = false;
        m_solveBusy   = false;
        if (m_btnSolve)   m_btnSolve->setEnabled(true);
        if (m_btnSolveAi) m_btnSolveAi->setVisible(true);
        setStatus(code == 0 ? "\xe2\x9c\x85  SymPy completato." : "\xe2\x9d\x8c  Errore nel calcolo SymPy.");
        if (code != 0) LogBus::post("\xe2\x9d\x8c Matematica: Errore nel calcolo SymPy.");

        /* ── Aggiorna il marcatore L sul canvas del limite ─────────────── */
        if (m_limitCanvas && code == 0) {
            /* Pattern: "lim[var→val] f = RISULTATO" nell'ultima riga SOLUZIONE FINALE */
            static const QRegularExpression reLim(
                R"(lim\[.+?\]\s+.+?=\s*(.+)$)",
                QRegularExpression::MultilineOption);
            const auto match = reLim.match(m_solveFullText);
            if (match.hasMatch()) {
                bool ok = false;
                const double L = match.captured(1).trimmed().toDouble(&ok);
                if (ok) m_limitCanvas->updateLimitValue(L);
            }
        }
    } else {
        setStatus(code != 0 ? QString("\xe2\x9d\x8c  Python uscito con codice %1.").arg(code)
                            : "\xe2\x9c\x85  Calcolo completato.");
        if (code != 0) LogBus::post(QString("\xe2\x9d\x8c Matematica: Python uscito con codice %1.").arg(code));
    }
    if (m_proc) {
        m_proc->deleteLater();
        m_proc = nullptr;
    }
}


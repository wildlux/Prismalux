#include "main_math.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../ai_client.h"

#include "main_graph.h"
#include "../widgets/formula_parser.h"
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

    auto* modelLbl = new QLabel("\xf0\x9f\xa4\x96  Modello AI:", modelBar);  /* 🤖 */
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
    connect(m_tabs, &QTabWidget::currentChanged,
            this, [this](int) { onAdjustTabHeight(); });
    m_tabs->addTab(buildSeqTab(),   "\xf0\x9f\x94\xa2  Sequenza \xe2\x86\x92 Formula");  /* 🔢 */
    m_tabs->addTab(buildConstTab(), "\xcf\x80  Costanti di precisione");
    m_tabs->addTab(buildNthTab(),   "#\xe2\x83\xbf  N-esimo");
    m_tabs->addTab(buildExprTab(),  "\xf0\x9f\xa7\xae  Espressione");                    /* 🧮 */
    m_tabs->addTab(buildSolveTab(), "\xf0\x9f\x93\x90  Risolvi Passi");                 /* 📐 */
    m_solveTabIdx = m_tabs->count() - 1;
    m_tabs->addTab(buildAnalisi1Tab(), "\xf0\x9f\x93\x98  Analisi 1");                  /* 📘 */
    m_tabs->addTab(buildAnalisi2Tab(), "\xf0\x9f\x93\x99  Analisi 2");                  /* 📙 */
    root->addWidget(m_tabs);   /* stretch=0: prende solo lo spazio che gli serve */

    /* ─── Output (prende tutto lo spazio restante) ─── */
    auto* outBox = new QWidget(this);
    auto* outLay = new QVBoxLayout(outBox);
    outLay->setContentsMargins(8, 4, 8, 8);
    outLay->setSpacing(4);

    /* Barra stato + pulsanti */
    auto* ctrlRow = new QHBoxLayout;
    m_status = new QLabel("\xf0\x9f\x93\x90  Pronto.", outBox);
    m_status->setObjectName("statusLabel");
    ctrlRow->addWidget(m_status, 1);

    auto* btnCopy = new QPushButton("\xf0\x9f\x93\x8b  Copia", outBox);
    btnCopy->setObjectName("actionBtn");
    btnCopy->setFixedHeight(26);
    btnCopy->setToolTip(tr("Copia tutto l'output negli appunti"));
    connect(btnCopy, &QPushButton::clicked, this, &MatematicaPage::onCopyClicked);
    ctrlRow->addWidget(btnCopy);

    auto* btnClear = new QPushButton("\xf0\x9f\x97\x91  Cancella", outBox);
    btnClear->setObjectName("actionBtn");
    btnClear->setFixedHeight(26);
    connect(btnClear, &QPushButton::clicked, this, &MatematicaPage::onClearOutputClicked);
    ctrlRow->addWidget(btnClear);

    auto* btnStop = new QPushButton("\xe2\x96\xa0  Stop", outBox);
    btnStop->setObjectName("stopBtn");
    btnStop->setFixedHeight(26);
    btnStop->setProperty("execFull", btnStop->text());
    btnStop->setProperty("execIcon", QString::fromUtf8("\xe2\x96\xa0"));
    btnStop->setProperty("execText", "Stop");
    connect(btnStop, &QPushButton::clicked, this, &MatematicaPage::onStopClicked);
    ctrlRow->addWidget(btnStop);

    auto* btnLatex = new QPushButton("\xf0\x9f\x94\xac LaTeX", outBox);  /* 🔬 */
    btnLatex->setObjectName("navBtn");
    btnLatex->setFixedHeight(26);
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

    auto* btnFile = new QPushButton("\xf0\x9f\x93\x82  Apri file", w);  /* 📂 */
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
    optRow->addWidget(new QLabel("Suggerisci i prossimi", w));
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

    grid->addWidget(new QLabel("Costante:", w), 0, 0);
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

    grid->addWidget(new QLabel("Cifre decimali:", w), 1, 0);
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

    auto* btnAll = new QPushButton("Tutte le costanti (100 cifre)", w);
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

    grid->addWidget(new QLabel("Tipo:", w), 0, 0);
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

    grid->addWidget(new QLabel("N =", w), 1, 0);
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
    auto* btnCalc = new QPushButton("#\xe2\x83\xbf  Calcola", w);
    btnCalc->setObjectName("actionBtn");
    btnCalc->setProperty("highlight", "true");
    btnCalc->setProperty("execFull", btnCalc->text());
    btnCalc->setProperty("execIcon", QString::fromUtf8("#\xe2\x83\xbf"));
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
    optRow->addWidget(new QLabel("Cifre di precisione:", w));
    m_exprPrec = new QSpinBox(w);
    m_exprPrec->setRange(10, 10000);
    m_exprPrec->setValue(50);
    m_exprPrec->setSuffix("  cifre");
    optRow->addWidget(m_exprPrec);
    optRow->addStretch(1);
    lay->addLayout(optRow);

    /* Esempi rapidi */
    auto* exGroup = new QGroupBox("Esempi rapidi", w);
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
        btn->setFixedHeight(24);
        btn->setProperty("mathExpr", QString::fromUtf8(ex.expr));
        connect(btn, &QPushButton::clicked, this, &MatematicaPage::onExampleClicked);
        exGrid->addWidget(btn, r, c);
        if (++c == 4) { c = 0; ++r; }
    }
    lay->addWidget(exGroup);

    auto* btnRow = new QHBoxLayout;
    auto* btnEval = new QPushButton("\xf0\x9f\xa7\xae  Calcola", w);
    btnEval->setObjectName("actionBtn");
    btnEval->setProperty("highlight", "true");
    btnEval->setProperty("execFull", btnEval->text());
    btnEval->setProperty("execIcon", QString::fromUtf8("\xf0\x9f\xa7\xae"));
    btnEval->setProperty("execText", "Calcola");
    connect(btnEval, &QPushButton::clicked, this, &MatematicaPage::onExprEvalClicked);
    btnRow->addWidget(btnEval);

    auto* btnSimplify = new QPushButton("\xe2\x99\xbe  Semplifica (sympy)", w);
    btnSimplify->setObjectName("actionBtn");
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
        setStatus("\xe2\x9d\x8c  Inserisci un numero intero positivo per N.");
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
    appendOutput(QString("#\xe2\x83\xbf  Calcolo in corso (N=%1)...\n\n").arg(N));
    runPython(py);
}

/* Forward declarations degli helper definiti più avanti nel file */
static int     mathModelScore(const QString& name);
static QString buildDomainHint(AiClient*, const QString&, const QComboBox*);
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

    setStatus("\xf0\x9f\xa4\x96  AI in analisi...");

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

    setStatus("\xe2\x8f\xb3  Calcolo in corso...");
    m_proc->start(P::findPython(), QStringList{"-c", code});
    if (!m_proc->waitForStarted(3000)) {
        setStatus("\xe2\x9d\x8c  Python non trovato nel PATH. Installa Python da python.org");
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
        setStatus("\xe2\x96\xa0  Calcolo interrotto.");
    }
}

/* ══════════════════════════════════════════════════════════════
   buildSympyScript — genera lo script Python/SymPy per ogni tipo
   ══════════════════════════════════════════════════════════════ */
static QString buildSympyScript(const QString& tipo, const QString& expr)
{
    /* Escape dell'espressione utente per inniezione sicura nel Python */
    QString safe = expr;
    safe.replace("\\", "\\\\").replace("'", "\\'").replace("\n", " ").replace("\r", "");

    /* Preambolo comune */
    QString py = QString(R"SCRIPT(
import sys, re, platform
import sympy as _sympy_mod
from sympy import *
import re  # ripristina: from sympy import * sovrascrive re con sympy.re()
from sympy.parsing.sympy_parser import (
    parse_expr, standard_transformations,
    implicit_multiplication_application, convert_xor)
x,y,z,n,t,a,b,c,k = symbols('x y z n t a b c k')
TR = standard_transformations + (implicit_multiplication_application, convert_xor)
LD = {'x':x,'y':y,'z':z,'n':n,'t':t,'a':a,'b':b,'c':c,'k':k,
      'pi':pi,'e':E,'I':I,'oo':oo,'inf':oo,
      'sqrt':sqrt,'sin':sin,'cos':cos,'tan':tan,
      'asin':asin,'acos':acos,'atan':atan,
      'log':log,'ln':log,'exp':exp,'abs':Abs}
LINE = '─'*56
def parse(s):
    return parse_expr(s.strip(), transformations=TR, local_dict=LD)
def pprint_inline(expr):
    """Restituisce una stringa 'da foglio': usa pretty() su una riga se possibile."""
    try:
        p = str(pretty(expr, use_unicode=True))
        # se pretty è multiriga prende solo la prima riga di una repr compatta
        lines = [l for l in p.splitlines() if l.strip()]
        if len(lines) == 1:
            return lines[0].strip()
    except:
        pass
    return str(expr)
def fmt(v):
    try:
        ev = complex(v.evalf(8))
        if abs(ev.imag) < 1e-7:
            f = ev.real
            base = pprint_inline(v)
            return base if abs(f - round(f)) < 1e-9 else f'{base}  ≈  {f:.6g}'
        else:
            return f'{pprint_inline(v)}  ≈  {ev.real:.5g} {ev.imag:+.5g}i'
    except:
        return pprint_inline(v)
print(LINE)
print('  Risolvi Passi — SymPy Engine')
print(f'  Python {platform.python_version()}  |  SymPy {_sympy_mod.__version__}')
print('  Motore: calcolo simbolico esatto (nessun LLM)')
print(LINE)
print()
)SCRIPT");

    py += QString("expr_str = '%1'\n").arg(safe);
    py += "print(f'Problema: {expr_str}')\nprint()\n";
    py += "try:\n";

    if (tipo == "Equazione") {
        py += R"SCRIPT(
    if '=' in expr_str:
        parts = expr_str.split('=', 1)
        lhs = parse(parts[0]); rhs = parse(parts[1])
    else:
        lhs = parse(expr_str); rhs = S.Zero
    residual = lhs - rhs
    free = residual.free_symbols
    var = x if x in free else (next(iter(free)) if free else x)

    print(f'PASSO 1 — Riscrittura in f({var})=0')
    print(f'  {lhs} = {rhs}  →  {residual} = 0')
    print(f'  Variabile: {var}')
    print(f'  ↳ Perché: portare tutto a sinistra crea la forma canonica f({var})=0,')
    print(f'    necessaria per applicare i teoremi di esistenza delle radici e gli')
    print(f'    algoritmi di risoluzione simbolica.\n')

    exp2 = expand(residual)
    if str(exp2) != str(residual):
        print(f'PASSO 2 — Espansione')
        print(f'  {exp2} = 0')
        print(f'  ↳ Perché: distribuire prodotti e potenze rivela i termini simili,')
        print(f'    rende visibile il grado e prepara alla fattorizzazione.\n')

    fac = factor(residual)
    if str(fac) not in (str(residual), str(exp2)):
        print(f'PASSO 3 — Fattorizzazione')
        print(f'  {fac} = 0')
        print(f'  ↳ Perché: scrivere f come prodotto di fattori lineari/irriducibili')
        print(f'    permette di leggere le radici direttamente (ogni fattore = 0).\n')

    if residual.is_polynomial(var):
        p = Poly(residual, var)
        deg = p.degree()
        print(f'PASSO 4 — Analisi polinomiale')
        print(f'  Grado: {deg}   Coefficienti [a_n…a_0]: {p.all_coeffs()}')
        print(f'  ↳ Perché: il grado stabilisce il numero massimo di soluzioni')
        print(f'    (Teorema Fondamentale dell\'Algebra: n radici contando molteplicità).')
        if deg == 2:
            cf = p.all_coeffs()
            a2 = Rational(cf[0]); b2 = Rational(cf[1]); c2 = Rational(cf[2])
            disc = b2**2 - 4*a2*c2
            print(f'  Discriminante Δ = ({b2})² - 4·({a2})·({c2}) = {disc}')
            if disc > 0:
                print(f'  ↳ Δ>0: due radici reali distinte (formula quadratica).')
            elif disc == 0:
                print(f'  ↳ Δ=0: radice doppia (tangente all\'asse x).')
            else:
                print(f'  ↳ Δ<0: nessuna radice reale (le soluzioni sono complesse coniugate).')
        print()

    print('PASSO 5 — Calcolo soluzioni')
    print('  ↳ Perché: SymPy applica formule chiuse (quadratica, cubica, quartica)')
    print('    se esistono; altrimenti usa algoritmi numerici (Newton-Raphson).')
    sols = solve(Eq(lhs, rhs), var) or solve(residual, var)
    if sols:
        print(f'  Soluzioni trovate: {len(sols)}')
        for i,s in enumerate(sols,1):
            print(f'    {var}_{i} = {fmt(s)}')
    else:
        print('  Nessuna soluzione simbolica. Ricerca numerica:')
        found = set()
        for x0 in [0,1,-1,2,-2,5,-5,10,-10,0.5,-0.5]:
            try:
                s = nsolve(residual, var, x0, tol=1e-8, verify=False)
                sv = round(float(s), 6)
                if all(abs(sv-f) > 1e-4 for f in found):
                    found.add(sv); print(f'    Radice ≈ {sv}  (vicino a {x0})')
            except: pass
        if not found: print('    Nessuna radice reale trovata.')
    print()
    print(LINE); print('SOLUZIONE FINALE:')
    for i,s in enumerate(sols or [],1): print(f'  {var}_{i} = {s}')
    if not sols: print('  (vedi ricerca numerica sopra)')
    print(LINE)
)SCRIPT";

    } else if (tipo == "Disequazione") {
        py += R"SCRIPT(
    op_m = re.search(r'(<=|>=|<|>)', expr_str)
    if not op_m:
        print('Errore: inserisci un operatore < > <= >=', file=sys.stderr); sys.exit(1)
    op = op_m.group(1)
    parts = re.split(r'<=|>=|<|>', expr_str, 1)
    lhs = parse(parts[0]); rhs = parse(parts[1])
    residual = lhs - rhs
    free = residual.free_symbols
    var = x if x in free else (next(iter(free)) if free else x)

    print(f'PASSO 1 — Riscrittura in forma standard')
    print(f'  {lhs} {op} {rhs}  →  {residual} {op} 0')
    print(f'  ↳ Perché: portare tutto a sinistra riduce il problema allo studio')
    print(f'    del segno di una sola espressione, tecnica fondamentale per')
    print(f'    le disequazioni algebriche e trascendenti.\n')

    fac = factor(residual)
    if str(fac) != str(residual):
        print(f'PASSO 2 — Fattorizzazione')
        print(f'  {fac} {op} 0')
        print(f'  ↳ Perché: un prodotto di fattori cambia segno solo negli zeri di')
        print(f'    ciascun fattore; la tabella dei segni si costruisce fattore per')
        print(f'    fattore (metodo degli intervalli).\n')

    print('PASSO 3 — Studio del segno e insieme soluzione')
    zeros = solve(residual, var)
    if zeros:
        print(f'  Zeri di f({var}): {zeros}')
        print(f'  ↳ Gli zeri sono i punti di confine degli intervalli di soluzione.')
    from sympy.solvers.inequalities import solve_univariate_inequality
    op_map = {'<': lhs < rhs, '>': lhs > rhs, '<=': lhs <= rhs, '>=': lhs >= rhs}
    result = solve_univariate_inequality(op_map[op], var, relational=False)
    print(f'  Insieme soluzione: {result}')
    print(f'  ↳ SymPy unisce gli intervalli dove f({var}) ha il segno richiesto')
    print(f'    (positivo per ">", negativo per "<", incluso gli zeri per "≥","≤").\n')
    print(LINE); print(f'SOLUZIONE FINALE:\n  {var} ∈ {result}'); print(LINE)
)SCRIPT";

    } else if (tipo == "Derivata") {
        py += R"SCRIPT(
    parts = [p.strip() for p in expr_str.split(',')]
    f_str = parts[0]
    var_str = parts[1].strip() if len(parts) > 1 else 'x'
    order = int(parts[2].strip()) if len(parts) > 2 else 1
    f = parse(f_str)
    var = parse(var_str)

    print(f'PASSO 1 — Riconoscimento della struttura')
    print(f'  f({var}) = {f}')
    print(f'  ↳ Perché: identificare se la funzione è una somma, prodotto,')
    print(f'    quoziente o composizione determina quale regola di derivazione')
    print(f'    applicare (linearità, Leibniz, regola della catena).\n')

    fs = simplify(f)
    if str(fs) != str(f):
        print(f'PASSO 2 — Pre-semplificazione')
        print(f'  f({var}) = {fs}')
        print(f'  ↳ Perché: una forma più semplice riduce il numero di termini')
        print(f'    su cui applicare le regole e limita gli errori algebrici.\n')

    df = diff(f, var, order)
    label = f"{''.join([\"'\"]*order)}"
    ord_name = 'prima' if order==1 else (str(order)+'ª')
    print(f'PASSO 3 — Derivata {ord_name} rispetto a {var}')
    print(f'  f{label}({var}) = {df}')
    rules = []
    if f.is_Add: rules.append('regola della somma (linearità)')
    if f.is_Mul: rules.append('regola del prodotto (Leibniz)')
    if f.is_Pow: rules.append('regola della potenza')
    if f.has(sin,cos,tan,exp,log): rules.append('derivata di funzioni elementari')
    rule_str = ', '.join(rules) if rules else 'regole standard di derivazione'
    print(f'  ↳ Applica: {rule_str}.')
    print(f'    La derivata misura il tasso di variazione istantaneo di f in {var}.\n')

    dfs = simplify(df)
    if str(dfs) != str(df):
        print(f'PASSO 4 — Semplificazione della derivata')
        print(f'  f{label}({var}) = {dfs}')
        print(f'  ↳ Perché: la forma semplificata è più leggibile e più utile')
        print(f'    per trovare zeri, segno e comportamento asintotico.\n')
        df = dfs

    if order == 1:
        cps = solve(df, var)
        if cps:
            print(f'PASSO 5 — Punti critici (f\'({var})=0)')
            print(f'  ↳ Perché: dove la derivata si annulla la funzione ha')
            print(f'    tangente orizzontale → candidati a massimo, minimo o flesso.')
            for i,cp in enumerate(cps,1):
                try: yv = f.subs(var,cp)
                except: yv = '?'
                d2 = diff(f, var, 2).subs(var, cp) if cps else None
                nature = ''
                try:
                    if d2 is not None:
                        if d2 > 0: nature = ' → minimo locale (f\'\'> 0)'
                        elif d2 < 0: nature = ' → massimo locale (f\'\'< 0)'
                        else: nature = ' → analisi ordine superiore necessaria'
                except: pass
                print(f'  {var}_{i} = {fmt(cp)},  f = {yv}{nature}')
            print()

    print(LINE); print(f'SOLUZIONE FINALE:\n  f{label}({var}) = {df}'); print(LINE)
)SCRIPT";

    } else if (tipo == "Integrale") {
        py += R"SCRIPT(
    parts = [p.strip() for p in expr_str.split(',')]
    f = parse(parts[0])
    free = f.free_symbols
    if len(parts) == 1:
        var = x if x in free else (next(iter(free)) if free else x)
        a_val = b_val = None
    elif len(parts) == 2:
        var = parse(parts[1]); a_val = b_val = None
    else:
        var = x if x in free else (next(iter(free)) if free else x)
        a_val = parse(parts[1]); b_val = parse(parts[2])

    if a_val is None:
        print(f'PASSO 1 — Identificazione: integrale indefinito')
        print(f'  ∫ {f} d{var}')
        print(f'  ↳ Perché: cerchiamo una funzione F tale che F\'({var})=f({var}).')
        print(f'    La costante +C riflette il fatto che infinite primitive differiscono')
        print(f'    per una costante additiva (famiglie di curve parallele).\n')
        result = integrate(f, var)
        rs = simplify(result)
        # Identifica la tecnica usata
        tech = 'integrazione diretta (tabelle standard)'
        if f.has(exp): tech = 'integrale di funzione esponenziale'
        elif f.has(log): tech = 'integrazione per parti (possibile)'
        elif f.is_polynomial(var): tech = 'regola della potenza ∫xⁿdx = xⁿ⁺¹/(n+1)'
        elif f.has(sin) or f.has(cos): tech = 'integrale di funzione trigonometrica'
        print(f'PASSO 2 — Calcolo della primitiva')
        print(f'  F({var}) = {rs}')
        print(f'  ↳ Tecnica: {tech}.')
        print(f'    SymPy usa heuristic integration, Risch algorithm o integrazione')
        print(f'    per parti/sostituzione a seconda della struttura dell\'integrando.\n')
        print(LINE); print(f'SOLUZIONE FINALE:\n  ∫ {f} d{var} = {rs} + C'); print(LINE)
    else:
        print(f'PASSO 1 — Identificazione: integrale definito')
        print(f'  ∫[{a_val}…{b_val}] {f} d{var}')
        print(f'  ↳ Perché: l\'integrale definito misura l\'area algebrica (con segno)')
        print(f'    sotto la curva f({var}) tra {a_val} e {b_val}.\n')
        indef = integrate(f, var)
        rs = simplify(indef)
        print(f'PASSO 2 — Primitiva F({var})')
        print(f'  F({var}) = {rs}')
        print(f'  ↳ Perché: trovare la primitiva è il passo obbligatorio prima di')
        print(f'    applicare il Teorema Fondamentale del Calcolo Integrale.\n')
        fa = rs.subs(var, b_val); fb = rs.subs(var, a_val)
        result = simplify(fa - fb)
        print(f'PASSO 3 — Teorema Fondamentale del Calcolo (Newton-Leibniz)')
        print(f'  F({b_val}) − F({a_val}) = ({fa}) − ({fb})')
        print(f'  = {result}')
        print(f'  ↳ Perché: il Teorema Fondamentale afferma ∫[a,b]f = F(b)−F(a),')
        print(f'    collegando integrazione e derivazione (operazioni inverse).')
        print(f'    Questo evita di calcolare la somma di Riemann con infiniti rettangoli.\n')
        print(LINE); print(f'SOLUZIONE FINALE:\n  ∫ = {result}  ({fmt(result)})'); print(LINE)
)SCRIPT";

    } else if (tipo == "Limite") {
        py += R"SCRIPT(
    # formati: "f, var, val[, dir]" oppure "f as var->val" oppure "f per var->val"
    m = re.match(r'(.+?)\s+(?:as|per|when)\s+(\w+)\s*->\s*(.+)', expr_str)
    if m:
        f_str, var_str, val_str, dir_str = m.group(1), m.group(2), m.group(3), '+-'
    else:
        parts = [p.strip() for p in expr_str.split(',')]
        f_str = parts[0]
        free = parse(f_str).free_symbols
        var_str = parts[1] if len(parts)>1 else (str(x) if x in free else (str(next(iter(free))) if free else 'x'))
        val_str = parts[2] if len(parts)>2 else '0'
        dir_str = parts[3] if len(parts)>3 else '+-'

    f = parse(f_str); var = parse(var_str); val = parse(val_str)
    dir_str = dir_str.strip()
    dir_label = '' if dir_str == '+-' else f' ({dir_str})'

    print(f'PASSO 1 — Analisi del problema')
    print(f'  lim[{var}→{val}{dir_label}]  {f}')
    print(f'  ↳ Perché: il limite descrive il comportamento di f({var}) quando {var} si')
    print(f'    avvicina a {val}, anche se f non è definita o continua in quel punto.')
    if dir_str in ('+','-'):
        side = 'destra' if dir_str=='+' else 'sinistra'
        print(f'    Il limite laterale ({side}) si usa quando la funzione ha un')
        print(f'    comportamento diverso a seconda da che lato si approccia {val}.')
    print()

    determined = False
    try:
        direct = f.subs(var, val)
        # Solo confronti 'is' — .has() e 'not in (...)' innescano
        # il bug SymPy 1.14 / Python 3.14: 'tuple has no attribute matches'
        _bad = (direct is zoo or direct is nan or
                direct is oo  or direct is -oo)
        if not _bad:
            # verifica ulteriore: deve essere un valore finito valutabile
            try:
                ev = float(direct.evalf(6))
                if -1e15 < ev < 1e15:
                    print(f'PASSO 2 — Sostituzione diretta (continuità)')
                    print(f'  f({val}) = {fmt(direct)}')
                    print(f'  ↳ Perché: se f è continua in {val}, il limite coincide con il valore')
                    print(f'    della funzione. La sostituzione diretta è il metodo più rapido.')
                    print(f'    Se avesse dato 0/0, ∞/∞ ecc. sarebbe stata necessaria l\'analisi')
                    print(f'    delle forme indeterminate (L\'Hôpital, sviluppi di Taylor).\n')
                    determined = True
            except: pass
    except: pass

    print(f'PASSO 3 — Calcolo limite simbolico')
    if not determined:
        print(f'  ↳ Perché: la sostituzione diretta ha generato una forma indeterminata.')
        print(f'    SymPy usa algebricamente: semplificazione razionale, sviluppi')
        print(f'    di Taylor/McLaurin, regola di L\'Hôpital o sostituzione trigonometrica.')
    if dir_str in ('+', '-'):
        result = limit(f, var, val, dir_str)
    else:
        result = limit(f, var, val)
    print(f'  lim[{var}→{val}{dir_label}] {f} = {result}')
    try:
        _inf = result is oo or result is -oo or result is zoo or result is nan
        if not _inf:
            print(f'  Valore numerico: {fmt(result)}')
        elif result is oo:
            print(f'  ↳ Il limite è +∞: la funzione diverge (cresce senza limite).')
        elif result is -oo:
            print(f'  ↳ Il limite è -∞: la funzione diverge negativamente.')
        elif result is zoo:
            print(f'  ↳ Il limite è ∞ complesso: la funzione oscilla o diverge.')
        elif result is nan:
            print(f'  ↳ Forma indeterminata irrisolvibile simbolicamente.')
    except: pass
    print()
    print(LINE); print(f'SOLUZIONE FINALE:\n  lim[{var}→{val}{dir_label}] {f} = {result}'); print(LINE)
)SCRIPT";

    } else { /* Semplificazione */
        py += R"SCRIPT(
    f = parse(expr_str)
    print(f'PASSO 1 — Forma originale di partenza')
    print(f'  {f}')
    print(f'  ↳ Perché: registrare la forma iniziale permette di confrontare')
    print(f'    le trasformazioni successive e verificare l\'equivalenza algebrica.\n')
    results = []

    exp2 = expand(f)
    if str(exp2) != str(f):
        print(f'PASSO 2 — Espansione')
        print(f'  {exp2}')
        print(f'  ↳ Perché: distribuire prodotti e binomi rivela i termini simili,')
        print(f'    utile per semplificare frazioni algebriche e trovare i coefficienti.\n')
        results.append(exp2)

    fac = factor(f)
    if str(fac) not in (str(f), str(exp2)):
        print(f'PASSO 3 — Fattorizzazione')
        print(f'  {fac}')
        print(f'  ↳ Perché: la forma fattorizzata evidenzia le radici e semplifica')
        print(f'    frazioni (cancellando fattori comuni al numeratore/denominatore).\n')
        results.append(fac)

    simp = simplify(f)
    if str(simp) not in [str(r) for r in [f,exp2,fac]]:
        print(f'PASSO 4 — Simplificazione generale (simplify)')
        print(f'  {simp}')
        print(f'  ↳ Perché: simplify() prova internamente expand, factor, trigsimp,')
        print(f'    radsimp e altre strategie, scegliendo la forma con meno operazioni.\n')
        results.append(simp)

    ts = trigsimp(f)
    if str(ts) not in [str(r) for r in [f,exp2,fac,simp]]:
        print(f'PASSO 5 — Semplificazione trigonometrica (trigsimp)')
        print(f'  {ts}')
        print(f'  ↳ Perché: usa identità fondamentali (sin²+cos²=1, formule di')
        print(f'    addizione, ecc.) per ridurre espressioni trigonometriche.\n')
        results.append(ts)

    ps = powsimp(f, deep=True)
    if str(ps) not in [str(r) for r in [f,exp2,fac,simp,ts]]:
        print(f'PASSO 6 — Semplificazione di potenze (powsimp)')
        print(f'  {ps}')
        print(f'  ↳ Perché: raccoglie xᵃ·xᵇ = xᵃ⁺ᵇ e (xᵃ)ᵇ = xᵃᵇ, riducendo')
        print(f'    il numero di operazioni aritmetiche nella forma finale.\n')
        results.append(ps)

    best = min([f]+results, key=lambda v: len(str(v)))
    print(LINE); print(f'SOLUZIONE FINALE (forma più compatta):\n  {best}'); print(LINE)
)SCRIPT";
    }

    py += R"SCRIPT(
except Exception as e:
    print(f'Errore SymPy: {e}', file=sys.stderr)
    sys.exit(1)
)SCRIPT";

    return py;
}

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
        if (!proc.waitForStarted(3000)) {
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
            if (!proc.waitForStarted(3000)) {
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
    if (!proc.waitForStarted(3000)) {
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
        setStatus("\xe2\x9d\x8c  " + err);
        LogBus::post("\xe2\x9d\x8c Matematica: " + err);
        QMessageBox::warning(this, "Errore importazione", err);
        return;
    }

    if (nums.isEmpty()) {
        setStatus("\xe2\x9a\xa0\xef\xb8\x8f  Nessun numero trovato nel file.");
        QMessageBox::information(this, "Nessun numero",
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

/* ── Genera avviso dominio + suggerisci modello migliore disponibile ─
   Restituisce un messaggio HTML da mostrare nell'output.
   Se il modello corrente è già math-capable restituisce stringa vuota. */
static QString buildDomainHint(AiClient* ai, const QString& query,
                               const QComboBox* combo)
{
    using D = AiClient::QueryDomain;
    const D dom = AiClient::detectQueryDomain(query);
    if (!AiClient::domainNeedsMathModel(dom)) return {};

    const QString cur = ai ? ai->model() : QString();
    if (mathModelScore(cur) >= 50) return {};  /* già adatto */

    /* Cerca il migliore disponibile nella combo */
    QString best;
    int bestSc = -1;
    if (combo) {
        for (int i = 0; i < combo->count(); ++i) {
            const QString m = combo->itemData(i).toString();
            const int sc = mathModelScore(m);
            if (sc > bestSc) { bestSc = sc; best = m; }
        }
    }

    static const char* domLabel[] = {
        nullptr, "matematica", "fisica", "chimica", "elettronica", nullptr, nullptr
    };
    const char* dl = (dom < 7) ? domLabel[dom] : nullptr;
    QString msg = QString(
        "\xf0\x9f\x92\xa1  <b>Query di %1</b> rilevata. "
        "Il modello attuale (<code>%2</code>) non \xc3\xa8 specializzato per calcoli STEM.")
        .arg(dl ? dl : "scienze").arg(cur.isEmpty() ? "nessuno" : cur);

    if (!best.isEmpty() && bestSc >= 50)
        msg += QString(" <span style='color:#a3e635;'>Consigliato: <b>%1</b> "
                       "(punteggio math: %2)</span>.").arg(best).arg(bestSc);
    else
        msg += " Installa <code>qwen2.5-math:7b</code> o <code>deepseek-r1:7b</code> "
               "per risultati migliori.";

    return msg;
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
                "print(f'{v} \xc2\xb0C  =  {r:.4f} \xc2\xb0F')\n"
            ).arg(val);
        }
        if (QString(u.fromU) == "fahrenheit" && QString(u.toU) == "celsius") {
            return QString(
                "v = %1\n"
                "r = (v - 32) * 5/9\n"
                "print(f'{v} \xc2\xb0F  =  {r:.4f} \xc2\xb0C')\n"
            ).arg(val);
        }
        if (QString(u.fromU) == "celsius" && QString(u.toU) == "kelvin") {
            return QString(
                "v = %1\n"
                "r = v + 273.15\n"
                "print(f'{v} \xc2\xb0C  =  {r:.4f} K')\n"
            ).arg(val);
        }
        if (QString(u.fromU) == "kelvin" && QString(u.toU) == "celsius") {
            return QString(
                "v = %1\n"
                "r = v - 273.15\n"
                "print(f'{v} K  =  {r:.4f} \xc2\xb0C')\n"
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
        setStatus("\xe2\x9d\x8c  Backend non raggiungibile \xe2\x80\x94"
                  " avvia Ollama o llama-server per le funzioni AI.");
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
        setStatus("\xe2\x9d\x8c  Backend non raggiungibile \xe2\x80\x94"
                  " le funzioni AI non sono disponibili.");
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
        m_seqResult->setText("\xe2\x9d\x8c  " + err);
        LogBus::post("\xe2\x9d\x8c Matematica: " + err);
        m_seqResult->setVisible(true);
        return;
    }
    const QString pat = detectPatternLocal(seq);
    m_seqResult->setText("\xf0\x9f\x94\x8d  " + pat);
    m_seqResult->setVisible(true);
}

void MatematicaPage::onSympyClicked()
{
    QString err;
    QVector<double> seq = parseSeq(m_seqInput->text(), err);
    if (!err.isEmpty()) { setStatus("\xe2\x9d\x8c  " + err); LogBus::post("\xe2\x9d\x8c Matematica: " + err); return; }
    if (seq.size() < 2) { setStatus("\xe2\x9d\x8c  Inserisci almeno 2 termini."); LogBus::post("\xe2\x9d\x8c Matematica: Inserisci almeno 2 termini."); return; }

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
    if (!err.isEmpty()) { setStatus("\xe2\x9d\x8c  " + err); LogBus::post("\xe2\x9d\x8c Matematica: " + err); return; }
    if (seq.size() < 2) { setStatus("\xe2\x9d\x8c  Inserisci almeno 2 termini."); LogBus::post("\xe2\x9d\x8c Matematica: Inserisci almeno 2 termini."); return; }
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
        m_nthDescLbl->setText("Restituisce la N-esima cifra decimale di \xcf\x80 (dopo il punto). "
                         "Es. N=1 \xe2\x86\x92 1, N=2 \xe2\x86\x92 4, N=3 \xe2\x86\x92 1...");
    else if (k == "e_digit")
        m_nthDescLbl->setText(tr("N-esima cifra decimale di e. Es. N=1 \xe2\x86\x92 7, N=2 \xe2\x86\x92 1..."));
    else if (k == "prime")
        m_nthDescLbl->setText("Il primo con indice N. p(1)=2, p(2)=3, p(3)=5... "
                         "(sympy per N fino a ~10 000 000)");
    else if (k == "fib")
        m_nthDescLbl->setText("F(1)=1, F(2)=1, F(3)=2, F(4)=3, F(5)=5... "
                         "Anche per N molto grandi (mpmath).");
    else if (k == "fact")
        m_nthDescLbl->setText("N! \xe2\x80\x94 fattoriale. 1!=1, 5!=120, 100!=93326215443944..."
                         " (precisione arbitraria).");
    else if (k == "pow2")
        m_nthDescLbl->setText(tr("2^N. Anche per N molto grandi (migliaia di cifre)."));
    else if (k == "pi_block")
        m_nthDescLbl->setText("Le prime N cifre di \xcf\x80 come blocco continuo "
                         "(includa la parte intera: 3.14159...).");
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
    setStatus("\xe2\x9c\x85  Analisi AI completata.");
}

void MatematicaPage::onAiSeqError(const QString& msg)
{
    delete m_aiSeqHolder;
    m_aiSeqHolder = nullptr;
    m_aiRunning = false;
    appendOutput("\n\xe2\x9d\x8c  Errore AI: " + msg);
    setStatus("\xe2\x9d\x8c  Errore AI.");
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

/* ══════════════════════════════════════════════════════════════
   Archivio formule per il pulsante 🔀 Casuale in Risolvi Passi.
   Include tutti i test di CAT-E + formule aggiuntive per copertura
   completa di equazioni, disequazioni, derivate, integrali, limiti
   e semplificazioni.
   ══════════════════════════════════════════════════════════════ */
namespace {
struct SolveExample { const char* expr; const char* tipo; const char* desc; };
static const SolveExample kSolveExamples[] = {
    /* ── Equazioni (10) ──────────────────────────────────── */
    { "x^3 - 6*x^2 + 11*x - 6 = 0",
      "Equazione", "Terzo grado - radici razionali 1, 2, 3 [TEST]" },
    { "x^4 - 13*x^2 + 36 = 0",
      "Equazione", "Biquadratica - radici +-2, +-3 [TEST]" },
    { "x^2 + x + 1 = 0",
      "Equazione", "Delta < 0 - radici complesse coniugate [TEST]" },
    { "2*x^3 + x^2 - 5*x + 2 = 0",
      "Equazione", "Cubica con radici 1/2, 1, -2" },
    { "x^4 - 5*x^2 + 4 = 0",
      "Equazione", "Biquadratica - radici +-1, +-2" },
    { "x^3 + 3*x^2 - 4 = 0",
      "Equazione", "Cubica con fattore (x-1)" },
    { "x^2 - 2*sqrt(3)*x + 3 = 0",
      "Equazione", "Delta = 0 - radice doppia sqrt(3)" },
    { "x^5 - x = 0",
      "Equazione", "Quintico fattorizzabile - 5 radici" },
    { "x^2 - 5 = 0",
      "Equazione", "Radici irrazionali +-sqrt(5)" },
    { "x^3 - x = 0",
      "Equazione", "Cubica - radici 0, +-1" },
    /* ── Disequazioni (5) ────────────────────────────────── */
    { "x^2 - 5*x + 6 > 0",
      "Disequazione", "Parabola > 0 su (-oo,2) U (3,+oo) [TEST]" },
    { "x^3 - x > 0",
      "Disequazione", "Cubica positiva in (-1,0) U (1,+oo)" },
    { "(x - 1)*(x + 2)*(x - 3) < 0",
      "Disequazione", "Tre radici 1, -2, 3" },
    { "x^4 - 5*x^2 + 4 <= 0",
      "Disequazione", "Biquadratica <= 0 su [-2,-1] U [1,2]" },
    { "x^2 - 4 >= 0",
      "Disequazione", "Parabola >= 0 su (-oo,-2] U [2,+oo)" },
    /* ── Derivate (10) ───────────────────────────────────── */
    { "sin(x^2 + 1), x",
      "Derivata", "Catena: D[sin(x^2+1)] = 2x*cos(x^2+1) [TEST]" },
    { "x^3*exp(x), x, 2",
      "Derivata", "Derivata seconda di x^3*e^x [TEST]" },
    { "atan(x), x",
      "Derivata", "D[arctan(x)] = 1/(1+x^2) [TEST]" },
    { "log(x^2 + 1)*sin(x), x",
      "Derivata", "Prodotto ln(x^2+1)*sin(x)" },
    { "(x^2 + 1)/(x^3 - 1), x",
      "Derivata", "Derivata di funzione razionale" },
    { "sin(x)^2, x",
      "Derivata", "D[sin^2(x)] = sin(2x)" },
    { "sqrt(x^2 + 1), x",
      "Derivata", "D[sqrt(x^2+1)] = x/sqrt(x^2+1)" },
    { "exp(sin(x)), x",
      "Derivata", "Catena doppia e^{sin(x)}" },
    { "log(x + sqrt(x^2 + 1)), x",
      "Derivata", "D[arcsinh(x)] = 1/sqrt(x^2+1)" },
    { "x^2*exp(-x), x",
      "Derivata", "Prodotto polinomio x esponenziale" },
    /* ── Integrali (10) ──────────────────────────────────── */
    { "x*exp(x), x",
      "Integrale", "Per parti: Int x*e^x dx = (x-1)*e^x + C [TEST]" },
    { "sin(x), x, 0, pi",
      "Integrale", "Definito esatto: Int_0^pi sin(x)dx = 2 [TEST]" },
    { "exp(-x^2), x, 0, oo",
      "Integrale", "Gaussiano: Int_0^oo e^{-x^2}dx = sqrt(pi)/2 [TEST]" },
    { "log(x), x",
      "Integrale", "Per parti: Int ln(x)dx = x*ln(x)-x + C" },
    { "x^2*sin(x), x",
      "Integrale", "Per parti iterata su x^2*sin(x)" },
    { "1/(x^2 + 1), x",
      "Integrale", "Int 1/(1+x^2)dx = arctan(x) + C" },
    { "sqrt(1 - x^2), x, -1, 1",
      "Integrale", "Area semicerchio = pi/2" },
    { "sin(x)^2, x",
      "Integrale", "Formula di riduzione: Int sin^2(x)dx" },
    { "1/(x^2 - 1), x",
      "Integrale", "Frazioni parziali: ln|(x-1)/(x+1)|/2 + C" },
    { "x^3*exp(x), x",
      "Integrale", "Per parti ripetuta - 4 passaggi" },
    /* ── Limiti (9) ──────────────────────────────────────── */
    { "sin(x)/x, x, 0",
      "Limite", "Limite notevole fondamentale -> 1 [TEST]" },
    { "(1 - cos(x))/x^2, x, 0",
      "Limite", "Forma 0/0 -> 1/2 (L'Hopital) [TEST]" },
    { "(x^2 + 1)/(x^2 - 1), x, oo",
      "Limite", "Limite all'inf di razionale -> 1 [TEST]" },
    { "(exp(x) - 1)/x, x, 0",
      "Limite", "Limite notevole (e^x-1)/x -> 1" },
    { "(1 + 1/x)^x, x, oo",
      "Limite", "Definizione di e: (1+1/x)^x -> e" },
    { "x*log(x), x, 0",
      "Limite", "Forma 0*oo -> 0" },
    { "(sqrt(x + 1) - 1)/x, x, 0",
      "Limite", "Forma 0/0 -> 1/2 (razionalizzazione)" },
    { "(x^3 - 8)/(x - 2), x, 2",
      "Limite", "Forma 0/0 - fattorizza cubo -> 12" },
    { "sin(3*x)/sin(5*x), x, 0",
      "Limite", "Rapporto seni con limiti notevoli -> 3/5" },
    /* ── Semplificazioni (8) ─────────────────────────────── */
    { "sin(x)^2 + cos(x)^2",
      "Semplificazione", "Identita' di Pitagora = 1 [TEST]" },
    { "(x^3 - 1)/(x - 1)",
      "Semplificazione", "Differenza cubi -> x^2+x+1 [TEST]" },
    { "series(exp(x), x, 0, 6)",
      "Semplificazione", "Taylor di e^x in 0 all'ordine 5 [TEST]" },
    { "factor(x^4 - 5*x^2 + 4)",
      "Semplificazione", "Fattorizzazione quartica" },
    { "expand((x + 1)^6)",
      "Semplificazione", "Binomio di Newton ordine 6" },
    { "simplify(tan(x)^2 + 1 - 1/cos(x)^2)",
      "Semplificazione", "Identita' sec^2(x) = tan^2(x)+1 -> 0" },
    { "series(sin(x), x, 0, 8)",
      "Semplificazione", "Taylor di sin(x) all'ordine 7" },
    { "factor(x^6 - 1)",
      "Semplificazione", "Fattorizzazione differenza sesta potenza" },
};
static const int kNSolveExamples =
    static_cast<int>(sizeof(kSolveExamples)/sizeof(kSolveExamples[0]));
} // namespace

/* ══════════════════════════════════════════════════════════════
   buildSolveTab — risoluzione passo per passo via LLM (stile Derive)
   ══════════════════════════════════════════════════════════════ */
QWidget* MatematicaPage::buildSolveTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(8);

    lay->addWidget(new QLabel(
        "<b>Risolvi un'equazione, disequazione o espressione PASSO PER PASSO (stile Derive):</b>", w));

    /* Riga input + tipo */
    auto* inputRow = new QHBoxLayout;
    m_solveInput = new QLineEdit(w);
    m_solveInput->setPlaceholderText(
        "Inserisci equazione o espressione: es. x\xc2\xb2 + 5x + 6 = 0,  2x > 4,  sin(x)/x");
    m_solveInput->setMinimumWidth(320);
    inputRow->addWidget(m_solveInput, 1);

    m_solveCmb = new QComboBox(w);
    m_solveCmb->setObjectName("settingCombo");
    m_solveCmb->addItem("Equazione",       "equazione");
    m_solveCmb->addItem("Disequazione",    "disequazione");
    m_solveCmb->addItem("Derivata",        "derivata");
    m_solveCmb->addItem("Integrale",       "integrale");
    m_solveCmb->addItem("Limite",          "limite");
    m_solveCmb->addItem("Semplificazione", "semplificazione");
    inputRow->addWidget(m_solveCmb);

    auto* btnRandom = new QPushButton("\xf0\x9f\x94\x80", w);  /* 🔀 */
    btnRandom->setObjectName("navBtn");
    btnRandom->setFixedWidth(dpiScale(34));
    btnRandom->setToolTip(
        QString("Formula casuale (%1 disponibili) — cambia tipo ed espressione")
        .arg(kNSolveExamples));
    connect(btnRandom, &QPushButton::clicked,
            this, &MatematicaPage::onSolveRandomClicked);
    inputRow->addWidget(btnRandom);

    auto* btnCopyInput = new QPushButton("\xf0\x9f\x93\x8b", w);  /* 📋 */
    btnCopyInput->setObjectName("navBtn");
    btnCopyInput->setFixedWidth(dpiScale(34));
    btnCopyInput->setToolTip(tr("Copia la formula negli appunti"));
    connect(btnCopyInput, &QPushButton::clicked, m_solveInput, [this]() {
        const QString t = m_solveInput->text();
        if (!t.isEmpty())
            QApplication::clipboard()->setText(t);
    });
    inputRow->addWidget(btnCopyInput);

    lay->addLayout(inputRow);

    /* Barra pulsanti */
    auto* btnRow = new QHBoxLayout;

    m_btnSolve = new QPushButton(
        "\xf0\x9f\x94\xa2  Risolvi passo per passo", w);   /* 🔢 */
    m_btnSolve->setObjectName("actionBtn");
    m_btnSolve->setProperty("highlight", "true");
    connect(m_btnSolve, &QPushButton::clicked, this, &MatematicaPage::onSolveClicked);
    btnRow->addWidget(m_btnSolve);

    auto* btnStop = new QPushButton("\xe2\x96\xa0  Stop", w);  /* ■ */
    btnStop->setObjectName("stopBtn");
    connect(btnStop, &QPushButton::clicked, this, &MatematicaPage::onSolveStopClicked);
    btnRow->addWidget(btnStop);

    m_btnSolveAi = new QPushButton("\xf0\x9f\xa4\x96  Spiega con AI", w);  /* 🤖 */
    m_btnSolveAi->setObjectName("actionBtn");
    m_btnSolveAi->setToolTip(tr("Usa l'LLM selezionato per spiegare i passi SymPy in italiano"));
    m_btnSolveAi->setVisible(false);
    connect(m_btnSolveAi, &QPushButton::clicked, this, &MatematicaPage::onSolveAiClicked);
    btnRow->addWidget(m_btnSolveAi);

    m_btnSolveCopy = new QPushButton("\xf0\x9f\x93\x8b  Copia", w);  /* 📋 */
    m_btnSolveCopy->setObjectName("actionBtn");
    connect(m_btnSolveCopy, &QPushButton::clicked, this, &MatematicaPage::onSolveCopyClicked);
    btnRow->addWidget(m_btnSolveCopy);

    btnRow->addStretch(1);
    lay->addLayout(btnRow);

    /* Nota informativa */
    auto* note = new QLabel(
        "<small><b>Notazione:</b> x^2 per x\xc2\xb2, sqrt(x) per \xe2\x88\x9ax, "
        "sin/cos/tan/log/exp. "
        "Derivata: <i>f(x), var[, ordine]</i> \xe2\x80\x94 "
        "Integrale: <i>f(x)[, a, b]</i> \xe2\x80\x94 "
        "Limite: <i>f(x), var, val</i>. "
        "Il risultato appare nel pannello in basso.</small>", w);
    note->setWordWrap(true);
    lay->addWidget(note);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   onSolveClicked — avvia la risoluzione passo per passo
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveClicked()
{
    if (m_solveBusy) return;

    const QString expr = m_solveInput->text().trimmed();
    if (expr.isEmpty()) {
        appendOutput("\xe2\x9d\x8c  Inserisci un'equazione o espressione prima di procedere.\n");
        LogBus::post("\xe2\x9d\x8c Matematica: Inserisci un'equazione o espressione prima di procedere.");
        return;
    }

    clearOutput();
    m_solveFullText.clear();
    if (m_btnSolveAi) m_btnSolveAi->setVisible(false);
    setStatus("\xf0\x9f\x93\x90  SymPy in calcolo...");

    const QString tipo = m_solveCmb->currentText();

    /* runPython chiama stopPython() internamente che azzera m_solvePyMode/m_solveBusy;
       le reimpostiamo DOPO la chiamata (il processo gira in modo asincrono). */
    runPython(buildSympyScript(tipo, expr));
    m_solvePyMode = true;
    m_solveBusy   = true;
    m_btnSolve->setEnabled(false);
}

/* ══════════════════════════════════════════════════════════════
   onSolveStopClicked — interrompe il calcolo (SymPy o AI)
   ══════════════════════════════════════════════════════════════ */
/* ══════════════════════════════════════════════════════════════
   onSolveRandomClicked — pesca una formula casuale dall'archivio
   kSolveExamples e la inserisce nel campo + imposta il tipo.
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveRandomClicked()
{
    const int idx = static_cast<int>(
        QRandomGenerator::global()->bounded(static_cast<quint32>(kNSolveExamples)));
    const SolveExample& ex = kSolveExamples[idx];

    if (m_solveInput)
        m_solveInput->setText(QString::fromUtf8(ex.expr));

    if (m_solveCmb) {
        const int ci = m_solveCmb->findText(QString::fromUtf8(ex.tipo));
        if (ci >= 0) m_solveCmb->setCurrentIndex(ci);
    }

    setStatus(QString("\xf0\x9f\x94\x80  %1: %2")   /* 🔀 */
              .arg(QString::fromUtf8(ex.tipo),
                   QString::fromUtf8(ex.desc)));
}

void MatematicaPage::onSolveStopClicked()
{
    if (!m_solveBusy) return;
    if (m_solvePyMode) {
        stopPython();   /* resetta m_solvePyMode e m_solveBusy */
    } else {
        if (m_ai) m_ai->abort();
        delete m_aiSolveHolder;
        m_aiSolveHolder = nullptr;
        m_solveBusy = false;
        if (m_btnSolve)   m_btnSolve->setEnabled(true);
        if (m_btnSolveAi) m_btnSolveAi->setVisible(true);
    }
    setStatus("\xe2\x96\xa0  Risoluzione interrotta.");
}

/* ══════════════════════════════════════════════════════════════
   onSolveAiClicked — spiega il risultato SymPy con l'LLM selezionato
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveAiClicked()
{
    if (m_solveBusy || !m_ai) return;
    const QString sympyOut = m_solveFullText.trimmed();
    if (sympyOut.isEmpty()) return;

    /* Applica il modello scelto nella combo condivisa */
    if (m_modelCombo) {
        const QString sel = m_modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    /* ── Hint dominio: suggerisci modello math-capable se la query è STEM ── */
    if (m_solveInput) {
        const QString hint = buildDomainHint(m_ai, m_solveInput->text(), m_modelCombo);
        if (!hint.isEmpty())
            appendOutput(hint + "\n\n");
    }

    m_solveBusy = true;
    m_btnSolve->setEnabled(false);
    if (m_btnSolveAi) m_btnSolveAi->setEnabled(false);
    setStatus("\xf0\x9f\xa4\x96  AI sta spiegando...");

    appendOutput("\n\n\xe2\x94\x80\xe2\x94\x80 \xf0\x9f\xa4\x96 Spiegazione AI "
                 "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n");

    const QString sys =
        "Sei un professore di matematica universitaria. Ricevi l'output di SymPy "
        "(calcolo simbolico esatto) e devi commentarlo in italiano con rigore didattico.\n\n"
        "PER OGNI PASSO dell'output devi rispondere a TRE domande:\n"
        "  1. COSA è stato fatto (brevemente — lo studente lo vede già).\n"
        "  2. PERCHÉ questo passo è necessario in questo momento — "
        "qual è la motivazione matematica, il teorema o il principio che lo giustifica.\n"
        "  3. COSA sarebbe successo se non lo avessimo fatto, o quale alternativa "
        "esisteva e perché non è stata scelta.\n\n"
        "Regole di stile:\n"
        "- Scrivi in prosa fluente, non in elenchi puntati.\n"
        "- Cita i teoremi per nome quando rilevante "
        "(es. 'Teorema Fondamentale del Calcolo', 'Regola di L\\'Hôpital').\n"
        "- NON ripetere i calcoli simbolici — commentali concettualmente.\n"
        "- Chiudi con una verifica o un'osservazione pratica utile allo studente.\n"
        "Rispondi ESCLUSIVAMENTE in italiano.";

    const QString user = QString(
        "Problema: %1\n\n"
        "Output SymPy (passi già eseguiti con motivazioni brevi):\n%2\n\n"
        "Approfondisci le motivazioni di ogni passo come descritto nel tuo ruolo.")
        .arg(m_solveInput->text(), sympyOut);

    delete m_aiSolveHolder;
    m_aiSolveHolder = new QObject(this);
    connect(m_ai, &AiClient::token,    m_aiSolveHolder,
            [this](const QString& t){ onSolveToken(t); });
    connect(m_ai, &AiClient::finished, m_aiSolveHolder,
            [this](const QString& f){ onSolveFinished(f); });
    connect(m_ai, &AiClient::error,    m_aiSolveHolder,
            [this](const QString& e){ onSolveError(e); });

    m_ai->chat(P::prependMathKnowledge(sys), user);
}

/* ══════════════════════════════════════════════════════════════
   onSolveCopyClicked — copia il testo dell'output negli appunti
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveCopyClicked()
{
    const QString txt = m_output->toPlainText();
    if (txt.isEmpty()) return;
    QApplication::clipboard()->setText(txt);
    if (m_btnSolveCopy) m_btnSolveCopy->setText(tr("\xe2\x9c\x85  Copiato!"));
    QTimer::singleShot(1500, this, &MatematicaPage::onSolveRestoreCopyBtn);
}

/* ══════════════════════════════════════════════════════════════
   onSolveRestoreCopyBtn — ripristina il testo del pulsante Copia
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveRestoreCopyBtn()
{
    if (m_btnSolveCopy)
        m_btnSolveCopy->setText(tr("\xf0\x9f\x93\x8b  Copia"));
}

/* ══════════════════════════════════════════════════════════════
   Slot AI — streaming token
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveToken(const QString& t)
{
    appendOutput(t);
}

/* ══════════════════════════════════════════════════════════════
   Slot AI — risposta completa
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveFinished(const QString& full)
{
    if (m_latexOut && m_latexOut->isVisible() && !full.isEmpty())
        m_latexOut->setLatexHtml(
            "<div class='ai-out'>" + full.toHtmlEscaped().replace("\n", "<br>") + "</div>");
    delete m_aiSolveHolder;
    m_aiSolveHolder = nullptr;
    m_solveBusy = false;
    if (m_btnSolve)   m_btnSolve->setEnabled(true);
    if (m_btnSolveAi) { m_btnSolveAi->setEnabled(true); m_btnSolveAi->setVisible(true); }
    setStatus("\xe2\x9c\x85  Spiegazione AI completata.");
}

/* ══════════════════════════════════════════════════════════════
   Slot AI — errore
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onSolveError(const QString& msg)
{
    delete m_aiSolveHolder;
    m_aiSolveHolder = nullptr;
    m_solveBusy = false;
    if (m_btnSolve)   m_btnSolve->setEnabled(true);
    if (m_btnSolveAi) { m_btnSolveAi->setEnabled(true); m_btnSolveAi->setVisible(true); }
    appendOutput("\n\xe2\x9d\x8c  Errore AI: " + msg + "\n");
    setStatus("\xe2\x9d\x8c  Errore AI.");
    LogBus::post("\xe2\x9d\x8c Matematica: Errore AI risoluzione: " + msg);
}

/* ══════════════════════════════════════════════════════════════
   Dati statici argomenti Analisi 1 e 2
   ══════════════════════════════════════════════════════════════ */
namespace {

struct AnalisiTopic { const char* name; const char* html; const char* ex; const char* type; const char* plotEx; };

/* Tipo stringa deve corrispondere al testo di m_solveCmb */
static const AnalisiTopic kA1[] = {
  { "Limiti",
    "<h3 style='color:#60a5fa'>Limiti di funzione</h3>"
    "<p><b>Definizione &epsilon;&ndash;&delta;:</b></p>"
    "\\[\\lim_{x\\to c} f(x) = L \\iff "
    "\\forall\\varepsilon>0\\;\\exists\\delta>0:\\;"
    "0<|x-c|<\\delta \\Rightarrow |f(x)-L|<\\varepsilon\\]"
    "<p><b>Limiti notevoli (\\(x\\to 0\\)):</b></p>"
    "<ul>"
    "<li>\\(\\dfrac{\\sin x}{x}\\to 1\\) &nbsp;&nbsp; "
    "\\(\\dfrac{1-\\cos x}{x^2}\\to\\dfrac{1}{2}\\)</li>"
    "<li>\\(\\dfrac{\\ln(1+x)}{x}\\to 1\\) &nbsp;&nbsp; "
    "\\(\\dfrac{e^x-1}{x}\\to 1\\)</li>"
    "<li>\\(\\dfrac{\\arctan x}{x}\\to 1\\) &nbsp;&nbsp; "
    "\\(\\dfrac{a^x-1}{x}\\to \\ln a\\)</li>"
    "<li>Per \\(x\\to\\infty\\): &nbsp; "
    "\\(\\left(1+\\dfrac{1}{x}\\right)^x\\to e\\)</li>"
    "</ul><hr>"
    "<div class='box-purple'>"
    "<b style='color:#c4b5fd'>&#x2728; Regola di L&rsquo;H&ocirc;pital</b> "
    "(forme \\(0/0\\) o \\(\\infty/\\infty\\)):"
    "\\[\\lim\\frac{f}{g}=\\lim\\frac{f'}{g'}\\]"
    "si applica iterativamente se la forma rimane indeterminata.</div>"
    "<div class='box-green'>"
    "<b style='color:#86efac'>&#x221e; Gerarchia degli infiniti</b> "
    "per \\(x\\to+\\infty\\):"
    "\\[\\log x \\ll x^{\\alpha} \\ll e^x \\ll x^x \\quad(\\alpha>0)\\]"
    "<small>Utile per limiti \\(\\infty-\\infty\\).</small></div>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Limite):</b><br>"
    "sin(x)/x, x, 0 &bull; (exp(x)-1)/x, x, 0<br>"
    "x*log(x), x, 0 &bull; (1+1/x)**x, x, oo</div>",
    "sin(x)/x, x, 0", "Limite", "sin(x)/x" },

  { "Derivate",
    "<h3 style='color:#60a5fa'>Derivate</h3>"
    "<p><b>Definizione:</b>"
    "\\[f'(x)=\\lim_{h\\to 0}\\frac{f(x+h)-f(x)}{h}\\]</p>"
    "<p><b>Tavola derivate fondamentali:</b></p>"
    "<ul>"
    "<li>\\(D[x^n]=nx^{n-1}\\) &bull; \\(D[e^x]=e^x\\) &bull; "
    "\\(D[\\ln x]=\\dfrac{1}{x}\\)</li>"
    "<li>\\(D[\\sin x]=\\cos x\\) &bull; \\(D[\\cos x]=-\\sin x\\)</li>"
    "<li>\\(D[\\tan x]=\\dfrac{1}{\\cos^2 x}\\) &bull; "
    "\\(D[\\arcsin x]=\\dfrac{1}{\\sqrt{1-x^2}}\\)</li>"
    "</ul><hr>"
    "<p><b>Regole:</b></p><ul>"
    "<li>\\((fg)'=f'g+fg'\\)</li>"
    "<li>\\(\\left(\\dfrac{f}{g}\\right)'=\\dfrac{f'g-fg'}{g^2}\\)</li>"
    "<li>\\((f\\circ g)'=(f'\\circ g)\\cdot g'\\) (catena)</li>"
    "</ul><hr>"
    "<p><b>Rolle:</b> \\(f(a)=f(b)\\Rightarrow\\exists c: f'(c)=0\\)<br>"
    "<b>Lagrange:</b> \\(\\exists c\\in(a,b):\\;"
    "f'(c)=\\dfrac{f(b)-f(a)}{b-a}\\)</p>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Derivata):</b><br>"
    "x**3 - 2*x + 1, x &bull; sin(x)*exp(x), x<br>"
    "log(x**2+1), x &bull; x**2*cos(x), x, 2</div>",
    "x**3 - 2*x + 1, x", "Derivata", "x**3 - 2*x + 1" },

  { "Integrali indefiniti",
    "<h3 style='color:#60a5fa'>Integrali indefiniti</h3>"
    "<p><b>Definizione:</b> \\(F'(x)=f(x)\\Rightarrow"
    "\\int f(x)\\,dx=F(x)+C\\)</p><hr>"
    "<p><b>Integrali immediati:</b></p><ul>"
    "<li>\\(\\displaystyle\\int x^n\\,dx=\\dfrac{x^{n+1}}{n+1}+C\\quad(n\\ne -1)\\)</li>"
    "<li>\\(\\displaystyle\\int e^x\\,dx=e^x+C\\) &bull; "
    "\\(\\displaystyle\\int\\sin x\\,dx=-\\cos x+C\\)</li>"
    "<li>\\(\\displaystyle\\int\\dfrac{1}{x}\\,dx=\\ln|x|+C\\) &bull; "
    "\\(\\displaystyle\\int\\dfrac{1}{1+x^2}\\,dx=\\arctan x+C\\)</li>"
    "</ul><hr>"
    "<p><b>Tecniche:</b></p><ul>"
    "<li><b>Sostituzione:</b> \\(\\int f(g(x))g'(x)\\,dx\\;\\to\\; t=g(x)\\)</li>"
    "<li><b>Per parti:</b> \\(\\int u\\,dv=uv-\\int v\\,du\\)</li>"
    "<li><b>Frazioni parziali:</b> \\(\\dfrac{p(x)}{q(x)}\\) con \\(q\\) fattorizzabile</li>"
    "</ul>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Integrale):</b><br>"
    "x**2*exp(x), x &bull; sin(x)**2, x<br>"
    "1/(x**2-1), x &bull; log(x), x</div>",
    "x**2*exp(x), x", "Integrale", "x**2*exp(x)" },

  { "Integrali definiti",
    "<h3 style='color:#60a5fa'>Integrali definiti</h3>"
    "<p><b>Def. (somme di Riemann):</b>"
    "\\[\\int_a^b f(x)\\,dx=\\lim_{n\\to\\infty}\\sum_k f(x_k)\\Delta x\\]</p>"
    "<p><b>Teorema fondamentale del calcolo:</b>"
    "\\[\\int_a^b f(x)\\,dx=F(b)-F(a)\\quad\\text{dove }F'=f\\]</p>"
    "<p><b>Propriet&agrave;:</b> linearit&agrave;, additivit&agrave; sugli intervalli, monotonia.<br>"
    "<b>Teorema della media:</b> "
    "\\(\\exists c\\in(a,b):\\int_a^b f\\,dx=f(c)(b-a)\\)</p>"
    "<p><b>Applicazioni:</b> area, lunghezza arco, volumi di rotazione.</p>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Integrale):</b><br>"
    "sin(x), x, 0, pi &bull; x**2, x, 0, 1<br>"
    "exp(-x**2), x, 0, 1 &bull; 1/x, x, 1, exp(1)</div>",
    "sin(x), x, 0, pi", "Integrale", "sin(x)" },

  { "Studio di funzione",
    "<h3 style='color:#60a5fa'>Studio di funzione</h3>"
    "<p><b>Schema completo:</b></p>"
    "<ol>"
    "<li><b>Dominio</b> &mdash; dove \\(f(x)\\) &egrave; definita</li>"
    "<li><b>Simmetrie</b> &mdash; pari (\\(f(-x)=f(x)\\)), dispari</li>"
    "<li><b>Limiti agli estremi</b> &mdash; asintoti orizzontali/verticali</li>"
    "<li><b>Segno</b> &mdash; \\(f(x)>0\\), \\(f(x)=0\\)</li>"
    "<li><b>Monotonia</b> &mdash; \\(f'(x)>0\\) crescente, \\(<0\\) decrescente</li>"
    "<li><b>Estremi relativi</b> &mdash; \\(f'(x_0)=0\\Rightarrow f''\\) decide</li>"
    "<li><b>Convessit&agrave;</b> &mdash; \\(f''>0\\) conv. \\(\\uparrow\\), "
    "\\(f''<0\\) conv. \\(\\downarrow\\)</li>"
    "<li><b>Asintoto obliquo:</b> \\(y=mx+q\\), "
    "\\(m=\\lim\\dfrac{f(x)}{x}\\), \\(q=\\lim[f(x)-mx]\\)</li>"
    "</ol>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Derivata):</b><br>"
    "(x**2-1)/(x**2+1), x &bull; x*exp(-x), x<br>"
    "x**3-3*x, x &bull; log(x)/x, x</div>",
    "(x**2-1)/(x**2+1), x", "Derivata", "(x**2-1)/(x**2+1)" },

  { "Serie di Taylor",
    "<h3 style='color:#60a5fa'>Serie di Taylor &amp; Maclaurin</h3>"
    "<p><b>Formula di Taylor in \\(x_0\\):</b>"
    "\\[f(x)=\\sum_{k=0}^{n}\\frac{f^{(k)}(x_0)}{k!}(x-x_0)^k + R_n(x)\\]</p>"
    "<p><b>Sviluppi di Maclaurin fondamentali (in 0):</b></p><ul>"
    "<li>\\(e^x=1+x+\\dfrac{x^2}{2!}+\\dfrac{x^3}{3!}+\\cdots\\)</li>"
    "<li>\\(\\sin x=x-\\dfrac{x^3}{3!}+\\dfrac{x^5}{5!}-\\cdots\\)</li>"
    "<li>\\(\\cos x=1-\\dfrac{x^2}{2!}+\\dfrac{x^4}{4!}-\\cdots\\)</li>"
    "<li>\\(\\ln(1+x)=x-\\dfrac{x^2}{2}+\\dfrac{x^3}{3}-\\cdots\\quad|x|<1\\)</li>"
    "<li>\\((1+x)^\\alpha=1+\\alpha x+"
    "\\dfrac{\\alpha(\\alpha-1)}{2!}x^2+\\cdots\\)</li>"
    "</ul>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Semplificazione):</b><br>"
    "series(exp(x), x, 0, 6) &rarr; usa il campo Espressione<br>"
    "series(sin(x), x, 0, 8)<br>"
    "series(log(1+x), x, 0, 5)</div>",
    "series(exp(x), x, 0, 6)", "Semplificazione", "exp(x)" },

  { "Successioni e serie numeriche",
    "<h3 style='color:#60a5fa'>Successioni e serie</h3>"
    "<p><b>Successione</b> \\(\\{a_n\\}\\): converge a \\(L\\) se "
    "\\(|a_n-L|\\to 0\\)</p>"
    "<p><b>Serie</b> \\(\\sum a_n\\): \\(S_n=a_1+\\cdots+a_n\\), "
    "converge se \\(S_n\\) ha limite finito</p><hr>"
    "<p><b>Criteri di convergenza:</b></p><ul>"
    "<li><b>Confronto:</b> \\(0\\le a_n\\le b_n\\); "
    "\\(b_n\\) conv. \\(\\Rightarrow a_n\\) conv.</li>"
    "<li><b>Rapporto (D&rsquo;Alembert):</b> "
    "\\(\\rho=\\lim\\left|\\dfrac{a_{n+1}}{a_n}\\right|\\); "
    "\\(\\rho<1\\) conv.</li>"
    "<li><b>Radice (Cauchy):</b> "
    "\\(\\rho=\\lim\\sqrt[n]{|a_n|}\\); \\(\\rho<1\\) conv.</li>"
    "<li><b>Leibniz:</b> \\(\\sum(-1)^n a_n\\) conv. se \\(a_n\\downarrow 0\\)</li>"
    "</ul>"
    "<p><b>Serie geometrica:</b> "
    "\\(\\displaystyle\\sum q^n=\\dfrac{1}{1-q}\\) per \\(|q|<1\\)</p>"
    "<div class='box-blue'><b style='color:#93c5fd'>Esempi SymPy (tipo Semplificazione):</b><br>"
    "Sum(1/n**2, (n,1,oo)) &rarr; usa Espressione<br>"
    "Sum((-1)**n/factorial(n), (n,0,oo))</div>",
    "Sum(1/n**2, (n,1,oo))", "Semplificazione", "1/x**2" },
};

static const AnalisiTopic kA2[] = {
  { "Derivate parziali",
    "<h3 style='color:#fb923c'>Derivate parziali</h3>"
    "<p><b>Definizione:</b>"
    "\\[\\frac{\\partial f}{\\partial x}(x_0,y_0)="
    "\\lim_{h\\to 0}\\frac{f(x_0+h,y_0)-f(x_0,y_0)}{h}\\]</p>"
    "<p><b>Differenziale totale:</b>"
    "\\[df=\\frac{\\partial f}{\\partial x}\\,dx+"
    "\\frac{\\partial f}{\\partial y}\\,dy\\]</p>"
    "<p>\\(f\\) differenziabile \\(\\Rightarrow\\) derivate parziali continue</p><hr>"
    "<p><b>Regola della catena:</b> \\(z=f(x(t),y(t))\\)"
    "\\[\\frac{dz}{dt}=\\frac{\\partial f}{\\partial x}\\frac{dx}{dt}+"
    "\\frac{\\partial f}{\\partial y}\\frac{dy}{dt}\\]</p>"
    "<div class='box-orange'><b style='color:#93c5fd'>Esempi SymPy (tipo Derivata):</b><br>"
    "x**2*y + sin(x*y), x &rarr; deriv. parz. in x<br>"
    "exp(x**2+y**2), y &bull; x**3*y**2, x, 2</div>",
    "x**2*y + sin(x*y), x", "Derivata", "x**2*y + sin(x*y)" },

  { "Gradiente e piano tangente",
    "<h3 style='color:#fb923c'>Gradiente e piano tangente</h3>"
    "<p><b>Gradiente</b> di \\(f(x,y)\\):"
    "\\[\\nabla f=\\left(\\frac{\\partial f}{\\partial x},"
    "\\frac{\\partial f}{\\partial y}\\right)\\]</p>"
    "<p><b>Derivata direzionale:</b> "
    "\\(D_{\\hat{u}}f=\\nabla f\\cdot\\hat{u}\\)<br>"
    "Massima crescita = direzione di \\(\\nabla f\\)</p><hr>"
    "<p><b>Piano tangente</b> a \\(z=f(x,y)\\) in \\((x_0,y_0)\\):"
    "\\[z=f(x_0,y_0)+f_x(x_0,y_0)(x-x_0)+f_y(x_0,y_0)(y-y_0)\\]</p>"
    "<div class='box-orange'><b style='color:#93c5fd'>Esempi SymPy (tipo Derivata):</b><br>"
    "x**2 + y**2, x &bull; x*y*exp(x+y), y<br>"
    "sin(x)*cos(y), x</div>",
    "x**2 + y**2, x", "Derivata", "x**2 + y**2" },

  { "Hessiana ed estremi liberi",
    "<h3 style='color:#fb923c'>Matrice Hessiana ed estremi</h3>"
    "<p><b>Matrice Hessiana</b> \\(H_f\\) in \\((x_0,y_0)\\):"
    "\\[H=\\begin{pmatrix}f_{xx}&amp;f_{xy}\\\\"
    "f_{yx}&amp;f_{yy}\\end{pmatrix}\\]</p>"
    "<p><b>Punti critici:</b> \\(\\nabla f=0\\) "
    "(\\(f_x=0,\\;f_y=0\\))</p>"
    "<p><b>Criterio dell&rsquo;Hessiana:</b></p><ul>"
    "<li>\\(\\det(H)>0\\) e \\(f_{xx}>0 \\Rightarrow\\) <b>minimo</b></li>"
    "<li>\\(\\det(H)>0\\) e \\(f_{xx}<0 \\Rightarrow\\) <b>massimo</b></li>"
    "<li>\\(\\det(H)<0 \\Rightarrow\\) <b>punto di sella</b></li>"
    "<li>\\(\\det(H)=0 \\Rightarrow\\) indecidibile</li>"
    "</ul>"
    "<div class='box-orange'><b style='color:#93c5fd'>Esempi SymPy (Semplificazione):</b><br>"
    "usa Espressione: hessian(x**4+y**4-4*x*y, [x,y])<br>"
    "oppure: solve([diff(x**3+y**3-3*x*y,x), diff(x**3+y**3-3*x*y,y)])</div>",
    "x**4 + y**4 - 4*x*y", "Semplificazione", "x**4 + y**4 - 4*x*y" },

  { "Moltiplicatori di Lagrange",
    "<h3 style='color:#fb923c'>Moltiplicatori di Lagrange</h3>"
    "<p><b>Problema:</b> max/min \\(f(x,y)\\) soggetto a \\(g(x,y)=0\\)</p>"
    "<p><b>Condizione necessaria</b> in un punto estremo \\(P_0\\):"
    "\\[\\nabla f(P_0)=\\lambda\\,\\nabla g(P_0)\\]</p>"
    "<p><b>Sistema da risolvere:</b>"
    "\\[f_x=\\lambda g_x,\\quad f_y=\\lambda g_y,\\quad g(x,y)=0\\]</p>"
    "<p>Estensione a pi&ugrave; variabili:"
    "\\[\\nabla f=\\lambda_1\\nabla g_1+\\lambda_2\\nabla g_2+\\cdots\\]</p>"
    "<div class='box-orange'><b>SymPy (tab Espressione):</b><br>"
    "solve([diff(x**2+y**2,x)-lam*diff(x+y-1,x),<br>"
    "&nbsp;diff(x**2+y**2,y)-lam*diff(x+y-1,y),x+y-1],[x,y,lam])</div>",
    "x**2 + y**2", "Semplificazione", "x**2 + y**2" },

  { "Integrali doppi",
    "<h3 style='color:#fb923c'>Integrali doppi</h3>"
    "<p><b>Teorema di Fubini</b> su \\([a,b]\\times[c,d]\\):"
    "\\[\\iint_D f\\,dA=\\int_a^b\\left[\\int_c^d f(x,y)\\,dy\\right]dx\\]</p>"
    "<p><b>Dominio normale</b> "
    "(\\(a\\le x\\le b\\), \\(\\varphi_1\\le y\\le\\varphi_2\\)):"
    "\\[\\iint f\\,dA=\\int_a^b\\int_{\\varphi_1(x)}^{\\varphi_2(x)}"
    "f(x,y)\\,dy\\,dx\\]</p>"
    "<p><b>Coordinate polari:</b> \\(x=r\\cos\\theta\\), "
    "\\(y=r\\sin\\theta\\)"
    "\\[\\iint f\\,dA=\\iint f(r\\cos\\theta,r\\sin\\theta)"
    "\\cdot r\\,dr\\,d\\theta\\]</p>"
    "<div class='box-orange'><b style='color:#93c5fd'>Esempi SymPy (Integrale):</b><br>"
    "x*y, x, 0, 1 (semplice)<br>"
    "Tab Espressione: integrate(integrate(x*y,(y,0,x)),(x,0,1))</div>",
    "integrate(x*y, (y, 0, x))", "Semplificazione", "x*y" },

  { "Equazioni differenziali",
    "<h3 style='color:#fb923c'>Equazioni differenziali ordinarie</h3>"
    "<p><b>EDO I ordine separabile:</b> \\(y'=f(x)g(y)\\)"
    "\\[\\int\\frac{dy}{g(y)}=\\int f(x)\\,dx\\]</p>"
    "<p><b>EDO I ordine lineare:</b> \\(y'+p(x)y=q(x)\\)<br>"
    "Fattore integrante: \\(\\mu(x)=e^{\\int p\\,dx}\\)"
    "\\[y=\\frac{\\int\\mu(x)q(x)\\,dx+C}{\\mu(x)}\\]</p>"
    "<p><b>EDO II ordine a coeff. costanti:</b> \\(ay''+by'+cy=0\\)<br>"
    "Eq. caratteristica: \\(a\\lambda^2+b\\lambda+c=0\\)</p><ul>"
    "<li>\\(\\Delta>0\\): \\(y=C_1 e^{\\lambda_1 x}+C_2 e^{\\lambda_2 x}\\)</li>"
    "<li>\\(\\Delta=0\\): \\(y=(C_1+C_2 x)e^{\\lambda x}\\)</li>"
    "<li>\\(\\Delta<0\\): "
    "\\(y=e^{\\alpha x}(C_1\\cos\\beta x+C_2\\sin\\beta x)\\)</li>"
    "</ul>"
    "<div class='box-orange'><b>SymPy (tab Espressione):</b><br>"
    "dsolve(f(x).diff(x) + f(x) - exp(x), f(x))<br>"
    "dsolve(f(x).diff(x,2)+f(x), f(x))</div>",
    "dsolve(f(x).diff(x) + f(x) - exp(x), f(x))", "Semplificazione", "exp(-x) + exp(x)/2" },

  { "Calcolo vettoriale",
    "<h3 style='color:#fb923c'>Calcolo vettoriale</h3>"
    "<p><b>Divergenza</b> di \\(\\mathbf{F}=(P,Q,R)\\):"
    "\\[\\operatorname{div}\\mathbf{F}="
    "\\frac{\\partial P}{\\partial x}+"
    "\\frac{\\partial Q}{\\partial y}+"
    "\\frac{\\partial R}{\\partial z}\\]</p>"
    "<p><b>Rotore (curl):</b>"
    "\\[\\operatorname{rot}\\mathbf{F}=\\nabla\\times\\mathbf{F}="
    "\\begin{vmatrix}\\mathbf{i}&amp;\\mathbf{j}&amp;\\mathbf{k}\\\\"
    "\\partial_x&amp;\\partial_y&amp;\\partial_z\\\\"
    "P&amp;Q&amp;R\\end{vmatrix}\\]</p>"
    "<p><b>Teorema di Gauss:</b>"
    "\\[\\iint_{\\partial V}\\mathbf{F}\\cdot\\mathbf{n}\\,dS="
    "\\iiint_V\\operatorname{div}\\mathbf{F}\\,dV\\]</p>"
    "<p><b>Teorema di Stokes:</b>"
    "\\[\\oint_{\\partial\\Sigma}\\mathbf{F}\\cdot d\\mathbf{r}="
    "\\iint_\\Sigma\\operatorname{rot}\\mathbf{F}\\cdot\\mathbf{n}\\,dS\\]</p>"
    "<p><b>Potenziale:</b> \\(\\operatorname{rot}\\mathbf{F}=0"
    "\\iff\\mathbf{F}=\\nabla\\varphi\\)</p>"
    "<div class='box-orange'><b>SymPy (tab Espressione):</b><br>"
    "from sympy.vector import CoordSys3D; N=CoordSys3D('N')<br>"
    "F = N.x**2*N.i + N.y**2*N.j; divergence(F)</div>",
    "x**2 + y**2 + z**2", "Semplificazione", "x**2 + y**2" },
};

} // namespace

/* ══════════════════════════════════════════════════════════════
   Helper — crea combo tipo identica a m_solveCmb (sottoinsieme)
   ══════════════════════════════════════════════════════════════ */
static QComboBox* makeAnalisiTypeCmb(QWidget* parent)
{
    auto* c = new QComboBox(parent);
    c->setObjectName("settingCombo");
    c->addItem("Derivata",        "Derivata");
    c->addItem("Integrale",       "Integrale");
    c->addItem("Limite",          "Limite");
    c->addItem("Semplificazione", "Semplificazione");
    c->addItem("Equazione",       "Equazione");
    return c;
}

/* ══════════════════════════════════════════════════════════════
   buildAnalisi1Tab — studio funzioni Analisi 1
   ══════════════════════════════════════════════════════════════ */
QWidget* MatematicaPage::buildAnalisi1Tab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(10, 8, 10, 8);
    lay->setSpacing(8);

    /* ─── Riga selettore argomento ─── */
    auto* topRow = new QHBoxLayout;
    auto* topLbl = new QLabel("<b>\xf0\x9f\x93\x98  Argomento:</b>", w);
    topRow->addWidget(topLbl);

    m_a1TopicCmb = new QComboBox(w);
    m_a1TopicCmb->setObjectName("settingCombo");
    const int nA1 = static_cast<int>(sizeof(kA1)/sizeof(kA1[0]));
    for (int i = 0; i < nA1; ++i)
        m_a1TopicCmb->addItem(QString::fromUtf8(kA1[i].name), i);
    topRow->addWidget(m_a1TopicCmb, 1);

    auto* btnAiExplain = new QPushButton(
        "\xf0\x9f\xa4\x96  Spiega con AI", w);
    btnAiExplain->setObjectName("actionBtn");
    btnAiExplain->setProperty("analisiLevel", 1);
    connect(btnAiExplain, &QPushButton::clicked, this, &MatematicaPage::onA1AiClicked);
    topRow->addWidget(btnAiExplain);
    lay->addLayout(topRow);

    /* ─── Splitter orizzontale: Teoria (sx) | Canvas interattivo (dx) ─── */
    auto* hSplit = new QSplitter(Qt::Horizontal, w);
    hSplit->setHandleWidth(5);

    m_a1Theory = new LatexView(hSplit);
    m_a1Theory->setMinimumWidth(180);
    hSplit->addWidget(m_a1Theory);

    m_a1Canvas = new GraficoCanvas(hSplit);
    m_a1Canvas->setMinimumWidth(180);
    hSplit->addWidget(m_a1Canvas);

    hSplit->setStretchFactor(0, 3);  /* teoria */
    hSplit->setStretchFactor(1, 4);  /* canvas */
    lay->addWidget(hSplit, 1);

    /* ─── Riga Risolvi ─── */
    auto* tryRow = new QHBoxLayout;
    tryRow->addWidget(new QLabel("\xf0\x9f\x93\x90  Risolvi:", w));
    m_a1Input = new QLineEdit(w);
    m_a1Input->setPlaceholderText(tr("Espressione SymPy (pre-compilata dall'argomento)"));
    tryRow->addWidget(m_a1Input, 1);
    m_a1TypeCmb = makeAnalisiTypeCmb(w);
    tryRow->addWidget(m_a1TypeCmb);
    auto* btnTry = new QPushButton(
        "\xf0\x9f\x93\x90  Risolvi nella scheda accanto", w);
    btnTry->setObjectName("actionBtn");
    btnTry->setProperty("highlight", "true");
    btnTry->setToolTip(
        "Copia la formula nella scheda \xe2\x80\x9c\xf0\x9f\x93\x90 Risolvi Passi\xe2\x80\x9d "
        "e avvia il calcolo SymPy passo per passo.");
    connect(btnTry, &QPushButton::clicked, this, &MatematicaPage::onA1TryClicked);
    tryRow->addWidget(btnTry);

    auto* btnQuickPlot1 = new QPushButton(
        "\xf0\x9f\x93\x88  Disegna grafico", w);
    btnQuickPlot1->setObjectName("actionBtn");
    btnQuickPlot1->setToolTip(tr("Copia l'espressione nel campo Grafico e traccia subito"));
    connect(btnQuickPlot1, &QPushButton::clicked, this, [this]() {
        if (m_a1Input && m_a1PlotInput)
            m_a1PlotInput->setText(m_a1Input->text());
        onA1PlotClicked();
    });
    tryRow->addWidget(btnQuickPlot1);
    lay->addLayout(tryRow);

    /* ─── Riga Grafico ─── */
    auto* plotRow = new QHBoxLayout;
    plotRow->addWidget(new QLabel("\xf0\x9f\x93\x88  Grafico:", w));
    m_a1PlotInput = new QLineEdit(w);
    m_a1PlotInput->setPlaceholderText(tr("f(x) da disegnare nel canvas"));
    plotRow->addWidget(m_a1PlotInput, 1);
    auto* btnPlot1 = new QPushButton(
        "\xf0\x9f\x93\x88  Disegna grafico", w);
    btnPlot1->setObjectName("actionBtn");
    btnPlot1->setToolTip(tr("Traccia f(x) nel canvas a destra"));
    connect(btnPlot1, &QPushButton::clicked, this, &MatematicaPage::onA1PlotClicked);
    plotRow->addWidget(btnPlot1);
    m_a1RenderCmb = new QComboBox(w);
    m_a1RenderCmb->addItem("\xe2\x80\x94  Linea",   0);
    m_a1RenderCmb->addItem("\xe2\x97\x8f  Punti",   1);
    m_a1RenderCmb->addItem("\xe2\x96\xb2  Area",    2);
    m_a1RenderCmb->setToolTip(tr("Stile rendering 2D"));
    m_a1RenderCmb->setMaximumWidth(110);
    connect(m_a1RenderCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MatematicaPage::onA1RenderChanged);
    plotRow->addWidget(m_a1RenderCmb);
    m_btnA1Expand = new QPushButton("\xe2\x86\x97", w);   /* ↗ */
    m_btnA1Expand->setToolTip(tr("Apri grafico in finestra separata"));
    m_btnA1Expand->setFixedWidth(32);
    connect(m_btnA1Expand, &QPushButton::clicked, this, &MatematicaPage::onA1ExpandClicked);
    plotRow->addWidget(m_btnA1Expand);
    lay->addLayout(plotRow);

    connect(m_a1TopicCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MatematicaPage::onA1TopicChanged);
    onA1TopicChanged();   /* carica il primo argomento */

    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildAnalisi2Tab — studio funzioni Analisi 2
   ══════════════════════════════════════════════════════════════ */
QWidget* MatematicaPage::buildAnalisi2Tab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(10, 8, 10, 8);
    lay->setSpacing(8);

    auto* topRow = new QHBoxLayout;
    auto* topLbl = new QLabel("<b>\xf0\x9f\x93\x99  Argomento:</b>", w);
    topRow->addWidget(topLbl);

    m_a2TopicCmb = new QComboBox(w);
    m_a2TopicCmb->setObjectName("settingCombo");
    const int nA2 = static_cast<int>(sizeof(kA2)/sizeof(kA2[0]));
    for (int i = 0; i < nA2; ++i)
        m_a2TopicCmb->addItem(QString::fromUtf8(kA2[i].name), i);
    topRow->addWidget(m_a2TopicCmb, 1);

    auto* btnAiExplain = new QPushButton(
        "\xf0\x9f\xa4\x96  Spiega con AI", w);
    btnAiExplain->setObjectName("actionBtn");
    connect(btnAiExplain, &QPushButton::clicked, this, &MatematicaPage::onA2AiClicked);
    topRow->addWidget(btnAiExplain);
    lay->addLayout(topRow);

    /* ─── Splitter orizzontale: Teoria (sx) | Canvas interattivo (dx) ─── */
    auto* hSplit2 = new QSplitter(Qt::Horizontal, w);
    hSplit2->setHandleWidth(5);

    m_a2Theory = new LatexView(hSplit2);
    m_a2Theory->setMinimumWidth(180);
    hSplit2->addWidget(m_a2Theory);

    m_a2Canvas = new GraficoCanvas(hSplit2);
    m_a2Canvas->setMinimumWidth(180);
    hSplit2->addWidget(m_a2Canvas);

    hSplit2->setStretchFactor(0, 3);
    hSplit2->setStretchFactor(1, 4);
    lay->addWidget(hSplit2, 1);

    /* ─── Riga Risolvi ─── */
    auto* tryRow = new QHBoxLayout;
    tryRow->addWidget(new QLabel("\xf0\x9f\x93\x90  Risolvi:", w));
    m_a2Input = new QLineEdit(w);
    m_a2Input->setPlaceholderText(tr("Espressione SymPy (pre-compilata dall'argomento)"));
    tryRow->addWidget(m_a2Input, 1);
    m_a2TypeCmb = makeAnalisiTypeCmb(w);
    tryRow->addWidget(m_a2TypeCmb);
    auto* btnTry = new QPushButton(
        "\xf0\x9f\x93\x90  Risolvi nella scheda accanto", w);
    btnTry->setObjectName("actionBtn");
    btnTry->setProperty("highlight", "true");
    btnTry->setToolTip(
        "Copia la formula nella scheda \xe2\x80\x9c\xf0\x9f\x93\x90 Risolvi Passi\xe2\x80\x9d "
        "e avvia il calcolo SymPy passo per passo.");
    connect(btnTry, &QPushButton::clicked, this, &MatematicaPage::onA2TryClicked);
    tryRow->addWidget(btnTry);

    auto* btnQuickPlot = new QPushButton(
        "\xf0\x9f\x93\x88  Disegna grafico", w);
    btnQuickPlot->setObjectName("actionBtn");
    btnQuickPlot->setToolTip(tr("Copia l'espressione nel campo Grafico e traccia subito"));
    connect(btnQuickPlot, &QPushButton::clicked, this, [this]() {
        if (m_a2Input && m_a2PlotInput)
            m_a2PlotInput->setText(m_a2Input->text());
        onA2PlotClicked();
    });
    tryRow->addWidget(btnQuickPlot);
    lay->addLayout(tryRow);

    /* ─── Riga Grafico ─── */
    auto* plotRow = new QHBoxLayout;
    plotRow->addWidget(new QLabel("\xf0\x9f\x93\x88  Grafico:", w));
    m_a2PlotInput = new QLineEdit(w);
    m_a2PlotInput->setPlaceholderText(tr("f(x,y) — con 'y' \xe2\x86\x92 3D"));
    plotRow->addWidget(m_a2PlotInput, 1);
    auto* btnPlot2 = new QPushButton(
        "\xf0\x9f\x93\x88  Disegna grafico", w);
    btnPlot2->setObjectName("actionBtn");
    btnPlot2->setToolTip(tr("Traccia f(x) o f(x,y) nel canvas a destra"));
    connect(btnPlot2, &QPushButton::clicked, this, &MatematicaPage::onA2PlotClicked);
    plotRow->addWidget(btnPlot2);
    m_a2RenderCmb = new QComboBox(w);
    m_a2RenderCmb->addItem("\xf0\x9f\x94\xb5  Punti",       GraficoCanvas::Points3D);
    m_a2RenderCmb->addItem("\xf0\x9f\x95\xb8  Wireframe",   GraficoCanvas::Wireframe3D);
    m_a2RenderCmb->addItem("\xe2\x96\xa0  Superficie",      GraficoCanvas::Surface3D);
    m_a2RenderCmb->addItem("\xf0\x9f\x9f\xa6  Solido",      GraficoCanvas::Solid3D);
    m_a2RenderCmb->setToolTip(tr("Stile rendering 3D"));
    m_a2RenderCmb->setMaximumWidth(130);
    connect(m_a2RenderCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MatematicaPage::onA2RenderChanged);
    plotRow->addWidget(m_a2RenderCmb);
    m_btnA2Expand = new QPushButton("\xe2\x86\x97", w);   /* ↗ */
    m_btnA2Expand->setToolTip(tr("Apri grafico in finestra separata"));
    m_btnA2Expand->setFixedWidth(32);
    connect(m_btnA2Expand, &QPushButton::clicked, this, &MatematicaPage::onA2ExpandClicked);
    plotRow->addWidget(m_btnA2Expand);
    lay->addLayout(plotRow);

    connect(m_a2TopicCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MatematicaPage::onA2TopicChanged);
    onA2TopicChanged();

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Slot Analisi 1
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onA1TopicChanged()
{
    if (!m_a1TopicCmb || !m_a1Theory) return;
    const int idx = m_a1TopicCmb->currentIndex();
    const int nA1 = static_cast<int>(sizeof(kA1)/sizeof(kA1[0]));
    if (idx < 0 || idx >= nA1) return;
    m_a1Theory->setLatexHtml(QString::fromUtf8(kA1[idx].html));
    if (m_a1Input)     m_a1Input->setText(QString::fromUtf8(kA1[idx].ex));
    if (m_a1PlotInput) m_a1PlotInput->setText(QString::fromUtf8(kA1[idx].plotEx));
    if (m_a1TypeCmb) {
        const QString t = QString::fromUtf8(kA1[idx].type);
        const int ti = m_a1TypeCmb->findData(t);
        if (ti >= 0) m_a1TypeCmb->setCurrentIndex(ti);
    }
    if (m_a1Canvas && kA1[idx].plotEx && kA1[idx].plotEx[0]) {
        m_a1Canvas->setCartesian(sympyToCanvas(QString::fromUtf8(kA1[idx].plotEx)), -8.0, 8.0);
        m_a1Canvas->setType(GraficoCanvas::Cartesian);
    }
}

void MatematicaPage::onA1TryClicked()
{
    if (!m_a1Input || !m_a1TypeCmb || !m_solveInput || !m_solveCmb) return;
    const QString expr = m_a1Input->text().trimmed();
    if (expr.isEmpty()) return;
    m_solveInput->setText(expr);
    const QString tipo = m_a1TypeCmb->currentData().toString();
    for (int i = 0; i < m_solveCmb->count(); ++i) {
        if (m_solveCmb->itemText(i) == tipo) {
            m_solveCmb->setCurrentIndex(i); break;
        }
    }

    /* ── Evidenzia zona limite nel canvas Analisi 1 ──────────────────── */
    if (tipo == "Limite" && m_a1Canvas) {
        /* Estrae il punto x→a dall'espressione: formati supportati:
           "f, var, val[, dir]"  oppure  "f as/per var->val" */
        QString xStr;
        static const QRegularExpression reArrow(
            R"((.+?)\s+(?:as|per)\s+\w+\s*->\s*(.+))");
        auto m = reArrow.match(expr);
        if (m.hasMatch()) {
            xStr = m.captured(2).trimmed();
        } else {
            const QStringList parts = expr.split(',');
            if (parts.size() >= 3) xStr = parts[2].trimmed();
        }

        if (!xStr.isEmpty()) {
            bool ok = false;
            const double xVal = xStr.toDouble(&ok);
            if (ok) {
                /* Auto-plot la funzione centrata intorno a x=a */
                const QStringList parts = expr.split(',');
                const QString func = parts.isEmpty() ? QString() : parts[0].trimmed();
                if (!func.isEmpty()) {
                    const double halfW = 6.0;
                    m_a1Canvas->setCartesian(sympyToCanvas(func),
                                             xVal - halfW, xVal + halfW);
                    m_a1Canvas->setType(GraficoCanvas::Cartesian);
                    if (m_a1PlotInput) m_a1PlotInput->setText(func);
                }
                m_a1Canvas->setLimitHighlight(xVal);
                m_limitCanvas = m_a1Canvas;
            }
        }
    } else if (m_limitCanvas) {
        /* Se cambiamo tipo, rimuoviamo il highlight precedente */
        m_limitCanvas->clearLimitHighlight();
        m_limitCanvas = nullptr;
    }

    if (m_solveTabIdx >= 0) m_tabs->setCurrentIndex(m_solveTabIdx);
    onSolveClicked();
}

void MatematicaPage::onA1AiClicked()
{
    if (!m_ai || m_aiRunning) return;
    const int idx = m_a1TopicCmb ? m_a1TopicCmb->currentIndex() : 0;
    const int nA1 = static_cast<int>(sizeof(kA1)/sizeof(kA1[0]));
    if (idx < 0 || idx >= nA1) return;
    const QString topicName = QString::fromUtf8(kA1[idx].name);

    if (m_modelCombo) {
        const QString sel = m_modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }
    m_aiRunning = true;
    clearOutput();
    appendOutput(QString("\xf0\x9f\x93\x98  Spiegazione AI: %1\n%2\n\n")
                 .arg(topicName, QString(topicName.length()+20, '-')));
    setStatus("\xf0\x9f\xa4\x96  AI in elaborazione...");

    const QString sys =
        "Sei un professore universitario di Analisi Matematica 1. "
        "Spiega in italiano il seguente argomento in modo chiaro e didattico: "
        "1) definizione formale con la notazione corretta; "
        "2) teoremi e propriet\xc3\xa0 fondamentali; "
        "3) due esempi svolti passo per passo; "
        "4) un esercizio proposto con soluzione. "
        "Usa OBBLIGATORIAMENTE la notazione LaTeX per le formule: "
        "\\(...\\) per inline, \\[...\\] per display (es. \\(x^2\\), \\[\\int_a^b f\\,dx\\]). "
        "Rispondi SOLO in italiano.";
    const QString user = QString("Argomento: %1").arg(topicName);

    delete m_aiAnalisiHolder;
    m_aiAnalisiHolder = new QObject(this);
    connect(m_ai, &AiClient::token,    m_aiAnalisiHolder,
            [this](const QString& t){ onAnalisiAiToken(t); });
    connect(m_ai, &AiClient::finished, m_aiAnalisiHolder,
            [this](const QString& f){ onAnalisiAiFinished(f); });
    connect(m_ai, &AiClient::error,    m_aiAnalisiHolder,
            [this](const QString& e){ onAnalisiAiError(e); });
    m_ai->chat(P::prependMathKnowledge(sys), user);
}

/* ══════════════════════════════════════════════════════════════
   Slot Analisi 2
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onA2TopicChanged()
{
    if (!m_a2TopicCmb || !m_a2Theory) return;
    const int idx = m_a2TopicCmb->currentIndex();
    const int nA2 = static_cast<int>(sizeof(kA2)/sizeof(kA2[0]));
    if (idx < 0 || idx >= nA2) return;
    m_a2Theory->setLatexHtml(QString::fromUtf8(kA2[idx].html));
    if (m_a2Input)     m_a2Input->setText(QString::fromUtf8(kA2[idx].ex));
    if (m_a2PlotInput) m_a2PlotInput->setText(QString::fromUtf8(kA2[idx].plotEx));
    if (m_a2TypeCmb) {
        const QString t = QString::fromUtf8(kA2[idx].type);
        const int ti = m_a2TypeCmb->findData(t);
        if (ti >= 0) m_a2TypeCmb->setCurrentIndex(ti);
    }
    if (m_a2Canvas && kA2[idx].plotEx && kA2[idx].plotEx[0]) {
        const QString plotExpr = QString::fromUtf8(kA2[idx].plotEx);
        static const QRegularExpression reY(R"(\by\b)");
        if (plotExpr.contains(reY)) {
            const auto pts = buildSurface3D(plotExpr);
            constexpr int kGridN = 40;
            m_a2Canvas->setScatter3D(pts, kGridN);
            m_a2Canvas->setType(GraficoCanvas::Scatter3D);
        } else {
            m_a2Canvas->setCartesian(sympyToCanvas(plotExpr), -8.0, 8.0);
            m_a2Canvas->setType(GraficoCanvas::Cartesian);
        }
    }
}

void MatematicaPage::onA2TryClicked()
{
    if (!m_a2Input || !m_a2TypeCmb || !m_solveInput || !m_solveCmb) return;
    const QString expr = m_a2Input->text().trimmed();
    if (expr.isEmpty()) return;
    m_solveInput->setText(expr);
    const QString tipo = m_a2TypeCmb->currentData().toString();
    for (int i = 0; i < m_solveCmb->count(); ++i) {
        if (m_solveCmb->itemText(i) == tipo) {
            m_solveCmb->setCurrentIndex(i); break;
        }
    }
    if (m_solveTabIdx >= 0) m_tabs->setCurrentIndex(m_solveTabIdx);
    onSolveClicked();
}

void MatematicaPage::onA2AiClicked()
{
    if (!m_ai || m_aiRunning) return;
    const int idx = m_a2TopicCmb ? m_a2TopicCmb->currentIndex() : 0;
    const int nA2 = static_cast<int>(sizeof(kA2)/sizeof(kA2[0]));
    if (idx < 0 || idx >= nA2) return;
    const QString topicName = QString::fromUtf8(kA2[idx].name);

    if (m_modelCombo) {
        const QString sel = m_modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }
    m_aiRunning = true;
    clearOutput();
    appendOutput(QString("\xf0\x9f\x93\x99  Spiegazione AI: %1\n%2\n\n")
                 .arg(topicName, QString(topicName.length()+20, '-')));
    setStatus("\xf0\x9f\xa4\x96  AI in elaborazione...");

    const QString sys =
        "Sei un professore universitario di Analisi Matematica 2. "
        "Spiega in italiano il seguente argomento in modo chiaro e didattico: "
        "1) definizione formale; 2) teoremi chiave; "
        "3) due esempi svolti passo per passo; "
        "4) un esercizio proposto con soluzione. "
        "Usa OBBLIGATORIAMENTE la notazione LaTeX per le formule: "
        "\\(...\\) per inline, \\[...\\] per display. "
        "Usa notazione vettoriale italiana. Rispondi SOLO in italiano.";
    const QString user = QString("Argomento: %1").arg(topicName);

    delete m_aiAnalisiHolder;
    m_aiAnalisiHolder = new QObject(this);
    connect(m_ai, &AiClient::token,    m_aiAnalisiHolder,
            [this](const QString& t){ onAnalisiAiToken(t); });
    connect(m_ai, &AiClient::finished, m_aiAnalisiHolder,
            [this](const QString& f){ onAnalisiAiFinished(f); });
    connect(m_ai, &AiClient::error,    m_aiAnalisiHolder,
            [this](const QString& e){ onAnalisiAiError(e); });
    m_ai->chat(P::prependMathKnowledge(sys), user);
}

/* ══════════════════════════════════════════════════════════════
   Slot AI Analisi (one-shot shared)
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onAnalisiAiToken(const QString& tok)
{
    appendOutput(tok);
}

void MatematicaPage::onAnalisiAiFinished(const QString& full)
{
    delete m_aiAnalisiHolder;
    m_aiAnalisiHolder = nullptr;
    m_aiRunning = false;
    setStatus("\xe2\x9c\x85  Spiegazione AI completata.");
    if (m_latexOut && m_latexOut->isVisible() && !full.isEmpty()) {
        /* toHtmlEscaped() non tocca \ — i delimitatori \(...\) e \[...\] rimangono
           intatti; KaTeX auto-render li trova e renderizza le formule. */
        m_latexOut->setLatexHtml(
            "<div class='ai-out'>" + full.toHtmlEscaped().replace("\n", "<br>") + "</div>");
    }
}

void MatematicaPage::onAnalisiAiError(const QString& msg)
{
    delete m_aiAnalisiHolder;
    m_aiAnalisiHolder = nullptr;
    m_aiRunning = false;
    appendOutput("\n\xe2\x9d\x8c  Errore AI: " + msg + "\n");
    setStatus("\xe2\x9d\x8c  Errore AI.");
    LogBus::post("\xe2\x9d\x8c Matematica: Errore AI analisi: " + msg);
}

/* ══════════════════════════════════════════════════════════════
   Grafici Analisi — GraficoCanvas nativo (zero subprocess)
   ══════════════════════════════════════════════════════════════ */

/* Converte notazione SymPy → FormulaParser:
   **  → ^
   log → ln  (SymPy usa log per log naturale; il parser usa ln)
   Exp → exp (case insensitive non necessario, SymPy emette minuscolo) */
QString MatematicaPage::sympyToCanvas(const QString& expr)
{
    QString r = expr;
    r.replace("**", "^");
    /* log(x) SymPy = logaritmo naturale → ln(x) per il parser */
    static const QRegularExpression reLog(R"(\blog\s*\()");
    r.replace(reLog, "ln(");
    return r;
}

/* Genera una griglia 3D da f(x,y): rimpiazza \by\b con il valore numerico
   e usa FormulaParser (già in uso nel canvas) per valutare f_y(x). */
QVector<GraficoCanvas::Pt3D> MatematicaPage::buildSurface3D(const QString& exprSym)
{
    const QString expr = sympyToCanvas(exprSym);
    constexpr int N = 40;
    constexpr double R = 4.0;
    constexpr double step = 2.0 * R / (N - 1);
    static const QRegularExpression reY(R"(\by\b)");

    /* Griglia sempre completa N×N — punti invalidi ricevono z=NaN.
       Questo preserva la topologia richiesta per Wireframe e Surface. */
    const double kNaN = std::numeric_limits<double>::quiet_NaN();
    QVector<GraficoCanvas::Pt3D> pts(N * N);
    for (int iy = 0; iy < N; ++iy) {
        const double yVal = -R + iy * step;
        QString formulaY = expr;
        formulaY.replace(reY, QString::number(yVal, 'g', 8));
        FormulaParser fp(formulaY);
        for (int ix = 0; ix < N; ++ix) {
            const double xVal = -R + ix * step;
            double z = kNaN;
            if (fp.ok()) {
                const double v = fp.eval(xVal);
                if (std::isfinite(v) && std::abs(v) < 1e5)
                    z = v;
            }
            pts[iy * N + ix] = {xVal, yVal, z, {}};
        }
    }
    return pts;
}

void MatematicaPage::onA1PlotClicked()
{
    if (!m_a1Canvas || !m_a1PlotInput) return;
    const QString expr = m_a1PlotInput->text().trimmed();
    if (expr.isEmpty()) return;
    const int mode = m_a1RenderCmb ? m_a1RenderCmb->currentIndex() : 0;
    if (mode == 1) {
        FormulaParser fp(sympyToCanvas(expr));
        m_a1Canvas->setScatter(fp.sample(-8.0, 8.0, 300));
        m_a1Canvas->setType(GraficoCanvas::ScatterXY);
    } else if (mode == 2) {
        FormulaParser fp(sympyToCanvas(expr));
        m_a1Canvas->setLine({fp.sample(-8.0, 8.0, 300)}, {});
        m_a1Canvas->setType(GraficoCanvas::Area);
    } else {
        m_a1Canvas->setCartesian(sympyToCanvas(expr), -8.0, 8.0);
        m_a1Canvas->setType(GraficoCanvas::Cartesian);
    }
}

void MatematicaPage::onA1RenderChanged(int /*idx*/)
{
    if (!m_a1PlotInput || m_a1PlotInput->text().trimmed().isEmpty()) return;
    onA1PlotClicked();
}

void MatematicaPage::onA2PlotClicked()
{
    if (!m_a2Canvas || !m_a2PlotInput) return;
    const QString expr = m_a2PlotInput->text().trimmed();
    if (expr.isEmpty()) return;
    static const QRegularExpression reY(R"(\by\b)");
    if (expr.contains(reY)) {
        constexpr int kGridN = 40;
        const auto pts = buildSurface3D(expr);
        m_a2Canvas->setScatter3D(pts, kGridN);   /* passa topologia griglia */
        m_a2Canvas->setType(GraficoCanvas::Scatter3D);
    } else {
        m_a2Canvas->setCartesian(sympyToCanvas(expr), -8.0, 8.0);
        m_a2Canvas->setType(GraficoCanvas::Cartesian);
    }
}

void MatematicaPage::onA2RenderChanged(int idx)
{
    if (!m_a2Canvas || !m_a2RenderCmb) return;
    const int mode = m_a2RenderCmb->itemData(idx).toInt();
    m_a2Canvas->setRenderMode3D(static_cast<GraficoCanvas::RenderMode3D>(mode));
}

/* helper — apre il canvas sorgente in un QDialog non-modale */
static void openCanvasInWindow(GraficoCanvas* src, const QString& title, QWidget* parent)
{
    if (!src) return;
    auto* dlg = new QDialog(parent, Qt::Window);
    dlg->setWindowTitle(title);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(800, 600);
    auto* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(4, 4, 4, 4);
    auto* canvas = new GraficoCanvas(dlg);

    /* copia stato */
    switch (src->currentType()) {
    case GraficoCanvas::Scatter3D:
        canvas->setScatter3D(src->pts3d(), src->grid3dCols());
        canvas->setType(GraficoCanvas::Scatter3D);
        canvas->setRenderMode3D(src->renderMode3D());
        break;
    case GraficoCanvas::ScatterXY:
        canvas->setScatter(src->scatterPts());
        canvas->setType(GraficoCanvas::ScatterXY);
        break;
    case GraficoCanvas::Area:
        canvas->setLine(src->lineSeries(), {});
        canvas->setType(GraficoCanvas::Area);
        break;
    default:
        canvas->setCartesian(src->cartFormula(), src->cartXMin(), src->cartXMax());
        canvas->setType(GraficoCanvas::Cartesian);
        break;
    }
    lay->addWidget(canvas);
    dlg->show();
}

void MatematicaPage::onA1ExpandClicked()
{
    openCanvasInWindow(m_a1Canvas, "Analisi 1 — Grafico", this);
}

void MatematicaPage::onA2ExpandClicked()
{
    openCanvasInWindow(m_a2Canvas, "Analisi 2 — Grafico 3D", this);
}

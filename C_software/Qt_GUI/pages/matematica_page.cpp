#include "matematica_page.h"
#include "../prismalux_paths.h"

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

    /* ── Splitter verticale: strumenti sopra, output sotto ── */
    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->setHandleWidth(4);

    /* ─── PARTE SUPERIORE: schede strumenti ─── */
    m_tabs = new QTabWidget(splitter);
    m_tabs->setObjectName("mainTabs");
    m_tabs->addTab(buildSeqTab(),   "\xf0\x9f\x94\xa2  Sequenza \xe2\x86\x92 Formula");  /* 🔢 */
    m_tabs->addTab(buildConstTab(), "\xcf\x80  Costanti di precisione");
    m_tabs->addTab(buildNthTab(),   "#\xe2\x83\xbf  N-esimo");
    m_tabs->addTab(buildExprTab(),  "\xf0\x9f\xa7\xae  Espressione");                    /* 🧮 */

    /* ─── PARTE INFERIORE: output + controlli ─── */
    auto* outBox = new QWidget(splitter);
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
    btnCopy->setToolTip("Copia tutto l'output negli appunti");
    connect(btnCopy, &QPushButton::clicked, this, [this]{
        QApplication::clipboard()->setText(m_output->toPlainText());
    });
    ctrlRow->addWidget(btnCopy);

    auto* btnClear = new QPushButton("\xf0\x9f\x97\x91  Cancella", outBox);
    btnClear->setObjectName("actionBtn");
    btnClear->setFixedHeight(26);
    connect(btnClear, &QPushButton::clicked, this, [this]{ clearOutput(); });
    ctrlRow->addWidget(btnClear);

    auto* btnStop = new QPushButton("\xe2\x96\xa0  Stop", outBox);
    btnStop->setObjectName("stopBtn");
    btnStop->setFixedHeight(26);
    btnStop->setProperty("execFull", btnStop->text());
    btnStop->setProperty("execIcon", QString::fromUtf8("\xe2\x96\xa0"));
    btnStop->setProperty("execText", "Stop");
    connect(btnStop, &QPushButton::clicked, this, [this]{ stopPython(); });
    ctrlRow->addWidget(btnStop);

    outLay->addLayout(ctrlRow);

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

    splitter->addWidget(m_tabs);
    splitter->addWidget(outBox);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);

    root->addWidget(splitter, 1);
}

/* ══════════════════════════════════════════════════════════════
   buildSeqTab — Sequenza di numeri → formula matematica
   ══════════════════════════════════════════════════════════════ */
QWidget* MatematicaPage::buildSeqTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(8);

    lay->addWidget(new QLabel(
        "<b>Inserisci una sequenza di numeri separati da virgole o spazi:</b>", w));

    auto* inputRow = new QHBoxLayout;
    m_seqInput = new QLineEdit(w);
    m_seqInput->setPlaceholderText("es. 1, 4, 9, 16, 25   oppure   1 1 2 3 5 8 13");
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

    /* Risultato rilevamento locale */
    m_seqResult = new QLabel("", w);
    m_seqResult->setObjectName("statusLabel");
    m_seqResult->setWordWrap(true);
    lay->addWidget(m_seqResult);

    /* Opzioni: termini + selezione modello AI */
    auto* optRow = new QHBoxLayout;
    optRow->addWidget(new QLabel("Suggerisci i prossimi", w));
    m_nextTerms = new QSpinBox(w);
    m_nextTerms->setRange(1, 20);
    m_nextTerms->setValue(5);
    optRow->addWidget(m_nextTerms);
    optRow->addWidget(new QLabel("termini", w));

    optRow->addSpacing(16);

    optRow->addWidget(new QLabel("con:", w));
    m_modelCombo = new QComboBox(w);
    m_modelCombo->setObjectName("settingCombo");
    m_modelCombo->setMinimumWidth(180);
    m_modelCombo->setToolTip("Modello usato da \"Analizza con AI\"");
    /* Voce iniziale — verrà sostituita quando arrivano i modelli */
    const QString curModel = m_ai ? m_ai->model() : QString();
    m_modelCombo->addItem(curModel.isEmpty() ? "(caricamento...)" : curModel, curModel);
    optRow->addWidget(m_modelCombo, 1);

    /* Pulsante refresh modelli */
    auto* btnRefresh = new QPushButton("\xf0\x9f\x94\x84", w);   /* 🔄 */
    btnRefresh->setObjectName("navBtn");
    btnRefresh->setFixedSize(26, 26);
    btnRefresh->setToolTip("Aggiorna lista modelli");
    optRow->addWidget(btnRefresh);

    /* Score per modelli matematici: piu' alto = migliore per matematica */
    auto mathScore = [](const QString& name) -> int {
        const QString n = name.toLower();
        if (n.contains("qwen2.5-math") || n.contains("qwen2_5-math")) return 100;
        if (n.contains("mathstral") || n.contains("math"))             return 90;
        if (n.contains("deepseek-r1"))  return 85;  /* ragionamento chain-of-thought */
        if (n.contains("qwq"))          return 80;
        if (n.contains("phi-4") || n.contains("phi4")) return 75;
        if (n.contains("deepseek") && !n.contains("coder")) return 60;
        if (n.contains("qwen")     && !n.contains("coder")) return 50;
        return 0;
    };

    auto isEmbed = [](const QString& n) -> bool {
        return n.contains("embed") || n.contains("minilm") ||
               n.contains("rerank") || n.contains("bge-") ||
               n.contains("e5-") || n.contains("-embed");
    };

    /* Helper condiviso: popola m_modelCombo auto-selezionando il miglior modello math */
    auto fillMathCombo = [this, mathScore, isEmbed](const QStringList& list, const QString& cur) {
        m_modelCombo->clear();
        int selIdx    = 0;
        int curIdx    = -1;
        int bestMath  = -1;
        int bestScore = -1;
        for (const QString& mdl : list) {
            if (isEmbed(mdl.toLower())) continue;
            const int pos = m_modelCombo->count();
            /* Etichetta: aggiunge badge per modelli matematici */
            const int sc = mathScore(mdl);
            const QString badge = (sc >= 90) ? "  \xf0\x9f\xa7\xae Ottimizzato Math"
                                : (sc >= 50) ? "  \xe2\x9c\x94 Buono per Math"
                                :              "";
            m_modelCombo->addItem(mdl + badge, mdl);
            if (mdl == cur) curIdx = pos;
            if (sc > bestScore) { bestScore = sc; bestMath = pos; }
        }
        if (m_modelCombo->count() == 0) {
            m_modelCombo->addItem(cur.isEmpty() ? "(nessun modello)" : cur, cur);
            return;
        }
        const bool curIsMath = (curIdx >= 0 && mathScore(cur) >= 50);
        if (bestMath >= 0 && !curIsMath)
            selIdx = bestMath;
        else if (curIdx >= 0)
            selIdx = curIdx;
        m_modelCombo->setCurrentIndex(selIdx);
        const QString chosen = m_modelCombo->currentData().toString();
        if (!chosen.isEmpty() && chosen != cur && m_ai)
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), chosen);
    };

    /* Lambda per popolare la combo (esclude embedding) */
    auto populateModels = [this, fillMathCombo]() {
        if (!m_ai) return;
        const QString cur = m_ai->model();
        auto* holder = new QObject(this);
        connect(m_ai, &AiClient::modelsReady, holder,
                [this, holder, fillMathCombo, cur](const QStringList& list) {
            holder->deleteLater();
            fillMathCombo(list, cur);
        });
        m_ai->fetchModels();
    };

    connect(btnRefresh, &QPushButton::clicked, this, populateModels);

    /* Carica subito la lista modelli */
    if (m_ai) {
        auto* holder = new QObject(this);
        connect(m_ai, &AiClient::modelsReady, holder,
                [this, holder, fillMathCombo](const QStringList& list) {
            holder->deleteLater();
            fillMathCombo(list, m_ai ? m_ai->model() : QString());
        });
        m_ai->fetchModels();
    }

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
    connect(btnLocal, &QPushButton::clicked, this, [this]{
        QString err;
        QVector<double> seq = parseSeq(m_seqInput->text(), err);
        if (!err.isEmpty()) {
            m_seqResult->setText("\xe2\x9d\x8c  " + err);
            return;
        }
        const QString pat = detectPatternLocal(seq);
        m_seqResult->setText("\xf0\x9f\x94\x8d  " + pat);
    });
    btnRow->addWidget(btnLocal);

    auto* btnSympy = new QPushButton(
        "\xcf\x83  Interpola con sympy (preciso)", w);
    btnSympy->setObjectName("actionBtn");
    tagExecM(btnSympy, "\xcf\x83", "Interpola");
    connect(btnSympy, &QPushButton::clicked, this, [this]{
        QString err;
        QVector<double> seq = parseSeq(m_seqInput->text(), err);
        if (!err.isEmpty()) { setStatus("\xe2\x9d\x8c  " + err); return; }
        if (seq.size() < 2) { setStatus("\xe2\x9d\x8c  Inserisci almeno 2 termini."); return; }

        /* Costruisce una lista Python */
        QString listStr = "[";
        for (int i = 0; i < seq.size(); ++i) {
            double v = seq[i];
            /* interi esatti? */
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
            /* Tenta interpolazione polinomiale (indici da 1) */
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
            /* Differenze finite */
            "diffs = [seq[i+1]-seq[i] for i in range(len(seq)-1)]\n"
            "diffs2 = [diffs[i+1]-diffs[i] for i in range(len(diffs)-1)] if len(diffs)>1 else []\n"
            "print(f'Prime differenze:  {diffs}')\n"
            "if diffs2: print(f'Seconde differenze: {diffs2}')\n"
        ).arg(listStr).arg(nxt);

        clearOutput();
        appendOutput("\xcf\x83  Analisi sympy in corso...\n\n");
        runPython(pyCode);
    });
    btnRow->addWidget(btnSympy);

    auto* btnAI = new QPushButton(
        "\xf0\x9f\xa4\x96  Analizza con AI (spiega + storia)", w);
    btnAI->setObjectName("actionBtn");
    btnAI->setProperty("highlight", "true");
    tagExecM(btnAI, "\xf0\x9f\xa4\x96", "Analizza AI");
    connect(btnAI, &QPushButton::clicked, this, [this]{
        QString err;
        QVector<double> seq = parseSeq(m_seqInput->text(), err);
        if (!err.isEmpty()) { setStatus("\xe2\x9d\x8c  " + err); return; }
        if (seq.size() < 2) { setStatus("\xe2\x9d\x8c  Inserisci almeno 2 termini."); return; }
        runAiSequence(m_seqInput->text().trimmed(), m_nextTerms->value());
    });
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
    connect(btnCalc, &QPushButton::clicked, this, [this]{ runConstant(); });
    btnRow->addWidget(btnCalc);

    auto* btnAll = new QPushButton("Tutte le costanti (100 cifre)", w);
    btnAll->setObjectName("actionBtn");
    connect(btnAll, &QPushButton::clicked, this, [this]{
        const QString py = R"(
from mpmath import mp, pi, e, phi, sqrt, euler, log, catalan
mp.dps = 110
consts = [
    ('\xcf\x80  pi', mp.pi),
    ('e   numero di Eulero', mp.e),
    ('\xcf\x86  sezione aurea', mp.phi),
    ('\xe2\x88\x9a2  radice di 2', mp.sqrt(2)),
    ('\xe2\x88\x9a3  radice di 3', mp.sqrt(3)),
    ('\xce\xb3   Eulero-Mascheroni', mp.euler),
    ('ln2 logaritmo naturale di 2', mp.log(2)),
    ('C   costante di Catalan', mp.catalan),
]
for nome, val in consts:
    s = mp.nstr(val, 100, strip_zeros=False)
    print(f'{nome}')
    print(f'  {s}')
    print()
)";
        clearOutput();
        appendOutput("\xcf\x80  Calcolo costanti a 100 cifre...\n\n");
        runPython(py);
    });
    btnRow->addWidget(btnAll);
    lay->addLayout(btnRow);

    /* Nota informativa */
    auto* note = new QLabel(
        "<small>Usa <b>mpmath</b> (installato) — "
        "precisione arbitraria. "
        "Per \xce\xb1 > 10 000 cifre considera che il calcolo pu\xc3\xb2 richiedere decine di secondi.</small>", w);
    note->setWordWrap(true);
    lay->addWidget(note);
    lay->addStretch(1);
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
    m_nthInput->setPlaceholderText("es. 1000000  (un milione)");
    m_nthInput->setText("100");
    grid->addWidget(m_nthInput, 1, 1);
    lay->addLayout(grid);

    /* Descrizione dinamica del tipo selezionato */
    auto* descLbl = new QLabel("", w);
    descLbl->setObjectName("statusLabel");
    descLbl->setWordWrap(true);

    auto updateDesc = [this, descLbl]{
        const QString k = m_nthType->currentData().toString();
        if (k == "pi_digit")
            descLbl->setText("Restituisce la N-esima cifra decimale di \xcf\x80 (dopo il punto). "
                             "Es. N=1 \xe2\x86\x92 1, N=2 \xe2\x86\x92 4, N=3 \xe2\x86\x92 1...");
        else if (k == "e_digit")
            descLbl->setText("N-esima cifra decimale di e. Es. N=1 \xe2\x86\x92 7, N=2 \xe2\x86\x92 1...");
        else if (k == "prime")
            descLbl->setText("Il primo con indice N. p(1)=2, p(2)=3, p(3)=5... "
                             "(sympy per N fino a ~10 000 000)");
        else if (k == "fib")
            descLbl->setText("F(1)=1, F(2)=1, F(3)=2, F(4)=3, F(5)=5... "
                             "Anche per N molto grandi (mpmath).");
        else if (k == "fact")
            descLbl->setText("N! — fattoriale. 1!=1, 5!=120, 100!=93326215443944..."
                             " (precisione arbitraria).");
        else if (k == "pow2")
            descLbl->setText("2^N. Anche per N molto grandi (migliaia di cifre).");
        else if (k == "pi_block")
            descLbl->setText("Le prime N cifre di \xcf\x80 come blocco continuo "
                             "(includa la parte intera: 3.14159...).");
        else if (k == "phi_block")
            descLbl->setText("Le prime N cifre di \xcf\x86 (sezione aurea).");
    };
    connect(m_nthType, &QComboBox::currentIndexChanged, this, updateDesc);
    updateDesc();
    lay->addWidget(descLbl);

    auto* btnRow = new QHBoxLayout;
    auto* btnCalc = new QPushButton("#\xe2\x83\xbf  Calcola", w);
    btnCalc->setObjectName("actionBtn");
    btnCalc->setProperty("highlight", "true");
    btnCalc->setProperty("execFull", btnCalc->text());
    btnCalc->setProperty("execIcon", QString::fromUtf8("#\xe2\x83\xbf"));
    btnCalc->setProperty("execText", "Calcola");
    connect(btnCalc, &QPushButton::clicked, this, [this]{ runNth(); });
    btnRow->addWidget(btnCalc);
    btnRow->addStretch(1);
    lay->addLayout(btnRow);

    lay->addStretch(1);
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
        const QString exprStr = ex.expr;
        connect(btn, &QPushButton::clicked, this, [this, exprStr]{
            m_exprInput->setText(exprStr);
            runExpr();
        });
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
    connect(btnEval, &QPushButton::clicked, this, [this]{ runExpr(); });
    btnRow->addWidget(btnEval);

    auto* btnSimplify = new QPushButton("\xe2\x99\xbe  Semplifica (sympy)", w);
    btnSimplify->setObjectName("actionBtn");
    btnSimplify->setProperty("execFull", btnSimplify->text());
    btnSimplify->setProperty("execIcon", QString::fromUtf8("\xe2\x99\xbe"));
    btnSimplify->setProperty("execText", "Semplifica");
    connect(btnSimplify, &QPushButton::clicked, this, [this]{
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
    });
    btnRow->addWidget(btnSimplify);

    connect(m_exprInput, &QLineEdit::returnPressed, this, [this]{ runExpr(); });
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
        "print(f'Cifre calcolate: {len(s.replace(\".\",\"\").replace(\"-\",\"\").lstrip(\"0\").__len__())} (appross.)')\n"
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

/* ══════════════════════════════════════════════════════════════
   runExpr — valuta espressione con sympy + mpmath
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::runExpr()
{
    const QString expr = m_exprInput->text().trimmed();
    if (expr.isEmpty()) return;
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

    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::token, holder, [this](const QString& tok){
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(tok);
        m_output->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, holder, [this, holder](const QString&){
        holder->deleteLater();
        m_aiRunning = false;
        setStatus("\xe2\x9c\x85  Analisi AI completata.");
    });
    connect(m_ai, &AiClient::error, holder, [this, holder](const QString& msg){
        holder->deleteLater();
        m_aiRunning = false;
        appendOutput("\n\xe2\x9d\x8c  Errore AI: " + msg);
        setStatus("\xe2\x9d\x8c  Errore AI.");
    });

    m_ai->chat(sys.arg(nextN), user);
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

    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this]{
        const QString txt = QString::fromUtf8(m_proc->readAllStandardOutput());
        appendOutput(txt);
    });
    connect(m_proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus){
        if (code != 0) {
            setStatus(QString("\xe2\x9d\x8c  Python uscito con codice %1.").arg(code));
        } else {
            setStatus("\xe2\x9c\x85  Calcolo completato.");
        }
        m_proc->deleteLater();
        m_proc = nullptr;
    });

    setStatus("\xe2\x8f\xb3  Calcolo in corso...");
    m_proc->start(P::findPython(), QStringList{"-c", code});
    if (!m_proc->waitForStarted(3000)) {
        setStatus("\xe2\x9d\x8c  Python non trovato nel PATH. Installa Python da python.org");
        m_proc->deleteLater();
        m_proc = nullptr;
    }
}

void MatematicaPage::stopPython()
{
    if (m_proc) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
        m_proc->deleteLater();
        m_proc = nullptr;
        setStatus("\xe2\x96\xa0  Calcolo interrotto.");
    }
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
        R"((?<![,\d])-?\d+(?:[.,]\d+)?(?!\d))");

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

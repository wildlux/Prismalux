/* main_math_bool.cpp — Algebra Booleana: UI + slot */
#include "main_math.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../ai_client.h"
#include "../dpi_utils.h"
#include "../widgets/formula_parser.h"
#include "main_graph.h"
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
#include <QToolButton>
#include <QMenu>
#include <QScrollArea>
#include <QStackedWidget>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <cmath>
#include <limits>

namespace P = PrismaluxPaths;

QWidget* MatematicaPage::buildBoolTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(8);

    /* ── Campo espressione booleana ── */
    lay->addWidget(new QLabel(
        tr("<b>Valuta espressioni booleane (SymPy):</b>"), w));
    lay->addWidget(new QLabel(
        "<small>Sintassi: <code>A & B</code> (AND)  <code>A | B</code> (OR)  "
        "<code>~A</code> (NOT)  <code>A ^ B</code> (XOR)  "
        "<code>Implies(A,B)</code>  <code>Equivalent(A,B)</code></small>", w));

    auto* inputRow = new QHBoxLayout;
    m_boolInput = new QLineEdit(w);
    m_boolInput->setPlaceholderText(
        tr("es.  (A & B) | (~A & C)   oppure   ~(A | B)   oppure   A ^ B"));
    inputRow->addWidget(m_boolInput, 1);
    lay->addLayout(inputRow);

    /* ── Pulsanti operatori rapidi ── */
    auto* opGroup = new QGroupBox(tr("Inserisci operatore / funzione"), w);
    auto* opLay   = new QHBoxLayout(opGroup);
    opLay->setSpacing(4);
    struct Op { const char* lbl; const char* ins; const char* tip; };
    const Op ops[] = {
        { "AND (\xe2\x88\xa7)",  " & ",          "AND logico (Congiunzione)" },
        { "OR (\xe2\x88\xa8)",   " | ",          "OR logico (Disgiunzione)" },
        { "NOT (\xc2\xac)",      "~",            "NOT logico (Negazione)" },
        { "XOR (\xe2\x8a\x95)",  " ^ ",          "XOR — Disgiunzione esclusiva" },
        { "NAND",                "Nand(,)",      "NAND — NOT(A AND B)" },
        { "NOR",                 "Nor(,)",       "NOR — NOT(A OR B)" },
        { "XNOR",                "Xnor(,)",      "XNOR — Equivalenza (A XNOR B = NOT XOR)" },
        { "IMP (\xe2\x86\x92)",  "Implies(,)",   "Implicazione logica A→B" },
        { "EQ (\xe2\x86\x94)",   "Equivalent(,)","Equivalenza A↔B" },
        { "True",                " True ",       "Costante vera" },
        { "False",               " False ",      "Costante falsa" },
    };
    for (const auto& op : ops) {
        auto* btn = new QPushButton(QString::fromUtf8(op.lbl), opGroup);
        btn->setObjectName("navBtn");
        btn->setToolTip(QString::fromUtf8(op.tip));
        btn->setProperty("boolIns", QString::fromUtf8(op.ins));
        connect(btn, &QPushButton::clicked, this, [this, btn](){
            if (!m_boolInput) return;
            m_boolInput->insert(btn->property("boolIns").toString());
            m_boolInput->setFocus();
        });
        opLay->addWidget(btn);
    }
    opLay->addStretch();
    lay->addWidget(opGroup);

    /* ── Pulsanti azione ── */
    auto* btnRow = new QHBoxLayout;
    auto* btnEval = new QPushButton(tr("\xe2\x9c\x94  Valuta"), w);  /* ✔ */
    btnEval->setObjectName("actionBtn");
    btnEval->setProperty("highlight", "true");
    connect(btnEval, &QPushButton::clicked, this, &MatematicaPage::onBoolEvalClicked);
    btnRow->addWidget(btnEval);

    auto* btnTable = new QPushButton(tr("\xf0\x9f\x93\x8b  Tabella di verità"), w);  /* 📋 */
    btnTable->setObjectName("actionBtn");
    connect(btnTable, &QPushButton::clicked, this, &MatematicaPage::onBoolTruthTableClicked);
    btnRow->addWidget(btnTable);

    auto* btnSimp = new QPushButton(tr("\xe2\x99\xbe  Semplifica"), w);  /* ♾ */
    btnSimp->setObjectName("actionBtn");
    connect(btnSimp, &QPushButton::clicked, this, &MatematicaPage::onBoolSimplifyClicked);
    btnRow->addWidget(btnSimp);

    connect(m_boolInput, &QLineEdit::returnPressed,
            this, &MatematicaPage::onBoolEvalClicked);
    btnRow->addStretch();
    lay->addLayout(btnRow);

    /* ── Output ── */
    m_boolOutput = new QTextBrowser(w);
    m_boolOutput->setObjectName("chatLog");
    m_boolOutput->setMaximumHeight(dpiScale(120));
    m_boolOutput->setOpenLinks(false);
    lay->addWidget(m_boolOutput);

    /* ── Separatore ── */
    auto* sep = new QFrame(w);
    sep->setFrameShape(QFrame::HLine);
    lay->addWidget(sep);

    /* ── Sezione Teoremi / Leggi — 2 colonne ── */
    lay->addWidget(new QLabel(tr("<b>Leggi dell'Algebra Booleana (Boole, De Morgan, Shannon)</b>"), w));

    static const char* kStyle =
        "<style>"
        "body{font-size:12px}"
        "h3{color:#1d4ed8;margin:8px 0 3px 0;font-size:12px;font-weight:bold}"
        "table{border-collapse:collapse;width:100%;margin:3px 0}"
        "th{background:#dbeafe;color:#1e3a8a;padding:3px 6px;text-align:left;"
        "border:1px solid #bfdbfe;font-size:11px}"
        "td{padding:2px 6px;border:1px solid #e2e8f0;font-size:11px;color:#1e293b}"
        "tr:nth-child(even){background:#f8fafc}"
        "code{color:#0f766e;font-size:11px;background:#f0fdf4;"
        "padding:1px 3px;border-radius:3px}"
        ".law{background:#eff6ff;border-left:3px solid #3b82f6;padding:2px 6px;"
        "margin:2px 0;border-radius:0 4px 4px 0;font-family:monospace;color:#1e293b;"
        "font-size:11px}"
        "</style>";

    /* Colonna sinistra: Operatori, Identità, Proprietà, De Morgan */
    auto* colLeft = new QTextBrowser(w);
    colLeft->setObjectName("chatLog");
    colLeft->setOpenLinks(false);
    /* Entita' HTML per simboli matematici — evita il parsing greedy di \xNNL
       dove L e' cifra hex (A-F): "\xacA" verrebbe letto come 0xACA out-of-range. */
    colLeft->setHtml(QString(kStyle) +
        "<h3>\xf0\x9f\x94\xa2 Operatori fondamentali</h3>"
        "<table><tr><th>Simbolo</th><th>Nome</th><th>SymPy</th><th>Descrizione</th></tr>"
        "<tr><td>&#8743; &amp;</td><td>AND</td><td><code>A &amp; B</code></td><td>Vero se entrambi veri</td></tr>"
        "<tr><td>&#8744; |</td><td>OR</td><td><code>A | B</code></td><td>Vero se almeno uno vero</td></tr>"
        "<tr><td>&#172; ~</td><td>NOT</td><td><code>~A</code></td><td>Negazione</td></tr>"
        "<tr><td>&#8853; ^</td><td>XOR</td><td><code>A ^ B</code></td><td>Esattamente uno vero</td></tr>"
        "<tr><td>&#8593;</td><td>NAND</td><td><code>Nand(A,B)</code></td><td>NOT(A AND B)</td></tr>"
        "<tr><td>&#8595;</td><td>NOR</td><td><code>Nor(A,B)</code></td><td>NOT(A OR B)</td></tr>"
        "<tr><td>&#8596;</td><td>XNOR</td><td><code>Xnor(A,B)</code></td><td>NOT(A XOR B)</td></tr>"
        "<tr><td>&#8594;</td><td>Implica</td><td><code>Implies(A,B)</code></td><td>&#172;A OR B</td></tr>"
        "</table>"

        "<h3>\xf0\x9f\x93\x8b Identit\xc3\xa0 fondamentali</h3>"
        "<div class='law'>Identit\xc3\xa0: A &#8743; 1 = A &nbsp; A &#8744; 0 = A</div>"
        "<div class='law'>Dominanza: A &#8743; 0 = 0 &nbsp; A &#8744; 1 = 1</div>"
        "<div class='law'>Idempotenza: A &#8743; A = A &nbsp; A &#8744; A = A</div>"
        "<div class='law'>Involuzione: &#172;&#172;A = A</div>"
        "<div class='law'>Complemento: A &#8743; &#172;A = 0 &nbsp; A &#8744; &#172;A = 1</div>"

        "<h3>\xf0\x9f\x94\x84 Propriet\xc3\xa0 strutturali</h3>"
        "<div class='law'>Commutativa AND: A &#8743; B = B &#8743; A</div>"
        "<div class='law'>Commutativa OR: A &#8744; B = B &#8744; A</div>"
        "<div class='law'>Associativa AND: A &#8743; (B &#8743; C) = (A &#8743; B) &#8743; C</div>"
        "<div class='law'>Associativa OR: A &#8744; (B &#8744; C) = (A &#8744; B) &#8744; C</div>"
        "<div class='law'>Distrib. AND/OR: A &#8743; (B &#8744; C) = (A&#8743;B) &#8744; (A&#8743;C)</div>"
        "<div class='law'>Distrib. OR/AND: A &#8744; (B &#8743; C) = (A&#8744;B) &#8743; (A&#8744;C)</div>"

        "<h3>\xf0\x9f\x8f\x9b Leggi di De Morgan</h3>"
        "<div class='law'><b>1\xc2\xaa:</b> &#172;(A &#8743; B) = &#172;A &#8744; &#172;B</div>"
        "<div class='law'><b>2\xc2\xaa:</b> &#172;(A &#8744; B) = &#172;A &#8743; &#172;B</div>"
        "<div class='law'><b>Gen. AND:</b> &#172;(A&#8321;&#8743;&hellip;&#8743;A&#8345;) = &#172;A&#8321;&#8744;&hellip;&#8744;&#172;A&#8345;</div>"
        "<div class='law'><b>Gen. OR:</b> &#172;(A&#8321;&#8744;&hellip;&#8744;A&#8345;) = &#172;A&#8321;&#8743;&hellip;&#8743;&#172;A&#8345;</div>"
    );

    /* Colonna destra: Assorbimento, Consenso, XOR, Shannon, Forme, Implicazione */
    auto* colRight = new QTextBrowser(w);
    colRight->setObjectName("chatLog");
    colRight->setOpenLinks(false);
    colRight->setHtml(QString(kStyle) +
        "<h3>\xf0\x9f\x9f\xa3 Assorbimento</h3>"
        "<div class='law'>A &#8743; (A &#8744; B) = A</div>"
        "<div class='law'>A &#8744; (A &#8743; B) = A</div>"
        "<div class='law'>A &#8743; (&#172;A &#8744; B) = A &#8743; B</div>"
        "<div class='law'>A &#8744; (&#172;A &#8743; B) = A &#8744; B</div>"

        "<h3>\xf0\x9f\x94\xa2 Teorema di consenso</h3>"
        "<div class='law'>A&#8743;B &#8744; &#172;A&#8743;C &#8744; B&#8743;C = A&#8743;B &#8744; &#172;A&#8743;C</div>"
        "<div class='law'>(A&#8744;B)&#8743;(&#172;A&#8744;C)&#8743;(B&#8744;C) = (A&#8744;B)&#8743;(&#172;A&#8744;C)</div>"

        "<h3>\xf0\x9f\xaa\x9e XOR &mdash; Propriet\xc3\xa0</h3>"
        "<div class='law'>A &#8853; 0 = A &nbsp;&nbsp; A &#8853; 1 = &#172;A</div>"
        "<div class='law'>A &#8853; A = 0 &nbsp;&nbsp; A &#8853; &#172;A = 1</div>"
        "<div class='law'>Commutativa: A &#8853; B = B &#8853; A</div>"
        "<div class='law'>Associativa: A &#8853; (B &#8853; C) = (A &#8853; B) &#8853; C</div>"
        "<div class='law'>Distrib.: A&#8743;(B&#8853;C) = (A&#8743;B)&#8853;(A&#8743;C)</div>"
        "<div class='law'>De Morgan: &#172;(A&#8853;B) = A&#8853;&#172;B = XNOR</div>"

        "<h3>\xf0\x9f\x94\x84 Shannon (Espansione di Boole)</h3>"
        "<div class='law'>f(A,...) = A&#8743;f(1,...) &#8744; &#172;A&#8743;f(0,...)</div>"
        "<div class='law'>f(A,...) = (A&#8744;f(0,...)) &#8743; (&#172;A&#8744;f(1,...))</div>"

        "<h3>\xf0\x9f\x93\x8b Forme canoniche</h3>"
        "<div class='law'><b>SOP</b> (Somma Mintermini): &#8721;m &mdash; OR di AND</div>"
        "<div class='law'><b>POS</b> (Prodotto Maxtermini): &#8719;m &mdash; AND di OR</div>"
        "<div class='law'>SOP &#8594; POS: applica De Morgan ai maxtermini</div>"

        "<h3>\xf0\x9f\x9b\xa1 Implicazione e Equivalenza</h3>"
        "<div class='law'>A &#8594; B = &#172;A &#8744; B</div>"
        "<div class='law'>A &#8596; B = (A&#8594;B) &#8743; (B&#8594;A)</div>"
        "<div class='law'>Contrapposizione: A&#8594;B = &#172;B&#8594;&#172;A</div>"
        "<div class='law'>Modus Ponens: A, A&#8594;B &#8866; B</div>"
        "<div class='law'>Modus Tollens: &#172;B, A&#8594;B &#8866; &#172;A</div>"
    );

    /* Altezza minima colonne: garantisce che le tabelle siano visibili */
    colLeft->setMinimumHeight(dpiScale(380));
    colRight->setMinimumHeight(dpiScale(380));

    /* Layout 2 colonne affiancate con scroll indipendente */
    auto* colRow = new QWidget(w);
    auto* colLay = new QHBoxLayout(colRow);
    colLay->setContentsMargins(0, 0, 0, 0);
    colLay->setSpacing(6);
    colLay->addWidget(colLeft,  1);
    colLay->addWidget(colRight, 1);
    lay->addWidget(colRow, 1);

    /* Scroll area verticale: se la finestra è bassa, tutto il contenuto resta raggiungibile */
    auto* scroll = new QScrollArea;
    scroll->setWidget(w);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    return scroll;
}

/* ── Slot: valuta espressione booleana con SymPy ── */
void MatematicaPage::onBoolEvalClicked()
{
    if (!m_boolInput || !m_boolOutput) return;
    const QString expr = m_boolInput->text().trimmed();
    if (expr.isEmpty()) return;

    const QString code =
        "from sympy.logic.boolalg import *\n"
        "from sympy import symbols\n"
        "# Definisce automaticamente simboli A-Z\n"
        "import string\n"
        "for _c in string.ascii_uppercase:\n"
        "    exec(f'{_c} = symbols(\"{_c}\")')\n"
        "try:\n"
        "    _r = " + expr + "\n"
        "    print('Risultato:', _r)\n"
        "    _s = simplify_logic(_r)\n"
        "    if str(_s) != str(_r): print('Semplificato:', _s)\n"
        "except Exception as e:\n"
        "    print('Errore:', e)\n";

    QString err;
    const QString out = _runPythonSync(code, err);
    m_boolOutput->setHtml(
        "<pre style='font-family:monospace;color:#86efac'>" +
        (err.isEmpty() ? out : out + "\n<span style='color:#f87171'>" + err + "</span>") +
        "</pre>");
}

/* ── Slot: genera tabella di verità ── */
void MatematicaPage::onBoolTruthTableClicked()
{
    if (!m_boolInput || !m_boolOutput) return;
    const QString expr = m_boolInput->text().trimmed();
    if (expr.isEmpty()) return;

    const QString code =
        "from sympy.logic.boolalg import *\n"
        "from sympy import symbols\n"
        "import string, re\n"
        "for _c in string.ascii_uppercase:\n"
        "    exec(f'{_c} = symbols(\"{_c}\")')\n"
        "try:\n"
        "    _e = " + expr + "\n"
        "    _vars = sorted(_e.free_symbols, key=str)\n"
        "    if not _vars:\n"
        "        print('Costante:', _e)\n"
        "    else:\n"
        "        hdr = ' | '.join(str(v) for v in _vars) + ' | f'\n"
        "        sep = '-' * len(hdr)\n"
        "        print(hdr); print(sep)\n"
        "        for vals in __import__('itertools').product([0,1], repeat=len(_vars)):\n"
        "            sub = {v:bool(b) for v,b in zip(_vars,vals)}\n"
        "            res = _e.subs(sub)\n"
        "            row = ' | '.join(str(int(b)) for b in vals)+' | '+str(int(bool(res)))\n"
        "            print(row)\n"
        "except Exception as e:\n"
        "    print('Errore:', e)\n";

    QString err;
    const QString out = _runPythonSync(code, err);
    m_boolOutput->setHtml(
        "<pre style='font-family:monospace;color:#e2e8f0;line-height:1.5'>" +
        out.toHtmlEscaped() +
        (err.isEmpty() ? "" : "\n<span style='color:#f87171'>" + err.toHtmlEscaped() + "</span>") +
        "</pre>");
}

/* ── Slot: semplifica espressione booleana ── */
void MatematicaPage::onBoolSimplifyClicked()
{
    if (!m_boolInput || !m_boolOutput) return;
    const QString expr = m_boolInput->text().trimmed();
    if (expr.isEmpty()) return;

    const QString code =
        "from sympy.logic.boolalg import *\n"
        "from sympy import symbols\n"
        "import string\n"
        "for _c in string.ascii_uppercase:\n"
        "    exec(f'{_c} = symbols(\"{_c}\")')\n"
        "try:\n"
        "    _e = " + expr + "\n"
        "    _s  = simplify_logic(_e, form='dnf')\n"
        "    _sc = simplify_logic(_e, form='cnf')\n"
        "    print('Originale: ', _e)\n"
        "    print('DNF (SOP):  ', _s)\n"
        "    print('CNF (POS):  ', _sc)\n"
        "    if str(_s) == 'True':  print('=> La funzione e SEMPRE VERA (tautologia)')\n"
        "    if str(_s) == 'False': print('=> La funzione e SEMPRE FALSA (contraddizione)')\n"
        "except Exception as e:\n"
        "    print('Errore:', e)\n";

    QString err;
    const QString out = _runPythonSync(code, err);
    m_boolOutput->setHtml(
        "<pre style='font-family:monospace;color:#86efac;line-height:1.5'>" +
        out.toHtmlEscaped() +
        (err.isEmpty() ? "" : "\n<span style='color:#f87171'>" + err.toHtmlEscaped() + "</span>") +
        "</pre>");
}

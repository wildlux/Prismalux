#include "matematica_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QApplication>
#include <QClipboard>
#include <QTextCursor>
#include <QTimer>
#include <QScroller>
#include <QScrollerProperties>
#include <QRandomGenerator>

/* ── 52 formule per il pulsante 🔀 (da gui/pages/main_math.cpp) ─ */
namespace {
struct SolveEx { const char* expr; const char* tipo; const char* desc; };
static const SolveEx kSolveExamples[] = {
    /* Equazioni (10) */
    {"x^3 - 6*x^2 + 11*x - 6 = 0",       "Equazione",       "Terzo grado - radici 1, 2, 3"},
    {"x^4 - 13*x^2 + 36 = 0",             "Equazione",       "Biquadratica - radici +-2, +-3"},
    {"x^2 + x + 1 = 0",                   "Equazione",       "Delta < 0 - radici complesse"},
    {"2*x^3 + x^2 - 5*x + 2 = 0",         "Equazione",       "Cubica con radici 1/2, 1, -2"},
    {"x^4 - 5*x^2 + 4 = 0",               "Equazione",       "Biquadratica - radici +-1, +-2"},
    {"x^3 + 3*x^2 - 4 = 0",               "Equazione",       "Cubica con fattore (x-1)"},
    {"x^2 - 2*sqrt(3)*x + 3 = 0",         "Equazione",       "Delta = 0 - radice doppia sqrt(3)"},
    {"x^5 - x = 0",                       "Equazione",       "Quintico fattorizzabile - 5 radici"},
    {"x^2 - 5 = 0",                       "Equazione",       "Radici irrazionali +-sqrt(5)"},
    {"x^3 - x = 0",                       "Equazione",       "Cubica - radici 0, +-1"},
    /* Disequazioni (5) */
    {"x^2 - 5*x + 6 > 0",                 "Disequazione",    "Parabola > 0 su (-oo,2) U (3,+oo)"},
    {"x^3 - x > 0",                       "Disequazione",    "Cubica positiva in (-1,0) U (1,+oo)"},
    {"(x - 1)*(x + 2)*(x - 3) < 0",       "Disequazione",    "Tre radici 1, -2, 3"},
    {"x^4 - 5*x^2 + 4 <= 0",             "Disequazione",    "Biquadratica <= 0 su [-2,-1] U [1,2]"},
    {"x^2 - 4 >= 0",                      "Disequazione",    "Parabola >= 0 su (-oo,-2] U [2,+oo)"},
    /* Derivate (10) */
    {"sin(x^2 + 1), x",                   "Derivata",        "Catena: D[sin(x^2+1)] = 2x*cos(x^2+1)"},
    {"x^3*exp(x), x, 2",                  "Derivata",        "Derivata seconda di x^3*e^x"},
    {"atan(x), x",                        "Derivata",        "D[arctan(x)] = 1/(1+x^2)"},
    {"log(x^2 + 1)*sin(x), x",            "Derivata",        "Prodotto ln(x^2+1)*sin(x)"},
    {"(x^2 + 1)/(x^3 - 1), x",            "Derivata",        "Derivata di funzione razionale"},
    {"sin(x)^2, x",                       "Derivata",        "D[sin^2(x)] = sin(2x)"},
    {"sqrt(x^2 + 1), x",                  "Derivata",        "D[sqrt(x^2+1)] = x/sqrt(x^2+1)"},
    {"exp(sin(x)), x",                    "Derivata",        "Catena doppia e^{sin(x)}"},
    {"log(x + sqrt(x^2 + 1)), x",         "Derivata",        "D[arcsinh(x)] = 1/sqrt(x^2+1)"},
    {"x^2*exp(-x), x",                    "Derivata",        "Prodotto polinomio x esponenziale"},
    /* Integrali (10) */
    {"x*exp(x), x",                       "Integrale",       "Per parti: x*e^x -> (x-1)*e^x + C"},
    {"sin(x), x, 0, pi",                  "Integrale",       "Definito esatto = 2"},
    {"exp(-x^2), x, 0, oo",               "Integrale",       "Gaussiano = sqrt(pi)/2"},
    {"log(x), x",                         "Integrale",       "Per parti: x*ln(x)-x + C"},
    {"x^2*sin(x), x",                     "Integrale",       "Per parti iterata"},
    {"1/(x^2 + 1), x",                    "Integrale",       "= arctan(x) + C"},
    {"sqrt(1 - x^2), x, -1, 1",           "Integrale",       "Area semicerchio = pi/2"},
    {"sin(x)^2, x",                       "Integrale",       "Formula di riduzione"},
    {"1/(x^2 - 1), x",                    "Integrale",       "Frazioni parziali"},
    {"x^3*exp(x), x",                     "Integrale",       "Per parti ripetuta - 4 passaggi"},
    /* Limiti (9) */
    {"sin(x)/x, x, 0",                    "Limite",          "Limite notevole -> 1"},
    {"(1 - cos(x))/x^2, x, 0",            "Limite",          "Forma 0/0 -> 1/2"},
    {"(x^2 + 1)/(x^2 - 1), x, oo",        "Limite",          "Razionale -> 1"},
    {"(exp(x) - 1)/x, x, 0",              "Limite",          "(e^x-1)/x -> 1"},
    {"(1 + 1/x)^x, x, oo",                "Limite",          "Def. di e"},
    {"x*log(x), x, 0",                    "Limite",          "Forma 0*oo -> 0"},
    {"(sqrt(x + 1) - 1)/x, x, 0",         "Limite",          "Forma 0/0 -> 1/2"},
    {"(x^3 - 8)/(x - 2), x, 2",           "Limite",          "Fattorizza cubo -> 12"},
    {"sin(3*x)/sin(5*x), x, 0",           "Limite",          "Rapporto seni -> 3/5"},
    /* Semplificazioni (8) */
    {"sin(x)^2 + cos(x)^2",               "Semplificazione", "Identita' Pitagora = 1"},
    {"(x^3 - 1)/(x - 1)",                 "Semplificazione", "Differenza cubi -> x^2+x+1"},
    {"series(exp(x), x, 0, 6)",            "Semplificazione", "Taylor di e^x ordine 5"},
    {"factor(x^4 - 5*x^2 + 4)",           "Semplificazione", "Fattorizzazione quartica"},
    {"expand((x + 1)^6)",                 "Semplificazione", "Binomio di Newton ordine 6"},
    {"simplify(tan(x)^2 + 1 - 1/cos(x)^2)", "Semplificazione", "Identita' sec^2(x)"},
    {"series(sin(x), x, 0, 8)",            "Semplificazione", "Taylor di sin(x) ordine 7"},
    {"factor(x^6 - 1)",                   "Semplificazione", "Fattorizzazione differenza sesta potenza"},
};
static constexpr int kNSolve = static_cast<int>(sizeof(kSolveExamples)/sizeof(kSolveExamples[0]));
} // namespace

static void applyTouchScroll(QScrollArea* sa)
{
    QScroller::grabGesture(sa->viewport(), QScroller::TouchGesture);
    QScroller* qs = QScroller::scroller(sa->viewport());
    QScrollerProperties sp = qs->scrollerProperties();
    sp.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor, 0.0);
    sp.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.0);
    sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    qs->setScrollerProperties(sp);
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
MatematicaPage::MatematicaPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    setObjectName("MatematicaPage");

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    applyTouchScroll(scroll);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);
    scroll->setWidget(inner);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    /* ── Titolo ── */
    auto* title = new QLabel(
        QString::fromUtf8("\xcf\x80  Laboratorio Matematico"), inner);
    QFont tf = title->font();
    tf.setPointSize(15); tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    /* ── Modalità ── */
    auto* modeGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x94\xa2  Modalità di calcolo"), inner);
    modeGroup->setObjectName("SettingsGroup");
    auto* modeVbox = new QVBoxLayout(modeGroup);

    m_modeCombo = new QComboBox(inner);
    m_modeCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x94\xa2") + "  Analisi Sequenza", 0);
    m_modeCombo->addItem(
        QString::fromUtf8("\xcf\x80") + "   Costanti Matematiche", 1);
    m_modeCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x94\xa2") + "  N-esimo Termine", 2);
    m_modeCombo->addItem(
        QString::fromUtf8("\xf0\x9f\xa7\xae") + "  Valuta Espressione", 3);
    m_modeCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x93\x90") + "  Geometria / Trigonometria", 4);
    m_modeCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x93\x88") + "  Statistica / Probabilità", 5);
    m_modeCombo->addItem(
        QString::fromUtf8("\xf0\x9f\x93\x90") + "  Risolvi Passi (52 formule)", 6);
    m_modeCombo->setMinimumHeight(48);
    modeVbox->addWidget(m_modeCombo);
    vbox->addWidget(modeGroup);

    /* ── Input ── */
    auto* inputGroup = new QGroupBox(
        QString::fromUtf8("\xe2\x9c\x8f\xef\xb8\x8f  Input"), inner);
    inputGroup->setObjectName("SettingsGroup");
    auto* inputVbox = new QVBoxLayout(inputGroup);

    m_inputHint = new QLabel("", inner);
    m_inputHint->setStyleSheet("color:#8890a8; font-size:12px;");
    m_inputHint->setWordWrap(true);
    inputVbox->addWidget(m_inputHint);

    m_inputEdit = new QTextEdit(inner);
    m_inputEdit->setMinimumHeight(80);
    m_inputEdit->setMaximumHeight(140);
    inputVbox->addWidget(m_inputEdit);

    /* Esempi rapidi (righe di bottoni touch) */
    m_examplesRow = new QWidget(inner);
    auto* exGrid  = new QGridLayout(m_examplesRow);
    exGrid->setContentsMargins(0, 4, 0, 0);
    exGrid->setSpacing(6);

    /* 6 bottoni esempio, popolati in updateModeUI */
    struct ExBtn { const char* label; const char* value; };
    static const ExBtn seqExamples[] = {
        { "1,1,2,3,5,8",  "1, 1, 2, 3, 5, 8, 13, 21" },
        { "1,4,9,16,25",  "1, 4, 9, 16, 25, 36, 49"  },
        { "2,4,8,16",     "2, 4, 8, 16, 32, 64"      },
        { "1,3,6,10",     "1, 3, 6, 10, 15, 21"      },
        { "π decimali",   "3.14159265358979323846"    },
        { "e decimali",   "2.71828182845904523536"    },
    };

    for (int i = 0; i < 6; ++i) {
        auto* btn = new QPushButton(seqExamples[i].label, m_examplesRow);
        btn->setObjectName("SecondaryBtn");
        btn->setMinimumHeight(40);
        btn->setProperty("mathExample", seqExamples[i].value);
        connect(btn, &QPushButton::clicked,
                this, &MatematicaPage::onExampleClicked);
        exGrid->addWidget(btn, i / 3, i % 3);
    }
    inputVbox->addWidget(m_examplesRow);

    /* Pulsante 🔀 + label info formula (visibili solo in mode 6) */
    m_solveRandomBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x80") + " Formula Casuale  (" +
        QString::number(kNSolve) + " disponibili)", inner);
    m_solveRandomBtn->setObjectName("PrimaryBtn");
    m_solveRandomBtn->setMinimumHeight(44);
    m_solveRandomBtn->setVisible(false);
    connect(m_solveRandomBtn, &QPushButton::clicked,
            this, &MatematicaPage::onSolveRandomClicked);
    inputVbox->addWidget(m_solveRandomBtn);

    m_solveInfoLbl = new QLabel("", inner);
    m_solveInfoLbl->setObjectName("StatusLabel");
    m_solveInfoLbl->setWordWrap(true);
    m_solveInfoLbl->setVisible(false);
    inputVbox->addWidget(m_solveInfoLbl);

    vbox->addWidget(inputGroup);

    /* ── Bottoni azione ── */
    auto* btnRow = new QHBoxLayout;
    m_calcBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\xa7\xae  Calcola / Analizza"), inner);
    m_calcBtn->setObjectName("PrimaryBtn");
    m_calcBtn->setMinimumHeight(56);
    QFont bf = m_calcBtn->font();
    bf.setBold(true); bf.setPointSize(13);
    m_calcBtn->setFont(bf);

    m_stopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9"), inner);
    m_stopBtn->setObjectName("StopBtn");
    m_stopBtn->setFixedSize(56, 56);
    m_stopBtn->setEnabled(false);

    m_copyBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x8b"), inner);
    m_copyBtn->setObjectName("SecondaryBtn");
    m_copyBtn->setFixedSize(56, 56);

    m_clearBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x97\x91"), inner);
    m_clearBtn->setObjectName("SecondaryBtn");
    m_clearBtn->setFixedSize(56, 56);

    btnRow->addWidget(m_calcBtn, 1);
    btnRow->addWidget(m_stopBtn);
    btnRow->addWidget(m_copyBtn);
    btnRow->addWidget(m_clearBtn);
    vbox->addLayout(btnRow);

    /* ── Progress / status ── */
    m_progressBar = new QProgressBar(inner);
    m_progressBar->setRange(0, 0);
    m_progressBar->setFixedHeight(4);
    m_progressBar->setVisible(false);
    vbox->addWidget(m_progressBar);

    m_statusLbl = new QLabel("", inner);
    m_statusLbl->setStyleSheet("color:#8890a8; font-size:12px;");
    m_statusLbl->setVisible(false);
    vbox->addWidget(m_statusLbl);

    /* ── Output ── */
    auto* outGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x84  Risultato"), inner);
    outGroup->setObjectName("SettingsGroup");
    auto* outVbox = new QVBoxLayout(outGroup);

    m_output = new QTextEdit(inner);
    m_output->setReadOnly(true);
    m_output->setMinimumHeight(280);
    m_output->setPlaceholderText(
        "Il risultato apparirà qui.\n\n"
        "Seleziona la modalità, inserisci l'input e premi Calcola / Analizza.");
    outVbox->addWidget(m_output);
    vbox->addWidget(outGroup, 1);

    /* ── Connessioni ── */
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MatematicaPage::onModeChanged);
    connect(m_calcBtn,   &QPushButton::clicked,
            this, &MatematicaPage::onCalcClicked);
    connect(m_stopBtn,   &QPushButton::clicked,
            this, &MatematicaPage::onStopClicked);
    connect(m_copyBtn,   &QPushButton::clicked,
            this, &MatematicaPage::onCopyClicked);
    connect(m_clearBtn,  &QPushButton::clicked,
            this, &MatematicaPage::onClearClicked);

    updateModeUI(0);
}

/* ══════════════════════════════════════════════════════════════
   updateModeUI — aggiorna hint e placeholder in base alla modalità
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::updateModeUI(int idx)
{
    struct ModeInfo {
        const char* hint;
        const char* placeholder;
    };
    static const ModeInfo modes[] = {
        { /* 0 Sequenza */
          "Inserisci i termini separati da virgola. "
          "L'AI rileverà il pattern e prevederà i prossimi termini.",
          "Esempio: 1, 1, 2, 3, 5, 8, 13, 21  (Fibonacci)\n"
          "oppure: 2, 4, 8, 16, 32  (geometrica)\n"
          "oppure: 1, 4, 9, 16, 25  (quadrati perfetti)"
        },
        { /* 1 Costanti */
          "Specifica quale costante e quante cifre decimali vuoi.",
          "Esempi:\n"
          "\"π con 50 cifre decimali\"\n"
          "\"e con 30 cifre decimali\"\n"
          "\"φ (rapporto aureo) con 20 cifre\"\n"
          "\"√2 con 40 cifre decimali\""
        },
        { /* 2 N-esimo */
          "Specifica il tipo di sequenza e il valore di N.",
          "Esempi:\n"
          "\"100° numero primo\"\n"
          "\"50° numero di Fibonacci\"\n"
          "\"10° numero triangolare\"\n"
          "\"200° cifra decimale di π\""
        },
        { /* 3 Espressione */
          "Scrivi l'espressione matematica. "
          "L'AI la valuterà e semplificherà.",
          "Esempi:\n"
          "\"sqrt(2) + sin(pi/4)\"\n"
          "\"(3^10 + 5!) / (2^8 - 1)\"\n"
          "\"integrale di x^2 * sin(x) dx\"\n"
          "\"limite di (1 + 1/n)^n per n → ∞\""
        },
        { /* 4 Geometria */
          "Descrivi il problema geometrico o trigonometrico.",
          "Esempi:\n"
          "\"Area di un triangolo con lati 5, 7, 9\"\n"
          "\"Altezza di un triangolo rettangolo con ipotenusa 13 e cateto 5\"\n"
          "\"Seno di 75° in forma esatta\"\n"
          "\"Volume di una sfera di raggio 3\""
        },
        { /* 5 Statistica */
          "Descrivi il problema statistico o probabilistico.",
          "Esempi:\n"
          "\"Media, varianza e deviazione standard di: 4, 7, 13, 2, 1, 9\"\n"
          "\"Probabilità di ottenere almeno un 6 in 3 lanci di dado\"\n"
          "\"Distribuzione binomiale: n=10, p=0.3, k≥5\"\n"
          "\"Correlazione tra [1,2,3] e [4,5,6]\""
        },
    };

    if (idx < 0 || idx >= 7) return;

    const bool isSolve = (idx == 6);
    m_examplesRow->setVisible(!isSolve);
    if (m_solveRandomBtn) m_solveRandomBtn->setVisible(isSolve);
    if (m_solveInfoLbl)   m_solveInfoLbl->setVisible(isSolve);

    if (idx < 6) {
        m_inputHint->setText(modes[idx].hint);
        m_inputEdit->setPlaceholderText(modes[idx].placeholder);
    } else {
        m_inputHint->setText(
            "Inserisci un'equazione, disequazione, derivata, integrale o limite. "
            "L'AI spiegherà ogni passaggio in dettaglio.");
        m_inputEdit->setPlaceholderText(
            "Esempi:\n"
            "x^2 - 5*x + 6 = 0   (equazione)\n"
            "sin(x)/x, x, 0       (limite)\n"
            "x*exp(x), x          (derivata)\n"
            "Oppure usa 🔀 Formula Casuale");
    }
}

void MatematicaPage::onModeChanged(int idx) { updateModeUI(idx); }

/* ══════════════════════════════════════════════════════════════
   buildSystemPrompt — prompt di sistema per ogni modalità
   ══════════════════════════════════════════════════════════════ */
QString MatematicaPage::buildSystemPrompt(int idx, const QString& input) const
{
    switch (idx) {
    case 0:
        return
            "Sei un matematico esperto. "
            "Analizza questa sequenza numerica: " + input + "\n"
            "1. Identifica il pattern (aritmetica, geometrica, Fibonacci, polinomiale, ecc.)\n"
            "2. Scrivi la formula generale del termine n-esimo\n"
            "3. Calcola i prossimi 5 termini\n"
            "4. Spiega il ragionamento in modo chiaro\n"
            "Mostra tutti i passaggi matematici.";

    case 1:
        return
            "Sei un matematico esperto in analisi numerica. "
            "Calcola con alta precisione: " + input + "\n"
            "Mostra il valore numerico con le cifre richieste. "
            "Se pertinente, spiega la definizione e le proprietà principali della costante. "
            "Usa la notazione matematica corretta.";

    case 2:
        return
            "Sei un matematico esperto. "
            "Calcola: " + input + "\n"
            "Mostra il risultato numerico preciso. "
            "Spiega il metodo di calcolo e la definizione della sequenza. "
            "Se possibile, fornisci la formula generale.";

    case 3:
        return
            "Sei un matematico esperto in calcolo simbolico. "
            "Valuta e analizza questa espressione matematica: " + input + "\n"
            "1. Calcola il valore numerico (con alta precisione se possibile)\n"
            "2. Semplifica l'espressione dove possibile\n"
            "3. Spiega ogni passaggio\n"
            "4. Indica le proprietà matematiche coinvolte\n"
            "Usa la notazione LaTeX dove utile: ad es. \\frac{a}{b}.";

    case 4:
        return
            "Sei un matematico esperto in geometria e trigonometria. "
            "Risolvi questo problema: " + input + "\n"
            "1. Identifica il tipo di problema\n"
            "2. Scrivi le formule applicabili\n"
            "3. Mostra tutti i passaggi numerici\n"
            "4. Fornisci il risultato con le unità di misura appropriate\n"
            "Usa schizzi ASCII se aiutano la comprensione.";

    case 5:
        return
            "Sei un esperto di statistica e probabilità. "
            "Risolvi questo problema: " + input + "\n"
            "1. Identifica il tipo di distribuzione o analisi richiesta\n"
            "2. Applica le formule corrette\n"
            "3. Mostra tutti i calcoli intermedi\n"
            "4. Interpreta il risultato in linguaggio semplice\n"
            "Se stai calcolando statistiche descrittive, mostra: media, mediana, moda, varianza, dev.std.";

    case 6:
    default:
        return
            "Sei un professore di matematica. Risolvi PASSO PER PASSO:\n\n"
            + input + "\n\n"
            "Per ogni passo: mostra l'operazione, spiega il motivo, mostra il risultato intermedio.\n"
            "Usa la notazione matematica corretta. "
            "Evidenzia chiaramente la soluzione finale.";
    }
}

/* ══════════════════════════════════════════════════════════════
   Slot
   ══════════════════════════════════════════════════════════════ */
void MatematicaPage::onCalcClicked()
{
    const QString input = m_inputEdit->toPlainText().trimmed();
    if (input.isEmpty()) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + "  Inserisci un valore nel campo input.");
        return;
    }
    if (m_busy || m_ai->busy()) return;

    m_busy = true;
    m_fullText.clear();
    m_output->clear();

    m_calcBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_progressBar->setVisible(true);
    m_statusLbl->setText(
        QString::fromUtf8("\xf0\x9f\xa7\xae") + "  Calcolo in corso...");
    m_statusLbl->setVisible(true);

    const int    idx = m_modeCombo->currentIndex();
    const QString sys = buildSystemPrompt(idx, input);

    m_tokConn = connect(m_ai, &AiClient::token,
                        this, &MatematicaPage::onToken);
    m_finConn = connect(m_ai, &AiClient::finished,
                        this, &MatematicaPage::onFinished);
    m_errConn = connect(m_ai, &AiClient::error,
                        this, &MatematicaPage::onError);

    const QString userMsg = (idx == 6)
        ? "Risolvi passo per passo: " + input
        : "Analizza: " + input;
    m_ai->chat(sys, userMsg);
}

void MatematicaPage::onStopClicked()
{
    m_ai->abort();
}

void MatematicaPage::onToken(const QString& t)
{
    if (!m_busy) return;
    m_fullText += t;
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(t);
    m_output->moveCursor(QTextCursor::End);
}

void MatematicaPage::onFinished(const QString& full)
{
    if (!m_busy) return;
    disconnect(m_tokConn);
    disconnect(m_finConn);
    disconnect(m_errConn);

    if (!full.isEmpty()) m_fullText = full;
    m_busy = false;
    m_calcBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_progressBar->setVisible(false);
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9c\x85") + "  Calcolo completato.");
}

void MatematicaPage::onError(const QString& e)
{
    if (!m_busy) return;
    disconnect(m_tokConn);
    disconnect(m_finConn);
    disconnect(m_errConn);

    m_busy = false;
    m_calcBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_progressBar->setVisible(false);
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9d\x8c") + "  Errore: " + e);
    m_output->append(
        "\n" + QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + e);
}

void MatematicaPage::onCopyClicked()
{
    const QString txt = m_output->toPlainText().trimmed();
    if (txt.isEmpty()) return;
    QApplication::clipboard()->setText(txt);
    m_copyBtn->setText(QString::fromUtf8("\xe2\x9c\x85"));
    QTimer::singleShot(2000, this, &MatematicaPage::onCopyRestore);
}

void MatematicaPage::onCopyRestore()
{
    if (m_copyBtn)
        m_copyBtn->setText(QString::fromUtf8("\xf0\x9f\x93\x8b"));
}

void MatematicaPage::onClearClicked()
{
    m_output->clear();
    m_statusLbl->setVisible(false);
}

void MatematicaPage::onExampleClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const QString ex = btn->property("mathExample").toString();
    if (!ex.isEmpty())
        m_inputEdit->setPlainText(ex);
}

void MatematicaPage::onSolveRandomClicked()
{
    const int idx = static_cast<int>(
        QRandomGenerator::global()->bounded(static_cast<quint32>(kNSolve)));
    const auto& ex = kSolveExamples[idx];
    m_inputEdit->setPlainText(QString::fromUtf8(ex.expr));
    if (m_solveInfoLbl)
        m_solveInfoLbl->setText(
            QString::fromUtf8("\xf0\x9f\x93\x90") + " "
            + ex.tipo + " — " + ex.desc);
    m_modeCombo->setCurrentIndex(6);
}

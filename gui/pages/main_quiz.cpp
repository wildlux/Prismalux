#include "main_quiz.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>
#include <QGroupBox>
#include <QProgressBar>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QMap>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QTextEdit>
#include <QDialog>
#include <QDialogButtonBox>

/* ══════════════════════════════════════════════════════════════
   buildGeneraTab — UI generazione quiz
   ══════════════════════════════════════════════════════════════ */
QWidget* buildGeneraTab(QuizPage* self,
                         QLineEdit*& topicEdit, QSpinBox*& nDomande,
                         QComboBox*& cmbTipo, QComboBox*& cmbDiff,
                         QPushButton*& btnGenera,
                         QPushButton*& btnCopy,
                         QPushButton*& btnGioca,
                         QTextEdit*& output)
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(10);

    /* ── Argomento ── */
    auto* topicRow = new QHBoxLayout;
    topicRow->setSpacing(8);
    auto* topicLbl = new QLabel("Argomento:", w);
    topicLbl->setObjectName("cardDesc");
    topicLbl->setFixedWidth(90);
    topicEdit = new QLineEdit(w);
    topicEdit->setObjectName("chatInput");
    topicEdit->setPlaceholderText(
        "es. Algoritmi di ordinamento, Sistemi operativi, Fotosintesi...");
    topicRow->addWidget(topicLbl);
    topicRow->addWidget(topicEdit, 1);
    lay->addLayout(topicRow);

    /* ── Opzioni in griglia 2x2 ── */
    auto* optCard = new QFrame(w);
    optCard->setObjectName("actionCard");
    auto* optLay = new QGridLayout(optCard);
    optLay->setContentsMargins(14, 10, 14, 10);
    optLay->setSpacing(10);

    optLay->addWidget(new QLabel("Domande:", optCard), 0, 0);
    nDomande = new QSpinBox(optCard);
    nDomande->setRange(3, 20);
    nDomande->setValue(5);
    nDomande->setSuffix("  domande");
    optLay->addWidget(nDomande, 0, 1);

    optLay->addWidget(new QLabel("Tipo:", optCard), 0, 2);
    cmbTipo = new QComboBox(optCard);
    cmbTipo->addItem("Risposta aperta");
    cmbTipo->addItem("Scelta multipla  (A/B/C/D)");
    cmbTipo->addItem("Misto  (aperta + multipla)");
    optLay->addWidget(cmbTipo, 0, 3);

    optLay->addWidget(new QLabel("Difficolt\xc3\xa0:", optCard), 1, 0);
    cmbDiff = new QComboBox(optCard);
    cmbDiff->addItem("Facile  \xe2\x80\x94 concetti base");
    cmbDiff->addItem("Medio  \xe2\x80\x94 applicazione");
    cmbDiff->addItem("Difficile  \xe2\x80\x94 analisi critica");
    cmbDiff->setCurrentIndex(1);
    optLay->addWidget(cmbDiff, 1, 1);

    optLay->setColumnStretch(1, 1);
    optLay->setColumnStretch(3, 1);
    lay->addWidget(optCard);

    /* ── Pulsanti ── */
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    btnGenera = new QPushButton("\xf0\x9f\x8e\xaf  Genera Quiz", w);
    btnGenera->setObjectName("actionBtn");

    btnCopy = new QPushButton("\xf0\x9f\x93\x8b  Copia", w);
    btnCopy->setObjectName("actionBtn");
    btnCopy->setEnabled(false);

    btnGioca = new QPushButton("\xf0\x9f\x8e\xae  Gioca!", w);
    btnGioca->setObjectName("actionBtn");
    btnGioca->setEnabled(false);
    btnGioca->setVisible(false);

    btnRow->addWidget(btnGenera);
    btnRow->addStretch(1);
    btnRow->addWidget(btnGioca);
    btnRow->addWidget(btnCopy);
    lay->addLayout(btnRow);

    /* ── Output ── */
    output = new QTextEdit(w);
    output->setObjectName("chatLog");
    output->setReadOnly(true);
    output->setPlaceholderText(
        "Il quiz generato appare qui...\n\n"
        "Suggerimento: puoi usare l\xe2\x80\x99" "argomento in italiano o in inglese.");
    lay->addWidget(output, 1);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildGiocaTab — quiz interattivo
   ══════════════════════════════════════════════════════════════ */
QWidget* buildGiocaTab(QuizPage* self,
                        QWidget*& giocaPanel,
                        QStackedWidget*& giocaStack,
                        QLabel*& progressLbl,
                        QLabel*& questionLbl,
                        QWidget*& optWidget,
                        QList<QPushButton*>& optBtns,
                        QLabel*& feedbackLbl,
                        QPushButton*& btnNext,
                        QWidget*& openWidget,
                        QTextEdit*& openAnswer,
                        QLabel*& riepilogoLbl,
                        QPushButton*& btnRivedi)
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(12);

    /* stack: 0=quiz in corso, 1=riepilogo */
    giocaStack = new QStackedWidget(w);

    /* ─── Pagina quiz (index 0) ─── */
    giocaPanel = new QWidget;
    auto* qLay = new QVBoxLayout(giocaPanel);
    qLay->setContentsMargins(0, 0, 0, 0);
    qLay->setSpacing(14);

    progressLbl = new QLabel("Domanda 1 / ?", giocaPanel);
    progressLbl->setObjectName("cardDesc");
    progressLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    qLay->addWidget(progressLbl);

    /* testo domanda */
    auto* qCard = new QFrame(giocaPanel);
    qCard->setObjectName("actionCard");
    auto* qCardLay = new QVBoxLayout(qCard);
    qCardLay->setContentsMargins(16, 14, 16, 14);
    questionLbl = new QLabel("", qCard);
    questionLbl->setObjectName("cardTitle");
    questionLbl->setWordWrap(true);
    questionLbl->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    qCardLay->addWidget(questionLbl);
    qLay->addWidget(qCard);

    /* pulsanti opzioni A/B/C/D */
    optWidget = new QWidget(giocaPanel);
    auto* optLay = new QVBoxLayout(optWidget);
    optLay->setContentsMargins(0, 0, 0, 0);
    optLay->setSpacing(8);
    const char* labels[] = {"A", "B", "C", "D"};
    for (int i = 0; i < 4; ++i) {
        auto* btn = new QPushButton(QString(labels[i]) + ") ...", optWidget);
        btn->setObjectName("actionBtn");
        btn->setMinimumHeight(40);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        optLay->addWidget(btn);
        optBtns.append(btn);
        QObject::connect(btn, &QPushButton::clicked, self, &QuizPage::onOptionSelected);
    }
    qLay->addWidget(optWidget);

    /* campo risposta aperta */
    openWidget = new QWidget(giocaPanel);
    auto* openLay = new QVBoxLayout(openWidget);
    openLay->setContentsMargins(0, 0, 0, 0);
    openLay->setSpacing(6);
    auto* openHint = new QLabel("\xf0\x9f\x93\x9d  Domanda aperta \xe2\x80\x94 scrivi la tua risposta:", openWidget);
    openHint->setObjectName("cardDesc");
    openAnswer = new QTextEdit(openWidget);
    openAnswer->setObjectName("chatInput");
    openAnswer->setFixedHeight(80);
    openAnswer->setPlaceholderText("Scrivi qui la tua risposta...");
    openLay->addWidget(openHint);
    openLay->addWidget(openAnswer);
    qLay->addWidget(openWidget);
    openWidget->setVisible(false);

    /* feedback */
    feedbackLbl = new QLabel("", giocaPanel);
    feedbackLbl->setObjectName("cardDesc");
    feedbackLbl->setWordWrap(true);
    feedbackLbl->setAlignment(Qt::AlignCenter);
    feedbackLbl->setVisible(false);
    qLay->addWidget(feedbackLbl);

    /* pulsante avanti */
    auto* nextRow = new QHBoxLayout;
    btnNext = new QPushButton("Avanti \xe2\x86\x92", giocaPanel);
    btnNext->setObjectName("actionBtn");
    btnNext->setVisible(false);
    QObject::connect(btnNext, &QPushButton::clicked, self, &QuizPage::onNextQuestion);
    nextRow->addStretch(1);
    nextRow->addWidget(btnNext);
    qLay->addLayout(nextRow);
    qLay->addStretch(1);

    giocaStack->addWidget(giocaPanel); // index 0

    /* ─── Pagina riepilogo (index 1) ─── */
    auto* riepilogoPage = new QWidget;
    auto* rLay = new QVBoxLayout(riepilogoPage);
    rLay->setContentsMargins(0, 0, 0, 0);
    rLay->setSpacing(16);
    rLay->addStretch(1);
    riepilogoLbl = new QLabel("", riepilogoPage);
    riepilogoLbl->setObjectName("pageTitle");
    riepilogoLbl->setWordWrap(true);
    riepilogoLbl->setAlignment(Qt::AlignCenter);
    rLay->addWidget(riepilogoLbl);
    btnRivedi = new QPushButton(
        "\xf0\x9f\x93\x96  Rivedi errori", riepilogoPage);
    btnRivedi->setObjectName("actionBtn");
    btnRivedi->setVisible(false);
    QObject::connect(btnRivedi, &QPushButton::clicked, self, &QuizPage::onRivediErroriClicked);
    auto* rivediRow = new QHBoxLayout;
    rivediRow->addStretch(1);
    rivediRow->addWidget(btnRivedi);
    rivediRow->addStretch(1);
    rLay->addLayout(rivediRow);
    rLay->addStretch(1);
    giocaStack->addWidget(riepilogoPage); // index 1

    /* ─── Pagina avviso (index 2) — mostrata quando non ci sono domande ─── */
    auto* avvisoPage = new QWidget;
    auto* aLay = new QVBoxLayout(avvisoPage);
    aLay->setContentsMargins(24, 24, 24, 24);
    aLay->addStretch(1);
    auto* avvisoLbl = new QLabel(
        "\xe2\x9a\xa0\xef\xb8\x8f  Per usare questa scheda,\n"
        "prima genera un quiz dalla scheda \"\xf0\x9f\x8e\xaf Genera\".",
        avvisoPage);
    avvisoLbl->setObjectName("cardDesc");
    avvisoLbl->setAlignment(Qt::AlignCenter);
    avvisoLbl->setWordWrap(true);
    aLay->addWidget(avvisoLbl);
    aLay->addStretch(1);
    giocaStack->addWidget(avvisoPage); // index 2

    giocaStack->setCurrentIndex(2); // mostra avviso di default

    lay->addWidget(giocaStack, 1);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildStoricoTab — scroll area con dashboard (riempita da loadDashboard)
   ══════════════════════════════════════════════════════════════ */
QWidget* buildStoricoTab(QWidget*& dashContent)
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(10);

    auto* scroll = new QScrollArea(w);
    scroll->setObjectName("chatLog");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    dashContent = new QWidget;
    dashContent->setLayout(new QVBoxLayout);
    dashContent->layout()->setContentsMargins(0, 0, 0, 0);
    scroll->setWidget(dashContent);
    lay->addWidget(scroll, 1);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildCcnaTab — UI sessione quiz CCNA da SQLite
   ══════════════════════════════════════════════════════════════ */
static QWidget* buildCcnaTab(QuizPage* /*self*/,
                              QuizCcnaDb* db,
                              QComboBox*& temaCombo,
                              QSpinBox*&  nSpin,
                              QPushButton*& startBtn,
                              QLabel*& statusLbl)
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(14);

    /* ── Intestazione ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\x8e\x93  Quiz CCNA 200-301  \xe2\x80\x94  Banca dati 500 domande", w);
    titleLbl->setObjectName("pageTitle");
    lay->addWidget(titleLbl);

    bool avail = db && db->isAvailable();
    int  total = avail ? db->totalQuestions() : 0;

    statusLbl = new QLabel(
        avail
          ? QString("\xe2\x9c\x85  Database caricato: <b>%1</b> domande in %2 temi.")
              .arg(total).arg(avail ? db->temi().size() : 0)
          : "\xe2\x9d\x8c  Database non trovato. Esegui prima:\n"
            "  python3 scripts/genera_quiz_ccna.py",
        w);
    statusLbl->setObjectName("cardDesc");
    statusLbl->setWordWrap(true);
    lay->addWidget(statusLbl);

    if (!avail) {
        lay->addStretch(1);
        startBtn = nullptr; temaCombo = nullptr; nSpin = nullptr;
        return w;
    }

    /* ── Opzioni ── */
    auto* card = new QFrame(w);
    card->setObjectName("actionCard");
    auto* cLay = new QGridLayout(card);
    cLay->setContentsMargins(16, 12, 16, 12);
    cLay->setSpacing(10);

    cLay->addWidget(new QLabel("Tema:", card), 0, 0);
    temaCombo = new QComboBox(card);
    temaCombo->addItem("Tutti i temi", QVariant(QString()));
    for (const QString& t : db->temi())
        temaCombo->addItem(t, t);
    cLay->addWidget(temaCombo, 0, 1);

    cLay->addWidget(new QLabel("Domande:", card), 1, 0);
    nSpin = new QSpinBox(card);
    nSpin->setRange(5, 50);
    nSpin->setValue(10);
    nSpin->setSuffix("  domande");
    cLay->addWidget(nSpin, 1, 1);

    cLay->setColumnStretch(1, 1);
    lay->addWidget(card);

    /* ── Pulsante ── */
    auto* row = new QHBoxLayout;
    startBtn = new QPushButton("\xf0\x9f\x9a\x80  Inizia Quiz CCNA", w);
    startBtn->setObjectName("actionBtn");
    row->addStretch(1);
    row->addWidget(startBtn);
    row->addStretch(1);
    lay->addLayout(row);

    lay->addStretch(1);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   QuizPage — costruttore
   ══════════════════════════════════════════════════════════════ */
QuizPage::QuizPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_tabs = new QTabWidget(this);

    /* Tab 0 — Genera */
    QWidget* generaTab = buildGeneraTab(this,
        m_topicEdit, m_nDomande, m_cmbTipo, m_cmbDiff,
        m_btnGenera, m_btnCopy, m_btnGioca, m_output);
    m_tabs->addTab(generaTab, "\xf0\x9f\x8e\xaf  Genera");

    /* Tab 1 — Gioca */
    QWidget* giocaTab = buildGiocaTab(this,
        m_giocaPanel, m_giocaStack,
        m_progressLbl, m_questionLbl,
        m_optWidget, m_optBtns,
        m_feedbackLbl, m_btnNext,
        m_openWidget, m_openAnswer,
        m_riepilogoLbl, m_btnRivedi);
    m_giocaTabIdx = m_tabs->addTab(giocaTab, "\xf0\x9f\x8e\xae  Gioca");

    /* Tab 2 — CCNA */
    m_ccnaDb = new QuizCcnaDb(this);
    QWidget* ccnaTab = buildCcnaTab(this, m_ccnaDb,
                                    m_ccnaTemaCombo, m_ccnaNSpin,
                                    m_ccnaStartBtn, m_ccnaStatusLbl);
    m_tabs->addTab(ccnaTab, "\xf0\x9f\x8e\x93  CCNA");

    /* Tab 3 — Storico */
    QWidget* storicoTab = buildStoricoTab(m_dashContent);
    m_tabs->addTab(storicoTab, "\xf0\x9f\x93\x8a  Storico");

    lay->addWidget(m_tabs);

    /* Carica storico quando si passa al tab Storico */
    connect(m_tabs, &QTabWidget::currentChanged, this, &QuizPage::onTabChanged);

    /* ── Connessioni pulsanti ── */
    connect(m_btnGenera, &QPushButton::clicked, this, &QuizPage::onGeneraClicked);
    connect(m_btnCopy,   &QPushButton::clicked, this, &QuizPage::onCopyClicked);
    connect(m_btnGioca,  &QPushButton::clicked, this, &QuizPage::onGiocaClicked);
    if (m_ccnaStartBtn)
        connect(m_ccnaStartBtn, &QPushButton::clicked, this, &QuizPage::onCcnaStartClicked);

    connect(m_topicEdit, &QLineEdit::returnPressed,
            this, &QuizPage::startGeneration);

    /* ── Segnali AI ── */
    connect(m_ai, &AiClient::token,    this, &QuizPage::onQuizToken);
    connect(m_ai, &AiClient::finished, this, &QuizPage::onQuizFinished);
    connect(m_ai, &AiClient::error,    this, &QuizPage::onQuizError);
}

/* ══════════════════════════════════════════════════════════════
   startGeneration — costruisce il prompt e avvia lo streaming
   ══════════════════════════════════════════════════════════════ */
void QuizPage::startGeneration() {
    QString topic = m_topicEdit->text().trimmed();
    if (topic.isEmpty()) {
        m_output->setPlainText(
            "\xe2\x9a\xa0  Inserisci un argomento prima di generare il quiz.");
        return;
    }
    if (m_ai->busy()) return;

    m_fullText.clear();
    m_output->clear();
    _setGenerateBusy(true);
    m_btnCopy->setEnabled(false);
    m_btnGioca->setEnabled(false);
    m_btnGioca->setVisible(false);

    int    n      = m_nDomande->value();
    QString tipo  = m_cmbTipo->currentIndex() == 0 ? "risposta aperta"
                  : m_cmbTipo->currentIndex() == 1 ? "scelta multipla con 4 opzioni (A/B/C/D) e soluzione in fondo"
                  : "miste (met\xc3\xa0 risposta aperta, met\xc3\xa0 scelta multipla)";
    QString diff  = m_cmbDiff->currentIndex() == 0 ? "facile, concetti base"
                  : m_cmbDiff->currentIndex() == 1 ? "medio, applicazione pratica"
                  : "difficile, analisi critica e ragionamento";

    const QString sys =
        "Sei un insegnante esperto. Genera quiz chiari e ben formattati. "
        "Rispondi SEMPRE e SOLO in italiano. "
        "Usa numerazione progressiva (1. 2. 3. ...). "
        "Per le domande a scelta multipla includi le 4 opzioni e indica la risposta corretta in fondo. "
        "Non aggiungere testo di introduzione o commenti fuori dal quiz.";

    const QString usr = QString(
        "Genera un quiz su: \"%1\"\n"
        "Numero domande: %2\n"
        "Tipo: %3\n"
        "Difficolt\xc3\xa0: %4\n\n"
        "Scrivi direttamente le domande, senza preamboli.")
        .arg(topic).arg(n).arg(tipo).arg(diff);

    m_ai->chat(sys, usr);
}

void QuizPage::stopGeneration() {
    m_ai->abort();
    _setGenerateBusy(false);
    m_btnCopy->setEnabled(!m_fullText.isEmpty());
}

// ─── Slot generazione ──────────────────────────────────────────────────────

void QuizPage::onGeneraClicked() {
    if (m_generating) stopGeneration(); else startGeneration();
}

void QuizPage::onCopyClicked()
{
    QApplication::clipboard()->setText(m_fullText);
    m_btnCopy->setText("\xe2\x9c\x85  Copiato!");
    QTimer::singleShot(2000, this, &QuizPage::onCopyRestoreText);
}
void QuizPage::onCopyRestoreText() { m_btnCopy->setText("\xf0\x9f\x93\x8b  Copia"); }

void QuizPage::onQuizToken(const QString& tok)
{
    if (!m_generating) return;
    m_fullText += tok;
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(tok);
    m_output->ensureCursorVisible();
}
void QuizPage::onQuizFinished(const QString&)
{
    if (!m_generating) return;
    _setGenerateBusy(false);
    m_btnCopy->setEnabled(!m_fullText.isEmpty());

    /* Abilita "Gioca!" solo se ci sono domande a scelta multipla */
    if (!m_fullText.isEmpty()) {
        QList<QuizQuestion> preview = parseQuiz(m_fullText);
        bool hasMulti = false;
        for (const auto& q : preview) {
            if (!q.options.isEmpty()) { hasMulti = true; break; }
        }
        if (hasMulti || !preview.isEmpty()) {
            m_btnGioca->setVisible(true);
            m_btnGioca->setEnabled(true);
        }
    }
}
void QuizPage::onQuizError(const QString& msg)
{
    if (!m_generating) return;
    m_output->append("\n\xe2\x9d\x8c  Errore: " + msg);
    _setGenerateBusy(false);
}

void QuizPage::_setGenerateBusy(bool busy)
{
    m_generating = busy;
    if (busy) {
        m_btnGenera->setText("\xe2\x8f\xb9  Stop");
        m_btnGenera->setProperty("danger", true);
    } else {
        m_btnGenera->setText("\xf0\x9f\x8e\xaf  Genera Quiz");
        m_btnGenera->setProperty("danger", false);
    }
    m_btnGenera->setEnabled(true);
    P::repolish(m_btnGenera);
}

// ─── Tab changed ───────────────────────────────────────────────────────────

void QuizPage::onTabChanged(int idx)
{
    /* Storico = ultimo tab (index 2 con gioca, 1 senza) */
    if (idx == m_tabs->count() - 1) loadDashboard();
}

/* ══════════════════════════════════════════════════════════════
   parseQuiz — parser testo LLM → lista QuizQuestion
   ══════════════════════════════════════════════════════════════ */
QList<QuizQuestion> QuizPage::parseQuiz(const QString& text)
{
    QList<QuizQuestion> result;
    QStringList lines = text.split('\n');

    // Regex
    static const QRegularExpression reQuestion(R"(^\s*\d+\.\s+(.+))");
    static const QRegularExpression reOption(
        R"(^\s*([A-Da-d])[\.:\)]\s*(.+))");
    static const QRegularExpression reAnswer(
        R"(^\s*[\(\[]?\s*[Rr]isposta\s*(?:[Cc]orretta\s*)?:?\s*\]?\)?\s*([A-Da-d])\b)");

    QuizQuestion current;
    bool inQuestion = false;

    auto flushCurrent = [&]() {
        if (inQuestion && !current.text.isEmpty()) {
            result.append(current);
        }
        current = QuizQuestion{};
        inQuestion = false;
    };

    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        QRegularExpressionMatch mQ = reQuestion.match(line);
        if (mQ.hasMatch()) {
            flushCurrent();
            current.text = mQ.captured(1).trimmed();
            inQuestion = true;
            continue;
        }

        if (inQuestion) {
            QRegularExpressionMatch mO = reOption.match(line);
            if (mO.hasMatch()) {
                current.options.append(mO.captured(2).trimmed());
                continue;
            }

            QRegularExpressionMatch mA = reAnswer.match(line);
            if (mA.hasMatch()) {
                QString letter = mA.captured(1).toUpper();
                current.correctIdx = letter.at(0).unicode() - 'A';
                continue;
            }

            /* riga di testo aggiuntiva alla domanda (multi-riga) */
            if (current.options.isEmpty()) {
                current.text += " " + line;
            }
        }
    }
    flushCurrent();

    /* Sanity: se correctIdx non trovato ma ci sono opzioni, lascia -1
       (verrà trattato come "open" in fase di feedback) */
    return result;
}

/* ══════════════════════════════════════════════════════════════
   onGiocaClicked — avvia la sessione interattiva
   ══════════════════════════════════════════════════════════════ */
void QuizPage::onGiocaClicked()
{
    m_parsedQuestions = parseQuiz(m_fullText);
    if (m_parsedQuestions.isEmpty()) {
        m_output->append("\n\xe2\x9a\xa0  Nessuna domanda riconosciuta nel testo.");
        return;
    }

    m_currentQ     = 0;
    m_score        = 0;
    m_answered     = false;
    m_wrongAnswers.clear();

    /* Passa al tab Gioca */
    m_tabs->setCurrentIndex(m_giocaTabIdx);

    m_giocaStack->setCurrentIndex(0);
    showQuestion(0);
}

/* ══════════════════════════════════════════════════════════════
   resetGiocaUI — ripristina lo stato della UI per una nuova domanda
   ══════════════════════════════════════════════════════════════ */
void QuizPage::resetGiocaUI()
{
    m_feedbackLbl->setVisible(false);
    m_feedbackLbl->setText("");
    m_feedbackLbl->setStyleSheet("");
    m_btnNext->setVisible(false);
    m_answered = false;

    for (auto* btn : m_optBtns) {
        btn->setEnabled(true);
        btn->setStyleSheet("");
    }
    if (m_openAnswer) m_openAnswer->clear();
}

/* ══════════════════════════════════════════════════════════════
   showQuestion — mostra la domanda idx
   ══════════════════════════════════════════════════════════════ */
void QuizPage::showQuestion(int idx)
{
    resetGiocaUI();

    const int total = m_parsedQuestions.size();
    m_progressLbl->setText(
        QString("Domanda %1 / %2   \xf0\x9f\x8f\x86 Punteggio: %3")
            .arg(idx + 1).arg(total).arg(m_score));

    const QuizQuestion& q = m_parsedQuestions[idx];
    m_questionLbl->setText(
        QString("<b>%1.</b> %2").arg(idx + 1).arg(q.text.toHtmlEscaped()));

    bool isMulti = (!q.options.isEmpty() && q.options.size() >= 2);

    if (isMulti) {
        /* mostra i pulsanti opzione */
        m_optWidget->setVisible(true);
        m_openWidget->setVisible(false);

        const char* labels[] = {"A", "B", "C", "D"};
        for (int i = 0; i < 4; ++i) {
            if (i < q.options.size()) {
                m_optBtns[i]->setText(
                    QString("%1) %2").arg(labels[i]).arg(q.options[i]));
                m_optBtns[i]->setVisible(true);
                m_optBtns[i]->setEnabled(true);
                m_optBtns[i]->setStyleSheet("");
                /* Salva l'indice nell'oggetto pulsante */
                m_optBtns[i]->setProperty("optIdx", i);
            } else {
                m_optBtns[i]->setVisible(false);
            }
        }
    } else {
        /* domanda aperta */
        m_optWidget->setVisible(false);
        m_openWidget->setVisible(true);
        /* Per aperta: pulsante "Avanti" con slot diverso */
        disconnect(m_btnNext, &QPushButton::clicked, this, &QuizPage::onNextQuestion);
        disconnect(m_btnNext, &QPushButton::clicked, this, &QuizPage::onOpenNext);
        connect(m_btnNext, &QPushButton::clicked, this, &QuizPage::onOpenNext);
        m_btnNext->setText("Avanti \xe2\x86\x92");
        m_btnNext->setVisible(true);
    }
}

/* ══════════════════════════════════════════════════════════════
   onOptionSelected — slot comune per tutti e 4 i pulsanti A/B/C/D
   ══════════════════════════════════════════════════════════════ */
void QuizPage::onOptionSelected()
{
    if (m_answered) return;
    m_answered = true;

    QPushButton* clicked = qobject_cast<QPushButton*>(sender());
    if (!clicked) return;

    int chosen = clicked->property("optIdx").toInt();
    const QuizQuestion& q = m_parsedQuestions[m_currentQ];

    /* Disabilita tutti i pulsanti */
    for (auto* btn : m_optBtns) btn->setEnabled(false);

    bool correct = (q.correctIdx >= 0 && chosen == q.correctIdx);

    if (correct) {
        ++m_score;
        clicked->setStyleSheet(
            "QPushButton { background:#1a7a1a; color:#ffffff; border:2px solid #22aa22; }");
        m_feedbackLbl->setText("\xe2\x9c\x85  Corretto!");
        m_feedbackLbl->setStyleSheet("color:#22cc22; font-weight:bold; font-size:15px;");
    } else {
        WrongAnswer wa;
        wa.question  = q;
        wa.chosenIdx = chosen;
        m_wrongAnswers.append(wa);

        clicked->setStyleSheet(
            "QPushButton { background:#7a1a1a; color:#ffffff; border:2px solid #cc2222; }");

        /* Evidenzia risposta corretta se nota */
        if (q.correctIdx >= 0 && q.correctIdx < m_optBtns.size()) {
            m_optBtns[q.correctIdx]->setStyleSheet(
                "QPushButton { background:#1a5a1a; color:#ffffff; border:2px solid #22aa22; }");
            const char* labels[] = {"A", "B", "C", "D"};
            m_feedbackLbl->setText(
                QString("\xe2\x9d\x8c  Sbagliato! La risposta corretta era: <b>%1</b>")
                    .arg(labels[q.correctIdx]));
        } else {
            m_feedbackLbl->setText("\xe2\x9d\x8c  Sbagliato!");
        }
        m_feedbackLbl->setStyleSheet("color:#cc2222; font-weight:bold; font-size:15px;");
    }
    m_feedbackLbl->setVisible(true);

    /* Ricollega btnNext al normale avanzamento */
    disconnect(m_btnNext, &QPushButton::clicked, this, &QuizPage::onNextQuestion);
    disconnect(m_btnNext, &QPushButton::clicked, this, &QuizPage::onOpenNext);
    connect(m_btnNext, &QPushButton::clicked, this, &QuizPage::onNextQuestion);

    bool isLast = (m_currentQ == m_parsedQuestions.size() - 1);
    m_btnNext->setText(isLast ? "\xf0\x9f\x8f\x86  Risultati" : "Avanti \xe2\x86\x92");
    m_btnNext->setVisible(true);
}

/* ══════════════════════════════════════════════════════════════
   onOpenNext — avanza senza punteggio (domanda aperta)
   ══════════════════════════════════════════════════════════════ */
void QuizPage::onOpenNext()
{
    m_answered = true;
    bool isLast = (m_currentQ == m_parsedQuestions.size() - 1);
    if (isLast) {
        showRiepilogo();
    } else {
        ++m_currentQ;
        /* Ricollega btnNext a onNextQuestion per la prossima domanda */
        disconnect(m_btnNext, &QPushButton::clicked, this, &QuizPage::onOpenNext);
        disconnect(m_btnNext, &QPushButton::clicked, this, &QuizPage::onNextQuestion);
        connect(m_btnNext, &QPushButton::clicked, this, &QuizPage::onNextQuestion);
        showQuestion(m_currentQ);
    }
}

/* ══════════════════════════════════════════════════════════════
   onNextQuestion — avanza alla domanda successiva
   ══════════════════════════════════════════════════════════════ */
void QuizPage::onNextQuestion()
{
    bool isLast = (m_currentQ == m_parsedQuestions.size() - 1);
    if (isLast) {
        showRiepilogo();
    } else {
        ++m_currentQ;
        showQuestion(m_currentQ);
    }
}

/* ══════════════════════════════════════════════════════════════
   showRiepilogo — schermata finale con punteggio
   ══════════════════════════════════════════════════════════════ */
void QuizPage::showRiepilogo()
{
    int total = m_parsedQuestions.size();
    /* Conta solo le domande a scelta multipla (quelle con correctIdx noto) */
    int countMulti = 0;
    for (const auto& q : m_parsedQuestions) {
        if (!q.options.isEmpty() && q.correctIdx >= 0) ++countMulti;
    }

    double pct = countMulti > 0 ? m_score * 100.0 / countMulti : 0.0;

    QString emoji;
    if (pct >= 80)       emoji = "\xf0\x9f\x8f\x86";
    else if (pct >= 50)  emoji = "\xf0\x9f\x91\x8d";
    else                 emoji = "\xf0\x9f\x92\xaa";

    QString msg = QString(
        "%1  Quiz completato!\n\n"
        "Punteggio: <b>%2 / %3</b> risposte corrette\n"
        "(%4%)\n\n"
        "Domande totali: %5  |  A scelta multipla: %6")
        .arg(emoji)
        .arg(m_score).arg(countMulti)
        .arg(pct, 0, 'f', 1)
        .arg(total).arg(countMulti);

    m_riepilogoLbl->setText(msg);
    if (m_btnRivedi)
        m_btnRivedi->setVisible(!m_wrongAnswers.isEmpty());
    m_giocaStack->setCurrentIndex(1);

    /* Salva sessione nel JSON storico */
    QString topic = m_topicEdit->text().trimmed();
    QString diff;
    switch (m_cmbDiff->currentIndex()) {
        case 0: diff = "facile"; break;
        case 2: diff = "difficile"; break;
        default: diff = "medio"; break;
    }

    const QString path = QDir::homePath() + "/.prismalux_quiz.json";
    QFile f(path);
    QJsonObject root;
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
    }
    QJsonArray sessions = root["sessions"].toArray();
    QJsonObject sess;
    sess["date"]      = QDateTime::currentDateTime().toString(Qt::ISODate);
    sess["subject"]   = topic.isEmpty() ? "Quiz" : topic;
    sess["difficulty"]= diff;
    sess["total"]     = countMulti;
    sess["correct"]   = m_score;
    sess["wrong"]     = countMulti - m_score;
    sessions.prepend(sess);
    root["sessions"]  = sessions;
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson());
        f.close();
    }
}

/* ══════════════════════════════════════════════════════════════
   loadDashboard — legge ~/.prismalux_quiz.json e popola il tab Storico
   ══════════════════════════════════════════════════════════════ */
void QuizPage::loadDashboard() {
    /* Svuota contenuto precedente */
    QLayout* root = m_dashContent->layout();
    while (QLayoutItem* item = root->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    auto* vlay = qobject_cast<QVBoxLayout*>(root);

    const QString path = QDir::homePath() + "/.prismalux_quiz.json";
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        auto* msg = new QLabel(
            "\xf0\x9f\x93\xad  Nessun quiz ancora completato.\n\n"
            "Completa almeno un quiz (nella scheda Impara) per vedere le statistiche qui.",
            m_dashContent);
        msg->setObjectName("cardDesc");
        msg->setWordWrap(true);
        msg->setAlignment(Qt::AlignCenter);
        vlay->addWidget(msg);
        vlay->addStretch(1);
        return;
    }
    QJsonArray sessions = QJsonDocument::fromJson(f.readAll()).object()["sessions"].toArray();
    f.close();

    if (sessions.isEmpty()) {
        auto* msg = new QLabel(
            "\xf0\x9f\x93\xad  Nessun quiz ancora completato.", m_dashContent);
        msg->setObjectName("cardDesc");
        msg->setAlignment(Qt::AlignCenter);
        vlay->addWidget(msg);
        vlay->addStretch(1);
        return;
    }

    /* ── Statistiche globali ── */
    int totQ = 0, totC = 0;
    QMap<QString, QPair<int,int>> bySubject;
    for (auto v : sessions) {
        auto s = v.toObject();
        int tot = s["total"].toInt(); int cor = s["correct"].toInt();
        totQ += tot; totC += cor;
        auto& p = bySubject[s["subject"].toString()];
        p.first += cor; p.second += tot;
    }
    double globPct = totQ > 0 ? totC * 100.0 / totQ : 0.0;

    auto* globBox = new QGroupBox("\xf0\x9f\x93\x88  Statistiche globali", m_dashContent);
    auto* globL   = new QHBoxLayout(globBox);

    /* statistica inline con widget locali */
    struct StatCol {
        QWidget* col; QVBoxLayout* cl; QLabel* lv; QLabel* ll;
    };
    auto addStat = [&](const QString& lbl, const QString& val) {
        auto* col = new QWidget(globBox);
        auto* cl  = new QVBoxLayout(col); cl->setSpacing(2); cl->setContentsMargins(12,4,12,4);
        auto* lv  = new QLabel(val, col); lv->setObjectName("pageTitle"); lv->setAlignment(Qt::AlignCenter);
        auto* ll  = new QLabel(lbl, col); ll->setObjectName("cardDesc"); ll->setAlignment(Qt::AlignCenter);
        cl->addWidget(lv); cl->addWidget(ll);
        globL->addWidget(col);
    };
    addStat("Quiz completati",   QString::number(sessions.size()));
    addStat("Domande totali",    QString::number(totQ));
    addStat("Risposte corrette", QString::number(totC));
    addStat("Percentuale",       QString("%1%").arg(globPct, 0, 'f', 1));
    vlay->addWidget(globBox);

    /* ── Per materia ── */
    auto* subjBox = new QGroupBox("\xf0\x9f\x93\x9a  Per materia", m_dashContent);
    auto* subjL   = new QVBoxLayout(subjBox);
    for (auto it = bySubject.constBegin(); it != bySubject.constEnd(); ++it) {
        int cor = it.value().first; int tot = it.value().second;
        double pct = tot > 0 ? cor * 100.0 / tot : 0.0;
        auto* row = new QWidget(subjBox);
        auto* rl  = new QHBoxLayout(row); rl->setContentsMargins(0,2,0,2); rl->setSpacing(10);
        auto* nm  = new QLabel(it.key(), row); nm->setFixedWidth(110); nm->setObjectName("cardTitle");
        auto* bar = new QProgressBar(row);
        bar->setRange(0, 100); bar->setValue((int)pct); bar->setTextVisible(false);
        bar->setFixedHeight(10); bar->setObjectName("resBar");
        auto* pctLbl = new QLabel(
            QString("%1%  (%2/%3)").arg(pct,0,'f',1).arg(cor).arg(tot), row);
        pctLbl->setObjectName("gaugePct"); pctLbl->setFixedWidth(100);
        rl->addWidget(nm); rl->addWidget(bar, 1); rl->addWidget(pctLbl);
        subjL->addWidget(row);
    }
    vlay->addWidget(subjBox);

    /* ── Sessioni recenti ── */
    auto* recBox = new QGroupBox("\xf0\x9f\x95\x90  Ultime 10 sessioni", m_dashContent);
    auto* recL   = new QVBoxLayout(recBox);
    int count = qMin((int)sessions.size(), 10);
    for (int i = 0; i < count; i++) {
        auto s = sessions[i].toObject();
        double pct = s["total"].toInt() > 0
            ? s["correct"].toInt() * 100.0 / s["total"].toInt() : 0;
        QString dt = QDateTime::fromString(s["date"].toString(), Qt::ISODate)
                         .toString("dd/MM/yyyy HH:mm");
        auto* row  = new QWidget(recBox);
        auto* rl   = new QHBoxLayout(row); rl->setContentsMargins(0,1,0,1); rl->setSpacing(12);
        auto* date = new QLabel(dt, row); date->setObjectName("cardDesc"); date->setFixedWidth(130);
        auto* subj = new QLabel(s["subject"].toString(), row);
        subj->setObjectName("cardTitle"); subj->setFixedWidth(100);
        auto* diff = new QLabel(s["difficulty"].toString(), row);
        diff->setObjectName("cardDesc"); diff->setFixedWidth(60);
        auto* score = new QLabel(
            QString("\xe2\x9c\x85%1/\xe2\x9d\x8c%2 \xe2\x80\x94 %3%")
            .arg(s["correct"].toInt()).arg(s["wrong"].toInt()).arg(pct,0,'f',0), row);
        score->setObjectName("cardDesc");
        rl->addWidget(date); rl->addWidget(subj); rl->addWidget(diff); rl->addWidget(score, 1);
        recL->addWidget(row);
    }
    vlay->addWidget(recBox);
    vlay->addStretch(1);
}

/* ══════════════════════════════════════════════════════════════
   onCcnaStartClicked — carica domande dal DB e avvia Gioca
   ══════════════════════════════════════════════════════════════ */
void QuizPage::onCcnaStartClicked()
{
    if (!m_ccnaDb || !m_ccnaDb->isAvailable()) return;

    int     n    = m_ccnaNSpin ? m_ccnaNSpin->value() : 10;
    QString tema = m_ccnaTemaCombo
                 ? m_ccnaTemaCombo->currentData().toString()
                 : QString();

    QList<CcnaQuestion> batch = m_ccnaDb->randomBatch(n, tema);
    if (batch.isEmpty()) return;

    m_parsedQuestions.clear();
    for (const CcnaQuestion& cq : batch) {
        QuizQuestion q;
        q.text       = cq.domanda;
        q.options    = cq.risposte;
        q.correctIdx = cq.corretta;
        m_parsedQuestions.append(q);
    }

    m_currentQ     = 0;
    m_score        = 0;
    m_answered     = false;
    m_wrongAnswers.clear();

    m_tabs->setCurrentIndex(m_giocaTabIdx);
    m_giocaStack->setCurrentIndex(0);
    showQuestion(0);
}

/* ══════════════════════════════════════════════════════════════
   onRivediErroriClicked — mostra dialog con le domande sbagliate
   ══════════════════════════════════════════════════════════════ */
void QuizPage::onRivediErroriClicked()
{
    if (m_wrongAnswers.isEmpty()) return;

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QString("\xf0\x9f\x93\x96  Revisione errori (%1 domande)")
                        .arg(m_wrongAnswers.size()));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(640, 520);

    auto* mainLay = new QVBoxLayout(dlg);
    mainLay->setContentsMargins(12, 12, 12, 12);
    mainLay->setSpacing(8);

    auto* scroll = new QScrollArea(dlg);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget;
    auto* contentLay = new QVBoxLayout(content);
    contentLay->setContentsMargins(4, 4, 4, 4);
    contentLay->setSpacing(12);

    const char* letters[] = {"A", "B", "C", "D"};

    for (int i = 0; i < m_wrongAnswers.size(); ++i) {
        const WrongAnswer& wa = m_wrongAnswers[i];
        const QuizQuestion& q = wa.question;

        auto* card = new QFrame(content);
        card->setObjectName("actionCard");
        auto* cLay = new QVBoxLayout(card);
        cLay->setContentsMargins(14, 10, 14, 10);
        cLay->setSpacing(6);

        auto* qLbl = new QLabel(
            QString("<b>%1.</b> %2").arg(i + 1).arg(q.text.toHtmlEscaped()),
            card);
        qLbl->setObjectName("cardTitle");
        qLbl->setWordWrap(true);
        qLbl->setTextFormat(Qt::RichText);
        cLay->addWidget(qLbl);

        if (!q.options.isEmpty()) {
            for (int j = 0; j < q.options.size() && j < 4; ++j) {
                bool isCorrect = (j == q.correctIdx);
                bool isChosen  = (j == wa.chosenIdx);
                QString prefix;
                QString style;
                if (isCorrect) {
                    prefix = "\xe2\x9c\x85 ";
                    style  = "color:#22cc22; font-weight:bold;";
                } else if (isChosen) {
                    prefix = "\xe2\x9d\x8c ";
                    style  = "color:#cc2222;";
                }
                auto* oLbl = new QLabel(
                    QString("%1) %2%3")
                    .arg(letters[j])
                    .arg(prefix)
                    .arg(q.options[j].toHtmlEscaped()),
                    card);
                oLbl->setObjectName("cardDesc");
                if (!style.isEmpty())
                    oLbl->setStyleSheet(style);
                cLay->addWidget(oLbl);
            }
        }

        if (q.correctIdx >= 0 && q.correctIdx < 4 && !q.options.isEmpty()) {
            auto* corrLbl = new QLabel(
                QString("<b>Risposta corretta:</b> %1) %2")
                .arg(letters[q.correctIdx])
                .arg(q.options[q.correctIdx].toHtmlEscaped()),
                card);
            corrLbl->setObjectName("cardDesc");
            corrLbl->setTextFormat(Qt::RichText);
            corrLbl->setStyleSheet("color:#22cc22; margin-top:4px;");
            cLay->addWidget(corrLbl);
        }

        contentLay->addWidget(card);
    }
    contentLay->addStretch(1);
    scroll->setWidget(content);
    mainLay->addWidget(scroll, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    mainLay->addWidget(bb);

    dlg->exec();
}

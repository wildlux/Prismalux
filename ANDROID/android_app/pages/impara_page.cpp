#include "impara_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QFrame>
#include <QShowEvent>
#include <QFont>
#include <QApplication>
#include <QClipboard>
#include <QTextCursor>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QScroller>
#include <QScrollerProperties>

/* ── Helper: applica scorrimento touch a uno scroll area ── */
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
ImparaPage::ImparaPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    setObjectName("ImparaPage");
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    m_inner = new QStackedWidget(this);
    m_inner->addWidget(buildMenu());   /* 0 — menu */
    m_inner->addWidget(buildTutor());  /* 1 — tutor */
    m_inner->addWidget(buildQuiz());   /* 2 — quiz  */
    m_inner->setCurrentIndex(0);
    vbox->addWidget(m_inner);
}

void ImparaPage::showEvent(QShowEvent* ev)
{
    QWidget::showEvent(ev);
    m_inner->setCurrentIndex(0);  /* torna sempre al menu */
}

/* ══════════════════════════════════════════════════════════════
   buildMenu — 2 card grandi (Tutor + Quiz)
   ══════════════════════════════════════════════════════════════ */
QWidget* ImparaPage::buildMenu()
{
    auto* w     = new QWidget;
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    applyTouchScroll(scroll);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(12, 16, 12, 16);
    vbox->setSpacing(14);
    scroll->setWidget(inner);

    auto* outerVbox = new QVBoxLayout(w);
    outerVbox->setContentsMargins(0, 0, 0, 0);
    outerVbox->addWidget(scroll);

    /* ── Titolo ── */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x9a  Impara con AI"), inner);
    QFont tf = title->font();
    tf.setPointSize(16); tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    auto* sub = new QLabel(
        "Scegli una modalità di apprendimento", inner);
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("color:#8890a8; font-size:13px;");
    vbox->addWidget(sub);
    vbox->addSpacing(8);

    /* ── Card Tutor AI ── */
    auto* tutorCard = new QPushButton(inner);
    tutorCard->setObjectName("DrawerNavBtn");
    tutorCard->setMinimumHeight(100);
    tutorCard->setFlat(true);
    tutorCard->setText(
        QString::fromUtf8("\xf0\x9f\x8e\x93") + "   Tutor AI\n\n"
        "Chat con un docente AI.\n"
        "Fai domande su qualsiasi materia.");
    QFont cf = tutorCard->font();
    cf.setPointSize(13);
    tutorCard->setFont(cf);
    tutorCard->setStyleSheet(
        "QPushButton#DrawerNavBtn { text-align:left; padding:16px; "
        "border-radius:12px; border:2px solid #6c63ff; }");
    connect(tutorCard, &QPushButton::clicked,
            this, &ImparaPage::onTutorCardClicked);
    vbox->addWidget(tutorCard);

    /* ── Card Quiz AI ── */
    auto* quizCard = new QPushButton(inner);
    quizCard->setObjectName("DrawerNavBtn");
    quizCard->setMinimumHeight(100);
    quizCard->setFlat(true);
    quizCard->setText(
        QString::fromUtf8("\xf0\x9f\xa7\xa0") + "   Quiz AI\n\n"
        "Domande A/B/C/D generate dall'AI.\n"
        "Testa le tue conoscenze.");
    quizCard->setFont(cf);
    quizCard->setStyleSheet(
        "QPushButton#DrawerNavBtn { text-align:left; padding:16px; "
        "border-radius:12px; border:2px solid #22c55e; }");
    connect(quizCard, &QPushButton::clicked,
            this, &ImparaPage::onQuizCardClicked);
    vbox->addWidget(quizCard);

    vbox->addStretch();
    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildTutor — chat con docente AI
   ══════════════════════════════════════════════════════════════ */
QWidget* ImparaPage::buildTutor()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    /* ── Header con back ── */
    auto* hdr = new QHBoxLayout;
    auto* backBtn = new QPushButton(
        QString::fromUtf8("\xe2\x86\x90"), w);
    backBtn->setObjectName("SecondaryBtn");
    backBtn->setFixedSize(44, 44);
    connect(backBtn, &QPushButton::clicked,
            this, &ImparaPage::onBackToMenu);

    auto* hdrLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x8e\x93  Tutor AI"), w);
    QFont hf = hdrLbl->font();
    hf.setPointSize(15); hf.setBold(true);
    hdrLbl->setFont(hf);
    hdr->addWidget(backBtn);
    hdr->addWidget(hdrLbl, 1);
    vbox->addLayout(hdr);

    /* ── Selezione materia ── */
    auto* subjGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x96  Materia"), w);
    subjGroup->setObjectName("SettingsGroup");
    auto* subjVbox = new QVBoxLayout(subjGroup);
    m_tutorSubj = new QComboBox(w);
    const QStringList materie = {
        "Matematica", "Fisica", "Chimica", "Informatica",
        "Italiano", "Storia", "Geografia", "Inglese",
        "Filosofia", "Biologia", "Economia", "Generale"
    };
    m_tutorSubj->addItems(materie);
    m_tutorSubj->setMinimumHeight(44);
    subjVbox->addWidget(m_tutorSubj);
    vbox->addWidget(subjGroup);

    /* ── Log chat ── */
    auto* logGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x92\xac  Conversazione"), w);
    logGroup->setObjectName("SettingsGroup");
    auto* logVbox = new QVBoxLayout(logGroup);

    auto* logScroll = new QScrollArea(w);
    logScroll->setWidgetResizable(true);
    logScroll->setFrameShape(QFrame::NoFrame);
    logScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    applyTouchScroll(logScroll);

    m_tutorLog = new QTextEdit(w);
    m_tutorLog->setReadOnly(true);
    m_tutorLog->setMinimumHeight(220);
    m_tutorLog->setPlaceholderText(
        "La conversazione con il tutor apparirà qui.\n\n"
        "Seleziona la materia e scrivi la tua domanda.");
    logScroll->setWidget(m_tutorLog);
    logVbox->addWidget(logScroll);

    m_tutorStatus = new QLabel("", w);
    m_tutorStatus->setVisible(false);
    m_tutorStatus->setStyleSheet("color:#8890a8; font-size:12px;");
    logVbox->addWidget(m_tutorStatus);
    vbox->addWidget(logGroup, 1);

    /* ── Input domanda ── */
    auto* inputGroup = new QGroupBox(
        QString::fromUtf8("\xe2\x9c\x8f\xef\xb8\x8f  La tua domanda"), w);
    inputGroup->setObjectName("SettingsGroup");
    auto* inputVbox = new QVBoxLayout(inputGroup);

    m_tutorInp = new QLineEdit(w);
    m_tutorInp->setPlaceholderText(
        "Scrivi la tua domanda...");
    m_tutorInp->setMinimumHeight(44);
    inputVbox->addWidget(m_tutorInp);

    auto* btnRow = new QHBoxLayout;
    m_tutorSend = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x9a\x80  Invia"), w);
    m_tutorSend->setObjectName("PrimaryBtn");
    m_tutorSend->setMinimumHeight(52);
    QFont bf = m_tutorSend->font();
    bf.setBold(true); bf.setPointSize(13);
    m_tutorSend->setFont(bf);

    m_tutorStop = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9  Stop"), w);
    m_tutorStop->setObjectName("StopBtn");
    m_tutorStop->setMinimumHeight(52);
    m_tutorStop->setEnabled(false);

    btnRow->addWidget(m_tutorSend, 2);
    btnRow->addWidget(m_tutorStop, 1);
    inputVbox->addLayout(btnRow);

    /* Bottone cancella chat */
    auto* clearBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x97\x91  Nuova sessione"), w);
    clearBtn->setObjectName("SecondaryBtn");
    clearBtn->setMinimumHeight(44);
    inputVbox->addWidget(clearBtn);
    vbox->addWidget(inputGroup);

    /* ── Connessioni ── */
    connect(m_tutorSend, &QPushButton::clicked,
            this, &ImparaPage::onTutorSendClicked);
    connect(m_tutorStop, &QPushButton::clicked,
            this, &ImparaPage::onTutorStopClicked);
    connect(m_tutorInp, &QLineEdit::returnPressed,
            this, &ImparaPage::onTutorSendClicked);
    connect(clearBtn, &QPushButton::clicked, this, [this] {
        m_tutorLog->clear();
        m_tutorHistory = QJsonArray();
    });

    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildQuiz — domande A/B/C/D generate dall'AI
   ══════════════════════════════════════════════════════════════ */
QWidget* ImparaPage::buildQuiz()
{
    auto* w      = new QWidget;
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    applyTouchScroll(scroll);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);
    scroll->setWidget(inner);

    auto* outerVbox = new QVBoxLayout(w);
    outerVbox->setContentsMargins(0, 0, 0, 0);
    outerVbox->addWidget(scroll);

    /* ── Header ── */
    auto* hdr = new QHBoxLayout;
    auto* backBtn = new QPushButton(
        QString::fromUtf8("\xe2\x86\x90"), inner);
    backBtn->setObjectName("SecondaryBtn");
    backBtn->setFixedSize(44, 44);
    connect(backBtn, &QPushButton::clicked,
            this, &ImparaPage::onBackToMenu);

    auto* hdrLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\xa7\xa0  Quiz AI"), inner);
    QFont hf = hdrLbl->font();
    hf.setPointSize(15); hf.setBold(true);
    hdrLbl->setFont(hf);
    hdr->addWidget(backBtn);
    hdr->addWidget(hdrLbl, 1);
    vbox->addLayout(hdr);

    /* ── Configurazione ── */
    auto* cfgGroup = new QGroupBox(
        QString::fromUtf8("\xe2\x9a\x99\xef\xb8\x8f  Configurazione"), inner);
    cfgGroup->setObjectName("SettingsGroup");
    auto* cfgGrid = new QGridLayout(cfgGroup);
    cfgGrid->setSpacing(8);

    cfgGrid->addWidget(new QLabel("Materia:", cfgGroup), 0, 0);
    m_quizSubj = new QComboBox(cfgGroup);
    const QStringList materie = {
        "Matematica", "Fisica", "Chimica", "Informatica",
        "Storia", "Italiano", "Inglese", "Biologia",
        "Filosofia", "Economia", "Generale"
    };
    m_quizSubj->addItems(materie);
    m_quizSubj->setMinimumHeight(44);
    cfgGrid->addWidget(m_quizSubj, 0, 1);

    cfgGrid->addWidget(new QLabel("Difficoltà:", cfgGroup), 1, 0);
    m_quizDiff = new QComboBox(cfgGroup);
    m_quizDiff->addItems({"Facile", "Media", "Difficile", "Universitaria"});
    m_quizDiff->setCurrentIndex(1);
    m_quizDiff->setMinimumHeight(44);
    cfgGrid->addWidget(m_quizDiff, 1, 1);
    vbox->addWidget(cfgGroup);

    /* ── Punteggio ── */
    m_quizScore = new QLabel(
        QString::fromUtf8("\xe2\x9c\x85 0   \xe2\x9d\x8c 0"), inner);
    QFont sf = m_quizScore->font();
    sf.setPointSize(14); sf.setBold(true);
    m_quizScore->setFont(sf);
    m_quizScore->setAlignment(Qt::AlignCenter);
    vbox->addWidget(m_quizScore);

    m_quizBar = new QProgressBar(inner);
    m_quizBar->setRange(0, 0);
    m_quizBar->setFixedHeight(4);
    m_quizBar->setVisible(false);
    vbox->addWidget(m_quizBar);

    /* ── Domanda ── */
    auto* domGroup = new QGroupBox(
        QString::fromUtf8("\xe2\x9d\x93  Domanda"), inner);
    domGroup->setObjectName("SettingsGroup");
    auto* domVbox = new QVBoxLayout(domGroup);

    m_quizSubjLbl = new QLabel("", inner);
    m_quizSubjLbl->setStyleSheet("color:#8890a8; font-size:12px;");
    domVbox->addWidget(m_quizSubjLbl);

    m_quizQuestion = new QLabel(
        "Premi \"Genera Domanda\" per iniziare.", inner);
    m_quizQuestion->setWordWrap(true);
    QFont qf = m_quizQuestion->font();
    qf.setPointSize(13);
    m_quizQuestion->setFont(qf);
    m_quizQuestion->setMinimumHeight(80);
    m_quizQuestion->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    domVbox->addWidget(m_quizQuestion);
    vbox->addWidget(domGroup);

    /* ── Opzioni A/B/C/D ── */
    auto* optsGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x9d  Risposte"), inner);
    optsGroup->setObjectName("SettingsGroup");
    auto* optsVbox = new QVBoxLayout(optsGroup);
    optsVbox->setSpacing(8);

    const char* labels[] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; ++i) {
        m_quizOpts[i] = new QPushButton(
            QString("  ") + labels[i] + ")  —", inner);
        m_quizOpts[i]->setObjectName("SecondaryBtn");
        m_quizOpts[i]->setMinimumHeight(52);
        m_quizOpts[i]->setEnabled(false);
        m_quizOpts[i]->setFlat(false);
        QFont of = m_quizOpts[i]->font();
        of.setPointSize(12);
        m_quizOpts[i]->setFont(of);
        m_quizOpts[i]->setStyleSheet(
            "QPushButton { text-align:left; padding-left:12px; }");
        optsVbox->addWidget(m_quizOpts[i]);
    }
    connect(m_quizOpts[0], &QPushButton::clicked,
            this, &ImparaPage::onQuizOpt0Clicked);
    connect(m_quizOpts[1], &QPushButton::clicked,
            this, &ImparaPage::onQuizOpt1Clicked);
    connect(m_quizOpts[2], &QPushButton::clicked,
            this, &ImparaPage::onQuizOpt2Clicked);
    connect(m_quizOpts[3], &QPushButton::clicked,
            this, &ImparaPage::onQuizOpt3Clicked);
    vbox->addWidget(optsGroup);

    /* ── Feedback risposta ── */
    m_quizFeedback = new QLabel("", inner);
    QFont ff = m_quizFeedback->font();
    ff.setPointSize(14); ff.setBold(true);
    m_quizFeedback->setFont(ff);
    m_quizFeedback->setAlignment(Qt::AlignCenter);
    m_quizFeedback->setVisible(false);
    vbox->addWidget(m_quizFeedback);

    m_quizExpLbl = new QLabel("", inner);
    m_quizExpLbl->setWordWrap(true);
    m_quizExpLbl->setStyleSheet("color:#8890a8; font-size:12px; padding:8px;");
    m_quizExpLbl->setVisible(false);
    vbox->addWidget(m_quizExpLbl);

    /* ── Bottoni azione ── */
    auto* actRow = new QHBoxLayout;

    m_quizGen = new QPushButton(
        QString::fromUtf8("\xe2\x9c\xa8  Genera Domanda"), inner);
    m_quizGen->setObjectName("PrimaryBtn");
    m_quizGen->setMinimumHeight(56);
    QFont gf = m_quizGen->font();
    gf.setBold(true); gf.setPointSize(13);
    m_quizGen->setFont(gf);

    m_quizStop = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9"), inner);
    m_quizStop->setObjectName("StopBtn");
    m_quizStop->setFixedSize(56, 56);
    m_quizStop->setEnabled(false);

    m_quizNext = new QPushButton(
        QString::fromUtf8("\xe2\x9e\xa1\xef\xb8\x8f  Prossima"), inner);
    m_quizNext->setObjectName("SecondaryBtn");
    m_quizNext->setMinimumHeight(56);
    m_quizNext->setEnabled(false);
    m_quizNext->setFont(gf);

    actRow->addWidget(m_quizGen, 2);
    actRow->addWidget(m_quizStop);
    actRow->addWidget(m_quizNext, 2);
    vbox->addLayout(actRow);

    vbox->addStretch();

    connect(m_quizGen,  &QPushButton::clicked,
            this, &ImparaPage::onQuizGeneraClicked);
    connect(m_quizStop, &QPushButton::clicked,
            this, &ImparaPage::onQuizStopClicked);
    connect(m_quizNext, &QPushButton::clicked,
            this, &ImparaPage::onQuizNextClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Navigazione
   ══════════════════════════════════════════════════════════════ */
void ImparaPage::onBackToMenu()
{
    if (m_tutorBusy) { m_ai->abort(); m_tutorBusy = false; }
    if (m_quiz.generating) { m_ai->abort(); m_quiz.generating = false; }
    m_inner->setCurrentIndex(0);
}

void ImparaPage::onTutorCardClicked() { m_inner->setCurrentIndex(1); }
void ImparaPage::onQuizCardClicked()  { m_inner->setCurrentIndex(2); }

/* ══════════════════════════════════════════════════════════════
   Tutor AI — slot
   ══════════════════════════════════════════════════════════════ */
void ImparaPage::onTutorSendClicked()
{
    const QString question = m_tutorInp->text().trimmed();
    if (question.isEmpty() || m_tutorBusy || m_ai->busy()) return;

    /* Aggiungi domanda al log */
    m_tutorLog->moveCursor(QTextCursor::End);
    m_tutorLog->insertHtml(
        "<p><b style='color:#6c63ff;'>"
        + QString::fromUtf8("\xf0\x9f\x91\xa4")
        + " Tu:</b> " + question.toHtmlEscaped() + "</p>");
    m_tutorLog->moveCursor(QTextCursor::End);

    /* Aggiungi alla history */
    QJsonObject usr;
    usr["role"]    = "user";
    usr["content"] = question;
    m_tutorHistory.append(usr);

    m_tutorInp->clear();
    m_tutorBusy = true;
    m_tutorSend->setEnabled(false);
    m_tutorStop->setEnabled(true);
    if (m_tutorStatus) {
        m_tutorStatus->setText(
            QString::fromUtf8("\xe2\x8f\xb3") + "  Il tutor sta elaborando...");
        m_tutorStatus->setVisible(true);
    }

    const QString materia = m_tutorSubj->currentText();
    const QString sys =
        "Sei un docente esperto di " + materia + " che insegna in italiano. "
        "Rispondi in modo chiaro, preciso e pedagogico. "
        "Usa esempi concreti quando utile. "
        "Se la domanda non riguarda " + materia + ", rispondi comunque in modo utile. "
        "Mantieni un tono incoraggiante e professionale.";

    m_tutorTokConn = connect(m_ai, &AiClient::token,
                             this, &ImparaPage::onTutorToken);
    m_tutorFinConn = connect(m_ai, &AiClient::finished,
                             this, &ImparaPage::onTutorFinished);
    m_tutorErrConn = connect(m_ai, &AiClient::error,
                             this, &ImparaPage::onTutorError);

    m_ai->chat(sys, question, m_tutorHistory);
}

void ImparaPage::onTutorStopClicked()
{
    m_ai->abort();
}

void ImparaPage::onTutorToken(const QString& t)
{
    if (!m_tutorBusy) return;
    m_tutorLog->moveCursor(QTextCursor::End);
    m_tutorLog->insertPlainText(t);
    m_tutorLog->moveCursor(QTextCursor::End);
}

void ImparaPage::onTutorFinished(const QString& full)
{
    if (!m_tutorBusy) return;
    disconnect(m_tutorTokConn);
    disconnect(m_tutorFinConn);
    disconnect(m_tutorErrConn);

    /* Aggiungi risposta alla history */
    if (!full.isEmpty()) {
        QJsonObject ass;
        ass["role"]    = "assistant";
        ass["content"] = full;
        m_tutorHistory.append(ass);
    }

    m_tutorLog->moveCursor(QTextCursor::End);
    m_tutorLog->insertHtml("<br>");

    m_tutorBusy = false;
    m_tutorSend->setEnabled(true);
    m_tutorStop->setEnabled(false);
    if (m_tutorStatus) m_tutorStatus->setVisible(false);
}

void ImparaPage::onTutorError(const QString& e)
{
    if (!m_tutorBusy) return;
    disconnect(m_tutorTokConn);
    disconnect(m_tutorFinConn);
    disconnect(m_tutorErrConn);

    m_tutorLog->moveCursor(QTextCursor::End);
    m_tutorLog->insertHtml(
        "<p style='color:#ef4444;'>"
        + QString::fromUtf8("\xe2\x9d\x8c")
        + " Errore: " + e.toHtmlEscaped() + "</p>");

    m_tutorBusy = false;
    m_tutorSend->setEnabled(true);
    m_tutorStop->setEnabled(false);
    if (m_tutorStatus) m_tutorStatus->setVisible(false);
}

void ImparaPage::onTutorAborted()
{
    onTutorFinished("");
}

/* ══════════════════════════════════════════════════════════════
   Quiz AI — generazione domanda
   ══════════════════════════════════════════════════════════════ */
void ImparaPage::onQuizGeneraClicked()
{
    if (m_quiz.generating || m_ai->busy()) return;

    m_quiz.rawBuf.clear();
    m_quiz.answered   = false;
    m_quiz.generating = true;

    m_quizQuestion->setText(
        QString::fromUtf8("\xe2\x8f\xb3") + "  Generazione in corso...");
    m_quizFeedback->setVisible(false);
    m_quizExpLbl->setVisible(false);
    m_quizNext->setEnabled(false);
    m_quizGen->setEnabled(false);
    m_quizStop->setEnabled(true);
    m_quizBar->setVisible(true);
    for (int i = 0; i < 4; ++i) {
        m_quizOpts[i]->setEnabled(false);
        m_quizOpts[i]->setText(
            QString("  ") + "ABCD"[i] + ")  —");
        m_quizOpts[i]->setStyleSheet(
            "QPushButton { text-align:left; padding-left:12px; }");
    }

    const QString materia  = m_quizSubj->currentText();
    const QString diff     = m_quizDiff->currentText();
    m_quiz.subject    = materia;
    m_quiz.difficulty = diff;
    m_quizSubjLbl->setText(materia + " — " + diff);

    const QString sys =
        "Sei un professore di " + materia + ". "
        "Genera UNA domanda a risposta multipla di difficoltà " + diff + ".\n"
        "Usa ESATTAMENTE questo formato (nessun testo prima o dopo):\n"
        "DOMANDA: <testo completo della domanda>\n"
        "A) <opzione A>\n"
        "B) <opzione B>\n"
        "C) <opzione C>\n"
        "D) <opzione D>\n"
        "RISPOSTA: <solo la lettera: A, B, C o D>\n"
        "SPIEGAZIONE: <spiegazione breve della risposta corretta>";

    m_quizTokConn = connect(m_ai, &AiClient::token,
                            this, &ImparaPage::onQuizToken);
    m_quizFinConn = connect(m_ai, &AiClient::finished,
                            this, &ImparaPage::onQuizFinished);
    m_quizErrConn = connect(m_ai, &AiClient::error,
                            this, &ImparaPage::onQuizError);

    m_ai->chat(sys, "Genera una domanda.");
}

void ImparaPage::onQuizStopClicked()
{
    m_ai->abort();
    m_quiz.generating = false;
    m_quizGen->setEnabled(true);
    m_quizStop->setEnabled(false);
    m_quizBar->setVisible(false);
    m_quizQuestion->setText("Generazione annullata.");
}

void ImparaPage::onQuizToken(const QString& t)
{
    if (!m_quiz.generating) return;
    m_quiz.rawBuf += t;
}

void ImparaPage::onQuizFinished(const QString& full)
{
    if (!m_quiz.generating) return;
    disconnect(m_quizTokConn);
    disconnect(m_quizFinConn);
    disconnect(m_quizErrConn);
    m_quiz.generating = false;
    m_quizBar->setVisible(false);
    m_quizGen->setEnabled(false);
    m_quizStop->setEnabled(false);
    parseAndShowQuestion(full.isEmpty() ? m_quiz.rawBuf : full);
}

void ImparaPage::onQuizError(const QString& e)
{
    if (!m_quiz.generating) return;
    disconnect(m_quizTokConn);
    disconnect(m_quizFinConn);
    disconnect(m_quizErrConn);
    m_quiz.generating = false;
    m_quizBar->setVisible(false);
    m_quizGen->setEnabled(true);
    m_quizStop->setEnabled(false);
    m_quizQuestion->setText(
        QString::fromUtf8("\xe2\x9d\x8c  Errore: ") + e);
}

/* ── parseAndShowQuestion ─────────────────────────────────────── */
void ImparaPage::parseAndShowQuestion(const QString& raw)
{
    /* Estrae DOMANDA: */
    static const QRegularExpression reDom(
        R"(DOMANDA:\s*(.+?)(?=\nA\)|\Z))",
        QRegularExpression::DotMatchesEverythingOption);
    /* Estrae A) B) C) D) */
    static const QRegularExpression reOpt(
        R"(([A-D])\)\s*(.+?)(?=\n[A-D]\)|\nRISPOSTA:|\Z))",
        QRegularExpression::DotMatchesEverythingOption);
    /* Estrae RISPOSTA: */
    static const QRegularExpression reResp(R"(RISPOSTA:\s*([A-D]))");
    /* Estrae SPIEGAZIONE: */
    static const QRegularExpression reSpie(
        R"(SPIEGAZIONE:\s*(.+))",
        QRegularExpression::DotMatchesEverythingOption);

    const auto mDom = reDom.match(raw);
    const QString domanda = mDom.hasMatch()
        ? mDom.captured(1).trimmed()
        : raw.left(200).trimmed();

    QString opts[4];
    auto it = reOpt.globalMatch(raw);
    while (it.hasNext()) {
        auto m = it.next();
        const char letter = m.captured(1).at(0).toLatin1();
        const int idx = letter - 'A';
        if (idx >= 0 && idx < 4)
            opts[idx] = m.captured(2).trimmed();
    }

    const auto mResp = reResp.match(raw);
    m_quiz.correctLetter = mResp.hasMatch() ? mResp.captured(1) : "A";

    const auto mSpie = reSpie.match(raw);
    m_quiz.explanation = mSpie.hasMatch() ? mSpie.captured(1).trimmed() : "";

    /* Mostra la domanda */
    m_quizQuestion->setText(domanda);
    m_quiz.answered = false;

    const char* labels[] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; ++i) {
        const QString text = opts[i].isEmpty()
            ? QString("—")
            : opts[i];
        m_quizOpts[i]->setText(
            "  " + QString(labels[i]) + ")  " + text);
        m_quizOpts[i]->setEnabled(!opts[i].isEmpty());
        m_quizOpts[i]->setStyleSheet(
            "QPushButton { text-align:left; padding-left:12px; }");
    }
}

/* ── submitAnswer ─────────────────────────────────────────────── */
void ImparaPage::submitAnswer(int optionIdx)
{
    if (m_quiz.answered) return;
    m_quiz.answered = true;

    const char* labels[] = { "A", "B", "C", "D" };
    const bool correct = (QString(labels[optionIdx]) == m_quiz.correctLetter);

    if (correct) {
        ++m_quiz.correct;
        showFeedback(true);
    } else {
        ++m_quiz.wrong;
        showFeedback(false);
    }

    /* Colora opzioni: verde = corretta, rosso = sbagliata */
    for (int i = 0; i < 4; ++i) {
        m_quizOpts[i]->setEnabled(false);
        if (QString(labels[i]) == m_quiz.correctLetter)
            m_quizOpts[i]->setStyleSheet(
                "QPushButton { text-align:left; padding-left:12px; "
                "background:#166534; color:#fff; border-color:#22c55e; }");
        else if (i == optionIdx)
            m_quizOpts[i]->setStyleSheet(
                "QPushButton { text-align:left; padding-left:12px; "
                "background:#7f1d1d; color:#fff; border-color:#ef4444; }");
    }

    m_quizScore->setText(
        QString::fromUtf8("\xe2\x9c\x85 %1   \xe2\x9d\x8c %2")
        .arg(m_quiz.correct).arg(m_quiz.wrong));

    m_quizNext->setEnabled(true);
    m_quizGen->setEnabled(false);
}

void ImparaPage::showFeedback(bool correct)
{
    if (correct) {
        m_quizFeedback->setText(
            QString::fromUtf8("\xe2\x9c\x85  Corretto!"));
        m_quizFeedback->setStyleSheet("color:#22c55e; font-size:16px; font-weight:bold;");
    } else {
        m_quizFeedback->setText(
            QString::fromUtf8("\xe2\x9d\x8c  Sbagliato — risposta: ")
            + m_quiz.correctLetter);
        m_quizFeedback->setStyleSheet("color:#ef4444; font-size:16px; font-weight:bold;");
    }
    m_quizFeedback->setVisible(true);

    if (!m_quiz.explanation.isEmpty()) {
        m_quizExpLbl->setText(
            QString::fromUtf8("\xf0\x9f\x92\xa1  ") + m_quiz.explanation);
        m_quizExpLbl->setVisible(true);
    }
}

void ImparaPage::onQuizNextClicked()
{
    m_quizFeedback->setVisible(false);
    m_quizExpLbl->setVisible(false);
    m_quizNext->setEnabled(false);
    m_quizGen->setEnabled(true);
    m_quizQuestion->setText("Premi \"Genera Domanda\" per la prossima.");
    for (int i = 0; i < 4; ++i) {
        m_quizOpts[i]->setText(
            QString("  ") + "ABCD"[i] + ")  —");
        m_quizOpts[i]->setEnabled(false);
        m_quizOpts[i]->setStyleSheet(
            "QPushButton { text-align:left; padding-left:12px; }");
    }
}

void ImparaPage::resetQuizButtons()
{
    m_quizGen->setEnabled(true);
    m_quizStop->setEnabled(false);
    m_quizBar->setVisible(false);
}

void ImparaPage::onQuizOpt0Clicked() { submitAnswer(0); }
void ImparaPage::onQuizOpt1Clicked() { submitAnswer(1); }
void ImparaPage::onQuizOpt2Clicked() { submitAnswer(2); }
void ImparaPage::onQuizOpt3Clicked() { submitAnswer(3); }

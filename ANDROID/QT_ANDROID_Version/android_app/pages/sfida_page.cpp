#include "sfida_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFont>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

static constexpr const char* kSfidaSys =
    "Sei un generatore di quiz didattici. Genera esattamente 5 domande a scelta multipla "
    "(4 opzioni A/B/C/D) sul tema richiesto. Rispondi SOLO con JSON valido:\n"
    "{\"questions\":[{\"text\":\"...\",\"options\":[\"A) ...\",\"B) ...\",\"C) ...\",\"D) ...\"],"
    "\"correct\":0,\"explanation\":\"...\"},...]}";

SfidaPage::SfidaPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x8e\xaf") + " Sfida! Quiz AI", this);
    {   QFont f = title->font(); f.setPointSize(14); f.setBold(true); title->setFont(f); }
    vbox->addWidget(title);

    /* Config */
    auto* form = new QFormLayout;
    form->setSpacing(6);
    m_topicCmb = new QComboBox(this);
    m_topicCmb->addItem("Informatica e programmazione");
    m_topicCmb->addItem("Matematica e fisica");
    m_topicCmb->addItem("Reti CCNA / networking");
    m_topicCmb->addItem("Storia e cultura generale");
    m_topicCmb->addItem("Biologia e medicina");
    m_topicCmb->addItem("Finanza e economia");
    m_topicCmb->addItem("Letteratura italiana");
    m_topicCmb->addItem("Lingua inglese");
    form->addRow("Tema:", m_topicCmb);

    m_diffCmb = new QComboBox(this);
    m_diffCmb->addItem("Facile");
    m_diffCmb->addItem("Medio");
    m_diffCmb->addItem("Difficile");
    m_diffCmb->setCurrentIndex(1);
    form->addRow("Difficolta':", m_diffCmb);
    vbox->addLayout(form);

    m_genBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x8e\xb2") + " Genera quiz (5 domande)", this);
    m_genBtn->setObjectName("PrimaryBtn");
    m_genBtn->setMinimumHeight(44);
    vbox->addWidget(m_genBtn);

    /* Punteggio e progress */
    auto* scoreRow = new QHBoxLayout;
    m_scoreLabel = new QLabel("Punteggio: 0/0", this);
    m_scoreLabel->setObjectName("StatusLabel");
    scoreRow->addWidget(m_scoreLabel, 1);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 5);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(8);
    scoreRow->addWidget(m_progress, 2);
    vbox->addLayout(scoreRow);

    /* Domanda */
    m_questionLabel = new QLabel("", this);
    m_questionLabel->setWordWrap(true);
    {   QFont f = m_questionLabel->font(); f.setPointSize(13); m_questionLabel->setFont(f); }
    m_questionLabel->setVisible(false);
    vbox->addWidget(m_questionLabel);

    /* Risposte */
    const char* labels[] = {"A", "B", "C", "D"};
    for (int i = 0; i < 4; ++i) {
        m_answerBtns[i] = new QPushButton(labels[i], this);
        m_answerBtns[i]->setMinimumHeight(44);
        m_answerBtns[i]->setVisible(false);
        m_answerBtns[i]->setStyleSheet(
            "QPushButton { background: #1e293b; border: 1px solid #334155; border-radius: 8px;"
            "color: #e2e8f0; text-align: left; padding: 8px 12px; font-size: 13px; }");
        vbox->addWidget(m_answerBtns[i]);
    }

    m_feedback = new QLabel("", this);
    m_feedback->setWordWrap(true);
    m_feedback->setObjectName("StatusLabel");
    m_feedback->setVisible(false);
    vbox->addWidget(m_feedback);

    m_nextBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9e\xa1") + " Avanti", this);
    m_nextBtn->setObjectName("PrimaryBtn");
    m_nextBtn->setMinimumHeight(44);
    m_nextBtn->setVisible(false);
    vbox->addWidget(m_nextBtn);

    m_status = new QLabel("", this);
    m_status->setObjectName("StatusLabel");
    vbox->addWidget(m_status);
    vbox->addStretch();

    connect(m_genBtn,       &QPushButton::clicked, this, &SfidaPage::onGenerateClicked);
    connect(m_answerBtns[0], &QPushButton::clicked, this, &SfidaPage::onAnswerA);
    connect(m_answerBtns[1], &QPushButton::clicked, this, &SfidaPage::onAnswerB);
    connect(m_answerBtns[2], &QPushButton::clicked, this, &SfidaPage::onAnswerC);
    connect(m_answerBtns[3], &QPushButton::clicked, this, &SfidaPage::onAnswerD);
    connect(m_nextBtn,      &QPushButton::clicked, this, &SfidaPage::nextQuestion);
    connect(m_ai, &AiClient::token,    this, &SfidaPage::onToken);
    connect(m_ai, &AiClient::finished, this, &SfidaPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &SfidaPage::onAiError);
}

void SfidaPage::onGenerateClicked()
{
    m_questions.clear();
    m_currentQ = 0;
    m_score    = 0;
    m_total    = 0;
    m_rawBuffer.clear();
    m_questionLabel->setVisible(false);
    m_feedback->setVisible(false);
    m_nextBtn->setVisible(false);
    for (int i = 0; i < 4; ++i) m_answerBtns[i]->setVisible(false);
    m_progress->setValue(0);
    m_scoreLabel->setText("Punteggio: 0/0");
    setBusy(true);

    const QString topic = m_topicCmb->currentText();
    const QString diff  = m_diffCmb->currentText().toLower();
    m_ai->chat(kSfidaSys,
        QString("Genera 5 domande di difficolta' %1 sul tema: %2").arg(diff, topic));
}

void SfidaPage::onToken(const QString& t)
{
    m_rawBuffer += t;
    m_status->setText(
        QString::fromUtf8("\xe2\x8f\xb3") +
        QString(" Generando... (%1 char)").arg(m_rawBuffer.size()));
}

void SfidaPage::onFinished(const QString& full)
{
    setBusy(false);
    parseQuiz(full);
}

void SfidaPage::parseQuiz(const QString& raw)
{
    QRegularExpression re("\\{[\\s\\S]*\\}");
    auto m = re.match(raw);
    if (!m.hasMatch()) {
        m_status->setText(
            QString::fromUtf8("\xe2\x9d\x8c") + " JSON non trovato nella risposta.");
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(m.captured(0).toUtf8());
    if (!doc.isObject()) {
        m_status->setText(QString::fromUtf8("\xe2\x9d\x8c") + " JSON non valido.");
        return;
    }
    QJsonArray arr = doc.object().value("questions").toArray();
    if (arr.isEmpty()) {
        m_status->setText(QString::fromUtf8("\xe2\x9d\x8c") + " Nessuna domanda nel JSON.");
        return;
    }
    m_questions.clear();
    for (const QJsonValue& v : arr) {
        QJsonObject obj = v.toObject();
        Question q;
        q.text    = obj.value("text").toString();
        q.correct = obj.value("correct").toInt(0);
        q.explanation = obj.value("explanation").toString();
        for (const QJsonValue& opt : obj.value("options").toArray())
            q.options.append(opt.toString());
        if (!q.text.isEmpty() && q.options.size() == 4)
            m_questions.append(q);
    }
    if (m_questions.isEmpty()) {
        m_status->setText(QString::fromUtf8("\xe2\x9d\x8c") + " Nessuna domanda valida.");
        return;
    }
    m_total   = m_questions.size();
    m_currentQ = 0;
    m_score    = 0;
    m_progress->setRange(0, m_total);
    m_status->setText("");
    showQuestion();
}

void SfidaPage::showQuestion()
{
    if (m_currentQ >= m_questions.size()) return;
    const Question& q = m_questions[m_currentQ];
    m_answered = false;
    m_questionLabel->setText(
        QString("<b>%1/%2</b> %3").arg(m_currentQ+1).arg(m_total).arg(q.text));
    m_questionLabel->setVisible(true);
    for (int i = 0; i < 4 && i < q.options.size(); ++i) {
        m_answerBtns[i]->setText(q.options[i]);
        m_answerBtns[i]->setVisible(true);
        m_answerBtns[i]->setEnabled(true);
        m_answerBtns[i]->setStyleSheet(
            "QPushButton { background:#1e293b;border:1px solid #334155;border-radius:8px;"
            "color:#e2e8f0;text-align:left;padding:8px 12px;font-size:13px; }");
    }
    m_feedback->setVisible(false);
    m_nextBtn->setVisible(false);
    m_progress->setValue(m_currentQ);
}

void SfidaPage::checkAnswer(int sel)
{
    if (m_answered) return;
    m_answered = true;
    const Question& q = m_questions[m_currentQ];
    bool correct = (sel == q.correct);
    if (correct) m_score++;
    m_scoreLabel->setText(
        QString("Punteggio: %1/%2").arg(m_score).arg(m_currentQ + 1));

    for (int i = 0; i < 4; ++i) {
        m_answerBtns[i]->setEnabled(false);
        if (i == q.correct)
            m_answerBtns[i]->setStyleSheet(
                "QPushButton{background:#14532d;border:2px solid #22c55e;border-radius:8px;"
                "color:#86efac;text-align:left;padding:8px 12px;font-size:13px;}");
        else if (i == sel)
            m_answerBtns[i]->setStyleSheet(
                "QPushButton{background:#7f1d1d;border:2px solid #ef4444;border-radius:8px;"
                "color:#fca5a5;text-align:left;padding:8px 12px;font-size:13px;}");
    }

    m_feedback->setText((correct ? QString::fromUtf8("\xe2\x9c\x85 Corretto! ") :
                                   QString::fromUtf8("\xe2\x9d\x8c Sbagliato. ")) +
                        q.explanation);
    m_feedback->setVisible(true);

    if (m_currentQ + 1 >= m_total) {
        m_nextBtn->setText(
            QString::fromUtf8("\xf0\x9f\x8f\x86") + " Fine quiz");
        m_nextBtn->setVisible(true);
    } else {
        m_nextBtn->setText(
            QString::fromUtf8("\xe2\x9e\xa1") + " Prossima domanda");
        m_nextBtn->setVisible(true);
    }
}

void SfidaPage::nextQuestion()
{
    m_currentQ++;
    if (m_currentQ >= m_questions.size()) {
        m_progress->setValue(m_total);
        m_questionLabel->setText(
            QString("<b>" + QString::fromUtf8("\xf0\x9f\x8f\x86") +
                    " Quiz completato!</b><br>Risultato finale: <b>%1/%2</b>")
                .arg(m_score).arg(m_total));
        m_questionLabel->setVisible(true);
        for (int i = 0; i < 4; ++i) m_answerBtns[i]->setVisible(false);
        m_feedback->setVisible(false);
        m_nextBtn->setVisible(false);
        m_scoreLabel->setText(
            QString("Punteggio finale: %1/%2").arg(m_score).arg(m_total));
    } else {
        showQuestion();
    }
}

void SfidaPage::onAnswerA() { checkAnswer(0); }
void SfidaPage::onAnswerB() { checkAnswer(1); }
void SfidaPage::onAnswerC() { checkAnswer(2); }
void SfidaPage::onAnswerD() { checkAnswer(3); }

void SfidaPage::onAiError(const QString& msg)
{
    setBusy(false);
    m_status->setText(QString::fromUtf8("\xe2\x9d\x8c") + " " + msg);
}

void SfidaPage::setBusy(bool busy)
{
    if (m_genBtn) m_genBtn->setEnabled(!busy);
    if (busy) m_status->setText(
        QString::fromUtf8("\xe2\x8f\xb3") + " Generazione quiz in corso...");
}

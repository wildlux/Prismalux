#include "quiz_page.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
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

/* ══════════════════════════════════════════════════════════════
   buildGeneraTab — UI generazione quiz
   ══════════════════════════════════════════════════════════════ */
QWidget* buildGeneraTab(QuizPage* self,
                         QLineEdit*& topicEdit, QSpinBox*& nDomande,
                         QComboBox*& cmbTipo, QComboBox*& cmbDiff,
                         QPushButton*& btnGenera,
                         QPushButton*& btnCopy, QTextEdit*& output)
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

    btnRow->addWidget(btnGenera);
    btnRow->addStretch(1);
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
        m_btnGenera, m_btnCopy, m_output);
    m_tabs->addTab(generaTab, "\xf0\x9f\x8e\xaf  Genera");

    /* Tab 1 — Storico */
    QWidget* storicoTab = buildStoricoTab(m_dashContent);
    m_tabs->addTab(storicoTab, "\xf0\x9f\x93\x8a  Storico");

    lay->addWidget(m_tabs);

    /* Carica storico quando si passa al tab 1 */
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int idx){
        if (idx == 1) loadDashboard();
    });

    /* ── Connessioni pulsanti ── */
    connect(m_btnGenera, &QPushButton::clicked, this, [this]{
        if (m_generating) { stopGeneration(); return; }
        startGeneration();
    });

    connect(m_btnCopy, &QPushButton::clicked, this, [this]{
        QApplication::clipboard()->setText(m_fullText);
        m_btnCopy->setText("\xe2\x9c\x85  Copiato!");
        QTimer::singleShot(2000, this,
            [this]{ m_btnCopy->setText("\xf0\x9f\x93\x8b  Copia"); });
    });

    connect(m_topicEdit, &QLineEdit::returnPressed,
            this, &QuizPage::startGeneration);

    /* ── Segnali AI ── */
    connect(m_ai, &AiClient::token, this, [this](const QString& tok){
        if (!m_generating) return;
        m_fullText += tok;
        m_output->moveCursor(QTextCursor::End);
        m_output->insertPlainText(tok);
        m_output->ensureCursorVisible();
    });

    connect(m_ai, &AiClient::finished, this, [this](const QString&){
        if (!m_generating) return;
        _setGenerateBusy(false);
        m_btnCopy->setEnabled(!m_fullText.isEmpty());
    });

    connect(m_ai, &AiClient::error, this, [this](const QString& msg){
        if (!m_generating) return;
        m_output->append("\n\xe2\x9d\x8c  Errore: " + msg);
        _setGenerateBusy(false);
    });
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
    auto addStat = [&](const QString& lbl, const QString& val){
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

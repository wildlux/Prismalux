#include "impara_page.h"
#include <QShowEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QScrollBar>
#include <QMenu>
#include <QGuiApplication>
#include <QClipboard>
#include <QProcess>

#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

/* ── Percorso storia quiz ─────────────────────────────────────── */
QString ImparaPage::historyPath() const {
    return QDir::homePath() + "/.prismalux_quiz.json";
}

/* ══════════════════════════════════════════════════════════════
   Barra selettore backend/modello (condivisa da Tutor e Quiz)
   ══════════════════════════════════════════════════════════════ */
QWidget* ImparaPage::buildModelBar(QWidget* parent) {
    auto* bar  = new QWidget(parent);
    auto* lay  = new QHBoxLayout(bar);
    lay->setContentsMargins(0, 0, 0, 0); lay->setSpacing(8);

    auto* lbl1 = new QLabel("\xf0\x9f\x94\x8c Backend:", bar); lbl1->setObjectName("cardDesc");
    /* Variabili LOCALI al posto dei puntatori membro m_cmbBackend/m_cmbModel.
     * buildModelBar() viene chiamata due volte (da buildTutor + buildQuiz): se si
     * usassero i puntatori membro, la seconda chiamata sovrascrive i puntatori e
     * i lambda della prima barra aggiornano i combo sbagliati → Tutor rimane vuoto. */
    auto* cmbBackend = new QComboBox(bar);
    cmbBackend->addItems({"Ollama (HTTP)", "llama-server (HTTP)", "llama.cpp locale"});

    auto* lbl2 = new QLabel("\xf0\x9f\xa4\x96 Modello:", bar); lbl2->setObjectName("cardDesc");
    auto* cmbModel = new QComboBox(bar);
    cmbModel->setMinimumWidth(180);

    auto* refreshBtn = new QPushButton("\xf0\x9f\x94\x84", bar);
    refreshBtn->setObjectName("actionBtn"); refreshBtn->setFixedWidth(36);
    refreshBtn->setToolTip("Aggiorna lista modelli");

    lay->addWidget(lbl1); lay->addWidget(cmbBackend);
    lay->addWidget(lbl2); lay->addWidget(cmbModel, 1);
    lay->addWidget(refreshBtn);

    /* Aggiorna modelli in base al backend selezionato */
    auto refreshModels = [=]{
        int idx = cmbBackend->currentIndex();
        cmbModel->clear();

        if (idx == 0) {
            /* Ollama */
            m_ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, "");
            m_ai->fetchModels();
        } else if (idx == 1) {
            /* llama-server */
            m_ai->setBackend(AiClient::LlamaServer, "127.0.0.1", 8080, "");
            m_ai->fetchModels();
        } else {
            /* llama.cpp locale: scansiona directory .gguf */
            QStringList dirs = {
                P::modelsDir(),
                P::llamaStudioDir() + "/models"
            };
            QStringList found;
            for (const auto& d : dirs) {
                QDir dir(d);
                if (!dir.exists()) continue;
                for (const auto& f : dir.entryInfoList({"*.gguf"}, QDir::Files))
                    found << f.absoluteFilePath();
            }
            for (const auto& f : found)
                cmbModel->addItem(QFileInfo(f).fileName(), f);

            if (found.isEmpty())
                cmbModel->addItem("(nessun .gguf trovato)", "");
        }
    };

    connect(cmbBackend, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int){ refreshModels(); });
    connect(refreshBtn, &QPushButton::clicked, this, [=]{ refreshModels(); });

    /* Quando arrivano i modelli Ollama/llama-server → popola QUESTO combo specifico.
     * Usa bar come context object: se il bar viene distrutto, la connessione si
     * disconnette automaticamente — evita doppio-popolamento tra le due barre. */
    connect(m_ai, &AiClient::modelsReady, bar, [=](const QStringList& list){
        if (cmbBackend->currentIndex() < 2) {
            cmbModel->clear();
            for (const auto& mdl : list) cmbModel->addItem(mdl, mdl);
        }
    });

    /* Quando l'utente sceglie un modello → aggiorna m_ai */
    connect(cmbModel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int i){
        if (i < 0) return;
        int bk = cmbBackend->currentIndex();
        if (bk == 2) {
            /* locale */
            QString bin = P::llamaCliBin();
            if (!QFileInfo::exists(bin))
                bin = P::root() + "/C_software/llama.cpp/build/bin/llama-cli";
            m_ai->setLocalBackend(bin, cmbModel->currentData().toString());
        } else {
            m_ai->setBackend(bk == 0 ? AiClient::Ollama : AiClient::LlamaServer,
                             m_ai->host(), m_ai->port(),
                             cmbModel->currentData().toString());
        }
    });

    /* Carica iniziale */
    refreshModels();
    return bar;
}

/* ══════════════════════════════════════════════════════════════
   MENU principale
   ══════════════════════════════════════════════════════════════ */
QWidget* ImparaPage::buildMenu() {
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(24, 20, 24, 16); lay->setSpacing(12);

    auto* div = new QFrame(w); div->setObjectName("pageDivider");
    div->setFrameShape(QFrame::HLine); lay->addWidget(div);

    struct Item { QString icon, title, desc; int page; };
    QList<Item> items = {
        {"🏛️", "Tutor AI — Oracolo",
         "Spiegazioni personalizzate su qualsiasi materia. Matematica, informatica, fisica e altro.", 1},
        {"📖", "Materie",
         "Tutor per argomento: Matematica, Fisica, Chimica, Sicurezza Informatica, Informatica, Algoritmi.", 4},
        {"⚡", "Simulatore Algoritmi",
         "Visualizza passo-passo Bubble, Selection, Insertion, Quick, Merge Sort, Linear e Binary Search.", 5},
    };

    for (auto& it : items) {
        auto* card = new QFrame(w); card->setObjectName("actionCard");
        auto* cl = new QHBoxLayout(card);
        cl->setContentsMargins(16, 14, 16, 14); cl->setSpacing(14);

        auto* ico = new QLabel(it.icon, card); ico->setObjectName("cardIcon"); ico->setFixedWidth(38);
        ico->setAlignment(Qt::AlignCenter);

        auto* txt  = new QWidget(card);
        auto* txtL = new QVBoxLayout(txt); txtL->setContentsMargins(0,0,0,0); txtL->setSpacing(3);
        auto* lt = new QLabel(it.title, txt); lt->setObjectName("cardTitle");
        auto* ld = new QLabel(it.desc,  txt); ld->setObjectName("cardDesc"); ld->setWordWrap(true);
        txtL->addWidget(lt); txtL->addWidget(ld);

        auto* btn = new QPushButton("Apri →", card); btn->setObjectName("actionBtn"); btn->setFixedWidth(80);
        const int pg = it.page;
        connect(btn, &QPushButton::clicked, this, [this, pg]{
            m_inner->setCurrentIndex(pg);
        });
        cl->addWidget(ico); cl->addWidget(txt, 1); cl->addWidget(btn);
        lay->addWidget(card);
    }

    lay->addStretch(1);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   TUTOR
   ══════════════════════════════════════════════════════════════ */
QWidget* ImparaPage::buildTutor() {
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(24, 16, 24, 16); lay->setSpacing(8);

    /* Header */
    auto* hdrW = new QWidget(w);
    auto* hdrL = new QHBoxLayout(hdrW); hdrL->setContentsMargins(0,0,0,0);
    auto* back = new QPushButton("← Torna", hdrW); back->setObjectName("actionBtn");
    auto* lbl  = new QLabel("🏛️  Tutor AI — Oracolo", hdrW); lbl->setObjectName("pageTitle");
    hdrL->addWidget(back); hdrL->addWidget(lbl, 1);
    lay->addWidget(hdrW);

    /* Barra backend */
    lay->addWidget(buildModelBar(w));

    auto* div = new QFrame(w); div->setObjectName("pageDivider"); div->setFrameShape(QFrame::HLine);
    lay->addWidget(div);

    /* Materia */
    auto* topRow = new QWidget(w);
    auto* topL   = new QHBoxLayout(topRow); topL->setContentsMargins(0,0,0,0); topL->setSpacing(10);
    topL->addWidget(new QLabel("📚 Materia:", topRow));
    m_tutorSubj = new QComboBox(topRow);
    m_tutorSubj->addItems({"Matematica","Informatica","Fisica","Chimica","Storia","Letteratura","Inglese","Libero"});
    auto* clrBtn = new QPushButton("🗑 Pulisci", topRow); clrBtn->setObjectName("actionBtn");
    topL->addWidget(m_tutorSubj, 1); topL->addWidget(clrBtn);
    lay->addWidget(topRow);

    /* Log */
    m_tutorLog = new QTextEdit(w); m_tutorLog->setObjectName("chatLog");
    m_tutorLog->setReadOnly(true);
    m_tutorLog->setPlaceholderText("🏛️  Modalità Oracolo attiva.\n\nFai una domanda sulla materia selezionata.");
    lay->addWidget(m_tutorLog, 1);

    /* ── Context menu: copia / leggi ── */
    m_tutorLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tutorLog, &QTextEdit::customContextMenuRequested, w, [this](const QPoint& pos){
        const QString sel   = m_tutorLog->textCursor().selectedText();
        const bool hasSel   = !sel.isEmpty();
        const QString label = hasSel ? "selezione" : "tutto";
        QMenu menu(m_tutorLog);
        QAction* actCopy = menu.addAction("\xf0\x9f\x97\x82  Copia " + label);
        QAction* actRead = menu.addAction("\xf0\x9f\x8e\x99  Leggi " + label);
        QAction* chosen  = menu.exec(m_tutorLog->mapToGlobal(pos));
        const QString txt = hasSel ? sel : m_tutorLog->toPlainText();
        if (chosen == actCopy) {
            QGuiApplication::clipboard()->setText(txt);
        } else if (chosen == actRead) {
            QStringList words = txt.split(' ', Qt::SkipEmptyParts);
            if (words.size() > 400) words = words.mid(words.size() - 400);
            QProcess::startDetached("espeak-ng", {"-v", "it+f3", "--punct=none", words.join(" ")});
        }
    });

    /* Input */
    /* Indicatore elaborazione */
    auto* waitLbl = new QLabel("\xe2\x8f\xb3  Elaborazione in corso...", w);
    waitLbl->setStyleSheet("color:#E5C400; font-style:italic; padding:2px 0;");
    waitLbl->setVisible(false);
    lay->addWidget(waitLbl);

    auto* inRow = new QWidget(w);
    auto* inL   = new QHBoxLayout(inRow); inL->setContentsMargins(0,0,0,0); inL->setSpacing(8);
    auto* inp  = new QLineEdit(inRow); inp->setObjectName("chatInput");
    inp->setPlaceholderText("Fai una domanda all'Oracolo..."); inp->setFixedHeight(38);
    auto* send = new QPushButton("Chiedi \xe2\x96\xb6", inRow); send->setObjectName("actionBtn");
    auto* stop = new QPushButton("\xe2\x8f\xb9", inRow);
    stop->setObjectName("actionBtn"); stop->setProperty("danger", true);
    stop->setFixedWidth(40); stop->setEnabled(false);
    inL->addWidget(inp, 1); inL->addWidget(send); inL->addWidget(stop);
    lay->addWidget(inRow);

    connect(back,   &QPushButton::clicked, this, [this]{ m_inner->setCurrentIndex(0); });
    connect(clrBtn, &QPushButton::clicked, m_tutorLog, &QTextEdit::clear);
    connect(stop,   &QPushButton::clicked, m_ai,  &AiClient::abort);

    auto sendFn = [=]{
        QString msg = inp->text().trimmed(); if (msg.isEmpty()) return;
        m_tutorLog->append(QString("\n👤  Tu [%1]: %2\n").arg(m_tutorSubj->currentText(), msg));
        m_tutorLog->append("🤖  Oracolo: ");
        inp->clear(); send->setEnabled(false); stop->setEnabled(true); waitLbl->setVisible(true);
        QString sys = QString("Sei l'Oracolo di Prismalux, un tutor AI esperto in %1. "
            "Spiega in modo chiaro con esempi pratici. Rispondi SEMPRE e SOLO in italiano.")
            .arg(m_tutorSubj->currentText());
        m_ai->chat(sys, msg);
    };

    connect(send, &QPushButton::clicked, this, sendFn);
    connect(inp,  &QLineEdit::returnPressed, this, sendFn);
    connect(m_ai, &AiClient::token,    this, [=](const QString& t){
        QTextCursor c(m_tutorLog->document()); c.movePosition(QTextCursor::End);
        c.insertText(t); m_tutorLog->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, this, [=](const QString&){
        m_tutorLog->append("\n──────────");
        send->setEnabled(true); stop->setEnabled(false); waitLbl->setVisible(false);
    });
    connect(m_ai, &AiClient::error, this, [=](const QString& e){
        m_tutorLog->append(QString("\n\xe2\x9d\x8c %1").arg(e));
        send->setEnabled(true); stop->setEnabled(false); waitLbl->setVisible(false);
    });
    connect(m_ai, &AiClient::aborted, this, [=]{
        m_tutorLog->append("\n\xe2\x8f\xb9  Interrotto.\n──────────");
        send->setEnabled(true); stop->setEnabled(false); waitLbl->setVisible(false);
    });
    return w;
}

/* ══════════════════════════════════════════════════════════════
   QUIZ — generazione domande MCQ
   ══════════════════════════════════════════════════════════════ */
QWidget* ImparaPage::buildQuiz() {
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(24, 16, 24, 16); lay->setSpacing(8);

    /* Header */
    auto* hdrW = new QWidget(w);
    auto* hdrL = new QHBoxLayout(hdrW); hdrL->setContentsMargins(0,0,0,0);
    auto* back = new QPushButton("← Torna", hdrW); back->setObjectName("actionBtn");
    auto* lbl  = new QLabel("📝  Quiz Interattivi", hdrW); lbl->setObjectName("pageTitle");
    hdrL->addWidget(back); hdrL->addWidget(lbl, 1);

    m_quizProgress = new QLabel("", hdrW); m_quizProgress->setObjectName("cardDesc");
    m_quizScore    = new QLabel("", hdrW); m_quizScore->setObjectName("cardTitle");
    hdrL->addWidget(m_quizProgress); hdrL->addWidget(m_quizScore);
    lay->addWidget(hdrW);

    /* Barra backend */
    lay->addWidget(buildModelBar(w));

    auto* div = new QFrame(w); div->setObjectName("pageDivider"); div->setFrameShape(QFrame::HLine);
    lay->addWidget(div);

    /* Config quiz */
    auto* cfgW = new QWidget(w);
    auto* cfgL = new QHBoxLayout(cfgW); cfgL->setContentsMargins(0,0,0,0); cfgL->setSpacing(12);

    cfgL->addWidget(new QLabel("📚 Materia:"));
    m_quizSubj = new QComboBox(w);
    m_quizSubj->addItems({"Matematica","Informatica","Fisica","Chimica","Storia","Letteratura","Inglese"});
    cfgL->addWidget(m_quizSubj);

    cfgL->addWidget(new QLabel("🎯 Difficoltà:"));
    m_quizDiff = new QComboBox(w);
    m_quizDiff->addItems({"Facile","Medio","Difficile"});
    m_quizDiff->setCurrentIndex(1);
    cfgL->addWidget(m_quizDiff);

    cfgL->addWidget(new QLabel("🔢 Domande:"));
    m_quizNum = new QComboBox(w);
    m_quizNum->addItems({"3","5","10"});
    m_quizNum->setCurrentIndex(1);
    cfgL->addWidget(m_quizNum);

    m_quizGen = new QPushButton("▶  Inizia Quiz", w);
    m_quizGen->setObjectName("actionBtn");
    cfgL->addWidget(m_quizGen);
    cfgL->addStretch(1);
    lay->addWidget(cfgW);

    /* Domanda */
    m_quizQuestion = new QLabel("Premi \"Inizia Quiz\" per cominciare.", w);
    m_quizQuestion->setObjectName("cardTitle");
    m_quizQuestion->setWordWrap(true);
    m_quizQuestion->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_quizQuestion->setContentsMargins(0, 8, 0, 8);
    lay->addWidget(m_quizQuestion);

    /* Opzioni A-D */
    static const QString labels[] = {"A", "B", "C", "D"};
    auto* optsW = new QWidget(w);
    auto* optsL = new QGridLayout(optsW); optsL->setSpacing(8);
    for (int i = 0; i < 4; i++) {
        m_quizOpts[i] = new QPushButton(QString("%1)").arg(labels[i]), w);
        m_quizOpts[i]->setObjectName("actionBtn");
        m_quizOpts[i]->setEnabled(false);
        m_quizOpts[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_quizOpts[i]->setStyleSheet("text-align:left; padding: 8px 14px;");
        optsL->addWidget(m_quizOpts[i], i / 2, i % 2);
        const int idx = i;
        connect(m_quizOpts[i], &QPushButton::clicked, this, [this, idx]{ submitAnswer(idx); });
    }
    lay->addWidget(optsW);

    /* Feedback */
    m_quizFeedback = new QLabel("", w);
    m_quizFeedback->setObjectName("cardDesc");
    m_quizFeedback->setWordWrap(true);
    lay->addWidget(m_quizFeedback);

    /* Prossima domanda */
    m_quizNext = new QPushButton("Prossima domanda →", w);
    m_quizNext->setObjectName("actionBtn");
    m_quizNext->setVisible(false);
    lay->addWidget(m_quizNext);

    lay->addStretch(1);

    /* Buffer raw AI (debug nascosto) */
    m_quizRaw = new QTextEdit(w);
    m_quizRaw->setVisible(false);

    /* Connessioni */
    connect(back, &QPushButton::clicked, this, [this]{ m_inner->setCurrentIndex(0); });
    connect(m_quizGen,  &QPushButton::clicked, this, &ImparaPage::generateQuestion);
    connect(m_quizNext, &QPushButton::clicked, this, &ImparaPage::nextQuestion);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Quiz logic
   ══════════════════════════════════════════════════════════════ */
void ImparaPage::generateQuestion() {
    /* Guard: ignora click multipli mentre l'AI risponde */
    if (m_quizBusy) return;
    m_quizBusy = true;

    /* Prima domanda della sessione → inizializza stato */
    if (m_quiz.currentQ == 0) {
        m_quiz.subject    = m_quizSubj->currentText();
        m_quiz.difficulty = m_quizDiff->currentText();
        m_quiz.totalQ     = m_quizNum->currentText().toInt();
        m_quiz.correct    = 0;
        m_quiz.wrong      = 0;
    }
    m_quiz.answered = false;
    m_quizRaw->clear();
    m_quizGen->setEnabled(false);
    m_quizNext->setVisible(false);
    m_quizFeedback->clear();
    m_quizQuestion->setText("⏳  Generazione domanda in corso...");
    for (auto* b : m_quizOpts) { b->setEnabled(false); b->setStyleSheet("text-align:left;padding:8px 14px;"); }

    m_quizProgress->setText(QString("Domanda %1 / %2")
                             .arg(m_quiz.currentQ + 1).arg(m_quiz.totalQ));
    m_quizScore->setText(QString("✅ %1  ❌ %2").arg(m_quiz.correct).arg(m_quiz.wrong));

    QString sys =
        "Sei un professore che genera domande a risposta multipla.\n"
        "Rispondi SOLO con il seguente formato — nessun testo prima o dopo:\n\n"
        "DOMANDA: [testo della domanda]\n"
        "A) [opzione]\n"
        "B) [opzione]\n"
        "C) [opzione]\n"
        "D) [opzione]\n"
        "CORRETTA: [A o B o C o D]\n"
        "SPIEGAZIONE: [spiegazione breve in 1-2 frasi]\n\n"
        "La risposta corretta deve essere randomica (non sempre A o B).\n"
        "Non aggiungere altro testo. Rispondi sempre in italiano.";

    QString usr = QString("Genera una domanda a scelta multipla su %1 di difficoltà %2. "
                          "Segui esattamente il formato indicato.")
                  .arg(m_quiz.subject, m_quiz.difficulty);

    /* Accumula risposta nel buffer raw per il parsing finale */
    static QMetaObject::Connection cTok, cFin, cErr;
    disconnect(cTok); disconnect(cFin); disconnect(cErr);

    cTok = connect(m_ai, &AiClient::token, this, [=](const QString& t){
        m_quizRaw->insertPlainText(t);
    });
    cFin = connect(m_ai, &AiClient::finished, this, [=](const QString& full){
        disconnect(cTok); disconnect(cFin); disconnect(cErr);
        m_quizBusy = false;
        parseAndShowQuestion(full.isEmpty() ? m_quizRaw->toPlainText() : full);
        m_quizGen->setEnabled(false); /* non rigenerare durante il quiz */
    });
    cErr = connect(m_ai, &AiClient::error, this, [=](const QString& e){
        disconnect(cTok); disconnect(cFin); disconnect(cErr);
        m_quizBusy = false;
        m_quizQuestion->setText(QString("❌  Errore: %1\n\nRiprova o cambia modello.").arg(e));
        m_quizGen->setEnabled(true);
    });

    m_ai->chat(sys, usr);
}

void ImparaPage::parseAndShowQuestion(const QString& raw) {
    /* Estrae i campi dal formato strutturato */
    auto extract = [&](const QString& key) -> QString {
        QRegularExpression re(key + "\\s*:?\\s*(.+)");
        auto m = re.match(raw);
        return m.hasMatch() ? m.captured(1).trimmed() : "";
    };

    QString question = extract("DOMANDA");
    QString a        = extract("A\\)");
    QString b        = extract("B\\)");
    QString c        = extract("C\\)");
    QString d        = extract("D\\)");
    QString correct  = extract("CORRETTA");
    QString explain  = extract("SPIEGAZIONE");

    if (question.isEmpty() || a.isEmpty() || correct.isEmpty()) {
        /* Parsing fallito: mostra raw e lascia riprovare */
        m_quizQuestion->setText("⚠️  Il modello non ha rispettato il formato.\nRiprova.");
        m_quizGen->setEnabled(true);
        return;
    }

    m_quiz.correctLetter = correct.left(1).toUpper();
    m_quiz.explanation   = explain;

    m_quizQuestion->setText(QString("❓  %1").arg(question));
    QStringList opts = {a, b, c, d};
    static const QString labs[] = {"A","B","C","D"};
    for (int i = 0; i < 4; i++) {
        m_quizOpts[i]->setText(QString("%1)  %2").arg(labs[i], opts[i]));
        m_quizOpts[i]->setEnabled(true);
        m_quizOpts[i]->setStyleSheet("text-align:left; padding:8px 14px;");
    }
}

void ImparaPage::submitAnswer(int idx) {
    if (m_quiz.answered) return;
    m_quiz.answered = true;

    static const QString labs[] = {"A","B","C","D"};
    bool correct = (labs[idx] == m_quiz.correctLetter);
    if (correct) m_quiz.correct++; else m_quiz.wrong++;

    /* Colora i bottoni */
    for (int i = 0; i < 4; i++) {
        m_quizOpts[i]->setEnabled(false);
        if (labs[i] == m_quiz.correctLetter)
            m_quizOpts[i]->setStyleSheet(
                "text-align:left;padding:8px 14px;background:#2e7d32;color:#fff;border-radius:6px;");
        else if (i == idx)
            m_quizOpts[i]->setStyleSheet(
                "text-align:left;padding:8px 14px;background:#c62828;color:#fff;border-radius:6px;");
    }

    m_quizScore->setText(QString("✅ %1  ❌ %2").arg(m_quiz.correct).arg(m_quiz.wrong));

    if (correct)
        m_quizFeedback->setText(QString("✅  Corretto!  —  %1").arg(m_quiz.explanation));
    else
        m_quizFeedback->setText(
            QString("❌  Sbagliato. La risposta corretta era <b>%1</b>.  —  %2")
            .arg(m_quiz.correctLetter, m_quiz.explanation));
    m_quizFeedback->setStyleSheet(correct ? "color:#4caf50;" : "color:#ef5350;");

    m_quiz.currentQ++;

    if (m_quiz.currentQ >= m_quiz.totalQ) {
        m_quizNext->setText("📊  Fine quiz — vedi risultati");
    } else {
        m_quizNext->setText(QString("Prossima domanda → (%1/%2)")
                            .arg(m_quiz.currentQ + 1).arg(m_quiz.totalQ));
    }
    m_quizNext->setVisible(true);
}

void ImparaPage::nextQuestion() {
    if (m_quiz.currentQ >= m_quiz.totalQ) {
        endSession();
    } else {
        generateQuestion();
    }
}

void ImparaPage::endSession() {
    saveSession();
    m_quizQuestion->setText(
        QString("🏁  Quiz completato!\n\n"
                "Materia: %1  |  Difficoltà: %2\n"
                "✅ Corrette: %3 / %4  |  ❌ Sbagliate: %5\n\n"
                "Il risultato è stato salvato nella Dashboard.")
        .arg(m_quiz.subject, m_quiz.difficulty)
        .arg(m_quiz.correct).arg(m_quiz.totalQ).arg(m_quiz.wrong));

    for (auto* b : m_quizOpts) { b->setEnabled(false); b->setText(""); b->setStyleSheet(""); }
    m_quizFeedback->clear();
    m_quizNext->setVisible(false);

    /* Ripristina per nuovo quiz */
    m_quiz.currentQ = 0;
    m_quizGen->setText("▶  Nuovo Quiz");
    m_quizGen->setEnabled(true);
}

void ImparaPage::saveSession() {
    /* Leggi storico esistente */
    QFile f(historyPath());
    QJsonObject root;
    if (f.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
    }
    QJsonArray sessions = root["sessions"].toArray();

    /* Aggiungi sessione */
    QJsonObject s;
    s["date"]       = QDateTime::currentDateTime().toString(Qt::ISODate);
    s["subject"]    = m_quiz.subject;
    s["difficulty"] = m_quiz.difficulty;
    s["total"]      = m_quiz.totalQ;
    s["correct"]    = m_quiz.correct;
    s["wrong"]      = m_quiz.wrong;
    sessions.prepend(s);   /* più recente in testa */

    /* Mantieni max 500 sessioni */
    while (sessions.size() > 500) sessions.removeLast();

    root["sessions"] = sessions;
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.close();
    }
}

/* ══════════════════════════════════════════════════════════════
   DASHBOARD
   ══════════════════════════════════════════════════════════════ */
QWidget* ImparaPage::buildDashboard() {
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(24, 16, 24, 16); lay->setSpacing(8);

    auto* hdrW = new QWidget(w);
    auto* hdrL = new QHBoxLayout(hdrW); hdrL->setContentsMargins(0,0,0,0);
    auto* back = new QPushButton("← Torna", hdrW); back->setObjectName("actionBtn");
    auto* lbl  = new QLabel("📊  Dashboard Statistica", hdrW); lbl->setObjectName("pageTitle");
    auto* refr = new QPushButton("🔄 Aggiorna", hdrW); refr->setObjectName("actionBtn");
    hdrL->addWidget(back); hdrL->addWidget(lbl, 1); hdrL->addWidget(refr);
    lay->addWidget(hdrW);

    auto* div = new QFrame(w); div->setObjectName("pageDivider"); div->setFrameShape(QFrame::HLine);
    lay->addWidget(div);

    /* Area scrollabile con il contenuto della dashboard */
    auto* scroll = new QScrollArea(w);
    scroll->setObjectName("chatLog");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    m_dashContent = new QWidget;
    m_dashContent->setLayout(new QVBoxLayout);
    m_dashContent->layout()->setContentsMargins(0,0,0,0);
    scroll->setWidget(m_dashContent);
    lay->addWidget(scroll, 1);

    connect(back, &QPushButton::clicked, this, [this]{ m_inner->setCurrentIndex(0); });
    connect(refr, &QPushButton::clicked, this, &ImparaPage::loadDashboard);
    return w;
}

void ImparaPage::loadDashboard() {
    /* Svuota il contenuto precedente */
    while (QLayoutItem* item = m_dashContent->layout()->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    auto* vlay = qobject_cast<QVBoxLayout*>(m_dashContent->layout());

    /* Leggi storia */
    QFile f(historyPath());
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        auto* msg = new QLabel("📭  Nessun quiz ancora completato.\n\n"
                               "Completa almeno un quiz per vedere le statistiche qui.", m_dashContent);
        msg->setObjectName("cardDesc"); msg->setWordWrap(true); msg->setAlignment(Qt::AlignCenter);
        vlay->addWidget(msg); vlay->addStretch(1);
        return;
    }
    QJsonArray sessions = QJsonDocument::fromJson(f.readAll()).object()["sessions"].toArray();
    f.close();

    if (sessions.isEmpty()) {
        auto* msg = new QLabel("📭  Nessun quiz ancora completato.", m_dashContent);
        msg->setObjectName("cardDesc"); msg->setAlignment(Qt::AlignCenter);
        vlay->addWidget(msg); vlay->addStretch(1);
        return;
    }

    /* ── Statistiche globali ── */
    int totQ = 0, totC = 0;
    QMap<QString, QPair<int,int>> bySubject; /* subject → (correct, total) */
    for (auto v : sessions) {
        auto s = v.toObject();
        int tot = s["total"].toInt(); int cor = s["correct"].toInt();
        totQ += tot; totC += cor;
        auto& p = bySubject[s["subject"].toString()];
        p.first += cor; p.second += tot;
    }
    double globPct = totQ > 0 ? totC * 100.0 / totQ : 0.0;

    auto* globBox = new QGroupBox("📈  Statistiche globali", m_dashContent);
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
    auto* subjBox = new QGroupBox("📚  Per materia", m_dashContent);
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
        auto* pctLbl = new QLabel(QString("%1%  (%2/%3)").arg(pct,0,'f',1).arg(cor).arg(tot), row);
        pctLbl->setObjectName("gaugePct"); pctLbl->setFixedWidth(100);
        rl->addWidget(nm); rl->addWidget(bar, 1); rl->addWidget(pctLbl);
        subjL->addWidget(row);
    }
    vlay->addWidget(subjBox);

    /* ── Sessioni recenti ── */
    auto* recBox = new QGroupBox("🕐  Ultime 10 sessioni", m_dashContent);
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
            QString("✅%1/❌%2 — %3%")
            .arg(s["correct"].toInt()).arg(s["wrong"].toInt()).arg(pct,0,'f',0), row);
        score->setObjectName("cardDesc");
        rl->addWidget(date); rl->addWidget(subj); rl->addWidget(diff); rl->addWidget(score, 1);
        recL->addWidget(row);
    }
    vlay->addWidget(recBox);
    vlay->addStretch(1);
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
void ImparaPage::showEvent(QShowEvent* ev) {
    QWidget::showEvent(ev);
    /* Ogni volta che la pagina diventa visibile (click sidebar "Impara con AI")
     * torna al menu principale interno — evita le "schermate fantasma" */
    if (m_inner) m_inner->setCurrentIndex(0);
}

ImparaPage::ImparaPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_inner = new QStackedWidget(this);
    m_inner->addWidget(buildMenu());      /* 0 */
    m_inner->addWidget(buildTutor());     /* 1 */
    m_inner->addWidget(buildQuiz());      /* 2 */
    m_inner->addWidget(buildDashboard()); /* 3 */
    m_materiePage = new MateriePage(m_ai, this);
    m_inner->addWidget(m_materiePage);    /* 4 */

    m_simulatorePage = new SimulatorePage(m_ai, this);
    connect(m_simulatorePage, &SimulatorePage::backRequested,
            this, [this]{ m_inner->setCurrentIndex(0); });
    m_inner->addWidget(m_simulatorePage);  /* 5 */

    lay->addWidget(m_inner);
}

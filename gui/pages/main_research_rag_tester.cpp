/* ══════════════════════════════════════════════════════════════
   main_research_rag_tester.cpp — Test comprensione documenti RAG
   Seleziona un documento, genera domande con AI (incluse trabocchetto),
   fa rispondere l'AI usando il testo come contesto, valuta la comprensione.
   ══════════════════════════════════════════════════════════════ */
#include "main_research.h"
#include "../prismalux_paths.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QFrame>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QProcess>
#include <QScrollArea>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   buildRagTesterTab — pannello test comprensione RAG
   ══════════════════════════════════════════════════════════════ */
QWidget* RicercaPage::buildRagTesterTab()
{
    auto* page    = new QWidget;
    auto* mainLay = new QVBoxLayout(page);
    mainLay->setContentsMargins(14, 12, 14, 12);
    mainLay->setSpacing(10);

    /* Titolo */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\xa7\xaa  <b>Test Comprensione RAG</b> "
        "\xe2\x80\x94 "
        "Verifica che l'AI abbia letto e compreso i documenti. "
        "Genera domande reali <b>e a trabocchetto</b>, poi valuta le risposte.", page);
    titleLbl->setObjectName("sectionTitle");
    titleLbl->setTextFormat(Qt::RichText);
    titleLbl->setWordWrap(true);
    mainLay->addWidget(titleLbl);

    /* ── Splitter principale: sinistra lista doc, destra area test ── */
    auto* splitter = new QSplitter(Qt::Horizontal, page);
    splitter->setChildrenCollapsible(false);

    /* ══════════════════════════════════
       Pannello SINISTRA: lista documenti
       ══════════════════════════════════ */
    auto* leftPanel = new QWidget;
    auto* leftLay   = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(8);

    auto* docTitle = new QLabel(tr("\xf0\x9f\x93\x81  Documenti RAG disponibili"), leftPanel);
    docTitle->setObjectName("cardTitle");
    leftLay->addWidget(docTitle);

    m_ragTesterDocList = new QListWidget(leftPanel);
    m_ragTesterDocList->setAlternatingRowColors(true);
    m_ragTesterDocList->setToolTip(tr("Seleziona un documento per testarne la comprensione"));
    leftLay->addWidget(m_ragTesterDocList, 1);

    auto* reloadBtn = new QPushButton(tr("\xf0\x9f\x94\x84  Aggiorna lista"), leftPanel);
    reloadBtn->setObjectName("actionBtn");
    leftLay->addWidget(reloadBtn);

    /* Popola la lista all'avvio */
    auto populateDocs = [this]() {
        if (!m_ragTesterDocList) return;
        m_ragTesterDocList->clear();
        const QStringList ragDirs = {
            QDir::homePath() + "/prismalux_rag_docs",
            P::ragDir()
        };
        static const QStringList kExts = {
            "*.pdf", "*.txt", "*.md", "*.csv",
            "*.doc", "*.docx", "*.jpg", "*.jpeg", "*.png", "*.mp3", "*.wav"
        };
        for (const QString& dir : ragDirs) {
            QDirIterator it(dir, kExts, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                auto* item = new QListWidgetItem(
                    QFileInfo(it.filePath()).fileName());
                item->setData(Qt::UserRole, it.filePath());
                item->setToolTip(it.filePath());
                m_ragTesterDocList->addItem(item);
            }
        }
    };
    populateDocs();

    connect(reloadBtn, &QPushButton::clicked, leftPanel, populateDocs);

    splitter->addWidget(leftPanel);

    /* ══════════════════════════════════
       Pannello DESTRA: area test
       ══════════════════════════════════ */
    auto* rightPanel = new QWidget;
    auto* rightLay   = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(8);

    /* Barra azioni */
    auto* btnRow = new QWidget(rightPanel);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    m_ragTesterGenBtn  = new QPushButton(
        "\xf0\x9f\x93\x9d  Genera domande", btnRow);
    m_ragTesterGenBtn->setObjectName("actionBtn");
    m_ragTesterGenBtn->setToolTip(
        "Legge il documento selezionato e genera 10 domande:\n"
        "7 sulla comprensione del testo + 3 trabocchetto (logica/fatto non presente)");

    m_ragTesterRunBtn  = new QPushButton(
        "\xf0\x9f\xa4\x96  Esegui test", btnRow);
    m_ragTesterRunBtn->setObjectName("actionBtn");
    m_ragTesterRunBtn->setEnabled(false);
    m_ragTesterRunBtn->setToolTip(
        "L'AI risponde a tutte le domande usando solo il testo del documento come contesto");

    m_ragTesterEvalBtn = new QPushButton(
        "\xe2\x9c\x85  Valuta risposte", btnRow);
    m_ragTesterEvalBtn->setObjectName("actionBtn");
    m_ragTesterEvalBtn->setEnabled(false);
    m_ragTesterEvalBtn->setToolTip(
        "L'AI valuta le proprie risposte: correttezza, logica, identificazione trabocchetti");

    m_ragTesterStatus = new QLabel("", btnRow);
    m_ragTesterStatus->setObjectName("hintLabel");

    btnLay->addWidget(m_ragTesterGenBtn);
    btnLay->addWidget(m_ragTesterRunBtn);
    btnLay->addWidget(m_ragTesterEvalBtn);
    btnLay->addWidget(m_ragTesterStatus, 1);
    rightLay->addWidget(btnRow);

    /* Area domande */
    auto* qGrp = new QGroupBox(
        "\xf0\x9f\x93\x9d  Domande generate (7 comprensione + 3 trabocchetto)", rightPanel);
    auto* qLay = new QVBoxLayout(qGrp);
    qLay->setContentsMargins(8, 8, 8, 8);
    m_ragTesterQuestions = new QTextEdit(qGrp);
    m_ragTesterQuestions->setReadOnly(true);
    m_ragTesterQuestions->setPlaceholderText(
        "Le domande appariranno qui dopo aver premuto \"Genera domande\".\n"
        "Le domande trabocchetto sono contrassegnate con [TRABOCCHETTO].");
    m_ragTesterQuestions->setMinimumHeight(180);
    qLay->addWidget(m_ragTesterQuestions);
    rightLay->addWidget(qGrp);

    /* Area risposte + valutazione */
    auto* aGrp = new QGroupBox(
        "\xf0\x9f\xa4\x96  Risposte AI + Valutazione", rightPanel);
    auto* aLay = new QVBoxLayout(aGrp);
    aLay->setContentsMargins(8, 8, 8, 8);
    m_ragTesterAnswers = new QTextEdit(aGrp);
    m_ragTesterAnswers->setReadOnly(true);
    m_ragTesterAnswers->setPlaceholderText(
        "Le risposte appariranno qui dopo \"Esegui test\".\n"
        "La valutazione finale apparirà dopo \"Valuta risposte\".");
    m_ragTesterAnswers->setMinimumHeight(220);
    aLay->addWidget(m_ragTesterAnswers);
    rightLay->addWidget(aGrp, 1);

    splitter->addWidget(rightPanel);
    splitter->setSizes({240, 700});
    mainLay->addWidget(splitter, 1);

    connect(m_ragTesterGenBtn,  &QPushButton::clicked, this, &RicercaPage::onRagTesterGenClicked);
    connect(m_ragTesterRunBtn,  &QPushButton::clicked, this, &RicercaPage::onRagTesterRunClicked);
    connect(m_ragTesterEvalBtn, &QPushButton::clicked, this, &RicercaPage::onRagTesterEvalClicked);

    return page;
}

/* ══════════════════════════════════════════════════════════════
   Slot: Genera domande
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onRagTesterGenClicked()
{
    auto* item = m_ragTesterDocList
                 ? m_ragTesterDocList->currentItem() : nullptr;
    if (!item) {
        if (m_ragTesterStatus)
            m_ragTesterStatus->setText(
                "\xe2\x9a\xa0\xef\xb8\x8f  Seleziona un documento dalla lista");
        return;
    }
    const QString path = item->data(Qt::UserRole).toString();
    const QString ext  = QFileInfo(path).suffix().toLower();

    if (m_ragTesterStatus)
        m_ragTesterStatus->setText(
            "\xe2\x8f\xb3  Estrazione testo in corso...");
    m_ragTesterGenBtn->setEnabled(false);

    auto onTextReady = [this](const QString& rawText) {
        m_ragTesterDocText = rawText.left(6000);
        if (m_ragTesterDocText.trimmed().isEmpty()) {
            if (m_ragTesterStatus)
                m_ragTesterStatus->setText(
                    "\xe2\x9d\x8c  Impossibile estrarre testo dal documento");
            if (m_ragTesterGenBtn) m_ragTesterGenBtn->setEnabled(true);
            return;
        }
        const QString modelName = (m_ai && !m_ai->model().isEmpty())
                                  ? m_ai->model() : QString("AI");
        if (m_ragTesterStatus)
            m_ragTesterStatus->setText(
                "\xf0\x9f\xa4\x96  Generazione domande con " + modelName + "...");

        const QString sysDomande =
            "Sei un esperto valutatore della comprensione di testi. "
            "Dato un testo, genera esattamente 10 domande: "
            "7 domande reali sulla comprensione del contenuto "
            "(fatti, concetti, relazioni, implicazioni) "
            "e 3 domande trabocchetto che contengono una premessa falsa o "
            "chiedono di qualcosa NON presente nel testo. "
            "Le domande trabocchetto devono essere plausibili ma ingannevoli. "
            "Formato di output:\n"
            "Q1: [domanda]\n"
            "Q2: [domanda]\n"
            "...\n"
            "Q8: [TRABOCCHETTO] [domanda]\n"
            "Q9: [TRABOCCHETTO] [domanda]\n"
            "Q10: [TRABOCCHETTO] [domanda]\n"
            "Numera sempre 1-10 e marca i trabocchetti con [TRABOCCHETTO].";

        const QString userDomande =
            "Genera 10 domande (7 reali + 3 trabocchetto) su questo testo:\n\n"
            + m_ragTesterDocText;

        if (m_ragTesterQuestions) m_ragTesterQuestions->clear();

        auto* holder = new QObject(this);
        connect(m_ai, &AiClient::token, holder, [this](const QString& t) {
            if (m_ragTesterQuestions) {
                m_ragTesterQuestions->moveCursor(QTextCursor::End);
                m_ragTesterQuestions->insertPlainText(t);
            }
        });
        connect(m_ai, &AiClient::finished, holder, [this, holder](const QString&) {
            holder->deleteLater();
            if (m_ragTesterStatus)
                m_ragTesterStatus->setText(
                    "\xe2\x9c\x85  Domande generate. Premi \"Esegui test\" per testare.");
            if (m_ragTesterGenBtn) m_ragTesterGenBtn->setEnabled(true);
            if (m_ragTesterRunBtn) m_ragTesterRunBtn->setEnabled(true);
        });
        connect(m_ai, &AiClient::error, holder, [this, holder](const QString& err) {
            holder->deleteLater();
            if (m_ragTesterStatus)
                m_ragTesterStatus->setText(tr("\xe2\x9d\x8c  Errore AI: ") + err);
            if (m_ragTesterGenBtn) m_ragTesterGenBtn->setEnabled(true);
        });
        m_ai->chat(sysDomande, userDomande);
    };

    if (ext == "pdf") {
        auto* proc = new QProcess(this);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, onTextReady](int, QProcess::ExitStatus) {
            const QString text = QString::fromUtf8(proc->readAllStandardOutput());
            proc->deleteLater();
            onTextReady(text);
        });
        connect(proc, &QProcess::errorOccurred, this,
            [this, proc](QProcess::ProcessError err) {
                if (err == QProcess::FailedToStart) {
                    if (m_ragTesterStatus)
                        m_ragTesterStatus->setText(
                            tr("\xe2\x9d\x8c  pdftotext non trovato \xe2\x80\x94 sudo apt install poppler-utils"));
                    proc->deleteLater();
                }
            });
        proc->start("pdftotext", {path, "-"});
    } else if (ext == "txt" || ext == "md" || ext == "csv" || ext == "log") {
        QFile f(path);
        QString text;
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            text = QString::fromUtf8(f.readAll());
        onTextReady(text);
    } else if (ext == "jpg" || ext == "jpeg" || ext == "png") {
        if (m_ragTesterStatus)
            m_ragTesterStatus->setText(
                "\xf0\x9f\xa4\x96  Analisi immagine con AI...");
        const QString sysVis =
            "Descrivi in modo dettagliato e strutturato il contenuto di questa immagine.";
        auto* holder = new QObject(this);
        connect(m_ai, &AiClient::finished, holder,
                [this, holder, onTextReady](const QString& desc) {
            holder->deleteLater();
            onTextReady(desc);
        });
        connect(m_ai, &AiClient::error, holder,
                [this, holder](const QString& err) {
            holder->deleteLater();
            if (m_ragTesterStatus)
                m_ragTesterStatus->setText(
                    "\xe2\x9d\x8c  Errore analisi immagine: " + err);
            if (m_ragTesterGenBtn) m_ragTesterGenBtn->setEnabled(true);
        });
        m_ai->chat(sysVis, "Descrivi questa immagine.", {}, AiClient::QuerySimple);
    } else {
        onTextReady(
            QString("Documento: %1\nTipo: %2\nNota: tipo non supportato per estrazione testo.").arg(
                QFileInfo(path).fileName(), ext));
    }
}

/* ══════════════════════════════════════════════════════════════
   Slot: Esegui test (AI risponde alle domande)
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onRagTesterRunClicked()
{
    const QString questions = m_ragTesterQuestions
                              ? m_ragTesterQuestions->toPlainText().trimmed() : "";
    if (questions.isEmpty() || m_ragTesterDocText.isEmpty()) return;

    if (m_ragTesterStatus)
        m_ragTesterStatus->setText(tr("\xf0\x9f\xa4\x96  Risposta alle domande in corso..."));
    m_ragTesterRunBtn->setEnabled(false);
    if (m_ragTesterAnswers) m_ragTesterAnswers->clear();

    const QString sysRisposte =
        "Sei un assistente preciso. Devi rispondere alle domande usando SOLO "
        "le informazioni presenti nel testo di contesto fornito. "
        "Se una domanda contiene una premessa falsa o chiede di qualcosa non presente "
        "nel testo, rispondi: \"[TRABOCCHETTO RILEVATO] La premessa è errata / "
        "questo non è presente nel documento.\" "
        "Rispondi a ogni domanda numerata.\n\n"
        "TESTO DEL DOCUMENTO:\n" + m_ragTesterDocText;

    const QString userRisposte = "Rispondi a queste domande:\n\n" + questions;

    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::token, holder, [this](const QString& t) {
        if (m_ragTesterAnswers) {
            m_ragTesterAnswers->moveCursor(QTextCursor::End);
            m_ragTesterAnswers->insertPlainText(t);
        }
    });
    connect(m_ai, &AiClient::finished, holder, [this, holder](const QString&) {
        holder->deleteLater();
        if (m_ragTesterStatus)
            m_ragTesterStatus->setText(
                "\xe2\x9c\x85  Test completato. Premi \"Valuta risposte\" per la valutazione.");
        if (m_ragTesterRunBtn)  m_ragTesterRunBtn->setEnabled(true);
        if (m_ragTesterEvalBtn) m_ragTesterEvalBtn->setEnabled(true);
    });
    connect(m_ai, &AiClient::error, holder, [this, holder](const QString& err) {
        holder->deleteLater();
        if (m_ragTesterStatus)
            m_ragTesterStatus->setText(tr("\xe2\x9d\x8c  Errore: ") + err);
        if (m_ragTesterRunBtn) m_ragTesterRunBtn->setEnabled(true);
    });
    m_ai->chat(sysRisposte, userRisposte);
}

/* ══════════════════════════════════════════════════════════════
   Slot: Valuta risposte
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onRagTesterEvalClicked()
{
    const QString questions = m_ragTesterQuestions
                              ? m_ragTesterQuestions->toPlainText().trimmed() : "";
    const QString answers   = m_ragTesterAnswers
                              ? m_ragTesterAnswers->toPlainText().trimmed() : "";
    if (questions.isEmpty() || answers.isEmpty()) return;

    if (m_ragTesterStatus)
        m_ragTesterStatus->setText(tr("\xf0\x9f\xa4\x96  Valutazione in corso..."));
    m_ragTesterEvalBtn->setEnabled(false);

    const QString sysValuta =
        "Sei un valutatore esperto. Analizza le domande e le relative risposte AI. "
        "Per ogni risposta indica:\n"
        "- [OK]: risposta corretta e completa\n"
        "- [PARZ]: risposta parzialmente corretta\n"
        "- [ERR]: risposta errata o mancante\n"
        "- [TRABOCCHETTO-RILEVATO]: l'AI ha correttamente identificato il trabocchetto\n"
        "- [TRABOCCHETTO-CADUTO]: l'AI non ha rilevato il trabocchetto e ha risposto come se fosse vera\n"
        "Alla fine dai un punteggio totale /10 e un giudizio sintetico sulla comprensione.\n\n"
        "TESTO ORIGINALE (prime 1500 char):\n" + m_ragTesterDocText.left(1500);

    const QString userValuta =
        "DOMANDE:\n" + questions + "\n\nRISPOSTE AI:\n" + answers +
        "\n\nFornisci la valutazione dettagliata con punteggio finale.";

    if (m_ragTesterAnswers) {
        m_ragTesterAnswers->append(
            "\n\n\xe2\x80\x94\xe2\x80\x94 VALUTAZIONE \xe2\x80\x94\xe2\x80\x94\n");
    }

    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::token, holder, [this](const QString& t) {
        if (m_ragTesterAnswers) {
            m_ragTesterAnswers->moveCursor(QTextCursor::End);
            m_ragTesterAnswers->insertPlainText(t);
        }
    });
    connect(m_ai, &AiClient::finished, holder, [this, holder](const QString&) {
        holder->deleteLater();
        if (m_ragTesterStatus)
            m_ragTesterStatus->setText(tr("\xe2\x9c\x85  Valutazione completata."));
        if (m_ragTesterEvalBtn) m_ragTesterEvalBtn->setEnabled(true);
    });
    connect(m_ai, &AiClient::error, holder, [this, holder](const QString& err) {
        holder->deleteLater();
        if (m_ragTesterStatus)
            m_ragTesterStatus->setText(tr("\xe2\x9d\x8c  ") + err);
        if (m_ragTesterEvalBtn) m_ragTesterEvalBtn->setEnabled(true);
    });
    m_ai->chat(sysValuta, userValuta);
}

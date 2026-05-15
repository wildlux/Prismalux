/* ══════════════════════════════════════════════════════════════
   programmazione_page_translitter.cpp

   Sub-tab "🔀 Translitter" — traduce codice da un linguaggio
   all'altro (C↔Python, Python↔JavaScript, ecc.) tramite LLM locale.

   Fa parte di ProgrammazionePage (dichiarazioni in programmazione_page.h).
   ══════════════════════════════════════════════════════════════ */
#include "programmazione_page.h"
#include "../prismalux_paths.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QSplitter>
#include <QFont>
#include <QTextCursor>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>
#include <QTimer>

namespace P = PrismaluxPaths;

/* ── Lista linguaggi supportati ────────────────────────────── */
static const QStringList kLangs = {
    "C", "C++", "Python", "JavaScript", "TypeScript",
    "Java", "Kotlin", "Rust", "Go", "PHP",
    "Bash", "SQL", "Swift", "Lua", "Ruby", "C#"
};

/* Mappa linguaggio → fence markdown (per estrarre il blocco codice) */
static QString langFence(const QString& lang)
{
    const QString l = lang.toLower();
    if (l == "c++")        return "cpp";
    if (l == "c#")         return "csharp";
    if (l == "typescript") return "ts";
    if (l == "javascript") return "js";
    if (l == "bash")       return "bash";
    if (l == "kotlin")     return "kotlin";
    if (l == "swift")      return "swift";
    return l;
}

/* ══════════════════════════════════════════════════════════════
   buildTranslitter — costruisce il sub-tab UI
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildTranslitter(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    const int appPt = QApplication::font().pointSize();
    monoFont.setPointSize(appPt > 0 ? appPt : 11);

    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(8);

    /* ── Riga controlli: src lang ↔ dst lang + modello ── */
    auto* ctrlRow = new QWidget(w);
    auto* ctrlLay = new QHBoxLayout(ctrlRow);
    ctrlLay->setContentsMargins(0, 0, 0, 0);
    ctrlLay->setSpacing(8);

    ctrlLay->addWidget(new QLabel("Da:", ctrlRow));
    m_trSrcLang = new QComboBox(ctrlRow);
    m_trSrcLang->setObjectName("settingCombo");
    m_trSrcLang->addItems(kLangs);
    m_trSrcLang->setCurrentText("C");
    m_trSrcLang->setToolTip("Linguaggio sorgente del codice da tradurre");
    ctrlLay->addWidget(m_trSrcLang);

    /* Pulsante scambia lingue */
    auto* btnSwap = new QPushButton("\xe2\x87\x84", ctrlRow);   /* ⇄ */
    btnSwap->setObjectName("actionBtn");
    btnSwap->setFixedWidth(36);
    btnSwap->setToolTip("Scambia linguaggio sorgente e destinazione");
    ctrlLay->addWidget(btnSwap);

    ctrlLay->addWidget(new QLabel("A:", ctrlRow));
    m_trDstLang = new QComboBox(ctrlRow);
    m_trDstLang->setObjectName("settingCombo");
    m_trDstLang->addItems(kLangs);
    m_trDstLang->setCurrentText("Python");
    m_trDstLang->setToolTip("Linguaggio di destinazione della traduzione");
    ctrlLay->addWidget(m_trDstLang);

    ctrlLay->addSpacing(16);
    ctrlLay->addWidget(new QLabel("Modello:", ctrlRow));
    m_trModel = new QComboBox(ctrlRow);
    m_trModel->setObjectName("settingCombo");
    m_trModel->setMinimumWidth(170);
    m_trModel->setToolTip("Modello AI da usare per la traduzione\n(lascia vuoto per usare quello attivo)");
    /* Voce iniziale = modello attivo corrente */
    if (m_ai) {
        const QString cur = m_ai->model();
        m_trModel->addItem(cur.isEmpty() ? "(modello attivo)" : cur,
                           cur);
    } else {
        m_trModel->addItem("(modello attivo)", QString());
    }
    ctrlLay->addWidget(m_trModel);

    ctrlLay->addStretch(1);

    m_btnTrRun = new QPushButton("\xf0\x9f\x94\x80  Traduci", ctrlRow);   /* 🔀 */
    m_btnTrRun->setObjectName("actionBtn");
    m_btnTrRun->setProperty("highlight", "true");
    m_btnTrRun->setToolTip("Avvia la traduzione del codice sorgente nel linguaggio scelto");
    ctrlLay->addWidget(m_btnTrRun);

    m_btnTrStop = new QPushButton("\xe2\x96\xa0  Stop", ctrlRow);
    m_btnTrStop->setObjectName("actionBtn");
    m_btnTrStop->setEnabled(false);
    ctrlLay->addWidget(m_btnTrStop);

    lay->addWidget(ctrlRow);

    /* ── Splitter orizzontale: input | output ── */
    auto* splitter = new QSplitter(Qt::Horizontal, w);
    splitter->setHandleWidth(6);

    /* Pannello sorgente */
    auto* srcGroup = new QGroupBox("Codice sorgente", splitter);
    auto* srcLay   = new QVBoxLayout(srcGroup);
    srcLay->setContentsMargins(6, 6, 6, 6);

    m_trInput = new QPlainTextEdit(srcGroup);
    m_trInput->setFont(monoFont);
    m_trInput->setObjectName("codeEditor");
    m_trInput->setPlaceholderText(
        "Incolla qui il codice da tradurre...\n\n"
        "Esempio C:\n"
        "  int somma(int a, int b) {\n"
        "      return a + b;\n"
        "  }");
    srcLay->addWidget(m_trInput, 1);

    /* Bottone copia dal main editor */
    auto* btnFromEditor = new QPushButton("\xe2\xac\x86  Importa dall'editor", srcGroup);
    btnFromEditor->setObjectName("actionBtn");
    btnFromEditor->setToolTip("Copia il codice dall'editor principale in questo pannello");
    srcLay->addWidget(btnFromEditor);

    splitter->addWidget(srcGroup);

    /* Pannello output tradotto */
    auto* dstGroup = new QGroupBox("Codice tradotto", splitter);
    auto* dstLay   = new QVBoxLayout(dstGroup);
    dstLay->setContentsMargins(6, 6, 6, 6);

    m_trOutput = new QTextEdit(dstGroup);
    m_trOutput->setObjectName("chatLog");
    m_trOutput->setReadOnly(true);
    m_trOutput->setFont(monoFont);
    m_trOutput->setPlaceholderText(
        "Il codice tradotto apparir\xc3\xa0 qui durante lo streaming...");
    dstLay->addWidget(m_trOutput, 1);

    /* Bottoni azione output */
    auto* outBtnRow = new QWidget(dstGroup);
    auto* outBtnLay = new QHBoxLayout(outBtnRow);
    outBtnLay->setContentsMargins(0, 0, 0, 0);
    outBtnLay->setSpacing(6);

    m_btnTrInsert = new QPushButton(
        "\xe2\xac\x86  Inserisci nell'editor", outBtnRow);
    m_btnTrInsert->setObjectName("actionBtn");
    m_btnTrInsert->setEnabled(false);
    m_btnTrInsert->setToolTip(
        "Estrae il primo blocco codice dalla risposta AI\n"
        "e lo inserisce nell'editor principale (sostituisce il contenuto attuale)");
    outBtnLay->addWidget(m_btnTrInsert);

    m_btnTrCopy = new QPushButton("📋  Copia", outBtnRow);
    m_btnTrCopy->setObjectName("actionBtn");
    m_btnTrCopy->setEnabled(false);
    m_btnTrCopy->setObjectName("btnTrCopy");
    m_btnTrCopy->setToolTip("Copia tutto il testo dell'output negli appunti");
    outBtnLay->addWidget(m_btnTrCopy);

    outBtnLay->addStretch(1);
    dstLay->addWidget(outBtnRow);

    splitter->addWidget(dstGroup);
    splitter->setSizes({ 1, 1 });
    lay->addWidget(splitter, 1);

    /* ── Riga stato ── */
    auto* m_trStatus = new QLabel(
        "\xf0\x9f\x94\x80  Inserisci il codice e premi \xe2\x80\x9cTraduci\xe2\x80\x9d.", w);
    m_trStatus->setObjectName("statusLabel");
    lay->addWidget(m_trStatus);

    /* ── Popola combo modelli ── */
    /* Popola al primo focus sul tab -- lazy */
    connect(m_trModel, &QComboBox::activated,
            this, &ProgrammazionePage::onTrModelActivated);

    /* ── Connessioni ── */
    /* -- Connessioni -- */
    connect(btnSwap, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnSwapLangsClicked);

    connect(btnFromEditor, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnFromEditorClicked);

    connect(m_btnTrRun, &QPushButton::clicked,
            this, &ProgrammazionePage::runTranslitter);

    connect(m_btnTrStop, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnTrStopClicked);

    connect(m_btnTrInsert, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnTrInsertClicked);

    connect(m_btnTrCopy, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnTrCopyClicked);

    /* Abilita/disabilita "Copia" in base al contenuto */
    connect(m_trOutput, &QTextEdit::textChanged,
            this, &ProgrammazionePage::onTrOutputTextChanged);

    /* Sincronizzazione modello */
    connect(m_ai, &AiClient::modelChanged,
            this, &ProgrammazionePage::onTrModelChanged);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   runTranslitter — compone il prompt e lancia lo streaming.

   System prompt: traduttore specializzato, preserva la logica,
   adatta nomi/idiomi al linguaggio target, produce solo codice
   in un blocco ``` ... ```.
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::runTranslitter()
{
    if (!m_ai) return;

    const QString sorgente = m_trInput ? m_trInput->toPlainText().trimmed() : QString();
    if (sorgente.isEmpty()) {
        if (m_trOutput)
            m_trOutput->setPlainText(
                "\xe2\x9d\x8c  Nessun codice sorgente da tradurre.\n"
                "Incolla il codice nel pannello di sinistra.");
        return;
    }

    if (m_ai->busy()) {
        if (m_trOutput)
            m_trOutput->setPlainText(
                "\xe2\x9a\xa0\xef\xb8\x8f  AI occupata. Attendi o premi Stop.");
        return;
    }

    const QString src  = m_trSrcLang ? m_trSrcLang->currentText() : "C";
    const QString dst  = m_trDstLang ? m_trDstLang->currentText() : "Python";

    if (src == dst) {
        if (m_trOutput)
            m_trOutput->setPlainText(
                "\xe2\x9a\xa0\xef\xb8\x8f  Linguaggio sorgente e destinazione sono identici.\n"
                "Seleziona due linguaggi diversi.");
        return;
    }

    /* Applica modello scelto */
    if (m_trModel) {
        const QString sel = m_trModel->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    const QString fence = langFence(dst);

    const QString sys = QString(
        "Sei un traduttore di codice esperto. "
        "Il tuo unico compito \xc3\xa8 tradurre codice %1 in codice %2.\n\n"
        "Regole:\n"
        "  1. Preserva COMPLETAMENTE la logica e le funzionalit\xc3\xa0 del codice originale.\n"
        "  2. Adatta nomi di variabili, funzioni e classi agli idiomi di %2\n"
        "     (es. snake_case in Python, camelCase in Java/JavaScript).\n"
        "  3. Usa le librerie standard di %2 quando possibile\n"
        "     (es. lists Python invece di array C, fmt in Rust invece di printf).\n"
        "  4. Sostituisci i costrutti non disponibili con gli equivalenti idiomatici\n"
        "     (es. puntatori C \xe2\x86\x92 riferimenti Python, malloc/free \xe2\x86\x92 gestione automatica memoria).\n"
        "  5. Rispondi con SOLO il codice tradotto in un blocco ```%3 ... ```.\n"
        "  6. Dopo il blocco codice, aggiungi una sezione 'NOTE' breve (max 3 punti)\n"
        "     solo se ci sono differenze comportamentali significative da segnalare.\n"
        "  7. Rispondi SEMPRE in italiano per le note.\n\n"
        "Non aggiungere spiegazioni o testo prima del blocco codice."
    ).arg(src, dst, fence);

    const QString user = QString(
        "Traduci il seguente codice %1 in %2:\n\n```%3\n%4\n```"
    ).arg(src, dst, langFence(src), sorgente);

    /* ── Avvio streaming ── */
    m_trOutput->clear();
    {
        const QString modelName = m_ai->model().isEmpty() ? "AI" : m_ai->model();
        m_trOutput->setPlainText(
            QString("\xf0\x9f\x94\x80  %1 \xe2\x86\x92 %2  |  Modello: %3\n%4\n\n")
            .arg(src, dst, modelName,
                 QString(qMax(src.length() + dst.length() + modelName.length() + 8, 30), '-')));
    }

    m_btnTrRun->setEnabled(false);
    m_btnTrStop->setEnabled(true);
    m_btnTrInsert->setEnabled(false);

    if (m_trTokenHolder) { delete m_trTokenHolder; m_trTokenHolder = nullptr; }
    m_trTokenHolder = new QObject(this);

    connect(m_ai, &AiClient::token, m_trTokenHolder,
            [this](const QString& tok){ onTrToken(tok); });
    connect(m_ai, &AiClient::finished, m_trTokenHolder,
            [this](const QString& full){ onTrFinished(full); });
    connect(m_ai, &AiClient::error, m_trTokenHolder,
            [this](const QString& msg){ onTrError(msg); });

    m_ai->chat(P::prependKnowledge(sys), user);
}

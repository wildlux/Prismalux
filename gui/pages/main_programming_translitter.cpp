/* ══════════════════════════════════════════════════════════════
   programmazione_page_translitter.cpp

   Sub-tab "🔀 Translitter" — traduce codice da un linguaggio
   all'altro (C↔Python, Python↔JavaScript, ecc.) tramite LLM locale.

   Fa parte di ProgrammazionePage (dichiarazioni in programmazione_page.h).
   ══════════════════════════════════════════════════════════════ */
#include "../dpi_utils.h"
#include "main_programming.h"
#include "../prismalux_paths.h"
#include "../ai_utils.h"
#include "../log_bus.h"

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
   buildTranslitter — sub-tab "🔀 Translitter"
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildTranslitter(QWidget* parent)
{
    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(8);

    QPushButton* btnSwap       = nullptr;
    QPushButton* btnFromEditor = nullptr;

    lay->addWidget(buildTrControlRow(w, btnSwap, btnFromEditor));
    lay->addWidget(buildTrSplitter(w), 1);

    /* Riga stato */
    auto* statusLbl = new QLabel(
        "\xf0\x9f\x94\x80  Inserisci il codice e premi "
        "\xe2\x80\x9c" "Traduci" "\xe2\x80\x9d.", w);
    statusLbl->setObjectName("statusLabel");
    lay->addWidget(statusLbl);

    QTimer::singleShot(0, this, &ProgrammazionePage::populateTrModels);
    setupTrConnections(btnSwap, btnFromEditor);
    return w;
}

/* ── Riga controlli: lingue + modello + pulsanti ── */
QWidget* ProgrammazionePage::buildTrControlRow(QWidget* parent,
                                                QPushButton*& outBtnSwap,
                                                QPushButton*& outBtnFromEditor)
{
    auto* ctrlRow = new QWidget(parent);
    auto* ctrlLay = new QHBoxLayout(ctrlRow);
    ctrlLay->setContentsMargins(0, 0, 0, 0);
    ctrlLay->setSpacing(8);

    ctrlLay->addWidget(new QLabel(tr("Da:"), ctrlRow));
    m_trSrcLang = new QComboBox(ctrlRow);
    m_trSrcLang->setObjectName("settingCombo");
    m_trSrcLang->addItems(kLangs);
    m_trSrcLang->setCurrentText("C");
    m_trSrcLang->setToolTip(tr("Linguaggio sorgente del codice da tradurre"));
    ctrlLay->addWidget(m_trSrcLang);

    outBtnSwap = new QPushButton("\xe2\x87\x84", ctrlRow);
    outBtnSwap->setObjectName("actionBtn");
    outBtnSwap->setFixedWidth(dpiScale(36));
    outBtnSwap->setToolTip(tr("Scambia linguaggio sorgente e destinazione"));
    ctrlLay->addWidget(outBtnSwap);

    ctrlLay->addWidget(new QLabel("A:", ctrlRow));
    m_trDstLang = new QComboBox(ctrlRow);
    m_trDstLang->setObjectName("settingCombo");
    m_trDstLang->addItems(kLangs);
    m_trDstLang->setCurrentText("Python");
    m_trDstLang->setToolTip(tr("Linguaggio di destinazione della traduzione"));
    ctrlLay->addWidget(m_trDstLang);

    ctrlLay->addSpacing(16);
    ctrlLay->addWidget(new QLabel(tr("Modello:"), ctrlRow));
    m_trModel = new QComboBox(ctrlRow);
    m_trModel->setObjectName("settingCombo");
    m_trModel->setMinimumWidth(170);
    m_trModel->setToolTip(
        "Modello AI da usare per la traduzione\n"
        "(lascia vuoto per usare quello attivo)");
    if (m_ai) {
        const QString cur = m_ai->model();
        m_trModel->addItem(cur.isEmpty() ? "(modello attivo)" : cur, cur);
    } else {
        m_trModel->addItem("(modello attivo)", QString());
    }
    ctrlLay->addWidget(m_trModel);
    ctrlLay->addStretch(1);

    m_btnTrRun = new QPushButton(tr("\xf0\x9f\x94\x80  Traduci"), ctrlRow);
    m_btnTrRun->setObjectName("actionBtn");
    m_btnTrRun->setProperty("highlight", "true");
    m_btnTrRun->setToolTip(tr("Avvia la traduzione del codice sorgente nel linguaggio scelto"));
    ctrlLay->addWidget(m_btnTrRun);

    m_btnTrStop = new QPushButton(tr("\xe2\x96\xa0  Stop"), ctrlRow);
    m_btnTrStop->setObjectName("actionBtn");
    m_btnTrStop->setEnabled(false);
    ctrlLay->addWidget(m_btnTrStop);

    /* btnFromEditor è nel pannello sorgente (buildTrSplitter), ma per
       semplicità lo creiamo qui e lo passiamo allo splitter tramite closure.
       Soluzione: deleghiamo la connessione a setupTrConnections. */
    outBtnFromEditor = nullptr;  /* sarà impostato da buildTrSplitter */

    return ctrlRow;
}

/* ── Splitter sorgente | output ── */
QWidget* ProgrammazionePage::buildTrSplitter(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    const int appPt = QApplication::font().pointSize();
    monoFont.setPointSize(appPt > 0 ? appPt : 11);

    auto* splitter = new QSplitter(Qt::Horizontal, parent);
    splitter->setHandleWidth(6);

    /* Pannello sorgente */
    auto* srcGroup = new QGroupBox(tr("Codice sorgente"), splitter);
    auto* srcLay   = new QVBoxLayout(srcGroup);
    srcLay->setContentsMargins(6, 6, 6, 6);

    m_trInput = new QPlainTextEdit(srcGroup);
    m_trInput->setFont(monoFont);
    m_trInput->setObjectName("trCodeEditor");
    m_trInput->setPlaceholderText(
        "Incolla qui il codice da tradurre...\n\n"
        "Esempio C:\n"
        "  int somma(int a, int b) {\n"
        "      return a + b;\n"
        "  }");
    srcLay->addWidget(m_trInput, 1);

    auto* btnFromEditor = new QPushButton(
        tr("\xe2\xac\x86  Importa dall'editor"), srcGroup);
    btnFromEditor->setObjectName("actionBtn");
    btnFromEditor->setToolTip(
        tr("Copia il codice dall'editor principale in questo pannello"));
    srcLay->addWidget(btnFromEditor);
    connect(btnFromEditor, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnFromEditorClicked);

    splitter->addWidget(srcGroup);

    /* Pannello output tradotto */
    auto* dstGroup = new QGroupBox(tr("Codice tradotto"), splitter);
    auto* dstLay   = new QVBoxLayout(dstGroup);
    dstLay->setContentsMargins(6, 6, 6, 6);

    m_trOutput = new QTextEdit(dstGroup);
    m_trOutput->setObjectName("chatLog");
    m_trOutput->setReadOnly(true);
    m_trOutput->setFont(monoFont);
    m_trOutput->setPlaceholderText(
        tr("Il codice tradotto apparir\xc3\xa0 qui durante lo streaming..."));
    dstLay->addWidget(m_trOutput, 1);

    auto* outBtnRow = new QWidget(dstGroup);
    auto* outBtnLay = new QHBoxLayout(outBtnRow);
    outBtnLay->setContentsMargins(0, 0, 0, 0);
    outBtnLay->setSpacing(6);

    m_btnTrInsert = new QPushButton(
        tr("\xe2\xac\x86  Inserisci nell'editor"), outBtnRow);
    m_btnTrInsert->setObjectName("actionBtn");
    m_btnTrInsert->setEnabled(false);
    m_btnTrInsert->setToolTip(
        "Estrae il primo blocco codice dalla risposta AI\n"
        "e lo inserisce nell'editor principale (sostituisce il contenuto attuale)");
    outBtnLay->addWidget(m_btnTrInsert);

    m_btnTrCopy = new QPushButton(tr("\xf0\x9f\x93\x8b  Copia"), outBtnRow);
    m_btnTrCopy->setObjectName("btnTrCopy");
    m_btnTrCopy->setEnabled(false);
    m_btnTrCopy->setToolTip(tr("Copia tutto il testo dell'output negli appunti"));
    outBtnLay->addWidget(m_btnTrCopy);
    outBtnLay->addStretch(1);

    dstLay->addWidget(outBtnRow);
    splitter->addWidget(dstGroup);
    splitter->setSizes({1, 1});
    return splitter;
}

/* ── Connessioni Translitter ── */
void ProgrammazionePage::setupTrConnections(QPushButton* btnSwap,
                                              QPushButton* /*btnFromEditor*/)
{
    /* btnFromEditor already wired inside buildTrSplitter */
    if (btnSwap) connect(btnSwap, &QPushButton::clicked,
                         this, &ProgrammazionePage::onBtnSwapLangsClicked);

    connect(m_btnTrRun,    &QPushButton::clicked, this, &ProgrammazionePage::runTranslitter);
    connect(m_btnTrStop,   &QPushButton::clicked, this, &ProgrammazionePage::onBtnTrStopClicked);
    connect(m_btnTrInsert, &QPushButton::clicked, this, &ProgrammazionePage::onBtnTrInsertClicked);
    connect(m_btnTrCopy,   &QPushButton::clicked, this, &ProgrammazionePage::onBtnTrCopyClicked);
    connect(m_trOutput,    &QTextEdit::textChanged, this, &ProgrammazionePage::onTrOutputTextChanged);
    connect(m_ai,          &AiClient::modelChanged, this, &ProgrammazionePage::onTrModelChanged);
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
        LogBus::post("\xe2\x9d\x8c Translitter: Nessun codice sorgente da tradurre.");
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

    disconnect(m_trTokenConn);
    disconnect(m_trFinishedConn);
    disconnect(m_trErrorConn);
    m_trTokenConn    = connect(m_ai, &AiClient::token,    this, &ProgrammazionePage::onTrToken);
    m_trFinishedConn = connect(m_ai, &AiClient::finished, this, &ProgrammazionePage::onTrFinished);
    m_trErrorConn    = connect(m_ai, &AiClient::error,    this, &ProgrammazionePage::onTrError);

    m_ai->chat(P::prependKnowledge(sys), user);
}

/* ======================================================================
   Sezione 11 — Translitter slots
   ====================================================================== */

void ProgrammazionePage::onBtnSwapLangsClicked()
{
    if (!m_trSrcLang || !m_trDstLang) return;
    const QString a = m_trSrcLang->currentText();
    const QString b = m_trDstLang->currentText();
    m_trSrcLang->setCurrentText(b);
    m_trDstLang->setCurrentText(a);
}

void ProgrammazionePage::onBtnFromEditorClicked()
{
    if (!m_editor || !m_trInput) return;
    const QString code = m_editor->toPlainText();
    if (!code.trimmed().isEmpty()) {
        m_trInput->setPlainText(code);
        /* Aggiorna il combo sorgente in base al linguaggio dell'editor */
        const QString edLang = m_lang ? m_lang->currentText() : "";
        if (!edLang.isEmpty() && m_trSrcLang && m_trSrcLang->findText(edLang) >= 0)
            m_trSrcLang->setCurrentText(edLang);
    }
}

void ProgrammazionePage::onBtnTrStopClicked()
{
    if (m_ai) m_ai->abort();
    if (m_btnTrRun)  m_btnTrRun->setEnabled(true);
    if (m_btnTrStop) m_btnTrStop->setEnabled(false);
}

void ProgrammazionePage::onBtnTrInsertClicked()
{
    if (!m_trOutput || !m_editor) return;
    const QString text = m_trOutput->toPlainText();
    /* Estrai primo blocco ``` ... ``` */
    static const QRegularExpression reBlock(
        "```(?:\\w+)?\\n([\\s\\S]*?)```",
        QRegularExpression::MultilineOption);
    const auto m = reBlock.match(text);
    const QString code = m.hasMatch() ? m.captured(1).trimmed() : text.trimmed();

    if (!m_editor->toPlainText().trimmed().isEmpty()) {
        if (QMessageBox::question(this,
                "Sovrascrivere il codice?",
                "L'editor contiene codice.\nVuoi sostituirlo con il codice traslitterato?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    m_editor->setPlainText(code);
    /* Aggiorna il combo linguaggio dell'editor */
    const QString dst = m_trDstLang ? m_trDstLang->currentText() : "";
    if (!dst.isEmpty() && m_lang) {
        int idx = m_lang->findText(dst);
        if (idx >= 0) m_lang->setCurrentIndex(idx);
    }
}

void ProgrammazionePage::onTrOutputTextChanged()
{
    if (!m_trOutput || !m_btnTrCopy) return;
    const bool hasContent = !m_trOutput->toPlainText().trimmed().isEmpty();
    m_btnTrCopy->setEnabled(hasContent);
}

void ProgrammazionePage::onTrModelChanged(const QString& newModel)
{
    if (!m_trModel) return;
    int idx = m_trModel->findData(newModel);
    if (idx < 0) idx = m_trModel->findText(newModel, Qt::MatchContains);
    if (idx >= 0) {
        m_trModel->blockSignals(true);
        m_trModel->setCurrentIndex(idx);
        m_trModel->blockSignals(false);
    } else {
        m_trModel->blockSignals(true);
        m_trModel->setItemText(0, newModel);
        m_trModel->setItemData(0, newModel);
        m_trModel->setCurrentIndex(0);
        m_trModel->blockSignals(false);
    }
}

void ProgrammazionePage::populateTrModels()
{
    if (m_ai && m_trModel) AiUtils::populateModelCombo(m_ai, m_trModel, this);
}


void ProgrammazionePage::onBtnTrCopyClicked()
{
    if (!m_trOutput || !m_btnTrCopy) return;
    QApplication::clipboard()->setText(m_trOutput->toPlainText());
    m_trCopyOrigTxt = m_btnTrCopy->text();
    m_btnTrCopy->setText(tr("\xe2\x9c\x85  Copiato!"));
    QTimer::singleShot(1500, this, &ProgrammazionePage::onTrCopyRestoreText);
}

void ProgrammazionePage::onTrCopyRestoreText()
{
    if (m_btnTrCopy) m_btnTrCopy->setText(m_trCopyOrigTxt);
}

void ProgrammazionePage::onTrModelActivated(int /*idx*/)
{
    if (m_trModel && m_trModel->count() <= 1)
        populateTrModels();
}

void ProgrammazionePage::onTrToken(const QString& tok)
{
    if (!m_trOutput) return;
    m_trOutput->moveCursor(QTextCursor::End);
    m_trOutput->insertPlainText(tok);
    m_trOutput->ensureCursorVisible();
}

void ProgrammazionePage::onTrFinished(const QString& /*full*/)
{
    disconnect(m_trTokenConn);
    disconnect(m_trFinishedConn);
    disconnect(m_trErrorConn);
    if (m_btnTrRun)  m_btnTrRun->setEnabled(true);
    if (m_btnTrStop) m_btnTrStop->setEnabled(false);
    const bool hasBlock = m_trOutput && m_trOutput->toPlainText().contains("```");
    if (m_btnTrInsert) m_btnTrInsert->setEnabled(hasBlock);
}

void ProgrammazionePage::onTrError(const QString& msg)
{
    disconnect(m_trTokenConn);
    disconnect(m_trFinishedConn);
    disconnect(m_trErrorConn);
    if (m_btnTrRun)  m_btnTrRun->setEnabled(true);
    if (m_btnTrStop) m_btnTrStop->setEnabled(false);
    if (m_trOutput) {
        m_trOutput->moveCursor(QTextCursor::End);
        m_trOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    }
}


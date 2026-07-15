/* ══════════════════════════════════════════════════════════════
   main_programming_reveng.cpp — ProgrammazionePage: Reverse Engineering
   ==========================================================================
   Sub-tab "🔍 Reverse Eng." — builder UI + slot. Split da
   main_programming.cpp/main_programming_slots.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_programming.h"
#include "main_programming_p.h"
#include "../prismalux_paths.h"
#include "../ai_utils.h"
#include "../log_bus.h"
#include "../dpi_utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QFrame>
#include <QFont>
#include <QTimer>
#include <QMessageBox>
#include <QTextCursor>
#include <QFileDialog>
#include <QDir>
#include <QFile>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   buildReverseEngineering — sub-tab "🔍 Reverse Eng."
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildReverseEngineering(QWidget* parent)
{
    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    buildRevHeader(lay, w);
    lay->addWidget(buildRevFileRow(w));  /* btnLoad wired inside */

    QPushButton* btnRefRev   = nullptr;
    QPushButton* btnClearRev = nullptr;

    lay->addWidget(buildRevOptionsRow(w, btnRefRev));
    lay->addWidget(buildRevPreviewGroup(w));
    lay->addWidget(buildRevOutputGroup(w, btnClearRev), 1);
    setupRevConnections(nullptr, btnRefRev, btnClearRev);
    return w;
}

/* ── Header: descrizione + separatore ── */
void ProgrammazionePage::buildRevHeader(QVBoxLayout* lay, QWidget* w)
{
    auto* desc = new QLabel(
        "\xf0\x9f\x94\x8d  <b>Reverse Engineering</b> \xe2\x80\x94 "
        "Carica un file compilato o offuscato: l'AI analizza i byte, "
        "estrae le stringhe leggibili e ricostruisce il codice sorgente approssimativo.", w);
    desc->setObjectName("hintLabel");
    desc->setWordWrap(true);
    lay->addWidget(desc);

    auto* sep = new QFrame(w);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    lay->addWidget(sep);
}

/* ── Riga caricamento file ── */
QWidget* ProgrammazionePage::buildRevFileRow(QWidget* parent)
{
    auto* fileRow = new QWidget(parent);
    auto* fileLay = new QHBoxLayout(fileRow);
    fileLay->setContentsMargins(0, 0, 0, 0);
    fileLay->setSpacing(8);

    auto* btnLoad = new QPushButton(tr("\xf0\x9f\x93\x82  Carica file..."), fileRow);
    btnLoad->setObjectName("actionBtn");
    fileLay->addWidget(btnLoad);

    m_revFilePath = new QLabel(tr("(nessun file caricato)"), fileRow);
    m_revFilePath->setObjectName("hintLabel");
    fileLay->addWidget(m_revFilePath, 1);

    connect(btnLoad, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnRevLoadClicked);

    return fileRow;
}

/* ── Riga opzioni + modello AI ── */
QWidget* ProgrammazionePage::buildRevOptionsRow(QWidget* parent,
                                                  QPushButton*& outBtnRefRev)
{
    /* Opzioni superiori */
    auto* optRow = new QWidget(parent);
    auto* optLay = new QHBoxLayout(optRow);
    optLay->setContentsMargins(0, 0, 0, 0);
    optLay->setSpacing(8);

    optLay->addWidget(new QLabel(tr("Linguaggio target:"), optRow));
    m_revTargetLang = new QComboBox(optRow);
    m_revTargetLang->setObjectName("settingCombo");
    m_revTargetLang->addItems({"Auto-rileva", "C", "C++", "Python",
                                "Assembly x86", "Java", "Rust"});
    m_revTargetLang->setFixedWidth(dpiScale(140));
    optLay->addWidget(m_revTargetLang);

    optLay->addSpacing(8);
    optLay->addWidget(new QLabel(tr("Dettaglio:"), optRow));
    m_revDetail = new QComboBox(optRow);
    m_revDetail->setObjectName("settingCombo");
    m_revDetail->addItem("Struttura (rapido)",  QString("fast"));
    m_revDetail->addItem("Completo (lento)",    QString("full"));
    m_revDetail->addItem("Con commenti estesi", QString("commented"));
    m_revDetail->setFixedWidth(dpiScale(180));
    optLay->addWidget(m_revDetail);

    optLay->addStretch(1);

    m_btnRevAnalyze = new QPushButton(
        tr("\xf0\x9f\x94\x8d  Analizza e Ricostruisci"), optRow);
    m_btnRevAnalyze->setObjectName("actionBtn");
    m_btnRevAnalyze->setProperty("highlight", "true");
    m_btnRevAnalyze->setEnabled(false);
    m_btnRevAnalyze->setToolTip(tr("Invia il file all'AI per la ricostruzione del sorgente"));
    optLay->addWidget(m_btnRevAnalyze);

    m_btnRevStop = new QPushButton(tr("\xe2\x96\xa0  Stop"), optRow);
    m_btnRevStop->setObjectName("actionBtn");
    m_btnRevStop->setProperty("danger", "true");
    m_btnRevStop->setEnabled(false);
    optLay->addWidget(m_btnRevStop);

    /* Selezione modello — costruita nello stesso widget composito */
    /* Wrap both rows in a container */
    auto* container = new QWidget(parent);
    auto* contLay   = new QVBoxLayout(container);
    contLay->setContentsMargins(0, 0, 0, 0);
    contLay->setSpacing(4);
    contLay->addWidget(optRow);

    auto* modelRow = new QWidget(container);
    auto* modelLay = new QHBoxLayout(modelRow);
    modelLay->setContentsMargins(0, 0, 0, 0);
    modelLay->setSpacing(8);

    auto* lblMod = new QLabel(tr("\xf0\x9f\xa4\x96  Modello AI:"), modelRow);
    lblMod->setObjectName("cardDesc");
    modelLay->addWidget(lblMod);

    m_revModel = new QComboBox(modelRow);
    m_revModel->setObjectName("settingCombo");
    m_revModel->addItem(
        m_ai ? (m_ai->model().isEmpty() ? "(nessun modello)" : m_ai->model())
             : "(AI non disponibile)",
        m_ai ? m_ai->model() : QString());
    modelLay->addWidget(m_revModel, 1);

    outBtnRefRev = new QPushButton("\xf0\x9f\x94\x84", modelRow);
    outBtnRefRev->setObjectName("actionBtn");
    outBtnRefRev->setFixedWidth(dpiScale(32));
    outBtnRefRev->setToolTip(tr("Aggiorna lista modelli disponibili"));
    modelLay->addWidget(outBtnRefRev);

    contLay->addWidget(modelRow);

    return container;
}

/* ── Preview hex dump ── */
QWidget* ProgrammazionePage::buildRevPreviewGroup(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(monoFontPt(10));

    auto* prevGroup = new QGroupBox(
        tr("\xf0\x9f\x93\x8b  Anteprima file (hex dump + stringhe estratte)"), parent);
    prevGroup->setObjectName("cardGroup");
    auto* prevLay = new QVBoxLayout(prevGroup);
    prevLay->setContentsMargins(4, 8, 4, 4);

    m_revPreview = new QPlainTextEdit(prevGroup);
    m_revPreview->setObjectName("revPreviewEditor");
    m_revPreview->setFont(monoFont);
    m_revPreview->setReadOnly(true);
    m_revPreview->setMaximumHeight(160);
    m_revPreview->setPlaceholderText(
        tr("Carica un file per vedere il hex dump e le stringhe ASCII estratte..."));
    prevLay->addWidget(m_revPreview);

    return prevGroup;
}

/* ── Output AI + pulsanti ── */
QWidget* ProgrammazionePage::buildRevOutputGroup(QWidget* parent,
                                                   QPushButton*& outBtnClearRev)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(monoFontPt(10));

    auto* outGroup = new QGroupBox(
        tr("\xf0\x9f\xa4\x96  Codice sorgente ricostruito (streaming AI)"), parent);
    outGroup->setObjectName("cardGroup");
    auto* outLay = new QVBoxLayout(outGroup);
    outLay->setContentsMargins(4, 8, 4, 4);
    outLay->setSpacing(6);

    m_revOutput = new QTextEdit(outGroup);
    m_revOutput->setObjectName("chatLog");
    m_revOutput->setReadOnly(true);
    m_revOutput->setFont(monoFont);
    m_revOutput->setPlaceholderText(
        "Il codice ricostruito dall'AI appari\xc3\xa0 qui in streaming...\n\n"
        "Carica un file e premi \xf0\x9f\x94\x8d Analizza e Ricostruisci.");
    outLay->addWidget(m_revOutput, 1);

    auto* revBtnRow = new QWidget(outGroup);
    auto* revBtnLay = new QHBoxLayout(revBtnRow);
    revBtnLay->setContentsMargins(0, 2, 0, 0);
    revBtnLay->setSpacing(8);

    m_btnRevInsert = new QPushButton(
        tr("\xe2\x86\x91  Apri in editor Programmazione"), revBtnRow);
    m_btnRevInsert->setObjectName("actionBtn");
    m_btnRevInsert->setEnabled(false);
    m_btnRevInsert->setToolTip(
        tr("Estrae il primo blocco codice e lo apre nel tab \xf0\x9f\x92\xbb Programmazione"));
    revBtnLay->addWidget(m_btnRevInsert);

    outBtnClearRev = new QPushButton(
        tr("\xf0\x9f\x97\x91  Pulisci output"), revBtnRow);
    outBtnClearRev->setObjectName("actionBtn");
    revBtnLay->addWidget(outBtnClearRev);
    revBtnLay->addStretch(1);

    outLay->addWidget(revBtnRow);
    return outGroup;
}

/* ── Connessioni Reverse Engineering ── */
void ProgrammazionePage::setupRevConnections(QPushButton* /*btnLoad*/,
                                              QPushButton* btnRefRev,
                                              QPushButton* btnClearRev)
{
    /* btnLoad already wired inside buildRevFileRow */
    if (btnRefRev) {
        connect(btnRefRev, &QPushButton::clicked,
                this, &ProgrammazionePage::populateRevModels);
        QTimer::singleShot(0, this, &ProgrammazionePage::populateRevModels);
    }

    connect(m_btnRevAnalyze, &QPushButton::clicked,
            this, &ProgrammazionePage::runReverseEngineering);
    connect(m_btnRevStop,    &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnRevStopClicked);

    if (btnClearRev)
        connect(btnClearRev, &QPushButton::clicked,
                this, &ProgrammazionePage::onBtnClearRevClicked);

    connect(m_btnRevInsert, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnRevInsertClicked);
}

/* ══════════════════════════════════════════════════════════════
   runReverseEngineering — compone il prompt RE e lancia lo streaming.

   Invia all'LLM: tipo file, hex dump, stringhe estratte, linguaggio
   target e livello di dettaglio. L'AI ricostruisce il sorgente
   approssimativo in un blocco ```lang ... ```.
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::runReverseEngineering()
{
    if (!m_ai || m_revFileData.isEmpty()) return;
    if (m_ai->busy()) {
        m_revOutput->setPlainText(
            "\xe2\x9a\xa0\xef\xb8\x8f  AI occupata. Attendi o premi Stop.");
        return;
    }

    /* Applica modello scelto */
    if (m_revModel) {
        const QString sel = m_revModel->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    const QString lang     = m_revTargetLang ? m_revTargetLang->currentText() : "C";
    const QString detail   = m_revDetail     ? m_revDetail->currentData().toString() : "full";
    const QString fileInfo = m_revFilePath   ? m_revFilePath->text() : "";
    const QString preview  = m_revPreview    ? m_revPreview->toPlainText() : "";

    /* Estensione fence per il blocco codice */
    const QString fence = (lang == "Auto-rileva")  ? "c"
                        : (lang == "Assembly x86") ? "asm"
                        : lang.toLower().section(' ', 0, 0);

    const QString detailDesc =
        (detail == "fast")
        ? "Ricostruisci solo la struttura: dichiarazioni di funzioni/classi, tipi, "
          "costanti e include, senza implementazione completa."
        : (detail == "full")
        ? "Ricostruisci il codice il pi\xc3\xb9 completo possibile, "
          "implementando ogni funzione con la logica dedotta."
        : "Ricostruisci il codice completo con commenti estesi che spiegano "
          "la logica di ogni sezione e le deduzioni RE.";

    const QString langTarget = (lang == "Auto-rileva")
        ? "il linguaggio pi\xc3\xb9 probabile (deducilo dal tipo di file e dai byte)"
        : lang;

    const QString sys = QString(
        "Sei un esperto di reverse engineering e analisi binaria. "
        "Ti viene fornita l'analisi di un file: tipo rilevato, hex dump dei primi byte "
        "e le stringhe ASCII estratte. "
        "Il tuo compito \xc3\xa8 ricostruire il codice sorgente originale approssimativo in %1.\n\n"
        "Metodologia:\n"
        "  1. Identifica architettura, OS target e librerie dai magic byte e dalle stringhe\n"
        "  2. Deduci le funzionalit\xc3\xa0 del programma dalle stringhe leggibili\n"
        "  3. %2\n"
        "  4. Documenta le ipotesi con commenti NOTE RE:\n\n"
        "Struttura la risposta:\n"
        "  [ANALISI] tipo file, architettura, librerie rilevate, funzionalit\xc3\xa0 dedotte\n"
        "  [SORGENTE RICOSTRUITO] codice in un blocco ```%3 ... ```\n"
        "  [NOTE RE] assunzioni, confidenza, punti incerti\n\n"
        "Il codice deve essere plausibile e sintatticamente corretto. "
        "Rispondi SEMPRE in italiano."
    ).arg(langTarget, detailDesc, fence);

    const QString user = QString(
        "File da analizzare:\n%1\n\n"
        "Dati estratti:\n```\n%2\n```\n\n"
        "Ricostruisci il codice sorgente in %3."
    ).arg(fileInfo, preview, langTarget);

    /* ── Avvio streaming ── */
    m_revOutput->clear();
    {
        const QString modelName = m_ai->model().isEmpty() ? "AI" : m_ai->model();
        m_revOutput->setPlainText(
            QString("\xf0\x9f\xa4\x96  Modello: %1\n"
                    "\xf0\x9f\x94\x8d  Linguaggio: %2\n"
                    "\xf0\x9f\x93\x8b  Dettaglio: %3\n%4\n\n")
            .arg(modelName, lang,
                 m_revDetail ? m_revDetail->currentText() : detail,
                 QString(qMax(modelName.length(), 24), '-')));
    }

    m_btnRevAnalyze->setEnabled(false);
    m_btnRevStop->setEnabled(true);
    m_btnRevInsert->setEnabled(false);

    disconnect(m_revTokenConn);
    disconnect(m_revFinishedConn);
    disconnect(m_revErrorConn);
    m_revTokenConn    = connect(m_ai, &AiClient::token,    this, &ProgrammazionePage::onRevToken);
    m_revFinishedConn = connect(m_ai, &AiClient::finished, this, &ProgrammazionePage::onRevFinished);
    m_revErrorConn    = connect(m_ai, &AiClient::error,    this, &ProgrammazionePage::onRevError);
    m_ai->chat(P::prependKnowledge(sys), user);
}

/* ======================================================================
   Sezione 6 — Reverse Engineering slots
   ====================================================================== */

void ProgrammazionePage::populateRevModels()
{
    if (m_ai && m_revModel) AiUtils::populateModelCombo(m_ai, m_revModel, this);
}

void ProgrammazionePage::onBtnRevLoadClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Carica file da analizzare",
        QDir::homePath(),
        "Tutti i file (*)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (m_revFilePath)
            m_revFilePath->setText(
                QString("\xe2\x9d\x8c  Impossibile aprire: %1").arg(path));
        LogBus::post("\xe2\x9d\x8c Programmazione: Impossibile aprire: " + path);
        return;
    }
    m_revFileData = f.read(1024 * 1024); /* max 1 MB */
    f.close();

    const QFileInfo fi(path);
    if (m_revFilePath)
        m_revFilePath->setText(
            QString("\xf0\x9f\x93\x84  %1  (%2 KB)")
            .arg(fi.fileName())
            .arg(m_revFileData.size() / 1024.0, 0, 'f', 1));

    /* Genera hex dump + stringhe leggibili */
    if (m_revPreview) {
        QString preview;

        /* Hex dump: primi 256 byte */
        const int hexBytes = qMin(256, m_revFileData.size());
        for (int i = 0; i < hexBytes; i += 16) {
            preview += QString("%1:  ").arg(i, 4, 16, QChar('0'));
            for (int j = i; j < qMin(i + 16, hexBytes); ++j)
                preview += QString("%1 ").arg(
                    static_cast<unsigned char>(m_revFileData[j]), 2, 16, QChar('0'));
            for (int j = hexBytes; j < i + 16; ++j)
                preview += "   ";
            preview += " |";
            for (int j = i; j < qMin(i + 16, hexBytes); ++j) {
                const char c = m_revFileData[j];
                preview += (c >= 0x20 && c < 0x7f) ? c : '.';
            }
            preview += "|\n";
        }

        /* Estrai stringhe ASCII */
        preview += "\n--- Stringhe ASCII estratte ---\n";
        QString cur;
        for (int i = 0; i < m_revFileData.size(); ++i) {
            const char c = m_revFileData[i];
            if (c >= 0x20 && c < 0x7f) {
                cur += c;
            } else {
                if (cur.length() >= 4)
                    preview += cur + "\n";
                cur.clear();
            }
        }
        if (cur.length() >= 4) preview += cur + "\n";

        m_revPreview->setPlainText(preview);
    }

    if (m_btnRevAnalyze) m_btnRevAnalyze->setEnabled(true);
}

void ProgrammazionePage::onBtnRevStopClicked()
{
    if (m_ai) m_ai->abort();
    if (m_btnRevAnalyze) m_btnRevAnalyze->setEnabled(true);
    if (m_btnRevStop)    m_btnRevStop->setEnabled(false);
}

void ProgrammazionePage::onBtnClearRevClicked()
{
    if (m_revOutput) m_revOutput->clear();
}

void ProgrammazionePage::onBtnRevInsertClicked()
{
    if (!m_revOutput || !m_editor) return;
    const QString text = m_revOutput->toPlainText();
    static const QRegularExpression re("```(?:\\w+)?\\n([\\s\\S]*?)```");
    const auto m = re.match(text);
    const QString code = m.hasMatch() ? m.captured(1).trimmed() : text.trimmed();
    if (code.isEmpty()) return;
    if (!m_editor->toPlainText().trimmed().isEmpty()) {
        if (QMessageBox::question(this,
                "Sovrascrivere il codice?",
                "L'editor contiene codice.\nVuoi sostituirlo con il sorgente ricostruito?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    m_editor->setPlainText(code);
    if (m_innerTabs) m_innerTabs->setCurrentIndex(0);
}

void ProgrammazionePage::onRevToken(const QString& tok)
{
    if (!m_revOutput) return;
    m_revOutput->moveCursor(QTextCursor::End);
    m_revOutput->insertPlainText(tok);
    m_revOutput->ensureCursorVisible();
}

void ProgrammazionePage::onRevFinished(const QString& full)
{
    disconnect(m_revTokenConn);
    disconnect(m_revFinishedConn);
    disconnect(m_revErrorConn);
    if (m_btnRevAnalyze) m_btnRevAnalyze->setEnabled(true);
    if (m_btnRevStop)    m_btnRevStop->setEnabled(false);
    const bool hasCode = full.contains("```");
    if (m_btnRevInsert) m_btnRevInsert->setEnabled(hasCode);
}

void ProgrammazionePage::onRevError(const QString& msg)
{
    disconnect(m_revTokenConn);
    disconnect(m_revFinishedConn);
    disconnect(m_revErrorConn);
    if (m_btnRevAnalyze) m_btnRevAnalyze->setEnabled(true);
    if (m_btnRevStop)    m_btnRevStop->setEnabled(false);
    if (m_revOutput) {
        m_revOutput->moveCursor(QTextCursor::End);
        m_revOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    }
    LogBus::post("\xe2\x9d\x8c Programmazione: Reverse Engineering errore AI: " + msg);
}


/* ══════════════════════════════════════════════════════════════
   main_tools_actions.cpp — StrumentiPage: slot categorie/cron/run
   =========================================================================
   Slot bottoni azione, categorie, cron toggle, code-model combo,
   avvio/stop esecuzione. Split da main_tools.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_tools.h"
#include "main_tools_p.h"
#include "../prismalux_paths.h"
#include "../widgets/proc_helper.h"

#include <QFileDialog>
#include <QButtonGroup>
#include <QVBoxLayout>

namespace P = PrismaluxPaths;

void StrumentiPage::onActBtnClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const int cat = btn->property("strCat").toInt();
    const int act = btn->property("strAct").toInt();
    m_currentCat = cat;
    m_navList->setCurrentRow(cat);
    m_cmbSub->setCurrentIndex(act);
    m_inputArea->setPlaceholderText(QString::fromUtf8(kPlaceholders[cat]));
    if (m_lblSel)
        m_lblSel->setText(
            "\xe2\x9c\x85  <b>" +
            QString::fromUtf8(kSubActions[cat][act]) +
            "</b>");
}

/* ══════════════════════════════════════════════════════════════
   Slot: cambio categoria (catGroup)
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onCatGroupIdClicked(int cat)
{
    static const char* kModelHints[6] = {
        "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>llama3</b>, <b>qwen3</b>"
        " \xe2\x80\x94 buona comprensione e spiegazione",
        "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>llama3</b>, <b>gemma3</b>"
        " \xe2\x80\x94 creativit\xc3\xa0 e fluidit\xc3\xa0 narrativa",
        "\xe2\x9c\xa8 Consigliati: <b>qwen3:30b</b>, <b>deepseek-r1</b>, <b>llama3</b>"
        " \xe2\x80\x94 ragionamento avanzato e fact-checking",
        "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>llama3</b>, <b>qwen3</b>"
        " \xe2\x80\x94 analisi letteraria e critica",
        "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>qwen3</b>, <b>phi4</b>"
        " \xe2\x80\x94 risposte strutturate e concise",
        "\xe2\x9c\xa8 Consigliati: <b>llama3</b>, <b>qwen3</b>, <b>mistral</b>"
        " \xe2\x80\x94 estrazione e sintesi testi lunghi",
    };

    if (m_cronBtn)  m_cronBtn->setChecked(false);
    if (m_cronPanel) m_cronPanel->setVisible(false);
    if (m_actStack) { m_actStack->setVisible(true); m_actStack->setCurrentIndex(cat); }
    if (m_lblSel)   m_lblSel->setVisible(true);
    if (m_inputRow) m_inputRow->setVisible(true);
    m_output->setVisible(true);
    m_currentCat = cat;
    m_navList->setCurrentRow(cat);
    m_cmbSub->setCurrentIndex(0);
    m_inputArea->setPlaceholderText(QString::fromUtf8(kPlaceholders[cat]));
    if (m_lblSel)
        m_lblSel->setText(
            "\xe2\x9c\x85  <b>" +
            QString::fromUtf8(kSubActions[cat][0]) +
            "</b>");
    m_pdfRow->setVisible(cat == 5);
    m_ragRow->setVisible(true);
    m_codeModelRow->setVisible(false);
    m_btnRun->setEnabled(true);
    m_ai->fetchModels();
    if (cat >= 0 && cat < 6)
        m_codeModelInfo->setText(QString::fromUtf8(kModelHints[cat]));
}

/* ══════════════════════════════════════════════════════════════
   Slot: seleziona PDF
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onPdfBtnClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Seleziona PDF", "",
        "PDF (*.pdf);;Tutti i file (*)");
    if (path.isEmpty()) return;
    m_pdfPath = path;
    m_pdfPathLbl->setText(QFileInfo(path).fileName());
}


/* ══════════════════════════════════════════════════════════════
   onCronBtnToggled
   Mostra/nasconde il pannello Cron; sincronizza catGroup e
   gestisce la lazy-init al primo clic.
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onCronBtnToggled(bool checked)
{
    if (m_actStack) m_actStack->setVisible(!checked);
    m_lblSel->setVisible(!checked);
    m_ragRow->setVisible(!checked);
    m_pdfRow->setVisible(false);
    if (m_inputRow) m_inputRow->setVisible(!checked);
    m_output->setVisible(!checked);
    m_cronPanel->setVisible(checked);

    /* Sincronizza il catGroup: quando Cron è attivo nessun bottone categoria
       deve apparire selezionato, e viceversa. */
    if (checked && m_catGroup) {
        m_catGroup->setExclusive(false);
        if (auto* cur = m_catGroup->checkedButton())
            cur->setChecked(false);
        m_catGroup->setExclusive(true);
    }

    /* Prima apertura: chiede a MainWindow di inizializzare il pannello Cron (lazy) */
    if (checked && !m_cronInstalled) {
        m_cronInstalled = true;
        emit cronPanelFirstOpen();
    }
}

/* ══════════════════════════════════════════════════════════════
   onCatTabChanged — cambio tab nel QTabWidget principale
   Tab 0-5: mostra area I/O condivisa, aggiorna stato categoria.
   Tab 6  : nasconde area I/O, lazy-init pannello Cron.
   Tab 7-9: nasconde area I/O, il tab è autosufficiente.
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onCatTabChanged(int idx)
{
    const bool isCategory = (idx >= 0 && idx < 6);

    m_sharedIoArea->setVisible(isCategory);

    /* Ridistribuisci spazio: per tab non-categoria, m_tabs si espande. */
    auto* rootLay = qobject_cast<QVBoxLayout*>(layout());
    if (rootLay) {
        rootLay->setStretch(0, isCategory ? 0 : 1);
        rootLay->setStretch(1, isCategory ? 1 : 0);
    }

    /* Lazy-init Cron alla prima selezione del tab */
    if (idx == 6 && !m_cronInstalled) {
        m_cronInstalled = true;
        emit cronPanelFirstOpen();
    }

    if (!isCategory) return;

    static const char* kModelHints[6] = {
        "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>llama3</b>, <b>qwen3</b>"
        " \xe2\x80\x94 buona comprensione e spiegazione",
        "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>llama3</b>, <b>gemma3</b>"
        " \xe2\x80\x94 creativit\xc3\xa0 e fluidit\xc3\xa0 narrativa",
        "\xe2\x9c\xa8 Consigliati: <b>qwen3:30b</b>, <b>deepseek-r1</b>, <b>llama3</b>"
        " \xe2\x80\x94 ragionamento avanzato e fact-checking",
        "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>llama3</b>, <b>qwen3</b>"
        " \xe2\x80\x94 analisi letteraria e critica",
        "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>qwen3</b>, <b>phi4</b>"
        " \xe2\x80\x94 risposte strutturate e concise",
        "\xe2\x9c\xa8 Consigliati: <b>llama3</b>, <b>qwen3</b>, <b>mistral</b>"
        " \xe2\x80\x94 estrazione e sintesi testi lunghi",
    };

    m_currentCat = idx;
    m_navList->setCurrentRow(idx);
    m_cmbSub->setCurrentIndex(0);
    if (m_inputArea)
        m_inputArea->setPlaceholderText(QString::fromUtf8(kPlaceholders[idx]));
    if (m_lblSel)
        m_lblSel->setText(
            "\xe2\x9c\x85  <b>" +
            QString::fromUtf8(kSubActions[idx][0]) +
            "</b>");

    /* Righe speciali: nascondi tutte, poi riabilita in base alla categoria */
    if (m_pdfRow)         m_pdfRow->setVisible(idx == 5);
    if (m_ragRow)         m_ragRow->setVisible(true);
    if (m_codeModelRow)   m_codeModelRow->setVisible(false);
    if (m_blenderRow)     m_blenderRow->setVisible(false);
    if (m_blenderHintRow) m_blenderHintRow->setVisible(false);
    if (m_officeRow)      m_officeRow->setVisible(false);
    if (m_officeHintRow)  m_officeHintRow->setVisible(false);
    if (m_freecadRow)     m_freecadRow->setVisible(false);
    if (m_freecadHintRow) m_freecadHintRow->setVisible(false);
    if (m_sketchRow)      m_sketchRow->setVisible(false);
    if (m_cloudCompareRow) m_cloudCompareRow->setVisible(false);

    if (m_btnRun) m_btnRun->setEnabled(true);
    m_ai->fetchModels();
    if (m_codeModelInfo)
        m_codeModelInfo->setText(QString::fromUtf8(kModelHints[idx]));
}

void StrumentiPage::onCodeModelRefreshClicked()
{
    if (!m_codeModelRefresh) return;
    m_codeModelRefresh->setEnabled(false);
    m_codeModelRefresh->setText(tr("\xe2\x8f\xb3"));
    m_ai->fetchModels();
}

void StrumentiPage::onCodeModelsReady(const QStringList& models)
{
    if (m_codeModelRefresh) {
        m_codeModelRefresh->setEnabled(true);
        m_codeModelRefresh->setText(tr("\xf0\x9f\x94\x84"));
    }
    if (!m_codeModelCombo) return;

    const QString current = m_codeModelCombo->count() > 0
        ? m_codeModelCombo->currentData().toString() : m_ai->model();
    m_codeModelCombo->blockSignals(true);
    m_codeModelCombo->clear();
    for (const QString& m : models) {
        const qint64 sz = m_ai->modelSizeBytes(m);
        m_codeModelCombo->addItem(P::modelIcon(sz, m) + m, m);
    }
    static const QStringList kGoodModels = {
        "coder", "code", "deepseek", "starcoder", "codellama",
        "llava", "vision", "phi4",
        "mistral", "llama3", "qwen3", "qwen2.5", "gemma3",
        "deepseek-r1", "command-r"
    };
    for (int i = 0; i < m_codeModelCombo->count(); ++i) {
        const QString name = m_codeModelCombo->itemText(i).toLower();
        for (const QString& kw : kGoodModels) {
            if (name.contains(kw)) {
                m_codeModelCombo->setItemData(i, QColor("#00a37f"), Qt::ForegroundRole);
                break;
            }
        }
    }
    int idx = m_codeModelCombo->findData(current);
    if (idx < 0) idx = m_codeModelCombo->findText(current);
    if (idx >= 0) m_codeModelCombo->setCurrentIndex(idx);
    m_codeModelCombo->blockSignals(false);
}

void StrumentiPage::onCodeModelError(const QString& msg)
{
    if (!m_codeModelRefresh || m_codeModelRefresh->isEnabled()) return;
    m_codeModelRefresh->setEnabled(true);
    m_codeModelRefresh->setText(tr("\xf0\x9f\x94\x84"));
    if (m_codeModelCombo && m_codeModelCombo->count() == 0)
        m_codeModelCombo->addItem("\xe2\x9a\xa0  " + msg, "");
}

void StrumentiPage::onBtnRunClicked()
{
    if (m_active) { m_ai->abort(); return; }
    const int navIdx = m_navList->currentRow();
    const int subIdx = m_cmbSub->currentIndex();
    if (navIdx < 0 || subIdx < 0) return;
    if (navIdx == 9) {
        m_output->setPlainText(
            "\xf0\x9f\x94\xb5 CloudCompare \xe2\x80\x94 funzionalit\xc3\xa0 non ancora disponibile.\n\n"
            "Il bridge CloudComPy \xc3\xa8 in sviluppo. "
            "Verr\xc3\xa0 integrato non appena sar\xc3\xa0 disponibile una versione stabile.");
        return;
    }

    QString userMsg = m_inputArea->toPlainText().trimmed();

    if (navIdx == 5 && !m_pdfPath.isEmpty()) {
        const auto ppr = ProcHelper::run("pdftotext", {m_pdfPath, "-"}, 15000);
        if (!ppr.ok) {
            m_output->append(
                "\xe2\x9a\xa0  pdftotext non risponde. "
                "Assicurati che poppler-utils sia installato "
                "(sudo apt install poppler-utils).");
            return;
        }
        const QString pdfText = ppr.out.trimmed();
        if (pdfText.isEmpty()) {
            m_output->append(
                "\xe2\x9a\xa0  Impossibile estrarre testo dal PDF. "
                "Il file potrebbe essere scansionato (immagine).");
            return;
        }
        userMsg = userMsg.isEmpty()
            ? "DOCUMENTO:\n" + pdfText
            : "DOCUMENTO:\n" + pdfText + "\n\nRICHIESTA:\n" + userMsg;
    }

    if (userMsg.isEmpty()) {
        m_output->append("\xe2\x9a\xa0  Inserisci del testo oppure carica un PDF.");
        return;
    }
    const char* sys = kSysPrompts[navIdx][subIdx];
    if (!sys) return;
    runTool(QString::fromUtf8(sys), userMsg);
}

void StrumentiPage::onAiAborted()
{
    m_active = false;
    if (m_waitLbl) m_waitLbl->setVisible(false);
    if (m_waitBar) m_waitBar->setVisible(false);
    _setRunBusy(false);
    if (m_output) m_output->append("\n\xe2\x8f\xb9  Interrotto.");
}



void StrumentiPage::onQuizAiModelsReady(const QStringList&)
{
    if (m_quizAi)
        m_quizAi->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_ai->model());
}

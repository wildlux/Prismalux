/* ══════════════════════════════════════════════════════════════
   main_tools_run.cpp — StrumentiPage: pipeline esecuzione AI generica
   =========================================================================
   runTool + token/finished/error streaming + stub morti (buildSubPage/
   buildStudio/buildScritturaCreativa/buildRicerca/buildLibri/
   buildProduttivita/sysPromptForAction — mai implementati, spostati
   intatti). Split da main_tools.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_tools.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../widgets/ai_error_widget.h"

#include <QTextCursor>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   runTool
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::runTool(const QString& sys, const QString& userMsg) {
    if (m_ai->busy()) {
        m_output->append(
            "\xe2\x9a\xa0  Un'altra operazione e' in corso. Attendi.");
        return;
    }
    m_errPanel->hide();
    /* Applica il modello scelto nella riga modello (per tutte le categorie) */
    if (m_codeModelCombo && m_codeModelCombo->count() > 0) {
        const QString sel = m_codeModelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(),
                             m_ai->port(), sel);
        m_codeModelInfo->setText(
            QString("\xf0\x9f\xa4\x96  Usando: <b>%1</b>").arg(
                m_codeModelCombo->currentText()));
    }

    /* Iniezione RAG: se attiva, prepend contesto al system prompt */
    QString finalSys = sys;
    if (m_ragCheck && m_ragCheck->isChecked() && !m_ragChunks.isEmpty()) {
        const QString ctx = ragBuildContext(userMsg);
        if (!ctx.isEmpty())
            finalSys = "Usa il seguente contesto estratto dai documenti dell'utente "
                       "per rispondere in modo preciso. "
                       "Cita il contesto quando rilevante.\n\n"
                       "CONTESTO DOCUMENTI:\n" + ctx +
                       "\n\n---\n\n" + sys;
    }

    m_output->clear();
    m_waitLbl->setText(tr("\xf0\x9f\x94\x84  Elaborazione in corso..."));
    m_waitLbl->setVisible(true); m_waitBar->setVisible(true);
    m_active = true;
    _setRunBusy(true);
    m_ai->chat(P::prependKnowledge(finalSys), userMsg);
}

/* ══════════════════════════════════════════════════════════════
   Slot AI
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onToken(const QString& t) {
    if (!m_active) return;
    m_waitLbl->setVisible(false); m_waitBar->setVisible(false);
    QTextCursor c(m_output->document());
    c.movePosition(QTextCursor::End);
    c.insertText(t);
    m_output->ensureCursorVisible();
}

void StrumentiPage::onFinished(const QString& full) {
    if (!m_active) return;
    m_active = false;
    m_waitLbl->setVisible(false); m_waitBar->setVisible(false);
    m_errPanel->hide();
    _setRunBusy(false);
    m_output->append("\n" + QString(40, QChar(0x2500)));

    /* ── Blender / Office / FreeCAD: estrai codice Python dal blocco ```...``` ── */
    if ((m_currentCat == 6 || m_currentCat == 7 || m_currentCat == 8) && !full.isEmpty()) {
        // Lambda locale: estrae il primo blocco ```python...``` o ```...```
        auto extractCode = [](const QString& text) -> QString {
            int start = text.indexOf("```python");
            if (start != -1) {
                start = text.indexOf('\n', start) + 1;
                int end = text.indexOf("```", start);
                if (end != -1) return text.mid(start, end - start).trimmed();
            }
            start = text.indexOf("```");
            if (start != -1) {
                start += 3;
                if (start < text.size() && text[start] == '\n') start++;
                int end = text.indexOf("```", start);
                if (end != -1) return text.mid(start, end - start).trimmed();
            }
            return text.trimmed();
        };

        const QString code = extractCode(full);
        if (m_currentCat == 6) {
            m_blenderCode = code;
            if (!m_blenderCode.isEmpty()) {
                m_blenderExecBtn->setEnabled(true);
                m_blenderStatusLbl->setText(
                    "\xf0\x9f\x90\x8d  Codice pronto \xe2\x80\x94 premi Esegui in Blender");
            }
        } else if (m_currentCat == 7) { // Office
            m_officeCode = code;
            if (!m_officeCode.isEmpty()) {
                m_officeExecBtn->setEnabled(true);
                m_officeStatusLbl->setText(
                    "\xf0\x9f\x93\x84  Codice pronto \xe2\x80\x94 premi Esegui in Office");
            }
        } else { // cat 8 — FreeCAD
            m_freecadCode = code;
            if (!m_freecadCode.isEmpty()) {
                m_freecadExecBtn->setEnabled(true);
                m_freecadStatusLbl->setText(
                    "\xf0\x9f\x94\xa9  Codice pronto \xe2\x80\x94 premi Esegui in FreeCAD");
            }
        }
    }
}

void StrumentiPage::onError(const QString& msg) {
    if (!m_active) return;
    m_active = false;
    m_waitLbl->setVisible(false); m_waitBar->setVisible(false);
    _setRunBusy(false);
    m_errPanel->showError(msg, [this]{ onBtnRunClicked(); });
    m_output->append(
        QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    LogBus::post("\xe2\x9d\x8c Strumenti: Errore AI: " + msg);
}

void StrumentiPage::_setRunBusy(bool busy)
{
    if (busy) {
        m_btnRun->setText(tr("\xe2\x8f\xb9  Stop"));
        m_btnRun->setProperty("danger", true);
    } else {
        m_btnRun->setText(tr("\xe2\x96\xb6  Esegui"));
        m_btnRun->setProperty("danger", false);
    }
    m_btnRun->setEnabled(true);
    P::repolish(m_btnRun);
}

/* Stub richiesti dall'header */
QWidget* StrumentiPage::buildSubPage(const QStringList&, const QString&) { return nullptr; }
QWidget* StrumentiPage::buildStudio()           { return nullptr; }
QWidget* StrumentiPage::buildScritturaCreativa(){ return nullptr; }
QWidget* StrumentiPage::buildRicerca()          { return nullptr; }
QWidget* StrumentiPage::buildLibri()            { return nullptr; }
QWidget* StrumentiPage::buildProduttivita()     { return nullptr; }
QString  StrumentiPage::sysPromptForAction(int, int) { return {}; }

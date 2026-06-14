/* ======================================================================
   programmazione_page_slots.cpp

   Implementazioni dei slot estratti dalle lambda di connect() e
   QTimer::singleShot() in ProgrammazionePage.

   Diviso in sezioni:
     1. Coding tab (costruttore)
     2. runCode / process slots
     3. AI panel slots (Coding)
     4. Fix slots
     5. Agentica slots
     6. Reverse Engineering slots
     7. Network Analyzer slots
     8. Rete LAN slots
     9. Git MCP slots
    10. Python REPL slots
    11. Translitter slots
    12. Driver & Kernel slots
   ====================================================================== */
#include "main_programming.h"
#include "../prismalux_paths.h"
#include "../ai_utils.h"
#include "../widgets/ai_error_widget.h"
#include "../widgets/proc_helper.h"
#include "../log_bus.h"

#include <QRandomGenerator>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QGroupBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QImage>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTextCursor>
#include <QCheckBox>
#include <QSettings>
#include <QTextEdit>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonObject>
#include <QJsonDocument>

namespace P = PrismaluxPaths;

/* ======================================================================
   Sezione 1 — Coding tab (costruttore)
   ====================================================================== */

void ProgrammazionePage::onAutoFixToggled(bool on)
{
    if (!on) {
        /* Spegnimento: reset loop */
        m_loopActive = false;
        m_loopCount  = 0;
        return;
    }

    /* Avviso one-shot — persistito in QSettings ("Non mostrare più") */
    QSettings s("Prismalux", "GUI");
    if (s.value(P::SK::kLoopFixWarning, false).toBool()) return;

    QMessageBox dlg(this);
    dlg.setWindowTitle(tr("Loop Fix — Esecuzione automatica di codice AI"));
    dlg.setIcon(QMessageBox::Warning);
    dlg.setText(
        "<b>Loop Fix eseguir\xc3\xa0 automaticamente il codice</b><br>"
        "generato dall'AI con i tuoi privilegi utente.<br><br>"
        "Assicurati di usare questa funzione <b>solo con AI di cui ti fidi</b> "
        "e con codice che non accede a dati sensibili o al filesystem.<br><br>"
        "Il loop si ferma automaticamente dopo il numero di tentativi indicato dallo slider "
        "o se rileva un errore intenzionale (SyntaxError custom).");
    dlg.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    dlg.button(QMessageBox::Ok)->setText(tr("Ho capito, abilita"));
    dlg.button(QMessageBox::Cancel)->setText(tr("Annulla"));

    auto* chk = new QCheckBox("Non mostrare pi\xc3\xb9 questo avviso", &dlg);
    dlg.setCheckBox(chk);

    if (dlg.exec() != QMessageBox::Ok) {
        /* Utente ha annullato: disattiva il toggle senza ri-emettere toggled */
        if (m_toggleAutoFix) {
            m_toggleAutoFix->blockSignals(true);
            m_toggleAutoFix->setChecked(false);
            m_toggleAutoFix->blockSignals(false);
        }
        return;
    }
    if (chk->isChecked())
        s.setValue(P::SK::kLoopFixWarning, true);
}

void ProgrammazionePage::onFixSliderChanged(int v)
{
    m_loopMax = v;
    if (m_fixSliderLbl)
        m_fixSliderLbl->setText(v == 10 ? "\xe2\x88\x9e" : QString::number(v));
}

void ProgrammazionePage::onLangChanged(int /*idx*/)
{
    /* Mappa ext -> CodeHighlighter::Language */
    static const struct { const char* ext; CodeHighlighter::Language lang; } kLangMap[] = {
        { "py",  CodeHighlighter::Python  },
        { "c",   CodeHighlighter::C       },
        { "cpp", CodeHighlighter::Cpp     },
        { "sh",  CodeHighlighter::Bash    },
        { "js",  CodeHighlighter::Python  }, /* fallback */
    };

    const QString ext = m_lang->currentData().toString();
    for (const auto& e : kLangMap) {
        if (ext == e.ext) {
            if (m_highlighter) m_highlighter->setLanguage(e.lang);
            break;
        }
    }
    if (m_editor) m_editor->setPlainText(currentTemplate());
}

void ProgrammazionePage::onBtnClearClicked()
{
    if (m_output) m_output->clear();
    if (m_status) m_status->setText(tr("Pronto."));
    if (m_chartGroup) m_chartGroup->hide();
    m_fullOutput.clear();
}

void ProgrammazionePage::onBtnAiToggled(bool on)
{
    if (!m_aiPanel) return;
    m_aiPanel->setVisible(on);
    if (on) {
        if (m_modelCombo->count() <= 1)
            populateAiModels();
        if (m_aiInput) m_aiInput->setFocus();
    }
}

void ProgrammazionePage::onBtnCloseAiClicked()
{
    if (m_btnAi) m_btnAi->setChecked(false);
    if (m_aiPanel) m_aiPanel->hide();
}

void ProgrammazionePage::sendToAi()
{
    if (!m_ai || !m_aiInput || !m_aiOutput) return;

    const QString request = m_aiInput->text().trimmed();
    if (request.isEmpty()) return;

    if (m_ai->busy()) {
        m_aiOutput->appendPlainText(
            "\xe2\x9a\xa0\xef\xb8\x8f  AI occupata. Attendi o premi Stop.");
        return;
    }

    /* Applica il modello scelto nella combo */
    if (m_modelCombo) {
        const QString sel = m_modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    /* Guardia embedding */
    {
        const QString mn = m_ai->model().toLower();
        const bool isEmbed = mn.contains("embed") || mn.contains("minilm") ||
                             mn.contains("rerank") || mn.contains("bge-") ||
                             mn.contains("e5-") || mn.contains("gte-") ||
                             mn.contains("-embed");
        if (isEmbed) {
            m_aiOutput->clear();
            m_aiOutput->insertPlainText(
                QString("\xe2\x9a\xa0\xef\xb8\x8f  \"%1\" \xc3\xa8 un modello di embedding:\n"
                        "non supporta la chat.\n\n"
                        "Seleziona un modello diverso dalla combo qui sopra\n"
                        "(es. qwen2.5-coder, llama3, deepseek-r1...).")
                .arg(m_ai->model()));
            return;
        }
    }

    const QString code = m_editor ? m_editor->toPlainText() : QString();
    const QString lang = m_lang ? m_lang->currentText() : "Python";

    const QString sys = QString(
        "Sei un assistente programmatore esperto specializzato in %1. "
        "Rispondi alla richiesta dell'utente riguardante il codice che ti mostra. "
        "Se generi codice, mettilo in un blocco ```%2 ... ```. "
        "Rispondi SEMPRE in italiano.")
        .arg(lang, m_lang ? m_lang->currentData().toString() : "py");

    const QString user = code.isEmpty()
        ? request
        : QString("Codice attuale:\n```%1\n%2\n```\n\nRichiesta: %3")
          .arg(m_lang ? m_lang->currentData().toString() : "py", code, request);

    m_aiOutput->clear();
    {
        const QString mn = m_ai->model().isEmpty() ? "AI" : m_ai->model();
        m_aiOutput->insertPlainText(
            QString("\xf0\x9f\xa4\x96  %1\n%2\n\n")
            .arg(mn, QString(qMax(mn.length(), 20), '-')));
    }

    m_aiMode = true;
    m_btnInsert->setEnabled(false);
    setRunning(true);

    disconnect(m_aiTokenConn);
    disconnect(m_aiFinishedConn);
    disconnect(m_aiErrorConn);
    m_aiTokenConn    = connect(m_ai, &AiClient::token,    this, &ProgrammazionePage::onAiToken);
    m_aiFinishedConn = connect(m_ai, &AiClient::finished, this, &ProgrammazionePage::onAiFinished);
    m_aiErrorConn    = connect(m_ai, &AiClient::error,    this, &ProgrammazionePage::onAiError);

    m_ai->chat(P::prependKnowledge(sys), user);
}

void ProgrammazionePage::onBtnInsertClicked()
{
    const QString code = extractCodeBlock();
    if (code.isEmpty()) return;
    if (m_editor && !m_editor->toPlainText().trimmed().isEmpty()) {
        if (QMessageBox::question(this,
                "Sovrascrivere il codice?",
                "L'editor contiene codice.\nVuoi sostituirlo con la risposta AI?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    if (m_editor) m_editor->setPlainText(code);
}

void ProgrammazionePage::onBtnRunClicked()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        setRunning(false);
        if (m_status) m_status->setText(tr("Esecuzione interrotta."));
        return;
    }
    runCode();
}

void ProgrammazionePage::onBtnFixClicked()
{
    /* Salva codice originale per diff */
    m_originalCode = m_editor ? m_editor->toPlainText() : QString();
    triggerFix(m_lastExitCode != 0 && !m_lastError.isEmpty());
}

void ProgrammazionePage::onModelChanged(const QString& newModel)
{
    /* Sincronizza tutte le combo modello */
    const auto syncCombo = [&newModel](QComboBox* cb) {
        if (!cb) return;
        int idx = cb->findData(newModel);
        if (idx < 0) idx = cb->findText(newModel, Qt::MatchContains);
        if (idx >= 0) {
            cb->blockSignals(true);
            cb->setCurrentIndex(idx);
            cb->blockSignals(false);
        } else {
            /* Aggiorna il primo elemento (modello attivo) */
            cb->blockSignals(true);
            cb->setItemText(0, newModel);
            cb->setItemData(0, newModel);
            cb->setCurrentIndex(0);
            cb->blockSignals(false);
        }
    };
    syncCombo(m_modelCombo);
    syncCombo(m_agentModel);
    syncCombo(m_revModel);
    syncCombo(m_gitAiModel);
}

void ProgrammazionePage::populateAiModels()
{
    if (m_ai && m_modelCombo) AiUtils::populateModelCombo(m_ai, m_modelCombo, this);
}

/* ======================================================================
   Sezione 2 — runCode process slots
   ====================================================================== */

void ProgrammazionePage::onProcReadyRead()
{
    if (!m_proc) return;
    const QString out = QString::fromLocal8Bit(m_proc->readAll());
    m_fullOutput += out;
    appendOutput(out);
}

void ProgrammazionePage::onProcFinished(int code, QProcess::ExitStatus /*status*/)
{
    m_lastExitCode = code;
    setRunning(false);

    if (code == 0) {
        if (m_status) m_status->setText(tr("\xe2\x9c\x85  Completato con successo."));
        tryShowChart();
        m_lastError.clear();
        m_loopActive = false;
        m_loopCount  = 0;
    } else {
        m_lastError = m_fullOutput.right(2000);
        if (m_status)
            m_status->setText(QString("\xe2\x9d\x8c  Errore (exit %1).").arg(code));
        LogBus::post(QString("\xe2\x9d\x8c Programmazione: Processo uscito con errore (exit %1).").arg(code));

        /* Loop Fix automatico */
        const QString src = m_editor ? m_editor->toPlainText() : QString();
        if (m_toggleAutoFix && m_toggleAutoFix->isChecked()
            && !isIntentionalError(m_fullOutput, src))
        {
            const int maxLoop = (m_loopMax >= 10) ? 9999 : m_loopMax;
            if (m_loopCount < maxLoop) {
                ++m_loopCount;
                if (m_status)
                    m_status->setText(
                        QString("\xf0\x9f\x94\x84  Loop Fix tentativo %1/%2...")
                        .arg(m_loopCount)
                        .arg(m_loopMax >= 10 ? QString("\xe2\x88\x9e") : QString::number(m_loopMax)));
                m_loopActive = true;
                m_originalCode = src;
                QTimer::singleShot(200, this, &ProgrammazionePage::onLoopFixTimer);
                return;
            } else {
                m_loopActive = false;
                if (m_status)
                    m_status->setText(QString("\xe2\x9d\x8c  Loop Fix esaurito (%1 tentativi).").arg(m_loopCount));
                LogBus::post(QString("\xe2\x9d\x8c Programmazione: Loop Fix esaurito (%1 tentativi).").arg(m_loopCount));
                m_loopCount = 0;
            }
        }
    }

    /* Pulizia file temp */
    if (!m_procFilePath.isEmpty())
        QFile::remove(m_procFilePath);
}

void ProgrammazionePage::onProcErrorOccurred(QProcess::ProcessError err)
{
    if (err == QProcess::FailedToStart) {
        if (m_status)
            m_status->setText(
                "\xe2\x9d\x8c  Impossibile avviare il processo. "
                "Controlla che il compilatore/interprete sia nel PATH.");
        LogBus::post("\xe2\x9d\x8c Programmazione: Impossibile avviare il processo.");
        setRunning(false);
    }
}

void ProgrammazionePage::onLoopFixTimer()
{
    triggerFix(true);
}

void ProgrammazionePage::onLoopRunTimer()
{
    runCode();
}

void ProgrammazionePage::onModelsReadyForFix(const QStringList&)
{
    disconnect(m_modelsReadyForFixConn);
    selectCoderModel();
    _doFix(m_pendingFixIncludeError, m_pendingFixCodice,
           m_pendingFixLang, m_pendingFixExt);
}

/* ======================================================================
   Sezione 3 — AI panel slots (Coding)
   ====================================================================== */

void ProgrammazionePage::onAiToken(const QString& tok)
{
    if (!m_aiOutput) return;
    m_aiOutput->moveCursor(QTextCursor::End);
    m_aiOutput->insertPlainText(tok);
    m_aiOutput->ensureCursorVisible();
}

void ProgrammazionePage::onAiFinished(const QString& /*full*/)
{
    disconnect(m_aiTokenConn);
    disconnect(m_aiFinishedConn);
    disconnect(m_aiErrorConn);
    m_aiMode = false;
    setRunning(false);
    if (m_btnInsert) m_btnInsert->setEnabled(!extractCodeBlock().isEmpty());
}

void ProgrammazionePage::onAiError(const QString& msg)
{
    disconnect(m_aiTokenConn);
    disconnect(m_aiFinishedConn);
    disconnect(m_aiErrorConn);
    m_aiMode = false;
    setRunning(false);
    if (m_aiOutput) {
        m_aiOutput->moveCursor(QTextCursor::End);
        m_aiOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore AI: %1").arg(msg));
    }
    LogBus::post("\xe2\x9d\x8c Programmazione: Errore AI: " + msg);
}

/* ======================================================================
   Sezione 4 — Fix slots
   ====================================================================== */

void ProgrammazionePage::onFixToken(const QString& tok)
{
    if (!m_aiOutput) return;
    m_aiOutput->moveCursor(QTextCursor::End);
    m_aiOutput->insertPlainText(tok);
    m_aiOutput->ensureCursorVisible();
}

void ProgrammazionePage::onFixFinished(const QString& full)
{
    disconnect(m_aiTokenConn);
    disconnect(m_aiFinishedConn);
    disconnect(m_aiErrorConn);

    /* Ripristina il modello originale */
    if (!m_fixOriginalModel.isEmpty() && m_fixOriginalModel != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_fixOriginalModel);

    m_aiMode = false;
    setRunning(false);
    if (m_fixErrPanel) m_fixErrPanel->hide();

    /* Estrai il codice e sostituisci l'editor */
    static const QRegularExpression re(
        "```(?:\\w+)?\\n([\\s\\S]*?)```");
    const auto m = re.match(full);
    if (m.hasMatch()) {
        const QString newCode = m.captured(1).trimmed();
        if (!newCode.isEmpty() && m_editor) {
            showDiff(m_originalCode, newCode);
            m_editor->setPlainText(newCode);
            m_editor->document()->setModified(true);
        }
    }

    if (m_btnInsert) m_btnInsert->setEnabled(!extractCodeBlock().isEmpty());

    /* Loop Fix: riesegui dopo il fix */
    if (m_loopActive) {
        if (m_status)
            m_status->setText(
                QString("\xf0\x9f\x94\x84  Loop Fix: rieseguo dopo il fix (%1)...")
                .arg(m_loopCount));
        QTimer::singleShot(300, this, &ProgrammazionePage::onLoopRunTimer);
    }
}

void ProgrammazionePage::onFixError(const QString& msg)
{
    disconnect(m_aiTokenConn);
    disconnect(m_aiFinishedConn);
    disconnect(m_aiErrorConn);

    /* Ripristina il modello originale */
    if (m_ai && !m_fixOriginalModel.isEmpty() && m_fixOriginalModel != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_fixOriginalModel);

    m_aiMode = false;
    m_loopActive = false;
    setRunning(false);
    if (m_fixErrPanel)
        m_fixErrPanel->showError(msg, [this]{ onBtnFixClicked(); });
    if (m_aiOutput) {
        m_aiOutput->moveCursor(QTextCursor::End);
        m_aiOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore Fix AI: %1").arg(msg));
    }
    LogBus::post("\xe2\x9d\x8c Programmazione: Errore Fix AI: " + msg);
}

/* ======================================================================
   Sezione 5 — Agentica slots
   ====================================================================== */

void ProgrammazionePage::populateAgentModels()
{
    if (m_ai && m_agentModel) AiUtils::populateModelCombo(m_ai, m_agentModel, this);
}

void ProgrammazionePage::onBtnAgentStopClicked()
{
    if (m_ai) m_ai->abort();
    if (m_btnAgentRun)  m_btnAgentRun->setEnabled(true);
    if (m_btnAgentStop) m_btnAgentStop->setEnabled(false);
}

void ProgrammazionePage::onBtnClearAgentClicked()
{
    if (m_agentOutput) m_agentOutput->clear();
}

void ProgrammazionePage::onBtnAgentInsertClicked()
{
    if (!m_agentOutput || !m_editor) return;
    const QString text = m_agentOutput->toPlainText();
    static const QRegularExpression re(
        "```(?:\\w+)?\\n([\\s\\S]*?)```");
    const auto m = re.match(text);
    const QString code = m.hasMatch() ? m.captured(1).trimmed() : text.trimmed();
    if (code.isEmpty()) return;
    if (!m_editor->toPlainText().trimmed().isEmpty()) {
        if (QMessageBox::question(this,
                "Sovrascrivere il codice?",
                "L'editor contiene codice.\nVuoi sostituirlo con il codice generato dall'agente?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    m_editor->setPlainText(code);
    if (m_innerTabs) m_innerTabs->setCurrentIndex(0);
}

void ProgrammazionePage::onAgentToken(const QString& tok)
{
    if (!m_agentOutput) return;
    m_agentOutput->moveCursor(QTextCursor::End);
    m_agentOutput->insertPlainText(tok);
    m_agentOutput->ensureCursorVisible();
}

void ProgrammazionePage::onAgentFinished(const QString& full)
{
    disconnect(m_agentTokenConn);
    disconnect(m_agentFinishedConn);
    disconnect(m_agentErrorConn);
    if (m_btnAgentRun)    m_btnAgentRun->setEnabled(true);
    if (m_btnAgentStop)   m_btnAgentStop->setEnabled(false);
    const bool hasCode = full.contains("```");
    if (m_btnAgentInsert) m_btnAgentInsert->setEnabled(hasCode);
}

void ProgrammazionePage::onAgentError(const QString& msg)
{
    disconnect(m_agentTokenConn);
    disconnect(m_agentFinishedConn);
    disconnect(m_agentErrorConn);
    if (m_btnAgentRun)  m_btnAgentRun->setEnabled(true);
    if (m_btnAgentStop) m_btnAgentStop->setEnabled(false);
    if (m_agentOutput) {
        m_agentOutput->moveCursor(QTextCursor::End);
        m_agentOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    }
    LogBus::post("\xe2\x9d\x8c Programmazione: Agentica errore AI: " + msg);
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

/* ======================================================================
   Sezione 7 — Network Analyzer slots
   ====================================================================== */

void ProgrammazionePage::onBtnNetClearClicked()
{
    if (m_netLog) m_netLog->clear();
    if (m_netAiOutput) m_netAiOutput->clear();
    if (m_btnNetAnalyze) m_btnNetAnalyze->setEnabled(false);
}

void ProgrammazionePage::onNetReadyRead()
{
    if (!m_netProc) return;
    const QString out = QString::fromLocal8Bit(
        m_netProc->readAllStandardOutput());
    if (m_netLog) {
        m_netLog->moveCursor(QTextCursor::End);
        m_netLog->insertPlainText(out);
        m_netLog->ensureCursorVisible();
    }
    if (m_btnNetAnalyze) m_btnNetAnalyze->setEnabled(true);
}

void ProgrammazionePage::onNetReadyReadStderr()
{
    if (!m_netProc) return;
    const QString err = QString::fromLocal8Bit(m_netProc->readAllStandardError());
    if (err.isEmpty()) return;

    /* Permesso negato — CAP_NET_RAW mancante */
    const bool permErr = err.contains("permission", Qt::CaseInsensitive)
                      || err.contains("CAP_NET_RAW", Qt::CaseInsensitive)
                      || err.contains("packet socket", Qt::CaseInsensitive);
    if (permErr) {
        m_netProc->kill();
        if (m_btnNetStart)   m_btnNetStart->setEnabled(true);
        if (m_btnNetStop)    m_btnNetStop->setEnabled(false);
        const QString fix = QString("sudo setcap cap_net_raw+eip %1").arg(m_netTool);
        if (m_netStatus)
            m_netStatus->setText(
                "\xf0\x9f\x94\x91  Permessi insufficienti. "
                "Clicca \xe2\x80\x9c" "Fix permessi\xe2\x80\x9d"
                " oppure esegui nel terminale: <code>" + fix + "</code>");
        return;
    }

    /* Filtra messaggi informativi di tshark (interfaccia pronta) */
    if (err.contains("Capturing on", Qt::CaseInsensitive)) return;

    if (m_netLog) {
        m_netLog->moveCursor(QTextCursor::End);
        m_netLog->insertPlainText(QString("[stderr] %1").arg(err));
        m_netLog->ensureCursorVisible();
    }
}

void ProgrammazionePage::netFixPermissions()
{
    if (m_netTool.isEmpty()) return;
    const QString pkexec = QStandardPaths::findExecutable("pkexec");
    if (pkexec.isEmpty()) {
        if (m_netStatus)
            m_netStatus->setText(
                "\xe2\x9d\x8c  pkexec non trovato. Esegui manualmente: "
                "sudo setcap cap_net_raw+eip " + m_netTool);
        LogBus::post("\xe2\x9d\x8c Programmazione: pkexec non trovato.");
        return;
    }
    if (m_netStatus)
        m_netStatus->setText(tr("\xe2\x8f\xb3  Richiesta permessi in corso..."));
    auto* proc = new QProcess(this);
    /* context=this, proc è figlio di this → cattura sicura */
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int code, QProcess::ExitStatus) {
                proc->deleteLater();
                if (m_netStatus)
                    m_netStatus->setText(code == 0
                        ? "\xe2\x9c\x85  Permessi applicati. Premi Start per avviare la cattura."
                        : "\xe2\x9d\x8c  Operazione annullata o fallita (code " +
                          QString::number(code) + ").");
                if (code != 0) LogBus::post(QString("\xe2\x9d\x8c Programmazione: setcap fallito (code %1).").arg(code));
            });
    proc->start(pkexec, {"setcap", "cap_net_raw+eip", m_netTool});
}

void ProgrammazionePage::onNetFinished(int code, QProcess::ExitStatus /*status*/)
{
    if (m_btnNetStart)  m_btnNetStart->setEnabled(true);
    if (m_btnNetStop)   m_btnNetStop->setEnabled(false);
    if (m_netStatus)
        m_netStatus->setText(
            code == 0
            ? "\xe2\x9c\x85  Cattura completata."
            : QString("\xe2\x9d\x8c  Terminato (code %1).").arg(code));
    if (code != 0) LogBus::post(QString("\xe2\x9d\x8c Programmazione: Network analyzer terminato (code %1).").arg(code));

    const bool hasData = m_netLog && !m_netLog->toPlainText().trimmed().isEmpty();
    if (m_btnNetAnalyze) m_btnNetAnalyze->setEnabled(hasData);
}

void ProgrammazionePage::onNetStopTimer()
{
    if (m_netProc && m_netProc->state() != QProcess::NotRunning)
        m_netProc->kill();
}

void ProgrammazionePage::onNetAiToken(const QString& tok)
{
    if (!m_netAiOutput) return;
    m_netAiOutput->moveCursor(QTextCursor::End);
    m_netAiOutput->insertPlainText(tok);
    m_netAiOutput->ensureCursorVisible();
}

void ProgrammazionePage::onNetAiFinished(const QString& /*full*/)
{
    disconnect(m_netAiTokenConn);
    disconnect(m_netAiFinishedConn);
    disconnect(m_netAiErrorConn);
    if (m_btnNetAnalyze) m_btnNetAnalyze->setEnabled(true);
}

void ProgrammazionePage::onNetAiError(const QString& msg)
{
    disconnect(m_netAiTokenConn);
    disconnect(m_netAiFinishedConn);
    disconnect(m_netAiErrorConn);
    if (m_btnNetAnalyze) m_btnNetAnalyze->setEnabled(true);
    if (m_netAiOutput) {
        m_netAiOutput->moveCursor(QTextCursor::End);
        m_netAiOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    }
    LogBus::post("\xe2\x9d\x8c Programmazione: Network AI errore: " + msg);
}

/* ======================================================================
   Sezione 8 — Rete LAN slots
   ====================================================================== */

void ProgrammazionePage::lanAddRow(const QString& ip, const QString& mac,
                                    const QString& host, const QString& stato)
{
    if (!m_lanTable) return;
    const int row = m_lanTable->rowCount();
    m_lanTable->insertRow(row);
    m_lanTable->setItem(row, 0, new QTableWidgetItem(ip));
    m_lanTable->setItem(row, 1, new QTableWidgetItem(mac));
    m_lanTable->setItem(row, 2, new QTableWidgetItem(host));
    m_lanTable->setItem(row, 3, new QTableWidgetItem(stato));
}

void ProgrammazionePage::lanResetBtns()
{
    if (m_lanScanArp)  m_lanScanArp->setEnabled(true);
    if (m_lanScanNmap) m_lanScanNmap->setEnabled(true);
    if (m_lanStopBtn)  m_lanStopBtn->setEnabled(false);
}

void ProgrammazionePage::onLanScanArpClicked()
{
    if (m_lanTable) m_lanTable->setRowCount(0);
    if (m_lanStatusLbl) m_lanStatusLbl->setText(
        "\xf0\x9f\x94\x8d  Lettura ARP cache...");
    if (m_lanScanArp)  m_lanScanArp->setEnabled(false);
    if (m_lanScanNmap) m_lanScanNmap->setEnabled(false);
    if (m_lanStopBtn)  m_lanStopBtn->setEnabled(true);

    if (m_lanProc && m_lanProc->state() != QProcess::NotRunning) {
        m_lanProc->kill();
        m_lanProc->waitForFinished(1000);
        m_lanProc->deleteLater();
        m_lanProc = nullptr;
    }
    m_lanBuf.clear();
    m_lanProc = new QProcess(this);

    connect(m_lanProc, &QProcess::errorOccurred,
            this, &ProgrammazionePage::onLanArpError);
    connect(m_lanProc, &QProcess::readyRead,
            this, &ProgrammazionePage::onLanArpReadyRead);
    connect(m_lanProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onLanArpFinished);

    /* Usa `ip neigh show` — non richiede root né net-tools, legge la cache ARP del kernel */
    m_lanProc->start("ip", {"neigh", "show"});
}

void ProgrammazionePage::onLanScanNmapClicked()
{
    if (m_lanTable) m_lanTable->setRowCount(0);
    if (m_lanStatusLbl) m_lanStatusLbl->setText(
        "\xf0\x9f\x8c\x90  Avvio scansione nmap...");
    if (m_lanScanArp)  m_lanScanArp->setEnabled(false);
    if (m_lanScanNmap) m_lanScanNmap->setEnabled(false);
    if (m_lanStopBtn)  m_lanStopBtn->setEnabled(true);

    /* Determina subnet dalla prima interfaccia attiva */
    QString subnet;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp)) continue;
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            const quint32 ipRaw  = e.ip().toIPv4Address();
            const quint32 mskRaw = e.netmask().toIPv4Address();
            int pfx = 0; quint32 tmp = mskRaw;
            while (tmp) { pfx += (tmp >> 31) & 1; tmp <<= 1; }
            const quint32 netRaw = ipRaw & mskRaw;
            subnet = QHostAddress(netRaw).toString() + "/" + QString::number(pfx);
            break;
        }
        if (!subnet.isEmpty()) break;
    }
    if (subnet.isEmpty()) {
        if (m_lanStatusLbl)
            m_lanStatusLbl->setText(
                "\xe2\x9d\x8c  Impossibile determinare la subnet.");
        LogBus::post("\xe2\x9d\x8c Programmazione: Impossibile determinare la subnet.");
        lanResetBtns();
        return;
    }
    m_lanNmapSubnet = subnet;

    if (m_lanProc && m_lanProc->state() != QProcess::NotRunning) {
        m_lanProc->kill();
        m_lanProc->waitForFinished(1000);
        m_lanProc->deleteLater();
        m_lanProc = nullptr;
    }
    m_lanBuf.clear();
    m_lanProc = new QProcess(this);

    connect(m_lanProc, &QProcess::errorOccurred,
            this, &ProgrammazionePage::onLanNmapError);
    connect(m_lanProc, &QProcess::readyRead,
            this, &ProgrammazionePage::onLanNmapReadyRead);
    connect(m_lanProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onLanNmapFinished);

    if (m_lanStatusLbl)
        m_lanStatusLbl->setText(
            QString("\xf0\x9f\x8c\x90  nmap -sn %1 ...").arg(subnet));

    m_lanProc->start("nmap", {"-sn", subnet});
}

void ProgrammazionePage::onLanStopBtnClicked()
{
    if (m_lanProc && m_lanProc->state() != QProcess::NotRunning) {
        m_lanProc->terminate();
        QTimer::singleShot(1500, this, &ProgrammazionePage::onLanStopTimer);
    }
    lanResetBtns();
    if (m_lanStatusLbl) m_lanStatusLbl->setText(
        "\xe2\x8f\xb9  Scansione interrotta.");
}

void ProgrammazionePage::onLanArpError(QProcess::ProcessError err)
{
    Q_UNUSED(err)
    if (m_lanStatusLbl)
        m_lanStatusLbl->setText(
            "\xe2\x9d\x8c  Impossibile eseguire 'ip neigh show'. "
            "Installa iproute2: sudo apt install iproute2");
    LogBus::post("\xe2\x9d\x8c Programmazione: Impossibile eseguire 'ip neigh show'.");
    lanResetBtns();
    if (auto* p = qobject_cast<QProcess*>(sender())) { p->deleteLater(); m_lanProc = nullptr; }
}

void ProgrammazionePage::onLanArpReadyRead()
{
    if (!m_lanProc) return;
    m_lanBuf += QString::fromLocal8Bit(m_lanProc->readAll());
}

void ProgrammazionePage::onLanArpFinished(int /*code*/, QProcess::ExitStatus /*status*/)
{
    /* Parsing output `ip neigh show`:
       10.42.0.132 dev eno1 lladdr 34:15:9e:3b:f0:5c REACHABLE
       Ignora le righe FAILED (nessun lladdr). */
    const QStringList lines = m_lanBuf.split('\n', Qt::SkipEmptyParts);
    int count = 0;
    static const QRegularExpression reNeigh(
        R"((\d+\.\d+\.\d+\.\d+)\s+\S+\s+(\S+)\s+lladdr\s+([\da-fA-F:]{17}))");
    for (const QString& line : lines) {
        const auto m = reNeigh.match(line.trimmed());
        if (!m.hasMatch()) continue;
        lanAddRow(m.captured(1), m.captured(3).toUpper(), m.captured(2), "cached");
        ++count;
    }
    if (m_lanStatusLbl)
        m_lanStatusLbl->setText(
            count == 0
            ? "\xf0\x9f\x9f\xa1  Cache ARP vuota (nessun host raggiunto di recente)."
            : QString("\xe2\x9c\x85  %1 host trovati nella cache ARP.").arg(count));
    lanResetBtns();
    if (auto* p = qobject_cast<QProcess*>(sender())) { p->deleteLater(); m_lanProc = nullptr; }
}

void ProgrammazionePage::onLanNmapError(QProcess::ProcessError err)
{
    Q_UNUSED(err)
    if (m_lanStatusLbl)
        m_lanStatusLbl->setText(
            "\xe2\x9d\x8c  Impossibile eseguire 'nmap'. "
            "Installa nmap: sudo apt install nmap");
    lanResetBtns();
    if (auto* p = qobject_cast<QProcess*>(sender())) { p->deleteLater(); m_lanProc = nullptr; }
}

void ProgrammazionePage::onLanNmapReadyRead()
{
    if (!m_lanProc) return;
    m_lanBuf += QString::fromLocal8Bit(m_lanProc->readAll());
    /* Aggiorna status con numero righe ricevute */
    const int lines = m_lanBuf.count('\n');
    if (m_lanStatusLbl && lines % 10 == 0)
        m_lanStatusLbl->setText(
            QString("\xf0\x9f\x8c\x90  nmap -sn %1 ... (%2 righe)")
            .arg(m_lanNmapSubnet).arg(lines));
}

void ProgrammazionePage::onLanNmapFinished(int /*code*/, QProcess::ExitStatus /*status*/)
{
    /* Parsing output nmap -sn — ordine reale delle righe per host:
         Nmap scan report for [hostname (]ip[)]
         Host is up (latency).
         MAC Address: AA:BB:CC:DD:EE:FF (Vendor)   ← solo con root
       Bufferizziamo ip/host/mac per ogni host e le emettiamo al blocco successivo
       (o alla fine), così il MAC viene letto prima di chiamare lanAddRow. */
    const QStringList lines = m_lanBuf.split('\n', Qt::SkipEmptyParts);
    int count = 0;
    QString ip, host, mac;
    bool hostUp = false;

    static const QRegularExpression reIP(R"((\d+\.\d+\.\d+\.\d+))");
    static const QRegularExpression reHost(R"(^([^\s(]+))");
    static const QRegularExpression reMac(R"(MAC Address:\s+([\da-fA-F:]{17}))");

    auto flushHost = [&]() {
        if (ip.isEmpty() || !hostUp) return;
        lanAddRow(ip, mac, host, "online");
        ++count;
    };

    for (const QString& line : lines) {
        const QString l = line.trimmed();
        if (l.startsWith("Nmap scan report for ")) {
            flushHost();
            ip.clear(); host.clear(); mac.clear(); hostUp = false;
            const QString rest = l.mid(21);
            const auto m1 = reIP.match(rest);
            if (m1.hasMatch()) ip = m1.captured(1);
            const auto m2 = reHost.match(rest);
            if (m2.hasMatch() && m2.captured(1) != ip) host = m2.captured(1);
        } else if (l.startsWith("MAC Address:")) {
            const auto mm = reMac.match(l);
            if (mm.hasMatch()) mac = mm.captured(1);
        } else if (l.startsWith("Host is up")) {
            hostUp = true;
        }
    }
    flushHost(); // ultimo host del buffer

    /* Senza root nmap non legge i MAC — arricchisce dalla cache ARP del kernel */
    enrichMacFromNeigh();

    if (m_lanStatusLbl)
        m_lanStatusLbl->setText(
            count == 0
            ? "\xf0\x9f\x9f\xa1  Nessun host trovato."
            : QString("\xe2\x9c\x85  %1 host online trovati con nmap.").arg(count));
    lanResetBtns();
    if (auto* p = qobject_cast<QProcess*>(sender())) { p->deleteLater(); m_lanProc = nullptr; }
}

void ProgrammazionePage::enrichMacFromNeigh()
{
    if (!m_lanTable || m_lanTable->rowCount() == 0) return;

    QMap<QString, QString> cache;

    /* 1. Cache ARP del kernel — host remoti già raggiunti */
    const QString arpOut = ProcHelper::readOutput("ip", {"neigh", "show"}, 2000);
    if (!arpOut.isEmpty()) {
        static const QRegularExpression reNeigh(
            R"((\d+\.\d+\.\d+\.\d+)\s+\S+\s+\S+\s+lladdr\s+([\da-fA-F:]{17}))");
        QRegularExpressionMatchIterator it = reNeigh.globalMatch(arpOut);
        while (it.hasNext()) {
            const auto m = it.next();
            cache[m.captured(1)] = m.captured(2).toUpper();
        }
    }

    /* 2. IP locali del PC — non compaiono in ip neigh (non ci si fa ARP con se stessi).
          Costruiamo una mappa ip→MAC dalle interfacce di sistema. */
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const QString hw = iface.hardwareAddress().toUpper();
        if (hw.isEmpty()) continue;
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            cache[e.ip().toString()] = hw;
        }
    }

    for (int r = 0; r < m_lanTable->rowCount(); ++r) {
        auto* macItem = m_lanTable->item(r, 1);
        if (!macItem || !macItem->text().isEmpty()) continue;
        auto* ipItem = m_lanTable->item(r, 0);
        if (!ipItem) continue;
        const QString mac = cache.value(ipItem->text());
        if (!mac.isEmpty())
            macItem->setText(mac);
    }
}

void ProgrammazionePage::onLanStopTimer()
{
    if (m_lanProc && m_lanProc->state() != QProcess::NotRunning)
        m_lanProc->kill();
}

/* ======================================================================
   Sezione 9 — Git MCP slots
   ====================================================================== */

void ProgrammazionePage::populateGitModels()
{
    if (m_ai && m_gitAiModel) AiUtils::populateModelCombo(m_ai, m_gitAiModel, this);
}

void ProgrammazionePage::onBtnGitBrowseClicked()
{
    const QString d = QFileDialog::getExistingDirectory(
        this, "Scegli repository git",
        m_gitRepoPath ? m_gitRepoPath->text() : QDir::homePath());
    if (!d.isEmpty() && m_gitRepoPath)
        m_gitRepoPath->setText(d);
}

void ProgrammazionePage::onBtnGitStopClicked()
{
    if (m_gitProc && m_gitProc->state() != QProcess::NotRunning)
        m_gitProc->kill();
}

void ProgrammazionePage::onBtnClearGitClicked()
{
    if (m_gitOutput) m_gitOutput->clear();
}

void ProgrammazionePage::onBtnCloseGitAiClicked()
{
    if (m_gitAiPanel) m_gitAiPanel->hide();
}

void ProgrammazionePage::onBtnGitPullClicked()
{
    if (QMessageBox::question(this, "git pull",
            "Eseguire git pull?\n\n"
            "Le modifiche remote verranno unite al branch corrente.",
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    gitRun("pull");
}

void ProgrammazionePage::onBtnAddCommitClicked()
{
    const QString msg = m_gitCommitMsg ? m_gitCommitMsg->text().trimmed() : QString();
    if (msg.isEmpty()) {
        QMessageBox::warning(this, "Messaggio mancante",
            "Inserisci un messaggio di commit prima di procedere.");
        return;
    }
    if (QMessageBox::question(this, "Add + Commit",
            QString("Eseguire:\n  git add -A\n  git commit -m \"%1\"\n\n"
                    "Tutte le modifiche verranno staged e committate.").arg(msg),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    m_gitPendingCommit = msg;
    gitRun("add", {"-A"});
}

void ProgrammazionePage::onBtnPushClicked()
{
    if (QMessageBox::question(this, "git push",
            "Eseguire git push?\n\n"
            "I commit locali verranno inviati al repository remoto.",
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    gitRun("push");
}

void ProgrammazionePage::onBtnGitAiClicked()
{
    if (!m_gitAiPanel) return;
    m_gitAiPanel->show();
    const QString context = m_gitOutput ? m_gitOutput->toPlainText() : QString();
    gitAiRequest(
        "Analizza l'output git, spiega cosa rappresenta e suggerisci "
        "le prossime azioni da eseguire.",
        context);
}

void ProgrammazionePage::onBtnGenCommitClicked()
{
    if (!m_gitAiPanel) return;
    m_gitAiPanel->show();

    /* Prima esegue diff --staged per avere il contesto completo */
    const QString context = m_gitOutput ? m_gitOutput->toPlainText() : QString();
    gitAiRequest(
        "Genera un messaggio di commit convenzionale (Conventional Commits) "
        "per le modifiche mostrate nell'output git diff --staged. "
        "Formato: <tipo>(<scope>): <descrizione breve>\n\n"
        "Wrap il messaggio tra [COMMIT] e [/COMMIT] in modo che io possa "
        "estrarlo automaticamente.",
        context);
}

void ProgrammazionePage::onBtnGitStatusClicked()     { gitRun("status"); }
void ProgrammazionePage::onBtnGitDiffClicked()       { gitRun("diff"); }
void ProgrammazionePage::onBtnGitDiffStagedClicked() { gitRun("diff", {"--cached"}); }
void ProgrammazionePage::onBtnGitLogClicked()        { gitRun("log", {"--oneline", "-20"}); }
void ProgrammazionePage::onBtnGitBranchClicked()     { gitRun("branch", {"-a"}); }

void ProgrammazionePage::onGitReadyRead()
{
    if (!m_gitProc) return;
    const QString out = QString::fromLocal8Bit(m_gitProc->readAll());
    if (m_gitOutput) m_gitOutput->appendPlainText(out);
}

void ProgrammazionePage::onGitFinished(int exitCode, QProcess::ExitStatus /*status*/)
{
    if (m_gitActRow)  m_gitActRow->setEnabled(true);
    if (m_btnGitStop) m_btnGitStop->setEnabled(false);

    if (m_gitOutput)
        m_gitOutput->appendPlainText(
            exitCode == 0
            ? "\n\xe2\x9c\x85  Operazione completata.\n"
            : QString("\n\xe2\x9d\x8c  Exit code: %1\n").arg(exitCode));

    /* Auto-commit dopo add -A riuscito */
    if (!m_gitPendingCommit.isEmpty() && exitCode == 0) {
        const QString msg = m_gitPendingCommit;
        m_gitPendingCommit.clear();
        gitRun("commit", {"-m", msg});
    }
}

void ProgrammazionePage::onGitErrorOccurred(QProcess::ProcessError err)
{
    Q_UNUSED(err)
    if (m_gitActRow)  m_gitActRow->setEnabled(true);
    if (m_btnGitStop) m_btnGitStop->setEnabled(false);
    if (m_gitOutput)
        m_gitOutput->appendPlainText(
            "\xe2\x9d\x8c  Impossibile avviare il processo git. "
            "Verifica che git sia installato e nel PATH.\n");
}

void ProgrammazionePage::onGitAiToken(const QString& tok)
{
    if (!m_gitAiOutput) return;
    m_gitAiOutput->moveCursor(QTextCursor::End);
    m_gitAiOutput->insertPlainText(tok);
    m_gitAiOutput->ensureCursorVisible();
}

void ProgrammazionePage::onGitAiFinished(const QString& full)
{
    disconnect(m_gitAiTokenConn);
    disconnect(m_gitAiFinishedConn);
    disconnect(m_gitAiErrorConn);
    /* Se la risposta contiene [COMMIT]...[/COMMIT], popola il campo commit msg */
    static const QRegularExpression reCommit(
        R"(\[COMMIT\]([\s\S]*?)\[/COMMIT\])");
    const auto m = reCommit.match(full);
    if (m.hasMatch() && m_gitCommitMsg) {
        const QString msg = m.captured(1).trimmed();
        if (!msg.isEmpty()) m_gitCommitMsg->setText(msg);
    }
}

void ProgrammazionePage::onGitAiError(const QString& msg)
{
    disconnect(m_gitAiTokenConn);
    disconnect(m_gitAiFinishedConn);
    disconnect(m_gitAiErrorConn);
    if (m_gitAiOutput) {
        m_gitAiOutput->moveCursor(QTextCursor::End);
        m_gitAiOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    }
}

/* ======================================================================
   Sezione 10 — Python REPL slots
   ====================================================================== */

void ProgrammazionePage::onReplReadyRead()
{
    if (!m_replProc) return;
    const QString out = QString::fromLocal8Bit(m_replProc->readAll());
    if (m_replOutput) {
        m_replOutput->moveCursor(QTextCursor::End);
        m_replOutput->insertPlainText(out);
        m_replOutput->ensureCursorVisible();
    }
}

void ProgrammazionePage::onReplStarted()
{
    if (m_replStatus)
        m_replStatus->setText(tr("\xe2\x9c\x85  Sessione attiva"));
    if (m_replInput) {
        m_replInput->setEnabled(true);
        m_replInput->setFocus();
    }
    if (m_btnSendRepl) m_btnSendRepl->setEnabled(true);
}

void ProgrammazionePage::onReplFinished(int code, QProcess::ExitStatus /*status*/)
{
    if (m_replStatus)
        m_replStatus->setText(
            code == 0
            ? "\xe2\xac\x9c  REPL terminato"
            : QString("\xe2\x9d\x8c  REPL uscito (code %1)").arg(code));
    if (m_replInput)   m_replInput->setEnabled(false);
    if (m_btnSendRepl) m_btnSendRepl->setEnabled(false);
    if (m_replOutput)
        m_replOutput->appendPlainText(
            "\n\xe2\x80\x94\xe2\x80\x94  Python REPL terminato  \xe2\x80\x94\xe2\x80\x94\n");
    if (auto* p = qobject_cast<QProcess*>(sender())) p->deleteLater();
    m_replProc = nullptr;
}

void ProgrammazionePage::onReplErrorOccurred(QProcess::ProcessError err)
{
    if (err != QProcess::FailedToStart) return;
    if (m_replStatus)
        m_replStatus->setText(tr("\xe2\x9d\x8c  python3 non trovato"));
    if (m_replOutput)
        m_replOutput->appendPlainText(
            "\xe2\x9d\x8c  python3 non trovato nel PATH. "
            "Installa Python 3.\n");
    if (m_replInput)   m_replInput->setEnabled(false);
    if (m_btnSendRepl) m_btnSendRepl->setEnabled(false);
    if (auto* p = qobject_cast<QProcess*>(sender())) p->deleteLater();
    m_replProc = nullptr;
}

void ProgrammazionePage::onReplTabChanged(int idx)
{
    /* Controlla se il widget del tab corrente contiene m_replOutput.
       La gerarchia e': replOutput -> outGroup -> w (QWidget).
       m_innerTabs->widget(idx) == w */
    if (!m_replOutput) return;
    /* Naviga 3 livelli su: m_replOutput->parent = outGroup,
       outGroup->parent = w, w e' il widget del tab */
    QWidget* replWidget =
        m_replOutput->parentWidget()   /* outGroup */
            ? qobject_cast<QWidget*>(m_replOutput->parentWidget()->parent())  /* w */
            : nullptr;
    if (!replWidget) return;
    if (m_innerTabs->widget(idx) != replWidget) return;
    if (!m_replProc || m_replProc->state() == QProcess::NotRunning)
        replStart();
}

void ProgrammazionePage::onBtnReplRestartClicked()
{
    replStart();
}

void ProgrammazionePage::onBtnReplClearClicked()
{
    if (m_replOutput) m_replOutput->clear();
}

void ProgrammazionePage::onBtnReplImportClicked()
{
    if (!m_replProc || m_replProc->state() != QProcess::Running) {
        if (m_replOutput)
            m_replOutput->appendPlainText(
                "\xe2\x9d\x8c  Avvia prima il REPL con "
                "\xf0\x9f\x94\x84 Riavvia.\n");
        return;
    }
    if (!m_editor) return;
    const QString code = m_editor->toPlainText().trimmed();
    if (code.isEmpty()) return;

    /* Scrivi su file temp, poi exec() nel REPL */
    const QString tmp = QStandardPaths::writableLocation(
        QStandardPaths::TempLocation) + "/prismalux_repl_import.py";
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    f.write(code.toUtf8());
    f.close();

    if (m_replOutput)
        m_replOutput->appendPlainText("\n# === Importa da editor ===\n");
    /* exec(open(...).read()) e' piu' robusto di exec() su stringa multiline */
    const QString cmd = QString("exec(open(r'%1').read())\n")
                        .arg(QDir::toNativeSeparators(tmp).replace('\\', '/'));
    m_replProc->write(cmd.toUtf8());
}

void ProgrammazionePage::sendReplLine()
{
    if (!m_replProc || m_replProc->state() != QProcess::Running) return;
    replSend();
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

/* ======================================================================
   Sezione 12 — Metodi di supporto (showDiff, runLint)
   Dichiarati nel header ma logica da implementare
   ====================================================================== */

void ProgrammazionePage::showDiff(const QString& before, const QString& after)
{
    /* Diff semplice riga-per-riga: verde = aggiunto, rosso = rimosso */
    if (!m_diffGroup || !m_diffView) return;

    const QStringList oldLines = before.split('\n');
    const QStringList newLines = after.split('\n');

    QString html = "<pre style=\"font-family:monospace;\">";
    /* Mostra le righe di before che non compaiono in after (rimosse) */
    for (const QString& l : oldLines) {
        if (!newLines.contains(l))
            html += "<span style=\"color:#e05050;background:#3a1010;\">- "
                  + l.toHtmlEscaped() + "</span>\n";
    }
    /* Mostra le righe di after che non compaiono in before (aggiunte) */
    for (const QString& l : newLines) {
        if (!oldLines.contains(l))
            html += "<span style=\"color:#50c050;background:#103a10;\">+ "
                  + l.toHtmlEscaped() + "</span>\n";
    }
    html += "</pre>";

    m_diffView->setHtml(html);
    m_diffGroup->setVisible(!before.isEmpty() && before != after);
}

void ProgrammazionePage::runLint()
{
    /* Placeholder: analisi statica (pyflakes, clang-tidy, eslint) */
    /* TODO: implementare quando il linting per-lingua sarà definito */
    if (m_status)
        m_status->setText(
            "\xf0\x9f\x94\x8d  Analisi statica non ancora implementata "
            "per questo linguaggio.");
}

/* ======================================================================
   Sezione 12 — Driver & Kernel slots
   ====================================================================== */

/* ── helper: esegue un comando read-only e mostra l'output in out ── */
void ProgrammazionePage::onDriverRunCmd(const QString& cmd)
{
    /* Determina quale QTextEdit usare in base al sender (non usato direttamente):
       gli slot chiamanti impostano m_driverAiActive prima di chiamare questa fn. */
    QTextEdit* out = m_driverAiActive ? m_driverAiActive : m_driverOutput;
    if (!out) return;

    if (m_driverProcess) {
        m_driverProcess->kill();
        m_driverProcess->waitForFinished(500);
        m_driverProcess->deleteLater();
        m_driverProcess = nullptr;
    }

    out->clear();
    out->append(QString("<span style='color:#aaa'>$ %1</span>").arg(cmd.toHtmlEscaped()));

    m_driverProcess = new QProcess(this);
    m_driverProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_driverProcess, &QProcess::readyReadStandardOutput,
            this, &ProgrammazionePage::onDriverCmdOutput);
    connect(m_driverProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onDriverCmdFinished);

    m_driverProcess->start("bash", QStringList() << "-c" << cmd);
}

void ProgrammazionePage::onDriverCmdOutput()
{
    if (!m_driverProcess) return;
    QTextEdit* out = m_driverAiActive ? m_driverAiActive : m_driverOutput;
    if (!out) return;
    /* MergedChannels: tutto su stdout */
    const QString txt = QString::fromUtf8(m_driverProcess->readAllStandardOutput());
    if (!txt.isEmpty()) {
        out->moveCursor(QTextCursor::End);
        out->insertPlainText(txt);
        out->ensureCursorVisible();
    }
}

void ProgrammazionePage::onDriverCmdFinished(int exitCode,
                                              QProcess::ExitStatus /*status*/)
{
    QTextEdit* out = m_driverAiActive ? m_driverAiActive : m_driverOutput;
    if (out) {
        const QString msg = exitCode == 0
            ? "\n\xe2\x9c\x85  Completato."
            : QString("\n\xe2\x9d\x8c  Exit code: %1").arg(exitCode);
        out->moveCursor(QTextCursor::End);
        out->insertPlainText(msg);
        out->ensureCursorVisible();
    }
    if (m_driverProcess) {
        m_driverProcess->deleteLater();
        m_driverProcess = nullptr;
    }
}

/* ── helper: invia richiesta guida AI e aggiorna l'output corretto ── */
void ProgrammazionePage::onDriverAiGuide(const QString& topic)
{
    if (m_driverAiBusy) return;

    QTextEdit* out = m_driverAiActive;
    if (!out) return;

    const QString sys =
        "Sei un esperto di sistemi Linux. Rispondi in italiano con comandi "
        "esatti, passo per passo, pronti per essere copiati nel terminale. "
        "Usa blocchi di codice markdown per i comandi.";

    out->clear();
    out->append(
        QString("<span style='color:#aaa'>\xf0\x9f\xa4\x96  %1...</span>")
        .arg("Guida AI in corso"));

    m_driverAiBusy = true;
    disconnect(m_driverAiTokenConn);
    disconnect(m_driverAiFinishedConn);
    disconnect(m_driverAiErrorConn);
    m_driverAiTokenConn    = connect(m_ai, &AiClient::token,
                                     this, &ProgrammazionePage::onDriverAiToken);
    m_driverAiFinishedConn = connect(m_ai, &AiClient::finished,
                                     this, &ProgrammazionePage::onDriverAiFinished);
    m_driverAiErrorConn    = connect(m_ai, &AiClient::error,
                                     this, &ProgrammazionePage::onDriverAiError);

    /* Primo clear per lo streaming pulito */
    out->clear();
    m_ai->chat(P::prependKnowledge(sys), topic);
}

void ProgrammazionePage::onDriverAiToken(const QString& t)
{
    QTextEdit* out = m_driverAiActive;
    if (!out) return;
    out->moveCursor(QTextCursor::End);
    out->insertPlainText(t);
    out->ensureCursorVisible();
}

void ProgrammazionePage::onDriverAiFinished(const QString& /*full*/)
{
    m_driverAiBusy = false;
    disconnect(m_driverAiTokenConn);
    disconnect(m_driverAiFinishedConn);
    disconnect(m_driverAiErrorConn);
}

void ProgrammazionePage::onDriverAiError(const QString& msg)
{
    m_driverAiBusy = false;
    disconnect(m_driverAiTokenConn);
    disconnect(m_driverAiFinishedConn);
    disconnect(m_driverAiErrorConn);
    QTextEdit* out = m_driverAiActive;
    if (out) {
        out->moveCursor(QTextCursor::End);
        out->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore AI: %1").arg(msg));
    }
}

/* ── NVIDIA slots ── */

void ProgrammazionePage::onNvidiaDetectClicked()
{
    m_driverAiActive = m_driverOutput;
    onDriverRunCmd("nvidia-smi");
}

void ProgrammazionePage::onNvidiaDownloadClicked()
{
    QDesktopServices::openUrl(QUrl("https://www.nvidia.com/drivers"));
}

void ProgrammazionePage::onNvidiaDkmsClicked()
{
    if (!m_driverOutput) return;
    m_driverAiActive = m_driverOutput;
    m_driverOutput->clear();
    m_driverOutput->append(
        "<b>Comando da eseguire nel terminale (richiede sudo):</b>\n");
    m_driverOutput->append(
        "<code>sudo apt install --reinstall nvidia-dkms-$(nvidia-smi "
        "--query-gpu=driver_version --format=csv,noheader | cut -d. -f1)</code>\n");
    m_driverOutput->append(
        "\n<span style='color:#aaa'>"
        "\xe2\x84\xb9  Il comando non viene eseguito automaticamente "
        "perch\xc3\xa9 richiede privilegi amministrativi. "
        "Copialo e incollalo nel terminale.</span>");
}

void ProgrammazionePage::onNvidiaGuideClicked()
{
    m_driverAiActive = m_driverOutput;
    onDriverAiGuide(
        "Come installare o aggiornare i driver NVIDIA su Ubuntu/Debian/Arch Linux. "
        "Dammi i comandi esatti passo per passo, inclusi: "
        "rilevamento GPU, aggiunta repository, installazione driver, "
        "configurazione DKMS e verifica finale con nvidia-smi.");
}

/* ── AMD slots ── */

void ProgrammazionePage::onAmdDetectClicked()
{
    m_driverAiActive = m_driverAmdOutput;
    onDriverRunCmd("lspci | grep -i vga ; glxinfo 2>/dev/null | grep -i renderer");
}

void ProgrammazionePage::onAmdDownloadClicked()
{
    QDesktopServices::openUrl(QUrl("https://www.amd.com/support"));
}

void ProgrammazionePage::onAmdGuideClicked()
{
    m_driverAiActive = m_driverAmdOutput;
    onDriverAiGuide(
        "Come configurare i driver AMD/amdgpu su Linux. "
        "Spiega la differenza tra il driver amdgpu incluso nel kernel e ROCm. "
        "Fornisci i comandi per: verificare il driver attivo, installare Mesa, "
        "installare ROCm per ML/AI su GPU AMD, e risolvere problemi comuni.");
}

/* ── Kernel Linux slots ── */

void ProgrammazionePage::onKernelVersionClicked()
{
    m_driverAiActive = m_driverKernelOutput;
    onDriverRunCmd("uname -r");
}

void ProgrammazionePage::onKernelListClicked()
{
    m_driverAiActive = m_driverKernelOutput;
    onDriverRunCmd("dpkg --list 2>/dev/null | grep linux-image || "
                   "rpm -qa 2>/dev/null | grep kernel || "
                   "ls /boot/vmlinuz* 2>/dev/null");
}

void ProgrammazionePage::onKernelGuideClicked()
{
    m_driverAiActive = m_driverKernelOutput;
    onDriverAiGuide(
        "Guida completa alla compilazione di un kernel Linux personalizzato. "
        "Spiega passo per passo: download dei sorgenti dal kernel.org, "
        "copia della configurazione attuale (cp /boot/config-$(uname -r) .config), "
        "make menuconfig, compilazione con make -j$(nproc), "
        "installazione moduli con make modules_install, "
        "installazione kernel con make install, "
        "aggiornamento bootloader con update-grub. "
        "Includi prerequisiti e avvertenze di sicurezza.");
}

void ProgrammazionePage::onKernelSafetyClicked()
{
    QMessageBox::warning(
        this,
        "\xe2\x9a\xa0  Nota sicurezza — Compilazione Kernel",
        "Compilare il kernel \xc3\xa8 un'operazione avanzata che comporta rischi:\n\n"
        "\xe2\x80\xa2 Fare sempre un backup completo del sistema prima di procedere.\n"
        "\xe2\x80\xa2 Su sistemi di produzione usare kernel precompilati e testati.\n"
        "\xe2\x80\xa2 Un kernel mal configurato pu\xc3\xb2 rendere il sistema non avviabile.\n"
        "\xe2\x80\xa2 Mantenere almeno un kernel funzionante nel bootloader.\n"
        "\xe2\x80\xa2 Testare prima in una macchina virtuale o ambiente di staging.\n\n"
        "Procedere solo se si ha esperienza con la gestione del sistema Linux.");
}

/* ══════════════════════════════════════════════════════════════
   Sezione 12b — Reverse Engineering Kernel / Driver
   ══════════════════════════════════════════════════════════════ */

void ProgrammazionePage::onReRunCmd(const QString& cmd, const QString& header)
{
    if (!m_reKernelOutput) return;

    if (!header.isEmpty())
        m_reKernelOutput->append(
            QString("\n<span style='color:#4fc3f7;font-weight:bold'>%1</span>")
                .arg(header.toHtmlEscaped()));
    m_reKernelOutput->append(
        QString("<span style='color:#888'>$ %1</span>").arg(cmd.toHtmlEscaped()));

    if (m_reProcess) {
        m_reProcess->kill();
        m_reProcess->waitForFinished(500);
        m_reProcess->deleteLater();
        m_reProcess = nullptr;
    }
    m_reProcess = new QProcess(this);
    m_reProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_reProcess, &QProcess::readyReadStandardOutput,
            this, &ProgrammazionePage::onReCmdOutput);
    connect(m_reProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onReCmdFinished);
    m_reProcess->start("bash", QStringList() << "-c" << cmd);
}

void ProgrammazionePage::onReCmdOutput()
{
    if (!m_reProcess || !m_reKernelOutput) return;
    const QString txt = QString::fromUtf8(m_reProcess->readAllStandardOutput());
    if (!txt.isEmpty()) {
        m_reKernelOutput->moveCursor(QTextCursor::End);
        m_reKernelOutput->insertPlainText(txt);
        m_reKernelOutput->ensureCursorVisible();
    }
}

void ProgrammazionePage::onReCmdFinished(int exitCode, QProcess::ExitStatus)
{
    if (m_reKernelOutput) {
        const QString msg = exitCode == 0
            ? "\n\xe2\x9c\x85  Completato."
            : QString("\n\xe2\x9d\x8c  Exit code: %1").arg(exitCode);
        m_reKernelOutput->moveCursor(QTextCursor::End);
        m_reKernelOutput->insertPlainText(msg + "\n");
        m_reKernelOutput->ensureCursorVisible();
    }
    if (m_reProcess) { m_reProcess->deleteLater(); m_reProcess = nullptr; }
}

static QString reTarget(QLineEdit* edit)
{
    return edit ? edit->text().trimmed() : QString{};
}

static QString shellQ(const QString& s)
{
    QString r = s;
    r.replace("'", "'\\''");
    return "'" + r + "'";
}

void ProgrammazionePage::onReFileClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) { onReRunCmd("file --version"); return; }
    onReRunCmd(QString("file %1").arg(
        shellQ(t)), "file");
}

void ProgrammazionePage::onReReadelfClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) { m_reKernelOutput->append("Specifica un target .ko / ELF."); return; }
    onReRunCmd(QString("readelf -a %1 2>&1 | head -300").arg(
        shellQ(t)), "readelf -a");
}

void ProgrammazionePage::onReObjdumpClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) { m_reKernelOutput->append("Specifica un target .ko / ELF."); return; }
    onReRunCmd(QString("objdump -d %1 2>&1 | head -400").arg(
        shellQ(t)), "objdump -d (prime 400 righe)");
}

void ProgrammazionePage::onReNmClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) { m_reKernelOutput->append("Specifica un target .ko / oggetto ELF."); return; }
    onReRunCmd(QString("nm --defined-only --demangle %1 2>&1 | sort").arg(
        shellQ(t)), "nm --defined-only");
}

void ProgrammazionePage::onReStringsClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) { m_reKernelOutput->append("Specifica un target binario."); return; }
    onReRunCmd(QString("strings -n 8 %1 2>&1 | head -300").arg(
        shellQ(t)), "strings -n 8");
}

void ProgrammazionePage::onReLddClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) { m_reKernelOutput->append("Specifica un eseguibile o libreria .so."); return; }
    onReRunCmd(QString("ldd %1 2>&1").arg(
        shellQ(t)), "ldd");
}

void ProgrammazionePage::onReModinfoClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    const QString arg = t.isEmpty() ? "snd_usb_audio" : t;
    onReRunCmd(QString("modinfo %1 2>&1").arg(
        shellQ(arg)), "modinfo");
}

void ProgrammazionePage::onReLsmodClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty())
        onReRunCmd("lsmod 2>&1 | head -80", "lsmod");
    else
        onReRunCmd(QString("lsmod 2>&1 | grep -i %1").arg(
            shellQ(t)), "lsmod (filtrato)");
}

void ProgrammazionePage::onReKallsymsClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) {
        onReRunCmd("wc -l /proc/kallsyms && head -20 /proc/kallsyms",
                   "kallsyms (prime 20 righe)");
        return;
    }
    onReRunCmd(QString("grep -i %1 /proc/kallsyms 2>&1 | head -100").arg(
        shellQ(t)), "kallsyms (ricerca)");
}

void ProgrammazionePage::onReDmesgDrvClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty())
        onReRunCmd("dmesg --level=err,warn 2>&1 | tail -100", "dmesg (err+warn)");
    else
        onReRunCmd(QString("dmesg 2>&1 | grep -i %1 | tail -100").arg(
            shellQ(t)), "dmesg (filtrato)");
}

void ProgrammazionePage::onReStraceClicked()
{
    if (!m_reKernelOutput) return;
    m_reKernelOutput->append(
        "\n\xf0\x9f\x95\xb5 <b>strace</b> richiede un PID o un comando da avviare.<br>"
        "Usa il terminale:<br>"
        "<code>  strace -c -p &lt;PID&gt;</code>  (statistiche syscall su processo in esecuzione)<br>"
        "<code>  strace -f -e trace=network ls</code>  (filtra famiglia syscall)<br>"
        "<code>  strace -o /tmp/trace.log &lt;comando&gt;</code>  (salva su file)"
    );
    const QString t = reTarget(m_reTargetEdit);
    if (!t.isEmpty() && t.startsWith("/")) {
        const QString cmd = QString("strace -c %1 2>&1").arg(shellQ(t));
        m_reKernelOutput->append(
            QString("\nAvvio: <code>%1</code>").arg(cmd.toHtmlEscaped()));
        onReRunCmd(cmd, "strace -c");
    }
}

void ProgrammazionePage::onReKprobesClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) {
        onReRunCmd(
            "ls /sys/kernel/debug/tracing/available_filter_functions 2>/dev/null"
            " | head -1 || echo 'kprobes: monta debugfs con: mount -t debugfs none /sys/kernel/debug'",
            "kprobes");
    } else {
        onReRunCmd(
            QString("grep -i %1 /sys/kernel/debug/tracing/available_filter_functions 2>&1"
                    " | head -80").arg(shellQ(t)),
            "kprobes (ricerca funzione)");
    }
}

void ProgrammazionePage::onReAiAnalyzeClicked()
{
    if (!m_reKernelOutput || !m_ai) return;
    const QString output = m_reKernelOutput->toPlainText().trimmed();
    if (output.isEmpty()) {
        m_reKernelOutput->append("\nNessun output da analizzare — esegui prima uno strumento.");
        return;
    }

    const QString target = reTarget(m_reTargetEdit);
    const QString sys =
        "Sei un esperto di reverse engineering e kernel Linux. "
        "L'utente ha eseguito un'analisi su un binario o modulo kernel. "
        "Spiega in italiano: cosa fa questo file, quali simboli/sezioni sono importanti, "
        "eventuali pattern sospetti, e suggerisci i prossimi passi RE.";

    const QString limitedOutput = output.length() > 6000
        ? output.left(3000) + "\n...[troncato]...\n" + output.right(2000)
        : output;

    const QString msg = QString(
        "Target: %1\n\nOutput analisi:\n```\n%2\n```\n\n"
        "Analizza e spiega cosa vedi.")
        .arg(target.isEmpty() ? "(non specificato)" : target)
        .arg(limitedOutput);

    m_reKernelOutput->append(
        "\n\xf0\x9f\xa4\x96 Analisi AI in corso...\n");

    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::token, holder,
            [this, holder](const QString& t){
        Q_UNUSED(holder)
        m_reKernelOutput->moveCursor(QTextCursor::End);
        m_reKernelOutput->insertPlainText(t);
        m_reKernelOutput->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, holder,
            [this, holder](const QString&){
        holder->deleteLater();
        m_reKernelOutput->append("\n");
    });
    connect(m_ai, &AiClient::error, holder,
            [this, holder](const QString& e){
        holder->deleteLater();
        m_reKernelOutput->append(
            QString("\n\xe2\x9d\x8c AI error: %1").arg(e));
    });
    m_ai->chat(sys, msg);
}

/* ══════════════════════════════════════════════════════════════
   Sezione 13 — USB / Firmware & Videocam LAN
   ══════════════════════════════════════════════════════════════ */

/* ── helper: esegue un comando bash e mostra l'output in m_usbOutput ── */
void ProgrammazionePage::onUsbRunCmd(const QString& cmd)
{
    if (!m_usbOutput) return;
    m_usbOutput->clear();
    m_usbOutput->append(
        QString("<span style='color:#aaa'>$ %1</span>").arg(cmd.toHtmlEscaped()));

    /* Riusa m_driverProcess per i comandi USB (un processo alla volta) */
    if (m_driverProcess) {
        m_driverProcess->kill();
        m_driverProcess->waitForFinished(500);
        m_driverProcess->deleteLater();
        m_driverProcess = nullptr;
    }

    m_driverAiActive = nullptr;   /* dirige output verso m_usbOutput */
    m_driverProcess  = new QProcess(this);
    m_driverProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_driverProcess, &QProcess::readyReadStandardOutput,
            this, &ProgrammazionePage::onUsbCmdOutput);
    connect(m_driverProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onUsbCmdFinished);
    m_driverProcess->start("bash", QStringList() << "-c" << cmd);
}

void ProgrammazionePage::onUsbCmdOutput()
{
    if (!m_driverProcess || !m_usbOutput) return;
    const QString txt =
        QString::fromUtf8(m_driverProcess->readAllStandardOutput());
    if (!txt.isEmpty()) {
        m_usbOutput->moveCursor(QTextCursor::End);
        m_usbOutput->insertPlainText(txt);
        m_usbOutput->ensureCursorVisible();
    }
}

void ProgrammazionePage::onUsbCmdFinished(int exitCode,
                                           QProcess::ExitStatus /*status*/)
{
    if (m_usbOutput) {
        const QString msg = exitCode == 0
            ? "\n\xe2\x9c\x85  Completato."
            : QString("\n\xe2\x9d\x8c  Exit code: %1").arg(exitCode);
        m_usbOutput->moveCursor(QTextCursor::End);
        m_usbOutput->insertPlainText(msg);
        m_usbOutput->ensureCursorVisible();
    }
    if (m_driverProcess) {
        m_driverProcess->deleteLater();
        m_driverProcess = nullptr;
    }
}

/* ── USB info ── */

void ProgrammazionePage::onUsbListClicked()
{
    onUsbRunCmd("lsusb");
}

void ProgrammazionePage::onUsbV4l2Clicked()
{
    onUsbRunCmd(
        "v4l2-ctl --list-devices 2>/dev/null || "
        "echo 'v4l2-ctl non trovato (installa: sudo apt install v4l-utils)'");
}

void ProgrammazionePage::onUsbDetailsClicked()
{
    if (!m_usbVidPidEdit) return;
    const QString vidpid = m_usbVidPidEdit->text().trimmed();
    if (vidpid.isEmpty()) {
        if (m_usbOutput)
            m_usbOutput->append(
                "\xe2\x84\xb9  Inserisci VID:PID nel campo (es. 0bda:5652) "
                "prima di richiedere dettagli.");
        return;
    }
    onUsbRunCmd(QString("lsusb -v -d %1 2>&1 | head -120").arg(vidpid));
}

void ProgrammazionePage::onUsbUdevClicked()
{
    if (!m_usbVidPidEdit) return;
    const QString vidpid = m_usbVidPidEdit->text().trimmed();
    QString sysPath;
    if (!vidpid.isEmpty()) {
        /* cerca il bus/device dal lsusb e costruisce il syspath */
        sysPath = QString(
            "DEVPATH=$(lsusb -d %1 2>/dev/null | "
            "awk '{print \"/dev/bus/usb/\" $2 \"/\" $4}' | tr -d ':' | head -1); "
            "[ -n \"$DEVPATH\" ] && udevadm info --query=all --name=$DEVPATH || "
            "echo 'Dispositivo non trovato. Verificare VID:PID.'")
            .arg(vidpid);
    } else {
        sysPath = "udevadm info --export-db 2>/dev/null | grep -A5 'ID_BUS=usb' | head -60 || "
                  "echo 'Inserisci VID:PID per info specifiche sul dispositivo.'";
    }
    onUsbRunCmd(sysPath);
}

/* ── DFU — dump / flash ── */

void ProgrammazionePage::onDfuListClicked()
{
    onUsbRunCmd(
        "dfu-util -l 2>&1 || "
        "echo 'dfu-util non trovato. Installa: sudo apt install dfu-util'");
}

void ProgrammazionePage::onDfuDumpClicked()
{
    const QString out =
        QDir::homePath() + "/wiby_firmware_dump.bin";
    if (m_usbOutput)
        m_usbOutput->append(
            QString("\xe2\x84\xb9  Il dump verr\xc3\xa0 salvato in: <b>%1</b>").arg(out));
    onUsbRunCmd(
        QString("dfu-util -U \"%1\" 2>&1 || "
                "echo 'Verificare che il dispositivo sia in modalita DFU "
                "(tieni premuto il tasto reset accendendo il dispositivo).'")
        .arg(out));
}

void ProgrammazionePage::onDfuFlashClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Seleziona firmware (.bin/.dfu)",
        QDir::homePath(),
        "Firmware (*.bin *.dfu *.hex);;Tutti i file (*)");
    if (path.isEmpty()) return;

    const int res = QMessageBox::warning(
        this,
        "\xe2\x9a\xa0  Flash Firmware",
        QString("Stai per flashare il firmware:\n%1\n\n"
                "Questa operazione sovrascriver\xc3\xa0 il firmware del dispositivo USB.\n"
                "Assicurati che sia il file corretto per il tuo dispositivo.\n\n"
                "Continuare?").arg(path),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (res != QMessageBox::Yes) return;

    onUsbRunCmd(
        QString("dfu-util -D \"%1\" 2>&1").arg(path));
}

/* ── Guida AI ── */

void ProgrammazionePage::onUsbAiGuideClicked()
{
    m_driverAiActive = m_usbOutput;
    onDriverAiGuide(
        "Sono un utente Linux con una videocamera USB (Wiby) che non e' piu' "
        "supportata dal produttore e dipende da server cloud ora offline. "
        "Voglio: 1) Analizzare il protocollo USB con lsusb e usbmon per capire "
        "come comunica. 2) Verificare se supporta DFU per modificare il firmware. "
        "3) Avviare uno stream MJPEG locale via V4L2 e ffmpeg/python-opencv "
        "accessibile sulla LAN. Dammi i comandi esatti per Linux, passo per passo.");
}

/* ── Server MJPEG LAN ── */

void ProgrammazionePage::onCamRefreshDevices()
{
    if (!m_camDeviceCombo) return;
    m_camDeviceCombo->clear();
    for (int i = 0; i < 8; ++i) {
        const QString dev = QString("/dev/video%1").arg(i);
        if (!QFileInfo::exists(dev)) continue;
        /* Legge il nome dal sysfs e scarta metadata/output device */
        QFile namef(QString("/sys/class/video4linux/video%1/name").arg(i));
        QString label = dev;
        if (namef.open(QIODevice::ReadOnly)) {
            const QString name = QString::fromUtf8(namef.readAll()).trimmed();
            namef.close();
            if (name.contains("metadata", Qt::CaseInsensitive) ||
                name.contains(" output", Qt::CaseInsensitive))
                continue;
            label = QString("%1  [%2]").arg(dev).arg(name);
        }
        m_camDeviceCombo->addItem(label, i);
    }
    if (m_camDeviceCombo->count() == 0) {
        m_camDeviceCombo->addItem("(nessuna webcam trovata)", -1);
        if (m_usbOutput)
            m_usbOutput->append(
                "\xe2\x84\xb9  Nessun capture device trovato.\n"
                "Collega la videocamera USB e riprova.");
    }
}

void ProgrammazionePage::onCamServerStartClicked()
{
    if (!m_camDeviceCombo || !m_camPortSpin || !m_camServerStatus) return;

    const int devIdx = m_camDeviceCombo->currentData().toInt();
    if (devIdx < 0) {
        QMessageBox::warning(this, "Nessuna webcam",
            "Seleziona un dispositivo V4L2 valido (/dev/videoN).");
        return;
    }
    const int port = m_camPortSpin->value();

    if (m_camStreamProc && m_camStreamProc->state() != QProcess::NotRunning) {
        m_camStreamProc->kill();
        m_camStreamProc->waitForFinished(800);
    }

    /* Script Python MJPEG server — usa opencv già presente nei requirements */
    const QString script =
        "import cv2, socket, threading, time, sys\n"
        "dev  = int(sys.argv[1]) if len(sys.argv) > 1 else 0\n"
        "port = int(sys.argv[2]) if len(sys.argv) > 2 else 8090\n"
        "cap  = cv2.VideoCapture(dev)\n"
        "if not cap.isOpened():\n"
        "    print(f'ERRORE: /dev/video{dev} non apribile', flush=True); sys.exit(1)\n"
        "def handle(conn):\n"
        "    conn.sendall(b'HTTP/1.1 200 OK\\r\\n'\n"
        "                 b'Content-Type: multipart/x-mixed-replace; boundary=frame\\r\\n'\n"
        "                 b'Access-Control-Allow-Origin: *\\r\\n\\r\\n')\n"
        "    try:\n"
        "        while True:\n"
        "            ok, frm = cap.read()\n"
        "            if not ok: break\n"
        "            _, jpg = cv2.imencode('.jpg', frm, [cv2.IMWRITE_JPEG_QUALITY, 75])\n"
        "            data = jpg.tobytes()\n"
        "            conn.sendall(b'--frame\\r\\nContent-Type: image/jpeg\\r\\n'\n"
        "                         + f'Content-Length: {len(data)}\\r\\n\\r\\n'.encode()\n"
        "                         + data + b'\\r\\n')\n"
        "            time.sleep(1/15.0)\n"
        "    except: pass\n"
        "    finally: conn.close()\n"
        "srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)\n"
        "srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)\n"
        "srv.bind(('0.0.0.0', port)); srv.listen(10)\n"
        "print(f'ONLINE:{port}', flush=True)\n"   /* dopo listen — server pronto */
        "while True:\n"
        "    conn, _ = srv.accept()\n"
        "    threading.Thread(target=handle, args=(conn,), daemon=True).start()\n";

    /* Salva lo script temporaneo */
    m_camStreamScript = QDir::tempPath() + "/prismalux_mjpeg_server.py";
    QFile f(m_camStreamScript);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(script.toUtf8());
        f.close();
    } else {
        QMessageBox::critical(this, "Errore",
            "Impossibile scrivere lo script server in " + m_camStreamScript);
        return;
    }

    m_camStreamProc = new QProcess(this);
    m_camStreamProc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_camStreamProc, &QProcess::readyReadStandardOutput,
            this, [this] {
        if (!m_camStreamProc || !m_usbOutput) return;
        const QString line =
            QString::fromUtf8(m_camStreamProc->readAllStandardOutput()).trimmed();
        if (line.startsWith("ONLINE:")) {
            const int p = line.mid(7).toInt();
            /* URL locale per il viewer interno */
            const QString localUrl = QString("http://127.0.0.1:%1/").arg(p);
            /* URL LAN per dispositivi esterni (hotspot, telefono, ecc.) */
            QString lanIp;
            for (const QNetworkInterface& ni : QNetworkInterface::allInterfaces()) {
                if (ni.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
                if (!ni.flags().testFlag(QNetworkInterface::IsUp)) continue;
                for (const QNetworkAddressEntry& ae : ni.addressEntries()) {
                    if (ae.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                        lanIp = ae.ip().toString();
                        break;
                    }
                }
                if (!lanIp.isEmpty()) break;
            }
            const QString lanUrl = lanIp.isEmpty()
                ? localUrl
                : QString("http://%1:%2/").arg(lanIp).arg(p);

            if (m_camServerStatus) {
                m_camServerStatus->setText(
                    QString("\xf0\x9f\x9f\xa2  Server attivo \xe2\x80\x94 "
                            "locale: <a href='%1'>%1</a>  "
                            "| LAN: <a href='%2'>%2</a>")
                    .arg(localUrl).arg(lanUrl));
                m_camServerStatus->setTextFormat(Qt::RichText);
                m_camServerStatus->setOpenExternalLinks(true);
                m_camServerStatus->setStyleSheet("color: #4ade80; font-size: 12px;");
            }
            if (m_usbOutput)
                m_usbOutput->append(
                    QString("\n\xf0\x9f\x9f\xa2  Server MJPEG pronto\n"
                            "  Viewer interno: %1\n"
                            "  Da browser/VLC/telefono: %2")
                    .arg(localUrl).arg(lanUrl));

            /* Auto-connessione del viewer con 400ms di margine */
            if (m_camPreviewUrl) m_camPreviewUrl->setText(localUrl);
            QTimer::singleShot(400, this,
                &ProgrammazionePage::onCamPreviewConnect);
        } else if (!line.isEmpty()) {
            if (m_usbOutput) m_usbOutput->append(line);
        }
    });

    connect(m_camStreamProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        if (m_camServerStatus)
            m_camServerStatus->setText(
                QString("\xe2\x97\x8f  Server terminato (exit %1)").arg(code));
        m_camStreamProc = nullptr;
    });

    m_camStreamProc->start(
        "python3",
        QStringList() << m_camStreamScript
                      << QString::number(devIdx)
                      << QString::number(port));

    if (!m_camStreamProc->waitForStarted(2000)) {
        QMessageBox::critical(this, "Errore avvio",
            "Impossibile avviare python3. Verifica che sia installato e "
            "che il pacchetto opencv-python sia disponibile:\n"
            "pip install opencv-python");
        m_camStreamProc->deleteLater();
        m_camStreamProc = nullptr;
        return;
    }

    m_camServerStatus->setText(
        "\xf0\x9f\x9f\xa1  Server in avvio...");
    m_camServerStatus->setStyleSheet("color: #facc15; font-size: 12px;");
    if (m_usbOutput)
        m_usbOutput->append(
            QString("\xe2\x84\xb9  Avvio server MJPEG su /dev/video%1 porta %2...")
            .arg(devIdx).arg(port));
}

void ProgrammazionePage::onCamServerStopClicked()
{
    if (m_camStreamProc) {
        m_camStreamProc->kill();
        m_camStreamProc->waitForFinished(1000);
        m_camStreamProc = nullptr;
    }
    if (!m_camStreamScript.isEmpty() && QFileInfo::exists(m_camStreamScript))
        QFile::remove(m_camStreamScript);
    if (m_camServerStatus) {
        m_camServerStatus->setText("\xe2\x97\x8f  Server fermato.");
        m_camServerStatus->setStyleSheet("color: #888; font-size: 12px;");
    }
    if (m_usbOutput)
        m_usbOutput->append("\xe2\x96\xa0  Server MJPEG fermato.");
}

/* ── Viewer MJPEG inline ── */

void ProgrammazionePage::onCamPreviewConnect()
{
    if (!m_camPreviewUrl || !m_camPreviewLbl) return;
    const QString url = m_camPreviewUrl->text().trimmed();
    if (url.isEmpty()) return;

    /* Disconnetti eventuale stream precedente */
    if (m_camReply) {
        m_camReply->abort();
        m_camReply->deleteLater();
        m_camReply = nullptr;
    }
    m_camBuf.clear();

    if (!m_camNam)
        m_camNam = new QNetworkAccessManager(this);

    QNetworkRequest req;
    req.setUrl(QUrl(url));
    req.setRawHeader("Connection", "keep-alive");
    m_camReply = m_camNam->get(req);

    connect(m_camReply, &QNetworkReply::readyRead,
            this, &ProgrammazionePage::onCamPreviewData);
    connect(m_camReply, &QNetworkReply::finished,
            this, &ProgrammazionePage::onCamPreviewFinished);

    m_camPreviewLbl->setText(
        "\xf0\x9f\x9f\xa1  Connessione a " + url + "...");
    m_camPreviewLbl->setStyleSheet(
        "background:#111; color:#facc15; border-radius:4px;");
}

void ProgrammazionePage::onCamPreviewData()
{
    if (!m_camReply || !m_camPreviewLbl) return;
    m_camBuf += m_camReply->readAll();

    /* Cerca frame JPEG completi (SOI=FFD8 … EOI=FFD9) */
    while (true) {
        const int soiPos = m_camBuf.indexOf("\xff\xd8");
        if (soiPos < 0) {
            if (m_camBuf.size() > 8192)
                m_camBuf.remove(0, m_camBuf.size() - 8192);
            break;
        }
        const int eoiPos = m_camBuf.indexOf("\xff\xd9", soiPos + 2);
        if (eoiPos < 0) {
            if (soiPos > 0) m_camBuf.remove(0, soiPos);
            break;
        }
        const QByteArray jpeg = m_camBuf.mid(soiPos, eoiPos - soiPos + 2);
        m_camBuf.remove(0, eoiPos + 2);

        QPixmap pix;
        if (pix.loadFromData(jpeg, "JPEG")) {
            const QSize sz = m_camPreviewLbl->size();
            m_camPreviewLbl->setPixmap(
                pix.scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void ProgrammazionePage::onCamPreviewFinished()
{
    if (m_camPreviewLbl && !m_camPreviewLbl->pixmap().isNull()) {
        /* lascia l'ultimo frame visibile */
    } else if (m_camPreviewLbl) {
        m_camPreviewLbl->setText(
            "Stream terminato o non raggiungibile.");
        m_camPreviewLbl->setStyleSheet(
            "background:#111; color:#888; border-radius:4px;");
    }
    if (m_camReply) {
        m_camReply->deleteLater();
        m_camReply = nullptr;
    }
    m_camBuf.clear();
}

/* ══════════════════════════════════════════════════════════════
   WIBY Camera PTZ — slot (Sezione 14)
   ══════════════════════════════════════════════════════════════ */

static const QString kWibyScript =
    P::root() + "/Tools/scripts/wiby_controller.py";

void ProgrammazionePage::wibyUpdateStatus(bool connected)
{
    m_wibyReady = connected;
    if (m_wibyStatusLbl) {
        if (connected)
            m_wibyStatusLbl->setText(
                "<span style='color:#4ade80;'>"
                "\xe2\x97\x8f  Controller connesso</span>");
        else
            m_wibyStatusLbl->setText(
                "<span style='color:#888;'>"
                "\xe2\x97\x8f  Non connesso</span>");
        m_wibyStatusLbl->setTextFormat(Qt::RichText);
    }
}

void ProgrammazionePage::wibySend(const QJsonObject& cmd)
{
    if (!m_wibyProc || !m_wibyReady) return;
    QByteArray line = QJsonDocument(cmd).toJson(QJsonDocument::Compact);
    line += '\n';
    m_wibyProc->write(line);
}

void ProgrammazionePage::onWibyDiscoverClicked()
{
    if (m_wibyStatusLbl)
        m_wibyStatusLbl->setText("Ricerca WIBY in LAN (8s)...");

    // Avvia il controller se non attivo, poi invia discover
    if (!m_wibyProc || m_wibyProc->state() == QProcess::NotRunning) {
        onWibyConnectClicked();
        // Aspetta ready asincrono: onWibyCmdOutput chiamerà discover quando ready
        m_wibyPendingDiscover = true;
        return;
    }
    if (!m_wibyReady) {
        m_wibyPendingDiscover = true;
        return;
    }
    m_wibyPendingDiscover = false;
    QJsonObject cmd;
    cmd["cmd"]     = "discover";
    cmd["timeout"] = 8.0;
    wibySend(cmd);
}

void ProgrammazionePage::onWibyConnectClicked()
{
    if (m_wibyProc && m_wibyProc->state() != QProcess::NotRunning)
        return;

    if (!m_wibyProc) {
        m_wibyProc = new QProcess(this);
        connect(m_wibyProc, &QProcess::readyReadStandardOutput,
                this, &ProgrammazionePage::onWibyCmdOutput);
        connect(m_wibyProc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &ProgrammazionePage::onWibyProcFinished);
    }

    m_wibyReady = false;
    m_wibyProc->start("python3", {kWibyScript});
    if (m_wibyStatusLbl)
        m_wibyStatusLbl->setText("Avvio controller...");
}

void ProgrammazionePage::onWibyDisconnectClicked()
{
    if (!m_wibyProc) return;
    m_wibyProc->terminate();
    m_wibyProc->waitForFinished(1500);
    wibyUpdateStatus(false);
}

void ProgrammazionePage::onWibyCmdOutput()
{
    if (!m_wibyProc) return;
    while (m_wibyProc->canReadLine()) {
        QByteArray raw = m_wibyProc->readLine().trimmed();
        if (raw.isEmpty()) continue;
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (doc.isNull()) continue;
        QJsonObject obj = doc.object();
        if (obj.value("ready").toBool()) {
            wibyUpdateStatus(true);
            if (m_wibyPendingDiscover) {
                m_wibyPendingDiscover = false;
                QJsonObject dc;
                dc["cmd"]     = "discover";
                dc["timeout"] = 8.0;
                wibySend(dc);
                if (m_wibyStatusLbl)
                    m_wibyStatusLbl->setText("Ricerca WIBY in LAN (8s)...");
            }
        } else if (obj.contains("ok")) {
            bool ok = obj.value("ok").toBool();
            QString err = obj.value("error").toString();
            if (ok) {
                /* Risposta discover: mostra device trovati */
                if (obj.value("data").isArray()) {
                    QJsonArray arr = obj.value("data").toArray();
                    if (arr.isEmpty()) {
                        if (m_wibyStatusLbl)
                            m_wibyStatusLbl->setText(
                                "\xe2\x9a\xa0  Nessun device Tuya trovato in LAN");
                    } else {
                        QStringList found;
                        for (const QJsonValue& v : arr) {
                            QJsonObject d = v.toObject();
                            found << QString("%1 (v%2) — %3")
                                     .arg(d.value("gwId").toString())
                                     .arg(d.value("version").toString())
                                     .arg(d.value("ip").toString());
                        }
                        if (m_wibyStatusLbl)
                            m_wibyStatusLbl->setText(
                                "\xe2\x9c\x85  Trovati: " + found.join(", "));
                        if (m_usbOutput)
                            m_usbOutput->append(
                                "<span style='color:#4ade80;'>WIBY discovery: "
                                + found.join("; ") + "</span>");
                    }
                    return;
                }
                /* Risposta stream_url: il campo Tuya si chiama "url" */
                QJsonObject data = obj.value("data").toObject();
                QString streamUrl = data.value("url").toString();
                if (streamUrl.isEmpty())
                    streamUrl = data.value("hls_pull_url").toString();
                if (streamUrl.isEmpty())
                    streamUrl = data.value("rtsp_pull_url").toString();
                if (!streamUrl.isEmpty() && m_wibyStreamUrl) {
                    m_wibyStreamUrl->setText(streamUrl);
                    if (m_usbOutput)
                        m_usbOutput->append(
                            "<span style='color:#4ade80;'>URL stream ottenuto "
                            "— avvio viewer...</span>");
                    /* Auto-avvia il viewer con l'URL appena ricevuto */
                    QTimer::singleShot(100, this,
                        &ProgrammazionePage::onWibyStartStream);
                }
            } else if (!err.isEmpty() && m_usbOutput) {
                m_usbOutput->append(
                    "<span style='color:#f87171;'>WIBY: " + err + "</span>");
            }
        }
    }
}

void ProgrammazionePage::onWibyProcFinished(int /*exitCode*/,
                                             QProcess::ExitStatus /*status*/)
{
    wibyUpdateStatus(false);
}

void ProgrammazionePage::onWibyPtzClicked(const QString& direction)
{
    if (!m_wibyReady) {
        onWibyConnectClicked();
        return;
    }
    wibySend(QJsonObject{{"cmd", "ptz_move"}, {"direction", direction}});
}

void ProgrammazionePage::onWibyPtzStop()
{
    if (!m_wibyReady) return;
    wibySend(QJsonObject{{"cmd", "ptz_stop"}});
}

/* Overload bool — chiamato dai QPushButton checkable */
void ProgrammazionePage::onWibyToggleDp(int /*dp*/, bool value, const QString& code)
{
    if (!m_wibyReady) return;
    wibySend(QJsonObject{{"cmd", "set"}, {"code", code}, {"value", value}});
}

/* Overload QString — chiamato dai QComboBox (enum/valore stringa) */
void ProgrammazionePage::onWibyToggleDp(int /*dp*/, const QString& value, const QString& code)
{
    if (!m_wibyReady) return;
    wibySend(QJsonObject{{"cmd", "set"}, {"code", code}, {"value", value}});
}

void ProgrammazionePage::onWibyGetStreamUrl()
{
    auto doRequest = [this]() {
        wibySend(QJsonObject{{"cmd", "stream_url"}, {"type", "hls"}});
        if (m_usbOutput)
            m_usbOutput->append("Richiedo URL stream al cloud Tuya...");
    };

    if (!m_wibyReady) {
        /* Avvia il controller e aspetta che sia pronto, poi invia */
        onWibyConnectClicked();
        /* Aspetta la risposta {"ready":true} — arriverà in onWibyCmdOutput.
           Usiamo un timer a polling per non bloccare il thread UI. */
        auto* timer = new QTimer(this);
        timer->setInterval(200);
        int* tries = new int(0);
        connect(timer, &QTimer::timeout, this, [this, timer, tries, doRequest]() {
            ++(*tries);
            if (m_wibyReady) {
                timer->stop();
                timer->deleteLater();
                delete tries;
                doRequest();
            } else if (*tries > 25) { /* 5 secondi max */
                timer->stop();
                timer->deleteLater();
                delete tries;
                if (m_usbOutput)
                    m_usbOutput->append(
                        "<span style='color:#f87171;'>Timeout avvio controller.</span>");
            }
        });
        timer->start();
    } else {
        doRequest();
    }
}

void ProgrammazionePage::onWibyStartStream()
{
    if (!m_wibyStreamUrl) return;
    const QString url = m_wibyStreamUrl->text().trimmed();
    if (url.isEmpty()) {
        if (m_usbOutput)
            m_usbOutput->append(
                "<span style='color:#f87171;'>Inserisci un URL HLS/RTSP valido.</span>");
        return;
    }

    onWibyStopStream();
    m_wibyFfmpegBuf.clear();

    /* ffmpeg legge lo stream HLS/RTSP e produce frame JPEG su stdout —
       nessun server intermedio, tutto direttamente in-process */
    m_wibyFfmpegProc = new QProcess(this);
    connect(m_wibyFfmpegProc, &QProcess::readyReadStandardOutput,
            this, &ProgrammazionePage::onWibyFfmpegFrame);
    connect(m_wibyFfmpegProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        if (m_camPreviewLbl && m_camPreviewLbl->pixmap().isNull())
            m_camPreviewLbl->setText("Stream terminato.");
        m_wibyFfmpegProc = nullptr;
    });

    m_wibyFfmpegProc->start("ffmpeg", {
        "-loglevel", "quiet",
        "-i",        url,
        "-vf",       "fps=10,scale=640:-1",
        "-f",        "image2pipe",
        "-vcodec",   "mjpeg",
        "-q:v",      "5",
        "pipe:1"
    });

    if (m_camPreviewLbl)
        m_camPreviewLbl->setText("Connessione stream WIBY...");
    if (m_usbOutput)
        m_usbOutput->append("<span style='color:#4ade80;'>Stream WIBY avviato (ffmpeg diretto).</span>");
}

void ProgrammazionePage::onWibyStopStream()
{
    if (!m_wibyFfmpegProc) return;
    m_wibyFfmpegProc->terminate();
    m_wibyFfmpegProc->waitForFinished(1500);
    m_wibyFfmpegProc->deleteLater();
    m_wibyFfmpegProc = nullptr;
    m_wibyFfmpegBuf.clear();
    if (m_camPreviewLbl)
        m_camPreviewLbl->setText("Stream fermato.");
}

void ProgrammazionePage::onWibyFfmpegFrame()
{
    if (!m_wibyFfmpegProc || !m_camPreviewLbl) return;
    m_wibyFfmpegBuf += m_wibyFfmpegProc->readAllStandardOutput();

    /* Estrai tutti i frame JPEG completi dal buffer (FF D8 ... FF D9) */
    while (true) {
        int soi = -1;
        for (int i = 0; i + 1 < m_wibyFfmpegBuf.size(); ++i) {
            if ((unsigned char)m_wibyFfmpegBuf[i]   == 0xFF &&
                (unsigned char)m_wibyFfmpegBuf[i+1] == 0xD8) {
                soi = i; break;
            }
        }
        if (soi < 0) { m_wibyFfmpegBuf.clear(); break; }
        if (soi > 0)  m_wibyFfmpegBuf = m_wibyFfmpegBuf.mid(soi);

        int eoi = -1;
        for (int i = 2; i + 1 < m_wibyFfmpegBuf.size(); ++i) {
            if ((unsigned char)m_wibyFfmpegBuf[i]   == 0xFF &&
                (unsigned char)m_wibyFfmpegBuf[i+1] == 0xD9) {
                eoi = i + 2; break;
            }
        }
        if (eoi < 0) break; /* frame incompleto — aspetta altri dati */

        QImage img;
        if (img.loadFromData(
                reinterpret_cast<const uchar*>(m_wibyFfmpegBuf.constData()),
                eoi, "JPEG")) {
            m_camPreviewLbl->setPixmap(
                QPixmap::fromImage(img).scaled(
                    m_camPreviewLbl->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
        }
        m_wibyFfmpegBuf = m_wibyFfmpegBuf.mid(eoi);
    }
}

void ProgrammazionePage::onWibyMitmStartClicked()
{
    if (m_wibyMitmProc &&
        m_wibyMitmProc->state() != QProcess::NotRunning) return;

    if (!m_wibyMitmProc) {
        m_wibyMitmProc = new QProcess(this);
        connect(m_wibyMitmProc, &QProcess::readyReadStandardOutput,
                this, &ProgrammazionePage::onWibyMitmOutput);
        connect(m_wibyMitmProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int, QProcess::ExitStatus){
            if (m_wibyMitmStatus)
                m_wibyMitmStatus->setText("\xe2\x97\x8f  Fermato");
            m_wibyMitmProc = nullptr;
        });
    }
    const QString script = P::root() + "/Tools/scripts/wiby_mitm.py";
    m_wibyMitmProc->start("pkexec", {"python3", script});

    if (m_wibyMitmStatus)
        m_wibyMitmStatus->setText(
            "<span style='color:#fbbf24;'>\xe2\x97\x8f  Avvio...</span>");
}

void ProgrammazionePage::onWibyMitmStopClicked()
{
    if (!m_wibyMitmProc) return;
    m_wibyMitmProc->write("stop\n");
    m_wibyMitmProc->waitForFinished(3000);
    if (m_wibyMitmProc &&
        m_wibyMitmProc->state() != QProcess::NotRunning)
        m_wibyMitmProc->terminate();
    if (m_wibyMitmStatus)
        m_wibyMitmStatus->setText("\xe2\x97\x8f  Inattivo");
}

void ProgrammazionePage::onWibyMitmOutput()
{
    if (!m_wibyMitmProc || !m_usbOutput) return;
    while (m_wibyMitmProc->canReadLine()) {
        QByteArray raw = m_wibyMitmProc->readLine().trimmed();
        if (raw.isEmpty()) continue;
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (doc.isNull()) {
            m_usbOutput->append(QString::fromUtf8(raw));
            continue;
        }
        QJsonObject o = doc.object();
        const QString ev = o.value("event").toString();

        if (ev == "ready") {
            const QString ssid = o.value("ssid").toString();
            const QString ip   = o.value("hotspot_ip").toString();
            const QString pwd  = o.value("password").toString();
            if (m_wibyMitmStatus)
                m_wibyMitmStatus->setText(
                    QString("<span style='color:#4ade80;'>"
                            "\xe2\x97\x8f  Attivo &mdash; SSID: <b>%1</b> "
                            "(%2) IP: %3</span>")
                    .arg(ssid, pwd, ip));
            m_usbOutput->append(
                QString("<b style='color:#4ade80;'>Hotspot attivo!</b> "
                        "Connetti la WIBY a '<b>%1</b>' (pwd: %2) "
                        "poi apri Smart Life.")
                .arg(ssid, pwd));
        } else if (ev == "status") {
            m_usbOutput->append(o.value("msg").toString());
        } else if (ev == "dns") {
            const QString d = o.value("domain").toString();
            /* Mostra solo domini Tuya rilevanti */
            if (d.contains("tuya") || d.contains("iot-11"))
                m_usbOutput->append(
                    "<span style='color:#60a5fa;'>DNS: " + d + "</span>");
        } else if (ev == "http") {
            m_usbOutput->append(
                "<span style='color:#a78bfa;'>"
                + o.value("method").toString() + " "
                + o.value("url").toString() + "</span>");
        } else if (ev == "udp") {
            const int size = o.value("size").toInt();
            /* Evidenzia pacchetti UDP grandi (frame video P2P) */
            if (size > 500) {
                m_usbOutput->append(
                    QString("<span style='color:#f59e0b;'>"
                            "UDP P2P %1 → %2 [<b>%3 B</b>]</span>")
                    .arg(o.value("src").toString(),
                         o.value("dst").toString())
                    .arg(size));
            }
        } else if (ev == "error") {
            m_usbOutput->append(
                "<span style='color:#f87171;'>MITM: "
                + o.value("msg").toString() + "</span>");
        }
    }
}

void ProgrammazionePage::onWibyFirmwareGuide()
{
    if (!m_usbOutput) return;
    m_usbOutput->clear();
    m_usbOutput->append(
    "<b style='font-size:13px;color:#60a5fa;'>"
    "Guida: firmware offline per WIBY JS-P161 (OpenIPC + UART)"
    "</b><br>"

    "<b style='color:#fbbf24;'>Obiettivo</b><br>"
    "Sostituire il firmware Tuya con OpenIPC: RTSP locale attivo, "
    "nessun cloud, PTZ via ONVIF, funziona completamente offline.<br><br>"

    "<b style='color:#fbbf24;'>Passo 1 — Apri la camera</b><br>"
    "Rimuovi il supporto rotante dal basso (viti a stella piccole). "
    "Separa delicatamente la scocca. Non forzare: ci sono clip laterali.<br><br>"

    "<b style='color:#fbbf24;'>Passo 2 — Identifica il chip SoC</b><br>"
    "Fotografa il quadratino nero pi\xc3\xb9 grande sulla scheda verde. "
    "Leggi il numero stampato sopra (es. <code>T31L</code>, "
    "<code>Hi3518EV300</code>, <code>SSC335</code>, <code>GK7205V200</code>). "
    "Mandami la foto — ti dico subito se OpenIPC lo supporta.<br><br>"

    "<b style='color:#fbbf24;'>Passo 3 — Collegati via UART</b><br>"
    "Sulla scheda trovi 3-4 pad etichettati <code>TX RX GND</code> "
    "(puntini d'oro vicino al bordo). Hai bisogno di un adattatore "
    "<b>USB-UART CH340</b> o <b>CP2102</b> (2-5\xe2\x82\xac su Amazon).<br>"
    "Collegamento:<br>"
    "<code>&nbsp;&nbsp;adattatore TX &rarr; pad RX camera</code><br>"
    "<code>&nbsp;&nbsp;adattatore RX &rarr; pad TX camera</code><br>"
    "<code>&nbsp;&nbsp;adattatore GND &rarr; pad GND camera</code><br>"
    "<span style='color:#f87171;'>Non collegare il VCC dell'adattatore "
    "— la camera si alimenta dal cavo USB.</span><br><br>"

    "<b style='color:#fbbf24;'>Passo 4 — Accedi al boot loader</b><br>"
    "Collega la camera al PC via USB-UART, poi apri un terminale:<br>"
    "<code>&nbsp;&nbsp;sudo screen /dev/ttyUSB0 115200</code><br>"
    "Accendi la camera. Vedrai il boot log. Premi un tasto subito per "
    "interrompere il boot e ottenere la shell U-Boot o root.<br><br>"

    "<b style='color:#fbbf24;'>Passo 5 — Dump del firmware originale</b><br>"
    "Dalla shell U-Boot esegui il dump del flash SPI (solitamente 8 o 16 MB):<br>"
    "<code>&nbsp;&nbsp;sf probe 0; sf read 0x82000000 0x0 0x800000</code><br>"
    "<code>&nbsp;&nbsp;md.b 0x82000000 0x800000</code><br>"
    "Salva l'output: \xc3\xa8 il tuo backup. "
    "Senza backup non puoi tornare indietro.<br><br>"

    "<b style='color:#fbbf24;'>Passo 6 — Flash OpenIPC</b><br>"
    "Scarica il firmware per il tuo chip da "
    "<code>github.com/OpenIPC/firmware</code>.<br>"
    "Verifica che il chip sia nella lista supportati. Flash via TFTP:<br>"
    "<code>&nbsp;&nbsp;setenv serverip 192.168.1.100; tftp 0x82000000 openipc.bin</code><br>"
    "<code>&nbsp;&nbsp;sf probe 0; sf erase 0x0 0x800000</code><br>"
    "<code>&nbsp;&nbsp;sf write 0x82000000 0x0 0x800000</code><br>"
    "<code>&nbsp;&nbsp;reset</code><br><br>"

    "<b style='color:#fbbf24;'>Passo 7 — Dopo OpenIPC</b><br>"
    "La camera espone <code>rtsp://192.168.1.222:554/stream=0</code> "
    "senza password di default. Inserisci quell'URL nel campo qui sopra "
    "e premi <b>Avvia viewer</b> — funziona offline, senza Tuya.<br>"
    "PTZ via ONVIF: usa il joystick qui sopra (aggiorna il controller "
    "da Tuya a ONVIF).<br><br>"

    "<span style='color:#94a3b8;font-size:11px;'>"
    "Modello: WIBY JS-P161 &mdash; IP: 192.168.1.222 &mdash; "
    "Firmware attuale: v30.1.14 Tuya v3.3"
    "</span>"
    );
}

/* ══════════════════════════════════════════════════════════════
   VPN & Tunnel — slot
   ══════════════════════════════════════════════════════════════ */

static const char* kVpnTemplates[] = {
    "# WireGuard — configurazione client\n"
    "# Salva come /etc/wireguard/wg0.conf e usa: sudo wg-quick up wg0\n"
    "\n"
    "[Interface]\n"
    "Address = 10.0.0.2/24\n"
    "PrivateKey = <CHIAVE_PRIVATA_CLIENT>\n"
    "DNS = 1.1.1.1\n"
    "\n"
    "[Peer]\n"
    "PublicKey = <CHIAVE_PUBBLICA_SERVER>\n"
    "AllowedIPs = 0.0.0.0/0, ::/0\n"
    "Endpoint = <SERVER_IP>:51820\n"
    "PersistentKeepalive = 25\n"
    "\n"
    "# Genera le chiavi con:\n"
    "#   wg genkey | tee privatekey | wg pubkey > publickey\n",

    "# OpenVPN — configurazione client (.ovpn)\n"
    "# Usa: sudo openvpn --config client.ovpn\n"
    "\n"
    "client\n"
    "dev tun\n"
    "proto udp\n"
    "remote <SERVER_IP> 1194\n"
    "resolv-retry infinite\n"
    "nobind\n"
    "persist-key\n"
    "persist-tun\n"
    "cipher AES-256-CBC\n"
    "auth SHA256\n"
    "verb 3\n"
    "keepalive 10 120\n"
    "\n"
    "<ca>\n"
    "# Incolla il contenuto di ca.crt\n"
    "</ca>\n"
    "<cert>\n"
    "# Incolla il contenuto di client.crt\n"
    "</cert>\n"
    "<key>\n"
    "# Incolla il contenuto di client.key\n"
    "</key>\n",

    "# SSH Tunnel — esempi pronti all'uso\n"
    "\n"
    "# 1. Tunnel LOCALE: porta locale 8080 -> server:80\n"
    "ssh -N -L 8080:localhost:80 utente@<SERVER>\n"
    "\n"
    "# 2. Tunnel REMOTO: porta remota 9090 -> localhost:80\n"
    "ssh -N -R 9090:localhost:80 utente@<SERVER>\n"
    "\n"
    "# 3. SOCKS proxy dinamico (porta 1080)\n"
    "ssh -N -D 1080 -C utente@<SERVER>\n"
    "\n"
    "# 4. Sessione persistente con autossh\n"
    "#    sudo apt install autossh\n"
    "autossh -M 20000 -N -L 8080:localhost:80 utente@<SERVER>\n"
    "\n"
    "# Configura in /etc/ssh/sshd_config del server:\n"
    "#   AllowTcpForwarding yes\n"
    "#   GatewayPorts yes       # solo per tunnel remoti\n",

    "#!/bin/bash\n"
    "# Wi-Fi Hotspot con NetworkManager\n"
    "# Adatta SSID, password e interfaccia (wlan0 / wlp3s0)\n"
    "\n"
    "IFACE=\"wlan0\"\n"
    "SSID=\"PrismaluxNet\"\n"
    "PASSWORD=\"password_sicura\"\n"
    "CON_NAME=\"PrismaluxAP\"\n"
    "\n"
    "nmcli con add type wifi ifname \"$IFACE\" con-name \"$CON_NAME\" \\\n"
    "  ssid \"$SSID\" mode ap\n"
    "\n"
    "nmcli con modify \"$CON_NAME\" \\\n"
    "  wifi.band bg wifi.channel 6 \\\n"
    "  wifi-sec.key-mgmt wpa-psk wifi-sec.psk \"$PASSWORD\"\n"
    "\n"
    "nmcli con modify \"$CON_NAME\" ipv4.method shared\n"
    "nmcli con up \"$CON_NAME\"\n"
    "echo \"Hotspot '$SSID' attivo su $IFACE\"\n"
    "\n"
    "# Per fermare:\n"
    "# nmcli con down \"$CON_NAME\"\n",

    /* idx 4 — n2n Supernode */
    "#!/bin/bash\n"
    "# n2n Supernode — nodo centrale WAN Prismalux (scritto in C)\n"
    "# Installa: sudo apt install n2n\n"
    "# Avvia sul server: sudo bash /tmp/prismalux_n2n_supernode.sh\n"
    "# Apri nel firewall: sudo ufw allow 7654/udp\n"
    "\n"
    "SUPERNODE_PORT=7654\n"
    "\n"
    "if ! command -v supernode &>/dev/null; then\n"
    "    echo '[n2n] Installo n2n...'\n"
    "    apt-get install -y n2n\n"
    "fi\n"
    "\n"
    "echo \"[Prismalux] Avvio supernode n2n su UDP $SUPERNODE_PORT...\"\n"
    "supernode -p \"$SUPERNODE_PORT\" -v\n"
    "\n"
    "# Dopo aver avviato il supernode, connetti anche questo host come edge\n"
    "# (IP server nella VPN = 10.10.0.1):\n"
    "# sudo edge -c prismalux_wan -k <PSK> -a 10.10.0.1/24 -l localhost:$SUPERNODE_PORT -f\n",

    /* idx 5 — n2n Edge (worker WAN) */
    "#!/bin/bash\n"
    "# n2n Edge — worker WAN Prismalux (esegui su ogni nodo remoto)\n"
    "# Installa: sudo apt install n2n\n"
    "# Avvia: sudo bash prismalux_n2n_edge.sh\n"
    "# Clicca 'Genera chiavi n2n' per riempire COMMUNITY e PSK automaticamente.\n"
    "\n"
    "SUPERNODE_IP=\"<SERVER_IP>\"\n"
    "SUPERNODE_PORT=7654\n"
    "COMMUNITY=\"prismalux_wan\"\n"
    "PSK=\"<CHIAVE_CONDIVISA_AES256>\"\n"
    "EDGE_IP=\"10.10.0.2/24\"\n"
    "# Cambia EDGE_IP per ogni worker: 10.10.0.3/24, 10.10.0.4/24 ...\n"
    "\n"
    "if ! command -v edge &>/dev/null; then\n"
    "    echo '[n2n] Installo n2n...'\n"
    "    apt-get install -y n2n\n"
    "fi\n"
    "\n"
    "echo \"[Prismalux] Connessione al supernode $SUPERNODE_IP:$SUPERNODE_PORT...\"\n"
    "edge -c \"$COMMUNITY\" \\\n"
    "     -k \"$PSK\" \\\n"
    "     -a \"$EDGE_IP\" \\\n"
    "     -l \"$SUPERNODE_IP:$SUPERNODE_PORT\" \\\n"
    "     -f\n"
    "\n"
    "# Il WAN Compute Prismalux (porta 11600) sara' raggiungibile via VPN:\n"
    "#   Server: 10.10.0.1:11600  |  Questo nodo: 10.10.0.2\n",
};

void ProgrammazionePage::onVpnTypeChanged(int idx)
{
    if (!m_vpnConfig) return;
    if (idx >= 0 && idx < 6)
        m_vpnConfig->setPlainText(QString::fromUtf8(kVpnTemplates[idx]));
    if (m_vpnGenKeysBtn)
        m_vpnGenKeysBtn->setVisible(idx >= 4);
}

void ProgrammazionePage::onVpnGenN2nKeys()
{
    if (!m_vpnConfig || !m_vpnLog) return;

    const QString alphanum = QStringLiteral("abcdefghijklmnopqrstuvwxyz0123456789");
    QString community = QStringLiteral("plx_");
    for (int i = 0; i < 8; ++i)
        community += alphanum[QRandomGenerator::global()->bounded(alphanum.size())];

    QString psk;
    for (int i = 0; i < 32; ++i)
        psk += QString::number(QRandomGenerator::global()->bounded(16), 16);

    QString cfg = m_vpnConfig->toPlainText();
    cfg.replace(QStringLiteral("prismalux_wan"), community);
    cfg.replace(QStringLiteral("<CHIAVE_CONDIVISA_AES256>"), psk);
    m_vpnConfig->setPlainText(cfg);

    m_vpnLog->append(
        QString("<span style='color:#4ade80;'>"
                "\xf0\x9f\x94\x91  Chiavi n2n generate:<br>"
                "&nbsp; Community: <b>%1</b><br>"
                "&nbsp; PSK&nbsp;&nbsp;&nbsp;&nbsp;: <b>%2</b><br>"
                "Usa <b>le stesse</b> su ogni nodo edge e sul server.</span>")
            .arg(community.toHtmlEscaped())
            .arg(psk.toHtmlEscaped()));

    LogBus::post(QString("\xf0\x9f\x94\x91 n2n — chiavi generate. Community: %1").arg(community));
}

void ProgrammazionePage::onVpnGenerateClicked()
{
    if (!m_vpnConfig || !m_vpnLog || !m_vpnStatusLbl) return;

    const QString config  = m_vpnConfig->toPlainText().trimmed();
    const QString tipoPkg = m_vpnTypeCombo
        ? m_vpnTypeCombo->currentText() : QStringLiteral("VPN");

    m_vpnStatusLbl->setText(tr("\xf0\x9f\xa4\x96  AI in elaborazione..."));
    m_vpnLog->append(
        QString("<span style='color:#94a3b8;'>\xf0\x9f\xa4\x96  "
                "Invio configurazione %1 all'AI...</span>").arg(tipoPkg));

    const QString sys =
        "Sei un esperto di reti e sicurezza informatica su Linux. "
        "Analizza la configurazione " + tipoPkg + " che ti fornisco, "
        "identifica eventuali problemi di sicurezza o configurazioni mancanti, "
        "e restituisci una versione migliorata e completa con commenti esplicativi. "
        "Rispondi con solo il file di configurazione migliorato, senza spiegazioni aggiuntive.";

    disconnect(m_vpnAiTokenConn);
    disconnect(m_vpnAiFinishedConn);
    disconnect(m_vpnAiErrorConn);

    m_vpnConfig->clear();

    m_vpnAiTokenConn = connect(m_ai, &AiClient::token,
                               this, &ProgrammazionePage::onVpnAiToken);
    m_vpnAiFinishedConn = connect(m_ai, &AiClient::finished,
                                  this, &ProgrammazionePage::onVpnAiFinished);
    m_vpnAiErrorConn = connect(m_ai, &AiClient::error,
                               this, &ProgrammazionePage::onVpnAiError);

    m_ai->chat(sys, config);
}

void ProgrammazionePage::onVpnAiToken(const QString& t)
{
    if (!m_vpnConfig) return;
    QTextCursor c = m_vpnConfig->textCursor();
    c.movePosition(QTextCursor::End);
    c.insertText(t);
    m_vpnConfig->setTextCursor(c);
}

void ProgrammazionePage::onVpnAiFinished(const QString& /*full*/)
{
    disconnect(m_vpnAiTokenConn);
    disconnect(m_vpnAiFinishedConn);
    disconnect(m_vpnAiErrorConn);
    if (m_vpnStatusLbl)
        m_vpnStatusLbl->setText(tr("\xe2\x9c\x85  Config aggiornata dall'AI"));
    m_vpnLog->append(
        "<span style='color:#4ade80;'>\xe2\x9c\x85  Configurazione migliorata dall'AI.</span>");
}

void ProgrammazionePage::onVpnAiError(const QString& msg)
{
    disconnect(m_vpnAiTokenConn);
    disconnect(m_vpnAiFinishedConn);
    disconnect(m_vpnAiErrorConn);
    if (m_vpnStatusLbl)
        m_vpnStatusLbl->setText(tr("\xe2\x9d\x8c  Errore AI"));
    m_vpnLog->append(
        "<span style='color:#f87171;'>\xe2\x9d\x8c  " + msg.toHtmlEscaped() + "</span>");
}

/* Rileva le interfacce VPN attive (nomi wg, tun, tap, ppp, n2n) e il loro IPv4,
   usando QNetworkInterface: nessun processo esterno, nessun privilegio root. */
void ProgrammazionePage::vpnRefreshStatus()
{
    if (!m_vpnLiveStatusLbl) return;

    QStringList active;
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& ni : ifaces) {
        const QString n = ni.name();
        const bool isVpn = n.startsWith("wg")  || n.startsWith("tun") ||
                           n.startsWith("tap") || n.startsWith("ppp") || n.startsWith("n2n");
        if (!isVpn) continue;
        const auto flags = ni.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning))
            continue;
        QString ip;
        for (const QNetworkAddressEntry& e : ni.addressEntries()) {
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                ip = e.ip().toString();
                break;
            }
        }
        active << QString("%1 (%2)").arg(n, ip.isEmpty() ? tr("nessun IP") : ip);
    }

    if (active.isEmpty()) {
        m_vpnLiveStatusLbl->setText(tr("\xe2\x9a\xaa  Nessuna VPN attiva"));
        m_vpnLiveStatusLbl->setStyleSheet("color:#94a3b8;");
    } else {
        m_vpnLiveStatusLbl->setText(QString::fromUtf8("\xf0\x9f\x9f\xa2  ") + active.join(", "));
        m_vpnLiveStatusLbl->setStyleSheet("color:#22c55e;");
    }
}

void ProgrammazionePage::onVpnTestClicked()
{
    vpnRefreshStatus();
    if (m_vpnLog && m_vpnLiveStatusLbl)
        m_vpnLog->append(QString::fromUtf8("\xf0\x9f\x94\x8d  ") +
                         tr("Stato VPN: ") + m_vpnLiveStatusLbl->text());
}

void ProgrammazionePage::onVpnApplyClicked()
{
    if (!m_vpnConfig || !m_vpnLog || !m_vpnTypeCombo) return;

    const int    idx    = m_vpnTypeCombo->currentIndex();
    const QString cfg   = m_vpnConfig->toPlainText().trimmed();

    if (cfg.isEmpty()) {
        m_vpnLog->append("<span style='color:#f87171;'>\xe2\x9d\x8c  Configurazione vuota.</span>");
        return;
    }

    /* SSH: copia il comando negli appunti e suggerisce di eseguirlo in terminale */
    if (idx == 2) {
        qApp->clipboard()->setText(cfg);
        m_vpnLog->append(
            "\xf0\x9f\x93\x8b  Comandi SSH copiati negli appunti.<br>"
            "<span style='color:#94a3b8;'>Incolla in un terminale per eseguirli.</span>");
        if (m_vpnStatusLbl)
            m_vpnStatusLbl->setText(tr("\xf0\x9f\x93\x8b  Copiato negli appunti"));
        return;
    }

    /* n2n Edge: gira sul nodo remoto — copia script negli appunti */
    if (idx == 5) {
        if (cfg.contains("<SERVER_IP>") || cfg.contains("<CHIAVE_CONDIVISA_AES256>")) {
            m_vpnLog->append(
                "<span style='color:#f87171;'>\xe2\x9d\x8c  "
                "Sostituisci &lt;SERVER_IP&gt; e genera le chiavi prima di procedere.</span>");
            LogBus::post("\xe2\x9d\x8c VPN n2n edge: configura SERVER_IP e chiavi prima di applicare.");
            return;
        }
        qApp->clipboard()->setText(cfg);
        m_vpnLog->append(
            "\xf0\x9f\x93\x8b  Script edge n2n copiato negli appunti.<br>"
            "<span style='color:#94a3b8;'>Incolla e avvia con "
            "<b>sudo bash</b> su ogni nodo worker remoto.</span>");
        if (m_vpnStatusLbl)
            m_vpnStatusLbl->setText(tr("\xf0\x9f\x93\x8b  Copiato negli appunti"));
        return;
    }

    /* Determina file temporaneo e comando */
    QString tmpPath, cmd;
    QStringList args;

    if (idx == 0) {
        /* WireGuard */
        tmpPath = "/tmp/prismalux_wg0.conf";
        cmd = "bash";
        args = {"-c",
                "cp " + tmpPath + " /etc/wireguard/wg0.conf && "
                "wg-quick up wg0 && echo 'WireGuard attivato.'"};
    } else if (idx == 1) {
        /* OpenVPN */
        tmpPath = "/tmp/prismalux_client.ovpn";
        cmd = "openvpn";
        args = {"--config", tmpPath};
    } else if (idx == 4) {
        /* n2n Supernode — avvia localmente con pkexec */
        tmpPath = "/tmp/prismalux_n2n_supernode.sh";
        cmd = "bash";
        args = {tmpPath};
    } else {
        /* Hotspot script bash (idx == 3) */
        tmpPath = "/tmp/prismalux_hotspot.sh";
        cmd = "bash";
        args = {tmpPath};
    }

    /* Scrivi il file temporaneo */
    QFile tf(tmpPath);
    if (!tf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_vpnLog->append(
            "<span style='color:#f87171;'>\xe2\x9d\x8c  Impossibile scrivere "
            + tmpPath.toHtmlEscaped() + "</span>");
        return;
    }
    tf.write(cfg.toUtf8());
    tf.close();

    m_vpnLog->append(
        QString("<span style='color:#94a3b8;'>\xe2\x96\xb6  pkexec %1 %2</span>")
            .arg(cmd).arg(args.join(" ")));

    if (!m_vpnProc) {
        m_vpnProc = new QProcess(this);
        m_vpnProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_vpnProc, &QProcess::readyRead,
                this, &ProgrammazionePage::onVpnProcReadyRead);
        connect(m_vpnProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &ProgrammazionePage::onVpnProcFinished);
    }

    QStringList pkArgs = {cmd};
    pkArgs += args;
    m_vpnProc->start("pkexec", pkArgs);
    if (m_vpnStatusLbl) m_vpnStatusLbl->setText(tr("\xf0\x9f\x94\x84  Esecuzione in corso..."));
}

void ProgrammazionePage::onVpnStopClicked()
{
    m_ai->abort();
    if (m_vpnProc && m_vpnProc->state() != QProcess::NotRunning) {
        m_vpnProc->terminate();
        m_vpnProc->waitForFinished(2000);
    }
    m_vpnValidating = false;
    if (m_vpnValidateBtn) m_vpnValidateBtn->setEnabled(true);
    if (m_vpnStatusLbl) m_vpnStatusLbl->setText(tr("\xe2\x8f\xb9  Fermato"));
}

/* ── Valida config — simulazione senza root ── */
void ProgrammazionePage::onVpnValidateClicked()
{
    if (!m_vpnConfig || !m_vpnLog || !m_vpnTypeCombo) return;
    if (m_vpnProc && m_vpnProc->state() != QProcess::NotRunning) {
        m_vpnLog->append(
            "<span style='color:#f87171;'>\xe2\x9d\x8c  "
            "Un processo VPN e' gia' in esecuzione.</span>");
        return;
    }

    const QString cfg = m_vpnConfig->toPlainText().trimmed();
    if (cfg.isEmpty()) {
        m_vpnLog->append(
            "<span style='color:#f87171;'>\xe2\x9d\x8c  Configurazione vuota.</span>");
        return;
    }

    /* Scrivi config in file temp */
    const QString tmpCfg = QStringLiteral("/tmp/prismalux_vpn_validate.cfg");
    QFile fc(tmpCfg);
    if (!fc.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_vpnLog->append(
            "<span style='color:#f87171;'>\xe2\x9d\x8c  "
            "Impossibile creare il file temporaneo.</span>");
        return;
    }
    fc.write(cfg.toUtf8());
    fc.close();

    /* Tipo corrente come stringa */
    static const char* kTypes[] = {
        "wireguard", "openvpn", "ssh", "hotspot", "n2n_supernode", "n2n_edge"
    };
    const int idx = m_vpnTypeCombo->currentIndex();
    const QString type = (idx >= 0 && idx < 6)
        ? QString::fromUtf8(kTypes[idx]) : QStringLiteral("unknown");

    /* Script bash di validazione (no root) */
    const QString script =
        "#!/bin/bash\n"
        "CFG=\"/tmp/prismalux_vpn_validate.cfg\"\n"
        "TYPE=\"" + type + "\"\n"
        "PASS=0; FAIL=0; WARN=0\n"
        "ok()   { echo \"[OK]  $1\"; ((PASS++)); }\n"
        "err()  { echo \"[ERR] $1\"; ((FAIL++)); }\n"
        "warn() { echo \"[WRN] $1\"; ((WARN++)); }\n"
        "\n"
        "# 1. Placeholder\n"
        "PH=$(grep -oP '<[A-Z_]+>' \"$CFG\" | sort -u | tr '\\n' ' ')\n"
        "if [ -z \"$PH\" ]; then ok \"Nessun placeholder da sostituire\";\n"
        "else err \"Placeholder non sostituiti: $PH\"; fi\n"
        "\n"
        "# 2. Binario richiesto\n"
        "case \"$TYPE\" in\n"
        "  wireguard)     BIN=wg;         PKG=wireguard ;;\n"
        "  openvpn)       BIN=openvpn;    PKG=openvpn ;;\n"
        "  ssh)           BIN=ssh;        PKG=openssh-client ;;\n"
        "  hotspot)       BIN=nmcli;      PKG=network-manager ;;\n"
        "  n2n_supernode) BIN=supernode;  PKG=n2n ;;\n"
        "  n2n_edge)      BIN=edge;       PKG=n2n ;;\n"
        "  *)             BIN=; PKG= ;;\n"
        "esac\n"
        "if [ -n \"$BIN\" ]; then\n"
        "  if command -v \"$BIN\" &>/dev/null;\n"
        "  then ok \"Binario trovato: $(command -v $BIN)\";\n"
        "  else err \"Binario mancante: $BIN  (installa: sudo apt install $PKG)\"; fi\n"
        "fi\n"
        "\n"
        "# 3. Porta locale libera\n"
        "if [ \"$TYPE\" = \"n2n_supernode\" ]; then\n"
        "  if ss -lun 2>/dev/null | grep -q ':7654 ';\n"
        "  then err \"Porta UDP 7654 gia' in uso\";\n"
        "  else ok \"Porta UDP 7654 libera\"; fi\n"
        "fi\n"
        "if [ \"$TYPE\" = \"wireguard\" ]; then\n"
        "  PORT=$(grep -oP 'ListenPort\\s*=\\s*\\K\\d+' \"$CFG\" | head -1)\n"
        "  [ -z \"$PORT\" ] && PORT=51820\n"
        "  if ss -lun 2>/dev/null | grep -q \":$PORT \";\n"
        "  then warn \"Porta UDP $PORT gia' in uso (server locale)\"; fi\n"
        "fi\n"
        "\n"
        "# 4. Estrai IP server e testa raggiungibilita'\n"
        "SRV=\"\"\n"
        "case \"$TYPE\" in\n"
        "  wireguard)  SRV=$(grep -oP 'Endpoint\\s*=\\s*\\K[^:\\s]+' \"$CFG\" | head -1) ;;\n"
        "  openvpn)    SRV=$(grep -oP '^remote\\s+\\K\\S+' \"$CFG\" | head -1) ;;\n"
        "  n2n_edge)   SRV=$(grep -oP 'SUPERNODE_IP=\"?\\K[^\"\\s]+' \"$CFG\" | head -1) ;;\n"
        "esac\n"
        "if [ -n \"$SRV\" ] && [[ \"$SRV\" != *'<'* ]]; then\n"
        "  warn \"Test ping $SRV (timeout 2s)...\"\n"
        "  if ping -c 1 -W 2 \"$SRV\" &>/dev/null 2>&1;\n"
        "  then ok \"Server raggiungibile: $SRV\";\n"
        "  else warn \"Ping non risponde: $SRV (potrebbe essere filtrato dal firewall)\"; fi\n"
        "fi\n"
        "\n"
        "# 5. Lunghezza minima config\n"
        "LINES=$(wc -l < \"$CFG\")\n"
        "if [ \"$LINES\" -lt 3 ]; then\n"
        "  warn \"Config molto corta ($LINES righe) — controlla che sia completa\"\n"
        "fi\n"
        "\n"
        "echo \"\"\n"
        "echo \"--- Validazione: $PASS OK  |  $FAIL errori  |  $WARN avvisi ---\"\n"
        "[ \"$FAIL\" -eq 0 ] && exit 0 || exit 1\n";

    const QString scriptPath = QStringLiteral("/tmp/prismalux_vpn_check.sh");
    QFile sf(scriptPath);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_vpnLog->append(
            "<span style='color:#f87171;'>\xe2\x9d\x8c  "
            "Impossibile creare lo script di validazione.</span>");
        return;
    }
    sf.write(script.toUtf8());
    sf.close();

    m_vpnValidating = true;
    if (m_vpnValidateBtn) m_vpnValidateBtn->setEnabled(false);
    m_vpnLog->append(
        "<span style='color:#94a3b8;'>\xf0\x9f\x94\x8d  "
        "Validazione in corso (senza root)...</span>");
    if (m_vpnStatusLbl) m_vpnStatusLbl->setText(tr("\xf0\x9f\x94\x8d  Validazione..."));
    LogBus::post(QString("\xf0\x9f\x94\x8d VPN — validazione avviata (%1)").arg(type));

    if (!m_vpnProc) {
        m_vpnProc = new QProcess(this);
        m_vpnProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_vpnProc, &QProcess::readyRead,
                this, &ProgrammazionePage::onVpnProcReadyRead);
        connect(m_vpnProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &ProgrammazionePage::onVpnProcFinished);
    }
    m_vpnProc->start(QStringLiteral("bash"), {scriptPath});
}

/* ── Importa config da file (DontUseNativeDialog evita crash Dolphin) ── */
void ProgrammazionePage::onVpnImportClicked()
{
    if (!m_vpnConfig || !m_vpnTypeCombo) return;

    static const char* kFilters[] = {
        "Config WireGuard (*.conf);;Tutti i file (*)",
        "Config OpenVPN (*.ovpn *.conf);;Tutti i file (*)",
        "Script shell (*.sh);;Tutti i file (*)",
        "Script shell (*.sh);;Tutti i file (*)",
        "Script shell (*.sh);;Tutti i file (*)",
        "Script shell (*.sh);;Tutti i file (*)",
    };
    const int idx = m_vpnTypeCombo->currentIndex();
    const QString filter = QString::fromUtf8(
        (idx >= 0 && idx < 6) ? kFilters[idx] : kFilters[2]);

    QFileDialog dlg(this, tr("Importa configurazione VPN"));
    dlg.setOption(QFileDialog::DontUseNativeDialog);   // evita crash Dolphin KDE
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setNameFilter(filter);

    if (dlg.exec() != QDialog::Accepted) return;
    const QStringList files = dlg.selectedFiles();
    if (files.isEmpty()) return;

    QFile f(files.first());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (m_vpnLog)
            m_vpnLog->append(
                "<span style='color:#f87171;'>\xe2\x9d\x8c  "
                "Impossibile aprire: " + files.first().toHtmlEscaped() + "</span>");
        return;
    }
    m_vpnConfig->setPlainText(QString::fromUtf8(f.readAll()));
    if (m_vpnLog)
        m_vpnLog->append(
            QString("<span style='color:#4ade80;'>\xf0\x9f\x93\x82  "
                    "Importato: %1</span>").arg(files.first().toHtmlEscaped()));
    LogBus::post("\xf0\x9f\x93\x82 VPN — config importata: " + files.first());
}

void ProgrammazionePage::onVpnProcReadyRead()
{
    if (!m_vpnProc || !m_vpnLog) return;
    const QString out = QString::fromUtf8(m_vpnProc->readAll()).trimmed();
    if (!out.isEmpty()) m_vpnLog->append(out.toHtmlEscaped());
}

void ProgrammazionePage::onVpnProcFinished(int code, QProcess::ExitStatus /*status*/)
{
    if (!m_vpnStatusLbl || !m_vpnLog) return;

    if (m_vpnValidating) {
        m_vpnValidating = false;
        if (m_vpnValidateBtn) m_vpnValidateBtn->setEnabled(true);
        if (code == 0) {
            m_vpnStatusLbl->setText(tr("\xe2\x9c\x85  Valida"));
            m_vpnLog->append(
                "<span style='color:#4ade80;'>\xe2\x9c\x85  "
                "<b>Configurazione valida</b> — pronta per essere applicata.</span>");
            LogBus::post("\xe2\x9c\x85 VPN — validazione superata");
        } else {
            m_vpnStatusLbl->setText(tr("\xe2\x9d\x8c  Errori trovati"));
            m_vpnLog->append(
                "<span style='color:#f87171;'>\xe2\x9d\x8c  "
                "<b>Trovati errori</b> — correggi prima di applicare.</span>");
            LogBus::post("\xe2\x9d\x8c VPN — validazione fallita");
        }
        return;
    }

    if (code == 0) {
        m_vpnStatusLbl->setText(tr("\xe2\x9c\x85  Completato"));
        m_vpnLog->append(
            "<span style='color:#4ade80;'>\xe2\x9c\x85  Comando completato correttamente.</span>");
    } else {
        m_vpnStatusLbl->setText(
            QString("\xe2\x9d\x8c  Uscito con codice %1").arg(code));
        m_vpnLog->append(
            QString("<span style='color:#f87171;'>\xe2\x9d\x8c  "
                    "Comando uscito con codice %1.</span>").arg(code));
    }
}


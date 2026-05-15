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
   ====================================================================== */
#include "programmazione_page.h"
#include "../prismalux_paths.h"

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
#include <QSlider>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTextCursor>
#include <QCheckBox>
#include <QSettings>
#include <QTextEdit>
#include <QTimer>

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
    dlg.setWindowTitle("Loop Fix — Esecuzione automatica di codice AI");
    dlg.setIcon(QMessageBox::Warning);
    dlg.setText(
        "<b>Loop Fix eseguir\xc3\xa0 automaticamente il codice</b><br>"
        "generato dall'AI con i tuoi privilegi utente.<br><br>"
        "Assicurati di usare questa funzione <b>solo con AI di cui ti fidi</b> "
        "e con codice che non accede a dati sensibili o al filesystem.<br><br>"
        "Il loop si ferma automaticamente dopo il numero di tentativi indicato dallo slider "
        "o se rileva un errore intenzionale (SyntaxError custom).");
    dlg.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    dlg.button(QMessageBox::Ok)->setText("Ho capito, abilita");
    dlg.button(QMessageBox::Cancel)->setText("Annulla");

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
    if (m_status) m_status->setText("Pronto.");
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

    if (m_tokenHolder) { delete m_tokenHolder; m_tokenHolder = nullptr; }
    m_tokenHolder = new QObject(this);

    connect(m_ai, &AiClient::token, m_tokenHolder,
            [this](const QString& tok){ onAiToken(tok); });
    connect(m_ai, &AiClient::finished, m_tokenHolder,
            [this](const QString& full){ onAiFinished(full); });
    connect(m_ai, &AiClient::error, m_tokenHolder,
            [this](const QString& msg){ onAiError(msg); });

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
        if (m_status) m_status->setText("Esecuzione interrotta.");
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
    if (!m_ai) return;
    auto* h = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, h,
        [this, h](const QStringList& models) {
            h->deleteLater();
            if (!m_modelCombo) return;
            const QString cur = m_modelCombo->currentData().toString();
            m_modelCombo->blockSignals(true);
            m_modelCombo->clear();
            for (const QString& m : models)
                m_modelCombo->addItem(P::modelIcon(0, m) + m, m);
            int idx = m_modelCombo->findData(cur);
            if (idx < 0) idx = 0;
            m_modelCombo->setCurrentIndex(idx);
            m_modelCombo->blockSignals(false);
        });
    m_ai->fetchModels();
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
        if (m_status) m_status->setText("\xe2\x9c\x85  Completato con successo.");
        tryShowChart();
        m_lastError.clear();
        m_loopActive = false;
        m_loopCount  = 0;
    } else {
        m_lastError = m_fullOutput.right(2000);
        if (m_status)
            m_status->setText(QString("\xe2\x9d\x8c  Errore (exit %1).").arg(code));

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
    if (m_tokenHolder) { m_tokenHolder->deleteLater(); m_tokenHolder = nullptr; }
    m_aiMode = false;
    setRunning(false);
    if (m_btnInsert) m_btnInsert->setEnabled(!extractCodeBlock().isEmpty());
}

void ProgrammazionePage::onAiError(const QString& msg)
{
    if (m_tokenHolder) { m_tokenHolder->deleteLater(); m_tokenHolder = nullptr; }
    m_aiMode = false;
    setRunning(false);
    if (m_aiOutput) {
        m_aiOutput->moveCursor(QTextCursor::End);
        m_aiOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore AI: %1").arg(msg));
    }
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
    if (m_tokenHolder) { m_tokenHolder->deleteLater(); m_tokenHolder = nullptr; }

    /* Ripristina il modello originale */
    if (!m_fixOriginalModel.isEmpty() && m_fixOriginalModel != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_fixOriginalModel);

    m_aiMode = false;
    setRunning(false);

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
    if (m_tokenHolder) { m_tokenHolder->deleteLater(); m_tokenHolder = nullptr; }

    /* Ripristina il modello originale */
    if (m_ai && !m_fixOriginalModel.isEmpty() && m_fixOriginalModel != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_fixOriginalModel);

    m_aiMode = false;
    m_loopActive = false;
    setRunning(false);
    if (m_aiOutput) {
        m_aiOutput->moveCursor(QTextCursor::End);
        m_aiOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore Fix AI: %1").arg(msg));
    }
}

/* ======================================================================
   Sezione 5 — Agentica slots
   ====================================================================== */

void ProgrammazionePage::populateAgentModels()
{
    if (!m_ai || !m_agentModel) return;
    auto* h = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, h,
        [this, h](const QStringList& models) {
            h->deleteLater();
            if (!m_agentModel) return;
            const QString cur = m_agentModel->currentData().toString();
            m_agentModel->blockSignals(true);
            m_agentModel->clear();
            for (const QString& m : models)
                m_agentModel->addItem(P::modelIcon(0, m) + m, m);
            int idx = m_agentModel->findData(cur);
            if (idx < 0) idx = 0;
            m_agentModel->setCurrentIndex(idx);
            m_agentModel->blockSignals(false);
        });
    m_ai->fetchModels();
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
    if (m_agentTokenHolder) { m_agentTokenHolder->deleteLater(); m_agentTokenHolder = nullptr; }
    if (m_btnAgentRun)    m_btnAgentRun->setEnabled(true);
    if (m_btnAgentStop)   m_btnAgentStop->setEnabled(false);
    const bool hasCode = full.contains("```");
    if (m_btnAgentInsert) m_btnAgentInsert->setEnabled(hasCode);
}

void ProgrammazionePage::onAgentError(const QString& msg)
{
    if (m_agentTokenHolder) { m_agentTokenHolder->deleteLater(); m_agentTokenHolder = nullptr; }
    if (m_btnAgentRun)  m_btnAgentRun->setEnabled(true);
    if (m_btnAgentStop) m_btnAgentStop->setEnabled(false);
    if (m_agentOutput) {
        m_agentOutput->moveCursor(QTextCursor::End);
        m_agentOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    }
}

/* ======================================================================
   Sezione 6 — Reverse Engineering slots
   ====================================================================== */

void ProgrammazionePage::populateRevModels()
{
    if (!m_ai || !m_revModel) return;
    auto* h = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, h,
        [this, h](const QStringList& models) {
            h->deleteLater();
            if (!m_revModel) return;
            const QString cur = m_revModel->currentData().toString();
            m_revModel->blockSignals(true);
            m_revModel->clear();
            for (const QString& m : models)
                m_revModel->addItem(P::modelIcon(0, m) + m, m);
            int idx = m_revModel->findData(cur);
            if (idx < 0) idx = 0;
            m_revModel->setCurrentIndex(idx);
            m_revModel->blockSignals(false);
        });
    m_ai->fetchModels();
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
    if (m_revTokenHolder) { m_revTokenHolder->deleteLater(); m_revTokenHolder = nullptr; }
    if (m_btnRevAnalyze) m_btnRevAnalyze->setEnabled(true);
    if (m_btnRevStop)    m_btnRevStop->setEnabled(false);
    const bool hasCode = full.contains("```");
    if (m_btnRevInsert) m_btnRevInsert->setEnabled(hasCode);
}

void ProgrammazionePage::onRevError(const QString& msg)
{
    if (m_revTokenHolder) { m_revTokenHolder->deleteLater(); m_revTokenHolder = nullptr; }
    if (m_btnRevAnalyze) m_btnRevAnalyze->setEnabled(true);
    if (m_btnRevStop)    m_btnRevStop->setEnabled(false);
    if (m_revOutput) {
        m_revOutput->moveCursor(QTextCursor::End);
        m_revOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    }
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
    const QString err = QString::fromLocal8Bit(
        m_netProc->readAllStandardError());
    /* Filtra output verboso di tshark (inizializzazione interfaccia) */
    if (m_netLog && !err.contains("Capturing on") && !err.isEmpty()) {
        m_netLog->moveCursor(QTextCursor::End);
        m_netLog->insertPlainText(
            QString("[stderr] %1").arg(err));
        m_netLog->ensureCursorVisible();
    }
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
    if (m_netTokenHolder) { m_netTokenHolder->deleteLater(); m_netTokenHolder = nullptr; }
    if (m_btnNetAnalyze) m_btnNetAnalyze->setEnabled(true);
}

void ProgrammazionePage::onNetAiError(const QString& msg)
{
    if (m_netTokenHolder) { m_netTokenHolder->deleteLater(); m_netTokenHolder = nullptr; }
    if (m_btnNetAnalyze) m_btnNetAnalyze->setEnabled(true);
    if (m_netAiOutput) {
        m_netAiOutput->moveCursor(QTextCursor::End);
        m_netAiOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    }
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

    m_lanProc->start("arp", {"-n"});
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
            "\xe2\x9d\x8c  Impossibile eseguire 'arp'. "
            "Potrebbe essere necessario installare net-tools.");
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
    /* Parsing output di `arp -n`:
       Address          HWtype  HWaddress           Flags Mask            Iface
       192.168.1.1      ether   aa:bb:cc:dd:ee:ff   C                     eth0 */
    const QStringList lines = m_lanBuf.split('\n', Qt::SkipEmptyParts);
    int count = 0;
    static const QRegularExpression reArp(
        R"(^(\d+\.\d+\.\d+\.\d+)\s+\w+\s+([\da-fA-F:]{17})\s+\w+\s+\S*\s+(\S+))");
    for (const QString& line : lines) {
        const auto m = reArp.match(line.trimmed());
        if (!m.hasMatch()) continue;
        lanAddRow(m.captured(1), m.captured(2), m.captured(3), "online");
        ++count;
    }
    if (m_lanStatusLbl)
        m_lanStatusLbl->setText(
            count == 0
            ? "\xf0\x9f\x9f\xa1  ARP cache vuota (nessun host raggiunto di recente)."
            : QString("\xe2\x9c\x85  %1 host trovati nella ARP cache.").arg(count));
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
    /* Parsing output nmap -sn:
       Nmap scan report for 192.168.1.1 (hostname.local)
       Host is up (0.0040s latency).
       MAC Address: AA:BB:CC:DD:EE:FF (Vendor)
    */
    const QStringList lines = m_lanBuf.split('\n', Qt::SkipEmptyParts);
    int count = 0;
    QString ip, host, mac;
    for (const QString& line : lines) {
        const QString l = line.trimmed();
        if (l.startsWith("Nmap scan report for ")) {
            ip.clear(); host.clear(); mac.clear();
            const QString rest = l.mid(21);
            /* Cerca IP tra parentesi: "hostname (ip)" oppure solo "ip" */
            static const QRegularExpression reIP(R"((\d+\.\d+\.\d+\.\d+))");
            const auto m1 = reIP.match(rest);
            if (m1.hasMatch()) ip = m1.captured(1);
            static const QRegularExpression reHost(R"(^([^\s(]+))");
            const auto m2 = reHost.match(rest);
            if (m2.hasMatch() && m2.captured(1) != ip) host = m2.captured(1);
        } else if (l.startsWith("MAC Address:")) {
            static const QRegularExpression reMac(
                R"(MAC Address:\s+([\da-fA-F:]{17}))");
            const auto mm = reMac.match(l);
            if (mm.hasMatch()) mac = mm.captured(1);
        } else if (l.startsWith("Host is up") && !ip.isEmpty()) {
            lanAddRow(ip, mac, host, "online");
            ++count;
        }
    }
    if (m_lanStatusLbl)
        m_lanStatusLbl->setText(
            count == 0
            ? "\xf0\x9f\x9f\xa1  Nessun host trovato."
            : QString("\xe2\x9c\x85  %1 host online trovati con nmap.").arg(count));
    lanResetBtns();
    if (auto* p = qobject_cast<QProcess*>(sender())) { p->deleteLater(); m_lanProc = nullptr; }
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
    if (!m_ai || !m_gitAiModel) return;
    auto* h = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, h,
        [this, h](const QStringList& models) {
            h->deleteLater();
            if (!m_gitAiModel) return;
            const QString cur = m_gitAiModel->currentData().toString();
            m_gitAiModel->blockSignals(true);
            m_gitAiModel->clear();
            for (const QString& m : models)
                m_gitAiModel->addItem(P::modelIcon(0, m) + m, m);
            int idx = m_gitAiModel->findData(cur);
            if (idx < 0) idx = 0;
            m_gitAiModel->setCurrentIndex(idx);
            m_gitAiModel->blockSignals(false);
        });
    m_ai->fetchModels();
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
void ProgrammazionePage::onBtnGitDiffStagedClicked() { gitRun("diff", {"--staged"}); }
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
    if (m_gitTokenHolder) { m_gitTokenHolder->deleteLater(); m_gitTokenHolder = nullptr; }
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
    if (m_gitTokenHolder) { m_gitTokenHolder->deleteLater(); m_gitTokenHolder = nullptr; }
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
        m_replStatus->setText("\xe2\x9c\x85  Sessione attiva");
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
        m_replStatus->setText("\xe2\x9d\x8c  python3 non trovato");
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
    if (!m_ai || !m_trModel) return;
    connect(m_ai, &AiClient::modelsReady, m_trModel,
        [this](const QStringList& models) {
            const QString cur = m_trModel->currentData().toString();
            m_trModel->blockSignals(true);
            m_trModel->clear();
            for (const QString& m : models)
                m_trModel->addItem(P::modelIcon(0, m) + m, m);
            int idx = m_trModel->findData(cur);
            if (idx < 0) idx = 0;
            m_trModel->setCurrentIndex(idx);
            m_trModel->blockSignals(false);
        }, static_cast<Qt::ConnectionType>(Qt::SingleShotConnection));
    m_ai->fetchModels();
}

void ProgrammazionePage::onBtnTrCopyClicked()
{
    if (!m_trOutput || !m_btnTrCopy) return;
    QApplication::clipboard()->setText(m_trOutput->toPlainText());
    m_trCopyOrigTxt = m_btnTrCopy->text();
    m_btnTrCopy->setText("\xe2\x9c\x85  Copiato!");
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
    if (m_trTokenHolder) { m_trTokenHolder->deleteLater(); m_trTokenHolder = nullptr; }
    if (m_btnTrRun)  m_btnTrRun->setEnabled(true);
    if (m_btnTrStop) m_btnTrStop->setEnabled(false);
    const bool hasBlock = m_trOutput && m_trOutput->toPlainText().contains("```");
    if (m_btnTrInsert) m_btnTrInsert->setEnabled(hasBlock);
}

void ProgrammazionePage::onTrError(const QString& msg)
{
    if (m_trTokenHolder) { m_trTokenHolder->deleteLater(); m_trTokenHolder = nullptr; }
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

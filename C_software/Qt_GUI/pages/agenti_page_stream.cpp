#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QElapsedTimer>
#include <QTextCursor>
#include <QSettings>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QTextBrowser>
#include <QPushButton>
#include <QProcess>
#include <QTemporaryFile>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <cmath>
#include "agents_config_dialog.h"
#include "../widgets/chart_widget.h"
#include "../widgets/formula_parser.h"

/* ══════════════════════════════════════════════════════════════
   Slot segnali AI
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onToken(const QString& t) {
    if (m_opMode == OpMode::AutoAssign) {
        m_autoBuffer += t;
        return;
    }
    if (m_opMode == OpMode::ConsiglioScientifico) {
        /* I peer gestiscono i loro token internamente via lambda — ignora */
        return;
    }
    /* In modalità Idle non siamo attivi: i token appartengono ad un'altra pagina.
       Ignorarli previene output fantasma nel log degli agenti. */
    if (m_opMode == OpMode::Idle) return;

    m_waitLbl->setVisible(false);
    QTextCursor cursor(m_log->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(t);
    if (!m_userScrolled) {
        m_suppressScrollSig = true;
        m_log->ensureCursorVisible();
        m_suppressScrollSig = false;
    }

    if (m_opMode == OpMode::Pipeline && !m_agentOutputs.isEmpty()) {
        m_agentOutputs.last() += t;

        m_tokenCount++;
        int total = 0, done = 0;
        for (int i = 0; i < MAX_AGENTS; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) total++;
        for (int i = 0; i < m_currentAgent; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) done++;
        if (total > 0) {
            double share    = 100.0 / total;
            double startPct = done * share;
            double fill     = share * (1.0 - std::exp(-m_tokenCount / 150.0)) * 0.92;
            emit pipelineStatus(qBound(0, (int)(startPct + fill), 99), QString());
        }
    }
    else if (m_opMode == OpMode::Byzantine) {
        if      (m_byzStep == 0) m_byzA += t;
        else if (m_byzStep == 2) m_byzC += t;
    } else if (m_opMode == OpMode::MathTheory) {
        if      (m_byzStep == 0) m_byzA += t;
        else if (m_byzStep == 2) m_byzC += t;
    } else if (m_opMode == OpMode::PipelineControl) {
        m_ctrlAccum += t;
    } else if (m_opMode == OpMode::Translating) {
        m_translateBuf += t;
    }
}

void AgentiPage::onFinished(const QString& /*full*/) {
    m_waitLbl->setVisible(false);

    if (m_opMode == OpMode::Translating) {
        /* Traduzione completata — aggiorna il task e riparte con la modalità originale */
        QString translated = m_translateBuf.trimmed();
        if (translated.isEmpty()) translated = m_taskOriginal; /* fallback: testo originale */
        m_taskOriginal = _inject_math(translated);
        m_log->append(QString("\n\xf0\x9f\x8c\x90  Traduzione: <i>%1</i>\n").arg(m_taskOriginal));
        m_log->append(QString(43, QChar(0x2500)));
        OpMode next = m_pendingMode;
        m_pendingMode = OpMode::Idle;
        m_opMode = OpMode::Idle;
        /* Riavvia la modalità richiesta con il testo tradotto già in m_taskOriginal */
        if (next == OpMode::Pipeline) {
            int count = 0;
            for (int i = 0; i < MAX_AGENTS; i++)
                if (m_cfgDlg->enabledChk(i)->isChecked()) count++;
            m_agentOutputs.clear();
            m_currentAgent = 0;
            m_maxShots = m_cfgDlg->numAgents();
            m_opMode   = OpMode::Pipeline;
            for (int i = 0; i < MAX_AGENTS; i++) m_cfgDlg->enabledChk(i)->setStyleSheet("");
            emit pipelineStatus(0, "Avvio pipeline...");
            { int idx = m_bubbleIdx++; m_bubbleTexts[idx] = m_taskOriginal;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildUserBubble(m_taskOriginal, idx)); }
            m_log->append("");
            m_waitLbl->setVisible(true);
            advancePipeline();
        } else if (next == OpMode::Byzantine) {
            m_byzStep = 0; m_byzA = m_byzC = "";
            m_opMode = OpMode::Byzantine;
            m_log->append("\xf0\x9f\x94\xae  Motore Byzantino — verifica a 4 agenti\n");
            m_log->append("\xf0\x9f\x85\x90  [Agente A — Originale]\n");
            m_waitLbl->setVisible(true);
            m_ai->chat(_buildSys(m_taskOriginal, QString(
                "Sei l'Agente A del Motore Byzantino di Prismalux. "
                "Fornisci una risposta diretta e ben argomentata. "
                "Rispondi SEMPRE e SOLO in italiano."),
                m_ai->model(), m_ai->backend()), m_taskOriginal);
        } else if (next == OpMode::MathTheory) {
            m_byzStep = 0; m_byzA = m_byzC = "";
            m_opMode = OpMode::MathTheory;
            m_log->append("\xf0\x9f\xa7\xae  Matematico Teorico — esplorazione a 4 agenti\n");
            m_log->append("\xf0\x9f\x85\xb0  [Agente 1 — Enunciatore]\n");
            m_waitLbl->setVisible(true);
            m_ai->chat(_buildSys(m_taskOriginal, QString(
                "Sei l'Enunciatore matematico. Riformula il problema in forma rigorosa. "
                "Rispondi SEMPRE e SOLO in italiano."),
                m_ai->model(), m_ai->backend()), m_taskOriginal);
        } else {
            /* next == Idle: traduzione esplicita senza pipeline successiva */
            QString translated = m_translateBuf.trimmed();
            m_log->append("\n" + translated);
            /* Ripristina modello precedente se era stato cambiato */
            if (!m_preTranslateModel.isEmpty() && m_preTranslateModel != m_ai->model())
                m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_preTranslateModel);
            m_preTranslateModel.clear();
            m_btnRun->setEnabled(true);
            if (m_btnTranslate) m_btnTranslate->setEnabled(true);
            m_btnStop->setEnabled(false);
        }
        return;
    }

    if (m_opMode == OpMode::AutoAssign) {
        m_opMode = OpMode::Idle;
        parseAutoAssign(m_autoBuffer);
        return;
    }

    /* ── Controller LLM completato ── */
    if (m_opMode == OpMode::PipelineControl) {
        m_opMode = OpMode::Pipeline;   /* ripristina prima di advance */

        /* Sostituisce il testo grezzo del controller con la bolla colorata */
        QString ctrlHtml = markdownToHtml(m_ctrlAccum.trimmed());
        QTextCursor selCtrl(m_log->document());
        selCtrl.setPosition(m_ctrlBlockStart);
        selCtrl.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        selCtrl.removeSelectedText();
        selCtrl.insertHtml(buildControllerBubble(ctrlHtml));

        advancePipeline();
        return;
    }

    if (m_opMode == OpMode::Pipeline) {
        if (m_currentAgent < MAX_AGENTS)
            m_cfgDlg->enabledChk(m_currentAgent)->setStyleSheet(
                "QCheckBox { color: #4caf50; font-weight: bold; }"
                "QCheckBox::indicator:checked { background-color: #4caf50; border: 2px solid #388e3c; border-radius: 3px; }");

        m_currentAgent++;

        int total = 0, done = 0;
        for (int i = 0; i < MAX_AGENTS; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) total++;
        for (int i = 0; i < m_currentAgent; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) done++;
        int pct = (total > 0) ? qMin(done * 100 / total, 99) : 0;
        emit pipelineStatus(pct, QString("\xe2\x9c\x85  Agente %1/%2 completato  (%3%)")
            .arg(done).arg(qMin(total, m_maxShots)).arg(pct));

        /* Sostituisce l'indicatore streaming + testo grezzo con la bolla AI completa */
        QString rawResp;
        if (m_currentAgent > 0 && !m_agentOutputs.isEmpty()) {
            rawResp = m_agentOutputs[m_currentAgent - 1];
            /* Rimuove blocchi <think>...</think> (reasoning models: qwen3, deepseek-r1, qwq...)
               Se dopo lo strip rimane vuoto (modelli piccoli che producono SOLO thinking
               senza risposta finale), si usa il contenuto del <think> come fallback. */
            {
                QRegularExpression reTh("<think>([\\s\\S]*?)</think>",
                                        QRegularExpression::CaseInsensitiveOption);
                /* Salva l'originale prima di rimuovere */
                const QString original = rawResp;
                rawResp.remove(QRegularExpression("<think>[\\s\\S]*?</think>",
                    QRegularExpression::CaseInsensitiveOption));
                rawResp = rawResp.trimmed();

                /* Fallback: se il modello ha prodotto solo reasoning (o output vuoto),
                 * mostra il contenuto del <think> se non vuoto.
                 * Caso comune con modelli 0.8-1.5B: generano <think>...</think>
                 * ma poi non aggiungono risposta finale. */
                if (rawResp.isEmpty()) {
                    auto m = reTh.match(original);
                    if (m.hasMatch()) {
                        /* Prova prima il contenuto trimmed, poi raw (per preservare newline) */
                        const QString thinkTrimmed = m.captured(1).trimmed();
                        rawResp = thinkTrimmed.isEmpty() ? m.captured(1) : thinkTrimmed;
                        if (rawResp.isEmpty()) {
                            /* Modello troppo piccolo: think vuoto + nessuna risposta */
                            rawResp = "[Il modello non ha prodotto risposta. "
                                      "Usa un modello più grande (≥3B) o premi "
                                      "Singolo per domande semplici.]";
                        }
                        /* Non aggiornare m_agentOutputs con il think: passiamo ai prossimi
                           agenti un placeholder chiaro invece di contenuto di ragionamento */
                        m_agentOutputs[m_currentAgent - 1] =
                            "[Modello ha prodotto solo ragionamento interno — nessuna risposta finale]";
                    } else {
                        /* Nessun tag <think> e risposta vuota — modello non ha risposto */
                        rawResp = "[Nessuna risposta dal modello. "
                                  "Verifica che Ollama sia avviato e il modello selezionato "
                                  "sia disponibile.]";
                        m_agentOutputs[m_currentAgent - 1] = rawResp;
                    }
                } else {
                    m_agentOutputs[m_currentAgent - 1] = rawResp;
                }
            }
            QString htmlContent = rawResp.isEmpty()
                ? "<p style='color:#6b7280;font-style:italic;margin:0;'>Nessun output.</p>"
                : markdownToHtml(rawResp);

            QTextCursor sel(m_log->document());
            sel.setPosition(m_agentBlockStart);
            sel.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            sel.removeSelectedText();
            { int idx = m_bubbleIdx++; m_bubbleTexts[idx] = rawResp;
              sel.insertHtml(buildAgentBubble(m_currentAgentLabel,
                                             m_currentAgentModel,
                                             m_currentAgentTime,
                                             htmlContent, idx)); }
        }

        /* ── Tool Executor: estrae ed esegue codice Python, poi avvia il Controller ── */
        QString pyCode = extractPythonCode(rawResp);
        if (!pyCode.isEmpty()) {
            /* Corregge i bug tipici nei codici generati dall'AI */
            pyCode = _sanitizePyCode(pyCode);

            /* ── [C1] Dialog conferma esecuzione codice AI ───────────────────────
               Il codice generato dall'LLM viene MOSTRATO prima dell'esecuzione.
               L'utente deve cliccare "Esegui" esplicitamente — MAI automatico.
               Motivazione: un LLM può produrre (per errore o per prompt injection
               in un documento caricato) codice che cancella file o esfiltra dati. */
            {
                auto* dlg = new QDialog(this);
                dlg->setWindowTitle(
                    "\xe2\x9a\xa0  Esegui codice generato dall\xe2\x80\x99" "AI?");
                dlg->setMinimumSize(660, 460);
                auto* lay = new QVBoxLayout(dlg);

                auto* warnLbl = new QLabel(
                    "\xe2\x9a\xa0  Stai per eseguire codice Python generato dall\xe2\x80\x99"
                    "AI con i tuoi permessi utente.\n"
                    "Verifica che non faccia operazioni indesiderate prima di procedere.",
                    dlg);
                warnLbl->setWordWrap(true);
                warnLbl->setStyleSheet(
                    "color:#facc15;font-weight:bold;padding:6px;"
                    "background:#292524;border-radius:4px;");
                lay->addWidget(warnLbl);

                auto* codeView = new QTextEdit(dlg);
                codeView->setReadOnly(true);
                codeView->setPlainText(pyCode);
                codeView->setFont(QFont(
                    "JetBrains Mono,Fira Code,Consolas,monospace", 10));
                codeView->setStyleSheet(
                    "background:#1e1e2e;color:#cdd6f4;"
                    "border:1px solid #45475a;padding:4px;");
                lay->addWidget(codeView, 1);

                auto* btnBox = new QDialogButtonBox(dlg);
                auto* btnRun = btnBox->addButton(
                    "\xe2\x96\xb6  Esegui", QDialogButtonBox::AcceptRole);
                btnBox->addButton(
                    "\xe2\x9c\x96  Annulla", QDialogButtonBox::RejectRole);
                btnRun->setStyleSheet(
                    "background:#ef4444;color:#fff;"
                    "font-weight:bold;padding:4px 18px;");
                connect(btnBox, &QDialogButtonBox::accepted,
                        dlg, &QDialog::accept);
                connect(btnBox, &QDialogButtonBox::rejected,
                        dlg, &QDialog::reject);
                lay->addWidget(btnBox);

                const bool accepted = (dlg->exec() == QDialog::Accepted);
                dlg->deleteLater();
                if (!accepted) {
                    /* Utente ha annullato — salta executor, pipeline avanza */
                    advancePipeline();
                    return;
                }
            }
            /* ── fine dialog conferma ─────────────────────────────────────── */

            /* Scrivi il codice in un file temporaneo con nome casuale (anti-TOCTOU).
               autoRemove=false: il file rimane su disco dopo close()/delete dell'oggetto.
               Viene rimosso dai QFile::remove(tmpPath) nei lambda finished sottostanti. */
            QTemporaryFile execTmp(
                PrismaluxPaths::safeTempPath() + "/prisma_exec_XXXXXX.py");
            execTmp.setAutoRemove(false);
            if (!execTmp.open()) { advancePipeline(); return; }
            execTmp.write(pyCode.toUtf8());
            execTmp.close();
            const QString tmpPath = execTmp.fileName();
            /* execTmp distrutto qui — handle chiuso, file resta su disco */

            m_executorOutput.clear();
            if (m_execProc) { m_execProc->kill(); m_execProc->deleteLater(); m_execProc = nullptr; }
            m_execProc = new QProcess(this);
            m_execProc->setProcessChannelMode(QProcess::MergedChannels);

            /* [B1] QSharedPointer evita il memory leak se il processo viene
               distrutto prima che finished() scatti (es. app chiusa durante
               esecuzione): il distruttore del shared_ptr libera il timer
               anche se il lambda non viene mai invocato. */
            auto tmr = QSharedPointer<QElapsedTimer>::create();
            tmr->start();

            connect(m_execProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, tmpPath, tmr](int exitCode, QProcess::ExitStatus) {
                const double ms = tmr->elapsed();
                QString out = QString::fromUtf8(m_execProc->readAll());
                m_execProc->deleteLater();
                m_execProc = nullptr;

                /* ── [C2] Auto-install modulo mancante con conferma utente ──────
                   Il nome del pacchetto viene dall'output dell'LLM — rischio
                   typosquatting/supply-chain. L'utente deve autorizzare
                   esplicitamente ogni `pip install`. Default: No (scelta sicura). */
                static QRegularExpression reModule(
                    "ModuleNotFoundError: No module named '([^']+)'");
                auto mMatch = reModule.match(out);
                if (exitCode != 0 && mMatch.hasMatch()) {
                    const QString pkg = mMatch.captured(1).split('.').first();

                    const int ans = QMessageBox::warning(this,
                        "\xe2\x9a\xa0  Installa pacchetto Python?",
                        QString(
                            "Il codice richiede il pacchetto <b>%1</b> non installato."
                            "<br><br>"
                            "\xe2\x9a\xa0  <b>Attenzione</b>: il nome viene da un"
                            " suggerimento dell\xe2\x80\x99" "AI.<br>"
                            "Verifica che <code>%1</code> sia il pacchetto corretto"
                            " su pypi.org prima di procedere.<br><br>"
                            "Eseguire <code>pip install %1</code>?").arg(pkg),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No);   /* default No — scelta sicura */

                    if (ans != QMessageBox::Yes) {
                        QFile::remove(tmpPath);
                        QTextCursor logC(m_log->document());
                        logC.movePosition(QTextCursor::End);
                        logC.insertHtml(QString(
                            "<div style='color:#f87171;margin:4px 0'>"
                            "\xe2\x9d\x8c  Installazione di \xe2\x80\x98%1\xe2\x80\x99"
                            " annullata dall\xe2\x80\x99utente.</div>").arg(pkg));
                        m_executorOutput = out;
                        if (m_cfgDlg->controllerEnabled()) runPipelineController();
                        else advancePipeline();
                        return;
                    }

                    QTextCursor logC(m_log->document());
                    logC.movePosition(QTextCursor::End);
                    logC.insertHtml(QString(
                        "<div style='color:#facc15;font-style:italic;margin:4px 0'>"
                        "\xf0\x9f\x93\xa6  Installo '%1' via pip...</div>").arg(pkg));
                    if (!m_userScrolled) m_log->ensureCursorVisible();

                    auto* pip = new QProcess(this);
                    pip->setProcessChannelMode(QProcess::MergedChannels);
                    connect(pip, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                            this, [this, pip, tmpPath, pkg](int rc, QProcess::ExitStatus) {
                        const QString pipOut = QString::fromUtf8(pip->readAll()).trimmed();
                        pip->deleteLater();

                        QTextCursor logC2(m_log->document());
                        logC2.movePosition(QTextCursor::End);
                        if (rc == 0) {
                            logC2.insertHtml(QString(
                                "<div style='color:#4ade80;margin:4px 0'>"
                                "\xe2\x9c\x85  '%1' installato. Riprovo l\xe2\x80\x99"
                                "esecuzione...</div>").arg(pkg));
                            if (!m_userScrolled) m_log->ensureCursorVisible();

                            auto* retry = new QProcess(this);
                            retry->setProcessChannelMode(QProcess::MergedChannels);
                            /* [B1] QSharedPointer anche per il timer del retry */
                            auto t2 = QSharedPointer<QElapsedTimer>::create();
                            t2->start();
                            connect(retry, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                                    this, [this, retry, tmpPath, t2](int rc2, QProcess::ExitStatus) {
                                const double ms2 = t2->elapsed();
                                QString out2 = QString::fromUtf8(retry->readAll());
                                retry->deleteLater();
                                QFile::remove(tmpPath);
                                m_executorOutput = out2;
                                QTextCursor c2(m_log->document());
                                c2.movePosition(QTextCursor::End);
                                c2.insertHtml(buildToolStrip(QString(),
                                    PrismaluxPaths::sanitizeErrorOutput(out2), rc2, ms2));
                                if (!m_userScrolled) m_log->ensureCursorVisible();
                                if (m_cfgDlg->controllerEnabled())
                                    runPipelineController();
                                else
                                    advancePipeline();
                            });
                            retry->start(PrismaluxPaths::findPython(), {tmpPath});
                        } else {
                            logC2.insertHtml(QString(
                                "<div style='color:#f87171;margin:4px 0'>"
                                "\xe2\x9d\x8c  pip install '%1' fallito. Installa manualmente:<br>"
                                "<code>pip install %1</code></div>").arg(pkg));
                            QFile::remove(tmpPath);
                            m_executorOutput = pipOut;
                            if (m_cfgDlg->controllerEnabled())
                                runPipelineController();
                            else
                                advancePipeline();
                        }
                    });
                    pip->start(PrismaluxPaths::findPython(), {"-m", "pip", "install", pkg,
                        "--quiet", "--trusted-host", "pypi.org",
                        "--trusted-host", "files.pythonhosted.org"});
                    return;  /* non rimuovere tmpPath: lo usa il retry */
                }
                /* ── fine auto-install con conferma ─────────────────────────── */

                QFile::remove(tmpPath);
                m_executorOutput = out;

                /* Tronca traceback eccessivamente lunghi prima di mostrarli in UI.
                   L'output completo è in m_executorOutput per il controller LLM. */
                const QString outDisplay = PrismaluxPaths::sanitizeErrorOutput(out);

                /* Inserisce la tool strip nel log */
                QTextCursor c(m_log->document());
                c.movePosition(QTextCursor::End);
                c.insertHtml(buildToolStrip(QString(), outDisplay, exitCode, ms));
                if (!m_userScrolled) {
                    m_suppressScrollSig = true;
                    m_log->ensureCursorVisible();
                    m_suppressScrollSig = false;
                }

                /* Avvia il controller LLM (solo se checkbox abilitato) */
                if (m_cfgDlg->controllerEnabled())
                    runPipelineController();
                else
                    advancePipeline();
            });

            connect(m_execProc, &QProcess::errorOccurred,
                    this, [this, tmr](QProcess::ProcessError err){
                if (err == QProcess::FailedToStart) {
                    m_execProc->deleteLater();
                    m_execProc = nullptr;
                    advancePipeline();   /* python3 non disponibile: salta executor */
                }
            });
            m_execProc->start(PrismaluxPaths::findPython(), {tmpPath});
        } else {
            /* Nessun codice trovato: avanza direttamente */
            advancePipeline();
        }
        return;
    }

    /* Matematico Teorico — strip think tags prima di usare l'output come contesto */
    if (m_opMode == OpMode::MathTheory) {
        {
            /* stripThink: rimuove <think>...</think>; se rimane vuoto usa il contenuto think */
            auto stripThink = [](QString& s) {
                static const QRegularExpression reCap("<think>([\\s\\S]*?)</think>",
                    QRegularExpression::CaseInsensitiveOption);
                static const QRegularExpression reRem("<think>[\\s\\S]*?</think>",
                    QRegularExpression::CaseInsensitiveOption);
                const QString orig = s;
                s.remove(reRem);
                s = s.trimmed();
                if (s.isEmpty()) {
                    auto m = reCap.match(orig);
                    if (m.hasMatch()) s = m.captured(1).trimmed();
                }
            };
            if (m_byzStep == 0) stripThink(m_byzA);
            if (m_byzStep == 2) stripThink(m_byzC);
        }
        m_byzStep++;
        static const char* mathLabels[] = {
            "\xf0\x9f\x94\xad  [Agente 2 \xe2\x80\x94 Esploratore]\n",
            "\xf0\x9f\x93\x90  [Agente 3 \xe2\x80\x94 Dimostratore]\n",
            "\xe2\x9c\xa8  [Agente 4 \xe2\x80\x94 Sintetizzatore]\n",
        };
        if (m_byzStep <= 3) {
            m_log->append(QString("\n") + QString(43, QChar(0x2500)));
            m_log->append(mathLabels[m_byzStep - 1]);
        }
        QString ctx;
        switch (m_byzStep) {
            case 1:
                ctx = QString("Problema originale: %1\n\nFormulazione rigorosa:\n%2").arg(m_taskOriginal, m_byzA);
                m_ai->chat("Sei l'Esploratore matematico. Individua teoremi o metodi applicabili. "
                           "Sii conciso (max 150 parole). Rispondi SOLO in italiano.", ctx);
                break;
            case 2:
                ctx = QString("Problema: %1\n\nFormulazione rigorosa:\n%2").arg(m_taskOriginal, m_byzA);
                m_ai->chat("Sei il Dimostratore matematico. Fornisci la soluzione passo per passo. "
                           "Sii conciso (max 200 parole). Rispondi SOLO in italiano.", ctx);
                break;
            case 3:
                ctx = QString("Problema: %1\n\nSoluzione:\n%2").arg(m_taskOriginal, m_byzC);
                m_ai->chat("Sei il Sintetizzatore matematico. Riassumi il risultato finale. "
                           "Sii conciso (max 150 parole). Rispondi SOLO in italiano.", ctx);
                break;
            default:
                m_log->append("\n\n\xe2\x9c\x85  Esplorazione matematica completata.");
                m_btnRun->setEnabled(true);
                m_btnStop->setEnabled(false);
                m_opMode = OpMode::Idle;
                tryShowChart(m_taskOriginal + "\n" + m_byzA + "\n" + m_byzC);
                emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
                break;
        }
        return;
    }

    /* Byzantino — strip think tags prima di usare l'output come contesto */
    {
        auto stripThink = [](QString& s) {
            static const QRegularExpression reCap("<think>([\\s\\S]*?)</think>",
                QRegularExpression::CaseInsensitiveOption);
            static const QRegularExpression reRem("<think>[\\s\\S]*?</think>",
                QRegularExpression::CaseInsensitiveOption);
            const QString orig = s;
            s.remove(reRem);
            s = s.trimmed();
            if (s.isEmpty()) {
                auto m = reCap.match(orig);
                if (m.hasMatch()) s = m.captured(1).trimmed();
            }
        };
        if (m_byzStep == 0) stripThink(m_byzA);
        if (m_byzStep == 2) stripThink(m_byzC);
    }
    m_byzStep++;
    static const char* labels[] = {
        "\xf0\x9f\x85\xb1  [Agente B \xe2\x80\x94 Avvocato del Diavolo]\n",
        "\xf0\x9f\x85\x92  [Agente C \xe2\x80\x94 Gemello Indipendente]\n",
        "\xf0\x9f\x85\x93  [Agente D \xe2\x80\x94 Giudice]\n",
    };
    if (m_byzStep <= 3) {
        m_log->append(QString("\n") + QString(43, QChar(0x2500)));
        m_log->append(labels[m_byzStep - 1]);
    }
    QString userMsg;
    switch (m_byzStep) {
        case 1:
            userMsg = QString("Domanda: %1\n\nRisposta A:\n%2").arg(m_taskOriginal, m_byzA);
            m_ai->chat(_buildSys(m_taskOriginal, QString(
                       "Sei l'Agente B del Motore Byzantino, l'Avvocato del Diavolo. "
                       "Cerca ATTIVAMENTE errori e contraddizioni nella risposta A. "
                       "Rispondi SEMPRE e SOLO in italiano."),
                       m_ai->model(), m_ai->backend()), userMsg);
            break;
        case 2:
            m_ai->chat(_buildSys(m_taskOriginal, QString(
                       "Sei l'Agente C del Motore Byzantino, il Gemello Indipendente. "
                       "Rispondi alla domanda originale da un angolo diverso. "
                       "Rispondi SEMPRE e SOLO in italiano."),
                       m_ai->model(), m_ai->backend()), m_taskOriginal);
            break;
        case 3:
            userMsg = QString("Domanda: %1\n\nRisposta A:\n%2\n\nRisposta C:\n%3")
                      .arg(m_taskOriginal, m_byzA, m_byzC);
            m_ai->chat(_buildSys(m_taskOriginal, QString(
                       "Sei l'Agente D del Motore Byzantino, il Giudice. "
                       "Se A e C concordano e B non trova errori validi: conferma. "
                       "Altrimenti segnala l'incertezza. Produci il verdetto finale. "
                       "Rispondi SEMPRE e SOLO in italiano."),
                       m_ai->model(), m_ai->backend()), userMsg);
            break;
        default:
            m_log->append("\n\n\xe2\x9c\x85  Verifica Byzantina completata.");
            m_log->append("\xe2\x9c\xa8 Verit\xc3\xa0 rivelata. Bevi la conoscenza.");
            m_btnRun->setEnabled(true);
            m_btnStop->setEnabled(false);
            m_opMode = OpMode::Idle;
            tryShowChart(m_taskOriginal + "\n" + m_byzA + "\n" + m_byzC);
            emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
            break;
    }
}
/* ── _categorizeError ─────────────────────────────────────────────────────────
 * Classifica l'errore e restituisce un messaggio localizzato con suggerimento.
 * Categorie (in ordine di priorità):
 *   ram       → RAM critica (già nel messaggio, nessun hint aggiuntivo)
 *   abort     → operazione annullata (silenzioso)
 *   network   → Connection refused / ECONNREFUSED
 *   closed    → RemoteHostClosed (crash server o VRAM esaurita)
 *   timeout   → socket timeout / nessuna risposta
 *   json      → risposta malformata / JSON non valido
 *   other     → errore generico con hint backend
 * ──────────────────────────────────────────────────────────────────────────── */
static QString _categorizeError(const QString& msg, AiClient::Backend backend) {
    const QString ml = msg.toLower();

    /* RAM critica — il messaggio è già autoesplicativo */
    if (ml.contains("ram critica")) return msg;

    /* Operazione annullata dall'utente o dal sistema — nessun output */
    if (ml.contains("operation canceled") || ml.contains("operazione annullata") ||
        ml.contains("canceled") || ml.contains("aborted"))
        return QString();   /* stringa vuota = silenzioso */

    /* Connessione rifiutata: backend non avviato */
    if (ml.contains("connection refused") || ml.contains("econnrefused") ||
        ml.contains("refused") || ml.contains("could not connect")) {
        return (backend == AiClient::Ollama)
            ? "\xe2\x9d\x8c  Backend non raggiungibile. Avvia Ollama: <code>ollama serve</code>"
            : "\xe2\x9d\x8c  Backend non raggiungibile. Avvia llama-server dalla tab \xf0\x9f\x94\x8c Backend.";
    }

    /* Connessione chiusa improvvisamente: crash del server o VRAM esaurita */
    if (ml.contains("remotehostclosed") || ml.contains("remote host closed") ||
        ml.contains("connection reset") || ml.contains("broken pipe")) {
        return "\xe2\x9a\xa0  Connessione chiusa improvvisamente.\n"
               "\xf0\x9f\x92\xbe  Possibili cause: VRAM esaurita, crash del server, modello troppo grande.";
    }

    /* Timeout: modello lento o in caricamento */
    if (ml.contains("timed out") || ml.contains("timeout") || ml.contains("socket timeout")) {
        return "\xe2\x8f\xb1  Timeout: nessuna risposta dal backend.\n"
               "\xf0\x9f\xa4\x94  Il modello potrebbe essere in caricamento o troppo grande per la RAM disponibile.";
    }

    /* Risposta JSON malformata */
    if (ml.contains("json") || ml.contains("parse") || ml.contains("malform") ||
        ml.contains("invalid content")) {
        return "\xe2\x9a\xa0  Risposta malformata dal backend. Riprova.";
    }

    /* Errore generico con hint backend */
    QString out = "\xe2\x9d\x8c  " + msg;
    out += (backend == AiClient::Ollama)
        ? "\n\xf0\x9f\x8c\xab  La nebbia copre la verit\xc3\xa0. Controlla che Ollama sia in esecuzione: <code>ollama serve</code>"
        : "\n\xf0\x9f\x8c\xab  La nebbia copre la verit\xc3\xa0. Controlla che llama-server sia avviato.";
    return out;
}

void AgentiPage::onError(const QString& msg) {
    m_waitLbl->setVisible(false);
    if (m_opMode == OpMode::AutoAssign) {
        m_autoLbl->setText(QString("\xe2\x9d\x8c  Errore auto-assegnazione: %1").arg(msg));
        m_btnAuto->setEnabled(true);
        m_opMode = OpMode::Idle;
        return;
    }

    const QString categorized = _categorizeError(msg, m_ai->backend());
    if (!categorized.isEmpty())
        m_log->append("\n" + categorized);
    /* Se categorized è vuota (abort silenzioso) non mostriamo nulla */

    m_btnRun->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_opMode = OpMode::Idle;
    m_waitLbl->setVisible(false);
}

/* ══════════════════════════════════════════════════════════════
   tryShowChart — rileva formula o coppie di punti nel testo AI
   e mostra il grafico nel pannello inferiore della pagina.
   Usa FormulaParser::tryExtract (pattern "y=..." o "f(x)=...")
   e FormulaParser::tryExtractPoints (coppie coordinate).
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::tryShowChart(const QString& text) {
    if (!m_chartPanel) return;

    /* Rimuovi ChartWidget precedente */
    const auto oldCharts = m_chartPanel->findChildren<ChartWidget*>();
    for (auto* c : oldCharts) { c->setParent(nullptr); c->deleteLater(); }

    /* Intervallo x: cerca nel testo (es. "da x=-5 a x=5", "x ∈ [-10,10]") */
    double xMin = -10.0, xMax = 10.0;
    FormulaParser::tryExtractXRange(text, xMin, xMax);

    /* 1. Cerca una formula "y = expr", "f(x) = expr" o "grafico di expr" */
    const QString expr = FormulaParser::tryExtract(text);
    if (!expr.isEmpty()) {
        FormulaParser fp(expr);
        if (fp.ok()) {
            auto pts = fp.sample(xMin, xMax, 400);
            if (!pts.isEmpty()) {
                auto* chart = new ChartWidget(m_chartPanel);
                chart->setTitle(expr);
                chart->setAxisLabels("x", "y");
                ChartWidget::Series s;
                s.name = expr;
                s.pts  = pts;
                chart->addSeries(s);
                m_chartPanel->layout()->addWidget(chart);
                m_chartPanel->setVisible(true);
                return;
            }
        }
    }

    /* 2. Cerca coppie di coordinate "(x, y)" nel testo */
    const auto pts = FormulaParser::tryExtractPoints(text);
    if (pts.size() >= 2) {
        auto* chart = new ChartWidget(m_chartPanel);
        chart->setAxisLabels("x", "y");
        ChartWidget::Series s;
        s.name = "Punti";
        s.pts  = pts;
        s.mode = ChartWidget::Scatter;
        chart->addSeries(s);
        m_chartPanel->layout()->addWidget(chart);
        m_chartPanel->setVisible(true);
    }
}


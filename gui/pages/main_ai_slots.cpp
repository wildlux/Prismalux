/* main_ai_slots.cpp */
#include "main_ai.h"
#include "main_ai_p.h"
#include "../dpi_utils.h"
#include "../widgets/latex_view.h"
#include <QPainter>
#include <QFont>
#include <QTextCharFormat>
#include <QMouseEvent>
#include <QColorDialog>
#include "../prismalux_paths.h"
#include "../log_bus.h"
namespace P = PrismaluxPaths;
#include "../app_config.h"
#include <QTime>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QShortcut>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLabel>
#include <QFrame>
#include <QUrl>
#include <QNetworkRequest>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QScrollArea>
#include <QScrollBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QTextStream>
#include <QPrinter>
#include <QPrintDialog>
#include <QPageSize>
#include <QImage>
#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <QTimer>
#include <QSettings>
#include <QToolTip>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QTextDocument>
#include <QTextCursor>
#include "../widgets/stt_whisper.h"
#include "../widgets/model_combo_helper.h"
#include "../widgets/chart_widget.h"
#include "../widgets/formula_parser.h"
#include "dialog_agents_config.h"
#include <QStandardPaths>
#include <QGroupBox>
#ifndef Q_OS_WIN
#  include <csignal>
#  include <sys/types.h>
#endif

static bool isVisionCapable(const QString& mdl)
{
    const QString m = mdl.toLower();
    return m.contains("vision") || m.contains("-vl") || m.contains("llava")
        || m.contains("minicpm-v") || m.contains("bakllava") || m.contains("cogvlm")
        || m.contains("moondream") || m.contains("idefics") || m.contains("phi-3-v")
        || m.contains("phi3-v") || m.contains("internvl") || m.contains("qwen-vl")
        || m.contains("qwen2-vl") || m.contains("omnivision");
}

/* Helper: ritorna true se il modello supporta function/tool calling */
static bool isToolCapable(const QString& mdl)
{
    const QString m = mdl.toLower();
    if (m.contains("deepseek-coder")) return false;
    if (m.contains("deepseek-r1"))    return false;
    if (m.contains("codellama"))      return false;
    if (m.contains("phi-2"))          return false;
    return true;
}

void AgentiPage::onCmbLLMIndexChanged(int idx)
{
    if (idx < 0 || !m_cmbLLM) return;
    const QString mdl = ModelComboHelper::currentModel(m_cmbLLM);
    if (mdl.isEmpty() || mdl == "(caricamento...)") return;
    m_pageModel = mdl;
    m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), mdl);

    /* ── Capability check: vision — avvisa in tooltip di Allega file ── */
    const bool isDeepSeek = mdl.toLower().contains("deepseek");
    const bool hasVision  = isVisionCapable(mdl);
    if (m_btnDoc) {
        if (isDeepSeek && !hasVision) {
            m_btnDoc->setToolTip(
                tr("Allega file alla chat.\n"
                   "Documenti: .txt .md .csv .json .py .cpp .h .pdf .xls\n"
                   "Immagini: .png .jpg .jpeg .gif .webp\n"
                   "\xe2\x9a\xa0  Attenzione: il modello attuale (DeepSeek) non supporta immagini."));
        } else if (!hasVision) {
            m_btnDoc->setToolTip(
                tr("Allega file alla chat.\n"
                   "Documenti: .txt .md .csv .json .py .cpp .h .pdf .xls\n"
                   "Immagini: .png .jpg .jpeg .gif .webp\n"
                   "\xe2\x84\xb9  Per inviare immagini usa un modello vision (*-vl, llava, gemma3\xe2\x80\xa6)"));
        } else {
            m_btnDoc->setToolTip(
                tr("Allega file alla chat.\n"
                   "Documenti: .txt .md .csv .json .py .cpp .h .pdf .xls\n"
                   "Immagini: .png .jpg .jpeg .gif .webp"));
        }
    }

    /* ── Capability check: tool use ── */
    const bool hasTool = isToolCapable(mdl);
    if (m_toolChk && !m_autoEnabled) {
        m_toolChk->setEnabled(hasTool);
        if (!hasTool) {
            m_toolChk->setChecked(false);
            m_toolChk->setToolTip(tr("%1 non supporta function calling — tools disabilitati").arg(mdl));
        } else {
            m_toolChk->setToolTip(
                "Abilita il function calling (Ollama tool use) nella prossima risposta.\n"
                "Il modello pu\xc3\xb2 chiamare: leggi_file, lista_file, calc, cerca_web, python.\n"
                "Richiede un modello tool-capable (qwen3, llama3.1, mistral-nemo...).\n"
                "In modalit\xc3\xa0 Agente Autonomo i tool sono sempre attivi.");
        }
    }

    /* ── Etichetta avviso combinata ── */
    QStringList warns;
    if (isDeepSeek && !hasVision) warns << "\xf0\x9f\x9b\x87 no vision";  /* 🛇 */
    if (!hasTool)                  warns << "\xf0\x9f\x94\xa7 no tools";  /* 🔧 */
    if (m_modelWarnLbl) {
        if (!warns.isEmpty()) {
            m_modelWarnLbl->setText(warns.join("  "));
            m_modelWarnLbl->setToolTip(
                (isDeepSeek && !hasVision ? tr("Questo modello non supporta le immagini in input.\n") : QString()) +
                (!hasTool ? tr("Questo modello non supporta il function calling.") : QString()));
            m_modelWarnLbl->setVisible(true);
        } else {
            m_modelWarnLbl->setVisible(false);
        }
    }

    /* Mostra il pulsante "Rigenera" solo se la chat ha già contenuto */
    if (m_btnRegen && m_log && !m_log->toPlainText().trimmed().isEmpty()) {
        QString shortMdl = mdl;
        if (shortMdl.length() > 18)
            shortMdl = shortMdl.left(16) + "\xe2\x80\xa6";  /* … */
        m_btnRegen->setText("\xf0\x9f\x94\x84 " + shortMdl);
        m_btnRegen->setVisible(true);
    }
}

/* ─────────────────────────────────────────────────────────────────
   onBtnRegenClicked — reinvia l'ultimo testo utente con il modello
   corrente appena selezionato nel combo.
   Strategia: cerca nell'HTML del log l'ultimo href "retry:N:BASE64URL"
   (inserito da buildUserBubble) e decodifica il testo originale.
   Se non trova il link fallback: non fa nulla (chat vuota o bolle senza idx).
   ───────────────────────────────────────────────────────────────── */
void AgentiPage::onBtnRegenClicked()
{
    if (!m_btnRegen || !m_log) return;
    m_btnRegen->setVisible(false);

    /* Cerca l'ultimo "retry:N:BASE64URL" nell'HTML corrente */
    const QString html = m_log->toHtml();
    static const QRegularExpression reRetry(
        "retry:\\d+:([A-Za-z0-9_-]+)",
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = reRetry.globalMatch(html);
    QString lastB64;
    while (it.hasNext())
        lastB64 = it.next().captured(1);   /* prende l'ultimo match */

    if (lastB64.isEmpty()) return;

    const QString origText = QString::fromUtf8(
        QByteArray::fromBase64(lastB64.toLatin1(), QByteArray::Base64UrlEncoding));
    if (origText.trimmed().isEmpty()) return;

    m_input->setPlainText(origText.trimmed());
    m_input->setFocus();
    m_input->moveCursor(QTextCursor::End);
    QTimer::singleShot(0, this, &AgentiPage::onBtnRunDelayedClick);
}

void AgentiPage::onModeToggleToggled(bool autoOn)
{
    m_autoEnabled   = autoOn;
    m_toolsEnabled  = autoOn || (m_toolChk && m_toolChk->isChecked());
    m_toolIteration = 0;
    m_modePipeline  = false;
    /* In modalità Autonomo: tools sempre attivi, checkbox bloccato spuntato */
    if (m_toolChk) {
        m_toolChk->blockSignals(true);
        if (autoOn) m_toolChk->setChecked(true);
        m_toolChk->setEnabled(!autoOn);
        m_toolChk->blockSignals(false);
    }

    m_btnRun->setText(autoOn
        ? "\xf0\x9f\xa4\x96  Avvia Agente"
        : "\xf0\x9f\x93\xa4 Invia");
    if (m_modeBtn) m_modeBtn->setActionText(m_btnRun->text());
}

/* ── Settore TriModeButton selezionato: 0=Chat  1=Agentico  2=Conversa ── */
void AgentiPage::onCycleModeShortcut()
{
    if (!m_modeBtn || !isVisible()) return;   /* solo quando tab AI è visibile */
    const int next = ((int)m_modeBtn->currentMode() + 1) % 3;
    m_modeBtn->setMode((TriModeButton::Mode)next, true);
}

void AgentiPage::onModeBtnChanged(int mode)
{
    /* 1. Ferma modalità precedente */
    if (m_autoEnabled) onModeToggleToggled(false);

    /* Ferma loop voce / registrazione attiva (con blockSignals per evitare re-entranza) */
    if (m_voiceLoopActive || m_sttState == SttState::Recording) {
        onVoiceLoopToggled(false);
    }
    if (m_btnVoiceLoop && m_btnVoiceLoop->isChecked()) {
        m_btnVoiceLoop->blockSignals(true);
        m_btnVoiceLoop->setChecked(false);
        m_btnVoiceLoop->blockSignals(false);
    }

    /* 2. Ripristina run button di default, poi specializza */
    m_btnRun->setText("\xf0\x9f\x93\xa4 Invia");
    if (m_modeBtn) m_modeBtn->setActionText("\xf0\x9f\x93\xa4 Invia");

    /* 3. Attiva nuova modalità */
    switch (mode) {
    case 0:   /* Chat */
        break;
    case 1:   /* Agentico */
        onModeToggleToggled(true);   /* gestisce anche il testo del run button + hub */
        break;
    case 2:   /* Conversa */
        m_btnRun->setText("\xf0\x9f\x8e\x99  Dialoga");
        if (m_modeBtn) m_modeBtn->setActionText("\xf0\x9f\x8e\x99  Dialoga");
        break;
    }
}

void AgentiPage::onLogScrollValueChanged(int value)
{
    if (m_suppressScrollSig) return;
    m_userScrolled = (value < m_log->verticalScrollBar()->maximum());
}

void AgentiPage::onBtnChartOpenClicked()
{
    emit requestShowInGrafico(m_lastChartExpr, m_lastChartXMin, m_lastChartXMax, m_lastChartPts);
}

void AgentiPage::onLogAnchorClicked(const QUrl& url)
{
    const QString s = url.toString();
    /* formato: "copy:IDX" | "tts:IDX" | "chart:show" | "settings:<tab>" | "fb:up/down:IDX" */
    /* ── Feedback \xf0\x9f\x91\x8d/\xf0\x9f\x91\x8e ── */
    if (s.startsWith("fb:")) {
        const QStringList parts = s.split(':');
        if (parts.size() >= 3) {
            const QString rating = parts[1];   /* "up" o "down" */
            const int     idx    = parts[2].toInt();
            saveFeedback(idx, rating == "up" ? 1 : -1);
        }
        return;
    }
    /* ── Rifai domanda con il modello corrente ── */
    if (s.startsWith("retry:")) {
        /* formato: "retry:IDX:BASE64URL" */
        const int c1r = s.indexOf(':');
        const int c2r = s.indexOf(':', c1r + 1);
        if (c2r > 0) {
            const int    retryIdx = s.mid(c1r + 1, c2r - c1r - 1).toInt();
            const QString b64r    = s.mid(c2r + 1);
            const QString origText = QString::fromUtf8(
                QByteArray::fromBase64(b64r.toLatin1(), QByteArray::Base64UrlEncoding));
            if (!origText.isEmpty()) {
                /* Tronca il log a partire dall'inizio della bolla utente con id='ubbl:N'
                   così la nuova bolla (+ risposta) viene inserita al posto giusto
                   senza duplicati né contenuto orfano. */
                if (m_log) {
                    QString html = m_log->toHtml();
                    const QString anchor = QString("id='ubbl:%1'").arg(retryIdx);
                    const int anchorPos  = html.indexOf(anchor);
                    if (anchorPos > 0) {
                        /* Risali al <table che contiene l'anchor */
                        const int tablePos = html.lastIndexOf("<table", anchorPos);
                        if (tablePos > 0) {
                            html = html.left(tablePos);
                            m_log->setHtml(html);
                            m_log->moveCursor(QTextCursor::End);
                        }
                    }
                }
                m_input->setPlainText(origText.trimmed());
                m_input->setFocus();
                m_input->moveCursor(QTextCursor::End);
                QTimer::singleShot(0, this, &AgentiPage::onBtnRunDelayedClick);
            }
        }
        return;
    }
    if (s.startsWith("settings:")) {
        emit requestOpenSettings(s.mid(9));
        return;
    }
    if (s.startsWith("insertinfo:")) {
        const QString topic = QString::fromUtf8(
            QByteArray::fromBase64(s.mid(11).toLatin1(), QByteArray::Base64UrlEncoding)).trimmed();
        bool ok = false;
        const QString answer = QInputDialog::getMultiLineText(
            this,
            "\xf0\x9f\x93\x9d  Aggiungi informazione manuale",
            QString("Inserisci la risposta su: <b>%1</b>\n"
                    "(Sar\xc3\xa0 salvata nel RAG e usata nelle prossime domande)")
                .arg(topic),
            QString(), &ok);
        if (!ok || answer.trimmed().isEmpty()) return;
        /* Salva in RAG/RICERCA come file Markdown */
        const QString ragDir = P::ragDir() + "/RICERCA";
        QDir().mkpath(ragDir);
        QString slug = topic.left(40);
        slug.replace(QRegularExpression("[^a-zA-Z0-9_ ]"), "_");
        slug = slug.simplified().replace(' ', '_');
        const QString outFile = ragDir + "/" +
            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_" + slug + ".md";
        QFile f(outFile);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << "# " << topic << "\n\n" << answer.trimmed() << "\n";
            f.close();
            m_log->moveCursor(QTextCursor::End);
            m_log->insertHtml(
                "<p style='color:#4ade80;font-size:11px;margin:4px 0;'>"
                "\xe2\x9c\x85  Informazione salvata nel RAG. "
                "Fai una nuova domanda per usarla come contesto.</p>");
            emit onlineSearchResultReady(outFile, topic);
        } else {
            m_log->moveCursor(QTextCursor::End);
            m_log->insertHtml(
                "<p style='color:#f87171;font-size:11px;margin:4px 0;'>"
                "\xe2\x9d\x8c  Impossibile salvare il file: " +
                outFile.toHtmlEscaped() + "</p>");
        }
        return;
    }
    if (s == "autoapply-params") {
        AiChatParams p = AiChatParams::load();
        p.temperature = 0.3;
        p.num_ctx     = 16384;
        p.top_p       = 0.9;
        p.num_predict = 4096;
        AiChatParams::save(p);
        if (m_ai) m_ai->setChatParams(p);
        m_log->moveCursor(QTextCursor::End);
        m_log->insertHtml(
            "<p style='color:#4ade80;font-size:11px;margin:4px 0;"
            "background:#0b1a10;border-left:3px solid #166534;"
            "border-radius:4px;padding:4px 10px;'>"
            "\xe2\x9c\x85 Parametri impostati: Temperatura 0.3 &mdash; "
            "Context 16384 &mdash; Top-P 0.9 &mdash; Max tokens 4096</p>");
        return;
    }
    if (s == "chart:show") {
        if (m_chartPanel) m_chartPanel->setVisible(true);
        return;
    }
    /* ── Elimina messaggio con conferma ── */
    if (s.startsWith("del:")) {
        QMessageBox ask(this);
        ask.setWindowTitle(tr("\xf0\x9f\x97\x91  Elimina messaggio"));
        ask.setText("<b>Eliminare questo messaggio dalla chat?</b>");
        ask.setInformativeText(
            "Questa operazione \xc3\xa8 irreversibile.");
        QPushButton* btnDel = ask.addButton("Elimina", QMessageBox::DestructiveRole);
        ask.addButton("Annulla", QMessageBox::RejectRole);
        ask.setDefaultButton(btnDel);
        ask.exec();
        if (ask.clickedButton() != btnDel) return;

        /* Salva snapshot per undo */
        m_undoHtmlStack.push(m_log->toHtml());

        /* Rimuovi il blocco della bolla dall'HTML usando il del:ID univoco */
        const QString c1s = s.mid(4, s.indexOf(':', 4) - 4); /* estrai ID */
        QString html = m_log->toHtml();
        QRegularExpression re(
            "<table[^>]*>(?:(?!</table>).)*?" +
            QRegularExpression::escape("del:" + c1s + ":") +
            ".*?</table>(?:\\s*<p[^>]*>\\s*</p>)?",
            QRegularExpression::DotMatchesEverythingOption);
        html.remove(re);
        m_log->setHtml(html);
        m_log->moveCursor(QTextCursor::End);
        return;
    }

    /* ── Copia blocco codice negli appunti ── */
    if (s.startsWith("code:copy:")) {
        bool ok3 = false;
        const int N = s.mid(10).toInt(&ok3);
        if (!ok3 || !m_codeBlocks.contains(N)) return;
        QGuiApplication::clipboard()->setText(m_codeBlocks[N].second);
        /* Feedback visivo: bolla temporanea nel log */
        m_log->moveCursor(QTextCursor::End);
        m_log->insertHtml(
            "<p style='color:#34d399;font-size:11px;margin:2px 0;"
            "font-style:italic;'>"
            "\xe2\x9c\x85 Codice copiato negli appunti."   /* ✅ */
            "</p>");
        return;
    }

    /* ── Salva blocco codice su disco ── */
    if (s.startsWith("code:save:")) {
        bool ok3 = false;
        const int N = s.mid(10).toInt(&ok3);
        if (!ok3 || !m_codeBlocks.contains(N)) return;
        const QString lang    = m_codeBlocks[N].first;
        const QString content = m_codeBlocks[N].second;

        /* Estendi al tipo di file corretto */
        static const QMap<QString,QString> extMap = {
            {"python","py"},{"py","py"},{"bash","sh"},{"sh","sh"},
            {"shell","sh"},{"c","c"},{"cpp","cpp"},{"c++","cpp"},
            {"h","h"},{"hpp","hpp"},{"java","java"},
            {"javascript","js"},{"js","js"},{"typescript","ts"},{"ts","ts"},
            {"html","html"},{"css","css"},{"sql","sql"},{"json","json"},
            {"yaml","yaml"},{"yml","yml"},{"xml","xml"},{"rust","rs"},
            {"go","go"},{"ruby","rb"},{"rb","rb"},{"php","php"},
            {"swift","swift"},{"kotlin","kt"},{"r","r"},{"lua","lua"},
            {"dart","dart"},{"cmake","cmake"},{"dockerfile","Dockerfile"},
            {"markdown","md"},{"md","md"},{"toml","toml"},{"ini","ini"},
        };
        const QString ext = extMap.value(lang, lang.isEmpty() ? "txt" : lang);
        const QString filter = ext == "Dockerfile"
            ? "Dockerfile (Dockerfile)"
            : QString("%1 (*.%2);;Tutti i file (*)").arg(ext.toUpper()).arg(ext);

        const QString path = QFileDialog::getSaveFileName(
            this,
            "Salva codice — " + (lang.isEmpty() ? "testo" : lang),
            QDir::homePath() + "/codice." + ext,
            filter);
        if (path.isEmpty()) return;

        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(content.toUtf8());
            f.close();
            m_log->moveCursor(QTextCursor::End);
            m_log->insertHtml(
                "<p style='color:#34d399;font-size:11px;margin:2px 0;"
                "font-style:italic;'>"
                "\xf0\x9f\x92\xbe Salvato: " +     /* 💾 */
                QFileInfo(path).fileName().toHtmlEscaped() +
                "</p>");
        } else {
            m_log->moveCursor(QTextCursor::End);
            m_log->insertHtml(
                "<p style='color:#f87171;font-size:11px;margin:2px 0;'>"
                "\xe2\x9d\x8c Errore salvataggio: " +   /* ❌ */
                f.errorString().toHtmlEscaped() + "</p>");
        }
        return;
    }

    /* ── Riesegui codice Python/C/C++ annullato in precedenza ── */
    if (s.startsWith("exec:run:")) {
        bool ok4 = false;
        const int execId = s.mid(9).toInt(&ok4);
        if (!ok4 || !m_pendingExecCodes.contains(execId)) return;
        const QString stored = m_pendingExecCodes.take(execId);
        /* formato: "lang:codice" */
        const int sep  = stored.indexOf(':');
        const QString execLang = (sep > 0) ? stored.left(sep)  : "python";
        const QString pyCode   = (sep > 0) ? stored.mid(sep+1) : stored;
        const bool    useSandbox = P::isSandboxReady() && execLang == "python";

        auto* dlg2 = new QDialog(this);
        dlg2->setWindowTitle(useSandbox
            ? "\xf0\x9f\x90\xb3  Esegui codice in sandbox Docker?"
            : "\xe2\x9a\xa0  Esegui codice generato dall\xe2\x80\x99" "AI?");
        dlg2->setMinimumSize(660, 460);
        auto* lay2 = new QVBoxLayout(dlg2);
        auto* warn2 = new QLabel(useSandbox
            ? "\xf0\x9f\x90\xb3  Il codice verr\xc3\xa0 eseguito in un container Docker isolato."
            : "\xe2\x9a\xa0  Codice Python generato dall\xe2\x80\x99" "AI — verifica prima di procedere.",
            dlg2);
        warn2->setWordWrap(true);
        warn2->setStyleSheet(useSandbox
            ? "color:#86efac;font-weight:bold;padding:6px;background:#052e16;border-radius:4px;"
            : "color:#facc15;font-weight:bold;padding:6px;background:#292524;border-radius:4px;");
        lay2->addWidget(warn2);
        auto* cv2 = new QTextEdit(dlg2);
        cv2->setReadOnly(true);
        cv2->setPlainText(pyCode);
        cv2->setFont(QFont("JetBrains Mono,Fira Code,Consolas,monospace", 10));
        cv2->setStyleSheet("background:#1e1e2e;color:#cdd6f4;border:1px solid #45475a;padding:4px;");
        lay2->addWidget(cv2, 1);
        auto* bb2 = new QDialogButtonBox(dlg2);
        auto* btnRun2 = bb2->addButton("\xe2\x96\xb6  Esegui", QDialogButtonBox::AcceptRole);
        bb2->addButton("\xe2\x9c\x96  Annulla", QDialogButtonBox::RejectRole);
        btnRun2->setStyleSheet(useSandbox
            ? "background:#16a34a;color:#fff;font-weight:bold;padding:4px 18px;"
            : "background:#ef4444;color:#fff;font-weight:bold;padding:4px 18px;");
        connect(bb2, &QDialogButtonBox::accepted, dlg2, &QDialog::accept);
        connect(bb2, &QDialogButtonBox::rejected, dlg2, &QDialog::reject);
        lay2->addWidget(bb2);
        if (dlg2->exec() != QDialog::Accepted) { dlg2->deleteLater(); return; }
        dlg2->deleteLater();

        /* Rilancia esecuzione — riusa la funzione già in uso per sandbox/locale */
        m_executorOutput.clear();
        if (m_execProc) { m_execProc->kill(); m_execProc->deleteLater(); m_execProc = nullptr; }
        m_execProc = new QProcess(this);
        m_execProc->setProcessChannelMode(QProcess::MergedChannels);
        auto tmrR = QSharedPointer<QElapsedTimer>::create();
        tmrR->start();

        auto onFinishedR = [this, tmrR](int exitCode, QProcess::ExitStatus) {
            const double ms  = tmrR->elapsed();
            const QString out = QString::fromUtf8(m_execProc->readAll());
            m_execProc->deleteLater();
            m_execProc = nullptr;
            m_executorOutput = out;
            const QString od = PrismaluxPaths::sanitizeErrorOutput(out);
            QTextCursor c(m_log->document());
            c.movePosition(QTextCursor::End);
            c.insertHtml(buildToolStrip(QString(), od, exitCode, ms));
            m_log->moveCursor(QTextCursor::End);
        };

        if (useSandbox) {
            const QSettings ss2("Prismalux", "GUI");
            const QString img = ss2.value(P::SK::kSandboxImage, "python:3.11-slim").toString();
            const QString mem = QString::number(ss2.value(P::SK::kSandboxMemory, 256).toInt()) + "m";
            connect(m_execProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, onFinishedR);
            m_execProc->start("docker", {"run","--rm","-i","--network","none",
                "--memory", mem, "--memory-swap", mem,
                "--cpus","1","--security-opt","no-new-privileges",
                img, "python3","-c", pyCode});
        } else {
            connect(m_execProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, onFinishedR);
            m_execProc->start("python3", {"-c", pyCode});
        }
        return;
    }

    /* ── Ricerca online → salva in RAG/RICERCA/<slug>.md ── */
    if (s.startsWith("websearch:")) {
        const QString q64  = s.mid(10);
        const QString query = QString::fromUtf8(
            QByteArray::fromBase64(q64.toLatin1(), QByteArray::Base64UrlEncoding)).trimmed();
        if (query.isEmpty()) return;

        /* Dialog modifica query: permette di raffinare (es. "conosci X?" → "X") */
        bool inputOk = false;
        const QString editedQuery = QInputDialog::getText(
            this,
            "\xf0\x9f\x94\x8d  Cerca online",
            "Modifica la query di ricerca\n"
            "(usa solo nome/cognome o azienda — non l'intera domanda):",
            QLineEdit::Normal, query, &inputOk);
        if (!inputOk || editedQuery.trimmed().isEmpty()) return;
        const QString finalQuery = editedQuery.trimmed();

        /* Cartella destinazione */
        const QString ragDir = P::ragDir() + "/RICERCA";
        QDir().mkpath(ragDir);
        /* Slug filename: primi 40 char, senza caratteri speciali */
        QString slug = finalQuery.left(40);
        slug.replace(QRegularExpression("[^a-zA-Z0-9_\xc3\xa0\xc3\xa8\xc3\xac\xc3\xb2\xc3\xb9 ]"), "_");
        slug = slug.simplified().replace(' ', '_');
        const QString outFile = ragDir + "/" +
            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_" + slug + ".md";

        /* Script Python: usa duckduckgo_search (pip install duckduckgo-search) */
        const QString script = QString(
            "import sys, json, datetime\n"
            "query = %1\n"
            "out_file = %2\n"
            "try:\n"
            "    from duckduckgo_search import DDGS\n"
            "    results = []\n"
            "    with DDGS() as ddgs:\n"
            "        for r in ddgs.text(query, max_results=5):\n"
            "            results.append(r)\n"
            "    if not results:\n"
            "        print('NORESULT', flush=True)\n"
            "        sys.exit(0)\n"
            "    lines = [f'# Ricerca: {query}', f'Data: {datetime.date.today()}', '']\n"
            "    for i, r in enumerate(results, 1):\n"
            "        lines.append(f'## {i}. {r.get(\"title\",\"\")}' )\n"
            "        lines.append(f'URL: {r.get(\"href\",\"\")}')\n"
            "        lines.append(r.get('body',''))\n"
            "        lines.append('')\n"
            "    with open(out_file, 'w', encoding='utf-8') as f:\n"
            "        f.write('\\n'.join(lines))\n"
            "    print('DONE:' + out_file, flush=True)\n"
            "except ImportError:\n"
            "    print('NODEPS', flush=True)\n"
            "    sys.exit(2)\n"
            "except Exception as e:\n"
            "    print(f'ERROR:{e}', flush=True)\n"
            "    sys.exit(1)\n"
        ).arg(QString("r\"\"\"%1\"\"\"").arg(finalQuery))
         .arg(QString("r\"%1\"").arg(outFile));

        auto* proc = new QProcess(this);
        /* Banner avvio */
        m_log->moveCursor(QTextCursor::End);
        m_log->insertHtml(
            "<p style='color:#60a5fa;font-size:11px;margin:4px 0;'>"
            "\xf0\x9f\x94\x8d  Ricerca in corso: <i>" +
            finalQuery.toHtmlEscaped() + "</i>...</p>");

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, finalQuery, outFile](int code, QProcess::ExitStatus) {
            proc->deleteLater();
            const QString out = proc->readAllStandardOutput().trimmed();
            m_log->moveCursor(QTextCursor::End);
            if (out.startsWith("DONE:")) {
                m_log->insertHtml(
                    "<p style='color:#4ade80;font-size:11px;margin:4px 0;'>"
                    "\xe2\x9c\x85  Salvato in RAG/RICERCA. "
                    "Fai una nuova domanda per usare il contesto.</p>");
                /* Inietta nel RAG automaticamente se RagGraph disponibile */
                emit onlineSearchResultReady(outFile, finalQuery);
            } else if (out == "NODEPS") {
                m_log->insertHtml(
                    "<p style='color:#facc15;font-size:11px;margin:4px 0;'>"
                    "\xe2\x9a\xa0  Installa prima: "
                    "<code>pip install duckduckgo-search</code></p>");
            } else if (out == "NORESULT") {
                const QString q64i = finalQuery.toUtf8()
                    .toBase64(QByteArray::Base64UrlEncoding);
                m_log->insertHtml(
                    "<p style='color:#94a3b8;font-size:11px;margin:4px 0;'>"
                    "\xf0\x9f\x94\x8d  Nessun risultato trovato per <i>" +
                    finalQuery.toHtmlEscaped() + "</i>. "
                    "Se conosci la risposta: "
                    "<a href='insertinfo:" + q64i + "' style='color:#60a5fa;'>"
                    "aggiorna informazioni</a></p>");
            } else {
                const QString q64i = finalQuery.toUtf8()
                    .toBase64(QByteArray::Base64UrlEncoding);
                m_log->insertHtml(
                    "<p style='color:#f87171;font-size:11px;margin:4px 0;'>"
                    "\xe2\x9d\x8c  Ricerca non disponibile (offline o errore " +
                    QString::number(code) + "). "
                    "Se conosci la risposta: "
                    "<a href='insertinfo:" + q64i + "' style='color:#60a5fa;'>"
                    "aggiorna informazioni</a></p>");
            }
        });
        proc->start("python3", {"-c", script});
        return;
    }

    if (!s.startsWith("copy:") && !s.startsWith("tts:") && !s.startsWith("edit:")) return;
    /* Formato nuovo: "copy:N:BASE64URL" — il testo è embedded nell'href.
       Formato vecchio: "copy:N"          — fallback su m_bubbleTexts. */
    const int c1 = s.indexOf(':');              // colon dopo "copy"/"tts"/"edit"
    const int c2 = s.indexOf(':', c1 + 1);      // secondo colon (nuovo formato), -1 se vecchio
    bool ok = false;
    const int idx = s.mid(c1 + 1, c2 < 0 ? -1 : c2 - c1 - 1).toInt(&ok);
    if (!ok) return;
    QString text;
    const QString origB64 = (c2 > 0) ? s.mid(c2 + 1) : QString();
    if (c2 > 0) {
        text = QString::fromUtf8(QByteArray::fromBase64(
            origB64.toLatin1(), QByteArray::Base64UrlEncoding));
    } else {
        if (!m_bubbleTexts.contains(idx)) return;
        text = m_bubbleTexts.value(idx);
    }

    if (s.startsWith("edit:")) {
        /* Apre un dialog di modifica: l'utente può editare il testo liberamente
           e scegliere se rimpiazzare la bolla nel log o inviare come nuovo task */
        auto* dlg  = new QDialog(this);
        dlg->setWindowTitle(tr("\xe2\x9c\x8f\xef\xb8\x8f  Modifica testo"));
        dlg->setMinimumSize(dpiSize(520, 360));
        auto* dlgLay = new QVBoxLayout(dlg);
        dlgLay->setSpacing(10);

        auto* hint = new QLabel(
            "<small>\xe2\x84\xb9  Modifica il testo. <b>Invia come task</b> lo carica nel campo "
            "input pronto per essere rielaborato dall'AI. "
            "<b>Aggiorna bolla</b> sostituisce il testo nel log.</small>");
        hint->setWordWrap(true);
        dlgLay->addWidget(hint);

        auto* editor = new QTextEdit(dlg);
        editor->setPlainText(text.trimmed());
        editor->setFocus();
        dlgLay->addWidget(editor, 1);

        auto* btnBox = new QDialogButtonBox(dlg);
        auto* btnTask   = btnBox->addButton("Invia come task \xe2\x96\xb6",
                                            QDialogButtonBox::AcceptRole);
        auto* btnUpdate = btnBox->addButton("Aggiorna bolla \xf0\x9f\x94\x84",
                                            QDialogButtonBox::ApplyRole);
        auto* btnCancel = btnBox->addButton(QDialogButtonBox::Cancel);
        btnCancel->setText(tr("Annulla"));
        dlgLay->addWidget(btnBox);

        /* done(1)=task, done(2)=aggiorna bolla, reject=annulla */
        connect(btnTask,   &QPushButton::clicked, dlg, [dlg]{ dlg->done(1); });
        connect(btnUpdate, &QPushButton::clicked, dlg, [dlg]{ dlg->done(2); });
        connect(btnBox,    &QDialogButtonBox::rejected, dlg, &QDialog::reject);

        const int dlgResult = dlg->exec();

        if (dlgResult == 1) {
            m_input->setPlainText(editor->toPlainText().trimmed());
            m_input->setFocus();
            m_input->moveCursor(QTextCursor::End);
            dlg->deleteLater();
            /* Avvia il pipeline dopo la chiusura del dialog */
            QTimer::singleShot(0, this, &AgentiPage::onBtnRunDelayedClick);
            return;
        }
        if (dlgResult == 2) {
            const QString newText = editor->toPlainText().trimmed();
            if (!newText.isEmpty() && !origB64.isEmpty()) {
                m_undoHtmlStack.push(m_log->toHtml());
                const QString newB64 = newText.left(4096).toUtf8().toBase64(
                    QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
                QString html = m_log->toHtml();
                html.replace(origB64, newB64);
                m_log->setHtml(html);
                m_log->moveCursor(QTextCursor::End);
            }
        }
        dlg->deleteLater();
        return;
    }

    if (s.startsWith("copy:")) {
        /* Rimuovi tag HTML prima di copiare */
        QString plain = text;
        plain.remove(QRegularExpression("<[^>]*>"));
        plain = plain.trimmed();
        QGuiApplication::clipboard()->setText(plain.isEmpty() ? text : plain);
        /* Notifica visiva */
        QToolTip::showText(QCursor::pos(),
            "\xe2\x9c\x85  Il testo \xc3\xa8 stato copiato in memoria",
            nullptr, {}, 2000);
    } else {
        /* TTS — rimuovi tag HTML, limita a 400 parole */
        QString plain = text;
        plain.remove(QRegularExpression("<[^>]*>"));
        plain = plain.trimmed();
        if (plain.isEmpty()) plain = text;
        QStringList words = plain.split(' ', Qt::SkipEmptyParts);
        if (words.size() > 400) words = words.mid(words.size() - 400);
        _ttsPlay(words.join(" "));
    }
}

void AgentiPage::onBtnRunDelayedClick()
{
    m_btnRun->click();
}

void AgentiPage::onLogContextMenuRequested(const QPoint& pos)
{
    const QString sel  = m_log->textCursor().selectedText();
    const bool hasSel  = !sel.isEmpty();
    const QString label = hasSel ? "selezione" : "tutto";

    QMenu menu(m_log);
    QAction* actCopy = menu.addAction(
        "\xf0\x9f\x97\x82  Copia " + label);
    QAction* actRead = menu.addAction(
        "\xf0\x9f\x8e\x99  Leggi " + label);

    QAction* chosen = menu.exec(m_log->mapToGlobal(pos));
    const QString txt = hasSel ? sel : m_log->toPlainText();

    if (chosen == actCopy) {
        QGuiApplication::clipboard()->setText(txt);
    } else if (chosen == actRead) {
        QStringList words = txt.split(' ', Qt::SkipEmptyParts);
        if (words.size() > 400) words = words.mid(words.size() - 400);
        _ttsPlay(words.join(" "));
    }
}

void AgentiPage::onBtnRagToggled(bool on)
{
    m_ragPanel->setVisible(on);
    m_btnRag->setText(on ? "\xf0\x9f\x93\x8e  RAG \xe2\x97\xa4"
                         : "\xf0\x9f\x93\x8e  RAG");
}

void AgentiPage::onBtnHintHideClicked()
{
    m_hintWidget->setVisible(false);
    AppConfig::s().setValue("ui/hintVisible", false);
}

void AgentiPage::onBtnRunClicked()
{
    /* ── Conversa mode: il Run button gestisce la voce ────────────────────────
       - Se AI occupata → stop
       - Se in registrazione → ferma e trascrive
       - Se in trascrizione → attendi
       - Se input vuoto → avvia registrazione (auto-loop se Voce è attiva)
       - Se input ha testo → invia come chat normale (fall-through)
       ────────────────────────────────────────────────────────────────────── */
    if (m_modeBtn && m_modeBtn->currentMode() == TriModeButton::Conversa) {
        if (m_ai->busy()) { m_ai->abort(); return; }
        if (m_sttState == SttState::Recording)    { onSttTimeout(); return; }
        if (m_sttState == SttState::Transcribing) { return; }
        if (m_input->toPlainText().trimmed().isEmpty()) {
            /* Avvia registrazione; auto-loop basato sullo stato del pulsante Voce */
            m_voiceLoopActive = true; /* in Conversa il loop è sempre attivo */
            _sttStartRecording();
            return;
        }
        /* Input non vuoto → invia come chat (fall-through) */
    }

    if (m_ai->busy()) { m_ai->abort(); return; }

    /* Avviso se i tool sono abilitati ma il modello non supporta function calling */
    if (m_toolsEnabled && !isToolCapable(m_ai->model())) {
        m_log->moveCursor(QTextCursor::End);
        m_log->insertHtml(
            "<p style='color:#f59e0b;font-size:11px;font-style:italic;margin:2px 0;'>"
            "\xf0\x9f\x94\xa7 Il modello <b>" + m_ai->model().toHtmlEscaped() +
            "</b> non supporta il function calling: i tool verranno ignorati. "
            "Seleziona qwen3, llama3.1, mistral-nemo o un altro modello tool-capable.</p>");
        m_log->append({});
    }

    /* ── Agente ricerca web: intercetta domande che richiedono info online ── */
    {
        const QString userMsg = m_input->toPlainText().trimmed();
        if (!userMsg.isEmpty() && _detectWebIntent(userMsg)) {
            m_input->clear();
            runWebSearchAgent(userMsg);
            return;
        }
    }

    /* Agente Autonomo: intercetta prima della pipeline normale */
    if (m_autoEnabled && !m_modePipeline) {
        const QString task = m_input->toPlainText().trimmed();
        if (task.isEmpty()) return;
        /* Reset stato ciclo ReAct */
        m_autoHistory    = QJsonArray();
        m_autoStep       = 0;
        m_autoBuf.clear();
        m_autoLastUserMsg = task;
        m_input->clear();
        /* Banner info — solo alla prima chat in modalità autonoma */
        if (!m_autoMsgShown) {
            m_autoMsgShown = true;
            m_log->moveCursor(QTextCursor::End);
            m_log->insertHtml(
                "<p style='color:#818cf8;font-size:11px;text-align:center;"
                "font-style:italic;margin:2px 0;'>"
                "\xf0\x9f\xa4\x96 Agente Autonomo attivato &mdash; "
                "l\xe2\x80\x99" "AI pianifica e usa strumenti automaticamente (max 8 step)</p>");
        }
        /* Bolla utente */
        { int idx = m_bubbleIdx++;
          m_bubbleTexts[idx] = task;
          m_log->moveCursor(QTextCursor::End);
          m_log->insertHtml(buildUserBubble(task, idx)); }
        m_log->append("");
        emit pipelineStatus(0, "\xf0\x9f\xa4\x96  Agente autonomo in esecuzione...");
        _setRunBusy(true);
        runAutonomousAgent();
        return;
    }

    /* Team di agenti ON: forza singolo agente con system prompt "team".
     * Il pipeline esistente viene riusato con maxShots=1 — il system prompt
     * viene sovrascritto in runAgent() quando m_btnTeam è attivo. */
    if (m_btnTeam && m_btnTeam->isChecked()) {
        m_cfgDlg->numAgentsSpinBox()->setValue(1);
        m_maxShots   = 1;
        m_modePipeline = false;
        runPipeline();
        return;
    }

    if (!m_modePipeline) {
        /* Modalità Singolo: forza 1 agente */
        m_cfgDlg->numAgentsSpinBox()->setValue(1);
        m_maxShots = 1;
    }
    const int mode = m_cmbMode->currentIndex();
    if      (mode == 10) runConsiglioScientifico();
    else if (mode == 2)  runMathTheory();
    else                 runPipeline();
}

void AgentiPage::onSymbolBtnClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn || !m_input) return;
    m_input->insertPlainText(btn->property("symbol").toString());
}

void AgentiPage::onBtnSymbolsClicked()
{
    const bool nowVisible = m_symbolsScrollArea && !m_symbolsScrollArea->isVisible();
    if (nowVisible) {
        /* Chiudi Tool Veloci e Tool Lenti */
        if (m_btnToolsToggle && m_btnToolsToggle->isChecked()) {
            m_btnToolsToggle->blockSignals(true);
            m_btnToolsToggle->setChecked(false);
            m_btnToolsToggle->blockSignals(false);
            if (m_toolsPanel) m_toolsPanel->setVisible(false);
        }
        if (m_btnMcpToggle && m_btnMcpToggle->isChecked()) {
            m_btnMcpToggle->blockSignals(true);
            m_btnMcpToggle->setChecked(false);
            m_btnMcpToggle->blockSignals(false);
            if (m_mcpPanel) m_mcpPanel->setVisible(false);
        }
    }
    if (m_symbolsScrollArea) m_symbolsScrollArea->setVisible(nowVisible);
    if (m_symbolSearch) {
        m_symbolSearch->setVisible(nowVisible);
        if (!nowVisible) m_symbolSearch->clear();
    }
}

void AgentiPage::onBtnTranslateClicked()
{
    if (m_ai->busy()) {
        m_log->append("\xe2\x9a\xa0  Un'altra operazione \xc3\xa8 in corso.");
        return;
    }
    const QString inputText = m_input->toPlainText().trimmed();
    if (inputText.isEmpty()) {
        m_log->append("\xe2\x9a\xa0  Inserisci il testo da tradurre nel campo input.");
        return;
    }

    QString src, dst, model;
    if (!_buildTranslateDialog(inputText, &src, &dst, &model))
        return;

    _startTranslation(src, dst, model, inputText);
}

/* ── Dialog selezione lingue/modello per la traduzione.
   Restituisce true se l'utente ha confermato, false se ha annullato.
   Scrive i parametri scelti nelle variabili puntate.               ── */
bool AgentiPage::_buildTranslateDialog(const QString& inputText,
                                       QString* outSrc, QString* outDst,
                                       QString* outModel)
{
    static const QStringList kLangs = {
        "Auto-rileva",
        "Italiano", "Inglese", "Francese", "Spagnolo", "Portoghese",
        "Tedesco", "Olandese", "Svedese", "Norvegese", "Danese",
        "Russo", "Ucraino", "Polacco", "Ceco", "Slovacco",
        "Cinese (Mandarino)", "Giapponese", "Coreano",
        "Arabo", "Persiano (Farsi)", "Turco", "Ebraico",
        "Hindi", "Bengalese", "Urdu",
        "Swahili", "Hausa", "Somalo",
        "Greco", "Rumeno", "Ungherese", "Finlandese",
        "Catalano", "Galiziano", "Basco",
        "Indonesiano", "Malese", "Tagalog (Filippino)",
        "Thai", "Vietnamita"
    };

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x8c\x90  Traduci testo"));
    dlg->setFixedWidth(dpiScale(420));
    auto* dlgLay = new QVBoxLayout(dlg);
    dlgLay->setSpacing(10);

    auto* srcRow = new QHBoxLayout;
    srcRow->addWidget(new QLabel("Da:", dlg));
    auto* cmbSrc = new QComboBox(dlg);
    cmbSrc->addItems(kLangs);
    int si = cmbSrc->findText(m_translateSrcLang);
    if (si >= 0) cmbSrc->setCurrentIndex(si);
    srcRow->addWidget(cmbSrc, 1);
    dlgLay->addLayout(srcRow);

    auto* dstRow = new QHBoxLayout;
    dstRow->addWidget(new QLabel("A:", dlg));
    auto* cmbDst = new QComboBox(dlg);
    for (const QString& l : kLangs)
        if (l != "Auto-rileva") cmbDst->addItem(l);
    cmbDst->setCurrentText(m_translateDstLang.isEmpty() ? "Italiano" : m_translateDstLang);
    dstRow->addWidget(cmbDst, 1);
    dlgLay->addLayout(dstRow);

    auto* mdlRow = new QHBoxLayout;
    mdlRow->addWidget(new QLabel("Modello:", dlg));
    auto* cmbMdl = new QComboBox(dlg);
    for (auto& mi : m_modelInfos) cmbMdl->addItem(mi.name);
    if (cmbMdl->count() == 0) cmbMdl->addItem(m_ai->model());
    else {
        int idx = cmbMdl->findText(m_ai->model());
        if (idx >= 0) cmbMdl->setCurrentIndex(idx);
    }
    mdlRow->addWidget(cmbMdl, 1);
    dlgLay->addLayout(mdlRow);

    auto* previewLbl = new QLabel(
        QString("\xf0\x9f\x93\x9d  Testo: <i>%1</i>")
        .arg(inputText.length() > 80
             ? inputText.left(80) + "\xe2\x80\xa6"
             : inputText), dlg);
    previewLbl->setWordWrap(true);
    previewLbl->setStyleSheet("color:#9ca3af; font-size:11px;");
    dlgLay->addWidget(previewLbl);

    auto* bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    bb->button(QDialogButtonBox::Ok)->setText(tr("\xf0\x9f\x8c\x90  Traduci"));
    dlgLay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return false; }

    *outSrc   = cmbSrc->currentText();
    *outDst   = cmbDst->currentText();
    *outModel = cmbMdl->currentText();
    dlg->deleteLater();
    return true;
}

/* ── Imposta lo stato e lancia la chat di traduzione ── */
void AgentiPage::_startTranslation(const QString& src, const QString& dst,
                                   const QString& model, const QString& inputText)
{
    m_translateSrcLang = src;
    m_translateDstLang = dst;

    m_preTranslateModel = m_ai->model();
    m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), model);

    QString prompt;
    if (src == "Auto-rileva")
        prompt = QString("Traducimi il seguente testo nella lingua %1. "
                         "Mantieni il significato originale nel modo pi\xc3\xb9 fedele possibile. "
                         "Rispondi SOLO con la traduzione, senza commenti aggiuntivi.\n\n"
                         "Testo:\n%2").arg(dst).arg(inputText);
    else
        prompt = QString("Traducimi il seguente testo da %1 a %2. "
                         "Mantieni il significato originale nel modo pi\xc3\xb9 fedele possibile. "
                         "Rispondi SOLO con la traduzione, senza commenti aggiuntivi.\n\n"
                         "Testo:\n%3")
                 .arg(src).arg(dst).arg(inputText);

    const QString sys =
        "Sei un traduttore professionale. "
        "Rispondi SEMPRE e SOLO con la traduzione richiesta, senza spiegazioni.";

    m_log->clear();
    m_log->append(QString("\xf0\x9f\x8c\x90  Traduzione  <b>%1</b> \xe2\x86\x92 <b>%2</b>"
                          "  [modello: %3]\n")
                  .arg(src, dst, model));
    m_log->append(QString(43, QChar(0x2500)));

    m_taskOriginal  = inputText;
    m_translateBuf.clear();
    m_pendingMode = OpMode::Idle;
    m_opMode      = OpMode::Translating;

    _setRunBusy(true);
    m_btnTranslate->setEnabled(false);
    m_waitLbl->setVisible(true);

    m_ai->chat(sys, prompt);
}

void AgentiPage::onBtnDocClicked()
{
    static const QString filter =
        "Tutti i file supportati "
        "(*.txt *.md *.csv *.json *.py *.cpp *.c *.h *.html *.xml *.rst *.log "
        "*.pdf *.xls *.xlsx *.ods *.ots *.fods "
        "*.png *.jpg *.jpeg *.gif *.webp);;"
        "Documenti (*.txt *.md *.csv *.json *.py *.cpp *.c *.h *.html *.xml *.rst *.log);;"
        "PDF (*.pdf);;"
        "Fogli di calcolo (*.xls *.xlsx *.ods *.ots *.fods);;"
        "Immagini (*.png *.jpg *.jpeg *.gif *.webp);;"
        "Tutti (*)";
    const QString fp = QFileDialog::getOpenFileName(
        this, "Allega file", QString(), filter);
    if (!fp.isEmpty()) loadDroppedFile(fp);
}

void AgentiPage::onBtnVoiceClicked()
{
    /* ── Stop durante registrazione ── */
    if (m_sttState == SttState::Recording) {
        if (m_recProc) { m_recProc->kill(); m_recProc->waitForFinished(P::kProcKillGraceMs); }
        m_sttState = SttState::Idle;
        m_btnVoice->setText(tr("\xf0\x9f\x8e\xa4 Trascrivi parlato"));
        m_btnVoice->setProperty("danger","false");
        P::repolish(m_btnVoice);
        m_btnVoice->setEnabled(true);
        return;
    }
    /* ── Ignora clic mentre trascrive o scarica ── */
    if (m_sttState == SttState::Transcribing ||
        m_sttState == SttState::Downloading) return;

    /* ── Controlla binario ── */
    if (SttWhisper::whisperBin().isEmpty()) {
        m_log->append(
            "<p style='color:#e2e8f0;'>"
            "\xe2\x9a\xa0  <b>whisper-cli non trovato.</b> "
            "<a href=\"settings:trascrivi\" style=\"color:#93c5fd;\">"
            "Clicca qui</a> per aprire le Impostazioni &rarr; Trascrivi"
            " e installare il riconoscimento vocale."
            "</p>");
        return;
    }

    /* ── Modello assente: avvia download automatico ── */
    if (SttWhisper::whisperModel().isEmpty()) {
        downloadWhisperModel();
        return;
    }

    _sttStartRecording();
}

void AgentiPage::onNumAgentsChanged(int v)
{
    m_maxShots = v;
}

void AgentiPage::onCmbModePresetChanged(int idx)
{
    if (idx < 3) { m_autoLbl->setVisible(false); return; }
    m_cfgDlg->applyPreset(idx);
    static const char* lbls[] = {
        "\xf0\x9f\x93\x90  Preset Matematica applicato.",
        "\xf0\x9f\x92\xbb  Preset Informatica applicato.",
        "\xf0\x9f\x94\x90  Preset Sicurezza applicato.",
        "\xe2\x9a\x9b\xef\xb8\x8f  Preset Fisica applicato.",
        "\xf0\x9f\xa7\xaa  Preset Chimica applicato.",
        "\xf0\x9f\x8c\x90  Preset Lingue applicato.",
        "\xf0\x9f\x8c\x8d  Preset Generico applicato.",
    };
    m_autoLbl->setText(QString::fromUtf8(lbls[idx - 3])
                       + "  \xe2\x80\x94  Puoi modificarli in \xe2\x9a\x99\xef\xb8\x8f Configura Agenti.");
    m_autoLbl->setVisible(true);
}

void AgentiPage::onCmbModeMathChanged(int idx)
{
    if (idx != 2) return;
    static const QStringList mathKw  = {"r1","math","reason","think","qwq","deepseek-r"};
    static const QStringList coderKw = {"coder","code","starcoder","codellama","qwen2.5-coder"};
    static const QStringList largeKw = {"qwen3","llama3","gemma","mistral","phi"};

    auto bestMatch = [&](int i) -> int {
        QComboBox* cb = m_cfgDlg->modelCombo(i);
        for (int p = 0; p < cb->count(); p++) {
            QString n = cb->itemText(p).toLower();
            for (auto& kw : mathKw) if (n.contains(kw)) return p;
        }
        for (int p = 0; p < cb->count(); p++) {
            QString n = cb->itemText(p).toLower();
            for (auto& kw : coderKw) if (n.contains(kw)) return p;
        }
        for (int p = 0; p < cb->count(); p++) {
            QString n = cb->itemText(p).toLower();
            for (auto& kw : largeKw) if (n.contains(kw)) return p;
        }
        return -1;
    };
    int assigned = 0;
    for (int i = 0; i < MAX_AGENTS; i++) {
        if (!m_cfgDlg->enabledChk(i)->isChecked()) continue;
        int best = bestMatch(i);
        if (best >= 0) { m_cfgDlg->modelCombo(i)->setCurrentIndex(best); assigned++; }
    }
    m_autoLbl->setText(assigned > 0
        ? "\xf0\x9f\xa7\xae  Modelli reasoning/coder pre-selezionati per Matematico Teorico."
        : "\xf0\x9f\x92\xa1  Consigliato: modelli reasoning (deepseek-r1, qwen3, qwq).");
    m_autoLbl->setVisible(true);
}

void AgentiPage::onAiAborted()
{
    m_waitLbl->setVisible(false);

    /* Rimuove il contenuto parziale dello streaming (testo grezzo non ancora
       convertito in bolla) che va da m_agentBlockStart alla fine del documento. */
    if (m_agentBlockStart > 0) {
        QTextCursor sel(m_log->document());
        sel.setPosition(m_agentBlockStart);
        sel.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        if (!sel.selectedText().trimmed().isEmpty())
            sel.removeSelectedText();
        m_agentBlockStart = 0;
    }

    m_opMode       = OpMode::Idle;
    m_currentAgent = 0;
    m_agentOutputs.clear();
    m_byzStep      = 0;
    /* Reset background mode se abort avviene mentre siamo in background */
    m_bgMode = false;
    m_bgBuffer.clear();
    m_bgHtmlSave.clear();
    _setRunBusy(false);
    emit pipelineStatus(0, "\xe2\x9c\x8b  Interrotto");
    for (int i = 0; i < MAX_AGENTS; i++)
        m_cfgDlg->enabledChk(i)->setStyleSheet("");
    m_log->moveCursor(QTextCursor::End);
    m_log->append("\n\xe2\x9c\x8b  Pipeline interrotta.");
}

/* ──────────────────────────────────────────────────────────────
   _ingestRagFiles — gestisce i file droppati nella zona RAG
   specializzata per PDF / .txt / .md.

   .txt / .md → lettura diretta + addEntry() in m_ragInline
   .pdf       → loadDroppedFile() (estrazione asincrona)
   ────────────────────────────────────────────────────────────── */
void AgentiPage::_ingestRagFiles(const QList<QUrl>& urls)
{
    /* Cartelle RAG persistenti (stesse scansionate da RagGraph) */
    const QString ragDir  = P::ragDir();
    const QString ragDocs = QDir::cleanPath(QDir::homePath() + "/prismalux_rag_docs");

    for (const QUrl& u : urls) {
        const QString path = u.toLocalFile();
        if (path.isEmpty()) continue;

        /* ── Copia in RAG/ se il file è fuori dalle cartelle persistenti (con conferma) ── */
        const QString canon = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        const bool inRag = canon.startsWith(ragDir) || canon.startsWith(ragDocs);
        if (!inRag) {
            const QString dest = ragDir + "/" + QFileInfo(path).fileName();
            if (!QFile::exists(dest)) {
                const QString msg = QString(
                    "Vuoi copiare \"%1\" nella cartella RAG?\n\n"
                    "Destinazione: %2\n\n"
                    "Copiando il file, Prismalux potr\xc3\xa0 indicizzarlo automaticamente "
                    "e usarlo come base di conoscenza in tutte le sessioni future.")
                    .arg(QFileInfo(path).fileName(), ragDir);
                const auto ans = QMessageBox::question(
                    this, "Copia nella cartella RAG", msg,
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                if (ans == QMessageBox::Yes) {
                    QDir().mkpath(ragDir);
                    if (QFile::copy(path, dest)) {
                        if (m_ragStatusLbl)
                            m_ragStatusLbl->setText(
                                "\xf0\x9f\x93\x84  Copiato in RAG/ \xe2\x80\x94 "
                                "il documento \xc3\xa8 ora persistente");
                    }
                }
            }
        }

        const QString pl = path.toLower();

        if (pl.endsWith(".txt") || pl.endsWith(".md")) {
            /* Testo: lettura sincrona → addEntry nel RAG inline */
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QString content = QString::fromUtf8(f.readAll()).trimmed();
            f.close();
            if (content.isEmpty()) continue;

            m_ragIngesting = true;
            if (m_ragStatusLbl)
                m_ragStatusLbl->setText(
                    "\xf0\x9f\x94\x84  Indicizzazione in corso...");

            if (m_ragInline)
                m_ragInline->addEntry(QFileInfo(path).fileName(), content);

            /* Completa subito (sincrono) */
            QTimer::singleShot(0, this, &AgentiPage::onRagIngestionDone);

        } else if (pl.endsWith(".pdf")) {
            /* PDF: estrazione asincrona via loadDroppedFile.
               Il testo estratto va in m_docContext (disponibile per la prossima query).
               Mostriamo feedback immediato e resettiamo dopo 2s. */
            m_ragIngesting = true;
            if (m_ragStatusLbl)
                m_ragStatusLbl->setText(
                    "\xf0\x9f\x94\x84  Estrazione PDF in corso...");
            if (m_ragDropZone)
                m_ragDropZone->setText(
                    "\xf0\x9f\x93\x84  Estrazione PDF in corso...");

            loadDroppedFile(path);

            /* Timeout heuristico: il completamento reale è gestito da loadDroppedFile
               tramite il log; qui resettiamo solo il feedback visivo della zona drop. */
            QTimer::singleShot(2500, this, &AgentiPage::onRagIngestionDone);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────
   Web reading via RAG — onRagUrlAddClicked / onRagUrlFetched
   ───────────────────────────────────────────────────────────────── */
void AgentiPage::onRagUrlAddClicked()
{
    if (!m_ragUrlLine) return;
    const QString urlStr = m_ragUrlLine->text().trimmed();
    if (!urlStr.startsWith("http://") && !urlStr.startsWith("https://")) {
        if (m_ragStatusLbl) m_ragStatusLbl->setText(tr("\xe2\x9a\xa0\xef\xb8\x8f  URL non valido"));
        return;
    }

    if (!m_ragUrlNam)
        m_ragUrlNam = new QNetworkAccessManager(this);

    if (m_ragUrlReply) {
        m_ragUrlReply->abort();
        m_ragUrlReply->deleteLater();
        m_ragUrlReply = nullptr;
    }

    if (m_ragStatusLbl) m_ragStatusLbl->setText(tr("\xf0\x9f\x8c\x90  Recupero pagina..."));

    QNetworkRequest req;
    req.setUrl(QUrl(urlStr));
    req.setHeader(QNetworkRequest::UserAgentHeader, "Prismalux/1.0 (RAG web reader)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_ragUrlReply = m_ragUrlNam->get(req);
    connect(m_ragUrlReply, &QNetworkReply::finished, this, &AgentiPage::onRagUrlFetched);
}

void AgentiPage::onRagUrlFetched()
{
    if (!m_ragUrlReply) return;
    const QUrl  finalUrl = m_ragUrlReply->url();
    const auto  err      = m_ragUrlReply->error();
    const QByteArray raw = m_ragUrlReply->readAll();
    m_ragUrlReply->deleteLater();
    m_ragUrlReply = nullptr;

    if (err != QNetworkReply::NoError) {
        if (m_ragStatusLbl) m_ragStatusLbl->setText("\xe2\x9d\x8c  Errore rete: " +
            QString::number(static_cast<int>(err)));
        LogBus::post(QString("\xe2\x9d\x8c AI UI: Errore rete RAG URL: %1 (codice %2)")
                     .arg(finalUrl.toString())
                     .arg(static_cast<int>(err)));
        return;
    }

    QString text = QString::fromUtf8(raw);

    /* Rimuovi tag HTML e decodifica entità comuni */
    static const QRegularExpression reTag("<[^>]+>");
    static const QRegularExpression reWs("\\s{3,}");
    text.replace(reTag, " ");
    text.replace("&amp;",  "&");
    text.replace("&lt;",   "<");
    text.replace("&gt;",   ">");
    text.replace("&nbsp;", " ");
    text.replace("&quot;", "\"");
    text.replace("&#39;",  "'");
    text = text.replace(reWs, "\n\n").trimmed();

    /* Tronca a ~8000 caratteri per non saturare il contesto */
    if (text.size() > 8000)
        text = text.left(8000) + "\n\n[...]";

    if (text.isEmpty()) {
        if (m_ragStatusLbl) m_ragStatusLbl->setText(tr("\xe2\x9a\xa0\xef\xb8\x8f  Nessun testo trovato"));
        return;
    }

    const QString title = finalUrl.host() + finalUrl.path();
    if (m_ragInline) m_ragInline->addEntry(title, text);
    if (m_ragUrlLine) m_ragUrlLine->clear();
    if (m_ragStatusLbl) {
        m_ragStatusLbl->setText(
            QString("\xe2\x9c\x85  Pagina aggiunta (%1 car.)").arg(text.size()));
        QTimer::singleShot(3000, m_ragStatusLbl, &QLabel::clear);
    }
}

/* ══════════════════════════════════════════════════════════════
   buildToolsPanel — due pannelli separati:
     m_toolsPanel  ⚡ Tool Veloci   (Function Tools, in-process)
     m_mcpPanel    🔌 Tool Lenti    (MCP Plugin, subprocess)
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::buildToolsPanel(QVBoxLayout* lay)
{
    static const struct {
        const char* name; const char* icon;
        const char* label; const char* desc;
    } kTools[] = {
        { "calc",         "\xf0\x9f\x94\xa2", "Calcola espressioni",
          "Valuta espressioni matematiche (2+2, sqrt(16), sin(pi/4)...)" },
        { "ricerca",      "\xf0\x9f\x94\x8d", "Cerca online",
          "Cerca informazioni su internet via DuckDuckGo" },
        { "fetch_url",    "\xf0\x9f\x8c\x90", "Scarica pagina",
          "Scarica e legge il contenuto di una pagina web" },
        { "leggi_file",   "\xf0\x9f\x93\x84", "Leggi file",
          "Legge un file di testo dal disco (percorso assoluto)" },
        { "lista_file",   "\xf0\x9f\x93\x82", "Elenca cartella",
          "Mostra l'elenco dei file in una directory" },
        { "python",       "\xf0\x9f\x90\x8d", "Esegui Python",
          "Esegue un frammento di codice Python in ambiente sandbox" },
        { "search_rag",   "\xf0\x9f\x93\x9a", "Cerca documenti",
          "Cerca nei documenti che hai indicizzato nel RAG" },
        { "graph_memory", "\xf0\x9f\x95\xb8", "Memoria sessioni",
          "Cerca nella memoria persistente delle sessioni precedenti (GraphMemory)" },
        { "get_knowledge","\xf0\x9f\xa7\xa0", "Base conoscenza",
          "Legge la tua Knowledge Base personale (file KNOWLEDGE_USER/)" },
        { "spawn_agent",  "\xf0\x9f\xa4\x96", "Crea agente",
          "Avvia un sub-agente specializzato per un sotto-compito autonomo" },
    };
    constexpr int kNTools = 10;

    /* ══ PANNELLO 1: ⚡ Tool Veloci (Function Tools, in-process) ══ */
    m_toolsPanel = new QFrame(this);
    m_toolsPanel->setObjectName("symbolsPanel");
    m_toolsPanel->setVisible(false);

    auto* fastLay = new QVBoxLayout(m_toolsPanel);
    fastLay->setContentsMargins(8, 6, 8, 6);
    fastLay->setSpacing(6);

    /* Header ⚡ */
    {
        auto* hdrRow = new QWidget(m_toolsPanel);
        auto* hdrLay = new QHBoxLayout(hdrRow);
        hdrLay->setContentsMargins(0, 0, 0, 0);
        hdrLay->setSpacing(8);

        auto* hdr = new QLabel(hdrRow);
        hdr->setTextFormat(Qt::RichText);
        hdr->setText(
            "<b>\xe2\x9a\xa1 Tool Veloci</b>"
            "<span style='color:#16a34a;font-size:10px;font-weight:bold;'>"
            " \xe2\x80\x94 eseguiti in-process, &lt;1ms</span>"
            "<br><span style='color:#64748b;font-size:9px;'>"
            "Function Tools integrati: calcola, cerca, leggi file, Python, RAG\xe2\x80\xa6</span>");
        hdrLay->addWidget(hdr, 1);

        auto* btnAll  = new QPushButton("\xe2\x9c\x85  Tutti",   hdrRow);
        auto* btnNone = new QPushButton("\xe2\x96\xa1  Nessuno", hdrRow);
        btnAll->setObjectName("actionBtn");
        btnNone->setObjectName("actionBtn");
        btnAll->setFixedHeight(dpiScale(22));
        btnNone->setFixedHeight(dpiScale(22));
        hdrLay->addWidget(btnAll);
        hdrLay->addWidget(btnNone);
        fastLay->addWidget(hdrRow);

        /* Grid 2 colonne */
        auto* grid = new QWidget(m_toolsPanel);
        auto* gl   = new QGridLayout(grid);
        gl->setContentsMargins(0, 0, 0, 0);
        gl->setSpacing(4);
        gl->setColumnStretch(0, 1);
        gl->setColumnStretch(1, 1);

        for (int i = 0; i < kNTools; ++i) {
            const QString name  = QString::fromLatin1(kTools[i].name);
            const QString icon  = QString::fromUtf8(kTools[i].icon);
            const QString label = QString::fromUtf8(kTools[i].label);
            const QString desc  = QString::fromUtf8(kTools[i].desc);

            auto* chk = new QCheckBox(icon + "  " + label + "  (" + name + ")", grid);
            chk->setToolTip(desc);
            chk->setChecked(true);
            chk->setMinimumHeight(dpiScale(22));
            m_enabledTools.insert(name);
            gl->addWidget(chk, i / 2, i % 2);

            connect(chk, &QCheckBox::toggled, this, [this, name](bool on){
                if (on) m_enabledTools.insert(name);
                else    m_enabledTools.remove(name);
                onToolEnabledChanged();
            });
        }

        connect(btnAll, &QPushButton::clicked, grid, [grid]{
            for (auto* c : grid->findChildren<QCheckBox*>()) c->setChecked(true);
        });
        connect(btnNone, &QPushButton::clicked, grid, [grid]{
            for (auto* c : grid->findChildren<QCheckBox*>()) c->setChecked(false);
        });

        /* Barra di ricerca — filtra e riorganizza il grid senza buchi */
        auto* fastSearch = new QLineEdit(m_toolsPanel);
        fastSearch->setPlaceholderText("\xf0\x9f\x94\x8d  Cerca tool per nome o descrizione\xe2\x80\xa6");
        fastSearch->setClearButtonEnabled(true);
        fastSearch->setFixedHeight(dpiScale(26));
        fastLay->addWidget(fastSearch);
        fastLay->addWidget(grid);

        connect(fastSearch, &QLineEdit::textChanged, grid,
                [grid, gl](const QString& raw){
            const QString q = raw.trimmed().toLower();
            const auto chks = grid->findChildren<QCheckBox*>(
                QString(), Qt::FindDirectChildrenOnly);
            for (auto* c : chks) gl->removeWidget(c);
            int pos = 0;
            for (auto* c : chks) {
                const bool match = q.isEmpty()
                    || c->text().toLower().contains(q)
                    || c->toolTip().toLower().contains(q);
                c->setVisible(match);
                if (match) { gl->addWidget(c, pos / 2, pos % 2); ++pos; }
            }
        });
    }

    /* Hint modello tool-capable — visibile solo dentro questo pannello */
    {
        auto* hintLbl = new QLabel(
            "<span style='color:#475569;font-size:10px;'>"
            "Richiedono un modello tool-capable "
            "(qwen3, llama3.1, mistral-nemo\xe2\x80\xa6)</span>",
            m_toolsPanel);
        hintLbl->setTextFormat(Qt::RichText);
        hintLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fastLay->addWidget(hintLbl);
    }

    lay->addWidget(m_toolsPanel);

    /* ══ PANNELLO 2: 🔌 Tool Lenti — MCP Plugin (subprocess JSON-RPC) ══ */

    /* Descrizioni italiane per MCP noti; fallback automatico per gli altri */
    static const QHash<QString, QString> kMcpLabels = {
        { "ai_memory_mcp",        "Memoria AI" },
        { "android_adb_mcp",      "Android ADB" },
        { "anki_mcp",             "Flashcard Anki" },
        { "arch_analyzer_mcp",    "Analisi architettura" },
        { "devagent_mcp",         "Dev Agent" },
        { "gns3_mcp",             "Simulatore GNS3" },
        { "knowledge_mcp",        "Aggiorna conoscenza" },
        { "mypy_mcp",             "Analisi Python (mypy)" },
        { "ollama_mcp",           "Modelli Ollama" },
        { "opencode_mcp",         "Editor AI OpenCode" },
        { "owasp_mcp",            "Sicurezza OWASP" },
        { "perf_analyzer_mcp",    "Analisi performance" },
        { "prismalux_build_mcp",  "Build Prismalux" },
        { "prismalux_search_mcp", "Ricerca Prismalux" },
        { "qt_i18n_mcp",          "Traduzioni Qt" },
        { "rag_manager_mcp",      "Gestione RAG" },
        { "rdkit_mcp",            "Chimica (RDKit)" },
        { "sast_mcp",             "Sicurezza statica" },
        { "secrets_scanner_mcp",  "Scanner segreti" },
        { "snippet_mcp",          "Snippet codice" },
        { "sqlite_inspector_mcp", "Inspector SQLite" },
        { "ssh_remote_mcp",       "Connessione SSH" },
        { "system_monitor_mcp",   "Monitor sistema" },
        { "test_generator_mcp",   "Genera test" },
        { "tinymcp",              "Gestore MCP" },
        { "translation_mcp",      "Traduzione testi" },
        { "ui_ux_checker_mcp",    "Verifica UI/UX" },
        { "web_scraper_mcp",      "Web scraping" },
    };
    auto mcpLabel = [](const QString& name) -> QString {
        if (kMcpLabels.contains(name)) return kMcpLabels.value(name);
        QString s = name;
        if (s.endsWith("_mcp")) s.chop(4);
        s.replace('_', ' ');
        if (!s.isEmpty()) s[0] = s[0].toUpper();
        return s;
    };

    m_mcpPanel = new QFrame(this);
    m_mcpPanel->setObjectName("symbolsPanel");
    m_mcpPanel->setVisible(false);
    m_mcpPanel->setMaximumHeight(dpiScale(280));

    auto* slowLay = new QVBoxLayout(m_mcpPanel);
    slowLay->setContentsMargins(8, 6, 8, 6);
    slowLay->setSpacing(6);

    /* Header 🔌 con Tutti/Nessuno */
    {
        auto* hdrRow = new QWidget(m_mcpPanel);
        auto* hdrLay = new QHBoxLayout(hdrRow);
        hdrLay->setContentsMargins(0, 0, 0, 0);
        hdrLay->setSpacing(8);

        auto* hdr = new QLabel(hdrRow);
        hdr->setTextFormat(Qt::RichText);
        hdr->setText(
            "<b>\xf0\x9f\x94\x8c Tool Lenti (MCP)</b>"
            "<span style='color:#dc2626;font-size:10px;font-weight:bold;'>"
            " \xe2\x80\x94 processo separato, +latenza</span>"
            "<br><span style='color:#64748b;font-size:9px;'>"
            "MCP Plugin: avviati come subprocess JSON-RPC 2.0 stdio</span>");
        hdrLay->addWidget(hdr, 1);

        auto* btnAllMcp  = new QPushButton("\xe2\x9c\x85  Tutti",   hdrRow);
        auto* btnNoneMcp = new QPushButton("\xe2\x96\xa1  Nessuno", hdrRow);
        btnAllMcp->setObjectName("actionBtn");
        btnNoneMcp->setObjectName("actionBtn");
        btnAllMcp->setFixedHeight(dpiScale(22));
        btnNoneMcp->setFixedHeight(dpiScale(22));
        hdrLay->addWidget(btnAllMcp);
        hdrLay->addWidget(btnNoneMcp);
        slowLay->addWidget(hdrRow);

        /* Scansiona MCPs/ */
        QStringList mcpNames;
        {
            const QString mcpsRoot = P::root() + "/MCPs";
            for (const QString& d :
                 QDir(mcpsRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
                if (QFileInfo::exists(mcpsRoot + "/" + d + "/server.py"))
                    mcpNames << d;
        }

        /* Barra di ricerca MCP */
        auto* mcpSearch = new QLineEdit(m_mcpPanel);
        mcpSearch->setPlaceholderText("\xf0\x9f\x94\x8d  Cerca MCP per nome o etichetta\xe2\x80\xa6");
        mcpSearch->setClearButtonEnabled(true);
        mcpSearch->setFixedHeight(dpiScale(26));
        slowLay->addWidget(mcpSearch);

        /* Scroll area per i ~50 MCP */
        auto* mcpScroll = new QScrollArea(m_mcpPanel);
        mcpScroll->setWidgetResizable(true);
        mcpScroll->setFrameShape(QFrame::NoFrame);
        mcpScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        mcpScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        auto* grid = new QWidget;
        auto* gl   = new QGridLayout(grid);
        gl->setContentsMargins(0, 2, 0, 2);
        gl->setSpacing(4);
        gl->setColumnStretch(0, 1);
        gl->setColumnStretch(1, 1);
        gl->setColumnStretch(2, 1);
        gl->setColumnStretch(3, 1);

        if (mcpNames.isEmpty()) {
            gl->addWidget(new QLabel(
                "<span style='color:#64748b;'>Nessun MCP in MCPs/</span>", grid),
                0, 0, 1, 4);
        } else {
            QSettings s("Prismalux", "GUI");
            const QStringList savedEnabled =
                s.value("tools/enabledMcps", mcpNames).toStringList();

            for (int i = 0; i < mcpNames.size(); ++i) {
                const QString& name = mcpNames[i];
                const bool en = savedEnabled.contains(name);
                if (en) m_enabledMcps.insert(name);

                auto* chk = new QCheckBox(
                    "\xf0\x9f\x94\x8c  " + mcpLabel(name) + "  (" + name + ")", grid);
                chk->setChecked(en);
                chk->setMinimumHeight(dpiScale(22));
                chk->setToolTip("MCPs/" + name + "/server.py\n"
                    "Processo Python separato (JSON-RPC 2.0 stdio).\n"
                    "Usalo da AppController \xe2\x86\x92 TinyMCP per chiamate dirette.");
                gl->addWidget(chk, i / 4, i % 4);

                connect(chk, &QCheckBox::toggled, this, [this, name](bool on){
                    if (on) m_enabledMcps.insert(name);
                    else    m_enabledMcps.remove(name);
                    QSettings s2("Prismalux", "GUI");
                    s2.setValue("tools/enabledMcps",
                        QStringList(m_enabledMcps.begin(), m_enabledMcps.end()));
                    onToolEnabledChanged();
                });
            }

            connect(btnAllMcp, &QPushButton::clicked, grid, [grid]{
                for (auto* c : grid->findChildren<QCheckBox*>()) c->setChecked(true);
            });
            connect(btnNoneMcp, &QPushButton::clicked, grid, [grid]{
                for (auto* c : grid->findChildren<QCheckBox*>()) c->setChecked(false);
            });

            /* Filtro ricerca MCP: riorganizza la grid senza buchi */
            connect(mcpSearch, &QLineEdit::textChanged, grid,
                    [grid, gl, &mcpNames, mcpLabel](const QString& raw){
                const QString q = raw.trimmed().toLower();
                const auto chks = grid->findChildren<QCheckBox*>(
                    QString(), Qt::FindDirectChildrenOnly);
                for (auto* c : chks) gl->removeWidget(c);
                int pos = 0;
                for (auto* c : chks) {
                    const bool match = q.isEmpty()
                        || c->text().toLower().contains(q)
                        || c->toolTip().toLower().contains(q);
                    c->setVisible(match);
                    if (match) { gl->addWidget(c, pos / 4, pos % 4); ++pos; }
                }
            });
        }

        mcpScroll->setWidget(grid);
        slowLay->addWidget(mcpScroll, 1);
    }

    lay->addWidget(m_mcpPanel);
}

/* ══════════════════════════════════════════════════════════════
   buildBottomBar — barra inferiore: 🔧 Tools · 🧠 Hermes · 🔄
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::buildBottomBar(QVBoxLayout* lay)
{
    auto* bar = new QWidget(this);
    bar->setObjectName("bottomBar");
    auto* bl  = new QHBoxLayout(bar);
    bl->setContentsMargins(0, 2, 0, 0);
    bl->setSpacing(6);

    bl->addStretch();

    lay->addWidget(bar);

    /* ── Connessioni ── */
    connect(m_hermesToggleBar, &QPushButton::toggled, this,
            [this](bool on){
        m_hermesToggleBar->setText(
            on ? "\xf0\x9f\xa7\xa0  Memoria persistente \xe2\x9c\x94"
               : "\xf0\x9f\xa7\xa0  Memoria persistente");
        if (m_hermesReflectBar) m_hermesReflectBar->setVisible(on);
        /* Sincronizza con il vecchio toggle per compatibilità */
        if (m_hermesToggle && m_hermesToggle->isChecked() != on)
            m_hermesToggle->setChecked(on);
        onHermesToggled(on);
    });

    connect(m_hermesReflectBar, &QPushButton::clicked,
            this, &AgentiPage::onHermesReflectClicked);

    /* Aggiorna label iniziale */
    updateToolsBtnLabel();
}

/* ── Aggiorna il testo dei due pulsanti Tool Veloci / Tool Lenti ── */
void AgentiPage::updateToolsBtnLabel()
{
    const int nT = static_cast<int>(m_enabledTools.size());
    const int nM = static_cast<int>(m_enabledMcps.size());

    if (m_btnToolsToggle) {
        m_btnToolsToggle->setText(
            nT == 0
            ? "\xe2\x9a\xa1  Tool Veloci  (disabilitati)"
            : QString("\xe2\x9a\xa1  Tool Veloci  (%1)").arg(nT));
    }
    if (m_btnMcpToggle) {
        m_btnMcpToggle->setText(
            nM == 0
            ? "\xf0\x9f\x94\x8c  Tool Lenti  (disabilitati)"
            : QString("\xf0\x9f\x94\x8c  Tool Lenti  (%1)").arg(nM));
    }
}

void AgentiPage::onToolsPanelToggle()
{
    const bool opening = m_btnToolsToggle && m_btnToolsToggle->isChecked();
    if (opening) {
        /* Chiudi Simboli e Tool Lenti */
        if (m_symbolsScrollArea) m_symbolsScrollArea->setVisible(false);
        if (m_symbolSearch) { m_symbolSearch->setVisible(false); m_symbolSearch->clear(); }
        if (m_btnMcpToggle && m_btnMcpToggle->isChecked()) {
            m_btnMcpToggle->blockSignals(true);
            m_btnMcpToggle->setChecked(false);
            m_btnMcpToggle->blockSignals(false);
            if (m_mcpPanel) m_mcpPanel->setVisible(false);
        }
    }
    if (m_toolsPanel) m_toolsPanel->setVisible(opening);
}

void AgentiPage::onMcpPanelToggle()
{
    const bool opening = m_btnMcpToggle && m_btnMcpToggle->isChecked();
    if (opening) {
        /* Chiudi Simboli e Tool Veloci */
        if (m_symbolsScrollArea) m_symbolsScrollArea->setVisible(false);
        if (m_symbolSearch) { m_symbolSearch->setVisible(false); m_symbolSearch->clear(); }
        if (m_btnToolsToggle && m_btnToolsToggle->isChecked()) {
            m_btnToolsToggle->blockSignals(true);
            m_btnToolsToggle->setChecked(false);
            m_btnToolsToggle->blockSignals(false);
            if (m_toolsPanel) m_toolsPanel->setVisible(false);
        }
    }
    if (m_mcpPanel) m_mcpPanel->setVisible(opening);
}

void AgentiPage::onToolEnabledChanged()
{
    updateToolsBtnLabel();
    m_toolsEnabled = !m_enabledTools.isEmpty();
    if (m_toolChk && m_toolChk->isChecked() != m_toolsEnabled)
        m_toolChk->setChecked(m_toolsEnabled);
}

void AgentiPage::onBubbleStyleChanged()
{
    if (!m_log || m_log->toPlainText().trimmed().isEmpty()) return;

    const int newBr = AppConfig::s().value(P::SK::kBubbleRadius, 10).toInt();
    const int scrollPos = m_log->verticalScrollBar()->value();

    QString html = m_log->toHtml();
    static const QRegularExpression reBr("border-radius:\\s*\\d+px");
    html.replace(reBr, QString("border-radius: %1px").arg(newBr));

    m_log->setHtml(html);
    m_log->verticalScrollBar()->setValue(scrollPos);
}

/* ══════════════════════════════════════════════════════════════
   recolorLog — ricolora le bolle esistenti al cambio tema.
   Sostituisce i colori CSS inline delle bolle (background, testo,
   bordi) in modo contestuale usando prefissi CSS come discriminante,
   così non tocca il contenuto dell'utente (codice, testo puro).
   Chiamata dal segnale ThemeManager::changed.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::recolorLog()
{
    if (!m_log || m_log->toPlainText().trimmed().isEmpty()) return;

    const int scrollPos = m_log->verticalScrollBar()->value();
    QString html = m_log->toHtml();

    /* Ogni riga: { stringa_sorgente, stringa_destinazione }
     * Il prefisso CSS (background-color:, color:, border:, …) fa da
     * disambiguatore: tocca solo attributi style, mai testo libero. */
    struct Pair { const char* src; const char* dst; };

    /* Direzione: dark→light o light→dark in base al tema attivo */
    const bool toLight = isLightTheme();

    /* ── Sostituzioni dark→light ── */
    static const Pair kD2L[] = {
        /* Background bolle (univoci → sicuri al 100%) */
        {"background-color:#162544", "background-color:#dbeafe"},   /* user     */
        {"background-color:#0e1624", "background-color:#f1f5f9"},   /* agent    */
        {"background-color:#0e1a12", "background-color:#dcfce7"},   /* local    */
        {"background-color:#0b1a10", "background-color:#f0fdf4"},   /* tool ok  */
        {"background-color:#1a0a0a", "background-color:#fef2f2"},   /* tool err */
        {"background-color:#1a1400", "background-color:#fffbeb"},   /* ctrl wrn */
        {"background-color:#1e1527", "background-color:#f5f3ff"},   /* no-so    */
        {"background-color:#1e3a5f", "background-color:#bfdbfe"},   /* user btn */
        {"background-color:#1a2641", "background-color:#ede9fe"},   /* agent btn*/
        {"background-color:#0e2318", "background-color:#bbf7d0"},   /* local btn*/
        {"background-color:#2d3a54", "background-color:#e2e8f0"},   /* agent sep*/
        /* Colori testo principali (prefisso "color:" li distingue dal testo) */
        {"color:#e2e8f0",  "color:#1e293b"},  /* testo bolla principale  */
        {"color:#93c5fd",  "color:#1d4ed8"},  /* header bolla user       */
        {"color:#a78bfa",  "color:#7c3aed"},  /* header/accent agent     */
        {"color:#c4b5fd",  "color:#5b21b6"},  /* accent secondario agent */
        {"color:#4ade80",  "color:#15803d"},  /* accent verde local      */
        {"color:#86efac",  "color:#166534"},  /* verde chiaro local      */
        {"color:#8b949e",  "color:#374151"},  /* testo muted tool        */
        {"color:#94a3b8",  "color:#4b5563"},  /* testo muted agent       */
        {"color:#facc15",  "color:#92400e"},  /* accent warn controller  */
        {"color:#f87171",  "color:#dc2626"},  /* accento errore          */
        /* Border bolle */
        {"border:1px solid #1d4ed8",  "border:1px solid #3b82f6"},
        {"border:1px solid #1e2d47",  "border:1px solid #94a3b8"},
        {"border:1px solid #166534",  "border:1px solid #4ade80"},
        {"border:1px solid #7f1d1d",  "border:1px solid #fca5a5"},
        {"border:1px solid #7c3aed",  "border:1px solid #a78bfa"},
        {"border:1px solid #4c1d95",  "border:1px solid #c4b5fd"},
        {nullptr, nullptr}
    };

    /* ── Sostituzioni light→dark ── */
    static const Pair kL2D[] = {
        {"background-color:#dbeafe", "background-color:#162544"},
        {"background-color:#f1f5f9", "background-color:#0e1624"},
        {"background-color:#dcfce7", "background-color:#0e1a12"},
        {"background-color:#f0fdf4", "background-color:#0b1a10"},
        {"background-color:#fef2f2", "background-color:#1a0a0a"},
        {"background-color:#fffbeb", "background-color:#1a1400"},
        {"background-color:#f5f3ff", "background-color:#1e1527"},
        {"background-color:#bfdbfe", "background-color:#1e3a5f"},
        {"background-color:#ede9fe", "background-color:#1a2641"},
        {"background-color:#bbf7d0", "background-color:#0e2318"},
        {"background-color:#e2e8f0", "background-color:#2d3a54"},
        {"color:#1e293b",  "color:#e2e8f0"},
        {"color:#1d4ed8",  "color:#93c5fd"},
        {"color:#7c3aed",  "color:#a78bfa"},
        {"color:#5b21b6",  "color:#c4b5fd"},
        {"color:#15803d",  "color:#4ade80"},
        {"color:#166534",  "color:#86efac"},
        {"color:#374151",  "color:#8b949e"},
        {"color:#4b5563",  "color:#94a3b8"},
        {"color:#92400e",  "color:#facc15"},
        {"color:#dc2626",  "color:#f87171"},
        {"border:1px solid #3b82f6",  "border:1px solid #1d4ed8"},
        {"border:1px solid #94a3b8",  "border:1px solid #1e2d47"},
        {"border:1px solid #4ade80",  "border:1px solid #166534"},
        {"border:1px solid #fca5a5",  "border:1px solid #7f1d1d"},
        {"border:1px solid #a78bfa",  "border:1px solid #7c3aed"},
        {"border:1px solid #c4b5fd",  "border:1px solid #4c1d95"},
        {nullptr, nullptr}
    };

    const Pair* map = toLight ? kD2L : kL2D;
    for (int i = 0; map[i].src; ++i)
        html.replace(QString::fromLatin1(map[i].src),
                     QString::fromLatin1(map[i].dst),
                     Qt::CaseInsensitive);

    /* Mantieni invariato il border-radius (non dipende dal tema) */
    m_log->setHtml(html);
    m_log->verticalScrollBar()->setValue(scrollPos);
}

/* ══════════════════════════════════════════════════════════════
   TablePickerPopup — griglia hover stile LibreOffice/Word
   Nessun Q_OBJECT: usa std::function come callback.
   ══════════════════════════════════════════════════════════════ */
class TablePickerPopup : public QFrame {
    static constexpr int kRows = 8, kCols = 8, kCell = 24, kPad = 6, kLblH = 20;
    int m_hr = 0, m_hc = 0;
    std::function<void(int,int)> m_cb;
public:
    TablePickerPopup(QWidget* parent, std::function<void(int,int)> cb)
        : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
        , m_cb(std::move(cb))
    {
        setMouseTracking(true);
        setFixedSize(kPad*2 + kCols*kCell + 1,
                     kPad*2 + kRows*kCell + kLblH + 4);
        /* Stile segue palette sistema */
        setAutoFillBackground(true);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        /* Sfondo e bordo */
        p.fillRect(rect(), palette().window());
        p.setPen(QPen(palette().mid().color(), 1));
        p.drawRect(rect().adjusted(0,0,-1,-1));
        /* Label dimensione */
        const QString lbl = (m_hr > 0 && m_hc > 0)
            ? QString("%1 \xc3\x97 %2").arg(m_hc).arg(m_hr)   /* NxM */
            : tr("Tabella");
        p.setPen(palette().windowText().color());
        p.setFont(QFont(font().family(), 9, QFont::Bold));
        p.drawText(QRect(kPad, kPad, kCols*kCell, kLblH),
                   Qt::AlignCenter, lbl);
        /* Griglia */
        const int top = kPad + kLblH + 2;
        const QColor hlCol  = palette().highlight().color();
        const QColor bgCol  = palette().base().color();
        const QColor midCol = palette().mid().color();
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c) {
                QRect cell(kPad + c*kCell, top + r*kCell, kCell - 1, kCell - 1);
                p.fillRect(cell, (r < m_hr && c < m_hc) ? hlCol : bgCol);
                p.setPen(midCol);
                p.drawRect(cell);
            }
        }
    }
    void mouseMoveEvent(QMouseEvent* ev) override {
        const int top = kPad + kLblH + 2;
        m_hc = qBound(0, (ev->pos().x() - kPad) / kCell + 1, kCols);
        m_hr = qBound(0, (ev->pos().y() - top)  / kCell + 1, kRows);
        update();
    }
    void mousePressEvent(QMouseEvent*) override {
        if (m_hr > 0 && m_hc > 0 && m_cb) m_cb(m_hr, m_hc);
        close();
    }
    void leaveEvent(QEvent*) override { m_hr = m_hc = 0; update(); }
};

/* ══════════════════════════════════════════════════════════════
   AccentPickerPopup — popup accenti stile Apple (1 carattere)
   ══════════════════════════════════════════════════════════════ */
class AccentPickerPopup : public QFrame {
    std::function<void(const QString&)> m_cb;
public:
    AccentPickerPopup(QWidget* parent,
                      const QVector<QString>& variants,
                      std::function<void(const QString&)> cb)
        : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
        , m_cb(std::move(cb))
    {
        setAutoFillBackground(true);
        setStyleSheet(
            "QFrame{background:palette(window);border:1px solid palette(mid);"
            "border-radius:10px;}"
            "QPushButton{font-size:17px;min-width:42px;min-height:42px;"
            "max-width:42px;max-height:42px;"
            "border:none;border-radius:7px;background:transparent;}"
            "QPushButton:hover{background:palette(highlight);"
            "color:palette(highlighted-text);}");
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(7, 6, 7, 6);
        lay->setSpacing(2);
        for (const QString& v : variants) {
            auto* btn = new QPushButton(v, this);
            btn->setFont(QFont{});
            connect(btn, &QPushButton::clicked, this, [this, v](){
                m_cb(v); close();
            });
            lay->addWidget(btn);
        }
        adjustSize();
    }
};

/* Tabella accenti per lettera — minuscolo + maiuscolo */
static const QHash<QChar, QVector<QString>>& accentMap()
{
    static const QHash<QChar, QVector<QString>> m = {
        {'a', {"à","á","â","ã","ä","å","æ","ā","ă","ą"}},
        {'A', {"À","Á","Â","Ã","Ä","Å","Æ","Ā","Ă","Ą"}},
        {'e', {"è","é","ê","ë","ē","ě","ė","ę"}},
        {'E', {"È","É","Ê","Ë","Ē","Ě","Ė","Ę"}},
        {'i', {"ì","í","î","ï","ī","ĭ","į","ĩ"}},
        {'I', {"Ì","Í","Î","Ï","Ī","Ĭ","Į","Ĩ"}},
        {'o', {"ò","ó","ô","õ","ö","ø","œ","ō","ŏ"}},
        {'O', {"Ò","Ó","Ô","Õ","Ö","Ø","Œ","Ō","Ŏ"}},
        {'u', {"ù","ú","û","ü","ū","ű","ů","ų","ũ"}},
        {'U', {"Ù","Ú","Û","Ü","Ū","Ű","Ů","Ų","Ũ"}},
        {'n', {"ñ","ń","ṅ","ň","ŋ"}},
        {'N', {"Ñ","Ń","Ṅ","Ň","Ŋ"}},
        {'c', {"ç","ć","č","ĉ"}},
        {'C', {"Ç","Ć","Č","Ĉ"}},
        {'s', {"ś","š","ş","ŝ","ß"}},
        {'S', {"Ś","Š","Ş","Ŝ"}},
        {'z', {"ź","ž","ż"}},
        {'Z', {"Ź","Ž","Ż"}},
        {'y', {"ÿ","ý"}},
        {'Y', {"Ÿ","Ý"}},
        {'l', {"ł","ĺ","ľ","ļ"}},
        {'L', {"Ł","Ĺ","Ľ","Ļ"}},
        {'d', {"đ","ð","ď"}},
        {'D', {"Đ","Ð","Ď"}},
        {'t', {"ț","ţ","ť","þ"}},
        {'T', {"Ț","Ţ","Ť","Þ"}},
        {'r', {"ř","ŗ","ŕ"}},
        {'R', {"Ř","Ŗ","Ŕ"}},
        {'g', {"ğ","ĝ","ġ","ģ"}},
        {'G', {"Ğ","Ĝ","Ġ","Ģ"}},
        {'h', {"ĥ","ħ"}},
        {'H', {"Ĥ","Ħ"}},
        {'k', {"ķ","ḳ"}},
        {'K', {"Ķ","Ḳ"}},
    };
    return m;
}

/* ══════════════════════════════════════════════════════════════
   buildInputFormatBar — mini-toolbar formattazione testo
   Appare sopra la selezione nel campo input come frame flottante.
   Parent = this per non essere clippato da m_input.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::buildInputFormatBar()
{
    m_fmtBar = new QFrame(this);
    m_fmtBar->setObjectName("fmtBar");
    m_fmtBar->setFrameShape(QFrame::StyledPanel);
    m_fmtBar->setFrameShadow(QFrame::Raised);
    m_fmtBar->setAutoFillBackground(true);
    /* Stile minimo: solo bordo arrotondato e hover — il resto segue la palette */
    m_fmtBar->setStyleSheet(
        "QFrame#fmtBar{border:1px solid palette(mid);border-radius:6px;"
        "background:palette(window);}"
        "QPushButton{background:transparent;border:none;padding:2px 7px;"
        "border-radius:3px;min-width:20px;font-size:12px;}"
        "QPushButton:hover{background:palette(highlight);"
        "color:palette(highlighted-text);}"
        "QPushButton:pressed{background:palette(dark);}");
    m_fmtBar->setVisible(false);

    auto* vlay = new QVBoxLayout(m_fmtBar);
    vlay->setContentsMargins(6, 5, 6, 5);
    vlay->setSpacing(4);

    /* ── Riga pulsanti con tile verticali (nome sopra, simbolo sotto) ── */
    auto* btnRow = new QWidget(m_fmtBar);
    auto* lay    = new QHBoxLayout(btnRow);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(3);
    vlay->addWidget(btnRow);

    /* Separatore verticale adattivo */
    auto addSep = [&](){
        auto* sep = new QFrame(btnRow);
        sep->setFrameShape(QFrame::VLine);
        sep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        lay->addWidget(sep);
    };

    /* Tile verticale: nome piccolo sopra + pulsante simbolo sotto, entrambi centrati */
    auto addTile = [&](const QString& nome, const QString& sym, const QString& tip,
                       const QString& bef, const QString& aft,
                       const QString& symStyle = {}) -> QPushButton*
    {
        auto* tile = new QWidget(btnRow);
        auto* tl   = new QVBoxLayout(tile);
        tl->setContentsMargins(0, 0, 0, 0);
        tl->setSpacing(1);

        auto* lbl = new QLabel(nome, tile);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("font-size:9px;color:palette(mid);");
        tl->addWidget(lbl);

        auto* btn = new QPushButton(sym, tile);
        btn->setToolTip(tip);
        btn->setFixedWidth(lbl->fontMetrics().horizontalAdvance(nome) + 14);
        QString ss = "QPushButton{background:transparent;border:none;border-radius:3px;"
                     "font-size:13px;" + symStyle + "}"
                     "QPushButton:hover{background:palette(highlight);"
                     "color:palette(highlighted-text);}"
                     "QPushButton:pressed{background:palette(dark);}";
        btn->setStyleSheet(ss);
        tl->addWidget(btn, 0, Qt::AlignHCenter);

        connect(btn, &QPushButton::clicked, m_fmtBar,
                [this, bef, aft]{ onFmtBtnClicked(bef, aft); });
        lay->addWidget(tile);
        return btn;
    };

    /* ── Gruppo 1: grassetto / corsivo / sottolineato ── */
    /* Grassetto / Corsivo / Sottolineato → QTextCharFormat (rich text nativo) */
    auto addRichTile = [&](const QString& nome, const QString& sym,
                           const QString& tip,  const QString& symStyle,
                           auto applyFmt) {
        auto* tile = new QWidget(btnRow);
        auto* tl   = new QVBoxLayout(tile);
        tl->setContentsMargins(0,0,0,0); tl->setSpacing(1);
        auto* lbl  = new QLabel(nome, tile);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("font-size:9px;color:palette(mid);");
        tl->addWidget(lbl);
        auto* btn  = new QPushButton(sym, tile);
        btn->setFixedWidth(lbl->fontMetrics().horizontalAdvance(nome) + 14);
        btn->setToolTip(tip);
        btn->setStyleSheet(
            QString("QPushButton{background:transparent;border:none;border-radius:3px;"
                    "font-size:14px;%1}"
                    "QPushButton:hover{background:palette(highlight);"
                    "color:palette(highlighted-text);}").arg(symStyle));
        tl->addWidget(btn, 0, Qt::AlignHCenter);
        lay->addWidget(tile);
        connect(btn, &QPushButton::clicked, this, [this, applyFmt](){
            if (!m_input) return;
            QTextCursor cur = m_input->textCursor();
            if (!cur.hasSelection()) return;
            applyFmt(cur);
            m_input->setTextCursor(cur);
            if (m_fmtBar) m_fmtBar->hide();
            m_input->setFocus();
        });
    };
    addRichTile("Grassetto","B","Grassetto","font-weight:bold;",
        [](QTextCursor& cur){
            QTextCharFormat f;
            f.setFontWeight(cur.charFormat().fontWeight() >= QFont::Bold
                            ? QFont::Normal : QFont::Bold);
            cur.mergeCharFormat(f);
        });
    addRichTile("Corsivo","I","Corsivo","font-style:italic;",
        [](QTextCursor& cur){
            QTextCharFormat f;
            f.setFontItalic(!cur.charFormat().fontItalic());
            cur.mergeCharFormat(f);
        });
    addRichTile("Sott.","U","Sottolineato","text-decoration:underline;",
        [](QTextCursor& cur){
            QTextCharFormat f;
            f.setFontUnderline(!cur.charFormat().fontUnderline());
            cur.mergeCharFormat(f);
        });

    /* ── Gruppo 2: allineamento ── */
    addSep();
    addTile("Sinistra", "\xe2\x86\x90", "Allinea a sinistra",  "<div align=\"left\">",    "</div>");
    addTile("Centro",   "\xe2\x86\x94", "Centra",               "<div align=\"center\">",  "</div>");
    addTile("Destra",   "\xe2\x86\x92", "Allinea a destra",     "<div align=\"right\">",   "</div>");
    addTile("Giustif.", "\xe2\x89\xa1", "Giustifica",            "<div align=\"justify\">", "</div>");

    /* ── Gruppo 3: citazione / codice ── */
    addSep();
    addTile("Citazione", "\"\"",  "Citazione (> testo)",  "> ",    "",      "");
    addTile("Codice",    "`c`",   "Codice inline",         "`",     "`",     "font-family:monospace;font-size:11px;");
    addTile("Blocco",    "{ }",   "Blocco codice",  "```\n", "\n```", "font-family:monospace;font-size:11px;");

    /* ── Gruppo 4: colore testo / sfondo ── */
    addSep();
    auto makeFgStyle = [](const QColor& c) {
        return QString("QPushButton{background:transparent;border:none;border-radius:3px;"
                       "font-size:14px;font-weight:bold;border-bottom:3px solid %1;}"
                       "QPushButton:hover{background:palette(highlight);"
                       "color:palette(highlighted-text);}").arg(c.name());
    };
    auto makeBgStyle = [](const QColor& c) {
        const QString fg = c.lightness() > 128 ? "#111" : "#eee";
        return QString("QPushButton{border:1px solid palette(mid);border-radius:3px;"
                       "font-size:11px;background:%1;color:%2;padding:1px 5px;}"
                       "QPushButton:hover{background:palette(highlight);"
                       "color:palette(highlighted-text);}").arg(c.name(), fg);
    };
    {
        /* Tile Colore testo */
        auto* fgTile = new QWidget(btnRow);
        auto* ftl    = new QVBoxLayout(fgTile);
        ftl->setContentsMargins(0,0,0,0); ftl->setSpacing(1);
        auto* fgLbl = new QLabel("Colore", fgTile);
        fgLbl->setAlignment(Qt::AlignCenter);
        fgLbl->setStyleSheet("font-size:9px;color:palette(mid);");
        ftl->addWidget(fgLbl);
        m_btnFmtFg = new QPushButton("A", fgTile);
        m_btnFmtFg->setFixedWidth(fgLbl->fontMetrics().horizontalAdvance("Colore") + 14);
        m_btnFmtFg->setStyleSheet(makeFgStyle(m_fmtFgColor));
        m_btnFmtFg->setToolTip("Colore testo (apre selettore colore)");
        ftl->addWidget(m_btnFmtFg, 0, Qt::AlignHCenter);
        lay->addWidget(fgTile);
        connect(m_btnFmtFg, &QPushButton::clicked, this, [this, makeFgStyle](){
            const QColor c = QColorDialog::getColor(m_fmtFgColor, this, "Colore testo");
            if (!c.isValid()) return;
            m_fmtFgColor = c;
            m_btnFmtFg->setStyleSheet(makeFgStyle(c));
            if (!m_input) return;
            QTextCursor cur = m_input->textCursor();
            if (!cur.hasSelection()) return;
            QTextCharFormat fmt; fmt.setForeground(c);
            cur.mergeCharFormat(fmt);
            m_input->setTextCursor(cur);
            if (m_fmtBar) m_fmtBar->hide();
            m_input->setFocus();
        });
    }
    {
        /* Tile Sfondo testo */
        auto* bgTile = new QWidget(btnRow);
        auto* btl    = new QVBoxLayout(bgTile);
        btl->setContentsMargins(0,0,0,0); btl->setSpacing(1);
        auto* bgLbl = new QLabel("Sfondo", bgTile);
        bgLbl->setAlignment(Qt::AlignCenter);
        bgLbl->setStyleSheet("font-size:9px;color:palette(mid);");
        btl->addWidget(bgLbl);
        m_btnFmtBg = new QPushButton("A", bgTile);
        m_btnFmtBg->setFixedWidth(bgLbl->fontMetrics().horizontalAdvance("Sfondo") + 14);
        m_btnFmtBg->setStyleSheet(makeBgStyle(m_fmtBgColor));
        m_btnFmtBg->setToolTip("Colore sfondo testo (apre selettore colore)");
        btl->addWidget(m_btnFmtBg, 0, Qt::AlignHCenter);
        lay->addWidget(bgTile);
        connect(m_btnFmtBg, &QPushButton::clicked, this, [this, makeBgStyle](){
            const QColor c = QColorDialog::getColor(m_fmtBgColor, this, "Colore sfondo");
            if (!c.isValid()) return;
            m_fmtBgColor = c;
            m_btnFmtBg->setStyleSheet(makeBgStyle(c));
            if (!m_input) return;
            QTextCursor cur = m_input->textCursor();
            if (!cur.hasSelection()) return;
            QTextCharFormat fmt; fmt.setBackground(c);
            cur.mergeCharFormat(fmt);
            m_input->setTextCursor(cur);
            if (m_fmtBar) m_fmtBar->hide();
            m_input->setFocus();
        });
    }

    /* ── Gruppo 5: tabella ── */
    addSep();
    QPushButton* btnTbl = nullptr;
    {
        auto* tblTile = new QWidget(btnRow);
        auto* ttl     = new QVBoxLayout(tblTile);
        ttl->setContentsMargins(0, 0, 0, 0);
        ttl->setSpacing(1);
        auto* tblLbl = new QLabel("Tabella", tblTile);
        tblLbl->setAlignment(Qt::AlignCenter);
        tblLbl->setStyleSheet("font-size:9px;color:palette(mid);");
        ttl->addWidget(tblLbl);
        btnTbl = new QPushButton("\xe2\x8a\x9e", tblTile);
        btnTbl->setFixedWidth(tblLbl->fontMetrics().horizontalAdvance("Tabella") + 14);
        btnTbl->setStyleSheet(
            "QPushButton{background:transparent;border:none;border-radius:3px;font-size:13px;}"
            "QPushButton:hover{background:palette(highlight);color:palette(highlighted-text);}");
        btnTbl->setToolTip("Inserisci tabella — scegli dimensioni con la griglia");
        ttl->addWidget(btnTbl, 0, Qt::AlignHCenter);
        lay->addWidget(tblTile);
    }

    /* ── Hint accenti (sotto i pulsanti) ── */
    static const char* kHintDefault =
        "<span style='font-size:9px;'>"
        "Seleziona una lettera per vedere gli accenti disponibili"
        "</span>";
    auto* hintLbl = new QLabel(kHintDefault, m_fmtBar);
    hintLbl->setTextFormat(Qt::RichText);
    hintLbl->setStyleSheet("color:palette(mid);");
    hintLbl->setAlignment(Qt::AlignLeft);
    vlay->addWidget(hintLbl);

    /* Event filter: hover su pulsante → mostra descrizione, leave → ripristina */
    struct HintFilter : public QObject {
        QLabel* lbl;
        HintFilter(QLabel* l, QObject* p) : QObject(p), lbl(l) {}
        bool eventFilter(QObject* o, QEvent* e) override {
            if (auto* b = qobject_cast<QPushButton*>(o)) {
                if (e->type() == QEvent::Enter && !b->toolTip().isEmpty())
                    lbl->setText("<span style='font-size:9px;'>" +
                                 b->toolTip().toHtmlEscaped() + "</span>");
                else if (e->type() == QEvent::Leave)
                    lbl->setText(kHintDefault);
            }
            return false;
        }
    };
    auto* hf = new HintFilter(hintLbl, m_fmtBar);
    for (auto* btn : btnRow->findChildren<QPushButton*>())
        btn->installEventFilter(hf);

    connect(btnTbl, &QPushButton::clicked, m_fmtBar, [this, btnTbl](){
        auto* picker = new TablePickerPopup(this,
            [this](int rows, int cols){
                /* Genera tabella Markdown rows x cols */
                QString tbl = "\n";
                tbl += "|";
                for (int c = 0; c < cols; ++c)
                    tbl += QString(" Col%1 |").arg(c + 1);
                tbl += "\n|";
                for (int c = 0; c < cols; ++c)
                    tbl += " --- |";
                tbl += "\n";
                for (int r = 0; r < rows - 1; ++r) {
                    tbl += "|";
                    for (int c = 0; c < cols; ++c)
                        tbl += "  |";
                    tbl += "\n";
                }
                if (m_input) {
                    m_input->insertPlainText(tbl);
                    m_input->setFocus();
                }
            });
        /* Mostra il picker sotto il pulsante Tabella */
        picker->move(btnTbl->mapToGlobal(
            QPoint(0, btnTbl->height() + 2)));
        picker->show();
    });

    connect(m_input, &QTextEdit::selectionChanged,
            this, &AgentiPage::onInputSelectionChanged);
}

void AgentiPage::onInputSelectionChanged()
{
    if (!m_fmtBar || !m_input) return;
    const QTextCursor cur = m_input->textCursor();
    if (!cur.hasSelection()) {
        m_fmtBar->hide();
        return;
    }

    /* ── Popup accenti stile Apple: 1 solo carattere ── */
    const QString selText = cur.selectedText();
    if (selText.size() == 1) {
        const auto& am = accentMap();
        const auto it  = am.constFind(selText[0]);
        if (it != am.constEnd()) {
            m_fmtBar->hide();
            QTextCursor startCur = cur;
            startCur.setPosition(cur.selectionStart());
            const QRect cr = m_input->cursorRect(startCur);

            QTextCursor capCur = cur;
            auto* picker = new AccentPickerPopup(
                this, it.value(),
                [this, capCur](const QString& v) mutable {
                    capCur.insertText(v);
                    m_input->setTextCursor(capCur);
                });

            const QSize ps = picker->sizeHint();
            QPoint gpos    = m_input->mapToGlobal(cr.topLeft());
            gpos.setY(gpos.y() - ps.height() - 6);
            gpos.setX(qMax(gpos.x() - ps.width() / 2, 4));
            picker->move(gpos);
            picker->show();
            return;
        }
    }

    /* ── Toolbar formattazione: selezione di più caratteri ── */
    m_fmtBar->adjustSize();
    const QSize hint = m_fmtBar->sizeHint();
    const int fh = hint.height();
    const int fw = hint.width();

    /* Coordinate cursore in m_input → convertite a questo widget (parent della fmtBar) */
    QTextCursor startCur = cur;
    startCur.setPosition(cur.selectionStart());
    const QRect cr      = m_input->cursorRect(startCur);
    const QPoint origin = m_input->mapTo(this, cr.topLeft());

    int x = origin.x();
    int y = origin.y() - fh - 6;
    /* Se non c'è spazio sopra, metti sotto la selezione */
    if (y < 2)  y = m_input->mapTo(this, cr.bottomLeft()).y() + 4;
    /* Clampa orizzontalmente entro la pagina */
    if (x + fw > width() - 4)  x = width() - fw - 4;
    if (x < 2)                  x = 2;

    m_fmtBar->setGeometry(x, y, fw, fh);
    m_fmtBar->raise();
    m_fmtBar->show();
}

/* ══════════════════════════════════════════════════════════════
   onSymbolSearchChanged — filtra simboli nel pannello ricerca
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onSymbolSearchChanged(const QString& query)
{
    if (!m_symbolSearch || !m_symbolSearchGrid || !m_symbolSearchPanel) return;

    auto* searchScroll = qobject_cast<QScrollArea*>(
        m_symbolSearch->property("resultsScroll").value<QObject*>());

    const QString q = query.trimmed().toLower();
    const bool searching = !q.isEmpty();

    /* Mostra pannello categorie o risultati ricerca */
    if (m_symbolsScrollArea) m_symbolsScrollArea->setVisible(!searching);
    if (searchScroll)        searchScroll->setVisible(searching);

    if (!searching) return;

    /* Svuota la grid risultati precedenti */
    while (QLayoutItem* item = m_symbolSearchGrid->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    const int BTN_W = dpiScale(38);
    const int BTN_H = dpiScale(26);
    const int perRow = std::max(4, 760 / (BTN_W + 2));

    int col = 0, row = 0;
    int found = 0;
    for (const auto& pair : std::as_const(m_allSymbols)) {
        if (!pair.second.contains(q, Qt::CaseInsensitive)
            && !pair.first.contains(q, Qt::CaseInsensitive))
            continue;
        if (col >= perRow) { col = 0; ++row; }
        auto* b = new QPushButton(pair.first, m_symbolSearchPanel);
        b->setObjectName("symbolBtn");
        b->setFixedSize(BTN_W, BTN_H);
        b->setToolTip(pair.second);
        b->setProperty("symbol", pair.first);
        connect(b, &QPushButton::clicked, this, &AgentiPage::onSymbolBtnClicked);
        m_symbolSearchGrid->addWidget(b, row, col++);
        if (++found >= 300) break;  /* limite sicurezza */
    }

    if (found == 0) {
        auto* lbl = new QLabel(
            tr("Nessun simbolo trovato per \"") + query.toHtmlEscaped() + "\"",
            m_symbolSearchPanel);
        lbl->setStyleSheet("color:#6b7280;font-size:12px;padding:8px;");
        m_symbolSearchGrid->addWidget(lbl, 0, 0, 1, perRow);
    }

    m_symbolSearchPanel->adjustSize();
}

void AgentiPage::onFmtBtnClicked(const QString& before, const QString& after)
{
    if (!m_input) return;
    QTextCursor cur = m_input->textCursor();
    if (!cur.hasSelection()) return;
    /* QTextEdit usa U+2029 (paragrafo) come separatore di riga nella selezione */
    const QString sel = cur.selectedText().replace(QChar(0x2029), '\n');
    QString result;
    if (before == "> " && after.isEmpty()) {
        /* Citazione: prefissa > a ogni riga della selezione */
        const QStringList lines = sel.split('\n');
        QStringList quoted;
        for (const QString& line : lines)
            quoted += "> " + line;
        result = quoted.join('\n');
    } else {
        result = before + sel + after;
    }
    cur.insertText(result);
    m_input->setTextCursor(cur);
    if (m_fmtBar) m_fmtBar->hide();
    m_input->setFocus();
}


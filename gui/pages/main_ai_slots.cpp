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

static bool isVisionCapable(const QString& mdl) { return PrismaluxPaths::isVisionModel(mdl); }
static bool isToolCapable  (const QString& mdl) { return PrismaluxPaths::isToolsModel(mdl); }

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
        m_btnRegen->setText(tr("\xf0\x9f\x94\x84 ") + shortMdl);
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
    m_btnRun->setText(tr("\xf0\x9f\x93\xa4 Invia"));
    if (m_modeBtn) m_modeBtn->setActionText(tr("\xf0\x9f\x93\xa4 Invia"));

    /* 3. Attiva nuova modalità */
    switch (mode) {
    case 0:   /* Chat */
        break;
    case 1:   /* Agentico */
        onModeToggleToggled(true);   /* gestisce anche il testo del run button + hub */
        break;
    case 2:   /* Conversa */
        m_btnRun->setText(tr("\xf0\x9f\x8e\x99  Dialoga"));
        if (m_modeBtn) m_modeBtn->setActionText(tr("\xf0\x9f\x8e\x99  Dialoga"));
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
    /* ── Prova comando dalla tabella help (colonna \xe2\x96\xb6 Prova) ──
       formato: "prova:BASE64URL" — inserisce il comando d'esempio nella
       casella e lo invia, come se l'utente l'avesse digitato. */
    if (s.startsWith("prova:")) {
        const QString cmd = QString::fromUtf8(QByteArray::fromBase64(
            s.mid(6).toLatin1(), QByteArray::Base64UrlEncoding)).trimmed();
        if (!cmd.isEmpty()) {
            m_input->setPlainText(cmd);
            m_input->setFocus();
            m_input->moveCursor(QTextCursor::End);
            if (m_ai->busy()) {
                /* Una risposta è in corso: interrompila e invia il comando
                   dopo che l'abort si è propagato — senza questo il clic
                   verrebbe interpretato come "Stop" e il comando scartato. */
                m_ai->abort();
                QTimer::singleShot(400, this, &AgentiPage::onBtnRunDelayedClick);
            } else {
                QTimer::singleShot(0, this, &AgentiPage::onBtnRunDelayedClick);
            }
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
    /* ── Rigenera bypassando la cache risposte esatte (D-25) ──
       Stesso schema di troncamento di "retry:" (id='ubbl:IDX' = bolla
       utente che precede la risposta cachata), ma imposta
       m_bypassResponseCache prima di ripresentare la domanda: senza
       quel flag la cache restituirebbe di nuovo la stessa risposta
       invece di richiamare davvero il modello. */
    if (s.startsWith("regen:")) {
        const int c1g = s.indexOf(':');
        const int c2g = s.indexOf(':', c1g + 1);
        if (c2g > 0) {
            const int    userIdx  = s.mid(c1g + 1, c2g - c1g - 1).toInt();
            const QString b64g    = s.mid(c2g + 1);
            const QString origText = QString::fromUtf8(
                QByteArray::fromBase64(b64g.toLatin1(), QByteArray::Base64UrlEncoding));
            if (!origText.isEmpty()) {
                if (m_log) {
                    QString html = m_log->toHtml();
                    const QString anchor = QString("id='ubbl:%1'").arg(userIdx);
                    const int anchorPos  = html.indexOf(anchor);
                    if (anchorPos > 0) {
                        const int tablePos = html.lastIndexOf("<table", anchorPos);
                        if (tablePos > 0) {
                            html = html.left(tablePos);
                            m_log->setHtml(html);
                            m_log->moveCursor(QTextCursor::End);
                        }
                    }
                }
                m_bypassResponseCache = true;
                m_input->setPlainText(origText.trimmed());
                m_input->setFocus();
                m_input->moveCursor(QTextCursor::End);
                QTimer::singleShot(0, this, &AgentiPage::onBtnRunDelayedClick);
            }
        }
        return;
    }
    if (s.startsWith("toggle:simplify:")) {
        onToggleThunk(s.mid(16).toInt());
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
            "\xe2\x9c\x85 Parametri reimpostati: Temperatura 0.30 &mdash; "
            "Context 16384 &mdash; Top-P 0.90 &mdash; Max tokens 4096</p>");
        return;
    }
    /* Modifica singolo parametro AI con step pre-calcolato da banner "non lo so" */
    if (s.startsWith("param-set:")) {
        const QStringList parts = s.split(':');
        if (parts.size() < 3) return;
        const QString pName = parts[1];
        const QString pVal  = parts[2];
        AiChatParams p = AiChatParams::load();
        QString displayName, displayVal;
        if (pName == "temperature") {
            const double v = pVal.toDouble();
            if (v < 0.0 || v > 2.0) return;
            p.temperature = v;
            displayName = "Temperatura";
            displayVal  = QString::number(v, 'f', 2);
        } else if (pName == "num_ctx") {
            const int v = pVal.toInt();
            if (v < 512 || v > 131072) return;
            p.num_ctx = v;
            displayName = "Context (num_ctx)";
            displayVal  = QString::number(v);
        } else if (pName == "num_predict") {
            const int v = pVal.toInt();
            if (v < 64 || v > 32768) return;
            p.num_predict = v;
            displayName = "Max tokens";
            displayVal  = QString::number(v);
        } else if (pName == "top_p") {
            const double v = pVal.toDouble();
            if (v < 0.0 || v > 1.0) return;
            p.top_p = v;
            displayName = "Top-P";
            displayVal  = QString::number(v, 'f', 2);
        } else {
            return;
        }
        AiChatParams::save(p);
        if (m_ai) m_ai->setChatParams(p);
        m_log->moveCursor(QTextCursor::End);
        m_log->insertHtml(
            "<p style='color:#4ade80;font-size:11px;margin:4px 0;"
            "background:#0b1a10;border-left:3px solid #166534;"
            "border-radius:4px;padding:4px 10px;'>"
            "\xe2\x9c\x85 <b>" + displayName + "</b> impostato a <b>" + displayVal +
            "</b> &mdash; in effetto dalla prossima richiesta</p>");
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
        ask.setText(tr("<b>Eliminare questo messaggio dalla chat?</b>"));
        ask.setInformativeText(
            "Questa operazione \xc3\xa8 irreversibile.");
        QPushButton* btnDel = ask.addButton(tr("Elimina"), QMessageBox::DestructiveRole);
        ask.addButton(tr("Annulla"), QMessageBox::RejectRole);
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
        auto* btnRun2 = bb2->addButton(tr("\xe2\x96\xb6  Esegui"), QDialogButtonBox::AcceptRole);
        bb2->addButton(tr("\xe2\x9c\x96  Annulla"), QDialogButtonBox::RejectRole);
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
            connect(m_execProc, &QProcess::errorOccurred,
                    this, [this](QProcess::ProcessError err) {
                if (err == QProcess::FailedToStart)
                    qWarning() << "[exec_proc_docker] processo non avviato:" << m_execProc->program();
            });
            m_execProc->start("docker", {"run","--rm","-i","--network","none",
                "--memory", mem, "--memory-swap", mem,
                "--cpus","1","--security-opt","no-new-privileges",
                img, "python3","-c", pyCode});
        } else {
            connect(m_execProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, onFinishedR);
            connect(m_execProc, &QProcess::errorOccurred,
                    this, [this](QProcess::ProcessError err) {
                if (err == QProcess::FailedToStart)
                    qWarning() << "[exec_proc] processo non avviato:" << m_execProc->program();
            });
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
        connect(proc, &QProcess::errorOccurred,
                this, [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                qWarning() << "[websearch] python3 non avviato (FailedToStart)";
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
        auto* btnTask   = btnBox->addButton(tr("Invia come task \xe2\x96\xb6"),
                                            QDialogButtonBox::AcceptRole);
        auto* btnUpdate = btnBox->addButton(tr("Aggiorna bolla \xf0\x9f\x94\x84"),
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
        /* STOP utente: se il loop voce è in QUALUNQUE fase (registrazione,
           trascrizione, risposta AI) il clic sull'hub ferma tutto. Il clic
           PROGRAMMATICO dell'auto-invio (m_sttAutoSending) invece prosegue
           al normale invio del testo trascritto. */
        if (!m_sttAutoSending &&
            (m_voiceLoopActive || m_sttState != SttState::Idle)) {
            _voiceConversaStop();
            return;
        }
        if (m_ai->busy()) { m_ai->abort(); return; }
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
        m_ctxAuto->clear();
        m_autoStep       = 0;
        m_autoBuf.clear();
        m_autoAborted    = false;
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

/* ── Pre-query LLM: riscrive la domanda in modo chiaro e conciso ─────────
   Flusso:
     1. Legge testo da m_input
     2. Chiama AI con system prompt di normalizzazione (NO cronologia)
     3. Risposta → sostituisce il testo nel campo (cursore in fondo)
     4. L'utente vede il testo riformulato e preme Invia manualmente
   Se l'AI è già occupata, mostra avviso e non fa nulla.             ─────── */
QString AgentiPage::buildThunkHtml(int idx, const QString& text, bool open) const
{
    const QString anchor = "simplify" + QString::number(idx);
    const QString href   = "toggle:simplify:" + QString::number(idx);
    const QString arrow  = open
        ? "\xe2\x96\xbc"   /* ▼ */
        : "\xe2\x96\xb6";  /* ▶ */

    QString html =
        "<p style='margin:0 8px 2px 8px;'>"
        "<a name='" + anchor + "'/>"
        "<a href='" + href + "' style='color:#64748b;font-size:10px;"
        "text-decoration:none;'>"
        + arrow + " " + tr("Query normalizzata") +
        "</a>";

    if (open) {
        html +=
            "<br>"
            "<span style='color:#94a3b8;font-size:10px;font-style:italic;"
            "padding-left:14px;'>" +
            text.toHtmlEscaped() +
            "</span>";
    }

    html += "</p>";
    return html;
}

void AgentiPage::onToggleThunk(int idx)
{
    const bool wasOpen = m_thunkOpen.contains(idx);
    if (wasOpen) m_thunkOpen.remove(idx);
    else m_thunkOpen.insert(idx);

    const QString anchorName = "simplify" + QString::number(idx);
    const QString newHtml = buildThunkHtml(idx, m_thunkTexts.value(idx), !wasOpen);

    QTextDocument* doc = m_log->document();
    for (QTextBlock blk = doc->begin(); blk != doc->end(); blk = blk.next()) {
        for (QTextBlock::iterator it = blk.begin(); !it.atEnd(); ++it) {
            if (it.fragment().charFormat().anchorNames().contains(anchorName)) {
                QTextCursor cur(blk);
                cur.select(QTextCursor::BlockUnderCursor);
                cur.insertHtml(newHtml);
                return;
            }
        }
    }
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
    srcRow->addWidget(new QLabel(tr("Da:"), dlg));
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
    mdlRow->addWidget(new QLabel(tr("Modello:"), dlg));
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
    m_autoRetryActive = false;
    m_autoRetrySearchResults.clear();  /* niente salvataggio RAG da retry interrotto */
    m_autoAborted     = true;   /* ferma le continuazioni ReAct schedulate */
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
                    tr("\xf0\x9f\x94\x84  Indicizzazione in corso..."));

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
                    tr("\xf0\x9f\x94\x84  Estrazione PDF in corso..."));
            if (m_ragDropZone)
                m_ragDropZone->setText(
                    tr("\xf0\x9f\x93\x84  Estrazione PDF in corso..."));

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
        if (m_ragStatusLbl) m_ragStatusLbl->setText(tr("\xe2\x9d\x8c  Errore rete: ") +
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


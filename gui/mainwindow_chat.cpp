/* ══════════════════════════════════════════════════════════════
   mainwindow_chat.cpp — MainWindow: cronologia chat + onboarding
   ============================================================================
   Salvataggio/caricamento sessioni, migrazione HTML legacy, riassunto LLM
   in background, wizard di benvenuto al primo avvio.
   Split da mainwindow.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "mainwindow.h"
#include "prismalux_paths.h"
#include "dpi_utils.h"
#include "pages/main_ai.h"
#include "theme_manager.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QGroupBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QColor>
#include <QHash>
#include <QRegularExpression>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTextEdit>
#include <QTextCursor>
#include <QTextDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStatusBar>
#include <QProgressBar>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   stripBodyBackground — rimuove il background-color dal tag <body>
   dell'HTML generato da QTextEdit::toHtml().

   Qt serializza il colore QPalette::Base nel body style; quando la chat
   viene ricaricata in un tema diverso, quel colore fisso sovrascrive il
   background del documento e la chatLog appare con lo sfondo del tema
   precedente.  Rimuovendo solo quella proprietà il QSS del tema attivo
   può applicarsi correttamente tramite QPalette::Base.
   ══════════════════════════════════════════════════════════════ */
static QString stripBodyBackground(const QString& html)
{
    QString out = html;
    /* Qt genera:  <body style=" color:#...; background-color:#RRGGBB;">
     * Cattura tutto fino a background-color nel group 1, salta il valore. */
    static QRegularExpression re(
        "(<body\\b[^>]*style\\s*=\\s*\"[^\"]*?)background-color\\s*:\\s*[^;\"]+;?\\s*",
        QRegularExpression::CaseInsensitiveOption);
    out.replace(re, "\\1");
    return out;
}

/* ══════════════════════════════════════════════════════════════
   migrateLegacyChat — converte le chat pre-bolla nel nuovo formato.

   Estrae il testo grezzo dalla vecchia HTML (QTextDocument::toPlainText),
   identifica task / intestazioni agente / risposte tramite pattern,
   e ricostruisce il documento con le bolle buildUserBubble /
   buildAgentBubble / buildLocalBubble di AgentiPage.

   Se l'HTML è già nel nuovo formato (contiene il marker della bolla
   utente) lo restituisce invariato.
   ══════════════════════════════════════════════════════════════ */
static QString migrateLegacyChat(const QString& html)
{
    /* Già nel nuovo formato → niente da fare.
     * Controlla sia kDark.uBg (#162544) sia kLight.uBg (#dbeafe):
     * le chat salvate in tema chiaro hanno il secondo colore nelle bolle utente. */
    if (html.contains("bgcolor='#162544'") || html.contains("bgcolor=\"#162544\"") ||
        html.contains("bgcolor='#dbeafe'") || html.contains("bgcolor=\"#dbeafe\""))
        return html;

    /* Estrai testo grezzo */
    QTextDocument doc;
    doc.setHtml(html);
    const QString plain = doc.toPlainText();

    /* ── Risposta locale (0 token) ── */
    static QRegularExpression localRe(
        "(?:Risposta locale|Calcolo locale)[^\\n]*\\n([^\\n]+)\\n"
        "[^\\n]*tempo:[^\\n]*(\\d+[.,]\\d+)\\s*ms");
    {
        auto m = localRe.match(plain);
        if (m.hasMatch()) {
            QString result = m.captured(1).trimmed();
            double ms = m.captured(2).replace(',', '.').toDouble();
            /* Recupera il task dalla riga "Task:" */
            static QRegularExpression taskRe("Task:\\s*(.+)");
            QString task;
            auto tm = taskRe.match(plain);
            if (tm.hasMatch()) task = tm.captured(1).trimmed();
            return AgentiPage::buildUserBubble(task.isEmpty() ? result : task)
                 + AgentiPage::buildLocalBubble(result, ms);
        }
    }

    /* ── Pipeline ── */
    /* Estrae task */
    QString task;
    {
        static QRegularExpression taskRe("Task:\\s*(.+)");
        auto m = taskRe.match(plain);
        if (m.hasMatch()) task = m.captured(1).trimmed();
    }

    /* Spezza il testo in blocchi agente:
       intestazione = riga con "[Agente N"  o "Agente N —"
       fine blocco  = riga con "completato" oppure "Pipeline completata" */
    struct AgentBlock { QString label, model, time, body; };
    QVector<AgentBlock> agents;

    const QStringList lines = plain.split('\n');
    static QRegularExpression hdrRe(
        R"(\[Agente\s+(\d+)\s*[\—\-]\s*([^\]]+)\]\s*(?:[\xf0\x9f\xa4\x96\x1f916]+)?\s*(\S[^\xf0\x9f\x95\x90\n]*)\s*(?:[\xf0\x9f\x95\x90]+)?\s*(\d{2}:\d{2}:\d{2})?)");
    /* Pattern più semplice: cerca linee con "Agente N" e un modello */
    static QRegularExpression simpleHdr(
        "Agente\\s+(\\d+)[^\\[\\]]*\\]?\\s*([a-zA-Z0-9_.:/-]+)\\s*(\\d{2}:\\d{2}:\\d{2})?");

    AgentBlock current;
    bool inAgent = false;

    for (const QString& line : lines) {
        const QString t = line.trimmed();

        /* Fine blocco agente */
        if (inAgent && (t.contains("completato") || t.contains("Pipeline completata"))) {
            agents.append(current);
            current = {};
            inAgent = false;
            continue;
        }

        /* Nuova intestazione agente */
        if (t.contains("[Agente") && t.contains("]")) {
            if (inAgent) agents.append(current);
            current = {};
            inAgent = true;

            /* Estrai role dal pattern [Agente N — Role] */
            static QRegularExpression roleRe(R"(\[Agente\s+(\d+)\s*[—\-]\s*([^\]]+)\])");
            auto rm = roleRe.match(t);
            if (rm.hasMatch())
                current.label = "\xf0\x9f\xa4\x96  Agente " + rm.captured(1) + " \xe2\x80\x94 " + rm.captured(2).trimmed();

            /* Estrai modello (token dopo 🤖) */
            static QRegularExpression modelRe(R"([\xf0\x9f\xa4\x96]\s*(\S+))");
            auto mm = modelRe.match(t);
            if (mm.hasMatch()) current.model = mm.captured(1);

            /* Estrai orario HH:mm:ss */
            static QRegularExpression timeRe(R"((\d{2}:\d{2}:\d{2}))");
            auto tm = timeRe.match(t);
            if (tm.hasMatch()) current.time = tm.captured(1);
            continue;
        }

        if (inAgent) {
            /* Salta righe separatori o "generando..." */
            if (t.startsWith("\xe2\x94") || t.startsWith("\xe2\x80\x94") ||
                t.contains("generando") || t.isEmpty())
                continue;
            current.body += line + '\n';
        }
    }
    if (inAgent) agents.append(current);

    /* Costruisci l'HTML con le bolle */
    if (agents.isEmpty() && task.isEmpty()) {
        /* Formato sconosciuto: mostra il testo grezzo in un card neutro */
        QString safe = plain;
        safe.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
        safe.replace("\n","<br>");
        /* Usa colori neutri (grigio chiaro) così il box è leggibile
         * sia in tema chiaro che scuro senza hardcode di palette. */
        return "<table width='100%' cellpadding='0' cellspacing='4'><tr>"
               "<td style='border:1px solid #888888;border-radius:8px;"
               "padding:12px;'>"
               "<p style='color:#888888;font-size:11px;margin:0 0 6px 0;'>"
               "\xf0\x9f\x93\x9c  Chat storica (formato precedente)</p>"
               + safe + "</td></tr></table>";
    }

    QString result = task.isEmpty() ? QString() : AgentiPage::buildUserBubble(task);
    for (const auto& ag : agents) {
        QString content = AgentiPage::markdownToHtml(ag.body.trimmed());
        if (content.isEmpty())
            content = "<p style='color:#6b7280;font-style:italic;'>Nessun contenuto salvato.</p>";
        result += AgentiPage::buildAgentBubble(
            ag.label.isEmpty() ? "\xf0\x9f\xa4\x96  Agente" : ag.label,
            ag.model.isEmpty() ? "—" : ag.model,
            ag.time.isEmpty()  ? "—" : ag.time,
            content);
    }
    return result;
}

/* ══════════════════════════════════════════════════════════════
   Chat History — salvataggio e lista
   ══════════════════════════════════════════════════════════════ */
void MainWindow::onChatCompleted(const QString& title, const QString& logHtml) {
    /* Se siamo già in una sessione attiva, aggiorna quella (chat continua).
       Se la sessione è vuota o non esiste più, crea una nuova voce. */
    bool sessionValid = !m_currentChatId.isEmpty()
        && !m_chatHistory.loadLog(m_currentChatId).isEmpty();

    if (!sessionValid) {
        m_currentChatId = m_chatHistory.newSession(title);
        /* Dopo il primo scambio: genera titolo + riassunti via LLM in background
           (sostituisce il titolo troncato non appena la generazione termina). */
        requestSessionSummary(m_currentChatId, logHtml);
    }

    /* Una sola scrittura atomica: log + mappe + history ReAct */
    auto* ap = qobject_cast<AgentiPage*>(m_mainTabs ? m_mainTabs->widget(0) : nullptr);
    m_chatHistory.saveSession(m_currentChatId, logHtml,
                              ap ? ap->bubbleTexts()  : QMap<int,QString>{},
                              ap ? ap->codeBlocks()   : QMap<int,QPair<QString,QString>>{},
                              ap ? ap->autoHistory()  : QJsonArray{});

    refreshChatList();

    appendLog(QString("\xe2\x9c\x85 Pipeline completata: <b>%1</b>")
              .arg(title.isEmpty() ? "(senza titolo)" : title.toHtmlEscaped()), LogAI);
}

/* Rimuove blocchi <think>...</think> prodotti da modelli reasoning, stesso pattern
 * usato in main_multi_agent.cpp (parsePlan). */
static QString stripThinkTagsMw(const QString& s)
{
    static const QRegularExpression re("<think>[\\s\\S]*?</think>",
                                        QRegularExpression::CaseInsensitiveOption);
    QString out = s;
    out.remove(re);
    return out.trimmed();
}

void MainWindow::requestSessionSummary(const QString& sessionId, const QString& logHtml)
{
    if (!m_ai || sessionId.isEmpty()) return;
    if (m_summaryAi && m_summaryAi->busy()) return;  /* generazione già in corso: salta */

    QTextDocument doc;
    doc.setHtml(logHtml);
    const QString plainText = doc.toPlainText().left(3000);
    if (plainText.trimmed().isEmpty()) return;

    if (!m_summaryAi) m_summaryAi = new AiClient(this);
    m_summaryAi->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_ai->model());
    m_pendingSummarySessionId = sessionId;

    const QString sys =
        "Genera metadati per una conversazione chat. Rispondi SOLO con JSON valido, "
        "nessun testo fuori dal JSON, nessun blocco di codice. Formato esatto:\n"
        "{\"titolo\":\"...\",\"riassunto_breve\":\"...\",\"riassunto_lungo\":\"...\"}\n"
        "Regole: titolo massimo 6 parole, senza punteggiatura finale; "
        "riassunto_breve una frase di massimo 20 parole; "
        "riassunto_lungo 2-4 frasi, massimo 400 caratteri. Scrivi in italiano.";
    const QString user = "Conversazione:\n\n" + plainText;

    auto* holder = new QObject(this);
    connect(m_summaryAi, &AiClient::finished, holder,
            [this, holder, sessionId](const QString& full) {
                holder->deleteLater();
                if (sessionId != m_pendingSummarySessionId) return;

                QString clean = stripThinkTagsMw(full);
                static const QRegularExpression reFence(R"(```[a-z]*\n?([\s\S]*?)```)");
                const auto fence = reFence.match(clean);
                if (fence.hasMatch()) clean = fence.captured(1).trimmed();
                static const QRegularExpression reJson(R"(\{[\s\S]*\})");
                const auto m = reJson.match(clean);
                if (m.hasMatch()) clean = m.captured(0);

                QJsonParseError err;
                const QJsonDocument jdoc = QJsonDocument::fromJson(clean.toUtf8(), &err);
                if (err.error != QJsonParseError::NoError || !jdoc.isObject()) return;

                const QJsonObject obj   = jdoc.object();
                const QString newTitle  = obj["titolo"].toString().trimmed();
                const QString brief     = obj["riassunto_breve"].toString().trimmed();
                const QString longSumm  = obj["riassunto_lungo"].toString().trimmed();
                if (newTitle.isEmpty()) return;

                m_chatHistory.updateTitleAndSummary(sessionId, newTitle, brief, longSumm);
                refreshChatList();
            });

    connect(m_summaryAi, &AiClient::error, holder,
            [holder](const QString&) {
                /* Non critico: il titolo troncato resta valido come fallback */
                holder->deleteLater();
            });

    m_summaryAi->chat(sys, user);
}

/* Slot: usa le funzioni statiche stripBodyBackground/migrateLegacyChat definite sopra */
void MainWindow::onChatItemClicked(QListWidgetItem* item)
{
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;
    const QString rawHtml = m_chatHistory.loadLog(id);
    if (rawHtml.isEmpty()) return;
    const QString html = stripBodyBackground(migrateLegacyChat(rawHtml));

    auto* ap = qobject_cast<AgentiPage*>(m_mainTabs ? m_mainTabs->widget(0) : nullptr);

    if (ap) {
        if (auto* log = ap->findChild<QTextEdit*>()) {
            log->setHtml(html);
            log->moveCursor(QTextCursor::End);
        }
        /* Ripristina mappe bubble/codice così code:copy, code:save e TTS funzionano */
        QMap<int,QString> bt;
        QMap<int,QPair<QString,QString>> cb;
        m_chatHistory.loadMaps(id, bt, cb);
        ap->loadSessionMaps(bt, cb);

        /* Ripristina history ReAct agente autonomo */
        const QJsonArray ah = m_chatHistory.loadAutoHistory(id);
        ap->setAutoHistory(ah);
    }

    m_currentChatId = id;
    navigateTo(0);
}

void MainWindow::refreshChatList() {
    if (!m_chatList) return;
    m_chatList->clear();

    const auto sessions = m_chatHistory.list();

    /* Conta quante sessioni hanno lo stesso titolo — per i duplicati
     * aggiunge " · HH:mm" così l'utente può distinguerle visivamente.
     * Il caricamento resta sempre basato sull'ID univoco (Qt::UserRole). */
    QMap<QString, int> titleCount;
    for (const auto& s : sessions) {
        const QString base = s.title.isEmpty() ? "(senza titolo)" : s.title;
        titleCount[base]++;
    }

    if (sessions.isEmpty()) {
        auto* placeholder = new QListWidgetItem(
            "\xf0\x9f\x92\xac  Nessuna chat salvata\n"
            "Inizia una conversazione\nnella pagina AI");  /* 💬 */
        placeholder->setFlags(Qt::NoItemFlags);  /* non selezionabile */
        placeholder->setForeground(QColor("#888"));
        placeholder->setTextAlignment(Qt::AlignCenter);
        m_chatList->addItem(placeholder);
        return;
    }

    for (const auto& s : sessions) {
        const QString base = s.title.isEmpty() ? "(senza titolo)" : s.title;
        QString display = base;
        if (titleCount.value(base) > 1)
            display += " \xc2\xb7 " + s.createdAt.toString("HH:mm");
        auto* item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, s.id);
        QString tooltip = s.createdAt.toString("dd/MM/yyyy HH:mm:ss");
        if (!s.summaryBrief.isEmpty()) tooltip += "\n\n" + s.summaryBrief;
        item->setToolTip(tooltip);
        m_chatList->addItem(item);
    }
}

void MainWindow::onPipelineStatus(int pct, const QString& text) {
    if (!m_statusProgress) return;
    if (pct == -2) {
        /* FEAT-2: TTFT — aggiorna solo la status bar, senza toccare la progressbar */
        if (!text.isEmpty())
            statusBar()->showMessage(text, 5000);
        return;
    }
    if (pct < 0) {
        /* Resetta: pipeline terminata — nascondi la barra */
        m_statusProgress->setValue(0);
        m_statusProgress->setFormat("");
        m_statusProgress->setVisible(false);
        return;
    }
    m_statusProgress->setVisible(true);
    m_statusProgress->setValue(pct);
    if (!text.isEmpty()) {
        m_statusProgress->setFormat(text);
        statusBar()->showMessage(text, 8000);
    }
}

/* ══════════════════════════════════════════════════════════════
   showOnboardingWizard — dialog di benvenuto al primo avvio.
   3 step: backend, modello consigliato, tema.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::showOnboardingWizard()
{
    QSettings ss("Prismalux", "GUI");

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Benvenuto in Prismalux \xf0\x9f\x8d\xba"));
    dlg->setMinimumWidth(dpiScale(480));
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto* vlay = new QVBoxLayout(dlg);
    vlay->setSpacing(14);

    /* Logo + titolo */
    auto* hdrLbl = new QLabel(
        "<h2 style='margin:0'>\xf0\x9f\x8d\xba  Benvenuto in Prismalux</h2>"
        "<p style='color:#6c63ff;margin:4px 0 0 0;'>"
        "Costruito per i mortali che aspirano alla saggezza.</p>", dlg);
    hdrLbl->setTextFormat(Qt::RichText);
    vlay->addWidget(hdrLbl);

    auto* sep = new QFrame(dlg);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    vlay->addWidget(sep);

    /* Step 1 — backend */
    auto* backendGrp = new QGroupBox(tr("1. Backend AI"), dlg);
    auto* bLay = new QVBoxLayout(backendGrp);
    auto* backendCombo = new QComboBox(backendGrp);
    backendCombo->addItem("\xf0\x9f\xa6\x99  Ollama (consigliato)", 0);
    backendCombo->addItem("\xf0\x9f\xa6\x99  llama-server (avanzato)", 1);
    const int savedBe = ss.value(P::SK::kActiveBackend, 0).toInt();
    backendCombo->setCurrentIndex(savedBe < backendCombo->count() ? savedBe : 0);
    auto* backendHint = new QLabel(
        "<small style='color:#888'>Ollama: scarica ed esegui modelli con un click. "
        "llama-server: maggior controllo su layer GPU.</small>", backendGrp);
    backendHint->setTextFormat(Qt::RichText);
    backendHint->setWordWrap(true);
    bLay->addWidget(backendCombo);
    bLay->addWidget(backendHint);
    vlay->addWidget(backendGrp);

    /* Step 2 — modello consigliato */
    auto* modelGrp = new QGroupBox(tr("2. Modello consigliato"), dlg);
    auto* mLay = new QVBoxLayout(modelGrp);
    auto* modelCombo = new QComboBox(modelGrp);
    modelCombo->addItem("qwen3:4b  \xe2\x80\x94  ~2.6 GB  (8 GB RAM, veloce)", "qwen3:4b");
    modelCombo->addItem("qwen3:8b  \xe2\x80\x94  ~5 GB   (16 GB RAM, bilanciato)", "qwen3:8b");
    modelCombo->addItem("qwen3:14b \xe2\x80\x94  ~9 GB   (16 GB RAM, qualit\xc3\xa0)", "qwen3:14b");
    modelCombo->addItem("qwen3:30b \xe2\x80\x94  ~19 GB  (32 GB RAM, ottimo)", "qwen3:30b");
    modelCombo->addItem("gemma3:4b \xe2\x80\x94  ~3 GB   (8 GB RAM, multimodale)", "gemma3:4b");
    const QString savedModel = ss.value(P::SK::kActiveModel, "").toString();
    int modelIdx = savedModel.isEmpty() ? 0 : modelCombo->findData(savedModel);
    if (modelIdx < 0) modelIdx = 0;
    modelCombo->setCurrentIndex(modelIdx);
    auto* modelHint = new QLabel(
        "<small style='color:#888'>Puoi cambiarlo in qualsiasi momento da "
        "Impostazioni \xe2\x86\x92 AI Locale \xe2\x86\x92 LLM Consigliati.</small>", modelGrp);
    modelHint->setTextFormat(Qt::RichText);
    modelHint->setWordWrap(true);
    mLay->addWidget(modelCombo);
    mLay->addWidget(modelHint);
    vlay->addWidget(modelGrp);

    /* Step 3 — tema */
    auto* themeGrp = new QGroupBox(tr("3. Tema"), dlg);
    auto* tLay = new QVBoxLayout(themeGrp);
    auto* themeCombo = new QComboBox(themeGrp);

    /* Mappa statica id → accent color (hardcoded, nessuna I/O a runtime) */
    static const QHash<QString, QString> kAccent = {
        {"dark_cyan",     "#00b8d9"}, {"dark_amber",   "#ffb300"},
        {"dark_classic",  "#4a90e2"}, {"dark_purple",  "#9c5ff0"},
        {"dark_ocean",    "#20c4da"}, {"dark_sunset",  "#fd7e14"},
        {"dark_green",    "#2ecc71"}, {"dark_lavender","#8b68e8"},
        {"dark_rainbow",  "#ff6b6b"}, {"hacker",       "#00ff00"},
        {"neon",          "#00ff9d"}, {"solar",        "#268bd2"},
        {"pink",          "#ec4899"}, {"military",     "#6b8e23"},
        {"light",         "#0072c6"}, {"light_mint",   "#00897b"},
        {"light_rose",    "#c2185b"}, {"light_sand",   "#e65100"},
        {"light_sky",     "#0277bd"}, {"venom_green",  "#00ff00"},
        {"venom_blue",    "#00bfff"}, {"venom_orange", "#ff4500"},
        {"venom_red",     "#ff0000"},
    };

    const auto& allThemes = ThemeManager::instance()->themes();
    /* Popola SENZA emettere currentIndexChanged (bloccato finché non connesso) */
    themeCombo->blockSignals(true);
    for (const auto& t : allThemes) {
        themeCombo->addItem(t.label, t.id);
        const QColor accent(kAccent.value(t.id, "#888888"));
        /* colora la voce con l'accent del tema */
        const int last = themeCombo->count() - 1;
        themeCombo->setItemData(last, accent,          Qt::ForegroundRole);
        themeCombo->setItemData(last, accent.darker(500), Qt::BackgroundRole);
    }
    const QString savedTheme = ss.value(P::SK::kTheme, P::SK::kDefaultTheme).toString();
    const int themeIdx = qMax(0, themeCombo->findData(savedTheme));
    themeCombo->setCurrentIndex(themeIdx);
    themeCombo->blockSignals(false);

    /* Anteprima live: QTimer::singleShot(0) evita di bloccare la UI
       se setStyleSheet è lento su molti widget */
    const QString origTheme = ThemeManager::instance()->currentId();
    connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            themeCombo, [themeCombo](int idx) {
                const QString id = themeCombo->itemData(idx).toString();
                if (!id.isEmpty())
                    QTimer::singleShot(0, qApp, [id]{ ThemeManager::instance()->apply(id); });
            });

    tLay->addWidget(themeCombo);
    vlay->addWidget(themeGrp);

    /* Checkbox "non mostrare più" — DEFAULT SPUNTATA (fonte unica di verità
     * per la ricomparsa del wizard). Spuntata di default evita la vecchia
     * trappola: chi preme "Inizia!" o X senza notarla ottiene comunque il
     * comportamento sicuro (wizard mostrato una sola volta). Chi la toglie
     * ESPLICITAMENTE vuole rivederlo al prossimo riavvio — è una scelta. */
    auto* dontShow = new QCheckBox(
        tr("Non mostrare pi\xc3\xb9 al prossimo riavvio"), dlg);
    dontShow->setChecked(true);
    vlay->addWidget(dontShow);
    m_onbDontShow = dontShow;

    /* X/Esc senza confermare → ripristina tema originale. kSetupDone segue la
     * checkbox (default spuntata): X = non mostrare più, salvo che l'utente
     * l'abbia despuntata di proposito. */
    connect(dlg, &QDialog::rejected, dlg, [origTheme, dontShow]() {
        ThemeManager::instance()->apply(origTheme);
        QSettings("Prismalux", "GUI")
            .setValue(P::SK::kSetupDone, dontShow->isChecked());
    });

    /* Bottoni */
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok, dlg);
    btnBox->button(QDialogButtonBox::Ok)->setText(tr("\xf0\x9f\x8d\xba  Inizia!"));
    vlay->addWidget(btnBox);

    m_onbBackend = backendCombo;
    m_onbModel   = modelCombo;
    m_onbTheme   = themeCombo;
    m_onbDlg     = dlg;
    connect(btnBox, &QDialogButtonBox::accepted, this, &MainWindow::onOnboardingAccepted);

    dlg->exec();
}

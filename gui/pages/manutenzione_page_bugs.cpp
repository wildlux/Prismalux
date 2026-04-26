/* ══════════════════════════════════════════════════════════════
   manutenzione_page_bugs.cpp — Bug Tracker LLM

   Cerca bug noti per un modello su GitHub Issues e Reddit,
   li analizza con AI e suggerisce fix ai parametri Ollama.
   I fix vengono salvati in ~/.prismalux/model_params.json e
   applicati automaticamente da AiClient::chat() alle chiamate successive.
   ══════════════════════════════════════════════════════════════ */
#include "manutenzione_page.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTextBrowser>
#include <QTextCursor>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QGroupBox>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QDateTime>
#include <QFrame>

/* ──────────────────────────────────────────────────────────────
   buildBugTracker — restituisce il widget del tab "🔍 Bug Tracker"
   ────────────────────────────────────────────────────────────── */
QWidget* ManutenzioneePage::buildBugTracker()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(14, 12, 14, 12);
    lay->setSpacing(10);

    /* Descrizione */
    auto* desc = new QLabel(
        "\xf0\x9f\x94\x8d  <b>Analizzatore Bug Modelli</b> \xe2\x80\x94 "
        "Cerca bug noti per il modello su <b>GitHub Issues</b> e <b>Reddit r/LocalLLaMA</b>, "
        "li analizza con AI ed estrae fix ai parametri Ollama applicabili subito.", w);
    desc->setObjectName("hintLabel");
    desc->setWordWrap(true);
    desc->setTextFormat(Qt::RichText);
    lay->addWidget(desc);

    auto* sep = new QFrame(w);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    lay->addWidget(sep);

    /* Riga ricerca */
    auto* searchRow = new QWidget(w);
    auto* searchLay = new QHBoxLayout(searchRow);
    searchLay->setContentsMargins(0, 0, 0, 0);
    searchLay->setSpacing(8);

    searchLay->addWidget(new QLabel("\xf0\x9f\xa4\x96  Modello:", searchRow));

    m_bugModelCombo = new QComboBox(searchRow);
    m_bugModelCombo->setMinimumWidth(220);
    m_bugModelCombo->setToolTip("Seleziona il modello da analizzare");
    if (!m_ai->model().isEmpty())
        m_bugModelCombo->addItem(m_ai->model());
    searchLay->addWidget(m_bugModelCombo, 1);

    auto* btnRefMod = new QPushButton("\xf0\x9f\x94\x84", searchRow);
    btnRefMod->setObjectName("actionBtn");
    btnRefMod->setFixedWidth(34);
    btnRefMod->setToolTip("Aggiorna lista modelli");
    searchLay->addWidget(btnRefMod);

    m_btnSearchBug = new QPushButton("\xf0\x9f\x94\x8d  Cerca bug", searchRow);
    m_btnSearchBug->setObjectName("actionBtn");
    searchLay->addWidget(m_btnSearchBug);

    lay->addWidget(searchRow);

    /* Log risultati + analisi */
    auto* logGroup = new QGroupBox("\xf0\x9f\x93\x8b  Risultati e Analisi AI", w);
    logGroup->setObjectName("cardGroup");
    auto* logLay = new QVBoxLayout(logGroup);
    logLay->setContentsMargins(4, 8, 4, 4);
    logLay->setSpacing(4);

    m_bugLog = new QTextBrowser(w);
    m_bugLog->setObjectName("chatLog");
    m_bugLog->setOpenExternalLinks(true);
    m_bugLog->setPlaceholderText(
        "Seleziona un modello e premi \"\xf0\x9f\x94\x8d Cerca bug\" per avviare l'analisi.\n\n"
        "Fonti: GitHub Issues (ollama/ollama, ggerganov/llama.cpp) \xe2\x80\xa2 Reddit r/LocalLLaMA\n\n"
        "I fix trovati vengono salvati in ~/.prismalux/model_params.json\n"
        "e applicati automaticamente alle chiamate Ollama successive.");
    logLay->addWidget(m_bugLog, 1);

    lay->addWidget(logGroup, 1);

    /* Riga fix */
    auto* fixRow = new QWidget(w);
    auto* fixLay = new QHBoxLayout(fixRow);
    fixLay->setContentsMargins(0, 0, 0, 0);
    fixLay->setSpacing(10);

    m_btnApplyFix = new QPushButton(
        "\xe2\x9c\x85  Applica fix suggeriti", fixRow);
    m_btnApplyFix->setObjectName("actionBtn");
    m_btnApplyFix->setEnabled(false);
    m_btnApplyFix->setToolTip(
        "Salva i parametri suggeriti in model_params.json.\n"
        "Verranno usati automaticamente nelle prossime chiamate a questo modello.");
    fixLay->addWidget(m_btnApplyFix);

    auto* btnClearFix = new QPushButton(
        "\xf0\x9f\x97\x91  Rimuovi fix", fixRow);
    btnClearFix->setObjectName("actionBtn");
    btnClearFix->setToolTip("Rimuovi i fix salvati per il modello corrente");
    fixLay->addWidget(btnClearFix);

    m_bugStatusLbl = new QLabel("", fixRow);
    m_bugStatusLbl->setObjectName("hintLabel");
    m_bugStatusLbl->setWordWrap(true);
    fixLay->addWidget(m_bugStatusLbl, 1);

    lay->addWidget(fixRow);

    /* NAM dedicato (non condiviso con altri widget) */
    m_namBug = new QNetworkAccessManager(w);

    /* Popola combo quando arrivano modelli */
    connect(m_ai, &AiClient::modelsReady, this, [this](const QStringList& list){
        if (!m_bugModelCombo) return;
        const QString cur = m_bugModelCombo->currentText();
        m_bugModelCombo->blockSignals(true);
        m_bugModelCombo->clear();
        for (const auto& m : list)
            m_bugModelCombo->addItem(m);
        int idx = m_bugModelCombo->findText(cur);
        if (idx < 0) idx = m_bugModelCombo->findText(m_ai->model());
        if (idx >= 0) m_bugModelCombo->setCurrentIndex(idx);
        m_bugModelCombo->blockSignals(false);
    });

    connect(btnRefMod, &QPushButton::clicked, m_ai, &AiClient::fetchModels);

    connect(m_btnSearchBug, &QPushButton::clicked, this, [this]{
        const QString model = m_bugModelCombo->currentText().trimmed();
        if (model.isEmpty() || model.startsWith("(")) {
            m_bugStatusLbl->setText(
                "\xe2\x9a\xa0  Seleziona prima un modello valido.");
            return;
        }
        searchBugs(model);
    });

    connect(m_btnApplyFix, &QPushButton::clicked, this, [this]{
        applyBugFix();
    });

    connect(btnClearFix, &QPushButton::clicked, this, [this]{
        const QString model = m_bugModelCombo->currentText().trimmed();
        if (model.isEmpty()) return;
        QFile f(P::modelParamsPath());
        if (!f.open(QIODevice::ReadOnly)) {
            m_bugStatusLbl->setText(
                "\xe2\x84\xb9  Nessun fix salvato per questo modello.");
            return;
        }
        QJsonObject all = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        if (!all.contains(model)) {
            m_bugStatusLbl->setText(
                QString("\xe2\x84\xb9  Nessun fix salvato per <b>%1</b>.").arg(model));
            return;
        }
        all.remove(model);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(QJsonDocument(all).toJson(QJsonDocument::Indented));
        m_bugStatusLbl->setText(
            QString("\xf0\x9f\x97\x91  Fix rimossi per <b>%1</b>.").arg(model));
        m_btnApplyFix->setEnabled(false);
    });

    return w;
}

/* ──────────────────────────────────────────────────────────────
   searchBugs — lancia 3 ricerche in parallelo
   1. GitHub Issues ollama/ollama
   2. GitHub Issues ggerganov/llama.cpp
   3. Reddit r/LocalLLaMA
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::searchBugs(const QString& model)
{
    m_bugLog->clear();
    m_bugSnippets.clear();
    m_bugSearchPending = 0;
    m_bugFixJson       = QJsonObject();
    m_bugCurrentModel  = model;
    m_btnSearchBug->setEnabled(false);
    m_btnApplyFix->setEnabled(false);
    m_bugStatusLbl->setText("");

    /* Intestazione */
    {
        QTextCursor c(m_bugLog->document());
        c.movePosition(QTextCursor::End);
        c.insertHtml(QString(
            "<p><b>\xf0\x9f\x94\x8d  Ricerca bug per: <span style='color:#60a5fa'>%1</span></b><br>"
            "<small style='color:#888'>%2</small></p>")
            .arg(model.toHtmlEscaped())
            .arg(QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss")));
    }

    const QString enc = QString::fromUtf8(
        QUrl::toPercentEncoding(model.section(":", 0, 0)));  /* solo nome, no tag */

    /* ── GitHub Issues ── */
    const QStringList repos = {"ollama/ollama", "ggerganov/llama.cpp"};
    for (const QString& repo : repos) {
        m_bugSearchPending++;
        const QString url = QString(
            "https://api.github.com/search/issues"
            "?q=%1+bug+repo:%2&sort=updated&per_page=4")
            .arg(enc).arg(repo);

        QNetworkRequest req((QUrl(url)));
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      "Prismalux-BugTracker/1.0");
        req.setRawHeader("Accept", "application/vnd.github+json");

        auto* reply = m_namBug->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, repo]{
            reply->deleteLater();
            QTextCursor c(m_bugLog->document());
            c.movePosition(QTextCursor::End);

            if (reply->error() == QNetworkReply::NoError) {
                const QJsonArray items =
                    QJsonDocument::fromJson(reply->readAll())
                    .object()["items"].toArray();

                c.insertHtml(QString(
                    "<hr><p><b>\xf0\x9f\x90\x99  GitHub \xe2\x80\x94 %1</b>"
                    "  <small>(%2 issue)</small></p>")
                    .arg(repo.toHtmlEscaped()).arg(items.size()));

                const int take = qMin(4, (int)items.size());
                for (int i = 0; i < take; ++i) {
                    const QJsonObject iss = items[i].toObject();
                    const QString title  = iss["title"].toString().toHtmlEscaped();
                    const QString url    = iss["html_url"].toString();
                    const QString state  = iss["state"].toString();
                    const QString body   = iss["body"].toString();
                    const QString icon   = (state == "open")
                                          ? "\xf0\x9f\x9f\xa1" : "\xf0\x9f\x9f\xa2";
                    const QString snip   = body.left(500).replace("\n", " ")
                                              .replace("\r", "").toHtmlEscaped();

                    c.insertHtml(QString(
                        "<p>%1  <a href='%2'><b>%3</b></a>"
                        " <small>[%4]</small><br>"
                        "<small style='color:#9ca3af;'>%5</small></p>")
                        .arg(icon).arg(url).arg(title).arg(state).arg(snip));

                    m_bugSnippets << QString("[GitHub %1] %2\n%3")
                        .arg(repo).arg(iss["title"].toString()).arg(body.left(600));
                }

                if (take == 0)
                    c.insertHtml(
                        "<p><small><i>\xe2\x9c\x85  Nessun issue trovato.</i></small></p>");
            } else {
                c.insertHtml(QString(
                    "<p><small>\xe2\x9d\x8c  GitHub %1: %2</small></p>")
                    .arg(repo.toHtmlEscaped())
                    .arg(reply->errorString().toHtmlEscaped()));
            }

            m_bugSearchPending--;
            if (m_bugSearchPending == 0) analyzeBugResults();
        });
    }

    /* ── Reddit r/LocalLLaMA ── */
    m_bugSearchPending++;
    {
        const QString url = QString(
            "https://www.reddit.com/r/LocalLLaMA/search.json"
            "?q=%1+bug&restrict_sr=1&sort=new&limit=4&raw_json=1")
            .arg(enc);
        QNetworkRequest req((QUrl(url)));
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 Prismalux-BugTracker/1.0");
        auto* reply = m_namBug->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]{
            reply->deleteLater();
            QTextCursor c(m_bugLog->document());
            c.movePosition(QTextCursor::End);

            if (reply->error() == QNetworkReply::NoError) {
                const QJsonArray children =
                    QJsonDocument::fromJson(reply->readAll())
                    .object()["data"].toObject()["children"].toArray();

                c.insertHtml(QString(
                    "<hr><p><b>\xf0\x9f\x9f\xa0  Reddit r/LocalLLaMA</b>"
                    "  <small>(%1 post)</small></p>").arg(children.size()));

                const int take = qMin(4, (int)children.size());
                for (int i = 0; i < take; ++i) {
                    const QJsonObject post =
                        children[i].toObject()["data"].toObject();
                    const QString title  = post["title"].toString().toHtmlEscaped();
                    const QString url    = "https://reddit.com"
                                        + post["permalink"].toString();
                    const QString selftext = post["selftext"].toString();
                    const QString snip   = selftext.left(350).replace("\n", " ")
                                              .replace("\r", "").toHtmlEscaped();

                    c.insertHtml(QString(
                        "<p>\xf0\x9f\x9f\xa0  <a href='%1'><b>%2</b></a><br>"
                        "<small style='color:#9ca3af;'>%3</small></p>")
                        .arg(url).arg(title).arg(snip));

                    m_bugSnippets << QString("[Reddit] %1\n%2")
                        .arg(post["title"].toString()).arg(selftext.left(500));
                }

                if (take == 0)
                    c.insertHtml(
                        "<p><small><i>\xe2\x9c\x85  Nessun post trovato.</i></small></p>");
            } else {
                c.insertHtml(QString(
                    "<p><small>\xe2\x9d\x8c  Reddit: %1</small></p>")
                    .arg(reply->errorString().toHtmlEscaped()));
            }

            m_bugSearchPending--;
            if (m_bugSearchPending == 0) analyzeBugResults();
        });
    }
}

/* ──────────────────────────────────────────────────────────────
   analyzeBugResults — invia gli snippet all'AI per l'analisi
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::analyzeBugResults()
{
    {
        QTextCursor c(m_bugLog->document());
        c.movePosition(QTextCursor::End);
        c.insertHtml(
            "<hr><p><b>\xf0\x9f\xa4\x96  Analisi AI in corso...</b></p>");
    }

    if (m_bugSnippets.isEmpty()) {
        QTextCursor c(m_bugLog->document());
        c.movePosition(QTextCursor::End);
        c.insertHtml(
            "<p><i>\xe2\x9a\xa0  Nessun dato da analizzare. "
            "Il modello potrebbe non avere bug segnalati, o le fonti non sono raggiungibili.</i></p>");
        m_btnSearchBug->setEnabled(true);
        return;
    }

    const QString sys =
        "Sei un esperto di LLM locali (Ollama, llama.cpp). "
        "Analizzi segnalazioni di bug per aiutare l'utente a capire se il suo modello "
        "ha problemi noti e come risolverli con parametri Ollama. "
        "Rispondi SEMPRE e SOLO in italiano. "
        "Sii conciso e pratico.";

    const QString prompt = QString(
        "Modello analizzato: %1\n\n"
        "Risultati ricerca bug dalle fonti (GitHub Issues + Reddit):\n"
        "════════════════════════════════════════\n"
        "%2\n"
        "════════════════════════════════════════\n\n"
        "Analizza e dimmi:\n"
        "1. Qual \xc3\xa8 il bug esatto (se trovato)\n"
        "2. Quale versione minima di Ollama risolve il problema\n"
        "3. Quali parametri Ollama possono mitigare il problema "
        "(es. aumentare num_predict, num_ctx, abbassare temperature)\n\n"
        "Se esistono fix parametrici applicabili, concludi con ESATTAMENTE questo blocco JSON "
        "(niente prima, niente dopo il blocco):\n"
        "```json\n"
        "{\n"
        "  \"bug\": \"descrizione breve del bug\",\n"
        "  \"versione_minima_ollama\": \"0.x.y o null\",\n"
        "  \"parametri\": {\n"
        "    \"num_predict\": 2048\n"
        "  }\n"
        "}\n"
        "```\n"
        "Se non ci sono fix parametrici da applicare ometti completamente il blocco JSON.")
        .arg(m_bugCurrentModel)
        .arg(m_bugSnippets.join("\n\n---\n\n").left(5000));

    /* Usa il pattern one-shot connHolder per non interferire con altri slot */
    auto* holder = new QObject(this);

    /* Mostra i token in streaming direttamente nel log */
    connect(m_ai, &AiClient::token, holder, [this](const QString& tok){
        QTextCursor c(m_bugLog->document());
        c.movePosition(QTextCursor::End);
        c.insertText(tok);
        m_bugLog->ensureCursorVisible();
    });

    connect(m_ai, &AiClient::finished, holder, [this, holder](const QString& full){
        holder->deleteLater();
        parseBugFix(full);
        m_btnSearchBug->setEnabled(true);
    });

    connect(m_ai, &AiClient::error, holder, [this, holder](const QString& err){
        holder->deleteLater();
        QTextCursor c(m_bugLog->document());
        c.movePosition(QTextCursor::End);
        c.insertHtml(QString(
            "<p style='color:#ef4444;'>\xe2\x9d\x8c  Errore AI: %1</p>")
            .arg(err.toHtmlEscaped()));
        m_btnSearchBug->setEnabled(true);
    });

    m_ai->chat(sys, prompt);
}

/* ──────────────────────────────────────────────────────────────
   parseBugFix — estrae il blocco JSON dalla risposta AI
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::parseBugFix(const QString& response)
{
    static const QRegularExpression re(
        "```json\\s*([\\s\\S]*?)```",
        QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(response);

    if (!m.hasMatch()) {
        m_bugStatusLbl->setText(
            "\xe2\x84\xb9  Nessun fix parametrico identificato dall'AI.");
        return;
    }

    const QJsonDocument doc =
        QJsonDocument::fromJson(m.captured(1).trimmed().toUtf8());
    if (!doc.isObject()) return;

    m_bugFixJson = doc.object();
    const QJsonObject params = m_bugFixJson["parametri"].toObject();

    if (params.isEmpty()) {
        m_bugStatusLbl->setText(
            "\xe2\x84\xb9  Nessun parametro da applicare.");
        return;
    }

    QStringList parts;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        parts << QString("%1=%2").arg(it.key()).arg(
            it.value().toVariant().toString());

    m_bugStatusLbl->setText(
        QString("\xf0\x9f\x94\xa7  Fix pronti: <b>%1</b>").arg(parts.join(", ")));
    m_btnApplyFix->setEnabled(true);

    /* Riquadro riepilogo nel log */
    QTextCursor c(m_bugLog->document());
    c.movePosition(QTextCursor::End);
    c.insertHtml(QString(
        "<hr><p><b>\xf0\x9f\x94\xa7  Fix applicabili per %1:</b><br>"
        "<span style='color:#60a5fa;'>%2</span></p>"
        "<p><small>Premi <b>\xe2\x9c\x85 Applica fix</b> per salvarli.</small></p>")
        .arg(m_bugCurrentModel.toHtmlEscaped())
        .arg(parts.join(" &nbsp;\xe2\x80\xa2&nbsp; ")));
}

/* ──────────────────────────────────────────────────────────────
   applyBugFix — salva i parametri in model_params.json
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::applyBugFix()
{
    if (m_bugFixJson.isEmpty() || m_bugCurrentModel.isEmpty()) return;

    const QJsonObject params = m_bugFixJson["parametri"].toObject();
    if (params.isEmpty()) return;

    /* Crea la directory se non esiste */
    QDir().mkpath(QDir::homePath() + "/.prismalux");

    /* Carica il file esistente */
    QJsonObject allParams;
    {
        QFile f(P::modelParamsPath());
        if (f.open(QIODevice::ReadOnly))
            allParams = QJsonDocument::fromJson(f.readAll()).object();
    }

    /* Unisce i nuovi parametri a quelli esistenti per questo modello */
    QJsonObject modelParams = allParams[m_bugCurrentModel].toObject();
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        modelParams[it.key()] = it.value();
    allParams[m_bugCurrentModel] = modelParams;

    /* Salva */
    QFile fOut(P::modelParamsPath());
    if (!fOut.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_bugStatusLbl->setText(
            "\xe2\x9d\x8c  Impossibile scrivere model_params.json.");
        return;
    }
    fOut.write(QJsonDocument(allParams).toJson(QJsonDocument::Indented));

    QStringList parts;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        parts << QString("%1=%2").arg(it.key()).arg(
            it.value().toVariant().toString());

    m_bugStatusLbl->setText(
        QString("\xe2\x9c\x85  Fix salvati per <b>%1</b>: %2<br>"
                "<small>Attivi dalla prossima chiamata AI.</small>")
        .arg(m_bugCurrentModel.toHtmlEscaped())
        .arg(parts.join(", ")));

    m_btnApplyFix->setEnabled(false);

    QTextCursor c(m_bugLog->document());
    c.movePosition(QTextCursor::End);
    c.insertHtml(QString(
        "<p style='color:#4ade80;'><b>\xe2\x9c\x85  Fix applicati e salvati.</b><br>"
        "<small>File: %1</small></p>")
        .arg(P::modelParamsPath().toHtmlEscaped()));
}

#include "strumenti_file_page.h"
#include "../prismalux_paths.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QTextBrowser>
#include <QGroupBox>
#include <QSplitter>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QTimer>
#include <QTemporaryFile>
#include <QFont>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextCursor>
#include <QStandardPaths>

namespace P = PrismaluxPaths;

StrumentiFilePage::StrumentiFilePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildFileSearchTab(), "\xf0\x9f\x97\x82  File AI");        /* 🗂 */
    tabs->addTab(buildWikiTab(),       "\xf0\x9f\x93\x96  Wiki & Web");     /* 📖 */
    tabs->addTab(buildDatiTab(),       "\xf0\x9f\x93\x8a  Excel/CSV");      /* 📊 */
    tabs->addTab(buildPdfTab(),        "\xf0\x9f\x93\x84  PDF");            /* 📄 */
    tabs->addTab(buildWordTab(),       "\xf0\x9f\x93\x9d  Word/Testo");     /* 📝 */

    lay->addWidget(tabs);
}

/* ══════════════════════════════════════════════════════════════
   buildFileSearchTab
   ══════════════════════════════════════════════════════════════ */
QWidget* StrumentiFilePage::buildFileSearchTab()
{
    auto* panel = new QWidget(this);
    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(0, 6, 0, 6);
    lay->setSpacing(8);

    auto* title = new QLabel(
        "\xf0\x9f\x97\x82  <b>Ricerca File con AI</b>  \xe2\x80\x94  "
        "cerca file locali per parola chiave e falli analizzare dal modello", panel);
    title->setTextFormat(Qt::RichText);
    title->setObjectName("cardDesc");
    lay->addWidget(title);

    auto* dirRow = new QWidget(panel);
    auto* dirLay = new QHBoxLayout(dirRow);
    dirLay->setContentsMargins(0, 0, 0, 0);
    dirLay->setSpacing(6);
    auto* dirLbl = new QLabel("\xf0\x9f\x93\x81  Cartella:", dirRow);
    dirLbl->setObjectName("cardDesc");
    dirLay->addWidget(dirLbl);
    m_fileSearchDir = new QLineEdit(dirRow);
    m_fileSearchDir->setPlaceholderText(
        QDir::homePath() + "  (lascia vuoto per usare la home)");
    m_fileSearchDir->setText(QDir::homePath());
    dirLay->addWidget(m_fileSearchDir, 1);
    auto* btnBrowse = new QPushButton(
        "\xf0\x9f\x93\x82  Sfoglia", dirRow);
    btnBrowse->setObjectName("actionBtn");
    btnBrowse->setFixedWidth(90);
    dirLay->addWidget(btnBrowse);
    lay->addWidget(dirRow);

    auto* qRow = new QWidget(panel);
    auto* qLay = new QHBoxLayout(qRow);
    qLay->setContentsMargins(0, 0, 0, 0);
    qLay->setSpacing(6);
    auto* qLbl = new QLabel("\xf0\x9f\x94\x8d  Cerca:", qRow);
    qLbl->setObjectName("cardDesc");
    qLay->addWidget(qLbl);
    m_fileSearchQuery = new QLineEdit(qRow);
    m_fileSearchQuery->setPlaceholderText(
        "cosa stai cercando? (es. \"configurazione database\")");
    qLay->addWidget(m_fileSearchQuery, 1);
    auto* btnSearch = new QPushButton(
        "\xf0\x9f\x94\x8d  Cerca", qRow);
    btnSearch->setObjectName("actionBtn");
    btnSearch->setFixedWidth(90);
    qLay->addWidget(btnSearch);
    auto* btnAI = new QPushButton(
        "\xf0\x9f\xa4\x96  Analisi AI", qRow);
    btnAI->setObjectName("actionBtn");
    btnAI->setFixedWidth(100);
    btnAI->setEnabled(false);
    btnAI->setToolTip("Manda i file trovati al modello AI per analisi");
    qLay->addWidget(btnAI);
    lay->addWidget(qRow);

    m_fileSearchOut = new QTextBrowser(panel);
    m_fileSearchOut->setPlaceholderText(
        "I file trovati appariranno qui...\n\n"
        "Suggerimento: usa termini precisi (nomi funzioni, variabili, argomenti)");
    m_fileSearchOut->setOpenLinks(false);
    m_fileSearchOut->setMinimumHeight(120);
    m_fileSearchOut->setStyleSheet(
        "QTextBrowser { background:#0f172a; color:#e2e8f0;"
        "border:1px solid #334155; border-radius:6px; padding:8px; "
        "font-family: 'JetBrains Mono','Fira Code',monospace; font-size:12px; }");
    lay->addWidget(m_fileSearchOut, 1);

    connect(btnBrowse, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, "Seleziona cartella da ricercare", m_fileSearchDir->text());
        if (!dir.isEmpty()) m_fileSearchDir->setText(dir);
    });

    connect(btnSearch, &QPushButton::clicked, this, [this, btnSearch, btnAI]() {
        const QString query = m_fileSearchQuery->text().trimmed();
        const QString root  = m_fileSearchDir->text().trimmed();
        if (query.isEmpty()) return;
        if (root.isEmpty() || !QDir(root).exists()) {
            m_fileSearchOut->setPlainText(
                "\xe2\x9d\x8c  Cartella non valida: " + root);
            return;
        }

        btnSearch->setEnabled(false);
        btnAI->setEnabled(false);
        m_fileSearchOut->setPlainText(
            "\xf0\x9f\x94\x8d  Ricerca in corso...");

        if (m_fileSearchProc) {
            m_fileSearchProc->kill();
            m_fileSearchProc->deleteLater();
            m_fileSearchProc = nullptr;
        }

        const QString script = QString::fromUtf8(
            "import os,sys,json\n"
            "root=sys.argv[1]; query=sys.argv[2].lower(); kw=query.split()\n"
            "exts={'.txt','.md','.py','.cpp','.h','.js','.ts','.json','.csv','.rst','.yaml','.toml','.ini','.cfg'}\n"
            "results=[]\n"
            "for dp,dirs,files in os.walk(root):\n"
            "    dirs[:]=[d for d in dirs if not d.startswith('.')]\n"
            "    for f in files:\n"
            "        if os.path.splitext(f)[1].lower() not in exts: continue\n"
            "        path=os.path.join(dp,f)\n"
            "        try:\n"
            "            with open(path,'r',errors='replace') as fp: c=fp.read(8000)\n"
            "            score=sum(1 for k in kw if k in c.lower() or k in f.lower())\n"
            "            if score>0: results.append({'file':path,'score':score,'snippet':c[:600]})\n"
            "        except: pass\n"
            "results.sort(key=lambda x:-x['score'])\n"
            "print(json.dumps(results[:8]))\n");

        m_fileSearchProc = new QProcess(this);
        m_fileSearchProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_fileSearchProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, btnSearch, btnAI, query](int exitCode, QProcess::ExitStatus) {
            const QString raw = QString::fromUtf8(m_fileSearchProc->readAll()).trimmed();
            m_fileSearchProc->deleteLater();
            m_fileSearchProc = nullptr;
            btnSearch->setEnabled(true);

            if (exitCode != 0 || raw.isEmpty()) {
                m_fileSearchOut->setPlainText(
                    "\xe2\x9d\x8c  Errore nella ricerca:\n" + raw.left(300));
                return;
            }

            const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
            if (!doc.isArray() || doc.array().isEmpty()) {
                m_fileSearchOut->setPlainText(
                    "\xf0\x9f\x8c\xab  Nessun file trovato per: \"" + query + "\"");
                return;
            }

            QString html;
            html += "<p style='color:#94a3b8;font-size:12px;'>"
                    "\xf0\x9f\x94\x8d  <b>Risultati per:</b> "
                    + query.toHtmlEscaped() + "</p>";

            QString aiContext = QString("Query: %1\n\nFile rilevanti trovati:\n\n").arg(query);

            int i = 1;
            for (const QJsonValue& v : doc.array()) {
                const QJsonObject o = v.toObject();
                const QString file    = o["file"].toString();
                const QString snippet = o["snippet"].toString();
                const int score       = o["score"].toInt();

                html += QString(
                    "<div style='background:#1e293b;border:1px solid #334155;"
                    "border-radius:6px;padding:8px;margin:4px 0;'>"
                    "<p style='color:#60a5fa;font-size:11px;margin:0 0 4px 0;font-weight:bold;'>"
                    "[%1] %2 &nbsp;<span style='color:#64748b;font-weight:normal;'>"
                    "(score: %3)</span></p>"
                    "<pre style='color:#94a3b8;font-size:10px;margin:0;white-space:pre-wrap;'>"
                    "%4</pre></div>")
                    .arg(i).arg(file.toHtmlEscaped()).arg(score)
                    .arg(snippet.left(300).toHtmlEscaped());

                aiContext += QString("[%1] %2\n%3\n\n").arg(i).arg(file).arg(snippet);
                i++;
            }

            m_fileSearchOut->setHtml(html);
            m_fileSearchQuery->setProperty("aiContext", aiContext);
            m_fileSearchQuery->setProperty("aiQuery", query);
            btnAI->setEnabled(true);
        });
        m_fileSearchProc->start(P::findPython(), {"-c", script, root, query});
        QTimer::singleShot(30000, this, [this, btnSearch]() {
            if (m_fileSearchProc) {
                m_fileSearchProc->kill();
                btnSearch->setEnabled(true);
                m_fileSearchOut->append(
                    "\n\xe2\x9a\xa0  Timeout: ricerca interrotta dopo 30s");
            }
        });
    });

    connect(btnAI, &QPushButton::clicked, this, [this]() {
        const QString ctx   = m_fileSearchQuery->property("aiContext").toString();
        const QString query = m_fileSearchQuery->property("aiQuery").toString();
        if (ctx.isEmpty()) return;
        const QString sys =
            "Sei un assistente esperto nell'analisi di codice e documenti. "
            "Hai ricevuto snippet di file locali rilevanti per una query. "
            "Analizza i file e rispondi in modo preciso e utile. "
            "Cita i percorsi dei file quando fai riferimento a contenuti specifici. "
            "Rispondi in italiano.";
        /* One-shot output nel fileSearchOut */
        m_fileSearchOut->append("\n\n--- Analisi AI ---\n");
        auto* h = new QObject(this);
        connect(m_ai, &AiClient::token, h, [this](const QString& t){
            /* Append plain text to browser — insert at end */
            QTextCursor c = m_fileSearchOut->textCursor();
            c.movePosition(QTextCursor::End);
            m_fileSearchOut->setTextCursor(c);
            m_fileSearchOut->insertPlainText(t);
        });
        connect(m_ai, &AiClient::finished, h, [h](const QString&){ h->deleteLater(); });
        connect(m_ai, &AiClient::error, h, [this, h](const QString& msg){
            m_fileSearchOut->append("\n\xe2\x9d\x8c  " + msg);
            h->deleteLater();
        });
        m_ai->chat(P::prependKnowledge(sys), ctx + "\nDomanda: " + query);
    });

    return panel;
}

/* ══════════════════════════════════════════════════════════════
   buildWikiTab
   ══════════════════════════════════════════════════════════════ */
QWidget* StrumentiFilePage::buildWikiTab()
{
    auto* panel = new QWidget(this);
    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(0, 6, 0, 6);
    lay->setSpacing(8);

    auto* title = new QLabel(
        "\xf0\x9f\x93\x96  <b>Wikipedia + AI</b>  \xe2\x80\x94  "
        "cerca un articolo e fallo elaborare dal modello locale", panel);
    title->setTextFormat(Qt::RichText);
    title->setObjectName("cardDesc");
    lay->addWidget(title);

    auto* sRow = new QWidget(panel);
    auto* sLay = new QHBoxLayout(sRow);
    sLay->setContentsMargins(0, 0, 0, 0);
    sLay->setSpacing(6);

    m_wikiQuery = new QLineEdit(sRow);
    m_wikiQuery->setPlaceholderText(
        "argomento Wikipedia (es. \"Intelligenza artificiale\", \"Alan Turing\")");
    sLay->addWidget(m_wikiQuery, 1);

    m_wikiLangCombo = new QComboBox(sRow);
    m_wikiLangCombo->setObjectName("settingsCombo");
    m_wikiLangCombo->setFixedWidth(65);
    m_wikiLangCombo->addItem("it", "it");
    m_wikiLangCombo->addItem("en", "en");
    m_wikiLangCombo->addItem("fr", "fr");
    m_wikiLangCombo->addItem("de", "de");
    m_wikiLangCombo->addItem("es", "es");
    m_wikiLangCombo->setToolTip("Lingua Wikipedia");
    sLay->addWidget(m_wikiLangCombo);

    auto* btnFetch = new QPushButton(
        "\xf0\x9f\x94\x8d  Cerca", sRow);
    btnFetch->setObjectName("actionBtn");
    btnFetch->setFixedWidth(80);
    sLay->addWidget(btnFetch);

    auto* btnElabora = new QPushButton(
        "\xf0\x9f\xa4\x96  Elabora con AI", sRow);
    btnElabora->setObjectName("actionBtn");
    btnElabora->setEnabled(false);
    btnElabora->setToolTip(
        "Invia l\xe2\x80\x99" "articolo Wikipedia al modello per analisi, approfondimento o Q&A");
    sLay->addWidget(btnElabora);
    lay->addWidget(sRow);

    auto* actRow = new QWidget(panel);
    auto* actLay = new QHBoxLayout(actRow);
    actLay->setContentsMargins(0, 0, 0, 0);
    actLay->setSpacing(4);

    struct WikiAction { const char* label; const char* prompt; };
    static const WikiAction kActions[] = {
        { "\xf0\x9f\x93\x9d  Riassunto", "Riassumi l\xe2\x80\x99" "articolo in 5 punti chiave. Rispondi in italiano." },
        { "\xf0\x9f\xa7\xaa  Verifica fatti", "Identifica i fatti principali verificabili nell\xe2\x80\x99" "articolo e indica le fonti da controllare. Rispondi in italiano." },
        { "\xf0\x9f\x8f\xab  Spiega semplice", "Spiega l\xe2\x80\x99" "argomento come se parlassi a uno studente di liceo. Usa esempi concreti. Rispondi in italiano." },
        { "\xf0\x9f\x94\x97  Approfondisci", "Suggerisci 5 argomenti correlati da approfondire, con una breve spiegazione per ciascuno. Rispondi in italiano." },
    };
    for (const auto& a : kActions) {
        auto* ab = new QPushButton(QString::fromUtf8(a.label), actRow);
        ab->setObjectName("strActBtn");
        ab->setEnabled(false);
        ab->setProperty("wikiPrompt", QString::fromUtf8(a.prompt));
        actLay->addWidget(ab);
        connect(ab, &QPushButton::clicked, this, [this, ab]() {
            if (m_wikiExtract.isEmpty()) return;
            const QString prompt = ab->property("wikiPrompt").toString();
            const QString sys =
                "Sei un assistente esperto. Usa le informazioni dell\xe2\x80\x99" "articolo Wikipedia "
                "come contesto. Rispondi in modo preciso e utile. Rispondi in italiano.";
            /* One-shot: output in m_wikiContent */
            m_wikiContent->append("\n\n--- AI ---\n");
            auto* h = new QObject(this);
            connect(m_ai, &AiClient::token, h, [this](const QString& t){
                QTextCursor c = m_wikiContent->textCursor();
                c.movePosition(QTextCursor::End);
                m_wikiContent->setTextCursor(c);
                m_wikiContent->insertPlainText(t);
            });
            connect(m_ai, &AiClient::finished, h, [h](const QString&){ h->deleteLater(); });
            connect(m_ai, &AiClient::error, h, [this, h](const QString& msg){
                m_wikiContent->append("\n\xe2\x9d\x8c  " + msg);
                h->deleteLater();
            });
            m_ai->chat(P::prependKnowledge(sys),
                    "[Articolo Wikipedia]\n" + m_wikiExtract + "\n\n" + prompt);
        });
        ab->setObjectName("wikiActBtn");
    }
    actLay->addStretch();
    lay->addWidget(actRow);

    m_wikiWaitLbl = new QLabel(panel);
    m_wikiWaitLbl->setObjectName("cardDesc");
    m_wikiWaitLbl->setVisible(false);
    lay->addWidget(m_wikiWaitLbl);

    m_wikiContent = new QTextBrowser(panel);
    m_wikiContent->setPlaceholderText(
        "Il testo dell\xe2\x80\x99" "articolo Wikipedia appare qui...\n\n"
        "Suggerimento: usa titoli precisi (es. \"Fisica quantistica\" e non \"fisica\")");
    m_wikiContent->setOpenLinks(false);
    m_wikiContent->setMinimumHeight(120);
    m_wikiContent->setStyleSheet(
        "QTextBrowser { background:#0f172a; color:#e2e8f0;"
        "border:1px solid #334155; border-radius:6px; padding:8px; "
        "font-size:13px; }");
    lay->addWidget(m_wikiContent, 1);

    connect(m_wikiQuery, &QLineEdit::returnPressed, btnFetch, &QPushButton::click);

    auto* nam = new QNetworkAccessManager(panel);

    auto enableActions = [panel, btnElabora]() {
        btnElabora->setEnabled(true);
        const auto btns = panel->findChildren<QPushButton*>("wikiActBtn");
        for (auto* b : btns) b->setEnabled(true);
    };

    connect(btnFetch, &QPushButton::clicked, this, [this, nam, enableActions]() {
        const QString q    = m_wikiQuery->text().trimmed();
        const QString lang = m_wikiLangCombo->currentData().toString();
        if (q.isEmpty()) return;

        m_wikiContent->clear();
        m_wikiExtract.clear();
        m_wikiWaitLbl->setText(
            "\xe2\x8f\xb3  Ricerca su Wikipedia (" + lang + ")...");
        m_wikiWaitLbl->setVisible(true);

        const QString encoded = QUrl::toPercentEncoding(q);
        const QUrl url(
            QString("https://%1.wikipedia.org/api/rest_v1/page/summary/%2")
            .arg(lang, encoded));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      "Prismalux/2.8 (wildlux@gmail.com)");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

        auto* reply = nam->get(req);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, q, lang, enableActions]() {
            reply->deleteLater();
            m_wikiWaitLbl->setVisible(false);

            if (reply->error() != QNetworkReply::NoError) {
                m_wikiContent->setPlainText(
                    "\xe2\x9d\x8c  Errore: " + reply->errorString() +
                    "\n\nSuggerimento: verifica l\xe2\x80\x99" "ortografia del titolo "
                    "e prova in altra lingua.");
                return;
            }

            const QJsonDocument doc =
                QJsonDocument::fromJson(reply->readAll());
            const QJsonObject obj = doc.object();

            const QString type = obj["type"].toString();
            if (type == "disambiguation") {
                m_wikiContent->setPlainText(
                    "\xf0\x9f\x93\x8b  Pagina di disambiguazione per \"" + q + "\".\n\n"
                    "Prova un titolo pi\xc3\xb9 specifico, ad esempio:\n"
                    + obj["extract"].toString().left(300));
                return;
            }
            if (type.contains("not_found") || obj["title"].toString().isEmpty()) {
                m_wikiContent->setPlainText(
                    "\xf0\x9f\x8c\xab  Articolo \"" + q + "\" non trovato in " + lang +
                    ".wikipedia.org.\n\n"
                    "Suggerimento: prova in inglese (en) o verifica il titolo.");
                return;
            }

            const QString title   = obj["title"].toString();
            const QString desc    = obj["description"].toString();
            const QString extract = obj["extract"].toString();

            m_wikiExtract = QString("Titolo: %1\nDescrizione: %2\n\n%3")
                            .arg(title, desc, extract);

            QString html;
            html += QString(
                "<h2 style='color:#60a5fa;margin-bottom:4px;'>%1</h2>"
                "<p style='color:#94a3b8;font-size:11px;margin:0 0 12px 0;"
                "font-style:italic;'>%2</p>")
                .arg(title.toHtmlEscaped(), desc.toHtmlEscaped());

            for (const QString& para : extract.split("\n\n", Qt::SkipEmptyParts)) {
                html += "<p style='color:#e2e8f0;margin:0 0 8px 0;'>"
                      + para.toHtmlEscaped() + "</p>";
            }
            html += QString(
                "<p style='color:#64748b;font-size:10px;margin-top:12px;'>"
                "\xf0\x9f\x8c\x90  Fonte: <a href='https://%1.wikipedia.org/wiki/%2' "
                "style='color:#3b82f6;'>%1.wikipedia.org/%3</a></p>")
                .arg(lang,
                     QUrl::toPercentEncoding(title),
                     title.toHtmlEscaped());

            m_wikiContent->setHtml(html);
            enableActions();
        });
    });

    connect(btnElabora, &QPushButton::clicked, this, [this]() {
        if (m_wikiExtract.isEmpty()) return;
        const QString sys =
            "Sei un assistente esperto. Usa le informazioni dell\xe2\x80\x99" "articolo Wikipedia "
            "come contesto. L\xe2\x80\x99" "utente ti pone domande sull\xe2\x80\x99" "argomento. "
            "Rispondi in modo preciso, critico e utile. Rispondi in italiano.";
        m_wikiContent->append("\n\n--- AI ---\n");
        auto* h = new QObject(this);
        connect(m_ai, &AiClient::token, h, [this](const QString& t){
            QTextCursor c = m_wikiContent->textCursor();
            c.movePosition(QTextCursor::End);
            m_wikiContent->setTextCursor(c);
            m_wikiContent->insertPlainText(t);
        });
        connect(m_ai, &AiClient::finished, h, [h](const QString&){ h->deleteLater(); });
        connect(m_ai, &AiClient::error, h, [this, h](const QString& msg){
            m_wikiContent->append("\n\xe2\x9d\x8c  " + msg);
            h->deleteLater();
        });
        const QString question = m_wikiQuery->text().trimmed();
        m_ai->chat(P::prependKnowledge(sys),
                "[Articolo Wikipedia]\n" + m_wikiExtract + "\n\n" + question);
    });

    return panel;
}

/* ══════════════════════════════════════════════════════════════
   buildDatiTab
   ══════════════════════════════════════════════════════════════ */
QWidget* StrumentiFilePage::buildDatiTab()
{
    auto* panel = new QWidget(this);
    auto* vbox  = new QVBoxLayout(panel);
    vbox->setContentsMargins(12, 12, 12, 12);
    vbox->setSpacing(8);

    auto* titleLbl = new QLabel(
        "<b>\xf0\x9f\x93\x8a  Dati AI</b> \xe2\x80\x94 "
        "Analisi intelligente di Excel, CSV e ODS", panel);
    titleLbl->setTextFormat(Qt::RichText);
    vbox->addWidget(titleLbl);

    auto* fileRow = new QWidget(panel);
    auto* fileLay = new QHBoxLayout(fileRow);
    fileLay->setContentsMargins(0, 0, 0, 0);
    fileLay->setSpacing(8);
    auto* fileBtn = new QPushButton(
        "\xf0\x9f\x93\x82  Carica dati", fileRow);
    fileBtn->setObjectName("actionBtn");
    fileBtn->setFixedWidth(140);
    m_datiFileLbl = new QLabel("Nessun file caricato", fileRow);
    m_datiFileLbl->setObjectName("hintLabel");
    fileLay->addWidget(fileBtn);
    fileLay->addWidget(m_datiFileLbl, 1);
    vbox->addWidget(fileRow);

    auto* hintLbl = new QLabel(
        "\xe2\x84\xb9  Formati supportati: XLSX, XLS, CSV, ODS\n"
        "Excel richiede <code>pip install openpyxl</code> (gi\xc3\xa0 in requirements.txt)",
        panel);
    hintLbl->setTextFormat(Qt::RichText);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    vbox->addWidget(hintLbl);

    auto* actionRow = new QWidget(panel);
    auto* actionLay = new QHBoxLayout(actionRow);
    actionLay->setContentsMargins(0, 0, 0, 0);
    actionLay->setSpacing(8);
    auto* actionLbl = new QLabel("Analisi:", actionRow);
    m_datiActionCombo = new QComboBox(actionRow);
    m_datiActionCombo->addItems({
        "\xf0\x9f\x93\x8a  Analisi generale",
        "\xf0\x9f\x94\x8d  Trova pattern e tendenze",
        "\xf0\x9f\x93\x88  Statistiche descrittive",
        "\xf0\x9f\x9b\xa1  Anomalie e outlier",
        "\xf0\x9f\x92\xa1  Suggerisci grafici",
        "\xf0\x9f\xa7\xb9  Pulizia dati (problemi rilevati)",
        "\xf0\x9f\x94\x97  Correlazioni tra colonne",
        "\xf0\x9f\x93\x9d  Riassunto esecutivo"
    });
    m_datiActionCombo->setFixedWidth(230);
    actionLay->addWidget(actionLbl);
    actionLay->addWidget(m_datiActionCombo);
    actionLay->addStretch();
    vbox->addWidget(actionRow);

    auto* btnRow = new QWidget(panel);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);
    auto* previewBtn = new QPushButton(
        "\xf0\x9f\x91\x81  1. Anteprima", btnRow);
    previewBtn->setObjectName("actionBtn");
    auto* analyzeBtn = new QPushButton(
        "\xf0\x9f\xa4\x96  2. Analizza con AI", btnRow);
    analyzeBtn->setObjectName("actionBtn");
    btnLay->addWidget(previewBtn);
    btnLay->addWidget(analyzeBtn);
    btnLay->addStretch();
    vbox->addWidget(btnRow);

    auto* splitter = new QSplitter(Qt::Horizontal, panel);

    auto* previewGroup = new QGroupBox(
        "\xf0\x9f\x91\x81  Anteprima dati (prime 20 righe)", splitter);
    auto* pgLay = new QVBoxLayout(previewGroup);
    pgLay->setContentsMargins(4, 4, 4, 4);
    m_datiPreview = new QTextEdit(previewGroup);
    m_datiPreview->setObjectName("chatLog");
    m_datiPreview->setReadOnly(true);
    m_datiPreview->setFont(QFont("Monospace", 9));
    m_datiPreview->setPlaceholderText(
        "L'anteprima del foglio appare qui...\n\n"
        "Carica un file Excel, CSV o ODS per iniziare.");
    pgLay->addWidget(m_datiPreview);
    splitter->addWidget(previewGroup);

    auto* outputGroup = new QGroupBox(
        "\xf0\x9f\xa4\x96  Analisi AI", splitter);
    auto* ogLay = new QVBoxLayout(outputGroup);
    ogLay->setContentsMargins(4, 4, 4, 4);
    m_datiOutput = new QTextEdit(outputGroup);
    m_datiOutput->setObjectName("chatLog");
    m_datiOutput->setReadOnly(true);
    m_datiOutput->setPlaceholderText(
        "L'analisi AI appare qui...\n\n"
        "Clicca 'Analizza con AI' dopo aver caricato i dati.");
    ogLay->addWidget(m_datiOutput);
    splitter->addWidget(outputGroup);

    splitter->setSizes({350, 450});
    vbox->addWidget(splitter, 1);

    connect(fileBtn, &QPushButton::clicked, this, [this, previewBtn]{
        const QString path = QFileDialog::getOpenFileName(
            this,
            "Seleziona file dati",
            QDir::homePath(),
            "Dati (*.xlsx *.xls *.csv *.ods);;"
            "CSV (*.csv *.tsv *.txt);;"
            "Tutti i file (*)");
        if (path.isEmpty()) return;

        m_datiFileLbl->setText(QFileInfo(path).fileName());
        m_datiPreview->clear();
        m_datiOutput->clear();
        m_datiCsvText.clear();

        const QString ext = QFileInfo(path).suffix().toLower();

        if (ext == "csv" || ext == "tsv" || ext == "txt") {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_datiPreview->setPlainText("\xe2\x9d\x8c  Impossibile aprire il file.");
                return;
            }
            QTextStream in(&f);
            QString all = in.readAll();
            m_datiCsvText = all;
            QStringList lines = all.split('\n', Qt::SkipEmptyParts);
            const int nShow = qMin(lines.size(), 20);
            QString preview;
            for (int i = 0; i < nShow; ++i)
                preview += lines[i] + "\n";
            if (lines.size() > 20)
                preview += QString("... (%1 righe totali)").arg(lines.size());
            m_datiPreview->setPlainText(preview);
        } else {
            const QString python = P::findPython();
            auto* tmpScript = new QTemporaryFile(
                P::safeTempPath() + "/prisma_xls_XXXXXX.py", this);
            tmpScript->setAutoRemove(false);
            if (!tmpScript->open()) {
                m_datiPreview->setPlainText(
                    "\xe2\x9d\x8c  Impossibile creare script temporaneo.");
                return;
            }
            {
                QTextStream ts(tmpScript);
                ts << "import sys, csv, io\n"
                   << "path = sys.argv[1]\n"
                   << "try:\n"
                   << "    import openpyxl\n"
                   << "    wb = openpyxl.load_workbook(path, read_only=True, data_only=True)\n"
                   << "    ws = wb.active\n"
                   << "    out = io.StringIO()\n"
                   << "    w = csv.writer(out)\n"
                   << "    for row in ws.iter_rows(values_only=True):\n"
                   << "        w.writerow(['' if c is None else str(c) for c in row])\n"
                   << "    print(out.getvalue(), end='')\n"
                   << "except ImportError:\n"
                   << "    try:\n"
                   << "        import xlrd\n"
                   << "        wb = xlrd.open_workbook(path)\n"
                   << "        ws = wb.sheet_by_index(0)\n"
                   << "        out = io.StringIO()\n"
                   << "        w = csv.writer(out)\n"
                   << "        for i in range(ws.nrows):\n"
                   << "            w.writerow([str(ws.cell_value(i,j)) for j in range(ws.ncols)])\n"
                   << "        print(out.getvalue(), end='')\n"
                   << "    except ImportError:\n"
                   << "        print('ERR:no_openpyxl', end='')\n";
            }
            tmpScript->close();
            const QString script = tmpScript->fileName();

            m_datiPreview->setPlainText("\xe2\x8c\x9b  Conversione in corso...");

            auto* proc = new QProcess(this);
            proc->start(python, {script, path});
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, proc, tmpScript](int code, QProcess::ExitStatus){
                const QString text = QString::fromUtf8(
                    proc->readAllStandardOutput()).trimmed();
                proc->deleteLater();
                tmpScript->remove();
                tmpScript->deleteLater();
                if (code == 0 && !text.isEmpty() && !text.startsWith("ERR:")) {
                    m_datiCsvText = text;
                    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
                    const int nShow = qMin(lines.size(), 20);
                    QString preview;
                    for (int i = 0; i < nShow; ++i)
                        preview += lines[i] + "\n";
                    if (lines.size() > 20)
                        preview += QString("... (%1 righe totali)").arg(lines.size());
                    m_datiPreview->setPlainText(preview);
                } else if (text.startsWith("ERR:no_openpyxl")) {
                    m_datiPreview->setPlainText(
                        "\xe2\x9d\x8c  openpyxl non installato.\n"
                        "Installa con: pip install openpyxl");
                } else {
                    m_datiPreview->setPlainText(
                        "\xe2\x9d\x8c  Conversione fallita.\n"
                        "Controlla che il file non sia corrotto.");
                }
            });
        }
    });

    connect(previewBtn, &QPushButton::clicked, this, [this]{
        if (m_datiCsvText.isEmpty()) {
            m_datiPreview->setPlainText(
                "\xe2\x9d\x8c  Carica prima un file.");
        }
    });

    static const char* kDatiActions[] = {
        "Analizza il seguente CSV e fornisci un'analisi generale: struttura, colonne, tipo di dati, qualit\xc3\xa0. Rispondi in italiano.",
        "Analizza il seguente CSV e identifica pattern, tendenze e correlazioni interessanti. Sii specifico con i dati. Rispondi in italiano.",
        "Calcola le statistiche descrittive delle colonne numeriche nel seguente CSV: media, mediana, min, max, deviazione standard. Rispondi in italiano.",
        "Identifica anomalie, outlier e valori sospetti nel seguente CSV. Spiega cosa potrebbe causarli. Rispondi in italiano.",
        "Analizza il seguente CSV e suggerisci i tipi di grafici pi\xc3\xb9 efficaci per visualizzare i dati. Descrivi ogni grafico. Rispondi in italiano.",
        "Analizza la qualit\xc3\xa0 dei dati nel seguente CSV: valori mancanti, duplicati, formati inconsistenti, problemi da correggere. Rispondi in italiano.",
        "Analizza le correlazioni tra le colonne del seguente CSV. Identifica le relazioni pi\xc3\xb9 significative. Rispondi in italiano.",
        "Crea un riassunto esecutivo del seguente CSV come se dovessi presentarlo a un manager. Includi insight chiave. Rispondi in italiano.",
    };

    connect(analyzeBtn, &QPushButton::clicked, this, [this]{
        if (m_datiCsvText.isEmpty()) {
            m_datiOutput->setPlainText(
                "\xe2\x9d\x8c  Carica prima un file dati.");
            return;
        }
        const int idx = m_datiActionCombo->currentIndex();
        const QString sys = P::prependKnowledge(
            idx >= 0 && idx < 8 ? QString::fromUtf8(kDatiActions[idx])
                                : "Analizza il CSV fornito. Rispondi in italiano.");

        QString csv = m_datiCsvText;
        if (csv.size() > 12000)
            csv = csv.left(12000) + "\n... [troncato per limiti di contesto]";

        m_datiOutput->clear();

        auto* h = new QObject(this);
        connect(m_ai, &AiClient::token, h, [this](const QString& t){
            m_datiOutput->moveCursor(QTextCursor::End);
            m_datiOutput->insertPlainText(t);
        });
        connect(m_ai, &AiClient::finished, h, [h](const QString&){ h->deleteLater(); });
        connect(m_ai, &AiClient::error, h, [this, h](const QString& msg){
            m_datiOutput->append("\n\xe2\x9d\x8c  " + msg);
            h->deleteLater();
        });

        m_ai->chat(sys,
            "Dati CSV da analizzare:\n\n```csv\n" + csv + "\n```");
    });

    return panel;
}

/* ══════════════════════════════════════════════════════════════
   buildPdfTab
   ══════════════════════════════════════════════════════════════ */
QWidget* StrumentiFilePage::buildPdfTab()
{
    auto* panel = new QWidget(this);
    auto* vbox  = new QVBoxLayout(panel);
    vbox->setContentsMargins(12, 12, 12, 12);
    vbox->setSpacing(8);

    auto* titleLbl = new QLabel(
        "<b>\xf0\x9f\x93\x84  Analisi PDF</b> \xe2\x80\x94 "
        "Carica un PDF e analizzalo con AI", panel);
    titleLbl->setTextFormat(Qt::RichText);
    vbox->addWidget(titleLbl);

    auto* fileRow = new QWidget(panel);
    auto* fileLay = new QHBoxLayout(fileRow);
    fileLay->setContentsMargins(0, 0, 0, 0);
    fileLay->setSpacing(8);
    auto* fileBtn = new QPushButton(
        "\xf0\x9f\x93\x84  Carica PDF", fileRow);
    fileBtn->setObjectName("actionBtn");
    fileBtn->setFixedWidth(130);
    m_pdfFileLbl = new QLabel("Nessun PDF caricato", fileRow);
    m_pdfFileLbl->setObjectName("hintLabel");
    fileLay->addWidget(fileBtn);
    fileLay->addWidget(m_pdfFileLbl, 1);
    vbox->addWidget(fileRow);

    auto* hintLbl = new QLabel(
        "\xe2\x84\xb9  Richiede <code>pdftotext</code> (poppler-utils) "
        "oppure Python <code>pdfplumber</code>.<br>"
        "Installa: <code>sudo apt install poppler-utils</code> "
        "oppure <code>pip install pdfplumber</code>",
        panel);
    hintLbl->setTextFormat(Qt::RichText);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    vbox->addWidget(hintLbl);

    auto* actionRow = new QWidget(panel);
    auto* actionLay = new QHBoxLayout(actionRow);
    actionLay->setContentsMargins(0, 0, 0, 0);
    actionLay->setSpacing(8);
    auto* actionLbl = new QLabel("Azione:", actionRow);
    m_pdfActionCombo = new QComboBox(actionRow);
    m_pdfActionCombo->addItems({
        "\xf0\x9f\x93\x9d  Riassumi",
        "\xf0\x9f\x92\xac  Q&A sul documento",
        "\xf0\x9f\x94\x91  Punti chiave",
        "\xf0\x9f\x93\x96  Parafrasi",
        "\xf0\x9f\xa7\xaa  Analisi critica"
    });
    m_pdfActionCombo->setFixedWidth(230);
    actionLay->addWidget(actionLbl);
    actionLay->addWidget(m_pdfActionCombo);
    actionLay->addStretch();
    vbox->addWidget(actionRow);

    auto* btnAnalyze = new QPushButton(
        "\xf0\x9f\xa4\x96  Analizza con AI", panel);
    btnAnalyze->setObjectName("actionBtn");
    vbox->addWidget(btnAnalyze);

    m_pdfOutput = new QTextEdit(panel);
    m_pdfOutput->setObjectName("chatLog");
    m_pdfOutput->setReadOnly(true);
    m_pdfOutput->setPlaceholderText(
        "Il risultato dell'analisi appare qui...\n\n"
        "Carica un PDF e clicca 'Analizza con AI'.");
    vbox->addWidget(m_pdfOutput, 1);

    connect(fileBtn, &QPushButton::clicked, this, [this]{
        const QString path = QFileDialog::getOpenFileName(
            this, "Seleziona PDF", QDir::homePath(),
            "PDF (*.pdf);;Tutti i file (*)");
        if (path.isEmpty()) return;
        m_pdfPath = path;
        m_pdfText.clear();
        m_pdfFileLbl->setText(QFileInfo(path).fileName());
        m_pdfOutput->clear();
    });

    connect(btnAnalyze, &QPushButton::clicked, this, [this]{
        if (m_pdfPath.isEmpty()) {
            m_pdfOutput->setPlainText("\xe2\x9d\x8c  Carica prima un file PDF.");
            return;
        }

        static const char* kPdfActions[] = {
            "Riassumi il seguente documento PDF in modo conciso e strutturato. Rispondi in italiano.",
            "Rispondi alle domande dell'utente basandoti sul contenuto del documento PDF. Rispondi in italiano.",
            "Estrai i punti chiave e le informazioni principali dal documento PDF. Elencali numerati. Rispondi in italiano.",
            "Riscrivi il documento con parole pi\xc3\xb9 semplici e accessibili mantenendo il senso originale. Rispondi in italiano.",
            "Analizza criticamente il documento: punti forti, punti deboli, coerenza, qualit\xc3\xa0 delle argomentazioni. Rispondi in italiano.",
        };
        const int idx = m_pdfActionCombo->currentIndex();
        const QString sys = P::prependKnowledge(
            idx >= 0 && idx < 5 ? QString::fromUtf8(kPdfActions[idx])
                                : "Analizza il documento fornito. Rispondi in italiano.");

        /* Estrazione testo: prova pdftotext, poi pdfplumber */
        auto runAnalysis = [this, sys](const QString& text){
            if (text.trimmed().isEmpty()) {
                m_pdfOutput->setPlainText(
                    "\xe2\x9a\xa0  Impossibile estrarre testo dal PDF.\n"
                    "Il file potrebbe essere scansionato (solo immagine).");
                return;
            }
            m_pdfText = text;
            m_pdfOutput->clear();
            auto* h = new QObject(this);
            connect(m_ai, &AiClient::token, h, [this](const QString& t){
                m_pdfOutput->moveCursor(QTextCursor::End);
                m_pdfOutput->insertPlainText(t);
            });
            connect(m_ai, &AiClient::finished, h, [h](const QString&){ h->deleteLater(); });
            connect(m_ai, &AiClient::error, h, [this, h](const QString& msg){
                m_pdfOutput->append("\n\xe2\x9d\x8c  " + msg);
                h->deleteLater();
            });
            QString doc = text;
            if (doc.size() > 14000)
                doc = doc.left(14000) + "\n... [troncato per limiti di contesto]";
            m_ai->chat(sys, "DOCUMENTO PDF:\n\n" + doc);
        };

        if (!m_pdfText.isEmpty()) {
            runAnalysis(m_pdfText);
            return;
        }

        m_pdfOutput->setPlainText("\xe2\x8c\x9b  Estrazione testo in corso...");

        /* Prova pdftotext */
        auto* proc = new QProcess(this);
        proc->start("pdftotext", {m_pdfPath, "-"});
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, runAnalysis](int code, QProcess::ExitStatus){
            const QString text = QString::fromUtf8(
                proc->readAllStandardOutput()).trimmed();
            proc->deleteLater();
            if (code == 0 && !text.isEmpty()) {
                runAnalysis(text);
                return;
            }
            /* Fallback: Python pdfplumber */
            const QString script =
                "import sys, pdfplumber\n"
                "with pdfplumber.open(sys.argv[1]) as pdf:\n"
                "    print('\\n\\n'.join(p.extract_text() or '' for p in pdf.pages))\n";
            auto* proc2 = new QProcess(this);
            proc2->start(P::findPython(), {"-c", script, m_pdfPath});
            connect(proc2, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, proc2, runAnalysis](int code2, QProcess::ExitStatus){
                const QString text2 = QString::fromUtf8(
                    proc2->readAllStandardOutput()).trimmed();
                proc2->deleteLater();
                if (code2 == 0 && !text2.isEmpty()) {
                    runAnalysis(text2);
                } else {
                    m_pdfOutput->setPlainText(
                        "\xe2\x9d\x8c  Impossibile estrarre testo dal PDF.\n\n"
                        "Installa uno dei seguenti:\n"
                        "  sudo apt install poppler-utils\n"
                        "  pip install pdfplumber");
                }
            });
        });
    });

    return panel;
}

/* ══════════════════════════════════════════════════════════════
   buildWordTab
   ══════════════════════════════════════════════════════════════ */
QWidget* StrumentiFilePage::buildWordTab()
{
    auto* panel = new QWidget(this);
    auto* vbox  = new QVBoxLayout(panel);
    vbox->setContentsMargins(12, 12, 12, 12);
    vbox->setSpacing(8);

    auto* titleLbl = new QLabel(
        "<b>\xf0\x9f\x93\x9d  Word / Testo</b> \xe2\x80\x94 "
        "Analisi AI di documenti Word, testo e codice", panel);
    titleLbl->setTextFormat(Qt::RichText);
    vbox->addWidget(titleLbl);

    /* File picker */
    auto* fileRow = new QWidget(panel);
    auto* fileLay = new QHBoxLayout(fileRow);
    fileLay->setContentsMargins(0, 0, 0, 0);
    fileLay->setSpacing(8);
    auto* fileBtn = new QPushButton(
        "\xf0\x9f\x93\x82  Carica file", fileRow);
    fileBtn->setObjectName("actionBtn");
    fileBtn->setFixedWidth(130);
    auto* fileLbl = new QLabel("Nessun file caricato", fileRow);
    fileLbl->setObjectName("hintLabel");
    fileLay->addWidget(fileBtn);
    fileLay->addWidget(fileLbl, 1);
    vbox->addWidget(fileRow);

    auto* hintLbl = new QLabel(
        "\xe2\x84\xb9  Formati testo puro: TXT, MD, RST, PY, CPP, H, JS, JSON...\n"
        "Per .docx/.odt richiede Python <code>python-docx</code> / <code>odfpy</code>.\n"
        "Installa: <code>pip install python-docx odfpy</code>",
        panel);
    hintLbl->setTextFormat(Qt::RichText);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    vbox->addWidget(hintLbl);

    auto* actionRow = new QWidget(panel);
    auto* actionLay = new QHBoxLayout(actionRow);
    actionLay->setContentsMargins(0, 0, 0, 0);
    actionLay->setSpacing(8);
    auto* actionLbl = new QLabel("Azione:", actionRow);
    auto* actionCombo = new QComboBox(actionRow);
    actionCombo->addItems({
        "\xf0\x9f\x93\x9d  Riassumi",
        "\xf0\x9f\x96\x8a  Analizza stile",
        "\xf0\x9f\x8c\x8d  Traduci in italiano",
        "\xf0\x9f\x90\x9b  Trova bug (per codice)",
        "\xf0\x9f\x93\x8b  Genera domande"
    });
    actionCombo->setFixedWidth(230);
    actionLay->addWidget(actionLbl);
    actionLay->addWidget(actionCombo);
    actionLay->addStretch();
    vbox->addWidget(actionRow);

    auto* btnAnalyze = new QPushButton(
        "\xf0\x9f\xa4\x96  Analizza con AI", panel);
    btnAnalyze->setObjectName("actionBtn");
    vbox->addWidget(btnAnalyze);

    auto* outputEdit = new QTextEdit(panel);
    outputEdit->setObjectName("chatLog");
    outputEdit->setReadOnly(true);
    outputEdit->setPlaceholderText(
        "Il risultato dell'analisi appare qui...\n\n"
        "Carica un file di testo e clicca 'Analizza con AI'.");
    vbox->addWidget(outputEdit, 1);

    /* Stato interno */
    auto* fileContent = new QString;  /* owned by lambda captures */

    connect(fileBtn, &QPushButton::clicked, this, [this, fileLbl, outputEdit, fileContent]{
        const QString path = QFileDialog::getOpenFileName(
            this,
            "Seleziona file",
            QDir::homePath(),
            "Testo (*.txt *.md *.rst *.py *.cpp *.h *.js *.ts *.json *.yaml *.toml *.ini);;"
            "Word/ODT (*.docx *.odt);;"
            "Tutti i file (*)");
        if (path.isEmpty()) return;

        fileLbl->setText(QFileInfo(path).fileName());
        fileContent->clear();
        outputEdit->clear();

        const QString ext = QFileInfo(path).suffix().toLower();

        if (ext == "docx") {
            const QString script =
                "import sys, docx\n"
                "doc = docx.Document(sys.argv[1])\n"
                "print('\\n'.join(p.text for p in doc.paragraphs))\n";
            auto* proc = new QProcess(this);
            proc->start(P::findPython(), {"-c", script, path});
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [outputEdit, fileContent, proc](int code, QProcess::ExitStatus){
                const QString text = QString::fromUtf8(
                    proc->readAllStandardOutput()).trimmed();
                proc->deleteLater();
                if (code == 0 && !text.isEmpty()) {
                    *fileContent = text;
                    outputEdit->setPlainText(
                        "\xe2\x9c\x85  File caricato ("
                        + QString::number(text.size()) + " caratteri). "
                        "Clicca 'Analizza con AI' per procedere.");
                } else {
                    outputEdit->setPlainText(
                        "\xe2\x9d\x8c  Impossibile leggere il .docx.\n"
                        "Installa: pip install python-docx");
                }
            });
        } else if (ext == "odt") {
            const QString script =
                "import sys\n"
                "from odf.opendocument import load\n"
                "from odf.text import P\n"
                "doc = load(sys.argv[1])\n"
                "body = doc.text\n"
                "paras = body.getElementsByType(P)\n"
                "print('\\n'.join(''.join(n.data for n in p.childNodes if hasattr(n,'data')) for p in paras))\n";
            auto* proc = new QProcess(this);
            proc->start(P::findPython(), {"-c", script, path});
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [outputEdit, fileContent, proc](int code, QProcess::ExitStatus){
                const QString text = QString::fromUtf8(
                    proc->readAllStandardOutput()).trimmed();
                proc->deleteLater();
                if (code == 0 && !text.isEmpty()) {
                    *fileContent = text;
                    outputEdit->setPlainText(
                        "\xe2\x9c\x85  File caricato ("
                        + QString::number(text.size()) + " caratteri). "
                        "Clicca 'Analizza con AI' per procedere.");
                } else {
                    outputEdit->setPlainText(
                        "\xe2\x9d\x8c  Impossibile leggere l'.odt.\n"
                        "Installa: pip install odfpy");
                }
            });
        } else {
            /* Testo puro */
            QFile f(path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream ts(&f);
                *fileContent = ts.readAll();
                outputEdit->setPlainText(
                    "\xe2\x9c\x85  File caricato ("
                    + QString::number(fileContent->size()) + " caratteri). "
                    "Clicca 'Analizza con AI' per procedere.");
            } else {
                outputEdit->setPlainText("\xe2\x9d\x8c  Impossibile aprire il file.");
            }
        }
    });

    connect(btnAnalyze, &QPushButton::clicked, this, [this, outputEdit, actionCombo, fileContent]{
        if (fileContent->isEmpty()) {
            outputEdit->setPlainText("\xe2\x9d\x8c  Carica prima un file.");
            return;
        }
        static const char* kWordActions[] = {
            "Riassumi il seguente testo in modo conciso e strutturato. Rispondi in italiano.",
            "Analizza lo stile di scrittura del seguente testo: chiarezza, registro, coerenza, punti di forza e debolezza. Rispondi in italiano.",
            "Traduci il seguente testo in italiano, mantenendo il significato e il tono originale.",
            "Analizza il seguente codice e identifica bug, problemi di sicurezza e suggerimenti di miglioramento. Rispondi in italiano.",
            "Genera 10 domande di comprensione sul seguente testo, con risposta breve. Rispondi in italiano.",
        };
        const int idx = actionCombo->currentIndex();
        const QString sys = P::prependKnowledge(
            idx >= 0 && idx < 5 ? QString::fromUtf8(kWordActions[idx])
                                : "Analizza il testo fornito. Rispondi in italiano.");

        QString text = *fileContent;
        if (text.size() > 14000)
            text = text.left(14000) + "\n... [troncato per limiti di contesto]";

        outputEdit->clear();

        auto* h = new QObject(this);
        connect(m_ai, &AiClient::token, h, [outputEdit](const QString& t){
            outputEdit->moveCursor(QTextCursor::End);
            outputEdit->insertPlainText(t);
        });
        connect(m_ai, &AiClient::finished, h, [h](const QString&){ h->deleteLater(); });
        connect(m_ai, &AiClient::error, h, [outputEdit, h](const QString& msg){
            outputEdit->append("\n\xe2\x9d\x8c  " + msg);
            h->deleteLater();
        });
        m_ai->chat(sys, "DOCUMENTO:\n\n" + text);
    });

    return panel;
}

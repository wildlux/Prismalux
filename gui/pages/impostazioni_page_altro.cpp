#include "impostazioni_page.h"
#include "../widgets/toggle_switch.h"
#include "../widgets/stt_whisper.h"
#include "personalizza_page.h"
#include "manutenzione_page.h"
#include "grafico_page.h"
#include "../theme_manager.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QButtonGroup>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <functional>
#include <QCoreApplication>
#include <QTimer>
#include <QRadioButton>
#include <QCheckBox>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QThread>
#include <QStackedWidget>
#include <QColorDialog>
#include <QPainter>
#include <QPen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGuiApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QDialog>
#include <QRegularExpression>

/* ── Helper: estrae i colori chiave dal QSS corrente dell'applicazione ──
   Tutti i QSS Prismalux hanno nella riga di commento:
     accent #XXXXXX | txt1 #XXXXXX | txt2 #XXXXXX
   e la riga:
     QMainWindow, QWidget { background-color: #XXXXXX; }
   Fallback robusto se un tema custom non segue questo schema.            */
static void extractQssColors(QString& bg, QString& bg2, QString& bg3,
                              QString& text, QString& muted,
                              QString& accent, QString& border)
{
    const QString qss = ThemeManager::instance()->currentQss();

    auto rx = [&](const QString& pat) {
        auto m = QRegularExpression(pat).match(qss);
        return m.hasMatch() ? m.captured(1) : QString();
    };

    accent = rx("accent\\s+(#[0-9a-fA-F]{6,8})");
    text   = rx("txt1\\s+(#[0-9a-fA-F]{6,8})");
    muted  = rx("txt2\\s+(#[0-9a-fA-F]{6,8})");
    bg     = rx("QWidget\\s*\\{\\s*background-color:\\s*(#[0-9a-fA-F]{6,8})");

    /* Fallback per temi senza commento (venom_*, custom) */
    if (bg.isEmpty())     bg     = rx("background-color:\\s*(#[0-9a-fA-F]{6,8})");
    if (accent.isEmpty()) accent = "#00c8ff";
    if (text.isEmpty())   text   = "#e8eaf0";
    if (muted.isEmpty())  muted  = "#8890a8";
    if (bg.isEmpty())     bg     = "#0f1117";

    const QColor bgC(bg);
    const bool dark = bgC.lightness() < 128;
    bg2    = (dark ? bgC.lighter(118) : bgC.darker(107)).name();
    bg3    = (dark ? bgC.lighter(140) : bgC.darker(116)).name();
    border = (dark ? bgC.lighter(165) : bgC.darker(128)).name();
}

QWidget* ImpostazioniPage::buildRingraziamentiTab()
{
    /* ── CSS parametrizzato: %1=bg %2=text %3=accent %4=muted %5=border %6=bg3 %7=bg2 ── */
    static const char* kCssFmt =
        "body  { background:%1; color:%2; "
        "        font-family:'Segoe UI',system-ui,sans-serif; "
        "        font-size:13px; margin:0; padding:10px 14px; }"
        "h1    { color:%2; font-size:20px; font-weight:700; "
        "        text-align:center; margin:0 0 3px 0; }"
        ".sub  { text-align:center; color:%4; font-style:italic; "
        "        font-size:12px; margin:0 0 2px 0; }"
        ".ver  { text-align:center; color:%2; font-size:12px; "
        "        margin:0 0 2px 0; }"
        ".lnks { text-align:center; font-size:12px; margin:0 0 2px 0; }"
        ".bdg  { text-align:center; color:%4; font-size:11px; "
        "        margin:0 0 4px 0; }"
        "a     { color:%3; text-decoration:none; }"
        "hr    { border:none; border-top:1px solid %5; margin:7px 0; }"
        "h2    { color:%3; font-size:12px; font-weight:700; "
        "        letter-spacing:0.4px; margin:8px 0 3px 0; "
        "        text-transform:uppercase; }"
        "table { border-collapse:collapse; width:100%%; margin:0 0 4px 0; }"
        "th    { background:%6; color:%4; padding:5px 9px; "
        "        text-align:left; font-size:11px; font-weight:600; "
        "        letter-spacing:0.3px; border-bottom:1px solid %5; }"
        "td    { padding:5px 9px; vertical-align:top; "
        "        border-bottom:1px solid %5; font-size:12px; }"
        ".alt  { background:%7; }"
        ".nm   { font-weight:600; white-space:nowrap; }"
        ".lic  { color:%4; font-family:monospace; font-size:11px; "
        "        white-space:nowrap; }"
        ".pre  { background:%7; color:%4; font-family:monospace; "
        "        font-size:11px; padding:10px 14px; "
        "        border:1px solid %5; white-space:pre-wrap; "
        "        line-height:1.5; margin:0; }"
        ".mtt  { text-align:center; color:%4; font-style:italic; "
        "        font-size:12px; margin:6px 0 0 0; }";

    /* ── Dati librerie ── */
    struct LibRow { const char* nome; const char* scopo; const char* lic;
                    const char* lbl;  const char* url; };
    static const LibRow kLibs[] = {
        { "Qt6",              "Framework GUI cross-platform (Widgets, Network, Svg)",             "LGPL v3",    "github.com/qt/qtbase",             "https://github.com/qt/qtbase" },
        { "Ollama",           "Server locale per inferenza LLM via API REST",                     "MIT",        "github.com/ollama/ollama",         "https://github.com/ollama/ollama" },
        { "llama.cpp",        "Inferenza LLM ottimizzata CPU/GPU (GGUF) \xe2\x80\x94 G. Gerganov","MIT",        "github.com/ggerganov/llama.cpp",   "https://github.com/ggerganov/llama.cpp" },
        { "Piper TTS",        "Sintesi vocale locale offline ad alta qualit\xc3\xa0",              "MIT",        "github.com/rhasspy/piper",         "https://github.com/rhasspy/piper" },
        { "Whisper.cpp",      "Trascrizione audio STT locale (Georgi Gerganov)",                  "MIT",        "github.com/ggerganov/whisper.cpp", "https://github.com/ggerganov/whisper.cpp" },
        { "nomic-embed-text", "Modello embedding per RAG (ricerca semantica documenti)",           "Apache 2.0", "github.com/nomic-ai/contrastors",  "https://github.com/nomic-ai/contrastors" },
        { "OpenBLAS",         "Algebra lineare ottimizzata per llama.cpp su CPU",                 "BSD 3-Clause","github.com/OpenMathLib/OpenBLAS",  "https://github.com/OpenMathLib/OpenBLAS" },
        { "md4c",             "Parser Markdown C (integrato in Qt6::Gui)",                        "MIT",        "github.com/mity/md4c",             "https://github.com/mity/md4c" },
        { "Poppler",          "Estrazione testo da PDF per indicizzazione RAG",                   "GPL v2/LGPL","gitlab.freedesktop.org/poppler",   "https://gitlab.freedesktop.org/poppler/poppler" },
        { "OpenCode",         "Agente coding AI con server HTTP+SSE integrato",                   "MIT",        "github.com/sst/opencode",          "https://github.com/sst/opencode" },
        { "Python 3",         "Esecuzione codice generato dall'AI in sandbox controllata",        "PSF",        "github.com/python/cpython",        "https://github.com/python/cpython" },
    };

    /* ── Dati plugin MCP ── */
    struct McpRow { const char* nome; const char* desc; const char* lbl; const char* url; };
    static const McpRow kMCPs[] = {
        { "\xf0\x9f\x8e\xa8 Blender",   "Controllo 3D bpy: crea/sposta/ruota oggetti, materiali, luci, render.",                                                           "blender.org",                    "https://www.blender.org" },
        { "\xf0\x9f\x96\xa5 Office",    "Automazione LibreOffice Writer/Calc/Impress via UNO, python-docx, openpyxl.",                                                     "libreoffice.org",                "https://www.libreoffice.org" },
        { "\xf0\x9f\x83\x8f Anki",      "Generazione mazzi flashcard da testi, appunti o spiegazioni AI.",                                                                 "github.com/ankitects/anki",      "https://github.com/ankitects/anki" },
        { "\xf0\x9f\x96\xa5 KiCAD",     "PCB design assistita: netlist, piazzamento componenti, script da linguaggio naturale.",                                            "gitlab.com/kicad",               "https://gitlab.com/kicad/code/kicad" },
        { "\xf0\x9f\xa4\x96 TinyMCP",   "Programmazione Arduino, ESP32, STM32: l'AI genera, compila e carica firmware.",                                                   "arduino.cc",                     "https://www.arduino.cc" },
        { "\xf0\x9f\x96\xa5 OpenCode",  "Agente coding con server HTTP+SSE (porta 8092): sessioni sviluppo in streaming.",                                                 "github.com/sst/opencode",        "https://github.com/sst/opencode" },
        { "\xf0\x9f\x93\xb9 OBS",       "Controllo OBS Studio: rec, streaming, cambio scene e sorgenti.",                                                                  "github.com/royshil/obs-mcp",     "https://github.com/royshil/obs-mcp" },
        { "\xf0\x9f\x92\xac WhatsApp",  "Invio/ricezione messaggi WhatsApp tramite bridge MCP, su autorizzazione.",                                                        "github.com/lharries/whatsapp-mcp","https://github.com/lharries/whatsapp-mcp" },
        { "\xf0\x9f\x93\xac Telegram",  "Bot Telegram: messaggi, canali, gruppi e notifiche automatiche.",                                                                 "github.com/chigwell/telegram-mcp","https://github.com/chigwell/telegram-mcp" },
        { "\xf0\x9f\x93\x9d WordPress", "Crea/modifica post, pagine e media WordPress senza aprire il browser.",                                                           "github.com/Automattic/wordpress-mcp","https://github.com/Automattic/wordpress-mcp" },
        { "\xf0\x9f\x8e\xae Godot",     "Crea scene, nodi e script GDScript; genera giochi 2D/3D da descrizioni testuali.",                                                "github.com/Coding-Solo/godot-mcp","https://github.com/Coding-Solo/godot-mcp" },
        { "\xf0\x9f\x8c\x90 GNS3",      "Topologie di rete (router, switch, firewall), configurazione e simulazione.",                                                     "github.com/ChistokhinSV/gns3-mcp","https://github.com/ChistokhinSV/gns3-mcp" },
        { "\xf0\x9f\x94\xac Cytoscape", "Reti biologiche/sociali: grafi, layout, statistiche, visualizzazioni.",                                                           "cytoscape.org",                  "https://cytoscape.org/" },
        { "\xf0\x9f\x94\xac RDKit",     "Chemioinformatica: SMILES, fingerprint, similarit\xc3\xa0 molecolare, propriet\xc3\xa0 chimiche.",                               "rdkit.org",                      "https://www.rdkit.org/" },
        { "\xf0\x9f\xa7\xaa Avogadro",  "Modellazione molecolare 3D: strutture, ottimizzazione geometrica. Formati SDF/PDB/XYZ.",                                          "avogadro.cc",                    "https://avogadro.cc/" },
        { "\xf0\x9f\x8c\xbf Bioconda",  "Tool bioinformatica (BLAST, BWA, GATK, Samtools) via canale conda. Pipeline genomiche.",                                          "bioconda.github.io",             "https://bioconda.github.io/" },
        { "\xf0\x9f\x97\xba Graphviz",  "Mappe concettuali e grafi DOT: diagrammi architettura, alberi decisionali, PNG/SVG inline.",                                     "graphviz.org",                   "https://graphviz.org/" },
    };

    /* ── Costruzione HTML (tema-adattiva) ── */
    auto* browser = new QTextBrowser;
    browser->setOpenExternalLinks(true);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setObjectName("ringraziamentiView");

    auto updateHtml = [browser]() {
        QString bg, bg2, bg3, text, muted, accent, border;
        extractQssColors(bg, bg2, bg3, text, muted, accent, border);
        const QString css = QString(kCssFmt).arg(bg, text, accent, muted, border, bg3, bg2);

        QString h;
        h.reserve(32768);

        auto td  = [](const QString& cls, const QString& t) {
            return QString("<td class=\"%1\">%2</td>").arg(cls, t);
        };
        auto lnk = [](const QString& lbl, const QString& url) {
            return QString("<a href=\"%1\">%2</a>").arg(url, lbl);
        };

        int ri = 0;
        auto TR = [&](const QString& cells) -> QString {
            const QString cls = ((ri++ % 2) == 1) ? " class=\"alt\"" : "";
            return "<tr" + cls + ">" + cells + "</tr>\n";
        };

        h += "<html><head><style>" + css + "</style></head><body>\n";

    /* intestazione */
    h += "<h1>\xf0\x9f\x8d\xba&nbsp; Prismalux</h1>\n"
         "<p class=\"sub\">Piattaforma AI locale open-source &mdash; "
         "multi-agente &middot; RAG &middot; matematica &middot; grafici &middot; MCPs<br>"
         "&ldquo;Costruito per i mortali che aspirano alla saggezza.&rdquo;</p>\n"
         "<p class=\"ver\"><b>v2.9</b> &middot; Qt6/C++ &middot; Ollama &middot; "
         "llama.cpp &middot; Linux / Windows / macOS</p>\n"
         "<p class=\"lnks\">"
         "<a href=\"https://github.com/wildlux/Prismalux\">&#128279; github.com/wildlux/Prismalux</a>"
         " &nbsp;&middot;&nbsp; "
         "<a href=\"https://github.com/wildlux/Prismalux/releases\">&#128230; Release</a>"
         " &nbsp;&middot;&nbsp; "
         "<a href=\"https://github.com/wildlux/Prismalux/issues\">&#128027; Bug</a>"
         " &nbsp;&middot;&nbsp; "
         "<a href=\"https://github.com/wildlux/Prismalux/wiki\">&#128214; Wiki</a>"
         " &nbsp;&middot;&nbsp; "
         "<a href=\"https://github.com/wildlux/Prismalux/blob/master/LICENSE\">&#128220; MIT</a></p>\n"
         "<p class=\"bdg\">Pipeline 6 agenti &middot; Motore Byzantino &middot; "
         "110 algoritmi &middot; RAG locale &middot; Matematica offline &middot; "
         "TTS/STT &middot; Cron scheduler &middot; 17 MCP integrati</p>\n"
         "<hr>\n";

    /* autore */
    h += "<h2>&#128100;&nbsp; Autore</h2>\n"
         "<table><tr>"
         "<th style=\"width:130px\">Nome</th>"
         "<th>Contributo</th>"
         "<th style=\"width:180px\">GitHub</th></tr>\n";
    ri = 0;
    h += TR(
        td("nm", "Paolo Lo Bello") +
        td("",   "Ideazione, progettazione e sviluppo integrale di Prismalux. "
                 "GUI C++/Qt6 &middot; Pipeline multi-agente &middot; Motore Byzantino "
                 "&middot; RAG locale &middot; Simulatore 110 algoritmi &middot; "
                 "Matematica locale &middot; Integrazione MCPs.") +
        td("",   lnk("github.com/wildlux", "https://github.com/wildlux"))
    );
    h += "</table><hr>\n";

    /* librerie */
    h += "<h2>&#128218;&nbsp; Librerie e backend</h2>\n"
         "<table><tr>"
         "<th style=\"width:130px\">Progetto</th>"
         "<th>Scopo</th>"
         "<th style=\"width:100px\">Licenza</th>"
         "<th style=\"width:210px\">Repository</th></tr>\n";
    ri = 0;
    for (const auto& r : kLibs) {
        h += TR(
            td("nm",  r.nome) +
            td("",    r.scopo) +
            td("lic", r.lic) +
            td("",    lnk(r.lbl, r.url))
        );
    }
    h += "</table><hr>\n";

    /* plugin MCP */
    h += "<h2>&#128300;&nbsp; Plugin MCP integrati</h2>\n"
         "<table><tr>"
         "<th style=\"width:140px\">Plugin</th>"
         "<th>Descrizione</th>"
         "<th style=\"width:200px\">Progetto</th></tr>\n";
    ri = 0;
    for (const auto& r : kMCPs) {
        h += TR(
            td("nm", r.nome) +
            td("",   r.desc) +
            td("",   lnk(r.lbl, r.url))
        );
    }
    h += "</table><hr>\n";

    /* licenza */
    h += "<h2>&#128220;&nbsp; Licenza &mdash; MIT</h2>\n"
         "<pre class=\"pre\">"
         "Copyright (c) 2024-2026 Paolo Lo Bello\n\n"
         "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
         "of this software and associated documentation files (the &quot;Software&quot;), to deal\n"
         "in the Software without restriction, including without limitation the rights\n"
         "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
         "copies of the Software, and to permit persons to whom the Software is\n"
         "furnished to do so, subject to the following conditions:\n\n"
         "The above copyright notice and this permission notice shall be included in all\n"
         "copies or substantial portions of the Software.\n\n"
         "THE SOFTWARE IS PROVIDED &quot;AS IS&quot;, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
         "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
         "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
         "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
         "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
         "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
         "SOFTWARE.</pre>\n";

    /* motto */
    h += "<p class=\"mtt\">&#127866;&nbsp; &ldquo;La birra &egrave; conoscenza divina "
         "&mdash; ogni sorso un dato in pi&ugrave;.&rdquo;</p>\n"
         "</body></html>\n";

        browser->setHtml(h);
    };

    updateHtml();

    connect(ThemeManager::instance(), &ThemeManager::changed,
            browser, [updateHtml](const QString&) { updateHtml(); });

    return browser;
}

/* ══════════════════════════════════════════════════════════════
   buildMcpTab — configurazione Model Context Protocol
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildMcpTab()
{
    const QString claudeCfgPath =
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
        + "/.claude/settings.json";

    /* helpers read/write settings.json */
    auto readCfg = [claudeCfgPath]() -> QJsonObject {
        QFile f(claudeCfgPath);
        if (!f.open(QIODevice::ReadOnly)) return {};
        return QJsonDocument::fromJson(f.readAll()).object();
    };
    auto writeCfg = [claudeCfgPath](const QJsonObject& obj) {
        QFileInfo fi(claudeCfgPath);
        QDir().mkpath(fi.absolutePath());
        QFile f(claudeCfgPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(QJsonDocument(obj).toJson());
    };

    auto* page = new QWidget;
    auto* vlay = new QVBoxLayout(page);
    vlay->setContentsMargins(16, 16, 16, 16);
    vlay->setSpacing(14);

    /* ── 1. MCP configurati in Claude Code ─────────────────────── */
    {
        auto* box  = new QGroupBox(
            "\xf0\x9f\x93\x8b  MCP configurati in Claude Code  (~/.claude/settings.json)");
        auto* blay = new QVBoxLayout(box);

        auto* table = new QTableWidget(0, 3);
        table->setHorizontalHeaderLabels({"Nome", "Tipo", "Comando / Args"});
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->setMaximumHeight(180);

        auto refreshTable = [table, readCfg]() {
            table->setRowCount(0);
            const QJsonObject mcp =
                readCfg().value("mcpServers").toObject();
            for (const QString& name : mcp.keys()) {
                const QJsonObject srv = mcp.value(name).toObject();
                const int r = table->rowCount();
                table->insertRow(r);
                table->setItem(r, 0, new QTableWidgetItem(name));
                table->setItem(r, 1, new QTableWidgetItem(
                    srv.value("type").toString("stdio")));
                QString cmd = srv.value("command").toString();
                for (const auto& a : srv.value("args").toArray())
                    cmd += " " + a.toString();
                table->setItem(r, 2, new QTableWidgetItem(cmd));
            }
        };
        refreshTable();

        auto* btnRow = new QWidget;
        auto* btnLay = new QHBoxLayout(btnRow);
        btnLay->setContentsMargins(0, 0, 0, 0);

        auto* btnRimuovi  = new QPushButton("\xe2\x9d\x8c  Rimuovi selezionato");
        auto* btnApri     = new QPushButton("\xf0\x9f\x93\x82  Apri settings.json");
        auto* btnRefresh  = new QPushButton("\xf0\x9f\x94\x84  Aggiorna");
        btnRimuovi->setObjectName("actionBtn");
        btnApri->setObjectName("actionBtn");
        btnRefresh->setObjectName("actionBtn");
        btnLay->addWidget(btnRimuovi);
        btnLay->addWidget(btnApri);
        btnLay->addStretch();
        btnLay->addWidget(btnRefresh);

        connect(btnRefresh, &QPushButton::clicked, page,
                [refreshTable]{ refreshTable(); });
        connect(btnApri, &QPushButton::clicked, page, [claudeCfgPath]{
            QProcess::startDetached("xdg-open", {claudeCfgPath});
        });
        connect(btnRimuovi, &QPushButton::clicked, page,
                [table, readCfg, writeCfg, refreshTable]{
            const int r = table->currentRow();
            if (r < 0) return;
            const QString nome = table->item(r, 0)->text();
            QJsonObject cfg = readCfg();
            QJsonObject mcp = cfg.value("mcpServers").toObject();
            mcp.remove(nome);
            cfg["mcpServers"] = mcp;
            writeCfg(cfg);
            refreshTable();
        });

        blay->addWidget(table);
        blay->addWidget(btnRow);
        vlay->addWidget(box);
    }

    /* ── 2. MCP Locali Prismalux ────────────────────────────────── */
    {
        auto* box  = new QGroupBox(
            "\xf0\x9f\x8f\xa0  MCP Locali Prismalux  (pronti da usare)");
        auto* blay = new QVBoxLayout(box);

        struct McpLocal {
            QString name, desc, scriptRel, cfgType, cfgCmd;
            QStringList cfgArgs;
            int port;   /* 0 = stdio puro, >0 = HTTP */
        };
        const QString root = P::root();
        const QList<McpLocal> cards = {
            { "Blender Bridge",
              "Controllo Blender tramite HTTP porta 6789. Avvia prima l'addon in Blender.",
              "MCPs/blender_addon/prismalux_bridge.py",
              "sse", "python3",
              { root + "/MCPs/blender_addon/prismalux_bridge.py" },
              6789 },
            { "Office Bridge",
              "Controllo LibreOffice/MS Office via UNO bridge (stdio).",
              "MCPs/office_bridge/prismalux_office_bridge.py",
              "stdio", "python3",
              { root + "/MCPs/office_bridge/prismalux_office_bridge.py" },
              0 },
            { "OpenCode MCP",
              "Bridge Claude Code \xe2\x86\x94 Prismalux stdio JSON-RPC. "
              "Gi\xc3\xa0 integrato in APP Controller.",
              "MCPs/opencode_mcp/server.py",
              "stdio", "python3",
              { root + "/MCPs/opencode_mcp/server.py" },
              0 },
        };

        for (const McpLocal& c : cards) {
            auto* card = new QFrame;
            card->setFrameShape(QFrame::StyledPanel);
            auto* cLay = new QHBoxLayout(card);

            auto* info  = new QVBoxLayout;
            auto* title = new QLabel("<b>" + c.name + "</b>");
            auto* desc  = new QLabel(c.desc);
            desc->setWordWrap(true);
            desc->setStyleSheet("color: gray; font-size: 11px;");
            info->addWidget(title);
            info->addWidget(desc);
            cLay->addLayout(info, 1);

            auto* btns = new QVBoxLayout;
            btns->setSpacing(4);

            auto* btnAgg = new QPushButton(
                "\xe2\x9e\x95  Aggiungi a Claude");
            btnAgg->setObjectName("actionBtn");
            btnAgg->setFixedWidth(165);
            btns->addWidget(btnAgg);

            /* QProcess per MCP HTTP (avviabile da Prismalux) */
            QProcess* proc = nullptr;
            if (c.port != 0) {
                proc = new QProcess(card);
                auto* btnStart = new QPushButton(
                    "\xe2\x96\xb6  Avvia server");
                btnStart->setObjectName("actionBtn");
                btnStart->setFixedWidth(165);
                btns->addWidget(btnStart);

                const QString scriptPath = root + "/" + c.scriptRel;
                connect(btnStart, &QPushButton::clicked, card,
                        [proc, btnStart, scriptPath]{
                    if (proc->state() == QProcess::NotRunning) {
                        proc->start("python3", {scriptPath});
                        btnStart->setText("\xe2\x96\xa0  Ferma server");
                    } else {
                        proc->terminate();
                        btnStart->setText("\xe2\x96\xb6  Avvia server");
                    }
                });
                connect(proc, &QProcess::stateChanged, card,
                        [btnStart](QProcess::ProcessState st){
                    if (st == QProcess::NotRunning)
                        btnStart->setText("\xe2\x96\xb6  Avvia server");
                });
            }

            McpLocal cap = c;
            connect(btnAgg, &QPushButton::clicked, page,
                    [cap, readCfg, writeCfg]{
                QJsonObject cfg = readCfg();
                QJsonObject mcp = cfg.value("mcpServers").toObject();
                QJsonObject srv;
                srv["type"]    = cap.cfgType;
                srv["command"] = cap.cfgCmd;
                QJsonArray arr;
                for (const QString& a : cap.cfgArgs) arr.append(a);
                srv["args"] = arr;
                mcp[cap.name] = srv;
                cfg["mcpServers"] = mcp;
                writeCfg(cfg);
            });

            cLay->addLayout(btns);
            blay->addWidget(card);
        }
        vlay->addWidget(box);
    }

    /* ── 2b. Ollama in locale ──────────────────────────────────── */
    {
        auto* box  = new QGroupBox(
            "\xf0\x9f\xa6\x99  Ollama in locale \xe2\x80\x94 usa i modelli locali in Claude Code");
        auto* blay = new QVBoxLayout(box);

        auto* desc = new QLabel(
            "Aggiunge un server MCP che espone i modelli Ollama locali come strumenti "
            "disponibili in Claude Code. Richiede <code>npm</code> (Node.js).");
        desc->setWordWrap(true);
        desc->setTextFormat(Qt::RichText);
        blay->addWidget(desc);

        auto* cmdLbl = new QLabel(
            "<b>Comando MCP:</b><br>"
            "<code>npx -y ollama-mcp-server</code>"
            "<br><span style='color:gray;font-size:11px;'>"
            "Host Ollama: http://localhost:11434 (predefinito)</span>");
        cmdLbl->setTextFormat(Qt::RichText);
        cmdLbl->setWordWrap(true);
        blay->addWidget(cmdLbl);

        auto* btnRow = new QWidget;
        auto* btnL   = new QHBoxLayout(btnRow);
        btnL->setContentsMargins(0,0,0,0); btnL->setSpacing(8);

        auto* btnAggOllama = new QPushButton("\xe2\x9e\x95  Aggiungi a Claude");
        btnAggOllama->setObjectName("actionBtn");
        auto* btnCopia = new QPushButton("\xf0\x9f\x93\x8b  Copia comando");
        btnCopia->setObjectName("actionBtn");
        auto* fbkOllama = new QLabel;

        btnL->addWidget(btnAggOllama);
        btnL->addWidget(btnCopia);
        btnL->addWidget(fbkOllama, 1);
        blay->addWidget(btnRow);

        connect(btnAggOllama, &QPushButton::clicked, page,
                [readCfg, writeCfg, fbkOllama]{
            QJsonObject cfg = readCfg();
            QJsonObject mcp = cfg.value("mcpServers").toObject();
            QJsonObject srv;
            srv["type"]    = QString("stdio");
            srv["command"] = QString("npx");
            QJsonArray args;
            args.append("-y");
            args.append("ollama-mcp-server");
            srv["args"] = args;
            mcp["ollama-locale"] = srv;
            cfg["mcpServers"] = mcp;
            writeCfg(cfg);
            fbkOllama->setStyleSheet("color: green;");
            fbkOllama->setText("\xe2\x9c\x85  Aggiunto a settings.json");
        });
        connect(btnCopia, &QPushButton::clicked, page, []{
            QGuiApplication::clipboard()->setText("npx -y ollama-mcp-server");
        });

        vlay->addWidget(box);
    }

    /* ── 3. Aggiungi MCP personalizzato ─────────────────────────── */
    {
        auto* box  = new QGroupBox(
            "\xe2\x9e\x95  Aggiungi MCP personalizzato a Claude Code");
        auto* form = new QFormLayout(box);
        form->setSpacing(8);

        auto* editNome = new QLineEdit;
        editNome->setPlaceholderText("es. my-mcp");
        auto* cmbTipo = new QComboBox;
        cmbTipo->addItems({"stdio", "sse", "http"});
        auto* editCmd = new QLineEdit;
        editCmd->setPlaceholderText("es. python3   oppure   npx");
        auto* editArgs = new QLineEdit;
        editArgs->setPlaceholderText(
            "es. /percorso/server.py   (separati da spazio)");
        auto* editEnv = new QLineEdit;
        editEnv->setPlaceholderText(
            "es. API_KEY=abc TOKEN=xyz   (opzionale)");

        form->addRow("Nome:", editNome);
        form->addRow("Tipo:", cmbTipo);
        form->addRow("Comando:", editCmd);
        form->addRow("Args:", editArgs);
        form->addRow("Env:", editEnv);

        auto* btnSalva = new QPushButton(
            "\xf0\x9f\x92\xbe  Salva in settings.json");
        btnSalva->setObjectName("actionBtn");
        auto* fbk = new QLabel;

        form->addRow("", btnSalva);
        form->addRow("", fbk);

        connect(btnSalva, &QPushButton::clicked, page,
                [editNome, cmbTipo, editCmd, editArgs, editEnv,
                 fbk, readCfg, writeCfg]{
            const QString nome = editNome->text().trimmed();
            const QString cmd  = editCmd->text().trimmed();
            if (nome.isEmpty() || cmd.isEmpty()) {
                fbk->setStyleSheet("color: red;");
                fbk->setText("Nome e Comando sono obbligatori.");
                return;
            }
            QJsonObject cfg = readCfg();
            QJsonObject mcp = cfg.value("mcpServers").toObject();
            QJsonObject srv;
            srv["type"]    = cmbTipo->currentText();
            srv["command"] = cmd;
            QJsonArray arr;
            for (const QString& a :
                 editArgs->text().split(' ', Qt::SkipEmptyParts))
                arr.append(a);
            srv["args"] = arr;
            QJsonObject env;
            for (const QString& kv :
                 editEnv->text().split(' ', Qt::SkipEmptyParts)) {
                const int idx = kv.indexOf('=');
                if (idx > 0) env[kv.left(idx)] = kv.mid(idx + 1);
            }
            if (!env.isEmpty()) srv["env"] = env;
            mcp[nome] = srv;
            cfg["mcpServers"] = mcp;
            writeCfg(cfg);
            fbk->setStyleSheet("color: green;");
            fbk->setText("\xe2\x9c\x85  \"" + nome +
                "\" aggiunto a settings.json");
            editNome->clear();
            editCmd->clear();
            editArgs->clear();
            editEnv->clear();
        });

        vlay->addWidget(box);
    }

    /* ── 4. MCP Popolari (hub ufficiale) ────────────────────────── */
    {
        auto* box  = new QGroupBox(
            "\xf0\x9f\x8c\x90  MCP Popolari  (copia il comando e installalo nel terminale)");
        auto* blay = new QVBoxLayout(box);

        auto* intro = new QLabel(
            "Premi \xf0\x9f\x93\x8b per copiare il comando npx/uvx negli appunti. "
            "Poi aggiungilo con la sezione \"Aggiungi MCP personalizzato\" sopra.");
        intro->setWordWrap(true);
        intro->setStyleSheet("color: gray; font-size: 11px;");
        blay->addWidget(intro);

        struct PopMcp { QString name, desc, installCmd; };
        const QList<PopMcp> pop = {
            { "filesystem",
              "Accesso al filesystem locale",
              "npx @modelcontextprotocol/server-filesystem" },
            { "brave-search",
              "Ricerca web tramite Brave Search API",
              "npx @modelcontextprotocol/server-brave-search" },
            { "github",
              "GitHub API (repo, PR, issue, code search)",
              "npx @modelcontextprotocol/server-github" },
            { "sqlite",
              "Query e gestione database SQLite",
              "npx @modelcontextprotocol/server-sqlite" },
            { "puppeteer",
              "Browser automation headless (Chromium)",
              "npx @modelcontextprotocol/server-puppeteer" },
            { "memory",
              "Memoria KV persistente per Claude",
              "npx @modelcontextprotocol/server-memory" },
            { "fetch",
              "Fetch URL / HTTP generico da Claude",
              "npx @modelcontextprotocol/server-fetch" },
            { "sequential-thinking",
              "Ragionamento strutturato a step",
              "npx @modelcontextprotocol/server-sequentialthinking" },
            { "postgres",
              "Query PostgreSQL dirette da Claude",
              "npx @modelcontextprotocol/server-postgres" },
            { "slack",
              "Lettura e invio messaggi Slack",
              "npx @modelcontextprotocol/server-slack" },
            { "godot-mcp",
              "Controllo motore Godot: scene, script GDScript, anteprima gioco",
              "npx -y godot-mcp" },
            { "ollama-mcp",
              "Usa modelli Ollama locali come strumenti in Claude Code",
              "npx -y ollama-mcp-server" },
        };

        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(4);
        int row = 0;
        for (const PopMcp& p : pop) {
            auto* lbl = new QLabel(
                "<b>" + p.name + "</b>  \xe2\x80\x94  " + p.desc);
            lbl->setTextFormat(Qt::RichText);
            auto* btnCopy = new QPushButton("\xf0\x9f\x93\x8b");
            btnCopy->setToolTip("Copia: " + p.installCmd);
            btnCopy->setFixedSize(28, 28);
            PopMcp cap = p;
            connect(btnCopy, &QPushButton::clicked, page, [cap]{
                QGuiApplication::clipboard()->setText(cap.installCmd);
            });
            grid->addWidget(lbl,     row, 0);
            grid->addWidget(btnCopy, row, 1);
            ++row;
        }
        blay->addLayout(grid);
        vlay->addWidget(box);
    }

    vlay->addStretch();

    auto* sc = new QScrollArea;
    sc->setWidgetResizable(true);
    sc->setFrameShape(QFrame::NoFrame);
    sc->setWidget(page);
    return sc;
}

/* ══════════════════════════════════════════════════════════════
   buildSandboxTab — Docker sandbox per esecuzione codice AI
   ══════════════════════════════════════════════════════════════ */


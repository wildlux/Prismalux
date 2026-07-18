/* main_ai_panels.cpp — buildToolsPanel, buildBottomBar, bubble/log recolor */
#include "main_ai.h"
#include "main_ai_p.h"
#include "../dpi_utils.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
namespace P = PrismaluxPaths;
#include "../app_config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QCheckBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QPushButton>
#include <QToolButton>
#include <QRegularExpression>
#include <QTextDocument>
#include <QTextCursor>
#include <QColorDialog>
#include <QLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QMessageBox>
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
        { "algoritmo",    "\xf0\x9f\xa7\xae", "Algoritmi classici",
          "MCD/MCM, fattorizzazione, Fibonacci, Catalan, Collatz, Hanoi (+ passo-passo), "
          "N-Regine (backtracking reale), profitto azioni, inversioni, ricerca lineare, "
          "edit distance/LCS, ricerca pattern (KMP)" },
        { "codice_fiscale","\xf0\x9f\x86\x94", "Codice Fiscale",
          "Calcola il Codice Fiscale (D.M.1976) da cognome/nome/nascita/sesso/comune" },
        { "finanza_calcola","\xf0\x9f\x92\xb0", "Calcoli finanziari",
          "Interesse composto, rata mutuo (ammortamento francese), rivalutazione TFR" },
        { "valida_documento","\xf0\x9f\x92\xb3", "Valida documenti",
          "IBAN (mod-97), Partita IVA (checksum), Codice Fiscale (D.M.1976 + decodifica)" },
        { "carta_astrale", "\xe2\x99\x8b", "Carta astrale",
          "Posizioni planetarie (Meeus) da data/ora/luogo di nascita" },
        { "converti",      "\xf0\x9f\x94\x81", "Conversioni",
          "Scienza/cucina: Ohm, velocità, temperatura forno, ingredienti, distanze astronomiche" },
        { "disegna_grafico","\xf0\x9f\x93\x88", "Disegna grafico",
          "Traccia y=f(x) e apre il pannello Grafico nella chat" },
    };
    constexpr int kNTools = 17;

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

        auto* btnAll  = new QPushButton(tr("\xe2\x9c\x85  Tutti"),   hdrRow);
        auto* btnNone = new QPushButton(tr("\xe2\x96\xa1  Nessuno"), hdrRow);
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

        /* "carta_astrale" nascosto finché non sbloccato — stesso flag
           dell'easter egg del sotto-tab Ricerca → Carta Astrale (doppio
           click su "conoscenza" in Impostazioni → Ringraziamenti), per
           coerenza: se la tab non è visibile, il tool non deve esserlo
           né essere invocabile dall'AI. */
        const bool astraleUnlocked = QSettings("Prismalux", "GUI")
            .value(P::SK::kAstraleUnlocked, false).toBool();

        int pos = 0;
        for (int i = 0; i < kNTools; ++i) {
            const QString name = QString::fromLatin1(kTools[i].name);
            if (name == "carta_astrale" && !astraleUnlocked) continue;

            const QString icon  = QString::fromUtf8(kTools[i].icon);
            const QString label = QString::fromUtf8(kTools[i].label);
            const QString desc  = QString::fromUtf8(kTools[i].desc);

            auto* chk = new QCheckBox(icon + "  " + label + "  (" + name + ")", grid);
            chk->setToolTip(desc);
            chk->setChecked(true);
            chk->setMinimumHeight(dpiScale(22));
            m_enabledTools.insert(name);
            gl->addWidget(chk, pos / 2, pos % 2);
            ++pos;

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
        fastSearch->setPlaceholderText(tr("\xf0\x9f\x94\x8d  Cerca tool per nome o descrizione\xe2\x80\xa6"));
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

        auto* btnAllMcp  = new QPushButton(tr("\xe2\x9c\x85  Tutti"),   hdrRow);
        auto* btnNoneMcp = new QPushButton(tr("\xe2\x96\xa1  Nessuno"), hdrRow);
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
        mcpSearch->setPlaceholderText(tr("\xf0\x9f\x94\x8d  Cerca MCP per nome o etichetta\xe2\x80\xa6"));
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
                tr("<span style='color:#64748b;'>Nessun MCP in MCPs/</span>"), grid),
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
                chk->setToolTip(tr("MCPs/") + name + "/server.py\n"
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

    /* Memoria persistente attiva di DEFAULT (ancoraggio anti-allucinazione:
       inietta nel prompt i nodi pertinenti già memorizzati). Resta spenta
       solo se l'utente l'ha disattivata esplicitamente.
       setChecked(true) emette toggled → onHermesToggled. */
    if (m_hermesToggleBar &&
            QSettings("Prismalux", "GUI")
                .value(P::SK::kHermesEnabled, true).toBool())
        m_hermesToggleBar->setChecked(true);

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
        /* Avvia la discovery MCP on-demand (idempotente, no-op se già fatta
         * o già in corso) — l'utente ha appena mostrato intenzione di usare
         * i tool MCP. */
        startMcpDiscovery();
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
        auto* fgLbl = new QLabel(tr("Colore"), fgTile);
        fgLbl->setAlignment(Qt::AlignCenter);
        fgLbl->setStyleSheet("font-size:9px;color:palette(mid);");
        ftl->addWidget(fgLbl);
        m_btnFmtFg = new QPushButton("A", fgTile);
        m_btnFmtFg->setFixedWidth(fgLbl->fontMetrics().horizontalAdvance("Colore") + 14);
        m_btnFmtFg->setStyleSheet(makeFgStyle(m_fmtFgColor));
        m_btnFmtFg->setToolTip(tr("Colore testo (apre selettore colore)"));
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
        auto* bgLbl = new QLabel(tr("Sfondo"), bgTile);
        bgLbl->setAlignment(Qt::AlignCenter);
        bgLbl->setStyleSheet("font-size:9px;color:palette(mid);");
        btl->addWidget(bgLbl);
        m_btnFmtBg = new QPushButton("A", bgTile);
        m_btnFmtBg->setFixedWidth(bgLbl->fontMetrics().horizontalAdvance("Sfondo") + 14);
        m_btnFmtBg->setStyleSheet(makeBgStyle(m_fmtBgColor));
        m_btnFmtBg->setToolTip(tr("Colore sfondo testo (apre selettore colore)"));
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
        auto* tblLbl = new QLabel(tr("Tabella"), tblTile);
        tblLbl->setAlignment(Qt::AlignCenter);
        tblLbl->setStyleSheet("font-size:9px;color:palette(mid);");
        ttl->addWidget(tblLbl);
        btnTbl = new QPushButton("\xe2\x8a\x9e", tblTile);
        btnTbl->setFixedWidth(tblLbl->fontMetrics().horizontalAdvance("Tabella") + 14);
        btnTbl->setStyleSheet(
            "QPushButton{background:transparent;border:none;border-radius:3px;font-size:13px;}"
            "QPushButton:hover{background:palette(highlight);color:palette(highlighted-text);}");
        btnTbl->setToolTip(tr("Inserisci tabella — scegli dimensioni con la griglia"));
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
                    lbl->setText(tr("<span style='font-size:9px;'>") +
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

    connect(btnTbl, &QPushButton::clicked, this, &AgentiPage::onBtnTblClicked);

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


void AgentiPage::onBtnTblClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    auto* picker = new TablePickerPopup(this, [this](int rows, int cols) {
        QString tbl = "\n|";
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
    picker->move(btn->mapToGlobal(QPoint(0, btn->height() + 2)));
    picker->show();
}

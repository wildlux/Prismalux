#include "mcpaddons_page.h"
#include <QHBoxLayout>
#include <QFrame>
#include <QFont>
#include <QInputDialog>
#include <QLineEdit>
#include <QScroller>
#include <QScrollerProperties>

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
McpAddonsPage::McpAddonsPage(QWidget* parent) : QWidget(parent)
{
    setObjectName("McpAddonsPage");
    QSettings s("Prismalux", "Mobile");

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScroller::grabGesture(scrollArea->viewport(), QScroller::TouchGesture);
    QScroller* qs = QScroller::scroller(scrollArea->viewport());
    QScrollerProperties sp = qs->scrollerProperties();
    sp.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor, 0.0);
    sp.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.0);
    sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    qs->setScrollerProperties(sp);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);
    scrollArea->setWidget(inner);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scrollArea);

    /* Titolo */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\x8c  MCP Add-ons"), inner);  /* 🔌 */
    QFont tf = title->font();
    tf.setPointSize(15); tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    auto* descLbl = new QLabel(
        "Abilita o disabilita i server MCP (Model Context Protocol)\n"
        "che Prismalux usa per estendere le capacità dell'AI.",
        inner);
    descLbl->setWordWrap(true);
    descLbl->setAlignment(Qt::AlignCenter);
    descLbl->setStyleSheet("color:#8890a8; font-size:13px;");
    vbox->addWidget(descLbl);

    /* ── Lista MCP predefiniti ── */
    auto* addonsGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x94\xa7  MCP Disponibili"), inner);  /* 🔧 */
    addonsGroup->setObjectName("SettingsGroup");
    m_dynLayout = new QVBoxLayout(addonsGroup);
    m_dynLayout->setSpacing(6);

    /* Definizione degli MCP noti */
    struct McpDef { const char* name; const char* desc; const char* url; };
    static const McpDef kDefs[] = {
        { "Filesystem MCP",
          "Accesso ai file locali del dispositivo (lettura/scrittura sicura)",
          "mcp://localhost/filesystem" },
        { "Ollama MCP",
          "Gestione modelli Ollama: lista, pull, delete tramite AI",
          "mcp://localhost:11434/ollama" },
        { "SQLite MCP",
          "Query su database SQLite locali via linguaggio naturale",
          "mcp://localhost/sqlite" },
        { "Web Search MCP",
          "Ricerca web via DuckDuckGo integrata nella chat",
          "mcp://localhost/websearch" },
        { "GitHub MCP",
          "Accesso a repository GitHub: issues, PR, codice",
          "mcp://localhost/github" },
        { "Graphviz MCP",
          "Generazione di grafi e diagrammi da testo",
          "mcp://localhost/graphviz" },
        { "GNS3 MCP",
          "Controllo simulatore di reti GNS3 dalla chat",
          "mcp://localhost:3080/gns3" },
        { "Godot MCP",
          "Integrazione con l'editor Godot per automatizzare scene e script",
          "mcp://localhost/godot" },
    };

    for (const auto& def : kDefs) {
        McpEntry entry;
        entry.name = QString::fromUtf8(def.name);
        entry.desc = QString::fromUtf8(def.desc);
        entry.url  = QString::fromUtf8(def.url);

        auto* row = new QWidget(addonsGroup);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(4, 4, 4, 4);
        rowLay->setSpacing(10);

        entry.chk = new QCheckBox(row);
        const bool enabled = s.value("mcp/enabled/" + entry.name, false).toBool();
        entry.chk->setChecked(enabled);
        entry.chk->setFixedSize(28, 28);

        auto* infoCol = new QVBoxLayout;
        auto* nameLbl = new QLabel(entry.name, row);
        QFont nf = nameLbl->font();
        nf.setBold(true); nf.setPointSize(12);
        nameLbl->setFont(nf);
        auto* descLbl2 = new QLabel(entry.desc, row);
        descLbl2->setWordWrap(true);
        descLbl2->setStyleSheet("color:#8890a8; font-size:11px;");
        infoCol->addWidget(nameLbl);
        infoCol->addWidget(descLbl2);

        rowLay->addWidget(entry.chk);
        rowLay->addLayout(infoCol, 1);

        connect(entry.chk, &QCheckBox::toggled, this, &McpAddonsPage::onToggled);
        m_dynLayout->addWidget(row);

        /* Separatore sottile */
        auto* sep = new QFrame(addonsGroup);
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        m_dynLayout->addWidget(sep);

        m_entries.append(entry);
    }

    vbox->addWidget(addonsGroup);

    /* ── Pulsante aggiungi MCP custom ── */
    auto* customGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x9b\xa0\xef\xb8\x8f  MCP Personalizzato"), inner);  /* 🛠️ */
    customGroup->setObjectName("SettingsGroup");
    auto* customVbox = new QVBoxLayout(customGroup);
    auto* customDesc = new QLabel(
        "Aggiungi un server MCP personalizzato specificando nome e URL.",
        customGroup);
    customDesc->setWordWrap(true);
    customDesc->setStyleSheet("color:#8890a8; font-size:12px;");
    customVbox->addWidget(customDesc);
    auto* addBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9e\x95  Aggiungi MCP..."), customGroup);  /* ➕ */
    addBtn->setObjectName("SecondaryBtn");
    addBtn->setMinimumHeight(44);
    customVbox->addWidget(addBtn);
    vbox->addWidget(customGroup);

    /* ── Info ── */
    auto* infoGroup = new QGroupBox(
        QString::fromUtf8("\xe2\x84\xb9\xef\xb8\x8f  Informazioni MCP"), inner);  /* ℹ️ */
    infoGroup->setObjectName("SettingsGroup");
    auto* infoVbox = new QVBoxLayout(infoGroup);
    auto* infoTxt = new QLabel(infoGroup);
    infoTxt->setTextFormat(Qt::RichText);
    infoTxt->setWordWrap(true);
    infoTxt->setText(
        "<b>MCP (Model Context Protocol)</b> è il protocollo sviluppato da Anthropic "
        "per connettere i modelli AI a strumenti e fonti di dati esterne.<br><br>"
        "\xf0\x9f\x94\x8c <b>Come funziona:</b><br>"  /* 🔌 */
        "Il server MCP viene avviato sul PC o sul telefono, poi "
        "Prismalux vi si connette via rete locale per ampliare le "
        "capacità della chat AI.<br><br>"
        "\xf0\x9f\x92\xa1 <b>Suggerimento:</b> i server MCP su PC sono accessibili "
        "via Wi-Fi usando l'IP del PC configurato nelle Impostazioni.");
    infoVbox->addWidget(infoTxt);
    vbox->addWidget(infoGroup);

    vbox->addStretch();

    connect(addBtn, &QPushButton::clicked, this, &McpAddonsPage::onAddCustom);
}

/* ── onToggled — salva preferenza ────────────────────────────── */
void McpAddonsPage::onToggled()
{
    QSettings s("Prismalux", "Mobile");
    for (const auto& entry : m_entries) {
        if (entry.chk)
            s.setValue("mcp/enabled/" + entry.name, entry.chk->isChecked());
    }
}

/* ── onAddCustom — aggiunge MCP custom ───────────────────────── */
void McpAddonsPage::onAddCustom()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, "Aggiungi MCP", "Nome del server MCP:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    const QString url = QInputDialog::getText(
        this, "Aggiungi MCP", "URL del server (es. mcp://192.168.1.165/mytool):",
        QLineEdit::Normal, "mcp://", &ok);
    if (!ok || url.trimmed().isEmpty()) return;

    QSettings s("Prismalux", "Mobile");

    McpEntry entry;
    entry.name = name.trimmed();
    entry.url  = url.trimmed();
    entry.desc = "Server MCP personalizzato";

    auto* row = new QWidget;
    auto* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(4, 4, 4, 4);
    entry.chk = new QCheckBox(row);
    entry.chk->setChecked(true);
    entry.chk->setFixedSize(28, 28);
    auto* infoCol = new QVBoxLayout;
    auto* nameLbl = new QLabel(entry.name, row);
    QFont nf = nameLbl->font(); nf.setBold(true); nameLbl->setFont(nf);
    auto* urlLbl = new QLabel(entry.url, row);
    urlLbl->setStyleSheet("color:#8890a8; font-size:11px;");
    infoCol->addWidget(nameLbl);
    infoCol->addWidget(urlLbl);
    rowLay->addWidget(entry.chk);
    rowLay->addLayout(infoCol, 1);

    m_dynLayout->addWidget(row);
    s.setValue("mcp/enabled/" + entry.name, true);
    s.setValue("mcp/url/"     + entry.name, entry.url);
    connect(entry.chk, &QCheckBox::toggled, this, &McpAddonsPage::onToggled);
    m_entries.append(entry);
}

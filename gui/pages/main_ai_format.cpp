#include "main_ai.h"
#include "main_ai_p.h"
#include <QRegularExpression>
#include <QMap>
#include <QPair>

/* ══════════════════════════════════════════════════════════════
   markdownToHtml — converte la risposta AI in HTML leggibile
   Gestisce: code fence con header (lingua+copia+salva),
             inline code, bold, italic, heading,
             liste puntate/numerate, tabelle, righe separatore.
   codeBlocks / codeCounter: se forniti, i blocchi vengono
   salvati per i gestori code:copy:N e code:save:N.
   ══════════════════════════════════════════════════════════════ */

/* ── Mappa linguaggio → {emoji, colore badge, estensione file} ── */
struct LangMeta { const char* icon; const char* color; const char* ext; };
static const QMap<QString, LangMeta>& langMap()
{
    static const QMap<QString, LangMeta> m = {
        {"python",     {"\xf0\x9f\x90\x8d", "#3572A5", "py"}},   /* 🐍 */
        {"py",         {"\xf0\x9f\x90\x8d", "#3572A5", "py"}},
        {"bash",       {"\xe2\x9a\xa1",      "#89e051", "sh"}},   /* ⚡ */
        {"sh",         {"\xe2\x9a\xa1",      "#89e051", "sh"}},
        {"shell",      {"\xe2\x9a\xa1",      "#89e051", "sh"}},
        {"zsh",        {"\xe2\x9a\xa1",      "#89e051", "sh"}},
        {"c",          {"\xe2\x9a\x99",      "#555555", "c"}},    /* ⚙ */
        {"cpp",        {"\xe2\x9a\x99",      "#f34b7d", "cpp"}},
        {"c++",        {"\xe2\x9a\x99",      "#f34b7d", "cpp"}},
        {"cxx",        {"\xe2\x9a\x99",      "#f34b7d", "cpp"}},
        {"h",          {"\xf0\x9f\x93\x8b",  "#f34b7d", "h"}},   /* 📋 */
        {"hpp",        {"\xf0\x9f\x93\x8b",  "#f34b7d", "hpp"}},
        {"java",       {"\xe2\x98\x95",      "#b07219", "java"}}, /* ☕ */
        {"javascript", {"\xf0\x9f\x9f\xa8",  "#f1e05a", "js"}},  /* 🟨 */
        {"js",         {"\xf0\x9f\x9f\xa8",  "#f1e05a", "js"}},
        {"typescript", {"\xf0\x9f\x94\xb7",  "#2b7489", "ts"}},  /* 🔷 */
        {"ts",         {"\xf0\x9f\x94\xb7",  "#2b7489", "ts"}},
        {"html",       {"\xf0\x9f\x8c\x90",  "#e34c26", "html"}},/* 🌐 */
        {"css",        {"\xf0\x9f\x8e\xa8",  "#563d7c", "css"}}, /* 🎨 */
        {"sql",        {"\xf0\x9f\x97\x84",  "#e38c00", "sql"}}, /* 🗄 */
        {"json",       {"\xf0\x9f\x93\xa6",  "#40d47e", "json"}},/* 📦 */
        {"yaml",       {"\xf0\x9f\x93\x84",  "#cb171e", "yaml"}},/* 📄 */
        {"yml",        {"\xf0\x9f\x93\x84",  "#cb171e", "yml"}},
        {"xml",        {"\xf0\x9f\x93\x83",  "#0060ac", "xml"}}, /* 📃 */
        {"rust",       {"\xf0\x9f\xa6\x80",  "#dea584", "rs"}},  /* 🦀 */
        {"go",         {"\xf0\x9f\x90\xb9",  "#00add8", "go"}},  /* 🐹 */
        {"ruby",       {"\xf0\x9f\x92\x8e",  "#701516", "rb"}},  /* 💎 */
        {"rb",         {"\xf0\x9f\x92\x8e",  "#701516", "rb"}},
        {"php",        {"\xf0\x9f\x90\x98",  "#4F5D95", "php"}}, /* 🐘 */
        {"swift",      {"\xf0\x9f\xa6\x85",  "#ffac45", "swift"}},/* 🦅 */
        {"kotlin",     {"\xf0\x9f\x8e\xaf",  "#F18E33", "kt"}},  /* 🎯 */
        {"r",          {"\xf0\x9f\x93\x8a",  "#198CE7", "r"}},   /* 📊 */
        {"matlab",     {"\xf0\x9f\x93\x90",  "#e16737", "m"}},   /* 📐 */
        {"dart",       {"\xf0\x9f\x8e\xaf",  "#00B4AB", "dart"}},
        {"cmake",      {"\xf0\x9f\x94\xa7",  "#064F8C", "cmake"}},/* 🔧 */
        {"makefile",   {"\xf0\x9f\x94\xa7",  "#427819", "mk"}},
        {"dockerfile", {"\xf0\x9f\x90\xb3",  "#384d54", "Dockerfile"}},/* 🐳 */
        {"markdown",   {"\xf0\x9f\x93\x9d",  "#083fa1", "md"}},  /* 📝 */
        {"md",         {"\xf0\x9f\x93\x9d",  "#083fa1", "md"}},
        {"lua",        {"\xf0\x9f\x8c\x99",  "#000080", "lua"}}, /* 🌙 */
        {"perl",       {"\xf0\x9f\xaa\xb2",  "#0298c3", "pl"}},  /* 🪲 */
        {"scala",      {"\xf0\x9f\x94\xb4",  "#c22d40", "scala"}},/* 🔴 */
        {"haskell",    {"\xce\xbb",           "#5e5086", "hs"}},  /* λ */
        {"toml",       {"\xf0\x9f\x93\x84",  "#9c4221", "toml"}},
        {"ini",        {"\xf0\x9f\x93\x84",  "#888888", "ini"}},
        {"text",       {"\xf0\x9f\x93\x84",  "#888888", "txt"}},
        {"txt",        {"\xf0\x9f\x93\x84",  "#888888", "txt"}},
    };
    return m;
}

/* Costruisce l'HTML della barra header del blocco codice */
static QString codeHeader(const QString& lang, int blockId)
{
    const auto& lm = langMap();
    const auto it  = lm.constFind(lang.toLower());

    QString icon  = "\xf0\x9f\x93\x84"; /* 📄 default */
    QString color = "#8b949e";
    if (it != lm.constEnd()) {
        icon  = QString::fromUtf8(it->icon);
        color = it->color;
    }
    const QString displayLang = lang.isEmpty() ? "text" : lang.toLower();
    const QString id          = QString::number(blockId);

    return
        "<table width='100%' cellpadding='0' cellspacing='0'"
        " style='background:#161b22;border:1px solid #30363d;"
        "border-radius:6px 6px 0 0;border-bottom:none;margin:8px 0 0 0;'>"
        "<tr>"
        "<td style='padding:5px 14px;'>"
          "<span style='color:" + color + ";font-size:11px;font-weight:bold;"
                "font-family:\"JetBrains Mono\",monospace;'>"
            + icon + " " + displayLang.toHtmlEscaped() +
          "</span>"
        "</td>"
        "<td align='right' style='padding:5px 14px;white-space:nowrap;'>"
          "<a href='code:copy:" + id + "'"
            " style='color:#58a6ff;font-size:11px;font-weight:bold;"
                   "text-decoration:none;'>"
            "\xf0\x9f\x93\x8b Copia"   /* 📋 */
          "</a>"
          "&nbsp;&nbsp;&nbsp;"
          "<a href='code:save:" + id + "'"
            " style='color:#58a6ff;font-size:11px;font-weight:bold;"
                   "text-decoration:none;'>"
            "\xf0\x9f\x92\xbe Salva"   /* 💾 */
          "</a>"
        "</td>"
        "</tr>"
        "</table>";
}

QString AgentiPage::markdownToHtml(const QString& md,
    QMap<int,QPair<QString,QString>>* codeBlocks,
    int* codeCounter)
{
    auto escHtml = [](const QString& s) {
        QString r = s;
        r.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
        return r;
    };

    auto inlineFmt = [&escHtml](QString s) -> QString {
        s = escHtml(s);
        /* Code span `text` — prima degli altri per proteggere il contenuto */
        static QRegularExpression cRe("`([^`]+)`");
        s.replace(cRe,
            "<code style='background:rgba(99,110,123,0.18);padding:1px 5px;"
            "border-radius:3px;font-family:\"JetBrains Mono\",monospace;"
            "font-size:0.88em;'>\\1</code>");
        /* Bold **text** */
        static QRegularExpression b2Re(R"(\*\*(.+?)\*\*)");
        s.replace(b2Re, "<b>\\1</b>");
        /* Bold __text__ */
        static QRegularExpression buRe(R"(__(.+?)__)");
        s.replace(buRe, "<b>\\1</b>");
        /* Italic *text* */
        static QRegularExpression i1Re(R"(\*([^\*\n]+?)\*)");
        s.replace(i1Re, "<i>\\1</i>");
        return s;
    };

    QString html;
    const QStringList lines = md.split('\n');
    bool    inCodeBlock   = false;
    bool    inOrderedList = false;
    bool    inTable       = false;
    bool    tableHdrDone  = false;
    bool    tableOddRow   = false;
    QString codeBuf;
    QString codeLang;

    auto closeList = [&](){
        if (inOrderedList) { html += "</ol>\n"; inOrderedList = false; }
    };

    auto isTableSep = [](const QString& line) -> bool {
        const QStringList cells = line.split('|', Qt::SkipEmptyParts);
        if (cells.isEmpty()) return false;
        for (const QString& c : cells) {
            const QString t = c.trimmed();
            if (t.isEmpty()) continue;
            for (QChar ch : t)
                if (ch != '-' && ch != ':' && ch != ' ') return false;
        }
        return true;
    };

    auto flushTable = [&](){
        if (!inTable) return;
        if (tableHdrDone) html += "</tbody>\n";
        html += "</table>\n";
        inTable = false; tableHdrDone = false; tableOddRow = false;
    };

    /* Emette un blocco codice con header barra lingua+azioni */
    auto flushCode = [&](const QString& buf, const QString& lang) {
        const QString esc = escHtml(buf).trimmed();
        if (esc.isEmpty()) return;

        /* Calcola ID per questo blocco */
        int blockId = 0;
        if (codeCounter) blockId = (*codeCounter)++;
        if (codeBlocks)  (*codeBlocks)[blockId] = { lang.toLower(), buf.trimmed() };

        /* Header barra solo se i gestori sono connessi (codeBlocks fornito) */
        if (codeBlocks)
            html += codeHeader(lang, blockId);

        /* Blocco <pre> */
        const QString radius = codeBlocks
            ? "0 0 6px 6px" : "6px";
        const QString marginTop = codeBlocks ? "0" : "8px";

        html += "<pre style='background:#0d1117;color:#e6edf3;"
                "border:1px solid #30363d;border-radius:" + radius + ";"
                "padding:10px 14px;margin:" + marginTop + " 0 8px 0;"
                "overflow-x:auto;"
                "font-family:\"JetBrains Mono\",\"Fira Code\",monospace;"
                "font-size:12px;line-height:1.5;white-space:pre;'>"
                + esc + "</pre>\n";
    };

    for (const QString& rawLine : lines) {
        /* ── Code fence ── */
        if (rawLine.startsWith("```")) {
            if (!inCodeBlock) {
                closeList();
                flushTable();
                inCodeBlock = true;
                codeBuf.clear();
                codeLang = rawLine.mid(3).trimmed(); /* es. "python", "bash" */
            } else {
                inCodeBlock = false;
                flushCode(codeBuf, codeLang);
                codeBuf.clear();
                codeLang.clear();
            }
            continue;
        }
        if (inCodeBlock) { codeBuf += rawLine + '\n'; continue; }

        const QString line = rawLine;

        /* ── Tabella Markdown (righe con |) ── */
        if (line.trimmed().startsWith('|')) {
            closeList();
            if (!inTable) {
                html += "<table style='border-collapse:collapse;width:100%;"
                        "margin:10px 0;font-size:13px;'>\n";
                inTable = true; tableHdrDone = false; tableOddRow = false;
            }
            if (isTableSep(line)) {
                tableHdrDone = true;
                html += "<tbody>\n";
                continue;
            }
            const QStringList cells = line.split('|', Qt::SkipEmptyParts);
            if (!tableHdrDone) {
                html += "<thead><tr>\n";
                for (const QString& cell : cells)
                    html += "<th style='border:1px solid #30363d;padding:7px 12px;"
                            "background:#1e293b;color:#79c0ff;font-weight:bold;"
                            "text-align:left;'>" + inlineFmt(cell.trimmed()) + "</th>\n";
                html += "</tr></thead>\n";
            } else {
                const QString bg = tableOddRow ? "#0d1117" : "#111827";
                tableOddRow = !tableOddRow;
                html += "<tr>\n";
                for (const QString& cell : cells)
                    html += "<td style='border:1px solid #30363d;padding:6px 12px;"
                            "color:#e2e8f0;background:" + bg + ";'>"
                            + inlineFmt(cell.trimmed()) + "</td>\n";
                html += "</tr>\n";
            }
            continue;
        }
        if (inTable) flushTable();

        /* ── Headings ── */
        if (line.startsWith("### ")) {
            closeList();
            html += "<h4 style='color:#58a6ff;margin:10px 0 4px;font-size:13px;'>"
                    + inlineFmt(line.mid(4)) + "</h4>\n";
            continue;
        }
        if (line.startsWith("## ")) {
            closeList();
            html += "<h3 style='color:#79c0ff;margin:12px 0 6px;font-size:15px;'>"
                    + inlineFmt(line.mid(3)) + "</h3>\n";
            continue;
        }
        if (line.startsWith("# ")) {
            closeList();
            html += "<h2 style='color:#79c0ff;margin:14px 0 8px;font-size:17px;font-weight:700;'>"
                    + inlineFmt(line.mid(2)) + "</h2>\n";
            continue;
        }

        /* ── Bullet list (-, *, +) ── */
        static QRegularExpression bulletRe("^[\\-\\*\\+]\\s+");
        QRegularExpressionMatch bm = bulletRe.match(line);
        if (bm.hasMatch()) {
            if (!inOrderedList) { html += "<ul style='margin:4px 0;padding-left:22px;'>"; inOrderedList = true; }
            html += "<li style='margin:2px 0;'>" + inlineFmt(line.mid(bm.capturedLength())) + "</li>";
            continue;
        }

        /* ── Numbered list ── */
        static QRegularExpression numRe("^\\d+[\\.):]\\s+");
        QRegularExpressionMatch nm = numRe.match(line);
        if (nm.hasMatch()) {
            if (!inOrderedList) { html += "<ol style='margin:4px 0;padding-left:22px;'>"; inOrderedList = true; }
            html += "<li style='margin:2px 0;'>" + inlineFmt(line.mid(nm.capturedLength())) + "</li>";
            continue;
        }

        closeList();

        /* ── Riga vuota ── */
        if (line.trimmed().isEmpty()) {
            html += "<div style='height:8px;'></div>\n";
            continue;
        }

        /* ── Separatore --- ── */
        if (line.trimmed() == "---" || line.trimmed() == "***") {
            html += "<hr style='border:none;border-top:1px solid #30363d;margin:10px 0;'>\n";
            continue;
        }

        /* ── Paragrafo normale ── */
        html += "<p style='margin:3px 0 7px 0;line-height:1.75;'>" + inlineFmt(line) + "</p>\n";
    }
    closeList();
    flushTable();

    /* Code block non chiuso (fallback) */
    if (inCodeBlock && !codeBuf.isEmpty())
        flushCode(codeBuf, codeLang);

    return html;
}

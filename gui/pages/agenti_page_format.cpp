#include "agenti_page.h"
#include "agenti_page_p.h"
#include <QRegularExpression>

/* ══════════════════════════════════════════════════════════════
   markdownToHtml — converte la risposta AI in HTML leggibile
   Gestisce: code fence, inline code, bold, italic, heading,
             liste puntate/numerate, righe separatore, paragrafi.
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::markdownToHtml(const QString& md)
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
    bool    inCodeBlock = false;
    bool    inOrderedList = false;
    QString codeBuf;

    auto closeList = [&](){
        if (inOrderedList) { html += "</ol>\n"; inOrderedList = false; }
    };

    for (const QString& rawLine : lines) {
        /* ── Code fence ── */
        if (rawLine.startsWith("```")) {
            if (!inCodeBlock) {
                closeList();
                inCodeBlock = true;
                codeBuf.clear();
            } else {
                inCodeBlock = false;
                QString esc = escHtml(codeBuf).trimmed();
                html += "<pre style='background:#0d1117;color:#e6edf3;"
                        "border:1px solid #30363d;border-radius:6px;"
                        "padding:10px 14px;margin:8px 0;overflow-x:auto;"
                        "font-family:\"JetBrains Mono\",\"Fira Code\",monospace;"
                        "font-size:12px;line-height:1.5;white-space:pre;'>"
                        + esc + "</pre>\n";
                codeBuf.clear();
            }
            continue;
        }
        if (inCodeBlock) { codeBuf += rawLine + '\n'; continue; }

        const QString line = rawLine;

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
            html += "<div style='height:6px;'></div>\n";
            continue;
        }

        /* ── Separatore --- ── */
        if (line.trimmed() == "---" || line.trimmed() == "***") {
            html += "<hr style='border:none;border-top:1px solid #30363d;margin:8px 0;'>\n";
            continue;
        }

        /* ── Paragrafo normale ── */
        html += "<p style='margin:1px 0;line-height:1.6;'>" + inlineFmt(line) + "</p>\n";
    }
    closeList();

    /* Code block non chiuso (fallback) */
    if (inCodeBlock && !codeBuf.isEmpty()) {
        html += "<pre style='background:#0d1117;color:#e6edf3;"
                "border:1px solid #30363d;border-radius:6px;"
                "padding:10px 14px;margin:8px 0;"
                "font-family:\"JetBrains Mono\",monospace;font-size:12px;'>"
                + escHtml(codeBuf).trimmed() + "</pre>\n";
    }

    return html;
}

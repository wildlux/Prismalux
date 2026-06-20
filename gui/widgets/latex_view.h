#pragma once
#ifdef HAVE_WEBENGINE_WIDGETS
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QString>
#include <QUrl>
#include <QDir>
#include <QCoreApplication>
#include <QSizePolicy>

/* LatexView — QWebEngineView con KaTeX per rendering formule LaTeX.
   Cerca KaTeX in: assets/katex/ (bundle) → /usr/share/javascript/katex/ (Linux).
   Fallback a QTextEdit se HAVE_WEBENGINE_WIDGETS non è definito. */

static inline QString katexBaseUrl()
{
    /* Candidati in ordine: bundle accanto all'eseguibile, poi percorsi di sistema */
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + "/assets/katex",
        appDir + "/../../assets/katex",
#ifdef Q_OS_LINUX
        "/usr/share/javascript/katex",
        "/usr/share/katex",
#elif defined(Q_OS_WIN)
        appDir + "/katex",
#elif defined(Q_OS_MACOS)
        appDir + "/../Resources/katex",
#endif
    };
    for (const QString& c : candidates) {
        if (QDir(c).exists())
            return "file:///" + QDir::cleanPath(c) + "/";
    }
    return "file:///usr/share/javascript/katex/";   /* Linux system fallback */
}

class LatexView : public QWebEngineView {
    Q_OBJECT
public:
    explicit LatexView(QWidget* parent = nullptr)
        : QWebEngineView(parent)
    {
        setContextMenuPolicy(Qt::NoContextMenu);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        page()->setBackgroundColor(Qt::transparent);
    }

    /* Imposta contenuto HTML; le formule nei delimitatori \(...\) e \[...\]
       vengono renderizzate da KaTeX tramite auto-render. */
    void setLatexHtml(const QString& bodyHtml,
                      const QString& bg = QStringLiteral("#1e293b"),
                      const QString& fg = QStringLiteral("#e2e8f0"))
    {
        QWebEngineView::setHtml(
            buildPage(bodyHtml, bg, fg),
            QUrl(katexBaseUrl()));
    }

private:
    static QString buildPage(const QString& body,
                             const QString& bg, const QString& fg)
    {
        static const char kHead[] =
            "<!DOCTYPE html><html><head>\n"
            "<meta charset=\"utf-8\">\n"
            "<link rel=\"stylesheet\" href=\"katex.min.css\">\n"
            "<script src=\"katex.min.js\"></script>\n"
            "<script src=\"contrib/auto-render.js\"></script>\n"
            "<style>\n"
            "*{box-sizing:border-box}\n"
            "body{font-family:system-ui,sans-serif;font-size:13px;"
            "padding:10px 12px;margin:0;line-height:1.65;"
            "background:%1;color:%2}\n"
            "h3{font-size:15px;font-weight:700;margin:0 0 8px 0}\n"
            "p{margin:4px 0}\n"
            "hr{border:none;border-top:1px solid #334155;margin:8px 0}\n"
            "ol,ul{margin:4px 0 4px 18px;padding:0}li{margin:3px 0}\n"
            "b{color:#93c5fd}small{font-size:11px;color:#94a3b8}\n"
            ".box-purple{background:#7c3aed22;border-left:4px solid #a78bfa;"
            "border-radius:4px;padding:7px 10px;margin:6px 0}\n"
            ".box-green{background:#16a34a22;border-left:4px solid #4ade80;"
            "border-radius:4px;padding:7px 10px;margin:6px 0}\n"
            ".box-blue{background:#1e3a5f;border-left:3px solid #3b82f6;"
            "border-radius:3px;padding:6px 10px;"
            "font-family:monospace;font-size:11px;color:#cbd5e1;margin:6px 0}\n"
            ".box-orange{background:#431407;border-left:3px solid #f97316;"
            "border-radius:3px;padding:6px 10px;"
            "font-family:monospace;font-size:11px;color:#fed7aa;margin:6px 0}\n"
            ".ai-out{white-space:pre-wrap}\n"
            ".katex{font-size:1.05em}\n"
            ".katex-display{margin:6px 0;overflow-x:auto}\n"
            "</style>\n"
            "</head><body>\n";

        static const char kFoot[] =
            "\n<script>\n"
            "renderMathInElement(document.body,{\n"
            "  delimiters:[\n"
            "    {left:'\\\\[',right:'\\\\]',display:true},\n"
            "    {left:'\\\\(',right:'\\\\)',display:false}\n"
            "  ],\n"
            "  throwOnError:false\n"
            "});\n"
            "</script>\n"
            "</body></html>";

        return QString::fromLatin1(kHead).arg(bg, fg) + body + QLatin1String(kFoot);
    }
};

#else // Fallback senza Qt6::WebEngineWidgets

#include <QTextEdit>
class LatexView : public QTextEdit {
    Q_OBJECT
public:
    explicit LatexView(QWidget* parent = nullptr)
        : QTextEdit(parent)
    {
        setReadOnly(true);
        setObjectName("chatLog");
    }
    void setLatexHtml(const QString& html,
                      const QString& = {},
                      const QString& = {})
    {
        setHtml(html);
    }
};
#endif

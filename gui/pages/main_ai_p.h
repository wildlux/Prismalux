#pragma once
// agenti_page_p.h — header privato interno, incluso SOLO dai agenti_page_*.cpp
// Non fare mai #include "main_ai_p.h" da file esterni.

#include "main_ai.h"
#include "../ai_client.h"
#include "../theme_manager.h"

/* ══════════════════════════════════════════════════════════════
   BubClr — palette bubble che segue il tema attivo
   ══════════════════════════════════════════════════════════════ */
struct BubClr {
    /* User bubble */
    const char* uBg;  const char* uBdr; const char* uHdr;
    const char* uTxt; const char* uBtn; const char* uBtnB;
    /* Agent bubble */
    const char* aBg;  const char* aBdr; const char* aHr;
    const char* aHdr; const char* aTxt;
    const char* aBtn; const char* aBtnB; const char* aBtnC;
    /* Local / math bubble */
    const char* lBg;  const char* lBdr; const char* lHr;
    const char* lHdr; const char* lTxt; const char* lRes;
    const char* lBtn; const char* lBtnB; const char* lBtnC;
    /* Tool strip */
    const char* tBgOk; const char* tBdOk;
    const char* tBgEr; const char* tBdEr;
    const char* tTxt;  const char* tStOk; const char* tStEr;
    /* Controller bubble */
    const char* cBgOk; const char* cBdOk; const char* cHdOk;
    const char* cBgWn; const char* cBdWn; const char* cHdWn;
    const char* cBgEr; const char* cBdEr; const char* cHdEr;
};

inline const BubClr kDark = {
    "#162544","#1d4ed8","#93c5fd","#e2e8f0","#1e3a5f","#1d4ed8",
    "#0e1624","#1e2d47","#2d3a54","#a78bfa","#e2e8f0","#1a2641","#4c3a7a","#c4b5fd",
    "#0e1a12","#166534","#166534","#4ade80","#e2e8f0","#86efac","#0e2318","#166534","#86efac",
    "#0b1a10","#166534","#1a0a0a","#7f1d1d","#8b949e","#4ade80","#f87171",
    "#0b1a10","#166534","#4ade80","#1a1400","#854d0e","#facc15","#1a0a0a","#7f1d1d","#f87171"
};

inline const BubClr kLight = {
    "#dbeafe","#3b82f6","#1d4ed8","#1e293b","#bfdbfe","#93c5fd",
    "#f1f5f9","#94a3b8","#e2e8f0","#7c3aed","#1e293b","#ede9fe","#a78bfa","#7c3aed",
    "#dcfce7","#4ade80","#86efac","#15803d","#1e293b","#15803d","#bbf7d0","#4ade80","#15803d",
    "#f0fdf4","#4ade80","#fef2f2","#f87171","#374151","#15803d","#dc2626",
    "#f0fdf4","#4ade80","#15803d","#fffbeb","#fcd34d","#92400e","#fef2f2","#fca5a5","#991b1b"
};

inline bool isLightTheme() {
    return ThemeManager::instance()->currentId().startsWith("light");
}

inline const BubClr& bc() {
    return isLightTheme() ? kLight : kDark;
}

/* ══════════════════════════════════════════════════════════════
   extractInputHtml — converte il QTextEdit in HTML leggero
   preservando bold/italic/underline/colore/sfondo.
   Usato al submit per costruire la bolla utente con formattazione.
   ══════════════════════════════════════════════════════════════ */
#include <QTextEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextCharFormat>
#include <QFont>

inline QString extractInputHtml(QTextEdit* edit)
{
    if (!edit) return {};
    const QTextDocument* doc = edit->document();
    QString html;
    QTextBlock block = doc->begin();
    while (block.isValid()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            const QTextCharFormat fmt = frag.charFormat();
            QString text = frag.text().toHtmlEscaped();
            text.replace(QChar(0x2029), "<br>"); /* Qt paragraph separator */

            /* Applica formattazione in ordine interno→esterno */
            const QColor bg = fmt.background().color();
            if (bg.isValid() && bg.alpha() > 0)
                text = QString("<span style='background:%1;'>%2</span>")
                           .arg(bg.name()).arg(text);
            /* Propaga il colore SOLO se l'utente lo ha impostato esplicitamente
               (hasProperty distingue colore-utente da colore-tema/palette). */
            if (fmt.hasProperty(QTextFormat::ForegroundBrush)) {
                const QColor fg = fmt.foreground().color();
                if (fg.isValid() && fg.alpha() > 0 &&
                    fg != QColor(Qt::black) && fg != QColor(Qt::white))
                    text = QString("<span style='color:%1;'>%2</span>")
                               .arg(fg.name()).arg(text);
            }
            if (fmt.fontUnderline())           text = "<u>" + text + "</u>";
            if (fmt.fontItalic())              text = "<i>" + text + "</i>";
            if (fmt.fontWeight() >= QFont::Bold) text = "<b>" + text + "</b>";
            html += text;
        }
        if (block.next().isValid()) html += "<br>";
        block = block.next();
    }
    return html;
}

// ── Forward declarations per funzioni helper condivise ──────────────────────
// Definite in agenti_page_math.cpp, usate anche da altri _*.cpp
QString _sanitize_prompt(const QString& raw);
// Definita in agenti_page_models.cpp
bool    _isEmbeddingModel(const QString& name);
bool    _is_likely_english(const QString& text);
QString _math_sys(const QString& task, const QString& base);
QString _buildSys(const QString& task, const QString& full,
                  const QString& small, const QString& modelName,
                  AiClient::Backend backend);
QString _buildSys(const QString& task, const QString& full,
                  const QString& modelName, AiClient::Backend backend);
QString _inject_math(const QString& task);
/** Calcola formule scientifiche (Ohm, RC, conversioni unità, chimica) direttamente.
 *  Antepone "[Calcolo locale: ...]" se la query corrisponde a un pattern noto. */
QString _inject_science(const QString& task);
/** Inietta numeri casuali reali nel task se l'utente li richiede.
 *  Usa std::random_device + std::mt19937 (= randomize C nativo con seed crittografico).
 *  Definita in agenti_page_tools.cpp. */
QString _inject_random(const QString& task);

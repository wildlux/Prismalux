#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QSettings>
#include <QTextDocument>

/* ══════════════════════════════════════════════════════════════
   buildUserBubble / buildAgentBubble
   Layout chat-style: utente a destra (blu), AI a sinistra (scuro).
   QTextEdit renderizza HTML4 + CSS2 parziale: usiamo <table> per il
   layout e style inline per colori/bordi/padding.
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::buildUserBubble(const QString& text, int bubbleIdx)
{
    /* Normalizza: minuscolo (meno memoria nel modello, aspetto più naturale) */
    QString safe = text.toLower();
    safe.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
    /* Preserva a capo */
    safe.replace("\n","<br>");

    /* Barra azioni dentro la bolla — link semplici (QTextBrowser gestisce solo <a href> piani) */
    const auto& c = bc();
    QString actionBar;
    if (bubbleIdx >= 0) {
        const QString id  = QString::number(bubbleIdx);
        const QString b64 = text.left(4096).toUtf8().toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        const QString lnk = QString("color:%1;font-size:12px;text-decoration:none;"
            "border:1px solid %2;padding:2px 10px;background:%3;")
            .arg(c.uHdr, c.uBtnB, c.uBtn);
        actionBar =
            "<p align='right' style='margin:6px 0 0 0;'>"
              "<a href='copy:" + id + ":" + b64 + "' style='" + lnk + "'>"
                "\xf0\x9f\x97\x82</a>"
              " &nbsp; "
              "<a href='edit:" + id + ":" + b64 + "' style='" + lnk + "'>"
                "\xe2\x9c\x8f\xef\xb8\x8f</a>"
              " &nbsp; "
              "<a href='tts:" + id + ":" + b64 + "' style='" + lnk + "'>"
                "\xf0\x9f\x8e\x99</a>"
              " &nbsp; "
              "<a href='del:" + id + ":" + b64 + "' style='" + lnk + "'>"
                "\xf0\x9f\x97\x91</a>"
            "</p>";
    }

    const int br = QSettings("Prismalux","GUI").value(P::SK::kBubbleRadius,10).toInt();
    const QString brs = QString::number(br) + "px";
    return
        "<table width='100%' cellpadding='0' cellspacing='0'>"
        "<tr>"
          "<td width='80'>&nbsp;</td>"
          "<td bgcolor='" + QString(c.uBg) + "' style='"
              "border:1px solid " + c.uBdr + ";"
              "border-radius:" + brs + ";"
              "padding:10px 14px;"
              "color:" + c.uTxt + ";"
          "'>"
            "<p style='color:" + c.uHdr + ";font-size:11px;font-weight:bold;"
                       "margin:0 0 5px 0;'>"
              "\xf0\x9f\x91\xa4  Tu</p>"
            "<p style='margin:0;line-height:1.6;color:" + c.uTxt + ";'>" + safe + "</p>"
            + actionBar +
          "</td>"
        "</tr>"
        "</table>"
        "<p style='margin:6px 0;'></p>";
}

QString AgentiPage::buildAgentBubble(const QString& label, const QString& model,
                                     const QString& time,  const QString& htmlContent,
                                     int bubbleIdx, const QString& thinkContent)
{
    /* Header bolla: "🛸  Agente 1 — Ricercatore  ·  🤖 deepseek-r1:7b  ·  01:55:47" */
    QString esc_model = model;
    esc_model.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
    QString esc_label = label;
    esc_label.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");

    QString header = esc_label
        + " &nbsp;\xc2\xb7\xc2\xb7&nbsp; "
        "\xf0\x9f\xa4\x96 " + esc_model
        + " &nbsp;\xc2\xb7\xc2\xb7&nbsp; "
        "\xf0\x9f\x95\x90 " + time;

    /* Barra azioni dentro la bolla agente */
    const auto& c = bc();
    QString actionBar;
    if (bubbleIdx >= 0) {
        const QString id = QString::number(bubbleIdx);
        QTextDocument _tmp;
        _tmp.setHtml(htmlContent);
        const QString b64 = _tmp.toPlainText().left(4096).toUtf8().toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        QString metaLeft;
        if (!time.isEmpty() || !model.isEmpty()) {
            QString esc_t = time;
            QString esc_m = model;
            esc_t.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
            esc_m.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
            metaLeft =
                "<span style='color:#6b7280;font-size:11px;'>"
                + (time.isEmpty()  ? QString() : "\xf0\x9f\x95\x90 " + esc_t)
                + ((!time.isEmpty() && !model.isEmpty()) ? " &nbsp;\xc2\xb7&nbsp; " : QString())
                + (model.isEmpty() ? QString() : "\xf0\x9f\xa4\x96 " + esc_m)
                + "</span>";
        }
        const QString lnk = QString("color:%1;font-size:12px;text-decoration:none;"
            "border:1px solid %2;padding:2px 10px;background:%3;")
            .arg(c.aBtnC, c.aBtnB, c.aBtn);
        actionBar =
            "<table width='100%' cellpadding='0' cellspacing='0' style='margin:6px 0 0 0;'>"
            "<tr>"
              "<td>" + metaLeft + "</td>"
              "<td align='right' style='white-space:nowrap;'>"
                "<a href='copy:" + id + ":" + b64 + "' style='" + lnk + "'>"
                  "\xf0\x9f\x97\x82</a>"
                " &nbsp; "
                "<a href='edit:" + id + ":" + b64 + "' style='" + lnk + "' "
                   "title='Modifica e reinvia'>"
                  "\xe2\x9c\x8f\xef\xb8\x8f</a>"
                " &nbsp; "
                "<a href='tts:" + id + ":" + b64 + "' style='" + lnk + "'>"
                  "\xf0\x9f\x8e\x99</a>"
                " &nbsp; "
                "<a href='del:" + id + ":" + b64 + "' style='" + lnk + "' "
                   "title='Elimina questo messaggio'>"
                  "\xf0\x9f\x97\x91</a>"
              "</td>"
            "</tr>"
            "</table>";
    }

    /* Toggle reasoning collassabile (solo se il modello ha prodotto <think>…</think>) */
    QString thinkBar;
    if (!thinkContent.isEmpty() && bubbleIdx >= 0) {
        const int words = thinkContent.split(' ', Qt::SkipEmptyParts).size();
        const QString id = QString::number(bubbleIdx);
        thinkBar =
            "<p style='margin:0 0 6px 0;'>"
              "<a href='think:toggle:" + id + "' "
                 "style='color:" + QString(c.aHdr) + ";font-size:11px;"
                 "text-decoration:none;opacity:0.75;'>"
                "\xe2\x96\xb6\xef\xb8\x8f Ragionamento (" + QString::number(words) + " par.)"
              "</a>"
            "</p>";
    }

    const int br = QSettings("Prismalux","GUI").value(P::SK::kBubbleRadius,10).toInt();
    const QString brs = QString::number(br) + "px";
    return
        "<p style='margin:6px 0;'></p>"
        "<table width='100%' cellpadding='0' cellspacing='0'>"
        "<tr>"
          "<td bgcolor='" + QString(c.aBg) + "' style='"
              "border:1px solid " + c.aBdr + ";"
              "border-radius:" + brs + ";"
              "padding:10px 14px;"
              "color:" + c.aTxt + ";"
          "'>"
            "<p style='color:" + c.aHdr + ";font-size:11px;font-weight:bold;"
                       "margin:0 0 6px 0;'>"
              + header +
            "</p>"
            "<hr style='border:none;border-top:1px solid " + c.aHr + ";margin:5px 0 8px 0;'>"
            + thinkBar
            + htmlContent
            + actionBar +
          "</td>"
          "<td width='30'>&nbsp;</td>"
        "</tr>"
        "</table>"
        "<p style='margin:4px 0;'></p>";
}

/* ══════════════════════════════════════════════════════════════
   buildLocalBubble — bolla per risposta locale (0 token, math)
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::buildLocalBubble(const QString& result, double ms, int bubbleIdx,
                                      const QString& extraLinks)
{
    QString safe = result;
    safe.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
    safe.replace("\n","<br>");

    QString timing = ms < 1.0
        ? QString::number(ms, 'f', 3) + " ms"
        : QString::number(ms / 1000.0, 'f', 3) + " s";

    /* Barra azioni dentro la bolla locale */
    const auto& c = bc();
    QString actionBar;
    if (bubbleIdx >= 0) {
        const QString id  = QString::number(bubbleIdx);
        const QString b64 = result.left(4096).toUtf8().toBase64(
            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        const QString linkStyle = QString(
            "color:%1;font-size:12px;text-decoration:none;"
            "border:1px solid %2;padding:2px 10px;background:%3;")
            .arg(c.lBtnC, c.lBtnB, c.lBtn);
        actionBar =
            "<p align='right' style='margin:6px 0 0 0;'>";
        if (!extraLinks.isEmpty())
            actionBar += extraLinks + " &nbsp; ";
        actionBar +=
              "<a href='copy:" + id + ":" + b64 + "' style='" + linkStyle + "'>"
                "\xf0\x9f\x97\x82</a>"
              " &nbsp; "
              "<a href='edit:" + id + ":" + b64 + "' style='" + linkStyle + "' "
                 "title='Modifica e reinvia'>"
                "\xe2\x9c\x8f\xef\xb8\x8f</a>"
              " &nbsp; "
              "<a href='tts:" + id + ":" + b64 + "' style='" + linkStyle + "'>"
                "\xf0\x9f\x8e\x99</a>"
            "</p>";
    }

    const int br = QSettings("Prismalux","GUI").value(P::SK::kBubbleRadius,10).toInt();
    const QString brs = QString::number(br) + "px";
    return
        "<p style='margin:6px 0;'></p>"
        "<table width='100%' cellpadding='0' cellspacing='0'>"
        "<tr>"
          "<td bgcolor='" + QString(c.lBg) + "' style='"
              "border:1px solid " + c.lBdr + ";"
              "border-radius:" + brs + ";"
              "padding:10px 14px;"
              "color:" + c.lTxt + ";"
          "'>"
            "<p style='color:" + c.lHdr + ";font-size:11px;font-weight:bold;margin:0 0 5px 0;'>"
              "\xe2\x9a\xa1  Risposta locale  \xc2\xb7\xc2\xb7  0 token  \xc2\xb7\xc2\xb7  "
              + timing +
            "</p>"
            "<hr style='border:none;border-top:1px solid " + c.lHr + ";margin:5px 0 8px 0;'>"
            "<p style='font-size:15px;font-weight:bold;margin:0;color:" + c.lRes + ";'>"
              + safe +
            "</p>"
            + actionBar +
          "</td>"
          "<td width='30'>&nbsp;</td>"
        "</tr>"
        "</table>"
        "<p style='margin:4px 0;'></p>";
}

/* ══════════════════════════════════════════════════════════════
   buildToolStrip — striscia esecutore Python
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::buildToolStrip(const QString& code, const QString& output,
                                   int exitCode, double ms)
{
    QString esc_out = output;
    esc_out.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
    if (esc_out.trimmed().isEmpty()) esc_out = "(nessun output)";
    if (esc_out.length() > 3000) esc_out = esc_out.left(3000) + "\n... [troncato]";
    esc_out.replace("\n","<br>");

    QString esc_code = code;
    esc_code.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
    if (esc_code.length() > 500) esc_code = esc_code.left(500) + "\n... [troncato]";
    esc_code.replace("\n","<br>");

    const auto& c = bc();
    const QString statusIcon  = (exitCode == 0) ? "\xe2\x9c\x85" : "\xe2\x9d\x8c";
    const QString statusColor = (exitCode == 0) ? c.tStOk : c.tStEr;
    const QString borderColor = (exitCode == 0) ? c.tBdOk : c.tBdEr;
    const QString bgColor     = (exitCode == 0) ? c.tBgOk : c.tBgEr;

    return
        "<p style='margin:2px 0;'></p>"
        "<table width='100%' cellpadding='0' cellspacing='0'>"
        "<tr>"
          "<td width='20'>&nbsp;</td>"
          "<td bgcolor='" + bgColor + "' style='"
              "border:1px solid " + borderColor + ";"
              "border-radius:8px;"
              "padding:8px 12px;"
              "color:" + QString(c.tTxt) + ";"
          "'>"
            "<p style='font-size:11px;font-weight:bold;margin:0 0 4px 0;color:" + statusColor + ";'>"
              "\xe2\x9a\x99\xef\xb8\x8f  Esecutore Python  \xc2\xb7\xc2\xb7  "
              + statusIcon + " exit " + QString::number(exitCode) +
              "  \xc2\xb7\xc2\xb7  " + QString::number(ms, 'f', 0) + " ms"
            "</p>"
            "<hr style='border:none;border-top:1px solid " + borderColor + ";margin:4px 0;'>"
            "<p style='font-family:\"JetBrains Mono\",monospace;font-size:11px;"
               "color:#8b949e;margin:0 0 6px 0;'>"
              + esc_out +
            "</p>"
          "</td>"
          "<td width='20'>&nbsp;</td>"
        "</tr>"
        "</table>"
        "<p style='margin:2px 0;'></p>";
}

/* ══════════════════════════════════════════════════════════════
   buildControllerBubble — bolla del controller LLM
   Colore in base al verdetto: ✅ verde / ⚠️ giallo / ❌ rosso
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::buildControllerBubble(const QString& htmlContent)
{
    /* Determina tono in base al verdetto presente nell'HTML */
    const auto& c = bc();
    QString bg = c.cBgOk, border = c.cBdOk, hdrColor = c.cHdOk;
    if (htmlContent.contains("AVVISO") || htmlContent.contains("\xe2\x9a\xa0")) {
        bg = c.cBgWn; border = c.cBdWn; hdrColor = c.cHdWn;
    }
    if (htmlContent.contains("ERRORE") || htmlContent.contains("\xe2\x9d\x8c")) {
        bg = c.cBgEr; border = c.cBdEr; hdrColor = c.cHdEr;
    }

    return
        "<p style='margin:2px 0;'></p>"
        "<table width='100%' cellpadding='0' cellspacing='0'>"
        "<tr>"
          "<td width='40'>&nbsp;</td>"
          "<td bgcolor='" + bg + "' style='"
              "border:1px solid " + border + ";"
              "border-radius:10px;"
              "padding:10px 14px;"
              "color:#e2e8f0;"
          "'>"
            "<p style='color:" + hdrColor + ";font-size:11px;font-weight:bold;margin:0 0 5px 0;'>"
              "\xf0\x9f\x9b\xa1\xef\xb8\x8f  Controller AI  \xc2\xb7\xc2\xb7  Validatore pipeline"
            "</p>"
            "<hr style='border:none;border-top:1px solid " + border + ";margin:5px 0 8px 0;'>"
            + htmlContent +
          "</td>"
        "</tr>"
        "</table>"
        "<p style='margin:6px 0;'></p>";
}


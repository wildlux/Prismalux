#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTextCursor>

/* ══════════════════════════════════════════════════════════════
   saveFeedback — salva rating 👍/👎 in ~/.prismalux/feedback.jsonl
   Ogni riga è un oggetto JSON compatto:
   {"timestamp":"…","model":"…","prompt":"…","response":"…","rating":±1}
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::saveFeedback(int bubbleIdx, int rating)
{
    /* Testo risposta AI dalla bolla indicizzata */
    const QString response = m_bubbleTexts.value(bubbleIdx).trimmed();
    if (response.isEmpty()) return;

    /* Prompt: la bolla utente più vicina con indice < bubbleIdx */
    QString prompt = m_taskOriginal;
    for (int i = bubbleIdx - 1; i >= 0; --i) {
        if (m_bubbleTexts.contains(i)) {
            const QString cand = m_bubbleTexts.value(i).trimmed();
            if (!cand.isEmpty()) { prompt = cand; break; }
        }
    }

    /* Modello attivo */
    const QString model = m_pageModel.isEmpty() ? m_ai->model() : m_pageModel;

    /* Costruisce entry JSONL */
    QJsonObject entry;
    entry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    entry["model"]     = model;
    entry["prompt"]    = prompt.left(2000);
    entry["response"]  = response.left(4000);
    entry["rating"]    = rating;   /* +1 = 👍  /  -1 = 👎 */

    /* Crea cartella e appende al file */
    const QString path = P::feedbackPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write(QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n');
        f.close();
    }

    /* Feedback visivo: riga colorata in fondo alla chat */
    const QString icon  = (rating > 0) ? "\xf0\x9f\x91\x8d" : "\xf0\x9f\x91\x8e";
    const QString color = (rating > 0) ? "#22c55e" : "#ef4444";
    m_log->moveCursor(QTextCursor::End);
    m_log->insertHtml(
        "<p style='color:" + color + ";font-size:11px;text-align:center;"
        "font-style:italic;margin:2px 0 6px 0;'>"
        + icon + "  Feedback salvato</p>");
}

#pragma once
/*
 * external_ai_import.h — Importa conversazioni esportate da servizi AI esterni
 * (OpenAI ChatGPT, Anthropic Claude, e formato generico "role/content" usato
 * da molte API OpenAI-compatible — DeepSeek, Qwen, ecc.) nello storico chat
 * locale di Prismalux (ChatHistory).
 *
 * Formati riconosciuti (verificati contro fonti pubbliche prima di scrivere
 * il parser — vedi note sotto per ciò che NON è verificato):
 *   1. OpenAI ChatGPT — "conversations.json" dall'export dati account
 *      (Impostazioni → Data controls → Export data): array di conversazioni,
 *      ciascuna con "mapping" (albero di nodi indicizzato per id) +
 *      "current_node" (nodo foglia del ramo attivo).
 *   2. Anthropic Claude — "conversations.json" dall'export dati account
 *      (Settings → Privacy → Export data): array di conversazioni con
 *      "chat_messages": [{sender, text|content[]}]. Lo schema NON è
 *      documentato ufficialmente da Anthropic (dedotto da fonti terze
 *      concordanti) — gestiamo solo l'ordine sequenziale confermato, non
 *      l'eventuale variante a lista collegata (parent_message_uuid) citata
 *      da una sola fonte non confermata altrove.
 *   3. Generico OpenAI-compatible — {"messages":[{"role","content"}, ...]}
 *      oppure un array bare [{"role","content"}, ...]. DeepSeek e Qwen NON
 *      hanno un export ufficiale proprio (verificato: nessuna fonte lo
 *      documenta) — le loro chat, se esportate tramite estensioni browser
 *      di terze parti, seguono tipicamente questo schema standard.
 *   4. Un array di conversazioni nel formato (3), ciascuna con "title"
 *      opzionale e "messages"/"conversation".
 *
 * NON gestito: export "Google Takeout" (sezione "Gemini Apps") — l'export
 * esiste ed è ufficiale, ma i nomi esatti dei campi JSON non sono
 * documentati in modo affidabile da nessuna fonte trovata: implementare un
 * parser dedicato avrebbe richiesto indovinare lo schema, quindi non è
 * incluso. Un file Takeout non corrisponde a nessun formato sopra e
 * parseFile() lo segnala come non riconosciuto invece di tentare
 * un'interpretazione incerta.
 */

#include <QString>
#include <QVector>
#include <QList>
#include <QSet>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QDateTime>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include "../chat_history.h"

namespace ExternalAiImport {

struct ImportedConversation {
    QString               title;
    QVector<ChatMessage>  messages;
};

/** Normalizza il ruolo del messaggio verso lo schema interno di Prismalux
 *  (ChatMessage::role = "user" | "pipeline" | "system" — "pipeline" è
 *  usato per le risposte generate dall'AI, vedi chat_history.h). */
inline QString normalizeRole(const QString& raw)
{
    const QString r = raw.trimmed().toLower();
    if (r == "user" || r == "human")                 return "user";
    if (r == "assistant" || r == "model" || r == "bot" || r == "ai") return "pipeline";
    return "system";
}

/** Estrae il testo da un nodo OpenAI: content.parts è un array che può
 *  contenere stringhe o oggetti (es. immagini) — teniamo solo le stringhe. */
inline QString openAiPartsText(const QJsonObject& content)
{
    QString text;
    for (const QJsonValue& p : content.value("parts").toArray())
        if (p.isString()) text += p.toString();
    return text;
}

/** OpenAI ChatGPT — una conversazione dal "mapping" ad albero.
 *  Segue i puntatori "parent" a ritroso da current_node fino alla radice,
 *  poi inverte per ottenere l'ordine cronologico del ramo effettivamente
 *  mostrato all'utente (ignora i rami alternativi di eventuali "rigenera"). */
inline QVector<ChatMessage> parseOpenAiConversation(const QJsonObject& conv)
{
    QVector<ChatMessage> out;
    const QJsonObject mapping = conv.value("mapping").toObject();
    QString id = conv.value("current_node").toString();
    if (id.isEmpty() || mapping.isEmpty()) return out;

    QStringList chain;
    QSet<QString> visited;
    while (!id.isEmpty() && mapping.contains(id) && !visited.contains(id)) {
        visited.insert(id);
        chain.prepend(id);
        id = mapping.value(id).toObject().value("parent").toString();
    }

    for (const QString& nodeId : chain) {
        const QJsonValue msgVal = mapping.value(nodeId).toObject().value("message");
        if (!msgVal.isObject()) continue;
        const QJsonObject msg  = msgVal.toObject();
        const QString     role = msg.value("author").toObject().value("role").toString();
        if (role.isEmpty() || role == "system" || role == "tool") continue;

        const QString text = openAiPartsText(msg.value("content").toObject());
        if (text.trimmed().isEmpty()) continue;

        ChatMessage cm;
        cm.role    = normalizeRole(role);
        cm.content = text;
        const double t = msg.value("create_time").toDouble(0.0);
        cm.timestamp = t > 0 ? QDateTime::fromSecsSinceEpoch(qint64(t)) : QDateTime::currentDateTime();
        out.append(cm);
    }
    return out;
}

/** Anthropic Claude — "chat_messages": [{sender, text}] oppure
 *  [{sender, content:[{type:"text", text:"..."}]}] a seconda della versione
 *  dell'export. */
inline QVector<ChatMessage> parseClaudeConversation(const QJsonObject& conv)
{
    QVector<ChatMessage> out;
    for (const QJsonValue& mv : conv.value("chat_messages").toArray()) {
        const QJsonObject m    = mv.toObject();
        const QString     role = m.value("sender").toString();
        if (role.isEmpty()) continue;

        QString text = m.value("text").toString();
        if (text.isEmpty()) {
            for (const QJsonValue& blk : m.value("content").toArray()) {
                const QJsonObject b = blk.toObject();
                if (b.value("type").toString() == "text")
                    text += b.value("text").toString();
            }
        }
        if (text.trimmed().isEmpty()) continue;

        ChatMessage cm;
        cm.role    = normalizeRole(role);
        cm.content = text;
        cm.timestamp = QDateTime::fromString(m.value("created_at").toString(), Qt::ISODate);
        if (!cm.timestamp.isValid()) cm.timestamp = QDateTime::currentDateTime();
        out.append(cm);
    }
    return out;
}

/** Formato generico OpenAI-compatible: array di {"role","content"}.
 *  Copre DeepSeek, Qwen e la maggior parte delle API/tool che seguono lo
 *  schema chat completion standard. */
inline QVector<ChatMessage> parseGenericMessages(const QJsonArray& arr)
{
    QVector<ChatMessage> out;
    for (const QJsonValue& v : arr) {
        const QJsonObject m = v.toObject();
        const QString role = m.value("role").toString();
        QString content = m.value("content").toString();
        /* alcune varianti usano content come array di blocchi {type,text} */
        if (content.isEmpty() && m.value("content").isArray()) {
            for (const QJsonValue& blk : m.value("content").toArray())
                content += blk.toObject().value("text").toString();
        }
        if (role.isEmpty() || content.trimmed().isEmpty()) continue;
        ChatMessage cm;
        cm.role      = normalizeRole(role);
        cm.content   = content;
        cm.timestamp = QDateTime::currentDateTime();
        out.append(cm);
    }
    return out;
}

/** Titolo leggibile per una conversazione senza "title" esplicito:
 *  prime parole del primo messaggio utente. */
inline QString fallbackTitle(const QVector<ChatMessage>& msgs)
{
    for (const ChatMessage& m : msgs)
        if (m.role == "user")
            return m.content.left(40).trimmed();
    return QObject::tr("Conversazione importata");
}

/** Analizza un file JSON e ritorna tutte le conversazioni riconosciute.
 *  errorMsg viene valorizzato solo in caso di fallimento (file illeggibile,
 *  JSON non valido, o formato non riconosciuto) — result vuoto altrimenti
 *  è un fallimento silenzioso "nessuna conversazione trovata", non un errore. */
inline QList<ImportedConversation> parseFile(const QString& path, QString& detectedFormat,
                                             QString& errorMsg)
{
    QList<ImportedConversation> result;
    detectedFormat.clear();
    errorMsg.clear();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = QObject::tr("Impossibile aprire il file.");
        return result;
    }
    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
    if (pe.error != QJsonParseError::NoError) {
        errorMsg = QObject::tr("JSON non valido: ") + pe.errorString();
        return result;
    }

    /* ── Caso 1: array di conversazioni (ChatGPT o Claude export) ── */
    if (doc.isArray()) {
        const QJsonArray arr = doc.array();
        if (arr.isEmpty()) { errorMsg = QObject::tr("File vuoto."); return result; }

        const QJsonObject first = arr.first().toObject();

        if (first.contains("mapping")) {
            detectedFormat = "OpenAI (ChatGPT)";
            for (const QJsonValue& cv : arr) {
                const QJsonObject conv = cv.toObject();
                const auto msgs = parseOpenAiConversation(conv);
                if (msgs.isEmpty()) continue;
                ImportedConversation ic;
                ic.title    = conv.value("title").toString();
                if (ic.title.isEmpty()) ic.title = fallbackTitle(msgs);
                ic.messages = msgs;
                result.append(ic);
            }
            return result;
        }

        if (first.contains("chat_messages")) {
            detectedFormat = "Anthropic (Claude)";
            for (const QJsonValue& cv : arr) {
                const QJsonObject conv = cv.toObject();
                const auto msgs = parseClaudeConversation(conv);
                if (msgs.isEmpty()) continue;
                ImportedConversation ic;
                ic.title    = conv.value("name").toString();
                if (ic.title.isEmpty()) ic.title = fallbackTitle(msgs);
                ic.messages = msgs;
                result.append(ic);
            }
            return result;
        }

        if (first.contains("role") && first.contains("content")) {
            /* array bare di messaggi — una sola conversazione */
            detectedFormat = "Generico (role/content)";
            const auto msgs = parseGenericMessages(arr);
            if (!msgs.isEmpty()) {
                ImportedConversation ic;
                ic.title    = fallbackTitle(msgs);
                ic.messages = msgs;
                result.append(ic);
            }
            return result;
        }

        if (first.contains("messages") || first.contains("conversation")) {
            /* array di conversazioni, ciascuna {"title","messages"} */
            detectedFormat = "Generico multi-conversazione";
            for (const QJsonValue& cv : arr) {
                const QJsonObject conv = cv.toObject();
                const QJsonArray msgArr = conv.contains("messages")
                    ? conv.value("messages").toArray()
                    : conv.value("conversation").toArray();
                const auto msgs = parseGenericMessages(msgArr);
                if (msgs.isEmpty()) continue;
                ImportedConversation ic;
                ic.title    = conv.value("title").toString();
                if (ic.title.isEmpty()) ic.title = fallbackTitle(msgs);
                ic.messages = msgs;
                result.append(ic);
            }
            return result;
        }

        errorMsg = QObject::tr("Formato JSON non riconosciuto.");
        return result;
    }

    /* ── Caso 2: singolo oggetto {"messages":[...]} ── */
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.contains("messages")) {
            detectedFormat = "Generico (messages)";
            const auto msgs = parseGenericMessages(obj.value("messages").toArray());
            if (!msgs.isEmpty()) {
                ImportedConversation ic;
                ic.title    = obj.value("title").toString();
                if (ic.title.isEmpty()) ic.title = fallbackTitle(msgs);
                ic.messages = msgs;
                result.append(ic);
            }
            return result;
        }
        if (obj.contains("mapping")) {
            detectedFormat = "OpenAI (ChatGPT)";
            const auto msgs = parseOpenAiConversation(obj);
            if (!msgs.isEmpty()) {
                ImportedConversation ic;
                ic.title    = obj.value("title").toString();
                if (ic.title.isEmpty()) ic.title = fallbackTitle(msgs);
                ic.messages = msgs;
                result.append(ic);
            }
            return result;
        }
        if (obj.contains("chat_messages")) {
            detectedFormat = "Anthropic (Claude)";
            const auto msgs = parseClaudeConversation(obj);
            if (!msgs.isEmpty()) {
                ImportedConversation ic;
                ic.title    = obj.value("name").toString();
                if (ic.title.isEmpty()) ic.title = fallbackTitle(msgs);
                ic.messages = msgs;
                result.append(ic);
            }
            return result;
        }
    }

    errorMsg = QObject::tr("Formato JSON non riconosciuto \xe2\x80\x94 atteso export "
                           "OpenAI/Anthropic o un array {\"role\",\"content\"}.");
    return result;
}

/** Genera un log HTML semplice e leggibile (non replica lo stile delle
 *  bolle live di AgentiPage — è un import, non serve pixel-identico) e
 *  la mappa bubble_texts (indice → testo grezzo, usata da Prismalux per
 *  "Rifai"/copia). Ritorna anche l'indice del messaggio per bubbleTexts. */
inline QString buildImportLogHtml(const QVector<ChatMessage>& msgs, QMap<int,QString>& bubbleTexts)
{
    QString html;
    int idx = 0;
    for (const ChatMessage& m : msgs) {
        const bool isUser = (m.role == "user");
        const QString bg     = isUser ? "#1e3a5f" : "#1e293b";
        const QString label  = isUser ? "Tu" : (m.role == "system" ? "Sistema" : "AI");
        html += QString(
            "<table id='ubbl:%1' width='100%' style='margin:6px 0;'><tr><td "
            "style='background:%2;border-radius:8px;padding:10px 14px;'>"
            "<b>%3</b><br>%4</td></tr></table>")
            .arg(idx).arg(bg, label, m.content.toHtmlEscaped().replace('\n', "<br>"));
        bubbleTexts[idx] = m.content;
        ++idx;
    }
    return html;
}

/** Importa tutte le conversazioni di un file nello storico chat locale.
 *  Ritorna il numero di conversazioni effettivamente salvate. */
inline int importIntoHistory(const QList<ImportedConversation>& convs)
{
    ChatHistory history;
    int saved = 0;
    for (const ImportedConversation& ic : convs) {
        if (ic.messages.isEmpty()) continue;
        const QString id = history.newSession(ic.title);
        QMap<int,QString> bubbleTexts;
        const QString html = buildImportLogHtml(ic.messages, bubbleTexts);
        history.saveSession(id, html, bubbleTexts, {}, {});
        if (!ic.title.isEmpty())
            history.updateTitleAndSummary(id, ic.title, "", "");
        ++saved;
    }
    return saved;
}

/* ══════════════════════════════════════════════════════════════
   Import verso il RAG (D-51) — converte export di AI esterne in file
   testuali dentro destDir, pronti per l'indicizzazione RagGraph:
   - JSON riconosciuto (ChatGPT mapping / Claude chat_messages /
     generico role-content) → un .md per conversazione, turni etichettati
   - testo libero o JSON non riconosciuto (Gemini, Grok, estensioni
     browser: nessuno standard) → copiato .txt così com'è
   - file binari (byte NUL) o vuoti → scartati e contati come errori
   ══════════════════════════════════════════════════════════════ */

/** Slug sicuro per nome file (lettere/numeri/underscore, max 40). */
inline QString importFileSlug(const QString& s)
{
    QString slug = s.toLower();
    slug.replace(QRegularExpression("[^a-z0-9]+"), "_");
    slug = slug.left(40);
    while (slug.startsWith('_')) slug.remove(0, 1);
    while (slug.endsWith('_'))   slug.chop(1);
    return slug.isEmpty() ? QStringLiteral("chat") : slug;
}

/** Converte/copia i file indicati in destDir (creata se manca).
 *  Ritorna il numero di file scritti; i contatori opzionali dettagliano
 *  conversazioni riconosciute / testi copiati / file scartati. */
inline int importFilesToDir(const QStringList& files, const QString& destDir,
                            int* nConv = nullptr, int* nText = nullptr,
                            int* nErr  = nullptr)
{
    QDir().mkpath(destDir);
    const QString stamp =
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    int written = 0, convs = 0, texts = 0, errs = 0;

    auto uniquePath = [&destDir](const QString& base, const QString& ext) {
        QString p = destDir + "/" + base + ext;
        for (int i = 2; QFile::exists(p); ++i)
            p = destDir + "/" + base + "_" + QString::number(i) + ext;
        return p;
    };

    for (const QString& src : files) {
        QString fmt, err;
        const auto convList = parseFile(src, fmt, err);

        if (!convList.isEmpty()) {
            /* Un .md per conversazione, turni etichettati: dà al RagGraph
               testo pulito invece del JSON grezzo pieno di metadati. */
            for (const ImportedConversation& c : convList) {
                if (c.messages.isEmpty()) continue;
                const QString title = c.title.isEmpty()
                    ? QFileInfo(src).completeBaseName() : c.title;
                QString md = "# " + title + "\n\n";
                md += QString("_Fonte: %1 (%2)_\n\n")
                          .arg(QFileInfo(src).fileName(), fmt);
                for (const ChatMessage& m : c.messages)
                    md += QString("**%1:** %2\n\n")
                              .arg(m.role == "user" ? QStringLiteral("Utente")
                                                    : QStringLiteral("AI"),
                                   m.content.trimmed());
                QFile out(uniquePath(importFileSlug(title) + "_" + stamp, ".md"));
                if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    out.write(md.toUtf8());
                    ++written; ++convs;
                } else ++errs;
            }
            continue;
        }

        /* Formato non riconosciuto: testo libero copiato as-is */
        QFile in(src);
        if (!in.open(QIODevice::ReadOnly)) { ++errs; continue; }
        const QByteArray raw = in.readAll();
        if (raw.isEmpty() || raw.contains('\0')) { ++errs; continue; }

        QFile out(uniquePath(
            importFileSlug(QFileInfo(src).completeBaseName()) + "_" + stamp,
            ".txt"));
        if (out.open(QIODevice::WriteOnly)) {
            out.write(raw);
            ++written; ++texts;
        } else ++errs;
    }

    if (nConv) *nConv = convs;
    if (nText) *nText = texts;
    if (nErr)  *nErr  = errs;
    return written;
}

} // namespace ExternalAiImport

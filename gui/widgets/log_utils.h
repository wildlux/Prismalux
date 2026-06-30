#pragma once
/*
 * log_utils.h — Logging centralizzato con prefisso categoria+livello.
 *
 * USO AUTOMATICO (zero modifiche ai qWarning esistenti):
 *   In main.cpp: PLog::installMessageHandler();
 *   → intercetta tutti i qDebug/qWarning/qCritical e aggiunge
 *     [CAT][LVL] emoji in testa, deducendo la categoria dal testo.
 *
 * USO ESPLICITO nei nuovi file:
 *   #include "widgets/log_utils.h"
 *   PLog::w(PLog::Mcp, "processo non avviato: " + bin);
 *   PLog::e(PLog::Io,  "impossibile aprire: "   + path);
 *
 * Formattazione per widget UI:
 *   label->setText(PLog::fmt(PLog::Net, PLog::Warn, "TLS non disponibile"));
 *   browser->append(PLog::html(PLog::Llm, PLog::Err, "Ollama non raggiungibile"));
 *
 * ─────────────────────────────────────────────────────────────────────────
 * Categorie:
 *   QT   — componenti Qt/UI, widget, rendering
 *   PY   — subprocess Python, script, pip
 *   LLM  — Ollama, llama-server, AI backend
 *   TOOL — tool in-process (calc, ricerca, python, ecc.)
 *   MCP  — plugin MCP subprocess / JSON-RPC
 *   IO   — file, DB SQLite, percorsi, lettura/scrittura
 *   NET  — HTTP, TCP, TLS, LAN server, API
 *   STT  — Whisper, faster-whisper, VAD, TTS/piper
 *   RAG  — RAG engine, embedding, indicizzazione
 *   SYS  — sistema, hardware, processi generici
 *   SEC  — sicurezza, token, autenticazione
 * ─────────────────────────────────────────────────────────────────────────
 * Livelli:
 *   DBG  🔍  — debug interno (visibile solo con PRISMALUX_DEBUG=1)
 *   INFO ℹ   — informazione normale
 *   WARN ⚠   — avvertimento, comportamento inatteso ma non fatale
 *   ERR  ✖   — errore, operazione fallita
 */

#include <QtGlobal>
#include <QString>
#include <cstdio>

namespace PLog {

/* ══════════════════════════════════════════════════════════════
   Enumerazioni
   ══════════════════════════════════════════════════════════════ */

enum Cat : uint8_t {
    QtC  = 0,
    Py   = 1,
    Llm  = 2,
    Tool = 3,
    Mcp  = 4,
    Io   = 5,
    Net  = 6,
    Stt  = 7,
    Rag  = 8,
    Sys  = 9,
    Sec  = 10,
};

enum Lvl : uint8_t { Dbg = 0, Info = 1, Warn = 2, Err = 3 };

/* ══════════════════════════════════════════════════════════════
   Stringhe etichetta (ASCII puro, allineate a 4 char con spazi)
   ══════════════════════════════════════════════════════════════ */

inline const char* catLabel(Cat c) noexcept {
    switch (c) {
    case QtC:  return "QT  ";
    case Py:   return "PY  ";
    case Llm:  return "LLM ";
    case Tool: return "TOOL";
    case Mcp:  return "MCP ";
    case Io:   return "IO  ";
    case Net:  return "NET ";
    case Stt:  return "STT ";
    case Rag:  return "RAG ";
    case Sys:  return "SYS ";
    case Sec:  return "SEC ";
    }
    return "?   ";
}

inline const char* lvlLabel(Lvl l) noexcept {
    switch (l) {
    case Dbg:  return "DBG ";
    case Info: return "INFO";
    case Warn: return "WARN";
    case Err:  return "ERR ";
    }
    return "?   ";
}

/* Simbolo ASCII visivo per livello (fallback universale terminali) */
inline const char* lvlSymbol(Lvl l) noexcept {
    switch (l) {
    case Dbg:  return "[?]";
    case Info: return "[i]";
    case Warn: return "[!]";
    case Err:  return "[X]";
    }
    return "[ ]";
}

/* Emoji UTF-8 per terminali moderni (Linux/macOS) */
inline const char* lvlEmoji(Lvl l) noexcept {
    switch (l) {
    case Dbg:  return "\xf0\x9f\x94\x8d";           /* 🔍 */
    case Info: return "\xe2\x84\xb9 ";              /* ℹ  + spazio */
    case Warn: return "\xe2\x9a\xa0\xef\xb8\x8f";  /* ⚠️  */
    case Err:  return "\xe2\x9c\x96 ";              /* ✖  + spazio */
    }
    return "   ";
}

/* ══════════════════════════════════════════════════════════════
   API di log su console
   ══════════════════════════════════════════════════════════════ */

/* Formato riga: [CAT ][LVL ] emoji  messaggio */
inline QString buildLine(Cat c, Lvl l, const QString& msg) {
    return QString("[%1][%2] %3 %4")
        .arg(QLatin1String(catLabel(c)),
             QLatin1String(lvlLabel(l)),
             QString::fromUtf8(lvlEmoji(l)),
             msg);
}

inline void d(Cat c, const QString& msg) {
    if (!qEnvironmentVariableIsSet("PRISMALUX_DEBUG")) return;
    fprintf(stderr, "%s\n", buildLine(c, Dbg, msg).toLocal8Bit().constData());
}
inline void i(Cat c, const QString& msg) {
    fprintf(stderr, "%s\n", buildLine(c, Info, msg).toLocal8Bit().constData());
}
inline void w(Cat c, const QString& msg) {
    fprintf(stderr, "%s\n", buildLine(c, Warn, msg).toLocal8Bit().constData());
}
inline void e(Cat c, const QString& msg) {
    fprintf(stderr, "%s\n", buildLine(c, Err, msg).toLocal8Bit().constData());
}

/* ══════════════════════════════════════════════════════════════
   Formattazione per widget UI
   ══════════════════════════════════════════════════════════════ */

/* Plain text con emoji: "⚠️  [MCP] processo non avviato" */
inline QString fmt(Cat c, Lvl l, const QString& msg) {
    return QString::fromUtf8(lvlEmoji(l)) + " [" +
           QLatin1String(catLabel(c)).trimmed() + "] " + msg;
}

/* HTML colorato per QTextBrowser / log widget */
inline QString html(Cat c, Lvl l, const QString& msg) {
    const char* color = (l == Err)  ? "#ef4444" :
                        (l == Warn) ? "#f59e0b" :
                        (l == Info) ? "#60a5fa" : "#6b7280";
    return QString(
        "<span style='color:%1;font-family:monospace;font-size:11px;font-weight:600;'>"
        "[%2][%3]</span>"
        "<span style='color:%1;'> %4</span>"
        "&nbsp;%5")
        .arg(QLatin1String(color),
             QLatin1String(catLabel(c)),
             QLatin1String(lvlLabel(l)),
             QString::fromUtf8(lvlEmoji(l)),
             msg.toHtmlEscaped());
}

/* ══════════════════════════════════════════════════════════════
   Message handler automatico — categorizza i log esistenti
   ══════════════════════════════════════════════════════════════ */

namespace detail {

inline Cat guessCategory(const QString& msg) noexcept {
    /* Ordine: più specifico prima */

    /* Sicurezza */
    if (msg.contains(QLatin1String("SECURITY"))
     || msg.contains(QLatin1String("QKeychain"), Qt::CaseInsensitive)
     || msg.contains(QLatin1String("token"),     Qt::CaseInsensitive)
     || msg.contains(QLatin1String("Bearer"),    Qt::CaseSensitive))
        return Sec;

    /* MCP */
    if (msg.contains(QLatin1String("MCP"),           Qt::CaseSensitive)
     || msg.contains(QLatin1String("mcp_call"),       Qt::CaseInsensitive)
     || msg.contains(QLatin1String("main_ai_tool"),   Qt::CaseInsensitive)
     || msg.contains(QLatin1String("knowledge_mcp"),  Qt::CaseInsensitive)
     || msg.contains(QLatin1String("JSON-RPC"),       Qt::CaseSensitive)
     || msg.contains(QLatin1String("json-rpc"),       Qt::CaseSensitive))
        return Mcp;

    /* IO / Database */
    if (msg.contains(QLatin1String("GraphMemory"), Qt::CaseSensitive)
     || msg.contains(QLatin1String("SQLite"),       Qt::CaseInsensitive)
     || msg.contains(QLatin1String("[SQL]"),        Qt::CaseSensitive)
     || msg.contains(QLatin1String("SQL"),          Qt::CaseSensitive)
     || msg.contains(QLatin1String("pdftotext"),    Qt::CaseInsensitive)
     || msg.contains(QLatin1String("scrivi_file"),  Qt::CaseInsensitive)
     || msg.contains(QLatin1String("leggi_file"),   Qt::CaseInsensitive)
     || msg.contains(QLatin1String("FileInfo"),     Qt::CaseInsensitive))
        return Io;

    /* Network / LAN */
    if (msg.contains(QLatin1String("LanServer"),  Qt::CaseSensitive)
     || msg.contains(QLatin1String("TLS"),         Qt::CaseSensitive)
     || msg.contains(QLatin1String("openssl"),     Qt::CaseInsensitive)
     || msg.contains(QLatin1String("TCP"),         Qt::CaseSensitive)
     || msg.contains(QLatin1String("HTTP"),        Qt::CaseSensitive)
     || msg.contains(QLatin1String("REPL"),        Qt::CaseSensitive)
     || msg.contains(QLatin1String("fetch_url"),   Qt::CaseInsensitive)
     || msg.contains(QLatin1String("bwrap"),       Qt::CaseInsensitive))
        return Net;

    /* STT / Audio */
    if (msg.contains(QLatin1String("whisper"),      Qt::CaseInsensitive)
     || msg.contains(QLatin1String("faster-whisper"),Qt::CaseInsensitive)
     || msg.contains(QLatin1String("STT"),          Qt::CaseSensitive)
     || msg.contains(QLatin1String("VAD"),          Qt::CaseSensitive)
     || msg.contains(QLatin1String("piper"),        Qt::CaseInsensitive)
     || msg.contains(QLatin1String("cam_stream"),   Qt::CaseInsensitive)
     || msg.contains(QLatin1String("audio"),        Qt::CaseInsensitive)
     || msg.contains(QLatin1String("settings_voice"),Qt::CaseInsensitive)
     || msg.contains(QLatin1String("ImpostazioniPage"), Qt::CaseSensitive))
        return Stt;

    /* LLM / AI */
    if (msg.contains(QLatin1String("Ollama"),    Qt::CaseSensitive)
     || msg.contains(QLatin1String("AiClient"),  Qt::CaseSensitive)
     || msg.contains(QLatin1String("llama"),     Qt::CaseInsensitive)
     || msg.contains(QLatin1String("backend"),   Qt::CaseInsensitive)
     || msg.contains(QLatin1String("LLM"),       Qt::CaseSensitive))
        return Llm;

    /* RAG */
    if (msg.contains(QLatin1String("RAG"),        Qt::CaseSensitive)
     || msg.contains(QLatin1String("embedding"),  Qt::CaseInsensitive)
     || msg.contains(QLatin1String("rag_graph"),  Qt::CaseInsensitive)
     || msg.contains(QLatin1String("RagEngine"),  Qt::CaseSensitive))
        return Rag;

    /* Python */
    if (msg.contains(QLatin1String("python"),    Qt::CaseInsensitive)
     || msg.contains(QLatin1String("pip"),       Qt::CaseInsensitive)
     || msg.contains(QLatin1String("script"),    Qt::CaseInsensitive)
     || msg.contains(QLatin1String("sandbox"),   Qt::CaseInsensitive)
     || msg.contains(QLatin1String("venv"),      Qt::CaseInsensitive))
        return Py;

    /* Sistema / processi */
    if (msg.contains(QLatin1String("processo"),  Qt::CaseInsensitive)
     || msg.contains(QLatin1String("process"),   Qt::CaseInsensitive)
     || msg.contains(QLatin1String("avviato"),   Qt::CaseInsensitive)
     || msg.contains(QLatin1String("hardware"),  Qt::CaseInsensitive)
     || msg.contains(QLatin1String("docker"),    Qt::CaseInsensitive)
     || msg.contains(QLatin1String("Docker"),    Qt::CaseSensitive))
        return Sys;

    return QtC;
}

inline Lvl qtTypeToLvl(QtMsgType type) noexcept {
    switch (type) {
    case QtDebugMsg:    return Dbg;
    case QtInfoMsg:     return Info;
    case QtWarningMsg:  return Warn;
    case QtCriticalMsg:
    case QtFatalMsg:    return Err;
    }
    return Info;
}

} // namespace detail

/* ── Handler globale ───────────────────────────────────────────
 * Installato in main() con PLog::installMessageHandler().
 * Tutti i qWarning/qCritical/qDebug vengono riformattati con
 * [CAT ][LVL ] emoji  senza toccare i siti di chiamata.        */
inline void messageHandler(QtMsgType type,
                            const QMessageLogContext& /*ctx*/,
                            const QString& msg)
{
    /* Filtra debug in produzione — attiva con PRISMALUX_DEBUG=1 */
    if (type == QtDebugMsg && !qEnvironmentVariableIsSet("PRISMALUX_DEBUG"))
        return;

    const Lvl lvl = detail::qtTypeToLvl(type);
    const Cat cat = detail::guessCategory(msg);

    fprintf(stderr, "%s\n",
            buildLine(cat, lvl, msg).toLocal8Bit().constData());

    if (type == QtFatalMsg) abort();
}

inline void installMessageHandler() {
    qInstallMessageHandler(messageHandler);
}

} // namespace PLog

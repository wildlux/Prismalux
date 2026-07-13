#include "main_ai.h"
#include "main_ai_p.h"
#include "../model_processor.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
namespace P = PrismaluxPaths;
#include "../app_config.h"
#include <QElapsedTimer>
#include <QLocale>
#include <QTimer>
#include <QTextCursor>
#include <QRegularExpression>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QProcess>
#include <QTemporaryFile>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <algorithm>
#include "../widgets/formula_parser.h"
#include "../widgets/chart_widget.h"
#include "../widgets/model_combo_helper.h"
#include "dialog_agents_config.h"

/* ══════════════════════════════════════════════════════════════
   D-29 — Pre-selezione function tools per categoria: i 7 tool "pesanti"
   aggiunti da D-33 (descrizioni lunghe, schema con molte proprietà)
   vengono inclusi nella chiamata SOLO se la query corrente contiene una
   keyword della loro categoria — riduce i token fissi delle definizioni
   tool ad ogni turno, sensibile sui modelli locali. I tool "core" (calc,
   ricerca, fetch_url, file, RAG, memoria, sub-agente, MCP — quelli
   precedenti a D-33) restano sempre disponibili se abilitati dall'utente:
   sono generici/imprevedibili da keyword e le loro descrizioni sono già
   brevi, quindi filtrarli rischierebbe di rompere funzionalità esistenti
   per un guadagno minimo.
   Fallback di sicurezza: se NESSUNA categoria matcha, si includono
   comunque TUTTI i tool pesanti — evita di negare al modello un tool che
   gli serve per una formulazione imprevista dalle keyword (meglio
   qualche token in più che una funzionalità silenziosamente assente).
   ══════════════════════════════════════════════════════════════ */
static const QSet<QString> kHeavyTools = {
    "algoritmo", "codice_fiscale", "finanza_calcola", "valida_documento",
    "carta_astrale", "converti", "disegna_grafico"
};

static QSet<QString> _relevantHeavyTools(const QString& query)
{
    const QString q = query.toLower();
    struct Cat { const char* tool; QStringList keywords; };
    static const QVector<Cat> kCats = {
        { "algoritmo", {"mcd","mcm","fattorizzazione","fibonacci","catalan",
            "collatz","hanoi","pascal","nqueens","n-regine","n regine",
            "backtracking","edit distance","distanza di edit","lcs",
            "sottosequenza","kmp","inversioni","profitto massimo"} },
        { "codice_fiscale", {"codice fiscale","c.f."} },
        { "finanza_calcola", {"mutuo","prestito","rata","interesse composto",
            "tfr","ammortamento","capitalizzazione"} },
        { "valida_documento", {"iban","partita iva","p.iva","valida",
            "checksum"} },
        { "carta_astrale", {"carta astrale","oroscopo","ascendente","zodiaco",
            "segno zodiacale","tema natale","astrologia"} },
        { "converti", {"converti","conversione","grammi","cucchia","tazza",
            "fahrenheit","gas mark","ohm","km/h","mph","anno luce",
            "unit\xc3\xa0 astronomica","forno"} },
        { "disegna_grafico", {"grafico","disegna","traccia","plotta","plot","chart"} },
    };
    QSet<QString> out;
    for (const auto& cat : kCats)
        for (const QString& kw : cat.keywords)
            if (q.contains(kw)) { out.insert(QString::fromLatin1(cat.tool)); break; }
    return out.isEmpty() ? kHeavyTools : out;
}

/* ══════════════════════════════════════════════════════════════
   _buildOllamaTools — array tools in formato Ollama function calling.
   Inviato nella chat() quando m_toolsEnabled + Ollama backend + CHAT single mode.
   Il modello chiama i tool via message.tool_calls → onNativeToolCall() esegue
   e chiama replyWithTool() per continuare la conversazione.
   ══════════════════════════════════════════════════════════════ */
static QJsonArray _buildOllamaTools()
{
    auto mkTool = [](const QString& name, const QString& desc,
                     const QJsonObject& params) -> QJsonObject {
        QJsonObject fn;
        fn["name"]        = name;
        fn["description"] = desc;
        fn["parameters"]  = params;
        QJsonObject t;
        t["type"]     = "function";
        t["function"] = fn;
        return t;
    };

    auto strParam = [](const QString& desc) -> QJsonObject {
        QJsonObject p;
        p["type"]        = "object";
        QJsonObject props;
        QJsonObject val; val["type"] = "string"; val["description"] = desc;
        props["value"] = val;
        p["properties"] = props;
        p["required"]   = QJsonArray{QString("value")};
        return p;
    };

    return QJsonArray {
        /* ── Tool generici ── */
        mkTool("calc",
               "Calcola un'espressione matematica. "
               "Supporta: +,-,*,/,**, sqrt, sin, cos, log, ecc.",
               strParam("Espressione matematica da calcolare")),
        mkTool("fetch_url",
               "Scarica il contenuto HTML o testo di una pagina web dato il suo URL. "
               "Usare quando si ha un URL diretto (http:// o https://).",
               strParam("URL completa della pagina da scaricare")),
        mkTool("ricerca",
               "Cerca informazioni testuali online via DuckDuckGo Instant Answer. "
               "NON usare per URL — usa fetch_url per scaricare pagine web.",
               strParam("Query di testo da cercare")),
        mkTool("leggi_file",
               "Legge il contenuto di un file locale.",
               strParam("Percorso assoluto del file")),
        mkTool("lista_file",
               "Elenca i file in una directory locale.",
               strParam("Percorso assoluto della cartella")),
        mkTool("python",
               "Esegue codice Python in sandbox e ritorna l'output.",
               strParam("Codice Python da eseguire")),
        /* ── Tool Prismalux-specifici ── */
        mkTool("search_rag",
               "Cerca testo nei documenti RAG indicizzati localmente "
               "(cartelle ~/prismalux_rag_docs/ e Prismalux/RAG/). "
               "Usare per trovare informazioni nei tuoi documenti personali.",
               strParam("Query di ricerca nei documenti RAG locali")),
        mkTool("graph_memory",
               "Cerca entità, concetti e fatti nella memoria a grafo di Prismalux "
               "(GraphMemory SQLite). Utile per trovare informazioni apprese in sessioni precedenti.",
               strParam("Query di ricerca nella memoria a grafo")),
        mkTool("get_knowledge",
               "Legge la Knowledge Base personale dell'utente "
               "(file user_knowledge.md aggiornato automaticamente da Prismalux).",
               strParam("(lascia vuoto per leggere tutta la Knowledge Base)")),
        /* ── Sub-agente ── */
        mkTool("spawn_agent",
               "Crea un sub-agente AI specializzato che esegue un sotto-compito e restituisce il risultato. "
               "Utile per parallelizzare analisi, delegare sotto-task complessi o ottenere un secondo parere. "
               "Massimo 4 sub-agenti per sessione. "
               "Il sub-agente non ha accesso alla cronologia corrente: fornigli tutto il contesto nel campo 'task'.",
               [&]() -> QJsonObject {
                   QJsonObject p;
                   p["type"] = QLatin1String("object");
                   QJsonObject props;
                   QJsonObject role; role["type"] = QLatin1String("string");
                   role["description"] = QLatin1String("Ruolo/persona del sub-agente (es. 'Esperto di finanza', 'Analista dati', 'Revisore critico')");
                   QJsonObject task; task["type"] = QLatin1String("string");
                   task["description"] = QLatin1String("Compito completo da eseguire, incluso tutto il contesto necessario");
                   props["role"] = role;
                   props["task"] = task;
                   p["properties"] = props;
                   p["required"]   = QJsonArray{ QLatin1String("role"), QLatin1String("task") };
                   return p;
               }()),
        /* ── Algoritmi classici (D-33) ── */
        mkTool("algoritmo",
               "Esegue un algoritmo classico con parametri strutturati: MCD/MCM, "
               "fattorizzazione in fattori primi, riga del triangolo di Pascal, "
               "N-esimo numero di Fibonacci/Catalan, congettura di Collatz, mosse minime "
               "Torre di Hanoi (o l'elenco passo-passo delle mosse con hanoi_passi, N<=10), "
               "N-Regine con conteggio esatto delle soluzioni via backtracking (nqueens, "
               "N<=12), profitto massimo comprando/vendendo azioni, conteggio inversioni in "
               "una lista, posizione di un elemento in un array, distanza di edit, "
               "sottosequenza comune piu' lunga (LCS), ricerca di un pattern in un testo "
               "(KMP). Usare per formulazioni libere o multi-turno di queste domande "
               "algoritmiche.",
               [&]() -> QJsonObject {
                   QJsonObject p; p["type"] = QLatin1String("object");
                   QJsonObject props;
                   auto sp = [](const QString& d) { QJsonObject v; v["type"] = QLatin1String("string"); v["description"] = d; return v; };
                   auto np = [](const QString& d) { QJsonObject v; v["type"] = QLatin1String("number"); v["description"] = d; return v; };
                   props["nome"] = sp("Nome algoritmo: mcd, mcm, fattorizzazione, pascal, fibonacci, "
                                      "catalan, collatz, hanoi, hanoi_passi, nqueens, profitto_azioni, "
                                      "inversioni, posizione_array, edit_distance, lcs, kmp");
                   props["a"]      = np("Primo intero (mcd, mcm)");
                   props["b"]      = np("Secondo intero (mcd, mcm)");
                   props["n"]      = np("Singolo intero (fattorizzazione, pascal riga, fibonacci N, catalan N, collatz, hanoi/hanoi_passi dischi, nqueens dimensione scacchiera)");
                   props["array"]  = sp("Lista di interi separati da virgola (profitto_azioni, inversioni, posizione_array)");
                   props["target"] = np("Elemento da cercare (posizione_array)");
                   props["s1"]     = sp("Prima stringa (edit_distance, lcs)");
                   props["s2"]     = sp("Seconda stringa (edit_distance, lcs)");
                   props["pattern"]= sp("Pattern da cercare (kmp)");
                   props["testo"]  = sp("Testo in cui cercare il pattern (kmp)");
                   p["properties"] = props;
                   p["required"]   = QJsonArray{ QLatin1String("nome") };
                   return p;
               }()),
        /* ── Codice Fiscale (D-33) ── */
        mkTool("codice_fiscale",
               "Calcola il Codice Fiscale italiano (D.M. 23/12/1976) da dati anagrafici: "
               "cognome, nome, data di nascita, sesso, comune di nascita (o codice Belfiore "
               "direttamente se gia' noto).",
               [&]() -> QJsonObject {
                   QJsonObject p; p["type"] = QLatin1String("object");
                   QJsonObject props;
                   auto sp = [](const QString& d) { QJsonObject v; v["type"] = QLatin1String("string"); v["description"] = d; return v; };
                   props["cognome"]         = sp("Cognome");
                   props["nome"]            = sp("Nome");
                   props["data_nascita"]    = sp("Data di nascita in formato AAAA-MM-GG (es. 1990-03-15)");
                   props["sesso"]           = sp("'M' oppure 'F'");
                   props["comune_nascita"]  = sp("Comune (o stato estero) di nascita, per il lookup automatico del codice Belfiore");
                   props["codice_belfiore"] = sp("Codice Belfiore a 4 caratteri, se gia' noto (opzionale, ha priorita' su comune_nascita)");
                   p["properties"] = props;
                   p["required"]   = QJsonArray{ QLatin1String("cognome"), QLatin1String("nome"),
                                                  QLatin1String("data_nascita"), QLatin1String("sesso") };
                   return p;
               }()),
        /* ── Calcoli finanziari (D-33) ── */
        mkTool("finanza_calcola",
               "Esegue un calcolo finanziario: interesse composto, rata di un mutuo/prestito "
               "(ammortamento francese) o rivalutazione del TFR (Trattamento di Fine Rapporto, "
               "calcolo indicativo art. 2120 c.c.). Usare per formulazioni libere o multi-turno "
               "di queste domande finanziarie.",
               [&]() -> QJsonObject {
                   QJsonObject p; p["type"] = QLatin1String("object");
                   QJsonObject props;
                   auto sp = [](const QString& d) { QJsonObject v; v["type"] = QLatin1String("string"); v["description"] = d; return v; };
                   auto np = [](const QString& d) { QJsonObject v; v["type"] = QLatin1String("number"); v["description"] = d; return v; };
                   props["tipo"] = sp("Tipo di calcolo: interesse_composto, rata_mutuo, tfr_rivalutazione");
                   props["capitale"]              = np("Capitale iniziale in euro (interesse_composto, rata_mutuo)");
                   props["tasso_annuo_pct"]        = np("Tasso annuo in percentuale (interesse_composto, rata_mutuo)");
                   props["anni"]                   = np("Durata in anni (tutti i tipi)");
                   props["stipendio_lordo_annuo"]  = np("Stipendio lordo annuo in euro (tfr_rivalutazione)");
                   props["inflazione_pct"]         = np("Inflazione media annua in percentuale (tfr_rivalutazione)");
                   p["properties"] = props;
                   p["required"]   = QJsonArray{ QLatin1String("tipo"), QLatin1String("anni") };
                   return p;
               }()),
        /* ── Validazione documenti (D-33) ── */
        mkTool("valida_documento",
               "Valida un IBAN (cifra di controllo mod-97), una Partita IVA italiana "
               "(checksum ufficiale a 11 cifre) o un Codice Fiscale italiano (checksum "
               "D.M.1976, con decodifica di data di nascita e sesso se valido).",
               [&]() -> QJsonObject {
                   QJsonObject p; p["type"] = QLatin1String("object");
                   QJsonObject props;
                   auto sp = [](const QString& d) { QJsonObject v; v["type"] = QLatin1String("string"); v["description"] = d; return v; };
                   props["tipo"]   = sp("Tipo di documento: iban, partita_iva, codice_fiscale");
                   props["valore"] = sp("Il valore da validare (IBAN, P.IVA a 11 cifre o Codice Fiscale a 16 caratteri)");
                   p["properties"] = props;
                   p["required"]   = QJsonArray{ QLatin1String("tipo"), QLatin1String("valore") };
                   return p;
               }()),
        /* ── Carta astrale (D-33) ── */
        mkTool("carta_astrale",
               "Calcola le posizioni planetarie astrologiche (carta natale: Sole, Luna, "
               "pianeti, Ascendente, Medio Cielo) per una data, ora e luogo di nascita, "
               "con algoritmi astronomici reali (Meeus).",
               [&]() -> QJsonObject {
                   QJsonObject p; p["type"] = QLatin1String("object");
                   QJsonObject props;
                   auto sp = [](const QString& d) { QJsonObject v; v["type"] = QLatin1String("string"); v["description"] = d; return v; };
                   auto np = [](const QString& d) { QJsonObject v; v["type"] = QLatin1String("number"); v["description"] = d; return v; };
                   props["data"] = sp("Data di nascita in formato AAAA-MM-GG (es. 1990-03-15)");
                   props["ora"]  = sp("Ora di nascita locale in formato HH:MM, 24 ore (es. 14:30)");
                   props["lat"]  = np("Latitudine del luogo di nascita in gradi decimali (Nord positivo)");
                   props["lon"]  = np("Longitudine del luogo di nascita in gradi decimali (Est positivo)");
                   p["properties"] = props;
                   p["required"]   = QJsonArray{ QLatin1String("data"), QLatin1String("ora"),
                                                  QLatin1String("lat"), QLatin1String("lon") };
                   return p;
               }()),
        /* ── Conversioni scienza/cucina (D-33) ── */
        mkTool("converti",
               "Converte un valore tra unità di misura scientifiche o da cucina: legge di Ohm "
               "(V/I/R), velocità (km/h, m/s, mph), temperatura del forno (Celsius, Fahrenheit, "
               "Gas Mark), ingredienti da cucina (ml/grammi per farina/zucchero/acqua/ecc., "
               "cucchiaino/cucchiaio/tazza), distanze astronomiche (anni luce, unità "
               "astronomiche, km). Descrivere la conversione in linguaggio naturale con unità "
               "esplicite nel campo 'richiesta'.",
               [&]() -> QJsonObject {
                   QJsonObject p; p["type"] = QLatin1String("object");
                   QJsonObject props;
                   QJsonObject r; r["type"] = QLatin1String("string");
                   r["description"] = QLatin1String(
                       "Richiesta di conversione in linguaggio naturale con unità esplicite, "
                       "es. \"200 ml di farina in grammi\", \"180 gradi forno in fahrenheit\", "
                       "\"100 km/h in mph\", \"2 anni luce in km\"");
                   props["richiesta"] = r;
                   p["properties"] = props;
                   p["required"]   = QJsonArray{ QLatin1String("richiesta") };
                   return p;
               }()),
        /* ── Disegna grafico (D-33) ── */
        mkTool("disegna_grafico",
               "Traccia il grafico di una funzione y=f(x) e lo apre nel pannello Grafico "
               "della chat. Usare quando l'utente chiede esplicitamente un grafico con "
               "formula e/o intervallo x specificati separatamente (non la formula grezza "
               "nella frase, gia' intercettata prima da una guardia piu' rapida).",
               [&]() -> QJsonObject {
                   QJsonObject p; p["type"] = QLatin1String("object");
                   QJsonObject props;
                   QJsonObject formula; formula["type"] = QLatin1String("string");
                   formula["description"] = QLatin1String("Espressione in x da tracciare, es. \"sin(x)*2\", \"x^2 - 4\"");
                   QJsonObject xmin; xmin["type"] = QLatin1String("number");
                   xmin["description"] = QLatin1String("Limite inferiore di x (default -10)");
                   QJsonObject xmax; xmax["type"] = QLatin1String("number");
                   xmax["description"] = QLatin1String("Limite superiore di x (default 10)");
                   props["formula"] = formula;
                   props["xmin"]    = xmin;
                   props["xmax"]    = xmax;
                   p["properties"] = props;
                   p["required"]   = QJsonArray{ QLatin1String("formula") };
                   return p;
               }()),
        /* ── Plugin MCP ── */
        mkTool("mcp_call",
               "Invoca un tool di un plugin MCP attivo in Prismalux. "
               "I plugin disponibili sono in MCPs/ (es. anki_mcp, ollama_mcp, devagent_mcp, knowledge_mcp). "
               "Usare per accedere a funzionalita' avanzate non coperte dagli altri tool built-in.",
               [&]() -> QJsonObject {
                   QJsonObject p; p["type"] = QLatin1String("object");
                   QJsonObject props;
                   QJsonObject plugin;   plugin["type"]    = QLatin1String("string");
                                         plugin["description"] = QLatin1String("Nome del plugin MCP (es. anki_mcp, ollama_mcp, knowledge_mcp)");
                   QJsonObject toolName; toolName["type"]  = QLatin1String("string");
                                         toolName["description"] = QLatin1String("Nome del tool da invocare nel plugin");
                   QJsonObject argsJson; argsJson["type"]  = QLatin1String("string");
                                         argsJson["description"] = QLatin1String("Argomenti come stringa JSON (es. {\"query\":\"...\"}); usa {} se nessun argomento");
                   props["plugin"]    = plugin;
                   props["tool_name"] = toolName;
                   props["args_json"] = argsJson;
                   p["properties"] = props;
                   p["required"]   = QJsonArray{ QLatin1String("plugin"), QLatin1String("tool_name") };
                   return p;
               }()),
    };
}

/* ══════════════════════════════════════════════════════════════
   Cache risposte esatte (D-25) — hash della query normalizzata →
   risposta AI precedente. Query ripetute identiche = zero token,
   con link "🔄 Rigenera" nella bolla per bypassare e richiamare
   davvero il modello. Solo per chat singola (numAgents<=1): la
   pipeline multi-agente e il Byzantino non passano da qui.
   Persistita in ~/.prismalux/response_cache.json, cap a
   kResponseCacheMax voci (evict le più vecchie per timestamp).
   ══════════════════════════════════════════════════════════════ */
static const int kResponseCacheMax = 300;

static QString _responseCachePath()
{
    return QDir::homePath() + "/.prismalux/response_cache.json";
}

/* Normalizzazione minima: solo per il confronto "stessa domanda",
 * non tocca il testo effettivamente inviato al modello. */
static QString _normalizeForCache(const QString& text)
{
    QString s = text.trimmed().toLower();
    s.replace(QRegularExpression("\\s+"), " ");
    return s;
}

static QString _cacheKeyHash(const QString& task)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        _normalizeForCache(task).toUtf8(), QCryptographicHash::Sha256).toHex());
}

/* Ritorna la risposta cachata per @p task, o stringa vuota se assente
 * o se il file di cache non esiste/è corrotto (fallback silenzioso:
 * un cache-miss è sempre sicuro, si procede normalmente col modello). */
static QString _lookupResponseCache(const QString& task)
{
    QFile f(_responseCachePath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return {};
    const QJsonObject entries = doc.object().value("entries").toObject();
    return entries.value(_cacheKeyHash(task)).toObject().value("response").toString();
}

/* Salva/aggiorna la voce di cache per @p task → @p response. Query oltre
 * i 500 caratteri non vengono cachate: più lunga la domanda, più bassa
 * la probabilità che si ripeta identica — non vale il costo di lettura/
 * scrittura del file ad ogni turno. */
static void _saveResponseCache(const QString& task, const QString& response)
{
    if (task.trimmed().isEmpty() || response.trimmed().isEmpty()) return;
    if (task.length() > 500) return;

    const QString path = _responseCachePath();
    QDir().mkpath(QDir::homePath() + "/.prismalux");
    QJsonObject root;
    {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) root = doc.object();
        }
    }
    QJsonObject entries = root.value("entries").toObject();

    QJsonObject entry;
    entry["query"]    = task.left(300);
    entry["response"] = response;
    entry["ts"]       = QDateTime::currentSecsSinceEpoch();
    entries[_cacheKeyHash(task)] = entry;

    /* Evict le voci più vecchie se si supera il limite */
    if (entries.size() > kResponseCacheMax) {
        QVector<QPair<qint64, QString>> byAge;
        for (auto it = entries.begin(); it != entries.end(); ++it)
            byAge << qMakePair(static_cast<qint64>(it.value().toObject().value("ts").toDouble()), it.key());
        std::sort(byAge.begin(), byAge.end());
        const int toRemove = entries.size() - kResponseCacheMax;
        for (int i = 0; i < toRemove; ++i) entries.remove(byAge[i].second);
    }

    root["entries"] = entries;
    QFile out(path);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        out.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

/* ══════════════════════════════════════════════════════════════
   _isChartRequest — rileva intento grafico nel linguaggio naturale.
   Controlla parole chiave italiane e inglesi comunemente usate quando
   l'utente vuole un grafico, senza richiedere una formula parsabile.
   ══════════════════════════════════════════════════════════════ */
static bool _isChartRequest(const QString& task) {
    const QString t = task.toLower();
    static const QStringList kKeywords = {
        "grafico", "grafici", "disegna", "plotta", "visualizza",
        "chart", "plot", "draw", "graph",
        "curva", "curve", "andamento", "trend", "barchart", "istogramma",
        "scatter", "dispersione", "mostra.*dati", "mostra.*numeri",
        "crea.*grafico", "fai.*grafico", "fammi.*grafico", "genera.*grafico",
        "mostrami.*grafico", "voglio.*grafico"
    };
    static const QRegularExpression re(kKeywords.join("|"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re.match(t).hasMatch();
}

/* ══════════════════════════════════════════════════════════════
   _showQrEventoBubble — se 'result' è un QR_EVENTO_JSON del tool
   crea_evento_calendario, inserisce la bolla con le immagini QR e
   ritorna true; false per qualunque altro risultato. Usata sia dal
   percorso tool della pipeline sia dalla guardia locale Evento.
   ══════════════════════════════════════════════════════════════ */
bool AgentiPage::_showQrEventoBubble(const QString& result)
{
    static const QString kQrTag = "QR_EVENTO_JSON:";
    if (!result.startsWith(kQrTag)) return false;

    const QJsonObject o = QJsonDocument::fromJson(
        result.mid(kQrTag.length()).toUtf8()).object();
    const QJsonArray pngs   = o["pngs"].toArray();
    const QJsonArray labels = o["labels"].toArray();
    const QString testo     = o["testo"].toString();
    const QString icsFile   = o["ics_file"].toString();

    const auto& c = bc();
    /* Affiancati (uno a sinistra, uno a destra) con una <table>, non <div
       style='display:inline-block'>: il motore rich-text di Qt (QTextEdit)
       non supporta inline-block sui div — li tratta come blocchi con
       ritorno a capo forzato, mandando i due QR uno sopra l'altro invece
       che fianco a fianco. Le celle di tabella invece funzionano bene. */
    // Dimensione per immagine in base a quanti QR ci sono: con 2 (il caso
    // più comune, Google+ics) 210px resta comodo da scansionare da monitor;
    // con 3 (Google https + Google Android intent + ics, formato "entrambi"
    // dopo l'aggiunta della variante Android) 170px evita che la riga
    // trabocchi restando comunque leggibile.
    const int qrPx = pngs.size() >= 3 ? 170 : 210;
    /* cellspacing (non padding sul singolo <td>) per lo spazio TRA i QR:
       a schermo 18px sembrava un gap ragionevole, ma inquadrando con la
       fotocamera del telefono da una certa distanza il mirino dello
       scanner include comunque il QR affiancato e legge quello sbagliato
       (segnalato da Paolo) — serve un margine molto più ampio di quanto
       basterebbe solo esteticamente. */
    QString imgsHtml = "<table cellpadding='0' cellspacing='60'><tr>";
    for (int i = 0; i < pngs.size(); ++i) {
        const QString path  = pngs[i].toString();
        const QString label = i < labels.size() ? labels[i].toString() : QString();
        imgsHtml += "<td style='text-align:center;padding:6px 0 0 0;'>"
            "<img src='" + QUrl::fromLocalFile(path).toString() + "' width='" + QString::number(qrPx)
            + "' height='" + QString::number(qrPx) + "'>"
            "<div style='font-size:11px;color:" + c.lHdr + ";margin-top:2px;'>" + label.toHtmlEscaped() + "</div>"
            "</td>";
    }
    imgsHtml += "</tr></table>";
    const int br = AppConfig::s().value(P::SK::kBubbleRadius, 10).toInt();
    m_log->moveCursor(QTextCursor::End);
    m_log->insertHtml(
        "<p style='margin:6px 0;'></p>"
        "<table width='100%' cellpadding='0' cellspacing='0'><tr><td style='"
            "background-color:" + QString(c.lBg) + ";border:1px solid " + c.lBdr + ";"
            "border-radius:" + QString::number(br) + "px;padding:10px 14px;color:" + c.lTxt + ";'>"
            "<p style='color:" + c.lHdr + ";font-size:11px;font-weight:bold;margin:0 0 8px 0;'>"
                "\xf0\x9f\x93\x85&nbsp;Evento calendario</p>"
            "<div>" + imgsHtml + "</div>"
            "<p style='font-size:13px;margin:8px 0 0 0;color:" + c.lRes + ";'>" + testo.toHtmlEscaped() + "</p>"
            + (icsFile.isEmpty() ? QString() :
               "<p style='font-size:11px;margin:6px 0 0 0;color:" + QString(c.lHdr) + ";'>"
               "\xf0\x9f\x92\xbe&nbsp;File di riserva salvato qui: " + icsFile.toHtmlEscaped()
               + " — se nessun QR funziona sul tuo telefono, invialo a te stesso "
                 "(Telegram, email, chiavetta) e aprilo: il telefono lo riconoscerà "
                 "come evento calendario.</p>")
        + "</td></tr></table><p style='margin:4px 0;'></p>");
    m_input->clear();
    emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
    return true;
}

/* ══════════════════════════════════════════════════════════════
   Pipeline sequenziale
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runPipeline() {
    m_userScrolled = false;  /* nuovo task: torna in auto-scroll */
    m_taskHtml = extractInputHtml(m_input); /* HTML leggero bolla utente — prima di clear() */
    const QString rawInput = m_input->toPlainText().trimmed();
    QString task = correctGuardTypos(_sanitize_prompt(rawInput));
    const QString& taskHtml = m_taskHtml;
    if (task.isEmpty()) { m_log->append("\xe2\x9a\xa0  Inserisci un task."); return; }

    /* Reset history se il log e' stato svuotato (es. "Nuova chat") */
    if (m_log->document()->isEmpty() || m_log->toPlainText().trimmed().isEmpty())
        m_ctxSingle->clear();

    /* Avviso se l'estrazione asincrona del file allegato non è ancora completata */
    if (m_docLoading) {
        m_log->append("\xe2\x8f\xb3  Attendi: estrazione del documento in corso...");
        return;
    }

    /* Grafici AI: pipeline risponde normalmente, tryShowChart() mostra inline.
       Il pulsante "Apri nel Grafico" nel panel consente di spostare poi. */

    {
        QElapsedTimer tmr; tmr.start();
        QString ris = guardiaMath(task);
        if (ris.isEmpty()) ris = guardiaDataOra(task);
        double ms = tmr.nsecsElapsed() / 1e6;
        if (!ris.isEmpty()) {
            m_log->clear();
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildUserBubble(task, i, taskHtml)); }
            m_log->append("");
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = ris;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildLocalBubble(ris, ms, i)); }
            m_input->clear();
            emit chatCompleted(task.left(40), m_log->toHtml());
            return;
        }
    }

    /* ── Guardia Grafico: formula rilevata → grafico locale, zero token AI ── */
    {
        const QString expr = FormulaParser::tryExtract(task);
        if (!expr.isEmpty()) {
            FormulaParser fp(expr);
            if (fp.ok()) {
                double xMin = -10.0, xMax = 10.0;
                FormulaParser::tryExtractXRange(task, xMin, xMax);
                const auto pts = fp.sample(xMin, xMax, 400);
                if (!pts.isEmpty()) {
                    /* In CHAT RAG i messaggi si accumulano (non clear); in Pipeline si resetta */
                    if (m_cfgDlg->numAgents() <= 1) {
                        /* CHAT RAG: non cancellare i messaggi precedenti */
                    } else {
                        m_log->clear();
                    }
                    m_taskOriginal = task;
                    { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
                      m_log->moveCursor(QTextCursor::End);
                      m_log->insertHtml(buildUserBubble(task, i, taskHtml)); }
                    m_log->append("");
                    /* Bolla chart: solo pulsante "Mostra grafico" senza testo formula */
                    {
                        const auto& _c = bc();
                        const int br = AppConfig::s().value(P::SK::kBubbleRadius,10).toInt();
                        const QString brs = QString::number(br) + "px";
                        const QString chartHtml =
                            "<p style='margin:6px 0;'></p>"
                            "<table width='100%' cellpadding='0' cellspacing='0'>"
                            "<tr>"
                              "<td bgcolor='" + QString(_c.lBg) + "' style='"
                                  "border:1px solid " + _c.lBdr + ";"
                                  "border-radius:" + brs + ";"
                                  "padding:10px 14px;"
                                  "color:" + _c.lTxt + ";"
                              "'>"
                                "<p style='color:" + _c.lHdr + ";font-size:11px;font-weight:bold;"
                                    "margin:0 0 8px 0;'>"
                                  "\xe2\x9a\xa1  Risposta locale  \xc2\xb7\xc2\xb7  0 token  \xc2\xb7\xc2\xb7  &lt;1 ms"
                                "</p>"
                                "<hr style='border:none;border-top:1px solid " + _c.lHr + ";margin:5px 0 8px 0;'>"
                                "<p align='center' style='margin:4px 0 8px 0;'>"
                                  "<a href='chart:show'"
                                     " style='color:" + _c.lBtn + ";font-size:14px;font-weight:bold;"
                                             "text-decoration:none;background:" + _c.lRes + ";"
                                             "border:1px solid " + _c.lBdr + ";"
                                             "border-radius:6px;padding:6px 20px;'>"
                                    "\xf0\x9f\x93\x88  Mostra grafico"
                                  "</a>"
                                "</p>"
                              "</td>"
                              "<td width='30'>&nbsp;</td>"
                            "</tr>"
                            "</table>"
                            "<p style='margin:4px 0;'></p>";
                        m_log->moveCursor(QTextCursor::End);
                        m_log->insertHtml(chartHtml);
                    }
                    tryShowChart(task);
                    m_input->clear();
                    emit chatCompleted(task.left(40), m_log->toHtml());
                    return;
                }
            }
        }
    }

    /* ── Guardia Help: "cosa sai fare?" → tabella Markdown statica delle
       domande rapide zero-LLM, zero token AI. Contenuto multi-riga con
       tabella: passa da markdownToHtml() (non buildLocalBubble(), che
       tratta il testo come plain text ed escaperebbe le pipe). ── */
    {
        const QString helpMd = _inject_help(task);
        static const QString kHelpTag = "HELP_MARKDOWN:";
        if (helpMd.startsWith(kHelpTag)) {
            QString html = markdownToHtml(helpMd.mid(kHelpTag.length()));
            /* Segnaposto {{PROVA:comando}} → link cliccabile che inserisce il
               comando nella casella e lo invia (handler "prova:" in
               onLogAnchorClicked). Sostituito qui, DOPO markdownToHtml: un
               <a> scritto direttamente nel markdown verrebbe escapato. */
            static const QRegularExpression reProva(R"(\{\{PROVA:([^}]+)\}\})");
            QRegularExpressionMatch mp;
            while ((mp = reProva.match(html)).hasMatch()) {
                const QString b64 = QString::fromLatin1(
                    mp.captured(1).toUtf8().toBase64(
                        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
                html.replace(mp.capturedStart(), mp.capturedLength(),
                    "<a href='prova:" + b64 + "' style='color:#3b82f6;"
                    "text-decoration:none;font-weight:bold;'>\xe2\x96\xb6 Prova</a>");
            }
            const auto& c = bc();
            const int br = AppConfig::s().value(P::SK::kBubbleRadius, 10).toInt();
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildUserBubble(task, i, taskHtml)); }
            m_log->append("");
            m_log->moveCursor(QTextCursor::End);
            m_log->insertHtml(
                "<table width='100%' cellpadding='0' cellspacing='0'><tr><td style='"
                    "background-color:" + QString(c.lBg) + ";border:1px solid " + QString(c.lBdr) + ";"
                    "border-radius:" + QString::number(br) + "px;padding:10px 14px;color:" + QString(c.lTxt) + ";'>"
                    "<p style='color:" + QString(c.lHdr) + ";font-size:11px;font-weight:bold;margin:0 0 6px 0;'>"
                        "\xe2\x9a\xa1&nbsp;Risposta locale &middot;&middot; 0 token</p>"
                    + html +
                "</td></tr></table><p style='margin:4px 0;'></p>");
            m_input->clear();
            emit chatCompleted(task.left(40), m_log->toHtml());
            return;
        }
    }

    /* ── Guardia Evento calendario: "creami un evento X il GG/MM alle HH" →
       parsing locale + tool crea_evento_calendario (sincrono) → QR in chat,
       zero LLM. Se manca la data risponde in locale chiedendola, con un
       esempio pronto da cliccare (stesso link "prova:" della tabella help). ── */
    {
        const QJsonObject ev = _parseEventoRequest(task);
        if (!ev.isEmpty()) {
            m_taskOriginal = task;
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildUserBubble(task, i, taskHtml)); }
            m_log->append("");
            if (ev.contains("data")) {
                QJsonObject call;
                call["tool"]  = QString("crea_evento_calendario");
                call["input"] = QString::fromUtf8(
                    QJsonDocument(ev).toJson(QJsonDocument::Compact));
                runToolCall(call, [this](const QString& result) {
                    if (_showQrEventoBubble(result)) return;
                    /* messaggio del tool (es. data non valida) → bolla locale */
                    int i = m_bubbleIdx++; m_bubbleTexts[i] = result;
                    m_log->moveCursor(QTextCursor::End);
                    m_log->insertHtml(buildLocalBubble(result, 0.0, i));
                    m_input->clear();
                    emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
                });
            } else {
                const QString esempio = "creami un evento "
                    + ev["titolo"].toString().toLower() + " il "
                    + QDate::currentDate().addDays(7).toString("dd/MM/yyyy")
                    + " dalle 21 alle 23";
                const QString b64 = QString::fromLatin1(esempio.toUtf8().toBase64(
                    QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
                const QString ris =
                    "Per generare il QR dell'evento \"" + ev["titolo"].toString()
                    + "\" serve almeno la DATA (e volendo orario e luogo).\n"
                      "Esempio: \xc2\xab" + esempio + "\xc2\xbb";
                int i = m_bubbleIdx++; m_bubbleTexts[i] = ris;
                m_log->moveCursor(QTextCursor::End);
                m_log->insertHtml(buildLocalBubble(ris, 0.0, i,
                    "<a href='prova:" + b64 + "' style='color:#3b82f6;"
                    "text-decoration:none;font-weight:bold;'>\xe2\x96\xb6 Prova con l'esempio</a>"));
                m_input->clear();
                emit chatCompleted(task.left(40), m_log->toHtml());
            }
            return;
        }
    }

    /* ── Guardia Calcoli Fisici/Date: _inject_science o _inject_date_calc hanno
       trovato una conversione nota (CV↔W, km↔mi, kg↔lb, Ohm, "quanti mesi
       mancano a...", ecc.) → risposta locale, zero token AI. I modelli piccoli
       sbagliano quasi sempre l'aritmetica di calendario, quindi per le date
       la via locale non è solo più veloce ma anche l'unica affidabile. ── */
    {
        QString injected = _inject_science(task);
        static const QString kTag = "[Calcolo locale:";
        if (!injected.startsWith(kTag)) injected = _inject_date_calc(task);
        if (!injected.startsWith(kTag)) injected = _inject_finance(task);
        if (!injected.startsWith(kTag)) injected = _inject_knowledge(task);
        if (!injected.startsWith(kTag)) injected = _inject_generator(task);
        if (!injected.startsWith(kTag)) injected = _inject_textstats(task);
        if (!injected.startsWith(kTag)) injected = _inject_algo(task);
        if (injected.startsWith(kTag)) {
            const int close = injected.indexOf(']');
            const QString calcResult = close > 0
                ? injected.mid(kTag.length(), close - kTag.length()).trimmed()
                : injected.left(injected.indexOf('\n')).trimmed();
            QElapsedTimer tmr; tmr.start();
            const double ms = tmr.nsecsElapsed() / 1e6;
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildUserBubble(task, i, taskHtml)); }
            m_log->append("");
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = calcResult;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildLocalBubble(calcResult, ms, i)); }
            m_input->clear();
            emit chatCompleted(task.left(40), m_log->toHtml());
            return;
        }
    }

    /* ── Guardia Cache risposte esatte (D-25) — solo chat singola:
       pipeline multi-agente/Byzantino hanno semantiche diverse (più
       agenti, contesto RAG condiviso) e non passano da questo fast-path.
       Bypassata una volta dal link "🔄 Rigenera" (vedi onLogAnchorClicked,
       "regen:") — altrimenti richiederebbe sempre la stessa risposta. ── */
    if (m_cfgDlg->numAgents() <= 1) {
        if (m_bypassResponseCache) {
            m_bypassResponseCache = false;
        } else {
            const QString cached = _lookupResponseCache(task);
            if (!cached.isEmpty()) {
                QElapsedTimer tmr; tmr.start();
                m_taskOriginal = task;
                const int userIdx = m_bubbleIdx++;
                m_bubbleTexts[userIdx] = task;
                m_log->moveCursor(QTextCursor::End);
                m_log->insertHtml(buildUserBubble(task, userIdx, taskHtml));
                m_log->append("");
                const double ms = tmr.nsecsElapsed() / 1e6;
                const int respIdx = m_bubbleIdx++;
                m_bubbleTexts[respIdx] = cached;
                const QString b64task = task.left(4096).toUtf8().toBase64(
                    QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
                const auto& c = bc();
                const QString linkStyle = QString(
                    "color:%1;font-size:12px;text-decoration:none;"
                    "border:1px solid %2;padding:2px 10px;background:%3;")
                    .arg(c.lBtnC, c.lBtnB, c.lBtn);
                const QString regenLink =
                    "<a href='regen:" + QString::number(userIdx) + ":" + b64task + "' style='"
                    + linkStyle + "' title='Ignora la cache: richiedi una nuova risposta al modello'>"
                    "&#128260; Rigenera</a>";
                m_log->moveCursor(QTextCursor::End);
                m_log->insertHtml(buildLocalBubble(cached, ms, respIdx, regenLink));
                m_input->clear();
                emit chatCompleted(task.left(40), m_log->toHtml());
                return;
            }
        }
    }

    int count = 0;
    for (int i = 0; i < MAX_AGENTS; i++)
        if (m_cfgDlg->enabledChk(i)->isChecked()) count++;
    if (count == 0) { m_log->append("\xe2\x9a\xa0  Abilita almeno un agente."); return; }

    /* Aggiunge il contesto documento se allegato, poi lo svuota (usa-e-getta) */
    if (!m_docContext.isEmpty()) {
        task += "\n\n--- DOCUMENTO ALLEGATO ---\n" + m_docContext.left(8000);
        m_docContext.clear();
        if (m_input)
            m_input->setPlaceholderText(tr("Scrivi un task o una domanda..."));
    }
    m_taskOriginal  = _inject_random(_inject_algo(_inject_textstats(_inject_generator(_inject_finance(
                          _inject_math(_inject_science(_inject_date_calc(task))))))));
    m_agentOutputs.clear();
    m_spawnedAgents = 0;
    m_currentAgent  = 0;
    m_maxShots      = m_cfgDlg->numAgents();
    m_toolIteration = 0;
    m_opMode       = OpMode::Pipeline;

    for (int i = 0; i < MAX_AGENTS; i++)
        m_cfgDlg->enabledChk(i)->setStyleSheet("");
    emit pipelineStatus(0, "Avvio pipeline...");

    /* NON si cancella il log: le Q&A si accumulano nella stessa chat */
    /* Bolla utente — mostra il testo originale (senza prefissi inject_science/math) */
    { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
      m_log->moveCursor(QTextCursor::End);
      m_log->insertHtml(buildUserBubble(task, i, taskHtml)); }
    m_log->append("");

    /* Thunk collassabile: se la normalizzazione ha modificato il testo, mostra cosa è stato inviato */
    if (task != rawInput && !task.isEmpty()) {
        const int tidx = m_thunkIdx++;
        m_thunkTexts[tidx] = task;
        m_thunkOpen.remove(tidx);
        m_log->moveCursor(QTextCursor::End);
        m_log->insertHtml(buildThunkHtml(tidx, task, false));
        m_log->append("");
    }

    /* ── Controllo RAM e dimensione modello pre-pipeline ── */
    if (!checkRam()) return;
    if (!checkModelSize(m_ai->model())) return;

    _setRunBusy(true);
    m_waitLbl->setVisible(true);
    m_input->clear();

    advancePipeline();
}

void AgentiPage::advancePipeline() {
    while (m_currentAgent < MAX_AGENTS && !m_cfgDlg->enabledChk(m_currentAgent)->isChecked())
        m_currentAgent++;

    if (m_currentAgent >= MAX_AGENTS || m_currentAgent >= m_maxShots) {
        /* Rilevamento formula → mostra grafico se presente */
        if (!m_agentOutputs.isEmpty())
            tryShowChart(m_taskOriginal + "\n" + m_agentOutputs.last());

        /* P5 — Estrattore nascosto: aggiorna user_knowledge.md a fine risposta.
           - Pipeline multi-agente: si attiva se ≥2 agenti hanno prodotto output.
           - CHAT RAG singolo: si attiva ogni kChatExtractEvery scambi (default 4). */
        {
            QSettings s("Prismalux", "GUI");
            const bool injectOn = s.value(P::SK::kInjectUserKnowledge, true).toBool();
            const int  filled   = std::count_if(m_agentOutputs.begin(), m_agentOutputs.end(),
                [](const QString& o){ return !o.trimmed().isEmpty(); });

            /* Contatore scambi singolo (incrementato prima della verifica soglia) */
            if (!m_modePipeline && filled == 1)
                m_singleChatTurns++;

            const bool doPipeline = (filled >= 2);
            const bool doChat     = (!m_modePipeline && filled == 1
                                     && m_singleChatTurns >= kChatExtractEvery);

            if (injectOn && (doPipeline || doChat)) {
                if (doChat) m_singleChatTurns = 0; /* reset dopo ogni estrazione */
                runKnowledgeExtract();   /* cambia opMode → KnowledgeExtract */
                return;                  /* onFinished gestirà la chiusura */
            }
        }

        /* rimosso: check kUncertainPhrases ridondante — reNonSo in _finishedPipeline
           è più preciso e include guardia falsi positivi (TASK-1 LLM) */

        /* Conversazione vocale continua: auto-TTS risposta singolo agente */
        if (m_voiceLoopActive && !m_modePipeline && !m_agentOutputs.isEmpty()) {
            QString resp = m_agentOutputs.last().trimmed();
            QStringList words = resp.split(' ', Qt::SkipEmptyParts);
            if (words.size() > 400) words = words.mid(0, 400);
            const QString ttsText = words.join(" ");
            if (!ttsText.isEmpty())
                QTimer::singleShot(200, this, [this, ttsText]{ _ttsPlay(ttsText); });
        }

        /* Salva turno in history con Headroom (solo chat singola) */
        if (m_maxShots == 1 && !m_taskOriginal.isEmpty() && !m_agentOutputs.isEmpty()) {
            m_ctxSingle->appendPair(m_taskOriginal, m_agentOutputs.last());
            _saveResponseCache(m_taskOriginal, m_agentOutputs.last()); /* D-25 */
        }
        _emitContextUsage();   /* aggiorna indicatore 🧠 nell'header */

        emit pipelineStatus(100, "\xe2\x9c\x85  Lavoro completato");
        _setRunBusy(false);
        m_opMode = OpMode::Idle;

        /* Hermes: memorizza la conversazione al termine */
        if (m_hermesEnabled && !m_taskOriginal.isEmpty() && !m_agentOutputs.isEmpty())
            hermesStoreConversation(m_taskOriginal, m_agentOutputs.last());

        emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
        return;
    }

    /* Check RAM inter-agente: non blocca con dialog, interrompe silenziosamente */
    if (m_currentAgent > 0) {
        QFile minfo("/proc/meminfo");
        if (minfo.open(QIODevice::ReadOnly)) {
            long long total_kb = 0, avail_kb = 0;
            while (!minfo.atEnd()) {
                const QByteArray line = minfo.readLine().trimmed();
                if (line.startsWith("MemTotal:"))
                    total_kb = line.split(' ').last().trimmed().toLongLong();
                else if (line.startsWith("MemAvailable:"))
                    avail_kb = line.split(' ').last().trimmed().toLongLong();
            }
            minfo.close();
            const double ramPct = (total_kb > 0)
                ? 100.0 * (total_kb - avail_kb) / total_kb : 0.0;
            if (ramPct >= 92.0) {
                m_log->append(QString(
                    "\xe2\x9a\xa0  <b>RAM al %1%</b> \xe2\x80\x94 pipeline interrotta "
                    "per proteggere il sistema. Chiudi altre applicazioni o scarica il modello.")
                    .arg(ramPct, 0, 'f', 0));
                emit pipelineStatus(-1, "");
                return;
            }
        }
    }

    int total = 0, done = 0;
    for (int i = 0; i < MAX_AGENTS; i++)
        if (m_cfgDlg->enabledChk(i)->isChecked()) total++;
    for (int i = 0; i < m_currentAgent; i++)
        if (m_cfgDlg->enabledChk(i)->isChecked()) done++;
    int pct = (total > 0) ? (done * 100 / total) : 0;

    auto roleList = AgentsConfigDialog::roles();
    int roleIdx = m_cfgDlg->roleCombo(m_currentAgent)->currentIndex();
    if (roleIdx < 0 || roleIdx >= roleList.size()) roleIdx = 0;
    QString roleName = roleList[roleIdx].icon + " " + roleList[roleIdx].name;

    m_tokenCount = 0;
    emit pipelineStatus(pct, QString("\xe2\x9a\x99\xef\xb8\x8f  Agente %1/%2 \xe2\x80\x94 %3...")
        .arg(done + 1).arg(total).arg(roleName));

    runAgent(m_currentAgent);
}

/* ── D-27: routing automatico dominio→modello ─────────────────────────────
 * Nucleo puro/testabile (nessuna dipendenza da widget/UI): dato un elenco
 * di modelli installati e il dominio già rilevato, sceglie il modello più
 * adatto o ritorna 'fallback' invariato. Separata da _routedModel() (che
 * legge lo stato reale della UI) per poter testare la logica senza dover
 * costruire un'intera AgentiPage. Priorità: immagine allegata → modello
 * vision (senza, la richiesta fallirebbe comunque); altrimenti dominio
 * "codice" → modello coder. Dominio generale/STEM: nessun routing, il
 * "modello leggero" dell'idea originale è già quello scelto manualmente. */
QString _pickRoutedModel(bool autoRoutingEnabled, bool hasImage,
                          AiClient::QueryDomain domain,
                          const QStringList& installedModels,
                          const QString& fallback)
{
    if (!autoRoutingEnabled) return fallback;

    auto findByCap = [&](bool (*capCheck)(const QString&)) -> QString {
        for (const QString& name : installedModels)
            if (!name.isEmpty() && capCheck(name)) return name;
        return QString();
    };

    if (hasImage) {
        const QString v = findByCap(P::isVisionModel);
        return v.isEmpty() ? fallback : v;
    }
    if (domain == AiClient::DomainCoding) {
        const QString c = findByCap(P::isCoderModel);
        if (!c.isEmpty()) return c;
    }
    return fallback;
}

/* Wrapper: legge lo stato reale della UI (checkbox, combo, allegato
 * immagine) e delega la scelta a _pickRoutedModel(). Opt-in, default OFF —
 * non scavalca MAI la scelta manuale del combo di default. */
QString AgentiPage::_routedModel(const QString& task, const QString& fallback) const
{
    if (!m_chkAutoRouting || !m_cmbLLM) return fallback;

    QStringList installed;
    installed.reserve(m_cmbLLM->count());
    for (int i = 0; i < m_cmbLLM->count(); ++i)
        installed << m_cmbLLM->itemData(i, Qt::UserRole).toString();

    return _pickRoutedModel(m_chkAutoRouting->isChecked(), !m_imgBase64.isEmpty(),
                             AiClient::detectQueryDomain(task), installed, fallback);
}

void AgentiPage::runAgent(int idx) {
    auto roleList = AgentsConfigDialog::roles();
    int  roleIdx  = m_cfgDlg->roleCombo(idx)->currentIndex();
    if (roleIdx < 0 || roleIdx >= roleList.size()) roleIdx = 0;
    const auto& role = roleList[roleIdx];

    /* Modalità singola (CHAT RAG, m_maxShots==1): usa sempre il combo LLM principale.
       In pipeline multi-agente usa il modello assegnato nel dialog Configura Agenti. */
    QString selectedModel;
    if (m_maxShots == 1 && m_cmbLLM) {
        selectedModel = ModelComboHelper::currentModel(m_cmbLLM);
    } else {
        selectedModel = m_cfgDlg->modelCombo(idx)->currentData().toString();
        if (selectedModel.isEmpty()) selectedModel = m_cfgDlg->modelCombo(idx)->currentText();
    }

    /* Sicurezza: se il modello è di embedding (non-chat) o non valido,
       ricade sul modello selezionato nel combo LLM principale. */
    if (selectedModel.isEmpty()
        || selectedModel == "(nessun modello)"
        || _isEmbeddingModel(selectedModel))
    {
        const QString fallback = ModelComboHelper::currentModel(m_cmbLLM);
        if (!fallback.isEmpty() && !_isEmbeddingModel(fallback))
            selectedModel = fallback;
    }

    if (!selectedModel.isEmpty()
        && selectedModel != "(nessun modello)"
        && !_isEmbeddingModel(selectedModel))
    {
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), selectedModel);
    }

    /* D-27: routing automatico (opt-in) — setModel() invece di setBackend():
     * non emette modelChanged(), quindi NON risincronizza m_cmbLLM (la
     * scelta manuale visibile all'utente resta intatta). selectedModel è
     * aggiornato qui così le label sotto (bolla, header agente) mostrano
     * il modello davvero usato, non quello nominale del combo. */
    {
        const QString routed = _routedModel(m_taskOriginal, selectedModel);
        if (routed != selectedModel && !routed.isEmpty()) {
            m_ai->setModel(routed);
            selectedModel = routed;
        }
    }

    QString ts = QTime::currentTime().toString("HH:mm:ss");

    /* Salva posizione PRIMA dell'header: l'intera bolla (header+contenuto) sarà sostituita su finish */
    {
        QTextCursor c(m_log->document());
        c.movePosition(QTextCursor::End);
        m_agentBlockStart = c.position();
    }

    /* Metadati per la bolla finale */
    const bool isSingleChat = (m_maxShots == 1);
    const bool isTeamMode   = isSingleChat && m_btnTeam && m_btnTeam->isChecked();
    m_currentAgentLabel = isTeamMode
        ? "\xf0\x9f\x91\xa5  Team di agenti"
        : (isSingleChat
            ? "\xf0\x9f\x93\xa4  Invia"
            : role.icon + QString("  Agente %1 \xe2\x80\x94 %2").arg(idx + 1).arg(role.name));
    m_currentAgentModel = selectedModel;
    m_currentAgentTime  = ts;

    /* Indicatore streaming temporaneo stilizzato (sarà sostituito dalla bolla su onFinished) */
    {
        const auto& c = bc();
        const QString backendTag = (m_ai->backend() == AiClient::Ollama)
            ? "Ollama" : "llama-server";
        const QString modelTag = selectedModel.isEmpty() ? "?" : selectedModel;
        const QString label = isSingleChat
            ? "\xf0\x9f\x93\xa4 Invia"
            : (role.icon + QString("  Agente %1 \xe2\x80\x94 %2").arg(idx + 1).arg(role.name));
        const QString hdr =
            "<table width='100%' cellpadding='0' cellspacing='0'>"
            "<tr>"
              "<td style='"
                "background:" + QString(c.aBg) + ";"
                "border:1px solid " + c.aBdr + ";"
                "border-radius:10px;"
                "padding:8px 14px;"
                "color:" + c.aTxt + ";"
              "'>"
                "<p style='color:" + QString(c.aHdr) + ";font-size:11px;margin:0 0 4px 0;'>"
                  + label.toHtmlEscaped() +
                  "  <span style='color:#64748b;font-weight:normal;'>"
                    "\xc2\xb7\xc2\xb7  "
                    + backendTag.toHtmlEscaped() + " \xc2\xb7 "
                    + modelTag.toHtmlEscaped() +
                    "  \xf0\x9f\x94\x84 generando..."  /* 🔄 */
                  "</span>"
                "</p>"
            /* Il testo dei token viene aggiunto qui come plain text */;
        m_log->moveCursor(QTextCursor::End);
        m_log->insertHtml(hdr);
    }
    m_agentTextStart = m_log->document()->characterCount() - 1;

    QString userPrompt;

    /* In modalità pipeline multi-agente, ogni agente vede l'obiettivo comune esplicito */
    if (!isSingleChat) {
        userPrompt  = "\xf0\x9f\x8e\xaf OBIETTIVO DEL TEAM: ";
        userPrompt += m_taskOriginal;
        userPrompt += "\n\n";
    } else {
        userPrompt = m_taskOriginal;
    }

    /* RAG inline (dal tab principale) + RAG condiviso (da AgentsConfigDialog) */
    if (m_ragInline && m_ragInline->hasContext())
        userPrompt += _sanitize_prompt(m_ragInline->ragContext());
    if (m_cfgDlg->sharedRagWidget() && m_cfgDlg->sharedRagWidget()->hasContext())
        userPrompt += _sanitize_prompt(m_cfgDlg->sharedRagWidget()->ragContext());
    /* RAG specifico per questo agente */
    if (m_cfgDlg->ragWidget(idx) && m_cfgDlg->ragWidget(idx)->hasContext())
        userPrompt += _sanitize_prompt(m_cfgDlg->ragWidget(idx)->ragContext());
    if (!m_agentOutputs.isEmpty()) {
        userPrompt += "\n\n\xe2\x80\x94\xe2\x80\x94 Output agenti precedenti \xe2\x80\x94\xe2\x80\x94\n";
        for (int i = 0; i < m_agentOutputs.size(); i++) {
            int prevRole = m_cfgDlg->roleCombo(i)->currentIndex();
            QString prevName = (prevRole >= 0 && prevRole < roleList.size())
                               ? roleList[prevRole].name : "Agente";
            userPrompt += QString("\n[Agente %1 \xe2\x80\x94 %2]:\n%3\n")
                          .arg(i + 1).arg(prevName).arg(m_agentOutputs[i]);
        }
        userPrompt += "\n" + QString(16, QChar(0x2500)) + "\n";
        userPrompt += QString("Ora esegui il tuo ruolo di %1.").arg(role.name);
    } else if (!isSingleChat) {
        /* Primo agente della pipeline: nessun output precedente, esplicita il ruolo */
        userPrompt += QString("Il tuo ruolo in questo team: %1.").arg(role.name);
    }

    /* In pipeline multi-agente arricchisce il system prompt con l'obiettivo del team:
       ogni agente conosce il task concreto nel suo contesto "permanente" (system). */
    const QString teamGoalFull  = isSingleChat ? QString()
        : QString("\n\n\xf0\x9f\x8e\xaf Obiettivo globale del team: ") + m_taskOriginal.left(200);
    const QString teamGoalSmall = isSingleChat ? QString()
        : QString(" Task: ") + m_taskOriginal.left(80);
    if (m_toolsEnabled && isSingleChat) startMcpDiscovery(); /* on-demand, idempotente */
    const QString toolSuffix = (m_toolsEnabled && isSingleChat) ? toolSystemSuffix() : QString();

    /* Modalita' Dizionario Etimologico — attiva se il pulsante Etimo e' premuto */
    static const char* kEtymoSys =
        "Sei un dizionario etimologico italiano in stile Wikipedia. "
        "Quando ti viene chiesta la definizione o l'origine di una parola, rispondi "
        "sempre con queste sezioni in Markdown:\n"
        "## [Parola]\n"
        "### Lingua di origine\n"
        "Indica se e' latina, greca antica, aramaica, araba, germanica ecc. "
        "Scrivi la forma originale nell'alfabeto originale "
        "(es. greco: \xcf\x84\xce\xb5\xcf\x87\xce\xbd\xce\xb7, latino: *domus, -i* f.).\n"
        "### Morfologia\n"
        "Classe grammaticale, paradigma (declinazione latina o greca con genitivo e genere), "
        "analisi prefisso+radice+suffisso con significato di ogni parte.\n"
        "### Significato letterale\n"
        "Traduzione diretta, primo significato attestato.\n"
        "### Evoluzione semantica\n"
        "Come il significato si e' trasformato attraverso i secoli "
        "(latino classico → latino tardo → volgare → italiano moderno).\n"
        "### Prima attestazione\n"
        "Autore piu' antico noto che usa la parola, opera, secolo.\n"
        "### Derivati moderni\n"
        "Derivati in italiano, francese, inglese, spagnolo con il significato.\n"
        "### Esempio d'uso originale\n"
        "Una frase nella lingua originale con traduzione interlineare parola per parola.\n"
        "### Voci correlate\n"
        "Altre parole della stessa radice indoeuropea o dello stesso campo semantico.\n"
        "Rispondi sempre in italiano. Usa solo le sezioni pertinenti alla parola chiesta.";

    const QString etymoPfx = (m_btnEtimo && m_btnEtimo->isChecked())
        ? QString(kEtymoSys) + "\n\n" : QString();

    /* Team di agenti: sovrascrive il system prompt con istruzioni multi-ruolo */
    static const QString kTeamSys =
        "Sei un team di esperti multidisciplinari. Rispondi strutturando la risposta "
        "in sezioni, ognuna scritta da uno specialista diverso tra quelli pertinenti: "
        "[Ricercatore], [Analista], [Consulente], [Critico]. "
        "Usa solo le sezioni utili per la domanda. "
        "Termina sempre con [Sintesi] concisa. Rispondi in italiano.";
    QString sysFullMut  = isTeamMode ? kTeamSys + toolSuffix
                                     : etymoPfx + role.sysPrompt      + teamGoalFull  + toolSuffix;
    QString sysSmallMut = isTeamMode ? kTeamSys + toolSuffix
                                     : etymoPfx + role.sysPromptSmall + teamGoalSmall + toolSuffix;

    /* Convenzione formattazione Markdown.
       Versione lunga per modelli grandi (≥7B): guida completa.
       Versione corta per modelli piccoli (<7B): solo l'essenziale — evita
       che modelli ≤3B producano output strutturati anomali (REFEEZ, PRGET…)
       invece di rispondere normalmente. */
    static const QString kFmtFull =
        "\n\nUsa Markdown nelle risposte: **grassetto** per concetti chiave, "
        "*corsivo* per termini tecnici, `codice` per valori inline, "
        "```lang blocchi``` per codice multi-riga (specifica sempre il linguaggio: "
        "```python, ```bash, ```cpp, ```java, ```json, ecc.), "
        "tabelle Markdown per confronti, "
        "> per citazioni/avvisi, elenchi - o 1. per liste. "
        "Usa emoji per migliorare la leggibilit\xc3\xa0 dove appropriato: "
        "\xe2\x9c\x85 conferme, \xe2\x9a\xa0 avvisi, \xf0\x9f\x92\xa1 suggerimenti, "
        "\xf0\x9f\x93\x8c punti chiave, \xe2\x9d\x8c errori, \xf0\x9f\x9a\x80 performance, "
        "\xf0\x9f\x94\xa7 configurazione, \xf0\x9f\x93\x8a dati, \xf0\x9f\xa7\xa0 concetti teorici, "
        "\xf0\x9f\x93\x96 documentazione, \xf0\x9f\x94\x8d dettagli, \xe2\xad\x90 importante. "
        "Non usare HTML inline. "
        "Non citare il progetto Prismalux come esempio in risposte a domande generali: "
        "usa solo esempi pertinenti al dominio della domanda. "
        "Rispondi nella stessa lingua dell'utente."
        /* ── Notazione matematica KaTeX (stile LibreOffice Math) ── */
        "\n\nPer le formule matematiche usa KaTeX con delimitatori \\(...\\) inline "
        "e \\[...\\] per display. Scegli sempre i simboli LaTeX appropriati:\n"
        "- Operatori unari/binari: \\pm \\mp \\times \\cdot \\div \\oplus \\ominus "
        "\\otimes \\oslash \\odot \\neg \\wedge \\vee\n"
        "- Relazioni: \\leq \\geq \\neq \\ll \\gg \\approx \\sim \\simeq \\equiv "
        "\\propto \\parallel \\perp \\rightarrow \\leftarrow \\Rightarrow "
        "\\Leftarrow \\Leftrightarrow \\leftrightarrow\n"
        "- Insiemi: \\in \\notin \\ni \\subset \\subseteq \\supset \\supseteq "
        "\\cap \\cup \\setminus \\emptyset \\mathbb{R} \\mathbb{Z} \\mathbb{N} "
        "\\mathbb{Q} \\mathbb{C}\n"
        "- Struttura: \\frac{a}{b} \\sqrt{a} ^{n}\\sqrt{a} \\sum_{i=0}^{n} "
        "\\prod_{i=1}^{n} \\int_{a}^{b} \\iint \\oint \\lim_{x\\to 0} "
        "\\binom{n}{k}\n"
        "- Funzioni: \\sin \\cos \\tan \\cot \\sinh \\cosh \\tanh \\coth "
        "\\arcsin \\arccos \\arctan \\ln \\log \\exp\n"
        "- Attributi/decoratori: \\vec{a} \\hat{a} \\bar{a} \\tilde{a} "
        "\\dot{a} \\ddot{a} \\overrightarrow{AB} \\mathbf{F} \\mathit{x}\n"
        "- Parentesi scalabili: \\left( \\right) \\left[ \\right] "
        "\\left\\{ \\right\\} \\left| \\right| \\left\\| \\right\\|\n"
        "- Altro: \\partial \\nabla \\infty \\hbar \\forall \\exists \\nexists "
        "\\lambda \\pi \\alpha \\beta \\gamma \\delta \\epsilon \\sigma \\omega "
        "\\Delta \\Sigma \\Pi \\Omega \\Gamma\n"
        "Mai usare * per moltiplicazione (usa \\cdot o \\times); "
        "mai ^ senza {apici} quando l'esponente ha >1 carattere.";

    static const QString kFmtSmall =
        "\nRispondi in modo chiaro e diretto. Usa emoji se utile. "
        "Non citare Prismalux in risposte a domande generali.";

    sysFullMut  += kFmtFull;
    sysSmallMut += kFmtSmall;

    /* Hermes: inietta memoria da sessioni precedenti (solo chat singola).
       Il prefisso chiarisce al modello che si tratta di contesto opzionale,
       non di istruzioni da seguire letteralmente. */
    if (m_hermesEnabled && isSingleChat && !m_taskOriginal.isEmpty())
        hermesInjectContext(sysFullMut, m_taskOriginal);

    const QString sysFull  = sysFullMut;
    const QString sysSmall = sysSmallMut;

    m_agentTimer.restart();
    m_ttftCaptured = false;   /* FEAT-2: reset TTFT per questo agente */
    m_agentOutputs.append("");

    /* Tool use nativo Ollama: attiva solo in CHAT singola con tool abilitati
       e backend Ollama (llama-server non supporta tool_calls nel formato Ollama). */
    if (m_toolsEnabled && isSingleChat && m_ai->backend() == AiClient::Ollama)
        m_ai->setActiveTools(buildEnabledTools(m_taskOriginal));
    else
        m_ai->clearActiveTools();

    /* Costruisce history JSON con Headroom: [summary] + ultimi N turni */
    QJsonArray histArray;
    if (isSingleChat && m_ctxSingle->messageCount() > 0)
        histArray = m_ctxSingle->buildContext();

    const QString sys = _buildSys(m_taskOriginal, sysFull, sysSmall,
                                  m_ai->model(), m_ai->backend(),
                                  m_ai->modelSizeBytes(m_ai->model()));

    /* Pre-processing per-modello (lingua, prefissi, system prompt override) */
    auto [sysFinal, userFinal] = ModelProcessor::instance().preProcess(sys, userPrompt, m_ai->model());

    /* Usa chatWithImage per il primo agente se c'è un'immagine allegata */
    if (idx == m_currentAgent && !m_imgBase64.isEmpty()) {
        m_ai->chatWithImage(sysFinal, userFinal, m_imgBase64, m_imgMime);
    } else if (isSingleChat && !histArray.isEmpty()) {
        m_ai->chat(sysFinal, userFinal, histArray, AiClient::QueryAuto);
    } else {
        m_ai->chat(sysFinal, userFinal);
    }
}

/* ══════════════════════════════════════════════════════════════
   _finishedPipelineControl — controller LLM completato
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_finishedPipelineControl() {
    m_opMode = OpMode::Pipeline;

    QString ctrlHtml = markdownToHtml(m_ctrlAccum.trimmed());
    QTextCursor selCtrl(m_log->document());
    selCtrl.setPosition(m_ctrlBlockStart);
    selCtrl.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    selCtrl.removeSelectedText();
    selCtrl.insertHtml(buildControllerBubble(ctrlHtml));

    advancePipeline();
}

/* ══════════════════════════════════════════════════════════════
   _finishedPipeline — risposta agente pipeline completata
   ══════════════════════════════════════════════════════════════ */
/* ══════════════════════════════════════════════════════════════
   _finishedPipeline — risposta agente pipeline completata
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_finishedPipeline(const QString& full) {
    if (m_currentAgent < MAX_AGENTS)
        m_cfgDlg->enabledChk(m_currentAgent)->setStyleSheet(
            "QCheckBox { color: #4caf50; font-weight: bold; }"
            "QCheckBox::indicator:checked { background-color: #4caf50; border: 2px solid #388e3c; border-radius: 3px; }");

    m_currentAgent++;

    int total = 0, done = 0;
    for (int i = 0; i < MAX_AGENTS; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) total++;
    for (int i = 0; i < m_currentAgent; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) done++;
    int pct = (total > 0) ? qMin(done * 100 / total, 99) : 0;
    emit pipelineStatus(pct, QString("\xe2\x9c\x85  Agente %1/%2 completato  (%3%)")
        .arg(done).arg(qMin(total, m_maxShots)).arg(pct));

    /* Sostituisce l'indicatore streaming + testo grezzo con la bolla AI completa */
    bool shouldAutoSearch = false;   /* TASK-2: segnala auto-ricerca web dopo il blocco */
    QString rawResp;
    if (m_currentAgent > 0 && !m_agentOutputs.isEmpty()) {
        rawResp = m_agentOutputs[m_currentAgent - 1];
        /* Fallback: modelli thinking-only (qwen3, deepseek-r1) rispondono tramite
           message.thinking invece di message.content → nessun token emesso →
           m_agentOutputs resta vuota. AiClient avvolge il thinking in <think>...</think>
           e lo passa via finished(). Usiamo quel valore se il buffer locale è vuoto. */
        if (rawResp.isEmpty() && !full.isEmpty())
            rawResp = m_agentOutputs[m_currentAgent - 1] = full;
        /* Fix toggle ▶️: thinking via message.thinking (campo separato da content).
           AiClient prepone <think>...</think> solo in finished(), non nei token.
           rawResp (dai token) non ha il think block; full sì.
           Usiamo full per estrarre il thinking e mostrare il toggle. */
        else if (!rawResp.isEmpty() && !full.isEmpty()
                 && full.contains("<think>", Qt::CaseInsensitive)
                 && !rawResp.contains("<think>", Qt::CaseInsensitive)) {
            rawResp = m_agentOutputs[m_currentAgent - 1] = full;
        }
        /* Rimuove blocchi <think>...</think> (reasoning models: qwen3, deepseek-r1, qwq...)
           Se dopo lo strip rimane vuoto (modelli piccoli che producono SOLO thinking
           senza risposta finale), si usa il contenuto del <think> come fallback.
           Il contenuto del <think> viene salvato in m_thinkTexts per il toggle collassabile. */
        QString extractedThink;  /* contenuto <think> estratto — salvato per la bolla */
        {
            QRegularExpression reTh("<think>([\\s\\S]*?)</think>",
                                    QRegularExpression::CaseInsensitiveOption);
            /* Salva l'originale prima di rimuovere */
            const QString original = rawResp;
            auto thinkMatch = reTh.match(original);
            if (thinkMatch.hasMatch())
                extractedThink = thinkMatch.captured(1).trimmed();

            rawResp.remove(QRegularExpression("<think>[\\s\\S]*?</think>",
                QRegularExpression::CaseInsensitiveOption));
            rawResp = rawResp.trimmed();

            /* <think> senza </think>: budget ragionamento esaurito a metà.
               Tutto ciò che segue un <think> non chiuso è thinking non terminato. */
            rawResp.remove(QRegularExpression("<think>[\\s\\S]*$",
                QRegularExpression::CaseInsensitiveOption));
            rawResp = rawResp.trimmed();

            /* Fallback: se il modello ha prodotto solo reasoning (o output vuoto),
             * mostra il contenuto del <think> se non vuoto.
             * Caso comune con modelli 0.8-1.5B: generano <think>...</think>
             * ma poi non aggiungono risposta finale. */
            if (rawResp.isEmpty()) {
                auto m = reTh.match(original);
                if (m.hasMatch()) {
                    /* Prova prima il contenuto trimmed, poi raw (per preservare newline) */
                    const QString thinkTrimmed = m.captured(1).trimmed();
                    rawResp = thinkTrimmed.isEmpty() ? m.captured(1) : thinkTrimmed;
                    if (rawResp.isEmpty()) {
                        /* Modello troppo piccolo: think vuoto + nessuna risposta */
                        rawResp = "[Il modello non ha prodotto risposta. "
                                  "Usa un modello più grande (≥3B) o premi "
                                  "Singolo per domande semplici.]";
                    }
                    /* Non aggiornare m_agentOutputs con il think: passiamo ai prossimi
                       agenti un placeholder chiaro invece di contenuto di ragionamento */
                    m_agentOutputs[m_currentAgent - 1] =
                        "[Modello ha prodotto solo ragionamento interno — nessuna risposta finale]";
                    extractedThink = "";  /* il think diventa la risposta: non serve toggle */
                } else {
                    /* Nessun tag <think> e risposta vuota — modello non ha risposto */
                    rawResp = "[Nessuna risposta dal modello. "
                              "Verifica che Ollama sia avviato e il modello selezionato "
                              "sia disponibile.]";
                    m_agentOutputs[m_currentAgent - 1] = rawResp;
                }
            } else {
                m_agentOutputs[m_currentAgent - 1] = rawResp;
            }
        }
        /* Strip ragionamento italiano inline: i modelli spesso iniziano con frasi-spia
           tipo "L'utente chiede...", "Devo rispondere...", "Sto analizzando..." prima
           della risposta vera. Rimuove le righe iniziali che corrispondono ai pattern. */
        {
            static const QRegularExpression reThinkLine(
                QString::fromUtf8(
                    "^(l['\xE2\x80\x99]utente\\s+(chiede|vuole|ha\\s+chiesto|sta|intende|sembra|desidera|ha\\s+fatto)|"
                    "devo\\s+rispondere|voglio\\s+rispondere|prima\\s+di\\s+rispondere|"
                    "sto\\s+(pensando|analizzando|considerando|riflettendo|cercando)|"
                    "analizziamo|penso\\s+di\\s+dover|capisco\\s+che|vediamo\\s+(cosa|come)|"
                    "mi\\s+viene\\s+chiesto|ho\\s+capito\\s+che|la\\s+domanda\\s+\\xC3\\xA8)"),
                QRegularExpression::CaseInsensitiveOption);
            QStringList lines = rawResp.split('\n');
            int firstReal = 0;
            for (int i = 0; i < lines.size(); i++) {
                const QString trimmed = lines[i].trimmed();
                if (trimmed.isEmpty() || reThinkLine.match(trimmed).hasMatch())
                    firstReal = i + 1;
                else
                    break;
            }
            if (firstReal > 0 && firstReal < lines.size()) {
                const QString stripped = QStringList(lines.mid(firstReal)).join('\n').trimmed();
                if (!stripped.isEmpty()) {
                    rawResp = stripped;
                    m_agentOutputs[m_currentAgent - 1] = rawResp;
                }
            }
        }

        /* Post-processing sincrono per-modello (markdown strip, regex, ecc.) */
        if (!rawResp.isEmpty()) {
            rawResp = ModelProcessor::instance().postProcess(rawResp, m_ai->model());
            m_agentOutputs[m_currentAgent - 1] = rawResp;
        }

        /* ── Tool Use Nativo: intercetta TOOL_CALL prima di costruire la bolla ── */
        if (m_toolsEnabled && m_maxShots == 1 && m_toolIteration < 2 && !rawResp.isEmpty()) {
            const QJsonObject tc = detectFirstToolCall(rawResp);
            if (!tc.isEmpty()) {
                m_toolIteration++;
                const int agentIdx = m_currentAgent - 1;

                /* Rimuove testo grezzo dello streaming */
                QTextCursor selTool(m_log->document());
                selTool.setPosition(m_agentBlockStart);
                selTool.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                selTool.removeSelectedText();

                /* Mostra indicatore compatto "tool in esecuzione" */
                const QString tn  = tc["tool"].toString().toHtmlEscaped();
                const QString tin = tc["input"].toString().left(120).toHtmlEscaped();
                m_log->moveCursor(QTextCursor::End);
                m_log->insertHtml(
                    "<p style='color:#94a3b8;font-size:11px;margin:4px 0;'>"
                    "\xf0\x9f\x94\xa7&nbsp;<b>Tool:</b>&nbsp;" + tn +
                    "&nbsp;\xe2\x80\x94&nbsp;<code>" + tin +
                    "</code>&nbsp;&nbsp;\xe2\x8f\xb3 in esecuzione...</p>");

                /* Rimuove l'ultima voce di m_agentOutputs: runAgent la re-appenderà */
                if (!m_agentOutputs.isEmpty())
                    m_agentOutputs.removeLast();

                runToolCall(tc, [this, agentIdx, tc](const QString& result) {
                    /* crea_evento_calendario: il risultato contiene path a immagini
                     * PNG (QR code) — vanno inserite come <img> nella bolla, non
                     * rilanciate all'LLM (che le riscriverebbe come testo,
                     * perdendo l'immagine o corrompendo il path). Chiude qui la
                     * pipeline invece di fare un altro giro di generazione. */
                    if (_showQrEventoBubble(result))
                        return;

                    /* Aggiorna il log con il risultato del tool */
                    m_log->moveCursor(QTextCursor::End);
                    m_log->insertHtml(
                        "<p style='color:#86efac;font-size:11px;margin:4px 0;'>"
                        "\xe2\x9c\x85&nbsp;<b>Risultato:</b>&nbsp;"
                        + result.left(300).toHtmlEscaped() + "</p>");

                    /* Inietta il risultato nel contesto del task */
                    m_taskOriginal += QString(
                        "\n\n[TOOL_RESULT: %1]\n%2\n\n"
                        "Rispondi ora all'utente in italiano.")
                        .arg(tc["tool"].toString(), result);

                    /* Re-run dello stesso agente con il contesto arricchito */
                    m_currentAgent = agentIdx;
                    runAgent(agentIdx);
                });
                return;
            }
        }

        /* Aggiunge il tempo di risposta all'header della bolla */
        {
            const double elapsedMs = static_cast<double>(m_agentTimer.elapsed());
            const QString elapsedStr = elapsedMs < 1000.0
                ? QString::number(qRound(elapsedMs)) + " ms"
                : QString::number(elapsedMs / 1000.0, 'f', 1) + " s";
            m_currentAgentTime += "  \xc2\xb7\xc2\xb7  " + elapsedStr;
        }

        QString htmlContent = rawResp.isEmpty()
            ? "<p style='color:#6b7280;font-style:italic;margin:0;'>Nessun output.</p>"
            : markdownToHtml(rawResp, &m_codeBlocks, &m_codeBlockCounter);

        /* Banner + auto-ricerca web quando il modello dichiara incertezza.
           In single-shot (m_maxShots==1) lancia anche una ricerca automatica (TASK-2/3).
           Non si attiva durante un auto-retry (m_autoRetryActive) per evitare loop. */
        if (!rawResp.isEmpty() && !m_autoRetryActive) {
            static const QRegularExpression reNonSo(
                QString::fromUtf8(
                "non\\s+(?:lo\\s+|ne\\s+|ho\\s+)?s[ao](?!p)|"
                "non\\s+(?:sono\\s+|riesco\\s+|posso\\s+)?in\\s+grado|"
                "non\\s+ho\\s+(?:abbastanza\\s+)?informazioni|"
                "non\\s+(?:posso|riesco)\\s+(?:rispondere|aiutarti)|"
                "non\\s+conosco|non\\s+ricordo|non\\s+dispongo|"
                "non\\s+mi\\s+\xc3\xa8\\s+possibile|"
                "non\\s+ho\\s+(?:accesso|dati)|"
                "non\\s+sono\\s+a\\s+conoscenza|"
                "questa\\s+informazione\\s+non|"
                "fuori\\s+dalla\\s+mia|beyond\\s+my\\s+knowledge|"
                "i\\s+don.?t\\s+know|i\\s+cannot\\s+(?:answer|provide)|"
                "i\\s+(?:do\\s+not|don.?t)\\s+have\\s+(?:access|information)"),
                QRegularExpression::CaseInsensitiveOption);
            /* Guardia falsi positivi: "non so se/quale/cosa/come..." non è incertezza */
            static const QRegularExpression reFalsoPositivo(
                "non\\s+so\\s+(?:se|quale|cosa|come|quando|dove|chi|bene)\\b",
                QRegularExpression::CaseInsensitiveOption);
            const bool uncertain = reNonSo.match(rawResp).hasMatch()
                                && !reFalsoPositivo.match(rawResp).hasMatch();
            if (uncertain) {
                /* Colori adattativi al tema corrente */
                const bool _lt = isLightTheme();
                const char* _nBg   = _lt ? "#f5f3ff" : "#1e1527";
                const char* _nBdr  = _lt ? "#a78bfa" : "#7c3aed";
                const char* _nHdr  = _lt ? "#6d28d9" : "#a78bfa";
                const char* _nSub  = _lt ? "#5b21b6" : "#c4b5fd";
                const char* _nTxt  = _lt ? "#1e1b4b" : "#e2e8f0";
                const char* _nVal  = _lt ? "#059669" : "#86efac";
                const char* _nMut  = _lt ? "#374151" : "#94a3b8";
                const char* _nSep  = _lt ? "#c4b5fd" : "#4c1d95";
                const char* _nLnk  = _lt ? "#7c3aed" : "#818cf8";
                const char* _nAcc  = _lt ? "#4338ca" : "#c4b5fd";

                /* ── Step parametri interattivi: legge i valori correnti e propone
                   passi pre-calcolati per ogni parametro come badge cliccabili ── */
                const AiChatParams _cur = AiChatParams::load();

                auto _badge = [&](const QString& href, const QString& vStr,
                                   const QString& lbl) -> QString {
                    return QString("<a href='%1' style='color:%2;font-size:10px;"
                                   "font-weight:bold;background:%3;border:1px solid %4;"
                                   "border-radius:3px;padding:2px 8px;text-decoration:none;"
                                   "margin:0 3px 2px 0;white-space:nowrap;'>"
                                   "&rarr;&nbsp;%5 <span style='font-weight:normal;"
                                   "font-size:9px;'>%6</span></a>")
                        .arg(href).arg(_nLnk).arg(_nBg).arg(_nBdr).arg(vStr).arg(lbl);
                };

                // Temperatura — abbassare rende le risposte più focalizzate
                QString _tLnks;
                {
                    const double T = _cur.temperature;
                    if (T > 0.55) {
                        _tLnks += _badge("param-set:temperature:0.50", "0.50", "bilanciato");
                        _tLnks += _badge("param-set:temperature:0.30", "0.30", "preciso");
                        _tLnks += _badge("param-set:temperature:0.10", "0.10", "rigido");
                    } else if (T > 0.35) {
                        _tLnks += _badge("param-set:temperature:0.30", "0.30", "preciso");
                        _tLnks += _badge("param-set:temperature:0.10", "0.10", "rigido");
                    } else if (T > 0.15) {
                        _tLnks += _badge("param-set:temperature:0.10", "0.10", "rigido");
                    }
                    if (T < 0.65)
                        _tLnks += _badge("param-set:temperature:0.70", "0.70",
                                          "creativo &uarr;");
                }

                // num_ctx — alzare dà più contesto al modello
                QString _cLnks;
                {
                    static const int   kCL[] = {8192, 16384, 32768, 65536};
                    static const char* kCN[] = {"8K", "16K", "32K", "64K"};
                    int cnt = 0;
                    for (int i = 0; i < 4 && cnt < 3; ++i) {
                        if (kCL[i] > _cur.num_ctx) {
                            const char* lbl = cnt == 0 ? "+contesto" :
                                               cnt == 1 ? "molto"     : "max";
                            _cLnks += _badge(
                                QString("param-set:num_ctx:%1").arg(kCL[i]),
                                kCN[i], lbl);
                            ++cnt;
                        }
                    }
                }

                // num_predict — alzare permette risposte e codice più lunghi
                QString _pLnks;
                {
                    static const int   kPL[] = {2048, 4096, 8192};
                    static const char* kPN[] = {"2K", "4K", "8K"};
                    int cnt = 0;
                    for (int i = 0; i < 3 && cnt < 2; ++i) {
                        if (kPL[i] > _cur.num_predict) {
                            _pLnks += _badge(
                                QString("param-set:num_predict:%1").arg(kPL[i]),
                                kPN[i],
                                cnt == 0 ? "risposta lunga" : "codice completo");
                            ++cnt;
                        }
                    }
                }

                // top_p — abbassare rende il campionamento più deterministico
                QString _tpLnks;
                {
                    const double TP = _cur.top_p;
                    if (TP > 0.85) {
                        _tpLnks += _badge("param-set:top_p:0.85", "0.85", "bilanciato");
                        _tpLnks += _badge("param-set:top_p:0.70", "0.70", "deterministico");
                    } else if (TP > 0.72) {
                        _tpLnks += _badge("param-set:top_p:0.70", "0.70", "deterministico");
                    }
                    if (TP < 0.92)
                        _tpLnks += _badge("param-set:top_p:0.95", "0.95",
                                           "pi&ugrave; vario &uarr;");
                }

                // Riga tabella: icona+nome | valore attuale | step cliccabili | perché
                auto _pRow = [&](const char* ico, const char* nm, const QString& curV,
                                  const QString& lnks, const char* why) -> QString {
                    const QString noStep =
                        QString("<span style='color:%1;font-size:10px;"
                                "font-style:italic;'>gi&agrave; ok</span>")
                        .arg(_nMut);
                    return
                        QString("<tr><td style='color:%1;padding:4px 8px 4px 0;"
                                "white-space:nowrap;'>").arg(_nTxt)
                        + ico + " <b>" + nm + "</b></td>"
                        + QString("<td style='color:%1;padding:4px 8px;"
                                  "white-space:nowrap;font-size:10px;font-style:italic;'>")
                          .arg(_nMut) + curV + "</td>"
                        + "<td style='padding:4px 8px 4px 0;'>"
                        + (lnks.isEmpty() ? noStep : lnks) + "</td>"
                        + QString("<td style='color:%1;font-size:10px;'>").arg(_nMut)
                        + why + "</td></tr>";
                };

                const QString _paramSec =
                    QString("<p style='color:%1;font-size:11px;margin:6px 0 4px 0;'>"
                            "<b style='color:%2;'>2. Ritocca parametri</b> "
                            "(<a href='settings:model' style='color:%3;font-size:11px;'>"
                            "Impostazioni &rarr; Modello</a>):</p>")
                    .arg(_nSub).arg(_nHdr).arg(_nLnk)
                    + "<table style='font-size:11px;border-collapse:collapse;"
                      "width:100%;margin-bottom:4px;'>"
                    + QString(
                        "<tr>"
                        "<th style='text-align:left;color:%1;padding:2px 8px 2px 0;"
                        "border-bottom:1px solid %2;'>Parametro</th>"
                        "<th style='text-align:left;color:%1;padding:2px 8px;"
                        "border-bottom:1px solid %2;'>Ora</th>"
                        "<th style='text-align:left;color:%1;padding:2px 8px 2px 0;"
                        "border-bottom:1px solid %2;'>Step consigliati <span "
                        "style='font-weight:normal;font-size:9px;'>(clicca)</span></th>"
                        "<th style='text-align:left;color:%1;padding:2px 0;"
                        "border-bottom:1px solid %2;'>Effetto</th>"
                        "</tr>").arg(_nLnk).arg(_nSep)
                    + _pRow("\xf0\x9f\x8c\xa1", "Temperatura",
                            QString::number(_cur.temperature, 'f', 2),
                            _tLnks,
                            "Abbassa &rarr; pi&ugrave; focalizzate")
                    + _pRow("\xf0\x9f\x93\x96", "Context (num_ctx)",
                            QString::number(_cur.num_ctx),
                            _cLnks,
                            "Alza se &lsquo;dimentica&rsquo; il contesto")
                    + _pRow("\xe2\x9c\x8f", "Max tokens",
                            QString::number(_cur.num_predict),
                            _pLnks,
                            "Alza per risposte o codice lunghi")
                    + _pRow("\xf0\x9f\x8e\xaf", "Top-P",
                            QString::number(_cur.top_p, 'f', 2),
                            _tpLnks,
                            "Abbassa &rarr; pi&ugrave; deterministico")
                    + "</table>";

                htmlContent +=
                    /* ── Banner "non lo so" adattivo al tema ── */
                    QString("<div style='border:1px solid %1;border-radius:8px;"
                    "background:%2;padding:10px 14px;margin:10px 0;'>")
                    .arg(_nBdr).arg(_nBg) +

                    /* Titolo */
                    QString("<p style='color:%1;font-size:12px;margin:0 0 8px 0;"
                    "font-weight:bold;'>")
                    .arg(_nHdr) +
                    "\xf0\x9f\x92\xa1  Il modello non riesce a rispondere &mdash; "
                    "<a href='settings:model' style='color:" + QString(_nAcc) + ";'>"
                    "apri Impostazioni \xe2\x86\x97</a></p>" +

                    /* Sezione 1: modello */
                    QString("<p style='color:%1;font-size:11px;margin:0 0 6px 0;'>"
                    "<b style='color:%2;'>1. Cambia modello</b> &mdash; "
                    "prova un modello pi\xc3\xb9 capace per la programmazione:</p>")
                    .arg(_nSub).arg(_nHdr) +
                    QString("<ul style='color:%1;font-size:11px;margin:0 0 6px 14px;'>")
                    .arg(_nTxt) +
                    "<li><b>qwen3:8b</b> &mdash; ottimo ragionamento, buono per codice</li>"
                    "<li><b>deepseek-coder:6.7b</b> &mdash; specializzato per programmazione</li>"
                    "<li><b>deepseek-r1:7b</b> &mdash; ragionamento passo-passo</li>"
                    "<li><b>codellama:7b</b> &mdash; ottimizzato per generare codice</li>"
                    "</ul>" +

                    /* Sezione 2: step parametri interattivi (valori calcolati da _paramSec) */
                    _paramSec +

                    /* Sezione 3: consiglio rapido */
                    QString("<p style='color:%1;font-size:10px;margin:8px 0 0 0;"
                    "border-top:1px solid %2;padding-top:6px;'>")
                    .arg(_nMut).arg(_nSep) +
                    "\xf0\x9f\x93\x8c <b>Consiglio per neofiti:</b><br>"
                    "\xe2\x80\xa2 Inizia con <b style='color:" + QString(_nAcc) + ";'>Temperatura 0.3</b> e "
                    "<b style='color:" + QString(_nAcc) + ";'>Context 16384</b>.<br>"
                    "\xe2\x80\xa2 Se il modello risponde in modo confuso, abbassa la temperatura verso 0.1.<br>"
                    "\xe2\x80\xa2 Se dimentica il contesto delle domande precedenti, aumenta il Context.<br>"
                    "\xe2\x80\xa2 <a href='settings:model' style='color:" + QString(_nLnk) + ";'>"
                    "Vai alle Impostazioni \xe2\x86\x92</a>"
                    "</p>"
                    /* Pulsante reset rapido a valori consigliati (alternativa agli step) */
                    "<p style='margin:6px 0 0 0;'>"
                    "<a href='autoapply-params' "
                    "style='color:#4ade80;font-size:11px;font-weight:bold;"
                    "background:#0b1a10;border:1px solid #166534;border-radius:4px;"
                    "padding:3px 10px;text-decoration:none;'>"
                    "\xe2\x9a\xa1 Reimposta tutto al consigliato"
                    " <span style='font-weight:normal;font-size:10px;'>"
                    "(Temp 0.30 &middot; Ctx 16K &middot; Top-P 0.90 &middot; Tok 4K)"
                    "</span></a></p>"

                    "</div>";
            /* TASK-2: in modalità chat singola avvia anche la ricerca web automatica */
            if (m_maxShots == 1 && !m_taskOriginal.isEmpty())
                shouldAutoSearch = true;
            }
        }

        /* ── Raccolta fonti: RAG + Hermes ── */
        {
            QStringList sources;
            const int ci = m_currentAgent;
            if (m_ragInline && m_ragInline->hasContext())
                sources += m_ragInline->sourceNames();
            if (m_cfgDlg->sharedRagWidget() && m_cfgDlg->sharedRagWidget()->hasContext())
                sources += m_cfgDlg->sharedRagWidget()->sourceNames();
            if (m_cfgDlg->ragWidget(ci) && m_cfgDlg->ragWidget(ci)->hasContext())
                sources += m_cfgDlg->ragWidget(ci)->sourceNames();
            for (const auto& s : std::as_const(m_hermesLastSources))
                sources << "\xf0\x9f\xa7\xa0 Memoria: " + s;  /* 🧠 */

            if (!sources.isEmpty()) {
                htmlContent +=
                    "<div style='margin:10px 0 2px 0;border-top:1px solid #334155;"
                    "padding-top:6px;'>"
                    "<p style='margin:0;font-size:10px;color:#64748b;'>"
                    "\xf0\x9f\x93\x96 <b>Fonti:</b>";  /* 📖 */
                QStringList seen;
                for (const auto& s : std::as_const(sources)) {
                    if (seen.contains(s)) continue;
                    seen << s;
                    htmlContent += "<br>\xe2\x80\xa2 " + s.toHtmlEscaped();
                }
                htmlContent += "</p></div>";
            }
        }

        QTextCursor sel(m_log->document());
        sel.setPosition(m_agentBlockStart);
        sel.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        sel.removeSelectedText();
        { int idx = m_bubbleIdx++; m_bubbleTexts[idx] = rawResp;
          if (!extractedThink.isEmpty()) m_thinkTexts[idx] = extractedThink;
          sel.insertHtml(buildAgentBubble(m_currentAgentLabel,
                                         m_currentAgentModel,
                                         m_currentAgentTime,
                                         htmlContent, idx,
                                         extractedThink)); }

        /* Traduzione LLM post-processing: avvia seconda chiamata se richiesta dal profilo.
           Usa il modello dedicato configurato nel profilo (tipicamente leggero e veloce,
           es. aya:8b o qwen2.5:1.5b); fallback al modello principale se non configurato. */
        if (!rawResp.isEmpty() && ModelProcessor::instance().needsLLMTranslation(m_ai->model())) {
            if (!m_translateAi) {
                m_translateAi = new AiClient(this);
                connect(m_translateAi, &AiClient::finished,
                        this, &AgentiPage::onTranslationFinished);
            }
            const QString transModel = ModelProcessor::instance().llmTranslateModel(m_ai->model());
            const QString modelToUse = transModel.isEmpty() ? m_ai->model() : transModel;
            m_translateAi->setBackend(m_ai->backend(), m_ai->host(),
                                      m_ai->port(), modelToUse);
            const QString lang = ModelProcessor::instance().llmTranslateLang(m_ai->model());
            const QString prompt = (lang == "it")
                ? "Traduci il seguente testo in italiano. Mantieni tutta la formattazione. "
                  "Non aggiungere commenti o note. Restituisci SOLO il testo tradotto.\n\n" + rawResp
                : "Translate the following text to English. Keep all formatting. "
                  "No extra comments or notes. Return ONLY the translated text.\n\n" + rawResp;
            m_translateAi->chat("Sei un traduttore professionale, preciso e veloce.", prompt);
        }
    }

    /* ── Retry riuscito: persisti domanda + risultati + sintesi nel RAG,
       così le domande simili future trovano i dati in locale (la bolla
       "🌐 Ricerca Online" è già stata inserita nel log qui sopra) ── */
    if (m_autoRetryActive && !m_autoRetrySearchResults.isEmpty() && !rawResp.isEmpty()) {
        _saveAutoSearchToRag(rawResp);
        m_autoRetrySearchResults.clear();
    }

    /* ── TASK-2/3: auto-ricerca web quando LLM incerto (solo single-shot) ── */
    if (shouldAutoSearch) {
        m_autoRetryActive = true;
        /* Aggiorna label bolla per la risposta di sintesi */
        m_currentAgentLabel = "\xf0\x9f\x8c\x90  Ricerca Online";  /* 🌐 */
        m_currentAgentModel = m_ai->model();
        m_currentAgentTime  = "";
        m_agentTimer.restart();
        /* Segna il punto di inizio streaming sintesi (dopo la prima bolla) */
        m_log->moveCursor(QTextCursor::End);
        m_log->append("");
        { QTextCursor cur(m_log->document()); cur.movePosition(QTextCursor::End);
          m_agentBlockStart = cur.position(); }
        m_agentOutputs << QString();   /* slot per i token della sintesi */
        emit pipelineStatus(20, "\xf0\x9f\x94\x8d  Cerco online...");
        QJsonObject wsCall;
        wsCall["tool"]  = "ricerca";
        wsCall["input"] = m_taskOriginal;
        runToolCall(wsCall, [this](const QString& sr) {
            if (sr.trimmed().isEmpty() || sr.startsWith("errore:") ||
                sr.contains("NORESULT") || sr.contains("nessun risultato")) {
                /* Ricerca fallita: avanza normalmente */
                m_autoRetryActive = false;
                advancePipeline();
                return;
            }
            /* TASK-3: re-interroga l'LLM con il contesto web */
            m_autoRetrySearchResults = sr;  /* → _saveAutoSearchToRag a fine sintesi */
            const QString now = QLocale(QLocale::Italian).toString(
                QDateTime::currentDateTime(), "dddd d MMMM yyyy, HH:mm");
            const QString sys = _buildSys(
                QString("Sei un assistente. Data attuale: %1. "
                        "Rispondi in italiano, conciso e diretto, "
                        "usando i risultati di ricerca forniti.").arg(now),
                QString(), m_ai->model(), m_ai->backend());
            const QString uMsg = "Domanda: " + m_taskOriginal
                + "\n\nRisultati ricerca web:\n" + sr
                + "\n\nRispondi alla domanda originale usando questi risultati, "
                  "poi fai un breve riassunto dei dati trovati. Se i risultati "
                  "non bastano a rispondere, dillo chiaramente senza inventare.";
            emit pipelineStatus(50, "\xf0\x9f\xa4\x96  Elaboro risultati ricerca...");
            m_currentAgentTime = QString::number(m_agentTimer.elapsed()) + " ms";
            m_ai->chat(sys, uMsg);
            /* La risposta fluisce via onToken → m_agentOutputs.last()
               e poi onFinished → _finishedPipeline (seconda chiamata, m_autoRetryActive=true)
               che chiuderà la pipeline normalmente */
        });
        return;  /* non avanzare la pipeline ora */
    }

    /* ── Tool Executor: estrae ed esegue codice Python/C/C++, poi avvia il Controller ── */
    ExecCode ec = extractExecutableCode(rawResp);
    if (ec.lang == "python") ec.code = _sanitizePyCode(ec.code);
    if (!ec.code.isEmpty()) {
        const QString pyCode    = ec.code;   /* alias per riuso del codice sotto */
        const bool useSandbox = P::isSandboxReady();

        /* [C1] Dialog conferma — testo e colore variano in base alla sandbox */
        {
            auto* dlg = new QDialog(this);
            dlg->setWindowTitle(useSandbox
                ? "\xf0\x9f\x90\xb3  Esegui codice in sandbox Docker?"
                : "\xe2\x9a\xa0  Esegui codice generato dall\xe2\x80\x99" "AI?");
            dlg->setMinimumSize(660, 460);
            auto* lay = new QVBoxLayout(dlg);

            const QString langUpper = ec.lang == "cpp" ? "C++" :
                                      ec.lang == "c"   ? "C"   : "Python";
            auto* warnLbl = new QLabel(useSandbox
                ? ("\xf0\x9f\x90\xb3  Il codice " + langUpper +
                   " verr\xc3\xa0 eseguito in un container Docker isolato.\n"
                   "Nessun accesso a file locali, rete disabilitata, max 256\xc2\xa0MB RAM.\n"
                   "Verifica il codice, poi clicca Esegui.")
                : ("\xe2\x9a\xa0  Stai per eseguire codice " + langUpper +
                   " generato dall\xe2\x80\x99" "AI con i tuoi permessi utente.\n"
                   "Verifica che non faccia operazioni indesiderate prima di procedere."),
                dlg);
            warnLbl->setWordWrap(true);
            warnLbl->setStyleSheet(useSandbox
                ? "color:#86efac;font-weight:bold;padding:6px;"
                  "background:#052e16;border-radius:4px;"
                : "color:#facc15;font-weight:bold;padding:6px;"
                  "background:#292524;border-radius:4px;");
            lay->addWidget(warnLbl);

            auto* codeView = new QTextEdit(dlg);
            codeView->setReadOnly(true);
            codeView->setPlainText(pyCode);
            codeView->setFont(QFont("JetBrains Mono,Fira Code,Consolas,monospace", 10));
            codeView->setStyleSheet("background:#1e1e2e;color:#cdd6f4;"
                                    "border:1px solid #45475a;padding:4px;");
            lay->addWidget(codeView, 1);

            auto* btnBox = new QDialogButtonBox(dlg);
            auto* btnRun = btnBox->addButton(
                "\xe2\x96\xb6  Esegui", QDialogButtonBox::AcceptRole);
            btnBox->addButton("\xe2\x9c\x96  Annulla", QDialogButtonBox::RejectRole);
            btnRun->setStyleSheet(useSandbox
                ? "background:#16a34a;color:#fff;font-weight:bold;padding:4px 18px;"
                : "background:#ef4444;color:#fff;font-weight:bold;padding:4px 18px;");
            connect(btnBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
            connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
            lay->addWidget(btnBox);

            const bool accepted = (dlg->exec() == QDialog::Accepted);
            dlg->deleteLater();
            if (!accepted) {
                /* Inserisce banner "Riesegui" nel log per poter eseguire dopo */
                const int execId = m_codeBlockCounter++;
                m_pendingExecCodes[execId] = ec.lang + ":" + pyCode;
                const QString lnk =
                    "color:#fbbf24;font-size:12px;text-decoration:none;"
                    "border:1px solid #92400e;padding:2px 12px;"
                    "background:#292524;border-radius:4px;";
                m_log->moveCursor(QTextCursor::End);
                m_log->insertHtml(
                    "<p style='margin:4px 0;'>"
                      "<span style='color:#94a3b8;font-size:11px;'>"
                        "\xe2\x9a\xa0 Esecuzione annullata. "  /* ⚠ */
                      "</span>"
                      "<a href='exec:run:" + QString::number(execId) + "' "
                         "style='" + lnk + "' "
                         "title='Esegui il codice ora'>"
                        "\xe2\x96\xb6 Riesegui in sandbox"    /* ▶ */
                      "</a>"
                    "</p>");
                advancePipeline();
                return;
            }
        }

        m_executorOutput.clear();
        if (m_execProc) { m_execProc->kill(); m_execProc->deleteLater(); m_execProc = nullptr; }
        m_execProc = new QProcess(this);
        m_execProc->setProcessChannelMode(QProcess::MergedChannels);
        auto tmr = QSharedPointer<QElapsedTimer>::create();
        tmr->start();

        /* ── C / C++: compilazione locale gcc/g++ ─────────────────────────── */
        if (ec.lang == "c" || ec.lang == "cpp") {
            const QString ext = (ec.lang == "cpp") ? ".cpp" : ".c";
            const QString compiler = (ec.lang == "cpp") ? "g++" : "gcc";
            QTemporaryFile srcTmp(PrismaluxPaths::safeTempPath() + "/prisma_src_XXXXXX" + ext);
            srcTmp.setAutoRemove(false);
            if (!srcTmp.open()) { advancePipeline(); return; }
            srcTmp.write(pyCode.toUtf8());
            srcTmp.close();
            const QString srcPath = srcTmp.fileName();
            const QString binPath = srcPath.left(srcPath.lastIndexOf('.'));

            connect(m_execProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, srcPath, binPath, tmr, compiler](int exitCode, QProcess::ExitStatus) {
                const double ms = tmr->elapsed();
                const QString out = QString::fromUtf8(m_execProc->readAll());
                m_execProc->deleteLater();
                m_execProc = nullptr;
                QFile::remove(srcPath);

                if (exitCode != 0) {
                    /* Errore di compilazione */
                    const QString od = PrismaluxPaths::sanitizeErrorOutput(out);
                    QTextCursor c(m_log->document());
                    c.movePosition(QTextCursor::End);
                    c.insertHtml(buildToolStrip(QString(), od, exitCode, ms));
                    m_executorOutput = out;
                    if (m_cfgDlg->controllerEnabled()) runPipelineController();
                    else advancePipeline();
                    return;
                }

                /* Compilazione OK: esegue il binario */
                QFile::remove(binPath);  /* cleanup esito precedente */
                auto* runProc = new QProcess(this);
                runProc->setProcessChannelMode(QProcess::MergedChannels);
                auto tmr2 = QSharedPointer<QElapsedTimer>::create();
                tmr2->start();
                connect(runProc,
                        QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [this, runProc, binPath, tmr2](int rc, QProcess::ExitStatus) {
                    const double ms2 = tmr2->elapsed();
                    const QString out2 = QString::fromUtf8(runProc->readAll());
                    runProc->deleteLater();
                    QFile::remove(binPath);
                    m_executorOutput = out2;
                    const QString od2 = PrismaluxPaths::sanitizeErrorOutput(out2);
                    QTextCursor c(m_log->document());
                    c.movePosition(QTextCursor::End);
                    c.insertHtml(buildToolStrip(QString(), od2, rc, ms2));
                    if (!m_userScrolled) { m_suppressScrollSig = true;
                        m_log->ensureCursorVisible(); m_suppressScrollSig = false; }
                    if (m_cfgDlg->controllerEnabled()) runPipelineController();
                    else advancePipeline();
                });
                QTimer::singleShot(10000, this, [this, runProc, binPath]{
                    if (runProc && runProc->state() != QProcess::NotRunning) {
                        runProc->kill(); QFile::remove(binPath);
                        m_executorOutput = "[timeout 10s]";
                        advancePipeline();
                    }
                });
                runProc->start(binPath, {});
            });

            m_execProc->start(compiler,
                { "-o", binPath, srcPath, "-lm", "-Wall", "-Wextra" });
            return;
        }

        if (useSandbox) {
            /* ── Sandbox Docker: stdin piping, rete/filesystem isolati ─────── */
            const QSettings ss("Prismalux", "GUI");
            const QString img = ss.value(P::SK::kSandboxImage,   "python:3.11-slim").toString();
            const QString mem = QString::number(
                ss.value(P::SK::kSandboxMemory, 256).toInt()) + "m";

            connect(m_execProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, tmr](int exitCode, QProcess::ExitStatus) {
                const double ms = tmr->elapsed();
                const QString out = QString::fromUtf8(m_execProc->readAll());
                m_execProc->deleteLater();
                m_execProc = nullptr;
                m_executorOutput = out;
                const QString outDisplay = PrismaluxPaths::sanitizeErrorOutput(out);
                QTextCursor c(m_log->document());
                c.movePosition(QTextCursor::End);
                c.insertHtml(buildToolStrip(QString(), outDisplay, exitCode, ms));
                if (!m_userScrolled) {
                    m_suppressScrollSig = true;
                    m_log->ensureCursorVisible();
                    m_suppressScrollSig = false;
                }
                if (m_cfgDlg->controllerEnabled()) runPipelineController();
                else advancePipeline();
            });

            connect(m_execProc, &QProcess::errorOccurred,
                    this, [this](QProcess::ProcessError err){
                if (err == QProcess::FailedToStart) {
                    m_execProc->deleteLater(); m_execProc = nullptr;
                    advancePipeline();
                }
            });

            m_execProc->start(P::findDocker(), {
                "run", "--rm", "--network", "none",
                "--memory", mem, "--cpus", "0.5",
                "--pids-limit", "64", "-i",
                img, "python3", "-"
            });
            if (m_execProc->waitForStarted(P::kProcessHeavyStartMs)) {
                m_execProc->write(pyCode.toUtf8());
                m_execProc->closeWriteChannel();
            }
            QTimer::singleShot(30000, this, [this]{
                if (m_execProc && m_execProc->state() != QProcess::NotRunning) {
                    m_execProc->kill();
                    m_executorOutput = "[timeout sandbox 30s]";
                    if (m_cfgDlg->controllerEnabled()) runPipelineController();
                    else advancePipeline();
                }
            });

        } else {
            /* ── Python locale: file temporaneo + pip install retry ─────────
               [B1] QSharedPointer evita memory leak se il processo viene
               distrutto prima che finished() scatti. */
            QTemporaryFile execTmp(
                PrismaluxPaths::safeTempPath() + "/prisma_exec_XXXXXX.py");
            execTmp.setAutoRemove(false);
            if (!execTmp.open()) { advancePipeline(); return; }
            execTmp.write(pyCode.toUtf8());
            execTmp.close();
            const QString tmpPath = execTmp.fileName();

            connect(m_execProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, tmpPath, tmr](int exitCode, QProcess::ExitStatus) {
                const double ms = tmr->elapsed();
                QString out = QString::fromUtf8(m_execProc->readAll());
                m_execProc->deleteLater();
                m_execProc = nullptr;

                /* [C2] Auto-install modulo mancante con conferma utente */
                static QRegularExpression reModule(
                    "ModuleNotFoundError: No module named '([^']+)'");
                auto mMatch = reModule.match(out);
                if (exitCode != 0 && mMatch.hasMatch()) {
                    const QString pkg = mMatch.captured(1).split('.').first();
                    const int ans = QMessageBox::warning(this,
                        "\xe2\x9a\xa0  Installa pacchetto Python?",
                        QString("Il codice richiede il pacchetto <b>%1</b> non installato."
                                "<br><br>"
                                "\xe2\x9a\xa0  <b>Attenzione</b>: il nome viene da un"
                                " suggerimento dell\xe2\x80\x99" "AI.<br>"
                                "Verifica che <code>%1</code> sia il pacchetto corretto"
                                " su pypi.org prima di procedere.<br><br>"
                                "Eseguire <code>pip install %1</code>?").arg(pkg),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No);
                    if (ans != QMessageBox::Yes) {
                        QFile::remove(tmpPath);
                        QTextCursor logC(m_log->document());
                        logC.movePosition(QTextCursor::End);
                        logC.insertHtml(QString(
                            "<div style='color:#f87171;margin:4px 0'>"
                            "\xe2\x9d\x8c  Installazione di \xe2\x80\x98%1\xe2\x80\x99"
                            " annullata.</div>").arg(pkg));
                        m_executorOutput = out;
                        if (m_cfgDlg->controllerEnabled()) runPipelineController();
                        else advancePipeline();
                        return;
                    }
                    QTextCursor logC(m_log->document());
                    logC.movePosition(QTextCursor::End);
                    logC.insertHtml(QString(
                        "<div style='color:#facc15;font-style:italic;margin:4px 0'>"
                        "\xf0\x9f\x93\xa6  Installo '%1' via pip...</div>").arg(pkg));
                    if (!m_userScrolled) m_log->ensureCursorVisible();
                    auto* pip = new QProcess(this);
                    pip->setProcessChannelMode(QProcess::MergedChannels);
                    connect(pip, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                            this, [this, pip, tmpPath, pkg](int rc, QProcess::ExitStatus) {
                        const QString pipOut = QString::fromUtf8(pip->readAll()).trimmed();
                        pip->deleteLater();
                        QTextCursor logC2(m_log->document());
                        logC2.movePosition(QTextCursor::End);
                        if (rc == 0) {
                            logC2.insertHtml(QString(
                                "<div style='color:#4ade80;margin:4px 0'>"
                                "\xe2\x9c\x85  '%1' installato. Riprovo...</div>").arg(pkg));
                            if (!m_userScrolled) m_log->ensureCursorVisible();
                            auto* retry = new QProcess(this);
                            retry->setProcessChannelMode(QProcess::MergedChannels);
                            auto t2 = QSharedPointer<QElapsedTimer>::create();
                            t2->start();
                            connect(retry, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                                    this, [this, retry, tmpPath, t2](int rc2, QProcess::ExitStatus) {
                                const double ms2 = t2->elapsed();
                                QString out2 = QString::fromUtf8(retry->readAll());
                                retry->deleteLater();
                                QFile::remove(tmpPath);
                                m_executorOutput = out2;
                                QTextCursor c2(m_log->document());
                                c2.movePosition(QTextCursor::End);
                                c2.insertHtml(buildToolStrip(QString(),
                                    PrismaluxPaths::sanitizeErrorOutput(out2), rc2, ms2));
                                if (!m_userScrolled) m_log->ensureCursorVisible();
                                if (m_cfgDlg->controllerEnabled()) runPipelineController();
                                else advancePipeline();
                            });
                            retry->start(PrismaluxPaths::findPython(), {tmpPath});
                        } else {
                            logC2.insertHtml(QString(
                                "<div style='color:#f87171;margin:4px 0'>"
                                "\xe2\x9d\x8c  pip install '%1' fallito.<br>"
                                "<code>pip install %1</code></div>").arg(pkg));
                            LogBus::post(QString("\xe2\x9d\x8c AI Pipeline: pip install '%1' fallito.").arg(pkg));
                            QFile::remove(tmpPath);
                            m_executorOutput = pipOut;
                            if (m_cfgDlg->controllerEnabled()) runPipelineController();
                            else advancePipeline();
                        }
                    });
                    pip->start(PrismaluxPaths::findPython(), {"-m", "pip", "install", pkg,
                        "--quiet", "--break-system-packages",
                        "--trusted-host", "pypi.org",
                        "--trusted-host", "files.pythonhosted.org"});
                    return;
                }

                QFile::remove(tmpPath);
                m_executorOutput = out;
                const QString outDisplay = PrismaluxPaths::sanitizeErrorOutput(out);
                QTextCursor c(m_log->document());
                c.movePosition(QTextCursor::End);
                c.insertHtml(buildToolStrip(QString(), outDisplay, exitCode, ms));
                if (!m_userScrolled) {
                    m_suppressScrollSig = true;
                    m_log->ensureCursorVisible();
                    m_suppressScrollSig = false;
                }
                if (m_cfgDlg->controllerEnabled()) runPipelineController();
                else advancePipeline();
            });

            connect(m_execProc, &QProcess::errorOccurred,
                    this, [this](QProcess::ProcessError err){
                if (err == QProcess::FailedToStart) {
                    m_execProc->deleteLater(); m_execProc = nullptr;
                    advancePipeline();
                }
            });
            m_execProc->start(PrismaluxPaths::findPython(), {tmpPath});
        }

    } else {
        /* Nessun codice trovato: avanza direttamente */
        advancePipeline();
    }
    m_autoRetryActive = false;  /* reset sempre alla fine (TASK-5) */
}

/* ══════════════════════════════════════════════════════════════
   _saveAutoSearchToRag — retry "LLM incerto" riuscito: scrive
   RAG/RICERCA/<ts>_<slug>.md con domanda + risultati web + sintesi
   (stesso formato e stesso segnale di ingest del percorso manuale
   websearch:/insertinfo: in main_ai_slots.cpp), così le domande
   simili future pescano dal RAG senza rifare la ricerca.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_saveAutoSearchToRag(const QString& synthesis)
{
    const QString ragDir = P::ragDir() + "/RICERCA";
    QDir().mkpath(ragDir);
    QString slug = m_taskOriginal.left(40);
    slug.replace(QRegularExpression(
        "[^a-zA-Z0-9_\xc3\xa0\xc3\xa8\xc3\xac\xc3\xb2\xc3\xb9 ]"), "_");
    slug = slug.simplified().replace(' ', '_');
    const QString outFile = ragDir + "/" +
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_" + slug + ".md";

    QFile f(outFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[auto-search] salvataggio RAG fallito:" << outFile;
        return;
    }
    const QString md =
        "# Ricerca: " + m_taskOriginal + "\n"
        "Data: " + QDate::currentDate().toString(Qt::ISODate) + "\n\n"
        "## Risultati web\n" + m_autoRetrySearchResults.trimmed() + "\n\n"
        "## Sintesi\n" + synthesis.trimmed() + "\n";
    f.write(md.toUtf8());
    f.close();

    /* Iniezione immediata nel RAG inline della chat: il file su disco serve
       solo al prossimo ingest del Grafo RAG (nessun receiver automatico di
       onlineSearchResultReady) — addEntry rende i dati subito disponibili
       alle prossime domande di QUESTA sessione. Dedup per nome interno. */
    if (m_ragInline)
        m_ragInline->addEntry(
            "\xf0\x9f\x8c\x90 Ricerca: " + m_taskOriginal.left(60),
            m_autoRetrySearchResults.trimmed() +
            "\n\nSintesi:\n" + synthesis.trimmed());

    m_log->moveCursor(QTextCursor::End);
    m_log->insertHtml(
        "<p style='color:#4ade80;font-size:11px;margin:4px 0;'>"
        "\xe2\x9c\x85  Ricerca e sintesi salvate in RAG/RICERCA e aggiunte "
        "al contesto RAG di questa chat.</p>");
    emit onlineSearchResultReady(outFile, m_taskOriginal);
}

/* ══════════════════════════════════════════════════════════════
   _emitContextUsage — stima i token già impegnati nella finestra
   di contesto (storia chat compressa + RAG inline + RAG condiviso,
   ~4 caratteri/token) e li emette per l'indicatore 🧠 nell'header.
   È una stima: il system prompt e l'overhead del formato chat non
   sono conteggiati (ordine di grandezza: poche centinaia di token).
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_emitContextUsage()
{
    qsizetype chars = 0;
    if (m_ctxSingle) {
        const QJsonArray ctx = m_ctxSingle->buildContext();
        for (const QJsonValue& v : ctx)
            chars += v.toObject().value("content").toString().size();
    }
    if (m_ragInline && m_ragInline->hasContext())
        chars += m_ragInline->ragContext().size();
    if (m_cfgDlg && m_cfgDlg->sharedRagWidget()
        && m_cfgDlg->sharedRagWidget()->hasContext())
        chars += m_cfgDlg->sharedRagWidget()->ragContext().size();

    const int usedTok = static_cast<int>(chars / 4);
    emit contextUsage(usedTok, AiChatParams::load().num_ctx);
}

/* ══════════════════════════════════════════════════════════════
   buildEnabledTools — array tools filtrato dalle checkbox del pannello
   E (D-29) dalla categoria della query corrente per i tool pesanti.
   Restituisce solo i tool con nome presente in m_enabledTools.
   Se m_enabledTools è vuoto, restituisce array vuoto (tool use disattivato).
   ══════════════════════════════════════════════════════════════ */
QJsonArray AgentiPage::buildEnabledTools(const QString& query) const
{
    const QJsonArray all = _buildOllamaTools();
    if (m_enabledTools.isEmpty()) return QJsonArray{};

    const QSet<QString> relevantHeavy = _relevantHeavyTools(query);

    QJsonArray out;
    for (const QJsonValue& v : all) {
        const QString name = v.toObject()["function"].toObject()["name"].toString();
        if (!m_enabledTools.contains(name)) continue;
        if (kHeavyTools.contains(name) && !relevantHeavy.contains(name)) continue;
        out.append(v);
    }
    return out;
}


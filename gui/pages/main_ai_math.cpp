#include "main_ai.h"
#include "main_ai_p.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
namespace P = PrismaluxPaths;
#include <cmath>
#include <cstdlib>
#include <QRegularExpression>
#include <QMessageBox>
#include <QFile>
#include <QSettings>
#include <QVector>
#include <QPair>

/* ══════════════════════════════════════════════════════════════
   GUARDIA MATEMATICA LOCALE
   (invariata — parser recursive descent)
   ══════════════════════════════════════════════════════════════ */

static const char* _gp_ptr;
static bool        _gp_err;

static void   _gp_ws()   { while(*_gp_ptr==' '||*_gp_ptr=='\t') _gp_ptr++; }
static double _gp_expr();

static double _gp_prim() {
    _gp_ws();
    if (*_gp_ptr=='(') {
        _gp_ptr++; double v=_gp_expr(); _gp_ws();
        if (*_gp_ptr==')') _gp_ptr++;
        return v;
    }
    if (isalpha((unsigned char)*_gp_ptr)) {
        const char* s=_gp_ptr;
        while (isalpha((unsigned char)*_gp_ptr)||isdigit((unsigned char)*_gp_ptr)) _gp_ptr++;
        int len=(int)(_gp_ptr-s); char nm[32]={};
        for(int i=0;i<len&&i<31;i++) nm[i]=tolower((unsigned char)s[i]);
        _gp_ws();
        if (!strcmp(nm,"pi"))  return M_PI;
        if (!strcmp(nm,"phi")) return 1.6180339887498948;
        if (!strcmp(nm,"e") && *_gp_ptr!='(') return M_E;
        if (*_gp_ptr=='(') {
            _gp_ptr++;
            double a=_gp_expr(); _gp_ws();
            double b=a;
            if (*_gp_ptr==',') { _gp_ptr++; b=_gp_expr(); _gp_ws(); }
            if (*_gp_ptr==')') _gp_ptr++;
            if (!strcmp(nm,"sqrt")||!strcmp(nm,"radice")) return a>=0?std::sqrt(a):NAN;
            if (!strcmp(nm,"cbrt"))  return std::cbrt(a);
            if (!strcmp(nm,"abs"))   return std::fabs(a);
            if (!strcmp(nm,"floor")) return std::floor(a);
            if (!strcmp(nm,"ceil"))  return std::ceil(a);
            if (!strcmp(nm,"round")) return std::round(a);
            if (!strcmp(nm,"trunc")) return std::trunc(a);
            if (!strcmp(nm,"sign"))  return (double)((a>0)-(a<0));
            if (!strcmp(nm,"exp"))   return std::exp(a);
            if (!strcmp(nm,"sin"))   return std::sin(a);
            if (!strcmp(nm,"cos"))   return std::cos(a);
            if (!strcmp(nm,"tan"))   return std::tan(a);
            if (!strcmp(nm,"asin")||!strcmp(nm,"arcsin")) return std::asin(a);
            if (!strcmp(nm,"acos")||!strcmp(nm,"arccos")) return std::acos(a);
            if (!strcmp(nm,"atan")||!strcmp(nm,"arctan")) return std::atan(a);
            if (!strcmp(nm,"atan2")) return std::atan2(a,b);
            if (!strcmp(nm,"log")||!strcmp(nm,"ln"))   return std::log(a);
            if (!strcmp(nm,"log2"))  return std::log2(a);
            if (!strcmp(nm,"log10")||!strcmp(nm,"lg")) return std::log10(a);
            if (!strcmp(nm,"logb"))  return b!=1?std::log(a)/std::log(b):NAN;
            if (!strcmp(nm,"min"))   return std::min(a,b);
            if (!strcmp(nm,"max"))   return std::max(a,b);
            if (!strcmp(nm,"pow"))   return std::pow(a,b);
            if (!strcmp(nm,"gcd")||!strcmp(nm,"mcd")) {
                long long ia=(long long)std::fabs(a),ib=(long long)std::fabs(b);
                while(ib){long long t=ib;ib=ia%ib;ia=t;} return (double)ia;
            }
            if (!strcmp(nm,"lcm")||!strcmp(nm,"mcm")) {
                long long ia=(long long)std::fabs(a),ib=(long long)std::fabs(b),g=ia,tb=ib;
                while(tb){long long t=tb;tb=g%tb;g=t;} return ia?(double)(ia/g*ib):0;
            }
            return a;
        }
        _gp_err=true; return 0.0;
    }
    char *e; double v=strtod(_gp_ptr,&e);
    if (e!=_gp_ptr){_gp_ptr=e;return v;}
    return 0.0;
}
static double _gp_pow() {
    _gp_ws(); int neg=(*_gp_ptr=='-'); if(neg||*_gp_ptr=='+') _gp_ptr++;
    double v=_gp_prim(); _gp_ws();
    if (*_gp_ptr=='^'){_gp_ptr++;v=std::pow(v,_gp_pow());}
    /* '!' = fattoriale SOLO se non e' '!=' (operatore disuguaglianza) */
    if (*_gp_ptr=='!' && *(_gp_ptr+1)!='='){_gp_ptr++;long long n=(long long)std::fabs(v),f=1;
        for(long long i=2;i<=n&&i<=20;i++)f*=i;v=(double)f;}
    return neg?-v:v;
}
static double _gp_term() {
    double v=_gp_pow();
    while(true){_gp_ws();char op=*_gp_ptr;
        if(op=='*'||op=='/'||op=='%'||op==':'){_gp_ptr++;double r=_gp_pow();
            if(op=='*')v*=r;
            else if(op=='/'||op==':')v=(r?v/r:NAN);
            else v=(r?std::fmod(v,r):NAN);}
        else break;}
    return v;
}
static double _gp_expr() {
    double v=_gp_term();
    while(true){_gp_ws();
        if(*_gp_ptr=='+'){_gp_ptr++;v+=_gp_term();}
        else if(*_gp_ptr=='-'){_gp_ptr++;v-=_gp_term();}
        else break;}
    return v;
}
static QString _gp_fmt(double v) {
    if (std::isnan(v))  return "errore (dominio non valido)";
    if (std::isinf(v))  return v>0?"infinito":"-infinito";
    if (v==(long long)v && std::fabs(v)<1e15) return QString::number((long long)v);
    return QString::number(v,'g',10);
}
static bool _gp_try(const QByteArray& ba, double& out) {
    _gp_err=false; _gp_ptr=ba.constData();
    out=_gp_expr(); _gp_ws();
    return !_gp_err && !*_gp_ptr;
}
/* ── Anti-prompt-injection ────────────────────────────────────────────────
   Rimuove o neutralizza sequenze che potrebbero indurre il modello a ignorare
   le istruzioni di sistema (role hijack) o a leakare dati interni.
   Applicata su: task utente, contesto RAG, testo documento allegato.

   Pattern neutralizzati:
     - Format token ChatML / Llama2 / Mistral (im_start, INST, SYS, ecc.)
     - Separatori di sezione "###" (usati da alcuni modelli per separare ruoli)
     - Role-override via doppio newline: "\n\nSystem:", "\n\nAssistant:", ecc.
     - Commenti HTML nascosti "<!-- "
     - Frasi di jailbreak comuni (case-insensitive)

   La funzione è conservativa: sostituisce i pattern con spazi per non
   alterare l'allineamento del testo; non rimuove caratteri legittimi.    */
/* ── Converti numeri italiani scritti in lettere → cifre ──────────────────
   La lista è ordinata per lunghezza decrescente: i termini più lunghi vengono
   testati prima così "diciassette" non viene spezzato da "dieci" o "sette".
   Usa word-boundary \b (Unicode-aware in Qt) per non toccare parti di parole:
   "cinquecento" non viene alterato da un eventuale match su "cinque" perché
   il motore processa prima "cinquecento" che è più lungo. ─────────────────── */
static QString normalizeNumbers(const QString& text)
{
    using P = QPair<QString,QString>;
    static const QVector<P> kMap = {
        /* ordinali (prima dei cardinali per non tagliare "primo" in "pri" + "mo") */
        {"decima",        "10\xc2\xb0"}, {"decimo",        "10\xc2\xb0"},
        {"nona",          "9\xc2\xb0"},  {"nono",          "9\xc2\xb0"},
        {"ottava",        "8\xc2\xb0"},  {"ottavo",        "8\xc2\xb0"},
        {"settima",       "7\xc2\xb0"},  {"settimo",       "7\xc2\xb0"},
        {"sesta",         "6\xc2\xb0"},  {"sesto",         "6\xc2\xb0"},
        {"quinta",        "5\xc2\xb0"},  {"quinto",        "5\xc2\xb0"},
        {"quarta",        "4\xc2\xb0"},  {"quarto",        "4\xc2\xb0"},
        {"terza",         "3\xc2\xb0"},  {"terzo",         "3\xc2\xb0"},
        {"seconda",       "2\xc2\xb0"},  {"secondo",       "2\xc2\xb0"},
        {"prima",         "1\xc2\xb0"},  {"primo",         "1\xc2\xb0"},
        /* grandi (prima delle centinaia/migliaia per evitare match parziali) */
        {"miliardo",      "1000000000"}, {"miliardi",      "1000000000"},
        {"milione",       "1000000"},    {"milioni",       "1000000"},
        /* migliaia composte */
        {"diecimila",     "10000"},
        {"novemila",      "9000"},       {"ottomila",      "8000"},
        {"settemila",     "7000"},       {"seimila",       "6000"},
        {"cinquemila",    "5000"},       {"quattromiła",   "4000"},
        {"quattromila",   "4000"},       {"tremila",       "3000"},
        {"duemila",       "2000"},
        {"mille",         "1000"},
        /* centinaia */
        {"novecento",     "900"},        {"ottocento",     "800"},
        {"settecento",    "700"},        {"seicento",      "600"},
        {"cinquecento",   "500"},        {"quattrocento",  "400"},
        {"trecento",      "300"},        {"duecento",      "200"},
        {"cento",         "100"},
        /* composti 11-19 (prima di dieci/sette/ecc.) */
        {"diciannove",    "19"},         {"diciassette",   "17"},
        {"diciotto",      "18"},         {"sedici",        "16"},
        {"quindici",      "15"},         {"quattordici",   "14"},
        {"tredici",       "13"},         {"dodici",        "12"},
        {"undici",        "11"},         {"dieci",         "10"},
        /* composti 21-29 */
        {"ventinove",     "29"},         {"ventotto",      "28"},
        {"ventisette",    "27"},         {"ventisei",      "26"},
        {"venticinque",   "25"},         {"ventiquattro",  "24"},
        {"ventitré",      "23"},         {"ventitre",      "23"},
        {"ventidue",      "22"},         {"ventuno",       "21"},
        {"venti",         "20"},
        /* composti 31-39 */
        {"trentanove",    "39"},         {"trentotto",     "38"},
        {"trentasette",   "37"},         {"trentasei",     "36"},
        {"trentacinque",  "35"},         {"trentaquattro", "34"},
        {"trentatré",     "33"},         {"trentatre",     "33"},
        {"trentadue",     "32"},         {"trentuno",      "31"},
        {"trenta",        "30"},
        /* composti 41-49 */
        {"quarantanove",  "49"},         {"quarantotto",   "48"},
        {"quarantasette", "47"},         {"quarantasei",   "46"},
        {"quarantacinque","45"},         {"quarantaquattro","44"},
        {"quarantatré",   "43"},         {"quarantatre",   "43"},
        {"quarantadue",   "42"},         {"quarantuno",    "41"},
        {"quaranta",      "40"},
        /* decine 50-90 */
        {"novanta",       "90"},         {"ottanta",       "80"},
        {"settanta",      "70"},         {"sessanta",      "60"},
        {"cinquanta",     "50"},
        /* unità 0-9 */
        {"zero",          "0"},
        {"una",           "1"},          {"uno",           "1"},
        {"due",           "2"},          {"tre",           "3"},
        {"quattro",       "4"},          {"cinque",        "5"},
        {"sei",           "6"},          {"sette",         "7"},
        {"otto",          "8"},          {"nove",          "9"},
    };

    QString result = text;
    for (const auto& entry : kMap) {
        const QRegularExpression re(
            "\\b" + QRegularExpression::escape(entry.first) + "\\b",
            QRegularExpression::CaseInsensitiveOption |
            QRegularExpression::UseUnicodePropertiesOption);
        result.replace(re, entry.second);
    }
    return result;
}

QString _sanitize_prompt(const QString& raw)
{
    /* converti prima i numeri in lettere → cifre */
    QString s = normalizeNumbers(raw);

    /* 1. Format token — rimpiazza con spazio */
    static const char* const kTokens[] = {
        "<|im_start|>", "<|im_end|>", "<|endoftext|>", "<|eot_id|>",
        "[INST]", "[/INST]", "<<SYS>>", "<</SYS>>",
        "<s>", "</s>",
        "###",
        "<!-- ",
        nullptr
    };
    for (int i = 0; kTokens[i]; ++i) {
        s.replace(QString::fromUtf8(kTokens[i]),
                  QString(strlen(kTokens[i]), u' '));
    }

    /* 2. Role-override via "\n\n<Parola>:" — rimuove solo il doppio newline */
    static const char* const kRoles[] = {
        "\n\nSystem:", "\n\nAssistant:", "\n\nUser:",
        "\n\nHuman:", "\n\nAI:", "\n\nIgnore",
        nullptr
    };
    for (int i = 0; kRoles[i]; ++i) {
        /* Sostituisce "\n\n" con "  " lasciando il testo */
        QString pat = QString::fromUtf8(kRoles[i]);
        int idx = 0;
        while ((idx = s.indexOf(pat, idx)) != -1) {
            s[idx]   = u' ';
            s[idx+1] = u' ';
            idx += 2;
        }
    }

    /* NOTA SICUREZZA [C3]: NON esiste un filtro testuale affidabile contro
       il prompt injection. Qualsiasi lista di frasi da bloccare è bypassabile
       con testo in italiano, altre lingue, unicode lookalike, split delle
       parole o semplice parafrasi.
       La protezione reale contro il prompt injection è:
         - non eseguire automaticamente azioni irreversibili (C1: dialog conferma)
         - non fidarsi dell'output LLM come fonte di comandi sicuri
       Il blocco kPhrases[] che filtrava "ignore previous instructions" ecc.
       è stato rimosso perché dava falsa sicurezza senza protezione concreta. */

    return s;
}

/* Rileva inglese contando articoli e parole funzionali comuni.
   Soglia: almeno 2 hit su testo di qualsiasi lunghezza → probabile inglese. */
bool _is_likely_english(const QString& text)
{
    static const QStringList kWords = {
        " the ", " a ", " an ", " is ", " are ", " was ", " were ",
        " it ", " its ", " this ", " that ", " these ", " those ",
        " in ", " on ", " at ", " of ", " to ", " for ", " with ",
        " from ", " by ", " as ", " and ", " or ", " not ", " but ",
        " have ", " has ", " had ", " do ", " does ", " did ",
        " will ", " would ", " can ", " could ", " should ", " may ",
        " you ", " he ", " she ", " we ", " they ", " me ", " him ",
        " your ", " his ", " her ", " our ", " their ", " my ",
        " write ", " create ", " make ", " build ", " generate ",
        " explain ", " describe ", " list ", " show ", " find ",
        " how ", " what ", " why ", " when ", " where ", " who ",
        " please ", " help ", " using ", " function ", " class ",
        " code ", " program ", " script ", " file ", " data ",
        "the "  /* anche a inizio frase — rimosso "a " (falsi positivi con "da ") */
    };
    QString low = " " + text.toLower() + " ";
    int hits = 0;
    for (const QString& w : kWords) {
        if (low.contains(w)) {
            hits++;
            if (hits >= 2) return true;
        }
    }
    return false;
}

/* Aggiunge al system prompt la nota "calcoli già fatti" se il task contiene [=N] */
QString _math_sys(const QString& task, const QString& base) {
    if (task.contains("[="))
        return base + " I valori nella forma 'espressione[=N]' nel task sono stati "
               "pre-calcolati da un motore matematico C collaudato: quei risultati sono "
               "corretti e definitivi, non ricalcolarli.";
    return base;
}

/* ── _buildSys ────────────────────────────────────────────────────────────────
 * Compone il system prompt finale applicando in ordine:
 *   1. _math_sys()   — inietta nota pre-calcoli se il task contiene [=N]
 *   2. adattamento per famiglia di modello:
 *      • LlamaLocal       → prompt minimo (≤ 2 frasi) per risparmiare contesto
 *      • modelli reasoning → aggiunge istruzione di risposta diretta
 *        (qwen3, qwq, deepseek-r1, marco-o1: hanno already internal <think>)
 *      • modelli piccoli ≤4B → tronca a max 220 caratteri per non saturare
 *        il context window limitato
 * ──────────────────────────────────────────────────────────────────────────── */
/* ── _buildSys (5 argomenti) ──────────────────────────────────────────────────
 * full       — prompt completo per modelli 7B+
 * small      — prompt corto per modelli ≤4B  (vuoto → fallback a full)
 * Flusso:
 *   1. Seleziona full o small in base a taglia modello / backend
 *   2. Inietta nota pre-calcoli _math_sys se task contiene [=N]
 *   3. LlamaLocal → tronca a max 2 frasi (contesto limitato)
 *   4. Reasoning (qwen3/qwq/deepseek-r1) → aggiunge istruzione risposta diretta
 * ──────────────────────────────────────────────────────────────────────────── */
QString _buildSys(const QString& task,
                          const QString& full, const QString& small,
                          const QString& modelName, AiClient::Backend backend)
{
    const QString ml = modelName.toLower();

    /* Rileva modelli piccoli (≤4.5B) dal nome */
    static QRegularExpression sizeRe(R"((\d+(?:\.\d+)?)\s*b\b)");
    auto sm = sizeRe.match(ml);
    const bool isSmall = sm.hasMatch() && sm.captured(1).toDouble() <= 4.5;

    /* Seleziona il prompt base: small se disponibile e il modello lo richiede */
    const bool useSmall = (backend == AiClient::LlamaLocal || isSmall) && !small.isEmpty();
    QString sys = _math_sys(task, useSmall ? small : full);

    /* ── LlamaLocal: massimo 2 frasi (priorità assoluta) ── */
    if (backend == AiClient::LlamaLocal) {
        int p1 = sys.indexOf(". ");
        if (p1 > 0) {
            int p2 = sys.indexOf(". ", p1 + 2);
            sys = sys.left(p2 > 0 ? p2 + 1 : p1 + 1);
        }
        return sys;
    }

    /* ── Reasoning models: rispondi direttamente (dopo la selezione prompt) ── */
    const bool isReasoning =
        ml.contains("qwen3")       || ml.contains("qwq")   ||
        ml.contains("deepseek-r1") || ml.contains("r1-")   ||
        ml.contains("-r1:")        || ml.contains("marco-o1");
    if (isReasoning)
        sys += " Nella risposta finale, vai direttamente al punto senza "
               "riformulare il processo di ragionamento.";

    /* Identità base: sempre iniettata per evitare "Non lo so chi sono" */
    static const QString kIdentity =
        "Il tuo nome e' Prismalux. Sei l'assistente AI integrato nell'applicazione Prismalux "
        "(sviluppata da Paolo). Quando ti viene chiesto come ti chiami o chi sei, "
        "rispondi sempre 'Prismalux'. ";
    /* Preferenza utente: report/confronti/elenchi leggibili con tabelle o
       liste Markdown invece di paragrafi lunghi — l'app le renderizza già
       correttamente (tabelle HTML vere, non testo con pipe grezze). */
    static const QString kFormatting =
        "Quando generi un report, un confronto, un elenco di dati, statistiche o un "
        "riepilogo, usa una tabella Markdown (riga intestazione | riga separatore "
        "|---|---| | riga dati) oppure un elenco puntato/numerato, invece di un lungo "
        "paragrafo di testo — l'interfaccia le mostra come vere tabelle formattate. ";
    sys = kIdentity + kFormatting + sys;

    const QString persona = P::personalityPrompt();
    if (!persona.isEmpty())
        sys = persona + "\n\n" + sys;
    return P::prependKnowledge(sys);
}

/* ── _buildSys (4 argomenti) — backward compat per prompt senza variante small ── */
QString _buildSys(const QString& task, const QString& full,
                          const QString& modelName, AiClient::Backend backend)
{
    return _buildSys(task, full, QString(), modelName, backend);
}

static bool _gp_is_prime(long long n) {
    if(n<2)return false; if(n<4)return true;
    if(n%2==0||n%3==0)return false;
    for(long long i=5;i*i<=n;i+=6) if(n%i==0||n%(i+2)==0)return false;
    return true;
}

/* ══════════════════════════════════════════════════════════════
   _gp_preprocess — normalizza la stringa prima di passarla al parser.
   Converte operatori alternativi in quelli riconosciuti dal parser.
   ══════════════════════════════════════════════════════════════ */
static QString _gp_preprocess(QString s) {
    s.replace("**", "^");          /* Python-style power: 3**4 → 3^4  */
    s.replace("\xc3\x97", "*");    /* × (U+00D7) → *                  */
    s.replace("\xc3\xb7", "/");    /* ÷ (U+00F7) → /                  */
    s.replace(",", ".");           /* virgola decimale → punto         */
    return s;
}

/* ══════════════════════════════════════════════════════════════
   injectMathResults — pre-elaborazione task ibridi (math + AI)
   Trova espressioni numeriche nel testo (es. "12345*6789") e le
   sostituisce con "12345*6789[=83810205]" prima di passare il
   task all'AI, così l'agente riceve già il valore calcolato.
   Restituisce il task modificato (invariato se nessuna expr trovata).
   ══════════════════════════════════════════════════════════════ */
QString _inject_math(const QString& task)
{
    /* Pre-pass: sqrt(X) / radq(X) → sqrt(X)[=result] */
    static QRegularExpression reSqrtInline(
        R"((?:sqrt|radq)\s*\(\s*([0-9]+(?:[.,][0-9]+)?(?:[eE][+\-]?[0-9]+)?)\s*\))");
    QString result = task;
    int off = 0;
    auto itSq = reSqrtInline.globalMatch(task);
    while (itSq.hasNext()) {
        auto m = itSq.next();
        double n = m.captured(1).replace(',','.').toDouble();
        double v = std::sqrt(n);
        QString rep = QString("sqrt(%1)[=%2]").arg(_gp_fmt(n)).arg(_gp_fmt(v));
        result.replace(off + m.capturedStart(0), m.capturedLength(0), rep);
        off += rep.length() - m.capturedLength(0);
    }

    /* Regex: sequenza di cifre/operatori/parentesi non banale (almeno un operatore) */
    static QRegularExpression reExpr(
        R"([\(\-]?[0-9]+(?:[.,][0-9]+)?(?:\s*[\+\-\*\/\^%]\s*[\(\-]?[0-9]+(?:[.,][0-9]+)?)+\)?)");
    int offset = 0;
    auto it = reExpr.globalMatch(result);
    while (it.hasNext()) {
        auto m = it.next();
        QString expr = m.captured(0).simplified();
        /* Normalizza: virgola → punto, spazi attorno op rimossi */
        QString normalized = _gp_preprocess(expr);
        normalized.remove(' ');
        QByteArray ba = normalized.toLatin1();
        double v;
        if (_gp_try(ba, v) && !std::isnan(v) && !std::isinf(v)) {
            QString replacement = QString("%1[=%2]").arg(expr).arg(_gp_fmt(v));
            result.replace(offset + m.capturedStart(0), m.capturedLength(0), replacement);
            offset += replacement.length() - m.capturedLength(0);
        }
    }
    return result;
}

QString AgentiPage::guardiaMath(const QString& input)
{
    /* Blocchi di codice: non intercettare mai come espressioni matematiche.
       Segnali inequivocabili di codice sorgente → passa direttamente all'AI. */
    if (input.contains('\n') && input.length() > 80) return {};
    static const QStringList codeKw = {
        "def ", "class ", "import ", "return ", "print(", "for ", "while ",
        "if (", "if(", "!= ", "==", "->", "=>", "#include", "public ", "void ",
        "np.", "pd.", "plt.", "os.", "sys.", "self."
    };
    for (const QString& kw : codeKw)
        if (input.contains(kw)) return {};

    QString low = input.toLower().trimmed();
    while (!low.isEmpty() && (low.back()=='?' || low.back()==' ')) low.chop(1);
    if (low.isEmpty()) return {};

    static const QStringList prefissi = {
        "quanto fa ", "quanto vale ", "quanto e' ", "quanto e ",
        "calcola ", "risultato di ", "dimmi ", "quanto risulta ",
        "quant'e' ", "quante fa ", "fammi ", "dimmi il risultato di ",
        "quanto fa' "
    };
    for (const QString& pref : prefissi) {
        if (low.startsWith(pref)) {
            QString expr = low.mid(pref.length()).trimmed();
            QByteArray ba = expr.toLatin1(); double v;
            if (_gp_try(ba,v))
                return QString("%1 = %2").arg(expr).arg(_gp_fmt(v));
            break;
        }
    }
    /* Cerca il prefisso anche in mezzo alla frase (es: "ciao quanto fa 5+5") */
    for (const QString& pref : prefissi) {
        int idx = low.indexOf(pref);
        if (idx > 0) {
            QString expr = low.mid(idx + pref.length()).trimmed();
            QByteArray ba = expr.toLatin1(); double v;
            if (_gp_try(ba,v))
                return QString("%1 = %2").arg(expr).arg(_gp_fmt(v));
        }
    }

    {
        /* sqrt/radq come input diretto: "sqrt(9)", "sqrt 9", "radq(9)", "radq 9" */
        static QRegularExpression reSqrt(
            R"(^(?:sqrt|radq)\s*\(?\s*([0-9]+(?:[.,][0-9]+)?(?:[eE][+\-]?[0-9]+)?)\s*\)?$)");
        auto ms=reSqrt.match(low);
        if(ms.hasMatch()){
            double n=ms.captured(1).replace(',','.').toDouble();
            return QString("sqrt(%1) = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::sqrt(n)));
        }
    }
    {
        static QRegularExpression re1("radice\\s+quadrata\\s+(?:di\\s+)?([0-9.e+\\-]+)");
        static QRegularExpression re2("radice\\s+cubica\\s+(?:di\\s+)?([0-9.e+\\-]+)");
        static QRegularExpression re3("radice\\s+(?:di\\s+)?([0-9.e+\\-]+)");
        auto m1=re1.match(low); if(m1.hasMatch()){double n=m1.captured(1).toDouble();return QString("sqrt(%1) = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::sqrt(n)));}
        auto m2=re2.match(low); if(m2.hasMatch()){double n=m2.captured(1).toDouble();return QString("cbrt(%1) = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::cbrt(n)));}
        auto m3=re3.match(low); if(m3.hasMatch()){double n=m3.captured(1).toDouble();return QString("sqrt(%1) = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::sqrt(n)));}
    }
    {
        static QRegularExpression re("log(?:aritmo)?\\s+(?:in\\s+)?base\\s+([0-9.]+)\\s+(?:di\\s+)?([0-9.e+\\-]+)");
        auto m=re.match(low);
        if(m.hasMatch()){
            double base=m.captured(1).toDouble(), n=m.captured(2).toDouble();
            return QString("log base %1 di %2 = %3").arg(_gp_fmt(base)).arg(_gp_fmt(n)).arg(_gp_fmt(std::log(n)/std::log(base)));
        }
    }
    {
        static QRegularExpression re("sconto\\s+([0-9.]+)%\\s+su\\s+([0-9.]+)");
        auto m=re.match(low);
        if(m.hasMatch()){
            double pct=m.captured(1).toDouble(), base=m.captured(2).toDouble();
            double risp=base*pct/100.0, fin=base-risp;
            return QString("Sconto %1% su %2\nRisparmio: %3   Prezzo finale: %4")
                   .arg(pct).arg(_gp_fmt(base)).arg(_gp_fmt(risp)).arg(_gp_fmt(fin));
        }
    }
    {
        static QRegularExpression re("([0-9.]+)%\\s+di\\s+([0-9.]+)");
        auto m=re.match(low);
        if(m.hasMatch()){
            double pct=m.captured(1).toDouble(), base=m.captured(2).toDouble();
            return QString("%1% di %2 = %3").arg(pct).arg(_gp_fmt(base)).arg(_gp_fmt(base*pct/100.0));
        }
    }
    {
        static QRegularExpression re("(?:numeri\\s+)?primi\\s+(?:da|tra|fra)\\s+([0-9]+)\\s+(?:a|e|fino\\s+a)\\s+([0-9]+)");
        auto m=re.match(low);
        if(m.hasMatch()){
            long long a=m.captured(1).toLongLong(), b=m.captured(2).toLongLong();
            if(b-a<=10000){
                QStringList lst;
                for(long long i=a;i<=b;i++) if(_gp_is_prime(i)) lst<<QString::number(i);
                if(lst.isEmpty()) return QString("Nessun numero primo tra %1 e %2.").arg(a).arg(b);
                return QString("Primi tra %1 e %2 (%3 totali):\n%4").arg(a).arg(b).arg(lst.size()).arg(lst.join(", "));
            }
        }
    }
    {
        /* "i primi N numeri primi" / "dimmi i primi N primi" / "elenca i primi N numeri primi" */
        static QRegularExpression re("(?:(?:dimmi|elenca|lista|calcola|trova|mostrami)\\s+)?(?:i\\s+)?primi\\s+([0-9]+)\\s+(?:numeri\\s+)?prim(?:i|o)");
        auto m = re.match(low);
        if (m.hasMatch()) {
            long long n = m.captured(1).toLongLong();
            if (n > 0 && n <= 1000) {
                QStringList lst;
                long long num = 2;
                while ((long long)lst.size() < n) {
                    if (_gp_is_prime(num)) lst << QString::number(num);
                    num++;
                }
                return QString("Primi %1 numeri primi:\n%2").arg(n).arg(lst.join(", "));
            }
        }
    }
    {
        static QRegularExpression re("([0-9]+)\\s+(?:e'|è|e)\\s+primo");
        static QRegularExpression re2("(?:e'|è)\\s+primo\\s+([0-9]+)");
        static QRegularExpression re3("primo\\s+([0-9]+)");
        auto mm=re.match(low); if(!mm.hasMatch()) mm=re2.match(low); if(!mm.hasMatch()) mm=re3.match(low);
        if(mm.hasMatch()){
            long long n=mm.captured(1).toLongLong();
            return QString("%1 %2 un numero primo.").arg(n).arg(_gp_is_prime(n)?"è":"non è");
        }
    }
    {
        static QRegularExpression re("somm(?:a|atoria)\\s+(?:da|da\\s+1\\s+a)?\\s*([0-9]+)\\s+(?:a|fino\\s+a)\\s+([0-9]+)");
        auto m=re.match(low);
        if(m.hasMatch()){
            long long a=m.captured(1).toLongLong(), b=m.captured(2).toLongLong();
            return QString("Somma da %1 a %2 = %3").arg(a).arg(b).arg((b-a+1)*(a+b)/2);
        }
        static QRegularExpression re2("somm(?:a|atoria)\\s+(?:da\\s+1\\s+a|dei\\s+primi)\\s+([0-9]+)");
        auto m2=re2.match(low);
        if(m2.hasMatch()){
            long long n=m2.captured(1).toLongLong();
            return QString("Somma 1..%1 = %2").arg(n).arg(n*(n+1)/2);
        }
    }
    /* ── Logaritmo naturale / ln ── */
    {
        static QRegularExpression re(
            "(?:log(?:aritmo)?\\s+naturale|ln)\\s+(?:di\\s+)?([0-9.e+\\-]+)");
        auto m = re.match(low);
        if (m.hasMatch()) {
            double n = m.captured(1).toDouble();
            if (n > 0)
                return QString("ln(%1) = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::log(n)));
        }
    }
    /* ── Logaritmo (base 10 implicita in italiano) ── */
    {
        static QRegularExpression re(
            "log(?:aritmo)?\\s+(?:di\\s+)?([0-9.e+\\-]+)(?!\\s+base|\\s+in\\s+base)");
        auto m = re.match(low);
        if (m.hasMatch() && !low.contains("base") && !low.contains("naturale")) {
            double n = m.captured(1).toDouble();
            if (n > 0)
                return QString("log10(%1) = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::log10(n)));
        }
    }
    /* ── Funzioni trigonometriche in linguaggio naturale ── */
    {
        struct TrigEntry { const char* pattern; const char* name; double(*fn)(double); };
        static const std::initializer_list<TrigEntry> trigs = {
            {"(?:seno|sin)\\s+(?:di\\s+)?([0-9.e+\\-]+)",          "sin",    std::sin },
            {"(?:coseno|cos)\\s+(?:di\\s+)?([0-9.e+\\-]+)",        "cos",    std::cos },
            {"(?:tangente|tan)\\s+(?:di\\s+)?([0-9.e+\\-]+)",      "tan",    std::tan },
            {"(?:arcoseno|arcsin|asin)\\s+(?:di\\s+)?([0-9.e+\\-]+)",  "asin", std::asin},
            {"(?:arcocoseno|arccos|acos)\\s+(?:di\\s+)?([0-9.e+\\-]+)","acos", std::acos},
            {"(?:arcotangente|arctan|atan)\\s+(?:di\\s+)?([0-9.e+\\-]+)","atan",std::atan},
        };
        bool gradi = low.contains("grad");  /* "in gradi" → converti deg→rad */
        for (const auto& t : trigs) {
            QRegularExpression re(t.pattern);
            auto m = re.match(low);
            if (m.hasMatch()) {
                double n = m.captured(1).toDouble();
                double arg = gradi ? (n * M_PI / 180.0) : n;
                double res = t.fn(arg);
                QString unit = gradi ? "°" : " rad";
                return QString("%1(%2%3) = %4").arg(t.name).arg(_gp_fmt(n)).arg(unit).arg(_gp_fmt(res));
            }
        }
    }
    /* ── Elevamento a potenza in linguaggio naturale ── */
    {
        static QRegularExpression re(
            "([0-9.]+)\\s+(?:elevato\\s+(?:alla\\s+)?|alla\\s+)?(?:potenza\\s+)?(?:di\\s+)?([0-9.]+)");
        static QRegularExpression reElevato("([0-9.]+)\\s+elevato\\s+a\\s+([0-9.]+)");
        auto m = reElevato.match(low);
        if (m.hasMatch()) {
            double base = m.captured(1).toDouble(), exp = m.captured(2).toDouble();
            return QString("%1^%2 = %3").arg(_gp_fmt(base)).arg(_gp_fmt(exp)).arg(_gp_fmt(std::pow(base,exp)));
        }
    }
    /* ── Quadrato / cubo ── */
    {
        static QRegularExpression reQ("(?:quadrato|quadra)\\s+(?:di\\s+)?([0-9.]+)");
        static QRegularExpression reC("cubo\\s+(?:di\\s+)?([0-9.]+)");
        auto mq = reQ.match(low);
        if (mq.hasMatch()) {
            double n = mq.captured(1).toDouble();
            return QString("%1² = %2").arg(_gp_fmt(n)).arg(_gp_fmt(n*n));
        }
        auto mc = reC.match(low);
        if (mc.hasMatch()) {
            double n = mc.captured(1).toDouble();
            return QString("%1³ = %2").arg(_gp_fmt(n)).arg(_gp_fmt(n*n*n));
        }
    }
    /* ── Fattoriale ── */
    {
        /* Lookahead negativo (?!=): esclude '!=' (operatore disuguaglianza Python/C) */
        static QRegularExpression re("(?:fattoriale\\s+(?:di\\s+)?)?([0-9]+)\\s*!(?!=)");
        static QRegularExpression re2("fattoriale\\s+(?:di\\s+)?([0-9]+)");
        auto m = re.match(low);
        if (!m.hasMatch()) m = re2.match(low);
        if (m.hasMatch()) {
            int n = m.captured(1).toInt();
            if (n >= 0 && n <= 20) {
                long long f = 1;
                for (int i = 2; i <= n; i++) f *= i;
                return QString("%1! = %2").arg(n).arg(f);
            }
        }
    }
    /* ── Valore assoluto ── */
    {
        static QRegularExpression re("(?:valore\\s+assoluto|modulo)\\s+(?:di\\s+)?([\\-]?[0-9.]+)");
        auto m = re.match(low);
        if (m.hasMatch()) {
            double n = m.captured(1).toDouble();
            return QString("|%1| = %2").arg(_gp_fmt(n)).arg(_gp_fmt(std::fabs(n)));
        }
    }
    /* ── Espressione pura (con pre-processing ** → ^, × → *, ecc.) ── */
    {
        QByteArray ba = _gp_preprocess(low).toLatin1();
        double v;
        if (_gp_try(ba, v))
            return QString("%1 = %2").arg(input.trimmed()).arg(_gp_fmt(v));
    }
    return {};
}

/* ══════════════════════════════════════════════════════════════
   checkRam — controllo RAM centralizzato pre-pipeline
   Ritorna true se si può procedere, false se bloccato/annullato.
   ══════════════════════════════════════════════════════════════ */
bool AgentiPage::checkRam()
{
    double ramPct = 0.0;
    QFile minfo("/proc/meminfo");
    if (minfo.open(QIODevice::ReadOnly)) {
        long long total = 0, avail = 0;
        static const QRegularExpression reWs("\\s+");
        while (!minfo.atEnd()) {
            QString line = minfo.readLine().trimmed();
            const auto parts = line.split(reWs);
            if (parts.size() >= 2) {
                if (line.startsWith("MemTotal:"))    total = parts[1].toLongLong();
                if (line.startsWith("MemAvailable:")) avail = parts[1].toLongLong();
            }
        }
        minfo.close();
        if (total > 0) ramPct = 100.0 * (total - avail) / total;
    }
    if (ramPct >= 92.0) {
        m_log->append(QString("\xe2\x9d\x8c  <b>RAM critica (%1% usata)</b> — operazione bloccata.")
                      .arg(ramPct, 0, 'f', 0));
        LogBus::post(QString("\xe2\x9d\x8c AI Math: RAM critica (%1% usata) — operazione bloccata.")
                     .arg(ramPct, 0, 'f', 0));
        m_log->append("\xf0\x9f\x92\xa1  Chiudi altre applicazioni o scarica il modello prima di continuare.");
        emit pipelineStatus(-1, "");
        return false;
    }
    if (ramPct >= 75.0) {
        auto btn = QMessageBox::warning(this, "RAM elevata",
            QString("RAM al %1% — la pipeline potrebbe crashare a met\xc3\xa0.\n\nContinuare comunque?")
                .arg(ramPct, 0, 'f', 0),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes) {
            emit pipelineStatus(-1, "");
            return false;
        }
    }
    return true;
}

/* ══════════════════════════════════════════════════════════════
   checkModelSize — avvisa se il modello pesa > 70% RAM libera
   ══════════════════════════════════════════════════════════════ */
bool AgentiPage::checkModelSize(const QString& model)
{
    /* Trova la dimensione del modello in m_modelInfos */
    qint64 sizeBytes = 0;
    for (const auto& mi : m_modelInfos) {
        if (mi.name == model) { sizeBytes = mi.size; break; }
    }
    if (sizeBytes <= 0) return true;  /* dimensione sconosciuta: procedi */

    /* Leggi RAM libera da /proc/meminfo (MemAvailable, in kB) */
    long long availKb = 0;
    QFile minfo("/proc/meminfo");
    if (minfo.open(QIODevice::ReadOnly)) {
        while (!minfo.atEnd()) {
            QString line = minfo.readLine().trimmed();
            if (line.startsWith("MemAvailable:")) {
                static const QRegularExpression reWs("\\s+");
                const auto parts = line.split(reWs);
                if (parts.size() >= 2) availKb = parts[1].toLongLong();
                break;
            }
        }
        minfo.close();
    }
    if (availKb <= 0) return true;  /* non su Linux: procedi */

    const double sizeGb  = sizeBytes / 1e9;
    const double availGb = availKb   / 1e6;

    if (sizeBytes > availKb * 1024LL * 0.7) {
        auto btn = QMessageBox::warning(this,
            "Modello grande — conferma",
            QString("Il modello <b>%1</b> pesa circa <b>%2 GB</b>.<br>"
                    "RAM libera disponibile: <b>%3 GB</b>.<br><br>"
                    "Il caricamento potrebbe causare rallentamenti o crash.<br>"
                    "Continuare comunque?")
                .arg(model.toHtmlEscaped())
                .arg(sizeGb,  0, 'f', 1)
                .arg(availGb, 0, 'f', 1),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        return btn == QMessageBox::Yes;
    }
    return true;
}

/* ══════════════════════════════════════════════════════════════
   _inject_science — calcoli scientifici conversazionali
   Fisica / Elettronica / Chimica / Conversioni unità
   Pre-appende il risultato prima di inviare al LLM.
   ══════════════════════════════════════════════════════════════ */

static double _num(const QString& s) { return s.trimmed().replace(',','.').toDouble(); }
static QString _sci(double v) {
    if (std::abs(v) >= 1e6  || (std::abs(v) < 1e-3 && v != 0.0))
        return QString::number(v, 'g', 5);
    if (std::abs(v - std::round(v)) < 1e-9) return QString::number((long long)std::round(v));
    return QString::number(v, 'f', 4).remove(QRegularExpression("0+$")).remove(QRegularExpression("\\.$"));
}

/* Regex di supporto */
static const QRegularExpression reN(R"(([+\-]?[0-9]+(?:[.,][0-9]+)?(?:[eE][+\-]?[0-9]+)?))");

QString _inject_science(const QString& task)
{
    const QString lo = task.toLower().trimmed();
    if (lo.length() > 300) return task; /* prompt troppo lungo — non intercettare */

    /* ── helpers lambda ── */
    auto matchN = [](const QString& txt, int group = 1) -> double {
        static QRegularExpression re(R"(([+\-]?[0-9]+(?:[.,][0-9]+)?(?:[eE][+\-]?[0-9]+)?))");
        auto m = re.globalMatch(txt);
        int i = 0;
        while (m.hasNext()) { auto c = m.next(); if (++i == group) return _num(c.captured(1)); }
        return std::numeric_limits<double>::quiet_NaN();
    };

    struct Result { bool ok = false; QString label; QString value; };
    Result res;

    /* ══ ELETTRONICA ══ */

    /* Legge di Ohm: V=IR, I=V/R, R=V/I */
    {
        static QRegularExpression reVIR(
            R"(v\s*=\s*)" + reN.pattern() + R"(\s*\*?\s*)" + reN.pattern(), QRegularExpression::CaseInsensitiveOption);
        /* "quanti volt con R=470 e I=25mA" */
        static QRegularExpression reVoltRE(
            R"((?:quanti\s+)?volt.{0,20}r\s*=?\s*)" + reN.pattern() + R"(\s*[ohmΩ]?.{0,10}i\s*=?\s*)" + reN.pattern() + R"(\s*(m?a)?)",
            QRegularExpression::CaseInsensitiveOption);
        auto mv = reVoltRE.match(lo);
        if (mv.hasMatch()) {
            double R = _num(mv.captured(1));
            double I = _num(mv.captured(2));
            if (lo.contains("ma")) I /= 1000.0;
            double V = I * R;
            res = {true, "Ohm: V = I × R", _sci(V) + " V"};
        }
    }
    if (!res.ok) {
        /* "quanta corrente con V=12V e R=470" */
        static QRegularExpression reCurrRE(
            R"((?:quanta\s+)?(?:corrente|amper.{0,5}).{0,20}v\s*=?\s*)" + reN.pattern() + R"(\s*v?.{0,10}r\s*=?\s*)" + reN.pattern(),
            QRegularExpression::CaseInsensitiveOption);
        auto mi = reCurrRE.match(lo);
        if (mi.hasMatch()) {
            double V = _num(mi.captured(1)), R = _num(mi.captured(2));
            double I = (R != 0) ? V / R : std::numeric_limits<double>::infinity();
            res = {true, "Ohm: I = V / R", _sci(I * 1000.0) + " mA (" + _sci(I) + " A)"};
        }
    }
    if (!res.ok) {
        /* "quanta resistenza con V=12 I=25mA" */
        static QRegularExpression reResRE(
            R"((?:quanta\s+)?(?:resistenza|ohm).{0,20}v\s*=?\s*)" + reN.pattern() + R"(\s*v?.{0,10}i\s*=?\s*)" + reN.pattern(),
            QRegularExpression::CaseInsensitiveOption);
        auto mr = reResRE.match(lo);
        if (mr.hasMatch()) {
            double V = _num(mr.captured(1)), I = _num(mr.captured(2));
            if (lo.contains("ma")) I /= 1000.0;
            double R = (I != 0) ? V / I : std::numeric_limits<double>::infinity();
            res = {true, "Ohm: R = V / I", _sci(R) + " \xce\xa9"};
        }
    }

    /* Potenza: P=VI, P=I²R, P=V²/R */
    if (!res.ok) {
        static QRegularExpression rePowRE(
            R"((?:quanta\s+)?potenza.{0,20}(?:v|u)\s*=?\s*)" + reN.pattern() + R"(\s*v?.{0,10}i\s*=?\s*)" + reN.pattern(),
            QRegularExpression::CaseInsensitiveOption);
        auto mp = rePowRE.match(lo);
        if (mp.hasMatch()) {
            double V = _num(mp.captured(1)), I = _num(mp.captured(2));
            if (lo.contains("ma")) I /= 1000.0;
            double P = V * I;
            res = {true, "P = V × I", _sci(P * 1000.0) + " mW (" + _sci(P) + " W)"};
        }
    }

    /* Costante di tempo RC: τ = R × C */
    if (!res.ok) {
        static QRegularExpression reTauRE(
            R"(tau|costante.{0,10}tempo.{0,20}r\s*=?\s*)" + reN.pattern() + R"(\s*k?[oΩ]?.{0,10}c\s*=?\s*)" + reN.pattern() + R"(\s*([un]?f)?)",
            QRegularExpression::CaseInsensitiveOption);
        auto mt = reTauRE.match(lo);
        if (mt.hasMatch()) {
            double R = _num(mt.captured(1)), C = _num(mt.captured(2));
            /* Unità: k → ×1000; µ/u → ×1e-6; n → ×1e-9 */
            if (lo.contains("k\xce\xa9") || lo.contains("kohm")) R *= 1e3;
            if (lo.contains("\xc2\xb5""f") || lo.contains("uf"))  C *= 1e-6;
            else if (lo.contains("nf"))                            C *= 1e-9;
            double tau = R * C;
            res = {true, "\xcf\x84 = R \xc3\x97 C", _sci(tau) + " s"};
        }
    }

    /* Frequenza di taglio RC: fc = 1/(2πRC) */
    if (!res.ok && (lo.contains("freq") || lo.contains("taglio") || lo.contains("fc"))) {
        static QRegularExpression reFcRE(
            R"(r\s*=?\s*)" + reN.pattern() + R"(\s*k?[oΩ]?.{0,10}c\s*=?\s*)" + reN.pattern() + R"(\s*([un]?f)?)",
            QRegularExpression::CaseInsensitiveOption);
        auto mf = reFcRE.match(lo);
        if (mf.hasMatch()) {
            double R = _num(mf.captured(1)), C = _num(mf.captured(2));
            if (lo.contains("k\xce\xa9") || lo.contains("kohm")) R *= 1e3;
            if (lo.contains("\xc2\xb5""f") || lo.contains("uf"))  C *= 1e-6;
            else if (lo.contains("nf"))                            C *= 1e-9;
            if (R > 0 && C > 0) {
                double fc = 1.0 / (2.0 * M_PI * R * C);
                res = {true, "fc = 1/(2\xcf\x80RC)", _sci(fc) + " Hz"};
            }
        }
    }

    /* ══ FISICA ══ */

    /* Velocità: v = d/t */
    if (!res.ok && (lo.contains("velocit") || lo.contains("m/s") || lo.contains("km/h"))) {
        static QRegularExpression reVelRE(
            R"((?:d|distanza)\s*=?\s*)" + reN.pattern() + R"(\s*k?m?.{0,10}(?:t|tempo)\s*=?\s*)" + reN.pattern(),
            QRegularExpression::CaseInsensitiveOption);
        auto mv = reVelRE.match(lo);
        if (mv.hasMatch()) {
            double d = _num(mv.captured(1)), t = _num(mv.captured(2));
            double v = (t != 0) ? d / t : std::numeric_limits<double>::infinity();
            res = {true, "v = d / t", _sci(v) + (lo.contains("km") ? " km/h" : " m/s")};
        }
    }

    /* Energia cinetica: Ek = 0.5 × m × v² */
    if (!res.ok && lo.contains("cinetica")) {
        static QRegularExpression reEkRE(
            R"((?:m|massa)\s*=?\s*)" + reN.pattern() + R"(\s*k?g?.{0,10}(?:v|velocit\S*)\s*=?\s*)" + reN.pattern(),
            QRegularExpression::CaseInsensitiveOption);
        auto me = reEkRE.match(lo);
        if (me.hasMatch()) {
            double m = _num(me.captured(1)), v = _num(me.captured(2));
            double Ek = 0.5 * m * v * v;
            res = {true, "Ek = \xc2\xbd m v\xc2\xb2", _sci(Ek) + " J"};
        }
    }

    /* ══ CONVERSIONI UNITÀ ══ */

    /* kWh → J */
    if (!res.ok && lo.contains("kwh") && (lo.contains("joule") || lo.contains("j"))) {
        double n = matchN(lo);
        if (!std::isnan(n)) res = {true, "kWh \xe2\x86\x92 J", _sci(n * 3.6e6) + " J"};
    }
    /* atm → Pa */
    if (!res.ok && lo.contains("atm") && lo.contains("pa")) {
        double n = matchN(lo);
        if (!std::isnan(n)) res = {true, "atm \xe2\x86\x92 Pa", _sci(n * 101325.0) + " Pa"};
    }
    /* bar → Pa */
    if (!res.ok && lo.contains("bar") && lo.contains("pa")) {
        double n = matchN(lo);
        if (!std::isnan(n)) res = {true, "bar \xe2\x86\x92 Pa", _sci(n * 1e5) + " Pa"};
    }
    /* kmh → ms */
    if (!res.ok && lo.contains("km/h") && lo.contains("m/s")) {
        double n = matchN(lo);
        if (!std::isnan(n)) res = {true, "km/h \xe2\x86\x92 m/s", _sci(n / 3.6) + " m/s"};
    }
    /* ms → kmh */
    if (!res.ok && lo.contains("m/s") && lo.contains("km/h")) {
        double n = matchN(lo);
        if (!std::isnan(n)) res = {true, "m/s \xe2\x86\x92 km/h", _sci(n * 3.6) + " km/h"};
    }
    /* dBm → mW */
    if (!res.ok && lo.contains("dbm") && (lo.contains("mw") || lo.contains("watt"))) {
        double dbm = matchN(lo);
        if (!std::isnan(dbm)) res = {true, "dBm \xe2\x86\x92 mW", _sci(std::pow(10.0, dbm / 10.0)) + " mW"};
    }
    /* mW → dBm */
    if (!res.ok && lo.contains("mw") && lo.contains("dbm")) {
        double mw = matchN(lo);
        if (!std::isnan(mw) && mw > 0) res = {true, "mW \xe2\x86\x92 dBm", _sci(10.0 * std::log10(mw)) + " dBm"};
    }
    /* dB tensione: Vout/Vin da guadagno dB */
    if (!res.ok && lo.contains("db") && lo.contains("guadagno")) {
        double db = matchN(lo);
        if (!std::isnan(db)) res = {true, "dB \xe2\x86\x92 rapporto tensione", _sci(std::pow(10.0, db / 20.0)) + "\xc3\x97 (Vout/Vin)"};
    }

    /* ══ CHIMICA ══ */

    /* moli ↔ grammi: m = n × M */
    if (!res.ok && (lo.contains("mol") && (lo.contains("gramm") || lo.contains(" g ")))) {
        /* "quanti grammi sono 2 mol di NaCl (M=58.44)" */
        static QRegularExpression reMolRE(
            R"()" + reN.pattern() + R"(\s*mol.{0,30}m(?:assa\s+molare)?\s*=?\s*)" + reN.pattern() + R"(\s*g)",
            QRegularExpression::CaseInsensitiveOption);
        auto mm = reMolRE.match(lo);
        if (mm.hasMatch()) {
            double n = _num(mm.captured(1)), M = _num(mm.captured(2));
            res = {true, "m = n \xc3\x97 M", _sci(n * M) + " g"};
        }
    }

    /* pH da [H+] */
    if (!res.ok && lo.contains("ph") && lo.contains("[h") && lo.contains("mol")) {
        double c = matchN(lo);
        if (!std::isnan(c) && c > 0) res = {true, "pH = -log\xe2\x82\x81\xe2\x82\x80[H\xe2\x81\xba]", _sci(-std::log10(c))};
    }
    /* [H+] da pH */
    if (!res.ok && lo.contains("ph") && lo.contains("concentrazione")) {
        double ph = matchN(lo);
        if (!std::isnan(ph)) res = {true, "[H\xe2\x81\xba] da pH", _sci(std::pow(10.0, -ph)) + " mol/L"};
    }

    /* Concentrazione molare: c = m / (M * V) */
    if (!res.ok && lo.contains("molarit")) {
        static QRegularExpression reConcRE(
            R"((\d+(?:[.,]\d+)?)\s*g.{0,30}(\d+(?:[.,]\d+)?)\s*g/mol.{0,30}(\d+(?:[.,]\d+)?)\s*(ml|l\b))",
            QRegularExpression::CaseInsensitiveOption);
        auto mc = reConcRE.match(lo);
        if (mc.hasMatch()) {
            double m = _num(mc.captured(1)), M = _num(mc.captured(2));
            double V = _num(mc.captured(3));
            if (mc.captured(4).toLower() == "ml") V /= 1000.0;
            double c = (M > 0 && V > 0) ? (m / M) / V : 0.0;
            res = {true, "c = m/(M\xc3\x97""V)", _sci(c) + " mol/L"};
        }
    }

    /* Legge dei gas ideali: PV = nRT  (R=0.08206 L\xc2\xb7atm/mol\xc2\xb7K) */
    if (!res.ok && (lo.contains("gas ideal") || (lo.contains("pv") && lo.contains("nrt")))) {
        static QRegularExpression reGasRE(
            R"((\d+(?:[.,]\d+)?)\s*mol.{0,20}(\d+(?:[.,]\d+)?)\s*k.{0,10}(\d+(?:[.,]\d+)?)\s*atm)",
            QRegularExpression::CaseInsensitiveOption);
        auto mg = reGasRE.match(lo);
        if (mg.hasMatch()) {
            double n = _num(mg.captured(1)), T = _num(mg.captured(2)), P = _num(mg.captured(3));
            double V = n * 0.082057 * T / P;
            res = {true, "V = nRT/P", _sci(V) + " L"};
        }
    }

    /* Numero di Avogadro: molecole = (m/M) * Na */
    if (!res.ok && (lo.contains("molecol") || lo.contains("avogadro")) && lo.contains("mol")) {
        static QRegularExpression reAvRE(
            R"((\d+(?:[.,]\d+)?)\s*g.{0,20}m(?:assa\s+molare)?\s*=?\s*(\d+(?:[.,]\d+)?)\s*g/mol)",
            QRegularExpression::CaseInsensitiveOption);
        auto mav = reAvRE.match(lo);
        if (mav.hasMatch()) {
            double m = _num(mav.captured(1)), M = _num(mav.captured(2));
            double mol = m / M;
            double N = mol * 6.02214076e23;
            res = {true, "N = (m/M)\xc3\x97""Na", _sci(N) + " molecole"};
        }
    }

    /* ══ TEMPERATURE ══ */

    /* K → °C */
    if (!res.ok && lo.contains("kelvin") && (lo.contains("celsius") || lo.contains("\xc2\xb0""c"))) {
        double K = matchN(lo);
        if (!std::isnan(K)) res = {true, "\xc2\xb0""C = K - 273.15", _sci(K - 273.15) + " \xc2\xb0""C"};
    }
    /* °C → K */
    if (!res.ok && (lo.contains("celsius") || lo.contains("\xc2\xb0""c")) && lo.contains("kelvin")) {
        double C = matchN(lo);
        if (!std::isnan(C)) res = {true, "K = \xc2\xb0""C + 273.15", _sci(C + 273.15) + " K"};
    }
    /* °C → °F */
    if (!res.ok && (lo.contains("celsius") || lo.contains("\xc2\xb0""c")) && lo.contains("fahrenheit")) {
        double C = matchN(lo);
        if (!std::isnan(C)) res = {true, "\xc2\xb0""F = \xc2\xb0""C \xc3\x97 1.8 + 32", _sci(C * 1.8 + 32.0) + " \xc2\xb0""F"};
    }
    /* °F → °C */
    if (!res.ok && lo.contains("fahrenheit") && (lo.contains("celsius") || lo.contains("\xc2\xb0""c"))) {
        double F = matchN(lo);
        if (!std::isnan(F)) res = {true, "\xc2\xb0""C = (\xc2\xb0""F-32)/1.8", _sci((F - 32.0) / 1.8) + " \xc2\xb0""C"};
    }

    /* ══ CUCINA ══ */

    /* Temperatura forno → °F: qui la sorgente °C è implicita (non serve che
       l'utente scriva "celsius"), a differenza del blocco generico sopra
       che richiede entrambe le unità esplicite nel testo. */
    if (!res.ok && lo.contains("forno") && lo.contains("fahrenheit")) {
        double C = matchN(lo);
        if (!std::isnan(C))
            res = {true, "Forno \xc2\xb0""F = \xc2\xb0""C \xc3\x97 1.8 + 32",
                   _sci(C * 1.8 + 32.0) + " \xc2\xb0""F"};
    }
    /* Temperatura forno → Gas Mark (scala forni UK) */
    if (!res.ok && lo.contains("forno") && lo.contains("gas mark")) {
        double C = matchN(lo);
        if (!std::isnan(C)) {
            static const QVector<QPair<double,QString>> kGas = {
                {135,"1"},{149,"2"},{163,"3"},{177,"4"},{190,"5"},
                {204,"6"},{218,"7"},{232,"8"},{246,"9"}
            };
            QString mark = "9+";
            for (const auto& g : kGas)
                if (C <= g.first + 7) { mark = g.second; break; }
            res = {true, "Forno \xc2\xb0""C \xe2\x86\x92 Gas Mark",
                   _sci(C) + " \xc2\xb0""C \xe2\x89\x88 Gas Mark " + mark};
        }
    }

    /* Ingredienti da ricetta: ml \xe2\x86\x94 grammi (densità approssimate).
       Direzione dedotta dal primo numero+unità che compare nel testo. */
    if (!res.ok) {
        struct Ing { QString n; double density; };
        static const QVector<Ing> kIng = {
            {"acqua",           1.00}, {"farina",   0.53},
            {"zucchero a velo", 0.56}, {"zucchero", 0.85},
            {"burro",           0.95}, {"latte",    1.03},
            {"olio",            0.92}, {"miele",    1.42},
            {"panna",           1.01},
        };
        QString foundIng; double density = 0.0;
        for (const auto& ing : kIng)
            if (lo.contains(ing.n)) { foundIng = ing.n; density = ing.density; break; }

        if (!foundIng.isEmpty() && lo.contains("grammi")
            && (lo.contains(" ml") || lo.contains("millilitri"))) {
            static QRegularExpression reNumUnit(
                R"((\d+(?:[.,]\d+)?)\s*(ml|millilitri|g|gr|grammi))",
                QRegularExpression::CaseInsensitiveOption);
            auto mu = reNumUnit.match(lo);
            if (mu.hasMatch()) {
                const double v = _num(mu.captured(1));
                const QString unit = mu.captured(2);
                if (unit == "ml" || unit == "millilitri") {
                    res = {true, "ml \xe2\x86\x92 g (" + foundIng + ")",
                           _sci(v) + " ml \xe2\x89\x88 " + _sci(v * density) + " g"};
                } else {
                    res = {true, "g \xe2\x86\x92 ml (" + foundIng + ")",
                           _sci(v) + " g \xe2\x89\x88 " + _sci(v / density) + " ml"};
                }
            }
        }
    }

    /* Cucchiaino/cucchiaio/tazza → ml */
    if (!res.ok && (lo.contains("ml") || lo.contains("millilitri"))) {
        struct Vol { QString n; double ml; };
        static const QVector<Vol> kVol = {
            {"cucchiaino", 5.0}, {"cucchiaini", 5.0},
            {"cucchiaio",  15.0}, {"cucchiai",   15.0},
            {"tazza",      240.0}, {"tazze",     240.0},
        };
        for (const auto& v : kVol) {
            if (!lo.contains(v.n)) continue;
            const double n = matchN(lo);
            if (!std::isnan(n))
                res = {true, v.n + " \xe2\x86\x92 ml",
                       _sci(n) + " " + v.n + " \xe2\x89\x88 " + _sci(n * v.ml) + " ml"};
            break;
        }
    }

    /* ══ ENERGIA / LAVORO ══ */

    /* J → cal */
    if (!res.ok && lo.contains(" j") && lo.contains("cal") && !lo.contains("kcal")) {
        double j = matchN(lo);
        if (!std::isnan(j)) res = {true, "cal = J / 4.184", _sci(j / 4.184) + " cal"};
    }
    /* cal → J */
    if (!res.ok && lo.contains("cal") && (lo.contains("joule") || lo.contains(" j "))) {
        double cal = matchN(lo);
        if (!std::isnan(cal)) res = {true, "J = cal \xc3\x97 4.184", _sci(cal * 4.184) + " J"};
    }
    /* J → eV */
    if (!res.ok && (lo.contains("joule") || lo.contains(" j ")) && lo.contains("ev")) {
        double j = matchN(lo);
        if (!std::isnan(j)) res = {true, "eV = J / 1.602\xc3\x97""10\xe2\x81\xbb\xc2\xb9\xc2\xb9",
                                   _sci(j / 1.602176634e-19) + " eV"};
    }
    /* eV → J */
    if (!res.ok && lo.contains("ev") && (lo.contains("joule") || lo.contains(" j "))) {
        double ev = matchN(lo);
        if (!std::isnan(ev)) res = {true, "J = eV \xc3\x97 1.602\xc3\x97""10\xe2\x81\xbb\xc2\xb9\xc2\xb9",
                                    _sci(ev * 1.602176634e-19) + " J"};
    }
    /* kJ/mol → eV */
    if (!res.ok && lo.contains("kj/mol") && lo.contains("ev")) {
        double kj = matchN(lo);
        if (!std::isnan(kj)) res = {true, "eV = kJ/mol / 96.485", _sci(kj / 96.485) + " eV/molecola"};
    }

    /* ══ POTENZA / FORZA ══ */

    /* W → CV (cavalli vapore: 1 CV = 735.499 W) */
    if (!res.ok && (lo.contains(" cv") || lo.contains("cavalli")) && lo.contains("watt")) {
        double cv = matchN(lo);
        if (!std::isnan(cv)) res = {true, "W = CV \xc3\x97 735.499", _sci(cv * 735.499) + " W"};
    }
    /* CV → W */
    if (!res.ok && lo.contains("watt") && (lo.contains("cv") || lo.contains("cavalli"))) {
        double w = matchN(lo);
        if (!std::isnan(w)) res = {true, "CV = W / 735.499", _sci(w / 735.499) + " CV"};
    }
    /* N → kgf */
    if (!res.ok && lo.contains(" n ") && lo.contains("kgf")) {
        double n = matchN(lo);
        if (!std::isnan(n)) res = {true, "kgf = N / 9.80665", _sci(n / 9.80665) + " kgf"};
    }
    /* kgf → N */
    if (!res.ok && lo.contains("kgf") && lo.contains(" n ")) {
        double kgf = matchN(lo);
        if (!std::isnan(kgf)) res = {true, "N = kgf \xc3\x97 9.80665", _sci(kgf * 9.80665) + " N"};
    }
    /* P = I²R */
    if (!res.ok && lo.contains("dissi") && lo.contains("resistenz") && lo.contains("ma")) {
        static QRegularExpression rePdissRE(
            R"(r\s*=?\s*(\d+(?:[.,]\d+)?)\s*[oΩ]?.{0,10}i\s*=?\s*(\d+(?:[.,]\d+)?)\s*ma)",
            QRegularExpression::CaseInsensitiveOption);
        auto pd = rePdissRE.match(lo);
        if (pd.hasMatch()) {
            double R = _num(pd.captured(1)), I = _num(pd.captured(2)) / 1000.0;
            res = {true, "P = I\xc2\xb2\xc3\x97""R", _sci(I * I * R * 1000.0) + " mW"};
        }
    }

    /* ══ VELOCITÀ ══ */

    /* km/h → mph */
    if (!res.ok && lo.contains("km/h") && lo.contains("mph")) {
        double kmh = matchN(lo);
        if (!std::isnan(kmh)) res = {true, "mph = km/h / 1.60934", _sci(kmh / 1.60934) + " mph"};
    }
    /* mph → km/h */
    if (!res.ok && lo.contains("mph") && lo.contains("km/h")) {
        double mph = matchN(lo);
        if (!std::isnan(mph)) res = {true, "km/h = mph \xc3\x97 1.60934", _sci(mph * 1.60934) + " km/h"};
    }

    /* ══ ELETTRONICA — UNITÀ PREFISSATE ══ */

    /* Ω ↔ kΩ ↔ MΩ */
    if (!res.ok && (lo.contains("kohm") || lo.contains("k\xce\xa9") || lo.contains("mohm") || lo.contains("m\xce\xa9"))
            && lo.contains("ohm")) {
        double v = matchN(lo);
        if (!std::isnan(v)) {
            if (lo.contains("mohm") || lo.contains("m\xce\xa9"))
                res = {true, "k\xce\xa9 = M\xce\xa9 \xc3\x97 1000", _sci(v) + " M\xce\xa9 = " + _sci(v * 1e3) + " k\xce\xa9 = " + _sci(v * 1e6) + " \xce\xa9"};
            else
                res = {true, "\xce\xa9 da k\xce\xa9", _sci(v) + " k\xce\xa9 = " + _sci(v * 1e3) + " \xce\xa9 = " + _sci(v / 1e3) + " M\xce\xa9"};
        }
    }
    /* Hz ↔ periodo T = 1/f */
    if (!res.ok && (lo.contains("periodo") || lo.contains("t = 1/f")) && lo.contains("hz")) {
        double f = matchN(lo);
        if (!std::isnan(f) && f > 0) res = {true, "T = 1/f", _sci(1.0 / f * 1000.0) + " ms"};
    }
    /* Hz ↔ kHz ↔ MHz ↔ GHz */
    if (!res.ok && (lo.contains("ghz") || lo.contains("mhz") || lo.contains("khz")) && lo.contains("hz")) {
        double v = matchN(lo);
        if (!std::isnan(v)) {
            double hz = lo.contains("ghz") ? v * 1e9 : lo.contains("mhz") ? v * 1e6 : v * 1e3;
            res = {true, "Hz espanso", _sci(hz) + " Hz = " + _sci(hz / 1e3) + " kHz = " + _sci(hz / 1e6) + " MHz = " + _sci(hz / 1e9) + " GHz"};
        }
    }
    /* F ↔ µF ↔ nF ↔ pF */
    if (!res.ok && (lo.contains("\xc2\xb5""f") || lo.contains("uf") || lo.contains("nf") || lo.contains("pf"))
            && (lo.contains("farad") || lo.contains("capacit"))) {
        double v = matchN(lo);
        if (!std::isnan(v)) {
            double F = lo.contains("pf") ? v * 1e-12 : lo.contains("nf") ? v * 1e-9 : v * 1e-6;
            res = {true, "F espanso", _sci(F) + " F = " + _sci(F * 1e6) + " \xc2\xb5""F = " + _sci(F * 1e9) + " nF = " + _sci(F * 1e12) + " pF"};
        }
    }
    /* H ↔ mH ↔ µH */
    if (!res.ok && (lo.contains("mh") || lo.contains("\xc2\xb5""h")) && lo.contains("henry")) {
        double v = matchN(lo);
        if (!std::isnan(v)) {
            double H = lo.contains("\xc2\xb5""h") ? v * 1e-6 : v * 1e-3;
            res = {true, "H espanso", _sci(H) + " H = " + _sci(H * 1e3) + " mH = " + _sci(H * 1e6) + " \xc2\xb5""H"};
        }
    }
    /* mA ↔ µA ↔ A */
    if (!res.ok && (lo.contains("ma") || lo.contains("\xc2\xb5""a")) && lo.contains("ampere")) {
        double v = matchN(lo);
        if (!std::isnan(v)) {
            double A = lo.contains("\xc2\xb5""a") ? v * 1e-6 : v * 1e-3;
            res = {true, "A espanso", _sci(A) + " A = " + _sci(A * 1e3) + " mA = " + _sci(A * 1e6) + " \xc2\xb5""A"};
        }
    }
    /* Pressione mmHg → kPa */
    if (!res.ok && lo.contains("mmhg") && lo.contains("kpa")) {
        double mmhg = matchN(lo);
        if (!std::isnan(mmhg)) res = {true, "kPa = mmHg \xc3\x97 0.133322", _sci(mmhg * 0.133322) + " kPa"};
    }
    /* kPa → mmHg */
    if (!res.ok && lo.contains("kpa") && lo.contains("mmhg")) {
        double kpa = matchN(lo);
        if (!std::isnan(kpa)) res = {true, "mmHg = kPa / 0.133322", _sci(kpa / 0.133322) + " mmHg"};
    }

    /* ══ ASTRONOMIA ══ */

    /* UA → km (rimosso blocco duplicato — ora solo nel secondo blocco sotto) */

    /* Anno luce → km */
    if (!res.ok && (lo.contains("anno luce") || lo.contains("anno-luce") || lo.contains("ly")) && lo.contains("km")) {
        double ly = matchN(lo);
        if (!std::isnan(ly)) res = {true, "km = al " "\xc3\x97" " 9.461" "\xc3\x97" "10\xc2\xb9\xc2\xb2", _sci(ly * 9.4607304725808e12) + " km"};
    }
    /* u → kg */
    if (!res.ok && (lo.contains("unita di massa") || lo.contains("massa atomica") || lo.contains(" u ") || lo.contains("amu")) && lo.contains("kg")) {
        double u = matchN(lo);
        if (!std::isnan(u)) res = {true, "kg = u " "\xc3\x97" " 1.661" "\xc3\x97" "10\xe2\x81\xbb\xc2\xb2\xc2\xb7",
                                   _sci(u * 1.66053906660e-27) + " kg"};
    }

    /* ══ MATEMATICA / TRIGONOMETRIA ══ */

    /* Notazione scientifica */
    if (!res.ok && lo.contains("notazione scientifica")) {
        double v = matchN(lo);
        if (!std::isnan(v) && v != 0.0) {
            int exp = (int)std::floor(std::log10(std::abs(v)));
            double mantissa = v / std::pow(10.0, exp);
            res = {true, "notaz. scientifica",
                   _sci(mantissa) + " \xc3\x97 10^" + QString::number(exp)};
        }
    }

    /* gradi → radianti */
    if (!res.ok && (lo.contains("gradi") || lo.contains("\xc2\xb0")) && lo.contains("radiant")) {
        double deg = matchN(lo);
        if (!std::isnan(deg)) res = {true, "rad = \xc2\xb0 \xc3\x97 \xcf\x80/180", _sci(deg * M_PI / 180.0) + " rad"};
    }
    /* radianti → gradi */
    if (!res.ok && lo.contains("radiant") && (lo.contains("grad") || lo.contains("\xc2\xb0"))) {
        double rad = matchN(lo);
        if (!std::isnan(rad)) res = {true, "\xc2\xb0 = rad \xc3\x97 180/\xcf\x80", _sci(rad * 180.0 / M_PI) + "\xc2\xb0"};
    }
    /* gradianti → gradi (1 gon = 0.9°) */
    if (!res.ok && (lo.contains("gradiante") || lo.contains(" gon"))) {
        double gon = matchN(lo);
        if (!std::isnan(gon)) res = {true, "\xc2\xb0 = gon \xc3\x97 0.9", _sci(gon * 0.9) + "\xc2\xb0"};
    }
    /* ln ↔ log10 */
    if (!res.ok && (lo.contains("logaritmo natural") || lo.contains(" ln(") || lo.contains("ln ")) && !lo.contains("log10")) {
        double x = matchN(lo);
        if (!std::isnan(x) && x > 0) res = {true, "ln(x)", _sci(std::log(x)) + " (log\xe2\x82\x81\xe2\x82\x80 = " + _sci(std::log10(x)) + ")"};
    }
    /* Area cerchio */
    if (!res.ok && lo.contains("area") && lo.contains("cerchio") && lo.contains("r")) {
        double r = matchN(lo);
        if (!std::isnan(r) && r > 0) res = {true, "A = \xcf\x80 r\xc2\xb2", _sci(M_PI * r * r) + " (unit\xc3\xa0\xc2\xb2)"};
    }
    /* Volume sfera */
    if (!res.ok && lo.contains("volume") && lo.contains("sfera")) {
        double r = matchN(lo);
        if (!std::isnan(r) && r > 0) res = {true, "V = (4/3)\xcf\x80 r\xc2\xb3", _sci((4.0 / 3.0) * M_PI * r * r * r) + " (unit\xc3\xa0\xc2\xb3)"};
    }

    /* ══ INFORMATICA — BASI NUMERICHE ══ */

    /* Binario → decimale */
    if (!res.ok && (lo.contains("binario") || lo.contains("bin ")) && lo.contains("decimal")) {
        static QRegularExpression reBinRE(R"(\b([01][01\s]{1,30})\b)");
        auto mb = reBinRE.match(lo);
        if (mb.hasMatch()) {
            QString bits = mb.captured(1).remove(' ');
            bool ok2; long long dec = bits.toLongLong(&ok2, 2);
            if (ok2) res = {true, "bin \xe2\x86\x92 dec", bits + "\xe2\x82\x82 = " + QString::number(dec) + "\xe2\x82\x81\xe2\x82\x80 = 0x" + QString::number(dec, 16).toUpper()};
        }
    }
    /* Decimale → esadecimale */
    if (!res.ok && (lo.contains("esadecimale") || lo.contains(" hex")) && lo.contains("decimal")) {
        double v = matchN(lo);
        if (!std::isnan(v) && v >= 0 && v < 1e15) {
            long long iv = (long long)std::round(v);
            res = {true, "dec \xe2\x86\x92 hex", QString::number(iv) + " = 0x" + QString::number(iv, 16).toUpper() + " = " + QString::number(iv, 2) + "\xe2\x82\x82"};
        }
    }
    /* Hex → decimale/binario */
    if (!res.ok && (lo.contains("0x") || lo.contains("hex ")) && lo.contains("decimal")) {
        static QRegularExpression reHexRE(R"(0[xX]([0-9A-Fa-f]+))");
        auto mh = reHexRE.match(lo);
        if (mh.hasMatch()) {
            bool ok2; long long dec = mh.captured(1).toLongLong(&ok2, 16);
            if (ok2) res = {true, "hex \xe2\x86\x92 dec", "0x" + mh.captured(1).toUpper() + " = " + QString::number(dec) + " = " + QString::number(dec, 2) + "\xe2\x82\x82"};
        }
    }

    /* Frequenza luce ↔ lunghezza d'onda: f = c/λ, λ = c/f */
    if (!res.ok && (lo.contains("lunghezza d") || lo.contains("lambda") || lo.contains("\xce\xbb") || lo.contains("nm ")) && lo.contains("frequen")) {
        double nm = matchN(lo);
        if (!std::isnan(nm) && nm > 0) {
            double lambda = nm * 1e-9;
            double f = 2.99792458e8 / lambda;
            res = {true, "f = c/\xce\xbb", _sci(f) + " Hz (" + _sci(f / 1e12) + " THz)"};
        }
    }

    /* UA → km (unit\xc3\xa0 astronomica) */
    if (!res.ok && (lo.contains(" ua ") || lo.contains("astronomica") || lo.contains("astronomica")) && lo.contains("km")) {
        double ua = matchN(lo);
        if (!std::isnan(ua)) res = {true, "km = UA \xc3\x97 149 597 871", _sci(ua * 1.495978707e8) + " km"};
    }

    /* Conversione A → mA / µA */
    if (!res.ok && (lo.contains("ampere") || lo.contains(" a ")) && (lo.contains("ma") || lo.contains("\xc2\xb5""a") || lo.contains("milliamper") || lo.contains("microamper"))) {
        double a = matchN(lo);
        if (!std::isnan(a) && a > 0 && a < 1000) {
            res = {true, "A \xe2\x86\x92 mA / \xc2\xb5""A", _sci(a) + " A = " + _sci(a * 1e3) + " mA = " + _sci(a * 1e6) + " \xc2\xb5""A"};
        }
    }

    /* Binario → ottale */
    if (!res.ok && (lo.contains("binario") || lo.contains("bin ")) && lo.contains("ottal")) {
        static QRegularExpression reBin2RE(R"(\b([01][01\s]{1,30})\b)");
        auto mb = reBin2RE.match(lo);
        if (mb.hasMatch()) {
            QString bits = mb.captured(1).remove(' ');
            bool ok2; long long dec = bits.toLongLong(&ok2, 2);
            if (ok2) res = {true, "bin \xe2\x86\x92 oct", bits + "\xe2\x82\x82 = " + QString::number(dec, 8) + "\xe2\x82\x88 = " + QString::number(dec) + "\xe2\x82\x81\xe2\x82\x80"};
        }
    }
    /* Decimale → ottale */
    if (!res.ok && lo.contains("decimal") && lo.contains("ottal")) {
        double v = matchN(lo);
        if (!std::isnan(v) && v >= 0 && v < 1e12) {
            long long iv = (long long)std::round(v);
            res = {true, "dec \xe2\x86\x92 oct", QString::number(iv) + "\xe2\x82\x81\xe2\x82\x80 = " + QString::number(iv, 8) + "\xe2\x82\x88"};
        }
    }

    /* Percentuale ↔ frazione ↔ decimale */
    if (!res.ok && lo.contains("%") && (lo.contains("frazioni") || lo.contains("frazione") || lo.contains("decimal"))) {
        static QRegularExpression rePctRE(R"((\d+(?:[.,]\d+)?)\s*%)");
        auto mp = rePctRE.match(lo);
        if (mp.hasMatch()) {
            double pct = _num(mp.captured(1));
            double dec = pct / 100.0;
            // calcola frazione ridotta
            long long num = (long long)std::round(pct * 10);
            long long den = 1000;
            auto gcd = [](long long a, long long b) -> long long {
                while (b) { long long t = b; b = a % b; a = t; } return a; };
            long long g = gcd(std::abs(num), den);
            num /= g; den /= g;
            res = {true, "% \xe2\x86\x92 fraz./dec.",
                   _sci(pct) + "% = " + _sci(dec) + " = " + QString::number(num) + "/" + QString::number(den)};
        }
    }

    if (!res.ok) return task;

    /* Prepende il risultato come contesto per l'AI */
    return QString("[Calcolo locale: %1 = %2]\n\n").arg(res.label, res.value) + task;
}


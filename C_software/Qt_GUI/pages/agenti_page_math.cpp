#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <cmath>
#include <cstdlib>
#include <QRegularExpression>
#include <QMessageBox>
#include <QFile>
#include <QSettings>

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
QString _sanitize_prompt(const QString& raw)
{
    QString s = raw;

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

    return sys;
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


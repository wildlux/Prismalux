#include "main_ai.h"
#include "main_ai_p.h"
#include <QRegularExpression>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QStringList>

/* ══════════════════════════════════════════════════════════════
   markdownToHtml — converte la risposta AI in HTML leggibile
   Gestisce: code fence con header (lingua+copia+salva),
             inline code, bold, italic, heading,
             liste puntate/numerate, tabelle, righe separatore.
   codeBlocks / codeCounter: se forniti, i blocchi vengono
   salvati per i gestori code:copy:N e code:save:N.
   ══════════════════════════════════════════════════════════════ */

/* ══════════════════════════════════════════════════════════════
   syntaxHighlight — mini-tokenizer stateful per colorare il codice
   Restituisce HTML con <span style='color:...'>token</span>.
   Copre: C/C++/Java/JS/TS/Rust/Go (C-like), Python, Bash, SQL,
          HTML/XML, JSON. Per gli altri: solo escape HTML.
   ══════════════════════════════════════════════════════════════ */

namespace SH {

/* Colori GitHub Dark Dimmed */
static const char* kKw  = "#ff7b72";   /* keywords */
static const char* kStr = "#a5d6ff";   /* stringhe */
static const char* kCom = "#8b949e";   /* commenti */
static const char* kNum = "#79c0ff";   /* numeri */
static const char* kFn  = "#d2a8ff";   /* funzioni */
static const char* kTy  = "#ffa657";   /* tipi/classi */
static const char* kTag = "#7ee787";   /* tag HTML */
static const char* kAt  = "#79c0ff";   /* attributi HTML / chiavi JSON */
static const char* kOp  = "#ff7b72";   /* operatori speciali */

static QString span(const char* color, const QString& text) {
    return QString("<span style='color:%1;'>%2</span>")
               .arg(color)
               .arg(text.toHtmlEscaped());
}
static QString plain(const QString& text) { return text.toHtmlEscaped(); }

/* ─── Keyword sets ─────────────────────────────────────────── */
static const QSet<QString>& kwC() {
    static const QSet<QString> s = {
        "auto","break","case","char","const","continue","default","do",
        "double","else","enum","extern","float","for","goto","if","inline",
        "int","long","register","restrict","return","short","signed","sizeof",
        "static","struct","switch","typedef","union","unsigned","void",
        "volatile","while","NULL","nullptr","true","false","bool",
        /* C++ */
        "class","new","delete","this","virtual","override","final","public",
        "private","protected","friend","operator","template","typename",
        "namespace","using","try","catch","throw","noexcept","constexpr",
        "explicit","mutable","static_cast","dynamic_cast","reinterpret_cast",
        "const_cast","decltype","auto","nullptr","std","string","vector",
        "map","set","pair","unique_ptr","shared_ptr","weak_ptr",
    };
    return s;
}
static const QSet<QString>& kwJava() {
    static const QSet<QString> s = {
        "abstract","assert","boolean","break","byte","case","catch","char",
        "class","const","continue","default","do","double","else","enum",
        "extends","final","finally","float","for","goto","if","implements",
        "import","instanceof","int","interface","long","native","new",
        "package","private","protected","public","return","short","static",
        "strictfp","super","switch","synchronized","this","throw","throws",
        "transient","try","void","volatile","while","var","record","sealed",
        "true","false","null","String","Integer","Double","List","Map","Set",
    };
    return s;
}
static const QSet<QString>& kwJs() {
    static const QSet<QString> s = {
        "async","await","break","case","catch","class","const","continue",
        "debugger","default","delete","do","else","export","extends","false",
        "finally","for","from","function","if","import","in","instanceof",
        "let","new","null","of","return","static","super","switch","this",
        "throw","true","try","typeof","undefined","var","void","while",
        "with","yield","type","interface","enum","declare","readonly","as",
        "implements","namespace","module","any","never","unknown","string",
        "number","boolean","object","symbol","bigint","console","Promise",
    };
    return s;
}
static const QSet<QString>& kwPy() {
    static const QSet<QString> s = {
        "False","None","True","and","as","assert","async","await",
        "break","class","continue","def","del","elif","else","except",
        "finally","for","from","global","if","import","in","is","lambda",
        "nonlocal","not","or","pass","raise","return","try","while","with",
        "yield","self","cls","print","len","range","type","int","str",
        "float","list","dict","set","tuple","bool","bytes","open","super",
    };
    return s;
}
static const QSet<QString>& kwRust() {
    static const QSet<QString> s = {
        "as","async","await","break","const","continue","crate","dyn","else",
        "enum","extern","false","fn","for","if","impl","in","let","loop",
        "match","mod","move","mut","pub","ref","return","self","Self","static",
        "struct","super","trait","true","type","unsafe","use","where","while",
        "i8","i16","i32","i64","i128","isize","u8","u16","u32","u64","u128",
        "usize","f32","f64","bool","char","str","String","Vec","Option",
        "Result","Ok","Err","Some","None","Box","Rc","Arc",
    };
    return s;
}
static const QSet<QString>& kwGo() {
    static const QSet<QString> s = {
        "break","case","chan","const","continue","default","defer","else",
        "fallthrough","for","func","go","goto","if","import","interface",
        "map","package","range","return","select","struct","switch","type",
        "var","true","false","nil","int","int8","int16","int32","int64",
        "uint","uint8","uint16","uint32","uint64","float32","float64",
        "bool","string","byte","rune","error","any","comparable",
    };
    return s;
}
static const QSet<QString>& kwSql() {
    static const QSet<QString> s = {
        "SELECT","FROM","WHERE","AND","OR","NOT","IN","IS","NULL",
        "JOIN","LEFT","RIGHT","INNER","OUTER","ON","AS","GROUP","BY",
        "ORDER","HAVING","LIMIT","OFFSET","UNION","INSERT","INTO","VALUES",
        "UPDATE","SET","DELETE","CREATE","TABLE","INDEX","VIEW","DROP",
        "ALTER","ADD","COLUMN","PRIMARY","KEY","FOREIGN","REFERENCES",
        "DISTINCT","COUNT","SUM","AVG","MIN","MAX","CASE","WHEN","THEN",
        "ELSE","END","EXISTS","BETWEEN","LIKE","ALL","ANY","SOME",
        "COMMIT","ROLLBACK","TRANSACTION","BEGIN","WITH","RECURSIVE",
    };
    return s;
}
static const QSet<QString>& kwBash() {
    static const QSet<QString> s = {
        "if","then","else","elif","fi","for","in","do","done","while",
        "until","case","esac","function","return","exit","break","continue",
        "local","declare","export","readonly","unset","shift","source",
        "echo","printf","read","cd","pwd","ls","cp","mv","rm","mkdir",
        "chmod","chown","grep","sed","awk","find","sort","cut","tr",
        "cat","head","tail","wc","curl","wget","ssh","git","sudo","apt",
        "true","false","test","set","eval","exec",
    };
    return s;
}

/* ─── Tokenizer generico C-like ──────────────────────────────── */
static QString highlightClike(const QString& code, const QSet<QString>& kws,
                               const QString& lineCommentPrefix = "//",
                               bool hasBlockComment = true,
                               bool tripleStrings = false)
{
    QString out;
    out.reserve(code.size() * 3);
    const int n = code.size();
    int i = 0;

    auto peek = [&](int off = 0) -> QChar {
        return (i + off < n) ? code[i + off] : QChar(0);
    };

    while (i < n) {
        /* ── Commento linea ── */
        if (!lineCommentPrefix.isEmpty() &&
            code.mid(i, lineCommentPrefix.size()) == lineCommentPrefix) {
            int end = code.indexOf('\n', i);
            if (end < 0) end = n;
            out += span(kCom, code.mid(i, end - i));
            i = end;
            continue;
        }

        /* ── Commento blocco /* ... *\/ ── */
        if (hasBlockComment && peek() == '/' && peek(1) == '*') {
            int end = code.indexOf("*/", i + 2);
            if (end < 0) end = n - 2;
            out += span(kCom, code.mid(i, end + 2 - i));
            i = end + 2;
            continue;
        }

        /* ── Stringa tripla Python """...""" o '''...''' ── */
        if (tripleStrings) {
            for (const QString& q3 : {QString("\"\"\""), QString("'''")}) {
                if (code.mid(i, 3) == q3) {
                    int end = code.indexOf(q3, i + 3);
                    if (end < 0) end = n - 3;
                    out += span(kStr, code.mid(i, end + 3 - i));
                    i = end + 3;
                    goto next_char;
                }
            }
        }

        /* ── Stringa " ... " o ' ... ' ── */
        if (peek() == '"' || peek() == '\'') {
            const QChar q = peek();
            int j = i + 1;
            while (j < n) {
                if (code[j] == '\\') { j += 2; continue; }
                if (code[j] == q)    { j++;    break; }
                j++;
            }
            out += span(kStr, code.mid(i, j - i));
            i = j;
            continue;
        }

        /* ── Stringa backtick JS ── */
        if (peek() == '`') {
            int j = i + 1;
            while (j < n) {
                if (code[j] == '\\') { j += 2; continue; }
                if (code[j] == '`')  { j++;    break; }
                j++;
            }
            out += span(kStr, code.mid(i, j - i));
            i = j;
            continue;
        }

        /* ── Numero ── */
        if (peek().isDigit() ||
            (peek() == '.' && peek(1).isDigit()) ||
            (peek() == '-' && peek(1).isDigit() &&
             (i == 0 || !code[i-1].isLetterOrNumber()))) {
            int j = i;
            if (code[j] == '-') j++;
            while (j < n && (code[j].isDigit() || code[j] == '.' ||
                              code[j] == 'x' || code[j] == 'X' ||
                              (code[j] >= 'a' && code[j] <= 'f') ||
                              (code[j] >= 'A' && code[j] <= 'F') ||
                              code[j] == '_' || code[j] == 'e' ||
                              code[j] == 'E' || code[j] == '+')) j++;
            out += span(kNum, code.mid(i, j - i));
            i = j;
            continue;
        }

        /* ── Identificatore / keyword ── */
        if (peek().isLetter() || peek() == '_') {
            int j = i;
            while (j < n && (code[j].isLetterOrNumber() || code[j] == '_')) j++;
            const QString word = code.mid(i, j - i);
            if (kws.contains(word)) {
                out += span(kKw, word);
            } else if (j < n && code[j] == '(') {
                /* Chiamata funzione: nome prima di ( */
                out += span(kFn, word);
            } else if (!word.isEmpty() && word[0].isUpper()) {
                /* Identificatore Capitalizzato → tipo/classe */
                out += span(kTy, word);
            } else {
                out += plain(word);
            }
            i = j;
            continue;
        }

        /* ── Tutto il resto: escape e emetti ── */
        out += plain(QString(code[i]));
        i++;
        next_char:;
    }
    return out;
}

/* ─── Highlighter Bash ─────────────────────────────────────── */
static QString highlightBash(const QString& code)
{
    const QSet<QString>& kws = kwBash();
    QString out;
    out.reserve(code.size() * 3);
    const int n = code.size();
    int i = 0;

    while (i < n) {
        /* Commento # */
        if (code[i] == '#') {
            int end = code.indexOf('\n', i);
            if (end < 0) end = n;
            out += span(kCom, code.mid(i, end - i));
            i = end;
            continue;
        }
        /* Stringa "$(...)" o "${...}" */
        if (code[i] == '$') {
            int j = i + 1;
            if (j < n && (code[j] == '(' || code[j] == '{')) {
                const QChar close = (code[j] == '(') ? ')' : '}';
                int depth = 1; j++;
                while (j < n && depth > 0) {
                    if (code[j] == code[i+1]) depth++;
                    else if (code[j] == close) depth--;
                    j++;
                }
                out += span(kOp, code.mid(i, j - i));
                i = j;
                continue;
            }
            /* $VAR */
            while (j < n && (code[j].isLetterOrNumber() || code[j] == '_')) j++;
            out += span(kOp, code.mid(i, j - i));
            i = j;
            continue;
        }
        /* Stringa " o ' */
        if (code[i] == '"' || code[i] == '\'') {
            const QChar q = code[i];
            int j = i + 1;
            while (j < n) {
                if (code[j] == '\\') { j += 2; continue; }
                if (code[j] == q)    { j++;    break; }
                j++;
            }
            out += span(kStr, code.mid(i, j - i));
            i = j;
            continue;
        }
        /* Keyword */
        if (code[i].isLetter() || code[i] == '_') {
            int j = i;
            while (j < n && (code[j].isLetterOrNumber() || code[j] == '_' || code[j] == '-')) j++;
            const QString word = code.mid(i, j - i);
            out += kws.contains(word) ? span(kKw, word) : plain(word);
            i = j;
            continue;
        }
        /* Numero */
        if (code[i].isDigit()) {
            int j = i;
            while (j < n && (code[j].isDigit() || code[j] == '.')) j++;
            out += span(kNum, code.mid(i, j - i));
            i = j;
            continue;
        }
        out += plain(QString(code[i]));
        i++;
    }
    return out;
}

/* ─── Highlighter SQL ──────────────────────────────────────── */
static QString highlightSql(const QString& code)
{
    const QSet<QString>& kws = kwSql();
    QString out;
    out.reserve(code.size() * 3);
    const int n = code.size();
    int i = 0;
    while (i < n) {
        /* Commento -- */
        if (i + 1 < n && code[i] == '-' && code[i+1] == '-') {
            int end = code.indexOf('\n', i);
            if (end < 0) end = n;
            out += span(kCom, code.mid(i, end - i));
            i = end;
            continue;
        }
        /* Commento /* */
        if (i + 1 < n && code[i] == '/' && code[i+1] == '*') {
            int end = code.indexOf("*/", i + 2);
            if (end < 0) end = n - 2;
            out += span(kCom, code.mid(i, end + 2 - i));
            i = end + 2;
            continue;
        }
        /* Stringa '...' */
        if (code[i] == '\'') {
            int j = i + 1;
            while (j < n) {
                if (code[j] == '\'' && j+1 < n && code[j+1] == '\'') { j += 2; continue; }
                if (code[j] == '\'') { j++; break; }
                j++;
            }
            out += span(kStr, code.mid(i, j - i));
            i = j;
            continue;
        }
        /* Keyword (case-insensitive: confronto uppercase) */
        if (code[i].isLetter() || code[i] == '_') {
            int j = i;
            while (j < n && (code[j].isLetterOrNumber() || code[j] == '_')) j++;
            const QString word = code.mid(i, j - i);
            out += kws.contains(word.toUpper()) ? span(kKw, word) : plain(word);
            i = j;
            continue;
        }
        /* Numero */
        if (code[i].isDigit()) {
            int j = i;
            while (j < n && (code[j].isDigit() || code[j] == '.')) j++;
            out += span(kNum, code.mid(i, j - i));
            i = j;
            continue;
        }
        out += plain(QString(code[i]));
        i++;
    }
    return out;
}

/* ─── Highlighter HTML/XML ─────────────────────────────────── */
static QString highlightHtml(const QString& code)
{
    QString out;
    out.reserve(code.size() * 3);
    const int n = code.size();
    int i = 0;
    while (i < n) {
        /* Commento HTML <!-- ... --> */
        if (code.mid(i, 4) == "<!--") {
            int end = code.indexOf("-->", i + 4);
            if (end < 0) end = n - 3;
            out += span(kCom, code.mid(i, end + 3 - i));
            i = end + 3;
            continue;
        }
        /* Tag < ... > */
        if (code[i] == '<') {
            int j = i + 1;
            /* Raccoglie nome tag */
            while (j < n && !code[j].isSpace() && code[j] != '>' && code[j] != '/') j++;
            const QString tagName = code.mid(i + 1, j - i - 1)
                                        .remove('!').remove('/').trimmed();
            /* Emetti < */
            out += plain("<");
            if (i + 1 < n && code[i+1] == '/') out += plain("/");
            if (!tagName.isEmpty()) out += span(kTag, tagName);
            i = (i + 1 < n && code[i+1] == '/') ? i + 1 + (int)tagName.size() + 1
                                                  : i + 1 + (int)tagName.size();
            /* Attributi fino a > */
            while (i < n && code[i] != '>') {
                if (code[i].isLetter() || code[i] == '_' || code[i] == '-') {
                    int k = i;
                    while (k < n && code[k] != '=' && code[k] != '>' &&
                           code[k] != ' ' && code[k] != '\n') k++;
                    out += span(kAt, code.mid(i, k - i));
                    i = k;
                } else if (code[i] == '"' || code[i] == '\'') {
                    const QChar q = code[i];
                    int k = i + 1;
                    while (k < n && code[k] != q) k++;
                    out += span(kStr, code.mid(i, k + 1 - i));
                    i = k + 1;
                } else {
                    out += plain(QString(code[i]));
                    i++;
                }
            }
            if (i < n) { out += plain(">"); i++; }
            continue;
        }
        out += plain(QString(code[i]));
        i++;
    }
    return out;
}

/* ─── Highlighter JSON ─────────────────────────────────────── */
static QString highlightJson(const QString& code)
{
    QString out;
    out.reserve(code.size() * 3);
    const int n = code.size();
    int i = 0;
    bool expectKey = false; /* dopo { o , alla stessa profondità */
    while (i < n) {
        if (code[i] == '{' || code[i] == '[') {
            out += plain(QString(code[i]));
            expectKey = (code[i] == '{');
            i++; continue;
        }
        if (code[i] == '}' || code[i] == ']') {
            out += plain(QString(code[i]));
            expectKey = false; i++; continue;
        }
        if (code[i] == ',') {
            out += plain(",");
            /* dopo virgola ci aspettiamo chiave se siamo in un oggetto */
            i++; continue;
        }
        /* Stringa */
        if (code[i] == '"') {
            int j = i + 1;
            while (j < n) {
                if (code[j] == '\\') { j += 2; continue; }
                if (code[j] == '"')  { j++;    break; }
                j++;
            }
            /* È una chiave se il prossimo non-spazio è ':' */
            int k = j;
            while (k < n && code[k].isSpace()) k++;
            const bool isKey = (k < n && code[k] == ':');
            out += span(isKey ? kAt : kStr, code.mid(i, j - i));
            i = j;
            continue;
        }
        /* Keyword JSON: true, false, null */
        if (code[i].isLetter()) {
            int j = i;
            while (j < n && code[j].isLetter()) j++;
            const QString w = code.mid(i, j - i);
            if (w == "true" || w == "false" || w == "null")
                out += span(kKw, w);
            else
                out += plain(w);
            i = j; continue;
        }
        /* Numero */
        if (code[i].isDigit() || code[i] == '-') {
            int j = i;
            if (code[j] == '-') j++;
            while (j < n && (code[j].isDigit() || code[j] == '.' ||
                              code[j] == 'e' || code[j] == 'E' ||
                              code[j] == '+' || code[j] == '-')) j++;
            out += span(kNum, code.mid(i, j - i));
            i = j; continue;
        }
        out += plain(QString(code[i]));
        i++;
    }
    return out;
}

/* ─── Entry point ─────────────────────────────────────────── */
static QString syntaxHighlight(const QString& rawCode, const QString& lang)
{
    const QString lo = lang.toLower();

    if (lo == "python" || lo == "py")
        return highlightClike(rawCode, kwPy(), "#", false, true);

    if (lo == "bash" || lo == "sh" || lo == "shell" || lo == "zsh")
        return highlightBash(rawCode);

    if (lo == "sql")
        return highlightSql(rawCode);

    if (lo == "html" || lo == "xml" || lo == "svg")
        return highlightHtml(rawCode);

    if (lo == "json")
        return highlightJson(rawCode);

    if (lo == "rust")
        return highlightClike(rawCode, kwRust());

    if (lo == "go")
        return highlightClike(rawCode, kwGo());

    if (lo == "java")
        return highlightClike(rawCode, kwJava());

    if (lo == "javascript" || lo == "js" ||
        lo == "typescript" || lo == "ts")
        return highlightClike(rawCode, kwJs(), "//", true, false);

    if (lo == "c" || lo == "cpp" || lo == "c++" || lo == "cxx" ||
        lo == "h" || lo == "hpp" || lo == "cuda")
        return highlightClike(rawCode, kwC());

    /* Fallback: solo escape HTML */
    return rawCode.toHtmlEscaped();
}

} // namespace SH

/* ── Mappa linguaggio → {emoji, colore badge, estensione file} ── */
struct LangMeta { const char* icon; const char* color; const char* ext; };
static const QMap<QString, LangMeta>& langMap()
{
    static const QMap<QString, LangMeta> m = {
        {"python",     {"\xf0\x9f\x90\x8d", "#3572A5", "py"}},   /* 🐍 */
        {"py",         {"\xf0\x9f\x90\x8d", "#3572A5", "py"}},
        {"bash",       {"\xe2\x9a\xa1",      "#89e051", "sh"}},   /* ⚡ */
        {"sh",         {"\xe2\x9a\xa1",      "#89e051", "sh"}},
        {"shell",      {"\xe2\x9a\xa1",      "#89e051", "sh"}},
        {"zsh",        {"\xe2\x9a\xa1",      "#89e051", "sh"}},
        {"c",          {"\xe2\x9a\x99",      "#555555", "c"}},    /* ⚙ */
        {"cpp",        {"\xe2\x9a\x99",      "#f34b7d", "cpp"}},
        {"c++",        {"\xe2\x9a\x99",      "#f34b7d", "cpp"}},
        {"cxx",        {"\xe2\x9a\x99",      "#f34b7d", "cpp"}},
        {"h",          {"\xf0\x9f\x93\x8b",  "#f34b7d", "h"}},   /* 📋 */
        {"hpp",        {"\xf0\x9f\x93\x8b",  "#f34b7d", "hpp"}},
        {"java",       {"\xe2\x98\x95",      "#b07219", "java"}}, /* ☕ */
        {"javascript", {"\xf0\x9f\x9f\xa8",  "#f1e05a", "js"}},  /* 🟨 */
        {"js",         {"\xf0\x9f\x9f\xa8",  "#f1e05a", "js"}},
        {"typescript", {"\xf0\x9f\x94\xb7",  "#2b7489", "ts"}},  /* 🔷 */
        {"ts",         {"\xf0\x9f\x94\xb7",  "#2b7489", "ts"}},
        {"html",       {"\xf0\x9f\x8c\x90",  "#e34c26", "html"}},/* 🌐 */
        {"css",        {"\xf0\x9f\x8e\xa8",  "#563d7c", "css"}}, /* 🎨 */
        {"sql",        {"\xf0\x9f\x97\x84",  "#e38c00", "sql"}}, /* 🗄 */
        {"json",       {"\xf0\x9f\x93\xa6",  "#40d47e", "json"}},/* 📦 */
        {"yaml",       {"\xf0\x9f\x93\x84",  "#cb171e", "yaml"}},/* 📄 */
        {"yml",        {"\xf0\x9f\x93\x84",  "#cb171e", "yml"}},
        {"xml",        {"\xf0\x9f\x93\x83",  "#0060ac", "xml"}}, /* 📃 */
        {"rust",       {"\xf0\x9f\xa6\x80",  "#dea584", "rs"}},  /* 🦀 */
        {"go",         {"\xf0\x9f\x90\xb9",  "#00add8", "go"}},  /* 🐹 */
        {"ruby",       {"\xf0\x9f\x92\x8e",  "#701516", "rb"}},  /* 💎 */
        {"rb",         {"\xf0\x9f\x92\x8e",  "#701516", "rb"}},
        {"php",        {"\xf0\x9f\x90\x98",  "#4F5D95", "php"}}, /* 🐘 */
        {"swift",      {"\xf0\x9f\xa6\x85",  "#ffac45", "swift"}},/* 🦅 */
        {"kotlin",     {"\xf0\x9f\x8e\xaf",  "#F18E33", "kt"}},  /* 🎯 */
        {"r",          {"\xf0\x9f\x93\x8a",  "#198CE7", "r"}},   /* 📊 */
        {"matlab",     {"\xf0\x9f\x93\x90",  "#e16737", "m"}},   /* 📐 */
        {"dart",       {"\xf0\x9f\x8e\xaf",  "#00B4AB", "dart"}},
        {"cmake",      {"\xf0\x9f\x94\xa7",  "#064F8C", "cmake"}},/* 🔧 */
        {"makefile",   {"\xf0\x9f\x94\xa7",  "#427819", "mk"}},
        {"dockerfile", {"\xf0\x9f\x90\xb3",  "#384d54", "Dockerfile"}},/* 🐳 */
        {"markdown",   {"\xf0\x9f\x93\x9d",  "#083fa1", "md"}},  /* 📝 */
        {"md",         {"\xf0\x9f\x93\x9d",  "#083fa1", "md"}},
        {"lua",        {"\xf0\x9f\x8c\x99",  "#000080", "lua"}}, /* 🌙 */
        {"perl",       {"\xf0\x9f\xaa\xb2",  "#0298c3", "pl"}},  /* 🪲 */
        {"scala",      {"\xf0\x9f\x94\xb4",  "#c22d40", "scala"}},/* 🔴 */
        {"haskell",    {"\xce\xbb",           "#5e5086", "hs"}},  /* λ */
        {"toml",       {"\xf0\x9f\x93\x84",  "#9c4221", "toml"}},
        {"ini",        {"\xf0\x9f\x93\x84",  "#888888", "ini"}},
        {"text",       {"\xf0\x9f\x93\x84",  "#888888", "txt"}},
        {"txt",        {"\xf0\x9f\x93\x84",  "#888888", "txt"}},
    };
    return m;
}

/* Costruisce l'HTML della barra header del blocco codice */
static QString codeHeader(const QString& lang, int blockId)
{
    const auto& lm = langMap();
    const auto it  = lm.constFind(lang.toLower());

    QString icon  = "\xf0\x9f\x93\x84"; /* 📄 default */
    QString color = "#8b949e";
    if (it != lm.constEnd()) {
        icon  = QString::fromUtf8(it->icon);
        color = it->color;
    }
    const QString displayLang = lang.isEmpty() ? "text" : lang.toLower();
    const QString id          = QString::number(blockId);

    return
        "<table width='100%' cellpadding='0' cellspacing='0'"
        " style='background:#161b22;border:1px solid #30363d;"
        "border-radius:6px 6px 0 0;border-bottom:none;margin:8px 0 0 0;'>"
        "<tr>"
        "<td style='padding:5px 14px;'>"
          "<span style='color:" + color + ";font-size:11px;font-weight:bold;"
                "font-family:\"JetBrains Mono\",monospace;'>"
            + icon + " " + displayLang.toHtmlEscaped() +
          "</span>"
        "</td>"
        "<td align='right' style='padding:5px 14px;white-space:nowrap;'>"
          "<a href='code:copy:" + id + "'"
            " style='color:#58a6ff;font-size:11px;font-weight:bold;"
                   "text-decoration:none;'>"
            "\xf0\x9f\x93\x8b Copia"   /* 📋 */
          "</a>"
          "&nbsp;&nbsp;&nbsp;"
          "<a href='code:save:" + id + "'"
            " style='color:#58a6ff;font-size:11px;font-weight:bold;"
                   "text-decoration:none;'>"
            "\xf0\x9f\x92\xbe Salva"   /* 💾 */
          "</a>"
        "</td>"
        "</tr>"
        "</table>";
}

QString AgentiPage::markdownToHtml(const QString& md,
    QMap<int,QPair<QString,QString>>* codeBlocks,
    int* codeCounter)
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
    bool    inCodeBlock   = false;
    bool    inOrderedList = false;
    bool    inTable       = false;
    bool    tableHdrDone  = false;
    bool    tableOddRow   = false;
    QString codeBuf;
    QString codeLang;

    auto closeList = [&](){
        if (inOrderedList) { html += "</ol>\n"; inOrderedList = false; }
    };

    auto isTableSep = [](const QString& line) -> bool {
        const QStringList cells = line.split('|', Qt::SkipEmptyParts);
        if (cells.isEmpty()) return false;
        for (const QString& c : cells) {
            const QString t = c.trimmed();
            if (t.isEmpty()) continue;
            for (QChar ch : t)
                if (ch != '-' && ch != ':' && ch != ' ') return false;
        }
        return true;
    };

    auto flushTable = [&](){
        if (!inTable) return;
        if (tableHdrDone) html += "</tbody>\n";
        html += "</table>\n";
        inTable = false; tableHdrDone = false; tableOddRow = false;
    };

    /* Emette un blocco codice con header barra lingua+azioni */
    auto flushCode = [&](const QString& buf, const QString& lang) {
        const QString trimmed = buf.trimmed();
        if (trimmed.isEmpty()) return;
        /* Syntax highlighting o semplice escape */
        const QString esc = SH::syntaxHighlight(trimmed, lang);

        /* Calcola ID per questo blocco */
        int blockId = 0;
        if (codeCounter) blockId = (*codeCounter)++;
        if (codeBlocks)  (*codeBlocks)[blockId] = { lang.toLower(), buf.trimmed() };

        /* Header barra solo se i gestori sono connessi (codeBlocks fornito) */
        if (codeBlocks)
            html += codeHeader(lang, blockId);

        /* Blocco <pre> */
        const QString radius = codeBlocks
            ? "0 0 6px 6px" : "6px";
        const QString marginTop = codeBlocks ? "0" : "8px";

        html += "<pre style='background:#0d1117;color:#e6edf3;"
                "border:1px solid #30363d;border-radius:" + radius + ";"
                "padding:10px 14px;margin:" + marginTop + " 0 8px 0;"
                "overflow-x:auto;"
                "font-family:\"JetBrains Mono\",\"Fira Code\",monospace;"
                "font-size:12px;line-height:1.5;white-space:pre;'>"
                + esc + "</pre>\n";
    };

    for (const QString& rawLine : lines) {
        /* ── Code fence ── */
        if (rawLine.startsWith("```")) {
            if (!inCodeBlock) {
                closeList();
                flushTable();
                inCodeBlock = true;
                codeBuf.clear();
                codeLang = rawLine.mid(3).trimmed(); /* es. "python", "bash" */
            } else {
                inCodeBlock = false;
                flushCode(codeBuf, codeLang);
                codeBuf.clear();
                codeLang.clear();
            }
            continue;
        }
        if (inCodeBlock) { codeBuf += rawLine + '\n'; continue; }

        const QString line = rawLine;

        /* ── Tabella Markdown (righe con |) ── */
        if (line.trimmed().startsWith('|')) {
            closeList();
            if (!inTable) {
                html += "<table style='border-collapse:collapse;width:100%;"
                        "margin:10px 0;font-size:13px;'>\n";
                inTable = true; tableHdrDone = false; tableOddRow = false;
            }
            if (isTableSep(line)) {
                tableHdrDone = true;
                html += "<tbody>\n";
                continue;
            }
            const QStringList cells = line.split('|', Qt::SkipEmptyParts);
            if (!tableHdrDone) {
                html += "<thead><tr>\n";
                for (const QString& cell : cells)
                    html += "<th style='border:1px solid #30363d;padding:7px 12px;"
                            "background:#1e293b;color:#79c0ff;font-weight:bold;"
                            "text-align:left;'>" + inlineFmt(cell.trimmed()) + "</th>\n";
                html += "</tr></thead>\n";
            } else {
                const QString bg = tableOddRow ? "#0d1117" : "#111827";
                tableOddRow = !tableOddRow;
                html += "<tr>\n";
                for (const QString& cell : cells)
                    html += "<td style='border:1px solid #30363d;padding:6px 12px;"
                            "color:#e2e8f0;background:" + bg + ";'>"
                            + inlineFmt(cell.trimmed()) + "</td>\n";
                html += "</tr>\n";
            }
            continue;
        }
        if (inTable) flushTable();

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
            html += "<div style='height:8px;'></div>\n";
            continue;
        }

        /* ── Separatore --- ── */
        if (line.trimmed() == "---" || line.trimmed() == "***") {
            html += "<hr style='border:none;border-top:1px solid #30363d;margin:10px 0;'>\n";
            continue;
        }

        /* ── Paragrafo normale ── */
        html += "<p style='margin:3px 0 7px 0;line-height:1.75;'>" + inlineFmt(line) + "</p>\n";
    }
    closeList();
    flushTable();

    /* Code block non chiuso (fallback) */
    if (inCodeBlock && !codeBuf.isEmpty())
        flushCode(codeBuf, codeLang);

    return html;
}

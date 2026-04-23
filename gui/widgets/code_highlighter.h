#pragma once
/*
 * code_highlighter.h -- Syntax highlighter per QPlainTextEdit
 * Supporta: Python . C . C++ . Bash . JavaScript
 * Colori ispirati a VS Code Dark+ (sfondi scuri e chiari).
 */
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QList>
#include <QColor>

class CodeHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    enum Language { Python = 0, C, Cpp, Bash, JavaScript };

    explicit CodeHighlighter(QTextDocument* parent = nullptr)
        : QSyntaxHighlighter(parent) { buildFormats(); buildRules(); }

    void setLanguage(Language lang) {
        m_lang = lang;
        buildRules();
        rehighlight();
    }
    Language language() const { return m_lang; }

protected:
    void highlightBlock(const QString& text) override
    {
        // Regole semplici (singola linea)
        for (const Rule& r : m_rules) {
            QRegularExpressionMatchIterator it = r.pattern.globalMatch(text);
            while (it.hasNext()) {
                const auto m = it.next();
                setFormat(m.capturedStart(), m.capturedLength(), r.fmt);
            }
        }

        // Commenti multi-linea tipo /* ... */ (C / C++ / JS, stato blocco 1)
        if (m_mlCommentStart.pattern().isEmpty()) {
            setCurrentBlockState(0);
            return;
        }

        int startIdx = 0;
        if (previousBlockState() != 1)
            startIdx = text.indexOf(m_mlCommentStart);

        while (startIdx >= 0) {
            const auto mEnd = m_mlCommentEnd.match(text, startIdx);
            int commentLen;
            if (mEnd.hasMatch()) {
                commentLen = mEnd.capturedStart() - startIdx + mEnd.capturedLength();
                setCurrentBlockState(0);
            } else {
                setCurrentBlockState(1);
                commentLen = text.length() - startIdx;
            }
            setFormat(startIdx, commentLen, m_commentFmt);
            startIdx = text.indexOf(m_mlCommentStart, startIdx + commentLen);
        }

        // Triple-quoted strings Python (stato 2 per """ e stato 3 per ''')
        if (m_lang == Python) {
            highlightTriple(text, "\"\"\"", 2);
            highlightTriple(text, "'''",   3);
        }
    }

private:
    struct Rule { QRegularExpression pattern; QTextCharFormat fmt; };

    Language    m_lang = Python;
    QList<Rule> m_rules;

    QTextCharFormat m_keywordFmt;
    QTextCharFormat m_keyword2Fmt;
    QTextCharFormat m_stringFmt;
    QTextCharFormat m_commentFmt;
    QTextCharFormat m_numberFmt;
    QTextCharFormat m_funcFmt;
    QTextCharFormat m_preprocFmt;

    QRegularExpression m_mlCommentStart;
    QRegularExpression m_mlCommentEnd;

    void buildFormats() {
        auto mk = [](const char* hex, bool bold = false, bool italic = false) {
            QTextCharFormat f;
            f.setForeground(QColor(hex));
            if (bold)   f.setFontWeight(QFont::Bold);
            if (italic) f.setFontItalic(true);
            return f;
        };
        m_keywordFmt  = mk("#569cd6", true);
        m_keyword2Fmt = mk("#4ec9b0");
        m_stringFmt   = mk("#ce9178");
        m_commentFmt  = mk("#6a9955", false, true);
        m_numberFmt   = mk("#b5cea8");
        m_funcFmt     = mk("#dcdcaa");
        m_preprocFmt  = mk("#c586c0");
    }

    void add(const QString& pat, const QTextCharFormat& fmt) {
        m_rules.append({ QRegularExpression(pat), fmt });
    }

    void buildRules() {
        m_rules.clear();
        m_mlCommentStart = QRegularExpression();
        m_mlCommentEnd   = QRegularExpression();
        switch (m_lang) {
        case Python:     buildPython();     break;
        case C:          buildC(false);     break;
        case Cpp:        buildC(true);      break;
        case Bash:       buildBash();       break;
        case JavaScript: buildJavaScript(); break;
        }
    }

    // ---------------------------------------------------------------
    // PYTHON
    // ---------------------------------------------------------------
    void buildPython() {
        add("\\b(False|None|True|and|as|assert|async|await|break|class|continue"
            "|def|del|elif|else|except|finally|for|from|global|if|import|in|is"
            "|lambda|nonlocal|not|or|pass|raise|return|try|while|with|yield)\\b",
            m_keywordFmt);

        add("\\b(abs|all|any|bin|bool|bytes|callable|chr|complex|dict|dir|divmod"
            "|enumerate|eval|exec|filter|float|format|frozenset|getattr|globals"
            "|hasattr|hash|help|hex|id|input|int|isinstance|issubclass|iter|len"
            "|list|locals|map|max|memoryview|min|next|object|oct|open|ord|pow"
            "|print|property|range|repr|reversed|round|set|setattr|slice|sorted"
            "|staticmethod|str|sum|super|tuple|type|vars|zip)\\b",
            m_keyword2Fmt);

        // decoratori
        add("@\\w+", m_preprocFmt);

        // numeri
        add("\\b(0[xXbBoO][\\da-fA-F_]+|\\d[\\d_]*\\.?[\\d_]*([eE][+-]?\\d+)?[jJ]?)\\b",
            m_numberFmt);

        // stringhe singola/doppia linea (semplici -- le triple sono gestite dopo)
        add("\"[^\"\\\\\\n]*(?:\\\\.[^\"\\\\\\n]*)*\"", m_stringFmt);
        add("'[^'\\\\\\n]*(?:\\\\.[^'\\\\\\n]*)*'",    m_stringFmt);
        // prefix f/b/r/u
        add("\\b[fFbBrRuU]+\"[^\"\\\\\\n]*(?:\\\\.[^\"\\\\\\n]*)*\"", m_stringFmt);
        add("\\b[fFbBrRuU]+'[^'\\\\\\n]*(?:\\\\.[^'\\\\\\n]*)*'",    m_stringFmt);

        // nome funzione dopo def
        add("\\bdef\\s+(\\w+)", m_funcFmt);

        // commento #
        add("#[^\\n]*", m_commentFmt);
    }

    // ---------------------------------------------------------------
    // C / C++
    // ---------------------------------------------------------------
    void buildC(bool cppMode) {
        // preprocessore
        add("^\\s*#\\s*(include|define|undef|pragma|ifdef|ifndef|endif|if|elif|else"
            "|error|warning|line)\\b[^\\n]*",
            m_preprocFmt);

        // keyword C base
        add("\\b(auto|break|case|const|continue|default|do|else|enum|extern|for"
            "|goto|if|inline|register|restrict|return|sizeof|static|struct|switch"
            "|typedef|union|unsigned|void|volatile|while)\\b",
            m_keywordFmt);

        if (cppMode) {
            add("\\b(alignas|alignof|and|and_eq|asm|bitand|bitor|bool|catch|char8_t"
                "|char16_t|char32_t|class|co_await|co_return|co_yield|compl|concept"
                "|consteval|constexpr|constinit|const_cast|decltype|delete"
                "|dynamic_cast|explicit|export|false|friend|mutable|namespace|new"
                "|noexcept|not|not_eq|nullptr|operator|or|or_eq|override|private"
                "|protected|public|reinterpret_cast|requires|static_assert"
                "|static_cast|template|this|throw|true|try|typeid|typename|using"
                "|virtual|xor|xor_eq)\\b",
                m_keywordFmt);
        }

        // tipi
        add("\\b(char|double|float|int|long|short|signed|wchar_t|size_t|ptrdiff_t"
            "|uint8_t|uint16_t|uint32_t|uint64_t|int8_t|int16_t|int32_t|int64_t"
            "|ssize_t|FILE|NULL|nullptr_t|string|vector|map|set|pair|tuple|optional"
            "|variant|unique_ptr|shared_ptr|weak_ptr|array|deque|list|queue|stack"
            "|unordered_map|unordered_set)\\b",
            m_keyword2Fmt);

        // numeri
        add("\\b(0[xX][\\da-fA-F]+[uUlL]*|\\d+\\.?\\d*([eE][+-]?\\d+)?[fFlLuU]*)\\b",
            m_numberFmt);

        // stringhe
        add("\"[^\"\\\\\\n]*(?:\\\\.[^\"\\\\\\n]*)*\"", m_stringFmt);
        add("'[^'\\\\\\n]*(?:\\\\.[^'\\\\\\n]*)*'",    m_stringFmt);

        // nome funzione prima di (
        add("\\b([A-Za-z_]\\w*)\\s*(?=\\()", m_funcFmt);

        // commento //
        add("//[^\\n]*", m_commentFmt);

        // commento multi-linea
        m_mlCommentStart = QRegularExpression("/\\*");
        m_mlCommentEnd   = QRegularExpression("\\*/");
    }

    // ---------------------------------------------------------------
    // BASH
    // ---------------------------------------------------------------
    void buildBash() {
        // shebang
        add("^#!.*", m_preprocFmt);

        // keyword
        add("\\b(if|then|else|elif|fi|for|while|do|done|case|esac|in|function"
            "|return|local|export|readonly|declare|typeset|unset|shift|source"
            "|alias|echo|printf|read|exit|break|continue|select|until)\\b",
            m_keywordFmt);

        // comandi comuni
        add("\\b(cd|ls|pwd|mv|cp|rm|mkdir|rmdir|chmod|chown|grep|sed|awk|find"
            "|sort|uniq|wc|cat|head|tail|cut|tr|xargs|tee|curl|wget|git|make"
            "|cmake|python3|python|pip3|pip|apt|dnf|pacman|systemctl|journalctl"
            "|ssh|scp|rsync)\\b",
            m_keyword2Fmt);

        // variabili $VAR ${VAR}
        add("\\$\\{?[A-Za-z_]\\w*\\}?|\\$\\(", m_preprocFmt);

        // numeri
        add("\\b\\d+\\.?\\d*\\b", m_numberFmt);

        // stringhe
        add("\"[^\"\\\\\\n]*(?:\\\\.[^\"\\\\\\n]*)*\"", m_stringFmt);
        add("'[^']*'", m_stringFmt);

        // commento #
        add("#[^\\n]*", m_commentFmt);
    }

    // ---------------------------------------------------------------
    // JAVASCRIPT
    // ---------------------------------------------------------------
    void buildJavaScript() {
        // keyword
        add("\\b(async|await|break|case|catch|class|const|continue|debugger"
            "|default|delete|do|else|export|extends|finally|for|from|function"
            "|if|import|in|instanceof|let|new|of|return|static|super|switch"
            "|throw|try|typeof|var|void|while|with|yield)\\b",
            m_keywordFmt);

        // builtin
        add("\\b(Array|Boolean|Date|Error|Function|JSON|Map|Math|Number|Object"
            "|Promise|Proxy|Reflect|RegExp|Set|String|Symbol|WeakMap|WeakSet"
            "|Infinity|NaN|undefined|null|true|false|console|document|window"
            "|globalThis|process|require|module|exports)\\b",
            m_keyword2Fmt);

        // numeri
        add("\\b(0[xX][\\da-fA-F]+n?|\\d+\\.?\\d*([eE][+-]?\\d+)?n?)\\b",
            m_numberFmt);

        // template literal
        add("`[^`\\\\]*(?:\\\\.[^`\\\\]*)*`", m_stringFmt);
        // stringhe
        add("\"[^\"\\\\\\n]*(?:\\\\.[^\"\\\\\\n]*)*\"", m_stringFmt);
        add("'[^'\\\\\\n]*(?:\\\\.[^'\\\\\\n]*)*'",    m_stringFmt);

        // nome funzione prima di (
        add("\\b([A-Za-z_$]\\w*)\\s*(?=\\()", m_funcFmt);

        // commento //
        add("//[^\\n]*", m_commentFmt);

        // commento multi-linea
        m_mlCommentStart = QRegularExpression("/\\*");
        m_mlCommentEnd   = QRegularExpression("\\*/");
    }

    // Gestione triple-quoted Python
    void highlightTriple(const QString& text, const QString& delim, int state)
    {
        const QRegularExpression re(QRegularExpression::escape(delim));
        int startIdx = 0;

        if (previousBlockState() == state) {
            const auto m = re.match(text, 0);
            if (m.hasMatch()) {
                setFormat(0, m.capturedStart() + m.capturedLength(), m_stringFmt);
                setCurrentBlockState(0);
                startIdx = m.capturedStart() + m.capturedLength();
            } else {
                setFormat(0, text.length(), m_stringFmt);
                setCurrentBlockState(state);
                return;
            }
        }

        while (true) {
            const auto mS = re.match(text, startIdx);
            if (!mS.hasMatch()) break;
            int s = mS.capturedStart();
            const auto mE = re.match(text, s + (int)delim.length());
            if (mE.hasMatch()) {
                int e = mE.capturedStart() + mE.capturedLength();
                setFormat(s, e - s, m_stringFmt);
                startIdx = e;
            } else {
                setFormat(s, text.length() - s, m_stringFmt);
                setCurrentBlockState(state);
                break;
            }
        }
    }
};

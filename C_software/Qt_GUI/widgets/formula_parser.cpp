#include "formula_parser.h"
#include <QRegularExpression>
#include <cmath>
#include <limits>

/* ══════════════════════════════════════════════════════════════
   FormulaParser — costruttore + validazione
   ══════════════════════════════════════════════════════════════ */
FormulaParser::FormulaParser(const QString& expr) : m_expr(expr.trimmed()) {
    /* test parse con x = 1.0 */
    m_pos = 0; m_xVal = 1.0; m_errHit = false;
    double r = parseExpr();
    (void)r;
    skipWs();
    if (m_errHit || m_pos < m_expr.size()) {
        m_ok  = false;
        m_err = "Espressione non valida";
    } else {
        m_ok = true;
    }
}

/* ══════════════════════════════════════════════════════════════
   eval — valuta per un singolo x
   ══════════════════════════════════════════════════════════════ */
double FormulaParser::eval(double xVal) const {
    m_pos = 0; m_xVal = xVal; m_errHit = false;
    double result = parseExpr();
    if (m_errHit) return std::numeric_limits<double>::quiet_NaN();
    return result;
}

/* ══════════════════════════════════════════════════════════════
   sample — campiona n punti nell'intervallo [xMin, xMax]
   ══════════════════════════════════════════════════════════════ */
QVector<QPointF> FormulaParser::sample(double xMin, double xMax, int n) const {
    QVector<QPointF> pts;
    if (n < 2 || xMax <= xMin) return pts;
    const double step = (xMax - xMin) / (n - 1);
    for (int i = 0; i < n; ++i) {
        double x = xMin + i * step;
        double y = eval(x);
        if (std::isfinite(y)) pts.append({x, y});
    }
    return pts;
}

/* ── helper: normalizza newline e spazi multipli in uno spazio ── */
static QString normalizeWs(const QString& s) {
    QString r = s;
    r.replace('\n', ' ').replace('\r', ' ').replace('\t', ' ');
    /* collassa spazi multipli */
    static const QRegularExpression reSpaces(R"( {2,})");
    r.replace(reSpaces, " ");
    return r.trimmed();
}

/* ══════════════════════════════════════════════════════════════
   tryExtract — estrae la formula RHS dal testo
   Pattern 1: y = …, f(x) = …, g(x) = …, h(x) = …, F(x) = …
   Pattern 2: keyword grafico/plotta/traccia/disegna + espressione con x
              es. "grafico di sin(x)*2", "plotta x^2+1 per x da -5 a 5"
   ══════════════════════════════════════════════════════════════ */
QString FormulaParser::tryExtract(const QString& text) {
    const QString norm = normalizeWs(text);  /* newline → spazio, spazi multipli → uno */

    /* ── Pattern 1: y=... o f(x)=... ── */
    static const QRegularExpression re1(
        R"((?:y|[fghFGH]\s*\(\s*x\s*\))\s*=\s*([^,;]{3,}))",
        QRegularExpression::CaseInsensitiveOption);
    auto m = re1.match(norm);
    if (m.hasMatch()) return m.captured(1).trimmed();

    /* ── Pattern 2: keyword + espressione (deve contenere x o funzione nota) ── */
    static const QRegularExpression re2(
        R"((?:grafico|plott[ao]|traccia|disegna|visualizza|mostra|stampa)\s+)"
        R"((?:(?:di|del|della|per|la(?:\s+funzione)?)\s+)?([^,;:]{3,}))",
        QRegularExpression::CaseInsensitiveOption);
    auto m2 = re2.match(norm);
    if (m2.hasMatch()) {
        QString candidate = m2.captured(1).trimmed();
        /* Deve contenere x o una funzione trigonometrica/matematica */
        static const QRegularExpression reHasX(
            R"(\bx\b|sin|cos|tan|sqrt|log|exp|abs)",
            QRegularExpression::CaseInsensitiveOption);
        if (reHasX.match(candidate).hasMatch()) {
            /* Escludi coppie di coordinate tipo "x=3 + y=10" o "x=3, y=10":
               se il candidato contiene "y = <numero>" è una coppia, non una formula */
            static const QRegularExpression reCoordPair(
                R"(\by\s*=\s*[+-]?\s*\d)", QRegularExpression::CaseInsensitiveOption);
            if (reCoordPair.match(candidate).hasMatch()) return {};
            /* Taglia l'espressione al primo token che non può far parte di essa
               (es. "per x da -5 a 5" dopo la formula) */
            static const QRegularExpression reStop(
                R"(\s+(?:per\s+x|da\s+x|con\s+x|nel(?:l[ao])?\s*intervall))");
            const int stop = candidate.indexOf(reStop);
            if (stop > 0) candidate = candidate.left(stop).trimmed();
            return candidate;
        }
    }
    return {};
}

/* ══════════════════════════════════════════════════════════════
   tryExtractXRange — cerca un intervallo x nel testo
   Riconosce:
     "da x=-5 a x=5"         "da x = -5 a x = 5"
     "x ∈ [-10, 10]"         "x in [-10, 10]"
     "x da -3 a 3"           "[-5, 5]" (vicino a keyword grafico)
   ══════════════════════════════════════════════════════════════ */
bool FormulaParser::tryExtractXRange(const QString& text, double& xMin, double& xMax) {
    const QString norm = normalizeWs(text);
    auto toD = [](const QString& s) -> double {
        return s.trimmed().replace(',', '.').replace(' ', "").toDouble();
    };

    /* Pattern 1: "da x=-5 a x=5" */
    static const QRegularExpression re1(
        R"(da\s+x\s*=\s*([+-]?\s*\d+(?:[.,]\d+)?)\s+a\s+x\s*=\s*([+-]?\s*\d+(?:[.,]\d+)?))",
        QRegularExpression::CaseInsensitiveOption);
    auto m1 = re1.match(norm);
    if (m1.hasMatch()) { xMin = toD(m1.captured(1)); xMax = toD(m1.captured(2)); return xMin < xMax; }

    /* Pattern 2: "x in [-10, 10]" */
    static const QRegularExpression re2(
        R"(x\s+in\s+\[?\s*([+-]?\s*\d+(?:[.,]\d+)?)\s*[,;]\s*([+-]?\s*\d+(?:[.,]\d+)?)\s*\]?)",
        QRegularExpression::CaseInsensitiveOption);
    auto m2 = re2.match(norm);
    if (m2.hasMatch()) { xMin = toD(m2.captured(1)); xMax = toD(m2.captured(2)); return xMin < xMax; }

    /* Pattern 3: "x da -3 a 3" */
    static const QRegularExpression re3(
        R"(x\s+da\s+([+-]?\s*\d+(?:[.,]\d+)?)\s+a\s+([+-]?\s*\d+(?:[.,]\d+)?))",
        QRegularExpression::CaseInsensitiveOption);
    auto m3 = re3.match(norm);
    if (m3.hasMatch()) { xMin = toD(m3.captured(1)); xMax = toD(m3.captured(2)); return xMin < xMax; }

    /* Pattern 4: "per x da -5 a 5" */
    static const QRegularExpression re4(
        R"(per\s+x\s+da\s+([+-]?\s*\d+(?:[.,]\d+)?)\s+a\s+([+-]?\s*\d+(?:[.,]\d+)?))",
        QRegularExpression::CaseInsensitiveOption);
    auto m4 = re4.match(norm);
    if (m4.hasMatch()) { xMin = toD(m4.captured(1)); xMax = toD(m4.captured(2)); return xMin < xMax; }

    return false;
}

/* ══════════════════════════════════════════════════════════════
   tryExtractPoints — estrae coppie (x, y) dal testo
   Riconosce:
     "x=3, y=5"   "x=3 e y=5"   "x = -2.5, y = 7"
     "(3, 5)"      "(3,5)"
     più coppie separate da ';' o '\n' o ' - '
   ══════════════════════════════════════════════════════════════ */
QVector<QPointF> FormulaParser::tryExtractPoints(const QString& text) {
    /* ── Trigger: testo deve contenere "grafico" o "cartesian" o "punto" ──
       Evita false positive su formule normali                             */
    const QString low = text.toLower();
    const bool hasCartesianKeyword =
        low.contains("grafico") || low.contains("cartesian") ||
        low.contains("punto")   || low.contains("punti")     ||
        low.contains("piano")   || low.contains("traccia")   ||
        low.contains("disegna") || low.contains("plotta");
    if (!hasCartesianKeyword) return {};

    QVector<QPointF> pts;

    /* Numero con segno opzionale e decimale (virgola o punto) */
    static const QString NUM =
        R"([+-]?\s*\d+(?:[.,]\d+)?)";

    /* Pattern 1: x=N ...qualcosa... y=M */
    static const QRegularExpression reXY(
        "x\\s*=\\s*(" + NUM + ")\\b.{0,20}?\\by\\s*=\\s*(" + NUM + ")",
        QRegularExpression::CaseInsensitiveOption);

    /* Pattern 2: (N, M) */
    static const QRegularExpression rePair(
        "\\(\\s*(" + NUM + ")\\s*,\\s*(" + NUM + ")\\s*\\)");

    auto toDouble = [](const QString& s) -> double {
        return s.trimmed().replace(',', '.').replace(' ', "").toDouble();
    };

    auto it = reXY.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        pts.append({ toDouble(m.captured(1)), toDouble(m.captured(2)) });
    }

    if (pts.isEmpty()) {
        auto it2 = rePair.globalMatch(text);
        while (it2.hasNext()) {
            auto m = it2.next();
            pts.append({ toDouble(m.captured(1)), toDouble(m.captured(2)) });
        }
    }

    /* Rimuovi duplicati */
    QVector<QPointF> uniq;
    for (const auto& pt : pts) {
        bool dup = false;
        for (const auto& u : uniq)
            if (qFuzzyCompare(u.x(), pt.x()) && qFuzzyCompare(u.y(), pt.y()))
                { dup = true; break; }
        if (!dup) uniq.append(pt);
    }
    return uniq;
}

/* ══════════════════════════════════════════════════════════════
   helpers di navigazione
   ══════════════════════════════════════════════════════════════ */
void FormulaParser::skipWs() const {
    while (m_pos < m_expr.size() && m_expr[m_pos].isSpace()) ++m_pos;
}
QChar FormulaParser::peek() const {
    skipWs();
    return m_pos < m_expr.size() ? m_expr[m_pos] : QChar(0);
}
QChar FormulaParser::consume() const {
    skipWs();
    return m_pos < m_expr.size() ? m_expr[m_pos++] : QChar(0);
}

/* ══════════════════════════════════════════════════════════════
   parser ricorsivo discendente
   ══════════════════════════════════════════════════════════════ */
double FormulaParser::parseExpr() const {
    double v = parseTerm();
    for (;;) {
        QChar c = peek();
        if      (c == '+') { consume(); v += parseTerm(); }
        else if (c == '-') { consume(); v -= parseTerm(); }
        else break;
    }
    return v;
}

double FormulaParser::parseTerm() const {
    double v = parsePow();
    for (;;) {
        QChar c = peek();
        if (c == '*') { consume(); v *= parsePow(); }
        else if (c == '/') {
            consume();
            double d = parsePow();
            v = (std::abs(d) > 1e-300) ? v / d
                                        : std::numeric_limits<double>::quiet_NaN();
        }
        else break;
    }
    return v;
}

double FormulaParser::parsePow() const {
    double base = parseUnary();
    if (peek() == '^') { consume(); base = std::pow(base, parseUnary()); }
    return base;
}

double FormulaParser::parseUnary() const {
    QChar c = peek();
    if (c == '-') { consume(); return -parseAtom(); }
    if (c == '+') { consume(); return  parseAtom(); }
    return parseAtom();
}

double FormulaParser::parseAtom() const {
    QChar c = peek();

    /* Parentesi */
    if (c == '(') {
        consume();
        double v = parseExpr();
        if (peek() == ')') consume();
        else                m_errHit = true;
        return v;
    }

    /* Numero letterale */
    if (c.isDigit() || c == '.') return parseNum();

    /* Variabile / costante / funzione */
    if (c.isLetter() || c == '_') {
        QString name = parseName();

        /* Costanti */
        if (name == "pi" || name == "PI") return M_PI;
        if (name == "e"  || name == "E")  return M_E;
        if (name == "x"  || name == "X")  return m_xVal;

        /* Funzioni unarie */
        if (peek() == '(') {
            consume();           /* '(' */
            double arg = parseExpr();
            if (peek() == ')') consume();
            else                m_errHit = true;

            if (name == "sin")          return std::sin(arg);
            if (name == "cos")          return std::cos(arg);
            if (name == "tan")          return std::tan(arg);
            if (name == "asin")         return std::asin(arg);
            if (name == "acos")         return std::acos(arg);
            if (name == "atan")         return std::atan(arg);
            if (name == "sinh")         return std::sinh(arg);
            if (name == "cosh")         return std::cosh(arg);
            if (name == "tanh")         return std::tanh(arg);
            if (name == "sqrt")         return std::sqrt(arg);
            if (name == "abs")          return std::fabs(arg);
            if (name == "exp")          return std::exp(arg);
            if (name == "log" ||
                name == "log10")        return std::log10(arg);
            if (name == "ln")           return std::log(arg);
            if (name == "log2")         return std::log2(arg);
            if (name == "ceil")         return std::ceil(arg);
            if (name == "floor")        return std::floor(arg);
            if (name == "round")        return std::round(arg);
            /* funzione sconosciuta */
            m_errHit = true;
            return 0.0;
        }
        /* nome non seguito da '(' e non costante/variabile */
        m_errHit = true;
        return 0.0;
    }

    m_errHit = true;
    return 0.0;
}

double FormulaParser::parseNum() const {
    skipWs();
    const int start = m_pos;
    bool hasDot = false, hasExp = false;
    while (m_pos < m_expr.size()) {
        QChar ch = m_expr[m_pos];
        if (ch.isDigit())                            { ++m_pos; }
        else if (ch == '.' && !hasDot)               { hasDot = true; ++m_pos; }
        else if ((ch == 'e' || ch == 'E') && !hasExp && m_pos > start) {
            hasExp = true; ++m_pos;
            if (m_pos < m_expr.size() &&
                (m_expr[m_pos] == '+' || m_expr[m_pos] == '-')) ++m_pos;
        }
        else break;
    }
    if (m_pos == start) { m_errHit = true; return 0.0; }
    return m_expr.mid(start, m_pos - start).toDouble();
}

QString FormulaParser::parseName() const {
    skipWs();
    const int start = m_pos;
    while (m_pos < m_expr.size() &&
           (m_expr[m_pos].isLetter() || m_expr[m_pos].isDigit() || m_expr[m_pos] == '_'))
        ++m_pos;
    return m_expr.mid(start, m_pos - start);
}

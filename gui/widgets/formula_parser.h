#pragma once
#include <QString>
#include <QVector>
#include <QPointF>

/*
 * FormulaParser — parser ricorsivo discendente per espressioni con variabile x
 *
 * Grammatica:
 *   expr  = term  ( ('+' | '-') term  )*
 *   term  = pow   ( ('*' | '/') pow   )*
 *   pow   = unary ( '^'         unary )*
 *   unary = ('-' | '+') atom | atom
 *   atom  = number | 'x' | 'pi' | 'e' | name '(' expr ')' | '(' expr ')'
 *
 * Funzioni supportate: sin cos tan asin acos atan sqrt abs exp log ln log2 ceil floor
 * Costanti: pi, e
 *
 * Uso:
 *   FormulaParser fp("sin(x) * 2 + 1");
 *   if (fp.ok()) {
 *       auto pts = fp.sample(-3.14, 3.14, 400);
 *       double y = fp.eval(1.5);
 *   }
 */
class FormulaParser {
public:
    explicit FormulaParser(const QString& expr);

    bool    ok()  const { return m_ok; }
    QString err() const { return m_err; }

    /** Valuta l'espressione per x = xVal. Ritorna NaN se errore. */
    double eval(double xVal) const;

    /** Campiona n punti in [xMin, xMax], esclude NaN/Inf. */
    QVector<QPointF> sample(double xMin, double xMax, int n = 400) const;

    /**
     * tryExtract — cerca pattern "y = …" / "f(x) = …" nel testo.
     * Ritorna la parte destra dell'uguale, oppure "" se non trovato.
     */
    static QString tryExtract(const QString& text);

    /**
     * tryExtractPoints — cerca coppie di coordinate nel testo.
     * Riconosce pattern come:
     *   "x=3, y=5"  "x=3 e y=5"  "(3, 5)"  "punti: (1,2),(3,4)"
     * Ritorna lista di punti (vuota se nessun pattern trovato).
     */
    static QVector<QPointF> tryExtractPoints(const QString& text);

    /**
     * tryExtractXRange — cerca un intervallo x nel testo.
     * Riconosce pattern come:
     *   "da x=-5 a x=5"  "x ∈ [-10, 10]"  "x da -3 a 3"  "[-5, 5]"
     * Ritorna true e scrive xMin/xMax se trovato.
     */
    static bool tryExtractXRange(const QString& text, double& xMin, double& xMax);

private:
    /* ── stato di parsing (mutable: eval() è const) ── */
    mutable int    m_pos     = 0;
    mutable double m_xVal    = 0.0;
    mutable bool   m_errHit  = false;

    /* ── parser ── */
    double  parseExpr()  const;
    double  parseTerm()  const;
    double  parsePow()   const;
    double  parseUnary() const;
    double  parseAtom()  const;
    double  parseNum()   const;
    QString parseName()  const;
    QChar   peek()       const;
    QChar   consume()    const;
    void    skipWs()     const;

    QString m_expr;
    bool    m_ok  = false;
    QString m_err;
};

/* ══════════════════════════════════════════════════════════════
   agenti_page_tools.cpp — Tool di pre-elaborazione per LLM

   Contiene "guardie strumento" che intercettano il task PRIMA di
   inviarlo all'AI, generano dati reali e li iniettano nel prompt
   come contesto, così l'LLM lavora su numeri veri e non allucina.

   Tool presenti:
     _inject_random(task) — genera numeri casuali con std::random_device
                            (equivalente alla randomizzazione C nativa)
                            e li inietta nel task come [DATI RANDOM: ...].

   Pattern d'uso in runPipeline():
     m_taskOriginal = _inject_random(_inject_math(task));
   ══════════════════════════════════════════════════════════════ */
#include "agenti_page.h"
#include "agenti_page_p.h"
#include <QRegularExpression>
#include <random>
#include <cmath>

/* ══════════════════════════════════════════════════════════════
   _parseRandomParams — estrae parametri dal testo naturale.

   Riconosce:
   • Conteggio: "10 numeri", "50 valori", "genera 30 dati"
   • Range:     "tra 1 e 100", "da -10 a 10", "min=0 max=255"
   • Tipo:      "decimali" / "float" / "interi" (default: intero)
   • Modalità chart: se il task chiede un grafico, usa count=50 default
   ══════════════════════════════════════════════════════════════ */
struct RandomParams {
    int    count   = 20;      ///< quanti numeri generare
    double rMin    = 1.0;     ///< estremo inferiore del range
    double rMax    = 100.0;   ///< estremo superiore del range
    bool   isFloat = false;   ///< true → decimali con 2 cifre
};

static RandomParams _parseRandomParams(const QString& task) {
    RandomParams p;
    const QString t = task.toLower();

    /* ── Conteggio: cerca "N numeri/valori/dati/campioni/punti" ── */
    {
        static const QRegularExpression reCount(
            R"((\d{1,4})\s*(?:numeri?|valori?|dati|campioni?|elementi?|punti?))",
            QRegularExpression::CaseInsensitiveOption);
        auto m = reCount.match(t);
        if (m.hasMatch()) {
            int n = m.captured(1).toInt();
            if (n >= 1 && n <= 10000) p.count = n;
        } else {
            /* "genera N" / "crea N" / "fammi N" */
            static const QRegularExpression reGen(
                R"((?:genera|crea|fammi|dai mi|dammi|producimi)\s+(\d{1,4}))",
                QRegularExpression::CaseInsensitiveOption);
            auto m2 = reGen.match(t);
            if (m2.hasMatch()) {
                int n = m2.captured(1).toInt();
                if (n >= 1 && n <= 10000) p.count = n;
            }
        }
    }

    /* Se viene richiesto un grafico, usa default 50 punti per densità visiva */
    if (t.contains("grafico") || t.contains("chart") || t.contains("plot"))
        if (p.count == 20) p.count = 50;

    /* ── Range: "tra A e B" / "da A a B" / "in [A, B]" / "min=A max=B" ── */
    {
        static const QRegularExpression reRange(
            R"((?:tra|da|from|in\s*\[?)\s*(-?[\d]+(?:[.,]\d+)?)\s*(?:e|a|to|,)\s*(-?[\d]+(?:[.,]\d+)?))",
            QRegularExpression::CaseInsensitiveOption);
        auto m = reRange.match(t);
        if (m.hasMatch()) {
            double a = m.captured(1).replace(',', '.').toDouble();
            double b = m.captured(2).replace(',', '.').toDouble();
            if (a < b) { p.rMin = a; p.rMax = b; }
        }
        /* "min=A" e "max=B" espliciti */
        static const QRegularExpression reMin(R"(min\s*[=:]\s*(-?[\d]+(?:[.,]\d+)?))",
            QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression reMax(R"(max\s*[=:]\s*(-?[\d]+(?:[.,]\d+)?))",
            QRegularExpression::CaseInsensitiveOption);
        auto mMin = reMin.match(t);
        auto mMax = reMax.match(t);
        if (mMin.hasMatch()) p.rMin = mMin.captured(1).replace(',', '.').toDouble();
        if (mMax.hasMatch()) p.rMax = mMax.captured(1).replace(',', '.').toDouble();
    }

    /* ── Tipo: float se menziona decimali / float / reali / con virgola ── */
    if (t.contains("decimal") || t.contains("float") ||
        t.contains("reali")   || t.contains("virgola") ||
        t.contains("frazion"))
        p.isFloat = true;

    return p;
}

/* ══════════════════════════════════════════════════════════════
   _isRandomRequest — rileva intento "numeri casuali" nel task.
   Separato da _inject_random per permettere screening rapido.
   ══════════════════════════════════════════════════════════════ */
static bool _isRandomRequest(const QString& task) {
    const QString t = task.toLower();
    /* Una singola alternanza senza split su più raw-literal:
       i `"` all'interno di raw string interlineate vengono inclusi
       letteralmente nel pattern — meglio usare una stringa normale. */
    static const QRegularExpression re(
        "numeri?\\s+casua|valori?\\s+casua|dati\\s+casua"
        "|random|casuali|casuale|randomici|randomico"
        "|genera\\s+numeri|crea\\s+numeri|fammi\\s+numeri"
        "|sample|campion(?:a|i)|genera\\s+dati|valori\\s+random",
        QRegularExpression::CaseInsensitiveOption);
    return re.match(t).hasMatch();
}

/* ══════════════════════════════════════════════════════════════
   _inject_random — tool randomize per LLM.

   Se il task contiene intento "numeri casuali":
     1. Estrae parametri (count, range, tipo)
     2. Genera con std::random_device + std::mt19937 (= C rand() con
        seeding crittografico — equivalente alla randomizzazione C nativa
        ma senza race condition né stato globale)
     3. Prepende "[DATI RANDOM generati localmente — N interi/float
        in [min, max]: x1, x2, ..., xN]" al task
     4. Restituisce il task modificato (con contesto iniettato)

   Se il task NON contiene intento random → restituisce il task invariato.
   ══════════════════════════════════════════════════════════════ */
QString _inject_random(const QString& task) {
    if (!_isRandomRequest(task)) return task;

    const RandomParams p = _parseRandomParams(task);

    /* Generatore: random_device per seed, mt19937 per sequenza */
    std::random_device rd;
    std::mt19937 gen(rd());

    QString numStr;
    if (p.isFloat) {
        std::uniform_real_distribution<double> dist(p.rMin, p.rMax);
        for (int i = 0; i < p.count; ++i) {
            if (i) numStr += ", ";
            numStr += QString::number(dist(gen), 'f', 2);
        }
    } else {
        const long long lo = static_cast<long long>(std::ceil(p.rMin));
        const long long hi = static_cast<long long>(std::floor(p.rMax));
        std::uniform_int_distribution<long long> dist(lo, hi);
        for (int i = 0; i < p.count; ++i) {
            if (i) numStr += ", ";
            numStr += QString::number(dist(gen));
        }
    }

    /* Statistiche rapide da iniettare come contesto aggiuntivo */
    const QString tipo = p.isFloat ? "float" : "interi";
    const QString header = QString(
        "[DATI RANDOM generati localmente con std::random_device+mt19937 "
        "(%1 %2 in [%3, %4]):\n%5]\n\n")
        .arg(p.count)
        .arg(tipo)
        .arg(p.isFloat ? QString::number(p.rMin, 'f', 2) : QString::number((long long)p.rMin))
        .arg(p.isFloat ? QString::number(p.rMax, 'f', 2) : QString::number((long long)p.rMax))
        .arg(numStr);

    return header + task;
}

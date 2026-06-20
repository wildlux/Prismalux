/* ══════════════════════════════════════════════════════════════
   ai_random_inject.cpp — Tool _inject_random puro, senza dipendenze UI.

   Compilato sia in Prismalux_GUI (incluso tramite main_ai_tools.cpp)
   sia in test_random_tool (link diretto senza AiClient/GraphMemory).
   ══════════════════════════════════════════════════════════════ */
#include <QRegularExpression>
#include <QString>
#include <cmath>
#include <random>

struct RandomParams {
    int    count   = 20;
    double rMin    = 1.0;
    double rMax    = 100.0;
    bool   isFloat = false;
};

static RandomParams _parseRandomParams(const QString& task) {
    RandomParams p;
    const QString t = task.toLower();

    {
        static const QRegularExpression reCount(
            R"((\d{1,4})\s*(?:numeri?|valori?|dati|campioni?|elementi?|punti?))",
            QRegularExpression::CaseInsensitiveOption);
        auto m = reCount.match(t);
        if (m.hasMatch()) {
            int n = m.captured(1).toInt();
            if (n >= 1 && n <= 10000) p.count = n;
        } else {
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

    if (t.contains("grafico") || t.contains("chart") || t.contains("plot"))
        if (p.count == 20) p.count = 50;

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
        static const QRegularExpression reMin(R"(min\s*[=:]\s*(-?[\d]+(?:[.,]\d+)?))",
            QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression reMax(R"(max\s*[=:]\s*(-?[\d]+(?:[.,]\d+)?))",
            QRegularExpression::CaseInsensitiveOption);
        auto mMin = reMin.match(t);
        auto mMax = reMax.match(t);
        if (mMin.hasMatch()) p.rMin = mMin.captured(1).replace(',', '.').toDouble();
        if (mMax.hasMatch()) p.rMax = mMax.captured(1).replace(',', '.').toDouble();
    }

    if (t.contains("decimal") || t.contains("float") ||
        t.contains("reali")   || t.contains("virgola") ||
        t.contains("frazion"))
        p.isFloat = true;

    return p;
}

static bool _isRandomRequest(const QString& task) {
    const QString t = task.toLower();
    static const QRegularExpression re(
        "numeri?\\s+casua|valori?\\s+casua|dati\\s+casua"
        "|random|casuali|casuale|randomici|randomico"
        "|genera\\s+numeri|crea\\s+numeri|fammi\\s+numeri"
        "|sample|campion(?:a|i)|genera\\s+dati|valori\\s+random",
        QRegularExpression::CaseInsensitiveOption);
    return re.match(t).hasMatch();
}

QString _inject_random(const QString& task) {
    if (!_isRandomRequest(task)) return task;

    const RandomParams p = _parseRandomParams(task);

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

#include "device_split.h"
#include "prismalux_paths.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSettings>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace P = PrismaluxPaths;

namespace DeviceSplit {

namespace {

/* Ripartisce `total` layer tra i device in proporzione ai budget.
   Metodo del resto maggiore: la somma è esattamente `total`. */
QVector<int> allocateLayers(int total, const QVector<qint64>& weights)
{
    const int n = weights.size();
    QVector<int> out(n, 0);
    if (n <= 0 || total <= 0)
        return out;

    qint64 sumWeights = 0;
    for (qint64 w : weights) sumWeights += qMax<qint64>(0, w);
    if (sumWeights <= 0)
        return out;

    QVector<double> remainders(n, 0.0);
    for (int i = 0; i < n; ++i) {
        const double exact = double(total) * double(qMax<qint64>(0, weights.at(i)))
                             / double(sumWeights);
        out[i] = int(std::floor(exact));
        remainders[i] = exact - double(out[i]);
    }

    /* Se total >= n, ogni device deve avere almeno un layer: si preleva
       dai device che ne hanno di più. */
    if (total >= n) {
        for (int i = 0; i < n; ++i) {
            if (out.at(i) > 0)
                continue;
            int donor = 0;
            for (int j = 1; j < n; ++j)
                if (out.at(j) > out.at(donor)) donor = j;
            if (out.at(donor) > 1) {
                --out[donor];
                out[i] = 1;
            }
        }
    }

    int assigned = 0;
    for (int v : out) assigned += v;
    int left = total - assigned;
    while (left > 0) {
        int best = 0;
        for (int i = 1; i < n; ++i)
            if (remainders.at(i) > remainders.at(best)) best = i;
        ++out[best];
        remainders[best] -= 1.0;
        --left;
    }
    return out;
}

} // namespace

Plan compute(const QVector<Device>& devices, int maxLayers)
{
    Plan plan;
    if (maxLayers <= 0) maxLayers = kMaxLayers;

    struct Used {
        int    index = 0;
        qint64 budgetMb = 0;
        int    rawLayers = 0;   ///< budget / 80, senza cap
        int    layers = 0;      ///< layer effettivi dopo il cap
    };
    QVector<Used> used;
    QSet<int> deviceTypes;
    QStringList skipped;

    for (int i = 0; i < devices.size(); ++i) {
        const Device& d = devices.at(i);
        if (!d.enabled || d.isCpu)
            continue;

        const qint64 avail  = qMax<qint64>(0, d.availMb);
        const qint64 budget = qBound<qint64>(0, d.budgetMb, avail);
        if (budget < kMinBudgetMb) {
            skipped << (d.name.isEmpty() ? QStringLiteral("device %1").arg(i) : d.name);
            continue;
        }

        Used u;
        u.index = i;
        u.budgetMb = budget;
        u.rawLayers = qMax(1, int(budget / kMbPerLayer));
        used.append(u);
        deviceTypes.insert(d.type);
    }

    if (used.isEmpty()) {
        plan.enabled = false;
        plan.warning = skipped.isEmpty()
            ? QStringLiteral("Nessuna GPU selezionata.")
            : QStringLiteral("Budget insufficiente (minimo %1 MB) per: %2")
                  .arg(kMinBudgetMb).arg(skipped.join(QStringLiteral(", ")));
        return plan;
    }

    plan.enabled = true;
    qint64 totalBudget = 0;
    int totalRawLayers = 0;
    QStringList budgetParts;

    for (const Used& u : used) {
        plan.usedDeviceIndices.append(u.index);
        totalBudget += u.budgetMb;
        totalRawLayers += u.rawLayers;
        budgetParts << QString::number(u.budgetMb);
    }

    plan.nGpuLayers = qMin(totalRawLayers, maxLayers);

    /* Layer effettivi: senza cap sono quelli grezzi; con il cap si
       ridistribuiscono in proporzione ai budget, così la somma mostrata
       coincide con -ngl (e con --tensor-split). */
    QVector<int> finalLayers;
    if (used.size() == 1) {
        finalLayers.append(plan.nGpuLayers);
    } else if (totalRawLayers <= maxLayers) {
        for (const Used& u : used)
            finalLayers.append(u.rawLayers);
    } else {
        QVector<qint64> weights;
        for (const Used& u : used)
            weights.append(u.budgetMb);
        finalLayers = allocateLayers(plan.nGpuLayers, weights);
    }

    QStringList layerParts;
    for (int i = 0; i < used.size(); ++i) {
        used[i].layers = finalLayers.value(i, 0);
        layerParts << QString::number(used.at(i).layers);
    }

    /* --tensor-split ha senso solo con almeno due GPU. Con una sola,
       -ngl basta e i layer restanti vanno in CPU. Le proporzioni sono
       sui budget (non sui layer) e l'ultima è calcolata per far
       sommare esattamente 1.0000. */
    if (used.size() >= 2 && totalBudget > 0) {
        QVector<double> fractions(used.size(), 0.0);
        double accumulated = 0.0;
        for (int i = 0; i < used.size(); ++i) {
            double f = double(used.at(i).budgetMb) / double(totalBudget);
            if (i == used.size() - 1) {
                f = qMax(0.0, 1.0 - accumulated);
            } else {
                f = std::round(f * 10000.0) / 10000.0;
                accumulated += f;
            }
            fractions[i] = f;
        }
        QStringList parts;
        for (double f : fractions)
            parts << QString::number(f, 'f', 4);
        plan.tensorSplit = parts.join(QLatin1Char(','));
    }

    if (used.size() == 1) {
        plan.summary = QStringLiteral(
            "GPU singola: %1 layer (%2 MB) \u2192 -ngl %3; resto in CPU")
            .arg(used.first().layers)
            .arg(used.first().budgetMb)
            .arg(plan.nGpuLayers);
    } else {
        plan.summary = QStringLiteral(
            "%1 GPU: %2 layer (%3 MB) \u2192 -ngl %4, tensor %5")
            .arg(used.size())
            .arg(layerParts.join(QLatin1Char('+')))
            .arg(budgetParts.join(QLatin1Char('+')))
            .arg(plan.nGpuLayers)
            .arg(plan.tensorSplit);
    }

    if (totalRawLayers > maxLayers)
        plan.warning += QStringLiteral("Layer richiesti %1, limitati a %2 (cap llama.cpp). ")
                            .arg(totalRawLayers).arg(maxLayers);
    if (!skipped.isEmpty())
        plan.warning += QStringLiteral("Esclusi per budget < %1 MB: %2. ")
                            .arg(kMinBudgetMb)
                            .arg(skipped.join(QStringLiteral(", ")));
    if (deviceTypes.size() > 1)
        plan.warning += QStringLiteral(
            "Dispositivi eterogenei: serve una build llama.cpp con pi\u00f9 backend; "
            "verifica l'ordine con --list-devices.");
    plan.warning = plan.warning.trimmed();
    return plan;
}

void save(const Plan& plan, const QVector<Device>& devices)
{
    QSettings s;
    s.setValue(P::SK::kLlamaSplitEnabled, plan.enabled);
    s.setValue(P::SK::kLlamaSplitNgl,     plan.nGpuLayers);
    s.setValue(P::SK::kLlamaSplitTensor,  plan.tensorSplit);
    s.setValue(P::SK::kLlamaSplitSummary, plan.summary);
    s.setValue(P::SK::kLlamaSplitWarning, plan.warning);

    QJsonObject budgets;
    for (const Device& d : devices) {
        if (d.isCpu || d.budgetMb <= 0)
            continue;
        budgets.insert(d.name, double(d.budgetMb));
    }
    s.setValue(P::SK::kLlamaSplitBudgets,
               QString::fromUtf8(QJsonDocument(budgets)
                                     .toJson(QJsonDocument::Compact)));
    s.sync();
}

Plan load()
{
    QSettings s;
    Plan plan;
    plan.enabled    = s.value(P::SK::kLlamaSplitEnabled, false).toBool();
    plan.nGpuLayers = s.value(P::SK::kLlamaSplitNgl, 0).toInt();
    plan.tensorSplit = s.value(P::SK::kLlamaSplitTensor).toString();
    plan.summary    = s.value(P::SK::kLlamaSplitSummary).toString();
    plan.warning    = s.value(P::SK::kLlamaSplitWarning).toString();
    return plan;
}

qint64 savedBudget(const QString& deviceName, qint64 fallbackMb)
{
    QSettings s;
    const QString raw = s.value(P::SK::kLlamaSplitBudgets).toString();
    if (raw.isEmpty())
        return fallbackMb;
    const QJsonObject obj = QJsonDocument::fromJson(raw.toUtf8()).object();
    if (!obj.contains(deviceName))
        return fallbackMb;
    return qint64(obj.value(deviceName).toDouble(fallbackMb));
}

void clear()
{
    QSettings s;
    s.remove(P::SK::kLlamaSplitEnabled);
    s.remove(P::SK::kLlamaSplitNgl);
    s.remove(P::SK::kLlamaSplitTensor);
    s.remove(P::SK::kLlamaSplitSummary);
    s.remove(P::SK::kLlamaSplitWarning);
    s.remove(P::SK::kLlamaSplitBudgets);
    s.sync();
}

} // namespace DeviceSplit

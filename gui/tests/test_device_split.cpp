/* ══════════════════════════════════════════════════════════════
   test_device_split.cpp — D-54, logica pura (nessun hardware)

   Copre:
     CAT-A  piano disabilitato / nessuna GPU
     CAT-B  GPU singola (solo -ngl, resto in CPU)
     CAT-C  due e tre GPU (--tensor-split, somma 1.0)
     CAT-D  cap 99 layer, budget oltre la VRAM, budget insufficiente
     CAT-E  dispositivi eterogenei (warning, non blocco)
     CAT-F  persistenza QSettings + savedBudget + clear
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QStringList>
#include <cmath>

#include "../device_split.h"

using DeviceSplit::Device;
using DeviceSplit::Plan;

static Device gpu(const QString& name, int type,
                  qint64 availMb, qint64 budgetMb, bool enabled = true)
{
    Device d;
    d.name = name;
    d.type = type;
    d.isCpu = false;
    d.availMb = availMb;
    d.budgetMb = budgetMb;
    d.enabled = enabled;
    return d;
}

static Device cpu(qint64 availMb)
{
    Device d;
    d.name = QStringLiteral("CPU");
    d.type = 1;
    d.isCpu = true;
    d.availMb = availMb;
    d.budgetMb = 0;
    d.enabled = false;
    return d;
}

class TestDeviceSplit : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("Prismalux"));
        QCoreApplication::setApplicationName(QStringLiteral("GUI"));
        DeviceSplit::clear();
    }

    void cleanup()
    {
        DeviceSplit::clear();
    }

    /* ── CAT-A ────────────────────────────────────────────────── */
    void nessuna_gpu()
    {
        const Plan p = DeviceSplit::compute({ gpu("NVIDIA0", 2, 8192, 6553, false) });
        QVERIFY(!p.enabled);
        QCOMPARE(p.nGpuLayers, 0);
        QVERIFY(p.tensorSplit.isEmpty());
        QVERIFY(!p.warning.isEmpty());
    }

    void solo_cpu()
    {
        const Plan p = DeviceSplit::compute({ cpu(65536) });
        QVERIFY(!p.enabled);
        QCOMPARE(p.nGpuLayers, 0);
        QVERIFY(p.usedDeviceIndices.isEmpty());
    }

    /* ── CAT-B ────────────────────────────────────────────────── */
    void gpu_singola()
    {
        const Plan p = DeviceSplit::compute({ gpu("NVIDIA0", 2, 8192, 6553) });
        QVERIFY(p.enabled);
        QCOMPARE(p.nGpuLayers, 81);           /* 6553 / 80 */
        QVERIFY(p.tensorSplit.isEmpty());      /* una GPU: niente split */
        QCOMPARE(p.usedDeviceIndices, QVector<int>{0});
        QVERIFY(p.summary.contains("-ngl 81"));
        QVERIFY(p.summary.contains("CPU"));
    }

    void cpu_piu_gpu_esclude_cpu()
    {
        const Plan p = DeviceSplit::compute({
            cpu(65536),
            gpu("NVIDIA0", 2, 8192, 6553),
        });
        QVERIFY(p.enabled);
        QCOMPARE(p.nGpuLayers, 81);
        QCOMPARE(p.usedDeviceIndices, QVector<int>{1});  /* solo la GPU */
    }

    /* ── CAT-C ────────────────────────────────────────────────── */
    void due_gpu_tensor_split()
    {
        const Plan p = DeviceSplit::compute({
            gpu("NVIDIA0", 2, 8192, 6553),
            gpu("NVIDIA1", 2, 4096, 3276),
        });
        QVERIFY(p.enabled);
        QCOMPARE(p.nGpuLayers, 99);           /* 81 + 40, cap 99 */
        QCOMPARE(p.tensorSplit, QStringLiteral("0.6667,0.3333"));
        QCOMPARE(p.usedDeviceIndices, (QVector<int>{0, 1}));
        /* Con il cap i layer mostrati sono ridistribuiti: 66+33 = 99. */
        QVERIFY2(p.summary.contains("66+33"), qPrintable(p.summary));

        const QStringList parts = p.tensorSplit.split(QLatin1Char(','));
        QCOMPARE(parts.size(), 2);
        double sum = 0.0;
        for (const QString& s : parts) sum += s.toDouble();
        QVERIFY(std::abs(sum - 1.0) < 1e-9);
        QVERIFY(parts.at(0).toDouble() > parts.at(1).toDouble());
    }

    void tre_gpu_somma_a_uno()
    {
        const Plan p = DeviceSplit::compute({
            gpu("A", 2, 5000, 5000),
            gpu("B", 2, 3500, 3500),
            gpu("C", 2, 2500, 2500),   /* ≥ 2048: non deve essere esclusa */
        });
        QVERIFY(p.enabled);
        QCOMPARE(p.nGpuLayers, 99);           /* 62 + 43 + 31 = 136 → cap */
        const QStringList parts = p.tensorSplit.split(QLatin1Char(','));
        QCOMPARE(parts.size(), 3);
        double sum = 0.0;
        for (const QString& s : parts) {
            QVERIFY(s.toDouble() > 0.0);
            sum += s.toDouble();
        }
        QVERIFY(std::abs(sum - 1.0) < 1e-9);
    }

    /* ── CAT-D ────────────────────────────────────────────────── */
    void cap_layer()
    {
        const Plan p = DeviceSplit::compute({
            gpu("A", 2, 100000, 100000),
            gpu("B", 2, 100000, 100000),
        });
        QVERIFY(p.enabled);
        QCOMPARE(p.nGpuLayers, DeviceSplit::kMaxLayers);
        QVERIFY2(p.warning.contains("limitati a 99"), qPrintable(p.warning));
        QCOMPARE(p.tensorSplit, QStringLiteral("0.5000,0.5000"));
    }

    void budget_oltre_vram_clampato()
    {
        const Plan p = DeviceSplit::compute({ gpu("A", 2, 4096, 99999) });
        QVERIFY(p.enabled);
        QCOMPARE(p.nGpuLayers, 51);           /* 4096 / 80 */
        QVERIFY2(p.summary.contains("4096 MB"), qPrintable(p.summary));
    }

    void budget_insufficiente()
    {
        const Plan p = DeviceSplit::compute({ gpu("Piccola", 3, 1024, 1024) });
        QVERIFY(!p.enabled);
        QVERIFY2(p.warning.contains("2048"), qPrintable(p.warning));
        QVERIFY2(p.warning.contains("Piccola"), qPrintable(p.warning));
    }

    void max_layers_custom()
    {
        const Plan p = DeviceSplit::compute({ gpu("A", 2, 8192, 6553) }, 20);
        QVERIFY(p.enabled);
        QCOMPARE(p.nGpuLayers, 20);
        QVERIFY2(p.summary.contains("20 layer"), qPrintable(p.summary));
    }

    /* ── CAT-E ────────────────────────────────────────────────── */
    void dispositivi_eterogenei()
    {
        const Plan p = DeviceSplit::compute({
            gpu("NVIDIA0", 2, 8192, 6553),
            gpu("Intel iGPU", 4, 8192, 6553),
        });
        QVERIFY(p.enabled);                   /* warning, non blocco */
        QVERIFY2(p.warning.contains("eterogenei"), qPrintable(p.warning));
        QVERIFY(p.warning.contains("--list-devices"));
    }

    /* ── CAT-F ────────────────────────────────────────────────── */
    void persistenza_round_trip()
    {
        const QVector<Device> devices = {
            gpu("NVIDIA0", 2, 8192, 6553),
            gpu("NVIDIA1", 2, 4096, 3276),
        };
        const Plan plan = DeviceSplit::compute(devices);
        QVERIFY(plan.enabled);

        DeviceSplit::save(plan, devices);

        const Plan loaded = DeviceSplit::load();
        QVERIFY(loaded.enabled);
        QCOMPARE(loaded.nGpuLayers, plan.nGpuLayers);
        QCOMPARE(loaded.tensorSplit, plan.tensorSplit);
        QCOMPARE(loaded.summary, plan.summary);

        QCOMPARE(DeviceSplit::savedBudget("NVIDIA0", 0), qint64(6553));
        QCOMPARE(DeviceSplit::savedBudget("NVIDIA1", 0), qint64(3276));
        QCOMPARE(DeviceSplit::savedBudget("Assente", 777), qint64(777));

        DeviceSplit::clear();
        const Plan cleared = DeviceSplit::load();
        QVERIFY(!cleared.enabled);
        QCOMPARE(cleared.nGpuLayers, 0);
        QVERIFY(cleared.tensorSplit.isEmpty());
    }

    void salva_piano_disabilitato()
    {
        const QVector<Device> devices = {
            gpu("NVIDIA0", 2, 8192, 6553, false),
        };
        const Plan plan;   /* enabled=false */
        DeviceSplit::save(plan, devices);

        const Plan loaded = DeviceSplit::load();
        QVERIFY(!loaded.enabled);
        QCOMPARE(loaded.nGpuLayers, 0);
        QVERIFY(loaded.tensorSplit.isEmpty());
        /* Il budget resta memorizzato anche a piano spento. */
        QCOMPARE(DeviceSplit::savedBudget("NVIDIA0", 0), qint64(6553));
    }
};

int main(int argc, char** argv)
{
    const QString home = QDir::tempPath()
        + QStringLiteral("/prismalux_split_test_%1")
              .arg(QCoreApplication::applicationPid());
    QDir(home).removeRecursively();
    QDir().mkpath(home + QStringLiteral("/.prismalux"));
    QDir().mkpath(home + QStringLiteral("/.config"));
    qputenv("HOME", home.toLocal8Bit());
    qputenv("XDG_CONFIG_HOME", (home + QStringLiteral("/.config")).toLocal8Bit());

    QApplication app(argc, argv);
    TestDeviceSplit t;
    const int status = QTest::qExec(&t, argc, argv);

    QDir(home).removeRecursively();
    return status;
}

#include "test_device_split.moc"

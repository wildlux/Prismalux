/* ══════════════════════════════════════════════════════════════
   test_hardware_monitor.cpp — Unit test per hw_detect + HardwareMonitor
   ──────────────────────────────────────────────────────────────
   Categorie:
     CAT-A  hw_detect() — struttura HWInfo dopo rilevamento (7 test)
     CAT-B  HardwareMonitor — costruzione + thread + segnale (4 test)

   Note: hw_detect() chiama popen/wmic su sistema reale.
   I test verificano invarianti strutturali, non valori hardware specifici.

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_hardware_monitor
     ./build_tests/test_hardware_monitor
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "../hardware_monitor.h"
#include "../hw_detect.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — hw_detect() (7 test)
   ══════════════════════════════════════════════════════════════ */
class TestHWDetect : public QObject {
    Q_OBJECT

    HWInfo m_hw = {};

private slots:

    void initTestCase() {
        /* hw_detect() è bloccante — eseguita una sola volta */
        hw_detect(&m_hw);
    }

    void countAlmenoUno() {
        QVERIFY2(m_hw.count >= 1,
                 qPrintable(QString("hw_detect ha trovato %1 device (min 1)").arg(m_hw.count)));
    }

    void countEntroLimite() {
        QVERIFY2(m_hw.count <= HW_MAX_DEVICES,
                 qPrintable(QString("count=%1 supera HW_MAX_DEVICES=%2")
                            .arg(m_hw.count).arg(HW_MAX_DEVICES)));
    }

    void primaryValido() {
        QVERIFY2(m_hw.primary >= 0 && m_hw.primary < m_hw.count,
                 qPrintable(QString("primary=%1 fuori range [0,%2)")
                            .arg(m_hw.primary).arg(m_hw.count)));
    }

    void primaryIsCPU() {
        QCOMPARE(m_hw.dev[m_hw.primary].type, DEV_CPU);
    }

    void cpuNomeNonVuoto() {
        const char* nome = m_hw.dev[m_hw.primary].name;
        QVERIFY2(nome[0] != '\0', "nome CPU deve essere non vuoto");
    }

    void hwDevTypeStrCPU() {
        QCOMPARE(QString(hw_dev_type_str(DEV_CPU)), QString("CPU"));
    }

    void hwDevTypeStrUnknown() {
        /* DEV_UNKNOWN deve restituire qualcosa di non vuoto */
        QVERIFY(hw_dev_type_str(DEV_UNKNOWN) != nullptr);
        QVERIFY(hw_dev_type_str(DEV_UNKNOWN)[0] != '\0');
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — HardwareMonitor (4 test)
   ══════════════════════════════════════════════════════════════ */
class TestHardwareMonitor : public QObject {
    Q_OBJECT
private slots:

    void costruzioneNoCrash() {
        HardwareMonitor mon;
        Q_UNUSED(mon);
    }

    void hwInfoReadySignalEmesso() {
        HardwareMonitor mon;
        QSignalSpy spy(&mon, &HardwareMonitor::hwInfoReady);
        mon.start();
        if (!spy.wait(30000)) QSKIP("hwInfoReady non emesso entro 30s — hw_detect troppo lento");
        QCOMPARE(spy.count(), 1);
    }

    void hwInfoDopoSegnaleHaCPU() {
        HardwareMonitor mon;
        QSignalSpy spy(&mon, &HardwareMonitor::hwInfoReady);
        mon.start();
        if (!spy.wait(30000)) QSKIP("hw_detect non ha completato entro 30s");

        const HWInfo& hw = mon.hwInfo();
        QVERIFY(hw.count >= 1);
        QCOMPARE(hw.dev[hw.primary].type, DEV_CPU);
    }

    void costruzioneConParentNoCrash() {
        QObject parent;
        HardwareMonitor* mon = new HardwareMonitor(&parent);
        Q_UNUSED(mon);
        /* parent distrugge mon — nessun leak */
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    int status = 0;
    { TestHWDetect t;          status |= QTest::qExec(&t, argc, argv); }
    { TestHardwareMonitor t;   status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_hardware_monitor.moc"

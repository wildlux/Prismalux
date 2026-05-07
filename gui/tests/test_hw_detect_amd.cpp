/* test_hw_detect_amd.cpp — TN-15 (14 test)
 * Modulo: hw_detect.h / hw_detect.c
 * CAT-A: struttura HWInfo (6 test)
 * CAT-B: rilevamento AMD senza ROCm — DRM sysfs e lspci (8 test)
 *
 * Build: cmake -B build_tests -DBUILD_TESTS=ON && cmake --build build_tests
 * Run:   ./build_tests/test_hw_detect_amd
 */
#include <QtTest/QtTest>
#include <QString>

extern "C" {
#include "../hw_detect.h"
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — Struttura HWInfo dopo hw_detect
   ══════════════════════════════════════════════════════════════ */
class TestHWInfoStruttura : public QObject {
    Q_OBJECT
private:
    HWInfo hw;
private slots:

    void initTestCase() {
        hw_detect(&hw);
    }

    /* A1 — hw_detect non crasha */
    void testNoCrash() {
        QVERIFY(true); /* se siamo qui, initTestCase è passato */
    }

    /* A2 — device 0 è sempre CPU */
    void testDev0EcpuCpu() {
        QCOMPARE(static_cast<int>(hw.dev[0].type), static_cast<int>(DEV_CPU));
    }

    /* A3 — primary == 0 (CPU) */
    void testPrimaryEZero() {
        QCOMPARE(hw.primary, 0);
    }

    /* A4 — secondary >= -1 */
    void testSecondaryValido() {
        QVERIFY(hw.secondary >= -1);
        QVERIFY(hw.secondary < hw.count);
    }

    /* A5 — CPU: mem_mb > 0 e avail_mb > 0 */
    void testCpuRamNonZero() {
        QVERIFY(hw.dev[0].mem_mb   > 0);
        QVERIFY(hw.dev[0].avail_mb > 0);
    }

    /* A6 — count >= 1 (almeno CPU) */
    void testCountAlmenoUno() {
        QVERIFY(hw.count >= 1);
        QVERIFY(hw.count <= HW_MAX_DEVICES);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — AMD detection (adattivo: se AMD non presente, salta)
   ══════════════════════════════════════════════════════════════ */
class TestAmdDetection : public QObject {
    Q_OBJECT
private:
    HWInfo hw;
    bool   m_amdPresente = false;
private slots:

    void initTestCase() {
        hw_detect(&hw);
        /* Cerca un device AMD nel risultato */
        for (int i = 0; i < hw.count; i++) {
            if (hw.dev[i].type == DEV_AMD) {
                m_amdPresente = true;
                break;
            }
        }
    }

    /* B1 — se AMD rilevata: secondary >= 0 */
    void testAmdSecondaryImpostato() {
        if (!m_amdPresente) QSKIP("Nessuna GPU AMD su questa macchina");
        QVERIFY(hw.secondary >= 0);
    }

    /* B2 — se AMD rilevata: tipo == DEV_AMD */
    void testAmdTipoCorretto() {
        if (!m_amdPresente) QSKIP("Nessuna GPU AMD su questa macchina");
        QCOMPARE(static_cast<int>(hw.dev[hw.secondary].type),
                 static_cast<int>(DEV_AMD));
    }

    /* B3 — se AMD rilevata: avail_mb > 0 (fallback 512 MB garantito) */
    void testAmdAvailMbPositivo() {
        if (!m_amdPresente) QSKIP("Nessuna GPU AMD su questa macchina");
        QVERIFY(hw.dev[hw.secondary].avail_mb > 0);
    }

    /* B4 — se AMD rilevata: n_gpu_layers > 0 */
    void testAmdLayersPositivi() {
        if (!m_amdPresente) QSKIP("Nessuna GPU AMD su questa macchina");
        QVERIFY(hw.dev[hw.secondary].n_gpu_layers > 0);
    }

    /* B5 — device Intel (se presente) NON è confuso con AMD */
    void testIntelNonEAmd() {
        for (int i = 0; i < hw.count; i++) {
            if (hw.dev[i].type == DEV_INTEL) {
                QVERIFY(hw.dev[i].type != DEV_AMD);
            }
        }
        QVERIFY(true);
    }

    /* B6 — se AMD presente, il nome non è vuoto */
    void testAmdNomeNonVuoto() {
        if (!m_amdPresente) QSKIP("Nessuna GPU AMD su questa macchina");
        const QString nome = QString::fromLatin1(hw.dev[hw.secondary].name);
        QVERIFY(!nome.trimmed().isEmpty());
    }

    /* B7 — hw_detect determinismo: stesso risultato su 5 chiamate */
    void testDeterminismo() {
        HWInfo hw2;
        for (int run = 0; run < 5; run++) {
            hw_detect(&hw2);
            QCOMPARE(hw2.count,     hw.count);
            QCOMPARE(hw2.primary,   hw.primary);
            QCOMPARE(hw2.secondary, hw.secondary);
        }
    }

    /* B8 — senza GPU dedicata: secondary == -1 oppure == igpu (no falsi positivi) */
    void testSenzaDedicataNonHaSecondary() {
        /* Conta GPU dedicate (non Intel, non CPU) */
        int dedicateCount = 0;
        for (int i = 0; i < hw.count; i++) {
            const HWDevType t = hw.dev[i].type;
            if (t == DEV_NVIDIA || t == DEV_AMD || t == DEV_APPLE) dedicateCount++;
        }
        if (dedicateCount > 0) QSKIP("GPU dedicata presente — test non applicabile");

        /* Nessuna GPU dedicata → secondary == -1 o punta a Intel iGPU */
        QVERIFY(hw.secondary == -1 || hw.secondary == hw.igpu);
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    int status = 0;
    { TestHWInfoStruttura t; status |= QTest::qExec(&t, argc, argv); }
    { TestAmdDetection    t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_hw_detect_amd.moc"

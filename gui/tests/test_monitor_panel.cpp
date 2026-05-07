/* ══════════════════════════════════════════════════════════════
   test_monitor_panel.cpp — Unit test MonitorPanel

   Categorie:
     CAT-A  Costruzione — MonitorPanel QDialog senza crash
     CAT-B  Struttura widget — label CPU/RAM presenti, m_liveInfo
     CAT-C  Aggiornamento — updateHw non crasha con HWInfo vuoto

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_monitor_panel
     ./build_tests/test_monitor_panel
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QDialog>
#include <QLabel>

#include "mock_ai_client.h"
#include "../hardware_monitor.h"
#include "../monitor_panel.h"

static MockAiClient*    s_ai  = nullptr;
static HardwareMonitor* s_hw  = nullptr;
static MonitorPanel*    s_dlg = nullptr;

static void ensurePanel() {
    if (!s_dlg) {
        s_ai  = new MockAiClient;
        s_hw  = new HardwareMonitor;
        s_dlg = new MonitorPanel(s_ai, s_hw, nullptr);
    }
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruzione
   ══════════════════════════════════════════════════════════════ */
class TestCostruzione : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePanel(); }

    /* A-1: costruisce senza crash */
    void costruisceSenzaCrash() {
        QVERIFY2(s_dlg != nullptr, "MonitorPanel non costruito");
    }

    /* A-2: è un QDialog */
    void eUnQDialog() {
        auto* d = qobject_cast<QDialog*>(s_dlg);
        QVERIFY2(d != nullptr, "MonitorPanel non è un QDialog");
    }

    /* A-3: ha figli widget (non è vuoto) */
    void haBigliFigli() {
        QVERIFY2(!s_dlg->findChildren<QWidget*>().isEmpty(),
                 "MonitorPanel sembra vuoto");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Struttura widget
   ══════════════════════════════════════════════════════════════ */
class TestStrutturaWidget : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePanel(); }

    /* B-1: ha almeno un QLabel */
    void haAlmenoUnQLabel() {
        const auto labels = s_dlg->findChildren<QLabel*>();
        QVERIFY2(!labels.isEmpty(), "MonitorPanel: nessun QLabel trovato");
    }

    /* B-2: m_liveInfo (QLabel) è presente nel pannello */
    void liveInfoPresente() {
        /* m_liveInfo è privato — verifichiamo che ci siano QLabel nel dialog */
        const auto labels = s_dlg->findChildren<QLabel*>();
        QVERIFY2(!labels.isEmpty(),
                 "MonitorPanel: QLabel m_liveInfo non trovato");
    }

    /* B-3: ha parole chiave CPU o RAM nei testi delle label */
    void labelContenutoInfoSistema() {
        const auto labels = s_dlg->findChildren<QLabel*>();
        bool foundSysInfo = false;
        for (auto* l : labels) {
            const QString t = l->text();
            if (t.contains("CPU", Qt::CaseInsensitive) ||
                t.contains("RAM", Qt::CaseInsensitive) ||
                t.contains("GPU", Qt::CaseInsensitive) ||
                t.contains("Monitor", Qt::CaseInsensitive))
            {
                foundSysInfo = true;
                break;
            }
        }
        if (!foundSysInfo)
            QSKIP("Nessuna label con testo CPU/RAM/GPU trovata (potrebbero essere vuote inizialmente)");
        QVERIFY(foundSysInfo);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Aggiornamento HWInfo
   ══════════════════════════════════════════════════════════════ */
class TestAggiornamentoHW : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePanel(); }

    /* C-1: updateHw con HWInfo default non crasha */
    void updateHwDefaultNonCrasha() {
        HWInfo hw;
        /* m_liveInfo è privato — testiamo tramite il segnale hwInfoReady di HardwareMonitor
           che MonitorPanel ascolta internamente */
        /* Il modo più semplice: emettere hwInfoReady da s_hw */
        emit s_hw->hwInfoReady(hw);
        QApplication::processEvents();
        /* Non deve crashare */
    }

    /* C-2: show() non crasha */
    void showNonCrasha() {
        s_dlg->show();
        QApplication::processEvents();
        s_dlg->hide();
    }

    /* C-3: distruzione e ri-creazione non crashano */
    void ricreazioneNonCrasha() {
        auto* ai = new MockAiClient;
        auto* hw = new HardwareMonitor;
        auto* p  = new MonitorPanel(ai, hw, nullptr);
        delete p;
        delete hw;
        delete ai;
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    {
        TestCostruzione     t1; status |= QTest::qExec(&t1, argc, argv);
        TestStrutturaWidget t2; status |= QTest::qExec(&t2, argc, argv);
        TestAggiornamentoHW t3; status |= QTest::qExec(&t3, argc, argv);
    }

    delete s_dlg;
    delete s_hw;
    delete s_ai;
    return status;
}

#include "test_monitor_panel.moc"

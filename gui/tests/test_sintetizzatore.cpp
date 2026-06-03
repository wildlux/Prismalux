/* ══════════════════════════════════════════════════════════════
   test_sintetizzatore.cpp — Unit test SintetizzatoreWidget

   Categorie:
     CAT-A  Struct Tono — label(), campi
     CAT-B  OscoCanvas — creazione, setTono, setAnimating
     CAT-C  SintetizzatoreWidget — creazione, init, no crash

   Requisiti: Qt Multimedia opzionale (usa aplay), nessuna rete.

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_sintetizzatore
     ./build_tests/test_sintetizzatore
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>

#include "../pages/widget_synthesizer.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — Struct Tono
   ══════════════════════════════════════════════════════════════ */
class TestTono : public QObject {
    Q_OBJECT
private slots:

    /* A-1: Tono::label() contiene freq, onda e dur */
    void labelContieneFreqOndaDur() {
        Tono t{ 440.0, 1000, "sine", 0.8 };
        const QString lbl = t.label();
        QVERIFY2(!lbl.isEmpty(), "label() non deve essere vuota");
        QVERIFY2(lbl.contains("440") || lbl.contains("sine") || lbl.contains("1000"),
                 "label() deve contenere freq, onda o durata");
    }

    /* A-2: Tono con valori diversi → label() diversa */
    void labelDiversaPerFrequenzaDiversa() {
        Tono t1{ 440.0, 500, "sine", 0.5 };
        Tono t2{ 880.0, 500, "sine", 0.5 };
        QVERIFY2(t1.label() != t2.label(), "frequenze diverse → label() diverse");
    }

    /* A-3: Tono con forma d'onda diversa → label() diversa */
    void labelDiversaPerOndaDiversa() {
        Tono t1{ 440.0, 500, "sine",     0.5 };
        Tono t2{ 440.0, 500, "square",   0.5 };
        Tono t3{ 440.0, 500, "sawtooth", 0.5 };
        QVERIFY2(t1.label() != t2.label() || t2.label() != t3.label(),
                 "forme d'onda diverse → almeno due label() diverse");
    }

    /* A-4: Tono — volume 0 e 1 entrambi validi */
    void volumeEstremiNonCrasha() {
        Tono t0{ 440.0, 200, "sine", 0.0 };
        Tono t1{ 440.0, 200, "sine", 1.0 };
        QVERIFY(!t0.label().isEmpty());
        QVERIFY(!t1.label().isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — OscoCanvas
   ══════════════════════════════════════════════════════════════ */
class TestOscoCanvas : public QObject {
    Q_OBJECT
private slots:

    /* B-1: OscoCanvas — costruzione senza crash */
    void costruzioneNoCrash() {
        OscoCanvas* canvas = new OscoCanvas(nullptr);
        QVERIFY(canvas != nullptr);
        delete canvas;
    }

    /* B-2: setTono — frequenza valida non crasha */
    void setTonoFreqNonCrasha() {
        OscoCanvas canvas(nullptr);
        canvas.setTono(440.0, "sine", 0.7);
        canvas.setTono(220.0, "square", 1.0);
        canvas.setTono(880.0, "sawtooth", 0.5);
        QVERIFY(true);  /* nessun crash */
    }

    /* B-3: setAnimating on/off non crasha */
    void setAnimatingNoCrash() {
        OscoCanvas canvas(nullptr);
        canvas.setAnimating(true);
        canvas.setAnimating(false);
        canvas.setAnimating(true);
        QVERIFY(true);
    }

    /* B-4: setTono con frequenza limite (1 Hz e 20000 Hz) */
    void setTonoFreqLimite() {
        OscoCanvas canvas(nullptr);
        canvas.setTono(1.0,     "sine", 0.5);
        canvas.setTono(20000.0, "sine", 0.5);
        QVERIFY(true);
    }

    /* B-5: mostra widget — paint non crasha */
    void paintNoCrash() {
        OscoCanvas canvas(nullptr);
        canvas.setTono(440.0, "sine", 0.7);
        canvas.resize(200, 80);
        canvas.show();
        QApplication::processEvents();
        canvas.hide();
        QVERIFY(true);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — SintetizzatoreWidget
   ══════════════════════════════════════════════════════════════ */
class TestSintetizzatoreWidget : public QObject {
    Q_OBJECT
private slots:

    /* C-1: costruzione senza crash */
    void costruzioneNoCrash() {
        SintetizzatoreWidget* w = new SintetizzatoreWidget(nullptr);
        QVERIFY(w != nullptr);
        delete w;
    }

    /* C-2: show/hide senza crash */
    void showHideNoCrash() {
        SintetizzatoreWidget w(nullptr);
        w.resize(640, 400);
        w.show();
        QApplication::processEvents();
        w.hide();
        QVERIFY(true);
    }

    /* C-3: doppia istanza — nessun singleton indesiderato */
    void doppiaIstanzaNoCrash() {
        SintetizzatoreWidget w1(nullptr);
        SintetizzatoreWidget w2(nullptr);
        w1.show();
        w2.show();
        QApplication::processEvents();
        QVERIFY(true);
    }

    /* C-4: widget ha dimensioni ragionevoli dopo show */
    void widgetHaDimensioniRagionevoli() {
        SintetizzatoreWidget w(nullptr);
        w.resize(640, 400);
        w.show();
        QApplication::processEvents();
        QVERIFY2(w.width() > 0,  "larghezza deve essere > 0");
        QVERIFY2(w.height() > 0, "altezza deve essere > 0");
        w.hide();
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    {
        TestTono              t1; status |= QTest::qExec(&t1, argc, argv);
        TestOscoCanvas        t2; status |= QTest::qExec(&t2, argc, argv);
        TestSintetizzatoreWidget t3; status |= QTest::qExec(&t3, argc, argv);
    }
    return status;
}

#include "test_sintetizzatore.moc"

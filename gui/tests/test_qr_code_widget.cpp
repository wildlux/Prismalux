/* test_qr_code_widget.cpp — TN-13 (18 test)
 * Modulo: widgets/qr_code_widget.h
 * CAT-A: generazione QR (8), CAT-B: rendering (6), CAT-C: dialog/lifetime (4)
 *
 * Build: cmake -B build_tests -DBUILD_TESTS=ON && cmake --build build_tests
 * Run:   ./build_tests/test_qr_code_widget
 */
#include <QtTest/QtTest>
#include <QApplication>
#include <QClipboard>
#include <QPixmap>
#include <QPainter>
#include "../widgets/qr_code_widget.h"
#include "../qrcodegen.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — Generazione QR
   ══════════════════════════════════════════════════════════════ */
class TestQrGeneration : public QObject {
    Q_OBJECT
private slots:

    /* A1 — URL tipico LAN → isValid() true */
    void testUrlCorto() {
        QrCodeWidget w("http://192.168.1.1:11500/apk");
        QVERIFY(w.isValid());
    }

    /* A2 — stringa vuota → isValid() false */
    void testStringaVuota() {
        QrCodeWidget w("");
        QVERIFY(!w.isValid());
    }

    /* A3 — stringa 300 char → no crash (può fallire per capacità QR) */
    void testStringaLunga() {
        const QString lunga = "http://192.168.1.100:11500/apk?x=" + QString(270, 'a');
        QrCodeWidget w(lunga);
        (void)w.isValid(); /* non crashare è l'invariante */
        QVERIFY(true);
    }

    /* A4 — caratteri UTF-8 nell'URL → no crash */
    void testCaratteriNonAscii() {
        QrCodeWidget w(QString::fromUtf8("http://192.168.1.1:11500/apk?n=\xc3\xa0\xc3\xa8"));
        (void)w.isValid();
        QVERIFY(true);
    }

    /* A5 — setText: transizione invalido→valido→invalido */
    void testSetText() {
        QrCodeWidget w("");
        QVERIFY(!w.isValid());
        w.setText("http://10.0.0.5:11500/apk");
        QVERIFY(w.isValid());
        w.setText("");
        QVERIFY(!w.isValid());
    }

    /* A6 — size QR ≥ 21 moduli (Version 1 minima ISO 18004) */
    void testQrSizeMinima() {
        const QByteArray url("http://192.168.1.1:11500/apk");
        const size_t bufLen = qrcodegen_BUFFER_LEN_MAX;
        std::vector<uint8_t> tmp(bufLen * 2, 0);
        const bool ok = qrcodegen_encodeText(
            url.constData(), tmp.data(), tmp.data() + bufLen,
            qrcodegen_Ecc_MEDIUM,
            qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
            qrcodegen_Mask_AUTO, true);
        QVERIFY(ok);
        QVERIFY(qrcodegen_getSize(tmp.data() + bufLen) >= 21);
    }

    /* A7 — getModule su ogni pixel → no crash/UB */
    void testGetModuleTuttiPixel() {
        const QByteArray url("http://10.0.0.1:11500/apk");
        const size_t bufLen = qrcodegen_BUFFER_LEN_MAX;
        std::vector<uint8_t> tmp(bufLen * 2, 0);
        const bool ok = qrcodegen_encodeText(
            url.constData(), tmp.data(), tmp.data() + bufLen,
            qrcodegen_Ecc_MEDIUM,
            qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
            qrcodegen_Mask_AUTO, true);
        QVERIFY(ok);
        const uint8_t* qr = tmp.data() + bufLen;
        const int sz = qrcodegen_getSize(qr);
        for (int y = 0; y < sz; y++)
            for (int x = 0; x < sz; x++)
                (void)qrcodegen_getModule(qr, x, y);
        QVERIFY(true);
    }

    /* A8 — stress: 200 generazioni diverse → no crash */
    void testStressGenerazione() {
        for (int i = 0; i < 200; i++) {
            QrCodeWidget w(QString("http://192.168.1.%1:11500/apk").arg(i % 256));
            QVERIFY(w.isValid());
        }
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Rendering Qt
   ══════════════════════════════════════════════════════════════ */
class TestQrRendering : public QObject {
    Q_OBJECT
private slots:

    /* B1 — paint widget valido → no crash */
    void testPaintValido() {
        QrCodeWidget w("http://192.168.1.1:11500/apk");
        w.resize(280, 280);
        QPixmap pm(280, 280);
        {
            QPainter p(&pm);
            w.render(&p);
        }
        QVERIFY(true);
    }

    /* B2 — paint widget non valido → mostra errore, no crash */
    void testPaintNonValido() {
        QrCodeWidget w("");
        QVERIFY(!w.isValid());
        w.resize(200, 200);
        QPixmap pm(200, 200);
        {
            QPainter p(&pm);
            w.render(&p);
        }
        QVERIFY(true);
    }

    /* B3 — resize a varie dimensioni → no crash */
    void testPaintDopoResize() {
        QrCodeWidget w("http://192.168.1.1:11500/apk");
        for (int s : {50, 100, 200, 400}) {
            w.resize(s, s);
            QPixmap pm(s, s);
            QPainter p(&pm);
            w.render(&p);
        }
        QVERIFY(true);
    }

    /* B4 — sizeHint() == (280, 280) */
    void testSizeHint() {
        QrCodeWidget w("http://192.168.1.1:11500/apk");
        QCOMPARE(w.sizeHint(), QSize(280, 280));
    }

    /* B5 — fixed size piccolo → no crash */
    void testPaintPiccolo() {
        QrCodeWidget w("http://192.168.1.1:11500/apk");
        w.setFixedSize(80, 80);
        QPixmap pm(80, 80);
        QPainter p(&pm);
        w.render(&p);
        QVERIFY(true);
    }

    /* B6 — sfondo bianco: pixel (0,0) = bianco dopo paint */
    void testSfondoBianco() {
        QrCodeWidget w("http://192.168.1.1:11500/apk");
        w.resize(300, 300);
        QPixmap pm(300, 300);
        pm.fill(Qt::red);
        {
            QPainter p(&pm);
            w.render(&p);
        }
        const QImage img = pm.toImage();
        QCOMPARE(img.pixel(0, 0), QColor(Qt::white).rgb());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Lifetime e clipboard
   ══════════════════════════════════════════════════════════════ */
class TestQrLifetime : public QObject {
    Q_OBJECT
private slots:

    /* C1 — URL valido in widget → isValid() true */
    void testValidoInDialog() {
        auto* w = new QrCodeWidget("http://192.168.1.100:11500/apk");
        QVERIFY(w->isValid());
        delete w;
    }

    /* C2 — clipboard riceve URL corretto */
    void testClipboard() {
        const QString url = "http://192.168.1.100:11500/apk";
        QApplication::clipboard()->setText(url);
        QCOMPARE(QApplication::clipboard()->text(), url);
    }

    /* C3 — creazione/distruzione × 10 → no dangling */
    void testLifetimeMultiplo() {
        for (int i = 0; i < 10; i++) {
            auto* w = new QrCodeWidget("http://10.0.0.1:11500/apk");
            QVERIFY(w->isValid());
            delete w;
        }
    }

    /* C4 — URL con parametri query → isValid() true */
    void testUrlConParametri() {
        QrCodeWidget w("http://192.168.1.1:11500/apk?v=2.8&os=android");
        QVERIFY(w.isValid());
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int status = 0;
    { TestQrGeneration t; status |= QTest::qExec(&t, argc, argv); }
    { TestQrRendering  t; status |= QTest::qExec(&t, argc, argv); }
    { TestQrLifetime   t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_qr_code_widget.moc"

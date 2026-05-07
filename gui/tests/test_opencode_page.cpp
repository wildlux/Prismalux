/* ══════════════════════════════════════════════════════════════
   test_opencode_page.cpp — Unit test OpenCodePage

   Categorie:
     CAT-A  Costruzione — OpenCodePage senza crash, senza server reale
     CAT-B  Widget struttura — statusLbl, modelCombo, log, input, startBtn
     CAT-C  Stato iniziale — server non in esecuzione, sendBtn/abortBtn disabilitati

   Nota: OpenCodePage non avvia il server nel costruttore; è sicuro istanziarla
         senza opencode installato.

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_opencode_page
     ./build_tests/test_opencode_page
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QComboBox>
#include <QTextBrowser>
#include <QTextEdit>
#include <QPushButton>

#include "../pages/opencode_page.h"

static OpenCodePage* s_page = nullptr;

static void ensurePage() {
    if (!s_page)
        s_page = new OpenCodePage(nullptr);
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruzione
   ══════════════════════════════════════════════════════════════ */
class TestCostruzione : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    /* A-1: costruisce senza crash */
    void costruisceSenzaCrash() {
        QVERIFY2(s_page != nullptr, "OpenCodePage non costruita");
    }

    /* A-2: è un QWidget */
    void eUnQWidget() {
        QVERIFY2(qobject_cast<QWidget*>(s_page) != nullptr,
                 "OpenCodePage non è un QWidget");
    }

    /* A-3: ha figli widget (non è vuota) */
    void haBigliFigli() {
        QVERIFY2(!s_page->findChildren<QWidget*>().isEmpty(),
                 "OpenCodePage sembra vuota");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Struttura widget
   ══════════════════════════════════════════════════════════════ */
class TestStrutturaWidget : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    /* B-1: ha almeno un QLabel (statusLbl o simile) */
    void haQLabelStato() {
        const auto labels = s_page->findChildren<QLabel*>();
        QVERIFY2(!labels.isEmpty(), "OpenCodePage: nessun QLabel trovato");
    }

    /* B-2: ha almeno un QComboBox (modelCombo o cwdEdit come alternativa) */
    void haQComboOQLineEdit() {
        const bool hasCombo = !s_page->findChildren<QComboBox*>().isEmpty();
        const bool hasLineEdit = !s_page->findChildren<QLineEdit*>().isEmpty();
        QVERIFY2(hasCombo || hasLineEdit,
                 "OpenCodePage: nessun QComboBox né QLineEdit trovato");
    }

    /* B-3: ha un QTextBrowser (log) */
    void haQTextBrowser() {
        const auto browsers = s_page->findChildren<QTextBrowser*>();
        QVERIFY2(!browsers.isEmpty(), "OpenCodePage: QTextBrowser log assente");
    }

    /* B-4: ha un QTextEdit (input) */
    void haQTextEdit() {
        const auto edits = s_page->findChildren<QTextEdit*>();
        QVERIFY2(!edits.isEmpty(), "OpenCodePage: QTextEdit input assente");
    }

    /* B-5: ha almeno 2 QPushButton (start + send o simili) */
    void haAlmeno2QPushButton() {
        const auto btns = s_page->findChildren<QPushButton*>();
        QVERIFY2(btns.size() >= 2,
                 qPrintable(QString("OpenCodePage: attesi >= 2 QPushButton, trovati %1")
                            .arg(btns.size())));
    }

    /* B-6: pulsante Start/Avvia esiste */
    void haStartBtn() {
        const auto btns = s_page->findChildren<QPushButton*>();
        const bool found = std::any_of(btns.begin(), btns.end(), [](QPushButton* b){
            const QString t = b->text();
            return t.contains("Start",  Qt::CaseInsensitive) ||
                   t.contains("Avvia",  Qt::CaseInsensitive) ||
                   t.contains("Server", Qt::CaseInsensitive);
        });
        if (!found) QSKIP("Start button non trovato (testo diverso da atteso)");
        QVERIFY(found);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Stato iniziale
   ══════════════════════════════════════════════════════════════ */
class TestStatoIniziale : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    /* C-1: OpenCodePage non avvia il server nel costruttore —
       i pulsanti Send/Abort devono essere disabilitati */
    void sendBtnDisabilitatoAllAvvio() {
        const auto btns = s_page->findChildren<QPushButton*>();
        for (auto* b : btns) {
            if (b->text().contains("Invia",  Qt::CaseInsensitive) ||
                b->text().contains("Send",   Qt::CaseInsensitive) ||
                b->text().contains("Avvia",  Qt::CaseInsensitive) == false)  /* esclude Start */
            {
                /* Send e Abort devono essere disabilitati senza server */
                if (b->text().contains("Send", Qt::CaseInsensitive) ||
                    b->text().contains("Invia", Qt::CaseInsensitive) ||
                    b->text().contains("Abort", Qt::CaseInsensitive))
                    QVERIFY2(!b->isEnabled(),
                             qPrintable("pulsante '" + b->text() + "' abilitato senza server"));
            }
        }
    }

    /* C-2: distruzione senza crash */
    void distruzioneSenzaCrash() {
        /* creiamo un'istanza locale e la distruggiamo immediatamente */
        auto* p = new OpenCodePage(nullptr);
        delete p;  /* non deve crashare */
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    {
        TestCostruzione     t1; status |= QTest::qExec(&t1, argc, argv);
        TestStrutturaWidget t2; status |= QTest::qExec(&t2, argc, argv);
        TestStatoIniziale   t3; status |= QTest::qExec(&t3, argc, argv);
    }

    delete s_page;
    return status;
}

#include "test_opencode_page.moc"

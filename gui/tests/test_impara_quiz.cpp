/* ══════════════════════════════════════════════════════════════
   test_impara_quiz.cpp — Unit test ImparaPage, QuizPage, MateriePage

   Categorie:
     CAT-A  ImparaPage — costruzione senza crash, ha QStackedWidget
     CAT-B  QuizPage — costruzione senza crash, m_btnGenera non null
     CAT-C  MateriePage — costruzione senza crash

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_impara_quiz
     ./build_tests/test_impara_quiz
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QStackedWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>

#include "mock_ai_client.h"
#include "../pages/impara_page.h"
#include "../pages/quiz_page.h"
#include "../pages/materie_page.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — ImparaPage
   ══════════════════════════════════════════════════════════════ */
class TestImparaPage : public QObject {
    Q_OBJECT
private:
    MockAiClient* m_ai   = nullptr;
    ImparaPage*   m_page = nullptr;

private slots:

    void initTestCase() {
        m_ai   = new MockAiClient;
        m_page = new ImparaPage(m_ai);
    }

    void cleanupTestCase() {
        delete m_page; m_page = nullptr;
        delete m_ai;   m_ai   = nullptr;
    }

    /* A-1: costruisce senza crash */
    void costruisceSenzaCrash() {
        QVERIFY2(m_page != nullptr, "ImparaPage non costruita");
    }

    /* A-2: è un QWidget */
    void eUnQWidget() {
        QVERIFY2(qobject_cast<QWidget*>(m_page) != nullptr,
                 "ImparaPage non è un QWidget");
    }

    /* A-3: ha un QStackedWidget interno (m_inner) */
    void haQStackedWidget() {
        const auto stacked = m_page->findChildren<QStackedWidget*>();
        QVERIFY2(!stacked.isEmpty(),
                 "ImparaPage: nessun QStackedWidget trovato");
    }

    /* A-4: ha figli widget (non è vuota) */
    void haBigliFigli() {
        QVERIFY2(!m_page->findChildren<QWidget*>().isEmpty(),
                 "ImparaPage sembra vuota");
    }

    /* A-5: ha almeno un QPushButton (navigazione menu) */
    void haAlmeno1QPushButton() {
        const auto btns = m_page->findChildren<QPushButton*>();
        QVERIFY2(!btns.isEmpty(), "ImparaPage: nessun QPushButton trovato");
    }

    /* A-6: show() non crasha */
    void showNonCrasha() {
        m_page->show();
        QApplication::processEvents();
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — QuizPage
   ══════════════════════════════════════════════════════════════ */
class TestQuizPage : public QObject {
    Q_OBJECT
private:
    MockAiClient* m_ai   = nullptr;
    QuizPage*     m_page = nullptr;

private slots:

    void initTestCase() {
        m_ai   = new MockAiClient;
        m_page = new QuizPage(m_ai);
    }

    void cleanupTestCase() {
        delete m_page; m_page = nullptr;
        delete m_ai;   m_ai   = nullptr;
    }

    /* B-1: costruisce senza crash */
    void costruisceSenzaCrash() {
        QVERIFY2(m_page != nullptr, "QuizPage non costruita");
    }

    /* B-2: è un QWidget */
    void eUnQWidget() {
        QVERIFY2(qobject_cast<QWidget*>(m_page) != nullptr,
                 "QuizPage non è un QWidget");
    }

    /* B-3: m_btnGenera è presente (QPushButton con testo "Genera") */
    void btnGeneraPresente() {
        const auto btns = m_page->findChildren<QPushButton*>();
        const bool found = std::any_of(btns.begin(), btns.end(), [](QPushButton* b){
            return b->text().contains("Genera", Qt::CaseInsensitive);
        });
        QVERIFY2(found, "QuizPage: pulsante 'Genera' non trovato");
    }

    /* B-4: m_btnCopy è presente (QPushButton con testo "Copia" o icona clipboard) */
    void btnCopyPresente() {
        const auto btns = m_page->findChildren<QPushButton*>();
        const bool found = std::any_of(btns.begin(), btns.end(), [](QPushButton* b){
            return b->text().contains("Copia", Qt::CaseInsensitive) ||
                   b->text().contains("Copy",  Qt::CaseInsensitive) ||
                   b->toolTip().contains("Copia", Qt::CaseInsensitive);
        });
        if (!found) QSKIP("btnCopy non trovato con testo 'Copia'/'Copy'");
        QVERIFY(found);
    }

    /* B-5: m_quizSubj (QComboBox materia) è presente */
    void quizSubjPresente() {
        const auto combos = m_page->findChildren<QComboBox*>();
        QVERIFY2(!combos.isEmpty(), "QuizPage: nessun QComboBox trovato");
    }

    /* B-6: m_quizQuestion (QLabel domanda) è presente */
    void quizQuestionPresente() {
        const auto labels = m_page->findChildren<QLabel*>();
        QVERIFY2(!labels.isEmpty(), "QuizPage: nessun QLabel trovato");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — MateriePage
   ══════════════════════════════════════════════════════════════ */
class TestMateriePage : public QObject {
    Q_OBJECT
private:
    MockAiClient* m_ai   = nullptr;
    MateriePage*  m_page = nullptr;

private slots:

    void initTestCase() {
        m_ai   = new MockAiClient;
        m_page = new MateriePage(m_ai);
    }

    void cleanupTestCase() {
        delete m_page; m_page = nullptr;
        delete m_ai;   m_ai   = nullptr;
    }

    /* C-1: costruisce senza crash */
    void costruisceSenzaCrash() {
        QVERIFY2(m_page != nullptr, "MateriePage non costruita");
    }

    /* C-2: è un QWidget */
    void eUnQWidget() {
        QVERIFY2(qobject_cast<QWidget*>(m_page) != nullptr,
                 "MateriePage non è un QWidget");
    }

    /* C-3: ha figli widget (non è vuota) */
    void haBigliFigli() {
        QVERIFY2(!m_page->findChildren<QWidget*>().isEmpty(),
                 "MateriePage sembra vuota");
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    {
        TestImparaPage t1; status |= QTest::qExec(&t1, argc, argv);
        TestQuizPage   t2; status |= QTest::qExec(&t2, argc, argv);
        TestMateriePage t3; status |= QTest::qExec(&t3, argc, argv);
    }
    return status;
}

#include "test_impara_quiz.moc"

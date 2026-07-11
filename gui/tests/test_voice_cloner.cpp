/* ══════════════════════════════════════════════════════════════
   test_voice_cloner.cpp — Unit test per VoiceClonerWidget (T-D20)

   Verifica il fix D-20 (coredump 2026-07-03, SIGSEGV): il distruttore
   di VoiceClonerWidget deve poter distruggere in sicurezza le probe
   QProcess anonime di checkTtsInstalled() anche se sono ancora in
   esecuzione — senza questo test, la stessa scena richiederebbe di
   aprire Multimedia → Clona Voce nella GUI reale e chiudere l'app
   subito dopo (T-D20 in TODO.md, mai verificato prima d'ora).

   Categorie:
     CAT-A  Costruzione — no crash, widget valido
     CAT-B  Distruzione immediata (D-20) — costruisci e distruggi subito,
            mentre le 3 probe di checkTtsInstalled() sono quasi certamente
            ancora in esecuzione (avvio Python + import libreria richiede
            decine di ms, la delete è sincrona e immediata dopo il
            costruttore, senza girare l'event loop)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_voice_cloner
     ./build_tests/test_voice_cloner
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include "../pages/widget_voice_cloner.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruzione
   ══════════════════════════════════════════════════════════════ */
class TestVoiceClonerConstruction : public QObject {
    Q_OBJECT
private slots:

    /* A-1: costruzione normale, no crash — lascia girare l'event loop
       finché le probe checkTtsInstalled() completano, poi distrugge */
    void buildAndSettleNoCrash() {
        auto* w = new VoiceClonerWidget();
        QVERIFY(w != nullptr);
        QTest::qWait(3000);   /* tempo per completare le 3 probe Python */
        delete w;
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B (D-20) — Distruzione immediata con probe ancora attive
   ══════════════════════════════════════════════════════════════ */
class TestVoiceClonerImmediateDestruction : public QObject {
    Q_OBJECT
private slots:

    /* B-1: costruisci e distruggi SUBITO, senza processare eventi — le
       3 probe QProcess di checkTtsInstalled() (python -c "import TTS/
       chatterbox/edge_tts") sono quasi certamente ancora in stato Running
       o Starting a questo punto. Prima del fix D-20, il ~QProcess di una
       probe ancora attiva durante deleteChildren() emetteva finished()
       sincrono → lambda → applyBackend() → setText() su label già
       distrutta → SIGSEGV (coredump reale 2026-07-03). Il successo di
       questo test (nessun crash del processo di test) È la verifica. */
    void deleteImmediatelyAfterConstructNoCrash() {
        auto* w = new VoiceClonerWidget();
        delete w;   /* nessun QTest::qWait()/processEvents() prima — apposta */
    }

    /* B-2: ripetuto 5 volte di seguito — riduce il rischio che una singola
       esecuzione fortunata (probe già terminate per un caso raro di
       sistema molto veloce) nasconda una regressione */
    void deleteImmediatelyRipetutoNoCrash() {
        for (int i = 0; i < 5; ++i) {
            auto* w = new VoiceClonerWidget();
            delete w;
        }
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int status = 0;
    { TestVoiceClonerConstruction         t1; status |= QTest::qExec(&t1, argc, argv); }
    { TestVoiceClonerImmediateDestruction t2; status |= QTest::qExec(&t2, argc, argv); }
    return status;
}

#include "test_voice_cloner.moc"

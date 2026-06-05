/* ══════════════════════════════════════════════════════════════
   test_stt_whisper_live.cpp — Unit test STT/Whisper: paths, settings, callback

   Categorie:
     CAT-A  SttWhisper paths — whisperBin(), whisperModelsDir(), preferredModel()
     CAT-B  ImpostazioniPage — costruzione con tab Voce (no crash)
     CAT-C  transcribe() callback contract — no crash su path invalido
     CAT-D  savePreferredModel / preferredModel round-trip via AppConfig

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_stt_whisper_live
     ./build_tests/test_stt_whisper_live
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QTabWidget>
#include <QDir>
#include <QFileInfo>

#include "mock_ai_client.h"
#include "../ai_client.h"
#include "../hardware_monitor.h"
#include "../app_config.h"
#include "../prismalux_paths.h"
#include "../pages/settings_main.h"
#include "../widgets/stt_whisper.h"

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   CAT-A — SttWhisper paths (nessuna dipendenza esterna richiesta)
   ══════════════════════════════════════════════════════════════ */
class TestSttWhisperPaths : public QObject {
    Q_OBJECT
private slots:

    /* A-1: whisperBin() restituisce stringa (vuota o path valida) */
    void binNotNull() {
        const QString bin = SttWhisper::whisperBin();
        /* Non crash — può essere vuota se non compilato */
        if (!bin.isEmpty()) {
            QVERIFY2(QFileInfo::exists(bin), "whisperBin() punta a un file inesistente");
        }
    }

    /* A-2: whisperModelsDir() restituisce una path plausibile */
    void modelsDirPlausible() {
        const QString dir = SttWhisper::whisperModelsDir();
        QVERIFY2(!dir.isEmpty(), "whisperModelsDir() e' vuota");
        /* Non usiamo QDir::exists() — la dir non deve per forza esistere senza setup */
        QVERIFY2(dir.contains("whisper") || dir.contains("model") || dir.contains("C_software"),
                 "whisperModelsDir() non sembra un path whisper");
    }

    /* A-3: whisperModel() o e' vuota o punta a file .bin esistente */
    void modelPathValid() {
        const QString model = SttWhisper::whisperModel();
        if (!model.isEmpty()) {
            QVERIFY2(model.endsWith(".bin"), "whisperModel() non ha estensione .bin");
            QVERIFY2(QFileInfo::exists(model), "whisperModel() punta a un file inesistente");
        }
    }

    /* A-4: preferredModel() e' vuota o coerente con QSettings */
    void preferredModelQSettings() {
        const QString pref = SttWhisper::preferredModel();
        const QString stored = AppConfig::s().value(P::SK::kSttModelPath).toString();
        QCOMPARE(pref, stored);
    }

    /* A-5: isAvailable() e' coerente con bin+model */
    void isAvailableCoherent() {
        const bool binOk   = !SttWhisper::whisperBin().isEmpty();
        const bool modelOk = !SttWhisper::whisperModel().isEmpty();
        QCOMPARE(SttWhisper::isAvailable(), binOk && modelOk);
    }

    /* A-6: setupMessage() e' vuota se isAvailable(), non vuota se non disponibile */
    void setupMessageLogic() {
        if (SttWhisper::isAvailable()) {
            QVERIFY2(SttWhisper::setupMessage().isEmpty(),
                     "setupMessage() non vuota nonostante isAvailable()");
        } else {
            QVERIFY2(!SttWhisper::setupMessage().isEmpty(),
                     "setupMessage() vuota quando STT non disponibile");
        }
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — ImpostazioniPage costruzione con tab Voce
   ══════════════════════════════════════════════════════════════ */
class TestSttWhisperImpostazioni : public QObject {
    Q_OBJECT
private:
    MockAiClient*    m_ai  = nullptr;
    HardwareMonitor* m_hw  = nullptr;

private slots:
    void initTestCase() {
        m_ai = new MockAiClient(this);
        m_hw = new HardwareMonitor(this);
    }

    /* B-1: costruzione ImpostazioniPage senza crash */
    void buildNocrash() {
        ImpostazioniPage* page = new ImpostazioniPage(m_ai, m_hw, nullptr);
        QVERIFY(page != nullptr);
        delete page;
    }

    /* B-2: ImpostazioniPage contiene un QTabWidget */
    void hasTabWidget() {
        ImpostazioniPage* page = new ImpostazioniPage(m_ai, m_hw, nullptr);
        QTabWidget* tab = page->findChild<QTabWidget*>();
        QVERIFY2(tab != nullptr, "ImpostazioniPage non ha un QTabWidget");
        delete page;
    }

    /* B-3: almeno un tab con testo contenente "Voce" o "Audio" o "LLM" */
    void hasVoiceOrLlmTab() {
        ImpostazioniPage* page = new ImpostazioniPage(m_ai, m_hw, nullptr);
        QTabWidget* tab = page->findChild<QTabWidget*>();
        if (!tab) { delete page; QSKIP("QTabWidget non trovato"); }

        bool found = false;
        for (int i = 0; i < tab->count(); ++i) {
            const QString t = tab->tabText(i);
            if (t.contains("Voce", Qt::CaseInsensitive) ||
                t.contains("Audio", Qt::CaseInsensitive) ||
                t.contains("LLM", Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Nessun tab Voce/Audio/LLM in ImpostazioniPage");
        delete page;
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — transcribe() callback contract
   ══════════════════════════════════════════════════════════════ */
class TestSttWhisperTranscribeContract : public QObject {
    Q_OBJECT
private slots:

    /* C-1: path inesistente → callback chiama onDone(_, false) senza crash */
    void invalidPathCallsCallback() {
        if (!SttWhisper::isAvailable()) {
            /* Whisper non disponibile: transcribe chiama onDone con setup msg */
            bool called = false;
            bool okVal  = true;
            SttWhisper::transcribe("/nonexistent.wav", "it", this,
                [&](const QString& txt, bool ok) {
                    called = true;
                    okVal  = ok;
                    QVERIFY(!txt.isEmpty());
                });
            /* Se non disponibile, la callback deve essere chiamata subito (sync) */
            QVERIFY2(called, "callback non chiamata quando STT non disponibile");
            QVERIFY2(!okVal, "ok deve essere false quando STT non disponibile");
            return;
        }

        /* Whisper disponibile ma file inesistente → processo avviato, finisce con errore */
        bool called    = false;
        bool okResult  = true;
        QEventLoop loop;

        QProcess* proc = SttWhisper::transcribe("/tmp/prismalux_test_nonexist_42.wav", "it",
            this, [&](const QString&, bool ok) {
                called   = true;
                okResult = ok;
                loop.quit();
            });

        if (proc) {
            /* Attendiamo al massimo 8s che il processo termini */
            QTimer::singleShot(8000, &loop, &QEventLoop::quit);
            loop.exec();
            QVERIFY2(called, "callback non chiamata entro 8s per file inesistente");
            QVERIFY2(!okResult, "ok deve essere false per file inesistente");
        } else {
            QVERIFY2(called, "proc==nullptr ma callback non chiamata");
        }
    }

    /* C-2: transcribe() restituisce nullptr quando STT non disponibile */
    void returnsNullptrWhenUnavailable() {
        if (SttWhisper::isAvailable()) QSKIP("STT disponibile: test non applicabile");

        QProcess* proc = SttWhisper::transcribe("/tmp/any.wav", "it", this,
            [](const QString&, bool) {});
        QVERIFY2(proc == nullptr, "transcribe() deve ritornare nullptr se non disponibile");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — savePreferredModel / preferredModel round-trip
   ══════════════════════════════════════════════════════════════ */
class TestSttWhisperPreferredModel : public QObject {
    Q_OBJECT
private:
    QString m_savedPrev;

private slots:
    void initTestCase() {
        /* Backup valore attuale per ripristinarlo dopo */
        m_savedPrev = SttWhisper::preferredModel();
    }

    void cleanupTestCase() {
        /* Ripristina il valore originale */
        SttWhisper::savePreferredModel(m_savedPrev);
    }

    /* D-1: save → preferredModel() restituisce lo stesso valore */
    void roundTripPath() {
        const QString fake = "/tmp/ggml-tiny-test.bin";
        SttWhisper::savePreferredModel(fake);
        QCOMPARE(SttWhisper::preferredModel(), fake);
    }

    /* D-2: save stringa vuota → preferredModel() e' vuota */
    void saveEmpty() {
        SttWhisper::savePreferredModel(QString());
        QVERIFY(SttWhisper::preferredModel().isEmpty());
    }

    /* D-3: AppConfig::s() riflette il valore salvato */
    void appConfigSync() {
        const QString fake = "/tmp/ggml-base-test.bin";
        SttWhisper::savePreferredModel(fake);
        const QString stored = AppConfig::s().value(P::SK::kSttModelPath).toString();
        QCOMPARE(stored, fake);
    }

    /* D-4: salvataggio multiplo — l'ultimo vince */
    void multiSaveLastWins() {
        SttWhisper::savePreferredModel("/tmp/ggml-first.bin");
        SttWhisper::savePreferredModel("/tmp/ggml-second.bin");
        SttWhisper::savePreferredModel("/tmp/ggml-third.bin");
        QCOMPARE(SttWhisper::preferredModel(), QString("/tmp/ggml-third.bin"));
    }
};

/* ── main ── */
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int status = 0;
    {
        TestSttWhisperPaths           t1; status |= QTest::qExec(&t1, argc, argv);
        TestSttWhisperImpostazioni    t2; status |= QTest::qExec(&t2, argc, argv);
        TestSttWhisperTranscribeContract t3; status |= QTest::qExec(&t3, argc, argv);
        TestSttWhisperPreferredModel  t4; status |= QTest::qExec(&t4, argc, argv);
    }
    return status;
}

#include "test_stt_whisper_live.moc"

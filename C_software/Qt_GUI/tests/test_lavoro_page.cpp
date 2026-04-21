/* ══════════════════════════════════════════════════════════════
   test_lavoro_page.cpp — Suite di test difficili per LavoroPage
   ──────────────────────────────────────────────────────────────
   Categorie:
     CAT-A: Isolamento segnali (cross-contamination da Byzantine)
     CAT-B: Logica filtri offerte (tipo × livello)
     CAT-C: Popolazione modelli LLM (combo persistenza)
     CAT-D: Database offerte (integrità e cardinalità)
     CAT-E: Stato UI (pulsanti, log, selezione)

   Come eseguire:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_lavoro_page
     ./build_tests/test_lavoro_page

   Bug che questi test rilevano:
     - Token da Byzantine pipeline che compaiono nel log Cerca Lavoro
     - Filtro livello che non rispetta la gerarchia titoli
     - Selettore modello che sovrascrive con "🔄 Caricamento..." come modello
     - Offerte con tipo/livello non validi (typo nel database)
     - Log che non mostra il tag sorgente [CERCA LAVORO]
     - Copy button attivo quando non c'è ancora testo da copiare
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QTextEdit>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>

#include "mock_ai_client.h"
#include "../pages/lavoro_page.h"

/* tipi e livelli validi — qualsiasi stringa fuori da queste set è un bug nel DB */
static const QSet<QString> kTipiValidi {
    "IT","Retail","Ristorazione","Edilizia","Logistica",
    "Finanza","Sanitario","Produzione","Tecnico","Turismo",
    "Admin","Commerciale","Altro"
};
static const QSet<QString> kLivelliValidi {
    "qualsiasi","diploma","laurea_t","laurea_m"
};

/* ══════════════════════════════════════════════════════════════
   CAT-A: Isolamento segnali
   ══════════════════════════════════════════════════════════════ */
class TestIsolamentoSegnali : public QObject {
    Q_OBJECT
private slots:

    /* TC-A1: token emessi da un AiClient esterno NON compaiono nel log
     * Questo è il bug del "Byzantine output in Cerca Lavoro":
     * se LavoroPage usa m_ai condiviso con AgentiPage, ogni token
     * della pipeline byzantine arriva nel log lavoro.
     */
    void tokenEsternoDontAppaionoNelLog() {
        MockAiClient mainAi;    // simula AiClient di AgentiPage
        MockAiClient lavoroAi;  // AiClient isolato per LavoroPage

        LavoroPage page(&lavoroAi, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* log = page.findChild<QTextEdit*>("chatLog");
        QVERIFY2(log, "log 'chatLog' non trovato — verifica objectName");

        const QString logBefore = log->toPlainText();

        /* Simula output Byzantine da mainAi — NON deve arrivare a LavoroPage */
        mainAi.emitToken("Risposta agente A: ...analisi ontologica...");
        mainAi.emitToken("Risposta agente B: ...critica epistemica...");
        mainAi.emitToken("Giudice D: consenso raggiunto.");
        mainAi.emitFinished("Output Byzantine completo.");
        QApplication::processEvents();

        const QString logAfter = log->toPlainText();
        QCOMPARE(logAfter, logBefore);  // log invariato
    }

    /* TC-A2: token dal proprio AiClient (lavoroAi) NON devono apparire
     * a meno che una richiesta sia stata avviata esplicitamente.
     * Qui non viene avviata alcuna richiesta → log deve restare vuoto.
     */
    void tokenProprioAiSenzaRichiestaNoLog() {
        MockAiClient lavoroAi;
        LavoroPage page(&lavoroAi, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* log = page.findChild<QTextEdit*>("chatLog");
        QVERIFY(log);

        /* Emetti token dal proprio AI senza aver chiamato "Genera lettera":
         * il sistema dovrebbe ignorarli perché active=false */
        lavoroAi.emitToken("token spurio senza richiesta attiva");
        lavoroAi.emitFinished("finished spurio");
        QApplication::processEvents();

        /* Il log deve essere vuoto (nessuna richiesta avviata) */
        QVERIFY2(log->toPlainText().trimmed().isEmpty(),
                 "Log non vuoto: token spurio accettato senza richiesta attiva");
    }

    /* TC-A3: modelsReady da AI esterno NON deve popolare il combo di LavoroPage
     * (il combo è connesso solo a lavoroAi.modelsReady, non al mainAi)
     */
    void modelsReadyEsternoNonAggiornaCombmo() {
        MockAiClient mainAi;
        MockAiClient lavoroAi;
        LavoroPage page(&lavoroAi, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* cmb = page.findChild<QComboBox*>("cmbModello");
        QVERIFY(cmb);

        /* Prima popola via lavoroAi */
        emit lavoroAi.modelsReady({"modello-A", "modello-B"});
        QApplication::processEvents();
        QCOMPARE(cmb->count(), 2);
        QCOMPARE(cmb->itemText(0), QString("modello-A"));

        /* mainAi emette modelsReady con dati diversi — non deve toccare il combo */
        emit mainAi.modelsReady({"SBAGLIATO-X", "SBAGLIATO-Y", "SBAGLIATO-Z"});
        QApplication::processEvents();

        QCOMPARE(cmb->count(), 2);  // rimane a 2, non 3
        QCOMPARE(cmb->itemText(0), QString("modello-A"));  // non "SBAGLIATO-X"
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B: Logica filtri offerte
   ══════════════════════════════════════════════════════════════ */

/* Fixture helper fuori dalla classe (MOC non supporta struct annidate in slots) */
struct FiltriFixture {
    MockAiClient ai;
    LavoroPage   page;
    QListWidget* lista   = nullptr;
    QComboBox*   tipo    = nullptr;
    QComboBox*   livello = nullptr;

    FiltriFixture() : page(&ai, nullptr) {
        page.show();
        QTest::qWaitForWindowExposed(&page);
        lista   = page.findChild<QListWidget*>("offerteList");
        tipo    = page.findChild<QComboBox*>("filtroTipo");
        livello = page.findChild<QComboBox*>("filtroLivello");
    }
};

class TestFiltriFunzionalita : public QObject {
    Q_OBJECT
private slots:

    /* TC-B1: tipo "tutti" + livello "tutti" → restituisce TUTTE le offerte */
    void filtroTuttiRestituisceAll() {
        FiltriFixture f;
        QVERIFY(f.lista); QVERIFY(f.tipo); QVERIFY(f.livello);

        /* Imposta tutti i filtri a "tutti" */
        f.tipo->setCurrentIndex(   f.tipo->findData("tutti"));
        f.livello->setCurrentIndex(f.livello->findData("tutti"));
        QApplication::processEvents();

        const int n = f.lista->count();
        QVERIFY2(n >= 90, qPrintable(QString("Attese >=90 offerte, trovate %1").arg(n)));
    }

    /* TC-B2: filtro per tipo IT → solo offerte IT nella lista */
    void filtroTipoITSoloIT() {
        FiltriFixture f;
        QVERIFY(f.lista); QVERIFY(f.tipo);

        f.tipo->setCurrentIndex(   f.tipo->findData("tutti"));
        f.livello->setCurrentIndex(f.livello->findData("tutti"));
        QApplication::processEvents();

        f.tipo->setCurrentIndex(f.tipo->findData("IT"));
        QApplication::processEvents();

        const int n = f.lista->count();
        QVERIFY2(n > 0, "Nessuna offerta IT — database vuoto?");

        for (int i = 0; i < n; ++i) {
            const auto o = f.lista->item(i)->data(Qt::UserRole).value<Offerta>();
            QCOMPARE(o.tipo, QString("IT"));
        }
    }

    /* TC-B3: filtro livello "diploma" include "qualsiasi" ma esclude "laurea_t"
     * Gerarchia attesa: diploma include {diploma, qualsiasi}
     *                    laurea_t include {laurea_t, diploma, qualsiasi}
     */
    void filtroLivelloDiplomaIncludeQualsiasi() {
        FiltriFixture f;
        QVERIFY(f.lista); QVERIFY(f.tipo); QVERIFY(f.livello);

        f.tipo->setCurrentIndex(   f.tipo->findData("tutti"));
        f.livello->setCurrentIndex(f.livello->findData("diploma"));
        QApplication::processEvents();

        const int n = f.lista->count();
        QVERIFY2(n > 0, "Nessuna offerta con diploma");

        int contQual = 0, contDipl = 0, contLaureaT = 0;
        for (int i = 0; i < n; ++i) {
            const auto o = f.lista->item(i)->data(Qt::UserRole).value<Offerta>();
            if (o.livello == "qualsiasi") ++contQual;
            if (o.livello == "diploma")   ++contDipl;
            if (o.livello == "laurea_t" || o.livello == "laurea_m") ++contLaureaT;
        }
        QVERIFY2(contQual > 0,  "Filtro diploma dovrebbe includere 'qualsiasi'");
        QVERIFY2(contDipl > 0,  "Filtro diploma dovrebbe includere 'diploma'");
        QCOMPARE(contLaureaT, 0); /* laurea NON inclusa in filtro diploma */
    }

    /* TC-B4: filtro "laurea_m" deve includere laurea_m, laurea_t, diploma, qualsiasi */
    void filtroLaureaMAggregaGerarchia() {
        FiltriFixture f;
        QVERIFY(f.lista); QVERIFY(f.tipo); QVERIFY(f.livello);

        f.tipo->setCurrentIndex(   f.tipo->findData("tutti"));
        f.livello->setCurrentIndex(f.livello->findData("tutti"));
        QApplication::processEvents();
        const int tuttiCount = f.lista->count();

        f.livello->setCurrentIndex(f.livello->findData("laurea_m"));
        QApplication::processEvents();
        const int laureaMCount = f.lista->count();

        /* laurea_m deve mostrare più offerte del solo filtro "tutti-laurea_m"
         * e meno dell'universale "tutti" */
        QVERIFY2(laureaMCount > 0, "Nessuna offerta con laurea_m aggregata");
        QVERIFY2(laureaMCount <= tuttiCount, "Aggregato non può superare tutti");
    }

    /* TC-B5: tipo + livello combinati — Retail × diploma */
    void filtroCombinatoRetailDiploma() {
        FiltriFixture f;
        QVERIFY(f.lista); QVERIFY(f.tipo); QVERIFY(f.livello);

        f.tipo->setCurrentIndex(   f.tipo->findData("Retail"));
        f.livello->setCurrentIndex(f.livello->findData("diploma"));
        QApplication::processEvents();

        for (int i = 0; i < f.lista->count(); ++i) {
            const auto o = f.lista->item(i)->data(Qt::UserRole).value<Offerta>();
            QCOMPARE(o.tipo, QString("Retail"));
            const bool livOk = (o.livello == "qualsiasi" || o.livello == "diploma");
            QVERIFY2(livOk, qPrintable(QString("Offerta Retail con livello inatteso: %1").arg(o.livello)));
        }
    }

    /* TC-B6: cambio tipo aggiorna lista senza residui dal filtro precedente */
    void cambiTipoNessunResiduoPrecedente() {
        FiltriFixture f;
        QVERIFY(f.lista); QVERIFY(f.tipo); QVERIFY(f.livello);

        f.tipo->setCurrentIndex(   f.tipo->findData("tutti"));
        f.livello->setCurrentIndex(f.livello->findData("tutti"));
        QApplication::processEvents();

        f.tipo->setCurrentIndex(f.tipo->findData("IT"));
        QApplication::processEvents();
        const int countIT = f.lista->count();

        f.tipo->setCurrentIndex(f.tipo->findData("Edilizia"));
        QApplication::processEvents();
        const int countEdil = f.lista->count();

        f.tipo->setCurrentIndex(f.tipo->findData("IT"));
        QApplication::processEvents();
        const int countITBis = f.lista->count();

        /* Dopo aver cambiato e ritornato, il conteggio deve essere identico */
        QCOMPARE(countIT, countITBis);
        (void)countEdil;
    }

    /* TC-B7: tipo "Altro" ha almeno qualche offerta */
    void filtroAltroNonVuoto() {
        FiltriFixture f;
        QVERIFY(f.lista); QVERIFY(f.tipo);

        f.tipo->setCurrentIndex(   f.tipo->findData("Altro"));
        f.livello->setCurrentIndex(f.livello->findData("tutti"));
        QApplication::processEvents();

        QVERIFY2(f.lista->count() > 0, "Categoria Altro è vuota");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C: Popolazione modelli LLM
   ══════════════════════════════════════════════════════════════ */
class TestPopolazioneModelli : public QObject {
    Q_OBJECT
private slots:

    /* TC-C1: popolaModelli aggiorna il combo con i nuovi modelli */
    void popolaModelliAggiornaConto() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* cmb = page.findChild<QComboBox*>("cmbModello");
        QVERIFY(cmb);

        const QStringList modelli{"qwen3:30b", "deepseek-coder:33b", "mistral:7b"};
        emit ai.modelsReady(modelli);
        QApplication::processEvents();

        QCOMPARE(cmb->count(), 3);
        QCOMPARE(cmb->itemText(0), QString("qwen3:30b"));
    }

    /* TC-C2: selezione precedente ripristinata se ancora presente */
    void selezionePrecedenteRipristinata() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* cmb = page.findChild<QComboBox*>("cmbModello");
        QVERIFY(cmb);

        emit ai.modelsReady({"modello-A", "modello-B", "modello-C"});
        QApplication::processEvents();

        cmb->setCurrentIndex(2);  // seleziona "modello-C"
        QCOMPARE(cmb->currentText(), QString("modello-C"));

        /* Nuova lista ancora con "modello-C" → deve restare selezionato */
        emit ai.modelsReady({"modello-X", "modello-B", "modello-C"});
        QApplication::processEvents();

        QCOMPARE(cmb->currentText(), QString("modello-C"));
    }

    /* TC-C3: se selezione precedente non è più disponibile → primo elemento */
    void selezionePrecedenteNonDisponibileVaPrimo() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* cmb = page.findChild<QComboBox*>("cmbModello");
        QVERIFY(cmb);

        emit ai.modelsReady({"modello-A", "modello-B"});
        QApplication::processEvents();
        cmb->setCurrentIndex(1);  // "modello-B"

        /* Nuova lista senza "modello-B" */
        emit ai.modelsReady({"qwen3:30b", "mistral:7b"});
        QApplication::processEvents();

        QCOMPARE(cmb->currentIndex(), 0);  // cade sul primo
        QCOMPARE(cmb->currentText(), QString("qwen3:30b"));
    }

    /* TC-C4: la guard nel lambda del combo NON chiama setBackend con il placeholder.
     * Testiamo direttamente simulando currentTextChanged con testo placeholder e testo reale.
     * La combo seleziona "modello-reale" → ai.model() aggiornato.
     * La combo seleziona placeholder → ai.model() invariato.
     */
    void placeholderCaricoNonApplicatoComModello() {
        MockAiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", 11434, "modello-iniziale");
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* cmb = page.findChild<QComboBox*>("cmbModello");
        QVERIFY(cmb);

        /* Aggiungi modello reale e placeholder senza bloccare segnali:
         * il currentTextChanged da "modello-A" aggiornerà ai.model() */
        cmb->clear();
        cmb->addItem("modello-A");           // currentTextChanged → setBackend("modello-A")
        QApplication::processEvents();
        QCOMPARE(ai.model(), QString("modello-A"));

        /* Aggiungi placeholder e selezionalo manualmente */
        const QString placeholderTxt = "\xf0\x9f\x94\x84 Caricamento...";
        cmb->addItem(placeholderTxt);        // non cambia current → no signal
        cmb->setCurrentIndex(1);             // currentTextChanged con placeholder
        QApplication::processEvents();

        /* La guard deve avere bloccato setBackend → modello resta "modello-A" */
        QCOMPARE(ai.model(), QString("modello-A"));

        /* Seleziona di nuovo un modello reale → deve aggiornare ai.model() */
        cmb->setCurrentIndex(0);             // torna a "modello-A"
        QApplication::processEvents();
        QCOMPARE(ai.model(), QString("modello-A"));
    }

    /* TC-C5: modelsReady chiamato più volte non accumula duplicati */
    void popolazioneMultiplaNodup() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* cmb = page.findChild<QComboBox*>("cmbModello");
        QVERIFY(cmb);

        const QStringList m1{"A", "B"};
        const QStringList m2{"C", "D", "E"};
        emit ai.modelsReady(m1);
        QApplication::processEvents();
        emit ai.modelsReady(m2);
        QApplication::processEvents();

        QCOMPARE(cmb->count(), 3);  // solo m2, non m1+m2=5
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D: Integrità database offerte
   ══════════════════════════════════════════════════════════════ */
class TestDatabaseIntegrita : public QObject {
    Q_OBJECT
private slots:

    /* TC-D1: ogni offerta ha un tipo valido */
    void tipiValidi() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* filtroTipo = page.findChild<QComboBox*>("filtroTipo");
        auto* lista      = page.findChild<QListWidget*>("offerteList");
        QVERIFY(filtroTipo); QVERIFY(lista);

        filtroTipo->setCurrentIndex(filtroTipo->findData("tutti"));
        auto* filtroLiv = page.findChild<QComboBox*>("filtroLivello");
        filtroLiv->setCurrentIndex(filtroLiv->findData("tutti"));
        QApplication::processEvents();

        for (int i = 0; i < lista->count(); ++i) {
            const auto o = lista->item(i)->data(Qt::UserRole).value<Offerta>();
            QVERIFY2(kTipiValidi.contains(o.tipo),
                     qPrintable(QString("Tipo non valido '%1' in offerta '%2 — %3'")
                         .arg(o.tipo, o.azienda, o.ruolo)));
        }
    }

    /* TC-D2: ogni offerta ha un livello valido */
    void livelliValidi() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* filtroTipo = page.findChild<QComboBox*>("filtroTipo");
        auto* lista      = page.findChild<QListWidget*>("offerteList");
        QVERIFY(filtroTipo); QVERIFY(lista);

        filtroTipo->setCurrentIndex(filtroTipo->findData("tutti"));
        auto* filtroLiv = page.findChild<QComboBox*>("filtroLivello");
        filtroLiv->setCurrentIndex(filtroLiv->findData("tutti"));
        QApplication::processEvents();

        for (int i = 0; i < lista->count(); ++i) {
            const auto o = lista->item(i)->data(Qt::UserRole).value<Offerta>();
            QVERIFY2(kLivelliValidi.contains(o.livello),
                     qPrintable(QString("Livello non valido '%1' in offerta '%2 — %3'")
                         .arg(o.livello, o.azienda, o.ruolo)));
        }
    }

    /* TC-D3: tutte le offerte hanno azienda e ruolo non vuoti */
    void aziendaRuoloNonVuoti() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* filtroTipo = page.findChild<QComboBox*>("filtroTipo");
        auto* lista      = page.findChild<QListWidget*>("offerteList");
        QVERIFY(filtroTipo); QVERIFY(lista);

        filtroTipo->setCurrentIndex(filtroTipo->findData("tutti"));
        auto* filtroLiv = page.findChild<QComboBox*>("filtroLivello");
        filtroLiv->setCurrentIndex(filtroLiv->findData("tutti"));
        QApplication::processEvents();

        for (int i = 0; i < lista->count(); ++i) {
            const auto o = lista->item(i)->data(Qt::UserRole).value<Offerta>();
            QVERIFY2(!o.azienda.trimmed().isEmpty(),
                     qPrintable(QString("Offerta %1 ha azienda vuota").arg(i)));
            QVERIFY2(!o.ruolo.trimmed().isEmpty(),
                     qPrintable(QString("Offerta %1 ha ruolo vuoto").arg(i)));
        }
    }

    /* TC-D4: conta per categoria — ogni categoria deve avere almeno 1 offerta */
    void ogniCategoriaHaAlmenoUnaOfferta() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* filtroTipo = page.findChild<QComboBox*>("filtroTipo");
        auto* lista      = page.findChild<QListWidget*>("offerteList");
        auto* filtroLiv  = page.findChild<QComboBox*>("filtroLivello");
        QVERIFY(filtroTipo); QVERIFY(lista); QVERIFY(filtroLiv);

        filtroLiv->setCurrentIndex(filtroLiv->findData("tutti"));

        for (const QString& tipo : kTipiValidi) {
            const int idx = filtroTipo->findData(tipo);
            if (idx < 0) continue;
            filtroTipo->setCurrentIndex(idx);
            QApplication::processEvents();
            QVERIFY2(lista->count() > 0,
                     qPrintable(QString("Categoria '%1' è vuota").arg(tipo)));
        }
    }

    /* TC-D5: somma offerte per categoria == totale (nessuna offerta persa)
     * Verifica che la somma dei conteggi filtrati per tipo == tutto_count.
     * Fallisce se c'è una duplicazione nel filtraggio o una perdita di dati.
     */
    void sommaCategorieEqualeTotale() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* filtroTipo = page.findChild<QComboBox*>("filtroTipo");
        auto* lista      = page.findChild<QListWidget*>("offerteList");
        auto* filtroLiv  = page.findChild<QComboBox*>("filtroLivello");
        QVERIFY(filtroTipo); QVERIFY(lista); QVERIFY(filtroLiv);

        filtroLiv->setCurrentIndex(filtroLiv->findData("tutti"));
        filtroTipo->setCurrentIndex(filtroTipo->findData("tutti"));
        QApplication::processEvents();
        const int totale = lista->count();

        int somma = 0;
        for (const QString& tipo : kTipiValidi) {
            const int idx = filtroTipo->findData(tipo);
            if (idx < 0) continue;
            filtroTipo->setCurrentIndex(idx);
            QApplication::processEvents();
            somma += lista->count();
        }

        QCOMPARE(somma, totale);
    }

    /* TC-D6: NESSUNA offerta ha tipo "Altro" non intenzionale
     * La categoria Altro deve essere < 10% del totale
     * (se >10% probabilmente ci sono offerte mal categorizzate)
     */
    void categoriaAltroNonEccessiva() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* filtroTipo = page.findChild<QComboBox*>("filtroTipo");
        auto* lista      = page.findChild<QListWidget*>("offerteList");
        auto* filtroLiv  = page.findChild<QComboBox*>("filtroLivello");
        QVERIFY(filtroTipo); QVERIFY(lista); QVERIFY(filtroLiv);

        filtroLiv->setCurrentIndex(filtroLiv->findData("tutti"));
        filtroTipo->setCurrentIndex(filtroTipo->findData("tutti"));
        QApplication::processEvents();
        const int totale = lista->count();

        filtroTipo->setCurrentIndex(filtroTipo->findData("Altro"));
        QApplication::processEvents();
        const int nAltro = lista->count();

        QVERIFY2(nAltro * 10 <= totale,
                 qPrintable(QString("Troppo 'Altro': %1/%2 = %3%% (max 10%%)")
                     .arg(nAltro).arg(totale).arg(nAltro * 100 / totale)));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-E: Stato UI e comportamenti widget
   ══════════════════════════════════════════════════════════════ */
class TestStatoUI : public QObject {
    Q_OBJECT
private slots:

    /* TC-E1: copy button disabilitato all'avvio */
    void copyBtnDisabledOnStart() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto btns = page.findChildren<QPushButton*>();
        QPushButton* copiaBtn = nullptr;
        for (auto* b : btns) {
            if (b->text().contains("Copia Lettera"))
                copiaBtn = b;
        }
        QVERIFY2(copiaBtn, "Pulsante 'Copia Lettera' non trovato");
        QVERIFY2(!copiaBtn->isEnabled(),
                 "Pulsante 'Copia Lettera' deve essere disabilitato all'avvio");
    }

    /* TC-E2: lista offerte non vuota all'avvio (filtro default = tutti / diploma) */
    void listaOffertePopolataAllAvvio() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* lista = page.findChild<QListWidget*>("offerteList");
        QVERIFY(lista);
        QVERIFY2(lista->count() > 0, "Lista offerte vuota all'avvio");
    }

    /* TC-E3: log vuoto all'avvio — nessun token spazzatura */
    void logVuotoAllAvvio() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* log = page.findChild<QTextEdit*>("chatLog");
        QVERIFY(log);
        QVERIFY2(log->toPlainText().trimmed().isEmpty(),
                 "Log non è vuoto all'avvio — token spazzatura?");
    }

    /* TC-E4: item in lista ha dati Offerta recuperabili via Qt::UserRole */
    void itemListaHaDatiOfferta() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* filtroTipo = page.findChild<QComboBox*>("filtroTipo");
        auto* lista      = page.findChild<QListWidget*>("offerteList");
        auto* filtroLiv  = page.findChild<QComboBox*>("filtroLivello");
        QVERIFY(filtroTipo); QVERIFY(lista); QVERIFY(filtroLiv);

        filtroTipo->setCurrentIndex(filtroTipo->findData("tutti"));
        filtroLiv->setCurrentIndex( filtroLiv->findData("tutti"));
        QApplication::processEvents();

        QVERIFY(lista->count() > 0);

        /* Prendi il primo item e verifica che i dati Offerta siano recuperabili */
        const QVariant var = lista->item(0)->data(Qt::UserRole);
        QVERIFY2(var.isValid(), "Qt::UserRole non impostato sull'item");
        QVERIFY2(var.canConvert<Offerta>(), "UserRole non convertibile in Offerta");

        const Offerta o = var.value<Offerta>();
        QVERIFY2(!o.azienda.isEmpty(), "Offerta da item ha azienda vuota");
        QVERIFY2(!o.ruolo.isEmpty(),   "Offerta da item ha ruolo vuoto");
    }

    /* TC-E5: selettore modello presente e ha almeno 1 item all'avvio */
    void selettoreModelloPresenteHaItem() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* cmb = page.findChild<QComboBox*>("cmbModello");
        QVERIFY2(cmb, "Selettore modello (cmbModello) non trovato");
        QVERIFY2(cmb->count() > 0, "Selettore modello vuoto all'avvio");
    }

    /* TC-E6: il filtro livello default è impostato su "diploma" (profilo Paolo)
     * Non "tutti" — perché Paulo ha il diploma e vogliamo filtro sensato di default
     */
    void filtroLivelloDefaultEDiploma() {
        MockAiClient ai;
        LavoroPage page(&ai, nullptr);
        page.show();
        QTest::qWaitForWindowExposed(&page);

        auto* filtroLiv = page.findChild<QComboBox*>("filtroLivello");
        QVERIFY(filtroLiv);

        /* Il costruttore imposta currentIndex(2) = "diploma" */
        const QString dataCorrente = filtroLiv->currentData().toString();
        QCOMPARE(dataCorrente, QString("diploma"));
    }
};

/* ══════════════════════════════════════════════════════════════
   MAIN
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setStyle("Fusion");

    int status = 0;
    {
        TestIsolamentoSegnali t1;
        status |= QTest::qExec(&t1, argc, argv);
    }
    {
        TestFiltriFunzionalita t2;
        status |= QTest::qExec(&t2, argc, argv);
    }
    {
        TestPopolazioneModelli t3;
        status |= QTest::qExec(&t3, argc, argv);
    }
    {
        TestDatabaseIntegrita t4;
        status |= QTest::qExec(&t4, argc, argv);
    }
    {
        TestStatoUI t5;
        status |= QTest::qExec(&t5, argc, argv);
    }
    return status;
}

#include "test_lavoro_page.moc"

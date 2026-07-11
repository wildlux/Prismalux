/* ══════════════════════════════════════════════════════════════
   test_tool_dispatchers_d33.cpp — Copertura T-D33 (TODO.md) per i
   dispatcher dei function tool D-33, SENZA un modello reale.

   T-D33 chiede di verificare, "in chat reale con un modello tool-capable",
   che il modello scelga e chiami correttamente 7 tool. Questo si separa
   in due parti indipendenti:
     1. il MODELLO sceglie di chiamare il tool giusto — richiede un vero
        LLM in conversazione, fuori scope qui (non riproducibile in un
        sandbox senza rete/modello dedicato).
     2. il DISPATCHER produce il risultato corretto dato l'argomento
        strutturato che un modello tool-capable passerebbe — funzioni
        pure, verificabili chiamandole DIRETTAMENTE con gli stessi
        argomenti elencati in T-D33, esattamente come CAT-K/CAT-L hanno
        già fatto per le guardie zero-LLM.

   Questa suite copre SOLO la parte 2, per 6 dei 7 tool di T-D33
   (algoritmo/codice_fiscale/finanza_calcola/valida_documento/
   carta_astrale/converti). Esclusi: disegna_grafico (tocca la UI,
   già segnato "non verificabile in sandbox" in D-33/T-D33) e la
   scelta autonoma del modello (richiede un vero LLM).

   Tecnica: i dispatcher onTool*() sono privati in AgentiPage — stesso
   pattern "#define private public" già usato in test_matematica_page.cpp/
   test_lan_wan_core.cpp per accedere a metodi privati in questa unica
   translation unit.

   Valori attesi: per gli algoritmi puri (mcd/hanoi_passi/nqueens) e la
   conversione farina, calcolati/verificati indipendentemente. Per
   codice_fiscale/carta_astrale/iban/p.iva (algoritmi complessi già
   coperti da suite dedicate — PraticoFinanza 27 PASS, Astrale 31 PASS —
   e non riderivati qui) si confronta l'output del dispatcher con una
   chiamata DIRETTA alla stessa funzione sottostante già testata altrove:
   verifica che il WRAPPER inoltri correttamente gli argomenti, non che
   l'algoritmo sia corretto (già garantito da quelle suite).

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_tool_dispatchers_d33
     ./build_tests/test_tool_dispatchers_d33
   ══════════════════════════════════════════════════════════════ */

/* Rende accessibili i metodi private di AgentiPage in questo TU. */
#define private public
#define protected public
#include "../pages/main_ai.h"
#undef protected
#undef private

#include <QtTest/QtTest>
#include <QApplication>
#include <QJsonObject>
#include <QJsonDocument>
#include "mock_ai_client.h"
#include "../pages/pratico_calcs.h"
#include "../widgets/astro_calc.h"

/* Dichiarate in main_ai_p.h — usate qui per il confronto diretto */
bool _ibanValid(const QString& ibanRaw);

static QString jsonOf(std::initializer_list<QPair<QString, QJsonValue>> kv)
{
    QJsonObject o;
    for (const auto& p : kv) o.insert(p.first, p.second);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

class TestToolDispatchersD33 : public QObject {
    Q_OBJECT
private:
    MockAiClient* m_ai   = nullptr;
    AgentiPage*   m_page = nullptr;

    /* onTool*() sono callback-style: cattura il risultato in una variabile
       locale tramite lambda passata come onDone. */
    QString call(void (AgentiPage::*fn)(const QString&, const std::function<void(QString)>&),
                 const QString& input)
    {
        QString out;
        (m_page->*fn)(input, [&out](QString r) { out = r; });
        return out;
    }

private slots:
    void init() {
        m_ai   = new MockAiClient;
        m_page = new AgentiPage(m_ai);
    }
    void cleanup() {
        delete m_page; m_page = nullptr;
        delete m_ai;   m_ai   = nullptr;
    }

    /* T-D33 esempio 1: "calcola l'mcd tra 48 e 18 col tool" */
    void algoritmoMcd() {
        const QString r = call(&AgentiPage::onToolAlgoritmo,
            jsonOf({{"nome", "mcd"}, {"a", 48}, {"b", 18}}));
        QCOMPARE(r, QString("MCD(48,18) = 6"));
    }

    /* T-D33: hanoi_passi con 6 dischi -> 2^6-1 = 63 mosse */
    void algoritmoHanoiPassi() {
        const QString r = call(&AgentiPage::onToolAlgoritmo,
            jsonOf({{"nome", "hanoi_passi"}, {"n", 6}}));
        QVERIFY2(r.contains("63 mosse"), qPrintable(r));
    }

    /* T-D33: nqueens con N=8 -> 92 soluzioni (OEIS A000170, già verificato
       indipendentemente nel TODO stesso alla voce D-33 punto 8) */
    void algoritmoNQueens() {
        const QString r = call(&AgentiPage::onToolAlgoritmo,
            jsonOf({{"nome", "nqueens"}, {"n", 8}}));
        QVERIFY2(r.contains("92 soluzioni totali"), qPrintable(r));
    }

    /* T-D33: codice_fiscale(Rossi Mario, 01/05/1985, M, Roma) — confronta
       col risultato di PraticoCalcs::calcolaCodiceFiscale() chiamata
       direttamente (stessa funzione già coperta da PraticoFinanza,
       27 PASS): verifica che il wrapper inoltri bene gli argomenti. */
    void codiceFiscale() {
        const QString belfiore = PraticoCalcs::cercaBelfiore("Roma");
        QVERIFY2(!belfiore.isEmpty(), "Roma deve essere nell'elenco Belfiore locale");
        const QString atteso = PraticoCalcs::calcolaCodiceFiscale(
            "Rossi", "Mario", QDate(1985, 5, 1), /*maschio=*/true, belfiore);
        QVERIFY2(!atteso.isEmpty(), "calcolo diretto fallito — impossibile confrontare");

        const QString r = call(&AgentiPage::onToolCodiceFiscale, jsonOf({
            {"cognome", "Rossi"}, {"nome", "Mario"},
            {"data_nascita", "1985-05-01"}, {"sesso", "M"},
            {"comune_nascita", "Roma"},
        }));
        QVERIFY2(r.contains(atteso), qPrintable(QString("atteso '%1' in: %2").arg(atteso, r)));
    }

    /* T-D33: finanza_calcola mutuo 100000€ al 3% in 20 anni -> rata 554.60€
       (stesso valore già verificato indipendentemente in CAT-L/T-D14c) */
    void finanzaCalcolaMutuo() {
        const QString r = call(&AgentiPage::onToolFinanzaCalcola, jsonOf({
            {"tipo", "rata_mutuo"}, {"capitale", 100000}, {"tasso_annuo_pct", 3}, {"anni", 20},
        }));
        QVERIFY2(r.contains("554.60"), qPrintable(r));
    }

    /* T-D33: valida_documento per IBAN/P.IVA/CF — confronta col risultato
       delle funzioni pure già usate da _inject_finance (stesso helper,
       non riderivato): verifica solo che il wrapper riconosca il tipo e
       inoltri il valore correttamente. */
    void validaDocumentoIban() {
        const QString iban = "IT60X0542811101000000123456";
        const bool atteso = _ibanValid(iban);
        const QString r = call(&AgentiPage::onToolValidaDocumento,
            jsonOf({{"tipo", "iban"}, {"valore", iban}}));
        QCOMPARE(r.contains("NON VALIDO"), !atteso);
        QVERIFY2(r.contains(atteso ? "VALIDO" : "NON VALIDO"), qPrintable(r));
    }
    void validaDocumentoPiva() {
        /* Stesso esempio già usato in T-D14c/help table — P.IVA valida
           verificata indipendentemente con Python nella sessione precedente */
        const QString r = call(&AgentiPage::onToolValidaDocumento,
            jsonOf({{"tipo", "partita_iva"}, {"valore", "12345678903"}}));
        QVERIFY2(r.contains("VALIDA") && !r.contains("NON VALIDA"), qPrintable(r));
    }

    /* T-D33: carta_astrale(data+ora+lat/lon) — confronta con AstroCalc::compute()
       chiamata direttamente (stessa funzione, già coperta da 31 test nella
       suite Astrale): verifica solo che il wrapper parsi/inoltri bene i campi. */
    void cartaAstrale() {
        const auto atteso = AstroCalc::compute(2000, 1, 1, 12, 0, 41.9, 12.5);
        QVERIFY2(atteso.ok, "calcolo diretto AstroCalc fallito — impossibile confrontare");

        const QString r = call(&AgentiPage::onToolCartaAstrale, jsonOf({
            {"data", "2000-01-01"}, {"ora", "12:00"}, {"lat", 41.9}, {"lon", 12.5},
        }));
        QVERIFY2(!r.startsWith("errore"), qPrintable(r));
        QVERIFY2(r.contains("Ascendente"), qPrintable(r));
        QVERIFY2(r.contains("Medio Cielo"), qPrintable(r));
        /* Stesso numero di pianeti presenti nell'output diretto */
        for (const auto& pl : atteso.planets)
            QVERIFY2(r.contains(pl.name + ":"), qPrintable(QString("manca %1 in: %2").arg(pl.name, r)));
    }

    /* T-D33: converti("200 ml di farina in grammi") — densità farina 0.53,
       200 * 0.53 = 106 g (calcolato a mano) */
    void converti() {
        const QString r = call(&AgentiPage::onToolConverti,
            jsonOf({{"richiesta", "200 ml di farina in grammi"}}));
        QVERIFY2(r.contains("106"), qPrintable(r));
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestToolDispatchersD33 t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_tool_dispatchers_d33.moc"

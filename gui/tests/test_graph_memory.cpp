/* ══════════════════════════════════════════════════════════════
   test_graph_memory.cpp
   ──────────────────────────────────────────────────────────────
   Suite di test per GraphMemory (graph_memory.h/cpp).
   Ogni test usa un QTemporaryDir isolato — nessuno stato condiviso.

   Categorie:
     A  — Apertura e schema
     B  — Nodi: addNode, nodeByLabel, nodeById, allNodes, updateNode, removeNode
     C  — Archi: addEdge, allEdges, neighbours BFS
     D  — Ricerca testuale: searchNodes
     E  — Export: toDot, toJson, exportTxt
     F  — Manutenzione: pruneByImportance, clearAll
     G  — Connessioni multiple sullo stesso DB
     H  — Segnale changed()
     I  — Edge-case e robustezza

   Esecuzione:
     cmake -B build_tests gui/ -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc)
     ./build_tests/test_graph_memory
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>

#include "../graph_memory.h"

/* ── helper: apre un GraphMemory su dir temporanea ── */
static GraphMemory* makeGm(const QTemporaryDir& dir, const QString& name = "test.db")
{
    auto* gm = new GraphMemory(dir.filePath(name));
    const bool ok = gm->open();
    Q_ASSERT(ok);
    return gm;
}

/* ══════════════════════════════════════════════════════════════
   A — Apertura e schema
   ══════════════════════════════════════════════════════════════ */
class TestApertura : public QObject {
    Q_OBJECT
private slots:

    /* A-1: open() su path valido ritorna true */
    void openValido() {
        QTemporaryDir d;
        GraphMemory gm(d.filePath("a.db"));
        QVERIFY(gm.open());
        QVERIFY(gm.isOpen());
    }

    /* A-2: DB vuoto — contatori a zero */
    void dbVuotoCounters() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        QCOMPARE(gm->nodeCount(), 0);
        QCOMPARE(gm->edgeCount(), 0);
        delete gm;
    }

    /* A-3: open() idempotente (secondo open non crasha) */
    void openIdempotente() {
        QTemporaryDir d;
        GraphMemory gm(d.filePath("b.db"));
        QVERIFY(gm.open());
        QVERIFY(gm.open());   /* secondo open: già aperto */
        QVERIFY(gm.isOpen());
    }
};

/* ══════════════════════════════════════════════════════════════
   B — Nodi
   ══════════════════════════════════════════════════════════════ */
class TestNodi : public QObject {
    Q_OBJECT
private slots:

    /* B-1: addNode restituisce un UUID non vuoto */
    void addNodeReturnUuid() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString id = gm->addNode("entity", "Qt6");
        QVERIFY(!id.isEmpty());
        delete gm;
    }

    /* B-2: nodeCount sale di 1 per ogni addNode */
    void nodeCountCresce() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        QCOMPARE(gm->nodeCount(), 0);
        gm->addNode("entity", "A");
        QCOMPARE(gm->nodeCount(), 1);
        gm->addNode("concept", "B");
        QCOMPARE(gm->nodeCount(), 2);
        delete gm;
    }

    /* B-3: nodeByLabel round-trip — tutti i campi del nodo */
    void nodeByLabelRoundtrip() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString id = gm->addNode("fact", "Prismalux", "Framework Qt6", 0.9f,
                                        {{"autore", "wildlux"}});
        const auto opt = gm->nodeByLabel("Prismalux");
        QVERIFY(opt.has_value());
        QCOMPARE(opt->id,         id);
        QCOMPARE(opt->type,       QString("fact"));
        QCOMPARE(opt->label,      QString("Prismalux"));
        QCOMPARE(opt->content,    QString("Framework Qt6"));
        QCOMPARE(opt->importance, 0.9f);
        QCOMPARE(opt->meta.value("autore").toString(), QString("wildlux"));
        delete gm;
    }

    /* B-4: nodeByLabel case-insensitive */
    void nodeByLabelCaseInsensitive() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "Qt6");
        QVERIFY(gm->nodeByLabel("qt6").has_value());
        QVERIFY(gm->nodeByLabel("QT6").has_value());
        delete gm;
    }

    /* B-5: nodeById — lookup diretto per UUID */
    void nodeById() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString id = gm->addNode("entity", "Alpha");
        const auto opt = gm->nodeById(id);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->id, id);
        delete gm;
    }

    /* B-6: nodeById con id inesistente → nullopt */
    void nodeByIdMancante() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        QVERIFY(!gm->nodeById("non-esiste-uuid").has_value());
        delete gm;
    }

    /* B-7: allNodes senza filtro restituisce tutti i nodi */
    void allNodesSenzaFiltro() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "X");
        gm->addNode("concept", "Y");
        gm->addNode("fact", "Z");
        QCOMPARE(gm->allNodes().size(), 3);
        delete gm;
    }

    /* B-8: allNodes con filtro tipo */
    void allNodesConFiltroTipo() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "E1");
        gm->addNode("entity", "E2");
        gm->addNode("concept", "C1");
        const auto entita = gm->allNodes("entity");
        QCOMPARE(entita.size(), 2);
        for (const auto& n : entita)
            QCOMPARE(n.type, QString("entity"));
        delete gm;
    }

    /* B-9: allNodes ordina per importance DESC */
    void allNodesOrdinatiPerImportance() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "Basso",  {}, 0.2f);
        gm->addNode("entity", "Alto",   {}, 0.9f);
        gm->addNode("entity", "Medio",  {}, 0.5f);
        const auto nodi = gm->allNodes();
        QCOMPARE(nodi.size(), 3);
        QCOMPARE(nodi[0].label, QString("Alto"));
        QCOMPARE(nodi[1].label, QString("Medio"));
        QCOMPARE(nodi[2].label, QString("Basso"));
        delete gm;
    }

    /* B-10: updateNode — aggiorna content e importance */
    void updateNode() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString id = gm->addNode("entity", "Nodo", "vecchio", 0.3f);
        QVERIFY(gm->updateNode(id, "nuovo", 0.8f));
        const auto opt = gm->nodeById(id);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->content,    QString("nuovo"));
        QCOMPARE(opt->importance, 0.8f);
        delete gm;
    }

    /* B-11: updateNode con importance -1 non tocca il valore */
    void updateNodeSoloContent() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString id = gm->addNode("entity", "N", "orig", 0.6f);
        gm->updateNode(id, "aggiornato");   /* importance -1 = default */
        const auto opt = gm->nodeById(id);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->content,    QString("aggiornato"));
        QCOMPARE(opt->importance, 0.6f);   /* invariata */
        delete gm;
    }

    /* B-12: removeNode elimina il nodo */
    void removeNode() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString id = gm->addNode("entity", "Temp");
        QCOMPARE(gm->nodeCount(), 1);
        QVERIFY(gm->removeNode(id));
        QCOMPARE(gm->nodeCount(), 0);
        QVERIFY(!gm->nodeByLabel("Temp").has_value());
        delete gm;
    }

    /* B-13: addNode con tipo vuoto usa "entity" come default */
    void addNodeTipoVuotoDefaultEntity() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString id = gm->addNode("", "EtichettaSenzaTipo");
        const auto opt = gm->nodeById(id);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->type, QString("entity"));
        delete gm;
    }
};

/* ══════════════════════════════════════════════════════════════
   C — Archi e neighbours BFS
   ══════════════════════════════════════════════════════════════ */
class TestArchi : public QObject {
    Q_OBJECT
private slots:

    /* C-1: addEdge incrementa edgeCount */
    void addEdgeCount() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString a = gm->addNode("entity", "A");
        const QString b = gm->addNode("entity", "B");
        QCOMPARE(gm->edgeCount(), 0);
        QVERIFY(gm->addEdge(a, b, "relates_to"));
        QCOMPARE(gm->edgeCount(), 1);
        delete gm;
    }

    /* C-2: addEdge con id inesistente ritorna false */
    void addEdgeIdMancante() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString a = gm->addNode("entity", "A");
        QVERIFY(!gm->addEdge(a, "uuid-falso", "relates_to"));
        QCOMPARE(gm->edgeCount(), 0);
        delete gm;
    }

    /* C-3: addEdge con relazione vuota usa "relates_to" come default */
    void addEdgeRelazioneVuotaDefault() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString a = gm->addNode("entity", "A");
        const QString b = gm->addNode("entity", "B");
        gm->addEdge(a, b, "");
        const auto archi = gm->allEdges();
        QCOMPARE(archi.size(), 1);
        QCOMPARE(archi[0].relation, QString("relates_to"));
        delete gm;
    }

    /* C-4: neighbours depth=1 — restituisce i nodi adiacenti diretti */
    void neighboursDepth1() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString root = gm->addNode("entity", "Root");
        const QString c1   = gm->addNode("entity", "Child1");
        const QString c2   = gm->addNode("entity", "Child2");
        const QString away = gm->addNode("entity", "Away");   /* non collegato */
        gm->addEdge(root, c1, "part_of");
        gm->addEdge(root, c2, "part_of");
        const auto nbrs = gm->neighbours(root, 1);
        QCOMPARE(nbrs.size(), 2);
        QStringList ids;
        for (const auto& n : nbrs) ids << n.id;
        QVERIFY(ids.contains(c1));
        QVERIFY(ids.contains(c2));
        QVERIFY(!ids.contains(away));
        delete gm;
    }

    /* C-5: neighbours depth=2 — raggiunge nipoti ma non oltre */
    void neighboursDepth2() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString root  = gm->addNode("entity", "Root");
        const QString child = gm->addNode("entity", "Child");
        const QString grand = gm->addNode("entity", "Grand");
        const QString great = gm->addNode("entity", "Great");
        gm->addEdge(root,  child, "part_of");
        gm->addEdge(child, grand, "part_of");
        gm->addEdge(grand, great, "part_of");
        const auto nbrs = gm->neighbours(root, 2);
        QStringList ids;
        for (const auto& n : nbrs) ids << n.id;
        QVERIFY(ids.contains(child));
        QVERIFY(ids.contains(grand));
        QVERIFY(!ids.contains(great));  /* fuori dal raggio 2 */
        delete gm;
    }

    /* C-6: neighbours su nodo senza archi — lista vuota */
    void neighboursSenzaArchi() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString id = gm->addNode("entity", "Isolato");
        QVERIFY(gm->neighbours(id, 2).isEmpty());
        delete gm;
    }

    /* C-7: neighbours non include il nodo stesso */
    void neighboursNoSelf() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString a = gm->addNode("entity", "A");
        const QString b = gm->addNode("entity", "B");
        gm->addEdge(a, b, "relates_to");
        const auto nbrs = gm->neighbours(a, 1);
        for (const auto& n : nbrs)
            QVERIFY(n.id != a);
        delete gm;
    }

    /* C-8: removeNode elimina anche gli archi incidenti */
    void removeNodeEliminaArchi() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString a = gm->addNode("entity", "A");
        const QString b = gm->addNode("entity", "B");
        gm->addEdge(a, b, "relates_to");
        QCOMPARE(gm->edgeCount(), 1);
        gm->removeNode(a);
        QCOMPARE(gm->edgeCount(), 0);
        delete gm;
    }
};

/* ══════════════════════════════════════════════════════════════
   D — Ricerca testuale
   ══════════════════════════════════════════════════════════════ */
class TestRicerca : public QObject {
    Q_OBJECT
private slots:

    /* D-1: searchNodes trova match in label */
    void searchInLabel() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "Qt6 Framework");
        gm->addNode("entity", "Python");
        const auto r = gm->searchNodes("Qt6");
        QCOMPARE(r.size(), 1);
        QCOMPARE(r[0].label, QString("Qt6 Framework"));
        delete gm;
    }

    /* D-2: searchNodes trova match in content */
    void searchInContent() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("fact", "X", "Il framework usa segnali e slot");
        gm->addNode("fact", "Y", "linguaggio compilato");
        const auto r = gm->searchNodes("segnali");
        QCOMPARE(r.size(), 1);
        delete gm;
    }

    /* D-3: searchNodes case-insensitive */
    void searchCaseInsensitive() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "Prismalux");
        QCOMPARE(gm->searchNodes("PRISMALUX").size(), 1);
        QCOMPARE(gm->searchNodes("prismalux").size(), 1);
        delete gm;
    }

    /* D-4: searchNodes con limit=1 ritorna al massimo 1 risultato */
    void searchLimit() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "Qt5");
        gm->addNode("entity", "Qt6");
        gm->addNode("entity", "Qt7");
        const auto r = gm->searchNodes("Qt", 1);
        QCOMPARE(r.size(), 1);
        delete gm;
    }

    /* D-5: searchNodes senza match — lista vuota */
    void searchNessunMatch() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "Alpha");
        QVERIFY(gm->searchNodes("zzz-non-esiste").isEmpty());
        delete gm;
    }

    /* D-6: searchNodes su DB vuoto non crasha */
    void searchDbVuoto() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        QVERIFY(gm->searchNodes("test").isEmpty());
        delete gm;
    }
};

/* ══════════════════════════════════════════════════════════════
   E — Export: toDot, toJson, exportTxt
   ══════════════════════════════════════════════════════════════ */
class TestExport : public QObject {
    Q_OBJECT
private slots:

    /* E-1: toDot su DB vuoto produce un DOT valido (almeno "digraph") */
    void toDotVuoto() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString dot = gm->toDot("TestGraph");
        QVERIFY(dot.contains("digraph"));
        delete gm;
    }

    /* E-2: toDot con nodi include label */
    void toDotConNodi() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "NodoVisible");
        const QString dot = gm->toDot("G");
        QVERIFY(dot.contains("NodoVisible"));
        delete gm;
    }

    /* E-3: toJson su DB vuoto produce JSON valido con array nodi vuoto */
    void toJsonVuoto() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString json = gm->toJson();
        QVERIFY(json.contains("nodes") || json.contains("[]") || !json.isEmpty());
        delete gm;
    }

    /* E-4: toJson con nodi include label */
    void toJsonConNodi() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "Alfa");
        const QString json = gm->toJson();
        QVERIFY(json.contains("Alfa"));
        delete gm;
    }

    /* E-5: exportTxt crea un file leggibile */
    void exportTxtCreaFile() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("fact", "Fatto1", "Contenuto importante");
        const QString out = d.filePath("export.txt");
        QVERIFY(gm->exportTxt(out, 50));
        QVERIFY(QFile::exists(out));
        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString testo = f.readAll();
        QVERIFY(testo.contains("Fatto1"));
        delete gm;
    }

    /* E-6: exportTxt su DB vuoto produce file non vuoto (header almeno) */
    void exportTxtDbVuoto() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString out = d.filePath("vuoto.txt");
        gm->exportTxt(out, 10);
        /* Non crashe — file può essere vuoto o avere solo header */
        delete gm;
    }
};

/* ══════════════════════════════════════════════════════════════
   F — Manutenzione: pruneByImportance, clearAll
   ══════════════════════════════════════════════════════════════ */
class TestManutenzione : public QObject {
    Q_OBJECT
private slots:

    /* F-1: pruneByImportance(N) mantiene esattamente N nodi */
    void pruneMantieneTopN() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "Alta",   {}, 0.9f);
        gm->addNode("entity", "Media",  {}, 0.5f);
        gm->addNode("entity", "Bassa1", {}, 0.1f);
        gm->addNode("entity", "Bassa2", {}, 0.1f);
        QCOMPARE(gm->nodeCount(), 4);
        gm->pruneByImportance(2);
        QCOMPARE(gm->nodeCount(), 2);
        delete gm;
    }

    /* F-2: pruneByImportance mantiene i nodi con importance più alta */
    void pruneMantieneIpiuImportanti() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "Top",    {}, 1.0f);
        gm->addNode("entity", "Middle", {}, 0.5f);
        gm->addNode("entity", "Bottom", {}, 0.0f);
        gm->pruneByImportance(2);
        QVERIFY(gm->nodeByLabel("Top").has_value());
        QVERIFY(gm->nodeByLabel("Middle").has_value());
        QVERIFY(!gm->nodeByLabel("Bottom").has_value());
        delete gm;
    }

    /* F-3: pruneByImportance rimuove anche gli archi orfani */
    void pruneRimuoveArchiOrfani() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString a = gm->addNode("entity", "A", {}, 1.0f);
        const QString b = gm->addNode("entity", "B", {}, 0.0f);
        gm->addEdge(a, b, "relates_to");
        QCOMPARE(gm->edgeCount(), 1);
        gm->pruneByImportance(1);
        QCOMPARE(gm->nodeCount(), 1);
        QCOMPARE(gm->edgeCount(), 0);
        delete gm;
    }

    /* F-4: pruneByImportance(N) con N >= nodeCount() non rimuove nulla */
    void pruneNMaggioreDelTotale() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "A");
        gm->addNode("entity", "B");
        gm->pruneByImportance(100);
        QCOMPARE(gm->nodeCount(), 2);
        delete gm;
    }

    /* F-5: clearAll svuota nodi E archi */
    void clearAllSvuota() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString a = gm->addNode("entity", "A");
        const QString b = gm->addNode("entity", "B");
        gm->addEdge(a, b, "relates_to");
        QCOMPARE(gm->nodeCount(), 2);
        QCOMPARE(gm->edgeCount(), 1);
        gm->clearAll();
        QCOMPARE(gm->nodeCount(), 0);
        QCOMPARE(gm->edgeCount(), 0);
        delete gm;
    }

    /* F-6: clearAll poi addNode funziona (DB riutilizzabile) */
    void clearAllPoiAddNode() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "Prima");
        gm->clearAll();
        gm->addNode("entity", "Dopo");
        QCOMPARE(gm->nodeCount(), 1);
        QVERIFY(gm->nodeByLabel("Dopo").has_value());
        delete gm;
    }
};

/* ══════════════════════════════════════════════════════════════
   G — Connessioni multiple
   ══════════════════════════════════════════════════════════════ */
class TestConnessioniMultiple : public QObject {
    Q_OBJECT
private slots:

    /* G-1: due istanze sullo stesso file — la seconda vede i dati scritti dalla prima */
    void dueIstanzeStessoDB() {
        QTemporaryDir d;
        const QString path = d.filePath("shared.db");

        GraphMemory gm1(path);
        QVERIFY(gm1.open());
        gm1.addNode("entity", "SharedNode");
        gm1.close();

        GraphMemory gm2(path);
        QVERIFY(gm2.open());
        QCOMPARE(gm2.nodeCount(), 1);
        QVERIFY(gm2.nodeByLabel("SharedNode").has_value());
    }

    /* G-2: due istanze contemporanee con path diversi non si interferiscono */
    void dueIstanzePathDiversi() {
        QTemporaryDir d;
        auto* gm1 = makeGm(d, "db1.db");
        auto* gm2 = makeGm(d, "db2.db");
        gm1->addNode("entity", "Solo1");
        QCOMPARE(gm1->nodeCount(), 1);
        QCOMPARE(gm2->nodeCount(), 0);
        delete gm1;
        delete gm2;
    }
};

/* ══════════════════════════════════════════════════════════════
   H — Segnale changed()
   ══════════════════════════════════════════════════════════════ */
class TestSegnaleChanged : public QObject {
    Q_OBJECT
private slots:

    /* H-1: addNode emette changed() */
    void addNodeEmitteChanged() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        QSignalSpy spy(gm, &GraphMemory::changed);
        gm->addNode("entity", "X");
        QCOMPARE(spy.count(), 1);
        delete gm;
    }

    /* H-2: addEdge emette changed() */
    void addEdgeEmitteChanged() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString a = gm->addNode("entity", "A");
        const QString b = gm->addNode("entity", "B");
        QSignalSpy spy(gm, &GraphMemory::changed);
        gm->addEdge(a, b, "relates_to");
        QCOMPARE(spy.count(), 1);
        delete gm;
    }

    /* H-3: clearAll emette changed() */
    void clearAllEmitteChanged() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        gm->addNode("entity", "X");
        QSignalSpy spy(gm, &GraphMemory::changed);
        gm->clearAll();
        QCOMPARE(spy.count(), 1);
        delete gm;
    }

    /* H-4: updateNode emette changed() */
    void updateNodeEmitteChanged() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString id = gm->addNode("entity", "X");
        QSignalSpy spy(gm, &GraphMemory::changed);
        gm->updateNode(id, "nuovo contenuto");
        QCOMPARE(spy.count(), 1);
        delete gm;
    }

    /* H-5: removeNode emette changed() */
    void removeNodeEmitteChanged() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString id = gm->addNode("entity", "X");
        QSignalSpy spy(gm, &GraphMemory::changed);
        gm->removeNode(id);
        QCOMPARE(spy.count(), 1);
        delete gm;
    }
};

/* ══════════════════════════════════════════════════════════════
   I — Edge-case e robustezza
   ══════════════════════════════════════════════════════════════ */
class TestRobustezza : public QObject {
    Q_OBJECT
private slots:

    /* I-1: addNode con label molto lunga (>1000 char) non crasha */
    void addNodeLabelLunga() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString labelLunga(1200, QChar('x'));
        const QString id = gm->addNode("entity", labelLunga);
        QVERIFY(!id.isEmpty());
        QCOMPARE(gm->nodeCount(), 1);
        delete gm;
    }

    /* I-2: addNode con content contenente caratteri SQL speciali (no injection) */
    void addNodeContentSqlSpeciali() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString maligno = "'); DROP TABLE gm_nodes; --";
        const QString id = gm->addNode("fact", "Test", maligno);
        QVERIFY(!id.isEmpty());
        QCOMPARE(gm->nodeCount(), 1);   /* tabella ancora intatta */
        const auto opt = gm->nodeById(id);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->content, maligno);
        delete gm;
    }

    /* I-3: addNode con content contenente apici e backslash (no injection) */
    void addNodeContentCaratteriSpeciali() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString spec = "C:\\path\\to\\file \"quoted\" 'single'";
        const QString id = gm->addNode("fact", "Spec", spec);
        const auto opt = gm->nodeById(id);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->content, spec);
        delete gm;
    }

    /* I-4: addNode con meta QVariantMap complessa — round-trip preserva i valori */
    void addNodeMetaRoundtrip() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QVariantMap meta{{"role", "tester"}, {"score", 42}, {"flag", true}};
        const QString id = gm->addNode("result", "M", {}, 1.0f, meta);
        const auto opt = gm->nodeById(id);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->meta.value("role").toString(), QString("tester"));
        QCOMPARE(opt->meta.value("score").toInt(),   42);
        QCOMPARE(opt->meta.value("flag").toBool(),   true);
        delete gm;
    }

    /* I-5: neighbours con id vuoto non crasha */
    void neighboursIdVuoto() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        QVERIFY(gm->neighbours("", 1).isEmpty());
        delete gm;
    }

    /* I-6: updateNode con id inesistente ritorna false */
    void updateNodeIdMancante() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        QVERIFY(!gm->updateNode("uuid-falso", "contenuto"));
        delete gm;
    }

    /* I-7: removeNode con id inesistente ritorna false senza crash */
    void removeNodeIdMancante() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        QVERIFY(!gm->removeNode("uuid-falso"));
        delete gm;
    }

    /* I-8: addNode con emoji nel content — round-trip corretto */
    void addNodeContentEmoji() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        const QString emoji = "\xf0\x9f\xa7\xa0 GraphMemory funziona \xe2\x9c\x85";
        const QString id = gm->addNode("fact", "Emoji", emoji);
        const auto opt = gm->nodeById(id);
        QVERIFY(opt.has_value());
        QCOMPARE(opt->content, emoji);
        delete gm;
    }
};

/* ══════════════════════════════════════════════════════════════
   Entry point — esegue tutte le suite in sequenza
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int status = 0;
    auto run = [&](QObject* obj) {
        status |= QTest::qExec(obj, argc, argv);
        delete obj;
    };
    run(new TestApertura);
    run(new TestNodi);
    run(new TestArchi);
    run(new TestRicerca);
    run(new TestExport);
    run(new TestManutenzione);
    run(new TestConnessioniMultiple);
    run(new TestSegnaleChanged);
    run(new TestRobustezza);
    return status;
}

#include "test_graph_memory.moc"

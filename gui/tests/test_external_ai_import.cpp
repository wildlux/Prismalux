/* ══════════════════════════════════════════════════════════════
   test_external_ai_import.cpp — Unit test per widgets/external_ai_import.h

   Copre il parsing dei formati riconosciuti (OpenAI ChatGPT, Anthropic
   Claude, generico role/content) e i casi di errore (JSON non valido,
   formato non riconosciuto, file assente).

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc)
     ./build_tests/test_external_ai_import
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "../widgets/external_ai_import.h"

class TestExternalAiImport : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString writeJson(const QString& name, const QByteArray& content) {
        const QString path = m_dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return {};
        f.write(content);
        f.close();
        return path;
    }

private slots:

    /* ── CAT-A: formato generico {"messages":[...]} ── */
    void genericMessages_parsesRolesAndContent() {
        const QString path = writeJson("generic.json", R"({
            "title": "Chat di prova",
            "messages": [
                {"role":"user","content":"ciao"},
                {"role":"assistant","content":"risposta"},
                {"role":"system","content":"istruzioni di sistema"}
            ]
        })");

        QString fmt, err;
        auto convs = ExternalAiImport::parseFile(path, fmt, err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(convs.size(), 1);
        QCOMPARE(convs[0].messages.size(), 3);
        QCOMPARE(convs[0].messages[0].role, QString("user"));
        QCOMPARE(convs[0].messages[0].content, QString("ciao"));
        QCOMPARE(convs[0].messages[1].role, QString("pipeline"));  /* assistant → pipeline */
        QCOMPARE(convs[0].messages[2].role, QString("system"));
        QCOMPARE(convs[0].title, QString("Chat di prova"));
    }

    /* ── CAT-A: array bare di messaggi (senza wrapper "messages") ── */
    void genericBareArray_singleConversation() {
        const QString path = writeJson("bare.json", R"([
            {"role":"user","content":"domanda"},
            {"role":"assistant","content":"risposta lunga"}
        ])");

        QString fmt, err;
        auto convs = ExternalAiImport::parseFile(path, fmt, err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(convs.size(), 1);
        QCOMPARE(convs[0].messages.size(), 2);
        QVERIFY(fmt.contains("Generico"));
    }

    /* ── CAT-A: OpenAI ChatGPT — albero "mapping" con ramo unico ── */
    void openAiMapping_walksParentChainInOrder() {
        const QString path = writeJson("openai.json", R"([{
            "title": "Conversazione ChatGPT",
            "current_node": "n2",
            "mapping": {
                "n0": {"id":"n0", "parent": null, "message": null},
                "n1": {"id":"n1", "parent": "n0", "message": {
                    "author": {"role":"user"},
                    "content": {"content_type":"text", "parts":["Prima domanda"]},
                    "create_time": 1700000000
                }},
                "n2": {"id":"n2", "parent": "n1", "message": {
                    "author": {"role":"assistant"},
                    "content": {"content_type":"text", "parts":["Prima risposta"]},
                    "create_time": 1700000010
                }}
            }
        }])");

        QString fmt, err;
        auto convs = ExternalAiImport::parseFile(path, fmt, err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(fmt, QString("OpenAI (ChatGPT)"));
        QCOMPARE(convs.size(), 1);
        QCOMPARE(convs[0].title, QString("Conversazione ChatGPT"));
        QCOMPARE(convs[0].messages.size(), 2);
        QCOMPARE(convs[0].messages[0].role, QString("user"));
        QCOMPARE(convs[0].messages[0].content, QString("Prima domanda"));
        QCOMPARE(convs[0].messages[1].role, QString("pipeline"));
        QCOMPARE(convs[0].messages[1].content, QString("Prima risposta"));
    }

    /* ── CAT-A: Anthropic Claude — chat_messages con sender/text ── */
    void claudeChatMessages_parsesSenderText() {
        const QString path = writeJson("claude.json", R"([{
            "name": "Chat Claude",
            "chat_messages": [
                {"sender":"human", "text":"Ciao Claude"},
                {"sender":"assistant", "text":"Ciao! Come posso aiutarti?"}
            ]
        }])");

        QString fmt, err;
        auto convs = ExternalAiImport::parseFile(path, fmt, err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(fmt, QString("Anthropic (Claude)"));
        QCOMPARE(convs.size(), 1);
        QCOMPARE(convs[0].messages.size(), 2);
        QCOMPARE(convs[0].messages[0].role, QString("user"));   /* human → user */
        QCOMPARE(convs[0].messages[1].role, QString("pipeline"));
    }

    /* ── CAT-B: JSON non valido → errore, nessuna eccezione ── */
    void invalidJson_setsErrorMessage() {
        const QString path = writeJson("broken.json", "{not valid json,,,");
        QString fmt, err;
        auto convs = ExternalAiImport::parseFile(path, fmt, err);
        QVERIFY(convs.isEmpty());
        QVERIFY(!err.isEmpty());
    }

    /* ── CAT-B: formato non riconosciuto → errore esplicito, no fallback silenzioso ── */
    void unrecognizedFormat_setsErrorMessage() {
        const QString path = writeJson("unknown.json", R"({"foo":"bar","baz":42})");
        QString fmt, err;
        auto convs = ExternalAiImport::parseFile(path, fmt, err);
        QVERIFY(convs.isEmpty());
        QVERIFY(!err.isEmpty());
    }

    /* ── CAT-B: file assente → errore, nessun crash ── */
    void missingFile_setsErrorMessage() {
        QString fmt, err;
        auto convs = ExternalAiImport::parseFile(m_dir.filePath("non_esiste.json"), fmt, err);
        QVERIFY(convs.isEmpty());
        QVERIFY(!err.isEmpty());
    }

    /* ── CAT-B: messaggi con content vuoto vengono scartati ── */
    void emptyContent_skipped() {
        const QString path = writeJson("empty_content.json", R"({
            "messages": [
                {"role":"user","content":""},
                {"role":"user","content":"   "},
                {"role":"user","content":"testo valido"}
            ]
        })");
        QString fmt, err;
        auto convs = ExternalAiImport::parseFile(path, fmt, err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(convs.size(), 1);
        QCOMPARE(convs[0].messages.size(), 1);
        QCOMPARE(convs[0].messages[0].content, QString("testo valido"));
    }

    /* ── CAT-B: multi-conversazione ChatGPT (array con più elementi) ── */
    void openAiMultipleConversations_allImported() {
        const QString path = writeJson("openai_multi.json", R"([
            {
                "title": "Chat 1",
                "current_node": "a1",
                "mapping": {
                    "a0": {"id":"a0","parent":null,"message":null},
                    "a1": {"id":"a1","parent":"a0","message":{
                        "author":{"role":"user"},
                        "content":{"parts":["Domanda 1"]},
                        "create_time": 1700000000
                    }}
                }
            },
            {
                "title": "Chat 2",
                "current_node": "b1",
                "mapping": {
                    "b0": {"id":"b0","parent":null,"message":null},
                    "b1": {"id":"b1","parent":"b0","message":{
                        "author":{"role":"user"},
                        "content":{"parts":["Domanda 2"]},
                        "create_time": 1700000001
                    }}
                }
            }
        ])");
        QString fmt, err;
        auto convs = ExternalAiImport::parseFile(path, fmt, err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(convs.size(), 2);
        QCOMPARE(convs[0].title, QString("Chat 1"));
        QCOMPARE(convs[1].title, QString("Chat 2"));
    }
};

QTEST_MAIN(TestExternalAiImport)
#include "test_external_ai_import.moc"

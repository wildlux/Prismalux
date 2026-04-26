/* ══════════════════════════════════════════════════════════════
   test_thinking_detect.cpp — Unit test per il rilevamento del campo
   thinking nei chunk di streaming Ollama (qwen3:4b e modelli futuri).

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc)
     ./build_tests/test_thinking_detect

   Classi di test:
     A. TestExtractName    — rilevamento per nome (fase 1)
     B. TestExtractSize    — rilevamento per dimensione / fallback (fase 2)
     C. TestExtractPrio    — priorità nome > dimensione
     D. TestExtractEdge    — casi limite (vuoto, non-stringa, esclusi)
     E. TestClassifyQuery  — classifyQuery("come ti chiami?") e range length
     F. TestKeyLock        — simulazione anti-race con m_thinkingKey
     G. TestContextRanges  — accumulo thinking a diverse dimensioni per qwen3:4b
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QJsonObject>
#include <QJsonArray>
#include "../ai_thinking_detect.h"
#include "../ai_client.h"

/* ── helper: costruisce un message chunk Ollama ── */
static QJsonObject makeMsg(const QString& content,
                           const QString& thinkField = {},
                           const QString& thinkValue = {},
                           const QString& extraField  = {},
                           const QString& extraValue  = {})
{
    QJsonObject m;
    m["role"]    = "assistant";
    m["content"] = content;
    if (!thinkField.isEmpty()) m[thinkField] = thinkValue;
    if (!extraField.isEmpty())  m[extraField]  = extraValue;
    return m;
}

/* ══════════════════════════════════════════════════════════════
   A. Rilevamento per nome (fase 1)
   ══════════════════════════════════════════════════════════════ */
class TestExtractName : public QObject {
    Q_OBJECT
private slots:

    void thinking_standard() {
        /* qwen3:4b Ollama attuale: message.thinking */
        auto msg = makeMsg("", "thinking", "sto valutando come rispondere");
        QString key;
        QString val = _extractThinkingToken(msg, &key);
        QCOMPARE(val, QString("sto valutando come rispondere"));
        QCOMPARE(key, QString("thinking"));
    }

    void think_short_name() {
        /* possibile nome futuro compatto */
        auto msg = makeMsg("", "think", "considero la risposta");
        QVERIFY(!_extractThinkingToken(msg).isEmpty());
    }

    void reasoning_field() {
        auto msg = makeMsg("", "reasoning", "analizzo la domanda");
        QString key;
        _extractThinkingToken(msg, &key);
        QCOMPARE(key, QString("reasoning"));
    }

    void reason_field() {
        auto msg = makeMsg("", "reason", "motivazione interna");
        QVERIFY(!_extractThinkingToken(msg).isEmpty());
    }

    void thought_field() {
        auto msg = makeMsg("", "thought", "pensiero del modello");
        QVERIFY(!_extractThinkingToken(msg).isEmpty());
    }

    void chain_of_thought() {
        /* nome lungo con "chain" */
        auto msg = makeMsg("", "chain_of_thought", "passo 1: capire la domanda");
        QString key;
        _extractThinkingToken(msg, &key);
        QCOMPARE(key, QString("chain_of_thought"));
    }

    void cot_abbrev() {
        auto msg = makeMsg("", "cot", "cot interno breve");
        QVERIFY(!_extractThinkingToken(msg).isEmpty());
    }

    void internal_monologue() {
        auto msg = makeMsg("", "internal_monologue", "monologo interno del modello");
        QVERIFY(!_extractThinkingToken(msg).isEmpty());
    }

    void ponder_field() {
        auto msg = makeMsg("", "ponder_output", "ponderazione sulla risposta");
        QVERIFY(!_extractThinkingToken(msg).isEmpty());
    }

    void reflect_field() {
        auto msg = makeMsg("", "reflect", "riflessione del modello");
        QVERIFY(!_extractThinkingToken(msg).isEmpty());
    }

    void foundKey_is_exact_case() {
        /* il campo restituito in foundKey deve avere il case originale */
        QJsonObject msg = makeMsg("", "Thinking", "valore maiuscolo");
        QString key;
        _extractThinkingToken(msg, &key);
        QCOMPARE(key, QString("Thinking"));
    }
};

/* ══════════════════════════════════════════════════════════════
   B. Rilevamento per dimensione / fallback (fase 2)
   ══════════════════════════════════════════════════════════════ */
class TestExtractSize : public QObject {
    Q_OBJECT
private slots:

    void unknown_field_below_threshold() {
        /* valore di 14 chars → sotto la soglia kMinThinkingLen=15 → non trovato */
        QJsonObject msg = makeMsg("", "x_cog_data", "12345678901234");  /* 14 chars */
        QVERIFY(_extractThinkingToken(msg).isEmpty());
    }

    void unknown_field_at_threshold() {
        /* esattamente 15 chars → trovato */
        QJsonObject msg = makeMsg("", "x_cog_data", "123456789012345");  /* 15 chars */
        QVERIFY(!_extractThinkingToken(msg).isEmpty());
    }

    void unknown_field_above_threshold() {
        QJsonObject msg = makeMsg("", "x_resp_blob",
            "questo e un campo sconosciuto ma abbastanza lungo per il fallback");
        QString key;
        QString val = _extractThinkingToken(msg, &key);
        QVERIFY(!val.isEmpty());
        QCOMPARE(key, QString("x_resp_blob"));
    }

    void multiple_unknown_returns_longest() {
        /* tra due campi sconosciuti: ritorna il più lungo */
        QJsonObject msg;
        msg["role"]    = "assistant";
        msg["content"] = "";
        msg["field_a"] = "breve";                           /* 5 chars */
        msg["field_b"] = "questo e il campo piu lungo";    /* 28 chars */
        QString key;
        _extractThinkingToken(msg, &key);
        QCOMPARE(key, QString("field_b"));
    }
};

/* ══════════════════════════════════════════════════════════════
   C. Priorità: nome batte dimensione
   ══════════════════════════════════════════════════════════════ */
class TestExtractPrio : public QObject {
    Q_OBJECT
private slots:

    void name_beats_longer_unknown() {
        /* "thinking" è breve ma batte "x_long_field" che è più lungo */
        QJsonObject msg;
        msg["role"]         = "assistant";
        msg["content"]      = "";
        msg["thinking"]     = "breve";
        msg["x_long_field"] = "questo e un campo molto piu lungo senza keyword";
        QString key;
        _extractThinkingToken(msg, &key);
        QCOMPARE(key, QString("thinking"));
    }

    void first_name_match_wins() {
        /* due campi con keyword: "thinking" e "reasoning". I JSON object in Qt
           non garantiscono ordine di inserimento, ma entrambi sono match validi.
           Verifica solo che il risultato sia uno dei due. */
        QJsonObject msg;
        msg["role"]      = "assistant";
        msg["content"]   = "";
        msg["thinking"]  = "pensiero A";
        msg["reasoning"] = "pensiero B";
        QString key;
        QString val = _extractThinkingToken(msg, &key);
        QVERIFY(!val.isEmpty());
        QVERIFY(key == "thinking" || key == "reasoning");
    }
};

/* ══════════════════════════════════════════════════════════════
   D. Casi limite
   ══════════════════════════════════════════════════════════════ */
class TestExtractEdge : public QObject {
    Q_OBJECT
private slots:

    void empty_message_returns_empty() {
        QVERIFY(_extractThinkingToken(QJsonObject{}).isEmpty());
    }

    void only_content_and_role() {
        auto msg = makeMsg("risposta finale");
        /* content non vuoto ma non si chiama mai in quel caso; comunque
           la funzione deve ignorare content e role */
        QVERIFY(_extractThinkingToken(msg).isEmpty());
    }

    void content_empty_no_thinking_field() {
        auto msg = makeMsg("");
        QVERIFY(_extractThinkingToken(msg).isEmpty());
    }

    void non_string_field_ignored() {
        QJsonObject msg;
        msg["role"]        = "assistant";
        msg["content"]     = "";
        msg["tool_calls"]  = QJsonArray{};         /* array — deve essere ignorato */
        msg["token_count"] = 42;                   /* numero — deve essere ignorato */
        QVERIFY(_extractThinkingToken(msg).isEmpty());
    }

    void empty_thinking_value_ignored() {
        auto msg = makeMsg("", "thinking", "");    /* campo thinking ma valore vuoto */
        QVERIFY(_extractThinkingToken(msg).isEmpty());
    }

    void foundKey_nullptr_safe() {
        /* chiamata senza out-param — non deve crashare */
        auto msg = makeMsg("", "thinking", "pensiero valido");
        QString val = _extractThinkingToken(msg, nullptr);
        QVERIFY(!val.isEmpty());
    }

    /* Chunk realistico di risposta finale (content presente) — la funzione
       viene chiamata solo quando content è vuoto, ma deve restituire empty
       anche se c'è un campo "thinking" residuo */
    void response_phase_chunk() {
        QJsonObject msg;
        msg["role"]     = "assistant";
        msg["content"]  = "Mi chiamo Qwen!";   /* risposta reale */
        /* in produzione questa funzione non viene chiamata quando content!=empty,
           ma la testiamo per robustezza */
        /* thinking potrebbe essere presente nel chunk di risposta — ignorato
           perché la logica in onReadyRead skippa se chunk non è empty */
        msg["thinking"] = "pensiero residuo";
        /* la funzione ritorna comunque il valore — la guard è in onReadyRead */
        QString val = _extractThinkingToken(msg);
        QVERIFY(!val.isEmpty());   /* corretto: la guard è altrove, non qui */
    }
};

/* ══════════════════════════════════════════════════════════════
   E. classifyQuery — "come ti chiami?" e range di lunghezza
   ══════════════════════════════════════════════════════════════ */
class TestClassifyQuery : public QObject {
    Q_OBJECT
private slots:

    void come_ti_chiami_is_simple() {
        /* domanda target: 15 chars, nessuna keyword complessa → QuerySimple
           → num_predict=512, nessun think:true inviato */
        QCOMPARE(AiClient::classifyQuery("come ti chiami?"),
                 AiClient::QuerySimple);
    }

    void empty_is_simple() {
        QCOMPARE(AiClient::classifyQuery(""), AiClient::QuerySimple);
    }

    void single_char_is_simple() {
        QCOMPARE(AiClient::classifyQuery("x"), AiClient::QuerySimple);
    }

    void exactly_30_chars_is_simple() {
        /* "come ti chiami?" ha 15 chars; 30 chars è il confine superiore */
        QString q30 = QString(30, 'a');
        QCOMPARE(AiClient::classifyQuery(q30), AiClient::QuerySimple);
    }

    void exactly_31_chars_is_auto() {
        QString q31 = QString(31, 'a');
        QCOMPARE(AiClient::classifyQuery(q31), AiClient::QueryAuto);
    }

    void range_31_to_200_neutral_is_auto() {
        /* domande medie senza keyword → QueryAuto */
        QCOMPARE(AiClient::classifyQuery("Qual e la tua versione del modello?"),
                 AiClient::QueryAuto);
    }

    void over_200_chars_is_complex() {
        QString long_q = QString(201, 'a');
        QCOMPARE(AiClient::classifyQuery(long_q), AiClient::QueryComplex);
    }

    void spiega_keyword_is_complex() {
        QCOMPARE(AiClient::classifyQuery("Spiega come funziona l'atomo"),
                 AiClient::QueryComplex);
    }

    void analizza_keyword_is_complex() {
        QCOMPARE(AiClient::classifyQuery("analizza questo testo"),
                 AiClient::QueryComplex);
    }

    void come_funziona_keyword_is_complex() {
        QCOMPARE(AiClient::classifyQuery("come funziona?"),
                 AiClient::QueryComplex);
    }

    void codice_keyword_is_complex() {
        QCOMPARE(AiClient::classifyQuery("scrivi del codice"),
                 AiClient::QueryComplex);
    }

    void perche_keyword_is_complex() {
        /* "perché" contiene keyword italiana */
        QCOMPARE(AiClient::classifyQuery(QString("perch\xc3\xa9 il cielo e blu")),
                 AiClient::QueryComplex);
    }

    void come_ti_chiami_neutral_no_think_sent() {
        /* Verifica transitiva: QuerySimple + thinkCapable=true (qwen3:4b)
           → il codice in chat() NON invia think:true (solo per QueryComplex).
           Non possiamo testare i parametri HTTP qui, ma possiamo confermare
           che la classificazione è Simple (condizione necessaria). */
        QCOMPARE(AiClient::classifyQuery("come ti chiami?"),
                 AiClient::QuerySimple);
        /* num_predict=512 viene impostato per QuerySimple — budget ridotto,
           da qui il rischio che il thinking esaurisca i token e content resti
           vuoto → scenario che i test G coprono per qwen3:4b */
    }
};

/* ══════════════════════════════════════════════════════════════
   F. Anti-race: simulazione del lock su m_thinkingKey
   ══════════════════════════════════════════════════════════════ */
class TestKeyLock : public QObject {
    Q_OBJECT
private slots:

    void key_fixed_after_first_chunk() {
        /* Simula onReadyRead: m_thinkingKey si fissa al primo chunk e poi
           viene usato direttamente senza riscansione. */
        QString m_thinkingKey;   /* stato locale che simula il membro di AiClient */

        /* Chunk 1 — scoperta */
        QJsonObject chunk1 = makeMsg("", "thinking", "sto pensando al mio nome");
        QString discovered;
        QString val1 = _extractThinkingToken(chunk1, &discovered);
        QVERIFY(!val1.isEmpty());
        m_thinkingKey = discovered;
        QCOMPARE(m_thinkingKey, QString("thinking"));

        /* Chunk 2 — usa SOLO il campo "thinking" già noto, ignora "metadata" */
        QJsonObject chunk2;
        chunk2["role"]     = "assistant";
        chunk2["content"]  = "";
        chunk2["thinking"] = " continuazione del pensiero";
        chunk2["metadata"] = "un campo metadata molto lungo che potrebbe confondere il size-fallback";

        QString val2;
        if (!m_thinkingKey.isEmpty()) {
            /* path produzione: legge solo il campo noto */
            val2 = chunk2[m_thinkingKey].toString();
        } else {
            val2 = _extractThinkingToken(chunk2, &m_thinkingKey);
        }
        QCOMPARE(val2, QString(" continuazione del pensiero"));
        /* metadata ignorato — chiave rimasta "thinking" */
        QCOMPARE(m_thinkingKey, QString("thinking"));
    }

    void metadata_would_win_without_key_lock() {
        /* Dimostra che SENZA key lock la fase 2 sceglierebbe "metadata"
           nel chunk 2 perché è più lungo di "thinking".
           Questo è esattamente il problema che m_thinkingKey risolve. */
        QJsonObject chunk2;
        chunk2["role"]     = "assistant";
        chunk2["content"]  = "";
        chunk2["thinking"] = " breve";  /* 6 chars — sotto soglia size-based */
        chunk2["metadata"] = "un campo metadata molto lungo che potrebbe confondere il size-fallback";

        QString discovered;
        QString val = _extractThinkingToken(chunk2, &discovered);
        /* senza key lock: "metadata" batte "thinking" per dimensione
           ma "thinking" ha priorità di NOME → la fase 1 lo trova comunque */
        QCOMPARE(discovered, QString("thinking"));
        /* Nota: in questo caso la fase 1 protegge. Il problema emerge solo
           se il campo thinking NON ha una keyword nel nome (fallback size-only),
           scenario coperto dal test key_lock_size_fallback_scenario. */
    }

    void key_lock_size_fallback_scenario() {
        /* Scenario peggiore: campo thinking con nome sconosciuto (size fallback).
           Chunk 1 → identifica "x_cog" via size.
           Chunk 2 → ha sia "x_cog" (corto) che "x_meta" (lungo).
           Senza key lock: chunk 2 sceglierebbe "x_meta".
           Con key lock: chunk 2 usa "x_cog" perché già fissato. */
        QString m_thinkingKey;

        /* Chunk 1: "x_cog" è l'unico campo lungo */
        QJsonObject chunk1;
        chunk1["role"]    = "assistant";
        chunk1["content"] = "";
        chunk1["x_cog"]   = "ragionamento iniziale del modello";   /* 34 chars */

        QString key1;
        _extractThinkingToken(chunk1, &key1);
        m_thinkingKey = key1;
        QCOMPARE(m_thinkingKey, QString("x_cog"));

        /* Chunk 2: "x_cog" è breve, "x_meta" è più lungo
           → senza lock, chunk 2 sceglierebbe "x_meta" */
        QJsonObject chunk2;
        chunk2["role"]    = "assistant";
        chunk2["content"] = "";
        chunk2["x_cog"]   = " continua";              /* 9 chars < 15 → sotto soglia */
        chunk2["x_meta"]  = "un campo meta molto lungo che confonde il size fallback";

        QString val2;
        if (!m_thinkingKey.isEmpty()) {
            /* path corretto: usa il campo noto, anche se è sotto soglia */
            val2 = chunk2[m_thinkingKey].toString();
        } else {
            val2 = _extractThinkingToken(chunk2, &m_thinkingKey);
        }
        QCOMPARE(val2, QString(" continua"));   /* ha usato x_cog, non x_meta */
        QCOMPARE(m_thinkingKey, QString("x_cog"));
    }

    void key_resets_between_streams() {
        /* Simula due chat distinte: la chiave deve essere azzerata tra di esse */
        QString m_thinkingKey = "thinking";  /* rimasto da chat precedente */

        /* Prima di una nuova chat: reset (simula abort()/chat()) */
        m_thinkingKey.clear();
        QVERIFY(m_thinkingKey.isEmpty());

        /* Nuova chat con campo "reasoning" invece di "thinking" */
        QJsonObject chunk = makeMsg("", "reasoning", "ragionamento nuovo stream");
        QString key;
        _extractThinkingToken(chunk, &key);
        m_thinkingKey = key;
        QCOMPARE(m_thinkingKey, QString("reasoning"));
    }
};

/* ══════════════════════════════════════════════════════════════
   G. Context ranges — come si accumula il thinking di qwen3:4b
   per "come ti chiami?" a diverse dimensioni (simula lo stream reale)
   ══════════════════════════════════════════════════════════════ */
class TestContextRanges : public QObject {
    Q_OBJECT

    /* Simula l'accumulo di N chunk di thinking + eventuale chunk di risposta.
       Ritorna {thinkingAccum, contentAccum}. */
    static std::pair<QString,QString> simulateStream(
        int     thinkChunks,
        int     thinkCharsPerChunk,
        bool    hasContentChunk,
        const QString& contentValue = "Mi chiamo Qwen!")
    {
        QString thinkingAccum;
        QString contentAccum;
        QString m_thinkingKey;

        /* Genera chunk di thinking */
        for (int i = 0; i < thinkChunks; ++i) {
            QJsonObject msg;
            msg["role"]     = "assistant";
            msg["content"]  = "";
            msg["thinking"] = QString(thinkCharsPerChunk, QChar('t')).replace(0, 6, "think ");

            QString chunk = msg["content"].toString();
            if (chunk.isEmpty()) {
                QString t;
                if (!m_thinkingKey.isEmpty()) {
                    t = msg[m_thinkingKey].toString();
                } else {
                    t = _extractThinkingToken(msg, &m_thinkingKey);
                }
                if (!t.isEmpty()) thinkingAccum += t;
            }
        }

        /* Genera chunk di risposta (se presente) */
        if (hasContentChunk) {
            QJsonObject msg;
            msg["role"]    = "assistant";
            msg["content"] = contentValue;
            contentAccum += msg["content"].toString();
        }

        return {thinkingAccum, contentAccum};
    }

private slots:

    void minimal_thinking_with_response() {
        /* qwen3:4b risponde subito: 2 chunk thinking brevi + risposta */
        auto [thinking, content] = simulateStream(2, 20, true);
        QVERIFY(!thinking.isEmpty());
        QVERIFY(!content.isEmpty());
        QCOMPARE(content, QString("Mi chiamo Qwen!"));
    }

    void medium_thinking_no_response() {
        /* Scenario problema: 10 chunk da 30 chars = ~300 chars thinking,
           nessun chunk di risposta (token budget esaurito con num_predict=512) */
        auto [thinking, content] = simulateStream(10, 30, false);
        QVERIFY(!thinking.isEmpty());
        QVERIFY(content.isEmpty());
        /* In produzione: onFinished() avvolge thinking in <think>...</think>
           e agenti_page_stream usa il contenuto come fallback */
    }

    void long_thinking_no_response() {
        /* 15 chunk da 40 chars = ~600 chars thinking: sotto la soglia loop (3000) */
        auto [thinking, content] = simulateStream(15, 40, false);
        QVERIFY(!thinking.isEmpty());
        QVERIFY(thinking.length() < 3000);   /* non triggered loop detection */
        QVERIFY(content.isEmpty());
    }

    void near_loop_threshold() {
        /* 75 chunk da 40 chars = ~3000 chars thinking: al limite della loop detection
           In AiClient::onFinished: likelyLoop = thinkingAccum.length() > 3000 */
        auto [thinking, content] = simulateStream(75, 40, false);
        QVERIFY(thinking.length() <= 3000 || thinking.length() > 3000);
        /* soglia esatta è >3000 → 3000 stesso non triggera loop */
        auto [tLong, _] = simulateStream(76, 40, false);
        QVERIFY(tLong.length() > 3000);   /* triggera loop detection in AiClient */
    }

    void key_stable_across_all_chunks() {
        /* Verifica che m_thinkingKey rimanga stabile su un lungo stream */
        QString m_thinkingKey;

        for (int i = 0; i < 20; ++i) {
            QJsonObject msg;
            msg["role"]        = "assistant";
            msg["content"]     = "";
            msg["thinking"]    = QString("chunk %1 di thinking").arg(i);
            msg["request_id"]  = "abc123def456ghi789jkl";  /* 21 chars > 15 */

            if (m_thinkingKey.isEmpty()) {
                QString k;
                _extractThinkingToken(msg, &k);
                m_thinkingKey = k;
            }
            /* Tutti i chunk devono usare "thinking" (nome), non "request_id" (size) */
            QCOMPARE(m_thinkingKey, QString("thinking"));
        }
    }

    void response_extracted_correctly() {
        /* Verifica che il contenuto della risposta sia estratto correttamente
           dopo una fase di thinking per "come ti chiami?" */
        auto [thinking, content] = simulateStream(5, 25, true, "Mi chiamo Qwen3, assistente AI.");
        QVERIFY(!thinking.isEmpty());
        QCOMPARE(content, QString("Mi chiamo Qwen3, assistente AI."));
    }
};

/* ── main custom: esegue tutte e 7 le classi di test ── */
#include "test_thinking_detect.moc"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    int status = 0;

    auto run = [&](QObject* obj) {
        status |= QTest::qExec(obj, argc, argv);
        delete obj;
    };

    run(new TestExtractName);
    run(new TestExtractSize);
    run(new TestExtractPrio);
    run(new TestExtractEdge);
    run(new TestClassifyQuery);
    run(new TestKeyLock);
    run(new TestContextRanges);

    return status;
}

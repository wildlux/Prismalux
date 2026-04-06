#pragma once
/* ══════════════════════════════════════════════════════════════
   MockAiClient — Versione controllabile di AiClient per i test.

   Sostituisce le chiamate di rete con emissioni manuali dei segnali,
   permettendo di simulare scenari di streaming in modo deterministico.

   Tipologia bug testati:
     - Dangling Observer (doppia connessione token)
     - Signal Leakage (token ricevuti da componenti non attivi)
     - Invariant Violation (stato logico ≠ stato visivo)
   ══════════════════════════════════════════════════════════════ */
#include "../ai_client.h"

class MockAiClient : public AiClient {
    Q_OBJECT
public:
    explicit MockAiClient(QObject* parent = nullptr)
        : AiClient(parent)
    {
        /* Configura un backend dummy per evitare assert interni */
        setBackend(AiClient::Ollama, "127.0.0.1", 11434, "test-model");
    }

    /* ── Controllo manuale dello streaming ──────────────────── */

    /** Emette un singolo token — simula un chunk di streaming. */
    void emitToken(const QString& tok) { emit token(tok); }

    /** Emette finished — simula il completamento di una risposta. */
    void emitFinished(const QString& full = "done") { emit finished(full); }

    /** Emette error — simula un errore di rete. */
    void emitError(const QString& msg) { emit error(msg); }

    /** Sequenza completa: N token identici → finished.
     *  Ritorna il testo totale atteso (concatenazione token). */
    QString emitFullResponse(const QStringList& tokens) {
        QString full;
        for (const QString& t : tokens) { emit token(t); full += t; }
        emit finished(full);
        return full;
    }

    /* ── Override chat: non fa nulla (no rete) ─────────────── */
    /* Non possiamo sovrascrivere chat() direttamente perché non è virtual.
       Usiamo invece emitToken/emitFinished dopo chat() per simulare la risposta.
       La chat() reale non viene chiamata nei test: settiamo busy=false via abort(). */

    /** Resetta lo stato busy senza avviare reti — per poter chiamare chat() nei test. */
    void resetBusy() { abort(); }
};

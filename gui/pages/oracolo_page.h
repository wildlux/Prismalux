#pragma once
#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include <QPointF>
#include "ai_client.h"
#include "rag_engine.h"

class ChatBubble;
class ChartWidget;

/* ══════════════════════════════════════════════════════════════
   OracoloPage — Chat diretta con un singolo LLM
   ════════════════════════════════════════════════════════════
   Layout (dall'alto verso il basso):
   1. ScrollArea  — bolle chat (stretch=1)
   2. WaitLabel   — "⏳ elaborazione..."  (nascosta di default)
   3. InputRow    — textarea + Invia + Nascondi
   4. QuickBar    — pillole azione rapida (toggle con Nascondi)

   Funzionalità:
   • ChatBubble utente (destra) / AI (sinistra)
   • Pulsanti 📋 Copia e 🔊 Leggi dentro ogni bolla
   • Pulsante 📊 Grafico per formule rilevate automaticamente
   • Pulsante 📷 Immagine — allega per LLM vision
   • Pillole rapide: Matematica / Finanza / Sviluppo / Grafico /
     Sinusoide 3D / Pulisci / Muto / Test Voce / Grafici / Aiuto
   ══════════════════════════════════════════════════════════════ */
class OracoloPage : public QWidget {
    Q_OBJECT
public:
    explicit OracoloPage(AiClient* ai, QWidget* parent = nullptr);

signals:
    /** Chiede a MainWindow di mostrare un grafico nel tab Grafico (Alt+3).
     *  Se @p formula è non vuota → cartesiano y=f(x) su [xMin, xMax].
     *  Se @p formula è vuota e @p points non è vuoto → scatter di punti. */
    void requestShowInGrafico(QString formula, double xMin, double xMax,
                              QVector<QPointF> points);

private slots:
    void sendMessage();
    void attachImage();
    void _setSendBusy(bool busy);

private:

    /* ── Gestione bubble ── */
    ChatBubble* addUserBubble(const QString& text);
    ChatBubble* addAIBubble(const QString& senderName);
    void        onChartRequested(ChatBubble* bubble, const QString& formula);
    void        showCartesianChart(const QVector<QPointF>& points);
    void        clearChat();

    /* ── Scroll ── */
    void scrollToBottom();

    /* ── AI Client ── */
    AiClient* m_ai = nullptr;

    /* ── Vista chat ── */
    QScrollArea* m_scroll         = nullptr;
    QWidget*     m_chatContainer  = nullptr;
    QVBoxLayout* m_chatLay        = nullptr;
    ChatBubble*  m_activeBubble   = nullptr;

    /* ── System prompt (collassabile) ── */
    QTextEdit* m_sysEdit = nullptr;

    /* ── Area input ── */
    QPlainTextEdit* m_input       = nullptr;
    QPushButton*    m_btnSend     = nullptr;  ///< bottone unico invia↔stop
    QPushButton*    m_btnNascondi = nullptr;  ///< mostra/nascondi quick bar
    QPushButton*    m_btnSys      = nullptr;  ///< mostra/nascondi system prompt
    QPushButton*    m_btnImg      = nullptr;
    QLabel*         m_waitLbl     = nullptr;

    /* ── Quick bar (pillole azione rapida) ── */
    QWidget*     m_quickBar   = nullptr;
    QPushButton* m_btnMute    = nullptr;   ///< toggle muto TTS
    bool         m_muted      = false;

    /* ── Immagine allegata ── */
    QByteArray  m_pendingImg;
    QString     m_pendingImgMime;
    QLabel*     m_imgPreview = nullptr;

    /* ── RAG ────────────────────────────────────────────────────── */
    RagEngine    m_rag;           ///< indice RAG con JLT
    bool         m_ragEnabled  = false;
    QString      m_pendingMsg;    ///< messaggio sospeso in attesa embedding RAG
    QPushButton* m_btnRag      = nullptr;
    QLabel*      m_ragLbl      = nullptr;  ///< "📚 N chunk"

    void startChatWithContext(const QString& userMsg);
    void updateRagBtn();

    /* ── Storia conversazione compressa ─────────────────────────────
       Strategia: manteniamo gli ultimi 3 turni completi in m_history.
       I turni più vecchi vengono compressi in m_historySummary (testo
       sintetico locale, senza chiamate AI extra).
       Vantaggi: il modello ha contesto senza mandare decine di turni raw.
       ─────────────────────────────────────────────────────────────── */
    struct ConvTurn { QString user; QString assistant; };
    QVector<ConvTurn> m_history;         ///< ultimi kMaxRecentTurns turni
    QString           m_historySummary;  ///< turni vecchi compressi in testo
    QString           m_lastUserMsg;     ///< messaggio utente corrente (per storia)

    static constexpr int kMaxRecentTurns = 3;  ///< turni completi da tenere raw

    /** Aggiunge un turno alla storia e comprime se supera kMaxRecentTurns. */
    void addToHistory(const QString& user, const QString& assistant);
    /** Comprime i turni eccedenti nel summary locale. */
    void compressHistory();
    /** Costruisce il QJsonArray da passare a AiClient::chat(). */
    QJsonArray buildHistoryArray() const;

    /* ── Stato ── */
    bool m_streaming = false;
};

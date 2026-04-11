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

private slots:
    void sendMessage();
    void attachImage();

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
    QPushButton*    m_btnSend     = nullptr;
    QPushButton*    m_btnStop     = nullptr;
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

    /* ── Stato ── */
    bool m_streaming = false;
};

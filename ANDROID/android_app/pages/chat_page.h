#pragma once
#include <QWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QJsonArray>

class AiClient;
class RagEngineSimple;

/* ══════════════════════════════════════════════════════════════
   ChatPage — schermata principale chat con streaming LLM + RAG.

   Layout:
     ┌────────────────────────────────┐
     │ [modello]        [✕ Stop] [🗑] │  ← header
     ├────────────────────────────────┤
     │                                │
     │   QTextBrowser (log chat)      │  ← flex
     │                                │
     ├────────────────────────────────┤
     │ [RAG] [Invia]  QLineEdit       │  ← input
     └────────────────────────────────┘
   ══════════════════════════════════════════════════════════════ */
class ChatPage : public QWidget {
    Q_OBJECT
public:
    explicit ChatPage(AiClient* ai, RagEngineSimple* rag, QWidget* parent = nullptr);

public slots:
    /** Prepende testo di contesto al prossimo invio (da CameraPage). */
    void prependContext(const QString& text);
    /** Segnala che l'indice RAG è stato ricaricato. */
    void onRagReloaded();

signals:
    void queryStarted();
    void queryFinished();

private slots:
    void onSend();
    void onToken(const QString& chunk);
    void onFinished(const QString& full);
    void onError(const QString& msg);
    void onStop();
    void onClear();

private:
    void appendBubble(const QString& role, const QString& text);
    void appendStreamBlock();
    QString buildSystemPrompt(const QString& userMsg) const;

    AiClient*        m_ai;
    RagEngineSimple* m_rag;

    QTextBrowser* m_log       = nullptr;
    QLineEdit*    m_input     = nullptr;
    QPushButton*  m_sendBtn   = nullptr;
    QPushButton*  m_stopBtn   = nullptr;
    QPushButton*  m_clearBtn  = nullptr;
    QPushButton*  m_ragBtn    = nullptr;  ///< toggle RAG on/off
    QLabel*       m_modelLbl  = nullptr;

    QJsonArray    m_history;              ///< turni precedenti per context window
    QString       m_streamAccum;          ///< testo in arrivo durante lo streaming
    QString       m_pendingContext;       ///< testo da Camera da iniettare
    bool          m_ragEnabled  = true;
    bool          m_streaming   = false;

    static constexpr int kMaxHistoryTurns = 6;  ///< turni mantenuti in memoria
};

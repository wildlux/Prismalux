#pragma once
#include <QWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QDialog>
#include <QListWidget>
#include <QJsonArray>

#ifdef HAVE_TTS
#include <QTextToSpeech>
#endif

class AiClient;
class RagEngineSimple;

/* --------------------------------------------------------------
   ModelPickerDialog -- selettore modello LLM a tutto schermo.
   -------------------------------------------------------------- */
class ModelPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelPickerDialog(const QStringList& models,
                                const QString& current,
                                const QString& serverEmoji,
                                QWidget* parent = nullptr);
    QString selectedModel() const { return m_selected; }
signals:
    void modelPicked(const QString& model);
private slots:
    void onItemClicked(QListWidgetItem* item);
private:
    QListWidget* m_list    = nullptr;
    QString      m_selected;
};

/* --------------------------------------------------------------
   ChatPage -- schermata principale chat con streaming LLM + RAG.

   Layout:
     ----------------------------------
     - [modello]        [? Stop] [?] -  ? header
     ----------------------------------
     -                                -
     -   QTextBrowser (log chat)      -  ? flex
     -                                -
     ----------------------------------
     - [RAG] [Invia]  QLineEdit       -  ? input
     ----------------------------------
   -------------------------------------------------------------- */
class ChatPage : public QWidget {
    Q_OBJECT
public:
    explicit ChatPage(AiClient* ai, RagEngineSimple* rag, QWidget* parent = nullptr);

public slots:
    /** Prepende testo di contesto al prossimo invio (da CameraPage). */
    void prependContext(const QString& text);
    /** Segnala che l'indice RAG ? stato ricaricato. */
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
    void onCopyLast();
    void onModelBtnClicked();
    void onModelPicked(const QString& model);
    void onExternalModelChanged(const QString& model);
    void onModelsReady(const QStringList& models);
    void onRagToggled(bool on);
    void fetchModels();
    void onBackendServer();
    void onBackendLocal();
    void onLocalModelLoaded(const QString& path);
    void onAiLocalModeChanged(bool on);
    void onCopyBtnRestore();
    void onSpeakLast();
    void onCloudModeClicked();

private:
    void appendBubble(const QString& role, const QString& text);
    void appendStreamBlock();
    QString buildSystemPrompt(const QString& userMsg) const;
    static bool isLanHost(const QString& host, int port);
    static QString serverEmoji(const QString& host, int port);

    AiClient*        m_ai;
    RagEngineSimple* m_rag;

    QTextBrowser* m_log       = nullptr;
    QLineEdit*    m_input     = nullptr;
    QPushButton*  m_sendBtn   = nullptr;
    QPushButton*  m_stopBtn   = nullptr;
    QPushButton*  m_clearBtn  = nullptr;
    QPushButton*  m_copyBtn   = nullptr;
    QPushButton*  m_ragBtn    = nullptr;
    QString       m_lastAssistantMsg;  ///< ultima risposta AI completa (per copia)
    QPushButton*  m_modelBtn         = nullptr;
    QPushButton*  m_cloudBtn         = nullptr;  ///< ☁️ API cloud esterna
    QPushButton*  m_serverBtn        = nullptr;
    QPushButton*  m_localBtn         = nullptr;
    QLabel*       m_backendStatusLbl = nullptr;  ///< banner colorato backend attivo
    QStringList   m_modelList;             ///< modelli disponibili dal server

#ifdef HAVE_TTS
    QTextToSpeech* m_tts    = nullptr;
#endif
    QPushButton*   m_speakBtn = nullptr;

    QJsonArray    m_history;              ///< turni precedenti per context window
    QString       m_streamAccum;          ///< testo in arrivo durante lo streaming
    QString       m_pendingContext;       ///< testo da Camera da iniettare
    QString       m_lastUserMsg;          ///< ultimo messaggio inviato (per history)
    bool          m_ragEnabled  = true;
    bool          m_streaming   = false;

    static constexpr int kMaxHistoryTurns = 6;  ///< turni mantenuti in memoria
};

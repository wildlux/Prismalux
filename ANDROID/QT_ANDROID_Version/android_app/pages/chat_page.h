#pragma once
#include <QWidget>
#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QDialog>
#include <QListWidget>
#include <QJsonArray>
#include <QMap>
#include <QButtonGroup>
#include <QElapsedTimer>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#ifdef HAVE_TTS
#include <QTextToSpeech>
#endif
#ifdef HAVE_MULTIMEDIA
#include <QMediaCaptureSession>
#include <QMediaRecorder>
#include <QMediaFormat>
#include <QAudioInput>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QTimer>
#endif

class AiClient;
class RagEngineSimple;

/* --------------------------------------------------------------
   ChatBubbleWidget — singola bolla chat.
   Le bolle AI mostrano azioni inline (📋 copia, 🔊 leggi) dopo
   la risposta completa, stile Gemini/Claude/Qwen.
   -------------------------------------------------------------- */
class ChatBubbleWidget : public QFrame {
    Q_OBJECT
public:
    explicit ChatBubbleWidget(const QString& role,
                               const QString& text,
                               QWidget* parent = nullptr);
    void appendText(const QString& chunk);
    void showActions();
    QString fullText() const { return m_fullText; }
signals:
    void copyClicked(const QString& text);
    void speakClicked(const QString& text);
    void saveMemClicked(const QString& text);
private slots:
    void onCopyClicked();
    void onSpeakClicked();
    void onCopyRestored();
    void onSaveMemClicked();
private:
    QLabel*      m_textLbl    = nullptr;
    QPushButton* m_copyBtn    = nullptr;
    QPushButton* m_speakBtn   = nullptr;
    QPushButton* m_saveMemBtn = nullptr;
    QString      m_fullText;
};

/* --------------------------------------------------------------
   ModelPickerDialog — selettore modello LLM a tutto schermo.
   -------------------------------------------------------------- */
class ModelPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelPickerDialog(const QStringList& models,
                                const QString& current,
                                const QString& serverEmoji,
                                const QMap<QString, qint64>& sizes,
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
   ChatPage — schermata principale chat con streaming LLM + RAG.

   Layout:
     ----------------------------------
     - [modello]        [✕ Stop] [🗑] -  ← header
     ----------------------------------
     - [☁️ Cloud] [🌐 Server] [📱 Locale] -
     ----------------------------------
     - [banner backend attivo]         -
     ----------------------------------
     -                                -
     -   QScrollArea con bolle chat   -  ← flex
     -                                -
     ----------------------------------
     - [RAG] [QLineEdit]    [➤ Invia] -  ← input
     ----------------------------------
   -------------------------------------------------------------- */
class ChatPage : public QWidget {
    Q_OBJECT
public:
    explicit ChatPage(AiClient* ai, RagEngineSimple* rag, QWidget* parent = nullptr);

public slots:
    void prependContext(const QString& text);
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
    void onModelBtnClicked();
    void onModelPicked(const QString& model);
    void onExternalModelChanged(const QString& model);
    void onModelsReady(const QStringList& models);
    void onAttachClicked();
    void fetchModels();
    void onBackendServer();
    void onBackendLocal();
    void onLocalModelLoaded(const QString& path);
    void onAiLocalModeChanged(bool on);
    void onBubbleCopyClicked(const QString& text);
    void onBubbleSpeakClicked(const QString& text);
    void onBubbleSaveMemClicked(const QString& text);
    void onCloudModeClicked();
    void scrollToBottom();
    void onClearHistory();
    void onExportClicked();
    void onVoiceLoopClicked();
#ifdef HAVE_TTS
    void onTtsStateChanged(QTextToSpeech::State s);
#endif
#ifdef HAVE_MULTIMEDIA
    void onLoopRecordTimeout();
    void onLoopWhisperReply();
#endif

private:
    void appendBubble(const QString& role, const QString& text);
    void appendStreamBlock();
    void initDb();
    void saveMessageToDb(const QString& role, const QString& content);
    void loadHistoryFromDb();
    QString buildSystemPrompt(const QString& userMsg) const;
    static bool isLanHost(const QString& host, int port);
    static QString serverEmoji(const QString& host, int port);

    AiClient*        m_ai;
    RagEngineSimple* m_rag;

    QScrollArea*      m_scrollArea    = nullptr;
    QWidget*          m_chatContainer = nullptr;
    QVBoxLayout*      m_chatLayout    = nullptr;
    ChatBubbleWidget* m_streamBubble  = nullptr;

    QLineEdit*   m_input     = nullptr;
    QPushButton* m_sendBtn        = nullptr;
    QPushButton* m_stopBtn        = nullptr;
    QPushButton* m_clearBtn       = nullptr;
    QPushButton* m_clearHistoryBtn = nullptr;
    QPushButton* m_ragBtn         = nullptr;
    QPushButton* m_modelBtn       = nullptr;
    QPushButton* m_exportBtn      = nullptr;
    QPushButton* m_hermesToggle   = nullptr;
    QPushButton*  m_cloudBtn        = nullptr;
    QPushButton*  m_serverBtn       = nullptr;
    QPushButton*  m_localBtn        = nullptr;
    QButtonGroup* m_backendGroup    = nullptr;
    QLabel*       m_backendStatusLbl = nullptr;
    QLabel*       m_ttftLbl          = nullptr;
    QPushButton*  m_voiceLoopBtn     = nullptr;
    bool          m_voiceLoopActive  = false;
    QString       m_loopLastResponse;
    QStringList  m_modelList;
    QElapsedTimer m_ttftTimer;
    bool          m_ttftStarted = false;

#ifdef HAVE_TTS
    QTextToSpeech* m_tts = nullptr;
#endif
#ifdef HAVE_MULTIMEDIA
    QMediaCaptureSession* m_loopSession   = nullptr;
    QMediaRecorder*       m_loopRecorder  = nullptr;
    QAudioInput*          m_loopAudioIn   = nullptr;
    QTimer*               m_loopRecTimer  = nullptr;
    QNetworkAccessManager* m_loopNam      = nullptr;
    QNetworkReply*         m_loopReply    = nullptr;
    QString                m_loopRecFile;
#endif

    QJsonArray  m_history;
    QString     m_pendingContext;
    QString     m_lastUserMsg;
    bool        m_streaming  = false;
    QStringList m_attachedContents;   ///< contenuto file allegati (max 5)
    QStringList m_attachedNames;      ///< nomi file per UI

    static constexpr int kMaxHistoryTurns = 6;
};

#pragma once
#include <QWidget>
#include <QAbstractSocket>
#include <QProcess>
#include <QJsonObject>
#include <QUrl>
#include <functional>
#include "../ai_client.h"
#include "../widgets/ai_error_widget.h"
/* main_security.h rimosso — SecurityAnalyzerPage è in ProgrammazionePage */

class QTabWidget;
class QLineEdit;
class QTextEdit;
class QTextBrowser;
class QComboBox;
class QPushButton;
class QLabel;
class QListWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QTcpSocket;
#include <QSet>
class QProgressBar;
class QCheckBox;
class QTimer;

/* ══════════════════════════════════════════════════════════════
   AppControllerPage — MCP bridges per applicazioni esterne

   Sub-tab:
   ┌──────────┬──────────┬──────────┬──────────────────────────┐
   │🎨 Blender│🔩 FreeCAD│📄 Office │🔵 CloudCompare           │
   ├──────────┴──────────┴──────────┴──────────────────────────┤
   │🃏 Anki MCP │🖥️ KiCAD MCP │🤖 TinyMCP (MCU)              │
   └──────────────────────────────────────────────────────────┘

   Ogni sub-tab:
   • Barra connessione (host:port / bridge toggle)
   • Combo azione + combo modello AI
   • Input testo  ▶ Esegui AI  ⏹ Stop
   • Output AI in streaming
   • Pulsante "Esegui in <App>" (abilitato dopo ricezione codice)
   ══════════════════════════════════════════════════════════════ */
class AppControllerPage : public QWidget {
    Q_OBJECT
public:
    explicit AppControllerPage(AiClient* ai, QWidget* parent = nullptr);
    ~AppControllerPage() override;

signals:
    /** Emesso quando un modulo Python necessario a un MCP risulta mancante.
     *  @p pipPkg — nome del pacchetto pip (es. "obsws-python") */
    void openSettingsDipendenze(const QString& pipPkg);

private:
    AiClient*      m_ai           = nullptr;
    QTabWidget*    m_tabs         = nullptr;
    QObject*       m_tokenHolder  = nullptr;
    bool           m_aiActive     = false;
    int            m_activeTab    = -1;
    AiErrorWidget*        m_aiErrorPanel = nullptr;
    QProgressBar*         m_aiProgress   = nullptr;  ///< progress indeterminata durante AI
    /* SecurityAnalyzerPage rimossa — ora in ProgrammazionePage */

    /* ── Blender ── */
    QLineEdit*           m_blenderHostEdit  = nullptr;
    QLabel*              m_blenderStatusLbl = nullptr;
    QPushButton*         m_blenderExecBtn   = nullptr;
    QComboBox*           m_blenderAction    = nullptr;
    QComboBox*           m_blenderModel     = nullptr;
    QTextEdit*           m_blenderInput     = nullptr;
    QTextEdit*           m_blenderOutput    = nullptr;
    QTextEdit*           m_blenderCodeEdit  = nullptr;  // editor codice Python diretto
    QPushButton*         m_blenderRunBtn    = nullptr;
    QPushButton*         m_blenderStopBtn   = nullptr;
    QString              m_blenderCode;
    QNetworkAccessManager* m_blenderNam     = nullptr;
    QLabel*              m_blenderWarnLbl   = nullptr;  ///< avviso modello < 7B

    /* ── FreeCAD ── */
    QLineEdit*  m_freecadHostEdit  = nullptr;
    QLabel*     m_freecadStatusLbl = nullptr;
    QPushButton* m_freecadExecBtn  = nullptr;
    QComboBox*  m_freecadAction    = nullptr;
    QComboBox*  m_freecadModel     = nullptr;
    QTextEdit*  m_freecadInput     = nullptr;
    QTextEdit*  m_freecadOutput    = nullptr;
    QPushButton* m_freecadRunBtn   = nullptr;
    QPushButton* m_freecadStopBtn  = nullptr;
    QString     m_freecadCode;

    /* ── Office ── */
    QPushButton* m_officeStartBtn   = nullptr;
    QLabel*      m_officeStatusLbl  = nullptr;
    QPushButton* m_officeExecBtn    = nullptr;
    QComboBox*   m_officeAction     = nullptr;
    QComboBox*   m_officeModel      = nullptr;
    QTextEdit*   m_officeInput      = nullptr;
    QTextEdit*   m_officeOutput     = nullptr;
    QPushButton* m_officeRunBtn     = nullptr;
    QPushButton* m_officeStopBtn    = nullptr;
    QString      m_officeCode;
    QString      m_officeBridgeToken;
    QNetworkAccessManager* m_officeNam       = nullptr;
    QProcess*              m_officeBridgeProc = nullptr;

    /* ── CloudCompare ── */
    QTextEdit*  m_ccOutput = nullptr;

    /* ── Anki MCP ── */
    QLineEdit*   m_ankiHostEdit  = nullptr;
    QLabel*      m_ankiStatusLbl = nullptr;
    QPushButton* m_ankiSendBtn   = nullptr;
    QComboBox*   m_ankiAction    = nullptr;
    QComboBox*   m_ankiModel     = nullptr;
    QTextEdit*   m_ankiInput     = nullptr;
    QTextEdit*   m_ankiOutput    = nullptr;
    QPushButton* m_ankiRunBtn    = nullptr;
    QPushButton* m_ankiStopBtn   = nullptr;
    QNetworkAccessManager* m_ankiNam = nullptr;
    QLineEdit*   m_ankiDeckEdit  = nullptr;  ///< salvato per onAnkiSendClicked

    /* ── KiCAD MCP ── */
    QLineEdit*   m_kicadHostEdit  = nullptr;
    QLabel*      m_kicadStatusLbl = nullptr;
    QPushButton* m_kicadExecBtn   = nullptr;
    QComboBox*   m_kicadAction    = nullptr;
    QComboBox*   m_kicadModel     = nullptr;
    QTextEdit*   m_kicadInput     = nullptr;
    QTextEdit*   m_kicadOutput    = nullptr;
    QPushButton* m_kicadRunBtn    = nullptr;
    QPushButton* m_kicadStopBtn   = nullptr;
    QString      m_kicadCode;
    QNetworkAccessManager* m_kicadNam = nullptr;

    /* ── TinyMCP (Microcontroller) ── */
    QComboBox*   m_mcuPort       = nullptr;
    QLabel*      m_mcuStatusLbl  = nullptr;
    QPushButton* m_mcuFlashBtn   = nullptr;
    QComboBox*   m_mcuAction     = nullptr;
    QComboBox*   m_mcuModel      = nullptr;
    QTextEdit*   m_mcuInput      = nullptr;
    QTextBrowser* m_mcuOutput    = nullptr;
    QPushButton* m_mcuRunBtn     = nullptr;
    QPushButton* m_mcuStopBtn    = nullptr;
    QString      m_mcuCode;
    QProcess*    m_mcuFlashProc  = nullptr;
    QComboBox*   m_mcuBoardCombo = nullptr;  ///< salvato per onMcuFlashClicked / onMcuRunClicked

    /* ── OBS MCP ── */
    QLineEdit*   m_obsHostEdit  = nullptr;
    QLabel*      m_obsStatusLbl = nullptr;
    QPushButton* m_obsExecBtn   = nullptr;
    QComboBox*   m_obsAction    = nullptr;
    QComboBox*   m_obsModel     = nullptr;
    QTextEdit*   m_obsInput     = nullptr;
    QTextBrowser* m_obsOutput   = nullptr;
    QPushButton* m_obsRunBtn    = nullptr;
    QPushButton* m_obsStopBtn   = nullptr;
    QString      m_obsCode;
    QProcess*    m_obsExecProc  = nullptr;

    /* ── Godot MCP (index 9) ── */
    QLabel*      m_godotStatusLbl = nullptr;
    QPushButton* m_godotExecBtn   = nullptr;
    QComboBox*   m_godotAction    = nullptr;
    QComboBox*   m_godotModel     = nullptr;
    QTextEdit*   m_godotInput     = nullptr;
    QTextEdit*   m_godotOutput    = nullptr;
    QPushButton* m_godotRunBtn    = nullptr;
    QPushButton* m_godotStopBtn   = nullptr;
    QString      m_godotCode;
    QProcess*    m_godotExecProc  = nullptr;

    /* ── Game Modding (tabIdx logico 15) ── */
    QComboBox*   m_moddingGameCombo  = nullptr;
    QComboBox*   m_moddingTypeCombo  = nullptr;
    QComboBox*   m_moddingModel      = nullptr;
    QTextEdit*   m_moddingInput      = nullptr;
    QTextEdit*   m_moddingOutput     = nullptr;
    QPushButton* m_moddingRunBtn     = nullptr;
    QPushButton* m_moddingStopBtn    = nullptr;
    QPushButton* m_moddingSaveBtn    = nullptr;
    QLineEdit*   m_moddingFolderEdit = nullptr;
    QLabel*      m_moddingStatusLbl  = nullptr;
    QString      m_moddingCode;

    /* ── runAi session state (saved for named slots) ── */
    QTextEdit*   m_runAiOutput   = nullptr;
    QPushButton* m_runAiRunBtn   = nullptr;
    QPushButton* m_runAiStopBtn  = nullptr;
    QComboBox*   m_runAiModelCombo = nullptr;
    int          m_runAiTabIdx   = -1;
    QString      m_runAiSys;
    QString      m_runAiUserMsg;
    /* Connessioni runAi (disconnesse prima di ogni nuova chiamata) */
    QMetaObject::Connection m_connToken;
    QMetaObject::Connection m_connFinished;
    QMetaObject::Connection m_connError;

    QWidget* buildBlenderTab();
    QWidget* buildFreeCADTab();
    QWidget* buildOfficeTab();
    QWidget* buildCloudCompareTab();
    QWidget* buildAnkiTab();
    QWidget* buildKiCADTab();
    QWidget* buildTinyMCPTab();
    QWidget* buildOBSTab();
    QWidget* buildGodotTab();
    QWidget* buildGameModdingTab();
    QWidget* buildTelegramTab();
    QWidget* buildWhatsAppTab();
public:
    QWidget* buildDevAgentTab();
private:
    void execAnkiAction(const QString& action, const QString& payload);
    void execKiCADAction(const QString& code);
    void detectSerialPorts();

    /** Helper TCP per Blender (estratto come metodo per evitare lambda locale) */
    void blenderSendTcp(const QString& host, int port,
                        const QByteArray& jsonMsg,
                        std::function<void(const QJsonObject&, bool)> cb);

    void runAi(int tabIdx, const QString& sys, const QString& userMsg,
               QTextEdit* output, QPushButton* runBtn, QPushButton* stopBtn,
               QComboBox* modelCombo);
    /* ── Slot estratti da lambda — constructor ── */
    void onModelsReady(const QStringList& models);

    /* ── Slot estratti da lambda — runAi() ── */
    void onRunAiToken(const QString& t);
    void onRunAiFinished(const QString& full);
    void onRunAiError(const QString& msg);

    /* ── Slot estratti da lambda — Blender ── */
    void onBlenderCodeChanged();
    void onBlenderPingClicked();
    void onBlenderExecClicked();
    void onBlenderHelpClicked();
    void onBlenderRunClicked();
    void onBlenderStopClicked();

    /* ── Slot estratti da lambda — FreeCAD ── */
    void onFreecadPingClicked();
    void onFreecadPingConnected();
    void onFreecadPingError(QAbstractSocket::SocketError err);
    void onFreecadPingTimeout();
    void onFreecadExecClicked();
    void onFreecadExecConnected();
    void onFreecadExecReadyRead();
    void onFreecadExecError(QAbstractSocket::SocketError err);
    void onFreecadRunClicked();
    void onFreecadStopClicked();
    void onFreecadHelpClicked();

    /* ── Slot estratti da lambda — Office ── */
    void onOfficeStartClicked();
    void onOfficeExecClicked();
    void onOfficeRunClicked();
    void onOfficeStopClicked();
    void onOfficeHelpClicked();
    void onOfficeBridgeFinished(int exitCode, QProcess::ExitStatus status);
    void onOfficeStatusReply();   ///< risposta GET /status dopo avvio bridge
    void onOfficeExecReply();     ///< risposta POST /execute

    /* ── Slot estratti da lambda — CloudCompare ── */
    void onCcHelpClicked();

    /* ── Slot estratti da lambda — Anki ── */
    void onAnkiPingClicked();
    void onAnkiPingReplyFinished();   ///< risposta GET version da ping
    void onAnkiSendClicked();
    void onAnkiRunClicked();
    void onAnkiStopClicked();
    void onAnkiHelpClicked();
    void onAnkiAddNotesReply();   ///< risposta addNotes da execAnkiAction

    /* ── Slot estratti da lambda — KiCAD ── */
    void onKicadPingClicked();
    void onKicadExecClicked();
    void onKicadRunClicked();
    void onKicadStopClicked();
    void onKicadHelpClicked();
    void onKicadExecReply();      ///< risposta POST /execute da execKiCADAction

    /* ── Slot estratti da lambda — TinyMCP ── */
    void onMcuDetectClicked();
    void onMcuFlashClicked();
    void onMcuRunClicked();
    void onMcuStopClicked();
    void onMcuHelpClicked();

    /* ── OBS ── */
    void onObsPingClicked();
    void onObsExecClicked();
    void onObsRunClicked();
    void onObsStopClicked();
    void onObsHelpClicked();
    void onObsProcReadyRead();
    void onObsProcFinished(int code, QProcess::ExitStatus status);

    /* ── Slot estratti da lambda — Godot ── */
    void onGodotExecClicked();
    void onGodotRunClicked();
    void onGodotStopClicked();

    /* ── Slot Game Modding ── */
    void onModdingGameChanged(int idx);
    void onModdingRunClicked();
    void onModdingStopClicked();
    void onModdingSaveClicked();
    void onModdingBrowseClicked();
    void onModdingOpenFolderClicked();

    /* ── Slot estratti da lambda — Telegram Bot ── */
    void onTelegramStartClicked();
    void onTelegramStopClicked();
    void onTelegramProcReadyRead();
    void onTelegramProcFinished(int code, QProcess::ExitStatus status);
    void onTelegramAddContactClicked();
    void onTelegramRemoveContactClicked();
    void onTelegramSendPromoClicked();

    /* ── Slot estratti da lambda — WhatsApp ── */
    void onWaAddContactClicked();
    void onWaRemoveContactClicked();
    void onWaSendPromoClicked();

    /* ── WhatsApp Bot rispondente ── */
    void onWaBotStartClicked();
    void onWaBotStopClicked();
    void onWaPollTick();
    void onWaPollReply();
    void onWaBotSendReply(const QString& toNumber, const QString& replyText);

    /* ── Link pip cliccabili nei log QTextBrowser ── */
    void onPipLinkClicked(const QUrl& url);

    /* ── Dev Agent LangGraph ── */
    void onDevAgentRunClicked();
    void onDevAgentStopClicked();
    void onDevAgentInstallClicked();
    void onDevAgentRestoreClicked();
    void onDevAgentLoadHistory();
    void onDevAgentGitLogClicked();
    void onDevAgentGitRestoreClicked();
    void onDevAgentGitFetchResetClicked();
    void onDevAgentGitStashPushClicked();
    void onDevAgentGitStashListClicked();
    void onDevAgentGitStashPopClicked();
    void onDevAgentReadOutput();
    void onDevAgentReadError();
    void onDevAgentFinished(int code, QProcess::ExitStatus status);

    /* ── Telegram Bot — membri ── */
    QLineEdit*   m_telegramTokenEdit     = nullptr;
    QLineEdit*   m_telegramWhitelistEdit = nullptr;
    QPushButton* m_telegramStartBtn      = nullptr;
    QPushButton* m_telegramStopBtn       = nullptr;
    QLabel*      m_telegramStatusLbl     = nullptr;
    QTextBrowser* m_telegramLog           = nullptr;
    QProcess*    m_telegramProc          = nullptr;
    QObject*     m_telegramAiHolder      = nullptr;
    bool         m_telegramIntentionalStop = false;
    int          m_telegramChatId        = 0;
    QLineEdit*   m_telegramPromoContactEdit = nullptr;
    QListWidget* m_telegramContactList      = nullptr;
    QTextEdit*   m_telegramPromoMsgEdit     = nullptr;
    QLabel*      m_telegramPromoStatusLbl   = nullptr;
    QNetworkAccessManager* m_telegramPromoNam = nullptr;
    QCheckBox*   m_telegramAutoAddCheck     = nullptr;
    QWidget*     m_telegramModuleBanner     = nullptr;

    /* ── WhatsApp — membri ── */
    QLineEdit*   m_waPromoContactEdit    = nullptr;
    QListWidget* m_waContactList         = nullptr;
    QTextEdit*   m_waPromoMsgEdit        = nullptr;
    QLabel*      m_waPromoStatusLbl      = nullptr;
    QLineEdit*   m_waBridgeUrlEdit       = nullptr;
    QNetworkAccessManager* m_waPromoNam  = nullptr;

    /* ── Dev Agent LangGraph — membri ── */
    QLineEdit*   m_devTaskEdit      = nullptr;
    QComboBox*   m_devModelCombo    = nullptr;
    QPushButton* m_devRunBtn        = nullptr;
    QPushButton* m_devStopBtn       = nullptr;
    QPushButton* m_devInstallBtn    = nullptr;
    QPushButton* m_devRestoreBtn    = nullptr;
    QTextBrowser* m_devLog           = nullptr;
    QTextEdit*   m_devDiff          = nullptr;
    QLabel*      m_devStatusLbl     = nullptr;
    QListWidget* m_devHistoryList   = nullptr;
    QListWidget* m_devGitLogList    = nullptr;  ///< commit git
    QListWidget* m_devStashList     = nullptr;  ///< stash git
    QPushButton* m_devGitRestoreBtn = nullptr;
    QPushButton* m_devGitStashPopBtn = nullptr;
    QLineEdit*   m_devGitBranchEdit = nullptr;
    QProcess*    m_devProc          = nullptr;
    QString      m_devPendingOutput;   ///< buffer righe parziali stdout

    /* ── WhatsApp Bot rispondente — membri ── */
    QLineEdit*             m_waWhitelistEdit  = nullptr;
    QCheckBox*             m_waAutoReplyCheck = nullptr;
    QPushButton*           m_waBotStartBtn    = nullptr;
    QPushButton*           m_waBotStopBtn     = nullptr;
    QLabel*                m_waBotStatusLbl   = nullptr;
    QTextBrowser*          m_waBotLog         = nullptr;
    QTimer*                m_waPollTimer      = nullptr;
    QNetworkAccessManager* m_waNam            = nullptr;
    QSet<QString>          m_waSeenMsgIds;
    QObject*               m_waBotAiHolder    = nullptr;
    int                    m_waPollFailCount  = 0;

    /* ── Stato transitorio per reply one-shot ── */
    QNetworkReply* m_ankiPendingReply  = nullptr;  ///< reply addNotes in volo
    int            m_ankiPendingCount  = 0;         ///< numero carte inviate
    QNetworkReply* m_kicadPendingReply = nullptr;   ///< reply execKiCAD in volo
    QNetworkReply* m_officeStatusReply = nullptr;   ///< reply GET /status
    QNetworkReply* m_officeExecReply   = nullptr;   ///< reply POST /execute

    /* ── Socket temporanei per ping/exec one-shot ── */
    QTcpSocket*    m_freecadPingSock   = nullptr;   ///< socket ping FreeCAD
    QTcpSocket*    m_freecadExecSock   = nullptr;   ///< socket exec FreeCAD
    QNetworkReply* m_ankiPingReply     = nullptr;   ///< reply ping AnkiConnect
    QTcpSocket*    m_obsPingSock       = nullptr;   ///< socket ping OBS
    QByteArray     m_freecadExecBody;               ///< payload da inviare su connected

public:
    /** Estrae il primo blocco ```...``` dall'output AI. Public per testabilità. */
    static QString extractCode(const QString& text);
    /** Indovina l'estensione file del mod generato dal contenuto del codice. */
    static QString detectModExtension(const QString& code);
};

#pragma once
#include <QWidget>
#include <QAbstractSocket>
#include <QProcess>
#include <QJsonObject>
#include <functional>
#include "../ai_client.h"
#include "../widgets/ai_error_widget.h"

class QTabWidget;
class QLineEdit;
class QTextEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QTcpSocket;
class QProgressBar;

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

private:
    AiClient*      m_ai           = nullptr;
    QTabWidget*    m_tabs         = nullptr;
    QObject*       m_tokenHolder  = nullptr;
    bool           m_aiActive     = false;
    int            m_activeTab    = -1;
    AiErrorWidget* m_aiErrorPanel = nullptr;
    QProgressBar*  m_aiProgress   = nullptr;  ///< progress indeterminata durante AI

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
    QTextEdit*   m_mcuOutput     = nullptr;
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
    QTextEdit*   m_obsOutput    = nullptr;
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
    void populateModels(QComboBox* combo);

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

    /* ── Slot estratti da lambda — OBS ── */
    void onObsPingClicked();
    void onObsPingConnected();
    void onObsPingError(QAbstractSocket::SocketError err);
    void onObsPingTimeout();
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
};

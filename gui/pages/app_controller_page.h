#pragma once
#include <QWidget>
#include "../ai_client.h"

class QTabWidget;
class QLineEdit;
class QTextEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QProcess;
class QNetworkAccessManager;
class QTcpSocket;

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
    AiClient*   m_ai          = nullptr;
    QTabWidget* m_tabs        = nullptr;
    QObject*    m_tokenHolder = nullptr;  ///< one-shot AI stream holder
    bool        m_aiActive    = false;
    int         m_activeTab   = -1;       ///< tab che ha lanciato l'AI corrente

    /* ── Blender ── */
    QLineEdit*           m_blenderHostEdit  = nullptr;
    QLabel*              m_blenderStatusLbl = nullptr;
    QPushButton*         m_blenderExecBtn   = nullptr;
    QComboBox*           m_blenderAction    = nullptr;
    QComboBox*           m_blenderModel     = nullptr;
    QTextEdit*           m_blenderInput     = nullptr;
    QTextEdit*           m_blenderOutput    = nullptr;
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

    QWidget* buildBlenderTab();
    QWidget* buildFreeCADTab();
    QWidget* buildOfficeTab();
    QWidget* buildCloudCompareTab();
    QWidget* buildAnkiTab();
    QWidget* buildKiCADTab();
    QWidget* buildTinyMCPTab();

    void execAnkiAction(const QString& action, const QString& payload);
    void execKiCADAction(const QString& code);
    void detectSerialPorts();

    void runAi(int tabIdx, const QString& sys, const QString& userMsg,
               QTextEdit* output, QPushButton* runBtn, QPushButton* stopBtn,
               QComboBox* modelCombo);
    void populateModels(QComboBox* combo);

public:
    /** Estrae il primo blocco ```...``` dall'output AI. Public per testabilità. */
    static QString extractCode(const QString& text);
};

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
   └──────────┴──────────┴──────────┴──────────────────────────┘

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

    QWidget* buildBlenderTab();
    QWidget* buildFreeCADTab();
    QWidget* buildOfficeTab();
    QWidget* buildCloudCompareTab();

    /* Helpers condivisi */
    void runAi(int tabIdx, const QString& sys, const QString& userMsg,
               QTextEdit* output, QPushButton* runBtn, QPushButton* stopBtn,
               QComboBox* modelCombo);
    static QString extractCode(const QString& text);
    void populateModels(QComboBox* combo);
};

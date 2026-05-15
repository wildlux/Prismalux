#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProcess>
#include <QProgressBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QAbstractSocket>
#include <QTcpSocket>
#include "../ai_client.h"
#include "../widgets/ai_error_widget.h"

/* ══════════════════════════════════════════════════════════════
   RicercaPage — "Ricerca e Sviluppo"
   Tab: Paper Scientifico · Brevetto · Doc Tecnico · Cerca Lavoro
        + Bioinformatica: Cytoscape · RDKit · Bioconda
   ══════════════════════════════════════════════════════════════ */
class RicercaPage : public QWidget {
    Q_OBJECT
public:
    explicit RicercaPage(AiClient* ai, QWidget* parent = nullptr);

private:
    AiClient*    m_ai;
    QTextEdit*   m_outCurrent    = nullptr;
    QPushButton* m_btnGenAttivo  = nullptr;
    QPushButton* m_btnStopAttivo = nullptr;

    /* ── Cytoscape MCP ── */
    QLineEdit*   m_cytoHostEdit  = nullptr;
    QLabel*      m_cytoStatusLbl = nullptr;
    QPushButton* m_cytoExecBtn   = nullptr;
    QComboBox*   m_cytoAction    = nullptr;
    QComboBox*   m_cytoModel     = nullptr;
    QTextEdit*   m_cytoInput     = nullptr;
    QTextEdit*   m_cytoOutput    = nullptr;
    QPushButton* m_cytoRunBtn    = nullptr;
    QPushButton* m_cytoStopBtn   = nullptr;
    QString      m_cytoCode;
    QProcess*    m_cytoProc      = nullptr;
    QTcpSocket*  m_cytoSock      = nullptr;

    /* ── RDKit MCP ── */
    QLabel*      m_rdkitStatusLbl = nullptr;
    QPushButton* m_rdkitExecBtn   = nullptr;
    QComboBox*   m_rdkitAction    = nullptr;
    QComboBox*   m_rdkitModel     = nullptr;
    QTextEdit*   m_rdkitInput     = nullptr;
    QTextEdit*   m_rdkitOutput    = nullptr;
    QPushButton* m_rdkitRunBtn    = nullptr;
    QPushButton* m_rdkitStopBtn   = nullptr;
    QString      m_rdkitCode;
    QProcess*    m_rdkitProc      = nullptr;

    /* ── Bioconda MCP ── */
    QLabel*      m_bioStatusLbl  = nullptr;
    QPushButton* m_bioExecBtn    = nullptr;
    QComboBox*   m_bioAction     = nullptr;
    QComboBox*   m_bioModel      = nullptr;
    QTextEdit*   m_bioInput      = nullptr;
    QTextEdit*   m_bioOutput     = nullptr;
    QPushButton* m_bioRunBtn     = nullptr;
    QPushButton* m_bioStopBtn    = nullptr;
    QString      m_bioCode;
    QProcess*    m_bioProc       = nullptr;

    /* ── Avogadro MCP ── */
    QLabel*      m_avoStatusLbl  = nullptr;
    QPushButton* m_avoExecBtn    = nullptr;
    QComboBox*   m_avoAction     = nullptr;
    QComboBox*   m_avoModel      = nullptr;
    QTextEdit*   m_avoInput      = nullptr;
    QTextEdit*   m_avoOutput     = nullptr;
    QPushButton* m_avoRunBtn     = nullptr;
    QPushButton* m_avoStopBtn    = nullptr;
    QString      m_avoCode;
    QProcess*    m_avoProc       = nullptr;

    /* ── AI streaming per tab science ── */
    QObject*       m_sciTokenHolder  = nullptr;
    AiErrorWidget* m_sciErrorPanel   = nullptr;
    QProgressBar*  m_sciProgress     = nullptr;  ///< progress indeterminata durante AI

    /* ── Connessioni one-shot LitAI ── */
    QMetaObject::Connection m_litAiTokenConn;
    QMetaObject::Connection m_litAiFinishedConn;
    QMetaObject::Connection m_litAiErrorConn;

    QWidget* buildPaperTab();
    QWidget* buildBrevettoTab();
    QWidget* buildDocTecnicoTab();
    QWidget* buildCercaLetteraturaTab();
    QWidget* buildCytoscapeTab();
    QWidget* buildRDKitTab();
    QWidget* buildBiocondaTab();
    QWidget* buildAvogadroTab();

    /* ── Cerca Letteratura ── */
    QLineEdit*            m_litQuery      = nullptr;
    QComboBox*            m_litSource     = nullptr;
    QTextEdit*            m_litResults    = nullptr;
    QPushButton*          m_litSearchBtn  = nullptr;
    QPushButton*          m_litAiBtn      = nullptr;
    QLabel*               m_litStatus     = nullptr;
    QNetworkAccessManager* m_litNet       = nullptr;

    void avvia(const QString& sys, const QString& msg,
               QTextEdit* out, QPushButton* btnGen, QPushButton* btnStop);
    void avviaSci(const QString& sys, const QString& userMsg,
                  QTextEdit* out, QPushButton* runBtn, QPushButton* stopBtn,
                  QComboBox* modelCombo,
                  QPushButton* execBtn, QString* codeRef, QLabel* statusLbl);
    void resetButtons();
    void sciPopulateModels(QComboBox* combo);

private slots:
    /* AI globali */
    void onSciModelsReady(const QStringList& models);
    void onAiToken(const QString& t);
    void onAiFinished(const QString& full);
    void onAiError(const QString& err);
    void onAiAborted();
    /* Cerca Letteratura */
    void onLitSearchClicked();
    void onLitReplyFinished();
    void onLitAiClicked();
    void onLitAiToken(const QString& t);
    void onLitAiFinished(const QString& full);
    void onLitAiError(const QString& e);
    /* Cytoscape */
    void onCytoPingClicked();
    void onCytoPingTimeout();
    void onCytoSockConnected();
    void onCytoSockError(QAbstractSocket::SocketError err);
    void onCytoExecClicked();
    void onCytoRunClicked();
    void onCytoStopClicked();
    /* RDKit */
    void onRdkitCheckClicked();
    void onRdkitCheckFinished(int code, QProcess::ExitStatus status);
    void onRdkitExecClicked();
    void onRdkitRunClicked();
    void onRdkitStopClicked();
    /* Bioconda */
    void onBioCheckClicked();
    void onBioCheckFinished(int code, QProcess::ExitStatus status);
    void onBioExecClicked();
    void onBioRunClicked();
    void onBioStopClicked();
    /* Avogadro */
    void onAvoCheckClicked();
    void onAvoCheckFinished(int code, QProcess::ExitStatus status);
    void onAvoExecClicked();
    void onAvoRunClicked();
    void onAvoStopClicked();

public:
    static void esportaPdf(QTextEdit* editor,
                           const QString& titolo, QWidget* parent);
    static void salvaMarkdown(QTextEdit* editor,
                              const QString& titolo, QWidget* parent);
};

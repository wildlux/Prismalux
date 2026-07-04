#ifndef MAIN_MCP_MANAGER_H
#define MAIN_MCP_MANAGER_H

#include <QWidget>
#include <QVector>
#include <QProcess>

class QVBoxLayout;
class QTextEdit;
class QLabel;
class QPushButton;

/* Un MCP scoperto nella cartella MCPs/. */
struct McpEntry {
    QString      name;                 // nome cartella (es. "anki_mcp")
    QString      dir;                  // path assoluto della cartella
    QString      serverPy;             // path del server.py (vuoto se non presente)
    bool         hasReq = false;       // requirements.txt presente
    QLabel*      statusLbl = nullptr;  // etichetta stato nella riga UI
    QLabel*      hashLbl   = nullptr;  // integrità SHA-256 server.py
    QPushButton* installBtn = nullptr;
    QPushButton* testBtn = nullptr;
};

/*
 * McpManagerPage — pannello unico per installare/testare i plugin MCP.
 *
 * - Scansiona MCPs/ ed elenca i server JSON-RPC (cartelle con server.py).
 * - Ambiente isolato: crea/usa un venv in ~/.prismalux/venv per le dipendenze,
 *   evitando di inquinare il Python di sistema (niente --break-system-packages).
 * - "Installa": pip install -r requirements.txt nel venv (con conferma).
 * - "Testa": smoke test JSON-RPC 2.0 (initialize + tools/list) sul server.py.
 *
 * Una sola operazione (install o test) alla volta, per non sovraccaricare.
 */
class McpManagerPage : public QWidget {
    Q_OBJECT
public:
    explicit McpManagerPage(QWidget* parent = nullptr);
    ~McpManagerPage() override;

    /* ── Helper statici (testabili senza UI) ─────────────────────────────── */
    static QString     venvDir();                 // ~/.prismalux/venv
    static QString     venvPython();              // eseguibile python del venv
    static bool        venvExists();              // venvPython() esiste ed è eseguibile
    static QStringList scanMcpServers(const QString& mcpsRoot); // nomi MCP con server.py
    /* Costruisce le 2 righe JSON-RPC dello smoke test (initialize + tools/list). */
    static QByteArray  smokeTestRequests();
    /* True se l'output stdout contiene almeno una risposta JSON-RPC con "result". */
    static bool        smokeTestPassed(const QByteArray& stdoutData);

private slots:
    void onPrepareVenvClicked();
    void onVenvProcFinished(int code, QProcess::ExitStatus st);
    void onRefreshClicked();
    void onInstallClicked();
    void onInstallFinished(int code, QProcess::ExitStatus st);
    void onTestClicked();
    void onTestAllClicked();
    void onTestMcpCallClicked();
    void onMcpGuideClicked();
    void onTestReadyRead();
    void onTestTimeout();
    void onTestFinished(int code, QProcess::ExitStatus st);

private:
    void buildUi();
    void rebuildList();
    void refreshStatuses();
    void appendLog(const QString& html);
    void setBusy(bool busy);
    void updateVenvLabel();
    McpEntry* entryByName(const QString& name);
    /* Conclude il test in corso una sola volta (guard su m_testProc). */
    void finishCurrentTest(bool passed, const QString& detail);
    void advanceTestQueue();

    QVBoxLayout* m_listLay = nullptr;   // layout che contiene le righe MCP
    QTextEdit*   m_log = nullptr;
    QLabel*      m_venvLbl = nullptr;
    QPushButton* m_prepareBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;

    QVector<McpEntry> m_entries;

    QProcess*   m_venvProc    = nullptr;  // creazione venv
    QProcess*   m_installProc = nullptr;  // pip install
    QProcess*   m_testProc    = nullptr;  // smoke test
    QByteArray  m_testBuf;               // stdout accumulato del test
    QString     m_busyName;              // MCP su cui sto operando
    bool        m_busy = false;
    QStringList m_testQueue;             // coda "Testa tutti" sequenziale
    QPushButton* m_testAllBtn     = nullptr;
    QPushButton* m_testMcpCallBtn = nullptr;  // testa via risoluzione python mcp_call
};

#endif // MAIN_MCP_MANAGER_H

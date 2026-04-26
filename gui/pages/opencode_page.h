#pragma once
/*
 * opencode_page.h — Tab "OpenCode" in Prismalux
 * ================================================
 * Integra OpenCode (https://opencode.ai) come agente di codice locale.
 * Avvia `opencode serve` e comunica via HTTP REST + SSE stream.
 *
 * Protocollo:
 *   POST /session                → crea sessione
 *   POST /session/{id}/message   → invia task
 *   GET  /event                  → SSE stream token
 *   POST /session/{id}/abort     → annulla
 *   GET  /session                → lista sessioni recenti
 *   GET  /models                 → modelli disponibili
 */

#include <QWidget>
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QTextBrowser>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDir>
#include <QSettings>

class OpenCodePage : public QWidget {
    Q_OBJECT
public:
    explicit OpenCodePage(QWidget* parent = nullptr);
    ~OpenCodePage();

private slots:
    void onStartStop();
    void onSendClicked();
    void onAbortClicked();
    void onSseData();
    void onSseDone();
    void onSessionsRefresh();
    void onSessionSelected(QListWidgetItem* item);
    void onCwdBrowse();
    void onPollTick();

private:
    /* ── Server lifecycle ─────────────────────────────────── */
    void startServer();
    void stopServer();
    void connectSseStream();
    void disconnectSseStream();

    /* ── HTTP helpers ─────────────────────────────────────── */
    void createSession();
    void sendMessage(const QString& sessionId);
    void abortSession(const QString& sessionId);
    void fetchSessions();
    void fetchModels();

    /* ── SSE parsing ──────────────────────────────────────── */
    void processLine(const QByteArray& line);
    void handleEvent(const QJsonObject& ev);

    /* ── UI helpers ───────────────────────────────────────── */
    void appendToken(const QString& text);
    void appendSystem(const QString& html);
    void finalizeBlock();
    void setServerState(bool running);
    QString baseUrl() const;

     /* ── Processo server ──────────────────────────────────── */
     QProcess* m_proc    = nullptr;
     int       m_port    = P::kOpenCodePort;
     bool      m_running = false;
     int       m_pollTicks = 0;
     QTimer*   m_pollTimer = nullptr;

    /* ── Rete ─────────────────────────────────────────────── */
    QNetworkAccessManager* m_nam     = nullptr;
    QNetworkReply*         m_sse     = nullptr;  ///< SSE aperta
    QByteArray             m_sseBuf;             ///< Buffer SSE parziale

     /* ── Stato sessione ───────────────────────────────────── */
     QString m_sessionId;      ///< ID sessione corrente
     QString m_aiMsgId;        ///< ID del messaggio AI corrente (per filtrare i token)
     bool    m_inBlock = false; ///< true = c'è un blocco AI aperto nel log
     int     m_pollErrorCount = 0; ///< Contatore errori polling consecutivi

    /* ── UI ───────────────────────────────────────────────── */
    QLabel*      m_statusLbl  = nullptr;
    QComboBox*   m_modelCombo = nullptr;
    QLineEdit*   m_cwdEdit    = nullptr;
    QTextBrowser* m_log       = nullptr;
    QTextEdit*   m_input      = nullptr;
    QPushButton* m_startBtn   = nullptr;
    QPushButton* m_sendBtn    = nullptr;
    QPushButton* m_abortBtn   = nullptr;
    QListWidget* m_sessList   = nullptr;
};

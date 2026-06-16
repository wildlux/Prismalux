#pragma once
/*
 * opencode_page.h — Tab "OpenCode" nell'app Android Prismalux
 * =============================================================
 * Si connette a `opencode serve` già avviato sul PC desktop (porta 8092).
 * Nessuna gestione processo locale: il server gira sul desktop.
 *
 * Protocollo HTTP:
 *   POST /session                     → crea sessione { id }
 *   POST /session/{id}/message        → invia testo
 *   GET  /event                       → SSE stream token/idle/error
 *   POST /session/{id}/abort          → annulla generazione
 *   GET  /session                     → lista sessioni recenti
 */
#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextBrowser>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QSettings>
#include <QTimer>

static constexpr int kOpenCodeMobilePort = 8092;

class OpenCodePage : public QWidget {
    Q_OBJECT
public:
    explicit OpenCodePage(QWidget* parent = nullptr);

private slots:
    void onSendClicked();
    void onNewSessionClicked();
    void onAbortClicked();
    void onSseData();
    void onSseDone();
    void onSessionSelected(QListWidgetItem* item);
    void onRefreshSessions();
    void onCreateSessionReply();
    void onSendMessageReply();
    void onFetchSessionsReply();

private:
    void createSession();
    void sendMessage();
    void connectSse();
    void disconnectSse();
    void fetchSessions();
    void processLine(const QByteArray& line);
    void handleEvent(const QJsonObject& ev);
    void appendToken(const QString& text);
    void appendSystem(const QString& html);
    void finalizeBlock();
    void setStatus(const QString& msg, bool ok = true);
    QString baseUrl() const;

    QNetworkAccessManager* m_nam               = nullptr;
    QNetworkReply*         m_sse               = nullptr;
    QByteArray             m_sseBuf;
    QNetworkReply*         m_createReply       = nullptr;
    QNetworkReply*         m_sendReply         = nullptr;
    QNetworkReply*         m_fetchReply        = nullptr;

    QString  m_sessionId;
    QString  m_aiMsgId;
    bool     m_inBlock    = false;
    int      m_sseErrors  = 0;
    QTimer*  m_sseRetry   = nullptr;

    /* UI */
    QLabel*       m_statusLbl  = nullptr;
    QTextBrowser* m_log        = nullptr;
    QTextEdit*    m_input      = nullptr;
    QPushButton*  m_sendBtn    = nullptr;
    QPushButton*  m_newBtn     = nullptr;
    QPushButton*  m_abortBtn   = nullptr;
    QListWidget*  m_sessList   = nullptr;
};

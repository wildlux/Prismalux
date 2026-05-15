#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QSet>
#include <QTimer>
#include "ai_client.h"

#if QT_CONFIG(ssl)
#  include <QSslServer>
#  include <QSslSocket>
#  include <QSslConfiguration>
#  include <QSslCertificate>
#  include <QSslKey>
#endif

class LanServer : public QObject {
    Q_OBJECT
public:
    explicit LanServer(AiClient* ai, QObject* parent = nullptr);
    ~LanServer();

    bool    start(quint16 port = 11500);
    void    stop();
    bool    isRunning() const;
    quint16 port()      const;
    int     clientCount() const;
    QStringList connectedIPs() const;

    /** Imposta il token Bearer (generato dalla UI se vuoto). Auth sempre richiesta se non vuoto. */
    void setAccessToken(const QString& token) { m_accessToken = token; }

    /** true se il server è in ascolto con TLS (HTTPS). */
    bool isTlsEnabled() const { return m_tlsEnabled; }

signals:
    void statusChanged(bool running);
    void clientConnected(const QString& addr);
    void clientDisconnected(const QString& addr);
    void requestHandled(const QString& method, const QString& path, const QString& addr);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    void onAiToken(const QString& chunk);
    void onAiFinished(const QString& full);
    void onAiError(const QString& msg);
    void onAiModelsReady(const QStringList& models);

private:
    struct Session {
        QTcpSocket* socket        = nullptr;
        QByteArray  buf;
        QString     addr;
        /* HTTP parse state */
        QString     method, path;
        int         contentLength = 0;
        bool        headersDone   = false;
        QByteArray  body;
        QString     authHeader;   ///< valore header Authorization (es. "Bearer TOKEN")
        /* true solo se ha chiamato almeno una API Ollama (non /apk né /) */
        bool        isApiClient   = false;
    };

    void processSession(Session& s);
    void handleTags(Session& s);
    void handleChat(Session& s);
    void handleGenerate(Session& s);
    void handleKnowledge(Session& s);
    void handleApk(Session& s);
    void handleIndex(Session& s);
    void handleWebChat(Session& s);
    void sendJson(QTcpSocket* sock, const QByteArray& json);
    void sendStreamLine(const QByteArray& json);
    void sendError(QTcpSocket* sock, int code, const QString& msg);
    void closeStreamSession();
    /** Genera header HTTP 200 con security headers; aggiunge HSTS se TLS attivo. */
    [[nodiscard]] QByteArray httpOkHeader(const char* contentType) const;
    [[nodiscard]] QByteArray httpStreamHeader() const;
    [[nodiscard]] static bool timingSafeEqual(const QString& a, const QString& b);
    static void appendAccessLog(const QString& addr, const QString& method, const QString& path);
    /** Genera certificato self-signed in ~/.prismalux/ se non esiste. Ritorna false se openssl non disponibile. */
    [[nodiscard]] static bool _ensureCert(QString& certPath, QString& keyPath);
    [[nodiscard]] bool checkChatRateLimit(Session& s); ///< true = limit exceeded (reply already sent)
    void onChatRateTimeout();            ///< reset contatore chat rate ogni 60s
    void onKnowledgeRateTimeout();       ///< reset contatore knowledge rate ogni 60s
    void onPendingTlsDisconnected();     ///< TLS handshake fallito: decrementa m_pendingTls

    QTcpServer*                m_server        = nullptr;
    bool                       m_tlsEnabled    = false;
    AiClient*                  m_ai;
    QMap<QTcpSocket*, Session> m_sessions;
    QSet<QString>              m_appClientIps; /* IP unici che hanno usato le API */

    /* Sessione corrente di streaming (una alla volta) */
    QTcpSocket* m_streamSock = nullptr;
    bool        m_genMode    = false;

    /* Fetch modelli in corso */
    QTcpSocket*             m_tagsSock  = nullptr;
    QMetaObject::Connection m_modelsConn;

    /* Rate limiting /knowledge: max 10 req/min per IP */
    QMap<QString, int> m_knowledgeReqCount;
    QTimer*            m_knowledgeRateTimer = nullptr;

    /* Rate limiting /api/chat + /api/generate: max 30 req/min per IP */
    QMap<QString, int> m_chatRateCount;
    QTimer*            m_chatRateTimer = nullptr;

    /* Token di accesso Bearer opzionale (vuoto = nessuna auth richiesta) */
    QString m_accessToken;

    static constexpr int kMaxSessions = 32;  ///< max sessioni TCP simultanee (DoS guard)
    int                m_pendingTls    = 0;  ///< connessioni TLS in attesa di handshake
};

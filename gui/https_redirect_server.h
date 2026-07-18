#pragma once
#include <QTcpServer>
#if QT_CONFIG(ssl)
#include <QSslServer>

class QTcpSocket;

/**
 * HttpsRedirectServer — QSslServer che reindirizza l'HTTP in chiaro su HTTPS.
 *
 * Un browser che apre http://IP:porta mentre il server è in TLS manda byte
 * HTTP in chiaro sulla porta TLS: con QSslServer puro l'handshake fallisce e
 * l'utente vede solo un reset di connessione. Qui il primo byte viene
 * sbirciato con recv(MSG_PEEK) PRIMA di creare qualunque socket Qt:
 *   0x16 (TLS ClientHello) → QSslServer::incomingConnection() standard;
 *   altrimenti             → QTcpSocket in chiaro che risponde
 *                            "301 Moved Permanently → https://host/path".
 *
 * Il peek a livello OS non consuma nulla: il ClientHello resta nel buffer
 * del kernel e l'handshake TLS parte intatto. Usato da LanServer e
 * Vision3DWidget al posto di QSslServer.
 */
class HttpsRedirectServer : public QSslServer {
    Q_OBJECT
public:
    explicit HttpsRedirectServer(QObject* parent = nullptr);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onProbeActivated();
    void onProbeTimeout();
    void onRedirectReadyRead();
    void onRedirectDisconnected();

private:
    void adoptPlainSocket(qintptr fd);
    void processRedirect(QTcpSocket* sock);

    static constexpr int kProbeTimeoutMs = 8000;  ///< attesa max primo byte / header completi
    static constexpr int kMaxHeaderBytes = 8192;  ///< header richiesta in chiaro max
};
#endif

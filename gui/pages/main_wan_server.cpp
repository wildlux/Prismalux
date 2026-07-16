/* ======================================================================
   main_wan_server.cpp — WAN server, client e worker di LanWanPage
   Estratto da main_lan_wan.cpp per ridurne le dimensioni.
   ====================================================================== */
#include "main_lan_wan.h"
#include "widget_ssh_manager.h"
#include "../dpi_utils.h"
#include "../lan_server.h"
#include "../prismalux_paths.h"
#include "../app_config.h"
#include "../widgets/qr_code_widget.h"
#include "../widgets/model_combo_box.h"
#include "../widgets/proc_helper.h"
#include "../log_bus.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QUuid>
#include <QFrame>
#include <QScrollArea>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QProcess>
#include <QTcpSocket>
#include <QTcpServer>
#if QT_CONFIG(ssl)
#  include <QSslServer>
#  include <QSslSocket>
#  include <QSslConfiguration>
#  include <QSslCertificate>
#  include <QSslKey>
#endif
#include <QMessageAuthenticationCode>
#include <QTimer>
#include <QPointer>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QTextCursor>
#include <QHeaderView>
#include <QDateTime>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStackedWidget>
#include <QFormLayout>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QRandomGenerator>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSplitter>
#include <QStandardItemModel>
#include <QSysInfo>
#include <QTextStream>
#include <QPainter>
#include <QPixmap>
#ifdef HAVE_QT_SQL
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#endif
namespace P = PrismaluxPaths;

static QString wanHmac(const QString& token, const QString& data)
{
    return QMessageAuthenticationCode::hash(
        data.toUtf8(), token.toUtf8(), QCryptographicHash::Sha256).toHex();
}

/* ──────────────────────────────────────────────────────────────
   WAN TLS helper — ricicla il certificato self-signed del LAN server
   (~/.prismalux/server.crt + server.key); lo genera se non esiste. */
#if QT_CONFIG(ssl)
static bool wanEnsureCert(QString& certPath, QString& keyPath)
{
    const QString dir = QDir::homePath() + "/.prismalux";
    QDir().mkpath(dir);
    certPath = dir + "/server.crt";
    keyPath  = dir + "/server.key";
    if (QFileInfo::exists(certPath) && QFileInfo::exists(keyPath))
        return true;
    const auto r = ProcHelper::run("openssl", {
        "req", "-x509", "-newkey", "rsa:2048", "-nodes",
        "-days", "3650",
        "-keyout", keyPath, "-out", certPath,
        "-subj", "/CN=Prismalux-WAN"
    }, 10000);
    if (!r.ok) return false;
    QFile::setPermissions(keyPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return QFileInfo::exists(certPath) && QFileInfo::exists(keyPath);
}
#endif

/* ══════════════════════════════════════════════════════════════
   WAN — Slot SERVER
   ══════════════════════════════════════════════════════════════ */
void LanWanPage::onWanStartBtnClicked()
{
    if (!m_wanServer) {
        m_wanServer = new QTcpServer(this);
        connect(m_wanServer, &QTcpServer::newConnection,
                this, &LanWanPage::onWanNewConnection);
    }

    if (m_wanStartBtn->isChecked()) {
        const int port = m_wanPortSpin->value();
        const bool exposeAll = m_wanExposeAllCheck && m_wanExposeAllCheck->isChecked();
        const bool wantTls   = m_wanTlsCheck        && m_wanTlsCheck->isChecked();

        /* Token obbligatorio — blocca avvio se assente o troppo corto */
        const QString wanTok = m_wanTokenEdit ? m_wanTokenEdit->text().trimmed() : QString();
        if (wanTok.length() < 8) {
            QMessageBox::critical(this,
                tr("Token obbligatorio"),
                tr("Imposta un token di almeno 8 caratteri prima di avviare il server WAN.\n\n"
                   "Senza token qualsiasi macchina in rete pu\xc3\xb2 eseguire "
                   "comandi sul tuo sistema."));
            m_wanStartBtn->setChecked(false);
            m_wanTokenEdit->setFocus();
            return;
        }

        const QHostAddress bindAddr = exposeAll ? QHostAddress::Any : QHostAddress::LocalHost;
        m_wanUseTls = false;

#if QT_CONFIG(ssl)
        if (wantTls) {
            QString certPath, keyPath;
            if (!wanEnsureCert(certPath, keyPath)) {
                QMessageBox::warning(this, tr("TLS non disponibile"),
                    tr("Impossibile generare il certificato TLS (openssl non trovato).\n"
                       "Il server avvierà in modalità TCP non cifrata."));
            } else {
                QSslCertificate cert;
                { QFile f(certPath); if (f.open(QIODevice::ReadOnly)) cert = QSslCertificate(&f, QSsl::Pem); }
                QSslKey key;
                { QFile f(keyPath);  if (f.open(QIODevice::ReadOnly)) key  = QSslKey(&f, QSsl::Rsa, QSsl::Pem); }

                if (!cert.isNull() && !key.isNull()) {
                    if (!m_wanSslServer) {
                        m_wanSslServer = new QSslServer(this);
                        connect(m_wanSslServer, &QTcpServer::newConnection,
                                this, &LanWanPage::onWanNewConnection);
                    }
                    QSslConfiguration cfg = QSslConfiguration::defaultConfiguration();
                    cfg.setLocalCertificate(cert);
                    cfg.setPrivateKey(key);
                    m_wanSslServer->setSslConfiguration(cfg);

                    if (m_wanSslServer->listen(bindAddr, static_cast<quint16>(port))) {
                        m_wanUseTls = true;
                        /* Salva fingerprint SHA-256 del cert in ~/.prismalux/wan_cert.pin
                         * I worker lo usano per verificare l'identità del server (pinning). */
                        const QString pinPath = QDir::homePath() + "/.prismalux/wan_cert.pin";
                        const QByteArray fp = cert.digest(QCryptographicHash::Sha256).toHex();
                        QSaveFile pf(pinPath);
                        if (pf.open(QIODevice::WriteOnly | QIODevice::Text)) {
                            pf.write(fp + "\n");
                            pf.commit();
                            QFile::setPermissions(pinPath,
                                QFileDevice::ReadOwner | QFileDevice::WriteOwner);
                        }
                        LogBus::post("[WAN] Cert pin: " + QString::fromLatin1(fp) +
                                     " (condividilo con i worker fuori banda)");
                    } else {
                        qWarning() << "[WAN] QSslServer listen fallito:" << m_wanSslServer->errorString()
                                   << "— fallback TCP";
                    }
                }
            }
        }
#endif

        if (!m_wanUseTls) {
            if (!m_wanServer->listen(bindAddr, static_cast<quint16>(port))) {
                m_wanStartBtn->setChecked(false);
                m_wanSrvStatusLbl->setText(
                    tr("\xe2\x9d\x8c  Errore: ") + m_wanServer->errorString());
                LogBus::post("\xe2\x9d\x8c LAN WAN: WAN server errore: " + m_wanServer->errorString());
                m_wanSrvStatusLbl->setStyleSheet("color:#f44336;");
                return;
            }
        }

        /* Avvia heartbeat 30s */
        if (!m_wanHeartbeatTimer) {
            m_wanHeartbeatTimer = new QTimer(this);
            connect(m_wanHeartbeatTimer, &QTimer::timeout,
                    this, &LanWanPage::onWanHeartbeatTick);
        }
        m_wanHeartbeatTimer->start(30000);
        onCheckOllamaExposed();
        const QString tlsTag = m_wanUseTls ? " \xf0\x9f\x94\x92 TLS" : " \xe2\x9a\xa0\xef\xb8\x8f TCP";
        if (exposeAll) {
            m_wanSrvStatusLbl->setText(
                "\xe2\x9c\x85  In ascolto su " + localLanIp() +
                ":" + QString::number(port) + tlsTag);
        } else {
            m_wanSrvStatusLbl->setText(
                "\xe2\x9c\x85  In ascolto su 127.0.0.1:" + QString::number(port) + tlsTag);
        }
        m_wanSrvStatusLbl->setStyleSheet(m_wanUseTls ? "color:#4caf50;" : "color:#ff9800;");
        m_wanPortSpin->setEnabled(false);
        m_wanExposeAllCheck->setEnabled(false);
        if (m_wanTlsCheck) m_wanTlsCheck->setEnabled(false);
    } else {
        /* Stop */
        if (m_wanHeartbeatTimer) m_wanHeartbeatTimer->stop();
        m_wanServer->close();
#if QT_CONFIG(ssl)
        if (m_wanSslServer) m_wanSslServer->close();
#endif
        for (auto& node : m_wanNodes)
            if (node.sock) node.sock->disconnectFromHost();
        m_wanNodes.clear();
        wanRefreshTables();
        updateWanStats();
        m_wanUseTls = false;
        m_wanStartBtn->setText(tr("\xe2\x96\xb6  Avvia Server"));
        m_wanSrvStatusLbl->setText(tr("\xe2\x9a\xab  Server fermo"));
        m_wanSrvStatusLbl->setStyleSheet("color:gray;");
        m_wanPortSpin->setEnabled(true);
        if (m_wanExposeAllCheck) m_wanExposeAllCheck->setEnabled(true);
        if (m_wanTlsCheck) m_wanTlsCheck->setEnabled(true);
    }
}

void LanWanPage::onCheckOllamaExposed()
{
    QProcess* proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int, QProcess::ExitStatus){
                onOllamaCheckDone(proc);
            });
    connect(proc, &QProcess::errorOccurred, proc, &QProcess::deleteLater);
    /* Niente shell: avvia ss direttamente, il filtro per porta avviene in onOllamaCheckDone */
    proc->start("ss", {"-tlnp"});
}

void LanWanPage::onOllamaCheckDone(QProcess* proc)
{
    const QString out = proc->readAllStandardOutput();
    proc->deleteLater();

    /* Check 1: Ollama in ascolto su 0.0.0.0 — verifica porta E indirizzo sulla stessa riga */
    const QString portStr = ":" + QString::number(P::kOllamaPort);
    bool exposed = false;
    for (const auto& line : out.split('\n'))
        if (line.contains(portStr) && line.contains("0.0.0.0")) { exposed = true; break; }

    /* Check 2: variabile d'ambiente OLLAMA_HOST pericolosa */
    const QString ollamaHost = qEnvironmentVariable("OLLAMA_HOST");
    if (!ollamaHost.isEmpty() && !ollamaHost.startsWith("127.") && ollamaHost != "localhost")
        exposed = true;

    if (exposed && m_wanSrvStatusLbl) {
        m_wanSrvStatusLbl->setText(
            m_wanSrvStatusLbl->text() +
            "\n\xe2\x9a\xa0\xef\xb8\x8f  Ollama esposto su rete — "
            "chiunque in LAN puo' usarlo senza token. "
            "Imposta OLLAMA_HOST=127.0.0.1 prima di avviare il server.");
        m_wanSrvStatusLbl->setStyleSheet("color:#ff9800;");
    }
}

void LanWanPage::onWanNewConnection()
{
    QTcpServer* srv = nullptr;
#if QT_CONFIG(ssl)
    if (m_wanUseTls && m_wanSslServer && m_wanSslServer->hasPendingConnections())
        srv = m_wanSslServer;
    else
#endif
    if (m_wanServer && m_wanServer->hasPendingConnections())
        srv = m_wanServer;

    while (srv && srv->hasPendingConnections()) {
        QTcpSocket* sock = srv->nextPendingConnection();
        connect(sock, &QTcpSocket::readyRead,
                this, &LanWanPage::onWanNodeReadyRead);
        connect(sock, &QTcpSocket::disconnected,
                this, &LanWanPage::onWanNodeDisconnected);
    }
}

void LanWanPage::onWanNodeReadyRead()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    while (sock->canReadLine()) {
        const QByteArray line = sock->readLine().trimmed();
        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &jerr);
        if (!doc.isObject()) continue;
        const QJsonObject msg = doc.object();
        const QString type    = msg["t"].toString();

        if (type == "hello") {
            /* Rate limiting: blocca IP dopo 5 token errati per 60 secondi */
            const QString peerIp = sock->peerAddress().toString();
            {
                auto& entry = m_wanBadTokens[peerIp];
                if (entry.blockedUntil.isValid() && QDateTime::currentDateTime() < entry.blockedUntil) {
                    wanSendJson(sock, QJsonObject{{"t","error"},{"msg","rate_limited"}});
                    sock->disconnectFromHost();
                    LogBus::post("[WAN] IP bloccato (rate limit): " + peerIp);
                    continue;
                }
            }

            /* Verifica token server — rifiuta nodi non autenticati */
            const QString serverToken = m_wanTokenEdit ? m_wanTokenEdit->text().trimmed() : QString();
            if (!serverToken.isEmpty()) {
                const QString presented = msg["token"].toString();
                if (!LanServer::timingSafeEqual(presented, serverToken)) {
                    auto& entry = m_wanBadTokens[peerIp];
                    ++entry.count;
                    if (entry.count >= 5) {
                        entry.blockedUntil = QDateTime::currentDateTime().addSecs(60);
                        entry.count = 0;
                        LogBus::post("[WAN] Rate limit attivato per " + peerIp +
                                     " (5 token errati consecutivi — blocco 60s)");
                    }
                    wanSendJson(sock, QJsonObject{{"t","error"},{"msg","auth_failed"}});
                    sock->disconnectFromHost();
                    continue;
                }
                /* Token corretto → azzera contatore fallimenti */
                m_wanBadTokens.remove(peerIp);
            }

            /* Registrazione nuovo nodo */
            WanNode node;
            node.id     = wanNextId();
            node.name   = msg["name"].toString("nodo-" + node.id);
            node.ip     = sock->peerAddress().toString();
            node.status = "idle";
            node.sock   = sock;
            const QJsonArray caps = msg["caps"].toArray();
            for (const auto& c : caps) node.caps.append(c.toString());
            /* Default: solo "ai" — "shell" è opt-in esplicito lato worker */
            if (node.caps.isEmpty()) node.caps << "ai";
            m_wanNodes.push_back(node);
            wanSendJson(sock, QJsonObject{{"t","welcome"},{"node_id", node.id}});
            wanRefreshTables();
            wanDispatch();

        } else if (type == "pong") {
            /* Risposta a heartbeat ping — aggiorna lastSeen */
            auto nodeIt = std::find_if(m_wanNodes.begin(), m_wanNodes.end(),
                [sock](const WanNode& n){ return n.sock == sock; });
            if (nodeIt != m_wanNodes.end()) {
                nodeIt->lastSeen    = QDateTime::currentDateTime();
                nodeIt->missedPings = 0;
            }

        } else if (type == "poll") {
            /* Client chiede un task — usa il priority scheduler */
            auto nodeIt = std::find_if(m_wanNodes.begin(), m_wanNodes.end(),
                [sock](const WanNode& n){ return n.sock == sock; });
            if (nodeIt == m_wanNodes.end()) { wanSendJson(sock, {{"t","idle"}}); continue; }
            nodeIt->status   = "idle";
            nodeIt->lastSeen = QDateTime::currentDateTime();
            wanDispatch();
            /* Se il nodo è rimasto idle (nessun task), notificalo */
            if (nodeIt->status == "idle")
                wanSendJson(sock, {{"t","idle"}});

        } else if (type == "result") {
            /* Risultato da un nodo — verifica HMAC se token impostato */
            const QString id     = msg["id"].toString();
            const QString status = msg["status"].toString("done");
            const QString result = msg["result"].toString();
            const QString srvToken2 = m_wanTokenEdit ? m_wanTokenEdit->text().trimmed() : QString();
            if (!srvToken2.isEmpty() && msg.contains("hmac")) {
                const QString expected = wanHmac(srvToken2, id + "|" + result);
                if (!LanServer::timingSafeEqual(msg["hmac"].toString(), expected)) {
                    LogBus::post("[WAN] HMAC risultato non valido per task " + id +
                                 " — risultato scartato (possibile manomissione)");
                    wanSendJson(sock, QJsonObject{{"t","error"},{"msg","hmac_invalid"}});
                    continue;
                }
            }
            auto taskIt = std::find_if(m_wanTasks.begin(), m_wanTasks.end(),
                [&id](const WanTask& t){ return t.id == id; });
            if (taskIt != m_wanTasks.end()) {
                taskIt->status = status;
                taskIt->result = result;
                if (status == "done") m_wanCompletedTs.append(QDateTime::currentDateTime());
            }
            auto nodeIt = std::find_if(m_wanNodes.begin(), m_wanNodes.end(),
                [sock](const WanNode& n){ return n.sock == sock; });
            if (nodeIt != m_wanNodes.end()) {
                nodeIt->status   = "idle";
                nodeIt->lastSeen = QDateTime::currentDateTime();
            }
            wanSendJson(sock, QJsonObject{{"t","ack"},{"id",id}});
            wanRefreshTables();
            updateWanStats();
            wanDispatch();

        } else if (type == "spawn_tasks") {
            /* Un agente llm_agent chiede di spawnare sub-agenti.
             * Crea un WanTask llm_agent per ogni entry nell'array "tasks". */
            const QString parentId = msg["parent_id"].toString();
            const QString chainId  = msg["chain_id"].toString();
            const QJsonArray tasks = msg["tasks"].toArray();
            if (tasks.size() > 10) {
                wanSendJson(sock, QJsonObject{{"t","error"},{"msg","too_many_subtasks"}});
                continue;
            }
            int spawned = 0;
            for (const auto& v : tasks) {
                if (!v.isObject()) continue;
                const QJsonObject t = v.toObject();
                WanTask sub;
                sub.id      = wanNextId();
                sub.kind    = t["kind"].toString("llm_agent");
                sub.payload = t["payload"].toString();
                sub.status  = "pending";
                sub.created = QDateTime::currentDateTime();
                // Annotazione nella tabella: mostra relazione parent → child
                sub.node    = QString("spawned-by:%1").arg(parentId);
                m_wanTasks.push_back(sub);
                spawned++;
            }
            wanSendJson(sock, QJsonObject{{"t","ack"},{"id",parentId}});
            wanRefreshTables();
            wanDispatch();
            // Log nel cron se attivo
            if (spawned > 0)
                wanLogCron(QString("[chain:%1] %2 sub-agente/i spawnati da %3")
                           .arg(chainId).arg(spawned).arg(parentId));
        }
    }
}

void LanWanPage::wanReassignNodeTasks(const QString& nodeName)
{
    for (auto& task : m_wanTasks) {
        if (task.status == "running" && task.node == nodeName) {
            task.retryCount++;
            if (task.retryCount >= 3) {
                task.status = "failed";
                wanLogCron(QString("[fault] task %1 fallito dopo 3 tentativi").arg(task.id));
            } else {
                task.status = "pending";
                task.node.clear();
                wanLogCron(QString("[fault] task %1 riassegnato (tentativo %2/3)")
                           .arg(task.id).arg(task.retryCount));
            }
        }
    }
}

void LanWanPage::onWanNodeDisconnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    auto nodeIt = std::find_if(m_wanNodes.begin(), m_wanNodes.end(),
        [sock](const WanNode& n){ return n.sock == sock; });
    if (nodeIt != m_wanNodes.end()) {
        wanReassignNodeTasks(nodeIt->name);
        wanLogCron(QString("[nodo] %1 disconnesso").arg(nodeIt->name));
    }
    m_wanNodes.erase(
        std::remove_if(m_wanNodes.begin(), m_wanNodes.end(),
            [sock](const WanNode& n){ return n.sock == sock; }),
        m_wanNodes.end());
    wanRefreshTables();
    updateWanStats();
    wanDispatch();
}

void LanWanPage::onWanHeartbeatTick()
{
    const QDateTime now = QDateTime::currentDateTime();
    for (auto& node : m_wanNodes) {
        if (!node.sock || node.sock->state() != QAbstractSocket::ConnectedState) continue;
        wanSendJson(node.sock, {{"t", "ping"}});
        /* Se lastSeen non è mai stato settato, inizializzalo ora */
        if (!node.lastSeen.isValid()) { node.lastSeen = now; continue; }
        /* Nodo silenzioso per >90s (3 cicli da 30s) → considerato morto */
        if (node.lastSeen.secsTo(now) > 90) {
            node.missedPings++;
            if (node.missedPings >= 3) {
                wanLogCron(QString("[heartbeat] nodo %1 non risponde — rimosso").arg(node.name));
                node.sock->disconnectFromHost(); /* onWanNodeDisconnected gestisce riassegnazione */
            }
        }
    }
}

void LanWanPage::onWanDashTick()
{
    updateWanThroughput();
}

void LanWanPage::updateWanThroughput()
{
    if (!m_wanThroughputLbl) return;
    const QDateTime cutoff = QDateTime::currentDateTime().addSecs(-3600);
    /* Rimuovi timestamp vecchi */
    m_wanCompletedTs.erase(
        std::remove_if(m_wanCompletedTs.begin(), m_wanCompletedTs.end(),
            [&cutoff](const QDateTime& dt){ return dt < cutoff; }),
        m_wanCompletedTs.end());
    const int perHour = m_wanCompletedTs.size();

    /* ETA: stima durata media dei task completati con startedAt valido */
    double avgSec = 0.0;
    int counted = 0;
    for (const auto& t : m_wanTasks) {
        if ((t.status == "done" || t.status == "error") && t.startedAt.isValid()) {
            avgSec += t.startedAt.msecsTo(QDateTime::currentDateTime()) / 1000.0;
            counted++;
        }
    }
    int pending = 0;
    for (const auto& t : m_wanTasks) if (t.status == "pending") pending++;
    int working = 0;
    for (const auto& n : m_wanNodes) if (n.status == "working") working++;
    const int effective = qMax(1, working);

    QString etaTxt;
    if (counted > 0 && pending > 0) {
        avgSec /= counted;
        const double etaSec = (avgSec * pending) / effective;
        if (etaSec < 60)       etaTxt = QString("  ETA ~%1s").arg(qRound(etaSec));
        else if (etaSec < 3600) etaTxt = QString("  ETA ~%1min").arg(qRound(etaSec/60));
        else                   etaTxt = QString("  ETA ~%1h").arg(etaSec/3600, 0, 'f', 1);
    }

    m_wanThroughputLbl->setText(
        QString("\xf0\x9f\x93\x88  Throughput: <b>%1</b> task/ora%2")
        .arg(perHour).arg(etaTxt));

    /* ── Istogramma throughput — 12 bin da 5 min (ultimi 60 min) ── */
    if (!m_wanChartWidget) return;
    const int kBins   = 12;
    const int kBinSec = 300; // 5 minuti
    const int chartW  = m_wanChartWidget->width();
    const int chartH  = m_wanChartWidget->height();
    QPixmap pix(chartW, chartH);
    pix.fill(QColor(30, 41, 59));   // sfondo scuro coerente con dark theme

    // Conta i task per bin
    QVector<int> bins(kBins, 0);
    const QDateTime now = QDateTime::currentDateTime();
    for (const QDateTime& ts : m_wanCompletedTs) {
        const qint64 secsAgo = ts.secsTo(now);
        if (secsAgo < 0 || secsAgo >= kBins * kBinSec) continue;
        const int binIdx = static_cast<int>(secsAgo / kBinSec);
        // binIdx 0 = bin più recente (destra), invertiamo sotto
        if (binIdx >= 0 && binIdx < kBins)
            bins[kBins - 1 - binIdx]++;
    }
    const int maxVal = *std::max_element(bins.constBegin(), bins.constEnd());

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int padL = dpiScale(4);
    const int padR = dpiScale(4);
    const int padT = dpiScale(4);
    const int padB = dpiScale(14); // spazio etichette asse X
    const int innerW = chartW - padL - padR;
    const int innerH = chartH - padT - padB;
    const int spacing = dpiScale(2);
    const int barW = (innerW - spacing * (kBins - 1)) / kBins;

    // Linea base
    p.setPen(QColor(71, 85, 105));
    p.drawLine(padL, chartH - padB, chartW - padR, chartH - padB);

    // Barre
    for (int i = 0; i < kBins; i++) {
        const int x = padL + i * (barW + spacing);
        const int barH = (maxVal > 0)
            ? static_cast<int>((static_cast<double>(bins[i]) / maxVal) * innerH)
            : 0;
        const int y = padT + innerH - barH;
        if (barH > 0) {
            // Gradiente verde: più piena = più luminosa
            const int green = 120 + static_cast<int>(100.0 * bins[i] / qMax(1, maxVal));
            p.fillRect(x, y, barW, barH, QColor(34, green, 80));
        } else {
            // Barra vuota — segnalino
            p.fillRect(x, chartH - padB - dpiScale(2), barW, dpiScale(2),
                       QColor(50, 65, 85));
        }
        // Etichetta ogni 4 bin: "-60", "-40", "-20", "0"
        if (i == 0 || i == 3 || i == 6 || i == 9 || i == 11) {
            const int minsAgo = (kBins - i) * 5;
            p.setPen(QColor(148, 163, 184));
            p.setFont(QFont("monospace", dpiScale(7)));
            const QString lbl = (i == 11) ? "0" : QString("-%1").arg(minsAgo);
            p.drawText(x, chartH - padB + dpiScale(2),
                       barW + spacing, padB - dpiScale(1),
                       Qt::AlignLeft | Qt::AlignTop, lbl);
        }
    }
    p.end();
    m_wanChartWidget->setPixmap(pix);
}

void LanWanPage::onWanExportCsvClicked()
{
    exportWanCsv();
}

void LanWanPage::exportWanCsv()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Esporta task WAN", QDir::homePath() + "/wan_tasks.csv",
        "CSV (*.csv)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    out << "id,kind,stato,nodo,retry,priorita,created,startedAt,payload,result\n";
    for (const auto& t : m_wanTasks) {
        auto esc = [](const QString& s) -> QString {
            return "\"" + QString(s).replace("\"", "\"\"").replace("\n", " ") + "\"";
        };
        out << esc(t.id) << "," << esc(t.kind) << "," << esc(t.status)
            << "," << esc(t.node) << "," << t.retryCount << "," << t.priority
            << "," << esc(t.created.toString(Qt::ISODate))
            << "," << esc(t.startedAt.isValid() ? t.startedAt.toString(Qt::ISODate) : "")
            << "," << esc(t.payload.left(200))
            << "," << esc(t.result.left(500)) << "\n";
    }
}

void LanWanPage::updateWanStats()
{
    if (!m_wanStatsLbl) return;
    int idle = 0, working = 0;
    for (const auto& n : m_wanNodes) {
        if (n.status == "idle")    idle++;
        else if (n.status == "working") working++;
    }
    int pending = 0, running = 0, done = 0, failed = 0;
    for (const auto& t : m_wanTasks) {
        if      (t.status == "pending") pending++;
        else if (t.status == "running") running++;
        else if (t.status == "done")    done++;
        else if (t.status == "failed" || t.status == "error") failed++;
    }
    m_wanStatsLbl->setText(
        QString("\xf0\x9f\x96\xa5  Nodi: <b>%1</b> idle / <b>%2</b> working"
                "  \xe2\x80\x94  "
                "\xf0\x9f\x93\x8b  Task: <b>%3</b> in coda / <b>%4</b> running"
                " / <b>%5</b> completati / <b>%6</b> falliti")
        .arg(idle).arg(working).arg(pending).arg(running).arg(done).arg(failed));
}

void LanWanPage::onWanAddTaskBtnClicked()
{
    const QString kind = m_wanTaskKind ? m_wanTaskKind->currentData().toString() : "ai_query";
    QString payload;

    if (kind == "llm_agent" && m_wanPayloadStack && m_wanPayloadStack->currentIndex() == 1) {
        /* Costruisce JSON dai campi del form */
        const QString role   = m_agentRoleEdit    ? m_agentRoleEdit->text().trimmed()           : QString();
        const QString prompt = m_agentPromptEdit  ? m_agentPromptEdit->toPlainText().trimmed()   : QString();
        const QString ctx    = m_agentContextEdit ? m_agentContextEdit->toPlainText().trimmed()  : QString();
        if (role.isEmpty() || prompt.isEmpty()) return;
        QJsonObject obj;
        obj["role"]     = role;
        obj["prompt"]   = prompt;
        obj["context"]  = ctx;
        obj["depth"]    = 0;
        obj["chain_id"] = "";
        payload = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    } else {
        payload = m_wanTaskPayload ? m_wanTaskPayload->toPlainText().trimmed() : QString();
    }

    if (payload.isEmpty()) return;
    WanTask t;
    t.id      = wanNextId();
    t.kind    = kind;
    t.payload = payload;
    t.status  = "pending";
    t.created = QDateTime::currentDateTime();
    m_wanTasks.push_back(t);
    wanRefreshTables();
    wanDispatch();
}

/* ── Cron ── */
void LanWanPage::onWanCronStartBtnClicked()
{
    const QString payload = m_wanCronPayload ? m_wanCronPayload->toPlainText().trimmed() : QString();
    if (payload.isEmpty()) {
        wanLogCron("Imposta prima il payload del task cron.");
        return;
    }
    if (!m_wanCronTimer) {
        m_wanCronTimer = new QTimer(this);
        connect(m_wanCronTimer, &QTimer::timeout,
                this, &LanWanPage::onWanCronFired);
    }
    const int ms = (m_wanCronInterval ? m_wanCronInterval->value() : 5) * 60 * 1000;
    m_wanCronTimer->start(ms);
    m_wanCronStartBtn->setEnabled(false);
    m_wanCronStopBtn->setEnabled(true);
    wanLogCron(QString("Cron avviato — ogni %1 min.")
               .arg(m_wanCronInterval ? m_wanCronInterval->value() : 5));
}

void LanWanPage::onWanCronStopBtnClicked()
{
    if (m_wanCronTimer) m_wanCronTimer->stop();
    m_wanCronStartBtn->setEnabled(true);
    m_wanCronStopBtn->setEnabled(false);
    wanLogCron("Cron fermato.");
}

void LanWanPage::onWanCronFired()
{
    const QString payload = m_wanCronPayload ? m_wanCronPayload->toPlainText().trimmed() : QString();
    if (payload.isEmpty()) return;
    WanTask t;
    t.id      = wanNextId();
    t.kind    = m_wanCronKind ? m_wanCronKind->currentData().toString() : "ai_query";
    t.payload = payload;
    t.status  = "pending";
    t.created = QDateTime::currentDateTime();
    m_wanTasks.push_back(t);
    wanRefreshTables();
    wanDispatch();
    wanLogCron("Task " + t.id + " aggiunto automaticamente (" + t.kind + ").");
}

/* ══════════════════════════════════════════════════════════════
   WAN — Slot CLIENT
   ══════════════════════════════════════════════════════════════ */
void LanWanPage::onWanCliConBtnClicked()
{
    const QString host  = m_wanCliHost ? m_wanCliHost->text().trimmed() : QString();
    const int     port  = m_wanCliPort ? m_wanCliPort->value() : P::kWanComputePort;
    const int     nWork = m_wanCliWorkerSpin ? m_wanCliWorkerSpin->value() : 1;
    if (host.isEmpty()) {
        wanCliAppendLog("Inserisci l'IP del server WAN.");
        return;
    }

    /* Disconnetti eventuali worker esistenti prima di ricrearli */
    if (!m_wanWorkers.isEmpty()) {
        onWanCliDisconBtnClicked();
        return;
    }

    const QString baseName = (m_wanCliName && !m_wanCliName->text().trimmed().isEmpty())
                             ? m_wanCliName->text().trimmed()
                             : QSysInfo::machineHostName();
    const QString token    = m_wanCliTokenEdit ? m_wanCliTokenEdit->text().trimmed() : QString();
    const bool    shell    = m_wanCliShellCheck && m_wanCliShellCheck->isChecked();
    const bool    useTls   = m_wanCliTlsCheck   && m_wanCliTlsCheck->isChecked();

    m_wanCliStatusLbl->setText(tr("\xe2\x8f\xb3  Connessione\xe2\x80\xa6"));
    m_wanCliStatusLbl->setStyleSheet("color:#E5C400;");
    m_wanCliConBtn->setEnabled(false);

    m_wanWorkers.resize(nWork);

    for (int i = 0; i < nWork; ++i) {
        WanWorker& w = m_wanWorkers[i];

        /* Socket — TCP plain o TLS secondo impostazione utente */
        QTcpSocket* sock = nullptr;
#if QT_CONFIG(ssl)
        if (useTls) {
            auto* ssl = new QSslSocket(this);
            ssl->setPeerVerifyMode(QSslSocket::VerifyNone); /* self-signed OK */
            sock = ssl;
        } else {
            sock = new QTcpSocket(this);
        }
#else
        sock = new QTcpSocket(this);
#endif
        w.sock = sock;
        connect(sock, &QTcpSocket::readyRead,
                this, &LanWanPage::onWanWorkerReadyRead);
        connect(sock, &QTcpSocket::disconnected,
                this, &LanWanPage::onWanWorkerDisconnected);
        connect(sock,
                QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
                this, &LanWanPage::onWanWorkerError);

        /* AiClient dedicato — copia configurazione backend dal m_ai principale */
        auto* ai = new AiClient(this);
        if (m_ai) {
            ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_ai->model());
        }
        w.ai = ai;

        /* Poll timer */
        auto* timer = new QTimer(this);
        w.pollTimer = timer;
        connect(timer, &QTimer::timeout, this, &LanWanPage::onWanWorkerPoll);

        /* Salva metadati nel worker: usati da onWanWorkerConnected */
        w.name         = (nWork == 1)
                         ? baseName
                         : baseName + "-worker" + QString::number(i + 1);
        w.token        = token;
        w.shellAllowed = shell;

        connect(sock, &QTcpSocket::connected,
                this, &LanWanPage::onWanWorkerConnected);

#if QT_CONFIG(ssl)
        if (auto* ssl = qobject_cast<QSslSocket*>(sock)) {
            /* encrypted() è emesso dopo l'handshake TLS — equivale a connected() per TCP */
            connect(ssl, &QSslSocket::encrypted,
                    this, &LanWanPage::onWanWorkerConnected);

            /* Certificate pinning: verifica il fingerprint del server se pin file presente */
            connect(ssl, &QSslSocket::sslErrors, ssl,
                    [ssl](const QList<QSslError>& errors) {
                        const QString pinPath = QDir::homePath() + "/.prismalux/wan_server.pin";
                        QFile pf(pinPath);
                        if (!pf.open(QIODevice::ReadOnly)) {
                            /* Nessun pin file: accetta self-signed con avviso */
                            ssl->ignoreSslErrors();
                            return;
                        }
                        const QByteArray expectedPin = pf.readAll().trimmed();
                        const QSslCertificate serverCert = ssl->peerCertificate();
                        const QByteArray actualPin = serverCert.digest(
                            QCryptographicHash::Sha256).toHex();
                        if (actualPin == expectedPin) {
                            ssl->ignoreSslErrors(); /* cert self-signed ma pin corretto */
                        } else {
                            /* Pin non corrisponde → disconnetti (possibile MITM) */
                            LogBus::post(
                                "[WAN] CERT PIN MISMATCH — connessione rifiutata (possibile MITM)!\n"
                                "Atteso: " + QString::fromLatin1(expectedPin) + "\n"
                                "Ricevuto: " + QString::fromLatin1(actualPin));
                            ssl->abort();
                        }
                        Q_UNUSED(errors)
                    });

            ssl->connectToHostEncrypted(host, static_cast<quint16>(port));
        } else {
            sock->connectToHost(host, static_cast<quint16>(port));
        }
#else
        sock->connectToHost(host, static_cast<quint16>(port));
#endif
    }

    /* Alias legacy → worker[0] per retrocompatibilità (es. onWanSimBtnClicked) */
    m_wanCliSock      = m_wanWorkers[0].sock.data();
    m_wanCliPollTimer = m_wanWorkers[0].pollTimer.data();
}

void LanWanPage::onWanCliDisconBtnClicked()
{
    /* Ferma e distrugge tutti i worker */
    for (WanWorker& w : m_wanWorkers) {
        if (w.pollTimer) { w.pollTimer->stop(); w.pollTimer->deleteLater(); }
        if (w.ai)        { w.ai->abort();       w.ai->deleteLater(); }
        if (w.sock)      { w.sock->disconnectFromHost(); w.sock->deleteLater(); }
        QObject::disconnect(w.tokConn);
        QObject::disconnect(w.finConn);
        QObject::disconnect(w.errConn);
    }
    m_wanWorkers.clear();

    /* Reset alias legacy */
    m_wanCliSock        = nullptr;
    m_wanCliPollTimer   = nullptr;
    m_wanCliCurrentTask.clear();
    m_wanCliAiActive  = false;

    m_wanCliConBtn->setEnabled(true);
    m_wanCliDisconBtn->setEnabled(false);
    m_wanCliStatusLbl->setText(tr("\xe2\x9a\xab  Non connesso"));
    m_wanCliStatusLbl->setStyleSheet("color:gray;");
}

void LanWanPage::onWanCliPoll()
{
    /* Legacy slot — usato dalla simulazione locale (worker 0) */
    if (m_wanCliAiActive) return;
    if (!m_wanCliCurrentTask.isEmpty()) return;
    wanCliSendJson({{"t","poll"}});
}

void LanWanPage::onWanCliSockReadyRead()
{
    if (!m_wanCliSock) return;
    while (m_wanCliSock->canReadLine()) {
        const QByteArray line = m_wanCliSock->readLine().trimmed();
        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &jerr);
        if (!doc.isObject()) continue;
        const QJsonObject msg = doc.object();
        const QString type    = msg["t"].toString();

        if (type == "welcome") {
            m_wanCliNodeId = msg["node_id"].toString();
            wanCliAppendLog("Registrato come nodo " + m_wanCliNodeId);

        } else if (type == "idle") {
            /* nessun task — polling automatico via timer */

        } else if (type == "task") {
            const QString id      = msg["id"].toString();
            const QString kind    = msg["kind"].toString();
            const QString payload = msg["payload"].toString();
            m_wanCliCurrentTask   = id;
            wanCliAppendLog(QString("Task ricevuto [%1] tipo: %2").arg(id, kind));
            wanCliHandleTask(id, kind, payload);

        } else if (type == "ack") {
            wanCliAppendLog("Server ha ricevuto risultato " + msg["id"].toString());
        }
    }
}

void LanWanPage::onWanCliSockDisconnected()
{
    if (m_wanCliPollTimer) m_wanCliPollTimer->stop();
    m_wanCliConBtn->setEnabled(true);
    m_wanCliDisconBtn->setEnabled(false);
    m_wanCliStatusLbl->setText(tr("\xe2\x9a\xab  Disconnesso"));
    m_wanCliStatusLbl->setStyleSheet("color:gray;");
    wanCliAppendLog("Disconnesso dal server.");
    m_wanCliCurrentTask.clear();
    m_wanCliAiActive = false;
}

void LanWanPage::onWanCliSockError(QAbstractSocket::SocketError)
{
    const QString msg = m_wanCliSock ? m_wanCliSock->errorString() : "errore sconosciuto";
    m_wanCliStatusLbl->setText(tr("\xe2\x9d\x8c  ") + msg);
    LogBus::post("\xe2\x9d\x8c LAN WAN: WAN client errore socket: " + msg);
    m_wanCliStatusLbl->setStyleSheet("color:#f44336;");
    m_wanCliConBtn->setEnabled(true);
    m_wanCliDisconBtn->setEnabled(false);
    wanCliAppendLog("Errore socket: " + msg);
}

/* ── AI execution sul client ── */
void LanWanPage::onWanCliAiToken(const QString& t)
{
    m_wanCliAiBuf.append(t);
}

void LanWanPage::onWanCliAiFinished(const QString&)
{
    QObject::disconnect(m_wanCliTokenConn);
    QObject::disconnect(m_wanCliFinishedConn);
    QObject::disconnect(m_wanCliErrorConn);
    m_wanCliTokenConn = m_wanCliFinishedConn = m_wanCliErrorConn = {};
    m_wanCliAiActive = false;

    const QString raw = m_wanCliAiBuf.trimmed();

    /* ── llm_agent: prova a parsare JSON con "result" + "spawn" opzionale ── */
    if (m_wanCliIsAgentTask) {
        m_wanCliIsAgentTask = false;
        const int     depth   = m_wanCliAgentDepth;
        const QString chainId = m_wanCliAgentChain;
        m_wanCliAgentDepth = 0;
        m_wanCliAgentChain.clear();

        // Estrai il blocco JSON dalla risposta (l'LLM può aggiungere testo prima/dopo)
        QString jsonCandidate;
        const int braceOpen  = raw.indexOf('{');
        const int braceClose = raw.lastIndexOf('}');
        if (braceOpen != -1 && braceClose > braceOpen)
            jsonCandidate = raw.mid(braceOpen, braceClose - braceOpen + 1);

        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(jsonCandidate.toUtf8(), &jerr);
        const bool hasSpawn = doc.isObject()
                           && doc.object().contains("result")
                           && doc.object()["spawn"].isArray()
                           && !doc.object()["spawn"].toArray().isEmpty();

        const QString result = hasSpawn
            ? doc.object()["result"].toString(raw)
            : raw;

        // Invia risultato al master (con HMAC se token impostato)
        {
            const QString cliTok = m_wanCliTokenEdit ? m_wanCliTokenEdit->text().trimmed() : QString();
            QJsonObject resMsg{{"t","result"}, {"id", m_wanCliCurrentTask},
                               {"status","done"}, {"result", result}};
            if (!cliTok.isEmpty())
                resMsg["hmac"] = wanHmac(cliTok, m_wanCliCurrentTask + "|" + result);
            wanCliSendJson(resMsg);
        }

        // Se ci sono sub-agenti da spawnare, informa il master
        if (hasSpawn) {
            const QJsonArray spawnArr = doc.object()["spawn"].toArray();
            QJsonArray tasks;
            for (const auto& v : spawnArr) {
                if (!v.isObject()) continue;
                const QJsonObject s = v.toObject();
                QJsonObject taskPayload{
                    {"role",     s["role"].toString("Assistente")},
                    {"prompt",   s["prompt"].toString()},
                    {"context",  result},
                    {"depth",    depth + 1},
                    {"chain_id", chainId}
                };
                tasks.append(QJsonObject{
                    {"kind",    "llm_agent"},
                    {"payload", QString::fromUtf8(
                        QJsonDocument(taskPayload).toJson(QJsonDocument::Compact))}
                });
            }
            wanCliSendJson(QJsonObject{
                {"t",          "spawn_tasks"},
                {"parent_id",  m_wanCliCurrentTask},
                {"chain_id",   chainId},
                {"tasks",      tasks}
            });
            wanCliAppendLog(QString("Agente ha richiesto %1 sub-agente/i.").arg(tasks.size()));
        }

        wanCliAppendLog("Task " + m_wanCliCurrentTask + " completato (agente).");
        m_wanCliCurrentTask.clear();
        m_wanCliAiBuf.clear();
        return;
    }

    wanCliSendJson(QJsonObject{
        {"t","result"},
        {"id", m_wanCliCurrentTask},
        {"status","done"},
        {"result", raw}
    });
    wanCliAppendLog("Task " + m_wanCliCurrentTask + " completato (AI).");
    m_wanCliCurrentTask.clear();
    m_wanCliAiBuf.clear();
}

void LanWanPage::onWanCliAiError(const QString& msg)
{
    QObject::disconnect(m_wanCliTokenConn);
    QObject::disconnect(m_wanCliFinishedConn);
    QObject::disconnect(m_wanCliErrorConn);
    m_wanCliTokenConn = m_wanCliFinishedConn = m_wanCliErrorConn = {};
    m_wanCliAiActive     = false;
    m_wanCliIsAgentTask  = false;
    m_wanCliAgentDepth   = 0;
    m_wanCliAgentChain.clear();

    wanCliSendJson(QJsonObject{
        {"t","result"},
        {"id", m_wanCliCurrentTask},
        {"status","error"},
        {"result", msg}
    });
    wanCliAppendLog("Task " + m_wanCliCurrentTask + " errore AI: " + msg);
    m_wanCliCurrentTask.clear();
}

/* ══════════════════════════════════════════════════════════════════════════
   Multi-worker helper — trova l'indice del worker dato sender()
   ══════════════════════════════════════════════════════════════════════════ */
int LanWanPage::wanWorkerIndex(QObject* obj) const
{
    for (int i = 0; i < m_wanWorkers.size(); ++i) {
        const WanWorker& w = m_wanWorkers[i];
        if (w.sock    && static_cast<QObject*>(w.sock.data())    == obj) return i;
        if (w.pollTimer && static_cast<QObject*>(w.pollTimer.data()) == obj) return i;
        if (w.ai      && static_cast<QObject*>(w.ai.data())      == obj) return i;
    }
    return -1;
}

void LanWanPage::wanWorkerSendJson(int idx, const QJsonObject& obj)
{
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    QTcpSocket* s = m_wanWorkers[idx].sock;
    if (s) s->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
}

void LanWanPage::wanWorkerAppendLog(int idx, const QString& msg)
{
    const QString prefix = (m_wanWorkers.size() > 1)
        ? QString("[W%1]").arg(idx + 1) : QString();
    wanCliAppendLog(prefix + msg);
}

/* ── Per-worker connected — invia hello e avvia polling ── */
void LanWanPage::onWanWorkerConnected()
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    WanWorker& w = m_wanWorkers[idx];
    if (!w.sock) return;

    /* Invia hello con nome, token e caps */
    QJsonArray caps{"ai"};
    if (w.shellAllowed) caps.append("shell");
    wanWorkerSendJson(idx, QJsonObject{
        {"t",     "hello"},
        {"name",  w.name},
        {"token", w.token},
        {"caps",  caps}
    });

    /* Solo worker 0 aggiorna lo stato UI generale */
    if (idx == 0) {
        m_wanCliStatusLbl->setText(tr("\xe2\x9c\x85  Connesso — in attesa task"));
        m_wanCliStatusLbl->setStyleSheet("color:#4caf50;");
        m_wanCliConBtn->setEnabled(false);
        m_wanCliDisconBtn->setEnabled(true);
        wanCliAppendLog(QString("Pool %1 worker connesso a %2:%3")
            .arg(m_wanWorkers.size())
            .arg(w.sock->peerAddress().toString())
            .arg(w.sock->peerPort()));
    } else {
        wanWorkerAppendLog(idx, QString("Worker %1 connesso.").arg(idx + 1));
    }

    /* Avvia il poll timer di questo worker */
    if (w.pollTimer) w.pollTimer->start(5000);
}

/* ── Per-worker poll ── */
void LanWanPage::onWanWorkerPoll()
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    const WanWorker& w = m_wanWorkers[idx];
    if (w.aiActive)            return;  // occupato
    if (!w.currentTask.isEmpty()) return;
    wanWorkerSendJson(idx, {{"t","poll"}});
}

/* ── Per-worker readyRead ── */
void LanWanPage::onWanWorkerReadyRead()
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    WanWorker& w = m_wanWorkers[idx];
    if (!w.sock) return;

    while (w.sock->canReadLine()) {
        const QByteArray line = w.sock->readLine().trimmed();
        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &jerr);
        if (!doc.isObject()) continue;
        const QJsonObject msg  = doc.object();
        const QString     type = msg["t"].toString();

        if (type == "welcome") {
            w.nodeId = msg["node_id"].toString();
            wanWorkerAppendLog(idx, "Registrato come nodo " + w.nodeId);

        } else if (type == "idle") {
            /* nessun task — polling automatico via timer */

        } else if (type == "task") {
            const QString id      = msg["id"].toString();
            const QString kind    = msg["kind"].toString();
            const QString payload = msg["payload"].toString();
            w.currentTask = id;
            wanWorkerAppendLog(idx,
                QString("Task ricevuto [%1] tipo: %2").arg(id, kind));
            wanWorkerHandleTask(idx, id, kind, payload);

        } else if (type == "ack") {
            wanWorkerAppendLog(idx,
                "Server ha ricevuto risultato " + msg["id"].toString());
        }
    }

    /* Aggiorna alias legacy dal worker 0 */
    if (idx == 0) {
        m_wanCliCurrentTask = m_wanWorkers[0].currentTask;
        m_wanCliAiActive    = m_wanWorkers[0].aiActive;
        m_wanCliNodeId      = m_wanWorkers[0].nodeId;
    }
}

/* ── Per-worker disconnected ── */
void LanWanPage::onWanWorkerDisconnected()
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    WanWorker& w = m_wanWorkers[idx];

    if (w.pollTimer) w.pollTimer->stop();
    w.currentTask.clear();
    w.aiActive = false;
    wanWorkerAppendLog(idx, "Disconnesso dal server.");

    /* Se tutti i worker sono disconnessi, aggiorna la UI */
    bool anyConnected = false;
    for (const WanWorker& ww : std::as_const(m_wanWorkers)) {
        if (ww.sock && ww.sock->state() == QAbstractSocket::ConnectedState) {
            anyConnected = true;
            break;
        }
    }
    if (!anyConnected) {
        m_wanCliConBtn->setEnabled(true);
        m_wanCliDisconBtn->setEnabled(false);
        m_wanCliStatusLbl->setText(tr("\xe2\x9a\xab  Disconnesso"));
        m_wanCliStatusLbl->setStyleSheet("color:gray;");
    }
}

/* ── Per-worker socket error ── */
void LanWanPage::onWanWorkerError(QAbstractSocket::SocketError)
{
    const int idx = wanWorkerIndex(sender());
    const QString errMsg = (idx >= 0 && idx < m_wanWorkers.size() && m_wanWorkers[idx].sock)
        ? m_wanWorkers[idx].sock->errorString()
        : "errore sconosciuto";

    if (idx == 0) {
        /* Solo il worker 0 aggiorna la UI di stato */
        m_wanCliStatusLbl->setText(tr("\xe2\x9d\x8c  ") + errMsg);
        LogBus::post("\xe2\x9d\x8c LAN WAN: WAN worker errore socket: " + errMsg);
        m_wanCliStatusLbl->setStyleSheet("color:#f44336;");
        m_wanCliConBtn->setEnabled(true);
        m_wanCliDisconBtn->setEnabled(false);
    }
    wanWorkerAppendLog(idx, "Errore socket: " + errMsg);
}

/* ── Per-worker AI token ── */
void LanWanPage::onWanWorkerAiToken(const QString& t)
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    m_wanWorkers[idx].aiBuf.append(t);
}

/* ── Per-worker AI finished ── */
void LanWanPage::onWanWorkerAiFinished(const QString&)
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    WanWorker& w = m_wanWorkers[idx];

    QObject::disconnect(w.tokConn);
    QObject::disconnect(w.finConn);
    QObject::disconnect(w.errConn);
    w.tokConn = w.finConn = w.errConn = {};
    w.aiActive = false;

    const QString raw = w.aiBuf.trimmed();

    /* ── llm_agent: prova a parsare JSON con "result" + "spawn" opzionale ── */
    if (w.isAgentTask) {
        w.isAgentTask = false;
        const int     depth   = w.agentDepth;
        const QString chainId = w.agentChain;
        w.agentDepth = 0;
        w.agentChain.clear();

        QString jsonCandidate;
        const int braceOpen  = raw.indexOf('{');
        const int braceClose = raw.lastIndexOf('}');
        if (braceOpen != -1 && braceClose > braceOpen)
            jsonCandidate = raw.mid(braceOpen, braceClose - braceOpen + 1);

        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(jsonCandidate.toUtf8(), &jerr);
        const bool hasSpawn = doc.isObject()
                           && doc.object().contains("result")
                           && doc.object()["spawn"].isArray()
                           && !doc.object()["spawn"].toArray().isEmpty();

        const QString result = hasSpawn
            ? doc.object()["result"].toString(raw)
            : raw;

        {
            QJsonObject resMsg{{"t","result"}, {"id", w.currentTask},
                               {"status","done"}, {"result", result}};
            if (!w.token.isEmpty())
                resMsg["hmac"] = wanHmac(w.token, w.currentTask + "|" + result);
            wanWorkerSendJson(idx, resMsg);
        }

        if (hasSpawn) {
            const QJsonArray spawnArr = doc.object()["spawn"].toArray();
            QJsonArray tasks;
            for (const auto& v : spawnArr) {
                if (!v.isObject()) continue;
                const QJsonObject s = v.toObject();
                QJsonObject taskPayload{
                    {"role",     s["role"].toString("Assistente")},
                    {"prompt",   s["prompt"].toString()},
                    {"context",  result},
                    {"depth",    depth + 1},
                    {"chain_id", chainId}
                };
                tasks.append(QJsonObject{
                    {"kind",    "llm_agent"},
                    {"payload", QString::fromUtf8(
                        QJsonDocument(taskPayload).toJson(QJsonDocument::Compact))}
                });
            }
            wanWorkerSendJson(idx, QJsonObject{
                {"t",          "spawn_tasks"},
                {"parent_id",  w.currentTask},
                {"chain_id",   chainId},
                {"tasks",      tasks}
            });
            wanWorkerAppendLog(idx,
                QString("Agente ha richiesto %1 sub-agente/i.").arg(tasks.size()));
        }

        wanWorkerAppendLog(idx, "Task " + w.currentTask + " completato (agente).");
        w.currentTask.clear();
        w.aiBuf.clear();
        return;
    }

    {
        QJsonObject resMsg{{"t","result"}, {"id", w.currentTask},
                           {"status","done"}, {"result", raw}};
        if (!w.token.isEmpty())
            resMsg["hmac"] = wanHmac(w.token, w.currentTask + "|" + raw);
        wanWorkerSendJson(idx, resMsg);
    }
    wanWorkerAppendLog(idx, "Task " + w.currentTask + " completato (AI).");
    w.currentTask.clear();
    w.aiBuf.clear();

    /* Aggiorna alias legacy */
    if (idx == 0) {
        m_wanCliCurrentTask = QString();
        m_wanCliAiActive    = false;
        m_wanCliAiBuf.clear();
    }
}

/* ── Per-worker AI error ── */
void LanWanPage::onWanWorkerAiError(const QString& msg)
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    WanWorker& w = m_wanWorkers[idx];

    QObject::disconnect(w.tokConn);
    QObject::disconnect(w.finConn);
    QObject::disconnect(w.errConn);
    w.tokConn = w.finConn = w.errConn = {};
    w.aiActive    = false;
    w.isAgentTask = false;
    w.agentDepth  = 0;
    w.agentChain.clear();

    wanWorkerSendJson(idx, QJsonObject{
        {"t","result"}, {"id", w.currentTask},
        {"status","error"}, {"result", msg}
    });
    wanWorkerAppendLog(idx, "Task " + w.currentTask + " errore AI: " + msg);
    w.currentTask.clear();

    if (idx == 0) {
        m_wanCliCurrentTask = QString();
        m_wanCliAiActive    = false;
    }
}

/* ── wanWorkerHandleTask — dispatcher per il worker idx ── */
void LanWanPage::wanWorkerHandleTask(int idx, const QString& id,
                                     const QString& kind, const QString& payload)
{
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    WanWorker& w = m_wanWorkers[idx];

    /* Mappa kind → system prompt AI (condivisa con wanCliHandleTask) */
    static const QHash<QString, QString> kAiPrompts {
        {"ai_query",
            "Sei un assistente AI preciso e conciso. Rispondi SEMPRE in italiano."},
        {"code_assist",
            "Sei un esperto programmatore. Scrivi codice pulito, commentato e funzionante. "
            "Il payload e' JSON con chiavi 'lang' e 'task'. Rispondi in italiano."},
        {"code_review",
            "Sei un esperto revisore di codice. Analizza il codice fornito: "
            "trova bug, vulnerabilita', inefficienze e suggerisci miglioramenti. "
            "Rispondi in italiano con sezioni: Bug, Sicurezza, Performance, Stile."},
        {"math_solve",
            "Sei un professore di matematica. Risolvi il problema passo per passo. "
            "Rispondi in italiano."},
        {"paper_gen",
            "Sei un ricercatore accademico. Genera un paper scientifico completo. "
            "Rispondi in italiano con formattazione Markdown."},
        {"code_translate",
            "Sei un esperto di traduzione tra linguaggi di programmazione. "
            "Traduci il codice preservando la logica. Rispondi solo con il codice + breve spiegazione."},
        {"math_seq",
            "Sei un matematico esperto. Data la sequenza, trova la formula generale. "
            "Rispondi in italiano."},
        {"ai_tutor",
            "Sei un tutor esperto. Spiega l'argomento in modo chiaro. Rispondi in italiano."},
        {"ai_data_analysis",
            "Sei un analista dati. Analizza i dati e trova pattern. Rispondi in italiano."},
        {"ai_730",
            "Sei un esperto fiscalista italiano specializzato nel modello 730. "
            "Rispondi SOLO in italiano."},
        {"ai_tfr",
            "Sei un esperto di diritto del lavoro. Calcola e spiega il TFR. Rispondi in italiano."},
    };

    /* ── llm_agent ── */
    if (kind == "llm_agent") {
        QString role    = "Assistente AI";
        QString prompt  = payload;
        QString context;
        int     depth   = 0;
        QString chainId;

        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &jerr);
        if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            role    = obj["role"].toString(role);
            prompt  = obj["prompt"].toString(payload);
            context = obj["context"].toString();
            depth   = obj["depth"].toInt(0);
            chainId = obj["chain_id"].toString();
        }
        if (chainId.isEmpty()) chainId = id;

        QString userMsg = prompt;
        if (!context.isEmpty())
            userMsg = "=== CONTESTO DALL'AGENTE PRECEDENTE ===\n" + context
                    + "\n\n=== IL TUO COMPITO ===\n" + prompt;

        constexpr int kMaxDepth = 4;
        QString sysPrompt = QString("Sei: %1\n\nEsegui il compito con precisione. "
                                    "Rispondi in italiano.\n").arg(role);
        if (depth < kMaxDepth) {
            sysPrompt +=
                "\nSe il tuo risultato richiede analisi aggiuntive puoi rispondere in JSON:\n"
                "{ \"result\": \"<risultato>\", \"spawn\": [{\"role\":\"...\","
                "\"prompt\":\"...\"}] }\n"
                "Se NON hai bisogno di altri agenti rispondi in testo libero normale.\n";
        }

        w.isAgentTask = true;
        w.agentDepth  = depth;
        w.agentChain  = chainId;
        w.aiActive    = true;
        w.aiBuf.clear();

        QObject::disconnect(w.tokConn);
        QObject::disconnect(w.finConn);
        QObject::disconnect(w.errConn);
        AiClient* ai = w.ai;
        if (!ai) return;
        w.tokConn = connect(ai, &AiClient::token,    this, &LanWanPage::onWanWorkerAiToken);
        w.finConn = connect(ai, &AiClient::finished, this, &LanWanPage::onWanWorkerAiFinished);
        w.errConn = connect(ai, &AiClient::error,    this, &LanWanPage::onWanWorkerAiError);
        wanWorkerAppendLog(idx,
            QString("Agente [depth=%1] ruolo: %2").arg(depth).arg(role));
        ai->chat(sysPrompt, userMsg);
        return;
    }

    /* ── Task sincroni (subprocess) ── */
    QString result;
    QString status = "done";

    if (kind == "python_repl" || kind == "eval_script") {
        if (!w.shellAllowed) {
            result = "[SICUREZZA] Esecuzione Python disabilitata.";
            status = "error";
        } else {
            const auto r = ProcHelper::run("python3", {"-c", payload}, 30000);
            result = r.out + r.err;
        }

    } else if (kind == "shell_cmd" || kind == "git_cmd") {
        if (!w.shellAllowed) {
            result = "[SICUREZZA] Esecuzione shell disabilitata.";
            status = "error";
        } else {
            const auto r = ProcHelper::run("bash", {"-c", payload}, 20000);
            result = r.out + r.err;
        }

    } else if (kind == "math_expr") {
        /* NON usa eval() Python — vedi commento gemello in main_wan_extra.cpp
           (stessa vulnerabilità, stesso fix: interprete AST whitelist-only,
           mai un vero eval()/exec()). */
        static const QString kMathExprScript =
            "import ast,math,cmath,statistics,operator,sys\n"
            "_names={}\n"
            "for _m in (math,cmath,statistics):\n"
            "    _names.update({k:v for k,v in vars(_m).items() if not k.startswith('_')})\n"
            "_binops={ast.Add:operator.add,ast.Sub:operator.sub,ast.Mult:operator.mul,"
            "ast.Div:operator.truediv,ast.FloorDiv:operator.floordiv,"
            "ast.Mod:operator.mod,ast.Pow:operator.pow}\n"
            "_unops={ast.UAdd:operator.pos,ast.USub:operator.neg}\n"
            "def _ev(n):\n"
            "    if isinstance(n,ast.Expression): return _ev(n.body)\n"
            "    if isinstance(n,ast.Constant):\n"
            "        if isinstance(n.value,(int,float,complex)): return n.value\n"
            "        raise ValueError('costante non numerica')\n"
            "    if isinstance(n,ast.BinOp) and type(n.op) in _binops:\n"
            "        return _binops[type(n.op)](_ev(n.left),_ev(n.right))\n"
            "    if isinstance(n,ast.UnaryOp) and type(n.op) in _unops:\n"
            "        return _unops[type(n.op)](_ev(n.operand))\n"
            "    if isinstance(n,ast.Call):\n"
            "        if not isinstance(n.func,ast.Name) or n.func.id not in _names:\n"
            "            raise ValueError('funzione non ammessa')\n"
            "        fn=_names[n.func.id]\n"
            "        if not callable(fn): raise ValueError('non e una funzione')\n"
            "        return fn(*[_ev(a) for a in n.args])\n"
            "    if isinstance(n,ast.Name):\n"
            "        if n.id in _names and not callable(_names[n.id]): return _names[n.id]\n"
            "        raise ValueError('nome non ammesso: '+n.id)\n"
            "    if isinstance(n,ast.List): return [_ev(e) for e in n.elts]\n"
            "    if isinstance(n,ast.Tuple): return tuple(_ev(e) for e in n.elts)\n"
            "    raise ValueError('espressione non ammessa: '+type(n).__name__)\n"
            "try:\n"
            "    print(_ev(ast.parse(sys.argv[1],mode='eval')))\n"
            "except Exception as e:\n"
            "    print('Errore:', e)\n";
        result = ProcHelper::readOutput(
            "python3", {"-c", kMathExprScript, "--", payload.trimmed().left(500)}, 5000);

    } else if (kind == "matplotlib_plot") {
        if (!w.shellAllowed) {
            result = "[SICUREZZA] Esecuzione Python disabilitata.";
            status = "error";
        } else {
            const auto r = ProcHelper::run("python3", {"-c", payload}, 30000);
            result = r.out + r.err;
        }

    } else if (kind == "graphviz_render") {
        const auto r = ProcHelper::runWithInput("dot", {"-Tsvg"}, payload.toUtf8(), 15000);
        result = r.out.isEmpty() ? "Errore Graphviz: " + r.err : r.out.left(8000);

    } else if (kind == "system_info") {
        QString ram;
        QFile meminfo("/proc/meminfo");
        if (meminfo.open(QIODevice::ReadOnly)) {
            QTextStream ts(&meminfo);
            QString line;
            while (ts.readLineInto(&line)) {
                if (line.startsWith("MemTotal:") || line.startsWith("MemAvailable:"))
                    ram += line + "\n";
            }
        }
        result = QString("OS: %1\nKernel: %2\nArch: %3\nHostname: %4\nQt: %5\n%6")
            .arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion(),
                 QSysInfo::currentCpuArchitecture(), QSysInfo::machineHostName(),
                 qVersion(), ram.trimmed());

    } else if (kind == "net_info") {
        QStringList lines;
        for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
            if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
            for (const QNetworkAddressEntry& e : iface.addressEntries()) {
                if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
                lines << QString("%1  %2").arg(iface.name(), -12).arg(e.ip().toString());
            }
        }
        result = lines.isEmpty() ? "Nessuna interfaccia IPv4." : lines.join("\n");

    } else {
        /* AI task noto o fallback generico */
        const QString sysPrompt = kAiPrompts.contains(kind)
            ? kAiPrompts[kind]
            : "Sei un assistente AI. Esegui il task. Rispondi in italiano.";

        w.aiActive = true;
        w.aiBuf.clear();
        QObject::disconnect(w.tokConn);
        QObject::disconnect(w.finConn);
        QObject::disconnect(w.errConn);
        AiClient* ai = w.ai;
        if (!ai) return;
        w.tokConn = connect(ai, &AiClient::token,    this, &LanWanPage::onWanWorkerAiToken);
        w.finConn = connect(ai, &AiClient::finished, this, &LanWanPage::onWanWorkerAiFinished);
        w.errConn = connect(ai, &AiClient::error,    this, &LanWanPage::onWanWorkerAiError);
        ai->chat(sysPrompt, kind == "ai_query" ? payload
                          : QString("[kind: %1]\n%2").arg(kind, payload));
        return;
    }

    /* Invia risultato sincrono */
    wanWorkerSendJson(idx, QJsonObject{
        {"t","result"}, {"id", id},
        {"status", status}, {"result", result.trimmed()}
    });
    wanWorkerAppendLog(idx, QString("Task %1 completato (%2).").arg(id, kind));
    w.currentTask.clear();
}

/* ══════════════════════════════════════════════════════════════════════════
/* ══════════════════════════════════════════════════════════════════════════
   onWanDecomposeBtnClicked — invia il compito al MasterAgent e crea task
   ══════════════════════════════════════════════════════════════════════════ */

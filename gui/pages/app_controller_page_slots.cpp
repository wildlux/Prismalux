/* ======================================================================
   app_controller_page_slots.cpp

   Implementazioni degli slot estratti dalle lambda di connect() e
   QTimer::singleShot() in AppControllerPage.

   Diviso in sezioni:
     1.  Constructor slots (modelsReady)
     2.  runAi() slots (token / finished / error)
     3.  Blender tab slots
     4.  FreeCAD tab slots
     5.  Office tab slots
     6.  CloudCompare tab slots
     7.  Anki tab slots
     8.  KiCAD tab slots
     9.  TinyMCP tab slots
    10.  OBS tab slots
    11.  Godot tab slots (science file)
   ====================================================================== */
#include "app_controller_page.h"
#include "../prismalux_paths.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QSharedPointer>
#include <QStringList>
#include <QTcpSocket>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QProgressBar>
#include <QVBoxLayout>

namespace P = PrismaluxPaths;

/* ─ external sys-prompt arrays (defined in app_controller_page.cpp) ─ */
extern const char* kBlenderSys[];
extern const char* kFreeCADSys[];
extern const char* kOfficeSys[];
extern const char* kAnkiSys[];
extern const char* kKiCADSys[];
extern const char* kMCUSys[];
extern const char* kOBSSys[];
/* kGodotSys defined in app_controller_page_science.cpp */
extern const char* kGodotSys[];

/* ======================================================================
   Sezione 1 — Constructor: modelsReady
   ====================================================================== */

void AppControllerPage::onModelsReady(const QStringList& models)
{
    QList<QComboBox*> combos = {
        m_blenderModel, m_freecadModel, m_officeModel,
        m_ankiModel, m_kicadModel, m_mcuModel,
        m_obsModel, m_godotModel
    };
    for (auto* cb : combos) {
        if (!cb) continue;
        const QString cur = cb->count() > 0 ? cb->currentData().toString() : QString();
        cb->blockSignals(true);
        cb->clear();
        for (const auto& m : models) {
            const qint64 sz = m_ai->modelSizeBytes(m);
            cb->addItem(P::modelIcon(sz, m) + m, m);
        }
        int idx = cb->findData(cur.isEmpty() ? m_ai->model() : cur);
        if (idx >= 0) cb->setCurrentIndex(idx);
        cb->blockSignals(false);
    }
}

/* ======================================================================
   Sezione 2 — runAi() error slot
   (token e finished sono definiti inline in app_controller_page.cpp)
   ====================================================================== */

void AppControllerPage::onRunAiError(const QString& msg)
{
    m_aiActive = false;
    if (m_runAiRunBtn)  m_runAiRunBtn->setEnabled(true);
    if (m_runAiStopBtn) m_runAiStopBtn->setEnabled(false);
    if (m_aiProgress)   m_aiProgress->setVisible(false);
    /* Disconnette segnali AI e libera token holder */
    disconnect(m_connToken);
    disconnect(m_connFinished);
    disconnect(m_connError);
    if (m_tokenHolder) {
        m_tokenHolder->deleteLater();
        m_tokenHolder = nullptr;
    }
    /* Cattura locale per il retry (copia i membri correnti) */
    const int      tabIdx    = m_runAiTabIdx;
    const QString  sys       = m_runAiSys;
    const QString  userMsg   = m_runAiUserMsg;
    QTextEdit*     output    = m_runAiOutput;
    QPushButton*   runBtn    = m_runAiRunBtn;
    QPushButton*   stopBtn   = m_runAiStopBtn;
    QComboBox*     modelCombo = m_runAiModelCombo;

    /* "Forbidden" = modello cloud selezionato ma Ollama è locale */
    if (msg.contains("Forbidden", Qt::CaseInsensitive)) {
        const QString model = modelCombo ? modelCombo->currentData().toString() : "?";
        m_aiErrorPanel->showError(
            "Modello non disponibile localmente: \"" + model + "\"\n"
            "Seleziona un modello locale (es. deepseek-coder:6.7b, llama3.2:3b) "
            "dalla combo Modello e riprova.",
            [this, tabIdx, sys, userMsg, output, runBtn, stopBtn, modelCombo]{
                runAi(tabIdx, sys, userMsg, output, runBtn, stopBtn, modelCombo);
            });
    } else {
        m_aiErrorPanel->showError(msg,
            [this, tabIdx, sys, userMsg, output, runBtn, stopBtn, modelCombo]{
                runAi(tabIdx, sys, userMsg, output, runBtn, stopBtn, modelCombo);
            });
    }
}

/* ======================================================================
   Sezione 3 — Blender tab slots
   ====================================================================== */

void AppControllerPage::onBlenderCodeChanged()
{
    m_blenderExecBtn->setEnabled(
        !m_blenderCodeEdit->toPlainText().trimmed().isEmpty());
}

void AppControllerPage::onBlenderPingClicked()
{
    QString addr = m_blenderHostEdit->text().trimmed();
    if (addr.isEmpty()) addr = "localhost:6789";
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 6789;
    m_blenderStatusLbl->setText("\xf0\x9f\x94\x84  Connessione...");
    QJsonObject req;
    req["type"]        = "execute";
    req["code"]        = "import bpy; result = {'ok': True, 'blender': bpy.app.version_string, "
                         "'objects': len(bpy.data.objects)}";
    req["strict_json"] = true;
    blenderSendTcp(host, port,
                   QJsonDocument(req).toJson(QJsonDocument::Compact),
                   [this](const QJsonObject& res, bool ok) {
        if (ok) {
            const QJsonObject r = res.value("result").toObject();
            const QString ver   = r.value("blender").toString(
                                      res.value("blender").toString("?"));
            const int     objs  = r.value("objects").toInt();
            m_blenderStatusLbl->setText(
                "\xe2\x9c\x85  Blender " + ver
                + "  \xc2\xb7  " + QString::number(objs) + " oggetti");
        } else {
            m_blenderStatusLbl->setText(
                "\xe2\x9d\x8c  " + res.value("error").toString("non raggiungibile"));
        }
    });
}

void AppControllerPage::onBlenderExecClicked()
{
    const QString code = m_blenderCodeEdit->toPlainText().trimmed();
    if (code.isEmpty()) return;
    m_blenderCode = code;
    QString addr = m_blenderHostEdit->text().trimmed();
    if (addr.isEmpty()) addr = "localhost:6789";
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 6789;

    m_blenderExecBtn->setEnabled(false);
    m_blenderStatusLbl->setText("\xf0\x9f\x94\x84  Verifica connessione...");

    /* Ping automatico prima di eseguire */
    QJsonObject pingReq;
    pingReq["type"]        = "execute";
    pingReq["code"]        = "import bpy; result = {'ok': True, 'blender': bpy.app.version_string, "
                             "'objects': len(bpy.data.objects)}";
    pingReq["strict_json"] = true;
    blenderSendTcp(host, port,
                   QJsonDocument(pingReq).toJson(QJsonDocument::Compact),
                   [this, host, port, code](const QJsonObject& pingRes, bool pingOk) {
        if (!pingOk) {
            m_blenderExecBtn->setEnabled(true);
            m_blenderStatusLbl->setText(
                "\xe2\x9d\x8c  Non raggiungibile: "
                + pingRes.value("error").toString("connessione rifiutata"));
            m_blenderOutput->append(
                "\n\xe2\x9d\x8c  Blender non connesso. "
                "Avvia il server MCP in Blender (N \xe2\x86\x92 MCP \xe2\x86\x92 Start).");
            return;
        }
        const QJsonObject r   = pingRes.value("result").toObject();
        const QString     ver = r.value("blender").toString("?");
        const int         objs = r.value("objects").toInt();
        m_blenderStatusLbl->setText(
            "\xf0\x9f\x94\x84  Blender " + ver + " \xc2\xb7 "
            + QString::number(objs) + " oggetti \xe2\x80\x94 Invio codice...");

        QJsonObject req;
        req["type"]        = "execute";
        req["code"]        = code;
        req["strict_json"] = true;
        blenderSendTcp(host, port,
                       QJsonDocument(req).toJson(QJsonDocument::Compact),
                       [this](const QJsonObject& res, bool ok) {
            m_blenderExecBtn->setEnabled(true);
            const QString raw = QString::fromUtf8(
                QJsonDocument(res).toJson(QJsonDocument::Compact));
            if (ok) {
                const QJsonValue rv = res.value("result");
                QString out;
                if (rv.isString())      out = rv.toString();
                else if (!rv.isNull())  out = QString::fromUtf8(
                    QJsonDocument(rv.toObject()).toJson(QJsonDocument::Compact));
                else                    out = raw;
                m_blenderStatusLbl->setText("\xe2\x9c\x85  Eseguito in Blender");
                m_blenderOutput->append("\n\xe2\x9c\x85  Blender: "
                    + (out.isEmpty() ? "OK" : out));
            } else {
                m_blenderStatusLbl->setText("\xe2\x9d\x8c  Errore esecuzione");
                m_blenderOutput->append("\n\xe2\x9d\x8c  Blender: "
                    + res.value("error").toString(raw));
            }
        });
    });
}

void AppControllerPage::onBlenderHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\x8e\xa8  Installazione Blender MCP");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(540, 480);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x8e\xa8 Blender MCP (addon ufficiale)</h3>"
        "<p style='background:#2a3a2a; border-left:4px solid #8c8; padding:8px; border-radius:4px;'>"
        "\xf0\x9f\x93\xa6 <b>File gi\xc3\xa0 inclusi in Prismalux</b><br>"
        "Nella cartella <code>MCPs/blender_addon/ADDONS_INSTALLAZIONE/</code> trovi:<br>"
        "&bull; <code>mcp-1.0.0.zip</code> \xe2\x80\x94 addon MCP ufficiale (installabile direttamente in Blender)<br>"
        "&bull; <code>blender_mcp_community.py</code> \xe2\x80\x94 versione community alternativa<br>"
        "&bull; <code>prismalux_bridge.py</code> \xe2\x80\x94 bridge Prismalux per esecuzione diretta</p>"
        "<h4>1. Installa Blender 5.1+</h4>"
        "<p><a href='https://www.blender.org/download/'>blender.org/download</a></p>"
        "<h4>2. Installa l'addon MCP</h4>"
        "<p>Blender \xe2\x86\x92 Edit \xe2\x86\x92 Preferences \xe2\x86\x92 Add-ons \xe2\x86\x92 <b>Install</b> "
        "\xe2\x86\x92 seleziona <code>mcp-1.0.0.zip</code> dalla cartella sopra \xe2\x86\x92 "
        "abilita \xe2\x86\x92 imposta porta <b>6789</b></p>"
        "<h4>3. Avvia il server</h4>"
        "<p>Il server parte automaticamente (Auto Start) oppure vai in "
        "3D Viewport \xe2\x86\x92 N \xe2\x86\x92 tab MCP \xe2\x86\x92 <b>Start MCP Server</b> (porta 6789).</p>"
        "<h4>4. Connetti</h4>"
        "<p>Torna qui \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.<br>"
        "Protocollo: TCP socket JSON null-terminated (porta 6789).</p>"
        "<p><i>L'AI gira via Ollama e genera codice Python eseguito direttamente in Blender via TCP. "
        "llama.cpp non \xc3\xa8 richiesto.</i></p>");
    auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

void AppControllerPage::onBlenderRunClicked()
{
    const int idx = m_blenderAction->currentIndex();
    if (idx < 0 || !kBlenderSys[idx]) return;
    runAi(0, QString::fromUtf8(kBlenderSys[idx]),
          m_blenderInput->toPlainText(),
          m_blenderOutput, m_blenderRunBtn, m_blenderStopBtn,
          m_blenderModel);
}

void AppControllerPage::onBlenderStopClicked()
{
    m_ai->abort();
    m_blenderRunBtn->setEnabled(true);
    m_blenderStopBtn->setEnabled(false);
    m_blenderOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

/* ======================================================================
   Sezione 4 — FreeCAD tab slots
   ====================================================================== */

void AppControllerPage::onFreecadPingClicked()
{
    QString addr = m_freecadHostEdit->text().trimmed();
    if (addr.isEmpty()) addr = "localhost:9876";
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 9876;
    m_freecadStatusLbl->setText("\xf0\x9f\x94\x84  Connessione...");
    if (m_freecadPingSock) { m_freecadPingSock->abort(); m_freecadPingSock->deleteLater(); }
    m_freecadPingSock = new QTcpSocket(this);
    connect(m_freecadPingSock, &QTcpSocket::connected,
            this, &AppControllerPage::onFreecadPingConnected);
    connect(m_freecadPingSock, &QAbstractSocket::errorOccurred,
            this, &AppControllerPage::onFreecadPingError);
    m_freecadPingSock->connectToHost(host, static_cast<quint16>(port));
    QTimer::singleShot(3000, this, &AppControllerPage::onFreecadPingTimeout);
}

void AppControllerPage::onFreecadPingConnected()
{
    if (!m_freecadPingSock) return;
    m_freecadPingSock->disconnectFromHost();
    m_freecadPingSock->deleteLater();
    m_freecadPingSock = nullptr;
    m_freecadStatusLbl->setText("\xe2\x9c\x85  FreeCAD connesso");
}

void AppControllerPage::onFreecadPingError(QAbstractSocket::SocketError)
{
    if (!m_freecadPingSock) return;
    m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + m_freecadPingSock->errorString());
    m_freecadPingSock->deleteLater();
    m_freecadPingSock = nullptr;
}

void AppControllerPage::onFreecadPingTimeout()
{
    if (m_freecadPingSock &&
        m_freecadPingSock->state() != QAbstractSocket::ConnectedState) {
        m_freecadStatusLbl->setText("\xe2\x9d\x8c  Timeout");
        m_freecadPingSock->abort();
        m_freecadPingSock->deleteLater();
        m_freecadPingSock = nullptr;
    }
}

void AppControllerPage::onFreecadExecClicked()
{
    if (m_freecadCode.isEmpty()) return;
    QString addr = m_freecadHostEdit->text().trimmed();
    if (addr.isEmpty()) addr = "localhost:9876";
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 9876;
    QJsonObject payload; payload["type"] = "run_script";
    QJsonObject params; params["script"] = m_freecadCode;
    payload["params"] = params;
    m_freecadExecBody = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    m_freecadExecBtn->setEnabled(false);
    m_freecadStatusLbl->setText("\xf0\x9f\x94\x84  Invio a FreeCAD...");
    if (m_freecadExecSock) { m_freecadExecSock->abort(); m_freecadExecSock->deleteLater(); }
    m_freecadExecSock = new QTcpSocket(this);
    connect(m_freecadExecSock, &QTcpSocket::connected,
            this, &AppControllerPage::onFreecadExecConnected);
    connect(m_freecadExecSock, &QTcpSocket::readyRead,
            this, &AppControllerPage::onFreecadExecReadyRead);
    connect(m_freecadExecSock, &QAbstractSocket::errorOccurred,
            this, &AppControllerPage::onFreecadExecError);
    m_freecadExecSock->connectToHost(host, static_cast<quint16>(port));
}

void AppControllerPage::onFreecadExecError(QAbstractSocket::SocketError)
{
    if (!m_freecadExecSock) return;
    m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + m_freecadExecSock->errorString());
    m_freecadExecBtn->setEnabled(true);
    m_freecadExecSock->deleteLater();
    m_freecadExecSock = nullptr;
}

void AppControllerPage::onFreecadExecConnected()
{
    if (!m_freecadExecSock) return;
    m_freecadExecSock->write(m_freecadExecBody);
    m_freecadExecSock->flush();
}

void AppControllerPage::onFreecadExecReadyRead()
{
    if (!m_freecadExecSock) return;
    const QByteArray data = m_freecadExecSock->readAll();
    m_freecadExecSock->disconnectFromHost();
    m_freecadExecSock->deleteLater();
    m_freecadExecSock = nullptr;
    m_freecadExecBtn->setEnabled(true);
    QJsonObject res = QJsonDocument::fromJson(data).object();
    const QString st = res["status"].toString();
    if (st == "ok" || st == "success") {
        m_freecadStatusLbl->setText("\xe2\x9c\x85  Eseguito");
        m_freecadOutput->append("\n\xe2\x9c\x85  FreeCAD: "
            + res["result"].toString("OK"));
    } else {
        m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + res["message"].toString());
        m_freecadOutput->append("\n\xe2\x9d\x8c  FreeCAD errore: "
            + res["message"].toString());
    }
}

void AppControllerPage::onFreecadRunClicked()
{
    const int idx = m_freecadAction->currentIndex();
    if (idx < 0 || !kFreeCADSys[idx]) return;
    runAi(1, QString::fromUtf8(kFreeCADSys[idx]),
          m_freecadInput->toPlainText(),
          m_freecadOutput, m_freecadRunBtn, m_freecadStopBtn,
          m_freecadModel);
}

void AppControllerPage::onFreecadStopClicked()
{
    m_ai->abort();
    m_freecadRunBtn->setEnabled(true);
    m_freecadStopBtn->setEnabled(false);
    m_freecadOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

void AppControllerPage::onFreecadHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\x94\xa9  Installazione FreeCAD MCP");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(540, 440);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x94\xa9 FreeCAD MCP</h3>"
        "<h4>1. Installa FreeCAD</h4>"
        "<p><code>sudo apt install freecad</code> oppure "
        "<a href='https://www.freecad.org/downloads.php'>freecad.org</a></p>"
        "<h4>2. Clona il bridge</h4>"
        "<pre>git clone https://github.com/manuelbb-upb/FreeCAD-MCP</pre>"
        "<p>Segui il README per installare il modulo addon in FreeCAD.</p>"
        "<h4>3. Avvia il server</h4>"
        "<p>FreeCAD \xe2\x86\x92 Strumenti \xe2\x86\x92 Macro \xe2\x86\x92 esegui lo script bridge "
        "(porta <b>9876</b>)</p>"
        "<h4>4. Collega</h4>"
        "<p>Torna qui \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.</p>");
    auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

/* ======================================================================
   Sezione 5 — Office tab slots
   ====================================================================== */

/** Legge il token Bearer dal file ~/.prismalux_office_token */
static void s_readOfficeBridgeToken(QString& outToken)
{
    const QString tokenFile = QDir::homePath() + "/.prismalux_office_token";
    QFile f(tokenFile);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    outToken = QString::fromUtf8(f.readAll()).trimmed();
}

/** Cerca prismalux_office_bridge.py risalendo la dir-tree */
static QString s_findOfficeBridgePath()
{
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 4; i++) {
        QString p = d.filePath("MCPs/office_bridge/prismalux_office_bridge.py");
        if (QFile::exists(p)) return p;
        d.cdUp();
    }
    return {};
}

void AppControllerPage::onOfficeStartClicked()
{
    if (m_officeBridgeProc &&
        m_officeBridgeProc->state() == QProcess::Running) {
        m_officeBridgeProc->terminate();
        m_officeBridgeProc->waitForFinished(2000);
        m_officeStartBtn->setText("\xe2\x96\xb6  Avvia bridge");
        m_officeStatusLbl->setText("\xe2\x9a\xaa  Bridge fermato");
        return;
    }
    const QString path = s_findOfficeBridgePath();
    if (path.isEmpty()) {
        m_officeStatusLbl->setText(
            "\xe2\x9d\x8c  prismalux_office_bridge.py non trovato");
        return;
    }
    if (!m_officeBridgeProc) {
        m_officeBridgeProc = new QProcess(this);
        m_officeBridgeProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_officeBridgeProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &AppControllerPage::onOfficeBridgeFinished);
    }
    m_officeStatusLbl->setText("\xf0\x9f\x94\x84  Avvio bridge...");
    m_officeBridgeProc->start("python3", {path});
    if (m_officeBridgeProc->state() == QProcess::Running) {
        m_officeStartBtn->setText("\xe2\x8f\xb9  Ferma bridge");
        QTimer::singleShot(1200, this, &AppControllerPage::onOfficeStatusReply);
    } else {
        m_officeStatusLbl->setText(
            "\xe2\x9d\x8c  Errore avvio (python3 non trovato?)");
    }
}

void AppControllerPage::onOfficeBridgeFinished(int /*exitCode*/, QProcess::ExitStatus /*status*/)
{
    m_officeStartBtn->setText("\xe2\x96\xb6  Avvia bridge");
    m_officeStatusLbl->setText("\xe2\x9a\xaa  Bridge fermato");
}

void AppControllerPage::onOfficeStatusReply()
{
    /* Chiamata da QTimer::singleShot 1200ms dopo avvio bridge */
    s_readOfficeBridgeToken(m_officeBridgeToken);
    QNetworkRequest req(QUrl("http://localhost:6790/status"));
    req.setTransferTimeout(2000);
    req.setRawHeader("Authorization",
                     ("Bearer " + m_officeBridgeToken).toUtf8());
    m_officeStatusReply = m_officeNam->get(req);
    connect(m_officeStatusReply, &QNetworkReply::finished,
            this, [this]() {
        auto* r = m_officeStatusReply;
        m_officeStatusReply = nullptr;
        if (!r) return;
        r->deleteLater();
        if (r->error() == QNetworkReply::NoError) {
            QJsonObject obj  = QJsonDocument::fromJson(r->readAll()).object();
            QJsonObject libs = obj["libraries"].toObject();
            QStringList ok;
            for (auto it = libs.begin(); it != libs.end(); ++it)
                if (it.value().toBool()) ok << it.key();
            m_officeStatusLbl->setText(
                "\xe2\x9c\x85  Pronto: " +
                (ok.isEmpty() ? "bridge attivo" : ok.join(", ")));
        } else {
            m_officeStatusLbl->setText(
                "\xe2\x9a\xa0  Bridge avviato (verifica fallita)");
        }
    });
}

void AppControllerPage::onOfficeExecClicked()
{
    if (m_officeCode.isEmpty()) return;
    s_readOfficeBridgeToken(m_officeBridgeToken);
    QJsonObject payload; payload["code"] = m_officeCode;
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkRequest req(QUrl("http://localhost:6790/execute"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    req.setRawHeader("Authorization", ("Bearer " + m_officeBridgeToken).toUtf8());
    req.setTransferTimeout(30000);
    m_officeExecBtn->setEnabled(false);
    m_officeStatusLbl->setText("\xf0\x9f\x94\x84  Invio a Office...");
    m_officeExecReply = m_officeNam->post(req, body);
    connect(m_officeExecReply, &QNetworkReply::finished,
            this, &AppControllerPage::onOfficeExecReply);
}

void AppControllerPage::onOfficeExecReply()
{
    auto* reply = m_officeExecReply;
    m_officeExecReply = nullptr;
    if (!reply) return;
    reply->deleteLater();
    m_officeExecBtn->setEnabled(true);
    if (reply->error() == QNetworkReply::NoError) {
        QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();
        if (res["ok"].toBool()) {
            m_officeStatusLbl->setText("\xe2\x9c\x85  Eseguito");
            m_officeOutput->append("\n\xe2\x9c\x85  Office: "
                + res["output"].toString("OK"));
        } else {
            m_officeStatusLbl->setText("\xe2\x9d\x8c  Errore");
            m_officeOutput->append("\n\xe2\x9d\x8c  Office errore: "
                + res["error"].toString());
        }
    } else {
        m_officeStatusLbl->setText("\xe2\x9d\x8c  " + reply->errorString());
    }
}

void AppControllerPage::onOfficeRunClicked()
{
    const int idx = m_officeAction->currentIndex();
    if (idx < 0 || !kOfficeSys[idx]) return;
    runAi(2, QString::fromUtf8(kOfficeSys[idx]),
          m_officeInput->toPlainText(),
          m_officeOutput, m_officeRunBtn, m_officeStopBtn,
          m_officeModel);
}

void AppControllerPage::onOfficeStopClicked()
{
    m_ai->abort();
    m_officeRunBtn->setEnabled(true);
    m_officeStopBtn->setEnabled(false);
    m_officeOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

void AppControllerPage::onOfficeHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\x96\xa5  Installazione Office Bridge");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(540, 420);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x96\xa5 Office Bridge (LibreOffice)</h3>"
        "<h4>1. Installa LibreOffice + python-uno</h4>"
        "<p><code>sudo apt install libreoffice python3-uno</code></p>"
        "<h4>2. Avvia il bridge</h4>"
        "<p>Clicca <b>\xe2\x96\xb6 Avvia bridge</b> in questa scheda: avvia automaticamente "
        "<code>prismalux_office_bridge.py</code> (porta <b>6790</b>).</p>"
        "<h4>3. Genera ed esegui</h4>"
        "<p>Scrivi l'istruzione nel campo testo \xe2\x86\x92 "
        "<b>Genera codice AI</b> \xe2\x86\x92 <b>Esegui in Office</b>.</p>"
        "<h4>Nota</h4>"
        "<p>Il bridge controlla LibreOffice Writer / Calc / Impress via API UNO. "
        "LibreOffice deve essere installato ma non necessariamente aperto.</p>");
    auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

/* ======================================================================
   Sezione 6 — CloudCompare tab slots
   ====================================================================== */

void AppControllerPage::onCcHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\x94\xb5  Installazione CloudComPy");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(560, 460);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x94\xb5 CloudCompare + CloudComPy</h3>"
        "<h4>1. Installa CloudCompare</h4>"
        "<p><code>sudo apt install cloudcompare</code> oppure "
        "<a href='https://www.danielgm.net/cc/'>danielgm.net/cc</a></p>"
        "<h4>2. Installa CloudComPy (Python wrapper)</h4>"
        "<pre>pip install cloudcompy</pre>"
        "<p>Oppure compila dal sorgente: "
        "<a href='https://github.com/CloudCompare/CloudComPy'>"
        "github.com/CloudCompare/CloudComPy</a></p>"
        "<h4>3. Formati supportati</h4>"
        "<p>LAS \xc2\xb7 PLY \xc2\xb7 E57 \xc2\xb7 PCD \xc2\xb7 BIN (formato nativo CC)</p>"
        "<h4>Stato attuale</h4>"
        "<p>\xe2\x8f\xb3 Il bridge \xc3\xa8 in fase di integrazione in Prismalux. "
        "Questa scheda sar\xc3\xa0 abilitata in una versione futura.</p>");
    auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

/* ======================================================================
   Sezione 7 — Anki tab slots
   ====================================================================== */

void AppControllerPage::onAnkiPingClicked()
{
    const QString host = m_ankiHostEdit->text().trimmed();
    QUrl url("http://" + host);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(3000);
    QJsonObject body;
    body["action"]  = "version";
    body["version"] = 6;
    if (m_ankiPingReply) { m_ankiPingReply->abort(); m_ankiPingReply->deleteLater(); }
    m_ankiPingReply = m_ankiNam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_ankiStatusLbl->setText("\xf0\x9f\x94\x84  Verifica...");
    connect(m_ankiPingReply, &QNetworkReply::finished,
            this, &AppControllerPage::onAnkiPingReplyFinished);
}

void AppControllerPage::onAnkiPingReplyFinished()
{
    if (!m_ankiPingReply) return;
    m_ankiPingReply->deleteLater();
    const bool ok = (m_ankiPingReply->error() == QNetworkReply::NoError);
    m_ankiPingReply = nullptr;
    if (ok) {
        m_ankiStatusLbl->setText("\xe2\x9c\x85  AnkiConnect attivo");
        m_ankiSendBtn->setEnabled(true);
    } else {
        m_ankiStatusLbl->setText("\xe2\x9d\x8c  Anki non raggiungibile (avvia Anki)");
    }
}

void AppControllerPage::onAnkiSendClicked()
{
    if (m_ankiOutput->toPlainText().trimmed().isEmpty()) return;
    const QString raw  = m_ankiOutput->toPlainText();
    const QString json = extractCode(raw);
    execAnkiAction(m_ankiDeckEdit ? m_ankiDeckEdit->text().trimmed() : "Default", json);
}

void AppControllerPage::onAnkiRunClicked()
{
    const int idx = m_ankiAction->currentIndex();
    if (idx < 0 || !kAnkiSys[idx]) return;
    runAi(4, QString::fromUtf8(kAnkiSys[idx]),
          m_ankiInput->toPlainText(),
          m_ankiOutput, m_ankiRunBtn, m_ankiStopBtn,
          m_ankiModel);
}

void AppControllerPage::onAnkiStopClicked()
{
    m_ai->abort();
    m_ankiRunBtn->setEnabled(true);
    m_ankiStopBtn->setEnabled(false);
    m_ankiOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

void AppControllerPage::onAnkiHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\x83\x8f  Installazione AnkiConnect");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(540, 420);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x83\x8f Anki MCP \xe2\x80\x94 AnkiConnect</h3>"
        "<h4>1. Installa Anki</h4>"
        "<p><a href='https://apps.ankiweb.net/'>apps.ankiweb.net</a> "
        "oppure <code>sudo apt install anki</code></p>"
        "<h4>2. Installa il plugin AnkiConnect</h4>"
        "<p>Anki \xe2\x86\x92 <b>Strumenti</b> \xe2\x86\x92 <b>Componenti aggiuntivi</b> "
        "\xe2\x86\x92 <b>Sfoglia e installa</b> \xe2\x86\x92 inserisci il codice:</p>"
        "<pre>2055492159</pre>"
        "<h4>3. Avvia Anki</h4>"
        "<p>AnkiConnect si avvia automaticamente con Anki (porta <b>8765</b>). "
        "Non chiudere Anki durante l'uso.</p>"
        "<h4>4. Collega</h4>"
        "<p>Torna qui \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.</p>");
    auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

void AppControllerPage::onAnkiAddNotesReply()
{
    auto* reply = m_ankiPendingReply;
    m_ankiPendingReply = nullptr;
    if (!reply) return;
    reply->deleteLater();
    const int count = m_ankiPendingCount;
    if (reply->error() == QNetworkReply::NoError) {
        m_ankiStatusLbl->setText(
            QString("\xe2\x9c\x85  %1 carte inviate ad Anki").arg(count));
        m_ankiOutput->append(
            QString("\n\xe2\x9c\x85  Inviate %1 carte nel deck.").arg(count));
    } else {
        m_ankiStatusLbl->setText("\xe2\x9d\x8c  " + reply->errorString());
        m_ankiOutput->append("\n\xe2\x9d\x8c  Errore invio: " + reply->errorString());
    }
}

/* ======================================================================
   Sezione 8 — KiCAD tab slots
   ====================================================================== */

void AppControllerPage::onKicadPingClicked()
{
    const QString host = m_kicadHostEdit->text().trimmed();
    auto* sock = new QTcpSocket(this);
    const QStringList parts = host.split(':');
    const int port = parts.size() > 1 ? parts[1].toInt() : 3000;
    m_kicadStatusLbl->setText("\xf0\x9f\x94\x84  Verifica...");
    connect(sock, &QTcpSocket::connected, this, [this, sock]() {
        m_kicadStatusLbl->setText("\xe2\x9c\x85  KiCAD MCP Server attivo");
        sock->disconnectFromHost();
        sock->deleteLater();
    });
    connect(sock, &QTcpSocket::errorOccurred, this,
            [this, sock](QAbstractSocket::SocketError) {
        m_kicadStatusLbl->setText("\xe2\x9d\x8c  KiCAD MCP non raggiungibile");
        sock->deleteLater();
    });
    sock->connectToHost(parts[0], static_cast<quint16>(port));
}

void AppControllerPage::onKicadExecClicked()
{
    if (m_kicadCode.isEmpty()) return;
    execKiCADAction(m_kicadCode);
}

void AppControllerPage::onKicadRunClicked()
{
    const int idx = m_kicadAction->currentIndex();
    if (idx < 0 || !kKiCADSys[idx]) return;
    runAi(5, QString::fromUtf8(kKiCADSys[idx]),
          m_kicadInput->toPlainText(),
          m_kicadOutput, m_kicadRunBtn, m_kicadStopBtn,
          m_kicadModel);
}

void AppControllerPage::onKicadStopClicked()
{
    m_ai->abort();
    m_kicadRunBtn->setEnabled(true);
    m_kicadStopBtn->setEnabled(false);
    m_kicadOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

void AppControllerPage::onKicadHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\x96\xa5  Installazione KiCAD MCP Server");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(540, 460);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x96\xa5 KiCAD MCP Server</h3>"
        "<h4>1. Installa KiCAD 7+</h4>"
        "<p><code>sudo apt install kicad</code> oppure "
        "<a href='https://www.kicad.org/download/'>kicad.org</a></p>"
        "<h4>2. Installa e avvia il server MCP</h4>"
        "<pre>git clone https://github.com/mixelpixx/KiCAD-MCP-Server\n"
        "cd KiCAD-MCP-Server\n"
        "npm install\n"
        "node server.js</pre>"
        "<p>Il server ascolta sulla porta <b>3000</b>.</p>"
        "<h4>3. Abilita Scripting Console in KiCAD</h4>"
        "<p>PCB Editor \xe2\x86\x92 Strumenti \xe2\x86\x92 <b>Console Scripting</b> "
        "(deve essere abilitata nel build di KiCAD)</p>"
        "<h4>4. Collega</h4>"
        "<p>Torna qui \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.</p>");
    auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

void AppControllerPage::onKicadExecReply()
{
    auto* reply = m_kicadPendingReply;
    m_kicadPendingReply = nullptr;
    if (!reply) return;
    reply->deleteLater();
    m_kicadExecBtn->setEnabled(true);
    if (reply->error() == QNetworkReply::NoError) {
        const QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();
        if (res["ok"].toBool(true)) {
            m_kicadStatusLbl->setText("\xe2\x9c\x85  Eseguito in KiCAD");
            m_kicadOutput->append("\n\xe2\x9c\x85  KiCAD: "
                + res["output"].toString("OK"));
        } else {
            m_kicadStatusLbl->setText("\xe2\x9d\x8c  Errore KiCAD");
            m_kicadOutput->append("\n\xe2\x9d\x8c  Errore: "
                + res["error"].toString(reply->errorString()));
        }
    } else {
        m_kicadStatusLbl->setText("\xe2\x9d\x8c  " + reply->errorString());
        m_kicadOutput->append("\n\xe2\x9d\x8c  " + reply->errorString());
    }
}

/* ======================================================================
   Sezione 9 — TinyMCP tab slots
   ====================================================================== */

void AppControllerPage::onMcuDetectClicked()
{
    detectSerialPorts();
}

void AppControllerPage::onMcuFlashClicked()
{
    if (m_mcuCode.isEmpty()) return;
    const QString port  = m_mcuPort->currentText().trimmed();
    const QString board = m_mcuBoardCombo ? m_mcuBoardCombo->currentText() : QString();
    if (port.isEmpty()) {
        m_mcuOutput->append("\n\xe2\x9a\xa0  Seleziona una porta seriale prima di flashare.");
        return;
    }
    const QString tmpPath = QDir::tempPath() + "/prismalux_mcu_sketch.ino";
    QFile f(tmpPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(m_mcuCode.toUtf8());
        f.close();
    }
    m_mcuOutput->append(
        QString("\n\xf0\x9f\x93\x9d  Codice salvato: %1\n"
                "\xe2\x9a\xa1  Per flashare su %2 (porta %3):\n"
                "   arduino-cli compile --fqbn arduino:avr:uno %1\n"
                "   arduino-cli upload -p %3 --fqbn arduino:avr:uno %1\n"
                "   (adatta l'fqbn alla tua scheda)")
            .arg(tmpPath, board, port));
    m_mcuStatusLbl->setText(
        QString("\xf0\x9f\x93\x9d  Salvato \xe2\x80\x94 usa arduino-cli per flashare"));
}

void AppControllerPage::onMcuRunClicked()
{
    const int idx = m_mcuAction->currentIndex();
    if (idx < 0 || !kMCUSys[idx]) return;
    const QString boardName = m_mcuBoardCombo ? m_mcuBoardCombo->currentText() : QString();
    const QString sys = QString::fromUtf8(kMCUSys[idx])
        + QString("\nScheda target: %1").arg(boardName);
    runAi(6, sys,
          m_mcuInput->toPlainText(),
          m_mcuOutput, m_mcuRunBtn, m_mcuStopBtn,
          m_mcuModel);
}

void AppControllerPage::onMcuStopClicked()
{
    m_ai->abort();
    m_mcuRunBtn->setEnabled(true);
    m_mcuStopBtn->setEnabled(false);
    m_mcuOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

void AppControllerPage::onMcuHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xe2\x9a\xa1  TinyMCP \xe2\x80\x94 Microcontrollori");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(560, 500);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xe2\x9a\xa1 TinyMCP \xe2\x80\x94 Microcontrollori</h3>"
        "<h4>Arduino CLI (Arduino Uno / Nano / Mega)</h4>"
        "<pre>sudo apt install arduino-cli\n"
        "arduino-cli core install arduino:avr</pre>"
        "<h4>ESP32 / ESP8266</h4>"
        "<pre>pip install esptool\n"
        "arduino-cli core install esp32:esp32</pre>"
        "<h4>MicroPython (ESP)</h4>"
        "<p>Scarica il firmware da "
        "<a href='https://micropython.org/download/'>micropython.org</a>, poi:</p>"
        "<pre>esptool.py erase_flash\n"
        "esptool.py write_flash 0x1000 firmware.bin</pre>"
        "<h4>Raspberry Pi Pico</h4>"
        "<p>Tieni premuto BOOTSEL \xe2\x86\x92 collega USB \xe2\x86\x92 trascina il file "
        "<code>.uf2</code> nel drive che appare.</p>"
        "<h4>Connessione</h4>"
        "<p>Collega il microcontrollore via USB \xe2\x86\x92 clicca "
        "<b>\xf0\x9f\x94\x8d Rileva</b> \xe2\x86\x92 genera il codice \xe2\x86\x92 "
        "<b>\xe2\x9a\xa1 Flash MCU</b> per salvare e ottenere il comando di upload.</p>");
    auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

/* ======================================================================
   Sezione 10 — OBS tab slots
   ====================================================================== */

void AppControllerPage::onObsPingClicked()
{
    const QString addr = m_obsHostEdit->text().trimmed();
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 4455;
    m_obsStatusLbl->setText("\xf0\x9f\x94\x84  Connessione...");
    auto* sock = new QTcpSocket(this);
    sock->connectToHost(host, static_cast<quint16>(port));
    connect(sock, &QTcpSocket::connected, this, [this, sock]() {
        sock->disconnectFromHost(); sock->deleteLater();
        m_obsStatusLbl->setText("\xe2\x9c\x85  OBS WebSocket attivo");
        m_obsExecBtn->setEnabled(!m_obsCode.isEmpty());
    });
    connect(sock, &QAbstractSocket::errorOccurred, this,
            [this, sock](QAbstractSocket::SocketError) {
        m_obsStatusLbl->setText("\xe2\x9d\x8c  " + sock->errorString());
        sock->deleteLater();
    });
    QPointer<QTcpSocket> sockPtr(sock);
    QTimer::singleShot(3000, this, [sockPtr, this]() {
        if (sockPtr && sockPtr->state() != QAbstractSocket::ConnectedState) {
            m_obsStatusLbl->setText("\xe2\x9d\x8c  Timeout \xe2\x80\x94 OBS non raggiungibile");
            sockPtr->abort(); sockPtr->deleteLater();
        }
    });
}

void AppControllerPage::onObsExecClicked()
{
    if (m_obsCode.isEmpty()) return;
    const QString tmpPath = QDir::tempPath() + "/prismalux_obs_script.py";
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    f.write(m_obsCode.toUtf8());
    f.close();

    if (!m_obsExecProc) {
        m_obsExecProc = new QProcess(this);
        m_obsExecProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_obsExecProc, &QProcess::readyRead,
                this, &AppControllerPage::onObsProcReadyRead);
        connect(m_obsExecProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &AppControllerPage::onObsProcFinished);
    }
    m_obsExecBtn->setEnabled(false);
    m_obsStatusLbl->setText("\xf0\x9f\x94\x84  Esecuzione script...");
    m_obsExecProc->start("python3", {tmpPath});
    if (m_obsExecProc->state() == QProcess::NotRunning)
        m_obsStatusLbl->setText("\xe2\x9d\x8c  python3 non trovato");
}

void AppControllerPage::onObsProcReadyRead()
{
    m_obsOutput->append(
        QString::fromUtf8(m_obsExecProc->readAll()).trimmed());
}

void AppControllerPage::onObsProcFinished(int code, QProcess::ExitStatus /*status*/)
{
    m_obsStatusLbl->setText(code == 0
        ? "\xe2\x9c\x85  Script eseguito"
        : "\xe2\x9d\x8c  Script terminato con errore");
    m_obsExecBtn->setEnabled(true);
}

void AppControllerPage::onObsRunClicked()
{
    const int idx = m_obsAction->currentIndex();
    if (idx < 0 || !kOBSSys[idx]) return;
    runAi(7, QString::fromUtf8(kOBSSys[idx]),
          m_obsInput->toPlainText(),
          m_obsOutput, m_obsRunBtn, m_obsStopBtn,
          m_obsModel);
}

void AppControllerPage::onObsStopClicked()
{
    m_ai->abort();
    m_obsRunBtn->setEnabled(true);
    m_obsStopBtn->setEnabled(false);
    m_obsOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

void AppControllerPage::onObsHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\x94\xb4  Installazione OBS MCP");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(560, 500);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x94\xb4 OBS MCP</h3>"
        "<h4>1. Installa OBS Studio</h4>"
        "<p><code>sudo apt install obs-studio</code> oppure "
        "<a href='https://obsproject.com/'>obsproject.com</a></p>"
        "<h4>2. Abilita OBS WebSocket</h4>"
        "<p>OBS \xe2\x86\x92 <b>Strumenti</b> \xe2\x86\x92 <b>WebSocket Server Settings</b> "
        "\xe2\x86\x92 abilita il server (porta <b>4455</b>). "
        "Disabilita la password per uso locale o impostala nel codice.</p>"
        "<h4>3. Installa obsws-python</h4>"
        "<pre>pip install obsws-python</pre>"
        "<h4>4. (Opzionale) Installa obs-mcp</h4>"
        "<p>Il plugin MCP ufficiale: "
        "<a href='https://github.com/royshil/obs-mcp'>github.com/royshil/obs-mcp</a><br>"
        "Permette di controllare OBS da client MCP come Claude Desktop.</p>"
        "<h4>5. Collega</h4>"
        "<p>Avvia OBS \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b> \xe2\x86\x92 "
        "genera lo script \xe2\x86\x92 <b>\xf0\x9f\x94\xb4 Esegui in OBS</b>.</p>");
    auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

/* ======================================================================
   Sezione 11 — Godot tab slots
   ====================================================================== */

void AppControllerPage::onGodotExecClicked()
{
    if (m_godotCode.isEmpty()) return;
    const QString path = QDir::homePath() + "/ai_generated.gd";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(m_godotCode.toUtf8());
        m_godotStatusLbl->setText("\xe2\x9c\x85  Salvato: " + path);
    } else {
        m_godotStatusLbl->setText("\xe2\x9d\x8c  Impossibile salvare il file");
    }
}

void AppControllerPage::onGodotRunClicked()
{
    const int idx = m_godotAction->currentIndex();
    if (idx < 0 || !kGodotSys[idx]) return;
    runAi(9, QString::fromUtf8(kGodotSys[idx]),
          m_godotInput->toPlainText(),
          m_godotOutput, m_godotRunBtn, m_godotStopBtn,
          m_godotModel);
}

void AppControllerPage::onGodotStopClicked()
{
    m_ai->abort();
    m_godotRunBtn->setEnabled(true);
    m_godotStopBtn->setEnabled(false);
    m_godotOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

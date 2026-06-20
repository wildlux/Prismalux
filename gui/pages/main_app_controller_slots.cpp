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
#include "main_app_controller.h"
#include "../log_bus.h"
#include "../prismalux_paths.h"
#include "../widgets/model_combo_helper.h"
#include "../widgets/proc_helper.h"

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
#include <QJsonArray>
#include <QListWidget>
#include <QMessageBox>
#include <QSharedPointer>
#include <QStringList>
#include <QTcpSocket>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QProgressBar>
#include <QCheckBox>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QUrl>
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
   Link pip cliccabili nei log QTextBrowser
   URL schema: pip://<pacchetto>  →  apre Impostazioni→Moduli Python
   ====================================================================== */

void AppControllerPage::onPipLinkClicked(const QUrl& url)
{
    if (url.scheme() == QLatin1String("pip")) {
        emit openSettingsDipendenze(url.path());
    } else if (url.scheme() == QLatin1String("tg-restart")) {
        onTelegramStartClicked();
    } else if (url.scheme() == QLatin1String("wa-restart")) {
        onWaBotStartClicked();
    }
}

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
        ModelComboHelper::populate(cb, m_ai, models);
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
    m_blenderStatusLbl->setText(tr("\xf0\x9f\x94\x84  Connessione..."));
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
            const QString blenderErr = res.value("error").toString("non raggiungibile");
            m_blenderStatusLbl->setText("\xe2\x9d\x8c  " + blenderErr);
            LogBus::post("\xe2\x9d\x8c Blender: Ping fallito: " + blenderErr);
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
    m_blenderStatusLbl->setText(tr("\xf0\x9f\x94\x84  Verifica connessione..."));

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
            const QString blenderPingErr = pingRes.value("error").toString("connessione rifiutata");
            m_blenderStatusLbl->setText("\xe2\x9d\x8c  Non raggiungibile: " + blenderPingErr);
            m_blenderOutput->append(
                "\n\xe2\x9d\x8c  Blender non connesso. "
                "Avvia il server MCP in Blender (N \xe2\x86\x92 MCP \xe2\x86\x92 Start).");
            LogBus::post("\xe2\x9d\x8c Blender: Non raggiungibile: " + blenderPingErr);
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
                m_blenderStatusLbl->setText(tr("\xe2\x9c\x85  Eseguito in Blender"));
                m_blenderOutput->append("\n\xe2\x9c\x85  Blender: "
                    + (out.isEmpty() ? "OK" : out));
            } else {
                const QString blenderExecErr = res.value("error").toString(raw);
                m_blenderStatusLbl->setText(tr("\xe2\x9d\x8c  Errore esecuzione"));
                m_blenderOutput->append("\n\xe2\x9d\x8c  Blender: " + blenderExecErr);
                LogBus::post("\xe2\x9d\x8c Blender: Errore esecuzione: " + blenderExecErr);
            }
        });
    });
}

void AppControllerPage::onBlenderHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x8e\xa8  Installazione Blender MCP"));
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

    /* ── Controllo modello troppo piccolo per Blender ── */
    if (m_blenderWarnLbl) {
        const QString modelName = m_blenderModel
            ? m_blenderModel->currentData().toString()
            : m_ai->model();
        QRegularExpression reBillion(R"((\d+(\.\d+)?)\s*[bB])");
        QRegularExpressionMatch m = reBillion.match(modelName);
        if (m.hasMatch()) {
            const double billions = m.captured(1).toDouble();
            if (billions < 7.0) {
                m_blenderWarnLbl->setText(
                    "\xe2\x9a\xa0\xef\xb8\x8f <b>Il modello <i>"
                    + modelName
                    + "</i> potrebbe essere troppo piccolo per Blender.</b> "
                    "Raccomandato: modello \xe2\x89\xa5 7B "
                    "(es. llama3.1:8b, qwen2.5-coder:7b).");
                m_blenderWarnLbl->setVisible(true);
            } else {
                m_blenderWarnLbl->setVisible(false);
            }
        } else {
            m_blenderWarnLbl->setVisible(false);
        }
    }

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
    m_freecadStatusLbl->setText(tr("\xf0\x9f\x94\x84  Connessione..."));
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
    m_freecadStatusLbl->setText(tr("\xe2\x9c\x85  FreeCAD connesso"));
}

void AppControllerPage::onFreecadPingError(QAbstractSocket::SocketError)
{
    if (!m_freecadPingSock) return;
    const QString freecadPingErr = m_freecadPingSock->errorString();
    m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + freecadPingErr);
    LogBus::post("\xe2\x9d\x8c FreeCAD: Ping errore: " + freecadPingErr);
    m_freecadPingSock->deleteLater();
    m_freecadPingSock = nullptr;
}

void AppControllerPage::onFreecadPingTimeout()
{
    if (m_freecadPingSock &&
        m_freecadPingSock->state() != QAbstractSocket::ConnectedState) {
        m_freecadStatusLbl->setText(tr("\xe2\x9d\x8c  Timeout"));
        LogBus::post("\xe2\x9d\x8c FreeCAD: Ping timeout.");
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
    m_freecadStatusLbl->setText(tr("\xf0\x9f\x94\x84  Invio a FreeCAD..."));
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
    const QString freecadExecErr = m_freecadExecSock->errorString();
    m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + freecadExecErr);
    LogBus::post("\xe2\x9d\x8c FreeCAD: Exec errore: " + freecadExecErr);
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
        m_freecadStatusLbl->setText(tr("\xe2\x9c\x85  Eseguito"));
        m_freecadOutput->append("\n\xe2\x9c\x85  FreeCAD: "
            + res["result"].toString("OK"));
    } else {
        const QString freecadResErr = res["message"].toString();
        m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + freecadResErr);
        m_freecadOutput->append("\n\xe2\x9d\x8c  FreeCAD errore: " + freecadResErr);
        LogBus::post("\xe2\x9d\x8c FreeCAD: Errore risposta: " + freecadResErr);
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
    dlg->setWindowTitle(tr("\xf0\x9f\x94\xa9  Installazione FreeCAD MCP"));
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
        m_officeStartBtn->setText(tr("\xe2\x96\xb6  Avvia bridge"));
        m_officeStatusLbl->setText(tr("\xe2\x9a\xaa  Bridge fermato"));
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
    m_officeStatusLbl->setText(tr("\xf0\x9f\x94\x84  Avvio bridge..."));
    m_officeBridgeProc->start(P::findPython(), {path});
    if (m_officeBridgeProc->state() == QProcess::Running) {
        m_officeStartBtn->setText(tr("\xe2\x8f\xb9  Ferma bridge"));
        QTimer::singleShot(1200, this, &AppControllerPage::onOfficeStatusReply);
    } else {
        m_officeStatusLbl->setText(
            "\xe2\x9d\x8c  Errore avvio (python3 non trovato?)");
    }
}

void AppControllerPage::onOfficeBridgeFinished(int /*exitCode*/, QProcess::ExitStatus /*status*/)
{
    m_officeStartBtn->setText(tr("\xe2\x96\xb6  Avvia bridge"));
    m_officeStatusLbl->setText(tr("\xe2\x9a\xaa  Bridge fermato"));
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
    m_officeStatusLbl->setText(tr("\xf0\x9f\x94\x84  Invio a Office..."));
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
            m_officeStatusLbl->setText(tr("\xe2\x9c\x85  Eseguito"));
            m_officeOutput->append("\n\xe2\x9c\x85  Office: "
                + res["output"].toString("OK"));
        } else {
            m_officeStatusLbl->setText(tr("\xe2\x9d\x8c  Errore"));
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
    dlg->setWindowTitle(tr("\xf0\x9f\x96\xa5  Installazione Office Bridge"));
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
    dlg->setWindowTitle(tr("\xf0\x9f\x94\xb5  Installazione CloudComPy"));
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
    m_ankiStatusLbl->setText(tr("\xf0\x9f\x94\x84  Verifica..."));
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
        m_ankiStatusLbl->setText(tr("\xe2\x9c\x85  AnkiConnect attivo"));
        m_ankiSendBtn->setEnabled(true);
    } else {
        m_ankiStatusLbl->setText(tr("\xe2\x9d\x8c  Anki non raggiungibile (avvia Anki)"));
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
    dlg->setWindowTitle(tr("\xf0\x9f\x83\x8f  Installazione AnkiConnect"));
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
    m_kicadStatusLbl->setText(tr("\xf0\x9f\x94\x84  Verifica..."));
    connect(sock, &QTcpSocket::connected, this, [this, sock]() {
        m_kicadStatusLbl->setText(tr("\xe2\x9c\x85  KiCAD MCP Server attivo"));
        sock->disconnectFromHost();
        sock->deleteLater();
    });
    connect(sock, &QTcpSocket::errorOccurred, this,
            [this, sock](QAbstractSocket::SocketError) {
        m_kicadStatusLbl->setText(tr("\xe2\x9d\x8c  KiCAD MCP non raggiungibile"));
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
    dlg->setWindowTitle(tr("\xf0\x9f\x96\xa5  Installazione KiCAD MCP Server"));
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
            m_kicadStatusLbl->setText(tr("\xe2\x9c\x85  Eseguito in KiCAD"));
            m_kicadOutput->append("\n\xe2\x9c\x85  KiCAD: "
                + res["output"].toString("OK"));
        } else {
            m_kicadStatusLbl->setText(tr("\xe2\x9d\x8c  Errore KiCAD"));
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
    dlg->setWindowTitle(tr("\xe2\x9a\xa1  TinyMCP \xe2\x80\x94 Microcontrollori"));
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
    if (QProcess::execute("python3", {"-c", "import obsws_python"}) != 0) {
        m_obsStatusLbl->setText(tr("\xe2\x9d\x8c  obsws-python non installato"));
        m_obsOutput->append(
            "<span style='color:#f87171;'>"
            "\xe2\x9d\x8c  Modulo mancante \xe2\x80\x94 clicca per installarlo: "
            "<a href='pip://obsws-python' style='color:#fbbf24;'>"
            "\xf0\x9f\x94\xa7 Installa obsws-python</a>"
            "</span>");
        return;
    }
    const QString addr = m_obsHostEdit->text().trimmed();
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 4455;
    m_obsStatusLbl->setText(tr("\xf0\x9f\x94\x84  Connessione..."));
    auto* sock = new QTcpSocket(this);
    sock->connectToHost(host, static_cast<quint16>(port));
    connect(sock, &QTcpSocket::connected, this, [this, sock]() {
        sock->disconnectFromHost(); sock->deleteLater();
        m_obsStatusLbl->setText(tr("\xe2\x9c\x85  OBS WebSocket attivo"));
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
            m_obsStatusLbl->setText(tr("\xe2\x9d\x8c  Timeout \xe2\x80\x94 OBS non raggiungibile"));
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
    m_obsStatusLbl->setText(tr("\xf0\x9f\x94\x84  Esecuzione script..."));
    m_obsExecProc->start(P::findPython(), {tmpPath});
    if (m_obsExecProc->state() == QProcess::NotRunning)
        m_obsStatusLbl->setText(tr("\xe2\x9d\x8c  Python non trovato"));
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
    dlg->setWindowTitle(tr("\xf0\x9f\x94\xb4  Installazione OBS MCP"));
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
        m_godotStatusLbl->setText(tr("\xe2\x9d\x8c  Impossibile salvare il file"));
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

/* ======================================================================
   Sezione 12 — Telegram Bot slots
   ====================================================================== */


void AppControllerPage::onTelegramStartClicked()
{
    m_telegramIntentionalStop = false;
    if (QProcess::execute("python3",
            {"-c", "from telegram.ext import Application"}) != 0) {
        m_telegramStatusLbl->setText(
            "\xe2\x9d\x8c  python-telegram-bot v20+ non installato");
        m_telegramLog->append(
            "<span style='color:#f87171;'>"
            "\xe2\x9d\x8c  Modulo mancante \xe2\x80\x94 clicca qui per installarlo: "
            "<a href='pip://python-telegram-bot' style='color:#fbbf24;'>"
            "\xf0\x9f\x94\xa7 Installa python-telegram-bot</a>"
            "</span>");
        return;
    }

    const QString token = m_telegramTokenEdit->text().trimmed();
    if (token.isEmpty()) {
        m_telegramStatusLbl->setText(
            "\xe2\x9d\x8c  Inserisci il Bot Token prima di avviare.");
        return;
    }

    /* Se già in esecuzione, ferma prima */
    if (m_telegramProc && m_telegramProc->state() == QProcess::Running) {
        m_telegramProc->terminate();
        m_telegramProc->waitForFinished(P::kProcessStartTimeoutMs);
    }

    /* Usa direttamente il file versionato — non sovrascrivere */
    const QString scriptPath = P::root() + "/MCPs/telegram_bot_runtime.py";
    if (!QFileInfo::exists(scriptPath)) {
        m_telegramStatusLbl->setText(
            "\xe2\x9d\x8c  File non trovato: " + scriptPath);
        return;
    }

    /* Crea il QProcess se non esiste */
    if (!m_telegramProc) {
        m_telegramProc = new QProcess(this);
        m_telegramProc->setProcessChannelMode(QProcess::SeparateChannels);
        connect(m_telegramProc, &QProcess::readyReadStandardOutput,
                this, &AppControllerPage::onTelegramProcReadyRead);
        connect(m_telegramProc, &QProcess::readyReadStandardError,
                this, [this]() {
            const QString err =
                QString::fromUtf8(m_telegramProc->readAllStandardError()).trimmed();
            if (!err.isEmpty()) {
                m_telegramLog->append(
                    "<span style='color:#f87171;'>"
                    + err.toHtmlEscaped() + "</span>");
                LogBus::post("\xe2\x9d\x8c Telegram Bot: " + err);
            }
        });
        connect(m_telegramProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &AppControllerPage::onTelegramProcFinished);
    }

    /* Imposta le env var — token e whitelist fuori dal codice */
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TELEGRAM_TOKEN",     token);
    env.insert("TELEGRAM_WHITELIST", m_telegramWhitelistEdit->text().trimmed());
    m_telegramProc->setProcessEnvironment(env);

    m_telegramProc->start(P::findPython(), {scriptPath});

    if (m_telegramProc->waitForStarted(P::kProcessStartTimeoutMs)) {
        m_telegramStartBtn->setEnabled(false);
        m_telegramStopBtn->setEnabled(true);
        m_telegramStatusLbl->setText(
            "\xf0\x9f\x94\x84  Avvio in corso...");
        m_telegramLog->append(
            "<b>\xe2\x96\xb6 Bot avviato.</b> Attendo conferma da Telegram...");
    } else {
        m_telegramStatusLbl->setText(
            "\xe2\x9d\x8c  Errore avvio (python3 non trovato?)");
        m_telegramLog->append(
            "\xe2\x9d\x8c  python3 non trovato. Verifica l'installazione.");
    }
}

void AppControllerPage::onTelegramStopClicked()
{
    m_telegramIntentionalStop = true;
    if (m_telegramProc && m_telegramProc->state() == QProcess::Running) {
        m_telegramProc->terminate();
        if (!m_telegramProc->waitForFinished(P::kProcessStartTimeoutMs))
            m_telegramProc->kill();
    }
    /* onTelegramProcFinished aggiornerà lo stato */
}

void AppControllerPage::onTelegramProcReadyRead()
{
    while (m_telegramProc->canReadLine()) {
        const QString line =
            QString::fromUtf8(m_telegramProc->readLine()).trimmed();
        if (line.isEmpty()) continue;

        /* Tenta parse JSON */
        const QJsonObject obj =
            QJsonDocument::fromJson(line.toUtf8()).object();

        const QString type = obj.value("type").toString();

        if (type == "ready") {
            m_telegramStatusLbl->setText(
                "\xe2\x9c\x85  Bot attivo \xe2\x80\x94 in ascolto");
            m_telegramLog->append(
                "<b style='color:#4ade80;'>"
                "\xe2\x9c\x85 Bot pronto.</b>");
            continue;
        }

        if (type == "error") {
            const QString msg = obj.value("msg").toString();
            m_telegramStatusLbl->setText(tr("\xe2\x9d\x8c  Errore bot"));
            LogBus::post("\xe2\x9d\x8c Telegram Bot: " + msg);
            const bool isMissing = msg.contains("Modulo mancante",
                                                Qt::CaseInsensitive) ||
                                   msg.contains("No module named",
                                                Qt::CaseInsensitive) ||
                                   msg.contains("non installato",
                                                Qt::CaseInsensitive);
            if (isMissing) {
                m_telegramLog->append(
                    "<span style='color:#f87171;'>"
                    "\xe2\x9d\x8c  " + msg.toHtmlEscaped() + "<br>"
                    "Clicca per installare: "
                    "<a href='pip://python-telegram-bot' style='color:#fbbf24;'>"
                    "\xf0\x9f\x94\xa7 Installa python-telegram-bot</a>"
                    "</span>");
            } else {
                m_telegramLog->append(
                    "<span style='color:#f87171;'>"
                    "\xe2\x9d\x8c  " + msg.toHtmlEscaped() + "</span>");
            }
            continue;
        }

        if (type == "query") {
            const int     chatId = obj.value("chat_id").toInt();
            const QString text   = obj.value("text").toString();
            m_telegramChatId = chatId;

            m_telegramLog->append(
                QString("<b>\xf0\x9f\x93\xa9 [%1]</b> %2")
                    .arg(chatId)
                    .arg(text.toHtmlEscaped()));

            /* Invia al modello AI locale */
            if (m_telegramAiHolder) {
                m_telegramAiHolder->deleteLater();
                m_telegramAiHolder = nullptr;
            }
            m_telegramAiHolder = new QObject(this);

            const int savedChatId = chatId;

            connect(m_ai, &AiClient::finished,
                    m_telegramAiHolder,
                    [this, savedChatId](const QString& result) {
                if (m_telegramAiHolder) {
                    m_telegramAiHolder->deleteLater();
                    m_telegramAiHolder = nullptr;
                }
                /* Rimuovi blocchi <think> se presenti */
                QString clean = result;
                static const QRegularExpression reThink(
                    "<think>[\\s\\S]*?</think>",
                    QRegularExpression::CaseInsensitiveOption);
                clean.remove(reThink);
                clean = clean.trimmed();

                m_telegramLog->append(
                    QString("<b>\xf0\x9f\xa4\x96 Bot \xe2\x86\x92 [%1]</b> %2")
                        .arg(savedChatId)
                        .arg(clean.left(200).toHtmlEscaped()
                             + (clean.length() > 200 ? "..." : "")));

                /* Scrivi risposta sullo stdin del processo */
                if (m_telegramProc &&
                    m_telegramProc->state() == QProcess::Running) {
                    const QJsonObject resp{
                        {"chat_id", savedChatId},
                        {"reply",   clean}
                    };
                    const QByteArray payload =
                        QJsonDocument(resp).toJson(QJsonDocument::Compact)
                        + "\n";
                    m_telegramProc->write(payload);
                }
            });

            connect(m_ai, &AiClient::error,
                    m_telegramAiHolder,
                    [this, savedChatId](const QString& errMsg) {
                if (m_telegramAiHolder) {
                    m_telegramAiHolder->deleteLater();
                    m_telegramAiHolder = nullptr;
                }
                m_telegramLog->append(
                    "<span style='color:#f87171;'>"
                    "\xe2\x9d\x8c AI error: "
                    + errMsg.toHtmlEscaped() + "</span>");

                if (m_telegramProc &&
                    m_telegramProc->state() == QProcess::Running) {
                    const QJsonObject resp{
                        {"chat_id", savedChatId},
                        {"reply",   "Errore AI: " + errMsg}
                    };
                    m_telegramProc->write(
                        QJsonDocument(resp).toJson(QJsonDocument::Compact)
                        + "\n");
                }
            });

            m_ai->chat(
                "Sei Prismalux, un assistente AI locale. "
                "Rispondi in modo conciso e utile.",
                text);
            continue;
        }

        if (type == "new_contact") {
            const QString cid  = QString::number(obj.value("chat_id").toInt());
            const QString user = obj.value("username").toString();
            const QString name = obj.value("first_name").toString();

            /* Logga sempre nel pannello bot */
            const QString displayName = user.isEmpty() ? name : "@" + user;
            m_telegramLog->append(
                QString("<span style='color:#86efac;'>"
                        "\xf0\x9f\x91\xa4 Nuovo contatto: <b>%1</b> (ID: %2)</span>")
                    .arg(displayName.toHtmlEscaped(), cid));

            /* Aggiunge alla lista solo se checkbox attivo e ID non già presente */
            if (m_telegramAutoAddCheck && m_telegramAutoAddCheck->isChecked()) {
                bool found = false;
                for (int i = 0; i < m_telegramContactList->count(); ++i) {
                    if (m_telegramContactList->item(i)->text() == cid) {
                        found = true; break;
                    }
                }
                if (!found) {
                    m_telegramContactList->addItem(cid);
                    QSettings s("Prismalux", "GUI");
                    QStringList all;
                    all.reserve(m_telegramContactList->count());
                    for (int i = 0; i < m_telegramContactList->count(); ++i)
                        all << m_telegramContactList->item(i)->text();
                    s.setValue("telegram_contacts", all);
                    m_telegramLog->append(
                        QString("<span style='color:#86efac;'>"
                                "\xe2\x9c\x85 ID %1 aggiunto ai destinatari.</span>")
                            .arg(cid));
                }
            }
            continue;
        }

        /* Messaggio generico dal processo */
        m_telegramLog->append(line.toHtmlEscaped());
    }
}

void AppControllerPage::onTelegramProcFinished(
    int code, QProcess::ExitStatus /*status*/)
{
    if (!m_telegramStartBtn || !m_telegramStopBtn) return;
    m_telegramStartBtn->setEnabled(true);
    m_telegramStopBtn->setEnabled(false);

    /* Libera l'holder AI se era attivo */
    if (m_telegramAiHolder) {
        m_telegramAiHolder->deleteLater();
        m_telegramAiHolder = nullptr;
    }

    if (m_telegramIntentionalStop) {
        m_telegramStatusLbl->setText(tr("\xe2\x9a\xaa  Bot fermato"));
        m_telegramLog->append("<b>\xe2\x8f\xb9 Bot fermato.</b>");
        return;
    }

    /* Crash inatteso — mostra notifica con link riavvio */
    const QString exitInfo = (code == 0)
        ? tr("exit 0 (riavvio inatteso)")
        : QString("exit %1").arg(code);
    m_telegramStatusLbl->setText(
        QString("\xe2\x9a\xa0\xef\xb8\x8f  Bot crashato (%1)").arg(exitInfo));
    m_telegramLog->append(
        QString("<span style='color:#f87171;'>"
                "\xe2\x9a\xa0\xef\xb8\x8f  <b>Bot terminato inaspettatamente</b> (%1). "
                "Controlla il log sopra per la causa."
                "</span><br>"
                "<a href='tg-restart://' style='color:#fbbf24;'>"
                "\xf0\x9f\x94\x84 Riavvia bot Telegram</a>")
        .arg(exitInfo.toHtmlEscaped()));
    LogBus::post(
        QString("\xe2\x9a\xa0 Telegram Bot crashato (%1) — riavvio disponibile in app").arg(exitInfo));
}

/* ======================================================================
   Sezione 13 — Telegram contatti promozionali
   ====================================================================== */

void AppControllerPage::onTelegramAddContactClicked()
{
    const QString contact = m_telegramPromoContactEdit->text().trimmed();
    if (contact.isEmpty())
        return;
    for (int i = 0; i < m_telegramContactList->count(); ++i) {
        if (m_telegramContactList->item(i)->text() == contact)
            return;
    }
    m_telegramContactList->addItem(contact);
    m_telegramPromoContactEdit->clear();

    QStringList all;
    all.reserve(m_telegramContactList->count());
    for (int i = 0; i < m_telegramContactList->count(); ++i)
        all << m_telegramContactList->item(i)->text();
    QSettings s("Prismalux", "GUI");
    s.setValue("telegram_contacts", all);
}

void AppControllerPage::onTelegramRemoveContactClicked()
{
    const int row = m_telegramContactList->currentRow();
    if (row < 0)
        return;
    delete m_telegramContactList->takeItem(row);

    QStringList all;
    all.reserve(m_telegramContactList->count());
    for (int i = 0; i < m_telegramContactList->count(); ++i)
        all << m_telegramContactList->item(i)->text();
    QSettings s("Prismalux", "GUI");
    s.setValue("telegram_contacts", all);
}

void AppControllerPage::onTelegramSendPromoClicked()
{
    const QString token = m_telegramTokenEdit->text().trimmed();
    if (token.isEmpty()) {
        m_telegramPromoStatusLbl->setText(
            "\xe2\x9d\x8c  Inserisci il Bot Token prima di inviare.");
        return;
    }
    const QString msg = m_telegramPromoMsgEdit->toPlainText().trimmed();
    if (msg.isEmpty()) {
        m_telegramPromoStatusLbl->setText(
            "\xe2\x9d\x8c  Scrivi il messaggio prima di inviare.");
        return;
    }
    const int total = m_telegramContactList->count();
    if (total == 0) {
        m_telegramPromoStatusLbl->setText(
            "\xe2\x9d\x8c  Nessun contatto in lista.");
        return;
    }

    m_telegramPromoStatusLbl->setText(
        QString("\xf0\x9f\x93\xa4  Invio a %1 contatti...").arg(total));

    for (int i = 0; i < total; ++i) {
        const QString chatId = m_telegramContactList->item(i)->text();

        const QUrl url(QString("https://api.telegram.org/bot%1/sendMessage")
                       .arg(token));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        const QJsonObject body{
            {"chat_id", chatId},
            {"text",    msg},
            {"parse_mode", "HTML"}
        };
        QNetworkReply* reply =
            m_telegramPromoNam->post(
                req, QJsonDocument(body).toJson(QJsonDocument::Compact));

        const int idx = i + 1;
        connect(reply, &QNetworkReply::finished,
                this, [this, reply, idx, total, chatId]() {
            const QByteArray body = reply->readAll();
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                /* Legge la descrizione reale dall'API Telegram */
                const QJsonObject tgObj = QJsonDocument::fromJson(body).object();
                const int errCode = tgObj.value("error_code").toInt(
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
                const QString tgDesc = tgObj.value("description").toString();

                QString hint;
                if (errCode == 403) {
                    if (tgDesc.contains("initiate"))
                        hint = " \xe2\x86\x92 il contatto deve inviare /start al bot";
                    else if (tgDesc.contains("blocked"))
                        hint = " \xe2\x86\x92 il contatto ha bloccato il bot";
                    else if (tgDesc.contains("not a member"))
                        hint = " \xe2\x86\x92 il bot non \xc3\xa8 membro del gruppo";
                    else if (chatId.startsWith('@') && !chatId.startsWith("-"))
                        hint = " \xe2\x86\x92 i bot non possono ricevere messaggi da altri bot";
                } else if (errCode == 400) {
                    if (tgDesc.contains("chat not found"))
                        hint = " \xe2\x86\x92 ID non trovato: usa l'ID numerico dopo che il contatto ha inviato /start";
                    else if (!chatId.at(0).isDigit() && !chatId.startsWith('-'))
                        hint = " \xe2\x86\x92 usa l'ID numerico, non @username";
                }

                const QString detail = tgDesc.isEmpty() ? reply->errorString()
                                                        : tgDesc;
                const QString errMsg =
                    QString("\xe2\x9d\x8c  Errore su %1 (%2): %3%4")
                        .arg(chatId).arg(errCode).arg(detail, hint);
                m_telegramPromoStatusLbl->setText(errMsg);
                LogBus::post("Telegram: " + errMsg);
            } else if (idx == total) {
                m_telegramPromoStatusLbl->setText(
                    QString("\xe2\x9c\x85  Inviato a %1 contatti.").arg(total));
            }
        });
    }
}

/* ======================================================================
   Sezione 14 — WhatsApp contatti promozionali
   ====================================================================== */

void AppControllerPage::onWaAddContactClicked()
{
    const QString contact = m_waPromoContactEdit->text().trimmed();
    if (contact.isEmpty())
        return;
    for (int i = 0; i < m_waContactList->count(); ++i) {
        if (m_waContactList->item(i)->text() == contact)
            return;
    }
    m_waContactList->addItem(contact);
    m_waPromoContactEdit->clear();

    QStringList all;
    all.reserve(m_waContactList->count());
    for (int i = 0; i < m_waContactList->count(); ++i)
        all << m_waContactList->item(i)->text();
    QSettings s("Prismalux", "GUI");
    s.setValue("whatsapp_contacts", all);
}

void AppControllerPage::onWaRemoveContactClicked()
{
    const int row = m_waContactList->currentRow();
    if (row < 0)
        return;
    delete m_waContactList->takeItem(row);

    QStringList all;
    all.reserve(m_waContactList->count());
    for (int i = 0; i < m_waContactList->count(); ++i)
        all << m_waContactList->item(i)->text();
    QSettings s("Prismalux", "GUI");
    s.setValue("whatsapp_contacts", all);
}

void AppControllerPage::onWaSendPromoClicked()
{
    const QString bridgeUrl = m_waBridgeUrlEdit->text().trimmed();
    if (bridgeUrl.isEmpty()) {
        m_waPromoStatusLbl->setText(
            "\xe2\x9d\x8c  Inserisci l\xe2\x80\x99" "URL del bridge.");
        return;
    }
    const QString msg = m_waPromoMsgEdit->toPlainText().trimmed();
    if (msg.isEmpty()) {
        m_waPromoStatusLbl->setText(
            "\xe2\x9d\x8c  Scrivi il messaggio prima di inviare.");
        return;
    }
    const int total = m_waContactList->count();
    if (total == 0) {
        m_waPromoStatusLbl->setText(
            "\xe2\x9d\x8c  Nessun contatto in lista.");
        return;
    }

    m_waPromoStatusLbl->setText(
        QString("\xf0\x9f\x93\xa4  Invio a %1 contatti...").arg(total));

    for (int i = 0; i < total; ++i) {
        const QString number = m_waContactList->item(i)->text();

        const QUrl url(bridgeUrl.endsWith('/') ?
                       bridgeUrl + "send" :
                       bridgeUrl + "/send");
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        const QJsonObject body{
            {"phone",   number},
            {"message", msg}
        };
        QNetworkReply* reply =
            m_waPromoNam->post(
                req, QJsonDocument(body).toJson(QJsonDocument::Compact));

        const int idx = i + 1;
        connect(reply, &QNetworkReply::finished,
                this, [this, reply, idx, total, number]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                const QString errMsg =
                    QString("\xe2\x9d\x8c  Errore su %1: %2")
                        .arg(number, reply->errorString());
                m_waPromoStatusLbl->setText(errMsg);
                LogBus::post("WhatsApp: " + errMsg);
            } else if (idx == total) {
                m_waPromoStatusLbl->setText(
                    QString("\xe2\x9c\x85  Inviato a %1 contatti.").arg(total));
            }
        });
    }
}

/* ══════════════════════════════════════════════════════════════
   WhatsApp Bot Rispondente AI — polling bridge + risposta AI
   ══════════════════════════════════════════════════════════════ */

void AppControllerPage::onWaBotStartClicked()
{
    const QString bridgeUrl = m_waBridgeUrlEdit->text().trimmed();
    if (bridgeUrl.isEmpty()) {
        m_waBotStatusLbl->setText(
            "\xe2\x9d\x8c  Inserisci l\xe2\x80\x99" "URL del bridge prima di avviare.");
        return;
    }

    m_waSeenMsgIds.clear();
    m_waBotLog->clear();
    m_waBotLog->append(
        QString("\xf0\x9f\x9f\xa2  Bot avviato \xe2\x80\x94 bridge: <b>%1</b>")
            .arg(bridgeUrl.toHtmlEscaped()));

    if (!m_waPollTimer) {
        m_waPollTimer = new QTimer(this);
        m_waPollTimer->setInterval(2000);
        connect(m_waPollTimer, &QTimer::timeout,
                this, &AppControllerPage::onWaPollTick);
    }
    m_waPollTimer->start();

    m_waBotStartBtn->setEnabled(false);
    m_waBotStopBtn->setEnabled(true);
    m_waBotStatusLbl->setText(tr("\xf0\x9f\x9f\xa2  Bot attivo — polling ogni 2s"));

    QSettings s("Prismalux", "GUI");
    s.setValue("whatsapp/bot_whitelist", m_waWhitelistEdit->text().trimmed());
    s.setValue("whatsapp/bot_auto_reply", m_waAutoReplyCheck->isChecked());
}

void AppControllerPage::onWaBotStopClicked()
{
    if (m_waPollTimer) {
        m_waPollTimer->stop();
    }
    if (m_waBotAiHolder) {
        m_waBotAiHolder->deleteLater();
        m_waBotAiHolder = nullptr;
    }
    m_waBotStartBtn->setEnabled(true);
    m_waBotStopBtn->setEnabled(false);
    m_waBotStatusLbl->setText(tr("\xe2\x9a\xab  Bot fermato"));
    m_waBotLog->append("\xf0\x9f\x94\xb4  Bot fermato.");
}

void AppControllerPage::onWaPollTick()
{
    const QString bridgeUrl = m_waBridgeUrlEdit->text().trimmed();
    if (bridgeUrl.isEmpty()) return;

    const QUrl url(bridgeUrl.endsWith('/')
        ? bridgeUrl + "messages"
        : bridgeUrl + "/messages");

    QNetworkRequest req(url);
    req.setTransferTimeout(1500);
    auto* reply = m_waNam->get(req);
    connect(reply, &QNetworkReply::finished,
            this, &AppControllerPage::onWaPollReply);
}

void AppControllerPage::onWaPollReply()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        ++m_waPollFailCount;
        m_waBotStatusLbl->setText(
            QString("\xe2\x9a\xa0\xef\xb8\x8f  Bridge non raggiungibile: %1")
                .arg(reply->errorString()));
        /* Dopo 5 fallimenti consecutivi offri restart del polling */
        if (m_waPollFailCount == 5) {
            m_waBotLog->append(
                "<span style='color:#f87171;'>"
                "\xe2\x9a\xa0\xef\xb8\x8f  <b>Bridge irraggiungibile da 5 poll consecutivi.</b><br>"
                "Verifica che il server WhatsApp bridge sia avviato, poi:"
                "</span><br>"
                "<a href='wa-restart://' style='color:#fbbf24;'>"
                "\xf0\x9f\x94\x84 Riavvia polling WhatsApp</a>");
        }
        return;
    }
    m_waPollFailCount = 0;

    if (!m_waAutoReplyCheck || !m_waAutoReplyCheck->isChecked()) return;

    const QByteArray body = reply->readAll();
    const auto doc = QJsonDocument::fromJson(body);
    if (!doc.isArray()) return;

    const QStringList whitelist = m_waWhitelistEdit
        ? m_waWhitelistEdit->text().split(',', Qt::SkipEmptyParts)
        : QStringList{};

    const QJsonArray msgs = doc.array();
    for (const QJsonValue& v : msgs) {
        const QJsonObject msg = v.toObject();
        const QString id     = msg.value("id").toString();
        const QString from   = msg.value("from").toString().trimmed();
        const QString text   = msg.value("body").toString().trimmed();

        if (id.isEmpty() || m_waSeenMsgIds.contains(id)) continue;
        m_waSeenMsgIds.insert(id);

        if (text.isEmpty()) continue;

        /* Fail-closed: whitelist vuota = nessuno autorizzato (WhatsApp è rete pubblica).
           Confronto sul numero normalizzato (solo cifre) — niente match per sottostringa,
           che permetteva a una voce parziale di autorizzare numeri non previsti. */
        if (whitelist.isEmpty()) continue;
        QString fromNorm; for (const QChar& c : from) if (c.isDigit()) fromNorm += c;
        bool authorized = false;
        for (const QString& wl : whitelist) {
            QString wlNorm; for (const QChar& c : wl) if (c.isDigit()) wlNorm += c;
            if (wlNorm.size() < 6) continue;  // ignora voci troppo corte (evita match larghi)
            if (fromNorm == wlNorm || (wlNorm.size() >= 9 && fromNorm.endsWith(wlNorm))) {
                authorized = true; break;
            }
        }
        if (!authorized) continue;

        m_waBotLog->append(
            QString("\xf0\x9f\x93\xa8  <b>%1</b>: %2")
                .arg(from.toHtmlEscaped(), text.toHtmlEscaped()));

        /* Invia all'AI locale per la risposta */
        if (m_waBotAiHolder) {
            m_waBotAiHolder->deleteLater();
            m_waBotAiHolder = nullptr;
        }
        m_waBotAiHolder = new QObject(this);

        const QString sysPrompt =
            "Sei un assistente AI su WhatsApp. "
            "Rispondi in modo conciso e utile al messaggio dell'utente.";
        const QString fromCopy = from;

        connect(m_ai, &AiClient::finished, m_waBotAiHolder,
                [this, fromCopy](const QString& replyText) {
            if (m_waBotAiHolder) {
                m_waBotAiHolder->deleteLater();
                m_waBotAiHolder = nullptr;
            }
            onWaBotSendReply(fromCopy, replyText);
        });
        connect(m_ai, &AiClient::error, m_waBotAiHolder,
                [this](const QString& err) {
            if (m_waBotAiHolder) {
                m_waBotAiHolder->deleteLater();
                m_waBotAiHolder = nullptr;
            }
            m_waBotLog->append(
                QString("\xe2\x9d\x8c  AI error: %1").arg(err.toHtmlEscaped()));
        });
        m_ai->chat(sysPrompt, text);
        break;   /* un messaggio alla volta per non sovraccaricare l'AI */
    }
}

void AppControllerPage::onWaBotSendReply(const QString& toNumber,
                                          const QString& replyText)
{
    const QString bridgeUrl = m_waBridgeUrlEdit->text().trimmed();
    if (bridgeUrl.isEmpty() || replyText.trimmed().isEmpty()) return;

    m_waBotLog->append(
        QString("\xf0\x9f\xa4\x96  Risposta a <b>%1</b>: %2")
            .arg(toNumber.toHtmlEscaped(), replyText.left(120).toHtmlEscaped()));

    const QUrl url(bridgeUrl.endsWith('/')
        ? bridgeUrl + "send"
        : bridgeUrl + "/send");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(5000);

    const QJsonObject body{
        {"phone",   toNumber},
        {"message", replyText}
    };
    auto* reply = m_waNam->post(req,
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, [reply]() {
        reply->deleteLater();
    });
}

/* ══════════════════════════════════════════════════════════════
   Dev Agent LangGraph — modifica il codice di Prismalux in autonomia
   ══════════════════════════════════════════════════════════════ */

void AppControllerPage::onDevAgentRunClicked()
{
    const QString task = m_devTaskEdit ? m_devTaskEdit->text().trimmed() : QString();
    if (task.isEmpty()) {
        if (m_devStatusLbl) m_devStatusLbl->setText(
            "\xe2\x9d\x8c  Descrivi il task prima di avviare.");
        return;
    }

    /* Percorso server Python */
    const QString scriptPath = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(scriptPath)) {
        if (m_devLog) m_devLog->append(
            "\xe2\x9d\x8c  <b>server.py non trovato</b> \xe2\x80\x94 "
            "Percorso atteso: <code>" + scriptPath + "</code>");
        return;
    }

    /* Avvia processo se non già in esecuzione */
    if (m_devProc && m_devProc->state() != QProcess::NotRunning) {
        m_devProc->terminate();
        m_devProc->waitForFinished(2000);
    }
    if (!m_devProc) {
        m_devProc = new QProcess(this);
        m_devProc->setProcessChannelMode(QProcess::SeparateChannels);
        connect(m_devProc, &QProcess::readyReadStandardOutput,
                this, &AppControllerPage::onDevAgentReadOutput);
        connect(m_devProc, &QProcess::readyReadStandardError,
                this, &AppControllerPage::onDevAgentReadError);
        connect(m_devProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &AppControllerPage::onDevAgentFinished);
    }

    if (m_devLog)  m_devLog->clear();
    if (m_devDiff) m_devDiff->clear();
    m_devPendingOutput.clear();

    const QString model = m_devModelCombo
        ? m_devModelCombo->currentData().toString()
        : QStringLiteral("deepseek-coder:6.7b");

    m_devProc->start(P::findPython(), {scriptPath});
    if (!m_devProc->waitForStarted(P::kProcessStartTimeoutMs)) {
        if (m_devStatusLbl) m_devStatusLbl->setText(
            "\xe2\x9d\x8c  Impossibile avviare il server Dev Agent.");
        return;
    }

    /* Invia il task via stdin */
    const QJsonObject req{
        {"task",         task},
        {"model",        model},
        {"project_root", P::root()}
    };
    m_devProc->write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");

    m_devRunBtn->setEnabled(false);
    m_devStopBtn->setEnabled(true);
    if (m_devStatusLbl) m_devStatusLbl->setText(
        "\xf0\x9f\x9f\xa1  Dev Agent in esecuzione...");
    if (m_devLog) m_devLog->append(
        QString("\xf0\x9f\x9a\x80  <b>Task:</b> %1<br>"
                "\xf0\x9f\xa4\x96  Modello: %2")
            .arg(task.toHtmlEscaped(), model));
}

void AppControllerPage::onDevAgentLoadHistory()
{
    const QString scriptPath = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(scriptPath)) return;

    /* Avvia processo solo per list_history — processo usa stdin e poi esce */
    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::SeparateChannels);
    proc->start(P::findPython(), {scriptPath});
    if (!proc->waitForStarted(2000)) { proc->deleteLater(); return; }

    proc->write(QJsonDocument(QJsonObject{
        {"cmd", "list_history"}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc](int, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "history_list") continue;
            if (!m_devHistoryList) return;
            m_devHistoryList->clear();
            const QJsonArray entries = obj.value("entries").toArray();
            for (const QJsonValue& v : entries) {
                const QJsonObject e = v.toObject();
                const QString ts   = e.value("timestamp").toString().left(19).replace("T"," ");
                const QString task = e.value("task").toString().left(60);
                const QString id   = e.value("id").toString();
                const int nf       = e.value("n_files").toInt();
                auto* item = new QListWidgetItem(
                    QString("\xf0\x9f\x95\x90  %1  |  %2  (%3 file)")
                        .arg(ts, task).arg(nf),
                    m_devHistoryList);
                item->setData(Qt::UserRole, id);
                m_devHistoryList->addItem(item);
            }
            if (m_devHistoryList->count() == 0) {
                auto* item = new QListWidgetItem(
                    "(nessuno snapshot salvato ancora)", m_devHistoryList);
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                m_devHistoryList->addItem(item);
            }
        }
    });
}

void AppControllerPage::onDevAgentRestoreClicked()
{
    if (!m_devHistoryList) return;
    auto* selected = m_devHistoryList->currentItem();
    if (!selected || !(selected->flags() & Qt::ItemIsEnabled)) return;

    const QString backupId = selected->data(Qt::UserRole).toString();
    if (backupId.isEmpty()) return;

    const auto ans = QMessageBox::question(
        this, "\xe2\x86\xa9  Ripristina snapshot",
        QString("Ripristinare lo snapshot:\n<b>%1</b>\n\n"
                "I file modificati torneranno allo stato precedente.\n"
                "Questa operazione non pu\xc3\xb2 essere annullata.")
            .arg(backupId),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans != QMessageBox::Yes) return;

    const QString scriptPath = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(scriptPath)) {
        if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xe2\x9d\x8c  server.py non trovato"));
        return;
    }

    if (m_devLog) m_devLog->append(
        QString("\xe2\x8f\xaa  <b>Ripristino snapshot:</b> %1...").arg(backupId));
    if (m_devStatusLbl) m_devStatusLbl->setText(
        "\xe2\x8f\xaa  Ripristino in corso...");

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::SeparateChannels);
    proc->start(P::findPython(), {scriptPath});
    if (!proc->waitForStarted(2000)) {
        if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xe2\x9d\x8c  Impossibile avviare il processo"));
        proc->deleteLater();
        return;
    }
    proc->write(QJsonDocument(QJsonObject{
        {"cmd",       "restore"},
        {"backup_id", backupId}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc, backupId](int, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "restore_done") continue;
            const bool ok  = obj.value("success").toBool();
            const QString msg = obj.value("msg").toString();
            if (m_devLog) m_devLog->append(
                (ok ? "\xe2\x9c\x85  <b>Ripristino completato</b>: " : "\xe2\x9d\x8c  ") +
                msg.toHtmlEscaped());
            if (m_devStatusLbl) m_devStatusLbl->setText(
                ok ? "\xe2\x9c\x85  Ripristinato" : "\xe2\x9d\x8c  Ripristino fallito");
        }
    });
}

void AppControllerPage::onDevAgentStopClicked()
{
    if (m_devProc && m_devProc->state() != QProcess::NotRunning) {
        m_devProc->terminate();
        if (!m_devProc->waitForFinished(P::kProcessStartTimeoutMs))
            m_devProc->kill();
    }
    if (m_devRunBtn)  m_devRunBtn->setEnabled(true);
    if (m_devStopBtn) m_devStopBtn->setEnabled(false);
    if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xe2\x9a\xab  Fermato"));
    if (m_devLog) m_devLog->append("\xf0\x9f\x94\xb4  Dev Agent fermato.");
}

void AppControllerPage::onDevAgentInstallClicked()
{
    if (m_devInstallBtn) m_devInstallBtn->setEnabled(false);
    if (m_devStatusLbl)  m_devStatusLbl->setText(
        "\xe2\x8f\xb3  Installazione dipendenze LangGraph...");

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAll()).trimmed().right(200);
        if (code == 0) {
            if (m_devStatusLbl) m_devStatusLbl->setText(
                "\xe2\x9c\x85  LangGraph installato correttamente.");
            if (m_devLog) m_devLog->append(
                "\xe2\x9c\x85  <b>LangGraph installato</b> \xe2\x80\x94 pronto per l\xe2\x80\x99"
                "avvio del Dev Agent.");
        } else {
            if (m_devStatusLbl) m_devStatusLbl->setText(
                "\xe2\x9d\x8c  Installazione fallita (code " + QString::number(code) + ")");
            if (m_devLog) m_devLog->append(
                "<span style='color:#f87171;'>"
                "\xe2\x9d\x8c  Errore installazione:<br><pre>" + out.toHtmlEscaped() + "</pre>"
                "Riprova manualmente: "
                "<a href='pip://langgraph langchain-community langchain-ollama unidiff'"
                " style='color:#fbbf24;'>"
                "\xf0\x9f\x94\xa7 Installa LangGraph + dipendenze</a>"
                "</span>");
        }
        if (m_devInstallBtn) m_devInstallBtn->setEnabled(true);
        proc->deleteLater();
    });
    proc->start(P::findPython(),
        {"-m", "pip", "install", "--quiet", "--break-system-packages",
         "langgraph", "langchain-community", "langchain-ollama", "unidiff"});
}

void AppControllerPage::onDevAgentReadOutput()
{
    if (!m_devProc) return;
    m_devPendingOutput += QString::fromUtf8(m_devProc->readAllStandardOutput());

    /* Processa righe complete */
    while (m_devPendingOutput.contains('\n')) {
        const int nl = m_devPendingOutput.indexOf('\n');
        const QString line = m_devPendingOutput.left(nl).trimmed();
        m_devPendingOutput.remove(0, nl + 1);
        if (line.isEmpty()) continue;

        const auto doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) {
            if (m_devLog) m_devLog->append(line.toHtmlEscaped());
            continue;
        }
        const QJsonObject obj = doc.object();
        const QString evt = obj.value("event").toString();

        if (evt == "step") {
            const QString node = obj.value("node").toString();
            const QString msg  = obj.value("msg").toString();
            const QJsonArray files = obj.value("files").toArray();
            QString html = QString("\xf0\x9f\x94\xb9  <b>%1</b>").arg(node.toHtmlEscaped());
            if (!msg.isEmpty())   html += " \xe2\x80\x94 " + msg.toHtmlEscaped();
            if (!files.isEmpty()) {
                QStringList fl;
                for (const auto& f : files) fl << f.toString();
                html += "<br><small>" + fl.join(", ").toHtmlEscaped() + "</small>";
            }
            if (m_devLog) m_devLog->append(html);

        } else if (evt == "compile_output") {
            const bool ok = obj.value("ok").toBool();
            const QString out = obj.value("output").toString().right(300);
            const QString icon = ok ? "\xe2\x9c\x85" : "\xe2\x9d\x8c";
            if (m_devLog) m_devLog->append(
                icon + "  <b>Compilazione</b> " + (ok ? "OK" : "ERRORI") +
                "<br><pre style='font-size:9px'>" + out.toHtmlEscaped() + "</pre>");

        } else if (evt == "backup_created") {
            /* Aggiunge la nuova entry in cima alla lista cronologia */
            const QString bid  = obj.value("backup_id").toString();
            const QString ts   = obj.value("timestamp").toString().left(19).replace("T"," ");
            const QString task = obj.value("task").toString().left(60);
            const int     nf   = obj.value("n_files").toInt();
            if (m_devHistoryList) {
                auto* item = new QListWidgetItem(
                    QString("\xf0\x9f\x95\x90  %1  |  %2  (%3 file)")
                        .arg(ts, task).arg(nf));
                item->setData(Qt::UserRole, bid);
                m_devHistoryList->insertItem(0, item);
            }
            if (m_devLog) m_devLog->append(
                QString("\xf0\x9f\x92\xbe  Snapshot salvato: <b>%1</b> (%2 file)")
                    .arg(bid.toHtmlEscaped()).arg(nf));

        } else if (evt == "done") {
            const bool success = obj.value("success").toBool();
            const QString msg  = obj.value("msg").toString();
            const QString diff = obj.value("diff").toString();
            if (m_devDiff && !diff.isEmpty()) {
                /* Colora diff: righe + verde, righe - rosso */
                QString html;
                for (const QString& l : diff.split('\n')) {
                    if (l.startsWith('+'))
                        html += QString("<span style='color:#4ade80'>%1</span><br>")
                                    .arg(l.toHtmlEscaped());
                    else if (l.startsWith('-'))
                        html += QString("<span style='color:#f87171'>%1</span><br>")
                                    .arg(l.toHtmlEscaped());
                    else
                        html += l.toHtmlEscaped() + "<br>";
                }
                m_devDiff->setHtml("<pre style='font-size:9px'>" + html + "</pre>");
            }
            const QString icon = success ? "\xf0\x9f\x9f\xa2" : "\xf0\x9f\x94\xb4";
            if (m_devLog) m_devLog->append(
                icon + "  <b>" + (success ? "Completato" : "Fallito") + "</b>"
                + (msg.isEmpty() ? "" : " \xe2\x80\x94 " + msg.toHtmlEscaped()));
            if (m_devStatusLbl) m_devStatusLbl->setText(
                success ? "\xf0\x9f\x9f\xa2  Completato" : "\xf0\x9f\x94\xb4  Fallito");
        }
    }
}

void AppControllerPage::onDevAgentReadError()
{
    if (!m_devProc) return;
    const QString err = QString::fromUtf8(
        m_devProc->readAllStandardError()).trimmed();
    if (!err.isEmpty() && m_devLog)
        m_devLog->append(
            "<span style='color:#888'><i>" + err.left(200).toHtmlEscaped() + "</i></span>");
}

void AppControllerPage::onDevAgentFinished(int code, QProcess::ExitStatus)
{
    if (m_devRunBtn)  m_devRunBtn->setEnabled(true);
    if (m_devStopBtn) m_devStopBtn->setEnabled(false);
    if (code != 0 && m_devStatusLbl)
        m_devStatusLbl->setText(
            QString("\xe2\x9d\x8c  Processo terminato (code %1)").arg(code));
}

/* ── Helper interno: avvia processo Python one-shot per comandi git ── */
static QProcess* devGitProc(QWidget* parent, const QString& scriptPath)
{
    auto* proc = new QProcess(parent);
    proc->setProcessChannelMode(QProcess::SeparateChannels);
    proc->start(PrismaluxPaths::findPython(), {scriptPath});
    if (!proc->waitForStarted(P::kProcessStartTimeoutMs)) {
        proc->deleteLater();
        return nullptr;
    }
    return proc;
}

void AppControllerPage::onDevAgentGitLogClicked()
{
    const QString script = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(script)) return;
    auto* proc = devGitProc(this, script);
    if (!proc) return;

    proc->write(QJsonDocument(QJsonObject{
        {"cmd",          "git_log"},
        {"project_root", P::root()},
        {"n",            25}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc](int, QProcess::ExitStatus) {
        const QString raw = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        if (!m_devGitLogList) return;
        m_devGitLogList->clear();
        for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "git_log") continue;
            const QJsonArray entries = obj.value("entries").toArray();
            for (const QJsonValue& v : entries) {
                const QJsonObject e  = v.toObject();
                const QString hash   = e.value("hash").toString();
                const QString shash  = e.value("short_hash").toString();
                const QString date   = e.value("date").toString();
                const QString subj   = e.value("subject").toString();
                auto* item = new QListWidgetItem(
                    QString("%1  %2  %3").arg(shash, date, subj.left(60)));
                item->setData(Qt::UserRole, hash);
                m_devGitLogList->addItem(item);
            }
            if (m_devGitLogList->count() == 0) {
                auto* item = new QListWidgetItem("(nessun commit trovato)");
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                m_devGitLogList->addItem(item);
            }
        }
    });
}

void AppControllerPage::onDevAgentGitRestoreClicked()
{
    if (!m_devGitLogList) return;
    auto* sel = m_devGitLogList->currentItem();
    if (!sel || !(sel->flags() & Qt::ItemIsEnabled)) return;

    const QString commit = sel->data(Qt::UserRole).toString();
    if (commit.isEmpty()) return;

    const QString label  = sel->text().left(80);
    const auto ans = QMessageBox::question(
        this, "\xe2\x86\xa9  Ripristina al commit",
        QString("Ripristinare il worktree al commit:\n<b>%1</b>\n\n"
                "Tutte le modifiche locali non committate andranno perse.\n"
                "Questa operazione non pu\xc3\xb2 essere annullata.")
            .arg(label),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans != QMessageBox::Yes) return;

    const QString script = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(script)) return;
    auto* proc = devGitProc(this, script);
    if (!proc) return;

    if (m_devLog) m_devLog->append(
        QString("\xe2\x8f\xaa  <b>git reset --hard</b> %1...").arg(commit.left(8)));
    if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xe2\x8f\xaa  Ripristino commit..."));

    proc->write(QJsonDocument(QJsonObject{
        {"cmd",          "git_restore"},
        {"project_root", P::root()},
        {"commit",       commit},
        {"files",        QJsonArray{}}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc, commit](int, QProcess::ExitStatus) {
        const QString raw = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "git_restore_done") continue;
            const bool ok  = obj.value("success").toBool();
            const QString msg = obj.value("msg").toString();
            const QString icon = ok ? "\xf0\x9f\x9f\xa2" : "\xf0\x9f\x94\xb4";
            if (m_devLog) m_devLog->append(
                icon + "  <b>Ripristino commit</b> " + commit.left(8).toHtmlEscaped()
                + "<br>" + msg.toHtmlEscaped());
            if (m_devStatusLbl) m_devStatusLbl->setText(
                ok ? "\xf0\x9f\x9f\xa2  Ripristinato" : "\xf0\x9f\x94\xb4  Errore ripristino");
        }
    });
}

void AppControllerPage::onDevAgentGitFetchResetClicked()
{
    const QString branch = m_devGitBranchEdit
        ? m_devGitBranchEdit->text().trimmed()
        : QStringLiteral("master");

    const auto ans = QMessageBox::warning(
        this, "\xf0\x9f\x8c\x90  Fetch + Reset da GitHub",
        QString("Eseguire:\n  git fetch origin\n  git reset --hard origin/%1\n\n"
                "\xe2\x9a\xa0  Tutte le modifiche locali non committate "
                "andranno PERSE.\n"
                "Assicurati di aver fatto uno stash o un commit prima di procedere.")
            .arg(branch),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (ans != QMessageBox::Yes) return;

    const QString script = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(script)) return;
    auto* proc = devGitProc(this, script);
    if (!proc) return;

    if (m_devLog) m_devLog->append(
        QString("\xf0\x9f\x8c\x90  <b>git fetch origin && git reset --hard origin/%1</b>...")
            .arg(branch.toHtmlEscaped()));
    if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xf0\x9f\x8c\x90  Fetch in corso..."));

    proc->write(QJsonDocument(QJsonObject{
        {"cmd",          "git_fetch_reset"},
        {"project_root", P::root()},
        {"remote",       "origin"},
        {"branch",       branch}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc, branch](int, QProcess::ExitStatus) {
        const QString raw = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "git_fetch_reset_done") continue;
            const bool ok     = obj.value("success").toBool();
            const QString msg = obj.value("msg").toString();
            const QString icon = ok ? "\xf0\x9f\x9f\xa2" : "\xf0\x9f\x94\xb4";
            if (m_devLog) m_devLog->append(
                icon + "  <b>Fetch+Reset origin/" + branch.toHtmlEscaped() + "</b><br>"
                + "<pre style='font-size:9px'>" + msg.toHtmlEscaped() + "</pre>");
            if (m_devStatusLbl) m_devStatusLbl->setText(
                ok ? "\xf0\x9f\x9f\xa2  Sincronizzato con GitHub"
                   : "\xf0\x9f\x94\xb4  Errore fetch/reset");
            if (ok) {
                /* Aggiorna il log commit dopo il fetch */
                QTimer::singleShot(200, this, &AppControllerPage::onDevAgentGitLogClicked);
            }
        }
    });
}

void AppControllerPage::onDevAgentGitStashPushClicked()
{
    const QString script = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(script)) return;
    auto* proc = devGitProc(this, script);
    if (!proc) return;

    if (m_devLog) m_devLog->append("\xf0\x9f\x93\xa6  <b>git stash push</b>...");
    if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xf0\x9f\x93\xa6  Stash in corso..."));

    proc->write(QJsonDocument(QJsonObject{
        {"cmd",          "git_stash_push"},
        {"project_root", P::root()},
        {"message",      "devagent stash"}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc](int, QProcess::ExitStatus) {
        const QString raw = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "git_stash_done") continue;
            const bool ok     = obj.value("success").toBool();
            const QString msg = obj.value("msg").toString();
            const QString icon = ok ? "\xf0\x9f\x9f\xa2" : "\xf0\x9f\x94\xb4";
            if (m_devLog) m_devLog->append(
                icon + "  <b>Stash push</b>: " + msg.toHtmlEscaped());
            if (m_devStatusLbl) m_devStatusLbl->setText(
                ok ? "\xf0\x9f\x9f\xa2  Stash salvato"
                   : "\xf0\x9f\x94\xb4  Stash fallito");
            if (ok)
                QTimer::singleShot(100, this,
                    &AppControllerPage::onDevAgentGitStashListClicked);
        }
    });
}

void AppControllerPage::onDevAgentGitStashListClicked()
{
    const QString script = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(script)) return;
    auto* proc = devGitProc(this, script);
    if (!proc) return;

    proc->write(QJsonDocument(QJsonObject{
        {"cmd",          "git_stash_list"},
        {"project_root", P::root()}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc](int, QProcess::ExitStatus) {
        const QString raw = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        if (!m_devStashList) return;
        m_devStashList->clear();
        for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "git_stash_list") continue;
            const QJsonArray entries = obj.value("entries").toArray();
            for (const QJsonValue& v : entries) {
                const QJsonObject e   = v.toObject();
                const QString ref     = e.value("ref").toString();
                const QString subject = e.value("subject").toString();
                const QString when    = e.value("when").toString();
                auto* item = new QListWidgetItem(
                    QString("%1  %2  (%3)").arg(ref, subject.left(50), when));
                item->setData(Qt::UserRole, ref);
                m_devStashList->addItem(item);
            }
            if (m_devStashList->count() == 0) {
                auto* item = new QListWidgetItem("(nessuno stash)");
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                m_devStashList->addItem(item);
            }
            if (m_devGitStashPopBtn)
                m_devGitStashPopBtn->setEnabled(m_devStashList->count() > 0
                    && (m_devStashList->item(0)->flags() & Qt::ItemIsEnabled));
        }
    });
}

void AppControllerPage::onDevAgentGitStashPopClicked()
{
    if (!m_devStashList) return;
    auto* sel = m_devStashList->currentItem();
    if (!sel || !(sel->flags() & Qt::ItemIsEnabled)) return;

    const QString ref = sel->data(Qt::UserRole).toString();
    if (ref.isEmpty()) return;

    const QString script = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(script)) return;
    auto* proc = devGitProc(this, script);
    if (!proc) return;

    if (m_devLog) m_devLog->append(
        QString("\xf0\x9f\x93\xa4  <b>git stash pop</b> %1...").arg(ref.toHtmlEscaped()));
    if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xf0\x9f\x93\xa4  Applico stash..."));

    proc->write(QJsonDocument(QJsonObject{
        {"cmd",          "git_stash_pop"},
        {"project_root", P::root()},
        {"ref",          ref}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc](int, QProcess::ExitStatus) {
        const QString raw = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "git_stash_done") continue;
            const bool ok     = obj.value("success").toBool();
            const QString msg = obj.value("msg").toString();
            const QString icon = ok ? "\xf0\x9f\x9f\xa2" : "\xf0\x9f\x94\xb4";
            if (m_devLog) m_devLog->append(
                icon + "  <b>Stash pop</b>: " + msg.toHtmlEscaped());
            if (m_devStatusLbl) m_devStatusLbl->setText(
                ok ? "\xf0\x9f\x9f\xa2  Stash applicato"
                   : "\xf0\x9f\x94\xb4  Stash pop fallito");
            if (ok)
                QTimer::singleShot(100, this,
                    &AppControllerPage::onDevAgentGitStashListClicked);
        }
    });
}

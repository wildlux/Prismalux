/* ======================================================================
   main_programming_wiby.cpp — IP Camera WIBY slot di ProgrammazionePage
   Estratto da main_programming_slots.cpp (sezione WIBY Camera)
   ====================================================================== */
#include "main_programming.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QImage>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTabWidget>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDesktopServices>
#include <QUrl>

namespace P = PrismaluxPaths;

static const QString kWibyScript =
    P::root() + "/Tools/scripts/wiby_controller.py";

void ProgrammazionePage::wibyUpdateStatus(bool connected)
{
    m_wibyReady = connected;
    if (m_wibyStatusLbl) {
        if (connected)
            m_wibyStatusLbl->setText(
                "<span style='color:#4ade80;'>"
                "\xe2\x97\x8f  Controller connesso</span>");
        else
            m_wibyStatusLbl->setText(
                "<span style='color:#888;'>"
                "\xe2\x97\x8f  Non connesso</span>");
        m_wibyStatusLbl->setTextFormat(Qt::RichText);
    }
}

void ProgrammazionePage::wibySend(const QJsonObject& cmd)
{
    if (!m_wibyProc || !m_wibyReady) return;
    QByteArray line = QJsonDocument(cmd).toJson(QJsonDocument::Compact);
    line += '\n';
    m_wibyProc->write(line);
}

void ProgrammazionePage::onWibyDiscoverClicked()
{
    if (m_wibyStatusLbl)
        m_wibyStatusLbl->setText("Ricerca WIBY in LAN (8s)...");

    // Avvia il controller se non attivo, poi invia discover
    if (!m_wibyProc || m_wibyProc->state() == QProcess::NotRunning) {
        onWibyConnectClicked();
        // Aspetta ready asincrono: onWibyCmdOutput chiamerà discover quando ready
        m_wibyPendingDiscover = true;
        return;
    }
    if (!m_wibyReady) {
        m_wibyPendingDiscover = true;
        return;
    }
    m_wibyPendingDiscover = false;
    QJsonObject cmd;
    cmd["cmd"]     = "discover";
    cmd["timeout"] = 8.0;
    wibySend(cmd);
}

void ProgrammazionePage::onWibyConnectClicked()
{
    if (m_wibyProc && m_wibyProc->state() != QProcess::NotRunning)
        return;

    if (!m_wibyProc) {
        m_wibyProc = new QProcess(this);
        connect(m_wibyProc, &QProcess::readyReadStandardOutput,
                this, &ProgrammazionePage::onWibyCmdOutput);
        connect(m_wibyProc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &ProgrammazionePage::onWibyProcFinished);
    }

    m_wibyReady = false;
    m_wibyProc->start(P::findPython(), {kWibyScript});
    if (m_wibyStatusLbl)
        m_wibyStatusLbl->setText("Avvio controller...");
}

void ProgrammazionePage::onWibyDisconnectClicked()
{
    if (!m_wibyProc) return;
    m_wibyProc->terminate();
    m_wibyProc->waitForFinished(1500);
    wibyUpdateStatus(false);
}

void ProgrammazionePage::onWibyCmdOutput()
{
    if (!m_wibyProc) return;
    while (m_wibyProc->canReadLine()) {
        QByteArray raw = m_wibyProc->readLine().trimmed();
        if (raw.isEmpty()) continue;
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (doc.isNull()) continue;
        QJsonObject obj = doc.object();
        if (obj.value("ready").toBool()) {
            wibyUpdateStatus(true);
            if (m_wibyPendingDiscover) {
                m_wibyPendingDiscover = false;
                QJsonObject dc;
                dc["cmd"]     = "discover";
                dc["timeout"] = 8.0;
                wibySend(dc);
                if (m_wibyStatusLbl)
                    m_wibyStatusLbl->setText("Ricerca WIBY in LAN (8s)...");
            }
        } else if (obj.contains("ok")) {
            bool ok = obj.value("ok").toBool();
            QString err = obj.value("error").toString();
            if (ok) {
                /* Risposta discover: mostra device trovati */
                if (obj.value("data").isArray()) {
                    QJsonArray arr = obj.value("data").toArray();
                    if (arr.isEmpty()) {
                        if (m_wibyStatusLbl)
                            m_wibyStatusLbl->setText(
                                "\xe2\x9a\xa0  Nessun device Tuya trovato in LAN");
                    } else {
                        QStringList found;
                        for (const QJsonValue& v : arr) {
                            QJsonObject d = v.toObject();
                            found << QString("%1 (v%2) — %3")
                                     .arg(d.value("gwId").toString())
                                     .arg(d.value("version").toString())
                                     .arg(d.value("ip").toString());
                        }
                        if (m_wibyStatusLbl)
                            m_wibyStatusLbl->setText(
                                "\xe2\x9c\x85  Trovati: " + found.join(", "));
                        if (m_usbOutput)
                            m_usbOutput->append(
                                "<span style='color:#4ade80;'>WIBY discovery: "
                                + found.join("; ") + "</span>");
                    }
                    return;
                }
                /* Risposta stream_url: il campo Tuya si chiama "url" */
                QJsonObject data = obj.value("data").toObject();
                QString streamUrl = data.value("url").toString();
                if (streamUrl.isEmpty())
                    streamUrl = data.value("hls_pull_url").toString();
                if (streamUrl.isEmpty())
                    streamUrl = data.value("rtsp_pull_url").toString();
                if (!streamUrl.isEmpty() && m_wibyStreamUrl) {
                    m_wibyStreamUrl->setText(streamUrl);
                    if (m_usbOutput)
                        m_usbOutput->append(
                            "<span style='color:#4ade80;'>URL stream ottenuto "
                            "— avvio viewer...</span>");
                    /* Auto-avvia il viewer con l'URL appena ricevuto */
                    QTimer::singleShot(100, this,
                        &ProgrammazionePage::onWibyStartStream);
                }
            } else if (!err.isEmpty() && m_usbOutput) {
                m_usbOutput->append(
                    "<span style='color:#f87171;'>WIBY: " + err + "</span>");
            }
        }
    }
}

void ProgrammazionePage::onWibyProcFinished(int /*exitCode*/,
                                             QProcess::ExitStatus /*status*/)
{
    wibyUpdateStatus(false);
}

void ProgrammazionePage::onWibyPtzClicked(const QString& direction)
{
    if (!m_wibyReady) {
        onWibyConnectClicked();
        return;
    }
    wibySend(QJsonObject{{"cmd", "ptz_move"}, {"direction", direction}});
}

void ProgrammazionePage::onWibyPtzStop()
{
    if (!m_wibyReady) return;
    wibySend(QJsonObject{{"cmd", "ptz_stop"}});
}

/* Overload bool — chiamato dai QPushButton checkable */
void ProgrammazionePage::onWibyToggleDp(int /*dp*/, bool value, const QString& code)
{
    if (!m_wibyReady) return;
    wibySend(QJsonObject{{"cmd", "set"}, {"code", code}, {"value", value}});
}

/* Overload QString — chiamato dai QComboBox (enum/valore stringa) */
void ProgrammazionePage::onWibyToggleDp(int /*dp*/, const QString& value, const QString& code)
{
    if (!m_wibyReady) return;
    wibySend(QJsonObject{{"cmd", "set"}, {"code", code}, {"value", value}});
}

void ProgrammazionePage::onWibyGetStreamUrl()
{
    auto doRequest = [this]() {
        wibySend(QJsonObject{{"cmd", "stream_url"}, {"type", "hls"}});
        if (m_usbOutput)
            m_usbOutput->append("Richiedo URL stream al cloud Tuya...");
    };

    if (!m_wibyReady) {
        /* Avvia il controller e aspetta che sia pronto, poi invia */
        onWibyConnectClicked();
        /* Aspetta la risposta {"ready":true} — arriverà in onWibyCmdOutput.
           Usiamo un timer a polling per non bloccare il thread UI. */
        auto* timer = new QTimer(this);
        timer->setInterval(200);
        int* tries = new int(0);
        connect(timer, &QTimer::timeout, this, [this, timer, tries, doRequest]() {
            ++(*tries);
            if (m_wibyReady) {
                timer->stop();
                timer->deleteLater();
                delete tries;
                doRequest();
            } else if (*tries > 25) { /* 5 secondi max */
                timer->stop();
                timer->deleteLater();
                delete tries;
                if (m_usbOutput)
                    m_usbOutput->append(
                        "<span style='color:#f87171;'>Timeout avvio controller.</span>");
            }
        });
        timer->start();
    } else {
        doRequest();
    }
}

void ProgrammazionePage::onWibyStartStream()
{
    if (!m_wibyStreamUrl) return;
    const QString url = m_wibyStreamUrl->text().trimmed();
    if (url.isEmpty()) {
        if (m_usbOutput)
            m_usbOutput->append(
                "<span style='color:#f87171;'>Inserisci un URL HLS/RTSP valido.</span>");
        return;
    }

    onWibyStopStream();
    m_wibyFfmpegBuf.clear();

    /* ffmpeg legge lo stream HLS/RTSP e produce frame JPEG su stdout —
       nessun server intermedio, tutto direttamente in-process */
    m_wibyFfmpegProc = new QProcess(this);
    connect(m_wibyFfmpegProc, &QProcess::readyReadStandardOutput,
            this, &ProgrammazionePage::onWibyFfmpegFrame);
    connect(m_wibyFfmpegProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        if (m_camPreviewLbl && m_camPreviewLbl->pixmap().isNull())
            m_camPreviewLbl->setText("Stream terminato.");
        m_wibyFfmpegProc = nullptr;
    });

    m_wibyFfmpegProc->start("ffmpeg", {
        "-loglevel", "quiet",
        "-i",        url,
        "-vf",       "fps=10,scale=640:-1",
        "-f",        "image2pipe",
        "-vcodec",   "mjpeg",
        "-q:v",      "5",
        "pipe:1"
    });

    if (m_camPreviewLbl)
        m_camPreviewLbl->setText("Connessione stream WIBY...");
    if (m_usbOutput)
        m_usbOutput->append("<span style='color:#4ade80;'>Stream WIBY avviato (ffmpeg diretto).</span>");
}

void ProgrammazionePage::onWibyStopStream()
{
    if (!m_wibyFfmpegProc) return;
    m_wibyFfmpegProc->terminate();
    m_wibyFfmpegProc->waitForFinished(1500);
    m_wibyFfmpegProc->deleteLater();
    m_wibyFfmpegProc = nullptr;
    m_wibyFfmpegBuf.clear();
    if (m_camPreviewLbl)
        m_camPreviewLbl->setText("Stream fermato.");
}

void ProgrammazionePage::onWibyFfmpegFrame()
{
    if (!m_wibyFfmpegProc || !m_camPreviewLbl) return;
    m_wibyFfmpegBuf += m_wibyFfmpegProc->readAllStandardOutput();

    /* Estrai tutti i frame JPEG completi dal buffer (FF D8 ... FF D9) */
    while (true) {
        int soi = -1;
        for (int i = 0; i + 1 < m_wibyFfmpegBuf.size(); ++i) {
            if ((unsigned char)m_wibyFfmpegBuf[i]   == 0xFF &&
                (unsigned char)m_wibyFfmpegBuf[i+1] == 0xD8) {
                soi = i; break;
            }
        }
        if (soi < 0) { m_wibyFfmpegBuf.clear(); break; }
        if (soi > 0)  m_wibyFfmpegBuf = m_wibyFfmpegBuf.mid(soi);

        int eoi = -1;
        for (int i = 2; i + 1 < m_wibyFfmpegBuf.size(); ++i) {
            if ((unsigned char)m_wibyFfmpegBuf[i]   == 0xFF &&
                (unsigned char)m_wibyFfmpegBuf[i+1] == 0xD9) {
                eoi = i + 2; break;
            }
        }
        if (eoi < 0) break; /* frame incompleto — aspetta altri dati */

        QImage img;
        if (img.loadFromData(
                reinterpret_cast<const uchar*>(m_wibyFfmpegBuf.constData()),
                eoi, "JPEG")) {
            m_camPreviewLbl->setPixmap(
                QPixmap::fromImage(img).scaled(
                    m_camPreviewLbl->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
        }
        m_wibyFfmpegBuf = m_wibyFfmpegBuf.mid(eoi);
    }
}

void ProgrammazionePage::onWibyMitmStartClicked()
{
    if (m_wibyMitmProc &&
        m_wibyMitmProc->state() != QProcess::NotRunning) return;

    if (!m_wibyMitmProc) {
        m_wibyMitmProc = new QProcess(this);
        connect(m_wibyMitmProc, &QProcess::readyReadStandardOutput,
                this, &ProgrammazionePage::onWibyMitmOutput);
        connect(m_wibyMitmProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int, QProcess::ExitStatus){
            if (m_wibyMitmStatus)
                m_wibyMitmStatus->setText("\xe2\x97\x8f  Fermato");
            m_wibyMitmProc = nullptr;
        });
    }
    const QString script = P::root() + "/Tools/scripts/wiby_mitm.py";
    m_wibyMitmProc->start("pkexec", {"python3", script});

    if (m_wibyMitmStatus)
        m_wibyMitmStatus->setText(
            "<span style='color:#fbbf24;'>\xe2\x97\x8f  Avvio...</span>");
}

void ProgrammazionePage::onWibyMitmStopClicked()
{
    if (!m_wibyMitmProc) return;
    m_wibyMitmProc->write("stop\n");
    m_wibyMitmProc->waitForFinished(P::kProcessStartTimeoutMs);
    if (m_wibyMitmProc &&
        m_wibyMitmProc->state() != QProcess::NotRunning)
        m_wibyMitmProc->terminate();
    if (m_wibyMitmStatus)
        m_wibyMitmStatus->setText("\xe2\x97\x8f  Inattivo");
}

void ProgrammazionePage::onWibyMitmOutput()
{
    if (!m_wibyMitmProc || !m_usbOutput) return;
    while (m_wibyMitmProc->canReadLine()) {
        QByteArray raw = m_wibyMitmProc->readLine().trimmed();
        if (raw.isEmpty()) continue;
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (doc.isNull()) {
            m_usbOutput->append(QString::fromUtf8(raw));
            continue;
        }
        QJsonObject o = doc.object();
        const QString ev = o.value("event").toString();

        if (ev == "ready") {
            const QString ssid = o.value("ssid").toString();
            const QString ip   = o.value("hotspot_ip").toString();
            const QString pwd  = o.value("password").toString();
            if (m_wibyMitmStatus)
                m_wibyMitmStatus->setText(
                    QString("<span style='color:#4ade80;'>"
                            "\xe2\x97\x8f  Attivo &mdash; SSID: <b>%1</b> "
                            "(%2) IP: %3</span>")
                    .arg(ssid, pwd, ip));
            m_usbOutput->append(
                QString("<b style='color:#4ade80;'>Hotspot attivo!</b> "
                        "Connetti la WIBY a '<b>%1</b>' (pwd: %2) "
                        "poi apri Smart Life.")
                .arg(ssid, pwd));
        } else if (ev == "status") {
            m_usbOutput->append(o.value("msg").toString());
        } else if (ev == "dns") {
            const QString d = o.value("domain").toString();
            /* Mostra solo domini Tuya rilevanti */
            if (d.contains("tuya") || d.contains("iot-11"))
                m_usbOutput->append(
                    "<span style='color:#60a5fa;'>DNS: " + d + "</span>");
        } else if (ev == "http") {
            m_usbOutput->append(
                "<span style='color:#a78bfa;'>"
                + o.value("method").toString() + " "
                + o.value("url").toString() + "</span>");
        } else if (ev == "udp") {
            const int size = o.value("size").toInt();
            /* Evidenzia pacchetti UDP grandi (frame video P2P) */
            if (size > 500) {
                m_usbOutput->append(
                    QString("<span style='color:#f59e0b;'>"
                            "UDP P2P %1 → %2 [<b>%3 B</b>]</span>")
                    .arg(o.value("src").toString(),
                         o.value("dst").toString())
                    .arg(size));
            }
        } else if (ev == "error") {
            m_usbOutput->append(
                "<span style='color:#f87171;'>MITM: "
                + o.value("msg").toString() + "</span>");
        }
    }
}

void ProgrammazionePage::onWibyFirmwareGuide()
{
    if (!m_usbOutput) return;
    m_usbOutput->clear();
    m_usbOutput->append(
    "<b style='font-size:13px;color:#60a5fa;'>"
    "Guida: firmware offline per WIBY JS-P161 (OpenIPC + UART)"
    "</b><br>"

    "<b style='color:#fbbf24;'>Obiettivo</b><br>"
    "Sostituire il firmware Tuya con OpenIPC: RTSP locale attivo, "
    "nessun cloud, PTZ via ONVIF, funziona completamente offline.<br><br>"

    "<b style='color:#fbbf24;'>Passo 1 — Apri la camera</b><br>"
    "Rimuovi il supporto rotante dal basso (viti a stella piccole). "
    "Separa delicatamente la scocca. Non forzare: ci sono clip laterali.<br><br>"

    "<b style='color:#fbbf24;'>Passo 2 — Identifica il chip SoC</b><br>"
    "Fotografa il quadratino nero pi\xc3\xb9 grande sulla scheda verde. "
    "Leggi il numero stampato sopra (es. <code>T31L</code>, "
    "<code>Hi3518EV300</code>, <code>SSC335</code>, <code>GK7205V200</code>). "
    "Mandami la foto — ti dico subito se OpenIPC lo supporta.<br><br>"

    "<b style='color:#fbbf24;'>Passo 3 — Collegati via UART</b><br>"
    "Sulla scheda trovi 3-4 pad etichettati <code>TX RX GND</code> "
    "(puntini d'oro vicino al bordo). Hai bisogno di un adattatore "
    "<b>USB-UART CH340</b> o <b>CP2102</b> (2-5\xe2\x82\xac su Amazon).<br>"
    "Collegamento:<br>"
    "<code>&nbsp;&nbsp;adattatore TX &rarr; pad RX camera</code><br>"
    "<code>&nbsp;&nbsp;adattatore RX &rarr; pad TX camera</code><br>"
    "<code>&nbsp;&nbsp;adattatore GND &rarr; pad GND camera</code><br>"
    "<span style='color:#f87171;'>Non collegare il VCC dell'adattatore "
    "— la camera si alimenta dal cavo USB.</span><br><br>"

    "<b style='color:#fbbf24;'>Passo 4 — Accedi al boot loader</b><br>"
    "Collega la camera al PC via USB-UART, poi apri un terminale:<br>"
    "<code>&nbsp;&nbsp;sudo screen /dev/ttyUSB0 115200</code><br>"
    "Accendi la camera. Vedrai il boot log. Premi un tasto subito per "
    "interrompere il boot e ottenere la shell U-Boot o root.<br><br>"

    "<b style='color:#fbbf24;'>Passo 5 — Dump del firmware originale</b><br>"
    "Dalla shell U-Boot esegui il dump del flash SPI (solitamente 8 o 16 MB):<br>"
    "<code>&nbsp;&nbsp;sf probe 0; sf read 0x82000000 0x0 0x800000</code><br>"
    "<code>&nbsp;&nbsp;md.b 0x82000000 0x800000</code><br>"
    "Salva l'output: \xc3\xa8 il tuo backup. "
    "Senza backup non puoi tornare indietro.<br><br>"

    "<b style='color:#fbbf24;'>Passo 6 — Flash OpenIPC</b><br>"
    "Scarica il firmware per il tuo chip da "
    "<code>github.com/OpenIPC/firmware</code>.<br>"
    "Verifica che il chip sia nella lista supportati. Flash via TFTP:<br>"
    "<code>&nbsp;&nbsp;setenv serverip 192.168.1.100; tftp 0x82000000 openipc.bin</code><br>"
    "<code>&nbsp;&nbsp;sf probe 0; sf erase 0x0 0x800000</code><br>"
    "<code>&nbsp;&nbsp;sf write 0x82000000 0x0 0x800000</code><br>"
    "<code>&nbsp;&nbsp;reset</code><br><br>"

    "<b style='color:#fbbf24;'>Passo 7 — Dopo OpenIPC</b><br>"
    "La camera espone <code>rtsp://192.168.1.222:554/stream=0</code> "
    "senza password di default. Inserisci quell'URL nel campo qui sopra "
    "e premi <b>Avvia viewer</b> — funziona offline, senza Tuya.<br>"
    "PTZ via ONVIF: usa il joystick qui sopra (aggiorna il controller "
    "da Tuya a ONVIF).<br><br>"

    "<span style='color:#94a3b8;font-size:11px;'>"
    "Modello: WIBY JS-P161 &mdash; IP: 192.168.1.222 &mdash; "
    "Firmware attuale: v30.1.14 Tuya v3.3"
    "</span>"
    );
}

/* ══════════════════════════════════════════════════════════════
   VPN & Tunnel — slot
   ══════════════════════════════════════════════════════════════ */


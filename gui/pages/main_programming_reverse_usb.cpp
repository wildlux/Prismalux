/* ======================================================================
   main_programming_reverse_usb.cpp — Reverse Engineering, USB/Driver,
   Cam preview e analisi LAN locale di ProgrammazionePage.
   Estratto da main_programming_slots.cpp.
   ====================================================================== */
#include "main_programming.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Sezione 12b — Reverse Engineering Kernel / Driver
   ══════════════════════════════════════════════════════════════ */

void ProgrammazionePage::onReRunCmd(const QString& cmd, const QString& header)
{
    if (!m_reKernelOutput) return;

    if (!header.isEmpty())
        m_reKernelOutput->append(
            QString("\n<span style='color:#4fc3f7;font-weight:bold'>%1</span>")
                .arg(header.toHtmlEscaped()));
    m_reKernelOutput->append(
        QString("<span style='color:#888'>$ %1</span>").arg(cmd.toHtmlEscaped()));

    if (m_reProcess) {
        m_reProcess->kill();
        m_reProcess->waitForFinished(P::kProcKillExtendedMs);
        m_reProcess->deleteLater();
        m_reProcess = nullptr;
    }
    m_reProcess = new QProcess(this);
    m_reProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_reProcess, &QProcess::readyReadStandardOutput,
            this, &ProgrammazionePage::onReCmdOutput);
    connect(m_reProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onReCmdFinished);
    connect(m_reProcess, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[reverse_usb] processo non avviato:" << m_reProcess->program();
    });
    m_reProcess->start("bash", QStringList() << "-c" << cmd);
}

void ProgrammazionePage::onReCmdOutput()
{
    if (!m_reProcess || !m_reKernelOutput) return;
    const QString txt = QString::fromUtf8(m_reProcess->readAllStandardOutput());
    if (!txt.isEmpty()) {
        m_reKernelOutput->moveCursor(QTextCursor::End);
        m_reKernelOutput->insertPlainText(txt);
        m_reKernelOutput->ensureCursorVisible();
    }
}

void ProgrammazionePage::onReCmdFinished(int exitCode, QProcess::ExitStatus)
{
    if (m_reKernelOutput) {
        const QString msg = exitCode == 0
            ? "\n\xe2\x9c\x85  Completato."
            : QString("\n\xe2\x9d\x8c  Exit code: %1").arg(exitCode);
        m_reKernelOutput->moveCursor(QTextCursor::End);
        m_reKernelOutput->insertPlainText(msg + "\n");
        m_reKernelOutput->ensureCursorVisible();
    }
    if (m_reProcess) { m_reProcess->deleteLater(); m_reProcess = nullptr; }
}

static QString reTarget(QLineEdit* edit)
{
    return edit ? edit->text().trimmed() : QString{};
}

static QString shellQ(const QString& s)
{
    QString r = s;
    r.replace("'", "'\\''");
    return "'" + r + "'";
}

void ProgrammazionePage::onReFileClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) { onReRunCmd("file --version"); return; }
    onReRunCmd(QString("file %1").arg(
        shellQ(t)), "file");
}

void ProgrammazionePage::onReReadelfClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) { m_reKernelOutput->append("Specifica un target .ko / ELF."); return; }
    onReRunCmd(QString("readelf -a %1 2>&1 | head -300").arg(
        shellQ(t)), "readelf -a");
}

void ProgrammazionePage::onReObjdumpClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) { m_reKernelOutput->append("Specifica un target .ko / ELF."); return; }
    onReRunCmd(QString("objdump -d %1 2>&1 | head -400").arg(
        shellQ(t)), "objdump -d (prime 400 righe)");
}

void ProgrammazionePage::onReNmClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) { m_reKernelOutput->append("Specifica un target .ko / oggetto ELF."); return; }
    onReRunCmd(QString("nm --defined-only --demangle %1 2>&1 | sort").arg(
        shellQ(t)), "nm --defined-only");
}

void ProgrammazionePage::onReStringsClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) { m_reKernelOutput->append("Specifica un target binario."); return; }
    onReRunCmd(QString("strings -n 8 %1 2>&1 | head -300").arg(
        shellQ(t)), "strings -n 8");
}

void ProgrammazionePage::onReLddClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) { m_reKernelOutput->append("Specifica un eseguibile o libreria .so."); return; }
    onReRunCmd(QString("ldd %1 2>&1").arg(
        shellQ(t)), "ldd");
}

void ProgrammazionePage::onReModinfoClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    const QString arg = t.isEmpty() ? "snd_usb_audio" : t;
    onReRunCmd(QString("modinfo %1 2>&1").arg(
        shellQ(arg)), "modinfo");
}

void ProgrammazionePage::onReLsmodClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty())
        onReRunCmd("lsmod 2>&1 | head -80", "lsmod");
    else
        onReRunCmd(QString("lsmod 2>&1 | grep -i %1").arg(
            shellQ(t)), "lsmod (filtrato)");
}

void ProgrammazionePage::onReKallsymsClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) {
        onReRunCmd("wc -l /proc/kallsyms && head -20 /proc/kallsyms",
                   "kallsyms (prime 20 righe)");
        return;
    }
    onReRunCmd(QString("grep -i %1 /proc/kallsyms 2>&1 | head -100").arg(
        shellQ(t)), "kallsyms (ricerca)");
}

void ProgrammazionePage::onReDmesgDrvClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty())
        onReRunCmd("dmesg --level=err,warn 2>&1 | tail -100", "dmesg (err+warn)");
    else
        onReRunCmd(QString("dmesg 2>&1 | grep -i %1 | tail -100").arg(
            shellQ(t)), "dmesg (filtrato)");
}

void ProgrammazionePage::onReStraceClicked()
{
    if (!m_reKernelOutput) return;
    m_reKernelOutput->append(
        "\n\xf0\x9f\x95\xb5 <b>strace</b> richiede un PID o un comando da avviare.<br>"
        "Usa il terminale:<br>"
        "<code>  strace -c -p &lt;PID&gt;</code>  (statistiche syscall su processo in esecuzione)<br>"
        "<code>  strace -f -e trace=network ls</code>  (filtra famiglia syscall)<br>"
        "<code>  strace -o /tmp/trace.log &lt;comando&gt;</code>  (salva su file)"
    );
    const QString t = reTarget(m_reTargetEdit);
    if (!t.isEmpty() && t.startsWith("/")) {
        const QString cmd = QString("strace -c %1 2>&1").arg(shellQ(t));
        m_reKernelOutput->append(
            QString("\nAvvio: <code>%1</code>").arg(cmd.toHtmlEscaped()));
        onReRunCmd(cmd, "strace -c");
    }
}

void ProgrammazionePage::onReKprobesClicked()
{
    const QString t = reTarget(m_reTargetEdit);
    if (t.isEmpty()) {
        onReRunCmd(
            "ls /sys/kernel/debug/tracing/available_filter_functions 2>/dev/null"
            " | head -1 || echo 'kprobes: monta debugfs con: mount -t debugfs none /sys/kernel/debug'",
            "kprobes");
    } else {
        onReRunCmd(
            QString("grep -i %1 /sys/kernel/debug/tracing/available_filter_functions 2>&1"
                    " | head -80").arg(shellQ(t)),
            "kprobes (ricerca funzione)");
    }
}

void ProgrammazionePage::onReAiAnalyzeClicked()
{
    if (!m_reKernelOutput || !m_ai) return;
    const QString output = m_reKernelOutput->toPlainText().trimmed();
    if (output.isEmpty()) {
        m_reKernelOutput->append("\nNessun output da analizzare — esegui prima uno strumento.");
        return;
    }

    const QString target = reTarget(m_reTargetEdit);
    const QString sys =
        "Sei un esperto di reverse engineering e kernel Linux. "
        "L'utente ha eseguito un'analisi su un binario o modulo kernel. "
        "Spiega in italiano: cosa fa questo file, quali simboli/sezioni sono importanti, "
        "eventuali pattern sospetti, e suggerisci i prossimi passi RE.";

    const QString limitedOutput = output.length() > 6000
        ? output.left(3000) + "\n...[troncato]...\n" + output.right(2000)
        : output;

    const QString msg = QString(
        "Target: %1\n\nOutput analisi:\n```\n%2\n```\n\n"
        "Analizza e spiega cosa vedi.")
        .arg(target.isEmpty() ? "(non specificato)" : target)
        .arg(limitedOutput);

    m_reKernelOutput->append(
        "\n\xf0\x9f\xa4\x96 Analisi AI in corso...\n");

    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::token, holder,
            [this, holder](const QString& t){
        Q_UNUSED(holder)
        m_reKernelOutput->moveCursor(QTextCursor::End);
        m_reKernelOutput->insertPlainText(t);
        m_reKernelOutput->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, holder,
            [this, holder](const QString&){
        holder->deleteLater();
        m_reKernelOutput->append("\n");
    });
    connect(m_ai, &AiClient::error, holder,
            [this, holder](const QString& e){
        holder->deleteLater();
        m_reKernelOutput->append(
            QString("\n\xe2\x9d\x8c AI error: %1").arg(e));
    });
    m_ai->chat(sys, msg);
}

/* ══════════════════════════════════════════════════════════════
   Sezione 13 — USB / Firmware & Videocam LAN
   ══════════════════════════════════════════════════════════════ */

/* ── helper: esegue un comando bash e mostra l'output in m_usbOutput ── */
void ProgrammazionePage::onUsbRunCmd(const QString& cmd)
{
    if (!m_usbOutput) return;
    m_usbOutput->clear();
    m_usbOutput->append(
        QString("<span style='color:#aaa'>$ %1</span>").arg(cmd.toHtmlEscaped()));

    /* Riusa m_driverProcess per i comandi USB (un processo alla volta) */
    if (m_driverProcess) {
        m_driverProcess->kill();
        m_driverProcess->waitForFinished(P::kProcKillExtendedMs);
        m_driverProcess->deleteLater();
        m_driverProcess = nullptr;
    }

    m_driverAiActive = nullptr;   /* dirige output verso m_usbOutput */
    m_driverProcess  = new QProcess(this);
    m_driverProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_driverProcess, &QProcess::readyReadStandardOutput,
            this, &ProgrammazionePage::onUsbCmdOutput);
    connect(m_driverProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onUsbCmdFinished);
    connect(m_driverProcess, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[usb_driver] processo non avviato:" << m_driverProcess->program();
    });
    m_driverProcess->start("bash", QStringList() << "-c" << cmd);
}

void ProgrammazionePage::onUsbCmdOutput()
{
    if (!m_driverProcess || !m_usbOutput) return;
    const QString txt =
        QString::fromUtf8(m_driverProcess->readAllStandardOutput());
    if (!txt.isEmpty()) {
        m_usbOutput->moveCursor(QTextCursor::End);
        m_usbOutput->insertPlainText(txt);
        m_usbOutput->ensureCursorVisible();
    }
}

void ProgrammazionePage::onUsbCmdFinished(int exitCode,
                                           QProcess::ExitStatus /*status*/)
{
    if (m_usbOutput) {
        const QString msg = exitCode == 0
            ? "\n\xe2\x9c\x85  Completato."
            : QString("\n\xe2\x9d\x8c  Exit code: %1").arg(exitCode);
        m_usbOutput->moveCursor(QTextCursor::End);
        m_usbOutput->insertPlainText(msg);
        m_usbOutput->ensureCursorVisible();
    }
    if (m_driverProcess) {
        m_driverProcess->deleteLater();
        m_driverProcess = nullptr;
    }
}

/* ── USB info ── */

void ProgrammazionePage::onUsbListClicked()
{
    onUsbRunCmd("lsusb");
}

void ProgrammazionePage::onUsbV4l2Clicked()
{
    onUsbRunCmd(
        "v4l2-ctl --list-devices 2>/dev/null || "
        "echo 'v4l2-ctl non trovato (installa: sudo apt install v4l-utils)'");
}

void ProgrammazionePage::onUsbDetailsClicked()
{
    if (!m_usbVidPidEdit) return;
    const QString vidpid = m_usbVidPidEdit->text().trimmed();
    if (vidpid.isEmpty()) {
        if (m_usbOutput)
            m_usbOutput->append(
                "\xe2\x84\xb9  Inserisci VID:PID nel campo (es. 0bda:5652) "
                "prima di richiedere dettagli.");
        return;
    }
    onUsbRunCmd(QString("lsusb -v -d %1 2>&1 | head -120").arg(vidpid));
}

void ProgrammazionePage::onUsbUdevClicked()
{
    if (!m_usbVidPidEdit) return;
    const QString vidpid = m_usbVidPidEdit->text().trimmed();
    QString sysPath;
    if (!vidpid.isEmpty()) {
        /* cerca il bus/device dal lsusb e costruisce il syspath */
        sysPath = QString(
            "DEVPATH=$(lsusb -d %1 2>/dev/null | "
            "awk '{print \"/dev/bus/usb/\" $2 \"/\" $4}' | tr -d ':' | head -1); "
            "[ -n \"$DEVPATH\" ] && udevadm info --query=all --name=$DEVPATH || "
            "echo 'Dispositivo non trovato. Verificare VID:PID.'")
            .arg(vidpid);
    } else {
        sysPath = "udevadm info --export-db 2>/dev/null | grep -A5 'ID_BUS=usb' | head -60 || "
                  "echo 'Inserisci VID:PID per info specifiche sul dispositivo.'";
    }
    onUsbRunCmd(sysPath);
}

/* ── DFU — dump / flash ── */

void ProgrammazionePage::onDfuListClicked()
{
    onUsbRunCmd(
        "dfu-util -l 2>&1 || "
        "echo 'dfu-util non trovato. Installa: sudo apt install dfu-util'");
}

void ProgrammazionePage::onDfuDumpClicked()
{
    const QString out =
        QDir::homePath() + "/wiby_firmware_dump.bin";
    if (m_usbOutput)
        m_usbOutput->append(
            QString("\xe2\x84\xb9  Il dump verr\xc3\xa0 salvato in: <b>%1</b>").arg(out));
    onUsbRunCmd(
        QString("dfu-util -U \"%1\" 2>&1 || "
                "echo 'Verificare che il dispositivo sia in modalita DFU "
                "(tieni premuto il tasto reset accendendo il dispositivo).'")
        .arg(out));
}

void ProgrammazionePage::onDfuFlashClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Seleziona firmware (.bin/.dfu)",
        QDir::homePath(),
        "Firmware (*.bin *.dfu *.hex);;Tutti i file (*)");
    if (path.isEmpty()) return;

    const int res = QMessageBox::warning(
        this,
        "\xe2\x9a\xa0  Flash Firmware",
        QString("Stai per flashare il firmware:\n%1\n\n"
                "Questa operazione sovrascriver\xc3\xa0 il firmware del dispositivo USB.\n"
                "Assicurati che sia il file corretto per il tuo dispositivo.\n\n"
                "Continuare?").arg(path),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (res != QMessageBox::Yes) return;

    onUsbRunCmd(
        QString("dfu-util -D \"%1\" 2>&1").arg(path));
}

/* ── Guida AI ── */

void ProgrammazionePage::onUsbAiGuideClicked()
{
    m_driverAiActive = m_usbOutput;
    onDriverAiGuide(
        "Sono un utente Linux con una videocamera USB (Wiby) che non e' piu' "
        "supportata dal produttore e dipende da server cloud ora offline. "
        "Voglio: 1) Analizzare il protocollo USB con lsusb e usbmon per capire "
        "come comunica. 2) Verificare se supporta DFU per modificare il firmware. "
        "3) Avviare uno stream MJPEG locale via V4L2 e ffmpeg/python-opencv "
        "accessibile sulla LAN. Dammi i comandi esatti per Linux, passo per passo.");
}

/* ── Server MJPEG LAN ── */

void ProgrammazionePage::onCamRefreshDevices()
{
    if (!m_camDeviceCombo) return;
    m_camDeviceCombo->clear();
    for (int i = 0; i < 8; ++i) {
        const QString dev = QString("/dev/video%1").arg(i);
        if (!QFileInfo::exists(dev)) continue;
        /* Legge il nome dal sysfs e scarta metadata/output device */
        QFile namef(QString("/sys/class/video4linux/video%1/name").arg(i));
        QString label = dev;
        if (namef.open(QIODevice::ReadOnly)) {
            const QString name = QString::fromUtf8(namef.readAll()).trimmed();
            namef.close();
            if (name.contains("metadata", Qt::CaseInsensitive) ||
                name.contains(" output", Qt::CaseInsensitive))
                continue;
            label = QString("%1  [%2]").arg(dev).arg(name);
        }
        m_camDeviceCombo->addItem(label, i);
    }
    if (m_camDeviceCombo->count() == 0) {
        m_camDeviceCombo->addItem("(nessuna webcam trovata)", -1);
        if (m_usbOutput)
            m_usbOutput->append(
                "\xe2\x84\xb9  Nessun capture device trovato.\n"
                "Collega la videocamera USB e riprova.");
    }
}

void ProgrammazionePage::onCamServerStartClicked()
{
    if (!m_camDeviceCombo || !m_camPortSpin || !m_camServerStatus) return;

    const int devIdx = m_camDeviceCombo->currentData().toInt();
    if (devIdx < 0) {
        QMessageBox::warning(this, tr("Nessuna webcam"),
            "Seleziona un dispositivo V4L2 valido (/dev/videoN).");
        return;
    }
    const int port = m_camPortSpin->value();

    if (m_camStreamProc && m_camStreamProc->state() != QProcess::NotRunning) {
        m_camStreamProc->kill();
        m_camStreamProc->waitForFinished(800);
    }

    /* Script Python MJPEG server — usa opencv già presente nei requirements */
    const QString script =
        "import cv2, socket, threading, time, sys\n"
        "dev  = int(sys.argv[1]) if len(sys.argv) > 1 else 0\n"
        "port = int(sys.argv[2]) if len(sys.argv) > 2 else 8090\n"
        "cap  = cv2.VideoCapture(dev)\n"
        "if not cap.isOpened():\n"
        "    print(f'ERRORE: /dev/video{dev} non apribile', flush=True); sys.exit(1)\n"
        "def handle(conn):\n"
        "    conn.sendall(b'HTTP/1.1 200 OK\\r\\n'\n"
        "                 b'Content-Type: multipart/x-mixed-replace; boundary=frame\\r\\n'\n"
        "                 b'Access-Control-Allow-Origin: *\\r\\n\\r\\n')\n"
        "    try:\n"
        "        while True:\n"
        "            ok, frm = cap.read()\n"
        "            if not ok: break\n"
        "            _, jpg = cv2.imencode('.jpg', frm, [cv2.IMWRITE_JPEG_QUALITY, 75])\n"
        "            data = jpg.tobytes()\n"
        "            conn.sendall(b'--frame\\r\\nContent-Type: image/jpeg\\r\\n'\n"
        "                         + f'Content-Length: {len(data)}\\r\\n\\r\\n'.encode()\n"
        "                         + data + b'\\r\\n')\n"
        "            time.sleep(1/15.0)\n"
        "    except: pass\n"
        "    finally: conn.close()\n"
        "srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)\n"
        "srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)\n"
        "srv.bind(('0.0.0.0', port)); srv.listen(10)\n"
        "print(f'ONLINE:{port}', flush=True)\n"   /* dopo listen — server pronto */
        "while True:\n"
        "    conn, _ = srv.accept()\n"
        "    threading.Thread(target=handle, args=(conn,), daemon=True).start()\n";

    /* Salva lo script temporaneo */
    m_camStreamScript = QDir::tempPath() + "/prismalux_mjpeg_server.py";
    QFile f(m_camStreamScript);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(script.toUtf8());
        f.close();
    } else {
        QMessageBox::critical(this, tr("Errore"),
            "Impossibile scrivere lo script server in " + m_camStreamScript);
        return;
    }

    m_camStreamProc = new QProcess(this);
    m_camStreamProc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_camStreamProc, &QProcess::readyReadStandardOutput,
            this, [this] {
        if (!m_camStreamProc || !m_usbOutput) return;
        const QString line =
            QString::fromUtf8(m_camStreamProc->readAllStandardOutput()).trimmed();
        if (line.startsWith("ONLINE:")) {
            const int p = line.mid(7).toInt();
            /* URL locale per il viewer interno */
            const QString localUrl = QString("http://127.0.0.1:%1/").arg(p);
            /* URL LAN per dispositivi esterni (hotspot, telefono, ecc.) */
            QString lanIp;
            for (const QNetworkInterface& ni : QNetworkInterface::allInterfaces()) {
                if (ni.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
                if (!ni.flags().testFlag(QNetworkInterface::IsUp)) continue;
                for (const QNetworkAddressEntry& ae : ni.addressEntries()) {
                    if (ae.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                        lanIp = ae.ip().toString();
                        break;
                    }
                }
                if (!lanIp.isEmpty()) break;
            }
            const QString lanUrl = lanIp.isEmpty()
                ? localUrl
                : QString("http://%1:%2/").arg(lanIp).arg(p);

            if (m_camServerStatus) {
                m_camServerStatus->setText(
                    QString("\xf0\x9f\x9f\xa2  Server attivo \xe2\x80\x94 "
                            "locale: <a href='%1'>%1</a>  "
                            "| LAN: <a href='%2'>%2</a>")
                    .arg(localUrl).arg(lanUrl));
                m_camServerStatus->setTextFormat(Qt::RichText);
                m_camServerStatus->setOpenExternalLinks(true);
                m_camServerStatus->setStyleSheet("color: #4ade80; font-size: 12px;");
            }
            if (m_usbOutput)
                m_usbOutput->append(
                    QString("\n\xf0\x9f\x9f\xa2  Server MJPEG pronto\n"
                            "  Viewer interno: %1\n"
                            "  Da browser/VLC/telefono: %2")
                    .arg(localUrl).arg(lanUrl));

            /* Auto-connessione del viewer con 400ms di margine */
            if (m_camPreviewUrl) m_camPreviewUrl->setText(localUrl);
            QTimer::singleShot(400, this,
                &ProgrammazionePage::onCamPreviewConnect);
        } else if (!line.isEmpty()) {
            if (m_usbOutput) m_usbOutput->append(line);
        }
    });

    connect(m_camStreamProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        if (m_camServerStatus)
            m_camServerStatus->setText(
                QString("\xe2\x97\x8f  Server terminato (exit %1)").arg(code));
        m_camStreamProc = nullptr;
    });

    m_camStreamProc->start(
        "python3",
        QStringList() << m_camStreamScript
                      << QString::number(devIdx)
                      << QString::number(port));
    connect(m_camStreamProc, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[cam_stream] processo non avviato:" << m_camStreamProc->program();
    });

    if (!m_camStreamProc->waitForStarted(2000)) {
        QMessageBox::critical(this, tr("Errore avvio"),
            "Impossibile avviare python3. Verifica che sia installato e "
            "che il pacchetto opencv-python sia disponibile:\n"
            "pip install opencv-python");
        m_camStreamProc->deleteLater();
        m_camStreamProc = nullptr;
        return;
    }

    m_camServerStatus->setText(
        tr("\xf0\x9f\x9f\xa1  Server in avvio..."));
    m_camServerStatus->setStyleSheet("color: #facc15; font-size: 12px;");
    if (m_usbOutput)
        m_usbOutput->append(
            QString("\xe2\x84\xb9  Avvio server MJPEG su /dev/video%1 porta %2...")
            .arg(devIdx).arg(port));
}

void ProgrammazionePage::onCamServerStopClicked()
{
    if (m_camStreamProc) {
        m_camStreamProc->kill();
        m_camStreamProc->waitForFinished(1000);
        m_camStreamProc = nullptr;
    }
    if (!m_camStreamScript.isEmpty() && QFileInfo::exists(m_camStreamScript))
        QFile::remove(m_camStreamScript);
    if (m_camServerStatus) {
        m_camServerStatus->setText(tr("\xe2\x97\x8f  Server fermato."));
        m_camServerStatus->setStyleSheet("color: #888; font-size: 12px;");
    }
    if (m_usbOutput)
        m_usbOutput->append("\xe2\x96\xa0  Server MJPEG fermato.");
}

/* ── Viewer MJPEG inline ── */

void ProgrammazionePage::onCamPreviewConnect()
{
    if (!m_camPreviewUrl || !m_camPreviewLbl) return;
    const QString url = m_camPreviewUrl->text().trimmed();
    if (url.isEmpty()) return;

    /* Disconnetti eventuale stream precedente */
    if (m_camReply) {
        m_camReply->abort();
        m_camReply->deleteLater();
        m_camReply = nullptr;
    }
    m_camBuf.clear();

    if (!m_camNam)
        m_camNam = new QNetworkAccessManager(this);

    QNetworkRequest req;
    req.setUrl(QUrl(url));
    req.setRawHeader("Connection", "keep-alive");
    m_camReply = m_camNam->get(req);

    connect(m_camReply, &QNetworkReply::readyRead,
            this, &ProgrammazionePage::onCamPreviewData);
    connect(m_camReply, &QNetworkReply::finished,
            this, &ProgrammazionePage::onCamPreviewFinished);

    m_camPreviewLbl->setText(
        "\xf0\x9f\x9f\xa1  Connessione a " + url + "...");
    m_camPreviewLbl->setStyleSheet(
        "background:#111; color:#facc15; border-radius:4px;");
}

void ProgrammazionePage::onCamPreviewData()
{
    if (!m_camReply || !m_camPreviewLbl) return;
    m_camBuf += m_camReply->readAll();

    /* Cerca frame JPEG completi (SOI=FFD8 … EOI=FFD9) */
    while (true) {
        const int soiPos = m_camBuf.indexOf("\xff\xd8");
        if (soiPos < 0) {
            if (m_camBuf.size() > 8192)
                m_camBuf.remove(0, m_camBuf.size() - 8192);
            break;
        }
        const int eoiPos = m_camBuf.indexOf("\xff\xd9", soiPos + 2);
        if (eoiPos < 0) {
            if (soiPos > 0) m_camBuf.remove(0, soiPos);
            break;
        }
        const QByteArray jpeg = m_camBuf.mid(soiPos, eoiPos - soiPos + 2);
        m_camBuf.remove(0, eoiPos + 2);

        QPixmap pix;
        if (pix.loadFromData(jpeg, "JPEG")) {
            const QSize sz = m_camPreviewLbl->size();
            m_camPreviewLbl->setPixmap(
                pix.scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void ProgrammazionePage::onCamPreviewFinished()
{
    if (m_camPreviewLbl && !m_camPreviewLbl->pixmap().isNull()) {
        /* lascia l'ultimo frame visibile */
    } else if (m_camPreviewLbl) {
        m_camPreviewLbl->setText(
            tr("Stream terminato o non raggiungibile."));
        m_camPreviewLbl->setStyleSheet(
            "background:#111; color:#888; border-radius:4px;");
    }
    if (m_camReply) {
        m_camReply->deleteLater();
        m_camReply = nullptr;
    }
    m_camBuf.clear();
}

/* ══════════════════════════════════════════════════════════════
   WIBY Camera PTZ — slot (Sezione 14)
   ══════════════════════════════════════════════════════════════ */


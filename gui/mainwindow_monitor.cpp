/* ══════════════════════════════════════════════════════════════
   mainwindow_monitor.cpp — MainWindow: hardware monitor + eventi finestra
   ============================================================================
   Callback aggiornamento hardware/benchmark VRAM, changeEvent/closeEvent.
   Split da mainwindow.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "mainwindow.h"
#include "prismalux_paths.h"
#include "pages/main_research.h"
#include "pages/settings_main.h"
#include "pages/main_ai.h"

#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QFileInfo>
#include <QCloseEvent>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Slot hardware
   ══════════════════════════════════════════════════════════════ */
/* ── 🧠 contesto LLM residuo (header, accanto alla GPU) ──────────
   Riceve la stima da AgentiPage::contextUsage() dopo ogni turno:
   mostra i token ancora liberi per domande e documenti RAG futuri. */
void MainWindow::onContextUsage(int usedTok, int maxTok)
{
    if (!m_ctxLbl || maxTok <= 0) return;
    const int freeTok = qMax(0, maxTok - usedTok);
    const double freePct = 100.0 * freeTok / maxTok;
    auto fmtK = [](int t) {
        return t >= 1000 ? QString::number(t / 1000.0, 'f', 1) + "k"
                         : QString::number(t);
    };
    const char* col = freePct > 50 ? "#4ade80"
                    : freePct > 20 ? "#facc15" : "#f87171";
    m_ctxLbl->setText(QString::fromUtf8("\xf0\x9f\xa7\xa0 %1").arg(fmtK(freeTok)));
    m_ctxLbl->setStyleSheet(
        QString("QLabel#ctxLabel{color:%1;font-size:11px;padding:0 4px;}").arg(col));
    m_ctxLbl->setToolTip(QString::fromUtf8(
        "\xf0\x9f\xa7\xa0  Contesto LLM ancora libero (stima ~4 caratteri/token)\n"
        "Usati: %1 tok su %2 (num_ctx, Impostazioni \xe2\x86\x92 Parametri AI)\n"
        "Conteggia storia chat compressa + documenti RAG della chat.\n"
        "Quello che resta \xc3\xa8 lo spazio per domande e RAG futuri.")
        .arg(fmtK(usedTok), fmtK(maxTok)));
}

void MainWindow::onHWUpdated(SysSnapshot snap) {
    if (!m_gCpu || !m_gRam || !m_gGpu) return;
    m_gCpu->update(snap.cpu_pct, snap.cpu_name);
    double rp = snap.ram_total > 0 ? snap.ram_used/snap.ram_total*100.0 : 0;
    m_gRam->update(rp, QString("%1/%2 GB")
                   .arg(snap.ram_used,0,'f',1)
                   .arg(snap.ram_total,0,'f',1));

    /* Aggiorna percentuale RAM libera in AiClient (usata per throttle pre-chat) */
    double freePct = snap.ram_total > 0 ? (snap.ram_total - snap.ram_used) / snap.ram_total * 100.0 : 100.0;
    m_ai->setRamFreePct(freePct);

    /* Colora 🚨 via property QSS in base alla RAM: normale→warn→high */
    if (m_emergencyBtn) {
        const char* urgency = rp > 75.0 ? "high" : rp > 40.0 ? "warn" : "";
        if (m_emergencyBtn->property("urgency").toString() != urgency) {
            m_emergencyBtn->setProperty("urgency", urgency);
            P::repolish(m_emergencyBtn);
        }
    }

    /* Auto-abort: se RAM >97% usata e AI occupato, interrompe subito.
     * Evita che il sistema si blocchi completamente durante l'inference.
     * Soglia 97%: modelli grandi usano ~80-90% RAM normalmente — abort solo in OOM reale. */
    if (rp > 97.0 && m_ai->busy()) {
        m_ai->abort();
        statusBar()->showMessage(
            "\xe2\x9a\xa0  RAM critica — inference AI interrotta automaticamente per proteggere il sistema.");
    }
    if (snap.gpu_ready) {
        QString gpuTip = snap.gpu_name;
        if (snap.vram_total > 0)
            gpuTip += QString(" | VRAM %1/%2 GB")
                      .arg(snap.vram_used,0,'f',1)
                      .arg(snap.vram_total,0,'f',1);
        m_gGpu->update(snap.gpu_pct, gpuTip);
    } else {
        m_gGpu->update(0, "Rilevamento...");
    }

    /* Indicatore temperatura header */
    if (m_tempLbl) {
        QStringList parts;
        if (snap.cpu_temp_c >= 0)
            parts << QString("CPU %1\xc2\xb0" "C").arg((int)snap.cpu_temp_c);
        if (snap.gpu_temp_c >= 0)
            parts << QString("GPU %1\xc2\xb0" "C").arg((int)snap.gpu_temp_c);
        if (!parts.isEmpty()) {
            m_tempLbl->setText("\xf0\x9f\x8c\xa1  " + parts.join(" | "));  /* 🌡 */
            const double maxTemp = qMax(snap.cpu_temp_c, snap.gpu_temp_c);
            if (maxTemp >= 90) {
                m_tempLbl->setStyleSheet(
                    "QLabel#tempLabel{color:#f87171;font-size:11px;padding:0 4px;font-weight:bold;}");
                if (!m_thermalCriticalWarned) {
                    m_thermalCriticalWarned = true;
                    QTimer::singleShot(0, this, [this, maxTemp]() {
                        auto* mb = new QMessageBox(this);
                        mb->setWindowTitle(tr("Temperatura critica"));
                        mb->setIcon(QMessageBox::Warning);
                        mb->setText(
                            QString("\xf0\x9f\x8c\xa1  <b>Temperatura %1\xc2\xb0""C</b> \xe2\x80\x94 "
                                    "rischio throttling o danno hardware!<br><br>"
                                    "Vuoi fermare il flusso AI per ridurre il carico?")
                                .arg((int)maxTemp));
                        mb->setTextFormat(Qt::RichText);
                        auto* btnStop = mb->addButton(
                            "\xf0\x9f\x9b\x91  Ferma AI", QMessageBox::AcceptRole);
                        mb->addButton(tr("Continua"), QMessageBox::RejectRole);
                        mb->setAttribute(Qt::WA_DeleteOnClose);
                        connect(mb, &QMessageBox::finished, this,
                                [this, mb, btnStop](int) {
                            if (mb->clickedButton() == btnStop)
                                m_ai->abort();
                        });
                        mb->show();
                    });
                }
            } else if (maxTemp >= 75)
                m_tempLbl->setStyleSheet(
                    "QLabel#tempLabel{color:#f59e0b;font-size:11px;padding:0 4px;}");
            else {
                m_tempLbl->setStyleSheet(
                    "QLabel#tempLabel{color:#94a3b8;font-size:11px;padding:0 4px;}");
                m_thermalCriticalWarned = false;  /* resetta quando la temp scende */
            }
            m_tempLbl->setVisible(true);
        }
    }

    /* Invia temperatura alla RicercaPage per il RAG thermal throttle */
    if (m_ricercaPage)
        m_ricercaPage->onThermalUpdate(snap.cpu_temp_c, snap.gpu_temp_c);
}

/* ══════════════════════════════════════════════════════════════
   maybeAutoVramBench — benchmark VRAM automatico al primo avvio
   ══════════════════════════════════════════════════════════════ */
void MainWindow::maybeAutoVramBench() {
    /* Già eseguito in precedenza → skip */
    if (QFileInfo::exists(P::vramProfilePath())) return;

    /* Binario non compilato → skip silenzioso */
    const QString bench = P::vramBenchBin();
    if (!QFileInfo::exists(bench)) return;

    /* Solo con Ollama attivo (il benchmark interroga /api/tags) */
    if (m_ai->backend() != AiClient::Ollama) return;

    statusBar()->showMessage(
        "\xf0\x9f\x94\xac  Primo avvio: benchmark VRAM in corso (background)...");

    auto* proc = new QProcess(this);
    proc->setWorkingDirectory(P::root() + "/C_software");
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onVramBenchFinished);

    proc->start(bench, {});
    if (!proc->waitForStarted(3000)) {
        if (proc->state() != QProcess::NotRunning) proc->kill();
        proc->deleteLater();
        statusBar()->showMessage(
            "\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
    }
}

/* ── Finestra torna in primo piano — invalida cache modelli ── */
void MainWindow::changeEvent(QEvent* ev)
{
    QMainWindow::changeEvent(ev);
    if (ev->type() == QEvent::ActivationChange && isActiveWindow())
        m_ai->invalidateModelCache();
}

/* ── Chiusura finestra — pulizia RAM residua ── */
void MainWindow::closeEvent(QCloseEvent* ev) {

    /* Se l'AI sta elaborando, chiedi se fermare e scaricare il modello */
    if (m_ai->busy()) {
        QMessageBox dlg(this);
        dlg.setWindowTitle("Prismalux — Chiusura");
        dlg.setIcon(QMessageBox::Question);
        dlg.setText("<b>Un agente AI \xc3\xa8 ancora in elaborazione.</b><br>"
                    "Vuoi fermare la generazione e scaricare il modello dalla RAM?");
        dlg.setInformativeText(
            "Modello attivo: <b>" + m_ai->model() + "</b><br>"
            "Se non lo scarichi rimarr\xc3\xa0 in memoria anche dopo la chiusura.");
        auto* btnUnload = dlg.addButton("Ferma e scarica dalla RAM", QMessageBox::AcceptRole);
        dlg.addButton("Chiudi comunque",                               QMessageBox::DestructiveRole);
        auto* btnCancel = dlg.addButton("Annulla",                     QMessageBox::RejectRole);
        dlg.setDefaultButton(btnCancel);
        dlg.exec();

        if (dlg.clickedButton() == btnCancel || dlg.clickedButton() == nullptr) {
            ev->ignore();   /* Annulla — non chiudere */
            return;
        }

        /* Ferma la generazione corrente */
        m_ai->abort();

        if (dlg.clickedButton() == btnUnload) {
            /* Ollama: DELETE /api/delete scarica il modello dalla VRAM/RAM */
            QString host  = m_ai->host();
            int     port  = m_ai->port();
            QString model = m_ai->model();
            if (!model.isEmpty()) {
                QNetworkAccessManager* nam = new QNetworkAccessManager(this);
                QNetworkRequest req(QUrl(
                    QString("http://%1:%2/api/delete").arg(host).arg(port)));
                req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                QJsonObject body; body["name"] = model;
                QNetworkReply* reply = nam->sendCustomRequest(
                    req, "DELETE", QJsonDocument(body).toJson());
                /* Attendi al massimo 2 secondi poi chiudi comunque */
                QTimer::singleShot(2000, reply, &QNetworkReply::abort);
            }
        }
    }

    /* Termina i processi della pagina AI (TTS/STT/exec) prima della distruzione
     * del widget: evita il waitForFinished(-1) implicito nel ~QProcess() che
     * blocca il thread principale quando un processo non risponde a SIGTERM. */
    if (m_mainTabs) {
        if (auto* ap = qobject_cast<AgentiPage*>(m_mainTabs->widget(0)))
            ap->prepareClose();
    }

    /* Ferma llama-server avviato dalla GUI (se in esecuzione) */
    if (m_serverProc && m_serverProc->state() != QProcess::NotRunning) {
        m_serverProc->terminate();
        m_serverProc->waitForFinished(3000);
    }

    /* Libera cache RAM residua (best-effort, senza dialogo) */
#ifndef Q_OS_WIN
    QProcess::startDetached("bash", {"-c",
        "ollama ps --no-trunc 2>/dev/null | awk 'NR>1{print $1}' | "
        "xargs -r -I{} ollama stop {} 2>/dev/null; "
        "sync 2>/dev/null"});
#else
    QProcess::startDetached("cmd.exe", {"/c",
        "for /f \"skip=1 tokens=1\" %m in ('ollama ps --no-trunc 2^>nul') do ollama stop %m"});
#endif

    /* Salva geometry e stato finestra per il prossimo avvio */
    QSettings s("Prismalux", "GUI");
    s.setValue("mainwindow/geometry", saveGeometry());
    s.setValue("mainwindow/state",    saveState());

    ev->accept();
}

void MainWindow::onHWReady(HWInfo hw) {
    const HWDevice& pd = hw.dev[hw.primary];

    /* Aggiorna label GPU nell'header */
    if (pd.type != DEV_CPU) {
        QString gpuName = QString::fromLocal8Bit(pd.name);
        m_lblModel->setText(gpuName);  /* temporaneo, sovrascritto dal modello AI */
    } else {
        /* Xeon → nessun allarme */
        QString cpuName = QString::fromLocal8Bit(hw.dev[0].name);
        if (cpuName.contains("Xeon", Qt::CaseInsensitive))
            statusBar()->showMessage("✓ Xeon rilevato — elaborazione CPU ad alta performance");
    }

    /* Notifica la pagina impostazioni se già creata */
    if (m_impPage) m_impPage->onHWReady(hw);
}

/* ══════════════════════════════════════════════════════════════
   Navigazione
   ══════════════════════════════════════════════════════════════ */
void MainWindow::navigateTo(int idx) {
    if (idx < 0 || idx > 6) return;
    /* Interrompe qualsiasi richiesta AI in corso prima di cambiare pagina:
       evita segnali fantasma (token/finished) che arrivano alla pagina precedente. */
    if (m_ai && m_ai->busy()) m_ai->abort();
    if (m_mainTabs) m_mainTabs->setCurrentIndex(idx);
}

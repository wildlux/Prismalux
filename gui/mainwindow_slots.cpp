/* mainwindow_slots.cpp — Implementazioni slot di MainWindow senza lambda.
 * Separato da mainwindow.cpp per mantenere quel file leggibile.
 * Regola: nessuna lambda nei connect() — solo slot nominati.
 */

#include "mainwindow.h"
#include "prismalux_paths.h"
#include "ai_client.h"
#include "theme_manager.h"
#include "widgets/whisper_autosetup.h"
#include "pages/main_ai.h"
#include "pages/settings_main.h"
#include "pages/main_tools.h"
#include "pages/main_graph.h"
#include "pages/main_programming.h"

#include <QApplication>
#include <QSettings>
#include <QFile>
#include <QProcess>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QTextEdit>
#include <QTextCursor>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QTextDocument>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QStatusBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QComboBox>
#include <QTabWidget>
#include <QListWidgetItem>
#include <QKeyEvent>

namespace P = PrismaluxPaths;

// ─── zRAM ─────────────────────────────────────────────────────────────────

void MainWindow::onZramSetupTimer()
{
#ifndef Q_OS_WIN
    QSettings zs("Prismalux", "GUI");
    if (!zs.value(P::SK::kAutoZramDoppia, true).toBool()) return;
    QFile swapsFile("/proc/swaps");
    if (swapsFile.open(QIODevice::ReadOnly)) {
        if (swapsFile.readAll().contains("zram"))
            return;
    }
    static const QString kZramScript =
        "echo 1 | tee /proc/sys/vm/compact_memory > /dev/null; "
        "sleep 1; "
        "for dev in /dev/zram*; do swapoff \"$dev\" 2>/dev/null; done; "
        "rmmod zram 2>/dev/null || true; "
        "sleep 0.3; "
        "modprobe zram num_devices=2; "
        "sleep 0.3; "
        "TOTAL=$(grep MemTotal /proc/meminfo | awk '{print $2}'); "
        "(echo zstd | tee /sys/block/zram0/comp_algorithm) || "
        " (echo lzo-rle | tee /sys/block/zram0/comp_algorithm); "
        "echo $(( TOTAL * 512 )) | tee /sys/block/zram0/disksize; "
        "mkswap /dev/zram0; swapon -p 100 /dev/zram0; "
        "(echo zstd | tee /sys/block/zram1/comp_algorithm) || "
        " (echo lzo-rle | tee /sys/block/zram1/comp_algorithm); "
        "echo $(( TOTAL * 256 )) | tee /sys/block/zram1/disksize; "
        "mkswap /dev/zram1; swapon -p 50 /dev/zram1";
    QProcess::startDetached("pkexec", {"bash", "-c", kZramScript});
    statusBar()->showMessage(
        "\xf0\x9f\x92\xbe  zRAM Doppia (zstd, 75% RAM) \xe2\x80\x94 "
        "richiesta autorizzazione amministratore...", 8000);
#endif
}

// ─── Whisper ──────────────────────────────────────────────────────────────

void MainWindow::onStartWhisperTimer()
{
    auto* wsp = new WhisperAutoSetup(
        [this](const QString& msg){ statusBar()->showMessage(msg); }, this);
    connect(wsp, &WhisperAutoSetup::ready,  this, &MainWindow::onWhisperReady);
    connect(wsp, &WhisperAutoSetup::failed, this, &MainWindow::onWhisperFailed);
    wsp->run();
}

void MainWindow::onWhisperReady()
{
    statusBar()->showMessage("\xe2\x9c\x85  Riconoscimento vocale pronto.");
    QTimer::singleShot(4000, this, &MainWindow::onRestoreDefaultStatus);
}

void MainWindow::onWhisperReadyStatus()
{
    onRestoreDefaultStatus();
}

void MainWindow::onWhisperFailed(const QString& err)
{
    statusBar()->showMessage("\xe2\x9a\xa0  Whisper setup: " + err);
}

// ─── Idle unload ──────────────────────────────────────────────────────────

void MainWindow::onIdleUnloadTimer()
{
    if (!m_ai->busy() && m_ai->isModelLoaded() && m_ai->backend() == AiClient::Ollama) {
        const double freePct = m_ai->ramFreePct();
        if (freePct < 60.0) {
            m_ai->unloadModel();
            statusBar()->showMessage(
                "\xf0\x9f\x97\x91  Auto-scarico: modello rimosso dalla RAM "
                "(RAM > 40% \xe2\x80\x94 scarico automatico dopo inattivit\xc3\xa0).");
            QTimer::singleShot(5000, this, &MainWindow::onRestoreDefaultStatus);
        }
    }
}

// ─── Modelli AI ───────────────────────────────────────────────────────────

void MainWindow::onInitialModelsReady(const QStringList& list)
{
    if (list.isEmpty()) {
        m_lblModel->setText("(Ollama non trovato)");
        statusBar()->showMessage(
            "\xe2\x9a\xa0\xef\xb8\x8f  Ollama non risponde — avvialo con: ollama serve");
        if (m_btnBackend) m_btnBackend->setStyleSheet("color:#ef4444;");
        maybeAutoVramBench();
        return;
    }
    const QString current = m_ai->model();
    const bool preferredAvail = !current.isEmpty() && list.contains(current);
    const QString model = preferredAvail ? current : list.first();
    if (!preferredAvail)
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), model);
    m_lblModel->setText(model);
    statusBar()->showMessage(
        QString("\xf0\x9f\x8d\xba  Backend Ollama | Modello: %1 | Modelli disponibili: %2")
        .arg(model).arg(list.size()));
    if (m_btnBackend) m_btnBackend->setStyleSheet("color:#10a37f;");
    maybeAutoVramBench();
}

void MainWindow::onModelChanged(const QString& model)
{
    m_lblModel->setText(model);
    statusBar()->showMessage(
        QString("\xe2\x9c\x85  Modello attivo: %1").arg(model), 3000);
}

void MainWindow::onApplyBackendModelsReady(const QStringList& list)
{
    if (!list.isEmpty()) {
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), list.first());
        m_lblModel->setText(list.first());
        appendLog(
            QString("\xe2\x9c\x85 Backend <b>%1</b> pronto \xe2\x80\x94 "
                    "modello: <b>%2</b> (%3 disponibili)")
            .arg(m_pendingBkName, list.first(), QString::number(list.size())));
        statusBar()->showMessage(
            QString("\xe2\x9c\x85  %1 | Modello: %2 | %3 disponibili")
            .arg(m_pendingBkName, list.first(), QString::number(list.size())));
        if (m_btnBackend) m_btnBackend->setStyleSheet("color:#10a37f;");
    } else {
        m_lblModel->setText("(server non raggiungibile)");
        appendLog(
            QString("\xe2\x9a\xa0\xef\xb8\x8f <b>%1</b> non risponde \xe2\x80\x94 nessun modelli disponibile")
            .arg(m_pendingBkName));
        statusBar()->showMessage(
            QString("\xe2\x9a\xa0\xef\xb8\x8f  %1 non risponde \xe2\x80\x94 avvialo prima di usare l'AI")
            .arg(m_pendingBkName));
        if (m_btnBackend) m_btnBackend->setStyleSheet("color:#ef4444;");
    }
}

// ─── Header buttons ───────────────────────────────────────────────────────

void MainWindow::onHamburgerClicked()
{
    if (m_sidebarWidget)
        m_sidebarWidget->setVisible(!m_sidebarWidget->isVisible());
}

void MainWindow::onLogBtnClicked()
{
    ensureLogDialog();
    m_logUnread = 0;
    m_logBadge->setVisible(false);
    m_logDlg->show();
    m_logDlg->raise();
    m_logDlg->activateWindow();
}

void MainWindow::onUnloadModelClicked()
{
    m_ai->unloadModel();
    statusBar()->showMessage(
        "\xf0\x9f\x97\x91  Richiesta scarico modello inviata a Ollama \xe2\x80\x94 "
        "il modello verr\xc3\xa0 rimosso dalla RAM.");
    QTimer::singleShot(4000, this, &MainWindow::onRestoreDefaultStatus);
}

void MainWindow::onBackendBtnClicked()
{
    auto* menu = new QMenu(m_btnBackend);
    menu->setObjectName("backendMenu");

    const bool serverRunning = m_serverProc &&
                               m_serverProc->state() != QProcess::NotRunning;
    const bool isOllama     = (m_ai->backend() == AiClient::Ollama
                                && m_ai->port() == P::kOllamaPort);
    const bool isDwarfStar  = (m_ai->backend() == AiClient::Ollama
                                && m_ai->port() == P::kDwarfStarPort);

    auto* actOllama = menu->addAction(
        "\xf0\x9f\xa6\x99  Ollama  \xe2\x80\x94  localhost:11434");
    actOllama->setCheckable(true);
    actOllama->setChecked(isOllama);
    connect(actOllama, &QAction::triggered, this, &MainWindow::onOllamaActionTriggered);

    auto* actDwarf = menu->addAction(
        "\xe2\xad\x90  DwarfStar  \xe2\x80\x94  localhost:11435");
    actDwarf->setCheckable(true);
    actDwarf->setChecked(isDwarfStar);
    connect(actDwarf, &QAction::triggered, this, &MainWindow::onDwarfStarActionTriggered);

    menu->addSeparator();

    if (serverRunning) {
        auto* actInfo = menu->addAction(
            QString("\xf0\x9f\xa6\x99\xe2\x98\x81\xef\xb8\x8f  llama-server :%1  \xe2\x97\x8f in esecuzione")
            .arg(m_serverPort));
        actInfo->setEnabled(false);

        auto* actStop = menu->addAction(
            QString("\xf0\x9f\x94\xb4  Ferma server  :%1").arg(m_serverPort));
        connect(actStop, &QAction::triggered, this, &MainWindow::stopLlamaServer);
    } else {
        auto* actLSrv = menu->addAction(
            "\xf0\x9f\xa6\x99\xe2\x98\x81\xef\xb8\x8f  Avvia llama-server...");
        actLSrv->setCheckable(true);
        actLSrv->setChecked(!isOllama);
        connect(actLSrv, &QAction::triggered, this, &MainWindow::showServerDialog);
    }

    menu->exec(m_btnBackend->mapToGlobal(QPoint(0, m_btnBackend->height())));
    menu->deleteLater();
}

void MainWindow::onOllamaActionTriggered()
{
    applyBackend(AiClient::Ollama, P::kLocalHost, P::kOllamaPort);
}

void MainWindow::onDwarfStarActionTriggered()
{
    applyBackend(AiClient::Ollama, P::kLocalHost, P::kDwarfStarPort);
}

// ─── Emergenza RAM ────────────────────────────────────────────────────────

void MainWindow::onEmergencyRamClicked()
{
    if (m_emergencyBtn) m_emergencyBtn->setEnabled(false);
    statusBar()->showMessage(
        "\xf0\x9f\x9a\xa8  Emergenza RAM \xe2\x80\x94 arresto modelli Ollama...");

    m_emergencyStopProc = new QProcess(this);
    m_emergencyStopProc->start("bash", {"-c",
        "ollama ps --no-trunc 2>/dev/null | awk 'NR>1{print $1}' | "
        "xargs -r -I{} ollama stop {}"});
    connect(m_emergencyStopProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onEmergencyStopFinished);
}

void MainWindow::onEmergencyStopFinished(int, QProcess::ExitStatus)
{
    if (m_emergencyStopProc) {
        m_emergencyStopProc->deleteLater();
        m_emergencyStopProc = nullptr;
    }
    statusBar()->showMessage(
        "\xf0\x9f\x9a\xa8  Modelli fermati \xe2\x80\x94 liberazione cache kernel...");

    m_emergencyCacheProc = new QProcess(this);
#ifdef Q_OS_WIN
    m_emergencyCacheProc->start("rundll32.exe", {"advapi32.dll,ProcessIdleTasks"});
#else
    m_emergencyCacheProc->start("pkexec",
        {"sh", "-c", "sync && echo 3 > /proc/sys/vm/drop_caches"});
#endif
    connect(m_emergencyCacheProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onEmergencyCacheFinished);
}

void MainWindow::onEmergencyCacheFinished(int code, QProcess::ExitStatus)
{
    if (m_emergencyCacheProc) {
        m_emergencyCacheProc->deleteLater();
        m_emergencyCacheProc = nullptr;
    }
    if (code == 0)
        statusBar()->showMessage(
            "\xe2\x9c\x85  Emergenza RAM completata \xe2\x80\x94 modelli fermati + cache liberata.");
    else
        statusBar()->showMessage(
            "\xe2\x9a\xa0\xef\xb8\x8f  Modelli fermati. Cache non liberata "
            "(password annullata o pkexec mancante).");
    if (m_emergencyBtn) m_emergencyBtn->setEnabled(true);
    QTimer::singleShot(6000, this, &MainWindow::onRestoreDefaultStatus);
}

// ─── llama-server ─────────────────────────────────────────────────────────

void MainWindow::onServerProcFinished(int code, QProcess::ExitStatus)
{
    /* Se il health-timer è ancora attivo il processo è crashato DURANTE lo startup,
     * prima che /health rispondesse. Non fare fallback automatico a Ollama in questo
     * caso: l'utente ha scelto llama-server e deve poter riprovare senza che il backend
     * cambi silenziosamente sotto di lui. */
    const bool duringStartup = (m_healthTimer != nullptr);
    if (duringStartup) {
        if (m_healthTimer) { m_healthTimer->stop(); m_healthTimer->deleteLater(); m_healthTimer = nullptr; }
        if (m_healthNam)   { m_healthNam->deleteLater(); m_healthNam = nullptr; }
        const QString lastLines = m_serverProc
            ? QString::fromLocal8Bit(m_serverProc->readAll()).right(400).trimmed()
            : QString();
        appendLog(
            QString("\xe2\x9d\x8c llama-server terminato durante lo startup (code <b>%1</b>). "
                    "Backend NON commutato a Ollama \xe2\x80\x94 riprova o scegli un altro modello.")
            .arg(code));
        if (!lastLines.isEmpty())
            appendLog(QString("<pre style='font-size:10px'>%1</pre>").arg(lastLines.toHtmlEscaped()));
        statusBar()->showMessage(
            QString("\xe2\x9d\x8c  llama-server terminato (code %1) prima di essere pronto. "
                    "Riprova o usa Ollama.")
            .arg(code));
        if (m_btnBackend) {
            m_btnBackend->setText("\xe2\x9d\x8c  Crash startup");
            QTimer::singleShot(4000, this, &MainWindow::refreshBackendBtn);
        }
        if (m_serverProc) { m_serverProc->deleteLater(); m_serverProc = nullptr; }
        return;
    }

    appendLog(
        QString("\xf0\x9f\x94\xb4 llama-server terminato (code <b>%1</b>) \xe2\x80\x94 ripristino Ollama")
        .arg(code));
    statusBar()->showMessage(
        QString("\xf0\x9f\x94\xb4  llama-server terminato (code %1). Backend tornato a Ollama.")
        .arg(code));
    if (m_serverProc) { m_serverProc->deleteLater(); m_serverProc = nullptr; }
    applyBackend(AiClient::Ollama, P::kLocalHost, P::kOllamaPort);
}

void MainWindow::onServerProcessError(QProcess::ProcessError err)
{
    if (err == QProcess::FailedToStart) {
        if (m_btnBackend) {
            m_btnBackend->setText("\xe2\x9d\x8c  Errore avvio");
            QTimer::singleShot(3000, this, &MainWindow::refreshBackendBtn);
        }
        appendLog(
            "\xe2\x9d\x8c <b>llama-server</b>: impossibile avviare \xe2\x80\x94 "
            "verifica il percorso binario");
        statusBar()->showMessage(
            "\xe2\x9d\x8c  Impossibile avviare llama-server. Verifica il percorso binario.");
        if (m_serverProc) { m_serverProc->deleteLater(); m_serverProc = nullptr; }
    }
}

void MainWindow::onHealthTick()
{
    static constexpr int MAX_HEALTH_TICKS = 300;   /* 5 minuti — sufficiente per modelli 33b+ su CPU */
    ++m_healthTicks;

    if (m_healthTicks % 5 == 0)
        statusBar()->showMessage(
            QString("\xe2\x8f\xb3  Caricamento modello... %1s / %2s (porta %3)")
            .arg(m_healthTicks).arg(MAX_HEALTH_TICKS).arg(m_serverPort));

    if (m_healthTicks > MAX_HEALTH_TICKS || !m_serverProc) {
        if (m_healthTimer) {
            m_healthTimer->stop();
            m_healthTimer->deleteLater();
            m_healthTimer = nullptr;
        }
        if (m_healthNam) { m_healthNam->deleteLater(); m_healthNam = nullptr; }
        if (m_btnBackend) {
            m_btnBackend->setText("\xe2\x9d\x8c  Timeout");
            QTimer::singleShot(3000, this, &MainWindow::refreshBackendBtn);
        }
        if (m_serverProc)
            statusBar()->showMessage(
                QString("\xe2\x9a\xa0\xef\xb8\x8f  llama-server non risponde dopo %1s sulla porta %2 "
                        "\xe2\x80\x94 il modello potrebbe essere troppo grande per la RAM disponibile.")
                .arg(MAX_HEALTH_TICKS).arg(m_serverPort));
        return;
    }

    QNetworkRequest req(QUrl(
        QString("http://%1:%2/health").arg(P::kLocalHost).arg(m_serverPort)));
    req.setTransferTimeout(900);
    auto* reply = m_healthNam->get(req);
    connect(reply, &QNetworkReply::finished, this, &MainWindow::onHealthReply);
}

void MainWindow::onHealthReply()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    const bool ok = (reply->error() == QNetworkReply::NoError);
    reply->deleteLater();
    if (!ok) return;

    if (m_healthTimer) {
        m_healthTimer->stop();
        m_healthTimer->deleteLater();
        m_healthTimer = nullptr;
    }
    if (m_healthNam) { m_healthNam->deleteLater(); m_healthNam = nullptr; }
    if (m_btnBackend) {
        m_btnBackend->setText("\xe2\x9c\x85  Pronto");
        QTimer::singleShot(2000, this, &MainWindow::refreshBackendBtn);
    }
    appendLog(
        QString("\xf0\x9f\xa6\x99 llama-server pronto su porta <b>%1</b>").arg(m_serverPort));
    statusBar()->showMessage(
        QString("\xe2\x9c\x85  llama-server pronto su porta %1 \xe2\x80\x94 backend commutato.")
        .arg(m_serverPort));
    applyBackend(AiClient::LlamaServer, P::kLocalHost, m_serverPort);
}

// ─── Chat sidebar ─────────────────────────────────────────────────────────

void MainWindow::onNewChatClicked()
{
    QTextEdit* log = nullptr;
    if (auto* ap = m_mainTabs ? m_mainTabs->widget(0) : nullptr)
        log = ap->findChild<QTextEdit*>();

    const bool hasContent = log && !log->toPlainText().trimmed().isEmpty();

    if (hasContent) {
        QMessageBox dlg(this);
        dlg.setWindowTitle("\xf0\x9f\x93\xbc  Salva chat");
        dlg.setText(
            "<b>\xf0\x9f\x93\xbc  Vuoi salvare questa chat?</b><br><br>"
            "La conversazione attuale verr\xc3\xa0 persa se non la salvi.");
        dlg.setIcon(QMessageBox::Question);
        QPushButton* btnSalva  = dlg.addButton(
            "\xf0\x9f\x93\xbc  Salva", QMessageBox::AcceptRole);
        QPushButton* btnScarta = dlg.addButton("Scarta", QMessageBox::DestructiveRole);
        dlg.addButton("Annulla", QMessageBox::RejectRole);
        dlg.setDefaultButton(btnSalva);
        dlg.exec();

        if (dlg.clickedButton() == btnSalva) {
            const QString html = log->toHtml();
            if (m_currentChatId.isEmpty())
                m_currentChatId = m_chatHistory.newSession("(salvato manualmente)");
            m_chatHistory.saveLog(m_currentChatId, html);
            refreshChatList();
        } else if (dlg.clickedButton() != btnScarta) {
            return;
        }
    }

    m_currentChatId.clear();
    if (log) log->clear();
    navigateTo(0);
}

void MainWindow::onChatSearchChanged(const QString& q)
{
    const QString query = q.trimmed().toLower();
    for (int i = 0; i < m_chatList->count(); ++i) {
        auto* item = m_chatList->item(i);
        if (!item) continue;
        const bool isPlaceholder = item->data(Qt::UserRole).toString().isEmpty();
        item->setHidden(!isPlaceholder && !query.isEmpty()
                        && !item->text().toLower().contains(query));
    }
}

void MainWindow::onChatContextMenuRequested(const QPoint& pos)
{
    auto* item = m_chatList->itemAt(pos);
    if (!item) return;
    m_ctxChatId    = item->data(Qt::UserRole).toString();
    m_ctxChatTitle = item->text();

    auto* menu = new QMenu(m_chatList);

    auto* actPdf = menu->addAction("\xf0\x9f\x93\x84  Salva come PDF");
    connect(actPdf, &QAction::triggered, this, &MainWindow::onChatActionPdf);

    menu->addSeparator();

    auto* actDel = menu->addAction("\xf0\x9f\x97\x91  Elimina");
    connect(actDel, &QAction::triggered, this, &MainWindow::onChatActionDelete);

    menu->exec(m_chatList->viewport()->mapToGlobal(pos));
    menu->deleteLater();
}

void MainWindow::onChatActionPdf()
{
    const QString html = m_chatHistory.loadLog(m_ctxChatId);
    if (html.isEmpty()) {
        statusBar()->showMessage("\xe2\x9a\xa0  Chat vuota, nessun PDF.");
        return;
    }
    const QString def  = QDir::homePath() + "/" + m_ctxChatTitle + ".pdf";
    const QString path = QFileDialog::getSaveFileName(
        this, "Salva conversazione come PDF", def, "PDF (*.pdf)");
    if (path.isEmpty()) return;

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    QTextDocument doc;
    doc.setDefaultStyleSheet(
        "body { font-family: 'Segoe UI', Arial, sans-serif; font-size: 11pt; color:#111; }"
        "p    { margin: 4px 0; }");
    doc.setHtml(html);
    doc.print(&writer);

    statusBar()->showMessage("\xe2\x9c\x85  PDF salvato: " + path);
}

void MainWindow::onChatActionDelete()
{
    const auto btn = QMessageBox::question(this, "Elimina chat",
        QString("Eliminare la chat \"%1\"?").arg(m_ctxChatTitle),
        QMessageBox::Yes | QMessageBox::No);
    if (btn != QMessageBox::Yes) return;
    m_chatHistory.remove(m_ctxChatId);
    refreshChatList();
}

// ─── Chat sidebar — Canc / Shift+Canc ──────────────────────────────────────

/* eventFilter: intercetta tasti su m_chatList */
bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_chatList && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Delete) {
            if (ke->modifiers() & Qt::ShiftModifier)
                onChatDeleteShift();
            else
                onChatDeleteConfirm();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

/* Canc senza Shift: mostra QMessageBox di conferma */
void MainWindow::onChatDeleteConfirm()
{
    auto* item = m_chatList ? m_chatList->currentItem() : nullptr;
    if (!item) return;
    const QString id    = item->data(Qt::UserRole).toString();
    const QString title = item->text();
    if (id.isEmpty()) return;

    const auto btn = QMessageBox::question(this, "Elimina chat",
        QString("Vuoi eliminare questa chat?\n\"%1\"").arg(title),
        QMessageBox::Yes | QMessageBox::No);
    if (btn != QMessageBox::Yes) return;
    m_chatHistory.remove(id);
    refreshChatList();
}

/* Shift+Canc: elimina subito ma offre Annulla per 5s nella status bar */
void MainWindow::onChatDeleteShift()
{
    auto* item = m_chatList ? m_chatList->currentItem() : nullptr;
    if (!item) return;
    const QString id    = item->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    /* Annulla eventuale undo precedente ancora pendente */
    if (m_undoTimer && m_undoTimer->isActive()) {
        m_undoTimer->stop();
        onChatUndoTimeout();   /* esegue la cancellazione precedente in sospeso */
    }

    m_undoChatId = id;

    /* Prepara label "Annulla (5s)" nella status bar (creata una volta sola) */
    if (!m_undoLabel) {
        m_undoLabel = new QLabel(this);
        m_undoLabel->setObjectName("undoChatLabel");
        m_undoLabel->setTextFormat(Qt::RichText);
        m_undoLabel->setOpenExternalLinks(false);
        connect(m_undoLabel, &QLabel::linkActivated,
                this, &MainWindow::onChatUndoDelete);
        statusBar()->addWidget(m_undoLabel);
    }
    m_undoLabel->setText(
        "<a href='undo' style='color:#f59e0b;'>"
        "Annulla eliminazione (5s)</a>");
    m_undoLabel->show();

    /* Rimuovi dalla lista UI subito (feedback visivo) */
    refreshChatList();

    /* Timer 5s per cancellazione definitiva */
    if (!m_undoTimer) {
        m_undoTimer = new QTimer(this);
        m_undoTimer->setSingleShot(true);
        connect(m_undoTimer, &QTimer::timeout,
                this, &MainWindow::onChatUndoTimeout);
    }
    m_undoTimer->start(5000);
}

/* L'utente ha cliccato "Annulla": ripristina la chat */
void MainWindow::onChatUndoDelete()
{
    if (m_undoTimer) m_undoTimer->stop();
    m_undoChatId.clear();
    if (m_undoLabel) {
        statusBar()->removeWidget(m_undoLabel);
        m_undoLabel->hide();
    }
    /* Non abbiamo ancora cancellato dal disco — nessun ripristino necessario */
    statusBar()->showMessage("Eliminazione annullata.", 2000);
}

/* 5 secondi scaduti: cancella definitivamente */
void MainWindow::onChatUndoTimeout()
{
    if (!m_undoChatId.isEmpty()) {
        m_chatHistory.remove(m_undoChatId);
        m_undoChatId.clear();
    }
    if (m_undoLabel) {
        statusBar()->removeWidget(m_undoLabel);
        m_undoLabel->hide();
    }
    refreshChatList();
}

// ─── Pagine contenuto ─────────────────────────────────────────────────────

void MainWindow::onCronPanelFirstOpen()
{
    ensureSettingsDialog();
    m_strumentiPage->installCronPanel(m_impPage->manutenzione());
}

void MainWindow::onGraficoRequestSettings(const QString& tabName)
{
    ensureSettingsDialog();
    m_impDlg->show();
    m_impDlg->raise();
    m_impDlg->activateWindow();
    if (m_impPage) m_impPage->switchToTab(tabName);
}

void MainWindow::onOpenSettingsDipendenze(const QString& pipPkg)
{
    ensureSettingsDialog();
    m_impDlg->show();
    m_impDlg->raise();
    m_impDlg->activateWindow();
    if (!m_impPage) return;
    /* Defer di un tick: la dialog processa il show() prima della navigazione tab */
    const QString pkg = pipPkg;
    QTimer::singleShot(0, m_impPage, [this, pkg]() {
        m_impPage->jumpToPipInstall(pkg);
    });
}

void MainWindow::onMathSubTabChanged(int idx)
{
    if (idx == 1 && m_grafCanvas)
        m_grafCanvas->repaint();
}

void MainWindow::onMainTabChanged(int idx)
{
    static int prevIdx = 0;
    if (prevIdx != idx) {
        auto* progPage = qobject_cast<ProgrammazionePage*>(m_mainTabs->widget(prevIdx));
        if (progPage && progPage->hasUnsavedWork()) {
            QMessageBox dlg(this);
            dlg.setWindowTitle("\xf0\x9f\x92\xbe  Lavoro non salvato");
            dlg.setText(
                "<b>Hai modifiche non salvate nella scheda Programmazione.</b><br>"
                "Vuoi salvarle prima di continuare?");
            dlg.setIcon(QMessageBox::Question);
            auto* btnSave = dlg.addButton("Salva", QMessageBox::AcceptRole);
            dlg.addButton("Continua senza salvare", QMessageBox::DestructiveRole);
            dlg.addButton("Torna indietro", QMessageBox::RejectRole);
            dlg.setDefaultButton(btnSave);
            dlg.exec();
            if (dlg.clickedButton() == btnSave) {
                progPage->saveCurrentFile();
            } else if (dlg.clickedButton() == nullptr ||
                       dlg.clickedButton()->text().contains("Torna")) {
                m_mainTabs->setCurrentIndex(prevIdx);
                return;
            }
        }
        prevIdx = idx;
    }
    if (idx >= 0 && idx < m_navBtns.size())
        m_navBtns[idx]->setChecked(true);
}

void MainWindow::onSyncNavBackendClone()
{
    if (m_navBackendClone) {
        m_navBackendClone->setText(m_btnBackend->text());
        m_navBackendClone->setStyleSheet(m_btnBackend->styleSheet());
    }
}

void MainWindow::onApplyExecBtnMode()
{
    applyExecBtnMode(m_pendingExecMode);
}

void MainWindow::onRequestShowInGrafico(const QString& formula, double xMin, double xMax,
                                        const QVector<QPointF>& points)
{
    if (!m_grafCanvas) return;
    if (m_mainTabs) {
        m_mainTabs->setCurrentIndex(4);
        if (auto* mc = qobject_cast<QWidget*>(m_mainTabs->widget(4)))
            if (auto* st = mc->findChild<QTabWidget*>("mathSubTabs"))
                st->setCurrentIndex(1);
    }
    if (!formula.isEmpty())
        m_grafCanvas->setCartesian(formula, xMin, xMax);
    else if (!points.isEmpty())
        m_grafCanvas->setScatter(points);
}

// ─── Impostazioni RAG progress ────────────────────────────────────────────

void MainWindow::onAutoRagIndex()
{
    ensureSettingsDialog();
    if (m_impPage)
        m_impPage->autoIndexIfEmpty();
}

void MainWindow::onIndexingProgress(int done, int total)
{
    statusBar()->showMessage(
        QString("\xe2\x8f\xb3  Indicizzazione RAG: %1 / %2 chunk...").arg(done).arg(total));
}

void MainWindow::onIndexingFinished(int n, bool aborted)
{
    if (aborted)
        statusBar()->showMessage(
            QString("\xe2\x8f\xb9  RAG interrotto \xe2\x80\x94 %1 chunk salvati.").arg(n), 6000);
    else if (n == 0)
        statusBar()->showMessage(
            "\xe2\x9d\x8c  RAG: embedding falliti \xe2\x80\x94 installa nomic-embed-text.", 8000);
    else
        statusBar()->showMessage(
            QString("\xe2\x9c\x85  RAG completato: %1 chunk indicizzati.").arg(n), 6000);
}

// ─── Log dialog ───────────────────────────────────────────────────────────

void MainWindow::onClearLogClicked()
{
    if (m_logView) m_logView->clear();
}

// ─── VRAM bench ───────────────────────────────────────────────────────────

void MainWindow::onVramBenchFinished(int code, QProcess::ExitStatus)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (proc) proc->deleteLater();
    if (code == 0 && QFileInfo::exists(P::vramProfilePath()))
        statusBar()->showMessage(
            "\xe2\x9c\x85  VRAM benchmark completato \xe2\x80\x94 profilo salvato in vram_profile.json");
    else
        statusBar()->showMessage(
            "\xe2\x9a\xa0  VRAM benchmark fallito (codice " +
            QString::number(code) + ") \xe2\x80\x94 verr\xc3\xa0 riprovato al prossimo avvio");
    QTimer::singleShot(6000, this, &MainWindow::onRestoreDefaultStatus);
}

// ─── Onboarding wizard ────────────────────────────────────────────────────

void MainWindow::onOnboardingAccepted()
{
    QSettings s2("Prismalux", "GUI");
    if (m_onbBackend) s2.setValue(P::SK::kActiveBackend, m_onbBackend->currentData().toInt());
    if (m_onbModel)   s2.setValue(P::SK::kActiveModel,   m_onbModel->currentData().toString());
    if (m_onbTheme) {
        const QString theme = m_onbTheme->currentData().toString();
        s2.setValue(P::SK::kTheme, theme);
        m_pendingTheme = theme;
    }
    s2.setValue(P::SK::kSetupDone, true);
    QMetaObject::invokeMethod(this, &MainWindow::onApplyPendingTheme, Qt::QueuedConnection);
    auto* dlg = m_onbDlg;
    m_onbDlg = nullptr;
    m_onbBackend = nullptr;
    m_onbModel   = nullptr;
    m_onbTheme   = nullptr;
    if (dlg) dlg->accept();
}

void MainWindow::onApplyPendingTheme()
{
    if (auto* tm = ThemeManager::instance())
        tm->apply(m_pendingTheme);
}

// ─── Status bar ───────────────────────────────────────────────────────────

void MainWindow::onRestoreDefaultStatus()
{
    statusBar()->showMessage(
        "\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
}

void MainWindow::onZoomMinusBtnClicked()
{
    onZoomPercentChanged(m_zoomPct - 10);
}

void MainWindow::onZoomPlusBtnClicked()
{
    onZoomPercentChanged(m_zoomPct + 10);
}

void MainWindow::onZoomResetBtnClicked()
{
    onZoomPercentChanged(100);
}

void MainWindow::onZoomPercentChanged(int pct)
{
    m_zoomPct = qBound(50, pct, 200);

    /* Aggiorna label e zoom scale immediatamente — nessun ritardo visivo */
    if (m_zoomPctLbl)
        m_zoomPctLbl->setText(QString::number(m_zoomPct) + "%");
    ThemeManager::instance()->setZoomScale(m_zoomPct / 100.0);

    /* Persisti subito così il valore sopravvive anche a crash */
    QSettings s("Prismalux", "GUI");
    s.setValue("ui/zoomPct", m_zoomPct);

    /* Debounce: riapplica il tema 200ms dopo l'ultimo click.
     * Click ravvicinati azzerano il timer → una sola passata di setStyleSheet. */
    if (m_zoomDebounce)
        m_zoomDebounce->start();
}

void MainWindow::onZoomApplyDebounced()
{
    /* Riapplica il QSS con il fattore zoom corrente.
     * Grazie alla cache per-zoom, zoom già usati in precedenza sono istantanei. */
    ThemeManager::instance()->reapply();

    /* Aggiorna anche il font app per widget non stilizzati via QSS */
    static constexpr int kBasePt = 10;
    const int pt = qMax(5, qRound(kBasePt * m_zoomPct / 100.0));
    QFont f = QApplication::font();
    f.setPointSize(pt);
    QApplication::setFont(f);
}

/* ══════════════════════════════════════════════════════════════
   checkForUpdates — controlla nuova versione su GitHub API
   Notifica non intrusiva nella status bar se disponibile.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::checkForUpdates()
{
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(
        QUrl("https://api.github.com/repos/wildlux/Prismalux/releases/latest"));
    req.setRawHeader("User-Agent", "Prismalux/2.9");
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(8000);
    auto* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) return;
        QString tag = doc.object().value("tag_name").toString().trimmed();
        if (tag.isEmpty()) return;
        if (tag.startsWith('v') || tag.startsWith('V')) tag = tag.mid(1);
        if (tag == "2.9") return;
        auto* lbl = new QLabel(this);
        lbl->setObjectName("updateStatusLbl");
        lbl->setTextFormat(Qt::RichText);
        lbl->setOpenExternalLinks(true);
        lbl->setStyleSheet("QLabel#updateStatusLbl{color:#f59e0b;font-weight:bold;}");
        lbl->setText(
            "\xf0\x9f\x86\x95 Prismalux <b>v" + tag + "</b> disponibile &mdash; "
            "<a href='https://github.com/wildlux/Prismalux/releases'>Scarica</a>");
        statusBar()->addPermanentWidget(lbl);
    });
}

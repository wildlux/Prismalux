#include "widget_ai_script_base.h"
#include "../ai_client.h"
#include "../widgets/ai_error_widget.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include <QFile>
#include <QProcess>
#include <QProgressBar>
#include <QStandardPaths>
#include <QTextCursor>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
namespace P = PrismaluxPaths;

AiScriptWidget::AiScriptWidget(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai) {}

/* ══════════════════════════════════════════════════════════════
   runSciScript — scrive lo script su file temporaneo e lo esegue
   ══════════════════════════════════════════════════════════════ */
void AiScriptWidget::runSciScript(const QString& code, bool isBash,
                                  QLabel* statusLbl, QPushButton* execBtn,
                                  QTextEdit* output, QProcess*& procRef,
                                  QObject* parent)
{
    if (code.isEmpty()) return;
    const QString suffix  = isBash ? ".sh" : ".py";
    const QString tmpPath = P::safeTempPath() + "/prismalux_bio_script" + suffix;
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusLbl->setText("\xe2\x9d\x8c  Impossibile creare script temporaneo");
        return;
    }
    f.write(code.toUtf8());
    f.close();

    if (!procRef) {
        procRef = new QProcess(parent);
        procRef->setProcessChannelMode(QProcess::MergedChannels);
        QObject::connect(procRef, &QProcess::readyRead, parent,
            [procRef, output](){
                output->append(QString::fromUtf8(procRef->readAll()).trimmed());
            });
        QObject::connect(procRef,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            parent, [statusLbl, execBtn](int code2, QProcess::ExitStatus){
                statusLbl->setText(code2 == 0
                    ? "\xe2\x9c\x85  Completato"
                    : "\xe2\x9d\x8c  Terminato con errore");
                execBtn->setEnabled(true);
            });
    }
    execBtn->setEnabled(false);
    statusLbl->setText("\xf0\x9f\x94\x84  Esecuzione...");
    if (isBash) {
        const QString bash = QStandardPaths::findExecutable("bash");
        procRef->start(bash.isEmpty() ? "bash" : bash, {tmpPath});
    } else {
        procRef->start(P::findPython(), {tmpPath});
    }
    if (procRef->state() == QProcess::NotRunning)
        statusLbl->setText("\xe2\x9d\x8c  Interprete non trovato");
}

/* ══════════════════════════════════════════════════════════════
   avviaSci — avvia streaming AI e aggiorna puntatori correnti
   ══════════════════════════════════════════════════════════════ */
void AiScriptWidget::avviaSci(const QString& sys, const QString& userMsg,
                               QTextEdit* out, QPushButton* runBtn,
                               QPushButton* stopBtn, QComboBox* modelCombo,
                               QPushButton* execBtn, QString* codeRef,
                               QLabel* statusLbl)
{
    if (m_ai->busy()) {
        out->append("\xe2\x9a\xa0  AI occupata, attendi o premi Stop.");
        return;
    }
    if (userMsg.trimmed().isEmpty()) {
        out->append("\xe2\x9a\xa0  Inserisci la richiesta prima di eseguire.");
        return;
    }
    if (modelCombo && modelCombo->count() > 0) {
        const QString sel = modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    m_sciOut        = out;
    m_sciRunBtn     = runBtn;
    m_sciStopBtn    = stopBtn;
    m_sciExecBtn    = execBtn;
    m_sciCodeRef    = codeRef;
    m_sciStatusLbl  = statusLbl;
    m_sciModelCombo = modelCombo;
    m_sciSys        = sys;
    m_sciUserMsg    = userMsg;

    QObject::disconnect(m_sciTokenConn);
    QObject::disconnect(m_sciFinishedConn);
    QObject::disconnect(m_sciErrorConn);
    m_sciTokenConn    = connect(m_ai, &AiClient::token,    this, &AiScriptWidget::onSciToken);
    m_sciFinishedConn = connect(m_ai, &AiClient::finished, this, &AiScriptWidget::onSciFinished);
    m_sciErrorConn    = connect(m_ai, &AiClient::error,    this, &AiScriptWidget::onSciError);

    runBtn->setEnabled(false);
    if (m_progress)  m_progress->setVisible(true);
    stopBtn->setEnabled(true);
    out->append("\n\xf0\x9f\x94\x84  Generazione in corso...\n" + QString(40, QChar(0x2500)));
    m_ai->chat(sys, userMsg);
}

void AiScriptWidget::onSciToken(const QString& t)
{
    if (!m_sciOut) return;
    m_sciOut->moveCursor(QTextCursor::End);
    m_sciOut->insertPlainText(t);
}

void AiScriptWidget::onSciFinished(const QString& full)
{
    QObject::disconnect(m_sciTokenConn);
    QObject::disconnect(m_sciFinishedConn);
    QObject::disconnect(m_sciErrorConn);
    m_sciTokenConn = m_sciFinishedConn = m_sciErrorConn = {};

    if (m_sciRunBtn)   m_sciRunBtn->setEnabled(true);
    if (m_sciStopBtn)  m_sciStopBtn->setEnabled(false);
    if (m_progress)    m_progress->setVisible(false);
    if (m_sciOut)      m_sciOut->append("\n" + QString(40, QChar(0x2500)));

    if (m_sciExecBtn && m_sciCodeRef && full.contains("```")) {
        int start = full.indexOf("```python");
        if (start == -1) start = full.indexOf("```");
        if (start != -1) {
            start = full.indexOf('\n', start) + 1;
            const int end = full.indexOf("```", start);
            if (end != -1) {
                *m_sciCodeRef = full.mid(start, end - start).trimmed();
                if (!m_sciCodeRef->isEmpty()) {
                    m_sciExecBtn->setEnabled(true);
                    if (m_sciStatusLbl)
                        m_sciStatusLbl->setText("\xe2\x9c\x85  Codice pronto \xe2\x80\x94 premi Esegui");
                }
            }
        }
    }
}

void AiScriptWidget::onSciError(const QString& msg)
{
    QObject::disconnect(m_sciTokenConn);
    QObject::disconnect(m_sciFinishedConn);
    QObject::disconnect(m_sciErrorConn);
    m_sciTokenConn = m_sciFinishedConn = m_sciErrorConn = {};

    if (m_sciRunBtn)  m_sciRunBtn->setEnabled(true);
    if (m_sciStopBtn) m_sciStopBtn->setEnabled(false);
    if (m_progress)   m_progress->setVisible(false);
    if (m_errorPanel) {
        m_errorPanel->setVisible(true);
        m_errorPanel->showError(msg, [this]{
            avviaSci(m_sciSys, m_sciUserMsg, m_sciOut, m_sciRunBtn, m_sciStopBtn,
                     m_sciModelCombo, m_sciExecBtn, m_sciCodeRef, m_sciStatusLbl);
        });
    }
    LogBus::post("\xe2\x9d\x8c AiScript: Errore AI: " + msg);
}

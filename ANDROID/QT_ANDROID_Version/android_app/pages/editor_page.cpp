#include "editor_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QProcess>
#include <QTemporaryFile>
#include <QTextCursor>
#include <QScroller>
#include <QDir>

static constexpr const char* kEditorSys =
    "Sei un esperto programmatore. Analizza il codice e rispondi in modo chiaro e conciso.";

EditorPage::EditorPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x9d") + " Editor + REPL", this);
    {   QFont f = title->font(); f.setPointSize(14); f.setBold(true); title->setFont(f); }
    vbox->addWidget(title);

    /* Toolbar */
    auto* toolbar = new QHBoxLayout;
    m_langCmb = new QComboBox(this);
    m_langCmb->addItem("Python", "python");
    m_langCmb->addItem("JavaScript", "js");
    m_langCmb->addItem("Bash", "bash");
    m_langCmb->addItem("Testo", "text");
    toolbar->addWidget(m_langCmb);

    m_runBtn = new QPushButton(
        QString::fromUtf8("\xe2\x96\xb6"), this);
    m_runBtn->setToolTip("Esegui");
    m_runBtn->setMinimumSize(44, 44);
    m_runBtn->setObjectName("PrimaryBtn");
    toolbar->addWidget(m_runBtn);

    m_clrBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x97\x91"), this);
    m_clrBtn->setToolTip("Pulisci output");
    m_clrBtn->setMinimumSize(44, 44);
    toolbar->addWidget(m_clrBtn);

    m_explBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\xa4\x96") + " Spiega", this);
    m_explBtn->setMinimumHeight(44);
    toolbar->addWidget(m_explBtn, 1);

    m_fixBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\xa7") + " Fix", this);
    m_fixBtn->setMinimumHeight(44);
    toolbar->addWidget(m_fixBtn, 1);

    m_stopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9"), this);
    m_stopBtn->setObjectName("DangerBtn");
    m_stopBtn->setMinimumSize(44, 44);
    m_stopBtn->setVisible(false);
    toolbar->addWidget(m_stopBtn);
    vbox->addLayout(toolbar);

    /* Editor */
    m_editor = new QTextEdit(this);
    m_editor->setObjectName("InputEdit");
    m_editor->setPlaceholderText("# Scrivi qui il tuo codice Python...\nprint('Hello, Android!')");
    {   QFont f("Monospace", 12); m_editor->setFont(f); }
    QScroller::grabGesture(m_editor->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_editor, 3);

    /* Output */
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setObjectName("OutputEdit");
    m_output->setPlaceholderText("Output e risultati AI...");
    {   QFont f("Monospace", 11); m_output->setFont(f); }
    QScroller::grabGesture(m_output->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_output, 2);

    m_status = new QLabel("", this);
    m_status->setObjectName("StatusLabel");
    vbox->addWidget(m_status);

    connect(m_runBtn,  &QPushButton::clicked, this, &EditorPage::onRunClicked);
    connect(m_clrBtn,  &QPushButton::clicked, this, &EditorPage::onClearClicked);
    connect(m_explBtn, &QPushButton::clicked, this, &EditorPage::onAiExplainClicked);
    connect(m_fixBtn,  &QPushButton::clicked, this, &EditorPage::onAiFixClicked);
    connect(m_stopBtn, &QPushButton::clicked, m_ai, &AiClient::abort);
    connect(m_ai, &AiClient::token,    this, &EditorPage::onToken);
    connect(m_ai, &AiClient::finished, this, &EditorPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &EditorPage::onAiError);
    connect(m_ai, &AiClient::aborted,  this, &EditorPage::onAborted);
}

void EditorPage::onRunClicked()
{
    const QString code = m_editor->toPlainText().trimmed();
    if (code.isEmpty()) return;
    const QString lang = m_langCmb->currentData().toString();

    if (lang == "text") {
        m_output->setPlainText(code);
        return;
    }

    m_output->clear();
    m_status->setText(QString::fromUtf8("\xe2\x8f\xb3") + " Esecuzione...");

    auto* proc = new QProcess(this);
    QStringList args;
    QString prog;

    QTemporaryFile* tmp = nullptr;

    if (lang == "python") {
        tmp = new QTemporaryFile(this);
        tmp->setFileTemplate(QDir::tempPath() + "/plx_XXXXXX.py");
        if (tmp->open()) { tmp->write(code.toUtf8()); tmp->flush(); }
        prog = "python3";
        args << tmp->fileName();
    } else if (lang == "bash") {
        prog = "sh";
        args << "-c" << code;
    } else if (lang == "js") {
        prog = "node";
        args << "-e" << code;
    }

    connect(proc, &QProcess::finished, this, [this, proc, tmp](int code, QProcess::ExitStatus) {
        const QString out = proc->readAllStandardOutput();
        const QString err = proc->readAllStandardError();
        if (!out.isEmpty()) m_output->append(out);
        if (!err.isEmpty())
            m_output->append(
                QString("<span style='color:#ef4444'>%1</span>")
                    .arg(err.toHtmlEscaped()));
        m_status->setText(
            code == 0 ? QString::fromUtf8("\xe2\x9c\x85 OK")
                      : QString::fromUtf8("\xe2\x9d\x8c Errore (exit %1)").arg(code));
        proc->deleteLater();
        if (tmp) tmp->deleteLater();
    });
    proc->start(prog, args);
    if (!proc->waitForStarted(3000))
        m_status->setText(
            QString::fromUtf8("\xe2\x9d\x8c ") + prog + " non trovato.");
}

void EditorPage::onClearClicked()
{
    m_output->clear();
    m_status->clear();
}

void EditorPage::onAiExplainClicked()
{
    const QString code = m_editor->toPlainText().trimmed();
    if (code.isEmpty()) return;
    m_aiMode = true;
    m_output->clear();
    setBusy(true);
    m_ai->chat(kEditorSys,
        "Spiega questo codice in modo chiaro e conciso:\n\n```\n" + code + "\n```");
}

void EditorPage::onAiFixClicked()
{
    const QString code = m_editor->toPlainText().trimmed();
    if (code.isEmpty()) return;
    m_aiMode = true;
    m_output->clear();
    setBusy(true);
    m_ai->chat(kEditorSys,
        "Trova e correggi i bug in questo codice. Mostra il codice corretto:\n\n```\n" +
        code + "\n```");
}

void EditorPage::onToken(const QString& t)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(t);
    m_output->moveCursor(QTextCursor::End);
}

void EditorPage::onFinished(const QString&)
{
    setBusy(false);
    m_status->setText(QString::fromUtf8("\xe2\x9c\x85 Completato."));
    m_aiMode = false;
}

void EditorPage::onAiError(const QString& msg)
{
    setBusy(false);
    m_status->setText(QString::fromUtf8("\xe2\x9d\x8c ") + msg);
    m_aiMode = false;
}

void EditorPage::onAborted()
{
    setBusy(false);
    m_status->setText("Interrotto.");
    m_aiMode = false;
}

void EditorPage::setBusy(bool busy)
{
    if (m_explBtn) m_explBtn->setEnabled(!busy);
    if (m_fixBtn)  m_fixBtn->setEnabled(!busy);
    if (m_runBtn)  m_runBtn->setEnabled(!busy);
    if (m_stopBtn) m_stopBtn->setVisible(busy);
    if (busy) m_status->setText(
        QString::fromUtf8("\xe2\x8f\xb3") + " AI sta elaborando...");
}

#include "hermes_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QTextCursor>
#include <QDateTime>
#include <QScroller>

/* ── Path file memoria utente ──────────────────────────────── */
QString HermesPage::memoryPath()
{
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/prismalux/";
    QDir().mkpath(dir);
    return dir + "user_knowledge.md";
}

static constexpr const char* kSysExtract =
    "Sei un agente preciso che estrae fatti utili su un utente da testi liberi. "
    "Dato il testo in input, estrai i fatti importanti come lista Markdown. "
    "Ogni fatto su una riga con '- '. Sii conciso e specifico. "
    "Rispondi SOLO con la lista di fatti, nessun altro testo.";

static constexpr const char* kSysReflect =
    "Sei Hermes, assistente di memoria personale. "
    "Analizza la memoria utente fornita e identifica pattern, connessioni e suggerimenti. "
    "Rispondi in Markdown con sezioni: ## Pattern, ## Connessioni, ## Suggerimenti. "
    "Sii conciso e pratico.";

HermesPage::HermesPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    m_gm = new GraphMemoryMobile(GraphMemoryMobile::defaultDbPath(), this);
    m_gm->open();

    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    /* Titolo */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\xa7\xa0") + " Hermes \xe2\x80\x94 Memoria Utente", this);
    {
        QFont f = title->font();
        f.setPointSize(14);
        f.setBold(true);
        title->setFont(f);
    }
    vbox->addWidget(title);

    /* Barra azioni */
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);

    m_saveBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x92\xbe") + " Salva", this);
    m_saveBtn->setObjectName("PrimaryBtn");
    m_saveBtn->setMinimumHeight(40);
    connect(m_saveBtn, &QPushButton::clicked, this, &HermesPage::onSaveClicked);
    btnRow->addWidget(m_saveBtn);

    m_reflectBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\xa7\xa0") + " Rifletti", this);
    m_reflectBtn->setObjectName("SecondaryBtn");
    m_reflectBtn->setMinimumHeight(40);
    connect(m_reflectBtn, &QPushButton::clicked, this, &HermesPage::onReflectClicked);
    btnRow->addWidget(m_reflectBtn);

    auto* clearBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x97\x91\xef\xb8\x8f"), this);
    clearBtn->setObjectName("DangerBtn");
    clearBtn->setMinimumHeight(40);
    clearBtn->setFixedWidth(48);
    connect(clearBtn, &QPushButton::clicked, this, &HermesPage::onClearClicked);
    btnRow->addWidget(clearBtn);

    m_stopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9") + " Stop", this);
    m_stopBtn->setObjectName("DangerBtn");
    m_stopBtn->setMinimumHeight(40);
    m_stopBtn->setVisible(false);
    connect(m_stopBtn, &QPushButton::clicked, m_ai, &AiClient::abort);
    btnRow->addWidget(m_stopBtn);

    vbox->addLayout(btnRow);

    /* Editor memoria */
    m_memEditor = new QTextEdit(this);
    m_memEditor->setObjectName("OutputEdit");
    m_memEditor->setPlaceholderText(
        "La tua memoria personale appare qui.\n"
        "Puoi editarla direttamente o usare 'Estrai e Salva' sotto.");
    {
        QFont f = m_memEditor->font();
        f.setPointSize(11);
        m_memEditor->setFont(f);
    }
    QScroller::grabGesture(m_memEditor->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_memEditor, 1);

    /* Sezione input estrazione */
    auto* addLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x96") + " Aggiungi alla memoria:", this);
    {
        QFont f = addLbl->font();
        f.setPointSize(11);
        f.setBold(true);
        addLbl->setFont(f);
    }
    vbox->addWidget(addLbl);

    auto* inputRow = new QHBoxLayout;
    inputRow->setSpacing(6);

    m_inputEdit = new QTextEdit(this);
    m_inputEdit->setFixedHeight(64);
    m_inputEdit->setPlaceholderText(
        "Scrivi fatti da memorizzare (es. \"Mi chiamo Mario, lavoro come sviluppatore C++\")...");
    m_inputEdit->setObjectName("InputEdit");
    inputRow->addWidget(m_inputEdit, 1);

    m_extractBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x96") + " Estrai", this);
    m_extractBtn->setObjectName("PrimaryBtn");
    m_extractBtn->setMinimumHeight(32);
    m_extractBtn->setFixedWidth(90);
    connect(m_extractBtn, &QPushButton::clicked, this, &HermesPage::onExtractClicked);
    inputRow->addWidget(m_extractBtn);

    vbox->addLayout(inputRow);

    /* Status */
    m_statusLbl = new QLabel("", this);
    m_statusLbl->setObjectName("StatusLabel");
    vbox->addWidget(m_statusLbl);

    connect(m_ai, &AiClient::token,    this, &HermesPage::onToken);
    connect(m_ai, &AiClient::finished, this, &HermesPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &HermesPage::onAiError);
    connect(m_ai, &AiClient::aborted,  this, &HermesPage::onAborted);

    loadMemory();
}

void HermesPage::loadMemory()
{
    QFile f(memoryPath());
    if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        m_memEditor->setPlainText(ts.readAll());
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9c\x85") + " Memoria caricata.");
    }
}

void HermesPage::onSaveClicked()
{
    saveMemory(m_memEditor->toPlainText());
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9c\x85") + " Memoria salvata.");
}

void HermesPage::saveMemory(const QString& content)
{
    QFile f(memoryPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << content;
    }
}

void HermesPage::onExtractClicked()
{
    const QString raw = m_inputEdit->toPlainText().trimmed();
    if (raw.isEmpty()) return;
    m_mode = Mode::Extracting;
    m_aiAccum.clear();
    setBusy(true);
    m_ai->chat(kSysExtract, raw);
}

void HermesPage::onReflectClicked()
{
    const QString mem = m_memEditor->toPlainText().trimmed();
    if (mem.isEmpty()) {
        m_statusLbl->setText("Nessuna memoria da analizzare.");
        return;
    }
    /* Prepend header della riflessione prima di avviare lo streaming */
    const QString header = "\n\n---\n## "
        + QString::fromUtf8("\xf0\x9f\xa7\xa0")
        + " Riflessione — "
        + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")
        + "\n\n";
    m_memEditor->moveCursor(QTextCursor::End);
    m_memEditor->insertPlainText(header);

    m_mode = Mode::Reflecting;
    m_aiAccum.clear();
    setBusy(true);
    m_ai->chat(kSysReflect, "Memoria utente:\n\n" + mem);
}

void HermesPage::onClearClicked()
{
    m_memEditor->clear();
    saveMemory("");
    m_statusLbl->setText("Memoria cancellata.");
}

void HermesPage::onToken(const QString& t)
{
    m_aiAccum += t;
    if (m_mode == Mode::Reflecting) {
        m_memEditor->moveCursor(QTextCursor::End);
        m_memEditor->insertPlainText(t);
        m_memEditor->moveCursor(QTextCursor::End);
    }
}

void HermesPage::onFinished(const QString&)
{
    if (m_mode == Mode::Extracting) {
        const QString ts = "\n\n---\n## "
            + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")
            + "\n" + m_aiAccum.trimmed() + "\n";
        const QString updated = m_memEditor->toPlainText().trimmed() + ts;
        m_memEditor->setPlainText(updated);
        saveMemory(updated);

        /* Salva fatti estratti anche nel grafo */
        const auto lines = m_aiAccum.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            const QString fact = line.trimmed();
            if (fact.startsWith("- ") && fact.length() > 4)
                m_gm->addNode("user_fact", fact.mid(2).left(80), fact.mid(2), 0.5f);
        }

        m_inputEdit->clear();
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9c\x85") + " Fatti estratti e salvati.");
    } else if (m_mode == Mode::Reflecting) {
        saveMemory(m_memEditor->toPlainText());
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9c\x85") + " Riflessione salvata in memoria.");
    }
    m_mode = Mode::Idle;
    setBusy(false);
}

void HermesPage::onAiError(const QString& msg)
{
    m_mode = Mode::Idle;
    setBusy(false);
    m_statusLbl->setText(QString::fromUtf8("\xe2\x9d\x8c") + " " + msg);
}

void HermesPage::onAborted()
{
    m_mode = Mode::Idle;
    setBusy(false);
    m_statusLbl->setText("Operazione interrotta.");
}

void HermesPage::setBusy(bool busy)
{
    m_saveBtn->setEnabled(!busy);
    m_reflectBtn->setEnabled(!busy);
    m_extractBtn->setEnabled(!busy);
    m_memEditor->setReadOnly(busy);
    m_stopBtn->setVisible(busy);
    if (busy) {
        const QString verb = (m_mode == Mode::Extracting) ? "estrae fatti" : "riflette";
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x8f\xb3") + " Hermes " + verb + "...");
    }
}

#include "simulatore_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QTextCursor>
#include <QScroller>

/* ── Definizioni algoritmi ──────────────────────────────────── */
namespace {
struct Algo {
    const char* icon;
    const char* name;
    const char* hint;
    const char* example;
    const char* sysPrompt;
};
static const Algo kAlgos[] = {
    {
        "\xf0\x9f\x94\x84", "Bubble Sort",
        "Inserisci numeri separati da virgola.",
        "5, 3, 8, 1, 9, 2, 7, 4",
        "Simula Bubble Sort passo per passo sull'array fornito. "
        "Per ogni passo mostra: la coppia confrontata, se vengono scambiati, "
        "e lo stato dell'array con formato [ a | b | c | ... ]. "
        "Evidenzia gli elementi confrontati con > <. "
        "Alla fine: array ordinato + numero totale di scambi e confronti."
    },
    {
        "\xe2\xac\x87\xef\xb8\x8f", "Insertion Sort",
        "Inserisci numeri separati da virgola.",
        "8, 5, 2, 9, 1, 3",
        "Simula Insertion Sort passo per passo. "
        "Per ogni elemento mostra dove viene inserito nella parte ordinata. "
        "Usa formato [ sorted... | *** elem *** | unsorted... ] per visualizzare. "
        "Alla fine: array ordinato + conteggio operazioni."
    },
    {
        "\xf0\x9f\x8e\xaf", "Quick Sort",
        "Inserisci numeri separati da virgola.",
        "7, 2, 9, 4, 1, 8, 3, 6",
        "Simula Quick Sort passo per passo con pivot = primo elemento. "
        "Per ogni partizione mostra: pivot scelto, elementi minori e maggiori, "
        "e ricorsione su sottoarray. Usa albero ASCII per le chiamate ricorsive. "
        "Alla fine: array ordinato + profondità massima ricorsione."
    },
    {
        "\xf0\x9f\x94\x8d", "Binary Search",
        "Inserisci: array ordinato; target (es. 1,3,5,7,9,11; 7)",
        "1, 3, 5, 7, 9, 11, 13, 15; 7",
        "Simula Binary Search passo per passo. "
        "Per ogni iterazione mostra: [low=X, mid=Y, high=Z], valore in mid, "
        "e quale metà viene scartata. "
        "Visualizza l'array con la finestra di ricerca evidenziata: [ ... [low..mid..high] ... ]. "
        "Alla fine: trovato/non trovato + numero di passi."
    },
    {
        "\xf0\x9f\x8c\x8a", "BFS — Breadth First Search",
        "Inserisci grafo come lista adiacenze: A:B,C B:D,E C:F nodo_inizio",
        "A:B,C B:D,E C:F,G D: E: F: G:; A",
        "Simula BFS passo per passo sul grafo fornito. "
        "Per ogni passo mostra: nodo visitato, coda corrente, nodi scoperti. "
        "Usa formato: Visito [X] → Coda: [Y, Z, ...] → Visitati: {A,B,...}. "
        "Alla fine: ordine di visita + livelli BFS (albero ASCII)."
    },
    {
        "\xf0\x9f\x8c\xb2", "DFS — Depth First Search",
        "Inserisci grafo come lista adiacenze: A:B,C B:D,E nodo_inizio",
        "A:B,C B:D E:F C:G; A",
        "Simula DFS passo per passo (iterativo con stack). "
        "Per ogni passo mostra: nodo visitato, stack corrente, nodi visitati. "
        "Usa formato: Visito [X] → Stack: [Y, Z, ...] → Visitati: {A,...}. "
        "Alla fine: ordine DFS + albero di ricerca ASCII."
    },
    {
        "\xf0\x9f\x94\x80", "Merge Sort",
        "Inserisci numeri separati da virgola.",
        "6, 3, 8, 2, 9, 1, 5, 4",
        "Simula Merge Sort passo per passo. "
        "Mostra la divisione ricorsiva come albero ASCII e la fase di merge. "
        "Per ogni merge: sottoliste in input e lista risultante. "
        "Alla fine: array ordinato + numero di operazioni di merge."
    },
    {
        "\xf0\x9f\x8e\x92", "Selection Sort",
        "Inserisci numeri separati da virgola.",
        "4, 7, 2, 9, 1, 5, 8, 3",
        "Simula Selection Sort passo per passo. "
        "Per ogni passo: trova il minimo nella parte non ordinata, "
        "mostra dove viene scambiato, e stato dell'array. "
        "Formato: [ sorted... || min* unsorted... ]. "
        "Alla fine: array ordinato + confronti effettuati."
    },
    {
        "\xf0\x9f\x9b\xa4\xef\xb8\x8f", "Dijkstra",
        "Inserisci grafo: A-B:4 A-C:2 B-D:3 C-B:1 nodo_inizio",
        "A-B:4 A-C:2 C-B:1 B-D:3 C-D:7; A",
        "Simula l'algoritmo di Dijkstra passo per passo. "
        "Per ogni passo: nodo estratto dalla priority queue, "
        "distanze aggiornate, stato della coda. "
        "Formato: Estraggo [X, d=N] → Aggiorno vicini → dist[Y]=N. "
        "Alla fine: albero dei cammini minimi + distanze finali."
    },
    {
        "\xf0\x9f\x8e\x92", "Knapsack 0/1",
        "Inserisci: pesi,valori,capacità (es. 2,3,4,5; 3,4,5,6; 8)",
        "2, 3, 4, 5; 3, 4, 5, 6; 8",
        "Simula il Knapsack 0/1 con programmazione dinamica. "
        "Mostra la tabella DP riga per riga (oggetti x capacità). "
        "Per ogni cella spiega la scelta: prendo/non prendo oggetto i con capacità j. "
        "Alla fine: valore massimo + oggetti selezionati + tabella DP completa."
    },
};
static constexpr int kAlgoCount = static_cast<int>(sizeof(kAlgos)/sizeof(kAlgos[0]));
} // namespace

SimulatorePage::SimulatorePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    /* Titolo */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\xa4\x96") + " Simulatore Algoritmi", this);
    {
        QFont f = title->font();
        f.setPointSize(14);
        f.setBold(true);
        title->setFont(f);
    }
    vbox->addWidget(title);

    /* Selettore algoritmo */
    m_algoCombo = new QComboBox(this);
    for (int i = 0; i < kAlgoCount; ++i)
        m_algoCombo->addItem(
            QString::fromUtf8(kAlgos[i].icon) + " " + kAlgos[i].name, i);
    m_algoCombo->setMinimumHeight(48);
    vbox->addWidget(m_algoCombo);

    /* Hint input */
    m_inputHint = new QLabel(kAlgos[0].hint, this);
    m_inputHint->setObjectName("StatusLabel");
    m_inputHint->setWordWrap(true);
    vbox->addWidget(m_inputHint);

    /* Input dati */
    m_inputEdit = new QTextEdit(this);
    m_inputEdit->setFixedHeight(56);
    m_inputEdit->setPlaceholderText(kAlgos[0].example);
    m_inputEdit->setObjectName("InputEdit");
    {
        QFont f = m_inputEdit->font();
        f.setFamily("monospace");
        m_inputEdit->setFont(f);
    }
    vbox->addWidget(m_inputEdit);

    /* Bottoni Run / Stop */
    auto* btnRow = new QHBoxLayout;
    m_runBtn = new QPushButton(
        QString::fromUtf8("\xe2\x96\xb6\xef\xb8\x8f") + " Simula", this);
    m_runBtn->setObjectName("PrimaryBtn");
    m_runBtn->setMinimumHeight(48);
    {
        QFont f = m_runBtn->font();
        f.setBold(true);
        m_runBtn->setFont(f);
    }
    connect(m_runBtn, &QPushButton::clicked, this, &SimulatorePage::onSimulate);
    btnRow->addWidget(m_runBtn, 1);

    m_stopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9") + " Stop", this);
    m_stopBtn->setObjectName("DangerBtn");
    m_stopBtn->setMinimumHeight(48);
    m_stopBtn->setVisible(false);
    connect(m_stopBtn, &QPushButton::clicked, m_ai, &AiClient::abort);
    btnRow->addWidget(m_stopBtn);
    vbox->addLayout(btnRow);

    /* Output */
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setObjectName("OutputEdit");
    m_output->setPlaceholderText("Seleziona algoritmo, inserisci dati e premi Simula...");
    {
        QFont f;
        f.setFamily("monospace");
        f.setPointSize(10);
        m_output->setFont(f);
    }
    QScroller::grabGesture(m_output->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_output, 1);

    /* Status */
    m_statusLbl = new QLabel("", this);
    m_statusLbl->setObjectName("StatusLabel");
    vbox->addWidget(m_statusLbl);

    connect(m_algoCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SimulatorePage::onAlgoChanged);
    connect(m_ai, &AiClient::token,    this, &SimulatorePage::onToken);
    connect(m_ai, &AiClient::finished, this, &SimulatorePage::onFinished);
    connect(m_ai, &AiClient::error,    this, &SimulatorePage::onAiError);
    connect(m_ai, &AiClient::aborted,  this, &SimulatorePage::onAborted);
}

void SimulatorePage::onAlgoChanged(int idx)
{
    if (idx < 0 || idx >= kAlgoCount) return;
    m_inputHint->setText(kAlgos[idx].hint);
    m_inputEdit->setPlaceholderText(kAlgos[idx].example);
    m_inputEdit->clear();
    m_output->clear();
    m_statusLbl->setText("");
}

void SimulatorePage::onSimulate()
{
    const int    algoIdx = m_algoCombo->currentData().toInt();
    const QString data   = m_inputEdit->toPlainText().trimmed();
    if (data.isEmpty()) {
        m_inputEdit->setPlainText(kAlgos[algoIdx].example);
        return;
    }
    setBusy(true);
    m_output->clear();
    m_ai->chat(kAlgos[algoIdx].sysPrompt,
               "Input: " + data);
}

void SimulatorePage::onToken(const QString& t)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(t);
    m_output->moveCursor(QTextCursor::End);
}

void SimulatorePage::onFinished(const QString&)
{
    setBusy(false);
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9c\x85") + " Simulazione completata.");
}

void SimulatorePage::onAiError(const QString& msg)
{
    setBusy(false);
    m_statusLbl->setText(QString::fromUtf8("\xe2\x9d\x8c") + " " + msg);
}

void SimulatorePage::onAborted()
{
    setBusy(false);
    m_statusLbl->setText("Simulazione interrotta.");
}

void SimulatorePage::setBusy(bool busy)
{
    m_runBtn->setEnabled(!busy);
    m_stopBtn->setVisible(busy);
    m_inputEdit->setEnabled(!busy);
    m_algoCombo->setEnabled(!busy);
    if (busy) {
        const int idx = m_algoCombo->currentData().toInt();
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x8f\xb3") + " Simulando "
            + kAlgos[idx].name + "...");
    }
}

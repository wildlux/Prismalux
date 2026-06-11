#pragma once
#include <QWidget>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   SimulatorePage — Visualizzatore algoritmi passo per passo.

   Algoritmi: Bubble/Insertion/Selection/Merge/Quick Sort,
              Binary Search, DFS, BFS, Dijkstra, Knapsack.
   L'AI simula ogni passo con stato array visivo ASCII.
   ══════════════════════════════════════════════════════════════ */
class SimulatorePage : public QWidget {
    Q_OBJECT
public:
    explicit SimulatorePage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onAlgoChanged(int idx);
    void onSimulate();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void onAborted();
    void setBusy(bool busy);

private:
    AiClient*    m_ai         = nullptr;
    QComboBox*   m_algoCombo  = nullptr;
    QTextEdit*   m_inputEdit  = nullptr;
    QLabel*      m_inputHint  = nullptr;
    QTextEdit*   m_output     = nullptr;
    QPushButton* m_runBtn     = nullptr;
    QPushButton* m_stopBtn    = nullptr;
    QLabel*      m_statusLbl  = nullptr;
};

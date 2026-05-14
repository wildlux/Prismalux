#pragma once
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   StudioPage — Assistente studio AI per CCNA e tutte le materie.

   Modalità:
     📖 Spiega    — spiegazione progressiva con esempi
     ❓ Quiz       — domande a risposta multipla (con soluzione)
     🔧 Esercizio — esercizio pratico step-by-step
     💡 Flashcard — coppia domanda/risposta da memorizzare
     🗺  Mappa     — mappa concettuale testuale
     📝 Riassumi  — riassume testo incollato nell'input
   ══════════════════════════════════════════════════════════════ */
class StudioPage : public QWidget {
    Q_OBJECT
public:
    explicit StudioPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onMateriaChanged(int idx);
    void onModeClicked(int modeIdx);
    void onNextClicked();
    void onToken(const QString& t);
    void onFinished(const QString& f);
    void onError(const QString& e);

private:
    void runStudy(int modeIdx);
    QString buildSystemPrompt(int materiaIdx, int modeIdx) const;
    QString materiaName() const;

    AiClient* m_ai = nullptr;

    QComboBox*    m_matCombo     = nullptr;
    QLineEdit*    m_materiaEdit  = nullptr;  ///< visibile solo per "Altro"
    QLineEdit*    m_argoEdit     = nullptr;  ///< argomento specifico
    QTextEdit*    m_inputArea    = nullptr;  ///< testo da riassumere (solo modalità Riassumi)
    QLabel*       m_inputLbl     = nullptr;  ///< etichetta input area
    QPushButton*  m_btnNext      = nullptr;  ///< "Prossima domanda / Prossimo"
    QPushButton*  m_btnStop      = nullptr;
    QLabel*       m_statusLbl    = nullptr;
    QProgressBar* m_progress     = nullptr;
    QTextEdit*    m_output       = nullptr;

    int  m_lastMode    = -1;   ///< ultima modalità usata (per "Prossimo")
    bool m_busy        = false;
};

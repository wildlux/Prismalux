#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include "../ai_client.h"
#include "../graph_memory_mobile.h"

/* ══════════════════════════════════════════════════════════════
   HermesPage — Memoria utente persistente con agente estrattore AI.

   Layout:
     Barra azioni: Salva / Riflessione / Cancella / Stop
     QTextEdit editor memoria (editabile + scrollabile)
     Campo input + pulsante Estrai e Salva
     Label status
   ══════════════════════════════════════════════════════════════ */
class HermesPage : public QWidget {
    Q_OBJECT
public:
    explicit HermesPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onSaveClicked();
    void onExtractClicked();
    void onReflectClicked();
    void onClearClicked();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void onAborted();
    void setBusy(bool busy);

private:
    void loadMemory();
    void saveMemory(const QString& content);
    static QString memoryPath();

    enum class Mode { Idle, Extracting, Reflecting };

    AiClient*          m_ai          = nullptr;
    GraphMemoryMobile* m_gm          = nullptr;
    QTextEdit*         m_memEditor   = nullptr;
    QTextEdit*         m_inputEdit   = nullptr;
    QPushButton*       m_saveBtn     = nullptr;
    QPushButton*       m_reflectBtn  = nullptr;
    QPushButton*       m_extractBtn  = nullptr;
    QPushButton*       m_stopBtn     = nullptr;
    QLabel*            m_statusLbl   = nullptr;

    Mode    m_mode    = Mode::Idle;
    QString m_aiAccum;
};

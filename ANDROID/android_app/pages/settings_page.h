#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSlider>

class AiClient;
class RagEngineSimple;

/* ══════════════════════════════════════════════════════════════
   SettingsPage — configurazione server Ollama, modello e RAG.

   Sezioni:
   ┌─────────────────────────────┐
   │  🌐 Server Ollama           │  IP + Porta + Test connessione
   │  🤖 Modello LLM             │  ComboBox modelli + Aggiorna
   │  🎛️ Parametri AI            │  Temperatura + NumPredict
   │  📚 RAG Documenti           │  Path cartella + Carica
   └─────────────────────────────┘
   ══════════════════════════════════════════════════════════════ */
class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(AiClient* ai, RagEngineSimple* rag, QWidget* parent = nullptr);

signals:
    /** Emesso quando host/porta/modello cambiano — collegato a AiClient::setServer. */
    void serverChanged(const QString& host, int port, const QString& model);
    /** Emesso quando viene caricato un nuovo indice RAG. */
    void ragIndexChanged(const QString& path);

private slots:
    void onApplyServer();
    void onTestConnection();
    void onRefreshModels();
    void onModelsReady(const QStringList& models);
    void onLoadRag();

private:
    QWidget* buildSection(const QString& title, QWidget* parent);

    AiClient*        m_ai;
    RagEngineSimple* m_rag;

    /* Server */
    QLineEdit*   m_hostEdit    = nullptr;
    QSpinBox*    m_portSpin    = nullptr;
    QPushButton* m_applyBtn    = nullptr;
    QPushButton* m_testBtn     = nullptr;
    QLabel*      m_connStatus  = nullptr;

    /* Modello */
    QComboBox*   m_modelCombo  = nullptr;
    QPushButton* m_refreshBtn  = nullptr;
    QLabel*      m_modelStatus = nullptr;

    /* Parametri AI */
    QSlider*     m_tempSlider  = nullptr;
    QLabel*      m_tempLbl     = nullptr;
    QSpinBox*    m_predictSpin = nullptr;

    /* RAG */
    QLineEdit*   m_ragPathEdit = nullptr;
    QPushButton* m_ragLoadBtn  = nullptr;
    QLabel*      m_ragStatus   = nullptr;
};

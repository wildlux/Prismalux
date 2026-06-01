#pragma once
/* ═══════════════════════════════════════════════════════════════════
   ModelComboBox — selettore LLM riusabile (header-only, Q_OBJECT)

   Uso:
       #include "../widgets/model_combo_box.h"
       auto* combo = new ModelComboBox(m_ai, this);
       combo->setToolTip("...");
       QString model = combo->selectedModel();   // leggi la selezione

   Comportamento automatico:
   - Mostra il modello corrente come placeholder prima del fetch.
   - Chiama AiClient::fetchModels() e riempie la lista con icone
     (P::modelIcon) e UserRole = nome modello puro.
   - Segue AiClient::modelChanged: aggiorna la selezione se il modello
     globale cambia dall'header/Impostazioni.
   - Se il backend non è raggiungibile mostra "⚠ messaggio errore".
   ═══════════════════════════════════════════════════════════════════ */
#include <QComboBox>
#include <QObject>
#include "../ai_client.h"
#include "../prismalux_paths.h"
#include "../dpi_utils.h"

class ModelComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit ModelComboBox(AiClient* ai, QWidget* parent = nullptr)
        : QComboBox(parent), m_ai(ai)
    {
        setObjectName("settingCombo");
        setMinimumWidth(dpiScale(180));

        if (!m_ai) return;

        /* Mostra subito il modello corrente come placeholder */
        const QString cur = m_ai->model();
        if (!cur.isEmpty())
            addItem(PrismaluxPaths::modelIcon(0, cur) + cur, cur);

        /* Aggiorna la selezione quando il modello globale cambia */
        connect(m_ai, &AiClient::modelChanged, this,
                [this](const QString& m) {
                    const int idx = findData(m);
                    if (idx >= 0) setCurrentIndex(idx);
                });

        refresh();
    }

    /** Modello selezionato (corrispondente a UserRole data). */
    QString selectedModel() const { return currentData().toString(); }

    /** Seleziona programmaticamente un modello per nome. */
    void setSelectedModel(const QString& model)
    {
        const int idx = findData(model);
        if (idx >= 0) setCurrentIndex(idx);
    }

    /** Ricarica la lista dal backend (es. dopo avvio Ollama). */
    void refresh()
    {
        if (!m_ai) return;
        delete m_fetchHolder;
        m_fetchHolder = new QObject(this);

        connect(m_ai, &AiClient::modelsReady, m_fetchHolder,
                [this](const QStringList& models) {
                    delete m_fetchHolder;
                    m_fetchHolder = nullptr;
                    fillFromList(models);
                });

        connect(m_ai, &AiClient::error, m_fetchHolder,
                [this](const QString& msg) {
                    delete m_fetchHolder;
                    m_fetchHolder = nullptr;
                    if (count() <= 1 && (count() == 0 || itemData(0).toString().isEmpty()))
                        addItem("\xe2\x9a\xa0  " + msg.left(60), QString());  /* ⚠ */
                });

        m_ai->fetchModels();
    }

private:
    AiClient* m_ai          = nullptr;
    QObject*  m_fetchHolder = nullptr;

    void fillFromList(const QStringList& models)
    {
        if (!m_ai) return;
        const QString prev = selectedModel();
        const QString cur  = m_ai->model();
        blockSignals(true);
        clear();
        for (const QString& m : models) {
            const qint64 sz = m_ai->modelSizeBytes(m);
            addItem(PrismaluxPaths::modelIcon(sz, m) + m, m);
        }
        /* Ripristina selezione: prima scelta > modello globale > primo */
        int idx = prev.isEmpty() ? -1 : findData(prev);
        if (idx < 0 && !cur.isEmpty()) idx = findData(cur);
        if (idx < 0 && count() > 0)    idx = 0;
        if (idx >= 0) setCurrentIndex(idx);
        blockSignals(false);
    }
};

#pragma once
#include <QWidget>
#include <QVector>
#include <QMap>
#include <QPointer>
#include "../ai_client.h"

class QLineEdit;
class QSpinBox;
class QListWidget;
class QComboBox;
class QTextEdit;
class QPlainTextEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QTabWidget;

/* Singola voce del dataset distillato. */
struct DistEntry {
    QString domanda;
    QMap<QString,QString> risposte;  // modello → risposta grezza
    QString migliorRisposta;
    QString modelloMiglior;
};

/* ══════════════════════════════════════════════════════════════
   DistillazionePage — Distillazione sintetica da LLM locali

   Flusso:
     1. Utente specifica dominio + n. domande + modelli
     2. Un LLM genera le domande (o l'utente le inserisce)
     3. Ogni modello selezionato risponde a ogni domanda
     4. Il LLM giudice sceglie la risposta migliore
     5. Il dataset JSONL viene esportato (formato Alpaca/unsloth)

   Usa QNetworkAccessManager diretto (porta kOllamaPort) per non
   interferire con il modello globale dell'AiClient principale.
   ══════════════════════════════════════════════════════════════ */
class DistillazionePage : public QWidget {
    Q_OBJECT
public:
    explicit DistillazionePage(AiClient* ai, QWidget* parent = nullptr);

private:
    QWidget* buildConfigPanel();
    QWidget* buildOutputPanel();

    enum class Fase { Idle, GenerandoDomande, Interrogando, Valutando, Fatto };
    void setFase(Fase f);
    void appendLog(const QString& msg);
    void processNextInterrogazione();
    void processNextValutazione();
    void parsaDomande(const QString& raw);
    void finalizzaEntry(const QString& sceltaGiudice);
    void aggiornaPreviewDataset();
    void callOllama(const QString& model, const QString& sys, const QString& user);

private slots:
    void onGeneraDomandeClicked();
    void onInterrogaModelliClicked();
    void onEsportaDatasetClicked();
    void onAbortClicked();
    void onFetchModelsReady(const QStringList& models);
    void onFetchModelsError(const QString& err);
    void onReplyFinished();

private:
    AiClient*              m_ai            = nullptr;
    QNetworkAccessManager* m_nam           = nullptr;
    QPointer<QNetworkReply> m_pendingReply;

    /* ── Config UI ── */
    QLineEdit*      m_dominioEdit    = nullptr;
    QSpinBox*       m_nDomandeSpn    = nullptr;
    QListWidget*    m_modelliList    = nullptr;
    QComboBox*      m_giudiceCombo   = nullptr;
    QPlainTextEdit* m_domandeEdit    = nullptr;
    QPushButton*    m_btnGenera      = nullptr;
    QPushButton*    m_btnInterroga   = nullptr;
    QPushButton*    m_btnEsporta     = nullptr;
    QPushButton*    m_btnAbort       = nullptr;
    QLabel*         m_statusLabel    = nullptr;

    /* ── Output UI ── */
    QTabWidget*   m_outTabs          = nullptr;
    QProgressBar* m_progress         = nullptr;
    QTextEdit*    m_log              = nullptr;
    QTextEdit*    m_datasetPreview   = nullptr;

    /* ── Stato ── */
    Fase        m_fase               = Fase::Idle;
    QStringList m_domande;
    QVector<DistEntry> m_dataset;

    /* ── Stato interrogazione ── */
    int         m_iDomanda           = 0;
    int         m_iModello           = 0;
    QStringList m_modelliSelezionati;
    DistEntry   m_entryCorrente;

    enum class AttesaTipo { Domande, Risposta, Valutazione };
    AttesaTipo  m_attesa             = AttesaTipo::Domande;
};

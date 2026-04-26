#pragma once
#include <QWidget>
#include <QDialog>
#include "../ai_client.h"
#include "../hardware_monitor.h"
#include "../monitor_panel.h"
#include "../rag_engine.h"

class PersonalizzaPage;
class ManutenzioneePage;
class GraficoCanvas;
class QTableWidget;
class QListWidget;

/* ══════════════════════════════════════════════════════════════
   ImpostazioniPage — Personalizzazione + Manutenzione
   Layout flat: QTabWidget con 2 tab, nessun menu intermedio.
   ══════════════════════════════════════════════════════════════ */
class ImpostazioniPage : public QWidget {
    Q_OBJECT
public:
    explicit ImpostazioniPage(AiClient* ai, HardwareMonitor* hw, QWidget* parent = nullptr);
    void onHWReady(HWInfo hw);

    /** Porta in primo piano il tab il cui titolo contiene @p name (case-insensitive) */
    void switchToTab(const QString& name);

    /** Aggiunge il tab "Grafico" con i controlli di posizione degli assi.
     *  Deve essere chiamato DOPO la costruzione, quando il canvas è disponibile. */
    void setGraficoCanvas(GraficoCanvas* canvas);

    /** Percorso del binario Piper installato localmente (o stringa vuota se assente). */
    static QString piperBinPath();
    /** Cartella voci ~/.prismalux/piper/voices/ */
    static QString piperVoicesDir();
    /** Voce attiva salvata dall'utente (nome file ONNX senza estensione). */
    static QString piperActiveVoice();
    /** Salva la voce attiva. */
    static void    savePiperActiveVoice(const QString& name);

signals:
    /** Emesso quando l'utente cambia la modalità etichette tab (in tempo reale). */
    void tabModeChanged(const QString& mode);
    /** Emesso quando l'utente cambia lo stile di navigazione (in tempo reale). */
    void navStyleChanged(const QString& style);
    /** Emesso quando l'utente cambia la modalità dei pulsanti di esecuzione. */
    void execBtnModeChanged(const QString& mode);
    /** Emesso ogni chunk durante l'indicizzazione (per progress bar in MainWindow). */
    void indexingProgress(int done, int total);
    /** Emesso al completamento o all'interruzione dell'indicizzazione. */
    void indexingFinished(int n, bool aborted);

private:
    QWidget* buildTemaTab();
    QWidget* buildTestTab();
    QWidget* buildVoceTab();
    QWidget* buildTrascriviTab();
    QWidget* buildGraficoTab(GraficoCanvas* canvas);
    QWidget* buildAiLocaleTab();
    QWidget* buildRagTab();
    QWidget* buildDipendenzeTab();
    QWidget* buildLlmConsigliatiTab();
    QWidget* buildLlmClassificaTab();  ///< ranking oggettivo open-weight (ArtificialAnalysis + benchmark locali)
    QWidget* buildAiParamsTab();   ///< parametri anti-allucinazione + preferenze modello
    QWidget* buildPuliziaTab();       ///< pulizia file temporanei, build, cache
    QWidget* buildRingraziamentiTab(); ///< licenza MIT + crediti autore e librerie

    PersonalizzaPage*  m_personalizza  = nullptr;
    ManutenzioneePage* m_manutenzione  = nullptr;
    QTabWidget*        m_tabs          = nullptr;
    QTabWidget*        m_tabAiLocale   = nullptr;  ///< inner tab: Connessione/HW/RAG/Voce/…
    QTabWidget*        m_tabLlm        = nullptr;  ///< inner tab: LLM/Classifica/Test
    QTabWidget*        m_tabVisuale    = nullptr;  ///< inner tab: Aspetto + Grafico (dinamico)
    QTabWidget*        m_tabSistema    = nullptr;  ///< inner tab: Pulizia/BugTracker/Cron
    AiClient*          m_ai            = nullptr;

    /* RAG indexing state (usato da buildRagTab) */
    RagEngine       m_rag;
    QStringList     m_ragQueue;           ///< chunk da indicizzare
    QStringList     m_ragQueueSource;     ///< nome file sorgente per ogni chunk (parallelo a m_ragQueue)
    int             m_ragQueuePos = 0;    ///< posizione corrente nel queue
    QLabel*         m_ragFeedbackLbl = nullptr;
    bool            m_ragNoSave      = false;  ///< se true, non salva l'indice su disco (M2)
    QPushButton*    m_btnStopIndex   = nullptr; ///< "Ferma indicizzazione" — abilitato durante indexing
    bool            m_indexAborted   = false;   ///< flag settato da "Ferma" per interrompere indexNext

    /* Hardware snapshot — aggiornato da onHWReady() */
    HWInfo          m_hwInfo         = {};
    QTableWidget*   m_rankTable      = nullptr;   ///< ptr tabella classifica (per refreshLlmColors)
    QListWidget*    m_consigliatiList = nullptr;   ///< ptr lista consigliati (per refreshLlmColors)

    void refreshLlmColors();  ///< colora verde/giallo/rosso i modelli in base all'HW rilevato
};

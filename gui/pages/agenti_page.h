#pragma once
#include <functional>
#include <QWidget>
#include <QFrame>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTextCursor>
#include <QMap>
#include <QSet>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QVector>
#include <QPointF>
#include <QStack>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "../ai_client.h"

class ChartWidget;  /* forward declare — chart_widget.h incluso in .cpp */

/* Forward declare — implementazione in agents_config_dialog.h */
class AgentsConfigDialog;

/* ══════════════════════════════════════════════════════════════
   AgentiPage — Pipeline configurabile + Motore Byzantino
   Layout stile ChatGPT: output grande, input in basso, ⚙️ dialog.
   ══════════════════════════════════════════════════════════════ */
class AgentiPage : public QWidget {
    Q_OBJECT
public:
    explicit AgentiPage(AiClient* ai, QWidget* parent = nullptr);

    /* ── Metodi statici pubblici: usati anche da mainwindow (migrazione chat storiche) ── */
    static QString buildUserBubble(const QString& text, int bubbleIdx = -1);
    static QString buildAgentBubble(const QString& label, const QString& model,
                                    const QString& time,  const QString& htmlContent,
                                    int bubbleIdx = -1,
                                    const QString& thinkContent = "");
    static QString buildLocalBubble(const QString& result, double ms, int bubbleIdx = -1,
                                    const QString& extraLinks = "");
    static QString markdownToHtml(const QString& md);

    /* ── Utility testabili (pubbliche per unit test e futuro refactor A2) ── */
    /** Estrae il primo blocco ```python...``` dall'output dell'agente */
    static QString extractPythonCode(const QString& text);
    /** Corregge bug tipici nel codice Python generato dall'AI (__name__ guard, ecc.) */
    static QString _sanitizePyCode(const QString& code);

signals:
    /** Emesso quando una pipeline/Byzantine/MathTheory completa — per salvare la chat */
    void chatCompleted(const QString& title, const QString& logHtml);
    /**
     * pipelineStatus — aggiorna la barra progresso nella status bar di MainWindow.
     * @param pct   0-100 = valore; -1 = nascondi/resetta
     * @param text  testo da mostrare nella status bar (vuoto = non aggiornare il testo)
     */
    void pipelineStatus(int pct, const QString& text);
    /** Chiede a MainWindow di aprire le Impostazioni sul tab indicato (es. "trascrivi") */
    void requestOpenSettings(const QString& tabName);
    /** Chiede a MainWindow di mostrare un grafico nel tab Grafico (Alt+3).
     *  Se @p formula è non vuota → grafico cartesiano y=f(x) su [xMin, xMax].
     *  Se @p formula è vuota e @p points non è vuoto → scatter di punti. */
    void requestShowInGrafico(QString formula, double xMin, double xMax,
                              QVector<QPointF> points);

private:
    AiClient*           m_ai;
    AgentsConfigDialog* m_cfgDlg    = nullptr;  ///< Dialog configurazione agenti

    /* ── Widget interfaccia principale ── */
    QTextBrowser* m_log       = nullptr;  ///< log conversazione (HTML, QTextBrowser per link click)
    bool          m_userScrolled       = false;  ///< true se l'utente ha scrollato su durante streaming
    bool          m_suppressScrollSig  = false;  ///< sopprime il segnale valueChanged durante auto-scroll
    QMap<int,QString> m_bubbleTexts;      ///< testo plain indicizzato per copia/TTS
    QMap<int,QString> m_thinkTexts;       ///< testo reasoning <think>...</think> per bolla
    QSet<int>         m_thinkShown;       ///< bolle con reasoning visibile
    int           m_bubbleIdx = 0;        ///< contatore bolle corrente
    QTextEdit*    m_input     = nullptr;
    QPushButton*  m_btnRun        = nullptr;  ///< Pulsante unico: run (idle) ↔ stop (busy)
    bool          m_modePipeline  = false;    ///< false=Singolo (default), true=Avvia pipeline
    QPushButton*  m_btnCfg        = nullptr;   ///< Apre dialog config agenti
    QPushButton*  m_btnModeToggle = nullptr;   ///< Toggle Mono-Agente / Multi-Agente
    QWidget*      m_multiAgentBar = nullptr;   ///< Container controlli pipeline (nascosto in Mono)
    QPushButton*  m_btnTranslate = nullptr;   ///< Apre dialog traduzione
    QFrame*       m_symbolsPanel = nullptr;   ///< Pannello inline caratteri speciali (toggle)
    QComboBox*    m_cmbMode   = nullptr;
    QCheckBox*    m_chkController = nullptr; ///< Abilita/disabilita il Controller LLM post-agente
    QLabel*       m_autoLbl   = nullptr;   ///< Stato auto-assegnazione / preset
    QLabel*       m_waitLbl   = nullptr;   ///< ⏳ durante elaborazione AI
    int           m_maxShots  = 6;
    int           m_tokenCount = 0;

    /* ── Modelli disponibili (Consiglio Scientifico) ── */
    struct ModelInfo { QString name; qint64 size; };
    QVector<ModelInfo> m_modelInfos;

    /* ── Stato pipeline sequenziale ── */
    static constexpr int MAX_AGENTS = 6;
    int              m_currentAgent = 0;
    QVector<QString> m_agentOutputs;
    QString          m_taskOriginal;

    /* ── Stato motore byzantino ── */
    int     m_byzStep = 0;
    QString m_byzA, m_byzC;

    /* ── Modello salvato prima della traduzione (ripristinato dopo) ── */
    QString m_preTranslateModel;

    /* ── Lingue selezionate nel dialog Traduci ── */
    QString m_translateSrcLang;
    QString m_translateDstLang;

    /* ── Stato modalità operative ── */
    enum class OpMode { Idle, Pipeline, PipelineControl, Byzantine, MathTheory, Translating, ConsiglioScientifico };
    OpMode  m_opMode      = OpMode::Idle;
    OpMode  m_pendingMode = OpMode::Idle;  ///< Modalità da eseguire dopo traduzione
    QString m_translateBuf;  ///< Accumulo token traduzione

    /* ── Consiglio Scientifico (query parallela multi-modello) ── */
    struct ConsiglioPeer {
        AiClient* client  = nullptr;
        QString   model;
        double    weight  = 1.0;
        QString   accum;
        bool      done    = false;
    };
    QVector<ConsiglioPeer> m_peers;
    int                    m_peersDone         = 0;
    int                    m_consiglioStrategy = 0;  ///< 0=Pesata, 1=Jaccard, 2=Sintesi

    /* ── ID sessione corrente (per ChatHistory) ── */
    QString m_sessionId;

    /* ── Stack undo per eliminazione bolle (del:) ── */
    QStack<QString> m_undoHtmlStack;

    /* ── Posizione nel documento per la sostituzione post-streaming ── */
    int     m_agentBlockStart = 0; ///< prima dell'intestazione agente (include header)
    int     m_agentTextStart  = 0; ///< dopo l'intestazione (per inserimento token)
    int     m_ctrlBlockStart  = 0; ///< posizione per la bolla del controller

    /* ── Metadata agente corrente (usati per costruire la bolla) ── */
    QString      m_currentAgentLabel; ///< "🛸 Agente 1 — Ricercatore"
    QString      m_currentAgentModel;
    QString      m_currentAgentTime;
    QElapsedTimer m_agentTimer;       ///< misura il tempo di risposta di ogni agente

    /* ── Tool executor + Controller LLM ── */
    QString   m_executorOutput;      ///< stdout/stderr dell'esecutore Python
    QString   m_ctrlAccum;           ///< accumulo token controller LLM
    QProcess* m_execProc = nullptr;  ///< processo esecutore corrente

    /* ── TTS — processo tracciato per stop/pausa ── */
    QProcess*    m_ttsProc    = nullptr;  ///< aplay (Linux) o PowerShell (Win)
    QProcess*    m_piperProc  = nullptr;  ///< piper TTS (solo Linux, pipe verso m_ttsProc)
    QPushButton* m_btnTtsStop  = nullptr;
    QPushButton* m_btnTtsPause = nullptr;  ///< pausa/riprendi lettura
    bool         m_ttsPaused   = false;    ///< true = lettura in pausa

    /* ── STT — pulsante + processi tracciati ── */
    QPushButton* m_btnVoice   = nullptr;  ///< pulsante Trascrivi voce (testo cambia in-place)
    QProcess*    m_recProc    = nullptr;  ///< arecord
    QProcess*    m_sttProc    = nullptr;  ///< whisper-cli
    enum class SttState { Idle, Recording, Transcribing, Downloading } m_sttState = SttState::Idle;

    /** Scarica ggml-small.bin in ~/.prismalux/whisper/models/ con progress nella chat */
    void downloadWhisperModel();
    /** Avvia registrazione audio + trascrizione whisper.cpp */
    void _sttStartRecording();
    /** Avvia TTS tracciato con feedback "Avvio lettura..." (Piper → espeak-ng → spd-say) */
    void _ttsPlay(const QString& tts);

    /* ── Lettore Documenti ── */
    QPushButton* m_btnDoc    = nullptr;
    QString      m_docContext;              ///< Testo estratto dal documento allegato

    /* ── Analizzatore Immagini ── */
    QPushButton* m_btnImg    = nullptr;
    QByteArray   m_imgBase64;  ///< Base64 dell'immagine allegata
    QString      m_imgMime;

    /* ── Drag & Drop file su m_input ── */
    /** Dispatcher per file trascinato: PDF, Excel, audio, immagine, testo */
    void loadDroppedFile(const QString& filePath);
    /** Converte file audio in testo (voce via whisper, o note musicali via aubionotes) */
    void _loadAudioAsText(const QString& filePath);
    void _extractPdfPython(const QString& filePath,
                            std::function<void(const QString&)> onText);
    void _extractXlsPython(const QString& filePath,
                            std::function<void(const QString&)> onText);

    /* ── Selettore LLM principale (toolbar) ── */
    QComboBox*   m_cmbLLM    = nullptr;  ///< Selettore LLM singolo nella toolbar
    QString      m_pageModel;            ///< Modello preferito per questa scheda (privato)

    /* ── Pannello grafico (appare quando l'AI restituisce una formula) ── */
    QWidget*     m_chartPanel    = nullptr;
    QPushButton* m_btnChartOpen  = nullptr;  ///< "Apri nel Grafico" nel panel inline
    QString      m_lastChartExpr;
    double       m_lastChartXMin = -10.0;
    double       m_lastChartXMax =  10.0;
    QVector<QPointF> m_lastChartPts;

    /**
     * checkRam() — Controlla la RAM disponibile prima di avviare una pipeline.
     * @return true  se si può procedere (< 75% usato, o utente ha confermato),
     *         false se RAM critica (>= 92%) o utente ha annullato.
     */
    bool checkRam();
    /** Avvisa se il modello selezionato pesa più del 70% della RAM libera.
     *  @return true se si può procedere, false se l'utente ha annullato. */
    bool checkModelSize(const QString& model);

    void _setRunBusy(bool busy);
    void setupUI();
    void runPipeline();
    void runByzantine();
    void runMathTheory();
    void runConsiglioScientifico();
    void aggregaConsiglio();
    static double jaccardSim(const QString& a, const QString& b);
    void runAgent(int idx);
    void advancePipeline();

    /** Guardia matematica locale — ritorna risultato se gestita, "" altrimenti */
    static QString guardiaMath(const QString& input);

    /** Bolla risposta locale (0 token — calcolo math) — pubblica sopra */

    /** Striscia tool executor: codice + output + exit code */
    static QString buildToolStrip(const QString& code, const QString& output,
                                  int exitCode, double ms);

    /** Bolla controller LLM: verde/giallo/rosso in base al verdetto */
    static QString buildControllerBubble(const QString& htmlContent);

    /** Avvia il controller LLM dopo l'esecutore */
    void runPipelineController();

    /** Cerca formule o coppie di punti nel testo e mostra il grafico se trovate */
    void tryShowChart(const QString& text);

private slots:
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onError(const QString& msg);
    void onModelsReady(const QStringList& list);
};

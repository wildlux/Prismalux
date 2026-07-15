#pragma once
#include <functional>
#include "../widgets/tri_mode_button.h"
#include "widget_formula_builder.h"
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
#include <QListWidget>
#include <QVector>
#include <QPointF>
#include <QStack>
#include <QElapsedTimer>
#include <QSharedPointer>
#include <QScrollArea>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QGroupBox>
#include "../ai_client.h"
#include "../chat_history.h"
#include "../context_compressor.h"
#include "../graph_memory.h"

class ChartWidget;  /* forward declare — chart_widget.h incluso in .cpp */

/* Forward declare — implementazione in agents_config_dialog.h */
class AgentsConfigDialog;
class RagDropWidget;

/* Forward declare layout classes — usati solo nelle firme dei build* privati */
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;

/* ══════════════════════════════════════════════════════════════
   AgentiPage — Pipeline configurabile + Motore Byzantino
   Layout stile ChatGPT: output grande, input in basso, ⚙️ dialog.
   ══════════════════════════════════════════════════════════════ */
class AgentiPage : public QWidget {
    Q_OBJECT
public:
    explicit AgentiPage(AiClient* ai, QWidget* parent = nullptr);
    ~AgentiPage() override;

    /* ── Metodi statici pubblici: usati anche da mainwindow (migrazione chat storiche) ── */
    static QString buildUserBubble(const QString& text, int bubbleIdx = -1,
                                   const QString& displayHtml = "");
    static QString buildAgentBubble(const QString& label, const QString& model,
                                    const QString& time,  const QString& htmlContent,
                                    int bubbleIdx = -1,
                                    const QString& thinkContent = "");
    static QString buildLocalBubble(const QString& result, double ms, int bubbleIdx = -1,
                                    const QString& extraLinks = "");
    static QString markdownToHtml(const QString& md,
        QMap<int,QPair<QString,QString>>* codeBlocks = nullptr,
        int* codeCounter = nullptr);

    /* ── Utility testabili (pubbliche per unit test e futuro refactor A2) ── */
    /** Guardia matematica locale — ritorna risultato se gestita, "" altrimenti */
    static QString guardiaMath(const QString& input);
    /** Guardia data/ora locale — domanda secca su ora/data → risposta
     *  dall'orologio di sistema (zero LLM, zero web), "" altrimenti */
    static QString guardiaDataOra(const QString& input);
    /** Estrae il primo blocco ```python...``` dall'output dell'agente */
    static QString extractPythonCode(const QString& text);
    /** Corregge bug tipici nel codice Python generato dall'AI (__name__ guard, ecc.) */
    static QString _sanitizePyCode(const QString& code);

    /** Risultato di extractExecutableCode: linguaggio + sorgente */
    struct ExecCode { QString lang; QString code; };
    /** Estrae il primo blocco eseguibile: python/py, c, cpp/c++ */
    static ExecCode extractExecutableCode(const QString& text);

    /* ── Agente Autonomo — pubbliche per testabilità ── */
    /** System prompt ReAct con lista strumenti e regole di formato */
    static QString _autoSystemPrompt();
    /** Genera la card HTML per uno step del ciclo ReAct */
    static QString buildAutoStepHtml(int step, const QString& thought,
                                     const QString& action, const QString& obs);
    /** Cerca il primo oggetto JSON {tool,input} in una risposta dell'AI */
    static QJsonObject detectFirstToolCall(const QString& text);

    /** History multi-turno agente autonomo — per persistenza tra sessioni */
    QJsonArray autoHistory() const { return m_ctxAuto ? m_ctxAuto->buildContext() : QJsonArray{}; }
    void setAutoHistory(const QJsonArray& h) { if (m_ctxAuto) m_ctxAuto->fromJsonArray(h); }

    /** Getter per persistenza sessione */
    const QMap<int,QString>&                    bubbleTexts() const { return m_bubbleTexts; }
    const QMap<int,QPair<QString,QString>>&     codeBlocks()  const { return m_codeBlocks;  }

    /** Termina tutti i processi figli (TTS/STT/exec) prima della chiusura.
     *  Chiamare dal closeEvent di MainWindow prima di ev->accept(). */
    void prepareClose();

    /** Ripristina mappe da sessione salvata e allinea i contatori */
    void loadSessionMaps(const QMap<int,QString>& bt,
                         const QMap<int,QPair<QString,QString>>& cb)
    {
        m_bubbleTexts      = bt;
        m_codeBlocks       = cb;
        m_bubbleIdx        = bt.isEmpty()  ? 0 : bt.lastKey()  + 1;
        m_codeBlockCounter = cb.isEmpty()  ? 0 : cb.lastKey()  + 1;
    }

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
    /** Emesso quando una ricerca online è completata e il file è pronto nel RAG */
    void onlineSearchResultReady(const QString& filePath, const QString& query);
    /** Stima occupazione finestra di contesto dopo ogni turno chat:
     *  usedTok = storia compressa + RAG inline/condiviso (~4 char/token),
     *  maxTok = num_ctx corrente. Per l'indicatore 🧠 nell'header. */
    void contextUsage(int usedTok, int maxTok);
    /** Chiede a MainWindow di mostrare un grafico nel tab Grafico (Alt+3).
     *  Se @p formula è non vuota → grafico cartesiano y=f(x) su [xMin, xMax].
     *  Se @p formula è vuota e @p points non è vuoto → scatter di punti. */
    void requestShowInGrafico(QString formula, double xMin, double xMax,
                              QVector<QPointF> points);

private:
    AiClient*           m_ai;
    AgentsConfigDialog* m_cfgDlg    = nullptr;  ///< Dialog configurazione agenti
    int                 m_spawnedAgents = 0;    ///< sub-agenti spawned in questa sessione (max 4)

    /* ── Widget interfaccia principale ── */
    QTextBrowser* m_log       = nullptr;  ///< log conversazione (HTML, QTextBrowser per link click)
    bool          m_userScrolled       = false;  ///< true se l'utente ha scrollato su durante streaming
    bool          m_suppressScrollSig  = false;  ///< sopprime il segnale valueChanged durante auto-scroll
    QMap<int,QString> m_bubbleTexts;      ///< testo plain indicizzato per copia/TTS
    QMap<int,QString> m_thinkTexts;       ///< testo reasoning estratto per la bolla (sempre visibile)
    AiClient*         m_translateAi = nullptr; ///< client secondario per traduzione LLM post-processing
    QStringList       m_hermesLastSources; ///< etichette nodi Hermes usati nell'ultima risposta
    QString           m_taskHtml;          ///< HTML leggero bolla utente (da extractInputHtml)
    QMap<int,QPair<QString,QString>> m_codeBlocks; ///< id → {lang, testo grezzo} per Copia/Salva
    int               m_codeBlockCounter = 0;      ///< contatore globale blocchi codice
    QMap<int,QString> m_pendingExecCodes;           ///< id → codice Python rifiutato, per Riesegui
    int           m_bubbleIdx = 0;        ///< contatore bolle corrente
    QTextEdit*    m_input     = nullptr;
    QPushButton*  m_btnRun        = nullptr;  ///< Pulsante unico: run (idle) ↔ stop (busy)
    bool          m_modePipeline  = false;    ///< false=Chat/Autonomo, true=Pipeline backend
    class TriModeButton* m_modeBtn = nullptr;   ///< Cerchio 3 settori: Chat / Agentico / Conversa
    QPushButton*  m_btnTranslate   = nullptr;  ///< mantenuto per compatibilità slot esistenti
    QPushButton*  m_btnKnowledge   = nullptr;  ///< Salva risposta in user_knowledge.md (P4)
    QPushButton*  m_btnEtimo       = nullptr;  ///< Toggle modalita' dizionario etimologico
    QFrame*       m_fmtBar        = nullptr;  ///< Toolbar formattazione inline (appare su selezione)
    QPushButton*  m_btnFmtFg     = nullptr;  ///< Pulsante colore testo
    QPushButton*  m_btnFmtBg     = nullptr;  ///< Pulsante colore sfondo
    QColor        m_fmtFgColor   = QColor(220, 38, 38);   ///< Colore testo corrente (rosso default)
    QColor        m_fmtBgColor   = QColor(250, 204, 21);  ///< Colore sfondo corrente (giallo default)
    QLineEdit*    m_symbolSearch  = nullptr;  ///< Campo ricerca nel pannello Simboli
    QWidget*      m_symbolSearchPanel = nullptr; ///< Pannello risultati ricerca simboli
    QGridLayout*  m_symbolSearchGrid  = nullptr; ///< Grid pulsanti risultati ricerca
    QVector<QPair<QString,QString>> m_allSymbols; ///< (simbolo, chiave ricerca) per tutti i simboli
    /* Pannello formule matematiche LaTeX */
    QPushButton*  m_btnMathToggle  = nullptr;  ///< Toggle pannello formule matematiche
    QWidget*              m_mathPanel        = nullptr;
    QTimer*               m_mathPreviewTimer = nullptr;
    QWidget*              m_mathPreview      = nullptr;  ///< LatexView* — QWidget* per non include WebEngine nell'header
    FormulaBuilderWidget* m_formulaBuilder   = nullptr;  ///< drag-and-drop formula builder
    /* ── Thunk "query normalizzata" ── */
    QMap<int, QString>   m_thunkTexts;    ///< testo normalizzato per ogni thunk nel log
    QSet<int>            m_thunkOpen;     ///< indici thunk attualmente espansi
    int                  m_thunkIdx = 0; ///< contatore thunk (monotono)
    /* LatexView forward-declared per evitare dipendenza WebEngine nell'header */
    QWidget*      m_hintWidget     = nullptr;  ///< Footer suggerimenti (nascondibile)
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
    bool             m_autoRetryActive = false; /* guardia anti-loop incertezza LLM */
    QString          m_autoRetrySearchResults;  /* risultati web del retry → salvataggio RAG */

    /* ── History conversazione (solo modalita' chat singola) ── */
    /* ContextCompressor gestisce la finestra scorrevole + summary LLM asincrono
       (Headroom-style). Sostituisce m_chatPairs + troncamento hard.            */
    ContextCompressor* m_ctxSingle = nullptr;

    /* ── Stato motore byzantino ── */
    int     m_byzStep = 0;
    QString m_byzA, m_byzC;

    /* ── Modello salvato prima della traduzione (ripristinato dopo) ── */
    QString m_preTranslateModel;

    /* ── Lingue selezionate nel dialog Traduci ── */
    QString m_translateSrcLang;
    QString m_translateDstLang;

    /* ── Stato modalità operative ── */
    enum class OpMode { Idle, Pipeline, PipelineControl, Byzantine, MathTheory, Translating, ConsiglioScientifico, KnowledgeExtract, AutonomousAgent };
    OpMode  m_opMode      = OpMode::Idle;
    OpMode  m_pendingMode = OpMode::Idle;  ///< Modalità da eseguire dopo traduzione
    QString m_translateBuf;     ///< Accumulo token traduzione
    QString m_knowledgeBuf;     ///< Accumulo token estrattore (non mostrato nel log)
    int     m_singleChatTurns = 0; ///< Contatore scambi in modalità singolo agente

    static constexpr int kChatExtractEvery = 4; ///< Ogni N scambi in singolo → estrazione knowledge

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

    /* ── Storico chat persistente ── */
    ChatHistory       m_chatHistory;

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
    bool         m_ttftCaptured = false; ///< FEAT-2: TTFT già registrato per l'agente corrente

    /* ── Tool executor + Controller LLM ── */
    QString   m_executorOutput;      ///< stdout/stderr dell'esecutore Python
    QString   m_ctrlAccum;           ///< accumulo token controller LLM
    QProcess* m_execProc = nullptr;  ///< processo esecutore corrente

    /* ── Knowledge MCP watchdog ── */
    QProcess* m_knowledgeProc     = nullptr;  ///< processo MCP knowledge_updater corrente
    QTimer*   m_knowledgeWatchdog = nullptr;  ///< watchdog 5s per il processo MCP

    /* ── Knowledge Save dialog (onSaveKnowledge) ── */
    QDialog*   m_saveDlg     = nullptr;
    QTextEdit* m_saveDlgEdit = nullptr;
    QComboBox* m_saveDlgSec  = nullptr;
    QComboBox* m_saveDlgMod  = nullptr;
    QLabel*    m_saveDlgHint = nullptr;

    /* ── Whisper download process ── */
    QProcess* m_whisperDlProc    = nullptr;  ///< processo wget/curl per download modello whisper
    QString   m_whisperDlDestDir;            ///< directory di destinazione del download
    QString   m_whisperDlDestFile;           ///< file di destinazione del download

    /* ── TTS — processo tracciato per stop/pausa ── */
    QProcess*    m_ttsProc    = nullptr;  ///< aplay (Linux) o PowerShell (Win)
    QProcess*    m_piperProc  = nullptr;  ///< piper TTS (solo Linux, pipe verso m_ttsProc)
    QPushButton* m_btnTtsStop  = nullptr;
    QPushButton* m_btnTtsPause = nullptr;  ///< pausa/riprendi lettura
    bool         m_ttsPaused   = false;    ///< true = lettura in pausa

    /* ── Conversazione Vocale continua (loop STT → AI → TTS) ── */
    QPushButton* m_btnVoiceLoop    = nullptr;
    bool         m_voiceLoopActive = false;

    /* ── Cache risposte esatte (D-25) — vero per un solo giro dopo il click
       su "🔄 Rigenera" nella bolla cachata, per bypassare la cache e
       richiamare davvero il modello invece di riservire la stessa risposta. */
    bool         m_bypassResponseCache = false;

    /* ── Tool Use ── */
    bool         m_toolsEnabled    = false;
    int          m_toolIteration   = 0;
    QCheckBox*   m_toolChk         = nullptr;  ///< rimosso dalla toolbar, tenuto per compatibilità

    /** Esegue uno strumento (calc/ricerca/python/leggi_file/lista_file/scrivi_file). Async. */
    void runToolCall(const QJsonObject& call, std::function<void(QString)> onDone);
    /* Un metodo onTool<Nome>() per ogni ramo di runToolCall() (D-35, 2026-07-10
       — era un unico if/else-if di 1455 righe). Implementati in
       main_ai_tools_calls.cpp, eccetto onToolMcpCall() che resta in
       main_ai_tools.cpp (dipende da un helper template locale). */
    void onToolCalc(const QString& input, const std::function<void(QString)>& onDone);
    void onToolAlgoritmo(const QString& input, const std::function<void(QString)>& onDone);
    void onToolCodiceFiscale(const QString& input, const std::function<void(QString)>& onDone);
    void onToolFinanzaCalcola(const QString& input, const std::function<void(QString)>& onDone);
    void onToolValidaDocumento(const QString& input, const std::function<void(QString)>& onDone);
    void onToolCartaAstrale(const QString& input, const std::function<void(QString)>& onDone);
    void onToolConverti(const QString& input, const std::function<void(QString)>& onDone);
    void onToolDisegnaGrafico(const QString& input, const std::function<void(QString)>& onDone);
    void onToolRicerca(const QString& input, const std::function<void(QString)>& onDone);
    void onToolCambioValuta(const QString& input, const std::function<void(QString)>& onDone);
    void onToolFetchUrl(const QString& input, const std::function<void(QString)>& onDone);
    void onToolPython(const QString& input, const std::function<void(QString)>& onDone);
    void onToolLeggiFile(const QString& input, const std::function<void(QString)>& onDone);
    void onToolListaFile(const QString& input, const std::function<void(QString)>& onDone);
    void onToolScriviFile(const QString& input, const std::function<void(QString)>& onDone);
    void onToolSearchRag(const QString& input, const std::function<void(QString)>& onDone);
    void onToolGraphMemory(const QString& input, const std::function<void(QString)>& onDone);
    void onToolGetDatetime(const QString& input, const std::function<void(QString)>& onDone);
    void onToolDateCalc(const QString& input, const std::function<void(QString)>& onDone);
    void onToolEventoCalendario(const QString& input, const std::function<void(QString)>& onDone);
    void onToolGetKnowledge(const QString& input, const std::function<void(QString)>& onDone);
    void onToolLeggiRiassunto(const QString& input, const std::function<void(QString)>& onDone);
    void onToolScriviRiassunto(const QString& input, const std::function<void(QString)>& onDone);
    void onToolSpawnAgent(const QString& input, const std::function<void(QString)>& onDone);
    void onToolMcpCall(const QString& input, const std::function<void(QString)>& onDone);
    /** Testo da aggiungere al system prompt quando tool use è attivo */
    static QString toolSystemSuffix();
    /** Esegue tools/list su ogni MCP in background per arricchire il system prompt */
    void startMcpDiscovery();

    /* ── Batching tool calls: raccoglie tutti i tool_calls di un turno prima di eseguirli ── */
    QVector<QPair<QString,QJsonObject>> m_incomingToolBatch;  ///< tool calls ricevuti in questo turno
    bool                                m_toolBatchScheduled = false; ///< true = processToolBatch() già schedulato
    QVector<QPair<QString,QString>>     m_toolBatchResults;  ///< risultati accumulati del batch
    int                                 m_toolBatchTotal = 0; ///< quanti tool_call in questo batch
    int                                 m_toolBatchDone  = 0; ///< quanti risultati ricevuti

    /** Rileva se il messaggio richiede ricerca online (pattern matching, senza LLM). */
    static bool _detectWebIntent(const QString& msg);
    /** Agente di ricerca web: cerca su DuckDuckGo e sintetizza la risposta con l'LLM. */
    void runWebSearchAgent(const QString& query);

    /* ── Pannello Tool Veloci (Function Tools) ── */
    QWidget*       m_toolsPanel     = nullptr;  ///< pannello ⚡ Tool Veloci (Function Tools)
    QPushButton*   m_btnToolsToggle = nullptr;  ///< toggle "⚡ Tool Veloci (N)"
    QSet<QString>  m_enabledTools;              ///< tool Ollama abilitati (tutti di default)

    /* ── Pannello Tool Lenti (MCP) ── */
    QFrame*        m_mcpPanel       = nullptr;  ///< pannello 🔌 Tool Lenti (MCP Plugin)
    QPushButton*   m_btnMcpToggle   = nullptr;  ///< toggle "🔌 Tool Lenti (M)"
    QSet<QString>  m_enabledMcps;               ///< MCP abilitati (tutti di default)

    /** Costruisce l'array tools filtrato per la chat() — sostituisce _buildOllamaTools().
     *  @p query: usata per la pre-selezione per categoria dei tool pesanti (D-29). */
    QJsonArray     buildEnabledTools(const QString& query) const;
    void           buildBottomBar(QVBoxLayout* lay);
    void           buildToolsPanel(QVBoxLayout* lay);
    void           updateToolsBtnLabel();

    /* ── Agente Autonomo (ReAct: Reasoning + Acting loop) ── */
    bool         m_autoEnabled    = false;   ///< true = modalità autonoma attiva
    bool         m_autoMsgShown  = false;   ///< true = banner "attivato" già mostrato
    ContextCompressor* m_ctxAuto = nullptr;  ///< Headroom per ReAct: comprime m_autoHistory
    QString      m_autoBuf;                  ///< accumulo token step corrente
    int          m_autoStep       = 0;       ///< step corrente del ciclo (0 = prima chiamata)
    int          m_autoMaxSteps   = 8;       ///< limite step prima di terminare forzatamente
    QString      m_autoLastUserMsg;          ///< task originale dell'utente

    void runAutonomousAgent();               ///< avvia/continua un ciclo ReAct
    void _autoAdvance(const QString& resp);  ///< processa risposta e avanza al passo successivo

    /* ── STT — pulsante + processi tracciati ── */
    QPushButton* m_btnVoice   = nullptr;  ///< pulsante Trascrivi voce (testo cambia in-place)
    QProcess*    m_recProc    = nullptr;  ///< arecord
    QProcess*    m_sttProc     = nullptr;  ///< whisper-cli / faster-whisper
    QProcess*    m_vadProc     = nullptr;  ///< vad_filter.py asincrono (solo percorso senza demone)
    bool         m_sttAutoSending = false; ///< true durante il click programmatico del loop voce (distingue dallo Stop utente)
    bool         m_autoAborted    = false; ///< Stop utente durante il ciclo ReAct: le continuazioni schedulate non ripartono
    QProcess*    m_diarizeProc = nullptr;  ///< speaker_diarize.py post-proc
    QTimer*      m_sttTick     = nullptr;  ///< countdown 1s visibile nel pulsante
    QString      m_sttWavPath;             ///< path file .wav registrato
    enum class SttState { Idle, Recording, Transcribing, Downloading } m_sttState = SttState::Idle;

    /** Scarica ggml-small.bin in ~/.prismalux/whisper/models/ con progress nella chat */
    void downloadWhisperModel();
    /** Avvia registrazione audio + trascrizione whisper.cpp */
    void _sttStartRecording();
    /** Avvia TTS tracciato con feedback "Avvio lettura..." (Piper → espeak-ng → spd-say) */
    void _ttsPlay(const QString& tts);

    /* ── Allega file (doc + immagini unificate) ── */
    QPushButton* m_btnDoc    = nullptr;
    QString      m_docContext;              ///< Testo estratto dal documento allegato
    QPushButton* m_btnImg    = nullptr;     ///< non più usato in UI — mantenuto per slot esistenti
    QByteArray   m_imgBase64;  ///< Base64 dell'immagine allegata
    QString      m_imgMime;
    bool         m_docLoading = false;  ///< true mentre estrazione PDF/Excel è in corso

    /* ── Pannello scroll caratteri speciali ── */
    QScrollArea*   m_symbolsScrollArea = nullptr;  ///< wrapper scrollabile del pannello simboli

    /* ── RAG inline (drag & drop diretto nel tab principale) ── */
    RagDropWidget* m_ragInline   = nullptr;
    QWidget*       m_ragPanel    = nullptr;  ///< wrapper collassabile
    QPushButton*   m_btnRag      = nullptr;  ///< toggle visibilità
    QPushButton*   m_btnTeam     = nullptr;  ///< toggle "Team di agenti"

    /* ── Query in background (cambio sessione durante elaborazione AI) ── */
    bool    m_bgMode    = false;   ///< true = AI in corso ma si sta visualizzando altra sessione
    QString m_bgBuffer;            ///< token accumulati mentre non vengono mostrati nel log
    QString m_bgHtmlSave;          ///< HTML parziale del log al momento dello switch

    /* ── Zona drop RAG per PDF / .txt / .md (indicizzazione nel RagEngine) ── */
    QLabel*        m_ragDropZone  = nullptr;  ///< drop target PDF/txt/md
    bool           m_ragIngesting = false;    ///< true durante indicizzazione
    QLabel*        m_ragStatusLbl = nullptr;  ///< stato indicizzazione

    /* ── Web reading via RAG — campo URL ── */
    QLineEdit*              m_ragUrlLine  = nullptr;
    QNetworkAccessManager*  m_ragUrlNam   = nullptr;
    QNetworkReply*          m_ragUrlReply = nullptr;

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
    QComboBox*   m_cmbLLM       = nullptr;  ///< Selettore LLM singolo nella toolbar
    QString      m_pageModel;               ///< Modello preferito per questa scheda (privato)
    QPushButton* m_btnRegen     = nullptr;  ///< "Rigenera con [modello]" — visibile dopo cambio LLM
    QLabel*      m_modelWarnLbl = nullptr;  ///< avviso capabilities modello (vision/tool)
    QCheckBox*   m_chkAutoRouting = nullptr;  ///< D-27: routing automatico dominio→modello (default ON)
    /** D-27: se "Routing automatico" è attivo, ritorna un modello installato più
     *  adatto al task (coder per domande di codice, vision se c'è un'immagine
     *  allegata); altrimenti ritorna 'fallback' invariato (scelta manuale del
     *  combo, mai scavalcata di default). Definita in main_ai_pipeline.cpp. */
    QString _routedModel(const QString& task, const QString& fallback) const;

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

    /* ── setupUI stepdown: livello 1 ── */
    void buildToolbar(QVBoxLayout* lay);
    void buildChatLog(QVBoxLayout* lay);
    void buildChartPanel(QVBoxLayout* lay);
    QPushButton* buildInputArea(QVBoxLayout* lay);
    void buildRagPanel(QVBoxLayout* lay);
    void buildHintFooter(QVBoxLayout* lay);
    void buildInputConnections(QPushButton* btnSymbols);
    void buildSymbolsPanel(QVBoxLayout* lay, QPushButton* btnSymbols);
    void buildExtraConnections();

    /* ── setupUI stepdown: livello 2 ── */
    void buildToolbarTtsSection(QHBoxLayout* toolLay, QWidget* toolbar);
    void buildToolbarExportSection(QHBoxLayout* toolLay, QWidget* toolbar);
    void buildToolbarVoiceLoop(QHBoxLayout* toolLay, QWidget* toolbar);
    void buildToolbarModeToggle(QHBoxLayout* toolLay, QWidget* toolbar);
    void buildToolbarLLMSelector(QHBoxLayout* toolLay, QWidget* toolbar);
    void buildInputTextField(QGridLayout* inputGrid, QWidget* inputArea);
    void buildMathPanel(QVBoxLayout* lay); ///< pannello formule matematiche LaTeX live-preview
    QPushButton* buildInputActionButtons(QGridLayout* inputGrid, QWidget* inputArea);
    void buildInputRagToggle(QGridLayout* inputGrid, QWidget* inputArea);
    void buildInputTabOrder(QPushButton* btnSymbols);
    void buildInputFormatBar();  ///< crea m_fmtBar come figlio di m_input
    void buildSymbolCategoryRow(QGridLayout* panGrid, int gridRow,
                                const char* cat, const char* chars,
                                int btnW, int btnH, int viewportW);

    /* ── onBtnTranslateClicked stepdown ── */
    bool _buildTranslateDialog(const QString& inputText,
                               QString* outSrc, QString* outDst, QString* outModel);
    void _startTranslation(const QString& src, const QString& dst,
                           const QString& model, const QString& inputText);

    void runPipeline();
    /** Bolla QR evento (risultato "QR_EVENTO_JSON:" del tool
     *  crea_evento_calendario) — true se il risultato è stato gestito */
    bool _showQrEventoBubble(const QString& result);
    /** Retry "LLM incerto" riuscito: salva domanda + risultati web + sintesi
     *  in RAG/RICERCA/<ts>_<slug>.md (stesso formato/segnale di websearch:) */
    void _saveAutoSearchToRag(const QString& synthesis);
    /** Calcola la stima token usati ed emette contextUsage() */
    void _emitContextUsage();
    void runByzantine();
    void runMathTheory();
    void runConsiglioScientifico();
    void aggregaConsiglio();
    static double jaccardSim(const QString& a, const QString& b);
    void runAgent(int idx);
    void advancePipeline();

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

    /* ── Knowledge P4/P5 ──────────────────────────────────────────────────── */
    /** Chiama il MCP knowledge_updater via QProcess (fire-and-forget, 5s timeout).
     *  summary  — testo con prefissi PREFERENZE:/PROGETTO:/PROCEDURA:/DECISIONE:/CONTESTO:
     *  label    — etichetta sessione per la sezione "contesto" (opzionale) */
    void callKnowledgeMcp(const QString& summary, const QString& label = {});
    void saveFeedback(int bubbleIdx, int rating);   ///< +1=👍 -1=👎 → ~/.prismalux/feedback.jsonl

    /** Apre il dialog "Salva in Knowledge" per salvare manualmente la risposta (P4) */
    void onSaveKnowledge();

    /** Avvia l'agente Estrattore nascosto al termine della pipeline (P5) */
    void runKnowledgeExtract();

    /* ── Hermes Agent — Persistent Memory + Skill Self-Improving ── */
    GraphMemory* m_hermesGm          = nullptr;  ///< DB: ~/.prismalux/hermes_memory.db
    QPushButton* m_hermesToggle      = nullptr;  ///< rimosso dalla toolbar, tenuto per compatibilità
    QPushButton* m_hermesToggleBar   = nullptr;  ///< pulsante in barra inferiore
    QPushButton* m_hermesReflectBar  = nullptr;  ///< riflessione in barra inferiore
    bool         m_hermesEnabled     = false;

    void hermesInit();
    /** Cerca in hermesGm nodi pertinenti alla query e aggiunge al prompt di sistema. */
    void hermesInjectContext(QString& sysPrompt, const QString& query);
    /** Salva la conversazione come nodo in hermesGm (fire-and-forget). */
    void hermesStoreConversation(const QString& userMsg, const QString& aiResp);
    /** Avvia la riflessione: l'AI analizza i nodi recenti e propone aggiornamenti. */
    void hermesReflect();

    /* ── Handler di completamento per onFinished() — un metodo per modalità ── */
    void _finishedTranslating(const QString& full);
    void _finishedKnowledgeExtract();
    void _finishedPipelineControl();
    void _finishedPipeline(const QString& full);
    /* Fasi di _finishedPipeline (D-35: estrazione meccanica).
       I bool segnalano al chiamante un'uscita anticipata da propagare
       (return) per preservare il flusso originale. */
    QString _fpExtractResponse(const QString& full, QString& extractedThink);
    bool    _fpInterceptToolCall(const QString& rawResp);
    QString _fpUncertaintyBanner(const QString& rawResp, bool& shouldAutoSearch);
    QString _fpSourcesFooter();
    void    _fpStartTranslationIfNeeded(const QString& rawResp);
    void    _fpStartAutoSearch();
    bool    _fpConfirmExecDialog(const ExecCode& ec, const QString& pyCode,
                                 bool useSandbox);
    void    _fpExecCompiled(const QString& pyCode, const QString& lang,
                            QSharedPointer<QElapsedTimer> tmr);
    void    _fpExecDocker(const QString& pyCode, QSharedPointer<QElapsedTimer> tmr);
    bool    _fpExecPythonLocal(const QString& pyCode, QSharedPointer<QElapsedTimer> tmr);
    void _finishedMathTheory();
    void _finishedByzantine();

public slots:
    void onBubbleStyleChanged();     ///< aggiorna border-radius bolle esistenti in m_log
    void recolorLog();               ///< ricolora bolle esistenti al cambio tema (ThemeManager::changed)

private slots:
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onError(const QString& msg);
    void onModelsReady(const QStringList& list);
    void onNativeToolCall(const QString& name, const QJsonObject& args);
    void processToolBatch();   ///< esegue tutti i tool_calls accumulati in m_incomingToolBatch
    void onSttTick();     ///< scatta ogni 1s durante registrazione: aggiorna testo pulsante
    void onSttTimeout();  ///< scatta a 6.5s: ferma registrazione e avvia trascrizione
    void onSttVadFinished();       ///< vad_filter.py terminato: SILENCE → stop, altrimenti trascrivi
    void onSttVadTimeout();        ///< watchdog 3.5s: VAD bloccata → kill e trascrivi comunque
    void _sttHandleSilence();      ///< bolla "silenzio rilevato" + eventuale retry loop voce
    void _sttRunTranscription();   ///< stato Transcribing + SttWhisper::transcribe(m_sttWavPath)

    /* ── TTS ── */
    void onTtsProcFinished(int code, QProcess::ExitStatus status);
    void onTtsProcError(QProcess::ProcessError err);
    void onTtsHideWaitLbl();

    /* ── UI toolbar / toolbar TTS ── */
    void onTtsStopClicked();
    void onTtsPauseClicked();
    void onBtnExportClicked();
    void onBtnExportPdfClicked();
    void onBtnInfoClicked();
    void onVoiceLoopToggled(bool on);
    void onToolChkToggled(bool on);
    void onCmbLLMIndexChanged(int idx);
    void onModeToggleToggled(bool autoOn);  ///< mantenuto per compatibilità (ora usato da ciclo 3 stati)
    void onModeBtnChanged(int mode);        ///< settore TriModeButton selezionato: 0=Chat 1=Agentico 2=Conversa
    void onCycleModeShortcut();             ///< Shift+Tab: cicla Chat→Agentico→Conversa
    void onBtnRegenClicked();  ///< Rigenera ultima risposta con il modello corrente
    void onHermesToggled(bool on);
    void onHermesReflectClicked();
    void onToolsPanelToggle();       ///< apre/chiude il pannello ⚡ Tool Veloci
    void onMcpPanelToggle();         ///< apre/chiude il pannello 🔌 Tool Lenti (MCP)
    void onToolEnabledChanged();     ///< una checkbox tool/MCP è cambiata → aggiorna label

    /* ── Log / scroll ── */
    void onLogScrollValueChanged(int value);
    void onBtnChartOpenClicked();
    void onLogAnchorClicked(const QUrl& url);
    void onBtnRunDelayedClick();    ///< QTimer::singleShot(0) → m_btnRun->click()
    void onLogContextMenuRequested(const QPoint& pos);

    /* ── Input area ── */
    void onBtnRagToggled(bool on);
    void onRagDropZoneEnter();
    void onRagDropZoneLeave();
    void onRagIngestionDone();
    void onRagUrlAddClicked();
    void onRagUrlFetched();

    /** Indica ai file URL droppati nella zona RAG specializzata (PDF/txt/md) */
    void _ingestRagFiles(const QList<QUrl>& urls);
    void onBtnHintHideClicked();
    void onHintLinkActivated(const QString& link);  ///< link "Cosa sai fare?" nei suggerimenti → esegue la domanda
    void onBtnRunClicked();
    void onSymbolBtnClicked();      ///< inserisce il simbolo da sender()->property("symbol")
    void onBtnSymbolsClicked();     ///< mostra/nasconde m_symbolsScrollArea
    void onBtnTranslateClicked();   ///< mantenuto — usato internamente da translatePanel
    void onBtnDocClicked();         ///< allega doc + immagini (unificate)
    void onBtnVoiceClicked();
    void onInputSelectionChanged();           ///< mostra/nasconde m_fmtBar sulla selezione
    void onFmtBtnClicked(const QString& before, const QString& after); ///< applica marcatori markdown
    void onSymbolSearchChanged(const QString& query); ///< filtra simboli visibili
    void updateMathPreview();           ///< aggiorna la LatexView con la conversione del testo corrente
    void onInsertBuilderFormula();      ///< inserisce il LaTeX del builder nel campo testo
    void onClearBuilderClicked();       ///< chiede conferma prima di svuotare il builder
    void onToggleThunk(int idx);        ///< apre/chiude il thunk "query normalizzata" nel log
    void onEtimoToggled(bool on);       ///< stile toggle dizionario etimologico
    void onMathToggleToggled(bool on);  ///< stile + visibilità pannello formule
    void onMathTplBtnClicked();         ///< inserisce template LaTeX da sender()->property("mathTpl")
    void onBtnTblClicked();             ///< apre TablePickerPopup sotto il pulsante Tabella
    QString buildThunkHtml(int idx, const QString& text, bool open) const;

    /* ── Pipeline / preset ── */
    void onNumAgentsChanged(int v);
    void onCmbModePresetChanged(int idx);
    void onCmbModeMathChanged(int idx);
    void onAiAborted();
    void onAiModelChanged(const QString& newModel);

    /* ── STT / download ── */
    void onRecProcFinished(int exitCode, QProcess::ExitStatus status);
    void onSttVoiceLoopAutoSend();   ///< singleShot(150) → m_btnRun->click() dopo STT ok
    void onSttVoiceLoopRetry();      ///< singleShot(1500) → _sttStartRecording() dopo STT fail
    void onEscShortcut();            ///< Esc: ferma registrazione/loop Conversa
    void _voiceConversaStop();       ///< STOP totale voce: rec, trascrizione, AI, loop
    void onWhisperDlProcReadyRead();
    void onWhisperDlProcFinished(int code, QProcess::ExitStatus status);

    /* ── Knowledge MCP ── */
    void onKnowledgeWatchdogTimeout();
    void onKnowledgeProcFinished(int code, QProcess::ExitStatus status);
    void onKnowledgeSaveBtnClicked();
    void onKnowledgeSaveDlgSectionChanged(const QString& sec);

    /* ── Consiglio Scientifico (peer paralleli) ── */
    void onConsiglioPeerToken(const QString& t);
    void onConsiglioPeerFinished(const QString& full);
    void onConsiglioPeerError(const QString& err);

    /* ── Traduzione LLM post-processing ── */
    void onTranslationFinished(const QString& translated);

    /** Salva la sessione corrente */
    void onChatCompletedSave(const QString& title, const QString& logHtml);
};

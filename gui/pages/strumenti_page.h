#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QString>
#include <QCheckBox>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QTableWidget>
#include <QProcess>
#include <QSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include "../ai_client.h"

class LavoroPage;
class LanServer;

/* ══════════════════════════════════════════════════════════════
   StrumentiPage — Assistente AI multi-dominio
   Layout a due colonne: nav sinistra (200px) + stack destra.
   Categorie: Studio, Scrittura Creativa, Ricerca, Libri, Produttività
   ══════════════════════════════════════════════════════════════ */
class StrumentiPage : public QWidget {
    Q_OBJECT
public:
    explicit StrumentiPage(AiClient* ai, QWidget* parent = nullptr);

    /* Funzioni utilitarie usate anche nei test */
    static QString     ragExtractText(const QString& path);
    static QStringList ragChunkText(const QString& text,
                                    int chunkSize = 600,
                                    int overlap   = 80);
    static QString     sysPromptForAction(int navIdx, int subIdx);

private:
    AiClient*       m_ai        = nullptr;
    QListWidget*    m_navList   = nullptr;
    QStackedWidget* m_stack     = nullptr;
    QTextEdit*      m_output    = nullptr;
    QTextEdit*      m_inputArea = nullptr;
    QPushButton*    m_btnRun    = nullptr;
    QComboBox*      m_cmbSub    = nullptr;  ///< Combo sotto-azione corrente
    QLabel*         m_waitLbl   = nullptr;
    bool            m_active    = false;

    /* ── Documenti PDF (cat = 5) ── */
    QWidget*        m_pdfRow    = nullptr;  ///< riga picker PDF (visibile solo per cat 5)
    QLabel*         m_pdfPathLbl = nullptr; ///< mostra il nome del PDF caricato
    QString         m_pdfPath;              ///< percorso assoluto del PDF selezionato

    /* ── Blender MCP (cat = 6) ── */
    int             m_currentCat   = 0;       ///< categoria selezionata corrente
    QWidget*        m_blenderRow   = nullptr; ///< barra connessione Blender
    QWidget*        m_blenderHintRow = nullptr; ///< avviso installazione addon MCP
    QLineEdit*      m_blenderHostEdit = nullptr; ///< host:porta del bridge Blender
    QLabel*         m_blenderStatusLbl = nullptr;
    QPushButton*    m_blenderExecBtn = nullptr;  ///< esegui codice in Blender
    QNetworkAccessManager* m_blenderNam = nullptr;
    QString         m_blenderCode;             ///< codice bpy estratto dall'ultima risposta AI

    /* ── FreeCAD MCP (cat = 8) ── */
    QWidget*        m_freecadRow     = nullptr; ///< barra connessione FreeCAD
    QWidget*        m_freecadHintRow = nullptr; ///< avviso installazione addon MCP
    QLineEdit*      m_freecadHostEdit  = nullptr;
    QLabel*         m_freecadStatusLbl = nullptr;
    QPushButton*    m_freecadExecBtn   = nullptr;
    QString         m_freecadCode;              ///< codice FreeCAD estratto dall'ultima risposta AI

    /* ── Sketch → 3D (Blender cat=6 + FreeCAD cat=8) ── */
    QWidget*        m_sketchRow      = nullptr;
    QLabel*         m_sketchFileLbl  = nullptr;
    QLineEdit*      m_sketchNotes    = nullptr;
    QPushButton*    m_btnSketchGen   = nullptr;
    QString         m_sketchFilePath;
    bool            m_sketchIsImage  = false;

    /* ── Selezione modello per categorie codice (Blender/Office/FreeCAD) ── */
    QWidget*        m_codeModelRow   = nullptr;
    QComboBox*      m_codeModelCombo = nullptr;
    QLabel*         m_codeModelInfo  = nullptr;

    /* ── CloudCompare (cat = 9, prossimamente) ── */
    QWidget*        m_cloudCompareRow = nullptr;

    /* Rete LAN spostata in ProgrammazionePage (sub-tab "🌐 Rete LAN") */
    QWidget*        m_reteRow        = nullptr;  ///< non più usato in questa pagina

    /* ── RAG in-page (tutte le categorie attive) ── */
    QWidget*        m_ragRow      = nullptr;
    QCheckBox*      m_ragCheck    = nullptr;  ///< abilita/disabilita iniezione RAG
    QLabel*         m_ragInfoLbl  = nullptr;  ///< "N chunk da M file"
    QStringList     m_ragChunks;              ///< testo dei chunk indicizzati
    QStringList     m_ragFileNames;           ///< nomi file caricati (solo display)

    void            ragAddFile(const QString& path);  ///< carica e indicizza un file
    QString         ragBuildContext(const QString& query,
                                    int topK = 3) const;  ///< top-k chunk per keyword

    /* ── Office/LibreOffice MCP (cat = 7) ── */
    QWidget*        m_officeRow      = nullptr; ///< barra connessione Office bridge
    QWidget*        m_officeHintRow  = nullptr; ///< avviso installazione MCP Office
    QLabel*         m_officeStatusLbl = nullptr;
    QPushButton*    m_officeExecBtn = nullptr;  ///< esegui codice office
    QPushButton*    m_officeStartBtn = nullptr; ///< avvia/ferma bridge Python
    QNetworkAccessManager* m_officeNam = nullptr;
    QProcess*       m_officeBridgeProc = nullptr; ///< processo bridge Python locale
    QString         m_officeCode;              ///< codice office estratto dall'ultima risposta AI
    QString         m_officeBridgeToken;       ///< token Bearer letto da ~/.prismalux_office_token

    /* ── Input row (nascosta quando Cerca Lavoro è attivo) ── */
    QWidget*        m_inputRow   = nullptr;

    /* ── Cerca Lavoro (mostrato quando lavoroBtn è selezionato) ── */
    LavoroPage*     m_lavoroPage = nullptr;

    /* ── Cron inline panel ── */
    QWidget*        m_cronPanel  = nullptr;

    /* ── Stable Diffusion panel ── */
    QWidget*        m_sdPanel        = nullptr;

    /* ── LAN Android panel ── */
    LanServer*      m_lanServer      = nullptr;
    QWidget*        m_lanPanel       = nullptr;
    QPushButton*    m_lanToggleBtn   = nullptr;
    QSpinBox*       m_lanPortSpin    = nullptr;
    QLabel*         m_lanStatusLbl   = nullptr;
    QLabel*         m_lanClientsLbl  = nullptr;

    /* ── Agentic File Search ── */
    QPushButton*  m_fileSearchBtn    = nullptr; ///< catBar button
    QWidget*      m_fileSearchPanel  = nullptr;
    QLineEdit*    m_fileSearchDir    = nullptr;
    QLineEdit*    m_fileSearchQuery  = nullptr;
    QTextBrowser* m_fileSearchOut    = nullptr;
    QProcess*     m_fileSearchProc   = nullptr;

    /* ── LLM Wiki (Wikipedia + AI) ── */
    QPushButton*  m_wikiBtn          = nullptr; ///< catBar button
    QWidget*      m_wikiPanel        = nullptr;
    QLineEdit*    m_wikiQuery        = nullptr;
    QComboBox*    m_wikiLangCombo    = nullptr;
    QTextBrowser* m_wikiContent      = nullptr; ///< testo Wikipedia grezzo
    QLabel*       m_wikiWaitLbl      = nullptr;
    QString       m_wikiExtract;                ///< estratto corrente (iniettato nell'AI)

    /* ── Costruttori sotto-pagine ── */
    QWidget* buildSubPage(const QStringList& actions, const QString& placeholder);
    QWidget* buildStudio();
    QWidget* buildScritturaCreativa();
    QWidget* buildRicerca();
    QWidget* buildLibri();
    QWidget* buildProduttivita();
    QWidget* buildFileSearchPanel();
    QWidget* buildWikiPanel();

    void runTool(const QString& sysPrompt, const QString& userMsg);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onError(const QString& msg);
    void _setRunBusy(bool busy);
};

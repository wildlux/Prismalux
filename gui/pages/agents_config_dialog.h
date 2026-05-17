#pragma once
/*
 * agents_config_dialog.h — Configurazione agenti in finestra separata
 * =====================================================================
 * Contiene:
 *   - RagDropWidget : drag-and-drop file/cartelle per contesto RAG
 *   - AgentsConfigDialog : QDialog con griglia 6-agenti
 *
 * Uso da AgentiPage:
 *   m_cfgDlg->roleCombo(i)->currentIndex()
 *   m_cfgDlg->modelCombo(i)->currentData().toString()
 *   m_cfgDlg->enabledChk(i)->isChecked()
 *   m_cfgDlg->ragWidget(i)->ragContext()
 *   m_cfgDlg->setModels(list)
 *   m_cfgDlg->applyPreset(modeIdx)
 *   m_cfgDlg->updateVisibility(numAgents)
 */

#include <QDialog>
#include <QFrame>
class AiClient;
class QNetworkAccessManager;
class QNetworkReply;
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QVector>

/* ══════════════════════════════════════════════════════════════
   RagDropWidget — area drag-and-drop per contesto RAG + URL web
   ══════════════════════════════════════════════════════════════ */
class RagDropWidget : public QFrame {
    Q_OBJECT
public:
    explicit RagDropWidget(QWidget* parent = nullptr);

    /** Testo aggregato da tutti i file/URL caricati (max 32 KB totali).
     *  Le voci web includono citazione [N] e istruzione di riferimento. */
    QString ragContext() const;
    bool    hasContext()    const { return !m_files.isEmpty(); }
    bool    hasWebEntries() const;

    /** Aggiunge una voce web pre-fetchata (chiamato dal fetcher async). */
    void addEntry(const QString& name, const QString& content);

signals:
    void webFetchStarted(const QString& url);
    void webFetchDone(const QString& url, bool ok);

protected:
    void dragEnterEvent(QDragEnterEvent*) override;
    void dragMoveEvent(QDragMoveEvent*)   override;
    void dropEvent(QDropEvent*)           override;
    void mousePressEvent(QMouseEvent*)    override;

private slots:
    void onUrlBtnClicked();
    void onClearBtnClicked();
    void onFetchUrlReplyFinished();
    void onLabelRestoreTimer();

private:
    void addPath(const QString& path);
    void addFile(const QString& path);
    void fetchUrl(const QString& url);
    void updateLabel();

    struct FileEntry { QString name; QString content; bool isWeb = false; };
    QVector<FileEntry>    m_files;
    QLabel*               m_lbl      = nullptr;
    QPushButton*          m_clearBtn = nullptr;
    QPushButton*          m_urlBtn   = nullptr;
    QNetworkAccessManager* m_nam     = nullptr;
    int                   m_totalBytes  = 0;
    int                   m_pendingFetches = 0;
    QNetworkReply*        m_fetchReply  = nullptr;  ///< Reply corrente fetchUrl (slot)

    static constexpr int MAX_PER_FILE = 4096;
    static constexpr int MAX_PER_WEB  = 12000;
    static constexpr int MAX_TOTAL    = 32768;
};

/* ══════════════════════════════════════════════════════════════
   AgentsConfigDialog — griglia configurazione 6 agenti
   ══════════════════════════════════════════════════════════════ */
class AgentsConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit AgentsConfigDialog(QWidget* parent = nullptr);

    static constexpr int MAX_AGENTS = 6;

    /* ── Accessori widget (usati da AgentiPage) ── */
    QComboBox*     roleCombo(int i)  { return m_roleCombo[i]; }
    QComboBox*     modelCombo(int i) { return m_modelCombo[i]; }
    QCheckBox*     enabledChk(int i) { return m_enabledChk[i]; }
    RagDropWidget* ragWidget(int i)  { return m_ragWidget[i]; }
    QSpinBox*      numAgentsSpinBox()  { return m_spinShots; }
    int            numAgents() const   { return m_spinShots ? m_spinShots->value() : MAX_AGENTS; }
    QComboBox*     modeCombo()         { return m_cmbMode; }

    /** Popola tutte le combo modello con la lista aggiornata dal backend.
     *  Se ai != nullptr aggiunge icona ☁️/🌍📍 in base a modelSizeBytes. */
    void setModels(const QStringList& models, AiClient* ai = nullptr);

    /** Aggiorna visibilità righe agente in base al numero selezionato */
    void updateVisibility(int numAgents);

    /** Applica preset categoria (modeIdx 3-9 come in AgentiPage::m_cmbMode) */
    void applyPreset(int modeIdx);

    /* ── Struttura ruolo — usata anche da AgentiPage ── */
    struct AgentRole {
        QString icon;
        QString name;
        QString sysPrompt;       /* prompt completo per modelli 7B+ */
        QString sysPromptSmall;  /* prompt corto per modelli ≤4B (direttivo, max 2-3 frasi) */
        QString category;        /* categoria di appartenenza (es. "Matematica") */
    };
    static QVector<AgentRole> roles();

    /** Seleziona il ruolo del combo agente 'agentIdx' cercando per indice vettore roles() */
    void setRoleByVecIndex(int agentIdx, int vecIdx);

    /** Stato del Controller LLM (spostato dal toolbar all'interno del dialog) */
    bool controllerEnabled() const { return m_controllerChk && m_controllerChk->isChecked(); }

    /** Contesto RAG condiviso da tutti gli agenti */
    RagDropWidget* sharedRagWidget() { return m_sharedRag; }

private:
    QSpinBox*      m_spinShots              = nullptr;
    QComboBox*     m_cmbMode               = nullptr;
    QComboBox*     m_roleCombo [MAX_AGENTS] = {};
    QComboBox*     m_modelCombo[MAX_AGENTS] = {};
    QCheckBox*     m_enabledChk[MAX_AGENTS] = {};
    RagDropWidget* m_ragWidget [MAX_AGENTS] = {};
    RagDropWidget* m_sharedRag              = nullptr;  ///< RAG condiviso da tutti gli agenti
    QCheckBox*     m_controllerChk          = nullptr;  ///< Controller LLM (ex toolbar)

    void setupUI();

private slots:
    void onEnabledChkToggled(bool on);
};

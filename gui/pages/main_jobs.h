#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QLabel>
#include <QTableWidget>
#include <QNetworkAccessManager>
#include "../ai_client.h"
#include "../widgets/collapsible_section.h"
#include "main_jobs_data.h"
#ifdef HAVE_JOB_ASSISTANT
#include <QWebEngineView>
#include <QWebChannel>
#include <QCheckBox>
#include <QScrollArea>
#include <QStackedWidget>
#include <QMap>
#include "../widgets/job_recorder_bridge.h"
#endif

class LavoroPage : public QWidget {
    Q_OBJECT
public:
    explicit LavoroPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    /* ── CV ── */
    void onSfogliaBtnClicked();

    /* ── Modello ── */
    void onModelloIndexChanged(int idx);

    /* ── Azioni lista offerte ── */
    void onOfferteItemChanged(QListWidgetItem* cur, QListWidgetItem* prev);
    void onEmailBtnClicked();
    void onLavoroLogTextChanged();
    void onCopiaLettBtnClicked();
    void onCoverLogTextChanged();
    void onCopiaCoverBtnClicked();
    void onGenBtnClicked();
    void onGenCoverBtnClicked();
    void onOfferteItemDoubleClicked(QListWidgetItem* item);
    void onToggleBtnClicked();

    /* ── Analizza URL / CV ── */
    void onAnalizzaUrlBtnClicked();
    void onAnalizzaCvBtnClicked();

    /* ── Foto profilo ── */
    void onFotoBtnClicked();

    /* ── Segnali AI ── */
    void onAiToken(const QString& t);
    void onAiFinished(const QString& text);
    void onAiError(const QString& err);
    void onAiAborted();

    /* ── Context menu log ── */
    void onLavoroLogContextMenu(const QPoint& pos);
    void onCoverLogContextMenu(const QPoint& pos);
    /* ── Tracker candidature ── */
    void onTrackerAddRow();
    void onTrackerRemoveRow();
    void onTrackerSave();
    /* ── Calcolatore euro/ore ── */
    void onCalcChanged();

#ifdef HAVE_JOB_ASSISTANT
    /* ── Assistente Candidature (browser embedded + macro) ── */
    void onAsstGoClicked();
    void onAsstBackClicked();
    void onAsstFwdClicked();
    void onAsstReloadClicked();
    void onAsstLoadFinished(bool ok);
    void onAsstRegistraToggled(bool on);
    void onAsstElementClicked(const QString& selettore, const QString& tag, const QString& hint);
    void onAsstStepDeleteClicked();
    void onAsstPlayClicked();
    void onAsstSitoComboChanged(int idx);
    void onAsstNewSiteClicked();
    void onAsstProfiloChanged();
    void onAsstToggleCandColumn(bool on);
    void onAsstToggleCtrlColumn(bool on);
    void onAsstProfiloComboChanged(int idx);
    void onAsstProfiloNewClicked();
    void onAsstProfiloCvBtnClicked();
#endif

private:
    /* ── Helper interni (non slot, non lambda) ── */
    void applicaFiltri();
    void caricaCV(const QString& path);
    void popolaModelli(const QStringList& models);
    void genLettera();
    void genCover();
    void sendFn(const QString& msg);
    void analizzaUrls(const QStringList& urlList);
    void aiDone();
    void loadTracker();
    QString trackerPath() const;
    QComboBox* makeStatoCombo(QWidget* parent);

    /* ── Costanti di testo ── */
    static const QString& cvFallback();
    static const QString& socraticoBase();

#ifdef HAVE_JOB_ASSISTANT
    /* ── Assistente Candidature: helper interni ── */
    QWidget* buildAssistenteTab(QWidget* parent, QWidget* candidatureContent);
    void injectRecorderScript();
    void eseguiStep(int idx);
    void logAsst(const QString& msg);
    void aggiornaListaPassi();
    QString macroPath(const QString& dominio) const;
    void salvaMacroCorrente();
    void caricaMacroPerDominio(const QString& dominio);
    QString valorePerCampo(const QString& campoDato) const;
    QString profiliCandidatoPath() const;
    void salvaProfiliCandidato();
    void caricaProfiliCandidato();
    void applicaProfiloAiCampi(const QString& nomeProfilo);
#endif

    /* ── Dati ── */
    AiClient*    m_ai;
    QString      m_cvText;
    quint64      m_myReqId      = 0;
    quint64      m_myCoverReqId = 0;
    bool         m_calcBusy     = false;

    /* ── Widget ── */
    QTextEdit*   m_lavoroLog     = nullptr;
    QTextEdit*   m_coverLog      = nullptr;
    QLabel*      m_cvStatus      = nullptr;
    QLineEdit*   m_cvPath        = nullptr;
    QLabel*      m_fotoLbl       = nullptr;  ///< Anteprima foto profilo (64×64, arrotondato)
    QString      m_fotoPath;
    QComboBox*   m_filtroTipo    = nullptr;
    QComboBox*   m_filtroLivello = nullptr;
    QListWidget* m_offerteLista  = nullptr;
    QComboBox*   m_cmbModello    = nullptr;
    QLabel*      m_modelloLbl    = nullptr;
    QLabel*      m_linksLbl      = nullptr;
    QPushButton* m_analizzaUrlBtn = nullptr;
    QPushButton* m_analizzaCvBtn  = nullptr;
    QPushButton* m_stopAiBtn      = nullptr;
    QNetworkAccessManager* m_nam  = nullptr;

    /* Widget promuossi per slot toggle/azioni */
    QWidget*     m_cvBox         = nullptr;
    QWidget*     m_llmBox        = nullptr;
    CollapsibleSection* m_cvSection  = nullptr;  ///< D-48: sezione arrotolabile CV
    CollapsibleSection* m_llmSection = nullptr;  ///< D-48: sezione arrotolabile Modello AI
    QWidget*     m_filtriRow     = nullptr;
    QPushButton* m_toggleBtn     = nullptr;
    QPushButton* m_emailBtn      = nullptr;
    QPushButton* m_copiaBtn      = nullptr;
    QPushButton* m_copiaCoverBtn = nullptr;
    QLabel*      m_waitLbl       = nullptr;
    QLabel*      m_selLbl        = nullptr;

    /* ── Tracker candidature ── */
    QTableWidget* m_trackerTable  = nullptr;
    QPushButton*  m_trackerAddBtn = nullptr;
    QPushButton*  m_trackerDelBtn = nullptr;

    /* ── Calcolatore euro/ore ── */
    QLineEdit*    m_calcOre       = nullptr;
    QLineEdit*    m_calcMensile   = nullptr;
    QLineEdit*    m_calcAnnuo     = nullptr;
    QLineEdit*    m_calcOrario    = nullptr;
    QLabel*       m_calcNettoLbl  = nullptr;
    QLabel*       m_mercatoLbl    = nullptr;

    void precompilaStipendioDaOfferta(const Offerta& o);

#ifdef HAVE_JOB_ASSISTANT
    /* ── Assistente Candidature: widget e stato ── */
    QWebEngineView*    m_asstView        = nullptr;
    QLineEdit*         m_asstUrlEdit     = nullptr;
    QComboBox*         m_asstSitoCombo   = nullptr;
    QPushButton*       m_asstRegBtn      = nullptr;
    QListWidget*       m_asstPassiList   = nullptr;
    QPushButton*       m_asstPlayBtn     = nullptr;
    QCheckBox*         m_asstConfermaInvio = nullptr;
    QTextEdit*         m_asstLog         = nullptr;
    JobRecorderBridge* m_asstBridge      = nullptr;
    QWebChannel*       m_asstChannel     = nullptr;
    QLineEdit*         m_asstNomeEdit    = nullptr;
    QLineEdit*         m_asstCognomeEdit = nullptr;
    QLineEdit*         m_asstEmailEdit   = nullptr;
    QLineEdit*         m_asstTelEdit     = nullptr;

    JobMacro           m_asstMacro;
    bool               m_asstRegistrando = false;
    CandidatoProfilo   m_asstProfilo;

    /* ── Profili candidato multipli (es. "IT", "Cameriere") — ognuno con
       il proprio CV, un solo set di campi condiviso alla volta ── */
    QComboBox*   m_asstProfiloCombo = nullptr;
    QLabel*      m_asstProfiloCvLbl = nullptr;
    QMap<QString, CandidatoProfilo> m_asstProfili;
    QString      m_asstProfiloAttivo;

    /* ── Menù laterale centrato (Candidature | Assistente) — una sola
       sezione visibile alla volta dentro m_asstSideStack, invece delle
       2 colonne fianco a fianco sempre visibili di prima ── */
    QWidget*        m_asstCandPane        = nullptr;
    QScrollArea*    m_asstCandScroll      = nullptr;
    QPushButton*    m_asstCandCollapseBtn = nullptr;
    QWidget*        m_asstCtrlPane        = nullptr;
    QWidget*        m_asstCtrlContent     = nullptr;
    QPushButton*    m_asstCtrlCollapseBtn = nullptr;
    QStackedWidget* m_asstSideStack       = nullptr;
#endif
};

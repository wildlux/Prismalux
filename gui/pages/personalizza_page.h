#pragma once
#include <QWidget>
#include <QStackedWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QProcess>
#include <QLineEdit>
#include <QProgressBar>
#include <functional>
#include <memory>

/* ══════════════════════════════════════════════════════════════
   PersonalizzaPage — Personalizzazione Avanzata Estrema
   Sotto-sezioni navigate via QStackedWidget interno
   ══════════════════════════════════════════════════════════════ */
class PersonalizzaPage : public QWidget {
    Q_OBJECT
public:
    explicit PersonalizzaPage(QWidget* parent = nullptr);

    /* Esposte per ImpostazioniPage che le aggiunge come tab dirette */
    QWidget* buildLlamaStudio();
    QWidget* buildLoraTab();      ///< Fine-tuning LoRA con llama-finetune / unsloth

private slots:
    /* ── llama.cpp Studio — navigazione stack ── */
    void onSubMenuBtnClicked();          ///< bottoni sub-menu → setCurrentIndex(pg)
    void onBackCompClicked();            ///< "← Menu llama" in pag. Compila
    void onBackModClicked();             ///< "← Menu llama" in pag. Modelli
    void onBackDlClicked();              ///< "← Menu llama" in pag. Scarica
    void onCompNowBtnClicked();          ///< banner "Compila ora"

    /* ── llama.cpp Studio — compilazione ── */
    void onLlamaCompBtnClicked();        ///< avvia git+cmake chain

    /* ── llama.cpp Studio — step di build (chain asincrona) ── */
    void onProc1ReadyRead();
    void onProc1Finished(int code, QProcess::ExitStatus);
    void onProc2ReadyRead();
    void onProc2Finished(int code, QProcess::ExitStatus);
    void onProc3ReadyRead();
    void onProc3Finished(int code, QProcess::ExitStatus);

    /* ── llama.cpp Studio — gestione modelli ── */
    void onPresetBtnClicked();           ///< preset URL → m_modUrlEdit
    void onDeleteModelBtnClicked();      ///< elimina file .gguf
    void onModDlBtnClicked();            ///< download da m_modUrlEdit (wget)
    void onModDlProcReadyRead();
    void onModDlProcFinished(int code, QProcess::ExitStatus);
    void onCurlReadyRead();
    void onCurlFinished(int code, QProcess::ExitStatus);
    void onRefreshBtnClicked();          ///< aggiorna lista modelli
    void onLlamaStackChanged(int idx);   ///< auto-refresh quando si va a pag. 2

    /* ── llama.cpp Studio — download modelli matematica ── */
    void onMathDlBtnClicked();           ///< download preset matematica/logica
    void onMathDlProcReadyRead();
    void onMathDlProcFinished(int code, QProcess::ExitStatus);
    void onDlCustomBtnClicked();         ///< download URL personalizzato
    void onDlCustomProcReadyRead();
    void onDlCustomProcFinished(int code, QProcess::ExitStatus);

    /* ── LoRA ── */
    void onModelBtnClicked();            ///< sfoglia modello .gguf
    void onDataBtnClicked();             ///< sfoglia dataset .jsonl
    void onOutBtnClicked();              ///< sfoglia cartella output
    void onLoraStartBtnClicked();        ///< avvia fine-tuning
    void onLoraStopBtnClicked();         ///< interrompi fine-tuning
    void onLoraInstallBtnClicked();      ///< pip install unsloth ...
    void onLoraProcReadyRead();
    void onLoraProcFinished(int code, QProcess::ExitStatus);
    void onLoraInstallReadyRead();
    void onLoraInstallFinished(int code, QProcess::ExitStatus);

    /* ── runProcArgs / runProc helpers ── */
    void onHelperProcReadyRead();
    void onHelperProcFinished(int code, QProcess::ExitStatus);

private:
    /* ── helpers ── */
    QTextEdit*  makeLog(const QString& placeholder = "");
    /** runProcArgs — versione SICURA: arglist separata, nessuna shell. */
    void        runProcArgs(QProcess* proc, const QString& program,
                            const QStringList& args,
                            QTextEdit* log, QPushButton* btn);
    /** runProc — DEPRECATO: usa sh -c. Solo per path interni, non per input utente. */
    void        runProc(QProcess* proc, const QString& cmd,
                        QTextEdit* log, QPushButton* btn);

    /** refreshModelList — ricostruisce la lista di modelli .gguf installati */
    void        refreshModelList();

    /* ── llama.cpp Studio — sotto-stack ── */
    QStackedWidget* m_llamaStack    = nullptr;
    QTextEdit*      m_llamaLog      = nullptr;
    QPushButton*    m_llamaCompBtn  = nullptr;
    QPushButton*    m_llamaServBtn  = nullptr;
    QLineEdit*      m_llamaModelPath = nullptr;
    QLineEdit*      m_llamaPort      = nullptr;
    QProcess*       m_llamaServProc  = nullptr;

    /* ── build chain stato (git→cmake configure→cmake build) ── */
    QProcess*       m_proc1         = nullptr;   ///< git clone/pull
    QProcess*       m_proc2         = nullptr;   ///< cmake configure
    QProcess*       m_proc3         = nullptr;   ///< cmake build
    QString         m_buildCloneDir;
    QString         m_buildDir;
    QStringList     m_buildGpuArgs;
    int             m_buildJobs     = 1;

    /* ── Gestione Modelli — widget che servono negli slot ── */
    QWidget*        m_listContainer = nullptr;   ///< container per le card modelli
    QLabel*         m_modDirLbl     = nullptr;   ///< label cartella modelli
    QLineEdit*      m_modUrlEdit    = nullptr;   ///< URL download
    QPushButton*    m_modDlBtn      = nullptr;   ///< pulsante Scarica URL
    QTextEdit*      m_modDlLog      = nullptr;   ///< log download modelli
    QProgressBar*   m_dlProgress    = nullptr;   ///< barra avanzamento
    QProcess*       m_modDlProc     = nullptr;   ///< wget process
    QProcess*       m_curlProc      = nullptr;   ///< curl fallback
    QString         m_pendingDelPath;            ///< path da eliminare (onDeleteModelBtnClicked)

    /* ── Download Modelli Matematica — widget ── */
    QTextEdit*      m_mathDlLog     = nullptr;
    /* Per i pulsanti preset matematica usiamo sender()+property */

    /* ── Download URL personalizzato ── */
    QLineEdit*      m_dlCustomUrlEdit = nullptr;
    QPushButton*    m_dlCustomBtn     = nullptr;
    QProcess*       m_dlCustomProc    = nullptr;

    /* ── compNowBtn (banner primo avvio) ── */
    QWidget*        m_firstLaunchBanner = nullptr;

    /* ── LoRA Fine-tuning — widget (buildLoraTab) ── */
    QTextEdit*      m_loraLog       = nullptr;
    QProcess*       m_loraProc      = nullptr;
    QPushButton*    m_loraStartBtn  = nullptr;
    QPushButton*    m_loraStopBtn   = nullptr;
    QLineEdit*      m_loraModelEdit = nullptr;  ///< path modello base
    QLineEdit*      m_loraDataEdit  = nullptr;  ///< path dataset
    QLineEdit*      m_loraOutEdit   = nullptr;  ///< cartella output
    class QSpinBox*       m_loraSpinEpochs = nullptr;
    class QSpinBox*       m_loraSpinR      = nullptr;
    class QSpinBox*       m_loraSpinAlpha  = nullptr;
    class QDoubleSpinBox* m_loraDspLr      = nullptr;
    class QSpinBox*       m_loraSpinBatch  = nullptr;
    class QSpinBox*       m_loraSpinCtx    = nullptr;
    QProcess*       m_loraInstallProc = nullptr;
    QTextEdit*      m_loraLog2      = nullptr;  ///< log install unsloth
};

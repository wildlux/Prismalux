#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QLabel>
#include <QNetworkAccessManager>
#include "../ai_client.h"
#include "lavoro_data.h"

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

    /* ── Segnali AI ── */
    void onAiToken(const QString& t);
    void onAiFinished(const QString& text);
    void onAiError(const QString& err);
    void onAiAborted();

    /* ── Context menu log ── */
    void onLavoroLogContextMenu(const QPoint& pos);
    void onCoverLogContextMenu(const QPoint& pos);

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

    /* ── Costanti di testo ── */
    static const QString& cvFallback();
    static const QString& socraticoBase();

    /* ── Dati ── */
    AiClient*    m_ai;
    QString      m_cvText;
    quint64      m_myReqId      = 0;
    quint64      m_myCoverReqId = 0;

    /* ── Widget ── */
    QTextEdit*   m_lavoroLog     = nullptr;
    QTextEdit*   m_coverLog      = nullptr;
    QLabel*      m_cvStatus      = nullptr;
    QLineEdit*   m_cvPath        = nullptr;
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
    QWidget*     m_filtriRow     = nullptr;
    QPushButton* m_toggleBtn     = nullptr;
    QPushButton* m_emailBtn      = nullptr;
    QPushButton* m_copiaBtn      = nullptr;
    QPushButton* m_copiaCoverBtn = nullptr;
    QLabel*      m_waitLbl       = nullptr;
    QLabel*      m_selLbl        = nullptr;
};

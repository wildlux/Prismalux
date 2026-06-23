#pragma once
#include <QWidget>
#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTabWidget>
#include <QLineEdit>
#include <QPixmap>
#include <QJsonArray>
#include "../ai_client.h"

/* --------------------------------------------------------------
   OffertaDialog — dialog per aggiungere una nuova offerta di lavoro.
   -------------------------------------------------------------- */
class OffertaDialog : public QDialog {
    Q_OBJECT
public:
    explicit OffertaDialog(QWidget* parent = nullptr);

    QString azienda()    const;
    QString ruolo()      const;
    QString stato()      const;

private slots:
    void onConfirm();

private:
    QLineEdit*   m_aziendaEdit = nullptr;
    QLineEdit*   m_ruoloEdit   = nullptr;
    QComboBox*   m_statoCb     = nullptr;
};

/* --------------------------------------------------------------
   LavoroPage -- Assistente AI per il lavoro.
   Tab 0: Assistente AI (CV, colloquio, stipendio, ecc.)
   Tab 1: Tracker Offerte (JSON locale, add/delete)
   Tab 2: Cover Letter (editor + genera via AI)
   -------------------------------------------------------------- */
class LavoroPage : public QWidget {
    Q_OBJECT
public:
    explicit LavoroPage(AiClient* ai, QWidget* parent = nullptr);

private:
    /* ── Helpers ── */
    void runAction(const QString& sys);
    void loadOfferte();
    void saveOfferte();
    void rebuildList();
    static QString offertePath();

    /* ── Widgets tab 0 (Assistente) ── */
    AiClient*     m_ai          = nullptr;
    QTextEdit*    m_input       = nullptr;
    QTextEdit*    m_output      = nullptr;
    QProgressBar* m_progress    = nullptr;
    QLabel*       m_status      = nullptr;
    QPushButton*  m_btnStop     = nullptr;
    QPushButton*  m_btnPdf      = nullptr;
    QLabel*       m_fotoLbl     = nullptr;
    QPushButton*  m_fotoBtn     = nullptr;
    bool          m_busy        = false;

    /* ── Widgets tab 1 (Tracker offerte) ── */
    QListWidget*  m_offerteList = nullptr;
    QPushButton*  m_addBtn      = nullptr;
    QPushButton*  m_delBtn      = nullptr;
    QJsonArray    m_offerte;

    /* ── Widgets tab 2 (Cover letter) ── */
    QTextEdit*   m_coverEdit     = nullptr;
    QPushButton* m_coverAiBtn    = nullptr;
    QPushButton* m_coverSaveBtn  = nullptr;
    QTextEdit*   m_coverOutput   = nullptr;
    QProgressBar* m_coverProgress = nullptr;
    bool          m_coverBusy    = false;

private slots:
    void onToken(const QString& t);
    void onFinished(const QString& f);
    void onError(const QString& e);
    void onAborted();
    void onLoadPdf();
    void onActionBtnClicked();
    void onFotoBtnClicked();
    void onEmailBtnClicked();

    /* tracker offerte */
    void onAddOfferta();
    void onDeleteOfferta();
    void onOffertaLongPressed(QListWidgetItem* item);

    /* cover letter */
    void onCoverAiClicked();
    void onCoverSaveClicked();
};

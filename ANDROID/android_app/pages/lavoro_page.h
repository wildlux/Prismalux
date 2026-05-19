#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include "../ai_client.h"

/* --------------------------------------------------------------
   LavoroPage -- Assistente AI per il lavoro.
   Azioni rapide: CV, lettera, colloquio, ricerca offerte, stipendio.
   -------------------------------------------------------------- */
class LavoroPage : public QWidget {
    Q_OBJECT
public:
    explicit LavoroPage(AiClient* ai, QWidget* parent = nullptr);

private:
    void runAction(const QString& sys);

    AiClient*     m_ai       = nullptr;
    QTextEdit*    m_input    = nullptr;
    QTextEdit*    m_output   = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel*       m_status   = nullptr;
    QPushButton*  m_btnStop  = nullptr;
    QPushButton*  m_btnPdf   = nullptr;
    bool          m_busy     = false;

private slots:
    void onToken(const QString& t);
    void onFinished(const QString& f);
    void onError(const QString& e);
    void onAborted();
    void onLoadPdf();
    void onActionBtnClicked();
};

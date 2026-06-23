#pragma once
#include <QWidget>
#include <QComboBox>
#include <QStackedWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   FinanzaPage — Calcolatori finanziari + chat AI.
   Sezioni: IVA · IRPEF · TFR · Chat AI Finanza · 730 · P.IVA
   ══════════════════════════════════════════════════════════════ */
class FinanzaPage : public QWidget {
    Q_OBJECT
public:
    explicit FinanzaPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onSectionChanged(int idx);
    void onCalcIva();
    void onCalcIrpef();
    void onCalcTfr();
    void onCalc730();
    void onCalcPiva();
    void onChatSend();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void onAborted();
    void setChatBusy(bool busy);

private:
    QWidget* buildIvaSection();
    QWidget* buildIrpefSection();
    QWidget* buildTfrSection();
    QWidget* buildChatSection();
    QWidget* build730Section();
    QWidget* buildPivaSection();

    AiClient*       m_ai         = nullptr;
    QComboBox*      m_secCombo   = nullptr;
    QStackedWidget* m_secStack   = nullptr;

    /* Sezione Chat AI */
    QTextEdit*   m_chatOutput  = nullptr;
    QTextEdit*   m_chatInput   = nullptr;
    QPushButton* m_chatSendBtn = nullptr;
    QPushButton* m_chatStopBtn = nullptr;
    QLabel*      m_chatStatus  = nullptr;
};

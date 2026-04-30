#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   RicercaPage — "Ricerca e Sviluppo"
   3 sotto-tab: Paper Scientifico · Brevetto · Documento Tecnico
   ══════════════════════════════════════════════════════════════ */
class RicercaPage : public QWidget {
    Q_OBJECT
public:
    explicit RicercaPage(AiClient* ai, QWidget* parent = nullptr);

private:
    AiClient*    m_ai;
    quint64      m_reqId         = 0;
    QTextEdit*   m_outCurrent    = nullptr;
    QPushButton* m_btnGenAttivo  = nullptr;
    QPushButton* m_btnStopAttivo = nullptr;

    QWidget* buildPaperTab();
    QWidget* buildBrevettoTab();
    QWidget* buildDocTecnicoTab();

    void avvia(const QString& sys, const QString& msg,
               QTextEdit* out, QPushButton* btnGen, QPushButton* btnStop);
    void resetButtons();

public:
    static void esportaPdf(QTextEdit* editor,
                           const QString& titolo, QWidget* parent);
    static void salvaMarkdown(QTextEdit* editor,
                              const QString& titolo, QWidget* parent);
};

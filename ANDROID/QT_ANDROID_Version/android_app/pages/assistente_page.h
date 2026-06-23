#pragma once
#include <QWidget>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QStackedWidget>
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   AssistentePage — Assistente AI a categorie
   Equivalente mobile di StrumentiPage desktop (main_tools.h).

   Categorie: Studio · Scrittura · Ricerca · Libri ·
              Produttività · Documenti

   Layout:
     QComboBox categoria
     Griglia bottoni azione (2 colonne, scroll)
     QTextEdit input
     [Avvia / Stop]
     QTextEdit output (streaming)
   ══════════════════════════════════════════════════════════════ */
class AssistentePage : public QWidget {
    Q_OBJECT
public:
    explicit AssistentePage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onCatChanged(int idx);
    void onActionClicked();
    void onRunClicked();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void onAborted();
    void setBusy(bool busy);

private:
    void buildActionGrid(int cat);
    static const char* sysPrompt(int cat, int action);

    AiClient*      m_ai;
    QComboBox*     m_catCombo    = nullptr;
    QStackedWidget*m_gridStack   = nullptr;  ///< una pagina per categoria
    QTextEdit*     m_input       = nullptr;
    QPushButton*   m_runBtn      = nullptr;
    QTextEdit*     m_output      = nullptr;
    QLabel*        m_statusLbl   = nullptr;

    int            m_selCat      = 0;
    int            m_selAction   = 0;
};

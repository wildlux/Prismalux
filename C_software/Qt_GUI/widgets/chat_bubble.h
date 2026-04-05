#pragma once
#include <QFrame>
#include <QTextBrowser>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

class ChartWidget;

/*
 * ChatBubble — singolo messaggio nella chat (utente o AI)
 *
 * Layout AI (sinistra, max 78% larghezza):
 *  ┌────────────────────────────────────────────┐
 *  │ 🤖 NomeModello                              │  ← sender label
 *  │ Testo risposta...                           │  ← QTextBrowser
 *  │ ┌──────────────────────────────────────┐   │  ← chart container
 *  │ │  [grafico QPainter]                  │   │     (opzionale)
 *  │ └──────────────────────────────────────┘   │
 *  │            [📊 Grafico] [📋 Copia] [🔊 Leggi] │  ← action bar
 *  └────────────────────────────────────────────┘
 *
 * Layout Utente (destra, max 78% larghezza):
 *                ┌──────────────────────────────┐
 *                │                          Tu  │
 *                │         Testo messaggio...   │
 *                │  [📋 Copia] [🔊 Leggi]        │
 *                └──────────────────────────────┘
 *
 * Streaming: appendToken() aggiorna il testo token per token senza
 * ricreare il documento (usa QTextCursor::insertText).
 * finalizeStream() al termine verifica se c'è una formula e mostra
 * il pulsante "📊 Grafico".
 */
class ChatBubble : public QFrame {
    Q_OBJECT
public:
    enum Role { User, AI };

    explicit ChatBubble(Role role,
                        const QString& sender,
                        const QString& text   = {},
                        QWidget*       parent = nullptr);

    /** Aggiunge un token in streaming. */
    void appendToken(const QString& token);

    /** Chiama alla fine dello stream: rileva formula e mostra il btn. */
    void finalizeStream();

    /** Testo plain corrente. */
    QString plainText() const { return m_plain; }

    /** Incorpora un ChartWidget sotto il testo (sostituisce l'eventuale precedente). */
    void embedChart(ChartWidget* chart);

signals:
    /** Utente ha cliccato "Mostra grafico": formula già estratta. */
    void chartRequested(const QString& formula);

private:
    void onCopy();
    void onTTS();

    Role          m_role;
    QString       m_plain;
    QTextBrowser* m_text          = nullptr;
    QWidget*      m_chartContainer = nullptr;  /* layout del grafico */
    QPushButton*  m_btnChart      = nullptr;
    QPushButton*  m_btnCopy       = nullptr;
    QPushButton*  m_btnTts        = nullptr;
};

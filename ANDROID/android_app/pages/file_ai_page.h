#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   FileAiPage — Carica PDF/TXT/MD e interroga l'AI sul contenuto.

   Layout:
     Bottone Carica File + label nome + anteprima
     Pillole azioni rapide scrollabili
     QTextEdit output streaming
     QTextEdit input + Invia/Stop
     Label status
   ══════════════════════════════════════════════════════════════ */
class FileAiPage : public QWidget {
    Q_OBJECT
public:
    explicit FileAiPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onLoadFile();
    void onQuickClicked();
    void onSendClicked();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void onAborted();
    void setBusy(bool busy);

private:
    static QString extractPdfText(const QByteArray& raw);
    void applyFile(const QString& name, const QString& content);

    AiClient*    m_ai         = nullptr;
    QLabel*      m_fileLbl    = nullptr;
    QLabel*      m_previewLbl = nullptr;
    QTextEdit*   m_output     = nullptr;
    QTextEdit*   m_input      = nullptr;
    QPushButton* m_sendBtn    = nullptr;
    QPushButton* m_stopBtn    = nullptr;
    QLabel*      m_statusLbl  = nullptr;

    QString m_fileName;
    QString m_fileContent;
};

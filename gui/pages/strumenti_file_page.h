#pragma once
#include <QWidget>
#include <QLabel>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QTextBrowser>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include "../ai_client.h"

class StrumentiFilePage : public QWidget {
    Q_OBJECT
public:
    explicit StrumentiFilePage(AiClient* ai, QWidget* parent = nullptr);
private:
    AiClient* m_ai = nullptr;
    // File Search
    QLineEdit*    m_fileSearchDir   = nullptr;
    QLineEdit*    m_fileSearchQuery = nullptr;
    QTextBrowser* m_fileSearchOut   = nullptr;
    QProcess*     m_fileSearchProc  = nullptr;
    QPushButton*  m_fileSearchBtn   = nullptr;  ///< btnSearch — captured by timer/finished
    QPushButton*  m_fileSearchAiBtn = nullptr;  ///< btnAI — captured by finished/timer
    // Wiki
    QLineEdit*    m_wikiQuery    = nullptr;
    QComboBox*    m_wikiLangCombo= nullptr;
    QTextBrowser* m_wikiContent  = nullptr;
    QLabel*       m_wikiWaitLbl  = nullptr;
    QString       m_wikiExtract;
    QPushButton*  m_wikiElaboraBtn = nullptr;  ///< per abilitare dopo fetch
    QWidget*      m_wikiActRow     = nullptr;  ///< riga bottoni azione Wiki
    QNetworkAccessManager* m_wikiNam = nullptr;
    QString       m_wikiLastQuery;             ///< query corrente per la fetch reply
    QString       m_wikiLastLang;              ///< lingua corrente per la fetch reply
    // Dati AI (Excel/CSV)
    QLabel*      m_datiFileLbl      = nullptr;
    QComboBox*   m_datiActionCombo  = nullptr;
    QTextEdit*   m_datiPreview      = nullptr;
    QTextEdit*   m_datiOutput       = nullptr;
    QString      m_datiCsvText;
    // PDF
    QLabel*      m_pdfFileLbl     = nullptr;
    QComboBox*   m_pdfActionCombo = nullptr;
    QTextEdit*   m_pdfOutput      = nullptr;
    QString      m_pdfPath;
    QString      m_pdfText;
    // Word/Testo
    QTextEdit*   m_wordOutputEdit   = nullptr;
    QComboBox*   m_wordActionCombo  = nullptr;
    QString      m_wordFileContent;
    QLabel*      m_wordFileLbl      = nullptr;

    QWidget* buildFileSearchTab();
    QWidget* buildWikiTab();
    QWidget* buildDatiTab();
    QWidget* buildPdfTab();
    QWidget* buildWordTab();

    // Helper: avvia analisi PDF con testo già estratto
    void runPdfAnalysis(const QString& text);

private slots:
    // File Search
    void onFileSearchBrowseClicked();
    void onFileSearchSearchClicked();
    void onFileSearchProcFinished(int exitCode, QProcess::ExitStatus status);
    void onFileSearchTimeout();
    void onFileSearchAiClicked();
    void onFileSearchAiToken(const QString& t);
    void onFileSearchAiError(const QString& msg);

    // Wiki
    void onWikiActBtnClicked();
    void onWikiFetchClicked();
    void onWikiReplyFinished();
    void onWikiElaboraClicked();
    void onWikiAiToken(const QString& t);
    void onWikiAiError(const QString& msg);
    void onWikiActAiToken(const QString& t);
    void onWikiActAiError(const QString& msg);

    // Dati AI
    void onDatiFileBtnClicked();
    void onDatiXlsProcFinished(int code, QProcess::ExitStatus status);
    void onDatiPreviewBtnClicked();
    void onDatiAnalyzeBtnClicked();
    void onDatiAiToken(const QString& t);
    void onDatiAiError(const QString& msg);

    // PDF
    void onPdfFileBtnClicked();
    void onPdfAnalyzeBtnClicked();
    void onPdfPdfttextFinished(int code, QProcess::ExitStatus status);
    void onPdfPlumberFinished(int code, QProcess::ExitStatus status);
    void onPdfAiToken(const QString& t);
    void onPdfAiError(const QString& msg);

    // Word/Testo
    void onWordFileBtnClicked();
    void onWordDocxProcFinished(int code, QProcess::ExitStatus status);
    void onWordOdtProcFinished(int code, QProcess::ExitStatus status);
    void onWordAnalyzeBtnClicked();
    void onWordAiToken(const QString& t);
    void onWordAiError(const QString& msg);
};

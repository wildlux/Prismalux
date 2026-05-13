#pragma once
#include <QWidget>
#include <QLabel>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QTextBrowser>
#include <QProcess>
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
    // Wiki
    QLineEdit*    m_wikiQuery    = nullptr;
    QComboBox*    m_wikiLangCombo= nullptr;
    QTextBrowser* m_wikiContent  = nullptr;
    QLabel*       m_wikiWaitLbl  = nullptr;
    QString       m_wikiExtract;
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
    QWidget* buildFileSearchTab();
    QWidget* buildWikiTab();
    QWidget* buildDatiTab();
    QWidget* buildPdfTab();
    QWidget* buildWordTab();
};

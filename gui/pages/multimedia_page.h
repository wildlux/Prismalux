#pragma once
#include <QWidget>
#include <QLabel>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QProcess>
#include "../ai_client.h"
#include "../widgets/ai_error_widget.h"

class MultimediaPage : public QWidget {
    Q_OBJECT
public:
    explicit MultimediaPage(AiClient* ai, QWidget* parent = nullptr);
private:
    AiClient* m_ai = nullptr;
    // Audio AI
    QLabel*      m_audioFileLbl     = nullptr;
    QComboBox*   m_audioActionCombo = nullptr;
    QTextEdit*   m_audioTranscript  = nullptr;
    QTextEdit*   m_audioOutput      = nullptr;
    QProcess*    m_audioProc        = nullptr;
    QString      m_audioFilePath;
    QPushButton* m_recBtn           = nullptr;
    QProcess*    m_recProc          = nullptr;
    // Graphviz
    QTextEdit*   m_graphvizInput    = nullptr;
    QLabel*      m_graphvizImg      = nullptr;
    QLabel*      m_graphvizStatus   = nullptr;
    QProcess*    m_graphvizProc     = nullptr;
    QString      m_graphvizTmpPng;
    AiErrorWidget* m_graphvizErr   = nullptr;
    AiErrorWidget* m_audioErr      = nullptr;
    QWidget* buildAudioTab();
    QWidget* buildSDTab();
    QWidget* buildGraphvizTab();
    void runGraphvizAi();
};

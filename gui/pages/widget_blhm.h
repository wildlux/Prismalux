#pragma once
#include <QWidget>
#include "../blhm_engine.h"
class QTableWidget;
class QLineEdit;
class QTextEdit;
class QLabel;
class QDoubleSpinBox;
class Rab0lCanvas;

class BlhmWidget : public QWidget {
    Q_OBJECT
public:
    explicit BlhmWidget(QWidget* parent = nullptr);
    ~BlhmWidget() override;

private:
    /* Calcolatore */
    QTableWidget* m_table  = nullptr;
    QLineEdit*    m_query  = nullptr;
    QTextEdit*    m_output = nullptr;

    /* Note */
    QTextEdit*    m_noteEdit = nullptr;

    /* DNA */
    Rab0lCanvas*  m_dnaCanvas = nullptr;
    QLineEdit*    m_dnaSeq1   = nullptr;
    QLineEdit*    m_dnaSeq2   = nullptr;
    QLabel*       m_dnaSimLbl = nullptr;

    /* Engine C */
    BLHMGraph*      m_graph          = nullptr;
    QLabel*         m_engineStatus   = nullptr;
    QLabel*         m_engineLatency  = nullptr;
    QLabel*         m_engineFactory  = nullptr;
    QLabel*         m_engineLink     = nullptr;
    QLabel*         m_engineUser     = nullptr;
    QLabel*         m_engineCombined = nullptr;
    QDoubleSpinBox* m_engineLrSpin   = nullptr;
    QTextEdit*      m_engineAutoftOut= nullptr;

private slots:
    void onComputeClicked();
    void onNoteSave();
    void onNoteLoad();
    void onAddRowClicked();
    void onDeleteRowClicked();
    void onNotesClearClicked();
    void onDnaClearClicked();
    void onDnaAnalyzeClicked();
    void onEngineSyncClicked();
    void onEngineRunClicked();
    void onEngineAutoftClicked();
    void onEngineSaveClicked();
    void onEngineLoadClicked();
};

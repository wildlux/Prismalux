#pragma once
#include <QWidget>
class Rab0lCanvas;
class QLineEdit;
class QLabel;

class Rab0lWidget : public QWidget {
    Q_OBJECT
public:
    explicit Rab0lWidget(QWidget* parent = nullptr);

private:
    Rab0lCanvas* m_canvas  = nullptr;
    QLineEdit*   m_seq1    = nullptr;
    QLineEdit*   m_seq2    = nullptr;
    QLabel*      m_simLbl  = nullptr;

private slots:
    void onAnalyzeClicked();
    void onClearClicked();
};

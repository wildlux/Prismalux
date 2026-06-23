#pragma once
#include <QWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QSlider>
#include <QProcess>
#include <QTimer>
#include <QVector>

class QGroupBox;
class QGridLayout;
class QHBoxLayout;

struct Tono {
    double  freq;
    int     dur;
    QString onda;
    double  vol;
    QString label() const;
};

/* Oscilloscopio animato — disegna la forma d'onda corrente */
class OscoCanvas : public QWidget {
    Q_OBJECT
public:
    explicit OscoCanvas(QWidget* parent = nullptr);
    void setTono(double freq, const QString& onda, double vol);
    void setAnimating(bool on);
protected:
    void paintEvent(QPaintEvent*) override;
private slots:
    void onTimerTick();
private:
    double  m_freq  = 440.0;
    QString m_onda  = "sine";
    double  m_vol   = 0.7;
    double  m_phase = 0.0;
    QTimer* m_timer = nullptr;
};

/* Widget completo: programmatore + assemblatore visuale */
class SintetizzatoreWidget : public QWidget {
    Q_OBJECT
public:
    explicit SintetizzatoreWidget(QWidget* parent = nullptr);

private slots:
    void onAggiungi();
    void onRimuovi();
    void onPreview();
    void onPlaySequenza();
    void onStop();
    void onPlayFinished(int code, QProcess::ExitStatus status);
    void onSeqSel();
    void onSalvaSeq();
    void onCaricaSeq();
    void onSalvaWav();

private:
    /* — UI — */
    QSpinBox*     m_freqSpin  = nullptr;
    QSpinBox*     m_durSpin   = nullptr;
    QComboBox*    m_ondaCombo = nullptr;
    QSlider*      m_volSlider = nullptr;
    QLabel*       m_volLbl    = nullptr;
    QListWidget*  m_seqList   = nullptr;
    QPushButton*  m_playBtn   = nullptr;
    QPushButton*  m_stopBtn   = nullptr;
    QLabel*       m_statusLbl = nullptr;
    OscoCanvas*   m_canvas    = nullptr;
    QProcess*     m_aplay     = nullptr;
    QVector<Tono> m_seq;
    QString       m_tmpWav;

    /* — Costruzione UI (stepdown) — */
    void         buildLayout();
    QLabel*      buildTitle();
    QWidget*     buildSplitter();
    QWidget*     buildProgrammatoreCard(QWidget* parent);
    QWidget*     buildAssemblatoreCard(QWidget* parent);
    QGridLayout* buildParametriGrid(QWidget* parent);
    QHBoxLayout* buildAzioniRow(QWidget* parent);
    QHBoxLayout* buildSalvaRow(QWidget* parent);
    QHBoxLayout* buildControlRow(QWidget* parent);
    void         setupConnections();

    /* — Audio — */
    QByteArray makeWav(const QVector<Tono>& toni) const;
    void       playWav(const QVector<Tono>& toni, const QString& label);
    void       refreshList();
};

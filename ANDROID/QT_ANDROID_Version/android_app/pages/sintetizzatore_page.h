#pragma once
#include <QWidget>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QSlider>
#include <QByteArray>
#include <QVector>
#include <QBuffer>

/* QAudioSink (raw PCM playback) è in Qt6::Multimedia.
   Viene compilato solo se HAVE_MULTIMEDIA è definito. */
#ifdef HAVE_MULTIMEDIA
#include <QAudioSink>
#include <QAudioFormat>
#endif

/* ── TonoDef — rappresenta un singolo tono della sequenza ── */
struct TonoDef {
    double  freq = 440.0;   /* frequenza in Hz */
    int     dur  = 500;     /* durata in ms */
    QString onda = "sine";  /* sine | square | triangle | sawtooth */
    double  vol  = 0.7;     /* 0.0 – 1.0 */

    QString label() const;
};

/* ──────────────────────────────────────────────────────────────
   SintetizzatorePage

   Pagina Android del sintetizzatore toni.
   Genera audio PCM 44100 Hz 16-bit mono in memoria e lo
   riproduce via QAudioSink (Qt6 Multimedia).
   Non include oscilloscopio (troppo pesante su mobile).
   –––––––––––––––––––––––––––––––––––––––––––––––––––––––––––– */
class SintetizzatorePage : public QWidget {
    Q_OBJECT
public:
    explicit SintetizzatorePage(QWidget* parent = nullptr);

private slots:
    void onAggiungi();
    void onRimuovi();
    void onPreview();
    void onPlaySequenza();
    void onStop();
    void onVolChanged(int v);
#ifdef HAVE_MULTIMEDIA
    void onAudioStateChanged(QAudio::State state);
#endif

private:
    /* widget */
    QSpinBox*    m_freqSpin  = nullptr;
    QSpinBox*    m_durSpin   = nullptr;
    QComboBox*   m_ondaCombo = nullptr;
    QSlider*     m_volSlider = nullptr;
    QLabel*      m_volLbl    = nullptr;
    QListWidget* m_seqList   = nullptr;
    QPushButton* m_addBtn    = nullptr;
    QPushButton* m_prevBtn   = nullptr;
    QPushButton* m_remBtn    = nullptr;
    QPushButton* m_playBtn   = nullptr;
    QPushButton* m_stopBtn   = nullptr;
    QLabel*      m_statusLbl = nullptr;

    /* dati */
    QVector<TonoDef> m_seq;

    /* audio — QAudioSink esegue playback PCM raw da QIODevice */
#ifdef HAVE_MULTIMEDIA
    QAudioSink* m_audioSink = nullptr;
#endif
    QBuffer*    m_audioBuf  = nullptr;

    /* helpers */
    QByteArray makeWav(const QVector<TonoDef>& toni) const;
    void       playWav(const QVector<TonoDef>& toni, const QString& label);
    void       stopAudio();
    void       refreshList();
    void       setPlayingState(bool playing);
};

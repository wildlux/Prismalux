#include "sintetizzatore_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── WAV helpers ──────────────────────────────────────────────────────────── */
static void u32le(QByteArray& b, quint32 v)
{
    b.append(char(v & 0xFF));
    b.append(char((v >>  8) & 0xFF));
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 24) & 0xFF));
}

static void u16le(QByteArray& b, quint16 v)
{
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
}

/* ── TonoDef::label ───────────────────────────────────────────────────────── */
QString TonoDef::label() const
{
    const QString ondaNome =
        onda == "sine"     ? "sinusoidale" :
        onda == "square"   ? "quadra"      :
        onda == "triangle" ? "triangolare" : "dente di sega";
    /* U+00B7 = MIDDLE DOT (·) — codifica UTF-8: 0xC2 0xB7 */
    return QString("%1 Hz \xc2\xb7 %2 ms \xc2\xb7 %3 \xc2\xb7 vol %4%")
        .arg((int)freq).arg(dur).arg(ondaNome).arg((int)(vol * 100));
}

/* ════════════════════════════════════════════════════════════════════════════
   SintetizzatorePage::makeWav
   Genera RIFF WAV PCM 44100 Hz 16-bit mono con fade-in/out di 20 ms
   (logica identica a sintetizzatore_widget.cpp della versione desktop).
   ════════════════════════════════════════════════════════════════════════════ */
QByteArray SintetizzatorePage::makeWav(const QVector<TonoDef>& toni) const
{
    const int SR = 44100;
    QByteArray pcm;

    for (const TonoDef& t : toni) {
        const int n    = qMax(1, (int)(t.dur * SR / 1000.0));
        const int fade = qMin(882, n / 4);   /* 20 ms fade o 25% del tono */

        for (int i = 0; i < n; i++) {
            const double ph = 2.0 * M_PI * t.freq * i / SR;
            double s = 0.0;

            if      (t.onda == "sine")
                s = std::sin(ph);
            else if (t.onda == "square")
                s = std::fmod(ph, 2.0 * M_PI) < M_PI ? 1.0 : -1.0;
            else if (t.onda == "triangle")
                s = 2.0 / M_PI * std::asin(std::sin(ph));
            else /* sawtooth */
                s = 2.0 * (std::fmod(ph, 2.0 * M_PI) / (2.0 * M_PI)) - 1.0;

            double env = 1.0;
            if      (i < fade)          env = (double)i / fade;
            else if (i > n - fade)      env = (double)(n - i) / fade;

            auto sample = (qint16)(s * t.vol * env * 32767.0);
            pcm.append(char(sample & 0xFF));
            pcm.append(char((sample >> 8) & 0xFF));
        }
    }

    QByteArray wav;
    const quint32 dataSize = (quint32)pcm.size();

    wav += "RIFF";   u32le(wav, 36 + dataSize);
    wav += "WAVE";
    wav += "fmt ";   u32le(wav, 16);
    u16le(wav, 1);   /* PCM */
    u16le(wav, 1);   /* mono */
    u32le(wav, SR);
    u32le(wav, SR * 2);
    u16le(wav, 2);   /* block align */
    u16le(wav, 16);  /* bits/sample */
    wav += "data";   u32le(wav, dataSize);
    wav += pcm;

    return wav;
}

/* ════════════════════════════════════════════════════════════════════════════
   Costruttore
   ════════════════════════════════════════════════════════════════════════════ */
SintetizzatorePage::SintetizzatorePage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("SintetizzatorePage");

    /* Scroll area — garantisce usabilità su schermi piccoli */
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);
    scroll->setWidget(inner);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    /* ── Titolo ── */
    /* U+1F50A = 🔊 — UTF-8: F0 9F 94 8A */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\x8a  Sintetizzatore Toni"), inner);
    QFont tf = title->font();
    tf.setPointSize(14); tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    /* ══════════════════════════════════════════════════════════
       Card 1 — Programmatore Toni
       ══════════════════════════════════════════════════════════ */
    /* U+1F3B9 = 🎹 — UTF-8: F0 9F 8E B9 */
    auto* addGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x8e\xb9  Programmatore Toni"), inner);
    auto* addLay = new QVBoxLayout(addGroup);
    addLay->setSpacing(8);

    auto* grid = new QGridLayout;
    grid->setSpacing(8);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    /* Frequenza */
    grid->addWidget(new QLabel("Frequenza (Hz):", addGroup), 0, 0);
    m_freqSpin = new QSpinBox(addGroup);
    m_freqSpin->setRange(20, 20000);
    m_freqSpin->setValue(440);
    m_freqSpin->setSingleStep(10);
    m_freqSpin->setObjectName("spinBox");
    grid->addWidget(m_freqSpin, 0, 1);

    /* Durata */
    grid->addWidget(new QLabel("Durata (ms):", addGroup), 0, 2);
    m_durSpin = new QSpinBox(addGroup);
    m_durSpin->setRange(50, 10000);
    m_durSpin->setValue(500);
    m_durSpin->setSingleStep(50);
    m_durSpin->setObjectName("spinBox");
    grid->addWidget(m_durSpin, 0, 3);

    /* Forma d'onda — U+2019 = ' (apostrofo curvo) */
    grid->addWidget(new QLabel(
        QString::fromUtf8("Forma d\xe2\x80\x99onda:"), addGroup), 1, 0);
    m_ondaCombo = new QComboBox(addGroup);
    m_ondaCombo->addItem("Sinusoidale",   "sine");
    m_ondaCombo->addItem("Quadra",        "square");
    m_ondaCombo->addItem("Triangolare",   "triangle");
    m_ondaCombo->addItem("Dente di sega", "sawtooth");
    grid->addWidget(m_ondaCombo, 1, 1);

    /* Volume */
    m_volLbl = new QLabel("Volume: 70%", addGroup);
    grid->addWidget(m_volLbl, 1, 2);
    m_volSlider = new QSlider(Qt::Horizontal, addGroup);
    m_volSlider->setRange(0, 100);
    m_volSlider->setValue(70);
    grid->addWidget(m_volSlider, 1, 3);

    addLay->addLayout(grid);

    /* Bottoni riga: Aggiungi · Ascolta · Rimuovi */
    auto* btnRow1 = new QHBoxLayout;
    m_addBtn  = new QPushButton("+ Aggiungi", addGroup);
    m_addBtn->setObjectName("actionBtn");
    /* U+1F50A = 🔊 */
    m_prevBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8a  Ascolta"), addGroup);
    m_prevBtn->setObjectName("actionBtn");
    /* U+1F5D1 = 🗑 — UTF-8: F0 9F 97 91 */
    m_remBtn  = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x97\x91  Rimuovi"), addGroup);
    m_remBtn->setObjectName("actionBtn");
    btnRow1->addWidget(m_addBtn);
    btnRow1->addWidget(m_prevBtn);
    btnRow1->addWidget(m_remBtn);
    addLay->addLayout(btnRow1);

    vbox->addWidget(addGroup);

    /* ══════════════════════════════════════════════════════════
       Card 2 — Sequenza
       ══════════════════════════════════════════════════════════ */
    /* U+1F4CB = 📋 — UTF-8: F0 9F 93 8B */
    auto* seqGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x8b  Sequenza Toni"), inner);
    auto* seqLay = new QVBoxLayout(seqGroup);
    seqLay->setSpacing(8);

    m_seqList = new QListWidget(seqGroup);
    m_seqList->setObjectName("chatLog");
    m_seqList->setMinimumHeight(120);
    seqLay->addWidget(m_seqList, 1);

    /* Bottoni riga: Riproduci · Stop */
    auto* btnRow2 = new QHBoxLayout;
    /* U+25B6 = ▶ — UTF-8: E2 96 B6 */
    m_playBtn = new QPushButton(
        QString::fromUtf8("\xe2\x96\xb6  Riproduci Sequenza"), seqGroup);
    m_playBtn->setObjectName("actionBtn");
    /* U+23F9 = ⏹ — UTF-8: E2 8F B9 */
    m_stopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9  Stop"), seqGroup);
    m_stopBtn->setObjectName("actionBtn");
    m_stopBtn->setEnabled(false);
    btnRow2->addWidget(m_playBtn);
    btnRow2->addWidget(m_stopBtn);
    seqLay->addLayout(btnRow2);

    vbox->addWidget(seqGroup);

    /* ── Status ── */
    m_statusLbl = new QLabel(
        "Aggiungi toni e premi Riproduci", inner);
    m_statusLbl->setObjectName("hintLabel");
    m_statusLbl->setAlignment(Qt::AlignCenter);
    m_statusLbl->setWordWrap(true);
    vbox->addWidget(m_statusLbl);

#ifndef HAVE_MULTIMEDIA
    /* Avviso visibile se Qt Multimedia non è presente */
    /* U+26A0 = ⚠ — UTF-8: E2 9A A0 */
    auto* warnLbl = new QLabel(
        QString::fromUtf8(
            "\xe2\x9a\xa0  Qt Multimedia non disponibile: "
            "audio disabilitato."), inner);
    warnLbl->setWordWrap(true);
    warnLbl->setAlignment(Qt::AlignCenter);
    vbox->addWidget(warnLbl);
    m_playBtn->setEnabled(false);
    m_prevBtn->setEnabled(false);
#endif

    vbox->addStretch();

    /* ── Connessioni — context object esplicito (regola progetto) ── */
    connect(m_volSlider, &QSlider::valueChanged,
            this, &SintetizzatorePage::onVolChanged);

    connect(m_addBtn,  &QPushButton::clicked,
            this, &SintetizzatorePage::onAggiungi);
    connect(m_prevBtn, &QPushButton::clicked,
            this, &SintetizzatorePage::onPreview);
    connect(m_remBtn,  &QPushButton::clicked,
            this, &SintetizzatorePage::onRimuovi);
    connect(m_playBtn, &QPushButton::clicked,
            this, &SintetizzatorePage::onPlaySequenza);
    connect(m_stopBtn, &QPushButton::clicked,
            this, &SintetizzatorePage::onStop);
}

/* ── onVolChanged ─────────────────────────────────────────────────────────── */
void SintetizzatorePage::onVolChanged(int v)
{
    m_volLbl->setText(QString("Volume: %1%").arg(v));
}

/* ── onAggiungi ───────────────────────────────────────────────────────────── */
void SintetizzatorePage::onAggiungi()
{
    TonoDef t;
    t.freq = m_freqSpin->value();
    t.dur  = m_durSpin->value();
    t.onda = m_ondaCombo->currentData().toString();
    t.vol  = m_volSlider->value() / 100.0;
    m_seq.append(t);
    refreshList();
}

/* ── onRimuovi ────────────────────────────────────────────────────────────── */
void SintetizzatorePage::onRimuovi()
{
    const int row = m_seqList->currentRow();
    if (row < 0 || row >= m_seq.size()) return;
    m_seq.remove(row);
    refreshList();
}

/* ── onPreview ────────────────────────────────────────────────────────────── */
void SintetizzatorePage::onPreview()
{
    TonoDef t;
    t.freq = m_freqSpin->value();
    t.dur  = m_durSpin->value();
    t.onda = m_ondaCombo->currentData().toString();
    t.vol  = m_volSlider->value() / 100.0;
    playWav({t}, QString("Anteprima %1 Hz").arg((int)t.freq));
}

/* ── onPlaySequenza ───────────────────────────────────────────────────────── */
void SintetizzatorePage::onPlaySequenza()
{
    if (m_seq.isEmpty()) {
        /* U+26A0 = ⚠ */
        m_statusLbl->setText(
            QString::fromUtf8(
                "\xe2\x9a\xa0  Aggiungi almeno un tono alla sequenza"));
        return;
    }
    playWav(m_seq, QString("%1 toni").arg(m_seq.size()));
}

/* ── onStop ───────────────────────────────────────────────────────────────── */
void SintetizzatorePage::onStop()
{
    stopAudio();
    m_statusLbl->setText("Fermato");
}

/* ── onAudioStateChanged ──────────────────────────────────────────────────── */
#ifdef HAVE_MULTIMEDIA
void SintetizzatorePage::onAudioStateChanged(QAudio::State state)
{
    if (state == QAudio::IdleState || state == QAudio::StoppedState) {
        /* Riproduzione terminata naturalmente o per errore */
        if (m_audioSink && state == QAudio::IdleState) {
            m_audioSink->stop();
        }
        setPlayingState(false);
        /* U+2705 = ✅ — UTF-8: E2 9C 85 */
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9c\x85  Riproduzione completata"));
    }
}
#endif

/* ── refreshList ──────────────────────────────────────────────────────────── */
void SintetizzatorePage::refreshList()
{
    const int sel = m_seqList->currentRow();
    m_seqList->clear();
    for (int i = 0; i < m_seq.size(); i++)
        m_seqList->addItem(QString("%1. %2").arg(i + 1).arg(m_seq[i].label()));
    if (sel >= 0 && sel < m_seqList->count())
        m_seqList->setCurrentRow(sel);
    m_statusLbl->setText(m_seq.isEmpty()
        ? "Aggiungi toni e premi Riproduci"
        : QString("%1 toni in sequenza").arg(m_seq.size()));
}

/* ── setPlayingState ──────────────────────────────────────────────────────── */
void SintetizzatorePage::setPlayingState(bool playing)
{
    m_playBtn->setEnabled(!playing);
    m_stopBtn->setEnabled(playing);
    m_addBtn->setEnabled(!playing);
    m_prevBtn->setEnabled(!playing);
    m_remBtn->setEnabled(!playing);
}

/* ── stopAudio ────────────────────────────────────────────────────────────── */
void SintetizzatorePage::stopAudio()
{
#ifdef HAVE_MULTIMEDIA
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink->deleteLater();
        m_audioSink = nullptr;
    }
#endif
    if (m_audioBuf) {
        m_audioBuf->close();
        m_audioBuf->deleteLater();
        m_audioBuf = nullptr;
    }
    setPlayingState(false);
}

/* ── playWav ──────────────────────────────────────────────────────────────── */
void SintetizzatorePage::playWav(const QVector<TonoDef>& toni, const QString& label)
{
#ifdef HAVE_MULTIMEDIA
    /* Ferma l'eventuale riproduzione in corso */
    stopAudio();

    const QByteArray wav = makeWav(toni);

    /* QBuffer — wrappa i dati WAV come QIODevice per QAudioSink */
    m_audioBuf = new QBuffer(this);
    m_audioBuf->setData(wav);
    m_audioBuf->open(QIODevice::ReadOnly);

    /* QAudioSink — playback PCM raw (Qt6 Multimedia) */
    QAudioFormat fmt;
    fmt.setSampleRate(44100);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);

    m_audioSink = new QAudioSink(fmt, this);
    connect(m_audioSink, &QAudioSink::stateChanged,
            this, &SintetizzatorePage::onAudioStateChanged);

    m_audioSink->start(m_audioBuf);
    setPlayingState(true);
    /* U+25B6 = ▶ */
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x96\xb6  ") + label + "...");
#else
    Q_UNUSED(toni)
    Q_UNUSED(label)
    /* U+274C = ❌ — UTF-8: E2 9D 8C */
    m_statusLbl->setText(
        QString::fromUtf8(
            "\xe2\x9d\x8c  Audio non disponibile (Qt Multimedia assente)"));
#endif
}

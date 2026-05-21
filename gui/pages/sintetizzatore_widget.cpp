#include "sintetizzatore_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QFile>
#include <QDir>
#include <QPainter>
#include <QPolygonF>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Tono::label ──────────────────────────────────────────── */
QString Tono::label() const {
    const QString ondaNome =
        onda == "sine"     ? "sinusoidale" :
        onda == "square"   ? "quadra"      :
        onda == "triangle" ? "triangolare" : "dente di sega";
    return QString("%1 Hz · %2 ms · %3 · vol %4%")
        .arg((int)freq).arg(dur).arg(ondaNome).arg((int)(vol * 100));
}

/* ── OscoCanvas ───────────────────────────────────────────── */
OscoCanvas::OscoCanvas(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(130);
    m_timer = new QTimer(this);
    m_timer->setInterval(33);
    connect(m_timer, &QTimer::timeout, this, [this]{
        m_phase += 2.0 * M_PI * m_freq * 0.033 / 8.0;
        if (m_phase > 2.0 * M_PI * 100) m_phase = 0.0;
        update();
    });
}

void OscoCanvas::setTono(double freq, const QString& onda, double vol)
{
    m_freq = freq; m_onda = onda; m_vol = vol;
    update();
}

void OscoCanvas::setAnimating(bool on)
{
    on ? m_timer->start() : m_timer->stop();
    if (!on) m_phase = 0.0;
    update();
}

void OscoCanvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int W = width(), H = height();
    const int mid = H / 2;

    p.fillRect(rect(), QColor(0x0f, 0x11, 0x17));

    /* griglia */
    p.setPen(QPen(QColor(0x2a, 0x2d, 0x4e), 1, Qt::DotLine));
    p.drawLine(0, mid, W, mid);
    for (int y = H / 4; y < H; y += H / 4)
        p.drawLine(0, y, W, y);

    /* forma d'onda — 3 cicli visibili */
    const QColor acc(0x6c, 0x63, 0xff);
    p.setPen(QPen(acc, 2));

    QPolygonF poly;
    poly.reserve(W);
    for (int i = 0; i < W; i++) {
        const double t    = (double)i / W;
        const double ph   = m_phase + t * 3.0 * 2.0 * M_PI;
        double s = 0.0;
        if      (m_onda == "sine")
            s = std::sin(ph);
        else if (m_onda == "square")
            s = std::fmod(ph, 2.0 * M_PI) < M_PI ? 1.0 : -1.0;
        else if (m_onda == "triangle")
            s = 2.0 / M_PI * std::asin(std::sin(ph));
        else /* sawtooth */
            s = 2.0 * (std::fmod(ph, 2.0 * M_PI) / (2.0 * M_PI)) - 1.0;
        poly << QPointF(i, mid - s * m_vol * (H / 2.0 - 10));
    }
    p.drawPolyline(poly);

    /* etichetta */
    p.setPen(QColor(0x66, 0x66, 0x88));
    p.setFont(QFont("monospace", 9));
    p.drawText(6, H - 6, QString("%1 Hz · %2").arg((int)m_freq).arg(m_onda));
}

/* ── WAV helper ───────────────────────────────────────────── */
static void u32le(QByteArray& b, quint32 v)
{
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 24) & 0xFF));
}
static void u16le(QByteArray& b, quint16 v)
{
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
}

QByteArray SintetizzatoreWidget::makeWav(const QVector<Tono>& toni) const
{
    const int SR = 44100;
    QByteArray pcm;

    for (const Tono& t : toni) {
        const int n    = qMax(1, (int)(t.dur * SR / 1000.0));
        const int fade = qMin(882, n / 4);   /* 20 ms fade o 25 % del tono */

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
            if (i < fade)          env = (double)i / fade;
            else if (i > n - fade) env = (double)(n - i) / fade;

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

/* ── SintetizzatoreWidget ─────────────────────────────────── */
SintetizzatoreWidget::SintetizzatoreWidget(QWidget* parent) : QWidget(parent)
{
    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(12, 12, 12, 12);
    mainLay->setSpacing(10);

    auto* title = new QLabel(
        "\xf0\x9f\x8e\xb5  <b>Sintetizzatore Toni Puri</b>", this);
    title->setTextFormat(Qt::RichText);
    title->setObjectName("sectionTitle");
    mainLay->addWidget(title);

    auto* splitter = new QSplitter(Qt::Vertical, this);

    /* ── Card 1: Programmatore ── */
    auto* prog = new QGroupBox(
        "\xf0\x9f\x8e\xb9  Programmatore Toni", splitter);
    auto* pLay = new QVBoxLayout(prog);
    pLay->setSpacing(8);

    auto* grid = new QGridLayout();
    grid->setSpacing(8);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    grid->addWidget(new QLabel("Frequenza (Hz):"), 0, 0);
    m_freqSpin = new QSpinBox(prog);
    m_freqSpin->setRange(20, 20000);
    m_freqSpin->setValue(440);
    m_freqSpin->setSingleStep(10);
    m_freqSpin->setObjectName("spinBox");
    grid->addWidget(m_freqSpin, 0, 1);

    grid->addWidget(new QLabel("Durata (ms):"), 0, 2);
    m_durSpin = new QSpinBox(prog);
    m_durSpin->setRange(50, 10000);
    m_durSpin->setValue(500);
    m_durSpin->setSingleStep(50);
    m_durSpin->setObjectName("spinBox");
    grid->addWidget(m_durSpin, 0, 3);

    grid->addWidget(new QLabel("Forma d\xe2\x80\x99onda:"), 1, 0);
    m_ondaCombo = new QComboBox(prog);
    m_ondaCombo->addItem("Sinusoidale",   "sine");
    m_ondaCombo->addItem("Quadra",        "square");
    m_ondaCombo->addItem("Triangolare",   "triangle");
    m_ondaCombo->addItem("Dente di sega", "sawtooth");
    grid->addWidget(m_ondaCombo, 1, 1);

    m_volLbl = new QLabel("Volume: 70%", prog);
    grid->addWidget(m_volLbl, 1, 2);
    m_volSlider = new QSlider(Qt::Horizontal, prog);
    m_volSlider->setRange(0, 100);
    m_volSlider->setValue(70);
    grid->addWidget(m_volSlider, 1, 3);

    pLay->addLayout(grid);

    auto* btnRow = new QHBoxLayout();
    auto* addBtn  = new QPushButton("+ Aggiungi", prog);
    addBtn->setObjectName("actionBtn");
    auto* prevBtn = new QPushButton(
        "\xf0\x9f\x94\x8a  Ascolta", prog);
    prevBtn->setObjectName("actionBtn");
    auto* remBtn  = new QPushButton(
        "\xf0\x9f\x97\x91  Rimuovi", prog);
    btnRow->addWidget(addBtn);
    btnRow->addWidget(prevBtn);
    btnRow->addWidget(remBtn);
    btnRow->addStretch();
    pLay->addLayout(btnRow);

    m_seqList = new QListWidget(prog);
    m_seqList->setObjectName("chatLog");
    m_seqList->setMinimumHeight(90);
    pLay->addWidget(m_seqList, 1);

    splitter->addWidget(prog);

    /* ── Card 2: Assemblatore Visuale ── */
    auto* asmW = new QGroupBox(
        "\xf0\x9f\x93\x8a  Assemblatore Visuale \xe2\x80\x94 Oscilloscopio", splitter);
    auto* aLay = new QVBoxLayout(asmW);
    aLay->setSpacing(8);

    m_canvas = new OscoCanvas(asmW);
    aLay->addWidget(m_canvas, 1);

    auto* ctrlRow = new QHBoxLayout();
    m_playBtn = new QPushButton(
        "\xe2\x96\xb6  Riproduci sequenza", asmW);
    m_playBtn->setObjectName("actionBtn");
    m_stopBtn = new QPushButton("\xe2\x8f\xb9  Stop", asmW);
    m_stopBtn->setEnabled(false);
    m_statusLbl = new QLabel(
        "Aggiungi toni e premi Riproduci", asmW);
    m_statusLbl->setObjectName("hintLabel");
    ctrlRow->addWidget(m_playBtn);
    ctrlRow->addWidget(m_stopBtn);
    ctrlRow->addWidget(m_statusLbl, 1);
    aLay->addLayout(ctrlRow);

    splitter->addWidget(asmW);
    splitter->setSizes({260, 260});

    mainLay->addWidget(splitter, 1);

    m_tmpWav = QDir::tempPath() + "/prismalux_sint.wav";

    /* ── Connessioni ── */
    connect(m_volSlider, &QSlider::valueChanged, this, [this](int v){
        m_volLbl->setText(QString("Volume: %1%").arg(v));
        m_canvas->setTono(m_freqSpin->value(),
                          m_ondaCombo->currentData().toString(), v / 100.0);
    });
    connect(m_freqSpin,
        QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){
        m_canvas->setTono(v, m_ondaCombo->currentData().toString(),
                          m_volSlider->value() / 100.0);
    });
    connect(m_ondaCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{
        m_canvas->setTono(m_freqSpin->value(),
                          m_ondaCombo->currentData().toString(),
                          m_volSlider->value() / 100.0);
    });

    connect(addBtn,    &QPushButton::clicked, this, &SintetizzatoreWidget::onAggiungi);
    connect(prevBtn,   &QPushButton::clicked, this, &SintetizzatoreWidget::onPreview);
    connect(remBtn,    &QPushButton::clicked, this, &SintetizzatoreWidget::onRimuovi);
    connect(m_playBtn, &QPushButton::clicked, this, &SintetizzatoreWidget::onPlaySequenza);
    connect(m_stopBtn, &QPushButton::clicked, this, &SintetizzatoreWidget::onStop);
    connect(m_seqList, &QListWidget::currentRowChanged,
            this, &SintetizzatoreWidget::onSeqSel);

    m_canvas->setTono(440.0, "sine", 0.7);
}

void SintetizzatoreWidget::onAggiungi()
{
    Tono t;
    t.freq = m_freqSpin->value();
    t.dur  = m_durSpin->value();
    t.onda = m_ondaCombo->currentData().toString();
    t.vol  = m_volSlider->value() / 100.0;
    m_seq.append(t);
    refreshList();
}

void SintetizzatoreWidget::onRimuovi()
{
    const int row = m_seqList->currentRow();
    if (row < 0 || row >= m_seq.size()) return;
    m_seq.remove(row);
    refreshList();
}

void SintetizzatoreWidget::refreshList()
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

void SintetizzatoreWidget::onSeqSel()
{
    const int row = m_seqList->currentRow();
    if (row < 0 || row >= m_seq.size()) return;
    const Tono& t = m_seq[row];
    m_canvas->setTono(t.freq, t.onda, t.vol);
}

void SintetizzatoreWidget::onPreview()
{
    Tono t;
    t.freq = m_freqSpin->value();
    t.dur  = m_durSpin->value();
    t.onda = m_ondaCombo->currentData().toString();
    t.vol  = m_volSlider->value() / 100.0;
    m_canvas->setTono(t.freq, t.onda, t.vol);
    playWav({t}, QString("Anteprima %1 Hz").arg((int)t.freq));
}

void SintetizzatoreWidget::onPlaySequenza()
{
    if (m_seq.isEmpty()) {
        m_statusLbl->setText(
            "\xe2\x9a\xa0  Aggiungi almeno un tono alla sequenza");
        return;
    }
    playWav(m_seq, QString("Sequenza %1 toni").arg(m_seq.size()));
}

void SintetizzatoreWidget::playWav(const QVector<Tono>& toni, const QString& label)
{
    if (m_aplay && m_aplay->state() != QProcess::NotRunning) {
        m_aplay->terminate();
        m_aplay->waitForFinished(500);
    }

    QFile f(m_tmpWav);
    if (!f.open(QFile::WriteOnly)) {
        m_statusLbl->setText(
            "\xe2\x9d\x8c  Impossibile scrivere file temporaneo");
        return;
    }
    f.write(makeWav(toni));
    f.close();

    m_aplay = new QProcess(this);
    connect(m_aplay,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &SintetizzatoreWidget::onPlayFinished);

    m_aplay->start("aplay", {m_tmpWav});
    if (!m_aplay->waitForStarted(2000)) {
        m_statusLbl->setText(
            "\xe2\x9d\x8c  aplay non trovato. "
            "Installa: sudo apt install alsa-utils");
        m_aplay->deleteLater();
        m_aplay = nullptr;
        return;
    }

    m_statusLbl->setText("\xe2\x96\xb6  " + label + "...");
    m_playBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_canvas->setAnimating(true);
}

void SintetizzatoreWidget::onStop()
{
    if (m_aplay && m_aplay->state() != QProcess::NotRunning)
        m_aplay->terminate();
    m_canvas->setAnimating(false);
    m_statusLbl->setText("Fermato");
    m_playBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
}

void SintetizzatoreWidget::onPlayFinished(int, QProcess::ExitStatus)
{
    if (m_aplay) { m_aplay->deleteLater(); m_aplay = nullptr; }
    m_canvas->setAnimating(false);
    m_playBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_statusLbl->setText("\xe2\x9c\x85  Riproduzione completata");
}

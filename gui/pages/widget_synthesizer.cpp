#include "widget_synthesizer.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
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

/* ── SintetizzatoreWidget — livello 0: entry point ───────────── */
SintetizzatoreWidget::SintetizzatoreWidget(QWidget* parent) : QWidget(parent)
{
    m_tmpWav = QDir::tempPath() + "/prismalux_sint.wav";
    buildLayout();
    setupConnections();
    m_canvas->setTono(440.0, "sine", 0.7);
}

/* ── Livello 1: struttura principale ─────────────────────────── */
void SintetizzatoreWidget::buildLayout()
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(10);
    lay->addWidget(buildTitle());
    lay->addWidget(buildSplitter(), 1);
}

QLabel* SintetizzatoreWidget::buildTitle()
{
    auto* lbl = new QLabel(
        "\xf0\x9f\x8e\xb5  <b>Sintetizzatore Toni Puri</b>", this);
    lbl->setTextFormat(Qt::RichText);
    lbl->setObjectName("sectionTitle");
    return lbl;
}

QWidget* SintetizzatoreWidget::buildSplitter()
{
    auto* sp = new QSplitter(Qt::Vertical, this);
    sp->addWidget(buildProgrammatoreCard(sp));
    sp->addWidget(buildAssemblatoreCard(sp));
    sp->setSizes({260, 260});
    return sp;
}

/* ── Livello 2: card principali ──────────────────────────────── */
QWidget* SintetizzatoreWidget::buildProgrammatoreCard(QWidget* parent)
{
    auto* box = new QGroupBox(
        "\xf0\x9f\x8e\xb9  Programmatore Toni", parent);
    auto* lay = new QVBoxLayout(box);
    lay->setSpacing(8);
    lay->addLayout(buildParametriGrid(box));
    lay->addLayout(buildAzioniRow(box));
    lay->addLayout(buildSalvaRow(box));
    m_seqList = new QListWidget(box);
    m_seqList->setObjectName("chatLog");
    m_seqList->setMinimumHeight(90);
    lay->addWidget(m_seqList, 1);
    return box;
}

QWidget* SintetizzatoreWidget::buildAssemblatoreCard(QWidget* parent)
{
    auto* box = new QGroupBox(
        "\xf0\x9f\x93\x8a  Assemblatore Visuale \xe2\x80\x94 Oscilloscopio", parent);
    auto* lay = new QVBoxLayout(box);
    lay->setSpacing(8);
    m_canvas = new OscoCanvas(box);
    lay->addWidget(m_canvas, 1);
    lay->addLayout(buildControlRow(box));
    return box;
}

/* ── Livello 3: componenti interni ───────────────────────────── */
QGridLayout* SintetizzatoreWidget::buildParametriGrid(QWidget* parent)
{
    auto* grid = new QGridLayout();
    grid->setSpacing(8);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    m_freqSpin = new QSpinBox(parent);
    m_freqSpin->setRange(20, 20000);
    m_freqSpin->setValue(440);
    m_freqSpin->setSingleStep(10);
    m_freqSpin->setObjectName("spinBox");

    m_durSpin = new QSpinBox(parent);
    m_durSpin->setRange(50, 10000);
    m_durSpin->setValue(500);
    m_durSpin->setSingleStep(50);
    m_durSpin->setObjectName("spinBox");

    m_ondaCombo = new QComboBox(parent);
    m_ondaCombo->addItem("Sinusoidale",   "sine");
    m_ondaCombo->addItem("Quadra",        "square");
    m_ondaCombo->addItem("Triangolare",   "triangle");
    m_ondaCombo->addItem("Dente di sega", "sawtooth");

    m_volLbl    = new QLabel("Volume: 70%", parent);
    m_volSlider = new QSlider(Qt::Horizontal, parent);
    m_volSlider->setRange(0, 100);
    m_volSlider->setValue(70);

    grid->addWidget(new QLabel("Frequenza (Hz):"),           0, 0);
    grid->addWidget(m_freqSpin,                              0, 1);
    grid->addWidget(new QLabel("Durata (ms):"),              0, 2);
    grid->addWidget(m_durSpin,                               0, 3);
    grid->addWidget(new QLabel("Forma d\xe2\x80\x99" "onda:"), 1, 0);
    grid->addWidget(m_ondaCombo,                             1, 1);
    grid->addWidget(m_volLbl,                                1, 2);
    grid->addWidget(m_volSlider,                             1, 3);
    return grid;
}

QHBoxLayout* SintetizzatoreWidget::buildAzioniRow(QWidget* parent)
{
    auto* row     = new QHBoxLayout();
    auto* addBtn  = new QPushButton("+ Aggiungi", parent);
    addBtn->setObjectName("actionBtn");
    auto* prevBtn = new QPushButton(
        "\xf0\x9f\x94\x8a  Ascolta", parent);
    prevBtn->setObjectName("actionBtn");
    auto* remBtn  = new QPushButton(
        "\xf0\x9f\x97\x91  Rimuovi", parent);
    row->addWidget(addBtn);
    row->addWidget(prevBtn);
    row->addWidget(remBtn);
    row->addStretch();
    connect(addBtn,  &QPushButton::clicked, this, &SintetizzatoreWidget::onAggiungi);
    connect(prevBtn, &QPushButton::clicked, this, &SintetizzatoreWidget::onPreview);
    connect(remBtn,  &QPushButton::clicked, this, &SintetizzatoreWidget::onRimuovi);
    return row;
}

QHBoxLayout* SintetizzatoreWidget::buildSalvaRow(QWidget* parent)
{
    auto* row        = new QHBoxLayout();
    auto* saveSeqBtn = new QPushButton(
        "\xf0\x9f\x92\xbe  Salva sequenza", parent);
    saveSeqBtn->setObjectName("actionBtn");
    auto* loadSeqBtn = new QPushButton(
        "\xf0\x9f\x93\x82  Carica sequenza", parent);
    loadSeqBtn->setObjectName("actionBtn");
    row->addWidget(saveSeqBtn);
    row->addWidget(loadSeqBtn);
    row->addStretch();
    connect(saveSeqBtn, &QPushButton::clicked, this, &SintetizzatoreWidget::onSalvaSeq);
    connect(loadSeqBtn, &QPushButton::clicked, this, &SintetizzatoreWidget::onCaricaSeq);
    return row;
}

QHBoxLayout* SintetizzatoreWidget::buildControlRow(QWidget* parent)
{
    auto* row    = new QHBoxLayout();
    m_playBtn = new QPushButton(
        "\xe2\x96\xb6  Riproduci sequenza", parent);
    m_playBtn->setObjectName("actionBtn");
    m_stopBtn = new QPushButton("\xe2\x8f\xb9  Stop", parent);
    m_stopBtn->setEnabled(false);
    auto* saveWavBtn = new QPushButton(
        "\xf0\x9f\x92\xbe  Salva WAV", parent);
    saveWavBtn->setObjectName("actionBtn");
    m_statusLbl = new QLabel("Aggiungi toni e premi Riproduci", parent);
    m_statusLbl->setObjectName("hintLabel");
    row->addWidget(m_playBtn);
    row->addWidget(m_stopBtn);
    row->addWidget(saveWavBtn);
    row->addWidget(m_statusLbl, 1);
    connect(m_playBtn,  &QPushButton::clicked, this, &SintetizzatoreWidget::onPlaySequenza);
    connect(m_stopBtn,  &QPushButton::clicked, this, &SintetizzatoreWidget::onStop);
    connect(saveWavBtn, &QPushButton::clicked, this, &SintetizzatoreWidget::onSalvaWav);
    return row;
}

/* ── Connessioni trasversali (controllo → oscilloscopio) ─────── */
void SintetizzatoreWidget::setupConnections()
{
    connect(m_volSlider, &QSlider::valueChanged, this, [this](int v) {
        m_volLbl->setText(QString("Volume: %1%").arg(v));
        m_canvas->setTono(m_freqSpin->value(),
                          m_ondaCombo->currentData().toString(), v / 100.0);
    });
    connect(m_freqSpin,
        QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_canvas->setTono(v, m_ondaCombo->currentData().toString(),
                          m_volSlider->value() / 100.0);
    });
    connect(m_ondaCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        m_canvas->setTono(m_freqSpin->value(),
                          m_ondaCombo->currentData().toString(),
                          m_volSlider->value() / 100.0);
    });
    connect(m_seqList, &QListWidget::currentRowChanged,
            this, &SintetizzatoreWidget::onSeqSel);
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
        m_aplay->waitForFinished(P::kProcKillExtendedMs);
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
    m_statusLbl->setText(tr("Fermato"));
    m_playBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
}

void SintetizzatoreWidget::onPlayFinished(int, QProcess::ExitStatus)
{
    if (m_aplay) { m_aplay->deleteLater(); m_aplay = nullptr; }
    m_canvas->setAnimating(false);
    m_playBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_statusLbl->setText(tr("\xe2\x9c\x85  Riproduzione completata"));
}

void SintetizzatoreWidget::onSalvaSeq()
{
    if (m_seq.isEmpty()) {
        m_statusLbl->setText(tr("\xe2\x9a\xa0  Nessun tono da salvare"));
        return;
    }
    QString path = QFileDialog::getSaveFileName(
        this, "Salva sequenza toni", QDir::homePath(),
        "Sequenza Prismalux (*.json)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".json")) path += ".json";

    QJsonArray arr;
    for (const Tono& t : m_seq) {
        QJsonObject obj;
        obj["freq"] = t.freq;
        obj["dur"]  = t.dur;
        obj["onda"] = t.onda;
        obj["vol"]  = t.vol;
        arr.append(obj);
    }
    QFile f(path);
    if (!f.open(QFile::WriteOnly)) {
        m_statusLbl->setText(tr("\xe2\x9d\x8c  Impossibile scrivere il file"));
        return;
    }
    f.write(QJsonDocument(arr).toJson());
    m_statusLbl->setText("\xe2\x9c\x85  Sequenza salvata: "
                         + QFileInfo(path).fileName());
}

void SintetizzatoreWidget::onCaricaSeq()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Carica sequenza toni", QDir::homePath(),
        "Sequenza Prismalux (*.json)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QFile::ReadOnly)) {
        m_statusLbl->setText(tr("\xe2\x9d\x8c  File non leggibile"));
        return;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        m_statusLbl->setText(tr("\xe2\x9d\x8c  File JSON non valido"));
        return;
    }
    m_seq.clear();
    for (const QJsonValue& v : doc.array()) {
        const QJsonObject obj = v.toObject();
        Tono t;
        t.freq = obj["freq"].toDouble(440.0);
        t.dur  = obj["dur"].toInt(500);
        t.onda = obj["onda"].toString("sine");
        t.vol  = obj["vol"].toDouble(0.7);
        m_seq.append(t);
    }
    refreshList();
    m_statusLbl->setText(
        QString("\xe2\x9c\x85  Caricati %1 toni").arg(m_seq.size()));
}

void SintetizzatoreWidget::onSalvaWav()
{
    if (m_seq.isEmpty()) {
        m_statusLbl->setText(tr("\xe2\x9a\xa0  Nessun tono da esportare"));
        return;
    }
    QString path = QFileDialog::getSaveFileName(
        this, "Esporta audio WAV", QDir::homePath(),
        "Audio WAV (*.wav)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".wav")) path += ".wav";

    QFile f(path);
    if (!f.open(QFile::WriteOnly)) {
        m_statusLbl->setText(tr("\xe2\x9d\x8c  Impossibile scrivere il file WAV"));
        return;
    }
    f.write(makeWav(m_seq));
    m_statusLbl->setText("\xe2\x9c\x85  WAV salvato: "
                         + QFileInfo(path).fileName());
}

#include "widget_voice_cloner.h"
#include "../prismalux_paths.h"
#include "../dpi_utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QStandardPaths>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>

namespace P = PrismaluxPaths;

/* ────────────────────────────────────────────────── costruttore ── */

VoiceClonerWidget::VoiceClonerWidget(QWidget* parent)
    : QWidget(parent)
{
    m_outputWav = QDir::tempPath() + "/prismalux_vc_out.wav";

    auto* outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(dpiScale(8), dpiScale(8), dpiScale(8), dpiScale(8));
    outerLay->setSpacing(dpiScale(8));

    /* Titolo */
    auto* title = new QLabel(
        "\xf0\x9f\x8e\xa4  <b>Sintetizzatore Vocale da Campione</b> "
        "<span style='color:#64748b;font-size:small;'>"
        "Clona una voce da campione audio &bull; multi-backend: XTTS-v2 / chatterbox / edge-tts</span>",
        this);
    title->setTextFormat(Qt::RichText);
    outerLay->addWidget(title);

    /* Splitter sinistra/destra */
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    /* ── Pannello sinistro ── */
    auto* left = new QWidget(splitter);
    auto* leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(dpiScale(6));
    m_sampleGroup = buildSampleGroup();
    leftLay->addWidget(m_sampleGroup);
    leftLay->addWidget(buildTextGroup());
    leftLay->addWidget(buildSettingsGroup());
    leftLay->addWidget(buildActionRow());
    leftLay->addStretch();

    /* ── Pannello destro ── */
    auto* right = new QWidget(splitter);
    auto* rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(dpiScale(6));
    rightLay->addWidget(buildOutputGroup());
    rightLay->addWidget(buildLogGroup());

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    outerLay->addWidget(splitter, 1);

    checkTtsInstalled();
}

VoiceClonerWidget::~VoiceClonerWidget()
{
    if (m_recProc   && m_recProc->state()   != QProcess::NotRunning) m_recProc->kill();
    if (m_cloneProc && m_cloneProc->state() != QProcess::NotRunning) m_cloneProc->kill();
    if (m_playProc  && m_playProc->state()  != QProcess::NotRunning) m_playProc->kill();
    if (m_instProc  && m_instProc->state()  != QProcess::NotRunning) m_instProc->kill();
}

/* ──────────────────────────────────────────── costruzione UI ── */

QGroupBox* VoiceClonerWidget::buildSampleGroup()
{
    auto* gb = new QGroupBox("\xf0\x9f\x8e\xa4  Campione vocale", this); /* 🎤 */
    auto* lay = new QVBoxLayout(gb);
    lay->setSpacing(dpiScale(4));

    m_sampleLbl = new QLabel("Nessun campione caricato", gb);
    m_sampleLbl->setStyleSheet("color:#94a3b8;font-style:italic;");

    auto* row = new QHBoxLayout;
    m_loadBtn = new QPushButton("\xf0\x9f\x93\x82  Carica campione", gb); /* 📂 */
    m_loadBtn->setToolTip("Carica un file WAV/MP3/OGG come campione vocale (ideale 5-30 secondi)");
    m_recBtn  = new QPushButton("\xf0\x9f\x94\xb4  Registra", gb); /* 🔴 */
    m_recBtn->setCheckable(true);
    m_recBtn->setToolTip("Registra un campione dal microfono (tieni premuto, poi rilascia)");
    row->addWidget(m_loadBtn);
    row->addWidget(m_recBtn);
    row->addStretch();

    lay->addLayout(row);
    lay->addWidget(m_sampleLbl);

    connect(m_loadBtn, &QPushButton::clicked, this, &VoiceClonerWidget::onLoadSampleClicked);
    connect(m_recBtn, &QPushButton::toggled, this, &VoiceClonerWidget::onRecordToggled);
    return gb;
}

QGroupBox* VoiceClonerWidget::buildTextGroup()
{
    auto* gb = new QGroupBox("\xf0\x9f\x93\x9d  Testo da sintetizzare", this); /* 📝 */
    auto* lay = new QVBoxLayout(gb);

    m_textEdit = new QTextEdit(gb);
    m_textEdit->setPlaceholderText("Scrivi qui il testo che vuoi far leggere con la voce clonata...");
    m_textEdit->setFixedHeight(dpiScale(100));

    lay->addWidget(m_textEdit);
    return gb;
}

QGroupBox* VoiceClonerWidget::buildSettingsGroup()
{
    auto* gb = new QGroupBox("\xe2\x9a\x99  Impostazioni", this); /* ⚙ */
    auto* lay = new QHBoxLayout(gb);

    lay->addWidget(new QLabel("Lingua:", gb));
    m_langCombo = new QComboBox(gb);
    m_langCombo->addItem("Italiano",  "it");
    m_langCombo->addItem("English",   "en");
    m_langCombo->addItem("Francais",  "fr");
    m_langCombo->addItem("Deutsch",   "de");
    m_langCombo->addItem("Espanol",   "es");
    m_langCombo->addItem("Portugues", "pt");
    m_langCombo->addItem("Polski",    "pl");
    m_langCombo->setToolTip("Lingua del testo sintetizzato (non del campione)");
    lay->addWidget(m_langCombo);

    m_backendLbl = new QLabel("", gb);
    m_backendLbl->setObjectName("hintLabel");
    lay->addWidget(m_backendLbl);

    lay->addStretch();

    m_installBtn = new QPushButton("\xf0\x9f\x93\xa6  Installa TTS", gb); /* 📦 */
    m_installBtn->setToolTip("Installa il backend TTS disponibile per questa versione di Python");
    lay->addWidget(m_installBtn);
    connect(m_installBtn, &QPushButton::clicked, this, &VoiceClonerWidget::onInstallClicked);
    return gb;
}

QWidget* VoiceClonerWidget::buildActionRow()
{
    auto* w = new QWidget(this);
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);

    m_genBtn  = new QPushButton("\xe2\x96\xb6  Genera voce", w); /* ▶ */
    m_genBtn->setFixedHeight(dpiScale(36));
    m_genBtn->setStyleSheet("QPushButton{background:#1d4ed8;color:#fff;border-radius:6px;font-weight:bold;}"
                            "QPushButton:hover{background:#2563eb;}"
                            "QPushButton:disabled{background:#334155;color:#64748b;}");
    m_genBtn->setEnabled(false);

    m_stopBtn = new QPushButton("\xe2\x8f\xb9  Stop", w); /* ⏹ */
    m_stopBtn->setEnabled(false);

    m_progress = new QProgressBar(w);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    m_progress->setFixedHeight(dpiScale(6));
    m_progress->setTextVisible(false);

    m_statusLbl = new QLabel("", w);
    m_statusLbl->setStyleSheet("color:#64748b;");

    lay->addWidget(m_genBtn);
    lay->addWidget(m_stopBtn);
    lay->addWidget(m_progress);
    lay->addWidget(m_statusLbl, 1);

    connect(m_genBtn,  &QPushButton::clicked, this, &VoiceClonerWidget::onGenerateClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &VoiceClonerWidget::onStopClicked);
    return w;
}

QGroupBox* VoiceClonerWidget::buildOutputGroup()
{
    auto* gb = new QGroupBox("\xf0\x9f\x94\x8a  Output", this); /* 🔊 */
    auto* lay = new QHBoxLayout(gb);

    m_playBtn = new QPushButton("\xe2\x96\xb6  Riproduci output", gb); /* ▶ */
    m_playBtn->setEnabled(false);
    m_saveBtn = new QPushButton("\xf0\x9f\x92\xbe  Salva WAV", gb); /* 💾 */
    m_saveBtn->setEnabled(false);

    lay->addWidget(m_playBtn);
    lay->addWidget(m_saveBtn);
    lay->addStretch();

    connect(m_playBtn, &QPushButton::clicked, this, &VoiceClonerWidget::onPlayOutputClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &VoiceClonerWidget::onSaveWavClicked);
    return gb;
}

QGroupBox* VoiceClonerWidget::buildLogGroup()
{
    auto* gb = new QGroupBox("Log", this);
    auto* lay = new QVBoxLayout(gb);

    m_logView = new QTextEdit(gb);
    m_logView->setReadOnly(true);
    m_logView->setStyleSheet("font-family:monospace;font-size:11px;background:#0f172a;color:#94a3b8;");

    lay->addWidget(m_logView);
    return gb;
}

/* ──────────────────────────────────────── slot campione ── */

void VoiceClonerWidget::onLoadSampleClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Carica campione vocale", QDir::homePath(),
        "Audio (*.wav *.mp3 *.ogg *.flac *.m4a *.aac);;Tutti i file (*)");
    if (path.isEmpty()) return;

    m_samplePath = path;
    QFileInfo fi(path);
    m_sampleLbl->setText(
        QString("<b>%1</b>  <span style='color:#64748b;'>%2</span>")
            .arg(fi.fileName())
            .arg(QString::number(fi.size() / 1024) + " KB"));
    m_sampleLbl->setStyleSheet("");
    appendLog("Campione caricato: " + path);
    m_genBtn->setEnabled(m_ttsInstalled && (m_backend != BackendEdge));
}

void VoiceClonerWidget::onRecordToggled(bool on)
{
    if (on) {
        m_recBtn->setText("\xe2\x8f\xb9  Registra (stop)");
        QString tmp = QDir::tempPath() + "/prismalux_vc_sample.wav";
        if (!m_recProc) {
            m_recProc = new QProcess(this);
            connect(m_recProc,
                    QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &VoiceClonerWidget::onRecProcFinished);
        }
        QStringList args;
        if (QProcess::execute("which", {"arecord"}) == 0) {
            args = {"-f", "cd", "-t", "wav", tmp};
            m_recProc->start("arecord", args);
        } else {
            args = {"-f", "alsa", "-i", "default", "-ar", "22050", "-ac", "1", tmp};
            m_recProc->start("ffmpeg", args);
        }
        m_samplePath = tmp;
        appendLog("Registrazione in corso...");
    } else {
        m_recBtn->setText("\xf0\x9f\x94\xb4  Registra");
        if (m_recProc && m_recProc->state() != QProcess::NotRunning)
            m_recProc->terminate();
    }
}

void VoiceClonerWidget::onRecProcFinished(int, QProcess::ExitStatus)
{
    QFileInfo fi(m_samplePath);
    if (fi.exists() && fi.size() > 0) {
        m_sampleLbl->setText(
            QString("<b>%1</b>  <span style='color:#64748b;'>registrazione %2 KB</span>")
                .arg(fi.fileName()).arg(fi.size() / 1024));
        m_sampleLbl->setStyleSheet("");
        m_genBtn->setEnabled(m_ttsInstalled && (m_backend != BackendEdge));
        appendLog("Registrazione completata: " + m_samplePath);
    } else {
        setStatus("Errore registrazione — verifica il microfono");
    }
    m_recBtn->setChecked(false);
    m_recBtn->setText("\xf0\x9f\x94\xb4  Registra");
}

/* ──────────────────────────────────────── generazione voce ── */

void VoiceClonerWidget::onGenerateClicked()
{
    QString testo = m_textEdit->toPlainText().trimmed();
    if (testo.isEmpty()) { setStatus("Scrivi il testo da sintetizzare"); return; }

    if (m_backend != BackendEdge && m_samplePath.isEmpty()) {
        setStatus("Carica prima un campione vocale"); return;
    }

    QString lang   = m_langCombo->currentData().toString();
    QString script = buildScript(testo, lang);

    if (!m_cloneProc) {
        m_cloneProc = new QProcess(this);
        connect(m_cloneProc, &QProcess::readyReadStandardOutput,
                this, &VoiceClonerWidget::onCloneProcReadyRead);
        connect(m_cloneProc, &QProcess::readyReadStandardError,
                this, &VoiceClonerWidget::onCloneProcReadyRead);
        connect(m_cloneProc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &VoiceClonerWidget::onCloneProcFinished);
    }

    m_genBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_playBtn->setEnabled(false);
    m_saveBtn->setEnabled(false);
    m_progress->setVisible(true);
    setStatus("Avvio sintesi...");
    appendLog("--- Avvio " + QString(m_backend == BackendCoqui ? "XTTS-v2"
                                  : m_backend == BackendChatterbox ? "chatterbox-tts"
                                  : "edge-tts") + " ---");
    if (m_backend != BackendEdge)
        appendLog("Campione: " + m_samplePath);
    appendLog("Testo: " + testo.left(80) + (testo.length() > 80 ? "..." : ""));

    m_cloneProc->start("python3", {"-c", script});
}

QString VoiceClonerWidget::buildScript(const QString& testo, const QString& lang) const
{
    /* Escape il testo per l'embedding nel sorgente Python */
    QString t = testo;
    t.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n");
    const QString sample = QString(m_samplePath).replace("\\", "\\\\");
    const QString out    = QString(m_outputWav).replace("\\", "\\\\");

    if (m_backend == BackendCoqui) {
        return QString(
            "import sys\n"
            "try:\n"
            "    from TTS.api import TTS\n"
            "except ImportError:\n"
            "    print('ERROR:TTS non installato', flush=True); sys.exit(1)\n"
            "print('INFO:Caricamento modello XTTS-v2...', flush=True)\n"
            "tts = TTS(model_name='tts_models/multilingual/multi-dataset/xtts_v2',"
            " progress_bar=False)\n"
            "print('INFO:Sintesi in corso...', flush=True)\n"
            "tts.tts_to_file(text=\"%1\", speaker_wav=\"%2\", language=\"%3\","
            " file_path=\"%4\")\n"
            "print('DONE:%4', flush=True)\n"
        ).arg(t).arg(sample).arg(lang).arg(out);
    }

    if (m_backend == BackendChatterbox) {
        return QString(
            "import sys\n"
            "try:\n"
            "    import torch, torchaudio\n"
            "    from chatterbox.tts import ChatterboxTTS\n"
            "except ImportError:\n"
            "    print('ERROR:chatterbox-tts non installato', flush=True); sys.exit(1)\n"
            "print('INFO:Caricamento modello chatterbox...', flush=True)\n"
            "dev = 'cuda' if torch.cuda.is_available() else 'cpu'\n"
            "model = ChatterboxTTS.from_pretrained(device=dev)\n"
            "print('INFO:Sintesi con clonazione vocale...', flush=True)\n"
            "wav = model.generate(\"%1\", audio_prompt_path=\"%2\")\n"
            "torchaudio.save(\"%3\", wav, model.sr)\n"
            "print('DONE:%3', flush=True)\n"
        ).arg(t).arg(sample).arg(out);
    }

    /* BackendEdge — online, nessun campione */
    const QString voice = edgeVoice(lang);
    return QString(
        "import sys, asyncio\n"
        "try:\n"
        "    import edge_tts\n"
        "except ImportError:\n"
        "    print('ERROR:edge-tts non installato', flush=True); sys.exit(1)\n"
        "print('INFO:Sintesi online con edge-tts (%1)...', flush=True)\n"
        "async def run():\n"
        "    c = edge_tts.Communicate(\"%2\", \"%1\")\n"
        "    await c.save(\"%3\")\n"
        "asyncio.run(run())\n"
        "print('DONE:%3', flush=True)\n"
    ).arg(voice).arg(t).arg(out);
}

QString VoiceClonerWidget::edgeVoice(const QString& lang)
{
    if (lang == "en") return "en-US-JennyNeural";
    if (lang == "fr") return "fr-FR-DeniseNeural";
    if (lang == "de") return "de-DE-KatjaNeural";
    if (lang == "es") return "es-ES-ElviraNeural";
    if (lang == "pt") return "pt-PT-FernandaNeural";
    if (lang == "pl") return "pl-PL-AgnieszkaNeural";
    return "it-IT-ElsaNeural";  /* default italiano */
}

void VoiceClonerWidget::onStopClicked()
{
    if (m_cloneProc && m_cloneProc->state() != QProcess::NotRunning)
        m_cloneProc->kill();
    m_genBtn->setEnabled(hasSample() && m_ttsInstalled && m_backend != BackendNone);
    m_stopBtn->setEnabled(false);
    m_progress->setVisible(false);
    setStatus("Interrotto dall'utente");
}

void VoiceClonerWidget::onCloneProcReadyRead()
{
    auto readAll = [this](bool err) {
        QString out = err ? m_cloneProc->readAllStandardError()
                          : m_cloneProc->readAllStandardOutput();
        for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
            appendLog(line);
            if (line.startsWith("INFO:"))
                setStatus(line.mid(5));
            else if (line.startsWith("ERROR:"))
                setStatus("\xe2\x9d\x8c  " + line.mid(6));
        }
    };
    readAll(false);
    readAll(true);
}

void VoiceClonerWidget::onCloneProcFinished(int code, QProcess::ExitStatus)
{
    m_progress->setVisible(false);
    m_stopBtn->setEnabled(false);
    m_genBtn->setEnabled(m_ttsInstalled && m_backend != BackendNone
                         && (m_backend == BackendEdge || hasSample()));

    if (code == 0 && QFile::exists(m_outputWav)) {
        m_playBtn->setEnabled(true);
        m_saveBtn->setEnabled(true);
        setStatus("\xe2\x9c\x85  Voce generata — premi Riproduci");
        appendLog("Output: " + m_outputWav);
    } else {
        setStatus("\xe2\x9d\x8c  Errore sintesi (codice " + QString::number(code) + ")");
    }
}

/* ──────────────────────────────────────── playback / salva ── */

void VoiceClonerWidget::onPlayOutputClicked()
{
    if (!m_playProc)
        m_playProc = new QProcess(this);
    if (m_playProc->state() != QProcess::NotRunning)
        m_playProc->kill();

    if (QProcess::execute("which", {"aplay"}) == 0)
        m_playProc->start("aplay", {m_outputWav});
    else
        m_playProc->start("ffplay", {"-nodisp", "-autoexit", m_outputWav});
    appendLog("Riproduzione output...");
}

void VoiceClonerWidget::onSaveWavClicked()
{
    QString dest = QFileDialog::getSaveFileName(
        this, "Salva voce clonata",
        QDir::homePath() + "/voce_clonata.wav",
        "File WAV (*.wav)");
    if (dest.isEmpty()) return;
    if (QFile::exists(dest)) QFile::remove(dest);
    if (QFile::copy(m_outputWav, dest)) {
        appendLog("Salvato in: " + dest);
        setStatus("File salvato in " + dest);
    } else {
        setStatus("Errore nel salvataggio");
    }
}

/* ──────────────────────────────────────── installazione TTS ── */

void VoiceClonerWidget::onInstallClicked()
{
    /* Su Python >= 3.12 non esiste Coqui TTS — usa chatterbox-tts */
    const bool needChatterbox = (m_backend == BackendNone || m_backend == BackendEdge);
    /* Se edge-tts è già installato, offri chatterbox come upgrade a voice cloning */

    QString pkg, info;
    if (needChatterbox) {
        pkg  = "chatterbox-tts";
        info = "Verr\xc3\xa0 installato <b>chatterbox-tts</b> (Resemble AI, Python 3.12+).<br>"
               "Supporta la clonazione vocale zero-shot da campione audio.<br>"
               "Dimensione modello: ~1 GB. CPU supportata (lenta), GPU raccomandata.<br>"
               "Richiede: torch, torchaudio (scaricati automaticamente).<br><br>"
               "Continuare?";
    } else {
        pkg  = "edge-tts";
        info = "Verr\xc3\xa0 installato <b>edge-tts</b> (Microsoft Edge TTS, online).<br>"
               "Non richiede GPU, supporta italiano e molte lingue.<br>"
               "<i>Nota: non clona la voce — usa voci predefinite ad alta qualit\xc3\xa0</i>.<br>"
               "Per voice cloning usa chatterbox-tts.<br><br>"
               "Continuare?";
    }

    auto reply = QMessageBox::question(this, "Installa TTS", info,
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    if (!m_instProc) {
        m_instProc = new QProcess(this);
        connect(m_instProc, &QProcess::readyReadStandardOutput,
                this, &VoiceClonerWidget::onInstallProcReadyRead);
        connect(m_instProc, &QProcess::readyReadStandardError,
                this, &VoiceClonerWidget::onInstallProcReadyRead);
        connect(m_instProc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &VoiceClonerWidget::onInstallProcFinished);
    }

    m_installBtn->setEnabled(false);
    setStatus("Installazione " + pkg + "...");
    appendLog("--- pip install " + pkg + " ---");
    m_instProc->start("pip3", {"install", "--break-system-packages", pkg});
}

void VoiceClonerWidget::onInstallProcReadyRead()
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    QString out = proc->readAllStandardOutput() + proc->readAllStandardError();
    for (const QString& line : out.split('\n', Qt::SkipEmptyParts))
        appendLog(line);
}

void VoiceClonerWidget::onInstallProcFinished(int code, QProcess::ExitStatus)
{
    m_installBtn->setEnabled(true);
    if (code == 0) {
        appendLog("Installazione completata. Rilevamento backend...");
        checkTtsInstalled();
    } else {
        setStatus("\xe2\x9d\x8c  Installazione fallita (codice " + QString::number(code) + ")");
        appendLog("Prova: pip3 install --break-system-packages chatterbox-tts");
        appendLog("Fallback senza voice cloning: pip3 install edge-tts");
    }
}

/* ──────────────────────────────────────── rilevamento backend ── */

/* Controlla nell'ordine: Coqui TTS → chatterbox-tts → edge-tts */
void VoiceClonerWidget::checkTtsInstalled()
{
    struct Check {
        const char* module;
        TtsBackend  backend;
    };
    static const Check checks[] = {
        { "TTS",           BackendCoqui       },
        { "chatterbox",    BackendChatterbox   },
        { "edge_tts",      BackendEdge         },
    };

    /* Scansiona in sequenza con puntatori condivisi */
    auto* results = new QVector<int>(3, -1);   /* -1=pending, 0=missing, 1=ok */
    int   total   = 3;
    auto* done    = new int(0);

    for (int i = 0; i < total; ++i) {
        auto* proc = new QProcess(this);
        const int idx = i;
        const TtsBackend bk = checks[i].backend;

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, results, done, total, idx, bk]
                (int code, QProcess::ExitStatus) {
            proc->deleteLater();
            (*results)[idx] = (code == 0) ? 1 : 0;
            ++(*done);

            if (*done == total) {
                /* Tutti completati — scegli il miglior backend */
                TtsBackend best = BackendNone;
                if ((*results)[0] == 1) best = BackendCoqui;
                else if ((*results)[1] == 1) best = BackendChatterbox;
                else if ((*results)[2] == 1) best = BackendEdge;

                m_backend      = best;
                m_ttsInstalled = (best != BackendNone);
                applyBackend();

                delete results;
                delete done;
            }
        });
        proc->start("python3", {"-c",
            QString("import %1; print('ok')").arg(checks[i].module)});
    }
}

void VoiceClonerWidget::applyBackend()
{
    switch (m_backend) {
    case BackendCoqui:
        m_installBtn->setText("\xe2\x9c\x85  XTTS-v2");
        m_installBtn->setEnabled(false);
        m_backendLbl->setText(
            "<span style='color:#4ade80;'>XTTS-v2 (voice cloning)</span>");
        setStatus("Coqui XTTS-v2 disponibile — carica un campione per iniziare");
        appendLog("Backend: Coqui TTS (XTTS-v2) — voice cloning completo.");
        if (m_sampleGroup) m_sampleGroup->setEnabled(true);
        break;

    case BackendChatterbox:
        m_installBtn->setText("\xe2\x9c\x85  chatterbox");
        m_installBtn->setEnabled(false);
        m_backendLbl->setText(
            "<span style='color:#4ade80;'>chatterbox-tts (voice cloning)</span>");
        setStatus("chatterbox-tts disponibile — carica un campione per iniziare");
        appendLog("Backend: chatterbox-tts (Resemble AI) — voice cloning su Python 3.12+.");
        if (m_sampleGroup) m_sampleGroup->setEnabled(true);
        break;

    case BackendEdge:
        m_installBtn->setText("Installa chatterbox");
        m_installBtn->setToolTip(
            "Installa chatterbox-tts per voice cloning reale. "
            "edge-tts e' gia' disponibile ma usa voci predefinite (no campione).");
        m_backendLbl->setText(
            "<span style='color:#fb923c;'>edge-tts online (no campione)</span>");
        setStatus("edge-tts attivo — nessun campione richiesto, voce predefinita online");
        appendLog("Backend: edge-tts (Microsoft Edge, online). Il campione non viene usato.");
        appendLog("Per voice cloning installa: pip3 install --break-system-packages chatterbox-tts");
        if (m_sampleGroup) m_sampleGroup->setEnabled(false);
        m_genBtn->setEnabled(true);  /* edge-tts non richiede campione */
        break;

    case BackendNone:
    default:
        m_installBtn->setText("\xf0\x9f\x93\xa6  Installa TTS");
        m_backendLbl->setText(
            "<span style='color:#f87171;'>nessun backend</span>");
        setStatus("TTS non installato — usa il pulsante per installare");
        appendLog("Nessun backend TTS trovato.");
        appendLog("Opzioni:");
        appendLog("  Python 3.12+: pip3 install --break-system-packages chatterbox-tts");
        appendLog("  Fallback online: pip3 install --break-system-packages edge-tts");
        appendLog("  Python < 3.12: pip3 install TTS");
        if (m_sampleGroup) m_sampleGroup->setEnabled(false);
        break;
    }

    m_genBtn->setEnabled(m_ttsInstalled &&
        (m_backend == BackendEdge || hasSample()));
}

/* ──────────────────────────────────────── helpers ── */

void VoiceClonerWidget::appendLog(const QString& line)
{
    m_logView->append(line);
    m_logView->verticalScrollBar()->setValue(m_logView->verticalScrollBar()->maximum());
}

void VoiceClonerWidget::setStatus(const QString& msg)
{
    m_statusLbl->setText(msg);
}

bool VoiceClonerWidget::hasSample() const
{
    return !m_samplePath.isEmpty() && QFile::exists(m_samplePath);
}

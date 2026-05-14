#include "impostazioni_page.h"
#include "../widgets/toggle_switch.h"
#include "../widgets/stt_whisper.h"
#include "personalizza_page.h"
#include "manutenzione_page.h"
#include "grafico_page.h"
#include "../theme_manager.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QButtonGroup>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <functional>
#include <QCoreApplication>
#include <QTimer>
#include <QRadioButton>
#include <QCheckBox>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QThread>
#include <QStackedWidget>
#include <QColorDialog>
#include <QPainter>
#include <QPen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGuiApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QDialog>
#include <QRegularExpression>

static QString s_piperDir() {
    return QCoreApplication::applicationDirPath() + "/piper";
}

QString ImpostazioniPage::piperBinPath() {
#ifdef Q_OS_WIN
    const QString name = "piper.exe";
#else
    const QString name = "piper";
#endif
    /* 1. Accanto all'eseguibile: <appDir>/piper/piper */
    const QString local = s_piperDir() + "/" + name;
    if (QFileInfo::exists(local)) return local;

    /* 2. Fallback: PATH di sistema */
    const QString sys = QStandardPaths::findExecutable("piper");
    if (!sys.isEmpty()) return sys;

    return {};
}

QString ImpostazioniPage::piperVoicesDir() {
    return s_piperDir() + "/voices";
}

QString ImpostazioniPage::piperActiveVoice() {
    QFile f(s_piperDir() + "/active_voice");
    if (f.open(QIODevice::ReadOnly))
        return QString::fromUtf8(f.readAll()).trimmed();
    return "it_IT-paola-medium";  /* default: voce femminile */
}

void ImpostazioniPage::savePiperActiveVoice(const QString& name) {
    QDir().mkpath(s_piperDir());
    QFile f(s_piperDir() + "/active_voice");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(name.toUtf8());
}

/* ══════════════════════════════════════════════════════════════
   buildVoceTab — sezione impostazioni TTS (Piper)
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildVoceTab()
{
    struct VoceInfo {
        QString id;         /* nome file ONNX senza estensione */
        QString label;      /* nome leggibile */
        QString sex;        /* "maschio" / "femmina" */
        QString quality;    /* "veloce" / "media" / "alta" */
        int     sizeMb;     /* dimensione approssimativa MB */
        QString hfPath;     /* percorso HuggingFace relativo alla root voices/ */
    };

    static const VoceInfo voices[] = {
        { "it_IT-riccardo-x_low",
          "Riccardo", "maschio", "veloce", 24,
          "it/it_IT/riccardo/x_low" },
        { "it_IT-paola-medium",
          "Paola", "femmina", "media", 66,
          "it/it_IT/paola/medium" },
        { "it_IT-fabian-medium",
          "Fabian", "maschio", "media", 66,
          "it/it_IT/fabian/medium" },
    };
    static constexpr int N_VOICES = int(sizeof(voices) / sizeof(voices[0]));

    /* (URL HuggingFace e binario Piper definiti inline nei lambda) */

    /* ── Colonna sinistra: TTS Piper (nessun scroll interno) ── */
    auto* inner = new QGroupBox(
        "\xf0\x9f\x94\x8a  Voce \xe2\x80\x94 Piper TTS");
    inner->setObjectName("cardGroup");
    auto* ilay  = new QVBoxLayout(inner);
    ilay->setContentsMargins(14, 10, 14, 10);
    ilay->setSpacing(8);

    /* ── Sezione 1: stato Piper ── */
    auto* secPiper = new QFrame(inner);
    secPiper->setObjectName("cardFrame");
    auto* secLay = new QVBoxLayout(secPiper);
    secLay->setContentsMargins(14, 10, 14, 10);
    secLay->setSpacing(8);

    auto* piperRow = new QWidget(secPiper);
    auto* piperRowLay = new QHBoxLayout(piperRow);
    piperRowLay->setContentsMargins(0, 0, 0, 0);
    piperRowLay->setSpacing(10);

    auto* lblPiperStatus = new QLabel(secPiper);
    lblPiperStatus->setObjectName("cardDesc");

    auto refreshStatus = [lblPiperStatus]() {
        const QString bin = ImpostazioniPage::piperBinPath();
        if (bin.isEmpty())
            lblPiperStatus->setText(
                "\xe2\x9d\x8c  Piper non trovato &mdash; clicca <b>Installa Piper</b>");
        else
            lblPiperStatus->setText(
                "\xe2\x9c\x85  Piper pronto: <code>" + bin + "</code>");
    };
    refreshStatus();

    auto* btnInstall = new QPushButton(secPiper);
    btnInstall->setObjectName("actionBtn");
    btnInstall->setToolTip("Scarica e installa il binario Piper nella cartella del progetto (<appDir>/piper/)");

    std::function<void()> refreshBtn = [btnInstall]() {
        const bool installed = !ImpostazioniPage::piperBinPath().isEmpty();
        if (installed)
            btnInstall->setText("\xe2\x9c\x85  Installato");
        else
            btnInstall->setText("\xf0\x9f\x93\xa5  Installa Piper");
    };
    refreshBtn();

    piperRowLay->addWidget(lblPiperStatus, 1);
    piperRowLay->addWidget(btnInstall);
    secLay->addWidget(piperRow);

    /* Nota fallback */
    auto* lblFallback = new QLabel(
        "\xf0\x9f\x92\xa1  Fallback automatico se Piper non disponibile: "
        "<b>espeak-ng</b> \xe2\x86\x92 spd-say \xe2\x86\x92 festival",
        secPiper);
    lblFallback->setObjectName("cardDesc");
    lblFallback->setTextFormat(Qt::RichText);
    secLay->addWidget(lblFallback);

    ilay->addWidget(secPiper);

    /* ── Sezione 2: voci italiane ── */
    auto* secVoci = new QFrame(inner);
    secVoci->setObjectName("cardFrame");
    auto* secVLay = new QVBoxLayout(secVoci);
    secVLay->setContentsMargins(14, 10, 14, 10);
    secVLay->setSpacing(6);

    auto* voceTitle = new QLabel(
        "\xf0\x9f\x87\xae\xf0\x9f\x87\xb9  <b>Voci Italiane disponibili</b>",
        secVoci);
    voceTitle->setObjectName("cardTitle");
    voceTitle->setTextFormat(Qt::RichText);
    secVLay->addWidget(voceTitle);

    /* Combo selezione voce attiva */
    auto* activeRow = new QWidget(secVoci);
    auto* activeRowLay = new QHBoxLayout(activeRow);
    activeRowLay->setContentsMargins(0, 0, 0, 0);
    activeRowLay->setSpacing(8);
    activeRowLay->addWidget(new QLabel("Voce attiva:", secVoci));
    auto* cmbVoice = new QComboBox(secVoci);
    cmbVoice->setObjectName("settingsCombo");
    for (int i = 0; i < N_VOICES; i++) {
        cmbVoice->addItem(
            QString("%1 (%2, %3)").arg(voices[i].label, voices[i].sex, voices[i].quality),
            voices[i].id);
    }
    /* Seleziona quella attiva */
    {
        const QString active = ImpostazioniPage::piperActiveVoice();
        for (int i = 0; i < N_VOICES; i++)
            if (voices[i].id == active) { cmbVoice->setCurrentIndex(i); break; }
    }
    QObject::connect(cmbVoice, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     inner, [cmbVoice](int idx){
        ImpostazioniPage::savePiperActiveVoice(cmbVoice->itemData(idx).toString());
    });
    activeRowLay->addWidget(cmbVoice, 1);
    secVLay->addWidget(activeRow);

    /* Griglia voci con pulsante scarica per ognuna */
    for (int i = 0; i < N_VOICES; i++) {
        const VoceInfo& v = voices[i];

        auto* row = new QWidget(secVoci);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 2, 0, 2);
        rowLay->setSpacing(10);

        /* Check se già installata */
        const QString onnxPath = ImpostazioniPage::piperVoicesDir()
                                + "/" + v.id + ".onnx";
        const bool installed = QFileInfo::exists(onnxPath);

        auto* lbl = new QLabel(
            QString("<b>%1</b>  <small>%2, %3, ~%4 MB</small>")
            .arg(v.label, v.sex, v.quality).arg(v.sizeMb),
            row);
        lbl->setObjectName("cardDesc");
        lbl->setTextFormat(Qt::RichText);

        auto* lblVoiceStatus = new QLabel(
            installed ? "\xe2\x9c\x85 Installata" : "\xe2\x80\x94", row);
        lblVoiceStatus->setObjectName("cardDesc");
        lblVoiceStatus->setMinimumWidth(100);

        auto* btnDl = new QPushButton(
            installed ? "\xf0\x9f\x94\x84 Reinstalla" : "\xf0\x9f\x93\xa5 Scarica",
            row);
        btnDl->setObjectName("actionBtn");
        btnDl->setToolTip(
            QString("Scarica %1.onnx (~%2 MB) da HuggingFace")
            .arg(v.id).arg(v.sizeMb));

        rowLay->addWidget(lbl, 1);
        rowLay->addWidget(lblVoiceStatus);
        rowLay->addWidget(btnDl);
        secVLay->addWidget(row);

        /* Cattura dati necessari nel lambda (copia by value) */
        const QString voiceId   = v.id;
        const QString hfPath    = v.hfPath;
        QObject::connect(btnDl, &QPushButton::clicked, inner,
                         [btnDl, lblVoiceStatus, voiceId, hfPath]() mutable
        {
            const QString voicesDir = ImpostazioniPage::piperVoicesDir();
            QDir().mkpath(voicesDir);

            static const QString hfBase =
                "https://huggingface.co/rhasspy/piper-voices/resolve/main/";
            const QString onnxUrl  = hfBase + hfPath + "/" + voiceId + ".onnx";
            const QString jsonUrl  = hfBase + hfPath + "/" + voiceId + ".onnx.json";
            const QString onnxDest = voicesDir + "/" + voiceId + ".onnx";
            const QString jsonDest = voicesDir + "/" + voiceId + ".onnx.json";

            btnDl->setEnabled(false);
            btnDl->setText("\xe2\x8f\xb3 Scaricamento...");
            lblVoiceStatus->setText("\xe2\x8f\xb3 Download .json...");

#ifdef Q_OS_WIN
            const QString downloader = "curl.exe";
#else
            const QString downloader = "curl";
#endif
            /* Prima scarica il .json (piccolo), poi .onnx (grande) */
            auto* procJson = new QProcess(btnDl);
            procJson->start(downloader,
                { "-L", "-o", jsonDest, jsonUrl });

            QObject::connect(procJson,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                btnDl, [procJson, btnDl, lblVoiceStatus,
                        downloader, onnxUrl, onnxDest, voiceId](int code, QProcess::ExitStatus) {
                    procJson->deleteLater();
                    if (code != 0) {
                        lblVoiceStatus->setText("\xe2\x9d\x8c Errore .json");
                        btnDl->setEnabled(true);
                        btnDl->setText("\xf0\x9f\x93\xa5 Scarica");
                        return;
                    }
                    lblVoiceStatus->setText("\xe2\x8f\xb3 Download .onnx...");
                    auto* procOnnx = new QProcess(btnDl);
                    procOnnx->start(downloader,
                        { "-L", "-o", onnxDest, onnxUrl });
                    QObject::connect(procOnnx,
                        QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                        btnDl, [procOnnx, btnDl, lblVoiceStatus, voiceId](int code2, QProcess::ExitStatus) {
                            procOnnx->deleteLater();
                            btnDl->setEnabled(true);
                            if (code2 == 0) {
                                lblVoiceStatus->setText("\xe2\x9c\x85 Installata");
                                btnDl->setText("\xf0\x9f\x94\x84 Reinstalla");
                            } else {
                                lblVoiceStatus->setText("\xe2\x9d\x8c Errore .onnx");
                                btnDl->setText("\xf0\x9f\x93\xa5 Scarica");
                            }
                        });
                });
        });
    }

    ilay->addWidget(secVoci);

    /* ── Sezione 3: installa binario Piper ── */
    QObject::connect(btnInstall, &QPushButton::clicked, inner,
                     [btnInstall, lblPiperStatus, refreshStatus]()
    {
#if defined(Q_OS_WIN)
        static const QString PIPER_BIN_URL2 =
            "https://github.com/rhasspy/piper/releases/download/2023.11.14-2/"
            "piper_windows_amd64.zip";
        static const QString PIPER_ARCHIVE2 = "piper_windows_amd64.zip";
#elif defined(Q_OS_MACOS)
        static const QString PIPER_BIN_URL2 =
            "https://github.com/rhasspy/piper/releases/download/2023.11.14-2/"
            "piper_macos_x86_64.tar.gz";
        static const QString PIPER_ARCHIVE2 = "piper_macos_x86_64.tar.gz";
#else
        static const QString PIPER_BIN_URL2 =
            "https://github.com/rhasspy/piper/releases/download/2023.11.14-2/"
            "piper_linux_x86_64.tar.gz";
        static const QString PIPER_ARCHIVE2 = "piper_linux_x86_64.tar.gz";
#endif
        const QString installDir = s_piperDir();
        QDir().mkpath(installDir);
        const QString archivePath = installDir + "/" + PIPER_ARCHIVE2;

        btnInstall->setEnabled(false);
        btnInstall->setText("\xe2\x8f\xb3 Scaricamento...");
        lblPiperStatus->setText("\xe2\x8f\xb3 Download binario Piper...");

#ifdef Q_OS_WIN
        const QString downloader = "curl.exe";
#else
        const QString downloader = "curl";
#endif
        auto* proc = new QProcess(btnInstall);
        proc->start(downloader, { "-L", "-o", archivePath, PIPER_BIN_URL2 });
        QObject::connect(proc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            btnInstall, [proc, btnInstall, lblPiperStatus, refreshStatus,
                         archivePath, installDir](int code, QProcess::ExitStatus) {
                proc->deleteLater();
                if (code != 0) {
                    lblPiperStatus->setText("\xe2\x9d\x8c Errore download. Verifica la connessione.");
                    btnInstall->setEnabled(true);
                    btnInstall->setText("\xf0\x9f\x93\xa5  Installa Piper");
                    return;
                }
                lblPiperStatus->setText("\xe2\x8f\xb3 Estrazione archivio...");
                /* Estrai: tar o unzip */
                auto* procEx = new QProcess(btnInstall);
#ifdef Q_OS_WIN
                procEx->setWorkingDirectory(installDir);
                procEx->start("powershell",
                    { "-Command", QString("Expand-Archive -Path \"%1\" -DestinationPath \"%2\" -Force")
                      .arg(archivePath, installDir) });
#else
                procEx->start("tar", { "-xzf", archivePath, "-C", installDir, "--strip-components=1" });
#endif
                QObject::connect(procEx,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    btnInstall, [procEx, btnInstall, lblPiperStatus,
                                 refreshStatus, installDir](int code2, QProcess::ExitStatus) {
                        procEx->deleteLater();
                        btnInstall->setEnabled(true);
#ifndef Q_OS_WIN
                        /* Rendi eseguibile */
                        QFile::setPermissions(installDir + "/piper",
                            QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                            QFileDevice::ExeOwner  | QFileDevice::ReadGroup  |
                            QFileDevice::ExeGroup  | QFileDevice::ReadOther  |
                            QFileDevice::ExeOther);
#endif
                        if (code2 == 0) {
                            refreshStatus();
                            btnInstall->setText(!ImpostazioniPage::piperBinPath().isEmpty()
                                ? "\xe2\x9c\x85  Installato"
                                : "\xf0\x9f\x93\xa5  Installa Piper");
                        } else {
                            lblPiperStatus->setText("\xe2\x9d\x8c Errore estrazione.");
                        }
                    });
            });
    });

    /* ── Sezione 4: test TTS ── */
    auto* secTest = new QFrame(inner);
    secTest->setObjectName("cardFrame");
    auto* secTLay = new QVBoxLayout(secTest);
    secTLay->setContentsMargins(14, 10, 14, 10);
    secTLay->setSpacing(8);

    auto* testTitle = new QLabel(
        "\xf0\x9f\x94\x8a  <b>Test voce</b>", secTest);
    testTitle->setObjectName("cardTitle");
    testTitle->setTextFormat(Qt::RichText);
    secTLay->addWidget(testTitle);

    auto* testRow = new QWidget(secTest);
    auto* testRowLay = new QHBoxLayout(testRow);
    testRowLay->setContentsMargins(0, 0, 0, 0);
    testRowLay->setSpacing(8);

    auto* txtTest = new QLineEdit(secTest);
    txtTest->setObjectName("chatInput");
    txtTest->setPlaceholderText("Scrivi un testo di prova...");
    txtTest->setText("Ciao, sono Prismalux. La conoscenza \xc3\xa8 potere.");

    auto* btnSpeak = new QPushButton(
        "\xf0\x9f\x94\x8a  Parla", secTest);
    btnSpeak->setObjectName("actionBtn");
    btnSpeak->setToolTip("Legge il testo con la voce selezionata");

    testRowLay->addWidget(txtTest, 1);
    testRowLay->addWidget(btnSpeak);
    secTLay->addWidget(testRow);

    auto* lblTestStatus = new QLabel("", secTest);
    lblTestStatus->setObjectName("cardDesc");
    secTLay->addWidget(lblTestStatus);

    QObject::connect(btnSpeak, &QPushButton::clicked, inner,
                     [btnSpeak, txtTest, lblTestStatus, cmbVoice]() {
        const QString text = txtTest->text().trimmed();
        if (text.isEmpty()) return;

        const QString piperBin = ImpostazioniPage::piperBinPath();
        const QString voiceId  = cmbVoice->currentData().toString();
        const QString onnxPath = ImpostazioniPage::piperVoicesDir()
                                + "/" + voiceId + ".onnx";

        if (!piperBin.isEmpty() && QFileInfo::exists(onnxPath)) {
            /* Usa Piper: echo TEXT | piper --model VOICE --output_raw | aplay */
            btnSpeak->setEnabled(false);
            lblTestStatus->setText("\xf0\x9f\x94\x8a  Piper in esecuzione...");
#ifdef Q_OS_WIN
            const QString cmd = QString(
                "echo %1 | \"%2\" --model \"%3\" --output_raw | "
                "powershell -Command \"$input | Set-Content piper_out.raw\"")
                .arg(text, piperBin, onnxPath);
            QProcess::startDetached("cmd", { "/c", cmd });
#else
            /* Pipeline: piper → aplay (Linux) */
            auto* proc = new QProcess(btnSpeak);
            proc->start("bash", { "-c",
                QString("echo %1 | \"%2\" --model \"%3\" --output_raw | aplay -r 22050 -f S16_LE -t raw -")
                .arg(QProcess::nullDevice(), piperBin, onnxPath)
                .replace(QProcess::nullDevice(),
                         "'" + text.toHtmlEscaped().replace("'","'\\''") + "'")
            });
            QObject::connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                btnSpeak, [proc, btnSpeak, lblTestStatus](int, QProcess::ExitStatus){
                    proc->deleteLater();
                    btnSpeak->setEnabled(true);
                    lblTestStatus->setText("");
                });
#endif
            return;
        }

        /* Fallback: espeak-ng */
        lblTestStatus->setText(piperBin.isEmpty()
            ? "\xe2\x9a\xa0  Piper non trovato, uso espeak-ng"
            : "\xe2\x9a\xa0  Voce non installata, uso espeak-ng");
        if (!QProcess::startDetached("espeak-ng", { "-l", "it", text }))
            QProcess::startDetached("spd-say", { "--lang", "it", "--", text });
        QTimer::singleShot(3000, lblTestStatus, [lblTestStatus]{ lblTestStatus->setText(""); });
    });

    ilay->addWidget(secTest);
    ilay->addStretch();
    return inner;
}

/* ══════════════════════════════════════════════════════════════
   buildTrascriviTab — impostazioni STT (whisper.cpp)
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildTrascriviTab()
{
    /* ── Modelli disponibili ── */
    struct ModelInfo {
        QString id;       /* nome file senza estensione */
        QString label;    /* nome leggibile */
        int     sizeMb;
        QString desc;
    };
    static const ModelInfo kModels[] = {
        { "ggml-tiny",     "Tiny",    39,   "Velocissimo, meno preciso" },
        { "ggml-base",     "Base",    74,   "Buon compromesso velocit\xc3\xa0/precisione" },
        { "ggml-small",    "Small",  141,   "Consigliato — bilanciato" },
        { "ggml-medium",   "Medium", 462,   "Alta precisione, pi\xc3\xb9 lento" },
        { "ggml-large-v3", "Large", 1550,   "Massima precisione" },
    };
    static constexpr int N = int(sizeof kModels / sizeof kModels[0]);

    const QString hfBase =
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/";
    const QString modelsDir = SttWhisper::whisperModelsDir();

    /* ── Colonna destra: STT Whisper (nessun scroll interno) ── */
    auto* inner = new QGroupBox(
        "\xf0\x9f\x8e\xa4  Trascrizione \xe2\x80\x94 Whisper STT");
    inner->setObjectName("cardGroup");
    auto* ilay = new QVBoxLayout(inner);
    ilay->setContentsMargins(14, 10, 14, 10);
    ilay->setSpacing(8);

    /* ══════════════════════════════════════════
       Sezione 1: stato binario whisper-cli
       ══════════════════════════════════════════ */
    auto* secBin = new QFrame(inner);
    secBin->setObjectName("actionCard");
    auto* secBinLay = new QVBoxLayout(secBin);
    secBinLay->setContentsMargins(14, 10, 14, 10);
    secBinLay->setSpacing(6);

    auto* binTitle = new QLabel("\xf0\x9f\x94\x8d  <b>Binario whisper-cli</b>", secBin);
    binTitle->setObjectName("cardTitle");
    binTitle->setTextFormat(Qt::RichText);
    secBinLay->addWidget(binTitle);

    auto* lblBin = new QLabel(secBin);
    lblBin->setObjectName("cardDesc");
    lblBin->setTextFormat(Qt::RichText);
    lblBin->setWordWrap(true);
    secBinLay->addWidget(lblBin);

    /* Pulsante ricontrolla — aggiorna lo stato senza riaprire le impostazioni */
    auto* btnRescan = new QPushButton("\xf0\x9f\x94\x84  Ricontrolla", secBin);
    btnRescan->setObjectName("actionBtn");
    btnRescan->setFixedHeight(28);
    btnRescan->setToolTip("Riscansiona i percorsi noti per whisper-cli.");
    secBinLay->addWidget(btnRescan);

    /* Pulsante compila da sorgente — visibile solo se binario mancante */
    auto* btnCompileWsp = new QPushButton(
        "\xf0\x9f\x94\xa8  Scarica e compila whisper.cpp da sorgente", secBin);
    btnCompileWsp->setObjectName("primaryBtn");
    btnCompileWsp->setFixedHeight(34);
    btnCompileWsp->setVisible(SttWhisper::whisperBin().isEmpty());
    btnCompileWsp->setToolTip(
        "Clona il repo whisper.cpp in ~/.prismalux/whisper.cpp/ e lo compila.\n"
        "Richiede: git, cmake, gcc/g++ (tutti disponibili su Ubuntu/Fedora/Arch).");
    secBinLay->addWidget(btnCompileWsp);

    /* Aggiorna label + visibilità pulsanti */
    auto refreshBinStatus = [lblBin, btnCompileWsp, btnRescan]() {
        const QString b = SttWhisper::whisperBin();
        if (b.isEmpty()) {
            lblBin->setText(
                "\xe2\x9d\x8c  Non trovato. Installa con:<br>"
                "<code>sudo apt install whisper-cpp</code>"
                "&nbsp;&nbsp;oppure&nbsp;&nbsp;"
                "<code>sudo dnf install whisper-cpp</code><br>"
                "Oppure compila da sorgente: "
                "<code>git clone https://github.com/ggml-org/whisper.cpp &amp;&amp; "
                "cd whisper.cpp &amp;&amp; cmake -B build &amp;&amp; "
                "cmake --build build -j$(nproc)</code>");
        } else {
            lblBin->setText("\xe2\x9c\x85  Trovato: <code>" + b + "</code>");
        }
        btnCompileWsp->setVisible(b.isEmpty());
        btnRescan->setVisible(b.isEmpty());  /* nasconde dopo il rilevamento */
    };

    connect(btnRescan, &QPushButton::clicked, secBin, refreshBinStatus);
    refreshBinStatus();   /* imposta lo stato corretto già all'apertura */

    ilay->addWidget(secBin);

    /* ── Card log compilazione (nascosta finché non si clicca) ── */
    auto* secCompile = new QFrame(inner);
    secCompile->setObjectName("actionCard");
    secCompile->setVisible(false);
    auto* secCLay = new QVBoxLayout(secCompile);
    secCLay->setContentsMargins(14, 10, 14, 10);
    secCLay->setSpacing(6);

    auto* compileTitle = new QLabel(
        "\xf0\x9f\x94\xa8  <b>Compilazione whisper.cpp</b>", secCompile);
    compileTitle->setObjectName("cardTitle");
    compileTitle->setTextFormat(Qt::RichText);
    secCLay->addWidget(compileTitle);

    auto* compileLog = new QPlainTextEdit(secCompile);
    compileLog->setReadOnly(true);
    compileLog->setMaximumBlockCount(800);
    compileLog->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 8));
    compileLog->setMaximumHeight(120);
    compileLog->setObjectName("inputArea");
    secCLay->addWidget(compileLog);

    auto* lblCompileStatus = new QLabel("", secCompile);
    lblCompileStatus->setObjectName("statusLabel");
    lblCompileStatus->setWordWrap(true);
    secCLay->addWidget(lblCompileStatus);

    ilay->addWidget(secCompile);

    /* ── Lambda che esegue la build a 3 step ── */
    connect(btnCompileWsp, &QPushButton::clicked, secCompile,
            [btnCompileWsp, secCompile, compileLog, lblCompileStatus,
             lblBin, refreshActive = (std::function<void()>)nullptr]() mutable
    {
        btnCompileWsp->setEnabled(false);
        secCompile->setVisible(true);
        compileLog->clear();
        lblCompileStatus->setText("\xe2\x8c\x9b  Avvio in corso...");

        const QString wspDir  = QDir::homePath() + "/.prismalux/whisper.cpp";
        const QString bldDir  = wspDir + "/build";
        const int     threads = std::max(1, QThread::idealThreadCount());

        /* Usiamo un QProcess* condiviso tramite un piccolo wrapper a step */
        struct StepRunner {
            QPlainTextEdit*  log;
            QLabel*          status;
            QPushButton*     btn;
            QLabel*          lblBin;
            int              step = 0;

            void run(const QString& prog, const QStringList& args,
                     const QString& desc,
                     std::function<void(bool ok)> next)
            {
                log->appendPlainText("\n\xe2\x96\xb6 " + desc);
                log->appendPlainText("$ " + prog + " " + args.join(" ") + "\n");
                status->setText("\xe2\x8c\x9b  " + desc + "...");

                auto* proc = new QProcess(log);
                QObject::connect(proc, &QProcess::readyReadStandardOutput,
                                 log, [proc, this]{
                    log->appendPlainText(
                        QString::fromLocal8Bit(proc->readAllStandardOutput()));
                });
                QObject::connect(proc, &QProcess::readyReadStandardError,
                                 log, [proc, this]{
                    log->appendPlainText(
                        QString::fromLocal8Bit(proc->readAllStandardError()));
                });
                QObject::connect(
                    proc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    log, [proc, next, this](int code, QProcess::ExitStatus) {
                        proc->deleteLater();
                        const bool ok = (code == 0);
                        if (!ok)
                            log->appendPlainText("\n\xe2\x9d\x8c  Step fallito (codice " +
                                                 QString::number(code) + ")");
                        next(ok);
                    });
                proc->start(prog, args);
                if (!proc->waitForStarted(3000)) {
                    log->appendPlainText(
                        "\xe2\x9d\x8c  Impossibile avviare: " + prog +
                        "\n  Verifica che git/cmake/gcc siano installati.");
                    proc->deleteLater();
                    next(false);
                }
            }
        };

        auto* runner = new StepRunner{compileLog, lblCompileStatus, btnCompileWsp, lblBin};

        /* Step 1: git clone o git pull */
        QString gitProg; QStringList gitArgs;
        if (QDir(wspDir + "/.git").exists()) {
            gitProg = "git";
            gitArgs = {"-C", wspDir, "pull", "--rebase", "--autostash"};
        } else {
            QDir().mkpath(QDir::homePath() + "/.prismalux");
            gitProg = "git";
            gitArgs = {"clone", "--depth=1",
                       "https://github.com/ggml-org/whisper.cpp", wspDir};
        }

        runner->run(gitProg, gitArgs, "Clona/aggiorna whisper.cpp",
            [runner, bldDir, wspDir, threads](bool ok1) {
            if (!ok1) {
                runner->status->setText(
                    "\xe2\x9d\x8c  git fallito. Verifica la connessione e che git sia installato.");
                runner->btn->setEnabled(true);
                delete runner; return;
            }

            /* Step 2: cmake configure */
            runner->run("cmake",
                {"-S", wspDir, "-B", bldDir,
                 "-DCMAKE_BUILD_TYPE=Release",
                 "-DBUILD_SHARED_LIBS=OFF",
                 "-DWHISPER_BUILD_TESTS=OFF",
                 "-DWHISPER_BUILD_EXAMPLES=ON"},
                "Configurazione cmake",
                [runner, bldDir, threads](bool ok2) {
                if (!ok2) {
                    runner->status->setText(
                        "\xe2\x9d\x8c  cmake fallito. Verifica che cmake e gcc siano installati.");
                    runner->btn->setEnabled(true);
                    delete runner; return;
                }

                /* Step 3: cmake build */
                runner->run("cmake",
                    {"--build", bldDir, "-j", QString::number(threads)},
                    "Compilazione (può richiedere qualche minuto)",
                    [runner](bool ok3) {
                    if (ok3) {
                        /* Cerca il binario compilato */
                        const QString bin = SttWhisper::whisperBin();
                        if (!bin.isEmpty()) {
                            runner->status->setText(
                                "\xe2\x9c\x85  whisper-cli compilato con successo!\n"
                                "Percorso: " + bin);
                            runner->log->appendPlainText(
                                "\n\xe2\x9c\x85  Build completata. Binario: " + bin);
                            runner->lblBin->setText(
                                "\xe2\x9c\x85  Trovato: <code>" + bin + "</code>");
                            runner->btn->setVisible(false);
                        } else {
                            runner->status->setText(
                                "\xe2\x9a\xa0  Compilato ma binario non trovato nel percorso atteso.\n"
                                "Controlla ~/.prismalux/whisper.cpp/build/bin/");
                            runner->log->appendPlainText(
                                "\n\xe2\x9a\xa0  Build completata, ma whisper-cli non trovato.");
                        }
                    } else {
                        runner->status->setText(
                            "\xe2\x9d\x8c  Compilazione fallita. Controlla il log sopra.");
                    }
                    runner->btn->setEnabled(true);
                    delete runner;
                });
            });
        });
    });

    /* ══════════════════════════════════════════
       Sezione 2: modello attivo + selettore
       ══════════════════════════════════════════ */
    auto* secActive = new QFrame(inner);
    secActive->setObjectName("actionCard");
    auto* secActLay = new QVBoxLayout(secActive);
    secActLay->setContentsMargins(14, 10, 14, 10);
    secActLay->setSpacing(8);

    auto* actTitle = new QLabel(
        "\xf0\x9f\x93\x8c  <b>Modello attivo</b>", secActive);
    actTitle->setObjectName("cardTitle");
    actTitle->setTextFormat(Qt::RichText);
    secActLay->addWidget(actTitle);

    auto* lblActivePath = new QLabel(secActive);
    lblActivePath->setObjectName("cardDesc");
    lblActivePath->setTextFormat(Qt::RichText);
    lblActivePath->setWordWrap(true);
    auto refreshActive = [lblActivePath](){
        const QString m = SttWhisper::whisperModel();
        if (m.isEmpty())
            lblActivePath->setText(
                "\xe2\x9d\x8c  Nessun modello trovato &mdash; scaricane uno qui sotto.");
        else
            lblActivePath->setText("\xe2\x9c\x85  <code>" + m + "</code>");
    };
    refreshActive();
    secActLay->addWidget(lblActivePath);

    /* Combo selezione + pulsante Applica */
    auto* selRow    = new QWidget(secActive);
    auto* selRowLay = new QHBoxLayout(selRow);
    selRowLay->setContentsMargins(0, 0, 0, 0);
    selRowLay->setSpacing(8);

    auto* cmbModel = new QComboBox(secActive);
    cmbModel->setObjectName("settingsCombo");
    for (int i = 0; i < N; i++) {
        const QString binPath = modelsDir + "/" + kModels[i].id + ".bin";
        const bool installed  = QFileInfo::exists(binPath);
        cmbModel->addItem(
            QString("%1  (%2 MB)%3").arg(kModels[i].label).arg(kModels[i].sizeMb)
                .arg(installed ? "  \xe2\x9c\x85" : ""),
            binPath);
        /* Preseleziona quello già attivo */
        if (SttWhisper::whisperModel() == binPath)
            cmbModel->setCurrentIndex(i);
    }

    auto* btnApply = new QPushButton("\xe2\x9c\x94  Applica", secActive);
    btnApply->setObjectName("actionBtn");
    btnApply->setToolTip("Imposta il modello selezionato come modello attivo");
    QObject::connect(btnApply, &QPushButton::clicked, inner,
                     [cmbModel, refreshActive, lblActivePath](){
        const QString path = cmbModel->currentData().toString();
        if (!QFileInfo::exists(path)) {
            lblActivePath->setText(
                "\xe2\x9a\xa0  Modello non ancora scaricato. "
                "Usa il pulsante \xe2\x86\x93\xc2\xa0Scarica qui sotto.");
            return;
        }
        SttWhisper::savePreferredModel(path);
        refreshActive();
    });

    selRowLay->addWidget(new QLabel("Seleziona:", secActive));
    selRowLay->addWidget(cmbModel, 1);
    selRowLay->addWidget(btnApply);
    secActLay->addWidget(selRow);
    ilay->addWidget(secActive);

    /* ══════════════════════════════════════════
       Sezione 3: griglia modelli con download
       ══════════════════════════════════════════ */
    auto* secModels = new QFrame(inner);
    secModels->setObjectName("actionCard");
    auto* secMLay = new QVBoxLayout(secModels);
    secMLay->setContentsMargins(14, 10, 14, 10);
    secMLay->setSpacing(6);

    auto* modTitle = new QLabel(
        "\xf0\x9f\x93\xa6  <b>Modelli disponibili</b> "
        "<small style='color:#888;'>(scaricati in ~/.prismalux/whisper/models/)</small>",
        secModels);
    modTitle->setObjectName("cardTitle");
    modTitle->setTextFormat(Qt::RichText);
    secMLay->addWidget(modTitle);

    QDir().mkpath(modelsDir);

    for (int i = 0; i < N; i++) {
        const ModelInfo& m = kModels[i];
        const QString binPath = modelsDir + "/" + m.id + ".bin";

        auto* row    = new QWidget(secModels);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 3, 0, 3);
        rowLay->setSpacing(10);

        /* Etichetta */
        auto* lbl = new QLabel(
            QString("<b>%1</b>  <small>~%2 MB &mdash; %3</small>")
                .arg(m.label).arg(m.sizeMb).arg(m.desc),
            row);
        lbl->setObjectName("cardDesc");
        lbl->setTextFormat(Qt::RichText);

        /* Stato */
        const bool installed = QFileInfo::exists(binPath);
        auto* lblStatus = new QLabel(
            installed ? "\xe2\x9c\x85 Presente" : "\xe2\x80\x94", row);
        lblStatus->setObjectName("cardDesc");
        lblStatus->setMinimumWidth(90);

        /* Pulsante download */
        auto* btnDl = new QPushButton(
            installed ? "\xf0\x9f\x94\x84  Riscarica" : "\xf0\x9f\x93\xa5  Scarica",
            row);
        btnDl->setObjectName("actionBtn");
        btnDl->setToolTip(
            QString("Scarica %1.bin (~%2 MB) da HuggingFace")
                .arg(m.id).arg(m.sizeMb));

        rowLay->addWidget(lbl, 1);
        rowLay->addWidget(lblStatus);
        rowLay->addWidget(btnDl);
        secMLay->addWidget(row);

        /* Lambda download */
        const QString modelId   = m.id;
        const QString modelPath = binPath;
        const QString dlUrl     = hfBase + modelId + ".bin";

        QObject::connect(btnDl, &QPushButton::clicked, inner,
            [btnDl, lblStatus, modelPath, dlUrl, cmbModel,
             refreshActive, modelsDir, modelId]() mutable
        {
            QDir().mkpath(modelsDir);
            btnDl->setEnabled(false);
            btnDl->setText("\xe2\x8f\xb3  Download...");
            lblStatus->setText("\xe2\x8f\xb3 Scaricamento...");

#ifdef Q_OS_WIN
            const QString dl = "curl.exe";
            QStringList args = { "-L", "--progress-bar", "-o", modelPath, dlUrl };
#else
            /* wget con progresso mega: stampa ogni ~1 MB */
            const bool hasWget =
                !QStandardPaths::findExecutable("wget").isEmpty();
            const QString dl = hasWget ? "wget" : "curl";
            QStringList args = hasWget
                ? QStringList{ "--progress=dot:mega", "-O", modelPath, dlUrl }
                : QStringList{ "-L", "--progress-bar", "-o", modelPath, dlUrl };
#endif
            auto* proc = new QProcess(btnDl);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            proc->start(dl, args);

            QObject::connect(proc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                btnDl,
                [proc, btnDl, lblStatus, modelPath, modelId,
                 cmbModel, refreshActive](int code, QProcess::ExitStatus)
            {
                proc->deleteLater();
                btnDl->setEnabled(true);
                const qint64 sz = QFileInfo(modelPath).size();
                if (code == 0 && sz > 10'000'000LL) {
                    lblStatus->setText("\xe2\x9c\x85 Presente");
                    btnDl->setText("\xf0\x9f\x94\x84  Riscarica");
                    /* Aggiorna il testo della voce nel combo */
                    for (int j = 0; j < cmbModel->count(); j++) {
                        if (cmbModel->itemData(j).toString() == modelPath) {
                            const QString cur = cmbModel->itemText(j);
                            if (!cur.contains("\xe2\x9c\x85"))
                                cmbModel->setItemText(j, cur + "  \xe2\x9c\x85");
                            /* Se nessun modello è ancora preferito, imposta questo */
                            if (SttWhisper::preferredModel().isEmpty())
                                SttWhisper::savePreferredModel(modelPath);
                            break;
                        }
                    }
                    refreshActive();
                } else {
                    /* Download incompleto — rimuovi file parziale */
                    if (sz < 10'000'000LL) QFile::remove(modelPath);
                    lblStatus->setText("\xe2\x9d\x8c Errore");
                    btnDl->setText("\xf0\x9f\x93\xa5  Scarica");
                }
            });
        });
    }

    ilay->addWidget(secModels);

    /* ══════════════════════════════════════════
       Sezione 4: nota lingua + test rapido
       ══════════════════════════════════════════ */
    auto* secNote = new QFrame(inner);
    secNote->setObjectName("actionCard");
    auto* secNoteLay = new QVBoxLayout(secNote);
    secNoteLay->setContentsMargins(14, 10, 14, 10);
    secNoteLay->setSpacing(6);

    auto* noteTitle = new QLabel(
        "\xf0\x9f\x92\xa1  <b>Note</b>", secNote);
    noteTitle->setObjectName("cardTitle");
    noteTitle->setTextFormat(Qt::RichText);
    secNoteLay->addWidget(noteTitle);

    auto* noteText = new QLabel(
        "\xe2\x80\xa2  La trascrizione avviene <b>offline</b> sul dispositivo &mdash; "
        "nessun dato inviato a server esterni.<br>"
        "\xe2\x80\xa2  La lingua predefinita \xc3\xa8 <b>Italiano</b>. "
        "whisper.cpp supporta oltre 99 lingue.<br>"
        "\xe2\x80\xa2  Requisiti: <code>arecord</code> (pacchetto <i>alsa-utils</i>) "
        "per la registrazione microfono.<br>"
        "\xe2\x80\xa2  Il modello <b>Small</b> (~141 MB) offre il miglior equilibrio "
        "velocit\xc3\xa0/precisione per l\xe2\x80\x99italiano.",
        secNote);
    noteText->setObjectName("cardDesc");
    noteText->setTextFormat(Qt::RichText);
    noteText->setWordWrap(true);
    secNoteLay->addWidget(noteText);

    ilay->addWidget(secNote);
    ilay->addStretch();
    return inner;
}

/* ══════════════════════════════════════════════════════════════
   buildLlmConsigliatiTab — catalogo LLM diviso in due sezioni:
   1) Ollama  — installa con "ollama pull"
   2) llama.cpp — scarica .gguf da HuggingFace
   ══════════════════════════════════════════════════════════════ */


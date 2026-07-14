/* ══════════════════════════════════════════════════════════════
   main_programming_driverkernel.cpp — ProgrammazionePage: Driver & Kernel
   ============================================================================
   Sub-tab "🔧 Driver & Kernel" — builder (scomposto in 5 sotto-metodi:
   NVIDIA/AMD/Kernel Linux/USB+WIBY/RE Kernel) + slot. Split da
   main_programming.cpp/main_programming_slots.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_programming.h"
#include "main_programming_p.h"
#include "../prismalux_paths.h"
#include "../dpi_utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QProcess>
#include <QGroupBox>
#include <QFrame>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QSizePolicy>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QTextCursor>

namespace P = PrismaluxPaths;

QWidget* ProgrammazionePage::buildDriverKernelTab(QWidget* parent)
{
    auto* wrap = new QWidget(parent);
    auto* lay  = new QVBoxLayout(wrap);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* tabs = new QTabWidget(wrap);
    tabs->setObjectName("innerTabs");

    buildDriverNvidiaSubTab(tabs);
    buildDriverAmdSubTab(tabs);
    buildDriverKernelSubTab(tabs);
    buildDriverUsbSubTab(tabs);
    buildDriverReKernelSubTab(tabs);

    lay->addWidget(tabs);
    return wrap;
}

void ProgrammazionePage::buildDriverNvidiaSubTab(QTabWidget* tabs)
{
    auto* w   = new QWidget(tabs);
    auto* vl  = new QVBoxLayout(w);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* info = new QLabel(w);
    info->setWordWrap(true);
    info->setTextFormat(Qt::RichText);
    info->setObjectName("hintLabel");
    info->setText(
        "<b>\xf0\x9f\x9f\xa2 Driver NVIDIA</b><br>"
        "Gestisci i driver proprietari NVIDIA per Linux. "
        "<code>nvidia-smi</code> mostra lo stato della GPU, i processi attivi "
        "e la versione del driver installato. "
        "DKMS garantisce che il modulo kernel venga ricompilato automaticamente "
        "dopo ogni aggiornamento del kernel."
    );
    vl->addWidget(info);

    /* Area output condivisa — puntata da m_driverOutput */
    m_driverOutput = new QTextEdit(w);
    m_driverOutput->setReadOnly(true);
    m_driverOutput->setObjectName("codeOutput");
    m_driverOutput->setPlaceholderText(
        "L'output dei comandi apparir\xc3\xa0 qui...");
    vl->addWidget(m_driverOutput, 1);

    auto* btnRow = new QWidget(w);
    auto* bl     = new QHBoxLayout(btnRow);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(8);

    auto* btnDetect   = new QPushButton(
        "\xf0\x9f\x94\x8d  Rilevamento GPU", btnRow);
    auto* btnDownload = new QPushButton(
        "\xf0\x9f\x93\xa6  Scarica Driver", btnRow);
    auto* btnGuide    = new QPushButton(
        "\xf0\x9f\x92\xa1  Guida AI", btnRow);
    auto* btnDkms     = new QPushButton(
        "\xf0\x9f\x94\x84  Reinstalla DKMS", btnRow);

    bl->addWidget(btnDetect);
    bl->addWidget(btnDownload);
    bl->addWidget(btnGuide);
    bl->addWidget(btnDkms);
    bl->addStretch();
    vl->addWidget(btnRow);

    connect(btnDetect,   &QPushButton::clicked,
            this, &ProgrammazionePage::onNvidiaDetectClicked);
    connect(btnGuide,    &QPushButton::clicked,
            this, &ProgrammazionePage::onNvidiaGuideClicked);
    connect(btnDownload, &QPushButton::clicked,
            this, &ProgrammazionePage::onNvidiaDownloadClicked);
    connect(btnDkms,     &QPushButton::clicked,
            this, &ProgrammazionePage::onNvidiaDkmsClicked);

    tabs->addTab(w, tr("\xf0\x9f\x9f\xa2  Driver NVIDIA"));
}

void ProgrammazionePage::buildDriverAmdSubTab(QTabWidget* tabs)
{
    auto* w   = new QWidget(tabs);
    auto* vl  = new QVBoxLayout(w);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* info = new QLabel(w);
    info->setWordWrap(true);
    info->setTextFormat(Qt::RichText);
    info->setObjectName("hintLabel");
    info->setText(
        "<b>\xf0\x9f\x94\xb4 Driver AMD</b><br>"
        "Il driver <code>amdgpu</code> \xc3\xa8 gi\xc3\xa0 incluso nel kernel Linux "
        "e viene caricato automaticamente per le GPU AMD/Radeon. "
        "<b>ROCm</b> \xc3\xa8 il framework AMD per compute GPU (ML/AI) "
        "e va installato separatamente. "
        "<code>lspci | grep VGA</code> mostra la GPU rilevata dal sistema."
    );
    vl->addWidget(info);

    auto* outAmd = new QTextEdit(w);
    outAmd->setReadOnly(true);
    outAmd->setObjectName("codeOutput");
    outAmd->setPlaceholderText(
        "L'output dei comandi apparir\xc3\xa0 qui...");
    vl->addWidget(outAmd, 1);

    auto* btnRow = new QWidget(w);
    auto* bl     = new QHBoxLayout(btnRow);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(8);

    auto* btnDetect   = new QPushButton(
        "\xf0\x9f\x94\x8d  Rilevamento GPU", btnRow);
    auto* btnDownload = new QPushButton(
        "\xf0\x9f\x93\xa6  Scarica ROCm", btnRow);
    auto* btnGuide    = new QPushButton(
        "\xf0\x9f\x92\xa1  Guida AI", btnRow);

    bl->addWidget(btnDetect);
    bl->addWidget(btnDownload);
    bl->addWidget(btnGuide);
    bl->addStretch();
    vl->addWidget(btnRow);

    m_driverAmdOutput = outAmd;

    connect(btnDetect,   &QPushButton::clicked,
            this, &ProgrammazionePage::onAmdDetectClicked);
    connect(btnGuide,    &QPushButton::clicked,
            this, &ProgrammazionePage::onAmdGuideClicked);
    connect(btnDownload, &QPushButton::clicked,
            this, &ProgrammazionePage::onAmdDownloadClicked);

    tabs->addTab(w, tr("\xf0\x9f\x94\xb4  Driver AMD"));
}

void ProgrammazionePage::buildDriverKernelSubTab(QTabWidget* tabs)
{
    auto* w   = new QWidget(tabs);
    auto* vl  = new QVBoxLayout(w);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* info = new QLabel(w);
    info->setWordWrap(true);
    info->setTextFormat(Qt::RichText);
    info->setObjectName("hintLabel");
    info->setText(
        "<b>\xf0\x9f\x90\xa7 Kernel Linux</b><br>"
        "Visualizza la versione kernel attiva con <code>uname -r</code>. "
        "Su sistemi Debian/Ubuntu puoi elencare i kernel installati con "
        "<code>dpkg --list | grep linux-image</code>. "
        "<b>Attenzione:</b> compilare un kernel personalizzato \xc3\xa8 un'operazione "
        "avanzata riservata a utenti esperti."
    );
    vl->addWidget(info);

    auto* outKernel = new QTextEdit(w);
    outKernel->setReadOnly(true);
    outKernel->setObjectName("codeOutput");
    outKernel->setPlaceholderText(
        "L'output dei comandi apparir\xc3\xa0 qui...");
    vl->addWidget(outKernel, 1);

    auto* btnRow = new QWidget(w);
    auto* bl     = new QHBoxLayout(btnRow);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(8);

    auto* btnVer    = new QPushButton(
        "\xf0\x9f\x94\x8d  Versione attuale", btnRow);
    auto* btnList   = new QPushButton(
        "\xf0\x9f\x93\x8b  Lista kernel", btnRow);
    auto* btnGuide  = new QPushButton(
        "\xf0\x9f\x92\xa1  Compila Kernel (AI)", btnRow);
    auto* btnSafety = new QPushButton(
        "\xe2\x9a\xa0  Nota sicurezza", btnRow);

    bl->addWidget(btnVer);
    bl->addWidget(btnList);
    bl->addWidget(btnGuide);
    bl->addWidget(btnSafety);
    bl->addStretch();
    vl->addWidget(btnRow);

    m_driverKernelOutput = outKernel;

    connect(btnVer,    &QPushButton::clicked,
            this, &ProgrammazionePage::onKernelVersionClicked);
    connect(btnList,   &QPushButton::clicked,
            this, &ProgrammazionePage::onKernelListClicked);
    connect(btnGuide,  &QPushButton::clicked,
            this, &ProgrammazionePage::onKernelGuideClicked);
    connect(btnSafety, &QPushButton::clicked,
            this, &ProgrammazionePage::onKernelSafetyClicked);

    tabs->addTab(w, tr("\xf0\x9f\x90\xa7  Kernel Linux"));
}

void ProgrammazionePage::buildDriverUsbSubTab(QTabWidget* tabs)
{
    auto* w  = new QWidget(tabs);
    auto* hl = new QHBoxLayout(w);
    hl->setContentsMargins(6, 6, 6, 6);
    hl->setSpacing(6);

    /* ── Colonna sinistra: scroll area con tutti i controlli ── */
    auto* leftScroll = new QScrollArea(w);
    leftScroll->setWidgetResizable(true);
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    leftScroll->setFixedWidth(dpiScale(310));
    leftScroll->setFrameShape(QFrame::NoFrame);

    auto* leftWidget = new QWidget(leftScroll);
    auto* vl = new QVBoxLayout(leftWidget);
    vl->setContentsMargins(4, 4, 4, 4);
    vl->setSpacing(5);
    leftScroll->setWidget(leftWidget);

    /* Header */
    auto* info = new QLabel(leftWidget);
    info->setWordWrap(true);
    info->setTextFormat(Qt::RichText);
    info->setObjectName("hintLabel");
    info->setText(
        "<b>\xf0\x9f\x93\xb9 USB / Firmware &amp; Videocam LAN</b><br>"
        "Analizza USB, dump/flash firmware DFU, server MJPEG locale senza cloud."
    );
    vl->addWidget(info);

    /* ── URL viewer ── */
    {
        auto* sep = new QFrame(leftWidget);
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        vl->addWidget(sep);

        auto* lbl = new QLabel(tr("\xf0\x9f\x94\x97  URL stream:"), leftWidget);
        vl->addWidget(lbl);

        auto* row = new QWidget(leftWidget);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(4);

        m_camPreviewUrl = new QLineEdit(row);
        m_camPreviewUrl->setPlaceholderText("http://10.42.0.1:8090/");
        m_camPreviewUrl->setText("http://10.42.0.1:8090/");

        auto* btnConn = new QPushButton(tr("\xf0\x9f\x94\x97  Connetti"), row);
        btnConn->setObjectName("primaryButton");
        auto* btnDisc = new QPushButton(tr("\xe2\x9c\x96  Disco."), row);

        rl->addWidget(m_camPreviewUrl, 1);
        rl->addWidget(btnConn);
        rl->addWidget(btnDisc);
        vl->addWidget(row);

        connect(btnConn, &QPushButton::clicked,
                this, &ProgrammazionePage::onCamPreviewConnect);
        connect(btnDisc, &QPushButton::clicked,
                this, &ProgrammazionePage::onCamPreviewFinished);
    }

    /* Status server */
    m_camServerStatus = new QLabel(
        "\xe2\x97\x8f  Server non avviato", leftWidget);
    m_camServerStatus->setStyleSheet("color: #888; font-size: 12px;");
    vl->addWidget(m_camServerStatus);

    /* ── USB / V4L2 ── */
    {
        auto* sep = new QFrame(leftWidget);
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        vl->addWidget(sep);

        auto* grpUsb = new QGroupBox(tr("USB / V4L2"), leftWidget);
        auto* gvl    = new QVBoxLayout(grpUsb);
        gvl->setSpacing(4);

        auto* row1 = new QWidget(grpUsb);
        auto* rl1  = new QHBoxLayout(row1);
        rl1->setContentsMargins(0, 0, 0, 0);
        rl1->setSpacing(4);
        auto* vidLbl = new QLabel(tr("VID:PID:"), row1);
        m_usbVidPidEdit = new QLineEdit(row1);
        m_usbVidPidEdit->setPlaceholderText(tr("es. 05c8:03ab"));
        rl1->addWidget(vidLbl);
        rl1->addWidget(m_usbVidPidEdit, 1);
        gvl->addWidget(row1);

        auto* row2 = new QWidget(grpUsb);
        auto* rl2  = new QHBoxLayout(row2);
        rl2->setContentsMargins(0, 0, 0, 0);
        rl2->setSpacing(4);
        auto* btnList   = new QPushButton(tr("\xf0\x9f\x94\x8d Elenca USB"), row2);
        auto* btnV4l2   = new QPushButton(tr("\xf0\x9f\x93\xb9 V4L2"),       row2);
        auto* btnDetail = new QPushButton(tr("\xf0\x9f\x93\x8b Dettagli"),    row2);
        auto* btnUdev   = new QPushButton(tr("\xf0\x9f\x94\xa7 udevadm"),     row2);
        btnList->setToolTip(tr("Elenca dispositivi USB collegati (lsusb)"));
        btnV4l2->setToolTip(tr("Elenca dispositivi Video4Linux: webcam, TV tuner (v4l2-ctl)"));
        btnDetail->setToolTip(tr("Dettagli completi del dispositivo USB selezionato"));
        btnUdev->setToolTip(tr("Info udevadm sul dispositivo: regole, attributi, path"));
        rl2->addWidget(btnList);
        rl2->addWidget(btnV4l2);
        rl2->addWidget(btnDetail);
        rl2->addWidget(btnUdev);
        gvl->addWidget(row2);
        vl->addWidget(grpUsb);

        connect(btnList,   &QPushButton::clicked,
                this, &ProgrammazionePage::onUsbListClicked);
        connect(btnV4l2,   &QPushButton::clicked,
                this, &ProgrammazionePage::onUsbV4l2Clicked);
        connect(btnDetail, &QPushButton::clicked,
                this, &ProgrammazionePage::onUsbDetailsClicked);
        connect(btnUdev,   &QPushButton::clicked,
                this, &ProgrammazionePage::onUsbUdevClicked);
    }

    /* ── DFU + Server MJPEG ── */
    {
        auto* grpDfu = new QGroupBox(tr("DFU / Server MJPEG"), leftWidget);
        auto* gvl    = new QVBoxLayout(grpDfu);
        gvl->setSpacing(4);

        auto* row1 = new QWidget(grpDfu);
        auto* rl1  = new QHBoxLayout(row1);
        rl1->setContentsMargins(0, 0, 0, 0);
        rl1->setSpacing(4);
        auto* btnDfuList  = new QPushButton(tr("\xf0\x9f\x93\xa6 Lista"),    row1);
        auto* btnDfuDump  = new QPushButton(tr("\xf0\x9f\x92\xbe Dump"),     row1);
        auto* btnDfuFlash = new QPushButton(tr("\xe2\x9a\xa1 Flash"),        row1);
        auto* btnAiGuide  = new QPushButton(tr("\xf0\x9f\x92\xa1 Guida AI"), row1);
        btnDfuList->setToolTip(tr("Elenca dispositivi DFU collegati (dfu-util -l)"));
        btnDfuDump->setToolTip(tr("Scarica firmware dal dispositivo DFU in un file (dfu-util -U)"));
        btnDfuFlash->setToolTip(tr("Carica file firmware sul dispositivo DFU (dfu-util -D)"));
        btnAiGuide->setToolTip(tr("Genera guida AI specifica per il dispositivo selezionato"));
        rl1->addWidget(btnDfuList);
        rl1->addWidget(btnDfuDump);
        rl1->addWidget(btnDfuFlash);
        rl1->addWidget(btnAiGuide);
        gvl->addWidget(row1);

        auto* row2 = new QWidget(grpDfu);
        auto* rl2  = new QHBoxLayout(row2);
        rl2->setContentsMargins(0, 0, 0, 0);
        rl2->setSpacing(4);
        m_camDeviceCombo = new QComboBox(row2);
        m_camDeviceCombo->addItem("/dev/video0", 0);
        m_camDeviceCombo->addItem("/dev/video1", 1);
        m_camDeviceCombo->addItem("/dev/video2", 2);
        m_camPortSpin = new QSpinBox(row2);
        m_camPortSpin->setRange(1024, 65535);
        m_camPortSpin->setValue(8090);
        m_camPortSpin->setFixedWidth(dpiScale(70));
        m_camPortSpin->setPrefix(":");
        auto* btnRefresh = new QPushButton("\xf0\x9f\x94\x84", row2);
        btnRefresh->setToolTip(tr("Aggiorna V4L2"));
        btnRefresh->setFixedWidth(dpiScale(30));
        auto* btnStart = new QPushButton(tr("\xe2\x96\xb6 Avvia"), row2);
        btnStart->setObjectName("primaryButton");
        btnStart->setToolTip(tr("Avvia server MJPEG sulla porta specificata"));
        auto* btnStop = new QPushButton(tr("\xe2\x96\xa0 Ferma"), row2);
        btnStop->setToolTip(tr("Ferma il server MJPEG"));
        rl2->addWidget(m_camDeviceCombo, 1);
        rl2->addWidget(btnRefresh);
        rl2->addWidget(m_camPortSpin);
        rl2->addWidget(btnStart);
        rl2->addWidget(btnStop);
        gvl->addWidget(row2);
        vl->addWidget(grpDfu);

        connect(btnDfuList,  &QPushButton::clicked,
                this, &ProgrammazionePage::onDfuListClicked);
        connect(btnDfuDump,  &QPushButton::clicked,
                this, &ProgrammazionePage::onDfuDumpClicked);
        connect(btnDfuFlash, &QPushButton::clicked,
                this, &ProgrammazionePage::onDfuFlashClicked);
        connect(btnAiGuide,  &QPushButton::clicked,
                this, &ProgrammazionePage::onUsbAiGuideClicked);
        connect(btnRefresh,  &QPushButton::clicked,
                this, &ProgrammazionePage::onCamRefreshDevices);
        connect(btnStart,    &QPushButton::clicked,
                this, &ProgrammazionePage::onCamServerStartClicked);
        connect(btnStop,     &QPushButton::clicked,
                this, &ProgrammazionePage::onCamServerStopClicked);
    }

    /* ── Pannello WIBY Camera PTZ ── */
    {
        auto* sep = new QFrame(leftWidget);
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        vl->addWidget(sep);

        auto* wibyLbl = new QLabel(
            "<b>\xf0\x9f\x8e\xa5 WIBY Smart Camera \xe2\x80\x94 PTZ LAN (192.168.1.222)</b>",
            leftWidget);
        wibyLbl->setTextFormat(Qt::RichText);
        wibyLbl->setWordWrap(true);
        vl->addWidget(wibyLbl);

        /* Riga connessione */
        auto* connRow = new QWidget(leftWidget);
        auto* crl = new QHBoxLayout(connRow);
        crl->setContentsMargins(0, 0, 0, 0);
        crl->setSpacing(4);
        auto* btnWibyDiscover = new QPushButton(
            "\xf0\x9f\x94\x8d Cerca LAN", connRow);
        auto* btnWibyConn = new QPushButton(
            "\xf0\x9f\x94\x97 Connetti", connRow);
        btnWibyConn->setObjectName("primaryButton");
        auto* btnWibyDisc = new QPushButton(
            "\xe2\x9c\x96 Disco.", connRow);
        crl->addWidget(btnWibyDiscover);
        crl->addWidget(btnWibyConn);
        crl->addWidget(btnWibyDisc);
        vl->addWidget(connRow);

        m_wibyStatusLbl = new QLabel(
            "\xe2\x97\x8f  Non connesso", leftWidget);
        m_wibyStatusLbl->setStyleSheet("color:#888; font-size:12px;");
        vl->addWidget(m_wibyStatusLbl);

        connect(btnWibyDiscover, &QPushButton::clicked,
                this, &ProgrammazionePage::onWibyDiscoverClicked);
        connect(btnWibyConn, &QPushButton::clicked,
                this, &ProgrammazionePage::onWibyConnectClicked);
        connect(btnWibyDisc, &QPushButton::clicked,
                this, &ProgrammazionePage::onWibyDisconnectClicked);

        /* Corpo: joystick PTZ (sinistra) + impostazioni (destra) */
        auto* bodyRow = new QWidget(leftWidget);
        auto* brl = new QHBoxLayout(bodyRow);
        brl->setContentsMargins(0, 0, 0, 0);
        brl->setSpacing(8);

        /* Griglia PTZ 3x3 */
        auto* ptzGroup = new QGroupBox(tr("PTZ"), bodyRow);
        auto* grid = new QGridLayout(ptzGroup);
        grid->setSpacing(2);

        auto makePtz = [&](const char* lbl, int r, int c, const QString& dir) {
            auto* b = new QPushButton(QString::fromUtf8(lbl), ptzGroup);
            b->setFixedSize(dpiScale(40), dpiScale(32));
            grid->addWidget(b, r, c);
            connect(b, &QPushButton::pressed,
                    this, [this, dir]{ onWibyPtzClicked(dir); });
            connect(b, &QPushButton::released,
                    this, &ProgrammazionePage::onWibyPtzStop);
            return b;
        };

        makePtz("\xe2\x86\x96", 0, 0, "up_left");
        makePtz("\xe2\x86\x91", 0, 1, "up");
        makePtz("\xe2\x86\x97", 0, 2, "up_right");
        makePtz("\xe2\x86\x90", 1, 0, "left");

        auto* btnStopPtz = new QPushButton("\xe2\x8f\xb9", ptzGroup);
        btnStopPtz->setFixedSize(dpiScale(40), dpiScale(32));
        btnStopPtz->setToolTip(tr("Stop PTZ"));
        grid->addWidget(btnStopPtz, 1, 1);
        connect(btnStopPtz, &QPushButton::clicked,
                this, &ProgrammazionePage::onWibyPtzStop);

        makePtz("\xe2\x86\x92", 1, 2, "right");
        makePtz("\xe2\x86\x99", 2, 0, "down_left");
        makePtz("\xe2\x86\x93", 2, 1, "down");
        makePtz("\xe2\x86\x98", 2, 2, "down_right");

        brl->addWidget(ptzGroup);

        /* Impostazioni camera */
        auto* toggleGroup = new QGroupBox(tr("Camera"), bodyRow);
        auto* tgl = new QVBoxLayout(toggleGroup);
        tgl->setSpacing(2);

        auto makeToggle = [&](const char* lbl, const char* code) {
            auto* b = new QPushButton(QString::fromUtf8(lbl), toggleGroup);
            b->setCheckable(true);
            tgl->addWidget(b);
            QString codeStr = QString::fromLatin1(code);
            connect(b, &QPushButton::toggled, toggleGroup,
                    [this, codeStr](bool on){
                onWibyToggleDp(0, on, codeStr);
            });
            return b;
        };

        makeToggle("\xf0\x9f\x92\xa1 LED",         "basic_indicator");
        makeToggle("\xf0\x9f\x94\x84 Capovolgi",   "basic_flip");
        makeToggle("\xf0\x9f\x95\x90 Timestamp",   "basic_osd");
        makeToggle("\xf0\x9f\x94\x92 Privacy",     "basic_private");
        makeToggle("\xf0\x9f\x8f\x83 Movimento",   "motion_switch");
        makeToggle("\xf0\x9f\x8e\xaf Tracking",    "motion_tracking");
        makeToggle("\xe2\x8f\xba Registra",        "record_switch");

        /* Enum: notturna */
        auto* rowNv = new QWidget(toggleGroup);
        auto* rlNv  = new QHBoxLayout(rowNv);
        rlNv->setContentsMargins(0,0,0,0); rlNv->setSpacing(3);
        rlNv->addWidget(new QLabel("\xf0\x9f\x8c\x99", rowNv));
        auto* cbNv = new QComboBox(rowNv);
        cbNv->addItem("Auto","0"); cbNv->addItem("Off","1"); cbNv->addItem("Forza","2");
        rlNv->addWidget(cbNv, 1);
        tgl->addWidget(rowNv);
        connect(cbNv, QOverload<int>::of(&QComboBox::currentIndexChanged),
                toggleGroup, [this, cbNv](int){
            onWibyToggleDp(108, cbNv->currentData().toString(), "basic_nightvision");
        });

        /* Enum: sensibilità */
        auto* rowMs = new QWidget(toggleGroup);
        auto* rlMs  = new QHBoxLayout(rowMs);
        rlMs->setContentsMargins(0,0,0,0); rlMs->setSpacing(3);
        rlMs->addWidget(new QLabel("\xf0\x9f\x93\xa1", rowMs));
        auto* cbMs = new QComboBox(rowMs);
        cbMs->addItem("Bassa","0"); cbMs->addItem("Media","1"); cbMs->addItem("Alta","2");
        rlMs->addWidget(cbMs, 1);
        tgl->addWidget(rowMs);
        connect(cbMs, QOverload<int>::of(&QComboBox::currentIndexChanged),
                toggleGroup, [this, cbMs](int){
            onWibyToggleDp(106, cbMs->currentData().toString(), "motion_sensitivity");
        });

        /* Enum: modalità registrazione */
        auto* rowRm = new QWidget(toggleGroup);
        auto* rlRm  = new QHBoxLayout(rowRm);
        rlRm->setContentsMargins(0,0,0,0); rlRm->setSpacing(3);
        rlRm->addWidget(new QLabel("\xf0\x9f\x93\xbc", rowRm));
        auto* cbRm = new QComboBox(rowRm);
        cbRm->addItem("Continua","1"); cbRm->addItem("Su mov.","2");
        rlRm->addWidget(cbRm, 1);
        tgl->addWidget(rowRm);
        connect(cbRm, QOverload<int>::of(&QComboBox::currentIndexChanged),
                toggleGroup, [this, cbRm](int){
            onWibyToggleDp(151, cbRm->currentData().toString(), "record_mode");
        });

        brl->addWidget(toggleGroup, 1);
        vl->addWidget(bodyRow);

        /* ── Stream video WIBY ── */
        {
            auto* sep2 = new QFrame(leftWidget);
            sep2->setFrameShape(QFrame::HLine);
            sep2->setFrameShadow(QFrame::Sunken);
            vl->addWidget(sep2);

            auto* hint = new QLabel(leftWidget);
            hint->setWordWrap(true);
            hint->setObjectName("hintLabel");
            hint->setTextFormat(Qt::RichText);
            hint->setText(
                "\xf0\x9f\x93\xa1 <b>Stream</b>: attiva <i>IoT Video Live</i> "
                "su iot.tuya.com, premi <b>Ottieni URL</b> oppure incolla HLS/RTSP."
            );
            vl->addWidget(hint);

            auto* streamRow = new QWidget(leftWidget);
            auto* srl = new QHBoxLayout(streamRow);
            srl->setContentsMargins(0, 0, 0, 0);
            srl->setSpacing(4);
            auto* btnGetUrl = new QPushButton(
                "\xf0\x9f\x93\xa1 Ottieni URL", streamRow);
            m_wibyStreamUrl = new QLineEdit(streamRow);
            m_wibyStreamUrl->setPlaceholderText(tr("URL HLS o RTSP..."));
            auto* btnStartStream = new QPushButton(
                "\xe2\x96\xb6 Avvia viewer", streamRow);
            btnStartStream->setObjectName("primaryButton");
            auto* btnStopStream = new QPushButton(
                "\xe2\x96\xa0 Ferma", streamRow);
            srl->addWidget(btnGetUrl);
            srl->addWidget(m_wibyStreamUrl, 1);
            srl->addWidget(btnStartStream);
            srl->addWidget(btnStopStream);
            vl->addWidget(streamRow);

            connect(btnGetUrl, &QPushButton::clicked,
                    this, &ProgrammazionePage::onWibyGetStreamUrl);
            connect(btnStartStream, &QPushButton::clicked,
                    this, &ProgrammazionePage::onWibyStartStream);
            connect(btnStopStream, &QPushButton::clicked,
                    this, &ProgrammazionePage::onWibyStopStream);
        }

        /* ── Firmware offline ── */
        {
            auto* btnFwGuide = new QPushButton(
                "\xf0\x9f\x93\x96 Guida: firmware offline (OpenIPC + UART)",
                leftWidget);
            vl->addWidget(btnFwGuide);
            connect(btnFwGuide, &QPushButton::clicked,
                    this, &ProgrammazionePage::onWibyFirmwareGuide);
        }

        /* ── MITM locale ── */
        {
            auto* sep4 = new QFrame(leftWidget);
            sep4->setFrameShape(QFrame::HLine);
            sep4->setFrameShadow(QFrame::Sunken);
            vl->addWidget(sep4);

            auto* mitmHint = new QLabel(leftWidget);
            mitmHint->setWordWrap(true);
            mitmHint->setObjectName("hintLabel");
            mitmHint->setTextFormat(Qt::RichText);
            mitmHint->setText(
                "\xf0\x9f\x95\xb5 <b>MITM locale</b> \xe2\x80\x94 "
                "Hotspot WiFi, connetti WIBY, intercetta tutto il traffico cloud."
            );
            vl->addWidget(mitmHint);

            auto* mitmRow = new QWidget(leftWidget);
            auto* mrl = new QHBoxLayout(mitmRow);
            mrl->setContentsMargins(0,0,0,0);
            mrl->setSpacing(4);
            auto* btnMitmStart = new QPushButton(
                "\xf0\x9f\x95\xb5 Avvia MITM", mitmRow);
            btnMitmStart->setObjectName("primaryButton");
            auto* btnMitmStop = new QPushButton(
                "\xe2\x96\xa0 Ferma", mitmRow);
            m_wibyMitmStatus = new QLabel(
                "\xe2\x97\x8f  Inattivo", mitmRow);
            m_wibyMitmStatus->setStyleSheet("color:#888;font-size:12px;");
            mrl->addWidget(btnMitmStart);
            mrl->addWidget(btnMitmStop);
            mrl->addSpacing(6);
            mrl->addWidget(m_wibyMitmStatus);
            vl->addWidget(mitmRow);

            connect(btnMitmStart, &QPushButton::clicked,
                    this, &ProgrammazionePage::onWibyMitmStartClicked);
            connect(btnMitmStop, &QPushButton::clicked,
                    this, &ProgrammazionePage::onWibyMitmStopClicked);
        }
    }

    vl->addStretch();

    /* ── Colonna destra: video viewer (grande) + log (piccolo) ── */
    auto* rightWidget = new QWidget(w);
    auto* rvl = new QVBoxLayout(rightWidget);
    rvl->setContentsMargins(0, 0, 0, 0);
    rvl->setSpacing(4);

    m_camPreviewLbl = new QLabel(rightWidget);
    m_camPreviewLbl->setAlignment(Qt::AlignCenter);
    m_camPreviewLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_camPreviewLbl->setMinimumSize(dpiScale(200), dpiScale(150));
    m_camPreviewLbl->setStyleSheet(
        "background:#111; color:#555; border-radius:6px;");
    m_camPreviewLbl->setText(
        "Nessun feed \xe2\x80\x94 avvia il server e premi \xf0\x9f\x94\x97 Connetti");
    rvl->addWidget(m_camPreviewLbl, 1);

    m_usbOutput = new QTextEdit(rightWidget);
    m_usbOutput->setReadOnly(true);
    m_usbOutput->setObjectName("codeOutput");
    m_usbOutput->setFixedHeight(dpiScale(110));
    m_usbOutput->setPlaceholderText(
        "L'output dei comandi USB apparir\xc3\xa0 qui...");
    rvl->addWidget(m_usbOutput);

    hl->addWidget(leftScroll);
    hl->addWidget(rightWidget, 1);

    tabs->addTab(w, tr("\xf0\x9f\x93\xb9  USB & Videocam LAN"));
}

void ProgrammazionePage::buildDriverReKernelSubTab(QTabWidget* tabs)
{
    auto* w  = new QWidget(tabs);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(7);

    auto* info = new QLabel(w);
    info->setWordWrap(true);
    info->setTextFormat(Qt::RichText);
    info->setObjectName("hintLabel");
    info->setText(
        "<b>\xf0\x9f\x94\x8d Reverse Engineering \xe2\x80\x94 Driver &amp; Kernel</b><br>"
        "Analizza binari kernel, moduli <code>.ko</code>, device driver: "
        "simboli ELF, disassembly, syscall tracing. "
        "Seleziona il target e usa gli strumenti, poi chiedi spiegazione all'AI."
    );
    vl->addWidget(info);

    /* ── Selezione target ── */
    {
        auto* row = new QWidget(w);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(6);

        rl->addWidget(new QLabel(tr("\xf0\x9f\x93\x81  Target:"), row));
        m_reTargetEdit = new QLineEdit(row);
        m_reTargetEdit->setPlaceholderText(
            "/path/to/module.ko   oppure   nome-simbolo-da-cercare");
        auto* btnBrowse = new QPushButton(tr("\xf0\x9f\x93\x82  Sfoglia"), row);
        btnBrowse->setFixedWidth(dpiScale(90));
        rl->addWidget(m_reTargetEdit, 1);
        rl->addWidget(btnBrowse);
        vl->addWidget(row);

        connect(btnBrowse, &QPushButton::clicked, w, [this]{
            QString f = QFileDialog::getOpenFileName(
                this, "Seleziona target RE", QDir::homePath(),
                "Binari / Moduli (*.ko *.o *.elf *.so *.bin *);;"
                "Tutti i file (*)");
            if (!f.isEmpty()) m_reTargetEdit->setText(f);
        });
    }

    /* ── Strumenti ELF / Binario ── */
    {
        auto* grp = new QGroupBox(tr("Analisi ELF / Binario"), w);
        auto* gl  = new QHBoxLayout(grp);
        gl->setSpacing(5);

        auto* btnFile    = new QPushButton(tr("\xf0\x9f\x93\x84  file"),        grp);
        auto* btnReadelf = new QPushButton(tr("\xf0\x9f\x93\x96  readelf"),     grp);
        auto* btnObjdump = new QPushButton(tr("\xf0\x9f\x92\xbb  objdump -d"),  grp);
        auto* btnNm      = new QPushButton(tr("\xf0\x9f\x94\xa4  nm"),          grp);
        auto* btnStrings = new QPushButton(tr("\xf0\x9f\x94\xa1  strings"),     grp);
        auto* btnLdd     = new QPushButton(tr("\xf0\x9f\x94\x97  ldd"),         grp);
        btnFile->setToolTip(tr("Identifica tipo e formato del file binario (file <nome>)"));
        btnReadelf->setToolTip(tr("Analizza struttura ELF: sezioni, simboli, note, dipendenze (readelf -a)"));
        btnObjdump->setToolTip(tr("Disassembla le sezioni .text dell'eseguibile (objdump -d)"));
        btnNm->setToolTip(tr("Elenca simboli definiti e importati nel binario (nm)"));
        btnStrings->setToolTip(tr("Estrae stringhe leggibili ASCII/UTF-8 dall'eseguibile (strings)"));
        btnLdd->setToolTip(tr("Mostra le librerie dinamiche richieste dall'eseguibile (ldd)"));

        gl->addWidget(btnFile);
        gl->addWidget(btnReadelf);
        gl->addWidget(btnObjdump);
        gl->addWidget(btnNm);
        gl->addWidget(btnStrings);
        gl->addWidget(btnLdd);
        gl->addStretch();
        vl->addWidget(grp);

        connect(btnFile,    &QPushButton::clicked,
                this, &ProgrammazionePage::onReFileClicked);
        connect(btnReadelf, &QPushButton::clicked,
                this, &ProgrammazionePage::onReReadelfClicked);
        connect(btnObjdump, &QPushButton::clicked,
                this, &ProgrammazionePage::onReObjdumpClicked);
        connect(btnNm,      &QPushButton::clicked,
                this, &ProgrammazionePage::onReNmClicked);
        connect(btnStrings, &QPushButton::clicked,
                this, &ProgrammazionePage::onReStringsClicked);
        connect(btnLdd,     &QPushButton::clicked,
                this, &ProgrammazionePage::onReLddClicked);
    }

    /* ── Strumenti Kernel / Moduli ── */
    {
        auto* grp = new QGroupBox(tr("Kernel / Moduli / Tracing"), w);
        auto* gl  = new QHBoxLayout(grp);
        gl->setSpacing(5);

        auto* btnModinfo  = new QPushButton(tr("\xf0\x9f\x94\x8e  modinfo"),      grp);
        auto* btnLsmod    = new QPushButton(tr("\xf0\x9f\x93\x8b  lsmod"),        grp);
        auto* btnKallsyms = new QPushButton(tr("\xf0\x9f\x94\x8d  kallsyms"),     grp);
        auto* btnDmesgDrv = new QPushButton(tr("\xf0\x9f\x93\x9d  dmesg driver"), grp);
        auto* btnStrace   = new QPushButton(tr("\xf0\x9f\x95\xb5  strace -c"),    grp);
        auto* btnKprobes  = new QPushButton(tr("\xe2\x9a\xa1  kprobes"),          grp);
        btnModinfo->setToolTip(tr("Informazioni dettagliate sul modulo kernel (modinfo <nome>)"));
        btnLsmod->setToolTip(tr("Elenca i moduli kernel caricati attualmente (lsmod)"));
        btnKallsyms->setToolTip(tr("Cerca simbolo nei simboli del kernel live (/proc/kallsyms)"));
        btnDmesgDrv->setToolTip(tr("Filtra i messaggi dmesg del driver/modulo selezionato"));
        btnStrace->setToolTip(tr("Traccia le system call del processo con statistiche (strace -c)"));
        btnKprobes->setToolTip(tr("Crea kprobe sul simbolo kernel per il tracing dinamico"));

        gl->addWidget(btnModinfo);
        gl->addWidget(btnLsmod);
        gl->addWidget(btnKallsyms);
        gl->addWidget(btnDmesgDrv);
        gl->addWidget(btnStrace);
        gl->addWidget(btnKprobes);
        gl->addStretch();
        vl->addWidget(grp);

        connect(btnModinfo,  &QPushButton::clicked,
                this, &ProgrammazionePage::onReModinfoClicked);
        connect(btnLsmod,    &QPushButton::clicked,
                this, &ProgrammazionePage::onReLsmodClicked);
        connect(btnKallsyms, &QPushButton::clicked,
                this, &ProgrammazionePage::onReKallsymsClicked);
        connect(btnDmesgDrv, &QPushButton::clicked,
                this, &ProgrammazionePage::onReDmesgDrvClicked);
        connect(btnStrace,   &QPushButton::clicked,
                this, &ProgrammazionePage::onReStraceClicked);
        connect(btnKprobes,  &QPushButton::clicked,
                this, &ProgrammazionePage::onReKprobesClicked);
    }

    /* ── Riga AI + Pulisci ── */
    {
        auto* row = new QWidget(w);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(6);

        auto* btnAiAnalyze = new QPushButton(
            "\xf0\x9f\xa4\x96  Analisi AI \xe2\x80\x94 spiega output", row);
        btnAiAnalyze->setObjectName("primaryButton");
        auto* btnClear = new QPushButton(tr("\xf0\x9f\x97\x91  Pulisci"), row);
        btnClear->setToolTip(tr("Pulisce l'output del reverse engineering"));

        rl->addWidget(btnAiAnalyze);
        rl->addStretch();
        rl->addWidget(btnClear);
        vl->addWidget(row);

        connect(btnAiAnalyze, &QPushButton::clicked,
                this, &ProgrammazionePage::onReAiAnalyzeClicked);
        connect(btnClear, &QPushButton::clicked, w,
                [this]{ m_reKernelOutput->clear(); });
    }

    /* ── Output ── */
    m_reKernelOutput = new QTextEdit(w);
    m_reKernelOutput->setReadOnly(true);
    m_reKernelOutput->setObjectName("codeOutput");
    m_reKernelOutput->setPlaceholderText(
        "Seleziona un target e usa uno degli strumenti sopra...\n\n"
        "Esempi utili:\n"
        "  file /lib/modules/$(uname -r)/kernel/drivers/usb/serial/ch341.ko\n"
        "  readelf -a mio_driver.ko\n"
        "  nm --defined-only --demangle mio_driver.ko\n"
        "  strings -n 8 /usr/lib/firmware/rtl_bt/rtl8761b_fw.bin\n"
        "  modinfo snd_usb_audio\n"
        "  grep -i <simbolo> /proc/kallsyms\n"
    );
    vl->addWidget(m_reKernelOutput, 1);

    tabs->addTab(w, tr("\xf0\x9f\x94\x8d  RE Kernel"));
}

/* ── helper: esegue un comando read-only e mostra l'output in out ── */
void ProgrammazionePage::onDriverRunCmd(const QString& cmd)
{
    /* Determina quale QTextEdit usare in base al sender (non usato direttamente):
       gli slot chiamanti impostano m_driverAiActive prima di chiamare questa fn. */
    QTextEdit* out = m_driverAiActive ? m_driverAiActive : m_driverOutput;
    if (!out) return;

    if (m_driverProcess) {
        m_driverProcess->kill();
        m_driverProcess->waitForFinished(P::kProcKillExtendedMs);
        m_driverProcess->deleteLater();
        m_driverProcess = nullptr;
    }

    out->clear();
    out->append(QString("<span style='color:#aaa'>$ %1</span>").arg(cmd.toHtmlEscaped()));

    m_driverProcess = new QProcess(this);
    m_driverProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_driverProcess, &QProcess::readyReadStandardOutput,
            this, &ProgrammazionePage::onDriverCmdOutput);
    connect(m_driverProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onDriverCmdFinished);

    m_driverProcess->start("bash", QStringList() << "-c" << cmd);
}

void ProgrammazionePage::onDriverCmdOutput()
{
    if (!m_driverProcess) return;
    QTextEdit* out = m_driverAiActive ? m_driverAiActive : m_driverOutput;
    if (!out) return;
    /* MergedChannels: tutto su stdout */
    const QString txt = QString::fromUtf8(m_driverProcess->readAllStandardOutput());
    if (!txt.isEmpty()) {
        out->moveCursor(QTextCursor::End);
        out->insertPlainText(txt);
        out->ensureCursorVisible();
    }
}

void ProgrammazionePage::onDriverCmdFinished(int exitCode,
                                              QProcess::ExitStatus /*status*/)
{
    QTextEdit* out = m_driverAiActive ? m_driverAiActive : m_driverOutput;
    if (out) {
        const QString msg = exitCode == 0
            ? "\n\xe2\x9c\x85  Completato."
            : QString("\n\xe2\x9d\x8c  Exit code: %1").arg(exitCode);
        out->moveCursor(QTextCursor::End);
        out->insertPlainText(msg);
        out->ensureCursorVisible();
    }
    if (m_driverProcess) {
        m_driverProcess->deleteLater();
        m_driverProcess = nullptr;
    }
}

/* ── helper: invia richiesta guida AI e aggiorna l'output corretto ── */
void ProgrammazionePage::onDriverAiGuide(const QString& topic)
{
    if (m_driverAiBusy) return;

    QTextEdit* out = m_driverAiActive;
    if (!out) return;

    const QString sys =
        "Sei un esperto di sistemi Linux. Rispondi in italiano con comandi "
        "esatti, passo per passo, pronti per essere copiati nel terminale. "
        "Usa blocchi di codice markdown per i comandi.";

    out->clear();
    out->append(
        QString("<span style='color:#aaa'>\xf0\x9f\xa4\x96  %1...</span>")
        .arg("Guida AI in corso"));

    m_driverAiBusy = true;
    disconnect(m_driverAiTokenConn);
    disconnect(m_driverAiFinishedConn);
    disconnect(m_driverAiErrorConn);
    m_driverAiTokenConn    = connect(m_ai, &AiClient::token,
                                     this, &ProgrammazionePage::onDriverAiToken);
    m_driverAiFinishedConn = connect(m_ai, &AiClient::finished,
                                     this, &ProgrammazionePage::onDriverAiFinished);
    m_driverAiErrorConn    = connect(m_ai, &AiClient::error,
                                     this, &ProgrammazionePage::onDriverAiError);

    /* Primo clear per lo streaming pulito */
    out->clear();
    m_ai->chat(P::prependKnowledge(sys), topic);
}

void ProgrammazionePage::onDriverAiToken(const QString& t)
{
    QTextEdit* out = m_driverAiActive;
    if (!out) return;
    out->moveCursor(QTextCursor::End);
    out->insertPlainText(t);
    out->ensureCursorVisible();
}

void ProgrammazionePage::onDriverAiFinished(const QString& /*full*/)
{
    m_driverAiBusy = false;
    disconnect(m_driverAiTokenConn);
    disconnect(m_driverAiFinishedConn);
    disconnect(m_driverAiErrorConn);
}

void ProgrammazionePage::onDriverAiError(const QString& msg)
{
    m_driverAiBusy = false;
    disconnect(m_driverAiTokenConn);
    disconnect(m_driverAiFinishedConn);
    disconnect(m_driverAiErrorConn);
    QTextEdit* out = m_driverAiActive;
    if (out) {
        out->moveCursor(QTextCursor::End);
        out->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore AI: %1").arg(msg));
    }
}

/* ── NVIDIA slots ── */

void ProgrammazionePage::onNvidiaDetectClicked()
{
    m_driverAiActive = m_driverOutput;
    onDriverRunCmd("nvidia-smi");
}

void ProgrammazionePage::onNvidiaDownloadClicked()
{
    QDesktopServices::openUrl(QUrl("https://www.nvidia.com/drivers"));
}

void ProgrammazionePage::onNvidiaDkmsClicked()
{
    if (!m_driverOutput) return;
    m_driverAiActive = m_driverOutput;
    m_driverOutput->clear();
    m_driverOutput->append(
        "<b>Comando da eseguire nel terminale (richiede sudo):</b>\n");
    m_driverOutput->append(
        "<code>sudo apt install --reinstall nvidia-dkms-$(nvidia-smi "
        "--query-gpu=driver_version --format=csv,noheader | cut -d. -f1)</code>\n");
    m_driverOutput->append(
        "\n<span style='color:#aaa'>"
        "\xe2\x84\xb9  Il comando non viene eseguito automaticamente "
        "perch\xc3\xa9 richiede privilegi amministrativi. "
        "Copialo e incollalo nel terminale.</span>");
}

void ProgrammazionePage::onNvidiaGuideClicked()
{
    m_driverAiActive = m_driverOutput;
    onDriverAiGuide(
        "Come installare o aggiornare i driver NVIDIA su Ubuntu/Debian/Arch Linux. "
        "Dammi i comandi esatti passo per passo, inclusi: "
        "rilevamento GPU, aggiunta repository, installazione driver, "
        "configurazione DKMS e verifica finale con nvidia-smi.");
}

/* ── AMD slots ── */

void ProgrammazionePage::onAmdDetectClicked()
{
    m_driverAiActive = m_driverAmdOutput;
    onDriverRunCmd("lspci | grep -i vga ; glxinfo 2>/dev/null | grep -i renderer");
}

void ProgrammazionePage::onAmdDownloadClicked()
{
    QDesktopServices::openUrl(QUrl("https://www.amd.com/support"));
}

void ProgrammazionePage::onAmdGuideClicked()
{
    m_driverAiActive = m_driverAmdOutput;
    onDriverAiGuide(
        "Come configurare i driver AMD/amdgpu su Linux. "
        "Spiega la differenza tra il driver amdgpu incluso nel kernel e ROCm. "
        "Fornisci i comandi per: verificare il driver attivo, installare Mesa, "
        "installare ROCm per ML/AI su GPU AMD, e risolvere problemi comuni.");
}

/* ── Kernel Linux slots ── */

void ProgrammazionePage::onKernelVersionClicked()
{
    m_driverAiActive = m_driverKernelOutput;
    onDriverRunCmd("uname -r");
}

void ProgrammazionePage::onKernelListClicked()
{
    m_driverAiActive = m_driverKernelOutput;
    onDriverRunCmd("dpkg --list 2>/dev/null | grep linux-image || "
                   "rpm -qa 2>/dev/null | grep kernel || "
                   "ls /boot/vmlinuz* 2>/dev/null");
}

void ProgrammazionePage::onKernelGuideClicked()
{
    m_driverAiActive = m_driverKernelOutput;
    onDriverAiGuide(
        "Guida completa alla compilazione di un kernel Linux personalizzato. "
        "Spiega passo per passo: download dei sorgenti dal kernel.org, "
        "copia della configurazione attuale (cp /boot/config-$(uname -r) .config), "
        "make menuconfig, compilazione con make -j$(nproc), "
        "installazione moduli con make modules_install, "
        "installazione kernel con make install, "
        "aggiornamento bootloader con update-grub. "
        "Includi prerequisiti e avvertenze di sicurezza.");
}

void ProgrammazionePage::onKernelSafetyClicked()
{
    QMessageBox::warning(
        this,
        "\xe2\x9a\xa0  Nota sicurezza — Compilazione Kernel",
        "Compilare il kernel \xc3\xa8 un'operazione avanzata che comporta rischi:\n\n"
        "\xe2\x80\xa2 Fare sempre un backup completo del sistema prima di procedere.\n"
        "\xe2\x80\xa2 Su sistemi di produzione usare kernel precompilati e testati.\n"
        "\xe2\x80\xa2 Un kernel mal configurato pu\xc3\xb2 rendere il sistema non avviabile.\n"
        "\xe2\x80\xa2 Mantenere almeno un kernel funzionante nel bootloader.\n"
        "\xe2\x80\xa2 Testare prima in una macchina virtuale o ambiente di staging.\n\n"
        "Procedere solo se si ha esperienza con la gestione del sistema Linux.");
}


/* ══════════════════════════════════════════════════════════════
   mainwindow_header.cpp — MainWindow: header/toolbar
   ==================================================================
   Barra superiore: hamburger, logo, gauges CPU/RAM/GPU, pulsanti
   azione (backend/log). Split da mainwindow.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "mainwindow.h"
#include "dpi_utils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

/* ══════════════════════════════════════════════════════════════
   buildHeader — barra superiore con logo + gauges hardware
   ══════════════════════════════════════════════════════════════ */
QWidget* MainWindow::buildHeader()
{
    auto* hdr = new QWidget(this);
    hdr->setObjectName("header");
    hdr->setFixedHeight(dpiScale(52));

    auto* lay = new QHBoxLayout(hdr);
    lay->setContentsMargins(dpiScale(16), dpiScale(8), dpiScale(16), dpiScale(8));
    lay->setSpacing(12);

    buildHamburgerSection(lay);
    buildLogoSection(lay);
    lay->addStretch(1);
    buildGaugesSection(lay);
    buildActionButtons(lay);

    /* Badge e spinner mantenuti come nullptr — lo stato è nel testo di m_btnBackend */
    m_badgeServer = nullptr;
    m_spinServer  = nullptr;

    return hdr;
}

/* ── Livello 2: hamburger ☰ + messaggi 📋 + impostazioni ⚙️ ──────── */
void MainWindow::buildHamburgerSection(QHBoxLayout* lay)
{
    auto* hdr = qobject_cast<QWidget*>(lay->parent());

    /* ☰ Mostra/Nascondi sidebar */
    auto* btnHamburger = new QPushButton("\xe2\x98\xb0", hdr);
    btnHamburger->setObjectName("hamburgerBtn");
    btnHamburger->setFixedSize(dpiSize(36, 36));
    btnHamburger->setToolTip(tr("Mostra / Nascondi la colonna sinistra"));
    connect(btnHamburger, &QPushButton::clicked, this, &MainWindow::onHamburgerClicked);
    lay->addWidget(btnHamburger);

    /* 📋 Messaggi — pulsante + badge non-letti sovrapposto */
    auto* logWrap = new QWidget(hdr);
    logWrap->setObjectName("logWrap");
    logWrap->setFixedSize(dpiSize(46, 36));
    m_logBtn = new QPushButton("\xf0\x9f\x93\x8b", logWrap);
    m_logBtn->setObjectName("hamburgerBtn");
    m_logBtn->setFixedSize(dpiSize(36, 36));
    m_logBtn->setToolTip(tr("Messaggi \xe2\x80\x94 log eventi, errori AI, backend, pipeline"));
    m_logBtn->setAccessibleName(tr("Apri log messaggi"));
    m_logBtn->move(0, 0);
    m_logBadge = new QLabel("", logWrap);
    m_logBadge->setAlignment(Qt::AlignCenter);
    m_logBadge->setFixedSize(dpiSize(16, 16));
    m_logBadge->setVisible(false);
    m_logBadge->setStyleSheet(
        "background:#e03030; color:#fff; border-radius:8px;"
        "font-size:9px; font-weight:bold;");
    m_logBadge->move(dpiScale(28), 0);
    connect(m_logBtn, &QPushButton::clicked, this, &MainWindow::onLogBtnClicked);
    lay->addWidget(logWrap);

    /* ⚙️ Impostazioni */
    m_settingsBtn = new QPushButton("\xe2\x9a\x99\xef\xb8\x8f", hdr);
    m_settingsBtn->setObjectName("hamburgerBtn");
    m_settingsBtn->setFixedSize(dpiSize(36, 36));
    m_settingsBtn->setToolTip(tr("Impostazioni \xe2\x80\x94 Backend, Hardware, Monitor AI, llama.cpp"));
    m_settingsBtn->setAccessibleName(tr("Apri impostazioni"));
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);
    lay->addWidget(m_settingsBtn);

    lay->addSpacing(4);
}

/* ── Livello 2: 🍺 logo + titolo PRISMALUX ───────────────────────── */
void MainWindow::buildLogoSection(QHBoxLayout* lay)
{
    auto* hdr = qobject_cast<QWidget*>(lay->parent());

    auto* beer = new QLabel("🍺", hdr);
    beer->setObjectName("headerBeer");
    lay->addWidget(beer);

    auto* title = new QLabel(tr("PRISMALUX"), hdr);
    title->setObjectName("headerTitle");
    lay->addWidget(title);

    /* Tenuti come membri per aggiornamenti interni (applyBackend, onModelChanged) */
    m_lblBackend = new QLabel("", hdr);
    m_lblBackend->hide();
    m_lblModel = new QLabel(this);
    m_lblModel->hide();
}

/* ── Livello 2: CPU · RAM · GPU gauges ───────────────────────────── */
void MainWindow::buildGaugesSection(QHBoxLayout* lay)
{
    auto* hdr = qobject_cast<QWidget*>(lay->parent());
    m_gCpu = new ResourceGauge("CPU ", hdr);
    m_gRam = new ResourceGauge("RAM ", hdr);
    m_gGpu = new ResourceGauge("GPU ", hdr);
    lay->addWidget(m_gCpu);
    lay->addWidget(m_gRam);
    lay->addWidget(m_gGpu);

    /* 🧠 contesto LLM — barra come CPU/RAM/GPU (percentuale usata) + indice
       numerico dei token liberi accanto. Aggiornati da AgentiPage::contextUsage()
       dopo ogni turno chat (onContextUsage). */
    m_gCtx = new ResourceGauge("CTX ", hdr);
    lay->addWidget(m_gCtx);

    m_ctxLbl = new QLabel("", hdr);
    m_ctxLbl->setObjectName("ctxLabel");
    m_ctxLbl->setStyleSheet(
        "QLabel#ctxLabel{color:#94a3b8;font-size:11px;padding:0 4px;}");
    lay->addWidget(m_ctxLbl);
    onContextUsage(0, AiChatParams::load().num_ctx);  /* stato iniziale: tutto libero */

    m_tempLbl = new QLabel("", hdr);
    m_tempLbl->setObjectName("tempLabel");
    m_tempLbl->setToolTip(tr("Temperatura CPU / GPU rilevata dal sensore termico"));
    m_tempLbl->setStyleSheet(
        "QLabel#tempLabel{color:#94a3b8;font-size:11px;padding:0 4px;}");
    m_tempLbl->setVisible(false);
    lay->addWidget(m_tempLbl);

    m_ttftLbl = new QLabel("", hdr);
    m_ttftLbl->setObjectName("ttftLabel");
    m_ttftLbl->setToolTip(tr("TTFT \xe2\x80\x94 Time To First Token dell'ultima risposta AI\n"
                              "Verde < 1s  \xc2\xb7  Arancio 1-3s  \xc2\xb7  Rosso > 3s"));
    m_ttftLbl->setStyleSheet("QLabel#ttftLabel{color:#64748b;font-size:11px;padding:0 4px;}");
    m_ttftLbl->setVisible(false);
    lay->addWidget(m_ttftLbl);
}

/* ── Livello 2: 🚨 emergenza RAM + Scarica LLM + backend toggle ───── */
void MainWindow::buildActionButtons(QHBoxLayout* lay)
{
    auto* hdr = qobject_cast<QWidget*>(lay->parent());

    /* 🚨 Emergenza RAM */
    m_emergencyBtn = new QPushButton("🚨", hdr);
    m_emergencyBtn->setObjectName("emergencyBtn");
    m_emergencyBtn->setToolTip(
        "EMERGENZA RAM\n"
        "1. Scarica modello corrente dalla RAM (keep_alive=0)\n"
        "2. Ferma tutti i modelli Ollama\n"
        "3. Libera cache kernel (richiede password admin)\n"
        "Diventa giallo se RAM > 40%, rosso se > 75%.");
    m_emergencyBtn->setFixedSize(dpiSize(42, 36));
    connect(m_emergencyBtn, &QPushButton::clicked, this, &MainWindow::onEmergencyRamClicked);
    lay->addWidget(m_emergencyBtn);

    /* 🦙 Backend toggle — reparentato in buildContent come corner widget */
    m_btnBackend = new QPushButton(tr("\xf0\x9f\xa6\x99  Ollama"), hdr);
    m_btnBackend->setObjectName("backendBtn");
    m_btnBackend->setToolTip(tr("Cambia backend AI — un click per passare da Ollama a llama-server e viceversa"));
    m_btnBackend->setAccessibleName(tr("Backend AI attivo"));
    m_btnBackend->setAccessibleDescription("Seleziona il backend: Ollama o llama-server");
    m_btnBackend->setFixedHeight(dpiScale(36));
    m_btnBackend->setMinimumWidth(dpiScale(130));
    connect(m_btnBackend, &QPushButton::clicked, this, &MainWindow::onBackendBtnClicked);
    m_btnBackend->setParent(this);  /* reparent temporaneo — buildContent lo riposiziona */
}


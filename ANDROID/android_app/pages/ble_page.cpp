#include "ble_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QClipboard>
#include <QGuiApplication>
#include <QMessageBox>

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
BlePage::BlePage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("BlePage");

    /* ── Header ── */
    m_statusLbl = new QLabel("Premi Scansiona per cercare dispositivi", this);
    m_statusLbl->setObjectName("StatusLabel");
    m_statusLbl->setWordWrap(true);

    m_countLbl = new QLabel("", this);
    m_countLbl->setObjectName("CountLabel");

    auto* headerRow = new QHBoxLayout;
    headerRow->addWidget(m_statusLbl, 1);
    headerRow->addWidget(m_countLbl);

    /* ── Lista dispositivi ── */
    m_list = new QListWidget(this);
    m_list->setObjectName("BleList");
    m_list->setAlternatingRowColors(true);
    m_list->setIconSize(QSize(16, 16));

    /* ── Pulsante scan ── */
    m_scanBtn = new QPushButton(
        "\xf0\x9f\x94\x8b  Scansiona BLE", this);  // 🔋
    m_scanBtn->setObjectName("PrimaryBtn");

    /* ── Layout ── */
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);
    vbox->addLayout(headerRow);
    vbox->addWidget(m_list, 1);
    vbox->addWidget(m_scanBtn);

    /* ── BLE Discovery Agent ── */
    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(8000);  // 8 secondi

    connect(m_scanBtn, &QPushButton::clicked, this, &BlePage::onStartScan);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BlePage::onDeviceDiscovered);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BlePage::onScanFinished);
    connect(m_agent,
            QOverload<QBluetoothDeviceDiscoveryAgent::Error>::of(
                &QBluetoothDeviceDiscoveryAgent::errorOccurred),
            this, &BlePage::onScanError);
    connect(m_list, &QListWidget::itemClicked,
            this, &BlePage::onDeviceTapped);
}

BlePage::~BlePage()
{
    stopScan();
}

/* ── stopScan ────────────────────────────────────────────────── */
void BlePage::stopScan()
{
    if (m_agent && m_agent->isActive()) {
        m_agent->stop();
        m_scanBtn->setText("\xf0\x9f\x94\x8b  Scansiona BLE");
        m_scanBtn->setEnabled(true);
    }
}

/* ── onStartScan ─────────────────────────────────────────────── */
void BlePage::onStartScan()
{
    if (m_agent->isActive()) { stopScan(); return; }

    m_list->clear();
    m_deviceIndex.clear();
    m_statusLbl->setText("\xf0\x9f\x94\x8d  Ricerca in corso…");  // 🔍
    m_countLbl->setText("");
    m_scanBtn->setText("\xe2\x9c\x95  Ferma");  // ✕
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

/* ── rssiIcon — emoji in base alla potenza del segnale ─────── */
QString BlePage::rssiIcon(int rssi) const
{
    if (rssi >= -60) return "\xf0\x9f\x9f\xa2";   // 🟢 vicino
    if (rssi >= -80) return "\xf0\x9f\x9f\xa1";   // 🟡 medio
    return             "\xf0\x9f\x94\xb4";          // 🔴 lontano
}

/* ── addOrUpdateDevice ───────────────────────────────────────── */
void BlePage::addOrUpdateDevice(const QBluetoothDeviceInfo& info)
{
    const QString mac  = info.address().toString();
    const QString name = info.name().isEmpty() ? "(senza nome)" : info.name();
    const int     rssi = info.rssi();
    const QString text = QString("%1  %2\n    %3  %4 dBm")
                         .arg(rssiIcon(rssi), name, mac, QString::number(rssi));

    auto it = m_deviceIndex.find(mac);
    if (it != m_deviceIndex.end()) {
        /* Aggiorna riga esistente */
        if (auto* item = m_list->item(it.value()))
            item->setText(text);
    } else {
        auto* item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, mac);
        item->setData(Qt::UserRole + 1, name);
        m_deviceIndex[mac] = m_list->count() - 1;
    }
}

/* ── onDeviceDiscovered ──────────────────────────────────────── */
void BlePage::onDeviceDiscovered(const QBluetoothDeviceInfo& info)
{
    addOrUpdateDevice(info);
    m_countLbl->setText(QString("%1 trovati").arg(m_list->count()));
}

/* ── onScanFinished ──────────────────────────────────────────── */
void BlePage::onScanFinished()
{
    m_statusLbl->setText(
        m_list->count() == 0
            ? "\xe2\x9a\xa0  Nessun dispositivo trovato"
            : QString("\xe2\x9c\x85  Scansione completata — %1 dispositivi")
              .arg(m_list->count()));
    m_scanBtn->setText("\xf0\x9f\x94\x8b  Scansiona BLE");
    m_scanBtn->setEnabled(true);
}

/* ── onScanError ─────────────────────────────────────────────── */
void BlePage::onScanError(QBluetoothDeviceDiscoveryAgent::Error err)
{
    Q_UNUSED(err)
    m_statusLbl->setText(
        "\xe2\x9d\x8c  Errore BLE: " + m_agent->errorString() +
        "\n\xf0\x9f\x92\xa1  Verifica che il Bluetooth sia attivo "
        "e i permessi siano concessi.");
    m_scanBtn->setText("\xf0\x9f\x94\x8b  Scansiona BLE");
    m_scanBtn->setEnabled(true);
}

/* ── onDeviceTapped — mostra dettagli e copia MAC ───────────── */
void BlePage::onDeviceTapped(QListWidgetItem* item)
{
    if (!item) return;
    const QString mac  = item->data(Qt::UserRole).toString();
    const QString name = item->data(Qt::UserRole + 1).toString();
    QGuiApplication::clipboard()->setText(mac);
    QMessageBox::information(this, "Dispositivo BLE",
        QString("Nome: %1\nMAC: %2\n\n(Indirizzo copiato negli appunti)")
        .arg(name, mac));
}

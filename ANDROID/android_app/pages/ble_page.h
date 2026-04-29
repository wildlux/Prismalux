#pragma once
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>

/* ══════════════════════════════════════════════════════════════
   BlePage — scanner Bluetooth LE.

   Funzionalità:
   - Scansiona dispositivi BLE nelle vicinanze
   - Mostra nome, indirizzo MAC e RSSI (segnale)
   - Tap su un dispositivo → dettagli e copia indirizzo
   - Badge colorato in base alla potenza del segnale:
       Verde  ≥ -60 dBm (vicino)
       Giallo -60…-80 dBm (medio)
       Rosso  < -80 dBm (lontano)
   ══════════════════════════════════════════════════════════════ */
class BlePage : public QWidget {
    Q_OBJECT
public:
    explicit BlePage(QWidget* parent = nullptr);
    ~BlePage() override;

    void stopScan();

private slots:
    void onStartScan();
    void onDeviceDiscovered(const QBluetoothDeviceInfo& info);
    void onScanFinished();
    void onScanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void onDeviceTapped(QListWidgetItem* item);

private:
    QString rssiIcon(int rssi) const;
    void    addOrUpdateDevice(const QBluetoothDeviceInfo& info);

    QBluetoothDeviceDiscoveryAgent* m_agent    = nullptr;
    QListWidget*   m_list      = nullptr;
    QPushButton*   m_scanBtn   = nullptr;
    QLabel*        m_statusLbl = nullptr;
    QLabel*        m_countLbl  = nullptr;

    /* mac → row index per aggiornamento in-place */
    QMap<QString, int> m_deviceIndex;
};

#include "thermal_monitor.h"
#include <QDir>
#include <QFile>

ThermalMonitor::ThermalMonitor(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &ThermalMonitor::onPoll);
}

void ThermalMonitor::start(int intervalMs)
{
    m_timer->setInterval(intervalMs);
    m_timer->start();
}

void ThermalMonitor::stop()
{
    m_timer->stop();
    m_paused = false;
}

void ThermalMonitor::onPoll()
{
    const float t = readMaxZone();
    if (t <= 0.0f) return;

    m_lastTemp = t;
    emit tempUpdated(t);

    if (!m_paused && t >= kPauseTemp) {
        m_paused = true;
        emit pauseRequired(t);
    } else if (m_paused && t < kWarnTemp) {
        m_paused = false;
        emit resumeAllowed();
    }
}

/* Scansiona tutte le zone termiche e restituisce la temperatura massima.
   I file espongono valori in milligradi (es. 45000 → 45.0 °C). */
float ThermalMonitor::readMaxZone()
{
    const QDir dir("/sys/class/thermal");
    if (!dir.exists()) return 0.0f;

    float maxT = 0.0f;
    for (const QString& entry : dir.entryList({"thermal_zone*"}, QDir::Dirs)) {
        QFile f("/sys/class/thermal/" + entry + "/temp");
        if (!f.open(QIODevice::ReadOnly)) continue;
        const float t = f.readAll().trimmed().toFloat() / 1000.0f;
        if (t > maxT && t > 0.0f && t < 120.0f)
            maxT = t;
    }
    return maxT;
}

#pragma once
#include <QObject>
#include <QTimer>

/*
   ThermalMonitor -- legge la temperatura CPU da
   /sys/class/thermal/thermal_zone[N]/temp ogni 3 secondi.

   Soglie:
     kWarnTemp  65 C -- avviso visivo
     kPauseTemp 72 C -- pausa inferenza (almeno 5 s)
     kCritTemp  78 C -- pausa lunga finche non scende

   La pausa minima di 5 s e gestita da LocalLlmClient.
*/
class ThermalMonitor : public QObject {
    Q_OBJECT
public:
    explicit ThermalMonitor(QObject* parent = nullptr);

    float currentTemp() const { return m_lastTemp; }
    bool  isPaused()    const { return m_paused; }

    static constexpr float kWarnTemp  = 65.0f;
    static constexpr float kPauseTemp = 72.0f;
    static constexpr float kCritTemp  = 78.0f;

public slots:
    void start(int intervalMs = 3000);
    void stop();

signals:
    void tempUpdated(float celsius);
    void pauseRequired(float celsius);
    void resumeAllowed();

private slots:
    void onPoll();

private:
    static float readMaxZone();

    QTimer* m_timer    = nullptr;
    float   m_lastTemp = 0.0f;
    bool    m_paused   = false;
};

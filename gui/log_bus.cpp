#include "log_bus.h"

LogBus::LogBus(QObject* parent) : QObject(parent) {}

LogBus* LogBus::instance()
{
    static LogBus inst;
    return &inst;
}

void LogBus::post(const QString& msg, const QString& category)
{
    emit instance()->event(msg, category);
}

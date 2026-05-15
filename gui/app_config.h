#pragma once
#include <QSettings>
#include <QVariant>
#include <QString>

/* ══════════════════════════════════════════════════════════════
   AppConfig — accesso centralizzato a QSettings("Prismalux","GUI")

   Usa un singleton Meyers con una singola istanza QSettings persistente
   per tutta la durata dell'applicazione. Qt memorizza i valori sul disco
   al primo setValue() o alla distruzione; le letture successive vanno
   dalla cache interna senza I/O.

   Uso:
     #include "app_config.h"
     AppConfig::s().value(P::SK::kTheme, "dark_ocean")
     AppConfig::s().setValue(P::SK::kTheme, id)
   ══════════════════════════════════════════════════════════════ */
class AppConfig {
public:
    static AppConfig& s() {
        static AppConfig instance;
        return instance;
    }

    QVariant value(const QString& key, const QVariant& def = {}) const {
        return m_settings.value(key, def);
    }

    void setValue(const QString& key, const QVariant& val) {
        m_settings.setValue(key, val);
    }

    bool contains(const QString& key) const {
        return m_settings.contains(key);
    }

    void remove(const QString& key) {
        m_settings.remove(key);
    }

    void sync() {
        m_settings.sync();
    }

private:
    AppConfig() : m_settings("Prismalux", "GUI") {}
    ~AppConfig() = default;
    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;

    mutable QSettings m_settings;
};

#include "theme_manager.h"
#include <QApplication>
#include <QFile>
#include <QSettings>

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager* ThemeManager::instance() {
    if (!s_instance)
        s_instance = new ThemeManager(qApp);
    return s_instance;
}

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
    m_themes = {
        { "dark_cyan",   "Dark Cyan (default)", ":/themes/dark_cyan.qss"   },
        { "dark_amber",  "Dark Amber",          ":/themes/dark_amber.qss"  },
        { "dark_purple", "Dark Purple",         ":/themes/dark_purple.qss" },
        { "light",       "Light",               ":/themes/light.qss"       },
    };
}

void ThemeManager::apply(const QString& id) {
    /* Cerca il tema nella lista */
    QString resource;
    for (const auto& t : m_themes) {
        if (t.id == id) { resource = t.resource; break; }
    }
    if (resource.isEmpty()) return;  /* id sconosciuto — ignora */

    QFile f(resource);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
    m_currentId = id;

    QSettings s("Prismalux", "GUI");
    s.setValue("theme", id);

    emit changed(id);
}

void ThemeManager::loadSaved() {
    QSettings s("Prismalux", "GUI");
    const QString saved = s.value("theme", "dark_cyan").toString();
    apply(saved);
}

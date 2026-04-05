#include "theme_manager.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager* ThemeManager::instance() {
    /* Variabile static locale: inizializzazione garantita thread-safe da C++11 */
    static ThemeManager inst(qApp);
    s_instance = &inst;
    return s_instance;
}

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
    m_themes = {
        { "dark_cyan",     "Dark Cyan (default)", ":/themes/dark_cyan.qss"     },
        { "dark_amber",    "Dark Amber",          ":/themes/dark_amber.qss"    },
        { "dark_purple",   "Dark Purple",         ":/themes/dark_purple.qss"   },
        { "light",         "Light",               ":/themes/light.qss"         },
        { "dark_green",    "\xf0\x9f\x8c\xbf Natura (Verde)",        ":/themes/dark_green.qss"    },
        { "dark_sunset",   "\xf0\x9f\x8c\x85 Sunset (Arancione)",    ":/themes/dark_sunset.qss"   },
        { "dark_ocean",    "\xf0\x9f\x8c\x8a Oceano (Azzurro)",      ":/themes/dark_ocean.qss"    },
        { "dark_lavender", "\xf0\x9f\x92\x9c Lavanda (Viola)",       ":/themes/dark_lavender.qss" },
        { "dark_rainbow",  "\xf0\x9f\x8c\x88 Arcobaleno",            ":/themes/dark_rainbow.qss"  },
        { "dark_classic",  "\xf0\x9f\x94\xb5 Classico (Blu)",        ":/themes/dark_classic.qss"  },
        { "neon",          "\xf0\x9f\x92\xa1 Neon (Verde/Blu)",       ":/themes/neon.qss"          },
        { "hacker",        "\xf0\x9f\x96\xa5 Hacker (Matrix)",        ":/themes/hacker.qss"        },
        { "solar",         "\xe2\x98\x80 Solarized Dark",             ":/themes/solar.qss"         },
        { "pink",          "\xf0\x9f\x8c\xb8 Pink (Magenta)",         ":/themes/pink.qss"          },
        { "military",      "\xf0\x9f\x8e\x96 Military (Oliva)",       ":/themes/military.qss"      },
        { "venom_green",   "\xe2\x9a\xa1 Venom Green",                ":/themes/venom_green.qss"   },
        { "venom_orange",  "\xe2\x9a\xa1 Venom Orange",               ":/themes/venom_orange.qss"  },
        { "venom_blue",    "\xe2\x9a\xa1 Venom Blue",                 ":/themes/venom_blue.qss"    },
        { "venom_red",     "\xe2\x9a\xa1 Venom Red",                  ":/themes/venom_red.qss"     },
        { "light_mint",    "\xf0\x9f\x8c\xbf Chiaro Menta",           ":/themes/light_mint.qss"    },
        { "light_rose",    "\xf0\x9f\x8c\xb9 Chiaro Rosa",            ":/themes/light_rose.qss"    },
        { "light_sand",    "\xe2\x98\x80 Chiaro Sabbia",               ":/themes/light_sand.qss"    },
        { "light_sky",     "\xf0\x9f\x8c\x8a Chiaro Cielo",           ":/themes/light_sky.qss"     },
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
    scanExternalThemes();   /* carica prima i temi custom dalla cartella */
    QSettings s("Prismalux", "GUI");
    const QString saved = s.value("theme", "dark_cyan").toString();
    apply(saved);
}

void ThemeManager::scanExternalThemes() {
    const QString dir = QCoreApplication::applicationDirPath() + "/themes";
    QStringList existing;
    for (const auto& t : std::as_const(m_themes)) existing << t.id;

    const QStringList filters{ "*.qss", "*.QSS" };
    for (const QFileInfo& fi : QDir(dir).entryInfoList(filters, QDir::Files)) {
        const QString id = fi.baseName();
        if (existing.contains(id)) continue;   /* non duplicare i built-in */
        /* Label = nome file con prima lettera maiuscola e underscore → spazio */
        QString label = id;
        label.replace('_', ' ');
        if (!label.isEmpty()) label[0] = label[0].toUpper();
        m_themes.append({ id, label + " (custom)", fi.absoluteFilePath() });
    }
}

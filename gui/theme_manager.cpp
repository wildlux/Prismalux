#include "theme_manager.h"
#include "prismalux_paths.h"
#include <QApplication>
namespace P = PrismaluxPaths;
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QWidget>

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager* ThemeManager::instance() {
    /* Variabile static locale: inizializzazione garantita thread-safe da C++11 */
    static ThemeManager inst(qApp);
    s_instance = &inst;
    return s_instance;
}

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
    /* Percorso della cartella temi accanto all'eseguibile — nessun QRC,
     * i file .qss vengono letti direttamente da disco a runtime.
     * Vantaggio: modificabili senza ricompilare; binario più leggero. */
    const QString d = QCoreApplication::applicationDirPath() + "/themes/";
    m_themes = {
        { "dark_cyan",     "Dark Cyan (default)",               d + "dark_cyan.qss"     },
        { "dark_amber",    "Dark Amber",                        d + "dark_amber.qss"    },
        { "dark_purple",   "Dark Purple",                       d + "dark_purple.qss"   },
        { "light",         "Light",                             d + "light.qss"         },
        { "dark_green",    "\xf0\x9f\x8c\xbf Natura (Verde)",   d + "dark_green.qss"    },
        { "dark_sunset",   "\xf0\x9f\x8c\x85 Sunset (Arancione)", d + "dark_sunset.qss" },
        { P::SK::kDefaultTheme,    "\xf0\x9f\x8c\x8a Oceano (Azzurro)", d + "dark_ocean.qss"    },
        { "dark_lavender", "\xf0\x9f\x92\x9c Lavanda (Viola)",  d + "dark_lavender.qss" },
        { "dark_rainbow",  "\xf0\x9f\x8c\x88 Arcobaleno",       d + "dark_rainbow.qss"  },
        { "dark_classic",  "\xf0\x9f\x94\xb5 Classico (Blu)",   d + "dark_classic.qss"  },
        { "neon",          "\xf0\x9f\x92\xa1 Neon (Verde/Blu)",  d + "neon.qss"          },
        { "hacker",        "\xf0\x9f\x96\xa5 Hacker (Matrix)",  d + "hacker.qss"        },
        { "solar",         "\xe2\x98\x80 Solarized Dark",        d + "solar.qss"         },
        { "pink",          "\xf0\x9f\x8c\xb8 Pink (Magenta)",   d + "pink.qss"          },
        { "military",      "\xf0\x9f\x8e\x96 Military (Oliva)", d + "military.qss"      },
        { "venom_green",   "\xe2\x9a\xa1 Venom Green",           d + "venom_green.qss"   },
        { "venom_orange",  "\xe2\x9a\xa1 Venom Orange",          d + "venom_orange.qss"  },
        { "venom_blue",    "\xe2\x9a\xa1 Venom Blue",            d + "venom_blue.qss"    },
        { "venom_red",     "\xe2\x9a\xa1 Venom Red",             d + "venom_red.qss"     },
        { "light_mint",    "\xf0\x9f\x8c\xbf Chiaro Menta",     d + "light_mint.qss"    },
        { "light_rose",    "\xf0\x9f\x8c\xb9 Chiaro Rosa",      d + "light_rose.qss"    },
        { "light_sand",    "\xe2\x98\x80 Chiaro Sabbia",         d + "light_sand.qss"    },
        { "light_sky",     "\xf0\x9f\x8c\x8a Chiaro Cielo",     d + "light_sky.qss"     },
    };
}

void ThemeManager::apply(const QString& id) {
    /* Cerca il tema nella lista */
    QString resource;
    for (const auto& t : m_themes) {
        if (t.id == id) { resource = t.resource; break; }
    }
    if (resource.isEmpty()) return;

    /* ── CSS cache: legge da disco solo la prima volta ── */
    if (!m_cssCache.contains(id)) {
        QFile f(resource);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        QString css = QString::fromUtf8(f.readAll());

        /* Appende base.qss (struttura scrollbar, ecc.) */
        QFile base(QCoreApplication::applicationDirPath() + "/themes/base.qss");
        if (base.open(QIODevice::ReadOnly | QIODevice::Text))
            css += "\n" + QString::fromUtf8(base.readAll());

        m_cssCache[id] = css;
    }

    const QString& css = m_cssCache.value(id);

    /* ── Applica stylesheet ottimizzato ──────────────────────────
       setUpdatesEnabled(false) sul top-level sopprime i repaint
       intermedi durante il traversal del widget tree: Qt calcola
       tutti gli stili ma accoda i repaint; setUpdatesEnabled(true)
       li scarica in un unico frame invece di N frame parziali.
       WaitCursor dà feedback visivo immediato al click. */
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QWidget* root = nullptr;
    for (QWidget* w : qApp->topLevelWidgets()) {
        if (w->isVisible()) { root = w; break; }
    }
    if (root) root->setUpdatesEnabled(false);

    qApp->setStyleSheet(css);

    if (root) {
        root->setUpdatesEnabled(true);
        root->update();
    }

    QApplication::restoreOverrideCursor();

    m_currentId = id;
    QSettings s("Prismalux", "GUI");
    s.setValue(P::SK::kTheme, id);
    emit changed(id);
}

void ThemeManager::loadSaved() {
    scanExternalThemes();   /* carica prima i temi custom dalla cartella */
    QSettings s("Prismalux", "GUI");
    const QString saved = s.value(P::SK::kTheme, P::SK::kDefaultTheme).toString();
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

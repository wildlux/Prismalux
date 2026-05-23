#include "theme_manager.h"
#include "prismalux_paths.h"
#include <QApplication>
#include <QRegularExpression>
#include <QScreen>
namespace P = PrismaluxPaths;
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QWidget>

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager* ThemeManager::instance() {
    /* Variabile static locale: inizializzazione garantita thread-safe da C++11 */
    static ThemeManager inst(nullptr);
    s_instance = &inst;
    return s_instance;
}

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
    /* Percorso della cartella temi accanto all'eseguibile — nessun QRC,
     * i file .qss vengono letti direttamente da disco a runtime.
     * Vantaggio: modificabili senza ricompilare; binario più leggero. */
    const QString d = QCoreApplication::applicationDirPath() + "/themes/";

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    /* Collega il segnale colorSchemeChanged di QStyleHints allo slot.
     * QGuiApplication::styleHints() è disponibile da Qt 5.5, ma
     * colorSchemeChanged è aggiunto in Qt 6.5. */
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, &ThemeManager::onColorSchemeChanged);
#endif
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

    /* ── Cache grezzo: legge da disco una sola volta, senza scaling ── */
    if (!m_rawCache.contains(id)) {
        QFile f(resource);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        QString raw = QString::fromUtf8(f.readAll());

        /* Appende base.qss dopo il tema (regole strutturali vincono) */
        QFile base(QCoreApplication::applicationDirPath() + "/themes/base.qss");
        if (base.open(QIODevice::ReadOnly | QIODevice::Text))
            raw += "\n" + QString::fromUtf8(base.readAll());

        m_rawCache[id] = raw;
    }

    /* ── Cache scalata: chiave "id@zoomPct" → ricalcolata solo se nuovo livello ── */
    const int zoomInt = qRound(m_zoomScale * 100);
    const QString cacheKey = id + "@" + QString::number(zoomInt);
    if (!m_cssCache.contains(cacheKey)) {
        QString css = m_rawCache[id];

        /* Fattore combinato: DPI adattivo × zoom utente */
        const qreal dpi = QGuiApplication::primaryScreen()
                          ? QGuiApplication::primaryScreen()->logicalDotsPerInch()
                          : 96.0;
        const qreal dpiScale  = (dpi > 108.0) ? (dpi / 96.0) : 1.0;
        const qreal totalScale = dpiScale * m_zoomScale;

        if (qAbs(totalScale - 1.0) > 0.005) {
            static const QRegularExpression reFontPx(
                R"(font-size\s*:\s*(\d+)\s*px)",
                QRegularExpression::CaseInsensitiveOption);
            QString patched;
            patched.reserve(css.size());
            int lastEnd = 0;
            auto it = reFontPx.globalMatch(css);
            while (it.hasNext()) {
                const auto m = it.next();
                patched += css.mid(lastEnd, m.capturedStart() - lastEnd);
                patched += QString("font-size:%1px")
                               .arg(qMax(1, qRound(m.captured(1).toInt() * totalScale)));
                lastEnd = m.capturedEnd();
            }
            patched += css.mid(lastEnd);
            css = std::move(patched);
        }

        m_cssCache[cacheKey] = css;
    }

    /* ── Applica stylesheet ottimizzato ── */
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QWidget* root = nullptr;
    for (QWidget* w : qApp->topLevelWidgets()) {
        if (w->isVisible()) { root = w; break; }
    }
    if (root) root->setUpdatesEnabled(false);

    qApp->setStyleSheet(m_cssCache.value(cacheKey));

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

/* ══════════════════════════════════════════════════════════════
   setZoomScale — imposta il fattore zoom UI (0.5=50%, 2.0=200%).
   Invalida la cache scalata e riapplica il tema corrente.
   ══════════════════════════════════════════════════════════════ */
void ThemeManager::setZoomScale(double scale) {
    m_zoomScale = qBound(0.5, scale, 2.0);
    /* Non riapplica subito: la cache per-zoom gestisce la freschezza.
     * Chiama reapply() (o apply()) per rendere effettivo il cambio. */
}

void ThemeManager::reapply() {
    if (!m_currentId.isEmpty())
        apply(m_currentId);
}

void ThemeManager::loadSaved() {
    scanExternalThemes();   /* carica prima i temi custom dalla cartella */
    QSettings s("Prismalux", "GUI");
    m_followSystem = s.value(P::SK::kFollowSystem, false).toBool();
    if (m_followSystem) {
        applySystemTheme();
    } else {
        const QString saved = s.value(P::SK::kTheme, P::SK::kDefaultTheme).toString();
        apply(saved);
    }
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

/* ══════════════════════════════════════════════════════════════
   applySystemTheme — rileva dark/light dal sistema operativo e
   applica il tema corrispondente.
   Usa "dark_cyan" per dark e "light" per light (fallback).
   Guard Qt 6.5 per colorScheme API.
   ══════════════════════════════════════════════════════════════ */
void ThemeManager::applySystemTheme() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark)
        apply("dark_cyan");
    else
        apply("light");
#endif
    /* Qt < 6.5: no-op — colorScheme non disponibile */
}

/* ══════════════════════════════════════════════════════════════
   setFollowSystem — attiva/disattiva il follow del tema di
   sistema. Salva la preferenza in QSettings e applica subito
   il tema di sistema se follow=true.
   ══════════════════════════════════════════════════════════════ */
void ThemeManager::setFollowSystem(bool follow) {
    m_followSystem = follow;
    QSettings s("Prismalux", "GUI");
    s.setValue(P::SK::kFollowSystem, follow);
    if (follow)
        applySystemTheme();
}

/* ══════════════════════════════════════════════════════════════
   onColorSchemeChanged — slot collegato a
   QStyleHints::colorSchemeChanged (Qt >= 6.5).
   Aggiorna il tema solo se m_followSystem è attivo.
   ══════════════════════════════════════════════════════════════ */
void ThemeManager::onColorSchemeChanged() {
    if (m_followSystem)
        applySystemTheme();
}

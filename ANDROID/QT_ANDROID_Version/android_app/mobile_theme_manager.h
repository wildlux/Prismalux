#pragma once
#include <QApplication>
#include <QColor>
#include <QFile>
#include <QList>
#include <QPalette>
#include <QSettings>
#include <QString>

struct MobileThemeInfo { QString id; QString name; QColor accent; QColor bg; };

/* Singleton leggero — nessun segnale Qt, non eredita da QObject. */
class MobileThemeManager {
public:
    static MobileThemeManager& instance() {
        static MobileThemeManager s;
        return s;
    }

    static QList<MobileThemeInfo> themes() {
        /* accent = colore primario del tema; bg = sfondo base del tema */
        return {
            { "dark_classic", "Classic (predefinito)", QColor(0x00,0xc8,0xff), QColor(0x0f,0x11,0x17) },
            { "dark_cyan",    "\xf0\x9f\x94\xb5  Cyan",      QColor(0x00,0xb8,0xd9), QColor(0x0a,0x12,0x1e) },
            { "neon",         "\xf0\x9f\x9f\xa2  Neon",      QColor(0x00,0xff,0x9d), QColor(0x0a,0x0a,0x0a) },
            { "hacker",       "\xf0\x9f\x9f\xa2  Hacker",    QColor(0x39,0xff,0x14), QColor(0x07,0x0e,0x07) },
            { "dark_purple",  "\xf0\x9f\x9f\xa3  Viola",     QColor(0x8b,0x5c,0xf6), QColor(0x13,0x11,0x1f) },
            { "dark_sunset",  "\xf0\x9f\x9f\xa0  Tramonto",  QColor(0xf9,0x73,0x16), QColor(0x1a,0x0f,0x07) },
            { "dark_ocean",   "\xf0\x9f\x94\xb5  Oceano",    QColor(0x02,0x84,0xc7), QColor(0x07,0x10,0x1e) },
            { "venom_red",    "\xf0\x9f\x94\xb4  Rosso",     QColor(0xef,0x44,0x44), QColor(0x1a,0x0a,0x0a) },
            { "dark_amber",   "\xf0\x9f\x9f\xa1  Ambra",     QColor(0xf5,0x9e,0x0b), QColor(0x1a,0x14,0x07) },
        };
    }

    /* Carica base QSS + eventuale override e applica.
       Aggiorna anche la QPalette per l'accent color del tema:
       questo evita artefatti (es. scrollbar ciano sul tema Viola)
       e il flash "light" al momento del cambio. Salva in QSettings. */
    void apply(const QString& themeId) {
        QString sheet;
        QFile base(":/style_mobile.qss");
        if (!base.open(QIODevice::ReadOnly)) {
            base.setFileName("style_mobile.qss");
            (void)base.open(QIODevice::ReadOnly);
        }
        if (base.isOpen()) sheet = QString::fromUtf8(base.readAll());

        if (themeId != "dark_classic") {
            QFile ov(":/themes_mobile/" + themeId + ".qss");
            if (ov.open(QIODevice::ReadOnly))
                sheet += "\n" + QString::fromUtf8(ov.readAll());
        }

        auto* a = qobject_cast<QApplication*>(qApp);
        if (a) {
            /* Aggiorna la QPalette accent prima del setStyleSheet per
               minimizzare il numero di frame con colori "sbagliati". */
            const auto tlist = themes();
            MobileThemeInfo info = tlist.first();
            for (const auto& t : tlist) {
                if (t.id == themeId) { info = t; break; }
            }

            QPalette p = a->palette();
            p.setColor(QPalette::Active,   QPalette::Window,    info.bg);
            p.setColor(QPalette::Inactive, QPalette::Window,    info.bg);
            p.setColor(QPalette::Disabled, QPalette::Window,    info.bg);
            p.setColor(QPalette::Active,   QPalette::Base,      info.bg.lighter(115));
            p.setColor(QPalette::Inactive, QPalette::Base,      info.bg.lighter(115));
            p.setColor(QPalette::Disabled, QPalette::Base,      info.bg);
            p.setColor(QPalette::Active,   QPalette::Highlight, info.accent);
            p.setColor(QPalette::Inactive, QPalette::Highlight, info.accent.darker(150));
            p.setColor(QPalette::Active,   QPalette::Link,      info.accent);
            a->setPalette(p);

            a->setStyleSheet(sheet);
        }

        m_current = themeId;
        QSettings s("Prismalux", "Mobile");
        s.setValue("ui/theme", themeId);
    }

    /* Carica il tema salvato (o classic se assente). */
    void loadSaved() {
        QSettings s("Prismalux", "Mobile");
        apply(s.value("ui/theme", "dark_classic").toString());
    }

    QString current() const { return m_current; }

private:
    MobileThemeManager() = default;
    QString m_current = "dark_classic";
};

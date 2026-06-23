#include <QApplication>
#include <QFile>
#include <QPalette>
#include "mainwindow.h"
#include "mobile_theme_manager.h"

int main(int argc, char* argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("PrismaluxMobile");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Prismalux");

#ifdef Q_OS_ANDROID
    /* Forza stile Fusion + palette dark completa (tutti gli stati):
       impedisce che il renderer Android inietti colori "chiari" nei
       widget non coperti dal QSS — inclusi popup combo, placeholder,
       widget disabilitati, finestra in background.
       Deve essere impostato PRIMA del setStyleSheet(). */
    app.setStyle("fusion");
    QPalette dark;

    /* ── Active (stato normale) ── */
    dark.setColor(QPalette::Active, QPalette::Window,          QColor(0x0f, 0x11, 0x17));
    dark.setColor(QPalette::Active, QPalette::WindowText,      QColor(0xe8, 0xea, 0xf0));
    dark.setColor(QPalette::Active, QPalette::Base,            QColor(0x12, 0x15, 0x1f));
    dark.setColor(QPalette::Active, QPalette::AlternateBase,   QColor(0x1a, 0x1d, 0x27));
    dark.setColor(QPalette::Active, QPalette::Text,            QColor(0xe8, 0xea, 0xf0));
    dark.setColor(QPalette::Active, QPalette::Button,          QColor(0x1a, 0x1d, 0x27));
    dark.setColor(QPalette::Active, QPalette::ButtonText,      QColor(0xe8, 0xea, 0xf0));
    dark.setColor(QPalette::Active, QPalette::Highlight,       QColor(0x00, 0xc8, 0xff));
    dark.setColor(QPalette::Active, QPalette::HighlightedText, QColor(0x0f, 0x11, 0x17));
    dark.setColor(QPalette::Active, QPalette::PlaceholderText, QColor(0x88, 0x90, 0xa8));
    dark.setColor(QPalette::Active, QPalette::ToolTipBase,     QColor(0x1a, 0x1d, 0x27));
    dark.setColor(QPalette::Active, QPalette::ToolTipText,     QColor(0xe8, 0xea, 0xf0));
    dark.setColor(QPalette::Active, QPalette::Link,            QColor(0x00, 0xc8, 0xff));
    dark.setColor(QPalette::Active, QPalette::BrightText,      QColor(0xff, 0xff, 0xff));
    dark.setColor(QPalette::Active, QPalette::Mid,             QColor(0x22, 0x26, 0x35));
    dark.setColor(QPalette::Active, QPalette::Midlight,        QColor(0x28, 0x2c, 0x3e));
    dark.setColor(QPalette::Active, QPalette::Dark,            QColor(0x0a, 0x0c, 0x12));
    dark.setColor(QPalette::Active, QPalette::Shadow,          QColor(0x00, 0x00, 0x00));

    /* ── Inactive (finestra in background / non in focus) ──
       Stessi colori di Active: evita il flash "light" quando
       l'app torna in foreground da un'altra app. */
    dark.setColor(QPalette::Inactive, QPalette::Window,          QColor(0x0f, 0x11, 0x17));
    dark.setColor(QPalette::Inactive, QPalette::WindowText,      QColor(0xe8, 0xea, 0xf0));
    dark.setColor(QPalette::Inactive, QPalette::Base,            QColor(0x12, 0x15, 0x1f));
    dark.setColor(QPalette::Inactive, QPalette::AlternateBase,   QColor(0x1a, 0x1d, 0x27));
    dark.setColor(QPalette::Inactive, QPalette::Text,            QColor(0xe8, 0xea, 0xf0));
    dark.setColor(QPalette::Inactive, QPalette::Button,          QColor(0x1a, 0x1d, 0x27));
    dark.setColor(QPalette::Inactive, QPalette::ButtonText,      QColor(0xe8, 0xea, 0xf0));
    dark.setColor(QPalette::Inactive, QPalette::Highlight,       QColor(0x00, 0x88, 0xaa));
    dark.setColor(QPalette::Inactive, QPalette::HighlightedText, QColor(0xe8, 0xea, 0xf0));
    dark.setColor(QPalette::Inactive, QPalette::PlaceholderText, QColor(0x88, 0x90, 0xa8));
    dark.setColor(QPalette::Inactive, QPalette::Mid,             QColor(0x22, 0x26, 0x35));
    dark.setColor(QPalette::Inactive, QPalette::Dark,            QColor(0x0a, 0x0c, 0x12));

    /* ── Disabled (widget disabilitati) ──
       Testo grigio su sfondo invariato: evita la resa "light" dei
       widget disabilitati che altrimenti usano i colori di sistema. */
    dark.setColor(QPalette::Disabled, QPalette::Window,          QColor(0x0f, 0x11, 0x17));
    dark.setColor(QPalette::Disabled, QPalette::WindowText,      QColor(0x44, 0x48, 0x60));
    dark.setColor(QPalette::Disabled, QPalette::Base,            QColor(0x12, 0x15, 0x1f));
    dark.setColor(QPalette::Disabled, QPalette::AlternateBase,   QColor(0x1a, 0x1d, 0x27));
    dark.setColor(QPalette::Disabled, QPalette::Text,            QColor(0x44, 0x48, 0x60));
    dark.setColor(QPalette::Disabled, QPalette::Button,          QColor(0x15, 0x18, 0x22));
    dark.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor(0x44, 0x48, 0x60));
    dark.setColor(QPalette::Disabled, QPalette::Highlight,       QColor(0x22, 0x44, 0x55));
    dark.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x44, 0x48, 0x60));
    dark.setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor(0x44, 0x48, 0x60));
    dark.setColor(QPalette::Disabled, QPalette::Mid,             QColor(0x18, 0x1b, 0x26));
    dark.setColor(QPalette::Disabled, QPalette::Dark,            QColor(0x0a, 0x0c, 0x12));

    app.setPalette(dark);
#endif

    /* ── Stylesheet mobile — carica tema salvato (base + eventuale override) ── */
    MobileThemeManager::instance().loadSaved();

    MainWindow w;
    w.show();
    return app.exec();
}

#include <QApplication>
#include <QFile>
#include <QPalette>
#include "mainwindow.h"

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
    /* Forza stile Fusion + palette dark: impedisce che il renderer Android
       inietti colori "chiari" nei widget non coperti dal QSS (popup combo,
       placeholder, decorazioni di sistema Qt).
       Deve essere impostato PRIMA del setStyleSheet(). */
    app.setStyle("fusion");
    QPalette dark;
    dark.setColor(QPalette::Window,          QColor(0x0f, 0x11, 0x17));
    dark.setColor(QPalette::WindowText,      QColor(0xe8, 0xea, 0xf0));
    dark.setColor(QPalette::Base,            QColor(0x12, 0x15, 0x1f));
    dark.setColor(QPalette::AlternateBase,   QColor(0x1a, 0x1d, 0x27));
    dark.setColor(QPalette::Text,            QColor(0xe8, 0xea, 0xf0));
    dark.setColor(QPalette::Button,          QColor(0x1a, 0x1d, 0x27));
    dark.setColor(QPalette::ButtonText,      QColor(0xe8, 0xea, 0xf0));
    dark.setColor(QPalette::Highlight,       QColor(0x00, 0xc8, 0xff));
    dark.setColor(QPalette::HighlightedText, QColor(0x0f, 0x11, 0x17));
    dark.setColor(QPalette::PlaceholderText, QColor(0x88, 0x90, 0xa8));
    dark.setColor(QPalette::ToolTipBase,     QColor(0x1a, 0x1d, 0x27));
    dark.setColor(QPalette::ToolTipText,     QColor(0xe8, 0xea, 0xf0));
    dark.setColor(QPalette::Link,            QColor(0x00, 0xc8, 0xff));
    dark.setColor(QPalette::BrightText,      QColor(0xff, 0xff, 0xff));
    app.setPalette(dark);
#endif

    /* ── Stylesheet mobile (QSS) ──────────────────────────────────
       Carica prima dalla risorsa embedded (funziona su Android e
       desktop); fallback al file accanto all'exe (debug desktop). */
    {
        QFile qss(":/style_mobile.qss");
        if (!qss.open(QIODevice::ReadOnly)) {
            qss.setFileName("style_mobile.qss");
            (void)qss.open(QIODevice::ReadOnly);
        }
        if (qss.isOpen())
            app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    MainWindow w;
    w.show();
    return app.exec();
}

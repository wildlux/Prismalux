#include <QApplication>
#include <QFile>
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

    /* ── Stylesheet mobile (QSS) ── */
    QFile qss("style_mobile.qss");
    if (qss.open(QIODevice::ReadOnly))
        app.setStyleSheet(qss.readAll());

    MainWindow w;
    w.show();
    return app.exec();
}

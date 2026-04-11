#include <QApplication>
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    /* ── Metadata applicazione (usato da QSettings in ThemeManager) ── */
    app.setApplicationName("Prismalux");
    app.setApplicationVersion("2.1");
    app.setOrganizationName("Prismalux");

    /* ── Finestra principale (carica il tema saved via ThemeManager) ── */
    MainWindow w;
    w.show();

    return app.exec();
}

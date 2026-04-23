#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include "mainwindow.h"

/* ── Carica tutti i font .ttf/.otf dalla cartella fonts/ accanto all'exe ──
   Se la cartella non esiste o è vuota, l'app usa i font di sistema.
   Per aggiungere un font: copia il file .ttf in Qt_GUI/fonts/ e riavvia. */
static void loadBundledFonts() {
    const QString fontsDir = QCoreApplication::applicationDirPath() + "/fonts";
    const QStringList filters{ "*.ttf", "*.otf", "*.TTF", "*.OTF" };
    for (const QFileInfo& fi : QDir(fontsDir).entryInfoList(filters, QDir::Files))
        QFontDatabase::addApplicationFont(fi.absoluteFilePath());
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    /* ── Metadata applicazione (usato da QSettings in ThemeManager) ── */
    app.setApplicationName("Prismalux");
    app.setApplicationVersion("2.1");
    app.setOrganizationName("Prismalux");

    /* ── Font professionali dalla cartella fonts/ (facoltativo) ── */
    loadBundledFonts();

    /* ── Finestra principale (carica il tema saved via ThemeManager) ── */
    MainWindow w;
    w.show();

    return app.exec();
}

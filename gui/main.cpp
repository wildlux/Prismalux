#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QSettings>
#include <QCoreApplication>
#include <QTranslator>
#include <QLocale>
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
    app.setApplicationVersion("2.9");
    app.setOrganizationName("Prismalux");

    /* ── Font professionali dalla cartella fonts/ (facoltativo) ── */
    loadBundledFonts();

    /* ── Internazionalizzazione (i18n) ────────────────────────────────────────
     * Carica la traduzione corrispondente alla locale di sistema se disponibile.
     * File .qm generati da: cmake --build build_gui (richiede Qt6::LinguistTools)
     * Per aggiungere una lingua: copia i18n/prismalux_it.ts → prismalux_XX.ts,
     * traduci le stringhe, esegui cmake per rigenerare i .qm.
     * La lingua predefinita è italiano (hardcoded): il traduttore è caricato
     * solo se la locale di sistema è diversa dall'italiano.             */
    static QTranslator s_translator;
    const QLocale locale = QLocale::system();
    const QString lang   = locale.name();          /* es. "en_US", "de_DE" */
    if (!lang.startsWith("it")) {
        const QString qmBase = QCoreApplication::applicationDirPath()
                               + "/i18n/prismalux_";
        /* Prova prima la locale completa (es. prismalux_en_US.qm),
         * poi solo la lingua (es. prismalux_en.qm) */
        if (s_translator.load(qmBase + lang + ".qm")
            || s_translator.load(qmBase + lang.left(2) + ".qm")) {
            app.installTranslator(&s_translator);
        }
    }

    /* ── Variabile di processo: modalità calcolo LLM ──────────────────────
     * Impostata UNA VOLTA qui, prima che qualsiasi componente venga creato.
     * Sovrascrive la configurazione falsa causata dalla race condition tra
     * il costruttore di ManutenzioneePage (senza hw-detect) e updateHWLabel.
     * Tutti i componenti leggono PRISMALUX_COMPUTE_MODE come fonte unica. */
    {
        QSettings s("Prismalux", "GUI");
        const QString cm = s.value("ai/computeMode", "").toString();
        if (!cm.isEmpty())
            qputenv("PRISMALUX_COMPUTE_MODE", cm.toUtf8());
    }

    /* ── Finestra principale (carica il tema saved via ThemeManager) ── */
    MainWindow w;
    w.show();

    return app.exec();
}

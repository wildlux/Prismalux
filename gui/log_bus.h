#pragma once
#include <QObject>
#include <QString>

/**
 * LogBus — singleton per inviare messaggi di log al pannello "Messaggi"
 * da qualsiasi pagina o widget senza dipendere da MainWindow.
 *
 * Uso:
 *   LogBus::post("❌ Errore: " + msg);              // finisce nella tab "Sistema"
 *   LogBus::post("📷 Scatto ricevuto", "3d");        // finisce nella tab dedicata "3D"
 *
 * category è una stringa libera (case-insensitive); MainWindow la mappa
 * alle tab del dialog "Messaggi" — vedi MainWindow::LogCategory. Vuota =
 * tab "Sistema" (comportamento invariato per i chiamanti esistenti).
 *
 * MainWindow connette LogBus::instance()->event a appendLog().
 */
class LogBus : public QObject {
    Q_OBJECT
public:
    static LogBus* instance();
    static void post(const QString& msg, const QString& category = QString());

signals:
    void event(const QString& msg, const QString& category);

private:
    explicit LogBus(QObject* parent = nullptr);
};

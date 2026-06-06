#pragma once
#include <QObject>
#include <QString>

/**
 * LogBus — singleton per inviare messaggi di log al pannello "Messaggi"
 * da qualsiasi pagina o widget senza dipendere da MainWindow.
 *
 * Uso:
 *   LogBus::post("❌ Errore: " + msg);
 *
 * MainWindow connette LogBus::instance()->event a appendLog().
 */
class LogBus : public QObject {
    Q_OBJECT
public:
    static LogBus* instance();
    static void post(const QString& msg);

signals:
    void event(const QString& msg);

private:
    explicit LogBus(QObject* parent = nullptr);
};

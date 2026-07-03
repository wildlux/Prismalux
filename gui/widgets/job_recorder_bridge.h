#pragma once
#include <QObject>
#include <QString>

/* JobRecorderBridge — ponte QWebChannel esposto alla pagina web embedded
   nell'Assistente Candidature. La pagina chiama elementClicked() via JS
   (window.pxBridge.elementClicked(...)) quando l'utente Alt+clicca un
   campo in modalità registrazione; il segnale clicked() arriva a
   LavoroPage che aggiunge il passo alla macro. */
class JobRecorderBridge : public QObject {
    Q_OBJECT
public:
    explicit JobRecorderBridge(QObject* parent = nullptr) : QObject(parent) {}

public slots:
    void elementClicked(const QString& selettore, const QString& tag, const QString& hint) {
        emit clicked(selettore, tag, hint);
    }

signals:
    void clicked(const QString& selettore, const QString& tag, const QString& hint);
};

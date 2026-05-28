#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

struct CcnaQuestion {
    QString     domanda;
    QStringList risposte;  // sempre 4
    int         corretta  = 0;
    QString     spiegazione;
    QString     tema;
};

class QuizCcnaDb : public QObject {
    Q_OBJECT
public:
    explicit QuizCcnaDb(QObject* parent = nullptr);
    ~QuizCcnaDb();

    bool isAvailable() const;

    int  totalQuestions()                         const;
    QStringList temi()                            const;

    CcnaQuestion randomQuestion()                 const;
    CcnaQuestion randomByTema(const QString& t)   const;

    /* Restituisce n domande casuali (con o senza filtro tema) */
    QList<CcnaQuestion> randomBatch(int n, const QString& tema = {}) const;

private:
    struct Impl;
    Impl* d = nullptr;
};

#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

/* --------------------------------------------------------------
   QuizQuestion — singola domanda a risposta multipla.
   -------------------------------------------------------------- */
struct QuizQuestion {
    QString domanda;
    QString risposte[4];   ///< 0..3
    int     corretta = 0;  ///< indice 0-based
    QString spiegazione;
    QString tema;
};

/* --------------------------------------------------------------
   QuizCcnaDb — database locale domande CCNA su SQLite3.

   Al primo avvio crea quiz_ccna.db in AppDataLocation e
   lo popola con le domande predefinite. Le query successive
   leggono dal DB senza coinvolgere i modelli AI.
   -------------------------------------------------------------- */
class QuizCcnaDb {
public:
    QuizCcnaDb();
    ~QuizCcnaDb();

    /** Lista di tutti i temi disponibili. */
    QStringList temi() const;

    /** Domanda casuale su tutti i temi. */
    QuizQuestion randomQuestion() const;

    /** Domanda casuale filtrata per tema. */
    QuizQuestion randomByTema(const QString& tema) const;

    int count() const { return m_count; }

private:
    void initDb();
    void populate();
    QuizQuestion rowToQuestion(const QString& dom,
                               const QString& r0, const QString& r1,
                               const QString& r2, const QString& r3,
                               int corretta, const QString& spiega,
                               const QString& tema) const;

    QString m_connName;
    int     m_count = 0;
};

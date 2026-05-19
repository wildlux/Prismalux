#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

/* --------------------------------------------------------------
   QuizQuestion — singola domanda a risposta multipla.
   -------------------------------------------------------------- */
struct QuizQuestion {
    QString domanda;
    QString risposte[4];  ///< 0..3
    int     corretta;     ///< indice 0-based
    QString spiegazione;
    QString tema;
};

/* --------------------------------------------------------------
   QuizCcnaDb — database locale domande CCNA pre-generate.

   Non usa SQLite runtime: le domande sono hardcoded nella .cpp
   e vengono filtrate per tema in memoria.
   -------------------------------------------------------------- */
class QuizCcnaDb {
public:
    QuizCcnaDb();

    /** Lista di tutti i temi disponibili. */
    QStringList temi() const;

    /** Domanda casuale su tutti i temi. */
    QuizQuestion randomQuestion() const;

    /** Domanda casuale filtrata per tema. */
    QuizQuestion randomByTema(const QString& tema) const;

    int count() const { return m_questions.count(); }

private:
    QVector<QuizQuestion> m_questions;

    void load();
};

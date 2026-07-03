#pragma once
#include <QString>
#include <QList>
#include <QMetaType>

struct Offerta {
    QString azienda, ruolo, sede, tipo, livello, email, requisiti;
};
Q_DECLARE_METATYPE(Offerta)

/* ── Dati ── */
const QList<Offerta>& kOfferte();

/* ── Algoritmo di filtro (puro — nessuna dipendenza Qt Widgets) ── */
QList<Offerta> offerteFiltrate(const QString& tipo, const QString& livello);

/* ── Presentazione dati (mappatura campi → icone, testabile in isolamento) ── */
QString tipoIcon(const QString& tipo);
QString livLabel(const QString& livello);

/* ══════════════════════════════════════════════════════════════
   Assistente Candidature — profilo candidato + macro di compilazione
   ══════════════════════════════════════════════════════════════ */

/* Dati anagrafici usati per compilare i form dei siti di lavoro. */
struct CandidatoProfilo {
    QString nome, cognome, email, telefono, cvPath, fotoPath;
};

enum class MacroAzione { Click, Scrivi };

/* Un passo registrato: click su un elemento, oppure scrittura di un
   valore (preso dal profilo candidato o testo letterale) in un campo. */
struct MacroStep {
    MacroAzione azione = MacroAzione::Click;
    QString selettore;   ///< selettore CSS calcolato lato JS
    QString etichetta;   ///< descrizione leggibile per la lista passi
    QString campoDato;   ///< "nome"|"cognome"|"email"|"telefono"|"cv"|"letterale:<testo>"
};

/* Sequenza di passi salvata per un dominio (es. "indeed.com"). */
struct JobMacro {
    QString dominio;
    QString nome;
    QList<MacroStep> passi;
};

/* ── Conversioni pure (testabili senza Qt Widgets) ── */
QString macroAzioneToString(MacroAzione a);
MacroAzione macroAzioneFromString(const QString& s);

/* Estrae il dominio da un URL (es. "https://it.indeed.com/foo" → "indeed.com"). */
QString dominioDaUrl(const QString& url);

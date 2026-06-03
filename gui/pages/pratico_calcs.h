#pragma once
/* ═══════════════════════════════════════════════════════════════
   pratico_calcs.h — Algoritmi finanziari e anagrafici (header-only)

   Esposti per i test unitari:
     PraticoCalcs::normalizzaComune()
     PraticoCalcs::cercaBelfiore()
     PraticoCalcs::calcolaCodiceFiscale()

   Uso in produzione: #include "pratico_calcs.h" in pratico_page.cpp
   Uso nei test:      #include "../pratico_calcs.h" in test_pratico_finanza.cpp
   ═══════════════════════════════════════════════════════════════ */
#include <QString>
#include <QDate>
#include <QHash>

namespace PraticoCalcs {

/** Normalizza il nome di un comune: strip accenti + toLower + trim. */
inline QString normalizzaComune(const QString& s)
{
    static const QHash<QChar,QChar> kAcc = {
        {u'\xe0','a'},{u'\xe1','a'},{u'\xe2','a'},
        {u'\xe8','e'},{u'\xe9','e'},{u'\xea','e'},
        {u'\xec','i'},{u'\xed','i'},{u'\xee','i'},
        {u'\xf2','o'},{u'\xf3','o'},{u'\xf4','o'},
        {u'\xf9','u'},{u'\xfa','u'},{u'\xfb','u'},
        {u'\xfc','u'}
    };
    QString r;
    for (QChar c : s.toLower().trimmed())
        r += kAcc.value(c, c);
    return r;
}

/** Restituisce il codice Belfiore (4 char) per il comune italiano,
 *  oppure stringa vuota se non trovato. */
inline QString cercaBelfiore(const QString& comune)
{
    static const QHash<QString,QString> kBelfiore = {
        /* ── Capoluoghi di regione ──────────────────────── */
        {"torino","L219"},     {"milano","F205"},     {"venezia","L736"},
        {"genova","D969"},     {"bologna","A944"},     {"firenze","D612"},
        {"ancona","A271"},     {"roma","H501"},        {"napoli","F839"},
        {"bari","A662"},       {"potenza","G942"},     {"catanzaro","C374"},
        {"palermo","G273"},    {"cagliari","B354"},    {"trento","L378"},
        {"trieste","L424"},    {"perugia","G478"},     {"campobasso","B519"},
        {"aosta","A326"},      {"l'aquila","A345"},    {"aquila","A345"},
        /* ── Capoluoghi di provincia ─────────────────────── */
        {"alessandria","A182"},{"asti","A479"},        {"avellino","A509"},
        {"belluno","A757"},    {"benevento","A783"},   {"bergamo","A794"},
        {"biella","A859"},     {"bolzano","A952"},     {"bozen","A952"},
        {"brescia","B157"},    {"brindisi","B180"},
        {"caltanissetta","B429"},{"campobasso","B519"},{"caserta","B963"},
        {"catania","C351"},    {"chieti","C632"},      {"como","C933"},
        {"cosenza","D086"},    {"cremona","D150"},     {"crotone","D122"},
        {"cuneo","D205"},      {"enna","C342"},        {"fermo","D542"},
        {"ferrara","D548"},    {"foggia","D643"},      {"forli","D704"},
        {"frosinone","D810"},  {"gorizia","E098"},
        {"grosseto","E202"},   {"imperia","E290"},     {"isernia","E335"},
        {"la spezia","E463"},  {"laspezia","E463"},    {"latina","E472"},
        {"lecce","E794"},      {"lecco","E507"},       {"livorno","E625"},
        {"lodi","E648"},       {"lucca","E715"},       {"macerata","E783"},
        {"mantova","E897"},    {"massa","F023"},       {"matera","F052"},
        {"messina","F158"},    {"modena","F257"},      {"monza","F704"},
        {"novara","F952"},     {"nuoro","F979"},
        {"oristano","G113"},   {"padova","G224"},
        {"parma","G337"},      {"pavia","G388"},
        {"pesaro","G453"},     {"pescara","G482"},     {"piacenza","G535"},
        {"pisa","G702"},       {"pistoia","G713"},     {"pordenone","G888"},
        {"prato","G999"},      {"ragusa","H163"},
        {"ravenna","H199"},    {"reggio calabria","H224"},
        {"reggio emilia","H223"},{"regio emilia","H223"},
        {"rieti","H282"},      {"rimini","H294"},
        {"rovigo","H620"},     {"salerno","H703"},     {"sassari","I452"},
        {"savona","I480"},     {"siena","I726"},       {"siracusa","I754"},
        {"sondrio","I829"},    {"taranto","L049"},     {"teramo","L103"},
        {"terni","L117"},      {"trapani","L331"},
        {"treviso","L407"},    {"udine","L483"},       {"varese","L682"},
        {"verbania","L746"},   {"vercelli","L750"},    {"verona","L781"},
        {"vibo valentia","F1104"},                     {"vicenza","L840"},
        {"viterbo","M082"},
        /* ── Grandi comuni extra ─────────────────────────── */
        {"giugliano in campania","E054"},
        {"andria","A285"},     {"barletta","A669"},    {"reggio","H224"},
        /* ── Paesi esteri (codici Z) ─────────────────────── */
        {"albania","Z100"},    {"algeria","Z301"},     {"argentina","Z600"},
        {"australia","Z700"},  {"austria","Z102"},     {"belgio","Z103"},
        {"brasile","Z602"},    {"canada","Z401"},      {"cina","Z210"},
        {"croazia","Z118"},    {"danimarca","Z104"},   {"egitto","Z336"},
        {"etiopia","Z315"},    {"filippine","Z216"},   {"finlandia","Z105"},
        {"francia","Z110"},    {"germania","Z112"},    {"giappone","Z219"},
        {"grecia","Z115"},     {"india","Z222"},       {"iran","Z224"},
        {"irlanda","Z116"},    {"israele","Z225"},     {"marocco","Z330"},
        {"messico","Z605"},    {"nigeria","Z325"},     {"norvegia","Z120"},
        {"paesi bassi","Z121"},{"olanda","Z121"},      {"pakistan","Z236"},
        {"polonia","Z127"},    {"portogallo","Z128"},  {"romania","Z129"},
        {"russia","Z130"},     {"senegal","Z343"},     {"somalia","Z342"},
        {"spagna","Z131"},     {"stati uniti","Z404"},{"usa","Z404"},
        {"america","Z404"},    {"svezia","Z132"},      {"svizzera","Z133"},
        {"tunisia","Z352"},    {"turchia","Z243"},     {"ucraina","Z138"},
        {"regno unito","Z114"},{"inghilterra","Z114"},{"gran bretagna","Z114"},
    };
    return kBelfiore.value(normalizzaComune(comune), "");
}

/** Calcola il Codice Fiscale italiano (D.M. 23/12/1976).
 *  Restituisce stringa vuota se i dati non sono validi. */
inline QString calcolaCodiceFiscale(
    const QString& cognome, const QString& nome,
    const QDate& nascita, bool maschio, const QString& belfiore)
{
    if (cognome.trimmed().isEmpty() || nome.trimmed().isEmpty()
        || !nascita.isValid() || belfiore.length() != 4)
        return {};

    auto lettera = [](QChar c) -> bool { return c.isLetter(); };
    auto isVoc   = [](QChar c) -> bool { return QString("AEIOU").contains(c); };
    auto cons = [&](const QString& s) {
        QString r;
        for (QChar c : s.toUpper())
            if (lettera(c) && !isVoc(c)) r += c;
        return r;
    };
    auto voc = [&](const QString& s) {
        QString r;
        for (QChar c : s.toUpper())
            if (lettera(c) && isVoc(c)) r += c;
        return r;
    };

    /* Cognome: prime 3 tra consonanti + vocali + X */
    QString cog = cons(cognome) + voc(cognome);
    while (cog.length() < 3) cog += 'X';
    cog = cog.left(3);

    /* Nome: >=4 consonanti → 1a+3a+4a; altrimenti cons+voc+X */
    const QString nc = cons(nome);
    const QString nv = voc(nome);
    QString nom;
    if (nc.length() >= 4)
        nom = QString(nc[0]) + nc[2] + nc[3];
    else {
        nom = nc + nv;
        while (nom.length() < 3) nom += 'X';
        nom = nom.left(3);
    }

    /* Anno (2 cifre), mese (lettera), giorno + sesso */
    const QString anno  = nascita.toString("yy");
    const char kMesi[]  = "ABCDEHLMPRST";
    const QChar  mese   = kMesi[nascita.month() - 1];
    const int    giorno = nascita.day() + (maschio ? 0 : 40);
    const QString gg    = QString("%1").arg(giorno, 2, 10, QChar('0'));

    const QString cf15 = cog + nom + anno + mese + gg + belfiore.toUpper();
    if (cf15.length() != 15) return {};

    /* Tabella dispari D.M. 1976 */
    static const int kDisp[36] = {
         1, 0, 5, 7, 9,13,15,17,19,21,
         1, 0, 5, 7, 9,13,15,17,19,21,
         2, 4,18,20,11, 3, 6, 8,12,14,
        16,10,22,25,24,23
    };

    int somma = 0;
    for (int i = 0; i < 15; ++i) {
        const QChar c = cf15[i];
        const int idx = c.isDigit() ? c.digitValue() : 10 + (c.unicode() - 'A');
        if (i % 2 == 0) somma += kDisp[idx];
        else            somma += idx;
    }
    return cf15 + QChar('A' + (somma % 26));
}

} // namespace PraticoCalcs

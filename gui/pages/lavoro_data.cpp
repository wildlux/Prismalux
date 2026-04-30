/*
 * lavoro_data.cpp — Database offerte di lavoro + algoritmo di filtro
 * ==================================================================
 * Fonte: Centro per l'Impiego di Catania, Aprile 2026
 * Nessuna dipendenza da Qt Widgets — solo QString/QList.
 * La UI (QListWidget, segnali, AI) è in lavoro_page.cpp.
 */
#include "lavoro_data.h"

const QList<Offerta>& kOfferte() {
    static const QList<Offerta> s = {
        // ── IT / Informatica ──────────────────────────────────────────
        {"Reddrop Srl",           "Web Dev / SEO / Social Media Manager",     "Tremestieri Etneo",    "IT",          "qualsiasi", "", "Conoscenza web, no requisiti formali"},
        {"Zucchetti S.p.a.",      "Software Consultant AMS",                  "Catania (Smart W.)",   "IT",          "diploma",   "", "Diploma o laurea informatica/economia/umanistica, inglese base"},
        {"Sibeg Srl",             "IT Specialist",                            "Catania",              "IT",          "diploma",   "", "Diploma o laurea, esperienza nel ruolo preferibile"},
        {"Sibeg Srl",             "ABAP Developer",                           "Catania",              "IT",          "diploma",   "", "Diploma o laurea, esperienza SAP preferibile"},
        {"bcame",                 "Back End Developer",                       "Catania",              "IT",          "diploma",   "", "Esperienza pregressa nel ruolo"},
        {"bcame",                 "Full Stack Developer",                     "Catania",              "IT",          "diploma",   "", "Linguaggi programmazione, database, framework, inglese"},
        {"bcame",                 "Senior Software Developer",                "Catania",              "IT",          "laurea_t",  "", "Comprovata esperienza nel ruolo"},
        {"Seasoft",               "Sviluppatore Visual Basic",                "Catania",              "IT",          "qualsiasi", "", "Programmazione C standard, Visual Basic, VB6"},
        {"Neperia Group",         "Java Developer",                           "Catania",              "IT",          "diploma",   "", "Esperienza minima 3 anni"},
        {"Xeniasoft",             "Sviluppatore .NET / ASP.NET / C# / MVC",  "Gravina di Catania",   "IT",          "diploma",   "", "Diploma tecnico-scientifico, esperienza pregressa"},
        {"Xeniasoft",             "IT Help Desk EN/ES",                       "Gravina di Catania",   "IT",          "diploma",   "", "Ottima conoscenza PC, inglese o spagnolo C1"},
        {"Netith",                "Addetto IT",                               "Patern\xc3\xb2",       "IT",          "diploma",   "", "Laurea o diploma indirizzo tecnico, 2 anni esperienza"},
        {"Flazio",                "Full Stack Developer",                     "Catania",              "IT",          "diploma",   "", "Esperienza 2-3 anni"},
        {"Flazio",                "Front End Developer",                      "Catania",              "IT",          "diploma",   "", "Esperienza 2-3 anni"},
        {"Flazio",                "Back End Developer",                       "Catania",              "IT",          "diploma",   "", "Subordinato full time"},
        {"Flazio",                "Web Designer",                             "Catania",              "IT",          "diploma",   "", "Subordinato full time"},
        {"Flazio",                "UX / UI Designer",                         "Catania",              "IT",          "diploma",   "", "Subordinato full time"},
        {"STMicroelectronics",    "Varie posizioni",                          "Catania",              "IT",          "laurea_t",  "", "Profili ingegneria elettronica, informatica e affini"},
        {"G.G.G. Elettromeccan.", "Disegnatore Meccanico CAD",               "Catania",              "IT",          "qualsiasi", "", "CAD Rhinoceros e Pro-e, settore automotive preferibile"},
        // ── Retail / Vendite al dettaglio ────────────────────────────
        {"JD Sports",             "Supervisor (Capo Reparto)",                "Catania",              "Retail",      "diploma",   "", "Esperienza retail strutturato, inglese, pacchetto Office"},
        {"JD Sports",             "Sales Assistant Part Time",                "Catania",              "Retail",      "qualsiasi", "", "Team work, capacit\xc3\xa0 comunicazione"},
        {"Foot Locker",           "Addetto Vendite Part Time",                "Catania/Misterbianco", "Retail",      "qualsiasi", "", "Esperienza vendita 0-3 anni, flessibilit\xc3\xa0 oraria"},
        {"Foot Locker",           "Addetti/e Vendita",                        "Catania/Misterbianco", "Retail",      "qualsiasi", "", "Part time, turni serali, festivi e weekend"},
        {"Gamelife",              "Addetto Vendite",                          "Misterbianco",         "Retail",      "qualsiasi", "", "Buone conoscenze informatiche, disponibilit\xc3\xa0 weekend"},
        {"Luisa Spagnoli",        "Sales Assistant Part Time orizzontale",    "Catania",              "Retail",      "diploma",   "", "5 anni esperienza abbigliamento, inglese B1/B2"},
        {"Pepco",                 "Shift Leader (Vice Referente di Negozio)", "Catania",              "Retail",      "diploma",   "", "1 anno responsabile reparto retail o GDO"},
        {"Pepco",                 "Addetto Vendite Part Time",                "Catania",              "Retail",      "diploma",   "", "Esperienza vendite retail o GDO circa 1 anno"},
        {"Leroy Merlin",          "Addetto Vendite Giardino",                 "Catania Fontanarossa", "Retail",      "qualsiasi", "", "Passione natura/outdoor, con o senza esperienza"},
        {"Leroy Merlin",          "Addetto Casse Week End",                   "Catania Fontanarossa", "Retail",      "qualsiasi", "", "Preferibilmente iscritto a corso di laurea"},
        {"Salmoiraghi e Vigan\xc3\xb2","Addetto Vendita Part Time",          "Catania",              "Retail",      "qualsiasi", "", "Esperienza vendita/assistenza clienti, inglese gradito"},
        {"Media World",           "Addetti/e Vendita",                        "Catania CC Porte",     "Retail",      "qualsiasi", "", "Passione tecnologia, disponibilit\xc3\xa0 turni e weekend"},
        {"Tecnomat",              "Hostess/Steward di Cassa e Accoglienza",   "Misterbianco CC",      "Retail",      "diploma",   "", "Diploma, esperienza retail/vendita, disponibilit\xc3\xa0 turni"},
        {"Camomilla Italia",      "Addetto Vendite",                          "Catania",              "Retail",      "diploma",   "", "Diploma, inglese, disponibilit\xc3\xa0 turni"},
        {"Inditex",               "Addetti alle Vendite",                     "Misterbianco",         "Retail",      "qualsiasi", "", "Disponibilit\xc3\xa0 turni flessibili part time o full time"},
        {"Sephora",               "Beauty Advisor",                           "Catania",              "Retail",      "diploma",   "", "Diploma, inglese, preferibile esperienza retail multinazionale"},
        {"Flying Tiger Copenhagen","Addetti Vendita",                         "Catania",              "Retail",      "qualsiasi", "", "Preferibile 2 anni esperienza, contratto intermittente"},
        {"Mondo Convenienza",     "Addetto/a Vendita Canali Virtuali",        "Catania",              "Retail",      "diploma",   "", "Diploma, customer service, creativit\xc3\xa0 e passione grafica"},
        {"CDS Spa",               "Store Manager",                            "Catania",              "Retail",      "diploma",   "", "Diploma o laurea triennale, 5 anni esperienza GDO"},
        {"Primark",               "Department Manager",                       "Nazionale",            "Retail",      "diploma",   "", "2 anni manager retail/GDO, disponibile trasferimento"},
        {"Primark",               "Team Manager",                             "Nazionale",            "Retail",      "laurea_t",  "", "1 anno manager retail, disponibile trasferimento"},
        // ── GDO / Supermercati — portali Lavora con noi ──────────────
        {"Coop",                  "Portale Lavora con noi",                   "Nazionale",            "Retail",      "qualsiasi", "", "https://www.coop.it/lavora-con-noi"},
        {"Lidl",                  "Portale Lavora con noi",                   "Nazionale",            "Retail",      "qualsiasi", "", "https://lavoro.lidl.it"},
        {"Esselunga",             "Portale Lavora con noi",                   "Nazionale",            "Retail",      "qualsiasi", "", "https://www.esselungajob.it"},
        {"Carrefour",             "Portale Lavora con noi",                   "Nazionale",            "Retail",      "qualsiasi", "", "https://www.carrefour.it/lavora-con-noi"},
        {"Penny Market",          "Portale Lavora con noi",                   "Nazionale",            "Retail",      "qualsiasi", "", "https://www.penny.it/lavora-con-noi"},
        {"Eurospin",              "Portale Lavora con noi",                   "Nazionale",            "Retail",      "qualsiasi", "", "https://lavoraconnoi.eurospin.it"},
        {"MD Spa",                "Portale Lavora con noi",                   "Nazionale",            "Retail",      "qualsiasi", "", "https://www.mdspa.it/lavora-con-noi"},
        {"Pam Panorama",          "Portale Lavora con noi",                   "Nazionale",            "Retail",      "qualsiasi", "", "https://www.pampanorama.it/lavora-con-noi"},
        {"Gruppo Arena",          "Portale Lavora con noi (Dec\xc3\xb2, SuperConveniente)", "Sicilia", "Retail",      "qualsiasi", "", "https://www.gruppoarena.it/lavora-con-noi"},
        {"Ergon Spa",             "Portale Lavora con noi (Eurospar, Interspar, Altasfera)", "Sicilia","Retail",      "qualsiasi", "", "https://www.ergonsrl.it/lavora-con-noi"},
        {"CDS Spa",               "Portale Lavora con noi (Centro Distribuzione Supermercati)", "Sicilia","Retail",   "qualsiasi", "", "https://www.cdsspa.it/lavora-con-noi"},
        // ── Ristorazione ─────────────────────────────────────────────
        {"Chef Express",          "Addetto alla Ristorazione",                "Aeroporto Catania",    "Ristorazione","diploma",   "", "Diploma preferibilmente alberghiero, part time su turni"},
        {"Curtigghiu",            "Barman / Chef / Cuoco / Cameriere",        "Catania area",         "Ristorazione","qualsiasi", "", "Gradita conoscenza inglese"},
        {"FUD Bottega Sicula",    "Addetti sala, cucina, pizzaioli",          "Catania",              "Ristorazione","qualsiasi", "", "Varie sedi: Catania, Palermo, Milano"},
        {"Grand Hotel Villa Itria","Capo Partita",                            "Viagrande",            "Ristorazione","qualsiasi", "", "Esperienza alta qualit\xc3\xa0, disponibilit\xc3\xa0 turni"},
        {"Ristorante Antica Sicilia","Cuoco / Aiuto Cuoco / Cameriere / Lavapiatti","Catania",        "Ristorazione","qualsiasi", "", "Requisiti in base al profilo"},
        {"Ergon Spa (Despar)",    "Addetto Salumeria \xe2\x80\x94 Sost. Maternit\xc3\xa0","Catania", "Ristorazione","qualsiasi", "", "Conoscenza salumi, part time 24h settimanali"},
        // ── Edilizia ─────────────────────────────────────────────────
        {"Cosedil",               "Vari profili settore edilizia",            "Catania",              "Edilizia",    "qualsiasi", "selezione@cosedil.com", "Inviare CV via email \xe2\x80\x94 candidatura diretta"},
        {"Acrobatica Group",      "Muratore / Operaio Edile su Fune",         "Catania",              "Edilizia",    "qualsiasi", "", "2 anni esperienza, agilit\xc3\xa0 fisica"},
        {"Acrobatica Group",      "Muratore / Operaio Edile",                 "Catania",              "Edilizia",    "qualsiasi", "", "2 anni esperienza, lavoro di squadra"},
        {"Strano Serramenti",     "Posatori / Serramentisti",                 "Mascalucia CT",        "Edilizia",    "qualsiasi", "", "Infissi alluminio/PVC, patente, 5 giorni su 7"},
        {"Strano Serramenti",     "Fabbro",                                   "Mascalucia CT",        "Edilizia",    "qualsiasi", "", "Grate/cancelli/carpenteria metallica, patente"},
        // ── Tecnico / Impianti ───────────────────────────────────────
        {"Telebit Group",         "Tecnico Sistemi di Sicurezza",             "Catania",              "Tecnico",     "diploma",   "", "BMS, TVCC, IP Networking, inglese fluente"},
        {"Telebit Group",         "Tecnico Sistemi Antincendio",              "Catania",              "Tecnico",     "diploma",   "", "Diploma elettrotecnica, NFPA/F-Gas/UNI, 3-5 anni"},
        {"Telebit Group",         "Project Coordinator Impianti Tecnologici", "Catania",              "Tecnico",     "diploma",   "", "Diploma tecnico o laurea, inglese, 5 anni esperienza"},
        {"Telebit Group",         "Progettista Impianti Elettrici e Speciali","Catania",              "Tecnico",     "laurea_t",  "", "Laurea ingegneria elettrica, inglese, 5 anni"},
        {"Doctor Glass",          "Tecnico Installatore Vetri",               "Misterbianco",         "Tecnico",     "qualsiasi", "", "Manualit\xc3\xa0, preferibile esperienza officine/centri ricambi"},
        {"SD Refrigerazioni",     "Apprendista Tecnico Frigorista",           "Belpasso",             "Tecnico",     "qualsiasi", "", "Campo elettrico/climatizzazione, et\xc3\xa0 16-29 anni"},
        {"Luxi Impianti",         "Operatore Elettrico Junior",               "Acireale",             "Tecnico",     "diploma",   "", "Diploma tecnico industriale / IPSIA elettrico, gradita esperienza"},
        {"Luxi Impianti",         "Operatore Elettrico Senior",               "Acireale",             "Tecnico",     "diploma",   "", "Diploma tecnico, 2 anni esperienza, patente C, F-Gas"},
        {"Acquanova Sicilia",      "Tecnici Installatori",                    "Sant'Agata Li Battiati","Tecnico",    "qualsiasi", "", "Gestione/installazione dispositivi trattamento acqua potabile"},
        {"Stima Vending",         "Tecnico Manutentore Distributori Automatici","Catania area",       "Tecnico",     "qualsiasi", "", "Formazione meccanica/elettrica, comprovata esperienza"},
        {"Sibeg Srl",             "Manutentore Elettrico / Elettronico",      "Catania",              "Tecnico",     "diploma",   "", "Diploma ambito elettrico/elettronico, 3 anni esperienza"},
        // ── Logistica / Magazzino ────────────────────────────────────
        {"Leroy Merlin",          "Addetto al Magazzino",                     "Catania Fontanarossa", "Logistica",   "diploma",   "", "Diploma, esperienza logistica in retail/GDO"},
        {"Tecnomat",              "Addetti alla Logistica",                   "Misterbianco CC",      "Logistica",   "diploma",   "", "Diploma, esperienza retail, disponibilit\xc3\xa0 turni"},
        {"Penny Market",          "Transport Planning Employee",               "Catania",              "Logistica",   "qualsiasi", "", "Sistemi informatici, inglese, esperienza pregressa"},
        {"Stima Vending",         "Addetto al Rifornimento Distributori",     "Catania area",         "Logistica",   "qualsiasi", "", "In somministrazione, obiettivo contratto indeterminato"},
        {"Sud Trasporti",         "Addetto Logistico",                        "Catania",              "Logistica",   "diploma",   "", "2 anni esperienza, Office, inglese e spagnolo graditi"},
        // ── Produzione Industriale ───────────────────────────────────
        {"Cavicondor",            "Operatori Esperti su Macchinari",          "Belpasso CT",          "Produzione",  "diploma",   "", "2 anni esperienza, titolo tecnico meccanico/elettronico"},
        {"Cavicondor",            "Operaio di Produzione \xe2\x80\x94 Linea Automatizzata","Belpasso CT","Produzione","diploma",  "", "Diploma tecnico industriale, elettromeccanica o affini"},
        {"Cavicondor",            "Specialista Gestione Qualit\xc3\xa0",      "Belpasso CT",          "Produzione",  "laurea_t",  "", "Laurea ingegneria o diploma tecnico, gestione qualit\xc3\xa0, inglese"},
        // ── Admin / Segreteria ───────────────────────────────────────
        {"Silaq",                 "Segreteria di Sede",                       "Catania",              "Admin",       "qualsiasi", "", "Programmazione/organizzazione, ottimo pacchetto Office"},
        {"Silaq Consulting",      "Segreteria Organizzativa",                 "Catania",              "Admin",       "qualsiasi", "", "Esperienza anche breve, subordinato full time"},
        {"Luxi Impianti",         "Segretario Amministrativo",                "Acireale",             "Admin",       "diploma",   "", "Diploma ragioneria o laurea economia, 2 anni esperienza"},
        {"Netith",                "Operatore Telefonico Inbound Enel",        "Aci Castello",         "Admin",       "qualsiasi", "", "Esperienza settore energetico, vendita e negoziazione"},
        {"Flazio",                "Customer Care Specialist",                  "Catania",              "Admin",       "qualsiasi", "", "5+ anni contatto con il pubblico"},
        // ── Finanza / Assicurazioni ──────────────────────────────────
        {"Generali Italia",       "Consulente Assicurativo",                  "Prov. di Catania",     "Finanza",     "diploma",   "", "Laurea o diploma indirizzo tecnico o economico"},
        {"HDI Assicurazioni",     "Liquidatore Sinistri Rami Danni",          "Catania",              "Finanza",     "laurea_t",  "", "Laurea giurisprudenza, esperienza nel settore"},
        {"HDI Assicurazioni",     "Liquidatore Sinistri Junior",               "Catania",              "Finanza",     "laurea_t",  "", "Laurea giurisprudenza, primo impiego, inglese"},
        {"Sibeg Srl",             "Addetto Contabilit\xc3\xa0 Generale",      "Catania",              "Finanza",     "laurea_t",  "", "Laurea discipline economiche, studi professionali o aziendali"},
        {"Sibeg Srl",             "Tax & Accounting Specialist",              "Catania",              "Finanza",     "laurea_t",  "", "Laurea economia, 5 anni esperienza pregressa"},
        // ── Commerciale / Marketing ──────────────────────────────────
        {"SIBEG",                 "Sales Executive IC (Horeca e Normal Trade)","Catania",             "Commerciale", "laurea_t",  "", "Esperienza commerciale food&bev., laurea triennale gradita"},
        {"Sud Trasporti",         "Commerciale Senior",                       "Catania",              "Commerciale", "diploma",   "", "5 anni esperienza, Office, inglese e spagnolo"},
        {"Sud Trasporti",         "Commerciale Junior",                       "Catania",              "Commerciale", "diploma",   "", "1 anno esperienza commerciale, Office, inglese obbligatorio"},
        {"Gruppo Altea",          "Tecnico di Vendita",                       "Aci Sant'Antonio",     "Commerciale", "diploma",   "", "Pacchetto Office, automunito, eccellenti capacit\xc3\xa0 comunicative"},
        {"Cosine Italy",          "Brand Ambassador / Promoter",              "Catania",              "Commerciale", "qualsiasi", "", "Da verificare in base al profilo"},
        {"Euromecc Srl",          "Profili Commerciali / Manager / Progettisti","Misterbianco",       "Commerciale", "diploma",   "", "Diploma o laurea secondo profilo, inglese"},
        {"Sibeg Srl",             "Sales Executive",                          "Catania",              "Commerciale", "laurea_t",  "", "Laurea triennale o magistrale gradita"},
        {"Xeniasoft",             "Digital Sales Account",                    "Gravina di Catania",   "Commerciale", "diploma",   "", "Diploma in marketing/commerciale, esperienza pregressa"},
        {"Flazio",                "Marketing Manager",                        "Catania",              "Commerciale", "laurea_t",  "", "2-4 anni marketing B2B, Marketing Automation, Google Analytics"},
        {"Flazio",                "Sales Manager",                            "Catania",              "Commerciale", "qualsiasi", "", "3-5 anni esperienza pregressa"},
        // ── Turismo / Crociere ───────────────────────────────────────
        {"Parc Hotels Italia",    "Vari profili (Booking/Ricevimento/Cuoco/Barman)","Catania",        "Turismo",     "qualsiasi", "", "Vedere lavoraconnoi.parchotels.it"},
        {"MSC Cruises",           "Varie posizioni personale di bordo",       "Personale di bordo",   "Turismo",     "qualsiasi", "", "Inglese, varie nazionalit\xc3\xa0 a bordo"},
        {"Costa Crociere",        "Varie posizioni personale di bordo",       "Personale di bordo",   "Turismo",     "qualsiasi", "", "Inglese requisito"},
        {"Royal Caribbean",       "Varie posizioni personale di bordo",       "Personale di bordo",   "Turismo",     "qualsiasi", "", "Inglese requisito"},
        // ── Sanitario ────────────────────────────────────────────────
        {"Hobby Zoo",             "Farmacisti",                               "Catania",              "Sanitario",   "laurea_m",  "", "Laurea farmacia o CTF, abilitazione, iscrizione albo"},
        {"Conad",                 "Farmacisti",                               "Italia",               "Sanitario",   "laurea_m",  "", "Laurea magistrale farmacia o CTF, abilitazione"},
        {"Silaq Consulting",      "Tecnologo Alimentare / Biologo",           "Catania",              "Sanitario",   "laurea_t",  "", "Laurea, subordinato full time tempo determinato"},
        {"Sibeg Srl",             "CQ Specialist",                            "Catania",              "Sanitario",   "laurea_t",  "", "Laurea chimica ambientale o scienze ambientali"},
        {"Virgin Active",         "Personal Trainer e Istruttori Fitness",    "Catania",              "Sanitario",   "laurea_t",  "", "Laurea scienze motorie, attestati specifici per profilo"},
        // ── Altro ────────────────────────────────────────────────────
        {"Ferrovie dello Stato",  "Vari profili (Ingegneri/Macchinisti/Capotreno)","Italia",          "Altro",       "diploma",   "", "Da verificare in base al profilo specifico"},
        {"Ferrovie dello Stato",  "Varie posizioni Diplomati",                "Tutta Italia",         "Altro",       "diploma",   "", "Diploma, tutta Italia"},
        {"Silaq Consulting",      "Consulente Tecnico Sicurezza sul Lavoro",  "Catania",              "Altro",       "laurea_t",  "", "Laurea ingegneria edile o tecniche della prevenzione"},
    };
    return s;
}

/* ── Algoritmo di filtro ─────────────────────────────────────────────────
   Restituisce le offerte che soddisfano tipo e livello.
   "tutti" su qualsiasi parametro significa nessun filtro su quella colonna.
   La compatibilità livello è transitiva: laurea_m include laurea_t e diploma.
   ───────────────────────────────────────────────────────────────────────── */
QList<Offerta> offerteFiltrate(const QString& tipo, const QString& livello)
{
    QStringList livCompat{"qualsiasi"};
    if (livello == "diploma"  || livello == "laurea_t" || livello == "laurea_m" || livello == "tutti")
        livCompat << "diploma";
    if (livello == "laurea_t" || livello == "laurea_m" || livello == "tutti")
        livCompat << "laurea_t";
    if (livello == "laurea_m" || livello == "tutti")
        livCompat << "laurea_m";

    QList<Offerta> result;
    for (const auto& o : kOfferte()) {
        if (tipo    != "tutti" && o.tipo    != tipo)                  continue;
        if (livello != "tutti" && !livCompat.contains(o.livello))     continue;
        result << o;
    }
    return result;
}

/* ── Presentazione campi dati ────────────────────────────────────────────
   Mappano i valori enum-like di Offerta::tipo e Offerta::livello
   a icone Unicode. Funzioni pure — nessuna dipendenza Qt Widgets.
   ───────────────────────────────────────────────────────────────────────── */
QString tipoIcon(const QString& t)
{
    if (t == "IT")           return "\xf0\x9f\x92\xbb ";
    if (t == "Retail")       return "\xf0\x9f\x9b\x92 ";
    if (t == "Ristorazione") return "\xf0\x9f\x8d\xbd ";
    if (t == "Edilizia")     return "\xf0\x9f\x8f\x97 ";
    if (t == "Logistica")    return "\xf0\x9f\x93\xa6 ";
    if (t == "Finanza")      return "\xf0\x9f\x92\xb0 ";
    if (t == "Sanitario")    return "\xf0\x9f\x8f\xa5 ";
    if (t == "Produzione")   return "\xe2\x9a\x99\xef\xb8\x8f ";
    if (t == "Tecnico")      return "\xf0\x9f\x94\xa7 ";
    if (t == "Turismo")      return "\xe2\x9c\x88\xef\xb8\x8f ";
    if (t == "Admin")        return "\xf0\x9f\x93\x8b ";
    if (t == "Commerciale")  return "\xf0\x9f\x93\x8a ";
    return "\xf0\x9f\x94\xb9 ";
}

QString livLabel(const QString& l)
{
    if (l == "qualsiasi") return "  \xf0\x9f\x9f\xa2";
    if (l == "diploma")   return "  \xf0\x9f\x9f\xa1";
    if (l == "laurea_t")  return "  \xf0\x9f\x9f\xa0";
    if (l == "laurea_m")  return "  \xf0\x9f\x94\xb4";
    return "";
}

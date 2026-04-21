#include "lavoro_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QGroupBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QSplitter>
#include <QMenu>
#include <QGuiApplication>
#include <QClipboard>
#include <QProcess>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <memory>

/* ══════════════════════════════════════════════════════════════
   Database offerte — Centro per l'Impiego di Catania, Aprile 2026
   Fonte: documento ufficiale CPI Catania
   ══════════════════════════════════════════════════════════════ */
static const QList<Offerta>& kOfferte() {
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

/* ══════════════════════════════════════════════════════════════
   caricaCV
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::caricaCV(const QString& path) {
    if (!m_cvStatus) return;
    QFileInfo fi(path);
    if (!fi.exists()) {
        m_cvStatus->setText("\xe2\x9d\x8c File non trovato");
        m_cvStatus->setStyleSheet("color:#F44336;");
        return;
    }
    QProcess proc;
    proc.start("pdftotext", {path, "-"});
    if (proc.waitForFinished(8000) && proc.exitCode() == 0) {
        const QString txt = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (!txt.isEmpty()) {
            m_cvText = txt;
            m_cvStatus->setText(QString("\xe2\x9c\x85 CV caricato: %1 (%2 car.)")
                .arg(fi.fileName()).arg(m_cvText.size()));
            m_cvStatus->setStyleSheet("color:#4CAF50;");
            return;
        }
    }
    m_cvText.clear();
    m_cvStatus->setText(QString("\xf0\x9f\x93\x84 CV selezionato: %1 (profilo integrato)").arg(fi.fileName()));
    m_cvStatus->setStyleSheet("color:#E5C400;");
}

/* ══════════════════════════════════════════════════════════════
   applicaFiltri
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::applicaFiltri() {
    if (!m_offerteLista || !m_filtroTipo || !m_filtroLivello) return;

    const QString tipo    = m_filtroTipo->currentData().toString();
    const QString livello = m_filtroLivello->currentData().toString();

    QStringList livCompat{"qualsiasi"};
    if (livello == "diploma" || livello == "laurea_t" || livello == "laurea_m" || livello == "tutti")
        livCompat << "diploma";
    if (livello == "laurea_t" || livello == "laurea_m" || livello == "tutti")
        livCompat << "laurea_t";
    if (livello == "laurea_m" || livello == "tutti")
        livCompat << "laurea_m";

    auto tipoIcon = [](const QString& t) -> QString {
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
    };

    auto livLabel = [](const QString& l) -> QString {
        if (l == "qualsiasi") return "  \xf0\x9f\x9f\xa2";
        if (l == "diploma")   return "  \xf0\x9f\x9f\xa1";
        if (l == "laurea_t")  return "  \xf0\x9f\x9f\xa0";
        if (l == "laurea_m")  return "  \xf0\x9f\x94\xb4";
        return "";
    };

    m_offerteLista->clear();
    for (const auto& o : kOfferte()) {
        if (tipo != "tutti" && o.tipo != tipo) continue;
        if (livello != "tutti" && !livCompat.contains(o.livello)) continue;

        QString emailStr = o.email.isEmpty() ? ""
                         : QString("  \xe2\x9c\x89 %1").arg(o.email);
        const QString text = tipoIcon(o.tipo) + o.azienda + " \xe2\x80\x94 " + o.ruolo
                           + "\n   \xf0\x9f\x93\x8d " + o.sede + livLabel(o.livello) + emailStr;

        auto* item = new QListWidgetItem(text, m_offerteLista);
        item->setSizeHint(QSize(-1, 52));
        item->setData(Qt::UserRole, QVariant::fromValue(o));
        if (!o.email.isEmpty())
            item->setForeground(QColor("#4CAF50"));
    }
}

/* ══════════════════════════════════════════════════════════════
   popolaModelli — aggiorna il combo con i modelli disponibili
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::popolaModelli(const QStringList& models) {
    if (!m_cmbModello) return;
    const QString current = m_cmbModello->currentText();
    m_cmbModello->blockSignals(true);
    m_cmbModello->clear();
    for (const auto& m : models)
        m_cmbModello->addItem(m);
    // ripristina selezione precedente se ancora disponibile
    const int idx = m_cmbModello->findText(current);
    m_cmbModello->setCurrentIndex(idx >= 0 ? idx : 0);
    m_cmbModello->blockSignals(false);
    if (m_modelloLbl)
        m_modelloLbl->setText("\xf0\x9f\xa4\x96 " + m_cmbModello->currentText());
}

/* ══════════════════════════════════════════════════════════════
   Costruttore LavoroPage
   ══════════════════════════════════════════════════════════════ */
LavoroPage::LavoroPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->setSpacing(8);

    /* ── Riga superiore: CV + Modello LLM ── */
    auto* topRow = new QWidget(this);
    auto* topL   = new QHBoxLayout(topRow);
    topL->setContentsMargins(0,0,0,0); topL->setSpacing(12);

    /* CV Picker */
    auto* cvBox = new QGroupBox("\xf0\x9f\x93\x84  Curriculum Vitae", topRow);
    auto* cvLay = new QHBoxLayout(cvBox);
    cvLay->setSpacing(8);

    m_cvPath = new QLineEdit(cvBox);
    m_cvPath->setPlaceholderText("Percorso file PDF...");
    m_cvPath->setReadOnly(true);

    auto* sfogliaBtn = new QPushButton("\xf0\x9f\x93\x82 Sfoglia...", cvBox);
    sfogliaBtn->setObjectName("actionBtn");
    sfogliaBtn->setFixedWidth(90);

    m_cvStatus = new QLabel("Nessun CV caricato", cvBox);
    m_cvStatus->setObjectName("pageSubtitle");

    cvLay->addWidget(m_cvPath, 1);
    cvLay->addWidget(sfogliaBtn);
    cvLay->addWidget(m_cvStatus);

    /* LLM Selector */
    auto* llmBox = new QGroupBox("\xf0\x9f\xa4\x96  Modello AI", topRow);
    auto* llmLay = new QHBoxLayout(llmBox);
    llmLay->setSpacing(8);

    m_cmbModello = new QComboBox(llmBox);
    m_cmbModello->setObjectName("cmbModello");
    m_cmbModello->setMinimumWidth(180);
    m_cmbModello->addItem("\xf0\x9f\x94\x84 Caricamento modelli...");

    m_modelloLbl = new QLabel("", llmBox);
    m_modelloLbl->setObjectName("pageSubtitle");

    auto* fetchBtn = new QPushButton("\xf0\x9f\x94\x84", llmBox);
    fetchBtn->setObjectName("actionBtn");
    fetchBtn->setFixedWidth(34);
    fetchBtn->setToolTip("Aggiorna lista modelli");

    llmLay->addWidget(m_cmbModello, 1);
    llmLay->addWidget(fetchBtn);
    llmLay->addWidget(m_modelloLbl);

    topL->addWidget(cvBox, 3);
    topL->addWidget(llmBox, 2);
    lay->addWidget(topRow);

    connect(sfogliaBtn, &QPushButton::clicked, this, [this]{
        const QString path = QFileDialog::getOpenFileName(
            this, "Seleziona Curriculum Vitae",
            QDir::homePath(), "PDF (*.pdf);;Testo (*.txt);;Tutti (*)");
        if (!path.isEmpty()) {
            m_cvPath->setText(path);
            caricaCV(path);
        }
    });

    // Pre-carica CV di Paolo automaticamente
    const QString defaultCv = "/home/wildlux/CURRICULUM/IT_CV_18_05_2025_Paolo_Lo_Bello.pdf";
    if (QFile::exists(defaultCv)) {
        m_cvPath->setText(defaultCv);
        caricaCV(defaultCv);
    }

    // Connessioni modello
    connect(m_ai, &AiClient::modelsReady, this, &LavoroPage::popolaModelli);
    connect(fetchBtn, &QPushButton::clicked, m_ai, &AiClient::fetchModels);
    connect(m_cmbModello, &QComboBox::currentTextChanged, this, [this](const QString& m){
        if (m_modelloLbl) m_modelloLbl->setText("\xf0\x9f\xa4\x96 " + m);
        if (!m.isEmpty() && !m.startsWith("\xf0\x9f\x94\x84"))
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m);
    });
    m_ai->fetchModels();

    /* ── Filtri ── */
    auto* filtriRow = new QWidget(this);
    auto* filtriL   = new QHBoxLayout(filtriRow);
    filtriL->setContentsMargins(0,0,0,0); filtriL->setSpacing(10);

    filtriL->addWidget(new QLabel("\xf0\x9f\x94\x8d Tipo:", filtriRow));
    m_filtroTipo = new QComboBox(filtriRow);
    m_filtroTipo->setObjectName("filtroTipo");
    m_filtroTipo->setFixedWidth(185);

    const struct { const char* label; const char* data; } tipi[] = {
        {"Tutti i tipi",                   "tutti"},
        {"\xf0\x9f\x92\xbb IT / Informatica",              "IT"},
        {"\xf0\x9f\x9b\x92 Retail / Vendite dettaglio",   "Retail"},
        {"\xf0\x9f\x8d\xbd Ristorazione",                  "Ristorazione"},
        {"\xf0\x9f\x8f\x97 Edilizia",                      "Edilizia"},
        {"\xf0\x9f\x93\xa6 Logistica / Magazzino",         "Logistica"},
        {"\xf0\x9f\x92\xb0 Finanza / Assicurazioni",       "Finanza"},
        {"\xf0\x9f\x8f\xa5 Sanitario",                     "Sanitario"},
        {"\xe2\x9a\x99\xef\xb8\x8f Produzione",            "Produzione"},
        {"\xf0\x9f\x94\xa7 Tecnico / Impianti",            "Tecnico"},
        {"\xe2\x9c\x88\xef\xb8\x8f Turismo / Crociere",   "Turismo"},
        {"\xf0\x9f\x93\x8b Admin / Segreteria",            "Admin"},
        {"\xf0\x9f\x93\x8a Commerciale / Marketing",       "Commerciale"},
        {"\xf0\x9f\x94\xb9 Altro",                         "Altro"},
    };
    for (const auto& t : tipi)
        m_filtroTipo->addItem(t.label, QString(t.data));
    filtriL->addWidget(m_filtroTipo);

    filtriL->addSpacing(16);
    filtriL->addWidget(new QLabel("\xf0\x9f\x8e\x93 Istruzione:", filtriRow));
    m_filtroLivello = new QComboBox(filtriRow);
    m_filtroLivello->setObjectName("filtroLivello");
    m_filtroLivello->setFixedWidth(210);

    const struct { const char* label; const char* data; } livelli[] = {
        {"Tutti i livelli",                      "tutti"},
        {"\xf0\x9f\x9f\xa2 Media inferiore (nessun titolo)",  "media"},
        {"\xf0\x9f\x9f\xa1 Diploma superiore",                 "diploma"},
        {"\xf0\x9f\x9f\xa0 Laurea triennale",                  "laurea_t"},
        {"\xf0\x9f\x94\xb4 Laurea magistrale / Master",        "laurea_m"},
    };
    for (const auto& l : livelli)
        m_filtroLivello->addItem(l.label, QString(l.data));
    m_filtroLivello->setCurrentIndex(2);  // default: Diploma — profilo di Paolo
    filtriL->addWidget(m_filtroLivello);

    auto* filtriBtn = new QPushButton("\xf0\x9f\x94\x84", filtriRow);
    filtriBtn->setObjectName("actionBtn");
    filtriBtn->setFixedWidth(36);
    filtriBtn->setToolTip("Aggiorna filtri");
    connect(filtriBtn, &QPushButton::clicked, this, &LavoroPage::applicaFiltri);
    connect(m_filtroTipo,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LavoroPage::applicaFiltri);
    connect(m_filtroLivello, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LavoroPage::applicaFiltri);
    filtriL->addWidget(filtriBtn);
    filtriL->addStretch(1);

    auto* legenda = new QLabel(
        "\xf0\x9f\x9f\xa2" " nessun titolo   "
        "\xf0\x9f\x9f\xa1" " diploma   "
        "\xf0\x9f\x9f\xa0" " laurea   "
        "\xf0\x9f\x94\xb4" " master   "
        "\xe2\x9c\x89" " email diretta", filtriRow);
    legenda->setObjectName("pageSubtitle");
    filtriL->addWidget(legenda);
    lay->addWidget(filtriRow);

    /* ── Splitter: lista | output AI ── */
    auto* splitter = new QSplitter(Qt::Vertical, this);

    auto* topPane = new QWidget(splitter);
    auto* topLay  = new QVBoxLayout(topPane);
    topLay->setContentsMargins(0,0,0,0); topLay->setSpacing(4);

    m_offerteLista = new QListWidget(topPane);
    m_offerteLista->setObjectName("offerteList");
    m_offerteLista->setWordWrap(true);
    m_offerteLista->setAlternatingRowColors(true);
    // Rimuoviamo colori hardcoded: tutto controllato dal tema QSS via #offerteList
    topLay->addWidget(m_offerteLista, 1);

    auto* azioniRow = new QWidget(topPane);
    auto* azioniL   = new QHBoxLayout(azioniRow);
    azioniL->setContentsMargins(0,4,0,0); azioniL->setSpacing(8);

    auto* genBtn   = new QPushButton("\xf0\x9f\xa4\x96 Genera Lettera con AI", azioniRow);
    genBtn->setObjectName("actionBtn");
    auto* emailBtn = new QPushButton("\xe2\x9c\x89 Copia Email", azioniRow);
    emailBtn->setObjectName("actionBtn");
    emailBtn->setEnabled(false);
    emailBtn->setToolTip("Disponibile solo per offerte con email diretta");
    auto* copiaBtn = new QPushButton("\xf0\x9f\x93\x8b Copia Lettera", azioniRow);
    copiaBtn->setObjectName("actionBtn");
    copiaBtn->setEnabled(false);
    auto* selLbl = new QLabel("Seleziona un'offerta (doppio clic = genera subito)", azioniRow);
    selLbl->setObjectName("pageSubtitle");
    azioniL->addWidget(genBtn); azioniL->addWidget(emailBtn); azioniL->addWidget(copiaBtn);
    azioniL->addWidget(selLbl, 1);
    topLay->addWidget(azioniRow);
    splitter->addWidget(topPane);

    auto* botPane = new QWidget(splitter);
    auto* botLay  = new QVBoxLayout(botPane);
    botLay->setContentsMargins(0,0,0,0); botLay->setSpacing(6);

    m_lavoroLog = new QTextEdit(botPane);
    m_lavoroLog->setObjectName("chatLog");
    m_lavoroLog->setReadOnly(true);
    m_lavoroLog->setPlaceholderText(
        "Seleziona un'offerta e clicca \"Genera Lettera con AI\".\n"
        "Metodo Socratico attivo: l'AI evidenzier\xc3\xa0 anche eventuali lacune del profilo.");
    m_lavoroLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_lavoroLog, &QTextEdit::customContextMenuRequested, m_lavoroLog, [this](const QPoint& pos){
        const QString sel = m_lavoroLog->textCursor().selectedText();
        const bool hasSel = !sel.isEmpty();
        QMenu menu(m_lavoroLog);
        QAction* actCopy = menu.addAction("\xf0\x9f\x97\x82  Copia " + QString(hasSel ? "selezione" : "tutto"));
        QAction* actRead = menu.addAction("\xf0\x9f\x8e\x99  Leggi " + QString(hasSel ? "selezione" : "tutto"));
        QAction* chosen  = menu.exec(m_lavoroLog->mapToGlobal(pos));
        const QString txt = hasSel ? sel : m_lavoroLog->toPlainText();
        if (chosen == actCopy) QGuiApplication::clipboard()->setText(txt);
        else if (chosen == actRead) {
            QStringList words = txt.split(' ', Qt::SkipEmptyParts);
            if (words.size() > 400) words = words.mid(words.size() - 400);
            QProcess::startDetached("espeak-ng", {"-v", "it+f3", "--punct=none", words.join(" ")});
        }
    });
    botLay->addWidget(m_lavoroLog, 1);

    auto* waitLbl = new QLabel("\xe2\x8f\xb3  AI in elaborazione...", botPane);
    waitLbl->setStyleSheet("color:#E5C400; font-style:italic; padding:2px 0;");
    waitLbl->setVisible(false);
    botLay->addWidget(waitLbl);

    auto* inputRow = new QWidget(botPane);
    auto* inL      = new QHBoxLayout(inputRow);
    inL->setContentsMargins(0,0,0,0); inL->setSpacing(8);
    auto* inp = new QLineEdit(inputRow);
    inp->setObjectName("chatInput");
    inp->setPlaceholderText("Follow-up: 'Cosa manca al mio profilo?' — 'Come miglioro la lettera?' — 'Punti deboli della candidatura?'");
    inp->setFixedHeight(38);
    auto* send = new QPushButton("Invia \xe2\x96\xb6", inputRow);
    send->setObjectName("actionBtn");
    auto* stopBtn = new QPushButton("\xe2\x8f\xb9", inputRow);
    stopBtn->setObjectName("actionBtn");
    stopBtn->setProperty("danger", true);
    stopBtn->setFixedWidth(40);
    stopBtn->setEnabled(false);
    inL->addWidget(inp, 1); inL->addWidget(send); inL->addWidget(stopBtn);
    botLay->addWidget(inputRow);
    splitter->addWidget(botPane);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    lay->addWidget(splitter, 1);

    /* ── Logica ── */
    auto active = std::make_shared<bool>(false);

    // Profilo CV di fallback — Paolo Lo Bello (parametri di default)
    const QString cvFallback =
        "Paolo Lo Bello, nato il 15/02/1989, Catania (36 anni).\n"
        "Email: wildlux@gmail.com | Tel: +39 340 96 25 057\n"
        "Patente B Europea. Dislessico (certificato ASL Catania).\n\n"
        "TITOLO DI STUDIO:\n"
        "- Perito Informatico — ITIS G. Marconi, Catania (2003-2010) — voto 67/100 — MQRF Level 4\n"
        "- CCNA Cisco Certificate Associate — ICE Malta (2023-2024)\n"
        "- Certificato E-Commerce (Joomla, PHP, HTML, Web Marketing) — CESIS (2010-2012)\n"
        "- Certificato Photoshop CS5 — ITIS Galileo Ferraris Acireale (2012)\n"
        "- Inglese A2 (ELA Malta, 2019)\n\n"
        "ESPERIENZE LAVORATIVE:\n"
        "- Lidl LTD Malta (lug 2024): cassa POS, muletto elettrico, controllo stock e date\n"
        "- Scott Supermarket Malta (apr 2020 - mag 2024): muletto manuale, facing product, ricollocamento\n"
        "- Playmobil/Poultons Ltd Malta (dic 2019 - feb 2020): operatore macchina, controllo qualita'\n"
        "- Convenience Shop Malta (giu-ago 2019): assistente negozio, cassa, scaffali\n"
        "- Karma Swim Catania (2014-2015): grafico freelance — Adobe Illustrator\n"
        "- Techno Work srl Catania (nov 2013): Python developer su Raspberry Pi 3, GNU/Linux\n"
        "- Almaviva Misterbianco (gen-feb 2012): call center Mediaset Premium\n"
        "- Mics SRL Misterbianco (giu-lug 2011): inbound call operator Enel Energia\n"
        "- Gio' Casa Misterbianco (ago 2005): assistenza e vendita condizionatori\n\n"
        "COMPETENZE TECNICHE:\n"
        "- Reti: CCNA Cisco, SSH, Kleopatra, PuTTY, FileZilla\n"
        "- Sviluppo: Python, C++, HTML, PHP, SQL, MySQL, JavaScript, Node.js, Assembler x86\n"
        "- OS: GNU/Linux, macOS, Windows\n"
        "- Web: WordPress, Joomla, Prestashop, Django\n"
        "- Grafica: Adobe Photoshop CS5, GIMP, Adobe Illustrator\n"
        "- 3D: Blender (2007-oggi) — mesh, rigging, rendering, video promozionali\n"
        "- Virtual: VirtualBox, Docker\n"
        "- Office: LibreOffice, Microsoft Office, gestione email";

    // Prompt Socratico comune — applicato a tutte le richieste AI di questa sezione
    const QString socraticoBase =
        "\n\nMETODOLOGIA SOCRATICA: Sii rigorosamente onesto. "
        "Non adulare il candidato. Se il profilo presenta lacune rispetto ai requisiti, "
        "indicale chiaramente. Proponi domande critiche che aiutino a migliorare la candidatura. "
        "Distingui tra punti di forza reali e affermazioni non verificabili. "
        "L'obiettivo e' la verita', non la compiacenza.";

    connect(m_offerteLista, &QListWidget::currentItemChanged, this,
        [=](QListWidgetItem* cur, QListWidgetItem*){
            if (!cur) { selLbl->setText("Seleziona un'offerta dalla lista"); emailBtn->setEnabled(false); return; }
            const auto o = cur->data(Qt::UserRole).value<Offerta>();
            selLbl->setText(o.azienda + " \xe2\x80\x94 " + o.ruolo);
            emailBtn->setEnabled(!o.email.isEmpty());
            if (!o.email.isEmpty()) emailBtn->setToolTip("\xf0\x9f\x93\x8b Copia: " + o.email);
        });

    connect(emailBtn, &QPushButton::clicked, this, [=]{
        auto* cur = m_offerteLista->currentItem();
        if (!cur) return;
        const auto o = cur->data(Qt::UserRole).value<Offerta>();
        if (!o.email.isEmpty()) QGuiApplication::clipboard()->setText(o.email);
    });
    connect(m_lavoroLog, &QTextEdit::textChanged, this, [=]{
        copiaBtn->setEnabled(!m_lavoroLog->toPlainText().trimmed().isEmpty());
    });
    connect(copiaBtn, &QPushButton::clicked, this, [this]{
        QGuiApplication::clipboard()->setText(m_lavoroLog->toPlainText());
    });

    auto genLettera = [=]{
        auto* cur = m_offerteLista->currentItem();
        if (!cur) { m_lavoroLog->append("\xe2\x9a\xa0  Seleziona prima un'offerta."); return; }
        const auto o = cur->data(Qt::UserRole).value<Offerta>();
        const QString cvInfo  = m_cvText.isEmpty() ? cvFallback : m_cvText.left(3500);
        const QString modello = m_cmbModello ? m_cmbModello->currentText() : m_ai->model();

        // Applica il modello selezionato
        if (!modello.isEmpty() && !modello.startsWith("\xf0\x9f\x94\x84"))
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modello);

        QString emailHint;
        if (!o.email.isEmpty())
            emailHint = QString("\n\nNOTA: L'email di candidatura e': %1").arg(o.email);

        const QString sys = QString(
            "Sei un esperto di risorse umane e scrittura professionale italiana.\n"
            "Scrivi una lettera di candidatura formale, concisa e personalizzata.\n\n"
            "=== CURRICULUM VITAE DEL CANDIDATO ===\n%1\n\n"
            "=== OFFERTA DI LAVORO ===\n"
            "Azienda: %2\nRuolo: %3\nSede: %4\nRequisiti: %5%6\n\n"
            "=== ISTRUZIONI ===\n"
            "- Tono professionale e diretto\n"
            "- Evidenzia SOLO le competenze rilevanti per QUESTO ruolo specifico\n"
            "- Massimo 280 parole\n"
            "- Inizia con 'Gentile Ufficio Risorse Umane di %2,'\n"
            "- Termina con disponibilit\xc3\xa0 per colloquio\n"
            "- Scrivi SOLO in italiano\n"
            "- Non inventare informazioni non presenti nel CV%7"
        ).arg(cvInfo, o.azienda, o.ruolo, o.sede, o.requisiti, emailHint, socraticoBase);

        // Header di output: TAG identifica chiaramente la sorgente
        m_lavoroLog->clear();
        m_lavoroLog->append(QString(
            "\xf0\x9f\x92\xbc  [CERCA LAVORO \xe2\x86\x92 Genera Lettera]\n"
            "\xf0\x9f\xa4\x96  Modello: %1\n"
            "\xf0\x9f\x8f\xa2  Azienda: %2 \xe2\x80\x94 %3\n"
            "\xf0\x9f\x93\x8d  Sede: %4\n").arg(modello, o.azienda, o.ruolo, o.sede));
        if (!o.email.isEmpty())
            m_lavoroLog->append(QString("\xe2\x9c\x89  Email destinatario: %1\n").arg(o.email));
        m_lavoroLog->append("\n\xf0\x9f\x93\x9d  Lettera:\n");

        send->setEnabled(false); stopBtn->setEnabled(true); waitLbl->setVisible(true);
        *active = true;
        m_ai->chat(sys, QString("Genera la lettera di candidatura per il ruolo di %1 presso %2.")
                        .arg(o.ruolo, o.azienda));
    };

    connect(genBtn, &QPushButton::clicked, this, genLettera);
    connect(m_offerteLista, &QListWidget::itemDoubleClicked, this, [=](QListWidgetItem*){ genLettera(); });

    auto sendFn = [=]{
        const QString msg = inp->text().trimmed();
        if (msg.isEmpty()) return;
        const QString cvInfo  = m_cvText.isEmpty() ? cvFallback.left(1500) : m_cvText.left(2000);
        const QString modello = m_cmbModello ? m_cmbModello->currentText() : m_ai->model();

        if (!modello.isEmpty() && !modello.startsWith("\xf0\x9f\x94\x84"))
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modello);

        const QString sys = QString(
            "Sei un career coach italiano esperto in ricerca del lavoro, CV e colloqui.\n"
            "Il candidato ha questo profilo:\n%1\n"
            "Fornisci consigli concreti e pratici. Rispondi SEMPRE in italiano.%2"
        ).arg(cvInfo, socraticoBase);

        // TAG: identifica la sorgente della richiesta
        m_lavoroLog->append(QString("\n\xf0\x9f\x92\xbc [CERCA LAVORO \xe2\x86\x92 %1] \xf0\x9f\xa4\x96 %2\n")
                                .arg(msg.left(40), modello));
        m_lavoroLog->append("\xf0\x9f\xa4\x96  AI: ");
        inp->clear();
        send->setEnabled(false); stopBtn->setEnabled(true); waitLbl->setVisible(true);
        *active = true;
        m_ai->chat(sys, msg);
    };
    connect(send, &QPushButton::clicked, this, sendFn);
    connect(inp,  &QLineEdit::returnPressed, this, sendFn);
    connect(stopBtn, &QPushButton::clicked, m_ai, &AiClient::abort);

    // Connessioni AI: usano `active` per ignorare segnali da altri tab (AiClient dedicato)
    connect(m_ai, &AiClient::token, this, [=](const QString& t){
        if (!*active) return;
        QTextCursor c(m_lavoroLog->document()); c.movePosition(QTextCursor::End);
        c.insertText(t); m_lavoroLog->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, this, [=](const QString&){
        if (!*active) return;
        *active = false;
        m_lavoroLog->append("\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
        send->setEnabled(true); stopBtn->setEnabled(false); waitLbl->setVisible(false);
    });
    connect(m_ai, &AiClient::error, this, [=](const QString& err){
        if (!*active) return;
        *active = false;
        const QString el = err.toLower();
        if (!el.contains("canceled") && !el.contains("operation canceled"))
            m_lavoroLog->append(QString("\n\xe2\x9d\x8c  [CERCA LAVORO] Errore: %1").arg(err));
        send->setEnabled(true); stopBtn->setEnabled(false); waitLbl->setVisible(false);
    });
    connect(m_ai, &AiClient::aborted, this, [=]{
        if (!*active) return;
        *active = false;
        m_lavoroLog->append("\n\xe2\x8f\xb9  Interrotto.\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
        send->setEnabled(true); stopBtn->setEnabled(false); waitLbl->setVisible(false);
    });

    applicaFiltri();
}

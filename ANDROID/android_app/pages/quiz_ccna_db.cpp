#include "quiz_ccna_db.h"
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

#ifdef HAVE_SQL
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#endif

/* ── Nome connessione univoco per istanza ── */
static int s_connCounter = 0;

QuizCcnaDb::QuizCcnaDb()
    : m_connName(QString("QuizDb_%1").arg(++s_connCounter))
{
    initDb();
}

QuizCcnaDb::~QuizCcnaDb()
{
#ifdef HAVE_SQL
    if (QSqlDatabase::contains(m_connName)) {
        QSqlDatabase::database(m_connName).close();
        QSqlDatabase::removeDatabase(m_connName);
    }
#endif
}

/* ══════════════════════════════════════════════════════════════
   initDb — apre/crea il database SQLite e popola se vuoto
   ══════════════════════════════════════════════════════════════ */
void QuizCcnaDb::initDb()
{
#ifdef HAVE_SQL
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString dbPath = dir + "/quiz_ccna.db";

    auto db = QSqlDatabase::addDatabase("QSQLITE", m_connName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << "QuizCcnaDb: cannot open" << dbPath << db.lastError().text();
        return;
    }

    QSqlQuery q(db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec(
        "CREATE TABLE IF NOT EXISTS questions ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  domanda    TEXT    NOT NULL,"
        "  r0         TEXT,"
        "  r1         TEXT,"
        "  r2         TEXT,"
        "  r3         TEXT,"
        "  corretta   INTEGER DEFAULT 0,"
        "  spiegazione TEXT,"
        "  tema       TEXT"
        ")");

    q.exec("SELECT COUNT(*) FROM questions");
    if (q.next() && q.value(0).toInt() == 0)
        populate();

    q.exec("SELECT COUNT(*) FROM questions");
    if (q.next()) m_count = q.value(0).toInt();
#else
    /* Fallback senza Qt6::Sql: conta 0, le query restituiranno QuizQuestion{} */
    m_count = 0;
#endif
}

/* ── Helper: costruisce QuizQuestion dai campi grezzi ── */
QuizQuestion QuizCcnaDb::rowToQuestion(
    const QString& dom,
    const QString& r0, const QString& r1,
    const QString& r2, const QString& r3,
    int corretta, const QString& spiega, const QString& tema) const
{
    QuizQuestion q;
    q.domanda      = dom;
    q.risposte[0]  = r0;
    q.risposte[1]  = r1;
    q.risposte[2]  = r2;
    q.risposte[3]  = r3;
    q.corretta     = (corretta >= 0 && corretta < 4) ? corretta : 0;
    q.spiegazione  = spiega;
    q.tema         = tema;
    return q;
}

/* ══════════════════════════════════════════════════════════════
   Lettura domande
   ══════════════════════════════════════════════════════════════ */
QuizQuestion QuizCcnaDb::randomQuestion() const
{
#ifdef HAVE_SQL
    auto db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    q.exec("SELECT domanda,r0,r1,r2,r3,corretta,spiegazione,tema "
           "FROM questions ORDER BY RANDOM() LIMIT 1");
    if (q.next())
        return rowToQuestion(q.value(0).toString(), q.value(1).toString(),
                             q.value(2).toString(), q.value(3).toString(),
                             q.value(4).toString(), q.value(5).toInt(),
                             q.value(6).toString(), q.value(7).toString());
#endif
    return {};
}

QuizQuestion QuizCcnaDb::randomByTema(const QString& tema) const
{
#ifdef HAVE_SQL
    auto db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    q.prepare("SELECT domanda,r0,r1,r2,r3,corretta,spiegazione,tema "
              "FROM questions WHERE tema=? ORDER BY RANDOM() LIMIT 1");
    q.addBindValue(tema);
    q.exec();
    if (q.next())
        return rowToQuestion(q.value(0).toString(), q.value(1).toString(),
                             q.value(2).toString(), q.value(3).toString(),
                             q.value(4).toString(), q.value(5).toInt(),
                             q.value(6).toString(), q.value(7).toString());
#endif
    return randomQuestion();
}

QStringList QuizCcnaDb::temi() const
{
    QStringList lst;
#ifdef HAVE_SQL
    auto db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    q.exec("SELECT DISTINCT tema FROM questions ORDER BY tema");
    while (q.next()) lst << q.value(0).toString();
#endif
    return lst;
}

/* ══════════════════════════════════════════════════════════════
   populate — inserisce le domande CCNA 200-301 nel DB SQLite
   ══════════════════════════════════════════════════════════════ */
void QuizCcnaDb::populate()
{
#ifdef HAVE_SQL
    auto db = QSqlDatabase::database(m_connName);
    db.transaction();
    QSqlQuery q(db);
    q.prepare("INSERT INTO questions (domanda,r0,r1,r2,r3,corretta,spiegazione,tema) "
              "VALUES (?,?,?,?,?,?,?,?)");

    struct Row {
        const char* dom;
        const char* r[4];
        int corr;
        const char* spiega;
        const char* tema;
    };

    static const Row kData[] = {
        /* ── OSI/TCP-IP ── */
        { "Quanti livelli ha il modello OSI?",
          {"5","6","7","8"}, 2,
          "Il modello OSI ha 7 livelli: Fisico, Data Link, Rete, Trasporto, Sessione, Presentazione, Applicazione.",
          "OSI/TCP-IP" },
        { "A quale livello OSI opera uno switch non gestito?",
          {"Livello 1 (Fisico)","Livello 2 (Data Link)","Livello 3 (Rete)","Livello 4 (Trasporto)"}, 1,
          "Gli switch operano a livello 2 (Data Link) usando gli indirizzi MAC per inoltrare i frame.",
          "OSI/TCP-IP" },
        { "Quale protocollo del livello 4 garantisce la consegna affidabile?",
          {"UDP","IP","TCP","ICMP"}, 2,
          "TCP usa handshake a 3 vie, acknowledgement e ritrasmissione per garantire la consegna affidabile.",
          "OSI/TCP-IP" },
        { "Cosa fa il protocollo ARP?",
          {"Risolve nomi di dominio in IP","Traduce IP in MAC address","Assegna IP dinamicamente","Cifra le comunicazioni"}, 1,
          "ARP (Address Resolution Protocol) converte un indirizzo IP in un indirizzo MAC nella stessa rete locale.",
          "OSI/TCP-IP" },
        { "Quale numero di porta usa HTTP?",
          {"21","22","80","443"}, 2,
          "HTTP usa la porta TCP 80. HTTPS usa la 443, SSH la 22, FTP la 21.",
          "OSI/TCP-IP" },
        { "Quale numero di porta usa HTTPS?",
          {"80","8080","443","8443"}, 2,
          "HTTPS usa la porta TCP 443 per comunicazioni cifrate con TLS/SSL.",
          "OSI/TCP-IP" },
        { "Quale protocollo usa la porta 53?",
          {"DHCP","DNS","SNMP","NTP"}, 1,
          "DNS usa la porta 53 sia UDP che TCP per risolvere i nomi di dominio.",
          "OSI/TCP-IP" },
        { "A quale livello OSI opera un router?",
          {"Livello 1","Livello 2","Livello 3","Livello 4"}, 2,
          "I router operano a livello 3 (Rete) usando gli indirizzi IP per instradare i pacchetti.",
          "OSI/TCP-IP" },
        { "Cos'e' la PDU al livello 2 (Data Link) OSI?",
          {"Pacchetto","Frame","Segmento","Bit"}, 1,
          "A livello 2 la PDU si chiama Frame. A livello 3 e' Pacchetto, livello 4 e' Segmento, livello 1 sono Bit.",
          "OSI/TCP-IP" },
        { "Quale protocollo usa la porta UDP 67/68?",
          {"DNS","SNMP","DHCP","TFTP"}, 2,
          "DHCP usa le porte UDP 67 (server) e 68 (client) per assegnare indirizzi IP dinamicamente.",
          "OSI/TCP-IP" },
        { "Quale protocollo usa la porta TCP 22?",
          {"Telnet","FTP","SSH","SMTP"}, 2,
          "SSH usa la porta TCP 22 per connessioni sicure e cifrate ai dispositivi di rete.",
          "OSI/TCP-IP" },
        { "Cosa trasmette la PDU al livello 1 (Fisico)?",
          {"Frame","Pacchetti","Segmenti","Bit"}, 3,
          "Il livello Fisico trasmette bit come segnali elettrici, ottici o radio sul mezzo trasmissivo.",
          "OSI/TCP-IP" },
        { "Quale protocollo usa la porta 25?",
          {"POP3","IMAP","SMTP","HTTP"}, 2,
          "SMTP usa la porta TCP 25 per l'invio di email tra server di posta.",
          "OSI/TCP-IP" },
        { "Quale protocollo usa la porta TCP 443?",
          {"HTTP","FTP","HTTPS","SSH"}, 2,
          "HTTPS (HTTP Secure) usa la porta 443 per trasmissione cifrata con TLS/SSL.",
          "OSI/TCP-IP" },

        /* ── Switching/VLAN ── */
        { "Cosa fa una VLAN?",
          {"Aumenta la velocita' della rete","Segmenta la rete in domini di broadcast separati","Cifra il traffico","Assegna IP automaticamente"}, 1,
          "Le VLAN separano il traffico in domini di broadcast logicamente distinti su switch fisici condivisi.",
          "Switching/VLAN" },
        { "Cosa significa 'trunk port' su uno switch Cisco?",
          {"Porta per una sola VLAN","Porta multi-VLAN con tag 802.1Q","Porta uplink ad alta velocita'","Porta riservata al management"}, 1,
          "Una trunk port usa IEEE 802.1Q per taggare i frame di ogni VLAN, permettendo traffico multi-VLAN su un singolo link.",
          "Switching/VLAN" },
        { "Qual e' il comando Cisco IOS per creare la VLAN 10?",
          {"vlan add 10","vlan create 10","vlan 10","create vlan 10"}, 2,
          "Il comando 'vlan 10' in modalita' global configuration crea la VLAN 10 sul database dello switch.",
          "Switching/VLAN" },
        { "Cosa fa il protocollo STP (Spanning Tree)?",
          {"Aumenta la larghezza di banda","Elimina i loop di livello 2","Bilancia il carico tra router","Gestisce le VLAN"}, 1,
          "STP previene i loop di livello 2 bloccando le porte ridondanti e mantenendo un unico percorso attivo.",
          "Switching/VLAN" },
        { "Quale porta diventa Root Port in STP?",
          {"La porta con costo piu' basso verso il Root Bridge","La porta piu' veloce","La porta configurata manualmente","La prima porta numericamente"}, 0,
          "La Root Port e' la porta con il costo STP piu' basso verso il Root Bridge (lo switch con Bridge ID piu' basso).",
          "Switching/VLAN" },
        { "Cos'e' l'EtherChannel?",
          {"Un protocollo di routing","Raggruppamento logico di piu' link fisici","Una tecnologia di cifratura","Un protocollo VLAN"}, 1,
          "EtherChannel aggrega 2-8 link fisici in un bundle logico, aumentando la banda e fornendo ridondanza.",
          "Switching/VLAN" },
        { "Quale comando mostra la tabella MAC di uno switch Cisco?",
          {"show mac-address-table","show arp","show interfaces","show running-config"}, 0,
          "'show mac-address-table' visualizza la tabella CAM con gli indirizzi MAC associati alle porte.",
          "Switching/VLAN" },
        { "Qual e' la Native VLAN predefinita su Cisco?",
          {"VLAN 0","VLAN 1","VLAN 99","VLAN 1000"}, 1,
          "La Native VLAN predefinita su Cisco e' la VLAN 1. Il suo traffico non viene taggato sulle trunk port.",
          "Switching/VLAN" },
        { "Qual e' la versione migliorata di STP raccomandata da Cisco?",
          {"RSTP (802.1w)","MSTP","PVST+","Rapid PVST+"}, 3,
          "Cisco raccomanda Rapid PVST+ (Rapid Per-VLAN Spanning Tree+) che combina RSTP con una istanza STP per VLAN.",
          "Switching/VLAN" },

        /* ── Routing ── */
        { "Quale comando verifica il routing su un router Cisco?",
          {"show arp","show ip route","show interfaces","show vlan"}, 1,
          "'show ip route' visualizza la tabella di routing con tutte le reti note e il next-hop.",
          "Routing" },
        { "Cosa significa AD (Administrative Distance)?",
          {"Velocita' del link in Mbps","Affidabilita' della sorgente di routing (piu' bassa = piu' preferita)","Numero di hop","Costo del percorso OSPF"}, 1,
          "L'AD indica l'affidabilita' della sorgente: 0=connected, 1=static, 90=EIGRP, 110=OSPF, 120=RIP.",
          "Routing" },
        { "Qual e' l'AD di OSPF?",
          {"90","100","110","120"}, 2,
          "L'AD di OSPF e' 110. Per confronto: EIGRP=90, RIP=120, BGP eBGP=20.",
          "Routing" },
        { "Cosa indica la lettera 'O' nella tabella di routing Cisco?",
          {"Rotta statica","Rotta OSPF","Rotta direttamente connessa","Rotta EIGRP"}, 1,
          "La lettera 'O' indica una rotta appresa tramite OSPF (Open Shortest Path First).",
          "Routing" },
        { "Quale metrica usa OSPF?",
          {"Hop count","Bandwidth","Cost (basato sulla bandwidth)","Delay"}, 2,
          "OSPF usa il Cost come metrica, calcolato come 10^8 diviso per la bandwidth del link in bps.",
          "Routing" },
        { "Come si configura il default route su Cisco?",
          {"ip route default 0.0.0.0","ip route 0.0.0.0 0.0.0.0 [next-hop]","default-route [next-hop]","ip gateway 0.0.0.0"}, 1,
          "'ip route 0.0.0.0 0.0.0.0 [next-hop o interfaccia]' configura il default route in Cisco IOS.",
          "Routing" },
        { "Qual e' il comando per abilitare OSPF su un router Cisco?",
          {"ospf enable","router ospf [process-id]","enable ospf","start ospf"}, 1,
          "'router ospf [process-id]' attiva il processo OSPF. Il process-id e' locale al router (1-65535).",
          "Routing" },
        { "Quale indirizzo multicast usa OSPF per i messaggi Hello?",
          {"224.0.0.1","224.0.0.5","224.0.0.9","255.255.255.255"}, 1,
          "OSPF usa 224.0.0.5 (AllSPFRouters) per i messaggi Hello e 224.0.0.6 (AllDRRouters) per altri messaggi.",
          "Routing" },
        { "Quale lettera indica una rotta statica nella tabella Cisco?",
          {"C","O","S","R"}, 2,
          "La lettera 'S' indica una rotta statica. 'C'=connected, 'O'=OSPF, 'R'=RIP.",
          "Routing" },

        /* ── IPv4/IPv6 ── */
        { "Quanti bit ha un indirizzo IPv4?",
          {"16","32","64","128"}, 1,
          "Un indirizzo IPv4 e' composto da 32 bit, suddivisi in 4 ottetti (es. 192.168.1.1).",
          "IPv4/IPv6" },
        { "Qual e' la subnet mask per /24?",
          {"255.255.0.0","255.255.255.0","255.255.255.128","255.0.0.0"}, 1,
          "Il prefisso /24 ha 24 bit di rete: subnet mask 255.255.255.0, 254 host usabili.",
          "IPv4/IPv6" },
        { "Quanti host usabili ha una subnet /30?",
          {"2","4","6","8"}, 0,
          "Una /30 ha 2^2=4 indirizzi totali: 1 network, 1 broadcast, 2 host usabili. Usata per link punto-punto.",
          "IPv4/IPv6" },
        { "Quale range di indirizzi e' privato secondo RFC 1918?",
          {"10.0.0.0 - 10.255.255.255","172.32.0.0 - 172.47.255.255","192.169.0.0/16","169.254.0.0/16"}, 0,
          "RFC 1918: 10.0.0.0/8, 172.16.0.0/12 (172.16-31.x.x), 192.168.0.0/16 sono privati.",
          "IPv4/IPv6" },
        { "Quanti bit ha un indirizzo IPv6?",
          {"32","64","96","128"}, 3,
          "IPv6 usa indirizzi da 128 bit, scritti come 8 gruppi di 4 cifre esadecimali (es. 2001:db8::1).",
          "IPv4/IPv6" },
        { "Cosa significa il prefisso IPv6 fe80::/10?",
          {"Indirizzo globale unicast","Indirizzo link-local","Indirizzo multicast","Indirizzo loopback"}, 1,
          "Gli indirizzi fe80::/10 sono link-local: usati solo nella rete locale, non instradabili tra router.",
          "IPv4/IPv6" },
        { "Quale protocollo sostituisce ARP in IPv6?",
          {"DHCPv6","NDP (Neighbor Discovery Protocol)","ICMPv6 solo","RIPng"}, 1,
          "NDP usa messaggi ICMPv6 per la risoluzione degli indirizzi, sostituendo ARP in IPv6.",
          "IPv4/IPv6" },
        { "Quanti host usabili ha una rete /25?",
          {"64","126","128","254"}, 1,
          "Una /25 ha 2^7=128 indirizzi totali: -1 network -1 broadcast = 126 host usabili.",
          "IPv4/IPv6" },
        { "Qual e' l'indirizzo di loopback IPv4?",
          {"0.0.0.0","127.0.0.1","192.168.0.1","255.255.255.255"}, 1,
          "127.0.0.1 e' l'indirizzo di loopback IPv4, usato per testare lo stack di rete locale.",
          "IPv4/IPv6" },
        { "Quale subnet /27 contiene quanti host usabili?",
          {"16","30","32","62"}, 1,
          "Una /27 ha 2^5=32 indirizzi, di cui 30 usabili (1 network + 1 broadcast esclusi).",
          "IPv4/IPv6" },

        /* ── Sicurezza ── */
        { "Quale comando abilita SSH su un router Cisco?",
          {"enable ssh","crypto key generate rsa","ip ssh enable","service ssh start"}, 1,
          "'crypto key generate rsa' genera la chiave RSA per SSH. Richiede anche 'ip ssh version 2' e hostname.",
          "Sicurezza" },
        { "Cosa fa una ACL standard su Cisco?",
          {"Filtra IP sorgente e destinazione","Filtra solo sull'IP sorgente","Gestisce le VLAN","Cifra le comunicazioni"}, 1,
          "Le ACL standard (1-99, 1300-1999) filtrano solo in base all'IP sorgente.",
          "Sicurezza" },
        { "Dove si applica una ACL in-bound per bloccare traffico in entrata?",
          {"Sull'interfaccia di uscita","Sull'interfaccia di ingresso","Sul gateway predefinito","Sul trunk port"}, 1,
          "Un'ACL 'in' viene applicata sull'interfaccia dalla quale entra il traffico.",
          "Sicurezza" },
        { "Cosa fa il Port Security su Cisco?",
          {"Abilita SSH sulle porte","Limita i MAC address su una porta switch","Configura VLAN di sicurezza","Abilita crittografia sul link"}, 1,
          "Port Security limita il numero di MAC address consentiti su una porta, proteggendo da MAC flooding.",
          "Sicurezza" },
        { "Quale versione SSH e' raccomandata?",
          {"SSH v1","SSH v2","SSH v3","Telnet cifrato"}, 1,
          "SSH v2 (configurata con 'ip ssh version 2') e' piu' sicura della v1 e va sempre preferita.",
          "Sicurezza" },
        { "Cosa fa il comando 'no shutdown' su un'interfaccia Cisco?",
          {"Riavvia l'interfaccia","Abilita e attiva l'interfaccia","Disabilita l'interfaccia","Resetta le impostazioni"}, 1,
          "Le interfacce Cisco sono in 'shutdown' per default. 'no shutdown' le attiva.",
          "Sicurezza" },
        { "Quale tipo di ACL usa numeri 100-199?",
          {"Standard","Estesa","Named","Dinamica"}, 1,
          "Le ACL estese usano numeri 100-199 (o 2000-2699) e filtrano per IP sorgente, destinazione, protocollo e porta.",
          "Sicurezza" },

        /* ── DHCP/NAT ── */
        { "Cosa fa il DHCP?",
          {"Risolve nomi di dominio","Assegna indirizzi IP automaticamente ai client","Protegge da intrusioni","Gestisce il routing"}, 1,
          "DHCP assegna automaticamente IP, subnet mask, gateway e DNS ai dispositivi client.",
          "DHCP/NAT" },
        { "Quale tipo di NAT mappa un IP privato in un IP pubblico fisso (1:1)?",
          {"NAT Overload (PAT)","NAT Statico","NAT Dinamico","NAT64"}, 1,
          "Il NAT Statico crea una mappatura 1:1 permanente tra un IP privato e un IP pubblico.",
          "DHCP/NAT" },
        { "Cosa significa PAT (Port Address Translation)?",
          {"Molti IP privati a molti IP pubblici","Molti IP privati a un IP pubblico con porte diverse","Un IP privato a un IP pubblico fisso","Traduzione indirizzi a livello 2"}, 1,
          "PAT (NAT Overload) permette a molti host di condividere un unico IP pubblico differenziandoli per porta.",
          "DHCP/NAT" },
        { "Quale comando verifica le traduzioni NAT attive su Cisco?",
          {"show nat translations","show ip nat translations","show nat table","show ip nat active"}, 1,
          "'show ip nat translations' visualizza la tabella NAT con le mappature IP:porta attive.",
          "DHCP/NAT" },
        { "Qual e' il processo DORA del DHCP?",
          {"Discover-Offer-Request-Acknowledge","Data-Order-Reply-Accept","Detect-Open-Read-Assign","Default-Option-Reserve-Assign"}, 0,
          "Il processo DHCP e' Discover (client broadcast), Offer (server offre IP), Request (client accetta), Acknowledge (server conferma).",
          "DHCP/NAT" },

        /* ── Wireless ── */
        { "Quale standard wireless opera esclusivamente a 5 GHz?",
          {"802.11b","802.11g","802.11a","802.11n"}, 2,
          "802.11a opera solo a 5 GHz (fino a 54 Mbps). 802.11b/g operano a 2.4 GHz. 802.11n supporta entrambe.",
          "Wireless" },
        { "Qual e' la banda di frequenza del Wi-Fi 6 (802.11ax)?",
          {"Solo 2.4 GHz","Solo 5 GHz","2.4 GHz e 5 GHz","6 GHz solo"}, 2,
          "802.11ax (Wi-Fi 6) opera su 2.4 GHz e 5 GHz. Wi-Fi 6E aggiunge la banda a 6 GHz.",
          "Wireless" },
        { "Cosa fa un Wireless LAN Controller (WLC)?",
          {"Fornisce connettivita' Internet","Gestisce centralmente gli Access Point","Instrada traffico wireless tra VLAN","Cifra il traffico Wi-Fi"}, 1,
          "Il WLC gestisce centralmente gli AP in modalita' Lightweight, per configurazione e monitoraggio unificati.",
          "Wireless" },
        { "Quale protocollo di sicurezza Wi-Fi e' attualmente raccomandato?",
          {"WEP","WPA","WPA2","WPA3"}, 3,
          "WPA3 e' lo standard piu' sicuro. WEP e WPA sono obsoleti e vulnerabili.",
          "Wireless" },
        { "Quale canale Wi-Fi in banda 2.4 GHz non si sovrappone con gli altri?",
          {"1, 5, 9","1, 6, 11","2, 6, 10","3, 7, 11"}, 1,
          "I canali 1, 6 e 11 sono i tre canali non sovrapposti nella banda 2.4 GHz (802.11).",
          "Wireless" },

        /* ── WAN/VPN ── */
        { "Qual e' la differenza principale tra LAN e WAN?",
          {"La LAN e' piu' veloce","La WAN copre aree geografiche piu' ampie della LAN","La WAN usa solo connessioni wireless","Non c'e' differenza"}, 1,
          "LAN copre un'area ristretta (ufficio, edificio). WAN copre aree geografiche ampie.",
          "WAN/VPN" },
        { "Cosa fa una VPN IPsec?",
          {"Accelera le connessioni Internet","Crea un tunnel cifrato su una rete pubblica","Assegna IP pubblici","Gestisce le VLAN"}, 1,
          "IPsec crea tunnel cifrati e autenticati per proteggere le comunicazioni su reti non sicure.",
          "WAN/VPN" },
        { "Cosa fa il protocollo BGP?",
          {"Routing all'interno di un AS","Routing tra AS diversi su Internet","Gestione IP in una LAN","Configurazione automatica delle interfacce"}, 1,
          "BGP (Border Gateway Protocol) e' il protocollo di routing tra Autonomous System (AS) diversi su Internet.",
          "WAN/VPN" },

        /* ── Comandi IOS ── */
        { "Quale modalita' usa il prompt 'Router(config)#'?",
          {"User EXEC mode","Privileged EXEC mode","Global Configuration mode","Interface Configuration mode"}, 2,
          "Il prompt '(config)#' indica la Global Configuration mode, accessibile con 'configure terminal'.",
          "Comandi IOS" },
        { "Quale comando salva la configurazione in NVRAM?",
          {"write","copy running-config startup-config","save config","commit"}, 1,
          "'copy running-config startup-config' (o 'write memory') salva la configurazione attuale nella NVRAM.",
          "Comandi IOS" },
        { "Quale comando mostra la configurazione attualmente attiva?",
          {"show config","show running-config","show startup-config","display config"}, 1,
          "'show running-config' mostra la configurazione RAM attiva. 'show startup-config' mostra quella in NVRAM.",
          "Comandi IOS" },
        { "Come si accede alla Privileged EXEC mode da User EXEC?",
          {"su privileged","enable","configure terminal","admin"}, 1,
          "'enable' porta dalla User EXEC mode (Router>) alla Privileged EXEC mode (Router#).",
          "Comandi IOS" },
        { "Quale comando assegna un IP a un'interfaccia Cisco?",
          {"set ip [IP] [mask]","ip address [IP] [mask]","interface ip [IP] [mask]","assign ip [IP] [mask]"}, 1,
          "In interface configuration mode: 'ip address [IP] [subnet mask]' assegna l'indirizzo, poi 'no shutdown'.",
          "Comandi IOS" },
        { "Cosa mostra il comando 'show interfaces'?",
          {"La tabella di routing","Statistiche dettagliate delle interfacce (stato, errori, traffico)","La configurazione VLAN","La tabella ARP"}, 1,
          "'show interfaces' visualizza stato fisico/logico, contatori di errori, MTU e larghezza di banda.",
          "Comandi IOS" },
        { "Quale comando visualizza le route OSPF nella tabella?",
          {"show ospf routes","show ip ospf","show ip route ospf","show ip route | include O"}, 2,
          "'show ip route ospf' o 'show ip route' (con O nella prima colonna) elenca le rotte OSPF.",
          "Comandi IOS" },
        { "Quale comando salva l'output di un comando in un file di testo?",
          {"redirect","pipe output","tee","| redirect"}, 0,
          "In Cisco IOS non esiste un redirect nativo; il modo comune e' usare 'copy running-config tftp' per esportare.",
          "Comandi IOS" },

        /* ── Automazione ── */
        { "Quale protocollo usa NETCONF per il trasporto?",
          {"HTTP","SSH","Telnet","SNMP"}, 1,
          "NETCONF usa SSH come protocollo di trasporto sicuro, comunicando tramite messaggi XML strutturati.",
          "Automazione" },
        { "Cosa fa il protocollo SNMP?",
          {"Gestisce e monitora i dispositivi di rete","Risolve i nomi DNS","Cifra il traffico","Assegna indirizzi IP"}, 0,
          "SNMP permette il monitoraggio e la gestione di dispositivi di rete tramite MIB.",
          "Automazione" },
        { "Quale libreria Python si usa tipicamente per SSH su Cisco?",
          {"requests","netmiko","scapy","paramiko-cisco"}, 1,
          "Netmiko e' la libreria Python piu' usata per automatizzare SSH su dispositivi di rete Cisco.",
          "Automazione" },
        { "Cosa fornisce RESTCONF rispetto a NETCONF?",
          {"Usa XML invece di JSON","Usa HTTP/HTTPS invece di SSH","Opera a livello 2","Richiede agent"}, 1,
          "RESTCONF usa HTTP/HTTPS con JSON e XML, rendendo l'automazione accessibile con strumenti REST standard.",
          "Automazione" },
        { "Quale formato dati usa YANG?",
          {"XML e JSON","Solo XML","Solo JSON","CSV"}, 0,
          "YANG e' il linguaggio di modellazione dei dati usato da NETCONF/RESTCONF; i dati sono in XML o JSON.",
          "Automazione" },

        /* ── QoS ── */
        { "Cosa fa la QoS (Quality of Service)?",
          {"Aumenta la larghezza di banda totale","Priorizza il traffico in base al tipo","Cifra il traffico","Gestisce gli IP"}, 1,
          "QoS classifica e priorizza il traffico (VoIP > video > dati) per garantire prestazioni ai servizi critici.",
          "QoS" },
        { "Quale campo dell'header IP usa QoS per la classificazione?",
          {"TTL","DSCP (Differentiated Services Code Point)","Header Checksum","Protocol"}, 1,
          "DSCP usa i primi 6 bit del campo DS (ex ToS) dell'header IPv4/IPv6 per classificare il traffico.",
          "QoS" },
        { "Cosa significa DSCP EF (Expedited Forwarding)?",
          {"Best effort","Alta priorita' per VoIP e video real-time","Traffico scartato","Traffico management"}, 1,
          "DSCP EF (101110 = 46) e' usato per traffico a bassa latenza come VoIP e videoconferenze.",
          "QoS" },
    };

    for (const auto& row : kData) {
        q.bindValue(0, QString::fromUtf8(row.dom));
        q.bindValue(1, QString::fromUtf8(row.r[0]));
        q.bindValue(2, QString::fromUtf8(row.r[1]));
        q.bindValue(3, QString::fromUtf8(row.r[2]));
        q.bindValue(4, QString::fromUtf8(row.r[3]));
        q.bindValue(5, row.corr);
        q.bindValue(6, QString::fromUtf8(row.spiega));
        q.bindValue(7, QString::fromUtf8(row.tema));
        if (!q.exec())
            qWarning() << "QuizCcnaDb insert error:" << q.lastError().text();
    }

    db.commit();
#endif
}

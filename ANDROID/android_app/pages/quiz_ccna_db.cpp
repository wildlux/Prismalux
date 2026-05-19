#include "quiz_ccna_db.h"
#include <QRandomGenerator>
#include <QSet>

/* ══════════════════════════════════════════════════════════════
   Macro helper per aggiungere domande in modo compatto
   ══════════════════════════════════════════════════════════════ */
#define Q(dom, a, b, c, d, corr, spiega, tema_str) \
    m_questions.append({ \
        QString::fromUtf8(dom), \
        { QString::fromUtf8(a), QString::fromUtf8(b), \
          QString::fromUtf8(c), QString::fromUtf8(d) }, \
        (corr), \
        QString::fromUtf8(spiega), \
        QString::fromUtf8(tema_str) \
    })

QuizCcnaDb::QuizCcnaDb() { load(); }

QStringList QuizCcnaDb::temi() const
{
    QSet<QString> s;
    for (const auto& q : m_questions) s.insert(q.tema);
    QStringList lst = s.values();
    lst.sort();
    return lst;
}

QuizQuestion QuizCcnaDb::randomQuestion() const
{
    if (m_questions.isEmpty()) return {};
    const int idx = QRandomGenerator::global()->bounded(m_questions.count());
    return m_questions.at(idx);
}

QuizQuestion QuizCcnaDb::randomByTema(const QString& tema) const
{
    QVector<QuizQuestion> filtered;
    for (const auto& q : m_questions)
        if (q.tema == tema) filtered.append(q);
    if (filtered.isEmpty()) return randomQuestion();
    const int idx = QRandomGenerator::global()->bounded(filtered.count());
    return filtered.at(idx);
}

/* ══════════════════════════════════════════════════════════════
   Domande CCNA 200-301 — organizzate per tema
   ══════════════════════════════════════════════════════════════ */
void QuizCcnaDb::load()
{
    /* ── Modello OSI e TCP/IP ── */
    Q("Quanti livelli ha il modello OSI?",
      "5", "6", "7", "8",
      2,
      "Il modello OSI ha 7 livelli: Fisico, Data Link, Rete, Trasporto, Sessione, Presentazione, Applicazione.",
      "OSI/TCP-IP");

    Q("A quale livello OSI opera uno switch non gestito?",
      "Livello 1 (Fisico)", "Livello 2 (Data Link)", "Livello 3 (Rete)", "Livello 4 (Trasporto)",
      1,
      "Gli switch operano a livello 2 (Data Link) usando gli indirizzi MAC per inoltrare i frame.",
      "OSI/TCP-IP");

    Q("Quale protocollo del livello 4 garantisce la consegna affidabile?",
      "UDP", "IP", "TCP", "ICMP",
      2,
      "TCP (Transmission Control Protocol) usa handshake a 3 vie, acknowledgement e ritrasmissione per garantire la consegna.",
      "OSI/TCP-IP");

    Q("Cosa fa il protocollo ARP?",
      "Risolve nomi di dominio in IP", "Traduce IP in MAC address", "Assegna IP dinamicamente", "Cifra le comunicazioni",
      1,
      "ARP (Address Resolution Protocol) converte un indirizzo IP in un indirizzo MAC nella stessa rete locale.",
      "OSI/TCP-IP");

    Q("Quale numero di porta usa HTTP?",
      "21", "22", "80", "443",
      2,
      "HTTP usa la porta TCP 80. HTTPS usa la 443, SSH la 22, FTP la 21.",
      "OSI/TCP-IP");

    Q("Quale numero di porta usa HTTPS?",
      "80", "8080", "443", "8443",
      2,
      "HTTPS usa la porta TCP 443 per comunicazioni HTTP cifrate con TLS/SSL.",
      "OSI/TCP-IP");

    Q("Quale protocollo usa la porta 53?",
      "DHCP", "DNS", "SNMP", "NTP",
      1,
      "DNS (Domain Name System) usa la porta 53 sia UDP che TCP per risolvere i nomi di dominio.",
      "OSI/TCP-IP");

    Q("A quale livello OSI opera un router?",
      "Livello 1", "Livello 2", "Livello 3", "Livello 4",
      2,
      "I router operano a livello 3 (Rete) usando gli indirizzi IP per instradare i pacchetti tra reti diverse.",
      "OSI/TCP-IP");

    Q("Cos'è la PDU al livello 2 (Data Link) OSI?",
      "Pacchetto", "Frame", "Segmento", "Bit",
      1,
      "A livello 2 la PDU si chiama Frame. A livello 3 è Pacchetto, livello 4 è Segmento, livello 1 sono Bit.",
      "OSI/TCP-IP");

    Q("Quale protocollo usa la porta UDP 67/68?",
      "DNS", "SNMP", "DHCP", "TFTP",
      2,
      "DHCP usa le porte UDP 67 (server) e 68 (client) per assegnare indirizzi IP dinamicamente.",
      "OSI/TCP-IP");

    /* ── Switching e VLAN ── */
    Q("Cosa fa una VLAN?",
      "Aumenta la velocità della rete", "Segmenta la rete in domini di broadcast separati",
      "Cifra il traffico di rete", "Assegna indirizzi IP automaticamente",
      1,
      "Le VLAN (Virtual LAN) separano il traffico in domini di broadcast logicamente distinti su switch fisici condivisi.",
      "Switching/VLAN");

    Q("Cosa significa 'trunk port' su uno switch Cisco?",
      "Una porta che trasporta il traffico di una sola VLAN",
      "Una porta che trasporta il traffico di pi\xc3\xb9 VLAN usando tag 802.1Q",
      "Una porta di uplink ad alta velocit\xc3\xa0",
      "Una porta riservata al management",
      1,
      "Una trunk port usa il protocollo IEEE 802.1Q per aggiungere tag VLAN ai frame, permettendo il traffico multi-VLAN su un singolo link.",
      "Switching/VLAN");

    Q("Qual è il comando Cisco IOS per creare la VLAN 10?",
      "vlan add 10", "vlan create 10", "vlan 10", "create vlan 10",
      2,
      "Il comando 'vlan 10' in modalità global configuration crea la VLAN 10 sul database dello switch.",
      "Switching/VLAN");

    Q("Cosa fa il protocollo STP (Spanning Tree)?",
      "Aumenta la larghezza di banda", "Elimina i loop di livello 2 nella rete",
      "Bilancia il carico tra pi\xc3\xb9 router", "Gestisce le VLAN",
      1,
      "STP (Spanning Tree Protocol) previene i loop di livello 2 bloccando le porte ridondanti e mantenendo un unico percorso attivo.",
      "Switching/VLAN");

    Q("Quale porta diventa Root Port in STP?",
      "La porta con il costo di percorso pi\xc3\xb9 basso verso il Root Bridge",
      "La porta con la velocit\xc3\xa0 pi\xc3\xb9 alta",
      "La porta configurata manualmente come root",
      "La prima porta numericamente",
      0,
      "La Root Port è la porta con il costo STP più basso verso il Root Bridge. Il Root Bridge è lo switch con il Bridge ID più basso.",
      "Switching/VLAN");

    Q("Cos'è l'EtherChannel?",
      "Un protocollo di routing dinamico",
      "Il raggruppamento logico di pi\xc3\xb9 link fisici in un unico link logico",
      "Una tecnologia di cifratura Ethernet",
      "Un protocollo per gestire le VLAN",
      1,
      "EtherChannel aggrega più link fisici (2-8) in un bundle logico, aumentando la larghezza di banda e fornendo ridondanza.",
      "Switching/VLAN");

    Q("Quale comando mostra la tabella MAC di uno switch Cisco?",
      "show mac-address-table", "show arp", "show interfaces", "show running-config",
      0,
      "Il comando 'show mac-address-table' visualizza la tabella CAM dello switch con gli indirizzi MAC associati alle porte.",
      "Switching/VLAN");

    Q("Qual è la Native VLAN predefinita su Cisco?",
      "VLAN 0", "VLAN 1", "VLAN 99", "VLAN 1000",
      1,
      "La Native VLAN predefinita su Cisco è la VLAN 1. Il traffico della Native VLAN non viene taggato sulle trunk port.",
      "Switching/VLAN");

    /* ── Routing ── */
    Q("Quale comando verifica il routing su un router Cisco?",
      "show arp", "show ip route", "show interfaces", "show vlan",
      1,
      "Il comando 'show ip route' visualizza la tabella di routing con tutte le reti note e il next-hop associato.",
      "Routing");

    Q("Cosa significa 'AD' (Administrative Distance) in Cisco IOS?",
      "La velocit\xc3\xa0 del link in Mbps",
      "L'affidabilit\xc3\xa0 di una sorgente di routing: pi\xc3\xb9 bassa = pi\xc3\xb9 preferita",
      "Il numero di hop verso la destinazione",
      "Il costo del percorso OSPF",
      1,
      "L'Administrative Distance indica l'affidabilità della sorgente: 0=connected, 1=static, 90=EIGRP, 110=OSPF, 120=RIP.",
      "Routing");

    Q("Qual è l'AD di OSPF?",
      "90", "100", "110", "120",
      2,
      "L'AD di OSPF è 110. Per confronto: EIGRP=90, RIP=120, BGP eBGP=20.",
      "Routing");

    Q("Cosa significa la lettera 'O' nella tabella di routing Cisco?",
      "Rotta statica", "Rotta OSPF", "Rotta direttamente connessa", "Rotta EIGRP",
      1,
      "La lettera 'O' indica una rotta appresa tramite OSPF (Open Shortest Path First).",
      "Routing");

    Q("Quale metrica usa OSPF?",
      "Hop count", "Bandwidth", "Cost (basato sulla bandwidth)", "Delay",
      2,
      "OSPF usa il Cost come metrica, calcolato come 10^8 diviso per la bandwidth del link in bps.",
      "Routing");

    Q("Come si configura il default route (gateway of last resort) su Cisco?",
      "ip route default 0.0.0.0", "ip route 0.0.0.0 0.0.0.0 [next-hop]",
      "default-route [next-hop]", "ip gateway 0.0.0.0 [next-hop]",
      1,
      "Il comando 'ip route 0.0.0.0 0.0.0.0 [next-hop o interfaccia]' configura il default route in Cisco IOS.",
      "Routing");

    Q("Qual è il comando per abilitare OSPF su un router Cisco?",
      "ospf enable", "router ospf [process-id]", "enable ospf [process-id]", "start ospf",
      1,
      "Il comando 'router ospf [process-id]' attiva il processo OSPF. Il process-id è locale al router (1-65535).",
      "Routing");

    Q("Quale strato fa il protocollo OSPF per trovare i vicini?",
      "Usa TCP port 179", "Usa multicast 224.0.0.5 e 224.0.0.6", "Usa broadcast",
      "Usa unicast a ogni router",
      1,
      "OSPF usa gli indirizzi multicast 224.0.0.5 (AllSPFRouters) e 224.0.0.6 (AllDRRouters) per comunicare con i vicini.",
      "Routing");

    /* ── IPv4/IPv6 ── */
    Q("Quanti bit ha un indirizzo IPv4?",
      "16", "32", "64", "128",
      1,
      "Un indirizzo IPv4 è composto da 32 bit, suddivisi in 4 ottetti (es. 192.168.1.1).",
      "IPv4/IPv6");

    Q("Qual è la subnet mask per /24?",
      "255.255.0.0", "255.255.255.0", "255.255.255.128", "255.0.0.0",
      1,
      "Il prefisso /24 corrisponde a 24 bit di rete: la subnet mask è 255.255.255.0, con 256 indirizzi (254 host usabili).",
      "IPv4/IPv6");

    Q("Quanti host usabili ha una subnet /30?",
      "2", "4", "6", "8",
      0,
      "Una subnet /30 ha 2^2=4 indirizzi totali: 1 network, 1 broadcast, 2 host usabili. Usata per link punto-punto.",
      "IPv4/IPv6");

    Q("Quale range di indirizzi è privato secondo RFC 1918?",
      "10.0.0.0 - 10.255.255.255", "172.32.0.0 - 172.47.255.255",
      "192.169.0.0 - 192.169.255.255", "169.254.0.0 - 169.254.255.255",
      0,
      "RFC 1918 definisce: 10.0.0.0/8, 172.16.0.0/12 (172.16-31.x.x), 192.168.0.0/16 come indirizzi privati.",
      "IPv4/IPv6");

    Q("Quanti bit ha un indirizzo IPv6?",
      "32", "64", "96", "128",
      3,
      "IPv6 usa indirizzi da 128 bit, scritti come 8 gruppi di 4 cifre esadecimali separati da ':' (es. 2001:db8::1).",
      "IPv4/IPv6");

    Q("Cosa significa il prefisso IPv6 fe80::/10?",
      "Indirizzo globale unicast", "Indirizzo link-local", "Indirizzo multicast", "Indirizzo loopback",
      1,
      "Gli indirizzi fe80::/10 sono link-local: usati solo sulla rete locale, non instradabili attraverso router.",
      "IPv4/IPv6");

    Q("Quale protocollo sostituisce ARP in IPv6?",
      "DHCPv6", "NDP (Neighbor Discovery Protocol)", "ICMPv6 solo", "RIPng",
      1,
      "NDP (Neighbor Discovery Protocol) sostituisce ARP in IPv6, usando messaggi ICMPv6 per la risoluzione degli indirizzi.",
      "IPv4/IPv6");

    Q("Quanti host usabili ha una rete /25?",
      "64", "126", "128", "254",
      1,
      "Una subnet /25 ha 2^7=128 indirizzi totali: -1 network -1 broadcast = 126 host usabili.",
      "IPv4/IPv6");

    /* ── Sicurezza ── */
    Q("Quale comando abilita SSH su un router Cisco?",
      "enable ssh", "crypto key generate rsa", "ip ssh enable", "service ssh start",
      1,
      "Il comando 'crypto key generate rsa' genera la chiave RSA necessaria per SSH. Richiede anche 'ip ssh version 2' e un hostname configurato.",
      "Sicurezza");

    Q("Cosa fa una ACL (Access Control List) standard?",
      "Filtra il traffico basandosi su IP sorgente e destinazione",
      "Filtra il traffico basandosi solo sull'IP sorgente",
      "Gestisce le VLAN",
      "Cifra le comunicazioni",
      1,
      "Le ACL standard (1-99, 1300-1999) filtrano solo in base all'IP sorgente. Le ACL estese (100-199) filtrano anche per destinazione, protocollo e porta.",
      "Sicurezza");

    Q("Dove si applica una ACL in-bound per bloccare traffico in entrata?",
      "Sull'interfaccia di uscita", "Sull'interfaccia di ingresso",
      "Sul gateway predefinito", "Sul trunk port",
      1,
      "Un'ACL 'in' viene applicata sull'interfaccia dalla quale entra il traffico. 'Out' viene applicata sull'interfaccia di uscita.",
      "Sicurezza");

    Q("Cosa fa il Port Security su Cisco?",
      "Abilita SSH sulle porte", "Limita il numero di MAC address su una porta switch",
      "Configura VLAN di sicurezza", "Abilita la crittografia sul link",
      1,
      "Port Security limita il numero di indirizzi MAC consentiti su una porta, proteggendo da MAC flooding e accessi non autorizzati.",
      "Sicurezza");

    Q("Quale versione SSH è consigliata per la sicurezza?",
      "SSH v1", "SSH v2", "SSH v3", "Telnet cifrato",
      1,
      "SSH versione 2 (configurata con 'ip ssh version 2') è più sicura della versione 1 e va sempre preferita.",
      "Sicurezza");

    Q("Cosa fa il comando 'no shutdown' su un'interfaccia Cisco?",
      "Riavvia l'interfaccia", "Abilita e attiva l'interfaccia", "Disabilita l'interfaccia", "Resetta le impostazioni",
      1,
      "Le interfacce Cisco sono in stato 'shutdown' (administratively down) per default. Il comando 'no shutdown' le attiva.",
      "Sicurezza");

    /* ── DHCP e NAT ── */
    Q("Cosa fa il DHCP?",
      "Risolve nomi di dominio", "Assegna indirizzi IP automaticamente ai client",
      "Protegge la rete da intrusioni", "Gestisce il routing tra reti",
      1,
      "DHCP (Dynamic Host Configuration Protocol) assegna automaticamente IP, subnet mask, gateway e DNS ai dispositivi client.",
      "DHCP/NAT");

    Q("Quale tipo di NAT mappa un IP privato in un IP pubblico fisso (1:1)?",
      "NAT Overload (PAT)", "NAT Statico", "NAT Dinamico", "NAT64",
      1,
      "Il NAT Statico (Static NAT) crea una mappatura 1:1 permanente tra un IP privato e un IP pubblico.",
      "DHCP/NAT");

    Q("Cosa significa PAT (Port Address Translation)?",
      "Molti IP privati mappati a molti IP pubblici",
      "Molti IP privati mappati a un singolo IP pubblico usando porte diverse",
      "Un IP privato mappato a un IP pubblico fisso",
      "La traduzione degli indirizzi a livello 2",
      1,
      "PAT (detto anche NAT Overload) permette a molti host interni di condividere un unico IP pubblico differenziandoli tramite le porte sorgente.",
      "DHCP/NAT");

    Q("Quale comando verifica le traduzioni NAT attive su Cisco?",
      "show nat translations", "show ip nat translations",
      "show nat table", "show ip nat active",
      1,
      "Il comando 'show ip nat translations' visualizza la tabella NAT con le mappature IP:porta attive.",
      "DHCP/NAT");

    /* ── Wireless ── */
    Q("Quale standard wireless opera esclusivamente a 5 GHz?",
      "802.11b", "802.11g", "802.11a", "802.11n",
      2,
      "802.11a opera solo a 5 GHz con velocità fino a 54 Mbps. 802.11b/g operano solo a 2.4 GHz. 802.11n supporta entrambe le bande.",
      "Wireless");

    Q("Qual è la banda di frequenza del Wi-Fi 6 (802.11ax)?",
      "Solo 2.4 GHz", "Solo 5 GHz", "2.4 GHz e 5 GHz", "6 GHz solo",
      2,
      "802.11ax (Wi-Fi 6) opera su 2.4 GHz e 5 GHz. Wi-Fi 6E aggiunge anche la banda a 6 GHz.",
      "Wireless");

    Q("Cosa fa un Wireless LAN Controller (WLC)?",
      "Fornisce connettività Internet", "Gestisce e configura centralmente gli Access Point",
      "Instrada il traffico wireless tra VLAN", "Cifra il traffico Wi-Fi",
      1,
      "Il WLC gestisce centralmente gli AP (Access Point) in modalità Lightweight, permettendo configurazione, monitoraggio e roaming unificati.",
      "Wireless");

    Q("Quale protocollo di sicurezza Wi-Fi è attualmente raccomandato?",
      "WEP", "WPA", "WPA2", "WPA3",
      3,
      "WPA3 è lo standard di sicurezza Wi-Fi più recente e sicuro. WEP e WPA sono obsoleti e vulnerabili.",
      "Wireless");

    /* ── WAN e VPN ── */
    Q("Qual è la differenza principale tra una rete LAN e una WAN?",
      "La LAN è pi\xc3\xb9 veloce", "La WAN copre aree geografiche pi\xc3\xb9 ampie della LAN",
      "La WAN usa solo connessioni wireless", "Non c'è differenza significativa",
      1,
      "LAN (Local Area Network) copre un'area ristretta (ufficio, edificio). WAN (Wide Area Network) copre aree geografiche ampie come città o paesi.",
      "WAN/VPN");

    Q("Cosa fa una VPN IPsec?",
      "Accelera le connessioni internet", "Crea un tunnel cifrato su una rete pubblica",
      "Assegna IP pubblici ai dispositivi", "Gestisce le VLAN",
      1,
      "IPsec crea tunnel cifrati e autenticati per proteggere le comunicazioni su reti non sicure come Internet.",
      "WAN/VPN");

    Q("Cosa fa il protocollo BGP?",
      "Routing all'interno di un AS (Interior Gateway Protocol)",
      "Routing tra AS diversi (Exterior Gateway Protocol) su Internet",
      "Gestione degli indirizzi IP in una LAN",
      "Configurazione automatica delle interfacce",
      1,
      "BGP (Border Gateway Protocol) è il protocollo di routing usato su Internet per scambiare informazioni tra Autonomous System (AS) diversi.",
      "WAN/VPN");

    /* ── Comandi Cisco IOS ── */
    Q("Quale modalit\xc3\xa0 usa il prompt 'Router(config)#'?",
      "User EXEC mode", "Privileged EXEC mode", "Global Configuration mode", "Interface Configuration mode",
      2,
      "Il prompt '(config)#' indica la modalità Global Configuration, accessibile con 'configure terminal' dalla Privileged EXEC mode.",
      "Comandi IOS");

    Q("Quale comando salva la configurazione in running-config su NVRAM?",
      "write", "copy running-config startup-config", "save config", "commit",
      1,
      "Il comando 'copy running-config startup-config' (o abbreviato 'write memory') salva la configurazione attuale nella NVRAM.",
      "Comandi IOS");

    Q("Quale comando mostra la configurazione attualmente attiva?",
      "show config", "show running-config", "show startup-config", "display config",
      1,
      "'show running-config' mostra la configurazione RAM attiva. 'show startup-config' mostra quella salvata in NVRAM.",
      "Comandi IOS");

    Q("Come si accede alla Privileged EXEC mode da User EXEC?",
      "Su privileged", "enable", "configure terminal", "admin",
      1,
      "Il comando 'enable' (abbreviato 'en') porta dalla User EXEC mode (Router>) alla Privileged EXEC mode (Router#).",
      "Comandi IOS");

    Q("Quale comando assegna un IP a un'interfaccia Cisco?",
      "set ip [IP] [mask]", "ip address [IP] [mask]", "interface ip [IP] [mask]", "assign ip [IP] [mask]",
      1,
      "Dalla modalità interface configuration: 'ip address [IP] [subnet mask]' assegna l'indirizzo, seguito da 'no shutdown'.",
      "Comandi IOS");

    Q("Cosa mostra il comando 'show interfaces'?",
      "La tabella di routing", "Statistiche dettagliate delle interfacce (stato, errori, traffico)",
      "La configurazione VLAN", "La tabella ARP",
      1,
      "'show interfaces' visualizza lo stato fisico/logico di ogni interfaccia, contatori di errori, MTU e larghezza di banda.",
      "Comandi IOS");

    Q("Come si abilita il logging degli accessi su Cisco IOS?",
      "service log enable", "logging on", "enable logging", "log service start",
      1,
      "Il comando 'logging on' abilita il logging. Si specifica poi la destinazione: 'logging buffered', 'logging console', 'logging [ip-syslog]'.",
      "Comandi IOS");

    /* ── Automazione ── */
    Q("Quale protocollo usa NETCONF per il trasporto?",
      "HTTP", "SSH", "Telnet", "SNMP",
      1,
      "NETCONF usa SSH come protocollo di trasporto sicuro, comunicando tramite messaggi XML strutturati.",
      "Automazione");

    Q("Cosa fa il protocollo SNMP?",
      "Gestisce e monitora i dispositivi di rete",
      "Risolve i nomi DNS",
      "Cifra il traffico di rete",
      "Assegna indirizzi IP",
      0,
      "SNMP (Simple Network Management Protocol) permette il monitoraggio e la gestione di dispositivi di rete tramite MIB (Management Information Base).",
      "Automazione");

    Q("Quale libreria Python si usa tipicamente per connettersi a dispositivi Cisco via SSH?",
      "requests", "netmiko", "scapy", "paramiko-cisco",
      1,
      "Netmiko è la libreria Python più usata per automatizzare l'interazione con dispositivi di rete via SSH, supportando Cisco IOS, NX-OS e molti altri.",
      "Automazione");

    Q("Cosa fornisce RESTCONF rispetto a NETCONF?",
      "Usa XML invece di JSON", "Usa HTTP/HTTPS invece di SSH",
      "Opera a livello 2", "Richiede agent sul dispositivo",
      1,
      "RESTCONF usa HTTP/HTTPS e supporta JSON e XML, rendendo l'automazione di rete accessibile con strumenti REST standard.",
      "Automazione");

    /* ── QoS ── */
    Q("Cosa fa la QoS (Quality of Service)?",
      "Aumenta la larghezza di banda totale",
      "Priorizza il traffico di rete in base al tipo",
      "Cifra il traffico sensibile",
      "Gestisce gli indirizzi IP",
      1,
      "QoS permette di classificare e priorizzare il traffico (es. VoIP > video > dati) per garantire le prestazioni ai servizi critici.",
      "QoS");

    Q("Quale campo dell'intestazione IP usa QoS per la classificazione?",
      "TTL", "DSCP (Differentiated Services Code Point)", "Header Checksum", "Protocol",
      1,
      "DSCP usa i primi 6 bit del campo DS (ex ToS) dell'header IPv4/IPv6 per classificare il traffico in classi di servizio.",
      "QoS");
}

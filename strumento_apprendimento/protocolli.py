"""
Simulatore Visivo di Protocolli
=================================
Demo passo per passo dei principali protocolli di rete, sicurezza,
architettura API e sistemi distribuiti.

Categorie:
  RETE       — TCP, UDP, HTTP, DNS, DHCP, ARP, FTP, SSH
  SICUREZZA  — TLS, OAuth 2.0, JWT, RSA, Diffie-Hellman, PKI
  API        — REST, GraphQL, gRPC, WebSocket, CORS, Rate Limiting
  DISTRIBUITI— Paxos, Raft, Gossip, 2PC, SAGA, Vector Clocks

Dipendenze:
  pip install rich
"""

import os
import sys
import time
import random
from rich.console import Console
try:
    from check_deps import Ridimensiona
except ImportError:
    class Ridimensiona(BaseException): pass
from rich.panel import Panel
from rich.table import Table
from rich.text import Text

console = Console()


# ── Helper visualizzazione ────────────────────────────────────────────────────

def aspetta():
    input("     Premi INVIO per il passo successivo... ")


def mostra_flusso(titolo, parti, passi, border="blue", larghezza=72):
    """Visualizza un flusso di messaggi tra N partecipanti."""
    os.system("cls" if sys.platform == "win32" else "clear")

    tab = Table(box=None, show_header=True, padding=(0, 1), expand=False)
    for p in parti:
        tab.add_column(f"[bold cyan]{p}[/]", min_width=larghezza // len(parti), justify="center")

    for passo in passi:
        tipo   = passo.get("tipo", "msg")
        da     = passo.get("da", "")
        a      = passo.get("a", "")
        msg    = passo.get("msg", "")
        note   = passo.get("note", "")
        colore = passo.get("colore", "yellow")

        if tipo == "sep":
            tab.add_row(*["─" * (larghezza // len(parti) - 2)] * len(parti))
            continue
        if tipo == "nota":
            tab.add_row(*[f"[dim]{msg}[/]"] + [""] * (len(parti) - 1))
            continue

        indice_da = parti.index(da) if da in parti else 0
        indice_a  = parti.index(a)  if a  in parti else len(parti) - 1
        freccia   = "→" if indice_a > indice_da else "←"

        cells = [""] * len(parti)
        if indice_a > indice_da:
            cells[indice_da] = f"[{colore}]{msg} {freccia}[/]"
            if note:
                cells[indice_da] += f"\n[dim]{note}[/]"
        else:
            cells[indice_da] = f"[{colore}]{freccia} {msg}[/]"
            if note:
                cells[indice_da] += f"\n[dim]{note}[/]"

        tab.add_row(*cells)

    console.print(Panel(tab, title=f"[bold]{titolo}[/]", border_style=border, padding=(1, 2)))


def mostra_stati(titolo, stati, corrente, spiegazione="", border="cyan"):
    """Visualizza una macchina a stati evidenziando lo stato corrente."""
    os.system("cls" if sys.platform == "win32" else "clear")
    row = ""
    for i, s in enumerate(stati):
        if s == corrente:
            row += f"[bold green][{s}][/]"
        else:
            row += f"[dim]{s}[/]"
        if i < len(stati) - 1:
            row += " → "
    console.print(Panel(
        row + (f"\n\n  [bold cyan]{spiegazione}[/]" if spiegazione else ""),
        title=f"[bold]{titolo}[/]", border_style=border, padding=(1, 2)
    ))


def mostra_pacchetto(titolo, campi, colore="green"):
    """Visualizza la struttura di un pacchetto/messaggio."""
    os.system("cls" if sys.platform == "win32" else "clear")
    tab = Table(box=None, show_header=True, padding=(0, 2))
    tab.add_column("Campo", style="bold cyan", min_width=22)
    tab.add_column("Valore", style=colore, min_width=36)
    tab.add_column("Descrizione", style="dim", min_width=28)
    for campo, valore, descr in campi:
        tab.add_row(campo, str(valore), descr)
    console.print(Panel(tab, title=f"[bold]{titolo}[/]", border_style=colore, padding=(1, 2)))


# ═══════════════════════════════════════════════════════════════════
# CATEGORIA 1 — PROTOCOLLI DI RETE
# ═══════════════════════════════════════════════════════════════════

def demo_tcp_handshake():
    """TCP 3-way handshake + 4-way close"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]TCP — Transmission Control Protocol[/]\n\n"
        "TCP è un protocollo ORIENTATO ALLA CONNESSIONE: prima di inviare dati\n"
        "stabilisce una connessione con il 3-way handshake.\n\n"
        "  [bold]SYN[/]     = Synchronize — voglio connettermi\n"
        "  [bold]SYN-ACK[/] = il server accetta e risponde\n"
        "  [bold]ACK[/]     = conferma — connessione stabilita\n\n"
        "Numeri di sequenza (SEQ/ACK) garantiscono l'ordine e la consegna dei dati.",
        border_style="blue"
    ))
    aspetta()

    console.print("\n  [bold cyan]── FASE 1: 3-WAY HANDSHAKE (apertura connessione) ──[/]\n")
    passi = [
        {"da":"CLIENT","a":"SERVER","msg":"SYN (seq=0)","note":"Voglio connettermi! Seq iniziale random","colore":"yellow"},
        {"da":"SERVER","a":"CLIENT","msg":"SYN-ACK (seq=0, ack=1)","note":"Accetto! Mio seq=0, confermo tuo seq+1","colore":"green"},
        {"da":"CLIENT","a":"SERVER","msg":"ACK (ack=1)","note":"Connessione STABILITA ✓","colore":"cyan"},
    ]
    for p in passi:
        mostra_flusso("TCP 3-Way Handshake", ["CLIENT","SERVER"], [p])
        aspetta()

    console.print("\n  [bold cyan]── FASE 2: TRASFERIMENTO DATI ──[/]\n")
    dati = [
        {"da":"CLIENT","a":"SERVER","msg":"DATA (seq=1, 500 bytes)","note":"Invio richiesta HTTP","colore":"yellow"},
        {"da":"SERVER","a":"CLIENT","msg":"ACK (ack=501)","note":"Confermo ricezione 500 bytes","colore":"green"},
        {"da":"SERVER","a":"CLIENT","msg":"DATA (seq=1, 1000 bytes)","note":"Risposta HTTP","colore":"yellow"},
        {"da":"CLIENT","a":"SERVER","msg":"ACK (ack=1001)","note":"Confermo ricezione","colore":"green"},
    ]
    for p in dati:
        mostra_flusso("TCP — Trasferimento Dati", ["CLIENT","SERVER"], [p])
        aspetta()

    console.print("\n  [bold cyan]── FASE 3: 4-WAY CLOSE (chiusura connessione) ──[/]\n")
    close = [
        {"da":"CLIENT","a":"SERVER","msg":"FIN","note":"Ho finito di inviare","colore":"red"},
        {"da":"SERVER","a":"CLIENT","msg":"ACK","note":"Ok, attendo che tu finisca","colore":"green"},
        {"da":"SERVER","a":"CLIENT","msg":"FIN","note":"Anche io ho finito","colore":"red"},
        {"da":"CLIENT","a":"SERVER","msg":"ACK","note":"Connessione CHIUSA ✓  (TIME_WAIT 2×MSL)","colore":"cyan"},
    ]
    for p in close:
        mostra_flusso("TCP 4-Way Close", ["CLIENT","SERVER"], [p])
        aspetta()

    console.print(Panel(
        "  [bold green]✓ TCP garantisce:[/]\n"
        "  • Consegna ordinata e affidabile\n"
        "  • Ritrasmissione automatica in caso di perdita\n"
        "  • Controllo di flusso (sliding window)\n"
        "  • Controllo della congestione\n\n"
        "  [bold red]✗ Overhead:[/] 3-way handshake prima di ogni connessione",
        title="[bold]TCP — RIEPILOGO[/]", border_style="blue"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_tcp_sliding_window():
    """TCP Sliding Window: controllo di flusso"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]TCP SLIDING WINDOW — Controllo di Flusso[/]\n\n"
        "Problema: il mittente invia troppo velocemente → il ricevitore non riesce a stare al passo.\n"
        "Soluzione: il ricevitore comunica quanti byte può accettare (finestra = buffer libero).\n\n"
        "  • Il mittente può inviare fino a win_size byte senza aspettare ACK\n"
        "  • Quando arriva ACK, la finestra 'scorre' in avanti\n"
        "  • Se win=0 → il mittente deve aspettare (back-pressure)\n\n"
        "Complessità: bilancia velocità e buffer — evita overflow del ricevitore.",
        border_style="blue"
    ))
    aspetta()

    WIN = 4
    TOTALE = 10
    inviati = 0
    confermati = 0

    while confermati < TOTALE:
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]TCP Sliding Window[/]  Finestra={WIN}  Pacchetti totali={TOTALE}")
        console.print(f"  Confermati: {confermati}  In volo: {inviati - confermati}  Da inviare: {TOTALE - inviati}\n")

        riga = "  "
        for i in range(TOTALE):
            if i < confermati:
                riga += f"[bold green][{i:2}✓][/]"
            elif i < inviati:
                riga += f"[bold yellow][{i:2}→][/]"
            elif i < confermati + WIN:
                riga += f"[bold cyan][{i:2} ][/]"
            else:
                riga += f"[dim][{i:2} ][/]"
        console.print(riga)
        console.print(f"\n  [green]■ = confermato[/]  [yellow]■ = in volo[/]  [cyan]■ = in finestra[/]  [dim]■ = fuori finestra[/]")

        da_inviare = min(confermati + WIN, TOTALE) - inviati
        if da_inviare > 0:
            console.print(f"\n  → Invio {da_inviare} pacchetti ({inviati}...{inviati+da_inviare-1})")
            inviati += da_inviare
            aspetta()

        da_confermare = min(2, inviati - confermati)
        if da_confermare > 0:
            console.print(f"  ← ACK {confermati + da_confermare}: confermo {da_confermare} pacchetti")
            confermati += da_confermare
            aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  Tutti {TOTALE} pacchetti inviati e confermati!\n\n"
        "  [bold green]✓ Sliding Window impedisce overflow del buffer del ricevitore[/]",
        title="[bold]TCP Sliding Window — COMPLETATO[/]", border_style="blue"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_udp():
    """UDP vs TCP: connectionless, nessuna garanzia"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]UDP — User Datagram Protocol[/]\n\n"
        "UDP è CONNECTIONLESS: invia datagrammi senza stabilire connessione.\n"
        "Nessun handshake, nessun ACK, nessun riordino.\n\n"
        "  [bold green]Vantaggi:[/] Velocissimo, bassa latenza, overhead minimo\n"
        "  [bold red]Svantaggi:[/] Pacchetti possono andare persi, duplicarsi, arrivare fuori ordine\n\n"
        "  Usato in: DNS, streaming video, gaming online, VoIP, QUIC/HTTP3",
        border_style="magenta"
    ))
    aspetta()

    console.print("\n  [bold]Confronto TCP vs UDP:[/]\n")
    tab = Table(box=None, show_header=True, padding=(0, 2))
    tab.add_column("Caratteristica", style="bold", min_width=28)
    tab.add_column("TCP", style="green", min_width=22)
    tab.add_column("UDP", style="yellow", min_width=22)
    righe = [
        ("Connessione",       "3-way handshake",    "Nessuna"),
        ("Affidabilità",      "Garantita (ACK)",     "Nessuna"),
        ("Ordine pacchetti",  "Garantito",           "Non garantito"),
        ("Overhead header",   "20-60 byte",          "8 byte"),
        ("Latenza",           "Alta (handshake)",    "Minima"),
        ("Controllo flusso",  "Sì (sliding window)", "No"),
        ("Uso tipico",        "HTTP, FTP, email",    "DNS, streaming, game"),
    ]
    for r in righe:
        tab.add_row(*r)
    console.print(tab)
    aspetta()

    console.print("\n  [bold cyan]Simulazione: perdita pacchetti UDP[/]\n")
    pacchetti = list(range(1, 9))
    persi = {3, 6}
    for pk in pacchetti:
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]UDP[/]  Client invia pacchetto #{pk}")
        if pk in persi:
            console.print(f"  [bold red]✗ Pacchetto #{pk} PERSO in rete — UDP non se ne accorge![/]")
            console.print(f"  [dim]Con TCP ci sarebbe stato un timeout e ritrasmissione automatica[/]")
        else:
            console.print(f"  [bold green]✓ Pacchetto #{pk} ricevuto dal server[/]")
        aspetta()

    console.print(Panel(
        f"  Pacchetti inviati: {len(pacchetti)}\n"
        f"  Pacchetti persi:   {len(persi)}  ({persi})\n"
        f"  Pacchetti ricevuti: {len(pacchetti)-len(persi)}\n\n"
        "  [bold yellow]UDP non rileva né corregge la perdita — l'applicazione decide cosa fare[/]",
        title="[bold]UDP — COMPLETATO[/]", border_style="magenta"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_http():
    """HTTP/1.1 request/response cycle"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]HTTP/1.1 — HyperText Transfer Protocol[/]\n\n"
        "Protocollo request-response su TCP. STATELESS: ogni richiesta è indipendente.\n\n"
        "  Metodi principali: GET POST PUT PATCH DELETE HEAD OPTIONS\n"
        "  Status code: 2xx=successo  3xx=redirect  4xx=errore client  5xx=errore server\n\n"
        "  Header importanti: Content-Type, Authorization, Cache-Control, Cookie, CORS\n"
        "  HTTP/1.1 problemi: head-of-line blocking — una richiesta alla volta per connessione",
        border_style="cyan"
    ))
    aspetta()

    passi_req = [
        {"tipo":"nota","msg":"── CLIENT invia richiesta ──"},
        {"da":"CLIENT","a":"SERVER","msg":"GET /api/utenti/42 HTTP/1.1","note":"","colore":"yellow"},
        {"da":"CLIENT","a":"SERVER","msg":"Host: api.esempio.com","note":"","colore":"dim"},
        {"da":"CLIENT","a":"SERVER","msg":"Authorization: Bearer <token>","note":"","colore":"dim"},
        {"da":"CLIENT","a":"SERVER","msg":"Accept: application/json","note":"","colore":"dim"},
    ]
    mostra_flusso("HTTP/1.1 Request", ["CLIENT","SERVER"], passi_req, border="cyan")
    aspetta()

    mostra_pacchetto("HTTP/1.1 Request — Struttura", [
        ("Metodo",       "GET",                     "Azione da compiere"),
        ("URL",          "/api/utenti/42",           "Risorsa richiesta"),
        ("Versione",     "HTTP/1.1",                "Versione protocollo"),
        ("Host",         "api.esempio.com",          "Server destinatario"),
        ("Authorization","Bearer eyJhbGci...",        "Token autenticazione"),
        ("Accept",       "application/json",          "Formato risposta desiderato"),
        ("Body",         "(vuoto per GET)",           "Corpo della richiesta"),
    ], colore="yellow")
    aspetta()

    passi_resp = [
        {"tipo":"nota","msg":"── SERVER risponde ──"},
        {"da":"SERVER","a":"CLIENT","msg":"HTTP/1.1 200 OK","note":"Status: tutto ok","colore":"green"},
        {"da":"SERVER","a":"CLIENT","msg":"Content-Type: application/json","note":"","colore":"dim"},
        {"da":"SERVER","a":"CLIENT","msg":"Cache-Control: max-age=3600","note":"","colore":"dim"},
        {"da":"SERVER","a":"CLIENT","msg":'{"id":42,"nome":"Mario"}','note':"Body JSON","colore":"cyan"},
    ]
    mostra_flusso("HTTP/1.1 Response", ["CLIENT","SERVER"], passi_resp, border="green")
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "  [bold]Status Code più usati:[/]\n\n"
        "  [bold green]200[/] OK               [bold green]201[/] Created    [bold green]204[/] No Content\n"
        "  [bold yellow]301[/] Moved Permanently [bold yellow]304[/] Not Modified\n"
        "  [bold red]400[/] Bad Request      [bold red]401[/] Unauthorized  [bold red]403[/] Forbidden\n"
        "  [bold red]404[/] Not Found        [bold red]429[/] Too Many Requests\n"
        "  [bold red]500[/] Internal Server Error  [bold red]502[/] Bad Gateway  [bold red]503[/] Unavailable\n\n"
        "  [bold]HTTP/2 migliora:[/] multiplexing (più richieste su 1 connessione), header compression, server push",
        title="[bold]HTTP — RIEPILOGO[/]", border_style="cyan"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_http2():
    """HTTP/2: multiplexing e header compression"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]HTTP/2 — Multiplexing e Compressione[/]\n\n"
        "HTTP/1.1 problema: una richiesta alla volta → HEAD-OF-LINE BLOCKING.\n"
        "HTTP/2 soluzione: MULTIPLEXING — più richieste parallele su UNA connessione TCP.\n\n"
        "  • Frame binari invece di testo — più efficiente da parsare\n"
        "  • HPACK: compressione header (risparmia 80-90% su header ripetuti)\n"
        "  • Server Push: il server invia risorse prima che vengano richieste\n"
        "  • Prioritizzazione degli stream",
        border_style="cyan"
    ))
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]HTTP/1.1 — Richieste sequenziali (head-of-line blocking):[/]\n")
    for i, (url, ms) in enumerate([("/index.html",200),("/style.css",150),("/script.js",180),("/logo.png",100)]):
        barra = "█" * (ms // 10)
        console.print(f"  Connessione {i+1}: [yellow]{url:<20}[/]  [bold]{barra}[/] {ms}ms")
    console.print(f"\n  Tempo totale sequenziale: ~630ms\n")
    aspetta()

    console.print("\n  [bold]HTTP/2 — Richieste parallele (multiplexing su 1 connessione):[/]\n")
    richieste = [("/index.html",200,"stream 1"),("/style.css",150,"stream 3"),("/script.js",180,"stream 5"),("/logo.png",100,"stream 7")]
    for url, ms, stream in richieste:
        barra = "█" * (ms // 10)
        console.print(f"  [{stream}]: [cyan]{url:<20}[/]  [bold]{barra}[/] {ms}ms")
    console.print(f"\n  [bold green]Tempo totale parallelo: ~200ms  (il più lento)[/]")
    console.print(f"  Risparmio: ~68% di tempo su 1 sola connessione TCP\n")
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "  [bold]HPACK — Header Compression:[/]\n\n"
        "  Prima richiesta: Authorization: Bearer eyJhbGciOiJSUzI1N...  (200+ byte)\n"
        "  Seconda richiesta: [bold green]:authority 3[/]  (3 byte — indice nella tabella!)\n\n"
        "  [bold]Server Push:[/]\n"
        "  Client richiede /index.html\n"
        "  Server risponde con /index.html + PUSH /style.css + PUSH /script.js\n"
        "  Client riceve già le risorse prima di leggerle nell'HTML!\n\n"
        "  [bold cyan]HTTP/3 (QUIC):[/] sostituisce TCP con UDP+QUIC — elimina TCP head-of-line blocking",
        title="[bold]HTTP/2 — RIEPILOGO[/]", border_style="cyan"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_dns():
    """DNS: risoluzione ricorsiva di un nome a dominio"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]DNS — Domain Name System[/]\n\n"
        "DNS è la 'rubrica telefonica' di Internet:\n"
        "traduce nomi leggibili (es. google.com) in indirizzi IP (es. 142.250.180.14).\n\n"
        "  Livelli DNS: Root → TLD (com, it, org) → Authoritative → Record\n"
        "  Tipi di record: A (IPv4), AAAA (IPv6), CNAME (alias), MX (mail), TXT, NS\n"
        "  TTL: Time To Live — quanto tempo fare caching del record\n"
        "  Risoluzione: ricorsiva (resolver fa tutto) o iterativa (client fa ogni step)",
        border_style="blue"
    ))
    aspetta()

    parti = ["BROWSER", "DNS RESOLVER", "ROOT NS", "TLD NS (.com)", "AUTHORITATIVE NS"]
    passi_dns = [
        {"da":"BROWSER","a":"DNS RESOLVER","msg":"www.esempio.com?","note":"Cache locale vuota","colore":"yellow"},
        {"da":"DNS RESOLVER","a":"ROOT NS","msg":"www.esempio.com?","note":"Chiedo ai Root Server (13 cluster nel mondo)","colore":"cyan"},
        {"da":"ROOT NS","a":"DNS RESOLVER","msg":"Vai al TLD .com","note":"Indirizzo del TLD .com NS","colore":"green"},
        {"da":"DNS RESOLVER","a":"TLD NS (.com)","msg":"www.esempio.com?","note":"Chiedo al TLD .com","colore":"cyan"},
        {"da":"TLD NS (.com)","a":"DNS RESOLVER","msg":"Vai all'Authoritative NS","note":"NS di esempio.com","colore":"green"},
        {"da":"DNS RESOLVER","a":"AUTHORITATIVE NS","msg":"www.esempio.com?","note":"Chiedo al server autorevole","colore":"cyan"},
        {"da":"AUTHORITATIVE NS","a":"DNS RESOLVER","msg":"93.184.216.34  TTL=3600","note":"Risposta definitiva!","colore":"bold green"},
        {"da":"DNS RESOLVER","a":"BROWSER","msg":"93.184.216.34","note":"Cache per 3600s (1 ora)","colore":"bold green"},
    ]

    for i, p in enumerate(passi_dns):
        mostra_flusso("DNS Resolution — www.esempio.com", parti, passi_dns[:i+1], larghezza=90)
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "  [bold]Record DNS più usati:[/]\n\n"
        "  [bold cyan]A[/]      example.com → 93.184.216.34  (IPv4)\n"
        "  [bold cyan]AAAA[/]   example.com → 2606:2800:220:1:248:1893:25c8:1946  (IPv6)\n"
        "  [bold cyan]CNAME[/]  www → example.com  (alias)\n"
        "  [bold cyan]MX[/]     mail.example.com  priorità=10  (email server)\n"
        "  [bold cyan]TXT[/]    'v=spf1 include:...'  (verifica email, DKIM, DMARC)\n"
        "  [bold cyan]NS[/]     ns1.example.com  (name server autorevole)\n\n"
        "  [bold green]DNS caching:[/] ISP (TTL), OS (/etc/hosts), Browser (chrome://dns)",
        title="[bold]DNS — RIEPILOGO[/]", border_style="blue"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_dhcp():
    """DHCP: assegnazione automatica IP — DORA"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]DHCP — Dynamic Host Configuration Protocol[/]\n\n"
        "Ogni dispositivo che si connette a una rete ha bisogno di:\n"
        "  • Indirizzo IP    • Subnet mask    • Default gateway    • DNS server\n\n"
        "DHCP assegna questi parametri AUTOMATICAMENTE con il processo DORA:\n"
        "  [bold yellow]D[/]iscover → [bold green]O[/]ffer → [bold cyan]R[/]equest → [bold magenta]A[/]cknowledge\n\n"
        "Porta: UDP 67 (server), UDP 68 (client)  — usa broadcast inizialmente",
        border_style="magenta"
    ))
    aspetta()

    parti = ["DEVICE (nuovo)", "DHCP SERVER"]
    passi = [
        {"da":"DEVICE (nuovo)","a":"DHCP SERVER","msg":"DISCOVER (broadcast)","note":"Chi è il DHCP server? Ho bisogno di un IP!","colore":"yellow"},
        {"da":"DHCP SERVER","a":"DEVICE (nuovo)","msg":"OFFER: IP=192.168.1.50, mask=/24, gw=192.168.1.1","note":"Ti offro questo IP per 24 ore","colore":"green"},
        {"da":"DEVICE (nuovo)","a":"DHCP SERVER","msg":"REQUEST: voglio 192.168.1.50","note":"Accetto l'offerta (broadcast — altri server sentono)","colore":"cyan"},
        {"da":"DHCP SERVER","a":"DEVICE (nuovo)","msg":"ACK: 192.168.1.50 è tuo per 86400s","note":"Lease confermato! DNS: 8.8.8.8, 8.8.4.4","colore":"bold green"},
    ]

    for i, p in enumerate(passi):
        os.system("cls" if sys.platform == "win32" else "clear")
        step_name = ["DISCOVER","OFFER","REQUEST","ACKNOWLEDGE"][i]
        console.print(f"\n  [bold cyan]DORA — Step {i+1}/4: {step_name}[/]\n")
        mostra_flusso("DHCP — DORA", parti, passi[:i+1])
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "  [bold]Parametri assegnati dal DHCP:[/]\n\n"
        "  IP Address:     192.168.1.50\n"
        "  Subnet Mask:    255.255.255.0  (/24)\n"
        "  Default Gateway: 192.168.1.1\n"
        "  DNS Server:     8.8.8.8  8.8.4.4\n"
        "  Lease Time:     86400s (24 ore)\n\n"
        "  [bold cyan]Rinnovo lease:[/] a metà lease, il device invia DHCP REQUEST unicast\n"
        "  [bold cyan]IP statico:[/] alternativa — configurato manualmente (server, router)",
        title="[bold]DHCP — RIEPILOGO[/]", border_style="magenta"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_arp():
    """ARP: risoluzione indirizzo IP → MAC"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]ARP — Address Resolution Protocol[/]\n\n"
        "IP è il livello 3 (rete), MAC è il livello 2 (link).\n"
        "Per inviare un frame sulla LAN, devo conoscere il MAC dell'IP destinatario.\n"
        "ARP risolve: IP address → MAC address sulla rete locale.\n\n"
        "  1. ARP Request: broadcast 'Chi ha IP 192.168.1.1?'\n"
        "  2. ARP Reply: unicast 'Sono io! Il mio MAC è aa:bb:cc:dd:ee:ff'\n"
        "  3. Cache ARP: memorizza la risposta per evitare richieste ripetute\n\n"
        "ARP Poisoning: attacco che invia risposte ARP false → man-in-the-middle!",
        border_style="blue"
    ))
    aspetta()

    parti = ["PC (192.168.1.10)", "SWITCH", "ROUTER (192.168.1.1)", "PC B (192.168.1.20)"]
    passi = [
        {"da":"PC (192.168.1.10)","a":"SWITCH","msg":"ARP WHO HAS 192.168.1.1? (broadcast)","note":"FF:FF:FF:FF:FF:FF — tutti ricevono","colore":"yellow"},
        {"da":"SWITCH","a":"ROUTER (192.168.1.1)","msg":"ARP broadcast inoltrato","note":"Lo switch lo manda a tutti i porte","colore":"dim"},
        {"da":"SWITCH","a":"PC B (192.168.1.20)","msg":"ARP broadcast inoltrato","note":"","colore":"dim"},
        {"da":"ROUTER (192.168.1.1)","a":"PC (192.168.1.10)","msg":"ARP REPLY: 192.168.1.1 is at aa:bb:cc:11:22:33","note":"Risposta unicast — solo al richiedente","colore":"bold green"},
    ]
    for i, p in enumerate(passi):
        mostra_flusso("ARP Resolution", parti, passi[:i+1], larghezza=90)
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "  [bold]Cache ARP (Linux: arp -n):[/]\n\n"
        "  192.168.1.1    aa:bb:cc:11:22:33   ether  REACHABLE\n"
        "  192.168.1.20   dd:ee:ff:44:55:66   ether  STALE\n\n"
        "  [bold red]ARP Poisoning (attacco):[/]\n"
        "  Attaccante invia ARP Reply false → le vittime associano IP sbagliati ai MAC\n"
        "  Risultato: tutto il traffico passa per l'attaccante (MITM)\n"
        "  Difesa: ARP inspection sui switch gestiti, IPv6 usa NDP (più sicuro)",
        title="[bold]ARP — RIEPILOGO[/]", border_style="blue"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_ssh():
    """SSH: connessione sicura con scambio chiavi"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]SSH — Secure Shell[/]\n\n"
        "SSH sostituisce Telnet/rsh aggiungendo crittografia end-to-end.\n"
        "Funziona su porta 22 (TCP).\n\n"
        "  1. Negoziazione algoritmi (key exchange, cipher, MAC, compressione)\n"
        "  2. Scambio chiavi Diffie-Hellman → session key condivisa\n"
        "  3. Autenticazione server (host key verificata o aggiunta a known_hosts)\n"
        "  4. Autenticazione utente (password o chiave pubblica)\n"
        "  5. Tunnel cifrato stabilito — tutto il traffico è criptato",
        border_style="green"
    ))
    aspetta()

    parti = ["CLIENT", "SERVER"]
    passi = [
        {"da":"CLIENT","a":"SERVER","msg":"TCP SYN → porta 22","note":"Connessione TCP standard","colore":"dim"},
        {"da":"SERVER","a":"CLIENT","msg":"SSH-2.0-OpenSSH_9.0","note":"Banner: versione SSH del server","colore":"cyan"},
        {"da":"CLIENT","a":"SERVER","msg":"SSH-2.0-OpenSSH_8.9","note":"Banner client","colore":"cyan"},
        {"da":"CLIENT","a":"SERVER","msg":"KEXINIT: ecdh-sha2-nistp256, aes256-gcm, hmac-sha2-256","note":"Propongo algoritmi supportati","colore":"yellow"},
        {"da":"SERVER","a":"CLIENT","msg":"KEXINIT: accordo su ecdh-sha2-nistp256","note":"Seleziono algoritmi comuni","colore":"green"},
        {"da":"CLIENT","a":"SERVER","msg":"KEX_ECDH_INIT: chiave pubblica temporanea","note":"Diffie-Hellman ephemeral","colore":"yellow"},
        {"da":"SERVER","a":"CLIENT","msg":"KEX_ECDH_REPLY: host_key + firma + chiave pubblica temp","note":"Deriva session key condivisa","colore":"green"},
        {"da":"CLIENT","a":"SERVER","msg":"NEWKEYS","note":"Ora uso la session key per cifrare","colore":"bold cyan"},
        {"da":"SERVER","a":"CLIENT","msg":"NEWKEYS","note":"Tunnel cifrato STABILITO ✓","colore":"bold green"},
        {"da":"CLIENT","a":"SERVER","msg":"[cifrato] AUTH username+public_key","note":"Autenticazione con chiave pubblica","colore":"bold green"},
        {"da":"SERVER","a":"CLIENT","msg":"[cifrato] AUTH_SUCCESS","note":"Sessione shell aperta","colore":"bold green"},
    ]
    for i, p in enumerate(passi):
        mostra_flusso("SSH Handshake", parti, passi[:i+1])
        aspetta()

    console.print(Panel(
        "  [bold green]✓ Tunnel SSH cifrato — tutto il traffico successivo è criptato[/]\n\n"
        "  [bold]Autenticazione con chiave pubblica (più sicura di password):[/]\n"
        "  1. Client ha coppia chiave privata/pubblica\n"
        "  2. Chiave pubblica copiata sul server (~/.ssh/authorized_keys)\n"
        "  3. Server sfida il client con un numero random\n"
        "  4. Client firma con la chiave privata → server verifica con pubblica\n\n"
        "  [bold cyan]ssh-keygen -t ed25519[/]  (genera coppia di chiavi)\n"
        "  [bold cyan]ssh-copy-id user@server[/]  (copia chiave pubblica)",
        title="[bold]SSH — RIEPILOGO[/]", border_style="green"
    ))
    input("\n  [INVIO per tornare al menu] ")


# ═══════════════════════════════════════════════════════════════════
# CATEGORIA 2 — SICUREZZA
# ═══════════════════════════════════════════════════════════════════

def demo_tls():
    """TLS 1.3 handshake completo"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]TLS 1.3 — Transport Layer Security[/]\n\n"
        "TLS cifra il traffico su TCP → HTTPS = HTTP + TLS.\n"
        "TLS 1.3 (2018) è più semplice e veloce di TLS 1.2:\n"
        "  • Handshake in 1 RTT invece di 2 (0-RTT per sessioni riprese!)\n"
        "  • Eliminati algoritmi deboli (RSA key exchange, MD5, SHA1, 3DES)\n"
        "  • Solo forward secrecy: anche se la chiave privata è compromessa,\n"
        "    le sessioni passate rimangono cifrate\n"
        "  • Certificati nascosti nell'handshake (privacy migliorata)",
        border_style="green"
    ))
    aspetta()

    parti = ["CLIENT (Browser)", "SERVER"]
    passi = [
        {"da":"CLIENT (Browser)","a":"SERVER","msg":"ClientHello","note":"TLS 1.3, cipher suites, client_random, key_share (ECDH pubkey)","colore":"yellow"},
        {"da":"SERVER","a":"CLIENT (Browser)","msg":"ServerHello","note":"Cipher scelto, server_random, key_share (ECDH pubkey)","colore":"green"},
        {"da":"SERVER","a":"CLIENT (Browser)","msg":"[derivano handshake_secret con ECDH]","note":"Entrambi calcolano la stessa chiave di sessione!","colore":"dim"},
        {"da":"SERVER","a":"CLIENT (Browser)","msg":"Certificate (cifrato)","note":"Certificato X.509 del server","colore":"cyan"},
        {"da":"SERVER","a":"CLIENT (Browser)","msg":"CertificateVerify (cifrato)","note":"Firma con chiave privata — prova di possesso","colore":"cyan"},
        {"da":"SERVER","a":"CLIENT (Browser)","msg":"Finished (cifrato, MAC)","note":"Handshake completato lato server","colore":"bold green"},
        {"da":"CLIENT (Browser)","a":"SERVER","msg":"Finished (cifrato, MAC)","note":"Verifica certificato con CA. Handshake OK ✓","colore":"bold green"},
        {"da":"CLIENT (Browser)","a":"SERVER","msg":"[dati applicativi cifrati]","note":"HTTP request cifrata con chiave di sessione","colore":"bold cyan"},
        {"da":"SERVER","a":"CLIENT (Browser)","msg":"[dati applicativi cifrati]","note":"HTTP response cifrata","colore":"bold cyan"},
    ]
    for i, p in enumerate(passi):
        mostra_flusso("TLS 1.3 Handshake", parti, passi[:i+1])
        aspetta()

    console.print(Panel(
        "  [bold]Cosa garantisce TLS:[/]\n\n"
        "  [bold green]Confidenzialità[/]   I dati sono leggibili solo da mittente e destinatario\n"
        "  [bold green]Integrità[/]         I dati non possono essere modificati in transito (MAC)\n"
        "  [bold green]Autenticità[/]       Il server è chi dice di essere (certificato firmato da CA)\n"
        "  [bold green]Forward Secrecy[/]   Chiavi di sessione effimere — compromettere la chiave privata\n"
        "                     non decripta le sessioni passate\n\n"
        "  [bold cyan]🔒 https://[/] nel browser = TLS attivo + certificato verificato",
        title="[bold]TLS 1.3 — RIEPILOGO[/]", border_style="green"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_oauth2():
    """OAuth 2.0: Authorization Code Flow"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]OAuth 2.0 — Authorization Code Flow[/]\n\n"
        "OAuth 2.0 è un framework di AUTORIZZAZIONE (non autenticazione!).\n"
        "Permette a un'app di accedere a risorse per conto dell'utente\n"
        "SENZA che l'utente condivida le sue credenziali con l'app.\n\n"
        "  Attori:\n"
        "  • Resource Owner = l'utente\n"
        "  • Client = l'app che vuole accedere\n"
        "  • Authorization Server = chi emette i token (es. Google, GitHub)\n"
        "  • Resource Server = l'API con i dati",
        border_style="yellow"
    ))
    aspetta()

    parti = ["UTENTE", "APP (Client)", "AUTH SERVER", "API (Resource)"]
    passi = [
        {"da":"UTENTE","a":"APP (Client)","msg":"Clicca 'Login con Google'","note":"","colore":"yellow"},
        {"da":"APP (Client)","a":"AUTH SERVER","msg":"GET /authorize?client_id=123&scope=email&redirect_uri=...","note":"Redirect l'utente all'auth server","colore":"cyan"},
        {"da":"AUTH SERVER","a":"UTENTE","msg":"Pagina di login Google","note":"L'utente si autentica con Google (NON con l'app!)","colore":"yellow"},
        {"da":"UTENTE","a":"AUTH SERVER","msg":"Acconsente ai permessi richiesti","note":"'Vuoi dare accesso alla tua email?'","colore":"green"},
        {"da":"AUTH SERVER","a":"APP (Client)","msg":"Redirect: /callback?code=AUTH_CODE_xyz","note":"Authorization Code (valido ~10 min, usabile 1 volta)","colore":"bold cyan"},
        {"da":"APP (Client)","a":"AUTH SERVER","msg":"POST /token: code=AUTH_CODE_xyz + client_secret","note":"Scambio code → access_token (server-side, sicuro!)","colore":"cyan"},
        {"da":"AUTH SERVER","a":"APP (Client)","msg":"access_token + refresh_token","note":"Access token (1h), refresh token (30gg)","colore":"bold green"},
        {"da":"APP (Client)","a":"API (Resource)","msg":"GET /userinfo  Authorization: Bearer access_token","note":"Usa il token per accedere alle risorse","colore":"bold cyan"},
        {"da":"API (Resource)","a":"APP (Client)","msg":'{"email":"utente@gmail.com","name":"Mario"}','note':"Risposta con i dati dell'utente","colore":"bold green"},
    ]
    for i, p in enumerate(passi):
        mostra_flusso("OAuth 2.0 — Authorization Code Flow", parti, passi[:i+1], larghezza=95)
        aspetta()

    console.print(Panel(
        "  [bold]Perché Authorization Code (non Implicit)?[/]\n"
        "  • Il code viene scambiato con token SERVER-SIDE — il token non appare mai nell'URL\n"
        "  • Il client_secret rimane sicuro sul backend\n"
        "  • L'Implicit Flow (deprecato!) metteva il token nell'URL → leakage\n\n"
        "  [bold]Grant Types:[/]\n"
        "  Authorization Code  → app web con backend\n"
        "  PKCE                → mobile/SPA senza backend (rimpiazza Implicit)\n"
        "  Client Credentials  → machine-to-machine (no utente)\n"
        "  Device Code         → TV, CLI senza browser",
        title="[bold]OAuth 2.0 — RIEPILOGO[/]", border_style="yellow"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_jwt():
    """JWT: struttura e validazione"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]JWT — JSON Web Token[/]\n\n"
        "JWT è un token SELF-CONTAINED: contiene tutte le info necessarie.\n"
        "Formato: header.payload.signature  (base64url encoded, separati da '.')\n\n"
        "  Header:    tipo di token e algoritmo di firma (es. RS256, HS256)\n"
        "  Payload:   i 'claims' — chi sei, cosa puoi fare, quando scade\n"
        "  Signature: firma crittografica — garantisce che non sia stato modificato\n\n"
        "  [bold red]ATTENZIONE:[/] payload è solo codificato (base64), NON criptato!\n"
        "  Non mettere segreti nel payload. Usa JWE per dati sensibili.",
        border_style="yellow"
    ))
    aspetta()

    mostra_pacchetto("JWT — Struttura", [
        ("Header (base64url)", '{"alg":"RS256","typ":"JWT"}',      "Algoritmo: RSA-SHA256"),
        ("Payload (base64url)", '{"sub":"42","name":"Mario Rossi"}', "Subject = user ID"),
        ("",                   '{"iat":1708600000}',                "Issued At (timestamp)"),
        ("",                   '{"exp":1708603600}',                "Expires: +1 ora"),
        ("",                   '{"roles":["admin","user"]}',        "Ruoli/permessi"),
        ("Signature",          "HMAC/RSA della parte header.payload","Verifica integrità"),
        ("Token completo",     "eyJhbGci...  (3 parti con '.' )",   "Inviato in Authorization: Bearer"),
    ], colore="yellow")
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]Processo di VALIDAZIONE JWT (lato server):[/]\n")
    passi_val = [
        ("Estrae le 3 parti",          "split('.') → header, payload, signature"),
        ("Decodifica header",           "base64url_decode → algoritmo = RS256"),
        ("Verifica firma",             "RSA verify(header.payload, signature, public_key)"),
        ("Controlla exp",              f"exp=1708603600 > now=1708600100 → NON scaduto ✓"),
        ("Controlla iss/aud",          "Issuer e Audience corretti ✓"),
        ("Estrae claims",              "sub=42, roles=['admin','user'] → autorizza la richiesta"),
    ]
    for i, (titolo, dettaglio) in enumerate(passi_val):
        console.print(f"  [bold cyan]{i+1}.[/] {titolo}")
        console.print(f"     [dim]{dettaglio}[/]\n")
    aspetta()

    console.print(Panel(
        "  [bold green]Vantaggi JWT:[/]\n"
        "  • Stateless — server non deve interrogare DB per ogni richiesta\n"
        "  • Self-contained — contiene tutti i dati necessari\n"
        "  • Scalabile — funziona con più server senza sessioni condivise\n\n"
        "  [bold red]Svantaggi JWT:[/]\n"
        "  • Non revocabile facilmente (finché non scade è valido)\n"
        "  • Se rubato è usabile fino alla scadenza\n"
        "  • Payload visibile — non mettere dati sensibili\n\n"
        "  [bold cyan]Best practice:[/] exp corto (15min-1h) + refresh token",
        title="[bold]JWT — RIEPILOGO[/]", border_style="yellow"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_diffie_hellman():
    """Diffie-Hellman key exchange: scambio segreto su canale pubblico"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]DIFFIE-HELLMAN — Scambio di Chiavi[/]\n\n"
        "Problema: come concordare un segreto comune su un canale pubblico\n"
        "dove un intercettatore può ascoltare tutto?\n\n"
        "Diffie-Hellman (1976): matematica dei logaritmi discreti.\n"
        "  • Parametri pubblici: p (primo grande), g (generatore)\n"
        "  • Ogni parte genera un numero SEGRETO privato (a, b)\n"
        "  • Scambiano i valori pubblici: g^a mod p, g^b mod p\n"
        "  • Derivano lo stesso segreto: (g^b)^a = (g^a)^b = g^ab mod p\n\n"
        "Analoga a mescolare vernici: il colore comune finale è segreto!",
        border_style="cyan"
    ))
    aspetta()

    p = 23; g = 5
    a = 6; b = 15

    A = pow(g, a, p)
    B = pow(g, b, p)
    segreto_alice = pow(B, a, p)
    segreto_bob   = pow(A, b, p)

    passi = [
        ("Parametri pubblici", f"p={p} (primo)  g={g} (generatore)", "Tutti conoscono p e g"),
        ("Alice sceglie privato", f"a={a}  (SEGRETO — solo Alice)", "Non lo condivide mai"),
        ("Bob sceglie privato",   f"b={b}  (SEGRETO — solo Bob)",   "Non lo condivide mai"),
        ("Alice → Bob (pubblico)", f"A = g^a mod p = {g}^{a} mod {p} = {A}", "Intercettabile!"),
        ("Bob → Alice (pubblico)", f"B = g^b mod p = {g}^{b} mod {p} = {B}", "Intercettabile!"),
        ("Alice calcola segreto", f"B^a mod p = {B}^{a} mod {p} = {segreto_alice}", "Solo Alice può farlo"),
        ("Bob calcola segreto",   f"A^b mod p = {A}^{b} mod {p} = {segreto_bob}",  "Solo Bob può farlo"),
        ("Segreto condiviso!",    f"Alice={segreto_alice}  Bob={segreto_bob}  → UGUALE ✓", "Senza mai condividerlo!"),
    ]

    for i, (titolo, valore, nota) in enumerate(passi):
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Diffie-Hellman — passo {i+1}/{len(passi)}[/]\n")
        console.print(Panel(
            f"  [bold cyan]{titolo}[/]\n\n"
            f"  [bold yellow]{valore}[/]\n\n"
            f"  [dim]{nota}[/]",
            border_style="cyan", padding=(1, 2)
        ))
        aspetta()

    console.print(Panel(
        "  [bold]Sicurezza:[/] per rompere DH bisogna risolvere il logaritmo discreto:\n"
        "  trovare 'a' conoscendo g, p, g^a mod p — computazionalmente impossibile\n"
        "  con p sufficientemente grande (2048+ bit in produzione).\n\n"
        "  [bold]ECDH (Elliptic Curve Diffie-Hellman):[/]\n"
        "  Stessa idea ma con curve ellittiche — chiavi più corte, stessa sicurezza\n"
        "  p-256, p-384, X25519 — usato in TLS 1.3, Signal, WhatsApp\n\n"
        "  [bold red]Attenzione:[/] DH base non autentica — combinare con firma RSA/ECDSA",
        title="[bold]Diffie-Hellman — RIEPILOGO[/]", border_style="cyan"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_rsa():
    """RSA: crittografia asimmetrica passo per passo"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]RSA — Crittografia Asimmetrica[/]\n\n"
        "RSA usa una coppia di chiavi:\n"
        "  [bold green]Chiave Pubblica[/]  — chiunque può cifrarci o verificare le firme\n"
        "  [bold red]Chiave Privata[/]   — solo tu puoi decifrare o firmare\n\n"
        "Basato sulla difficoltà di fattorizzare grandi numeri:\n"
        "  p × q = N  (facile)  →  fattorizzare N = p, q  (impossibile se N grande)\n\n"
        "Usato in: HTTPS (autenticazione), firma digitale, email cifrata (PGP)",
        border_style="red"
    ))
    aspetta()

    p, q = 61, 53
    N = p * q
    phi = (p - 1) * (q - 1)
    e = 17
    d = pow(e, -1, phi)

    passi = [
        ("1. Scegli due primi",        f"p={p}  q={q}",                 "In produzione: p,q > 2^1024"),
        ("2. Calcola N",               f"N = p×q = {p}×{q} = {N}",     "Chiave pubblica e privata condividono N"),
        ("3. Euler totient",           f"φ(N) = (p-1)(q-1) = {phi}",   "Segreto — non si rivela"),
        ("4. Scegli e (pubblica)",     f"e={e}  (coprimo con φ(N))",   "Parte della chiave pubblica"),
        ("5. Calcola d (privata)",     f"d = e⁻¹ mod φ(N) = {d}",     "Inverso modulare — SEGRETO"),
        ("Chiave PUBBLICA",            f"(e={e}, N={N})",               "Condivisa con tutti"),
        ("Chiave PRIVATA",             f"(d={d}, N={N})",               "MAI condivisa"),
    ]

    for titolo, valore, nota in passi:
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(Panel(
            f"  [bold cyan]{titolo}[/]\n\n"
            f"  [bold yellow]{valore}[/]\n\n"
            f"  [dim]{nota}[/]",
            title="[bold]RSA — Generazione Chiavi[/]", border_style="red", padding=(1,2)
        ))
        aspetta()

    m = 65
    c = pow(m, e, N)
    m2 = pow(c, d, N)

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  [bold]CIFRATURA:[/]\n"
        f"  Messaggio originale: m = {m}\n"
        f"  Cifrato: c = m^e mod N = {m}^{e} mod {N} = [bold cyan]{c}[/]\n\n"
        f"  [bold]DECIFRATURA:[/]\n"
        f"  Cifrato: c = {c}\n"
        f"  Decifrato: m = c^d mod N = {c}^{d} mod {N} = [bold green]{m2}[/]\n\n"
        f"  Match: {'✓' if m == m2 else '✗'}  m={m} == m2={m2}\n\n"
        "  [bold yellow]Nota:[/] In pratica RSA cifra solo chiavi simmetriche (AES).\n"
        "  L'RSA puro è troppo lento per cifrare grandi quantità di dati.",
        title="[bold]RSA — Cifratura/Decifratura[/]", border_style="red"
    ))
    input("\n  [INVIO per tornare al menu] ")


# ═══════════════════════════════════════════════════════════════════
# CATEGORIA 3 — ARCHITETTURA API
# ═══════════════════════════════════════════════════════════════════

def demo_rest():
    """REST API: principi e operazioni CRUD"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]REST — Representational State Transfer[/]\n\n"
        "Stile architetturale per API web basato su 6 principi:\n"
        "  1. [bold]Client-Server[/]     — separazione interfaccia da logica\n"
        "  2. [bold]Stateless[/]         — ogni richiesta contiene tutto il necessario\n"
        "  3. [bold]Cacheable[/]         — le risposte possono essere memorizzate\n"
        "  4. [bold]Uniform Interface[/] — URL come risorse, verbi HTTP per azioni\n"
        "  5. [bold]Layered System[/]    — proxy, gateway trasparenti\n"
        "  6. [bold]Code on Demand[/]    — opzionale: server può inviare codice\n\n"
        "  Risorsa: /utenti, /utenti/42, /utenti/42/ordini",
        border_style="cyan"
    ))
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]CRUD → verbi HTTP → status code:[/]\n")
    tab = Table(box=None, show_header=True, padding=(0, 2))
    tab.add_column("Azione", style="bold", min_width=14)
    tab.add_column("Metodo HTTP", style="cyan", min_width=12)
    tab.add_column("URL", style="yellow", min_width=22)
    tab.add_column("Body", style="dim", min_width=20)
    tab.add_column("Status OK", style="green", min_width=12)
    operazioni = [
        ("Lista utenti",  "GET",    "/utenti",    "—",                  "200 OK"),
        ("Crea utente",   "POST",   "/utenti",    '{"nome":"Mario"}',   "201 Created"),
        ("Leggi utente",  "GET",    "/utenti/42", "—",                  "200 OK"),
        ("Aggiorna tutto","PUT",    "/utenti/42", '{"nome":"Luigi"}',   "200 OK"),
        ("Aggiorna campo","PATCH",  "/utenti/42", '{"email":"..."}',    "200 OK"),
        ("Elimina",       "DELETE", "/utenti/42", "—",                  "204 No Content"),
    ]
    for r in operazioni:
        tab.add_row(*r)
    console.print(tab)
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]REST Best Practices:[/]\n")
    best = [
        ("URL → sostantivi, non verbi",  "/utenti/42 ✓   /getUtente ✗"),
        ("Versioning nell'URL o header", "/api/v1/utenti  o  Accept: application/vnd.api+json;v=1"),
        ("Paginazione",                  "?page=2&limit=20  o  ?cursor=eyJ..."),
        ("Filtering/Sorting",            "?ruolo=admin&sort=-createdAt"),
        ("HATEOAS",                      "Ogni risposta include link alle azioni disponibili"),
        ("Errori strutturati",           '{"error":"NOT_FOUND","message":"Utente 42 non trovato"}'),
        ("Idempotenza",                  "GET, PUT, DELETE = idempotenti. POST = non idempotente"),
    ]
    for titolo, esempio in best:
        console.print(f"  [bold cyan]►[/] {titolo}")
        console.print(f"    [dim]{esempio}[/]\n")
    aspetta()
    input("\n  [INVIO per tornare al menu] ")


def demo_graphql():
    """GraphQL vs REST: query, mutation, subscription"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]GraphQL — Query Language per API[/]\n\n"
        "Problema REST: over-fetching (troppi dati) e under-fetching (troppe chiamate).\n"
        "GraphQL: il client specifica ESATTAMENTE i dati che vuole, in UNA sola chiamata.\n\n"
        "  [bold]Query[/]        — legge dati (equivale a GET)\n"
        "  [bold]Mutation[/]     — modifica dati (equivale a POST/PUT/DELETE)\n"
        "  [bold]Subscription[/] — dati in tempo reale (WebSocket)\n\n"
        "  Un solo endpoint: POST /graphql  (tutto passa da lì)\n"
        "  Introspection: il client può chiedere al server il suo schema",
        border_style="magenta"
    ))
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]REST vs GraphQL — confronto:[/]\n")
    console.print("  [bold]Problema:[/] Voglio nome utente + i titoli dei suoi ultimi 3 post\n")

    console.print("  [bold red]REST — 2+ chiamate:[/]")
    console.print("  GET /utenti/42                    → {id, nome, email, avatar, bio, ...}")
    console.print("  GET /utenti/42/post?limit=3       → [{id, titolo, corpo, data, tags, ...}]\n")
    console.print("  [dim]Problema: riceviamo campi inutili (over-fetching)[/]\n")
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("  [bold green]GraphQL — 1 chiamata:[/]\n")
    query = '''  query {
    utente(id: 42) {
      nome
      post(limit: 3) {
        titolo
      }
    }
  }'''
    risposta = '''  {
    "data": {
      "utente": {
        "nome": "Mario Rossi",
        "post": [
          {"titolo": "Come usare Python"},
          {"titolo": "Docker in 5 minuti"},
          {"titolo": "REST vs GraphQL"}
        ]
      }
    }
  }'''
    console.print(f"  [yellow]{query}[/]\n")
    console.print(f"  [green]Risposta:[/]\n{risposta}\n")
    console.print("  [dim]Riceviamo ESATTAMENTE i campi richiesti. Zero over-fetching.[/]")
    aspetta()

    console.print(Panel(
        "  [bold green]GraphQL vantaggi:[/]\n"
        "  • Un solo endpoint — meno versioning da gestire\n"
        "  • Il client controlla i dati — frontend indipendente dal backend\n"
        "  • Introspection — documentazione automatica (es. GraphiQL)\n"
        "  • Subscriptions — real-time nativo\n\n"
        "  [bold red]GraphQL svantaggi:[/]\n"
        "  • Caching più complesso (POST invece di GET)\n"
        "  • N+1 problem — ogni campo può triggerare query DB separate (usare DataLoader)\n"
        "  • Curva di apprendimento più alta\n"
        "  • Over-engineering per API semplici",
        title="[bold]GraphQL — RIEPILOGO[/]", border_style="magenta"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_grpc():
    """gRPC: RPC con Protocol Buffers"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]gRPC — Google Remote Procedure Call[/]\n\n"
        "gRPC è un framework RPC ad alte prestazioni che usa:\n"
        "  • [bold]Protocol Buffers (protobuf)[/] — serializzazione binaria (3-10x più compatto di JSON)\n"
        "  • [bold]HTTP/2[/] — multiplexing, header compression, streaming bidirezionale\n"
        "  • [bold]Codegen[/] — genera automaticamente client/server da file .proto\n\n"
        "  4 tipi di chiamata:\n"
        "  Unary (1→1),  Server streaming (1→∞),  Client streaming (∞→1),  Bidirezionale",
        border_style="blue"
    ))
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]Definizione .proto:[/]\n")
    proto = '''  syntax = "proto3";

  service UserService {
    rpc GetUser (UserRequest) returns (UserResponse);      // Unary
    rpc ListUsers (Empty) returns (stream UserResponse);   // Server streaming
    rpc Chat (stream ChatMsg) returns (stream ChatMsg);    // Bidirezionale
  }

  message UserRequest { int32 id = 1; }
  message UserResponse {
    int32 id = 1;
    string nome = 2;
    string email = 3;
  }'''
    console.print(f"[yellow]{proto}[/]")
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]REST vs gRPC — confronto:[/]\n")
    tab = Table(box=None, show_header=True, padding=(0, 2))
    tab.add_column("Caratteristica", style="bold", min_width=24)
    tab.add_column("REST", style="cyan", min_width=26)
    tab.add_column("gRPC", style="yellow", min_width=26)
    righe = [
        ("Protocollo",       "HTTP/1.1 o HTTP/2",     "HTTP/2 (obbligatorio)"),
        ("Formato dati",     "JSON (testo, leggibile)","Protobuf (binario, compatto)"),
        ("Dimensione payload","100%",                   "~30% (3x più piccolo)"),
        ("Velocità",         "Riferimento",            "5-10x più veloce"),
        ("Streaming",        "Limitato (SSE, WS sep.)","Bidirezionale nativo"),
        ("Codegen",          "Manuale o OpenAPI",       "Automatico da .proto"),
        ("Browser support",  "Nativo",                  "Solo con grpc-web proxy"),
        ("Uso tipico",       "API pubblica, web, mobile","Microservizi interni"),
    ]
    for r in righe:
        tab.add_row(*r)
    console.print(tab)
    aspetta()
    input("\n  [INVIO per tornare al menu] ")


def demo_websocket():
    """WebSocket: comunicazione bidirezionale full-duplex"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]WebSocket — Comunicazione Full-Duplex[/]\n\n"
        "HTTP è half-duplex: solo il client può iniziare richieste.\n"
        "WebSocket: connessione persistente bidirezionale — server e client si inviano\n"
        "messaggi in qualsiasi momento, senza una nuova connessione.\n\n"
        "  Handshake iniziale su HTTP → upgrade a WebSocket protocol\n"
        "  Frame binari o testo — overhead minimo dopo l'handshake\n"
        "  Porta 80 (ws://) o 443 (wss:// con TLS)\n\n"
        "  Usato in: chat, gaming, trading, notifiche real-time, collaborazione",
        border_style="magenta"
    ))
    aspetta()

    parti = ["BROWSER", "SERVER"]
    passi_hs = [
        {"da":"BROWSER","a":"SERVER","msg":"GET /chat HTTP/1.1","note":"","colore":"dim"},
        {"da":"BROWSER","a":"SERVER","msg":"Upgrade: websocket","note":"Richiesta upgrade a WebSocket","colore":"yellow"},
        {"da":"BROWSER","a":"SERVER","msg":"Connection: Upgrade","note":"","colore":"dim"},
        {"da":"BROWSER","a":"SERVER","msg":"Sec-WebSocket-Key: dGhlIHNhbXBsZQ==","note":"Challenge random (base64)","colore":"yellow"},
        {"da":"SERVER","a":"BROWSER","msg":"HTTP/1.1 101 Switching Protocols","note":"Upgrade accettato!","colore":"bold green"},
        {"da":"SERVER","a":"BROWSER","msg":"Upgrade: websocket","note":"","colore":"dim"},
        {"da":"SERVER","a":"BROWSER","msg":"Sec-WebSocket-Accept: s3pPLMBi...","note":"SHA1(key + GUID) — verifica","colore":"green"},
    ]
    for i, p in enumerate(passi_hs):
        mostra_flusso("WebSocket Handshake", parti, passi_hs[:i+1])
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold cyan]Connessione WebSocket aperta — messaggi full-duplex:[/]\n")
    messaggi = [
        ("BROWSER", "SERVER", '{"type":"join","room":"chat-42"}', "Il client si unisce alla stanza"),
        ("SERVER", "BROWSER", '{"type":"users","count":15}',       "Il server risponde con info"),
        ("BROWSER", "SERVER", '{"type":"msg","text":"Ciao!"}',     "Il client invia un messaggio"),
        ("SERVER", "BROWSER", '{"type":"msg","from":"Mario","text":"Ciao!"}', "Server fa broadcast"),
        ("SERVER", "BROWSER", '{"type":"typing","user":"Luigi"}',  "Server notifica: Luigi sta scrivendo"),
        ("SERVER", "BROWSER", '{"type":"msg","from":"Luigi","text":"Hey!"}',  "Nuovo messaggio in arrivo"),
        ("BROWSER", "SERVER", "CLOSE frame (codice 1000)",          "Client chiude la connessione"),
        ("SERVER", "BROWSER", "CLOSE frame (1000) — bye!",          "Server conferma la chiusura"),
    ]
    for da, a, msg, nota in messaggi:
        freccia = "→" if da == "BROWSER" else "←"
        colore = "yellow" if da == "BROWSER" else "green"
        console.print(f"  [{colore}]{da} {freccia} {a}:[/]  [dim]{msg}[/]")
        console.print(f"  [dim]   {nota}[/]\n")
    aspetta()
    input("\n  [INVIO per tornare al menu] ")


def demo_cors():
    """CORS: Cross-Origin Resource Sharing"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]CORS — Cross-Origin Resource Sharing[/]\n\n"
        "Same-Origin Policy: il browser blocca le richieste a domini diversi.\n"
        "Origine = schema + dominio + porta (https://app.com:443 ≠ https://api.com)\n\n"
        "CORS permette al SERVER di dichiarare quali origini sono autorizzate.\n"
        "NON è una protezione server-side — è una protezione del browser!\n"
        "Una richiesta diretta (curl, Postman) ignora il CORS.\n\n"
        "  Preflight: per richieste 'non semplici' il browser invia prima un OPTIONS",
        border_style="red"
    ))
    aspetta()

    parti = ["BROWSER (app.com)", "API (api.com)"]
    console.print("\n  [bold cyan]Caso 1: Richiesta semplice (GET con headers standard)[/]\n")
    semplice = [
        {"da":"BROWSER (app.com)","a":"API (api.com)","msg":"GET /dati","note":"Origin: https://app.com","colore":"yellow"},
        {"da":"API (api.com)","a":"BROWSER (app.com)","msg":"200 OK + dati","note":"Access-Control-Allow-Origin: https://app.com ✓","colore":"green"},
    ]
    mostra_flusso("CORS — Richiesta Semplice", parti, semplice)
    aspetta()

    console.print("\n  [bold cyan]Caso 2: Preflight (metodo non standard o header custom)[/]\n")
    preflight = [
        {"da":"BROWSER (app.com)","a":"API (api.com)","msg":"OPTIONS /dati","note":"Origin: https://app.com, Method: DELETE, Headers: X-Custom","colore":"yellow"},
        {"da":"API (api.com)","a":"BROWSER (app.com)","msg":"204 No Content","note":"Allow-Origin: app.com, Allow-Methods: GET,POST,DELETE","colore":"green"},
        {"da":"BROWSER (app.com)","a":"API (api.com)","msg":"DELETE /dati/42","note":"Preflight OK → invia la richiesta reale","colore":"cyan"},
        {"da":"API (api.com)","a":"BROWSER (app.com)","msg":"200 OK","note":"Risposta alla richiesta reale","colore":"green"},
    ]
    mostra_flusso("CORS — Preflight", parti, preflight)
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "  [bold]Header CORS lato server (esempio Express/Python):[/]\n\n"
        "  Access-Control-Allow-Origin: https://app.com  (o * per tutti)\n"
        "  Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\n"
        "  Access-Control-Allow-Headers: Content-Type, Authorization\n"
        "  Access-Control-Max-Age: 3600  (cache preflight 1 ora)\n"
        "  Access-Control-Allow-Credentials: true  (invia cookie)\n\n"
        "  [bold red]⚠ Access-Control-Allow-Origin: *[/] non funziona con credentials!\n"
        "  [bold yellow]Errore comune:[/] CORS sul client — configurare SEMPRE lato server",
        title="[bold]CORS — RIEPILOGO[/]", border_style="red"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_rate_limiting():
    """Rate Limiting: Token Bucket e Sliding Window"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]RATE LIMITING — Protezione dalle Richieste Eccessive[/]\n\n"
        "Limita il numero di richieste per proteggere l'API da:\n"
        "  • Abuso/DDoS — troppe richieste dallo stesso client\n"
        "  • Fair use — garantire equità tra gli utenti\n"
        "  • Costo — ogni richiesta consuma risorse\n\n"
        "  Algoritmi principali:\n"
        "  [bold]Token Bucket[/]  — secchio di token che si ricarica nel tempo\n"
        "  [bold]Leaky Bucket[/]  — coda a tasso costante di uscita\n"
        "  [bold]Fixed Window[/]  — N richieste per finestra temporale\n"
        "  [bold]Sliding Window[/]— più preciso, evita burst al cambio finestra",
        border_style="yellow"
    ))
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]Token Bucket — simulazione:[/]")
    console.print("  Capacità: 5 token  |  Ricarica: 1 token/secondo\n")

    CAPACITA = 5
    bucket = CAPACITA
    richieste = [
        (0, "Richiesta API"),
        (0, "Richiesta API"),
        (0, "Richiesta API"),
        (1, "Richiesta API"),
        (0, "Richiesta API"),
        (0, "Richiesta API (burst!)"),
        (0, "Richiesta API (burst!)"),
        (2, "Richiesta API"),
    ]

    tempo = 0
    for delta_t, desc in richieste:
        tempo += delta_t
        bucket = min(CAPACITA, bucket + delta_t)

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Token Bucket[/]  t={tempo}s  Bucket: {'🪙'*bucket}{'·'*(CAPACITA-bucket)} ({bucket}/{CAPACITA} token)")
        console.print(f"\n  → {desc}")

        if bucket > 0:
            bucket -= 1
            console.print(f"  [bold green]✓ PERMESSA — consuma 1 token → {bucket} rimasti[/]")
        else:
            console.print(f"  [bold red]✗ RIFIUTATA — 429 Too Many Requests (bucket vuoto!)[/]")
        aspetta()

    console.print(Panel(
        "  [bold]Header di risposta con Rate Limit:[/]\n\n"
        "  X-RateLimit-Limit: 100          (max richieste per finestra)\n"
        "  X-RateLimit-Remaining: 73       (rimaste in questa finestra)\n"
        "  X-RateLimit-Reset: 1708603600   (timestamp reset)\n"
        "  Retry-After: 47                 (secondi da aspettare — 429)\n\n"
        "  [bold cyan]Dove implementare:[/]\n"
        "  API Gateway (Kong, Nginx, AWS Gateway) — prima di raggiungere il backend\n"
        "  Redis — contatore condiviso tra più istanze del server",
        title="[bold]Rate Limiting — RIEPILOGO[/]", border_style="yellow"
    ))
    input("\n  [INVIO per tornare al menu] ")


# ═══════════════════════════════════════════════════════════════════
# CATEGORIA 4 — SISTEMI DISTRIBUITI
# ═══════════════════════════════════════════════════════════════════

def demo_paxos():
    """Paxos: algoritmo di consenso distribuito"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]PAXOS — Consenso Distribuito[/]\n\n"
        "Problema: come fanno N nodi a raggiungere un ACCORDO (consenso) su un valore\n"
        "in presenza di guasti e messaggi persi?\n\n"
        "Paxos (Lamport, 1989) garantisce:\n"
        "  • Safety: viene scelto al massimo un valore\n"
        "  • Liveness: alla fine un valore viene scelto (se la maggioranza è viva)\n\n"
        "  Ruoli: Proposer, Acceptor, Learner\n"
        "  Fasi: Prepare → Promise → Accept → Accepted\n"
        "  Quorum: maggioranza (N/2 + 1) — con 5 nodi bastano 3",
        border_style="blue"
    ))
    aspetta()

    nodi = ["P (Proposer)", "A1 (Acceptor)", "A2 (Acceptor)", "A3 (Acceptor)"]
    passi = [
        {"da":"P (Proposer)","a":"A1 (Acceptor)","msg":"Prepare(n=1)","note":"Voglio proporre, con numero proposta n=1","colore":"yellow"},
        {"da":"P (Proposer)","a":"A2 (Acceptor)","msg":"Prepare(n=1)","note":"","colore":"yellow"},
        {"da":"P (Proposer)","a":"A3 (Acceptor)","msg":"Prepare(n=1)","note":"","colore":"yellow"},
        {"da":"A1 (Acceptor)","a":"P (Proposer)","msg":"Promise(n=1, val=None)","note":"Prometto di non accettare n<1","colore":"green"},
        {"da":"A2 (Acceptor)","a":"P (Proposer)","msg":"Promise(n=1, val=None)","note":"Idem — QUORUM raggiunto (2/3)!","colore":"bold green"},
        {"da":"P (Proposer)","a":"A1 (Acceptor)","msg":"Accept(n=1, val='X')","note":"Ho quorum — propongo il valore 'X'","colore":"cyan"},
        {"da":"P (Proposer)","a":"A2 (Acceptor)","msg":"Accept(n=1, val='X')","note":"","colore":"cyan"},
        {"da":"A1 (Acceptor)","a":"P (Proposer)","msg":"Accepted(n=1, val='X')","note":"Accetto il valore","colore":"bold green"},
        {"da":"A2 (Acceptor)","a":"P (Proposer)","msg":"Accepted(n=1, val='X')","note":"QUORUM — 'X' è SCELTO definitivamente!","colore":"bold green"},
    ]
    for i, p in enumerate(passi):
        mostra_flusso("Paxos — Consenso", nodi, passi[:i+1], larghezza=90)
        aspetta()

    console.print(Panel(
        "  [bold green]✓ Il valore 'X' è stato scelto con consenso di 2/3 acceptor[/]\n\n"
        "  [bold]Problemi di Paxos:[/]\n"
        "  • Complesso da capire e implementare correttamente\n"
        "  • Liveness non garantita se due proposer si contendono\n"
        "  • Multi-Paxos: ottimizzazione per log di comandi\n\n"
        "  [bold cyan]Usato in:[/] Google Chubby, Apache Zookeeper (variante ZAB)\n"
        "  Oggi spesso sostituito da [bold]Raft[/] — più comprensibile",
        title="[bold]Paxos — RIEPILOGO[/]", border_style="blue"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_raft():
    """Raft: consenso distribuito più comprensibile di Paxos"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]RAFT — Consenso Distribuito (Understandable)[/]\n\n"
        "Raft è stato progettato per essere più comprensibile di Paxos.\n"
        "Divide il problema in 3 sottoproblemi indipendenti:\n\n"
        "  1. [bold]Leader Election[/]      — chi comanda il cluster\n"
        "  2. [bold]Log Replication[/]      — il leader replica i comandi\n"
        "  3. [bold]Safety[/]              — mai due leader nello stesso termine\n\n"
        "  Stato dei nodi: Follower → Candidate → Leader\n"
        "  Termine (term): numero che incrementa ad ogni elezione\n"
        "  Heartbeat: il leader invia ping periodici per mantenere l'autorità",
        border_style="blue"
    ))
    aspetta()

    stati = ["FOLLOWER", "CANDIDATE", "LEADER"]
    nodi = {"N1":"FOLLOWER","N2":"FOLLOWER","N3":"FOLLOWER","N4":"FOLLOWER","N5":"FOLLOWER"}

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold cyan]FASE 1: Leader Election[/]\n")
    passi_raft = [
        ("N1 election timeout scaduto", "N1", "CANDIDATE",
         "N1: non ricevo heartbeat → divento CANDIDATE, incremento term=1"),
        ("N1 chiede voti", "N1", "CANDIDATE",
         "N1 → tutti: RequestVote(term=1, lastLog=0)"),
        ("N2, N3 votano N1", "N2", "FOLLOWER",
         "N2, N3 → N1: VoteGranted — QUORUM! N1 diventa LEADER"),
        ("N1 diventa LEADER", "N1", "LEADER",
         "N1 invia Heartbeat a tutti: AppendEntries(term=1, entries=[])"),
    ]

    for desc, nodo_corrente, nuovo_stato, spiegazione in passi_raft:
        nodi[nodo_corrente] = nuovo_stato
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Raft — Leader Election[/]  {desc}")
        riga = "  "
        for nome, stato in nodi.items():
            colore = "bold green" if stato == "LEADER" else ("bold yellow" if stato == "CANDIDATE" else "dim")
            riga += f"[{colore}][{nome}:{stato}][/]  "
        console.print(riga)
        console.print(f"\n  [dim]{spiegazione}[/]")
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold cyan]FASE 2: Log Replication[/]\n")
    log_client = [("SET x=1", "term=1"), ("SET y=2", "term=1"), ("DEL x", "term=1")]
    log_leader = []

    for cmd, term in log_client:
        log_leader.append((cmd, term))
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Raft Log Replication[/]  Client → N1 (leader): [bold cyan]{cmd}[/]")
        console.print(f"\n  N1 (LEADER) log: {log_leader}")
        console.print(f"  N1 → N2,N3,N4,N5: AppendEntries([{cmd},{term}])")
        console.print(f"  N2,N3 → N1: Success  (QUORUM!)")
        console.print(f"  N1 COMMIT {cmd} → risponde al client ✓")
        console.print(f"  N1 → tutti: commit({cmd})")
        aspetta()

    console.print(Panel(
        "  [bold green]✓ Il cluster ha raggiunto consenso su tutti i comandi[/]\n\n"
        "  [bold]Fault tolerance:[/] con 5 nodi, tollera 2 guasti (floor(5/2)=2)\n\n"
        "  [bold cyan]Usato in:[/] etcd (Kubernetes), CockroachDB, TiKV, Consul\n"
        "  Strumento di test: Jepsen — verifica linearizzabilità",
        title="[bold]Raft — RIEPILOGO[/]", border_style="blue"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_gossip():
    """Gossip Protocol: disseminazione informazioni in sistemi distribuiti"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]GOSSIP PROTOCOL — Epidemico Distribuito[/]\n\n"
        "Come si diffondono le voci in un villaggio? Ogni persona ne parla con 2-3 vicini.\n"
        "Gossip funziona così: ogni nodo, periodicamente, sceglie K nodi casuali\n"
        "e condivide il suo stato con loro.\n\n"
        "  • Convergenza garantita: dopo O(log N) round, tutti sanno tutto\n"
        "  • Fault tolerant: se un nodo muore, le info continuano a propagarsi\n"
        "  • Scalabile: ogni nodo parla solo con K altri — complessità O(N log N)\n\n"
        "  Usato in: Cassandra (membership), Redis Cluster, AWS DynamoDB, Consul",
        border_style="magenta"
    ))
    aspetta()

    N = 8
    nodi = list(range(N))
    informati = {0}

    for round_num in range(1, 6):
        nuovi = set()
        for nodo in list(informati):
            targets = random.sample([n for n in nodi if n not in informati], min(2, N - len(informati))) if len(informati) < N else []
            for t in targets:
                informati.add(t)
                nuovi.add(t)

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Gossip Protocol[/]  Round {round_num}  Nodi informati: {len(informati)}/{N}\n")
        riga = "  "
        for nd in nodi:
            if nd in informati:
                riga += f"[bold green][N{nd}✓][/]  "
            else:
                riga += f"[dim][N{nd} ][/]  "
        console.print(riga)
        if nuovi:
            console.print(f"\n  [dim]Nuovi informati questo round: {[f'N{n}' for n in sorted(nuovi)]}[/]")
        aspetta()

        if len(informati) >= N:
            break

    console.print(Panel(
        f"  Tutti {N} nodi informati in {round_num} round!\n"
        f"  log₂({N}) ≈ {N.bit_length()-1} — convergenza logaritmica\n\n"
        "  [bold]Tipi di Gossip:[/]\n"
        "  Push: nodo informato → push stato ai vicini\n"
        "  Pull: nodo non informato → pull stato dai vicini\n"
        "  Push-Pull: entrambe le direzioni — più veloce\n\n"
        "  [bold cyan]Anti-entropy:[/] confronto periodico dell'intero stato tra due nodi\n"
        "  [bold cyan]Phi Accrual Failure Detection:[/] Cassandra usa gossip per rilevare nodi morti",
        title="[bold]Gossip Protocol — RIEPILOGO[/]", border_style="magenta"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_2pc():
    """Two-Phase Commit: transazioni distribuite"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]2PC — Two-Phase Commit[/]\n\n"
        "Problema: come eseguire una transazione su più database/servizi in modo\n"
        "ATOMICO — o tutti confermano (commit) o tutti annullano (rollback)?\n\n"
        "  Esempio: trasferimento bancario\n"
        "  DB1 (banca A): -100€   DB2 (banca B): +100€\n"
        "  → devono avvenire ENTRAMBE o NESSUNA\n\n"
        "  Fasi:\n"
        "  1. [bold]Prepare[/]  — il coordinatore chiede 'puoi fare commit?'\n"
        "  2. [bold]Commit/Abort[/] — tutti hanno risposto YES → commit; altrimenti → abort",
        border_style="red"
    ))
    aspetta()

    parti = ["COORDINATORE", "DB1 (banca A)", "DB2 (banca B)"]

    console.print("\n  [bold cyan]Scenario 1: SUCCESSO — tutti i partecipanti rispondono YES[/]\n")
    passi_ok = [
        {"da":"COORDINATORE","a":"DB1 (banca A)","msg":"PREPARE: sottrai 100€","note":"Logga la richiesta, prepara ma non commita","colore":"yellow"},
        {"da":"COORDINATORE","a":"DB2 (banca B)","msg":"PREPARE: aggiungi 100€","note":"","colore":"yellow"},
        {"da":"DB1 (banca A)","a":"COORDINATORE","msg":"YES — pronto","note":"Lock acquisito, dati scritti nel redo log","colore":"green"},
        {"da":"DB2 (banca B)","a":"COORDINATORE","msg":"YES — pronto","note":"Tutti YES → il coordinatore decide COMMIT","colore":"bold green"},
        {"da":"COORDINATORE","a":"DB1 (banca A)","msg":"COMMIT","note":"Applica il cambiamento definitivamente","colore":"bold green"},
        {"da":"COORDINATORE","a":"DB2 (banca B)","msg":"COMMIT","note":"","colore":"bold green"},
        {"da":"DB1 (banca A)","a":"COORDINATORE","msg":"ACK","note":"","colore":"dim"},
        {"da":"DB2 (banca B)","a":"COORDINATORE","msg":"ACK","note":"Transazione completata ✓","colore":"dim"},
    ]
    for i, p in enumerate(passi_ok):
        mostra_flusso("2PC — Commit", parti, passi_ok[:i+1])
        aspetta()

    console.print("\n  [bold red]Scenario 2: FALLIMENTO — DB2 risponde NO[/]\n")
    passi_abort = [
        {"da":"COORDINATORE","a":"DB1 (banca A)","msg":"PREPARE: sottrai 100€","note":"","colore":"yellow"},
        {"da":"COORDINATORE","a":"DB2 (banca B)","msg":"PREPARE: aggiungi 100€","note":"","colore":"yellow"},
        {"da":"DB1 (banca A)","a":"COORDINATORE","msg":"YES","note":"","colore":"green"},
        {"da":"DB2 (banca B)","a":"COORDINATORE","msg":"NO — fallito (saldo insufficiente)","note":"Almeno un NO → il coordinatore deve ABORT","colore":"bold red"},
        {"da":"COORDINATORE","a":"DB1 (banca A)","msg":"ROLLBACK","note":"Annulla le modifiche preparate","colore":"red"},
        {"da":"COORDINATORE","a":"DB2 (banca B)","msg":"ROLLBACK","note":"Nessuna modifica applicata","colore":"red"},
    ]
    for i, p in enumerate(passi_abort):
        mostra_flusso("2PC — Abort", parti, passi_abort[:i+1])
        aspetta()

    console.print(Panel(
        "  [bold red]Problemi del 2PC:[/]\n"
        "  • Blocking: se il coordinatore muore in fase 2, i DB rimangono bloccati\n"
        "  • Performance: richiede 2 round-trip + lock su tutte le risorse\n"
        "  • Adatto a sistemi omogenei (es. stesso DB su più shard)\n\n"
        "  [bold cyan]Alternativa moderna: SAGA Pattern[/]\n"
        "  Sequenza di transazioni locali + compensazioni in caso di fallimento\n"
        "  Eventualmente consistente — adatto ai microservizi",
        title="[bold]2PC — RIEPILOGO[/]", border_style="red"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_saga():
    """SAGA Pattern: transazioni distribuite con compensazioni"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]SAGA PATTERN — Transazioni Distribuite nei Microservizi[/]\n\n"
        "2PC non scala nei microservizi — ogni servizio ha il suo DB.\n"
        "SAGA: sequenza di transazioni locali. Se una fallisce → esegui\n"
        "transazioni COMPENSATORIE che annullano i passi precedenti.\n\n"
        "  Varianti:\n"
        "  [bold]Choreography[/]: ogni servizio pubblica eventi, i successivi reagiscono\n"
        "  [bold]Orchestration[/]: un orchestratore centrale comanda la sequenza\n\n"
        "  Esempio: acquisto e-commerce\n"
        "  Ordine → Pagamento → Inventario → Spedizione",
        border_style="magenta"
    ))
    aspetta()

    parti = ["ORDER SVC", "PAYMENT SVC", "INVENTORY SVC", "SHIP SVC"]
    console.print("\n  [bold cyan]Scenario 1: SUCCESSO — tutti i servizi confermano[/]\n")
    passi_ok = [
        {"da":"ORDER SVC","a":"PAYMENT SVC","msg":"Addebita 50€","note":"OrderCreated event","colore":"yellow"},
        {"da":"PAYMENT SVC","a":"INVENTORY SVC","msg":"Riserva 1x prodotto A","note":"PaymentCompleted event","colore":"green"},
        {"da":"INVENTORY SVC","a":"SHIP SVC","msg":"Spedisci ordine #99","note":"InventoryReserved event","colore":"green"},
        {"da":"SHIP SVC","a":"ORDER SVC","msg":"In consegna — tracking #XYZ","note":"ShipmentCreated event ✓","colore":"bold green"},
    ]
    for i, p in enumerate(passi_ok):
        mostra_flusso("SAGA — Flusso Successo", parti, passi_ok[:i+1], larghezza=85)
        aspetta()

    console.print("\n  [bold red]Scenario 2: FALLIMENTO all'Inventario → compensazioni[/]\n")
    passi_fail = [
        {"da":"ORDER SVC","a":"PAYMENT SVC","msg":"Addebita 50€","note":"","colore":"yellow"},
        {"da":"PAYMENT SVC","a":"INVENTORY SVC","msg":"Riserva prodotto A","note":"PaymentCompleted","colore":"green"},
        {"da":"INVENTORY SVC","a":"PAYMENT SVC","msg":"FALLIMENTO: prodotto esaurito!","note":"InventoryFailed event","colore":"bold red"},
        {"da":"PAYMENT SVC","a":"ORDER SVC","msg":"RIMBORSO 50€ (compensazione)","note":"PaymentRefunded event","colore":"yellow"},
        {"da":"ORDER SVC","a":"ORDER SVC","msg":"Ordine annullato","note":"Saga completata con rollback ✓","colore":"dim"},
    ]
    for i, p in enumerate(passi_fail):
        mostra_flusso("SAGA — Rollback con Compensazioni", parti, passi_fail[:i+1], larghezza=85)
        aspetta()

    console.print(Panel(
        "  [bold]SAGA vs 2PC:[/]\n\n"
        "  SAGA: eventualmente consistente, scalabile, adatto microservizi\n"
        "  2PC:  fortemente consistente, bloccante, adatto a DB omogenei\n\n"
        "  [bold cyan]Implementazioni:[/]\n"
        "  Axon Framework (Java), Temporal.io, AWS Step Functions\n\n"
        "  [bold yellow]Sfida principale:[/] idempotenza — ogni passo deve essere safe\n"
        "  se eseguito più volte (retry in caso di guasto)",
        title="[bold]SAGA — RIEPILOGO[/]", border_style="magenta"
    ))
    input("\n  [INVIO per tornare al menu] ")


def demo_vector_clocks():
    """Vector Clocks: ordinamento causale degli eventi distribuiti"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]VECTOR CLOCKS — Causalità nei Sistemi Distribuiti[/]\n\n"
        "In un sistema distribuito non esiste un orologio globale.\n"
        "Come capire se evento A è avvenuto PRIMA di evento B?\n\n"
        "  Lamport Clock: intero che incrementa — ma non cattura la causalità\n"
        "  Vector Clock: array di N interi (uno per nodo) — cattura la causalità!\n\n"
        "  Regola:\n"
        "  • Evento locale: incrementa il tuo contatore VC[i]++\n"
        "  • Invio messaggio: includi il tuo VC corrente\n"
        "  • Ricezione: VC[j] = max(VC_locale[j], VC_ricevuto[j]) per ogni j, poi VC[i]++",
        border_style="cyan"
    ))
    aspetta()

    nodi_vc = ["A", "B", "C"]
    vc = {"A": [0,0,0], "B": [0,0,0], "C": [0,0,0]}

    def idx(n): return nodi_vc.index(n)
    def aggiorna_send(mittente): vc[mittente][idx(mittente)] += 1
    def aggiorna_recv(dest, vc_msg):
        for j in range(len(nodi_vc)):
            vc[dest][j] = max(vc[dest][j], vc_msg[j])
        vc[dest][idx(dest)] += 1

    eventi = [
        ("locale", "A", None, None, "A processa evento locale"),
        ("send", "A", "B", None, "A invia messaggio a B"),
        ("locale", "B", None, None, "B processa evento locale"),
        ("recv", "B", "A", None, "B riceve da A"),
        ("send", "B", "C", None, "B invia messaggio a C"),
        ("recv", "C", "B", None, "C riceve da B"),
        ("locale", "A", None, None, "A altro evento locale"),
    ]

    for tipo, nodo, altro, _, desc in eventi:
        vc_prima = {k: v[:] for k, v in vc.items()}

        if tipo == "locale":
            vc[nodo][idx(nodo)] += 1
        elif tipo == "send":
            vc[nodo][idx(nodo)] += 1
        elif tipo == "recv":
            aggiorna_recv(nodo, vc_prima[altro])

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Vector Clocks[/]  {desc}\n")
        tab = Table(box=None, show_header=True, padding=(0, 2))
        tab.add_column("Nodo", style="bold", min_width=8)
        tab.add_column("Prima", style="dim", min_width=18)
        tab.add_column("Dopo", style="bold green", min_width=18)
        for n in nodi_vc:
            tab.add_row(n, str(vc_prima[n]), str(vc[n]))
        console.print(tab)
        console.print(f"\n  [dim]Legenda: [A,B,C] = [eventi su A, eventi su B, eventi su C][/]")
        aspetta()

    console.print(Panel(
        "  [bold]Come confrontare due VC:[/]\n\n"
        "  VC_A < VC_B (A precede causalmente B) se:\n"
        "    VC_A[i] ≤ VC_B[i] per tutti i  E  VC_A[j] < VC_B[j] per almeno un j\n\n"
        "  VC_A ∥ VC_B (concorrenti — nessun ordine causale) se:\n"
        "    né A < B né B < A\n\n"
        "  [bold cyan]Usato in:[/] DynamoDB, Riak, CouchDB — risoluzione conflitti\n"
        "  Versioni: VC permette di rilevare scritture concorrenti → merge o last-write-wins",
        title="[bold]Vector Clocks — RIEPILOGO[/]", border_style="cyan"
    ))
    input("\n  [INVIO per tornare al menu] ")


# ═══════════════════════════════════════════════════════════════════
# MENU PRINCIPALE
# ═══════════════════════════════════════════════════════════════════

def menu_protocolli():
    DEMO = {
        # RETE
        1:  demo_tcp_handshake,
        2:  demo_tcp_sliding_window,
        3:  demo_udp,
        4:  demo_http,
        5:  demo_http2,
        6:  demo_dns,
        7:  demo_dhcp,
        8:  demo_arp,
        9:  demo_ssh,
        # SICUREZZA
        10: demo_tls,
        11: demo_oauth2,
        12: demo_jwt,
        13: demo_diffie_hellman,
        14: demo_rsa,
        # API
        15: demo_rest,
        16: demo_graphql,
        17: demo_grpc,
        18: demo_websocket,
        19: demo_cors,
        20: demo_rate_limiting,
        # DISTRIBUITI
        21: demo_paxos,
        22: demo_raft,
        23: demo_gossip,
        24: demo_2pc,
        25: demo_saga,
        26: demo_vector_clocks,
    }

    def _v(n, nome):
        return f"  [dim]{n:2}.[/] {nome}"

    while True:
        os.system("cls" if sys.platform == "win32" else "clear")

        tab = Table(box=None, show_header=False, expand=False, padding=(0, 2, 0, 1))
        tab.add_column(min_width=28, no_wrap=True)
        tab.add_column(min_width=28, no_wrap=True)
        tab.add_column(min_width=28, no_wrap=True)

        def sep(titolo, colore):
            tab.add_row(f"[bold {colore}]{titolo}[/]", "", "")

        sep("🌐 RETE — Fondamenti", "blue")
        tab.add_row(_v(1, "TCP Handshake + Close"), _v(2, "TCP Sliding Window"), _v(3, "UDP"))
        tab.add_row(_v(4, "HTTP/1.1"),             _v(5, "HTTP/2"),             _v(6, "DNS"))
        tab.add_row(_v(7, "DHCP"),                 _v(8, "ARP"),               _v(9, "SSH"))
        tab.add_row("","","")

        sep("🔒 SICUREZZA — Crittografia e Auth", "red")
        tab.add_row(_v(10,"TLS 1.3"),              _v(11,"OAuth 2.0"),          _v(12,"JWT"))
        tab.add_row(_v(13,"Diffie-Hellman"),        _v(14,"RSA"),               "")
        tab.add_row("","","")

        sep("🔌 API — Architettura e Comunicazione", "cyan")
        tab.add_row(_v(15,"REST"),                  _v(16,"GraphQL"),            _v(17,"gRPC"))
        tab.add_row(_v(18,"WebSocket"),             _v(19,"CORS"),               _v(20,"Rate Limiting"))
        tab.add_row("","","")

        sep("⚙️ SISTEMI DISTRIBUITI — Consenso e Coordinamento", "magenta")
        tab.add_row(_v(21,"Paxos"),                 _v(22,"Raft"),               _v(23,"Gossip Protocol"))
        tab.add_row(_v(24,"Two-Phase Commit"),       _v(25,"SAGA Pattern"),       _v(26,"Vector Clocks"))
        tab.add_row("","","")

        tab.add_row(_v(0, "Torna al menu principale"), "", "")

        console.print(Panel(
            tab,
            title="[bold]Libreria Mondiale dei Protocolli — 26 protocolli[/]",
            border_style="blue",
            padding=(1, 2),
        ))
        console.print("  [dim italic]Prismalux  ›  Apprendimento  ›  Protocolli[/]")

        try:
            scelta_raw = input("\n  ❯ ").strip()
        except Ridimensiona:
            continue

        if scelta_raw == "0":
            break

        try:
            s = int(scelta_raw)
        except ValueError:
            continue

        if s < 1 or s > 26:
            continue

        try:
            DEMO[s]()
        except KeyboardInterrupt:
            console.print("\n\n  Demo interrotta.")
            input("  [INVIO per tornare al menu] ")


if __name__ == "__main__":
    try:
        menu_protocolli()
    except KeyboardInterrupt:
        console.print("\n\n  🍺 Alla prossima libagione di sapere.\n")

"""
MCPs/_shared/net_safety.py — Validazione URL condivisa contro SSRF.

Usata da web_scraper_mcp, streamlink_mcp, api_tester_mcp — tool che
fanno fetch di URL forniti da un agente autonomo/LLM potenzialmente non
fidato (prompt injection da contenuto letto da file/web). Blocca schemi
diversi da http/https (incl. file://) e host che risolvono via DNS a
indirizzi privati/loopback/link-local/riservati/multicast — protezione
base anche da DNS rebinding (un dominio pubblico che risolve a un IP
privato viene comunque bloccato).
"""
import ipaddress
import socket
import urllib.parse
import urllib.request
import urllib.error


def is_public_ip(ip_str: str, allow_loopback: bool = False) -> bool:
    try:
        ip = ipaddress.ip_address(ip_str)
    except ValueError:
        return False
    if allow_loopback and ip.is_loopback:
        return True
    return not (ip.is_private or ip.is_loopback or ip.is_link_local
                or ip.is_reserved or ip.is_multicast or ip.is_unspecified)


def validate_url(url: str, allow_loopback: bool = False) -> str | None:
    """None se l'URL è sicuro da contattare, altrimenti un messaggio d'errore.

    allow_loopback=True permette esplicitamente 127.0.0.1/::1 — usato da
    api_tester_mcp, il cui scopo dichiarato è testare i servizi LOCALI di
    Prismalux (LAN server :11500, WAN Compute :11600, Ollama :11434,
    llama-server :8081). Resta comunque bloccata la scansione di altri
    host della LAN (192.168.x/10.x/172.16-31.x) e l'IP metadata cloud/
    link-local (169.254.x, incluso 169.254.169.254).
    """
    url = url.strip()
    try:
        parsed = urllib.parse.urlparse(url)
    except Exception:
        return "URL non valido"
    if parsed.scheme not in ("http", "https"):
        return f"schema non consentito: {parsed.scheme!r} (solo http/https)"
    host = parsed.hostname
    if not host:
        return "host mancante nell'URL"
    try:
        addrs = socket.getaddrinfo(host, None)
    except socket.gaierror as e:
        return f"impossibile risolvere l'host: {e}"
    for _family, _type, _proto, _canon, sockaddr in addrs:
        if not is_public_ip(sockaddr[0], allow_loopback=allow_loopback):
            return f"host risolve a un indirizzo non pubblico ({sockaddr[0]}), bloccato"
    return None


def build_safe_opener(allow_loopback: bool = False) -> urllib.request.OpenerDirector:
    """Opener urllib che rivalida ogni redirect con validate_url() prima di
    seguirlo — senza questo, un URL pubblico che supera il controllo
    iniziale potrebbe rispondere con un 302 verso un IP privato/localhost
    e aggirare il blocco."""

    class _SafeRedirectHandler(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):
            err = validate_url(newurl, allow_loopback=allow_loopback)
            if err:
                raise urllib.error.URLError(f"redirect bloccato verso {newurl!r}: {err}")
            return super().redirect_request(req, fp, code, msg, headers, newurl)

    return urllib.request.build_opener(_SafeRedirectHandler)

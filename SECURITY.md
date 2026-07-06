# Security Policy

## Segnalare una vulnerabilità

**Non aprire una issue pubblica per problemi di sicurezza.**

Canali privati, in ordine di preferenza:

1. **GitHub Security Advisory** (consigliato): [Report a vulnerability](https://github.com/wildlux/Prismalux/security/advisories/new) — resta privato finché non viene pubblicata una fix.
2. **Email**: wildlux@gmail.com con oggetto `[SECURITY] Prismalux`.

Includi se possibile: versione (tag o commit), piattaforma, passi per riprodurre, impatto stimato.

## Tempi di risposta indicativi

Prismalux è mantenuto da una sola persona nel tempo libero:

- **Presa in carico**: entro 7 giorni
- **Valutazione e triage**: entro 14 giorni
- **Fix per vulnerabilità confermate**: in base alla gravità; le critiche hanno precedenza su qualunque feature

## Ambito (scope)

Aree di interesse particolare — il progetto espone servizi di rete locali:

| Componente | Superficie |
|---|---|
| **LAN server** (`gui/lan_server*.cpp`) | HTTPS self-signed, token Bearer, web app `/web`, upload file, endpoint STT/AI |
| **Vision3D** (`gui/pages/widget_vision3d.*`) | Server HTTPS per telefoni (porta 8443), upload foto |
| **WAN Compute** (porta 11600) | Task distribuiti via TCP tra nodi registrati |
| **MCPs/** (59 plugin Python) | Subprocess JSON-RPC, dipendenze pip, SSRF nei tool di rete |
| **Web app** (`webchat.html`) | XSS, gestione sessione/cookie |
| **Auto-update / release** | Integrità dei download (vedi `SHA256SUMS.txt` nelle release) |

Fuori scope: vulnerabilità in Ollama, Qt o altre dipendenze upstream (segnalale ai rispettivi progetti); attacchi che richiedono accesso fisico alla macchina; l'uso dell'AppImage su distribuzioni EOL.

## Versioni supportate

Solo l'ultima release (`master` / tag più recente) riceve fix di sicurezza.

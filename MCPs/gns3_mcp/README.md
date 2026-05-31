# GNS3 MCP — Prismalux

**Tipo**: HTTP REST (GNS3 Server API)
**Porta**: 3080

## Funzione
Crea topologie di rete, aggiunge router/switch/PC, configura link
e avvia/ferma nodi GNS3 direttamente dall'AI di Prismalux.
Cache SQLite locale per ridurre i round-trip verso il server.

## Installazione

### 1. Dipendenze Python
```bash
pip install gns3fy requests
```

### 2. Installa GNS3
- **Linux**:
  ```bash
  sudo add-apt-repository ppa:gns3/ppa
  sudo apt update
  sudo apt install gns3-gui gns3-server
  ```
- **Windows / macOS**: scarica da https://www.gns3.com/software/download

### 3. Avvia GNS3 Server
```bash
# Avvio server standalone
gns3server --port 3080

# oppure avvia GNS3 GUI che include il server integrato
gns3
```

### 4. Verifica connessione
```bash
curl http://localhost:3080/v2/version
# risposta: {"local":true,"version":"2.x.x"}
```

### 5. Configurazione opzionale
GNS3 Server accetta connessioni solo da localhost di default.
Per accettare connessioni di rete modifica `gns3_server.conf`:
```ini
[Server]
host = 0.0.0.0
port = 3080
```

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "gns3": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/gns3_mcp/server.py"]
    }
  }
}
```

## API GNS3 principali
| Endpoint | Uso |
|----------|-----|
| `GET  /v2/projects` | lista progetti |
| `POST /v2/projects` | crea progetto |
| `POST /v2/projects/{id}/nodes` | aggiunge nodo |
| `POST /v2/projects/{id}/links` | collega nodi |
| `POST /v2/projects/{id}/nodes/start` | avvia tutti i nodi |
